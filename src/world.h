#pragma once

#include "render.h"
#include <string>
#include <vector>

struct Prop {                       // obiekt OBJ na mapie (drzewo, dom, kolumna...)
    const Mesh* mesh = nullptr;
    const Texture* texture = nullptr;
    glm::mat4 model{1.0f};
    glm::vec2 pos{0.0f};            // (x, z), nie (x, y) bo OpenGL
    float radius = 0.0f;            // kolizja okrągła, 0 = można przejść
    glm::vec2 halfSize{0.0f};       // kolizja prostokątna: połowa szerokości (x) i głębokości (z)
    float angle = 0.0f;             // radiany, jak model
    float emissive = 0.0f;
};

struct WorldLight { glm::vec3 pos; glm::vec3 color; float range; bool flicker; };
struct Zone { std::string name; glm::vec2 min, max; };

struct Portal {
    glm::vec2 pos;
    float radius;
    std::string target;             // plik mapy, np. "krypta.txt"
    glm::vec2 targetPos{0.0f};
    float targetYaw = 0.0f;
    bool toStart = false;           // true = targetPos ignorowane, gracz ląduje na starcie mapy
};

struct Spawn {                      // tylko miejsce z mapy - postacie i przedmioty tworzą moduły etapów 2-4
    std::string kind;               // "monster", "npc" albo "item"
    std::string type;               // id z tabel gry, np. "szkielet", "mikstura_mala"
    glm::vec2 pos;                  // na razie nie działa i raczej będzie zmieniane ale wolę żeby już było
    float yaw;
};

struct Location {
    std::string file;
    std::string title;
    bool outdoor = true;            // false = krypta z siatki znaków
    glm::vec2 start{10.0f};
    float startYaw = 0.0f;
    glm::vec2 boundsMin{0.0f}, boundsMax{0.0f};   // same zera = cała mapa
    SceneSettings scene;

    std::vector<Prop> props;
    std::vector<WorldLight> lights;
    std::vector<Zone> zones;
    std::vector<Portal> portals;
    std::vector<Spawn> spawns;

    // --- teren (dolina) ---
    float size = 128.0f;            // bok mapy w metrach (krypta też to ustawia)
    float maxHeight = 10.0f;
    int gridW = 0, gridH = 0;
    float spacing = 1.0f;
    std::vector<float> heights;     // wysokości wierzchołków, wiersz 0 = północ (z = 0)
    Mesh terrainMesh;
    const Texture* splat = nullptr;
    const Texture* layers[4] = {};
    const Texture* sky = nullptr;
    bool hasWater = false;
    float waterLevel = 0.0f;

    // --- krypta ---
    std::vector<std::string> grid;  // wiersz 0 = północ; komórka (kolumna i, wiersz j)
    float cell = 3.0f, ceiling = 3.5f;   // metry jakby co
    Mesh wallMesh, floorMesh, ceilingMesh;
    const Texture* wallTex = nullptr;
    const Texture* floorTex = nullptr;
    const Texture* ceilingTex = nullptr;

    bool load(const std::string& mapFile);
    float groundHeight(float x, float z) const;
    // Ściany krypty, granice mapy, zbyt strome zbocza i obiekty. Zwraca stopy (y = wysokość gruntu).
    glm::vec3 moveAndCollide(glm::vec3 pos, glm::vec3 delta, float radius) const;
    bool lineOfSight(glm::vec3 from, glm::vec3 to) const;    // tylko ściany krypty, na zewnątrz zawsze true
    std::string zoneAt(float x, float z) const;              // "" = poza strefami
    const Portal* portalNear(glm::vec3 pos, float reach) const;
    void collectLights(glm::vec3 center, float time, int maxCount, std::vector<PointLight>& out) const;

    void draw() const;
    void drawTransparent() const;   // po wszystkich nieprzezroczystych - woda miesza się z tym, co już narysowane
    void drawLightMarkers() const;

private:
    bool loadTerrain(const std::string& heightmap, const std::string& splatFile, const std::string layerFiles[4]);
    void buildDungeon();
    bool isWall(int i, int j) const;
    bool blocked(glm::vec2 p, float radius) const;
    void addProp(const std::string& model, const std::string& tex, glm::vec2 pos, float yaw, float scale,
                 const std::string& collision, float emissive);
    void addWallTorch(int i, int j);
};
