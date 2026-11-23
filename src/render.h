#pragma once

#include <GL/freeglut.h>   // na Windowsie wciąga windows.h, a z nim makra near i far
#include <GL/glext.h>      // na Windowsie kopia z labów w external/win32 (MSVC nie ma swojej)

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

// Funkcje OpenGL > 1.1. Na Windowsie trzeba je pobrać ręcznie przez wglGetProcAddress
// (jak extensionSetup() z labów), na Linuxie eksportuje je libGL.
// GLEW albo glad załatwiłyby to jedną linijką, ale zakaz middleware (TT-TT)
// Nowa funkcja GL = nowa linijka tutaj. Na Linuxie działa i bez tego, więc brak wyjdzie
// dopiero na Windowsie, najpewniej w dniu pokazu.
#define GL_FUNCTION_LIST(X) \
    X(PFNGLCREATESHADERPROC,            glCreateShader)            \
    X(PFNGLSHADERSOURCEPROC,            glShaderSource)            \
    X(PFNGLCOMPILESHADERPROC,           glCompileShader)           \
    X(PFNGLGETSHADERIVPROC,             glGetShaderiv)             \
    X(PFNGLGETSHADERINFOLOGPROC,        glGetShaderInfoLog)        \
    X(PFNGLDELETESHADERPROC,            glDeleteShader)            \
    X(PFNGLCREATEPROGRAMPROC,           glCreateProgram)           \
    X(PFNGLATTACHSHADERPROC,            glAttachShader)            \
    X(PFNGLLINKPROGRAMPROC,             glLinkProgram)             \
    X(PFNGLGETPROGRAMIVPROC,            glGetProgramiv)            \
    X(PFNGLGETPROGRAMINFOLOGPROC,       glGetProgramInfoLog)       \
    X(PFNGLUSEPROGRAMPROC,              glUseProgram)              \
    X(PFNGLGETUNIFORMLOCATIONPROC,      glGetUniformLocation)      \
    X(PFNGLUNIFORM1IPROC,               glUniform1i)               \
    X(PFNGLUNIFORM1FPROC,               glUniform1f)               \
    X(PFNGLUNIFORM2FPROC,               glUniform2f)               \
    X(PFNGLUNIFORM3FPROC,               glUniform3f)               \
    X(PFNGLUNIFORM4FPROC,               glUniform4f)               \
    X(PFNGLUNIFORM1FVPROC,              glUniform1fv)              \
    X(PFNGLUNIFORM3FVPROC,              glUniform3fv)              \
    X(PFNGLUNIFORMMATRIX4FVPROC,        glUniformMatrix4fv)        \
    X(PFNGLGENBUFFERSPROC,              glGenBuffers)              \
    X(PFNGLBINDBUFFERPROC,              glBindBuffer)              \
    X(PFNGLBUFFERDATAPROC,              glBufferData)              \
    X(PFNGLDELETEBUFFERSPROC,           glDeleteBuffers)           \
    X(PFNGLGENVERTEXARRAYSPROC,         glGenVertexArrays)         \
    X(PFNGLBINDVERTEXARRAYPROC,         glBindVertexArray)         \
    X(PFNGLDELETEVERTEXARRAYSPROC,      glDeleteVertexArrays)      \
    X(PFNGLVERTEXATTRIBPOINTERPROC,     glVertexAttribPointer)     \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLACTIVETEXTUREPROC,           glActiveTexture)           \
    X(PFNGLGENERATEMIPMAPPROC,          glGenerateMipmap)

#ifdef _WIN32
#define X(type, name) extern type name;
GL_FUNCTION_LIST(X)
#undef X
#endif

struct Vertex  { glm::vec3 pos; glm::vec3 normal; glm::vec2 uv; };
struct Texture { GLuint id = 0; int width = 0, height = 0; };
struct Mesh    { GLuint vao = 0, vbo = 0, ibo = 0; GLsizei indexCount = 0; };

struct PointLight { glm::vec3 pos; glm::vec3 color; float range; };

// yaw w stopniach: 0 = patrzy w -Z (północ), 90 = w +X (wschód); pitch > 0 = w górę
struct Camera {
    glm::vec3 pos{0.0f};
    float yaw = 0.0f, pitch = 0.0f;
    glm::vec3 forward() const;
    glm::vec3 flatForward() const;
    glm::vec3 right() const;
    glm::mat4 view() const;
};
glm::vec3 yawToDir(float yawDeg);
float dirToYaw(glm::vec3 dir);

struct SceneSettings {
    glm::vec3 sunDir{-0.4f, -1.0f, -0.3f};   // kierunek, w którym lecą promienie (od słońca)
    glm::vec3 sunColor{1.0f, 0.95f, 0.85f};  // (0,0,0) = brak słońca
    glm::vec3 ambient{0.25f};
    glm::vec3 fogColor{0.6f, 0.7f, 0.85f};
    float fogDensity = 0.01f;
    float time = 0.0f;
    std::vector<PointLight> lights;          // ponad MAX_LIGHTS są pomijane
};
const int MAX_LIGHTS = 16;                   // musi się zgadzać z MAX_LIGHTS w world.frag

bool initRenderer();   // dopiero po utworzeniu okna - bez kontekstu GL nic nie zadziała

// RGBA, wiersz 0 = dół obrazka. Magenta (255,0,255) staje się przezroczysta.
bool loadBMP(const std::string& path, int& width, int& height, std::vector<unsigned char>& rgba);
// pixelArt: NEAREST bez mipmap (sprite'y, ikony); inaczej LINEAR + mipmapy + powtarzanie
Texture createTexture(int width, int height, const unsigned char* rgba, bool pixelArt);
Mesh createMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices);
Mesh loadOBJ(const std::string& path);   // przy błędzie pusta siatka (indexCount = 0)

// Każdy plik ładowany tylko raz. Nazwy względem data/textures/ i data/models/.
// Brak pliku: szachownica / sześcian i komunikat w konsoli.
const Texture& getTexture(const std::string& file, bool pixelArt = false);
const Mesh& getModel(const std::string& file);
const Mesh& quadMesh();     // 1x1 w płaszczyźnie XY: x -0.5..0.5, y 0..1, normalna +Z
const Mesh& cubeMesh();     // 1x1x1 wokół środka

// --- Scena 3D: wywołania między beginScene a begin2D ---
void setWireframe(bool on);
void beginScene(const Camera& cam, const SceneSettings& settings, int width, int height);
void drawMesh(const Mesh& mesh, const Texture& tex, const glm::mat4& model,
              glm::vec3 tint = glm::vec3(1.0f), float specular = 0.25f, float emissive = 0.0f);
// Splatmapa: R, G, B = udział layers[1], [2], [3], reszta to layers[0].
// tiling = ile razy warstwy powtarzają się na całym terenie.
void drawTerrain(const Mesh& mesh, const Texture& splat, const Texture* const layers[4], float tiling);
// Billboard obracany wokół osi Y. Arkusz cols x rows klatek, wiersz 0 = górny wiersz obrazka.
void drawSprite(const Texture& sheet, glm::vec3 feet, glm::vec2 size, int col, int row,
                int cols, int rows, glm::vec3 tint = glm::vec3(1.0f));
// To samo, ale rozpada się na cząsteczki. t: 0..1
void drawSpriteExplode(const Texture& sheet, glm::vec3 feet, glm::vec2 size, int col, int row,
                       int cols, int rows, float t);
void drawSky(const Texture& sky);
void drawWater(glm::vec3 center, float size);
void drawLightMarker(glm::vec3 pos, glm::vec3 color, float size);

// --- 2D: piksele okna, (0,0) = lewy górny róg ---
void begin2D(int width, int height);
void drawRect(float x, float y, float w, float h, glm::vec4 color);
void drawImage(const Texture& tex, float x, float y, float w, float h,
               glm::vec4 uvRect = glm::vec4(0, 0, 1, 1), glm::vec4 color = glm::vec4(1.0f));
// UTF-8 z polskimi literami. Znak ma 8*scale x 10*scale pikseli.
void drawText(const std::string& text, float x, float y, float scale, glm::vec4 color);
float textWidth(const std::string& text, float scale);
// zwraca wysokość zajętą przez tekst
float drawTextWrapped(const std::string& text, float x, float y, float maxWidth, float scale, glm::vec4 color);

bool saveScreenshot(const std::string& path, int width, int height);   // przed glutSwapBuffers
