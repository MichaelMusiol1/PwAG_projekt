// render.h - cała grafika gry (OpenGL 3.3 core, bez stałego potoku):
// ładowanie funkcji GL, shadery, tekstury BMP, siatki (OBJ -> VAO/VBO/IBO), kamera,
// światła, teren, sprite'y, niebo, woda, efekt rozpadu, rysowanie 2D i tekstu, zrzut ekranu.
#pragma once

#include <GL/freeglut.h>   // okno + GL/gl.h (na Windowsie także windows.h)
#include <GL/glext.h>      // typy i stałe OpenGL > 1.1 (Windows: kopia z labów w external/win32)

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Funkcje OpenGL > 1.1. Na Windowsie trzeba je pobrać ręcznie przez wglGetProcAddress
// (jak extensionSetup() z labów), na Linuxie eksportuje je libGL.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Typy
// ---------------------------------------------------------------------------
struct Vertex  { glm::vec3 pos; glm::vec3 normal; glm::vec2 uv; };
struct Texture { GLuint id = 0; int width = 0, height = 0; };
struct Mesh    { GLuint vao = 0, vbo = 0, ibo = 0; GLsizei indexCount = 0; };

struct PointLight { glm::vec3 pos; glm::vec3 color; float range; };

// Kamera pierwszoosobowa. yaw: 0 = patrzy w -Z, 90 = w +X (stopnie); pitch: w górę > 0.
struct Camera {
    glm::vec3 pos{0.0f};
    float yaw = 0.0f, pitch = 0.0f;
    glm::vec3 forward() const;      // kierunek patrzenia (z pitch)
    glm::vec3 flatForward() const;  // kierunek w poziomie (bez pitch)
    glm::vec3 right() const;
    glm::mat4 view() const;
};
glm::vec3 yawToDir(float yawDeg);   // kierunek poziomy dla kąta yaw
float dirToYaw(glm::vec3 dir);      // odwrotność yawToDir

// Ustawienia oświetlenia i mgły dla jednej klatki
struct SceneSettings {
    glm::vec3 sunDir{-0.4f, -1.0f, -0.3f};   // kierunek padania światła słońca
    glm::vec3 sunColor{1.0f, 0.95f, 0.85f};  // (0,0,0) = brak słońca, np. w krypcie
    glm::vec3 ambient{0.25f};
    glm::vec3 fogColor{0.6f, 0.7f, 0.85f};
    float fogDensity = 0.01f;
    float time = 0.0f;                       // sekundy od startu (woda, efekty)
    std::vector<PointLight> lights;          // maksymalnie MAX_LIGHTS najbliższych
};
const int MAX_LIGHTS = 16;

// ---------------------------------------------------------------------------
// Inicjalizacja, zasoby
// ---------------------------------------------------------------------------
bool initRenderer();   // po utworzeniu okna: funkcje GL, shadery, czcionka, siatki pomocnicze

// Ładowanie BMP do pamięci (RGBA, wiersz 0 = dół obrazka). Kolor magenta (255,0,255) -> przezroczysty.
bool loadBMP(const std::string& path, int& width, int& height, std::vector<unsigned char>& rgba);
// pixelArt: filtrowanie NEAREST bez mipmap (sprite'y, ikony); inaczej LINEAR + mipmapy + powtarzanie
Texture createTexture(int width, int height, const unsigned char* rgba, bool pixelArt);
Mesh createMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices);
Mesh loadOBJ(const std::string& path);   // przy błędzie zwraca pustą siatkę (indexCount = 0)

// Zasoby z pamięcią podręczną: ten sam plik ładowany tylko raz. Nazwy względem
// data/textures/ i data/models/. Brak pliku -> szachownica / sześcian + komunikat.
const Texture& getTexture(const std::string& file, bool pixelArt = false);
const Mesh& getModel(const std::string& file);
const Mesh& quadMesh();     // kwadrat 1x1 w płaszczyźnie XY: x -0.5..0.5, y 0..1, normalna +Z
const Mesh& cubeMesh();     // sześcian 1x1x1 wokół środka

// ---------------------------------------------------------------------------
// Scena 3D (wywołania między beginScene a begin2D)
// ---------------------------------------------------------------------------
void setWireframe(bool on);   // podgląd siatki trójkątów (F2)
void beginScene(const Camera& cam, const SceneSettings& settings, int width, int height);
// emissive: świecenie własne (0 = brak), np. ogień, latarnia
void drawMesh(const Mesh& mesh, const Texture& tex, const glm::mat4& model,
              glm::vec3 tint = glm::vec3(1.0f), float specular = 0.25f, float emissive = 0.0f);
// Teren: splatmapa (R,G,B) miesza 4 warstwy; layers[0] tam, gdzie R+G+B = 0. tiling = ile razy
// warstwy powtarzają się na całym terenie.
void drawTerrain(const Mesh& mesh, const Texture& splat, const Texture* const layers[4], float tiling);
// Sprite (billboard obracany wokół osi Y). Arkusz cols x rows klatek; wiersz 0 = górny wiersz obrazka.
void drawSprite(const Texture& sheet, glm::vec3 feet, glm::vec2 size, int col, int row,
                int cols, int rows, glm::vec3 tint = glm::vec3(1.0f));
// To samo, ale rozpadające się na cząsteczki (shader geometrii z lab6). t: 0..1
void drawSpriteExplode(const Texture& sheet, glm::vec3 feet, glm::vec2 size, int col, int row,
                       int cols, int rows, float t);
void drawSky(const Texture& sky);                        // kopuła nieba wokół kamery
void drawWater(glm::vec3 center, float size);            // przezroczysta tafla z falami (lab4)
// F4: kulka z cienką czarną obwódką w miejscu źródła światła; widoczna także przez ściany
void drawLightMarker(glm::vec3 pos, glm::vec3 color, float size);

// ---------------------------------------------------------------------------
// 2D - interfejs w pikselach okna (0,0 = lewy górny róg)
// ---------------------------------------------------------------------------
void begin2D(int width, int height);
void drawRect(float x, float y, float w, float h, glm::vec4 color);
void drawImage(const Texture& tex, float x, float y, float w, float h,
               glm::vec4 uvRect = glm::vec4(0, 0, 1, 1), glm::vec4 color = glm::vec4(1.0f));
// Tekst UTF-8 (z polskimi literami). Znak ma 8*scale x 10*scale pikseli. '\n' = nowa linia.
void drawText(const std::string& text, float x, float y, float scale, glm::vec4 color);
float textWidth(const std::string& text, float scale);
// Tekst zawijany do szerokości maxWidth; zwraca wysokość zajętą przez tekst
float drawTextWrapped(const std::string& text, float x, float y, float maxWidth, float scale, glm::vec4 color);

// Zapis zawartości okna do pliku BMP (wywołać przed glutSwapBuffers)
bool saveScreenshot(const std::string& path, int width, int height);
