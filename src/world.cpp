// Opis komend map jest na początku każdego pliku w data/maps/.
#include "world.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

// Znaki siatki krypty oznaczające postacie i przedmioty powinno działać (przynajmniej mam nadzieję)
struct LegendEntry { char symbol; const char* kind; const char* type; };
const LegendEntry LEGEND[] = {
    {'r', "monster", "szczur"},        {'g', "monster", "goblin"},
    {'s', "monster", "szkielet"},      {'B', "monster", "boss"},
    {'m', "item", "mikstura_mala"},    {'M', "item", "mikstura_duza"},
    {'$', "item", "zloto"},            {'t', "item", "topor"},
    {'k', "item", "kolczuga"},
};

void addQuad(std::vector<Vertex>& v, std::vector<unsigned>& idx, glm::vec3 a, glm::vec3 b, glm::vec3 c,
             glm::vec3 d, glm::vec3 normal, glm::vec2 uvA, glm::vec2 uvC) {
    // a = lewy dół, b = prawy dół, c = prawy górny, d = lewy górny (patrząc od strony normalnej)
    unsigned base = unsigned(v.size());
    v.push_back({a, normal, {uvA.x, uvA.y}});
    v.push_back({b, normal, {uvC.x, uvA.y}});
    v.push_back({c, normal, {uvC.x, uvC.y}});
    v.push_back({d, normal, {uvA.x, uvC.y}});
    idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

std::string restOfLine(std::istringstream& in) {
    std::string rest;
    std::getline(in, rest);
    size_t start = rest.find_first_not_of(" \t");
    return start == std::string::npos ? "" : rest.substr(start);
}

} // namespace

bool Location::load(const std::string& mapFile) {
    file = mapFile;
    title = mapFile;
    std::ifstream in("data/maps/" + mapFile);
    if (!in) {
        std::printf("[BLAD] Nie mozna otworzyc mapy data/maps/%s\n", mapFile.c_str());
        return false;
    }

    std::string exitTarget;
    glm::vec2 exitPos(0.0f);
    float exitYaw = 0.0f;
    std::string line;
    int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t hash = line.find('#');
        std::string content = hash == std::string::npos ? line : line.substr(0, hash);
        std::istringstream s(content);
        std::string cmd;
        if (!(s >> cmd)) continue;

        if (cmd == "title") {
            title = restOfLine(s);
        } else if (cmd == "terrain") {
            std::string hm, sp, layerFiles[4];
            s >> hm >> sp >> layerFiles[0] >> layerFiles[1] >> layerFiles[2] >> layerFiles[3] >> size >> maxHeight;
            outdoor = true;
            loadTerrain(hm, sp, layerFiles);
        } else if (cmd == "sky") {
            std::string tex;
            s >> tex;
            sky = &getTexture(tex);
        } else if (cmd == "sun") {
            s >> scene.sunDir.x >> scene.sunDir.y >> scene.sunDir.z >> scene.sunColor.r >> scene.sunColor.g >> scene.sunColor.b;
        } else if (cmd == "ambient") {
            s >> scene.ambient.r >> scene.ambient.g >> scene.ambient.b;
        } else if (cmd == "fog") {
            s >> scene.fogColor.r >> scene.fogColor.g >> scene.fogColor.b >> scene.fogDensity;
        } else if (cmd == "water") {
            s >> waterLevel;
            hasWater = true;
        } else if (cmd == "start") {
            s >> start.x >> start.y >> startYaw;
        } else if (cmd == "bounds") {
            s >> boundsMin.x >> boundsMin.y >> boundsMax.x >> boundsMax.y;
        } else if (cmd == "zone") {
            Zone z;
            s >> z.min.x >> z.min.y >> z.max.x >> z.max.y;
            z.name = restOfLine(s);
            zones.push_back(z);
        } else if (cmd == "prop") {
            std::string model, tex, collision = "0";
            float x = 0, z = 0, yaw = 0, scale = 1, emissive = 0;
            s >> model >> tex >> x >> z >> yaw >> scale >> collision >> emissive;
            addProp(model, tex, {x, z}, yaw, scale, collision, emissive);
        } else if (cmd == "light") {
            WorldLight l;
            float x = 0, z = 0, h = 0;
            std::string flicker;
            s >> x >> z >> h >> l.color.r >> l.color.g >> l.color.b >> l.range >> flicker;
            l.pos = {x, groundHeight(x, z) + h, z};
            l.flicker = flicker == "flicker";
            lights.push_back(l);
        } else if (cmd == "portal") {
            Portal p;
            s >> p.pos.x >> p.pos.y >> p.radius >> p.target;
            if (!(s >> p.targetPos.x >> p.targetPos.y >> p.targetYaw)) p.toStart = true;   // bez celu = start mapy
            portals.push_back(p);
        } else if (cmd == "monster" || cmd == "npc" || cmd == "item") {
            Spawn sp{cmd, "", {0, 0}, 0.0f};
            s >> sp.type >> sp.pos.x >> sp.pos.y >> sp.yaw;
            spawns.push_back(sp);
        } else if (cmd == "dungeon") {
            std::string wall, floorFile, ceilingFile;
            s >> cell >> ceiling >> wall >> floorFile >> ceilingFile;
            outdoor = false;
            wallTex = &getTexture(wall);
            floorTex = &getTexture(floorFile);
            ceilingTex = &getTexture(ceilingFile);
        } else if (cmd == "exit") {
            s >> exitTarget >> exitPos.x >> exitPos.y >> exitYaw;
        } else if (cmd == "grid") {
            while (std::getline(in, line)) {   // reszta pliku to siatka - tu # jest ścianą, nie komentarzem
                if (!line.empty() && line.back() == '\r') line.pop_back();
                grid.push_back(line);
            }
        } else {
            std::printf("[UWAGA] %s:%d: nieznana komenda '%s'\n", mapFile.c_str(), lineNo, cmd.c_str());
        }
    }

    if (!outdoor) {
        for (int j = 0; j < int(grid.size()); ++j)
            for (int i = 0; i < int(grid[j].size()); ++i) {
                char c = grid[j][i];
                glm::vec2 center((i + 0.5f) * cell, (j + 0.5f) * cell);
                if (c == 'S') {
                    start = center;
                } else if (c == 'E') {
                    portals.push_back({center, cell * 0.5f, exitTarget, exitPos, exitYaw});
                } else if (c == 'T') {   // wisi na sąsiedniej ścianie, więc nie blokuje przejścia
                    addWallTorch(i, j);
                } else if (c == 'K') {   // stoi na środku pola - tylko w dużych salach, nie w przejściach bo dam po łapach
                    addProp("koksownik.obj", "koksownik.bmp", center, 0.0f, 1.0f, "0.4", 0.9f);
                    lights.push_back({{center.x, 2.2f, center.y}, {1.4f, 0.8f, 0.4f}, 13.0f, true});
                } else if (c == 'P') {
                    Prop p;
                    p.mesh = &getModel("kolumna.obj");
                    p.texture = &getTexture("kolumna.bmp");
                    p.pos = center;
                    p.radius = 0.6f;
                    // model kolumny ma 3.5 m, rozciągamy go do wysokości sufitu trochę przez to dziwnie wygląda lol
                    p.model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(center.x, 0.0f, center.y)),
                                         glm::vec3(1.0f, ceiling / 3.5f, 1.0f));
                    props.push_back(p);
                } else {
                    for (const LegendEntry& e : LEGEND)
                        if (e.symbol == c) spawns.push_back({e.kind, e.type, center, 0.0f});
                }
            }
        buildDungeon();
        scene.sunColor = glm::vec3(0.0f);   // pod ziemią nie ma słońca duuhh
    }
    std::printf("Mapa %s: %zu obiektow, %zu swiatel, %zu postaci/przedmiotow\n", mapFile.c_str(),
                props.size(), lights.size(), spawns.size());
    return true;
}

// Kolizja: liczba = promień okręgu, "AxB" = prostokąt A x B metrów (osie X i Z modelu,
// przed skalowaniem i obrotem), "0" = bez kolizji.
void Location::addProp(const std::string& model, const std::string& tex, glm::vec2 pos, float yaw, float scale,
                       const std::string& collision, float emissive) {
    Prop p;
    p.mesh = &getModel(model);
    p.texture = &getTexture(tex);
    p.pos = pos;
    p.emissive = emissive;
    p.angle = glm::radians(-yaw);   // minus, bo w mapie 90 = wschód, a glm::rotate kręci w drugą stronę
    float w = 0, d = 0;
    if (std::sscanf(collision.c_str(), "%fx%f", &w, &d) == 2) p.halfSize = glm::vec2(w, d) * 0.5f * scale;
    else p.radius = float(std::atof(collision.c_str()));

    // Na zboczu obiekt stoi na najniższym punkcie pod sobą (zagłębia się w grunt zamiast wisieć w powietrzu)
    glm::vec2 extent = p.halfSize.x > 0 ? p.halfSize : glm::vec2(p.radius * 0.7f);
    float y = groundHeight(pos.x, pos.y);
    float c = std::cos(p.angle), sn = std::sin(p.angle);
    for (glm::vec2 corner : {glm::vec2(1, 1), glm::vec2(-1, 1), glm::vec2(1, -1), glm::vec2(-1, -1)}) {
        glm::vec2 local = corner * extent;
        glm::vec2 world = pos + glm::vec2(local.x * c + local.y * sn, -local.x * sn + local.y * c);
        y = std::min(y, groundHeight(world.x, world.y));
    }
    p.model = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, y, pos.y));
    p.model = glm::rotate(p.model, p.angle, glm::vec3(0, 1, 0));
    p.model = glm::scale(p.model, glm::vec3(scale));
    props.push_back(p);
}

// Pierwsza ściana obok komórki (i, j), ogień skierowany do środka komórki
void Location::addWallTorch(int i, int j) {
    const int dirs[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};   // północ, wschód, południe, zachód
    glm::vec2 center((i + 0.5f) * cell, (j + 0.5f) * cell);
    for (const auto& d : dirs) {
        if (!isWall(i + d[0], j + d[1])) continue;
        glm::vec2 toWall{float(d[0]), float(d[1])};
        glm::vec2 onWall = center + toWall * (cell * 0.5f);
        addProp("pochodnia.obj", "pochodnia.bmp", onWall, dirToYaw(glm::vec3(-toWall.x, 0, -toWall.y)), 1.0f, "0", 0.9f);
        glm::vec2 lightPos = onWall - toWall * 0.5f;
        lights.push_back({{lightPos.x, 2.4f, lightPos.y}, {1.4f, 0.8f, 0.4f}, 12.0f, true});
        return;
    }
    std::printf("[UWAGA] %s: pochodnia w komorce (%d, %d) nie ma obok sciany\n", file.c_str(), i, j);
}

bool Location::loadTerrain(const std::string& heightmap, const std::string& splatFile, const std::string layerFiles[4]) {
    splat = &getTexture(splatFile);
    for (int i = 0; i < 4; ++i) layers[i] = &getTexture(layerFiles[i]);

    std::vector<unsigned char> rgba;
    if (!loadBMP("data/textures/" + heightmap, gridW, gridH, rgba)) {
        std::printf("[UWAGA] Brak heightmapy %s - teren plaski\n", heightmap.c_str());
        gridW = gridH = 2;   // płasko jak na Mazowszu
        rgba.assign(16, 0);
    }
    spacing = size / float(gridW - 1);
    heights.resize(size_t(gridW) * gridH);
    for (int t = 0; t < gridH; ++t)        // t = wiersz od góry obrazka (północ)
        for (int i = 0; i < gridW; ++i) {
            int r = gridH - 1 - t;         // loadBMP zwraca wiersze od dołu obrazka
            heights[size_t(t) * gridW + i] = rgba[(size_t(r) * gridW + i) * 4] / 255.0f * maxHeight;
        }

    auto h = [&](int i, int t) {
        i = std::clamp(i, 0, gridW - 1);
        t = std::clamp(t, 0, gridH - 1);
        return heights[size_t(t) * gridW + i];
    };
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    v.reserve(size_t(gridW) * gridH);
    for (int t = 0; t < gridH; ++t)
        for (int i = 0; i < gridW; ++i) {
            glm::vec3 normal = glm::normalize(glm::vec3(h(i - 1, t) - h(i + 1, t), 2.0f * spacing, h(i, t - 1) - h(i, t + 1)));
            glm::vec2 uv(float(i) / (gridW - 1), 1.0f - float(t) / (gridH - 1));   // splatmapa jak obrazek
            v.push_back({{i * spacing, h(i, t), t * spacing}, normal, uv});
        }
    for (int t = 0; t + 1 < gridH; ++t)
        for (int i = 0; i + 1 < gridW; ++i) {
            unsigned a = t * gridW + i, b = a + 1, c = a + gridW, d = c + 1;
            idx.insert(idx.end(), {a, c, b, b, c, d});
        }
    terrainMesh = createMesh(v, idx);
    return true;
}

// Ściany tylko tam, gdzie graniczą z podłogą - reszty i tak nikt nie zobaczy lmao
void Location::buildDungeon() {
    std::vector<Vertex> wv, fv, cv;
    std::vector<unsigned> wi, fi, ci;
    const float c = cell, h = ceiling, vTop = ceiling / cell;
    for (int j = 0; j < int(grid.size()); ++j)
        for (int i = 0; i < int(grid[j].size()); ++i) {
            if (isWall(i, j)) continue;
            float x0 = i * c, x1 = x0 + c, z0 = j * c, z1 = z0 + c;
            addQuad(fv, fi, {x0, 0, z1}, {x1, 0, z1}, {x1, 0, z0}, {x0, 0, z0}, {0, 1, 0}, {0, 0}, {1, 1});
            addQuad(cv, ci, {x0, h, z0}, {x1, h, z0}, {x1, h, z1}, {x0, h, z1}, {0, -1, 0}, {0, 0}, {1, 1});
            if (isWall(i, j - 1)) addQuad(wv, wi, {x0, 0, z0}, {x1, 0, z0}, {x1, h, z0}, {x0, h, z0}, {0, 0, 1}, {0, 0}, {1, vTop});
            if (isWall(i, j + 1)) addQuad(wv, wi, {x1, 0, z1}, {x0, 0, z1}, {x0, h, z1}, {x1, h, z1}, {0, 0, -1}, {0, 0}, {1, vTop});
            if (isWall(i - 1, j)) addQuad(wv, wi, {x0, 0, z1}, {x0, 0, z0}, {x0, h, z0}, {x0, h, z1}, {1, 0, 0}, {0, 0}, {1, vTop});
            if (isWall(i + 1, j)) addQuad(wv, wi, {x1, 0, z0}, {x1, 0, z1}, {x1, h, z1}, {x1, h, z0}, {-1, 0, 0}, {0, 0}, {1, vTop});
        }
    wallMesh = createMesh(wv, wi);
    floorMesh = createMesh(fv, fi);
    ceilingMesh = createMesh(cv, ci);
    size = std::max(float(grid.size()), 1.0f) * c;
}

bool Location::isWall(int i, int j) const {
    if (j < 0 || j >= int(grid.size()) || i < 0 || i >= int(grid[j].size())) return true;
    return grid[j][i] == '#' || grid[j][i] == ' ';   // spacja = poza kryptą
}

float Location::groundHeight(float x, float z) const {
    if (!outdoor || heights.empty()) return 0.0f;
    float gx = std::clamp(x / spacing, 0.0f, float(gridW - 1));
    float gz = std::clamp(z / spacing, 0.0f, float(gridH - 1));
    int i = std::min(int(gx), gridW - 2), t = std::min(int(gz), gridH - 2);
    float fx = gx - i, fz = gz - t;
    auto h = [&](int a, int b) { return heights[size_t(b) * gridW + a]; };
    float top = h(i, t) * (1 - fx) + h(i + 1, t) * fx;
    float bottom = h(i, t + 1) * (1 - fx) + h(i + 1, t + 1) * fx;
    return top * (1 - fz) + bottom * fz;
}

bool Location::blocked(glm::vec2 p, float r) const {
    if (outdoor) {
        glm::vec2 lo = boundsMax.x > 0 ? boundsMin : glm::vec2(0.0f);
        glm::vec2 hi = boundsMax.x > 0 ? boundsMax : glm::vec2(size);
        return p.x < lo.x + r || p.y < lo.y + r || p.x > hi.x - r || p.y > hi.y - r;
    }
    int i0 = int(std::floor((p.x - r) / cell)), i1 = int(std::floor((p.x + r) / cell));
    int j0 = int(std::floor((p.y - r) / cell)), j1 = int(std::floor((p.y + r) / cell));
    for (int j = j0; j <= j1; ++j)
        for (int i = i0; i <= i1; ++i) {
            if (!isWall(i, j)) continue;
            glm::vec2 closest(std::clamp(p.x, i * cell, (i + 1) * cell), std::clamp(p.y, j * cell, (j + 1) * cell));
            if (glm::length(p - closest) < r) return true;
        }
    return false;
}

glm::vec3 Location::moveAndCollide(glm::vec3 pos, glm::vec3 delta, float radius) const {
    glm::vec2 p(pos.x, pos.z);
    // ruch osobno w X i w Z - przy ścianie postać ślizga się wzdłuż niej
    for (int axis = 0; axis < 2; ++axis) {
        glm::vec2 next = p;
        next[axis] += axis == 0 ? delta.x : delta.z;
        float step = std::abs(next[axis] - p[axis]);
        // ponad 1.2 m w górę na metr (ok. 50 stopni) to już ściana
        bool tooSteep = groundHeight(next.x, next.y) - groundHeight(p.x, p.y) > step * 1.2f + 0.001f;
        if (!blocked(next, radius) && !tooSteep) p = next;
    }
    glm::vec2 beforePush = p;
    for (const Prop& prop : props) {
        if (prop.halfSize.x > 0.0f) {
            float c = std::cos(prop.angle), sn = std::sin(prop.angle);
            glm::vec2 d = p - prop.pos;
            glm::vec2 local(d.x * c - d.y * sn, d.x * sn + d.y * c);   // w układzie prostokąta
            glm::vec2 closest = glm::clamp(local, -prop.halfSize, prop.halfSize);
            glm::vec2 diff = local - closest;
            float dist = glm::length(diff);
            if (dist >= radius) continue;
            if (dist > 0.0001f) {
                local = closest + diff / dist * radius;
            } else {   // środek postaci w prostokącie - wypchnięcie najkrótszą drogą
                glm::vec2 depth = prop.halfSize - glm::abs(local);
                if (depth.x < depth.y) local.x = (local.x < 0 ? -1.0f : 1.0f) * (prop.halfSize.x + radius);
                else local.y = (local.y < 0 ? -1.0f : 1.0f) * (prop.halfSize.y + radius);
            }
            p = prop.pos + glm::vec2(local.x * c + local.y * sn, -local.x * sn + local.y * c);
        } else if (prop.radius > 0.0f) {
            glm::vec2 d = p - prop.pos;
            float dist = glm::length(d), minDist = radius + prop.radius;
            if (dist < minDist) p = prop.pos + (dist > 0.0001f ? d / dist : glm::vec2(1, 0)) * minDist;
        }
    }
    if (blocked(p, radius)) p = beforePush;   // obiekt nie może wypchnąć gracza w ścianę
    return {p.x, groundHeight(p.x, p.y), p.y};
}

bool Location::lineOfSight(glm::vec3 from, glm::vec3 to) const {
    if (outdoor) return true;
    glm::vec2 a(from.x, from.z), b(to.x, to.z);
    float len = glm::length(b - a);
    int steps = int(len / (cell * 0.25f)) + 1;
    for (int k = 1; k < steps; ++k) {
        glm::vec2 p = a + (b - a) * (float(k) / steps);
        if (isWall(int(std::floor(p.x / cell)), int(std::floor(p.y / cell)))) return false;
    }
    return true;
}

std::string Location::zoneAt(float x, float z) const {
    for (const Zone& zone : zones)
        if (x >= zone.min.x && x <= zone.max.x && z >= zone.min.y && z <= zone.max.y) return zone.name;
    return "";
}

const Portal* Location::portalNear(glm::vec3 pos, float reach) const {
    for (const Portal& p : portals)
        if (glm::length(glm::vec2(pos.x, pos.z) - p.pos) < p.radius + reach) return &p;
    return nullptr;
}

void Location::collectLights(glm::vec3 center, float time, int maxCount, std::vector<PointLight>& out) const {
    std::vector<std::pair<float, int>> byDistance;
    for (int i = 0; i < int(lights.size()); ++i)
        // minus zasięg: duże światło trochę dalej liczy się bardziej niż małe tuż obok
        byDistance.push_back({glm::length(lights[i].pos - center) - lights[i].range, i});
    std::sort(byDistance.begin(), byDistance.end());
    for (int k = 0; k < int(byDistance.size()) && k < maxCount; ++k) {
        const WorldLight& l = lights[byDistance[k].second];
        float intensity = 1.0f;
        if (l.flicker) {   // dwa sinusy o różnych częstotliwościach - fizyk by płakał, ale wygląda jak ogień
            float seed = float(byDistance[k].second) * 1.7f;
            intensity = 0.85f + 0.08f * std::sin(time * 9.0f + seed) + 0.07f * std::sin(time * 23.0f + seed * 2.3f);
        }
        out.push_back({l.pos, l.color * intensity, l.range});
    }
}

void Location::draw() const {
    if (outdoor) {
        if (sky) drawSky(*sky);
        if (terrainMesh.indexCount && splat) drawTerrain(terrainMesh, *splat, layers, size / 4.0f);   // tekstura co 4 m
    } else if (wallTex && floorTex && ceilingTex) {
        drawMesh(wallMesh, *wallTex, glm::mat4(1.0f), glm::vec3(1.0f), 0.1f);
        drawMesh(floorMesh, *floorTex, glm::mat4(1.0f), glm::vec3(1.0f), 0.2f);
        drawMesh(ceilingMesh, *ceilingTex, glm::mat4(1.0f), glm::vec3(1.0f), 0.0f);
    }
    for (const Prop& p : props) drawMesh(*p.mesh, *p.texture, p.model, glm::vec3(1.0f), 0.15f, p.emissive);
}

void Location::drawTransparent() const {
    if (hasWater) drawWater(glm::vec3(size * 0.5f, waterLevel, size * 0.5f), size);
}

void Location::drawLightMarkers() const {
    for (const WorldLight& l : lights) drawLightMarker(l.pos, glm::normalize(l.color) * 1.5f, 0.18f);
}
