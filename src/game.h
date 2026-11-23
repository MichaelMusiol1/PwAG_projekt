// game.h - rozgrywka: WSZYSTKIE struktury danych gry (gracz, przedmioty, potwory, NPC, dialogi)
// oraz funkcje modułów. Plik jest kompletny od początku, żeby kolejne etapy nie przerabiały struktur:
//   game.cpp      - rdzeń: start, ruch, kamera, interakcja, przejścia, HUD, ekrany   (etap 1)
//   inventory.cpp - przedmioty i ekwipunek                                           (etap 2)
//   combat.cpp    - potwory i walka                                                  (etap 3)
//   dialog.cpp    - NPC, dialogi, zadanie                                            (etap 4)
#pragma once

#include "render.h"
#include "world.h"
#include <set>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Wejście - wypełnia main.cpp, czyta gra
// ---------------------------------------------------------------------------
struct Input {
    bool down[256] = {};              // klawisze ASCII (małe litery) wciśnięte teraz
    bool pressed[256] = {};           // klawisze ASCII wciśnięte w tej klatce
    bool specialDown[256] = {};       // klawisze specjalne GLUT_KEY_* (strzałki, F1..F12, Shift)
    bool specialPressed[256] = {};
    int mouseX = 0, mouseY = 0;       // kursor w pikselach okna
    float mouseDX = 0, mouseDY = 0;   // ruch myszy w trybie rozglądania
    bool lmb = false, rmb = false;    // przyciski myszy wciśnięte
    bool lmbClick = false, rmbClick = false;   // przyciski kliknięte w tej klatce
    int width = 1280, height = 720;   // rozmiar okna
};

// ---------------------------------------------------------------------------
// Przedmioty (etap 2 - inventory.cpp)
// ---------------------------------------------------------------------------
enum class ItemType { Weapon, Armor, Potion, Gold, Quest };

struct ItemDef {
    std::string id;                   // identyfikator w mapach, np. "mikstura_mala"
    std::string name, description;
    std::string icon;                 // BMP 64x64 w data/textures/: ikona i sprite na ziemi
    ItemType type = ItemType::Quest;
    int minDamage = 0, maxDamage = 0; // broń
    float attackDelay = 0.0f;         // broń: sekundy między ciosami
    int defense = 0;                  // zbroja
    int heal = 0;                     // mikstura
    int gold = 0;                     // złoto
};

struct WorldItem { int item; glm::vec3 pos; };   // przedmiot leżący na ziemi

// ---------------------------------------------------------------------------
// Potwory (etap 3 - combat.cpp)
// ---------------------------------------------------------------------------
enum class MonsterState { Idle, Chase, Attack, Return, Dead };

struct MonsterDef {
    std::string id, name;
    std::string sprite;               // arkusz 4x2: (stoi, krok, atak, martwy) x (przód, tył)
    glm::vec2 size;                   // szerokość i wysokość sprite'a w metrach
    int hp, minDamage, maxDamage, defense, xp;
    float speed, aggroRange, attackRange, attackDelay;
    std::vector<std::pair<std::string, float>> loot;   // (przedmiot, szansa 0..1)
};

struct Monster {
    int def = 0;                      // indeks MonsterDef
    glm::vec3 pos{0.0f}, spawn{0.0f}, target{0.0f};
    float yaw = 0.0f;
    int hp = 0;
    MonsterState state = MonsterState::Idle;
    float stateTimer = 0, attackTimer = 0, flashTimer = 0, animTimer = 0, deathTimer = 0;
};

// ---------------------------------------------------------------------------
// NPC i dialogi (etap 4 - dialog.cpp)
// ---------------------------------------------------------------------------
struct NpcDef { std::string id, name, sprite; glm::vec2 size; int dialogStart; };
struct Npc { int def = 0; glm::vec3 pos{0.0f}; float yaw = 0.0f; };

enum class DialogCondition { None, HasItem, Flag, NotFlag };
enum class DialogAction { GiveItem, TakeItem, Heal, GiveGold, SetFlag };

struct DialogOption {
    std::string text;
    DialogCondition condition = DialogCondition::None;
    std::string conditionParam;       // id przedmiotu albo nazwa flagi
    std::vector<std::pair<DialogAction, std::string>> actions;
    int next = -1;                    // następny węzeł; -1 = koniec rozmowy
};
struct DialogNode { int id; std::string text; std::vector<DialogOption> options; };

// ---------------------------------------------------------------------------
// Gracz i stan gry
// ---------------------------------------------------------------------------
struct Player {
    glm::vec3 pos{0.0f};              // pozycja stóp
    float yaw = 0.0f, pitch = 0.0f;
    int hp = 100, maxHp = 100;
    int baseAttack = 2, baseDefense = 0;
    int level = 1, xp = 0, gold = 0;
    std::vector<int> inventory;       // indeksy ItemDef
    int weapon = -1, armor = -1;      // założone przedmioty (indeksy ItemDef) albo -1
    float attackTimer = 0, hurtTimer = 0, bobPhase = 0;
};

struct Level {                        // lokacja + jej postacie i przedmioty (stan zostaje po wyjściu)
    Location loc;
    std::vector<Monster> monsters;
    std::vector<Npc> npcs;
    std::vector<WorldItem> items;
};

enum class Screen { Play, Inventory, Dialog, Help, Dead };
struct Message { std::string text; float age; };

struct Game {
    Level levels[2];                  // 0 = dolina, 1 = krypta
    int current = 0;
    Player player;
    Screen screen = Screen::Help;
    std::vector<Message> messages;    // dziennik zdarzeń (lewy dół ekranu)
    std::set<std::string> flags;      // postęp zadań
    std::string zone;                 // strefa, w której jest gracz
    std::string banner;               // duży napis u góry (nazwa strefy/lokacji)
    float bannerTimer = 0.0f;
    std::string hint;                 // podpowiedź pod celownikiem, np. "[E] Podnieś: Miecz"
    float time = 0.0f;
    bool wireframe = false, freeCamera = false, showLights = false, quit = false;
    Camera freeCam;
    std::string screenshotRequest;    // niepusty = zapisz zrzut po narysowaniu klatki
    int dialogNpc = -1, dialogNode = -1;   // aktualna rozmowa (etap 4)

    Level& level() { return levels[current]; }
};
extern Game game;

// ---------------------------------------------------------------------------
// game.cpp - rdzeń
// ---------------------------------------------------------------------------
void gameInit();
void gameUpdate(Input& in, float dt);
void gameDraw(const Input& in);
bool gameWantsMouseCapture();          // true = ukryty kursor, rozglądanie myszą
void newGame();
void addMessage(const std::string& text);
Camera playerCamera();
int levelIndex(const std::string& mapFile);

// ---------------------------------------------------------------------------
// inventory.cpp - etap 2
// ---------------------------------------------------------------------------
int findItem(const std::string& id);   // indeks ItemDef albo -1
const ItemDef& itemDef(int item);
void spawnLevelItems(Level& level);    // przedmioty z punktów startowych lokacji
void spawnItem(int item, glm::vec3 pos);
bool giveItem(int item);               // false = pełny ekwipunek
bool hasItem(int item);
bool removeItem(int item);
int attackDamage();                    // losowe obrażenia ciosu gracza (broń + atak)
int defenseValue();                    // obrona gracza (zbroja + bazowa)
float attackDelay();                   // przerwa między ciosami gracza
std::string pickupHint();              // "" = nic do podniesienia
bool tryPickup();
void drawWorldItems();
void updateInventoryScreen(Input& in);
void drawInventoryScreen(const Input& in);

// ---------------------------------------------------------------------------
// combat.cpp - etap 3
// ---------------------------------------------------------------------------
void spawnLevelMonsters(Level& level);
void updateMonsters(float dt);
void playerAttack();                   // LPM / Spacja
glm::vec3 pushOutOfMonsters(glm::vec3 pos, float radius);
void drawMonsters();

// ---------------------------------------------------------------------------
// dialog.cpp - etap 4
// ---------------------------------------------------------------------------
void spawnLevelNpcs(Level& level);
std::string talkHint();                // "" = nikogo w pobliżu
bool tryTalk();
void drawNpcs();
void updateDialogScreen(Input& in);
void drawDialogScreen(const Input& in);
