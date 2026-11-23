// game.cpp - rdzeń rozgrywki: start gry, ruch gracza i kamera, kolizje, interakcja (E),
// przejścia między lokacjami, światła dynamiczne (latarnia gracza), HUD, ekrany pomocy
// i śmierci oraz klawisze debug (F2 siatka, F3 wolna kamera, F4 światła, F12 zrzut).
// Miejsca podpięcia kolejnych etapów oznaczone są komentarzem [etap N].
#include "game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

Game game;

namespace {

const float EYE_HEIGHT = 1.7f;
const float PLAYER_RADIUS = 0.4f;
const float WALK_SPEED = 4.0f, RUN_SPEED = 7.5f;   // m/s
const float TURN_SPEED = 120.0f;                    // stopnie/s (strzałki)
const float MOUSE_SENSITIVITY = 0.12f;              // stopnie na piksel
const float FREE_CAMERA_SPEED = 12.0f;

const glm::vec4 WHITE(1.0f), YELLOW(1.0f, 0.85f, 0.3f, 1.0f), GRAY(0.7f, 0.7f, 0.7f, 1.0f);

const char* HELP_TEXT =
    "W A S D, strzałki   ruch i obrót\n"
    "Mysz                rozglądanie\n"
    "PgUp / PgDn         patrzenie w górę / w dół\n"
    "Shift               bieg\n"
    "E                   przejdź (później: podnieś, rozmawiaj)\n"
    "LPM / Spacja        atak (etap 3)\n"
    "I                   ekwipunek (etap 2)\n"
    "1-4                 wybór opcji w rozmowie (etap 4)\n"
    "F1 / Esc            pomoc i pauza (Q: wyjście z gry)\n"
    "F2                  siatka trójkątów (wireframe)\n"
    "F3                  wolna kamera (Spacja/C: góra/dół)\n"
    "F4                  znaczniki źródeł światła\n"
    "F12                 zrzut ekranu (screenshots/)";

void showBanner(const std::string& text) {
    game.banner = text;
    game.bannerTimer = 3.5f;
}

void travel(const Portal& portal) {
    int target = levelIndex(portal.target);
    if (target < 0) {
        addMessage("To przejście nigdzie nie prowadzi.");
        return;
    }
    game.current = target;
    const Location& loc = game.level().loc;
    glm::vec2 pos = portal.toStart ? loc.start : portal.targetPos;
    game.player.pos = glm::vec3(pos.x, loc.groundHeight(pos.x, pos.y), pos.y);
    game.player.yaw = portal.toStart ? loc.startYaw : portal.targetYaw;
    game.player.pitch = 0.0f;
    game.zone = loc.zoneAt(pos.x, pos.y);   // baner pokazuje nazwę lokacji, strefa dopiero przy zmianie
    showBanner(loc.title);
    addMessage("Wchodzisz: " + loc.title);
}

// Obrót: mysz, strzałki w lewo/prawo, PgUp/PgDn w górę/w dół (jak w MM7)
void turn(const Input& in, float dt, float& yaw, float& pitch, float maxPitch) {
    yaw += in.mouseDX * MOUSE_SENSITIVITY;
    pitch -= in.mouseDY * MOUSE_SENSITIVITY;
    if (in.specialDown[GLUT_KEY_LEFT]) yaw -= TURN_SPEED * dt;
    if (in.specialDown[GLUT_KEY_RIGHT]) yaw += TURN_SPEED * dt;
    if (in.specialDown[GLUT_KEY_PAGE_UP]) pitch += TURN_SPEED * 0.5f * dt;
    if (in.specialDown[GLUT_KEY_PAGE_DOWN]) pitch -= TURN_SPEED * 0.5f * dt;
    pitch = std::clamp(pitch, -maxPitch, maxPitch);
}

// Ruch gracza: obrót, chodzenie z kolizjami, kołysanie kamery
void updatePlayer(Input& in, float dt) {
    Player& p = game.player;
    turn(in, dt, p.yaw, p.pitch, 89.0f);

    glm::vec3 forward = yawToDir(p.yaw), right(-forward.z, 0.0f, forward.x);
    glm::vec3 move(0.0f);
    if (in.down['w'] || in.specialDown[GLUT_KEY_UP]) move += forward;
    if (in.down['s'] || in.specialDown[GLUT_KEY_DOWN]) move -= forward;
    if (in.down['d']) move += right;
    if (in.down['a']) move -= right;

    if (glm::length(move) > 0.0f) {
        bool run = in.specialDown[GLUT_KEY_SHIFT_L] || in.specialDown[GLUT_KEY_SHIFT_R];
        float speed = run ? RUN_SPEED : WALK_SPEED;
        glm::vec3 delta = glm::normalize(move) * speed * dt;
        p.pos = game.level().loc.moveAndCollide(p.pos, delta, PLAYER_RADIUS);
        // [etap 3] p.pos = pushOutOfMonsters(p.pos, PLAYER_RADIUS);
        p.bobPhase += dt * speed * 2.2f;
    }
}

void updateFreeCamera(Input& in, float dt) {
    Camera& c = game.freeCam;
    turn(in, dt, c.yaw, c.pitch, 89.0f);
    float speed = FREE_CAMERA_SPEED * (in.specialDown[GLUT_KEY_SHIFT_L] ? 3.0f : 1.0f) * dt;
    if (in.down['w'] || in.specialDown[GLUT_KEY_UP]) c.pos += c.forward() * speed;
    if (in.down['s'] || in.specialDown[GLUT_KEY_DOWN]) c.pos -= c.forward() * speed;
    if (in.down['d']) c.pos += c.right() * speed;
    if (in.down['a']) c.pos -= c.right() * speed;
    if (in.down[' ']) c.pos.y += speed;
    if (in.down['c']) c.pos.y -= speed;
}

// Interakcja: podpowiedź pod celownikiem i klawisz E. Kolejność: NPC, przedmiot, przejście.
void updateInteraction(Input& in) {
    game.hint.clear();
    // [etap 4] game.hint = talkHint(); if (!game.hint.empty()) { if (in.pressed['e']) tryTalk(); return; }
    // [etap 2] game.hint = pickupHint(); if (!game.hint.empty()) { if (in.pressed['e']) tryPickup(); return; }
    const Portal* portal = game.level().loc.portalNear(game.player.pos, 0.8f);
    if (portal) {
        int target = levelIndex(portal->target);
        game.hint = "[E] Przejdź: " + (target >= 0 ? game.levels[target].loc.title : portal->target);
        if (in.pressed['e']) travel(*portal);
    }
}

void updateZone() {
    const glm::vec3& p = game.player.pos;
    std::string zone = game.level().loc.zoneAt(p.x, p.z);
    if (zone != game.zone) {
        game.zone = zone;
        if (!zone.empty()) showBanner(zone);
    }
}

// Klawisze działające na każdym ekranie
void updateDebugKeys(Input& in) {
    if (in.specialPressed[GLUT_KEY_F2]) {
        game.wireframe = !game.wireframe;
        addMessage(game.wireframe ? "Siatka trójkątów: włączona" : "Siatka trójkątów: wyłączona");
    }
    if (in.specialPressed[GLUT_KEY_F3]) {
        game.freeCamera = !game.freeCamera;
        if (game.freeCamera) game.freeCam = playerCamera();
        addMessage(game.freeCamera ? "Wolna kamera: włączona" : "Wolna kamera: wyłączona");
    }
    if (in.specialPressed[GLUT_KEY_F4]) game.showLights = !game.showLights;
    if (in.specialPressed[GLUT_KEY_F12]) {
        static int counter = 0;   // kilka zrzutów w tej samej sekundzie ma różne nazwy
        char stamp[32], name[64];
        std::time_t now = std::time(nullptr);
        std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", std::localtime(&now));
        std::snprintf(name, sizeof(name), "screenshots/zrzut_%s_%d.bmp", stamp, ++counter);
        game.screenshotRequest = name;
    }
}

// Latarnia gracza - źródło światła poruszające się razem z nim, pionowo pod kamerą.
// W dzień na zewnątrz świeci słabiej, w krypcie mocniej.
PointLight lanternLight() {
    glm::vec3 pos = playerCamera().pos - glm::vec3(0.0f, 0.6f, 0.0f);
    if (game.level().loc.outdoor) return {pos, glm::vec3(1.0f, 0.85f, 0.6f) * 0.45f, 8.0f};
    return {pos, {1.1f, 0.95f, 0.75f}, 12.0f};
}

// ----------------------------- HUD i ekrany ---------------------------------
void drawShadowText(const std::string& text, float x, float y, float scale, glm::vec4 color) {
    drawText(text, x + scale, y + scale, scale, glm::vec4(0, 0, 0, color.a * 0.8f));
    drawText(text, x, y, scale, color);
}

void drawCentered(const std::string& text, float y, float scale, glm::vec4 color, int width) {
    drawShadowText(text, (width - textWidth(text, scale)) * 0.5f, y, scale, color);
}

void drawHud(const Input& in) {
    const Player& p = game.player;
    const float w = float(in.width), h = float(in.height);

    if (p.hurtTimer > 0.0f) drawRect(0, 0, w, h, glm::vec4(0.8f, 0.0f, 0.0f, std::min(p.hurtTimer, 0.5f)));

    // celownik
    if (!game.freeCamera) {
        drawRect(w * 0.5f - 1, h * 0.5f - 7, 2, 14, glm::vec4(1, 1, 1, 0.6f));
        drawRect(w * 0.5f - 7, h * 0.5f - 1, 14, 2, glm::vec4(1, 1, 1, 0.6f));
    }
    if (!game.hint.empty()) drawCentered(game.hint, h * 0.5f + 28, 2.0f, YELLOW, in.width);

    // HP i doświadczenie
    float bx = 20, by = h - 62, bw = 280;
    drawRect(bx - 4, by - 4, bw + 8, 52, glm::vec4(0, 0, 0, 0.55f));
    drawRect(bx, by, bw, 22, glm::vec4(0.25f, 0.05f, 0.05f, 1.0f));
    drawRect(bx, by, bw * std::max(0, p.hp) / float(p.maxHp), 22, glm::vec4(0.75f, 0.1f, 0.1f, 1.0f));
    drawShadowText("HP " + std::to_string(std::max(0, p.hp)) + "/" + std::to_string(p.maxHp), bx + 6, by + 3, 2.0f, WHITE);
    float xpNeeded = 100.0f * p.level;
    drawRect(bx, by + 28, bw, 8, glm::vec4(0.15f, 0.15f, 0.05f, 1.0f));
    drawRect(bx, by + 28, bw * std::min(1.0f, p.xp / xpNeeded), 8, glm::vec4(0.85f, 0.7f, 0.2f, 1.0f));
    drawShadowText("Poziom " + std::to_string(p.level) + "   Złoto: " + std::to_string(p.gold), bx + bw + 16, by + 3, 2.0f, YELLOW);

    // dziennik zdarzeń - najnowszy na dole
    float my = by - 32;
    for (int i = int(game.messages.size()) - 1; i >= 0; --i, my -= 24) {
        float alpha = std::clamp(8.0f - game.messages[i].age, 0.0f, 1.0f);
        drawShadowText(game.messages[i].text, 20, my, 2.0f, glm::vec4(1, 1, 1, alpha));
    }

    // nazwa strefy / lokacji
    if (game.bannerTimer > 0.0f)
        drawCentered(game.banner, 50, 3.0f, glm::vec4(1.0f, 0.95f, 0.8f, std::min(1.0f, game.bannerTimer)), in.width);

    // pasek informacyjny u góry
    std::string info = "F1 pomoc   " + game.level().loc.title;
    if (game.wireframe) info += "   [SIATKA]";
    if (game.freeCamera) info += "   [WOLNA KAMERA]";
    if (game.showLights) {   // tryb debug: także pozycja gracza (przydatna przy edycji map)
        char pos[64];
        std::snprintf(pos, sizeof(pos), "   [ŚWIATŁA]   x %.1f  z %.1f", game.player.pos.x, game.player.pos.z);
        info += pos;
    }
    drawShadowText(info, 12, 10, 2.0f, GRAY);
}

void drawHelp(const Input& in) {
    const float w = float(in.width), h = float(in.height);
    drawRect(0, 0, w, h, glm::vec4(0, 0, 0, 0.7f));
    const float scale = 2.0f, panelW = 66 * 8 * scale, panelH = 400;
    const float x = (w - panelW) * 0.5f, y = (h - panelH) * 0.5f;
    drawRect(x - 20, y - 20, panelW + 40, panelH + 40, glm::vec4(0.12f, 0.1f, 0.08f, 0.95f));
    drawRect(x - 20, y - 20, panelW + 40, 4, YELLOW);
    drawCentered("PwAG RPG", y, 4.0f, YELLOW, in.width);
    drawText(HELP_TEXT, x, y + 70, scale, WHITE);
    drawCentered("Dowolny klawisz: graj      Q: wyjście z gry", y + panelH - 10, scale, GRAY, in.width);
}

void drawDead(const Input& in) {
    drawRect(0, 0, float(in.width), float(in.height), glm::vec4(0.35f, 0.0f, 0.0f, 0.6f));
    drawCentered("ZGINĄŁEŚ", in.height * 0.4f, 6.0f, WHITE, in.width);
    drawCentered("R: nowa gra      Q: wyjście z gry", in.height * 0.4f + 90, 2.0f, GRAY, in.width);
}

} // namespace

// ===========================================================================
// Funkcje publiczne
// ===========================================================================
int levelIndex(const std::string& mapFile) {
    for (int i = 0; i < 2; ++i)
        if (game.levels[i].loc.file == mapFile) return i;
    return -1;
}

void addMessage(const std::string& text) {
    game.messages.push_back({text, 0.0f});
    if (game.messages.size() > 6) game.messages.erase(game.messages.begin());
}

Camera playerCamera() {
    Camera c;
    c.pos = game.player.pos + glm::vec3(0.0f, EYE_HEIGHT + std::sin(game.player.bobPhase) * 0.04f, 0.0f);
    c.yaw = game.player.yaw;
    c.pitch = game.player.pitch;
    return c;
}

void gameInit() {
    game.levels[0].loc.load("dolina.txt");
    game.levels[1].loc.load("krypta.txt");
    newGame();
}

void newGame() {
    Game& g = game;
    g.player = Player();
    g.current = 0;
    const Location& loc = g.levels[0].loc;
    g.player.pos = glm::vec3(loc.start.x, loc.groundHeight(loc.start.x, loc.start.y), loc.start.y);
    g.player.yaw = loc.startYaw;
    g.messages.clear();
    g.flags.clear();
    g.zone = loc.zoneAt(loc.start.x, loc.start.y);
    for (Level& level : g.levels) {
        level.monsters.clear();
        level.npcs.clear();
        level.items.clear();
        // [etap 2] spawnLevelItems(level);
        // [etap 3] spawnLevelMonsters(level);
        // [etap 4] spawnLevelNpcs(level);
    }
    showBanner(loc.title);
    addMessage("Witaj w dolinie! F1 pokazuje sterowanie.");
}

void gameUpdate(Input& in, float dt) {
    Game& g = game;
    g.time += dt;
    if (g.screen == Screen::Play) g.bannerTimer = std::max(0.0f, g.bannerTimer - dt);   // pauza nie zjada banera
    g.player.hurtTimer = std::max(0.0f, g.player.hurtTimer - dt);
    for (Message& m : g.messages) m.age += dt;
    while (!g.messages.empty() && g.messages.front().age > 8.0f) g.messages.erase(g.messages.begin());

    updateDebugKeys(in);

    switch (g.screen) {
    case Screen::Help: {
        // Q = wyjście; każdy inny klawisz (także Esc) lub klik = powrót do gry. Klawisze F działają jako debug.
        bool anyKey = in.lmbClick || in.specialPressed[GLUT_KEY_F1];
        for (int k = 0; k < 256; ++k) anyKey |= in.pressed[k];
        if (in.pressed['q']) g.quit = true;
        else if (anyKey) g.screen = Screen::Play;
        break;
    }
    case Screen::Dead:
        if (in.pressed['r']) {
            newGame();
            g.screen = Screen::Play;
        }
        if (in.pressed['q']) g.quit = true;
        break;
    case Screen::Inventory:   // [etap 2] updateInventoryScreen(in);
    case Screen::Dialog:      // [etap 4] updateDialogScreen(in);
        if (in.pressed[27]) g.screen = Screen::Play;
        break;
    case Screen::Play:
        if (in.pressed[27] || in.specialPressed[GLUT_KEY_F1]) {
            g.screen = Screen::Help;
            break;
        }
        // [etap 2] if (in.pressed['i']) { g.screen = Screen::Inventory; break; }
        if (g.freeCamera) {
            updateFreeCamera(in, dt);
        } else {
            updatePlayer(in, dt);
            updateInteraction(in);
            updateZone();
            // [etap 3] if (in.lmbClick || in.pressed[' ']) playerAttack();
        }
        // [etap 3] updateMonsters(dt);
        if (g.player.hp <= 0) g.screen = Screen::Dead;
        break;
    }
}

void gameDraw(const Input& in) {
    Location& loc = game.level().loc;
    Camera cam = game.freeCamera ? game.freeCam : playerCamera();

    SceneSettings scene = loc.scene;
    scene.time = game.time;
    PointLight lantern = lanternLight();
    loc.collectLights(cam.pos, game.time, MAX_LIGHTS - 1, scene.lights);
    scene.lights.push_back(lantern);

    setWireframe(game.wireframe);
    beginScene(cam, scene, in.width, in.height);
    loc.draw();
    // [etap 2] drawWorldItems();
    // [etap 3] drawMonsters();
    // [etap 4] drawNpcs();
    loc.drawTransparent();
    if (game.showLights) {
        loc.drawLightMarkers();
        // latarnia gracza: mały znacznik pod kamerą (widać go, patrząc w dół), w wolnej kamerze większy
        glm::vec3 color = glm::normalize(lantern.color) * 1.5f;
        drawLightMarker(lantern.pos, color, game.freeCamera ? 0.18f : 0.03f);
    }

    begin2D(in.width, in.height);
    drawHud(in);
    if (game.screen == Screen::Help) drawHelp(in);
    if (game.screen == Screen::Dead) drawDead(in);
    // [etap 2] if (game.screen == Screen::Inventory) drawInventoryScreen(in);
    // [etap 4] if (game.screen == Screen::Dialog) drawDialogScreen(in);

    if (!game.screenshotRequest.empty()) {
        bool ok = saveScreenshot(game.screenshotRequest, in.width, in.height);
        std::printf("%s %s\n", ok ? "Zapisano zrzut" : "[BLAD] Nie udalo sie zapisac", game.screenshotRequest.c_str());
        addMessage(ok ? "Zrzut ekranu: " + game.screenshotRequest : "Nie udało się zapisać zrzutu");
        game.screenshotRequest.clear();
    }
}

bool gameWantsMouseCapture() { return game.screen == Screen::Play; }
