// world.h - lokacje gry: wczytywanie map z data/maps/, teren z heightmapy (dolina),
// krypta z siatki ASCII, obiekty (propy), światła, strefy, przejścia, punkty startowe
// postaci (potwory, NPC, przedmioty - używane przez moduły gry) oraz kolizje.
#pragma once

#include "render.h"
#include <string>
#include <vector>

struct Prop {                       // obiekt OBJ na mapie (drzewo, dom, skała...)
    const Mesh* mesh = nullptr;
    const Texture* texture = nullptr;
    glm::mat4 model{1.0f};
    glm::vec2 pos{0.0f};            // pozycja (x, z)
    float radius = 0.0f;            // kolizja okrągła: promień (0 = można przejść)
    glm::vec2 halfSize{0.0f};       // kolizja prostokątna: połowa szerokości (x) i głębokości (z)
    float angle = 0.0f;             // obrót prostokąta kolizji (radiany, jak model)
    float emissive = 0.0f;          // świecenie własne (ogień, latarnia)
};

struct WorldLight { glm::vec3 pos; glm::vec3 color; float range; bool flicker; };
struct Zone { std::string name; glm::vec2 min, max; };

struct Portal {                     // przejście do innej lokacji (klawisz E)
    glm::vec2 pos;
    float radius;
    std::string target;             // plik mapy docelowej, np. "krypta.txt"
    glm::vec2 targetPos{0.0f};      // gdzie pojawia się gracz
    float targetYaw = 0.0f;
    bool toStart = false;           // true = na punkcie startowym mapy docelowej
};

struct Spawn {                      // miejsce startowe postaci lub przedmiotu
    std::string kind;               // "monster", "npc" albo "item"
    std::string type;               // identyfikator z tabel gry, np. "szkielet", "mikstura_mala"
    glm::vec2 pos;
    float yaw;
};

struct Location {
    std::string file;               // nazwa pliku mapy, np. "dolina.txt"
    std::string title;              // nazwa wyświetlana graczowi
    bool outdoor = true;            // true = teren z heightmapy, false = krypta z siatki
    glm::vec2 start{10.0f};
    float startYaw = 0.0f;
    glm::vec2 boundsMin{0.0f}, boundsMax{0.0f};   // obszar do chodzenia (0 = cała mapa)
    SceneSettings scene;            // słońce, światło otoczenia, mgła

    std::vector<Prop> props;
    std::vector<WorldLight> lights;
    std::vector<Zone> zones;
    std::vector<Portal> portals;
    std::vector<Spawn> spawns;

    // --- teren (dolina) ---
    float size = 128.0f;            // bok mapy w metrach
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
    float cell = 3.0f, ceiling = 3.5f;
    Mesh wallMesh, floorMesh, ceilingMesh;
    const Texture* wallTex = nullptr;
    const Texture* floorTex = nullptr;
    const Texture* ceilingTex = nullptr;

    bool load(const std::string& mapFile);
    float groundHeight(float x, float z) const;
    // Ruch z kolizjami: ściany krypty, granice mapy, zbyt strome zbocza, obiekty (okręgi i prostokąty).
    // Zwraca nową pozycję stóp (y = wysokość gruntu).
    glm::vec3 moveAndCollide(glm::vec3 pos, glm::vec3 delta, float radius) const;
    bool lineOfSight(glm::vec3 from, glm::vec3 to) const;    // czy ściana nie zasłania
    std::string zoneAt(float x, float z) const;              // "" = poza strefami
    const Portal* portalNear(glm::vec3 pos, float reach) const;
    // Najbliższe światła (z migotaniem pochodni) - maksymalnie maxCount
    void collectLights(glm::vec3 center, float time, int maxCount, std::vector<PointLight>& out) const;

    void draw() const;              // niebo, teren albo krypta, obiekty
    void drawTransparent() const;   // woda (po wszystkich nieprzezroczystych obiektach)
    void drawLightMarkers() const;  // F4: znaczniki świateł widoczne przez ściany

private:
    bool loadTerrain(const std::string& heightmap, const std::string& splatFile, const std::string layerFiles[4]);
    void buildDungeon();
    bool isWall(int i, int j) const;
    bool blocked(glm::vec2 p, float radius) const;
    void addProp(const std::string& model, const std::string& tex, glm::vec2 pos, float yaw, float scale,
                 const std::string& collision, float emissive);
    void addWallTorch(int i, int j);
};
