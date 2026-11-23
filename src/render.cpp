// render.cpp - implementacja grafiki (opis funkcji w render.h).
// Wykorzystuje kod z labów: extensionSetup() (lab3-6), readShader()/setShaders() (lab3, lab6),
// getBMPData() (lab2, poprawiony), loadOBJ() + buildIndexedMeshMap() (lab3), materiały i światła (lab2).
#include "render.h"
#include "font8x8.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <tuple>

#ifdef _WIN32
#define X(type, name) type name = nullptr;
GL_FUNCTION_LIST(X)
#undef X
#endif

namespace {

// Tryby shadera world.frag
enum Mode { MODE_MODEL = 0, MODE_TERRAIN = 1, MODE_SPRITE = 2, MODE_UNLIT = 3, MODE_SKY = 4, MODE_WATER = 5 };

// Program shadera sceny 3D z lokalizacjami uniformów (pobierane raz)
struct WorldProgram {
    GLuint id = 0;
    GLint model, view, proj, uvRect, mode, tint, specular, tiling, camPos;
    GLint sunDir, sunColor, ambient, lightCount, lightPos, lightColor, lightRange;
    GLint fogColor, fogDensity, time, explodeT, explodeOrigin, emissive;
};
struct UiProgram { GLuint id = 0; GLint screen, color; };

WorldProgram gWorld;     // world.vert + world.frag
WorldProgram gExplode;   // world.vert + explode.geom + world.frag
UiProgram gUi;           // ui.vert + ui.frag
GLuint gCurrentProgram = 0;

Texture gWhite, gFont;
Mesh gQuad, gCube, gSphere, gSpriteGrid, gPlane;
GLuint gUiVao = 0, gUiVbo = 0;

std::map<std::string, Texture> gTextures;
std::map<std::string, Mesh> gModels;

// Stan bieżącej klatki
struct Frame {
    glm::mat4 view{1.0f}, proj{1.0f};
    glm::vec3 camPos{0.0f};
    float time = 0.0f;
    bool wireframe = false;
} gFrame;

void useProgram(GLuint id) {
    if (gCurrentProgram != id) { glUseProgram(id); gCurrentProgram = id; }
}

// ---------------------------------------------------------------------------
// Shadery (readShader + setShaders z labów, z kontrolą błędów)
// ---------------------------------------------------------------------------
std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

GLuint compileShader(GLenum type, const std::string& path) {
    std::string source = readFile(path);
    if (source.empty()) {
        std::printf("[BLAD] Nie mozna wczytac shadera %s\n", path.c_str());
        return 0;
    }
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::printf("[BLAD] Kompilacja %s:\n%s\n", path.c_str(), log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint loadProgram(const std::string& vs, const std::string& fs, const std::string& gs = "") {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint g = gs.empty() ? 0 : compileShader(GL_GEOMETRY_SHADER, gs);
    if (!v || !f || (!gs.empty() && !g)) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    if (g) glAttachShader(program, g);
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);
    if (g) glDeleteShader(g);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::printf("[BLAD] Linkowanie (%s, %s):\n%s\n", vs.c_str(), fs.c_str(), log);
        return 0;
    }
    return program;
}

void initWorldProgram(WorldProgram& p) {
    auto loc = [&](const char* name) { return glGetUniformLocation(p.id, name); };
    p.model = loc("uModel");           p.view = loc("uView");           p.proj = loc("uProj");
    p.uvRect = loc("uUVRect");         p.mode = loc("uMode");           p.tint = loc("uTint");
    p.specular = loc("uSpecular");     p.tiling = loc("uTiling");       p.camPos = loc("uCamPos");
    p.sunDir = loc("uSunDir");         p.sunColor = loc("uSunColor");   p.ambient = loc("uAmbient");
    p.lightCount = loc("uLightCount"); p.lightPos = loc("uLightPos");   p.lightColor = loc("uLightColor");
    p.lightRange = loc("uLightRange"); p.fogColor = loc("uFogColor");   p.fogDensity = loc("uFogDensity");
    p.time = loc("uTime");             p.explodeT = loc("uExplodeT");   p.explodeOrigin = loc("uExplodeOrigin");
    p.emissive = loc("uEmissive");
    useProgram(p.id);
    const char* samplers[5] = {"uTex0", "uTex1", "uTex2", "uTex3", "uTex4"};
    for (int i = 0; i < 5; ++i) glUniform1i(loc(samplers[i]), i);
}

// ---------------------------------------------------------------------------
// Siatki pomocnicze
// ---------------------------------------------------------------------------
Mesh makeGridQuad(int n) {   // kwadrat x -0.5..0.5, y 0..1 podzielony na n x n
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    for (int j = 0; j <= n; ++j)
        for (int i = 0; i <= n; ++i) {
            float u = float(i) / n, t = float(j) / n;
            v.push_back({{u - 0.5f, t, 0.0f}, {0, 0, 1}, {u, t}});
        }
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            unsigned a = j * (n + 1) + i, b = a + 1, c = a + n + 1, d = c + 1;
            idx.insert(idx.end(), {a, b, d, a, d, c});
        }
    return createMesh(v, idx);
}

Mesh makeCube() {
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    const glm::vec3 normals[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const glm::vec3& n : normals) {
        glm::vec3 up = std::abs(n.y) > 0.5f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        glm::vec3 side = glm::cross(up, n);
        unsigned base = (unsigned)v.size();
        v.push_back({(n - side - up) * 0.5f, n, {0, 0}});
        v.push_back({(n + side - up) * 0.5f, n, {1, 0}});
        v.push_back({(n + side + up) * 0.5f, n, {1, 1}});
        v.push_back({(n - side + up) * 0.5f, n, {0, 1}});
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return createMesh(v, idx);
}

Mesh makeSphere(int segments, int rings) {   // promień 1, u wokół osi Y, v od dołu (0) do góry (1)
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    for (int r = 0; r <= rings; ++r) {
        float lat = glm::pi<float>() * (float(r) / rings - 0.5f);
        for (int s = 0; s <= segments; ++s) {
            float lon = glm::two_pi<float>() * float(s) / segments;
            glm::vec3 p(std::cos(lat) * std::sin(lon), std::sin(lat), -std::cos(lat) * std::cos(lon));
            v.push_back({p, p, {float(s) / segments, float(r) / rings}});
        }
    }
    for (int r = 0; r < rings; ++r)
        for (int s = 0; s < segments; ++s) {
            unsigned a = r * (segments + 1) + s, b = a + 1, c = a + segments + 1, d = c + 1;
            idx.insert(idx.end(), {a, c, d, a, d, b});
        }
    return createMesh(v, idx);
}

Mesh makePlane() {   // poziomy kwadrat x,z -0.5..0.5, normalna +Y
    std::vector<Vertex> v = {{{-0.5f, 0, -0.5f}, {0, 1, 0}, {0, 0}}, {{0.5f, 0, -0.5f}, {0, 1, 0}, {1, 0}},
                             {{0.5f, 0, 0.5f}, {0, 1, 0}, {1, 1}},   {{-0.5f, 0, 0.5f}, {0, 1, 0}, {0, 1}}};
    return createMesh(v, {0, 1, 2, 0, 2, 3});
}

Texture makeChecker() {   // zastępstwo za brakującą teksturę
    unsigned char px[8 * 8 * 4];
    for (int i = 0; i < 64; ++i) {
        bool on = ((i % 8) / 2 + (i / 8) / 2) % 2 == 0;
        unsigned char c[4] = {(unsigned char)(on ? 220 : 30), 0, (unsigned char)(on ? 120 : 30), 255};
        std::memcpy(px + i * 4, c, 4);
    }
    return createTexture(8, 8, px, true);
}

// Atlas czcionki: 16 x 8 komórek 8x10 px; wiersz 0 tekstury = dół obrazka
Texture makeFontAtlas() {
    const int cols = 16, rows = 8, w = cols * FONT_CELL_W, h = rows * FONT_CELL_H;
    std::vector<unsigned char> px(w * h * 4, 0);
    const int count = int(sizeof(FONT_GLYPHS) / sizeof(FONT_GLYPHS[0]));
    for (int g = 0; g < count; ++g) {
        int cx = (g % cols) * FONT_CELL_W, cy = (g / cols) * FONT_CELL_H;
        for (int r = 0; r < FONT_CELL_H; ++r)
            for (int c = 0; c < FONT_CELL_W; ++c)
                if (FONT_GLYPHS[g][r] >> c & 1) {
                    int y = h - 1 - (cy + r);   // odwrócenie: obraz od góry -> tekstura od dołu
                    unsigned char* p = &px[(y * w + cx + c) * 4];
                    p[0] = p[1] = p[2] = p[3] = 255;
                }
    }
    return createTexture(w, h, px.data(), true);
}

// ---------------------------------------------------------------------------
// Pomocnicze do BMP
// ---------------------------------------------------------------------------
uint32_t rd32(const unsigned char* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }
uint16_t rd16(const unsigned char* p) { return uint16_t(p[0] | p[1] << 8); }

unsigned char channel(uint32_t pixel, uint32_t mask) {   // wyciąga kanał wg maski bitowej, skaluje do 0..255
    if (mask == 0) return 0;
    int shift = 0;
    while (!(mask >> shift & 1)) ++shift;
    uint32_t bits = mask >> shift, value = (pixel & mask) >> shift;
    return (unsigned char)(bits == 255 ? value : value * 255 / bits);
}

// Dekodowanie UTF-8 -> kod Unicode
unsigned nextCodepoint(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    unsigned cp = c & (0x3F >> extra);
    while (extra-- > 0 && i < s.size()) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
    return cp;
}

int glyphIndex(unsigned cp) {
    if (cp >= 32 && cp <= 126) return int(cp) - FONT_FIRST_ASCII;
    for (int i = 0; i < 18; ++i)
        if (FONT_POLISH_CODES[i] == cp) return 95 + i;
    return '?' - FONT_FIRST_ASCII;
}

// Rysuje trójkąty 2D: dane = (x, y, u, v) na wierzchołek
void drawUiTriangles(const std::vector<float>& data, GLuint texture, glm::vec4 color) {
    if (data.empty()) return;
    useProgram(gUi.id);
    glUniform4f(gUi.color, color.r, color.g, color.b, color.a);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(gUiVao);
    glBindBuffer(GL_ARRAY_BUFFER, gUiVbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(data.size() / 4));
}

void pushQuad(std::vector<float>& d, float x, float y, float w, float h, glm::vec4 uv) {
    float u0 = uv.x, v0 = uv.y, u1 = uv.x + uv.z, v1 = uv.y + uv.w;   // v1 = góra obrazka
    float q[24] = {x, y, u0, v1,     x + w, y, u1, v1,     x + w, y + h, u1, v0,
                   x, y, u0, v1,     x + w, y + h, u1, v0, x, y + h, u0, v0};
    d.insert(d.end(), q, q + 24);
}

// Wspólne ustawienia rysowania obiektu shaderem sceny
void setObject(const WorldProgram& p, Mode mode, const glm::mat4& model, glm::vec4 uvRect,
               glm::vec3 tint, float specular, float emissive = 0.0f) {
    useProgram(p.id);
    glUniform1i(p.mode, mode);
    glUniformMatrix4fv(p.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4f(p.uvRect, uvRect.x, uvRect.y, uvRect.z, uvRect.w);
    glUniform3f(p.tint, tint.r, tint.g, tint.b);
    glUniform1f(p.specular, specular);
    glUniform1f(p.emissive, emissive);
}

void drawElements(const Mesh& mesh) {
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
}

void bindTexture(int unit, const Texture& tex) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex.id ? tex.id : gWhite.id);
}

glm::mat4 spriteMatrix(glm::vec3 feet, glm::vec2 size) {   // billboard: obrót wokół Y w stronę kamery
    glm::vec3 toCam = gFrame.camPos - feet;
    float angle = std::atan2(toCam.x, toCam.z);
    glm::mat4 m = glm::translate(glm::mat4(1.0f), feet);
    m = glm::rotate(m, angle, glm::vec3(0, 1, 0));
    return glm::scale(m, glm::vec3(size.x, size.y, 1.0f));
}

glm::vec4 frameRect(const Texture& sheet, int col, int row, int cols, int rows) {
    float fw = 1.0f / cols, fh = 1.0f / rows;
    float insetU = sheet.width ? 0.5f / sheet.width : 0.0f, insetV = sheet.height ? 0.5f / sheet.height : 0.0f;
    float u0 = col * fw, v0 = 1.0f - (row + 1) * fh;   // wiersz 0 = górny wiersz obrazka
    return {u0 + insetU, v0 + insetV, fw - 2 * insetU, fh - 2 * insetV};
}

} // namespace

// ===========================================================================
// Kamera
// ===========================================================================
glm::vec3 yawToDir(float yawDeg) {
    float r = glm::radians(yawDeg);
    return {std::sin(r), 0.0f, -std::cos(r)};
}
float dirToYaw(glm::vec3 d) { return glm::degrees(std::atan2(d.x, -d.z)); }

glm::vec3 Camera::forward() const {
    float y = glm::radians(yaw), p = glm::radians(pitch);
    return {std::cos(p) * std::sin(y), std::sin(p), -std::cos(p) * std::cos(y)};
}
glm::vec3 Camera::flatForward() const { return yawToDir(yaw); }
glm::vec3 Camera::right() const {
    float y = glm::radians(yaw);
    return {std::cos(y), 0.0f, std::sin(y)};
}
glm::mat4 Camera::view() const { return glm::lookAt(pos, pos + forward(), glm::vec3(0, 1, 0)); }

// ===========================================================================
// Inicjalizacja
// ===========================================================================
bool initRenderer() {
    std::printf("OpenGL: %s\nKarta:  %s\nGLSL:   %s\n", (const char*)glGetString(GL_VERSION),
                (const char*)glGetString(GL_RENDERER), (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

#ifdef _WIN32
    bool loaded = true;
#define X(type, name)                                                         \
    name = (type)wglGetProcAddress(#name);                                    \
    if (!name) { std::printf("[BLAD] Brak funkcji OpenGL: %s\n", #name); loaded = false; }
    GL_FUNCTION_LIST(X)
#undef X
    if (!loaded) return false;
#endif

    gWorld.id = loadProgram("shaders/world.vert", "shaders/world.frag");
    gExplode.id = loadProgram("shaders/world.vert", "shaders/world.frag", "shaders/explode.geom");
    gUi.id = loadProgram("shaders/ui.vert", "shaders/ui.frag");
    if (!gWorld.id || !gExplode.id || !gUi.id) return false;
    initWorldProgram(gWorld);
    initWorldProgram(gExplode);
    useProgram(gUi.id);
    gUi.screen = glGetUniformLocation(gUi.id, "uScreen");
    gUi.color = glGetUniformLocation(gUi.id, "uColor");
    glUniform1i(glGetUniformLocation(gUi.id, "uTex"), 0);

    const unsigned char white[4] = {255, 255, 255, 255};
    gWhite = createTexture(1, 1, white, true);
    gFont = makeFontAtlas();

    gQuad = makeGridQuad(1);
    gSpriteGrid = makeGridQuad(8);
    gCube = makeCube();
    gSphere = makeSphere(32, 16);
    gPlane = makePlane();

    // Bufor dla rysowania 2D: (x, y, u, v) na wierzchołek, wypełniany przy każdym wywołaniu
    glGenVertexArrays(1, &gUiVao);
    glBindVertexArray(gUiVao);
    glGenBuffers(1, &gUiVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gUiVbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glEnable(GL_PROGRAM_POINT_SIZE);   // rozmiar cząsteczek ustawia shader geometrii
    return true;
}

// ===========================================================================
// Tekstury
// ===========================================================================
bool loadBMP(const std::string& path, int& width, int& height, std::vector<unsigned char>& rgba) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::vector<unsigned char> d((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (d.size() < 54 || d[0] != 'B' || d[1] != 'M') {
        std::printf("[BLAD] %s nie jest plikiem BMP\n", path.c_str());
        return false;
    }
    uint32_t offset = rd32(&d[10]), headerSize = rd32(&d[14]);
    int32_t w = int32_t(rd32(&d[18])), h = int32_t(rd32(&d[22]));
    uint16_t bpp = rd16(&d[28]);
    uint32_t compression = rd32(&d[30]);
    if ((bpp != 24 && bpp != 32) || (compression != 0 && compression != 3) || w <= 0 || h == 0) {
        std::printf("[BLAD] %s: obslugiwane sa tylko BMP 24/32-bit bez kompresji\n", path.c_str());
        return false;
    }
    bool topDown = h < 0;
    width = w;
    height = std::abs(h);

    // Maski kolorów dla 32-bit (domyślnie BGRA, GIMP zapisuje maski w nagłówku)
    uint32_t mask[4] = {0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000};
    if (bpp == 32 && compression == 3 && d.size() >= 70) {
        mask[0] = rd32(&d[54]);
        mask[1] = rd32(&d[58]);
        mask[2] = rd32(&d[62]);
        mask[3] = headerSize >= 56 ? rd32(&d[66]) : 0;
    }

    size_t stride = (size_t(width) * bpp / 8 + 3) & ~size_t(3);   // wiersze wyrównane do 4 bajtów
    if (offset + stride * height > d.size()) {
        std::printf("[BLAD] %s: plik jest uszkodzony\n", path.c_str());
        return false;
    }
    rgba.assign(size_t(width) * height * 4, 0);
    bool anyAlpha = false;
    for (int y = 0; y < height; ++y) {
        const unsigned char* row = &d[offset + stride * (topDown ? height - 1 - y : y)];
        for (int x = 0; x < width; ++x) {
            unsigned char* o = &rgba[(size_t(y) * width + x) * 4];
            if (bpp == 24) {   // BMP przechowuje BGR - zamiana na RGB (jak w lab2)
                o[0] = row[x * 3 + 2];
                o[1] = row[x * 3 + 1];
                o[2] = row[x * 3 + 0];
                o[3] = 255;
            } else {
                uint32_t px = rd32(row + x * 4);
                for (int c = 0; c < 4; ++c) o[c] = channel(px, mask[c]);
                anyAlpha |= o[3] != 0;
            }
        }
    }
    for (size_t i = 0; i < rgba.size(); i += 4) {
        if (bpp == 32 && !anyAlpha) rgba[i + 3] = 255;   // 32-bit bez kanału alfa
        if (rgba[i] > 250 && rgba[i + 1] < 5 && rgba[i + 2] > 250)   // magenta = przezroczysty
            rgba[i] = rgba[i + 1] = rgba[i + 2] = rgba[i + 3] = 0;
    }
    return true;
}

Texture createTexture(int width, int height, const unsigned char* rgba, bool pixelArt) {
    Texture t;
    t.width = width;
    t.height = height;
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    if (pixelArt) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {   // filtrowanie i powtarzanie jak w lab2 (zad. 2.1) + mipmapy
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    return t;
}

const Texture& getTexture(const std::string& file, bool pixelArt) {
    std::string key = file + (pixelArt ? "#pixel" : "");
    auto it = gTextures.find(key);
    if (it != gTextures.end()) return it->second;
    int w = 0, h = 0;
    std::vector<unsigned char> rgba;
    Texture tex;
    if (loadBMP("data/textures/" + file, w, h, rgba)) {
        tex = createTexture(w, h, rgba.data(), pixelArt);
    } else {
        std::printf("[UWAGA] Brak tekstury data/textures/%s - uzyto szachownicy\n", file.c_str());
        tex = makeChecker();
    }
    return gTextures[key] = tex;
}

// ===========================================================================
// Siatki
// ===========================================================================
Mesh createMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices) {
    Mesh m;
    if (vertices.empty() || indices.empty()) return m;
    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);

    glGenBuffers(1, &m.vbo);   // sprzętowy bufor wierzchołków
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m.ibo);   // sprzętowy bufor indeksów
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    m.indexCount = GLsizei(indices.size());
    return m;
}

// Loader OBJ z labów (lab3, lab6), rozszerzony: ściany trójkątne i wielokątne, formaty
// v/vt/vn, v//vn, v/vt, v. Indeksowanie wierzchołków przez std::map jak buildIndexedMeshMap() z lab3.
Mesh loadOBJ(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "r");
    if (!file) {
        std::printf("[BLAD] Nie mozna otworzyc modelu %s\n", path.c_str());
        return {};
    }
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::vector<Vertex> vertices;
    std::vector<unsigned> indices;
    std::map<std::tuple<int, int, int>, unsigned> known;   // (v, vt, vn) -> indeks wierzchołka
    int faceNumber = 0;
    bool ok = true;
    char line[1024];

    while (ok && std::fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
            glm::vec3 p(0.0f);
            std::sscanf(line + 2, "%f %f %f", &p.x, &p.y, &p.z);
            positions.push_back(p);
        } else if (line[0] == 'v' && line[1] == 't') {
            glm::vec2 t(0.0f);
            std::sscanf(line + 3, "%f %f", &t.x, &t.y);   // bez odwracania V (w labach było pod DDS)
            uvs.push_back(t);
        } else if (line[0] == 'v' && line[1] == 'n') {
            glm::vec3 n(0.0f);
            std::sscanf(line + 3, "%f %f %f", &n.x, &n.y, &n.z);
            normals.push_back(n);
        } else if (line[0] == 'f' && line[1] == ' ') {
            int corner[32][3];
            int count = 0;
            for (char* tok = std::strtok(line + 2, " \t\r\n"); tok && count < 32; tok = std::strtok(nullptr, " \t\r\n")) {
                char* p = tok;
                int v = std::strtol(p, &p, 10), t = 0, n = 0;
                if (*p == '/') {
                    ++p;
                    if (*p != '/') t = std::strtol(p, &p, 10);
                    if (*p == '/') n = std::strtol(p + 1, &p, 10);
                }
                // indeksy ujemne liczą się od końca listy
                if (v < 0) v += int(positions.size()) + 1;
                if (t < 0) t += int(uvs.size()) + 1;
                if (n < 0) n += int(normals.size()) + 1;
                if (v < 1 || v > int(positions.size()) || t > int(uvs.size()) || n > int(normals.size())) {
                    std::printf("[BLAD] %s: zly indeks w scianie\n", path.c_str());
                    ok = false;
                    break;
                }
                corner[count][0] = v;
                corner[count][1] = t;
                corner[count][2] = n;
                ++count;
            }
            ++faceNumber;
            for (int k = 1; ok && k + 1 < count; ++k) {   // wielokąt -> wachlarz trójkątów
                const int* tri[3] = {corner[0], corner[k], corner[k + 1]};
                glm::vec3 faceNormal = glm::cross(positions[tri[1][0] - 1] - positions[tri[0][0] - 1],
                                                  positions[tri[2][0] - 1] - positions[tri[0][0] - 1]);
                faceNormal = glm::length(faceNormal) > 0 ? glm::normalize(faceNormal) : glm::vec3(0, 1, 0);
                for (const int* c : tri) {
                    // bez normalnej w pliku: normalna ściany, wierzchołek nie jest współdzielony
                    auto key = std::make_tuple(c[0], c[1], c[2] ? c[2] : -faceNumber);
                    auto it = known.find(key);
                    if (it != known.end()) {
                        indices.push_back(it->second);
                        continue;
                    }
                    Vertex vert;
                    vert.pos = positions[c[0] - 1];
                    vert.uv = c[1] ? uvs[c[1] - 1] : glm::vec2(0.0f);
                    vert.normal = c[2] ? normals[c[2] - 1] : faceNormal;
                    known[key] = unsigned(vertices.size());
                    indices.push_back(unsigned(vertices.size()));
                    vertices.push_back(vert);
                }
            }
        }
    }
    std::fclose(file);
    if (!ok || indices.empty()) return {};
    return createMesh(vertices, indices);
}

const Mesh& getModel(const std::string& file) {
    auto it = gModels.find(file);
    if (it != gModels.end()) return it->second;
    Mesh mesh = loadOBJ("data/models/" + file);
    if (mesh.indexCount == 0) {
        std::printf("[UWAGA] Model data/models/%s niedostepny - uzyto szescianu\n", file.c_str());
        mesh = gCube;
    }
    return gModels[file] = mesh;
}

const Mesh& quadMesh() { return gQuad; }
const Mesh& cubeMesh() { return gCube; }

// ===========================================================================
// Scena 3D
// ===========================================================================
void setWireframe(bool on) { gFrame.wireframe = on; }

void beginScene(const Camera& cam, const SceneSettings& s, int width, int height) {
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);   // ściany widoczne z obu stron (płoty, liście, modele z odwróconymi ścianami)
    glClearColor(s.fogColor.r, s.fogColor.g, s.fogColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, gFrame.wireframe ? GL_LINE : GL_FILL);

    gFrame.view = cam.view();
    gFrame.proj = glm::perspective(glm::radians(70.0f), float(width) / float(std::max(height, 1)), 0.1f, 500.0f);
    gFrame.camPos = cam.pos;
    gFrame.time = s.time;

    // Światła punktowe spakowane do tablic dla shadera
    int count = std::min(int(s.lights.size()), MAX_LIGHTS);
    glm::vec3 pos[MAX_LIGHTS], color[MAX_LIGHTS];
    float range[MAX_LIGHTS];
    for (int i = 0; i < count; ++i) {
        pos[i] = s.lights[i].pos;
        color[i] = s.lights[i].color;
        range[i] = s.lights[i].range;
    }
    glm::vec3 sunDir = glm::normalize(s.sunDir);

    for (WorldProgram* p : {&gWorld, &gExplode}) {
        useProgram(p->id);
        glUniformMatrix4fv(p->view, 1, GL_FALSE, glm::value_ptr(gFrame.view));
        glUniformMatrix4fv(p->proj, 1, GL_FALSE, glm::value_ptr(gFrame.proj));
        glUniform3f(p->camPos, cam.pos.x, cam.pos.y, cam.pos.z);
        glUniform3f(p->sunDir, sunDir.x, sunDir.y, sunDir.z);
        glUniform3f(p->sunColor, s.sunColor.r, s.sunColor.g, s.sunColor.b);
        glUniform3f(p->ambient, s.ambient.r, s.ambient.g, s.ambient.b);
        glUniform3f(p->fogColor, s.fogColor.r, s.fogColor.g, s.fogColor.b);
        glUniform1f(p->fogDensity, s.fogDensity);
        glUniform1f(p->time, s.time);
        glUniform1i(p->lightCount, count);
        if (count > 0) {
            glUniform3fv(p->lightPos, count, glm::value_ptr(pos[0]));
            glUniform3fv(p->lightColor, count, glm::value_ptr(color[0]));
            glUniform1fv(p->lightRange, count, range);
        }
    }
}

void drawMesh(const Mesh& mesh, const Texture& tex, const glm::mat4& model, glm::vec3 tint, float specular, float emissive) {
    if (!mesh.indexCount) return;
    setObject(gWorld, MODE_MODEL, model, glm::vec4(0, 0, 1, 1), tint, specular, emissive);
    bindTexture(0, tex);
    drawElements(mesh);
}

void drawTerrain(const Mesh& mesh, const Texture& splat, const Texture* const layers[4], float tiling) {
    setObject(gWorld, MODE_TERRAIN, glm::mat4(1.0f), glm::vec4(0, 0, 1, 1), glm::vec3(1.0f), 0.05f);
    glUniform1f(gWorld.tiling, tiling);
    bindTexture(0, splat);
    for (int i = 0; i < 4; ++i) bindTexture(i + 1, *layers[i]);
    drawElements(mesh);
    glActiveTexture(GL_TEXTURE0);
}

void drawSprite(const Texture& sheet, glm::vec3 feet, glm::vec2 size, int col, int row, int cols, int rows, glm::vec3 tint) {
    setObject(gWorld, MODE_SPRITE, spriteMatrix(feet, size), frameRect(sheet, col, row, cols, rows), tint, 0.0f);
    bindTexture(0, sheet);
    drawElements(gQuad);
}

void drawSpriteExplode(const Texture& sheet, glm::vec3 feet, glm::vec2 size, int col, int row, int cols, int rows, float t) {
    setObject(gExplode, MODE_SPRITE, spriteMatrix(feet, size), frameRect(sheet, col, row, cols, rows), glm::vec3(1.0f), 0.0f);
    glUniform1f(gExplode.explodeT, t);
    glUniform3f(gExplode.explodeOrigin, feet.x, feet.y + size.y * 0.5f, feet.z);
    bindTexture(0, sheet);
    drawElements(gSpriteGrid);   // gęsta siatka -> więcej cząsteczek
}

void drawSky(const Texture& sky) {
    glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), gFrame.camPos), glm::vec3(300.0f));
    setObject(gWorld, MODE_SKY, model, glm::vec4(0, 0, 1, 1), glm::vec3(1.0f), 0.0f);
    bindTexture(0, sky);
    glDepthMask(GL_FALSE);   // niebo zawsze za wszystkim
    drawElements(gSphere);
    glDepthMask(GL_TRUE);
}

void drawWater(glm::vec3 center, float size) {
    glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), center), glm::vec3(size, 1.0f, size));
    setObject(gWorld, MODE_WATER, model, glm::vec4(0, 0, 1, 1), glm::vec3(1.0f), 0.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    drawElements(gPlane);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void drawLightMarker(glm::vec3 pos, glm::vec3 color, float size) {
    glDisable(GL_DEPTH_TEST);   // znacznik debugowy - widoczny także wewnątrz latarni i za ścianą
    bindTexture(0, gWhite);
    // cienka czarna obwódka (nieco większa kula pod spodem) - znacznik widać także na tle ognia
    glm::mat4 sphere = glm::translate(glm::mat4(1.0f), pos);
    setObject(gWorld, MODE_UNLIT, glm::scale(sphere, glm::vec3(size * 1.12f)), glm::vec4(0, 0, 1, 1), glm::vec3(0.0f), 0.0f);
    drawElements(gSphere);
    setObject(gWorld, MODE_UNLIT, glm::scale(sphere, glm::vec3(size)), glm::vec4(0, 0, 1, 1), color, 0.0f);
    drawElements(gSphere);
    glEnable(GL_DEPTH_TEST);
}

// ===========================================================================
// 2D
// ===========================================================================
void begin2D(int width, int height) {
    glViewport(0, 0, width, height);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);   // UI zawsze wypełnione, także w trybie siatki
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    useProgram(gUi.id);
    glUniform2f(gUi.screen, float(width), float(height));
}

void drawRect(float x, float y, float w, float h, glm::vec4 color) {
    std::vector<float> d;
    pushQuad(d, x, y, w, h, glm::vec4(0, 0, 1, 1));
    drawUiTriangles(d, gWhite.id, color);
}

void drawImage(const Texture& tex, float x, float y, float w, float h, glm::vec4 uvRect, glm::vec4 color) {
    std::vector<float> d;
    pushQuad(d, x, y, w, h, uvRect);
    drawUiTriangles(d, tex.id ? tex.id : gWhite.id, color);
}

void drawText(const std::string& text, float x, float y, float scale, glm::vec4 color) {
    const float cw = FONT_CELL_W * scale, ch = FONT_CELL_H * scale;
    const float atlasW = 16.0f * FONT_CELL_W, atlasH = 8.0f * FONT_CELL_H;
    std::vector<float> d;
    float cx = x, cy = y;
    for (size_t i = 0; i < text.size();) {
        unsigned cp = nextCodepoint(text, i);
        if (cp == '\n') {
            cx = x;
            cy += ch + 2 * scale;
            continue;
        }
        int g = glyphIndex(cp);
        float u = (g % 16) * FONT_CELL_W / atlasW;
        float v = 1.0f - ((g / 16) + 1) * FONT_CELL_H / atlasH;
        pushQuad(d, cx, cy, cw, ch, glm::vec4(u, v, FONT_CELL_W / atlasW, FONT_CELL_H / atlasH));
        cx += cw;
    }
    drawUiTriangles(d, gFont.id, color);
}

float textWidth(const std::string& text, float scale) {
    int longest = 0, current = 0;
    for (size_t i = 0; i < text.size();) {
        if (nextCodepoint(text, i) == '\n') current = 0;
        else longest = std::max(longest, ++current);
    }
    return longest * FONT_CELL_W * scale;
}

float drawTextWrapped(const std::string& text, float x, float y, float maxWidth, float scale, glm::vec4 color) {
    const float lineHeight = (FONT_CELL_H + 2) * scale;
    const int maxChars = std::max(1, int(maxWidth / (FONT_CELL_W * scale)));
    std::string result, line, word;
    auto length = [](const std::string& s) {   // liczba znaków (nie bajtów) w UTF-8
        int n = 0;
        for (size_t i = 0; i < s.size();) { nextCodepoint(s, i); ++n; }
        return n;
    };
    auto flushWord = [&]() {
        if (word.empty()) return;
        if (!line.empty() && length(line) + 1 + length(word) > maxChars) {
            result += line + "\n";
            line.clear();
        }
        line += (line.empty() ? "" : " ") + word;
        word.clear();
    };
    int lines = 1;
    for (char c : text) {
        if (c == ' ') flushWord();
        else if (c == '\n') {
            flushWord();
            result += line + "\n";
            line.clear();
        } else word += c;
    }
    flushWord();
    result += line;
    for (char c : result) lines += c == '\n';
    drawText(result, x, y, scale, color);
    return lines * lineHeight;
}

// ===========================================================================
// Zrzut ekranu (BMP 24-bit; kolejność wierszy OpenGL = kolejność BMP, od dołu)
// ===========================================================================
bool saveScreenshot(const std::string& path, int width, int height) {
    std::vector<unsigned char> px(size_t(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, px.data());

    std::error_code ec;
    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    const uint32_t stride = (uint32_t(width) * 3 + 3) & ~3u, dataSize = stride * height;
    unsigned char header[54] = {'B', 'M'};
    auto put32 = [&](int at, uint32_t v) { for (int i = 0; i < 4; ++i) header[at + i] = (v >> (8 * i)) & 255; };
    put32(2, 54 + dataSize);
    put32(10, 54);
    put32(14, 40);
    put32(18, uint32_t(width));
    put32(22, uint32_t(height));
    header[26] = 1;    // płaszczyzny
    header[28] = 24;   // bity na piksel
    put32(34, dataSize);
    f.write((const char*)header, 54);

    std::vector<unsigned char> row(stride, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {   // RGB -> BGR
            const unsigned char* p = &px[(size_t(y) * width + x) * 3];
            row[x * 3 + 0] = p[2];
            row[x * 3 + 1] = p[1];
            row[x * 3 + 2] = p[0];
        }
        f.write((const char*)row.data(), stride);
    }
    return true;
}
