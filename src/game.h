// Wszystkie struktury gry są tu od początku, żeby kolejne etapy nie musiały ich przerabiać.
// plik gotowy i wystarczy pozmieniać inne żeby skończyć wszystkie etapy
#pragma once

#include "render.h"
#include "world.h"
#include <set>
#include <string>
#include <utility>
#include <vector>

struct Input {
    bool down[256] = {};              // ASCII, zawsze małe litery (main.cpp robi tolower)
    bool pressed[256] = {};           // tylko w klatce, w której klawisz został wciśnięty
    bool specialDown[256] = {};       // GLUT_KEY_* (strzałki, F1..F12, Shift)
    bool specialPressed[256] = {};
    int mouseX = 0, mouseY = 0;
    float mouseDX = 0, mouseDY = 0;   // tylko w trybie rozglądania
    bool lmb = false, rmb = false;
    bool lmbClick = false, rmbClick = false;   // jak pressed - tylko w klatce kliknięcia
    int width = 1280, height = 720;
};

// --- Przedmioty (etap 2, inventory.cpp) ---
enum class ItemType { Weapon, Armor, Potion, Gold, Quest };

struct ItemDef {
    std::string id;                   // jak w plikach map, np. "mikstura_mala"
    std::string name, description;
    std::string icon;                 // BMP 64x64 z data/textures/ - ikona w ekwipunku i przedmiot na ziemi
    ItemType type = ItemType::Quest;
    int minDamage = 0, maxDamage = 0;
    float attackDelay = 0.0f;         // sekundy między ciosami
    int defense = 0;
    int heal = 0;
    int gold = 0;
};

struct WorldItem { int item; glm::vec3 pos; };   // item = indeks ItemDef

// --- Potwory (etap 3, combat.cpp) ---
enum class MonsterState { Idle, Chase, Attack, Return, Dead };

struct MonsterDef {
    std::string id, name;
    std::string sprite;               // arkusz 4x2: (stoi, krok, atak, martwy) x (przód, tył)
    glm::vec2 size;                   // metry
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

// --- NPC i dialogi (etap 4, dialog.cpp) ---
struct NpcDef { std::string id, name, sprite; glm::vec2 size; int dialogStart; };
struct Npc { int def = 0; glm::vec3 pos{0.0f}; float yaw = 0.0f; };

enum class DialogCondition { None, HasItem, Flag, NotFlag };
enum class DialogAction { GiveItem, TakeItem, Heal, GiveGold, SetFlag };

struct DialogOption {
    std::string text;
    DialogCondition condition = DialogCondition::None;
    std::string conditionParam;       // id przedmiotu albo nazwa flagi
    std::vector<std::pair<DialogAction, std::string>> actions;
    int next = -1;                    // -1 = koniec rozmowy
};
struct DialogNode { int id; std::string text; std::vector<DialogOption> options; };

// --- Gracz i stan gry ---
struct Player {
    glm::vec3 pos{0.0f};              // stopy, nie oczy
    float yaw = 0.0f, pitch = 0.0f;
    int hp = 100, maxHp = 100;
    int baseAttack = 2, baseDefense = 0;
    int level = 1, xp = 0, gold = 0;
    std::vector<int> inventory;       // indeksy ItemDef
    int weapon = -1, armor = -1;      // indeksy ItemDef, -1 = nic
    float attackTimer = 0, hurtTimer = 0, bobPhase = 0;
};

struct Level {                        // stan zostaje po wyjściu z lokacji (zabite potwory nie wracają)
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
    std::vector<Message> messages;
    std::set<std::string> flags;      // postęp zadań
    std::string zone;
    std::string banner;
    float bannerTimer = 0.0f;
    std::string hint;                 // np. "[E] Podnieś: Miecz"
    float time = 0.0f;
    bool wireframe = false, freeCamera = false, showLights = false, quit = false;
    Camera freeCam;
    std::string screenshotRequest;    // zrzut dopiero po narysowaniu całej klatki
    int dialogNpc = -1, dialogNode = -1;

    Level& level() { return levels[current]; }
};
extern Game game;

// --- game.cpp ---
void gameInit();
void gameUpdate(Input& in, float dt);
void gameDraw(const Input& in);
bool gameWantsMouseCapture();
void newGame();
void addMessage(const std::string& text);
Camera playerCamera();
int levelIndex(const std::string& mapFile);

// --- inventory.cpp (etap 2) ---
int findItem(const std::string& id);   // -1 = nie ma
const ItemDef& itemDef(int item);
void spawnLevelItems(Level& level);    // z level.loc.spawns (kind == "item")
void spawnItem(int item, glm::vec3 pos);
bool giveItem(int item);               // false = pełny ekwipunek
bool hasItem(int item);
bool removeItem(int item);
int attackDamage();                    // losowe: broń + baseAttack
int defenseValue();                    // zbroja + baseDefense
float attackDelay();
std::string pickupHint();              // "" = nic do podniesienia
bool tryPickup();
void drawWorldItems();
void updateInventoryScreen(Input& in);
void drawInventoryScreen(const Input& in);

// --- combat.cpp (etap 3) ---
void spawnLevelMonsters(Level& level);
void updateMonsters(float dt);
void playerAttack();
glm::vec3 pushOutOfMonsters(glm::vec3 pos, float radius);
void drawMonsters();

// --- dialog.cpp (etap 4) ---
void spawnLevelNpcs(Level& level);
std::string talkHint();                // "" = nikogo w pobliżu
bool tryTalk();
void drawNpcs();
void updateDialogScreen(Input& in);
void drawDialogScreen(const Input& in);
