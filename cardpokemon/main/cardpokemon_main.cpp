/*
 * CardPokemon standalone firmware.
 *
 * This is intentionally not a Mooncake launcher app. The firmware boots
 * directly into a small travel-driven monster collecting loop.
 */

#include <hal.h>
#include <keyboard/keymap.h>
#include <apps/utils/theme.h>
#include <M5Unified.hpp>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <cJSON.h>
#include "pokemon_color_badges.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

namespace {

extern const uint8_t cardpokemon_logo_png_start[] asm("_binary_cardpokemon_logo_png_start");
extern const uint8_t cardpokemon_logo_png_end[] asm("_binary_cardpokemon_logo_png_end");

constexpr const char* kTag = "CardPokemon";
constexpr int kScreenW = 240;
constexpr int kScreenH = 135;
constexpr int kMaxSpecies = 200;
constexpr int kOwnedBytes = (kMaxSpecies + 7) / 8;
constexpr int kDefaultSpeciesCount = 10;
constexpr int kStarterCount = 3;
constexpr uint32_t kSensorIntervalMs = 1000;
constexpr uint32_t kRenderIntervalMs = 120;
constexpr uint32_t kEncounterCooldownMs = 45000;
constexpr uint32_t kSaveSchemaVersion = 3;
constexpr uint32_t kFieldItemDurationMs = 10 * 60 * 1000;

enum class Scene {
    Starter,
    Travel,
    Battle,
    Pokedex,
    Center,
    Shop,
    Bag,
};

struct Species {
    int id;
    char name[24];
    char type[12];
    int baseHp;
    int atk;
    int def;
    int catchRate;
    int evolveLevel;
    int evolveTo;
    int evolveToId;
    uint16_t color;
};

struct SpeciesSeed {
    int id;
    const char* name;
    const char* type;
    int baseHp;
    int atk;
    int def;
    int catchRate;
    int evolveLevel;
    int evolveToId;
    uint16_t color;
};

struct PokemonState {
    uint8_t level = 1;
    uint16_t xp = 0;
    int16_t hp = 20;
};

struct SensorState {
    bool capAvailable = false;
    bool imuAvailable = false;
    bool gpsValid = false;
    bool gpsFresh = false;
    double lat = 0.0;
    double lng = 0.0;
    double speedKmph = 0.0;
    uint32_t gpsAgeMs = 0;
    int satellites = 0;
    double hdop = 99.9;
    float motionScore = 0.0f;
    float accelBase[3] = {0.0f, 0.0f, 1.0f};
    float gyroBase[3] = {0.0f, 0.0f, 0.0f};
    bool motionBaseReady = false;
    double distanceM = 0.0;
    double sessionDistanceM = 0.0;
    double distanceSinceEventM = 0.0;
    double nextEncounterTargetM = 160.0;
    double lastGpsStepM = 0.0;
    double lastStepM = 0.0;
};

struct BattleState {
    int wild = 0;
    uint8_t wildLevel = 1;
    int wildHp = 10;
    int wildMaxHp = 10;
    uint32_t startedMs = 0;
    uint32_t resolvedMs = 0;
    bool resolved = false;
    bool won = false;
    bool caught = false;
    bool ballUsed = false;
    bool greatBallUsed = false;
    int typeEffect = 100;
    int xp = 0;
    int gold = 0;
};

enum class RouteState {
    Walk,
    City,
    Road,
    Highway,
    Rough,
    Lost,
};

enum class PokedexMode {
    Grid,
    Detail,
};

constexpr SpeciesSeed kDefaultSpecies[kDefaultSpeciesCount] = {
    {1, "Bulbasaur", "Grass", 24, 6, 5, 42, 5, 2, TFT_GREEN},
    {4, "Charmander", "Fire", 22, 8, 3, 38, 5, 5, TFT_ORANGE},
    {7, "Squirtle", "Water", 25, 5, 7, 40, 5, 8, TFT_CYAN},
    {2, "Ivysaur", "Grass", 34, 9, 8, 25, 12, -1, TFT_DARKGREEN},
    {5, "Charmeleon", "Fire", 32, 11, 6, 23, 12, -1, TFT_RED},
    {8, "Wartortle", "Water", 35, 8, 10, 25, 12, -1, TFT_SKYBLUE},
    {16, "Pidgey", "Flying", 18, 5, 3, 70, -1, -1, TFT_LIGHTGREY},
    {19, "Rattata", "Normal", 17, 6, 2, 72, -1, -1, TFT_PURPLE},
    {25, "Pikachu", "Electric", 20, 8, 3, 20, -1, -1, TFT_YELLOW},
    {129, "Magikarp", "Water", 16, 2, 2, 80, -1, -1, TFT_BLUE},
};

struct Game {
    Scene scene = Scene::Starter;
    Scene previousScene = Scene::Travel;
    SensorState sensor;
    BattleState battle;
    std::array<Species, kMaxSpecies> species;
    std::array<PokemonState, kMaxSpecies> pokemon;
    std::array<uint8_t, kOwnedBytes> owned = {};
    std::array<uint8_t, kOwnedBytes> seen = {};
    int speciesCount = 0;
    int active = 0;
    int selected = 0;
    int pokedexCursor = 0;
    PokedexMode pokedexMode = PokedexMode::Grid;
    int menuCursor = 0;
    uint16_t gold = 0;
    uint8_t potions = 2;
    uint8_t balls = 8;
    uint8_t greatBalls = 0;
    uint8_t repels = 0;
    uint8_t lures = 0;
    int logCursor = 0;
    int heatCursor = 0;
    uint32_t rng = 0xC0FFEE;
    uint32_t lastSensorMs = 0;
    uint32_t lastRenderMs = 0;
    uint32_t lastSaveMs = 0;
    uint32_t lastEncounterMs = 0;
    uint32_t repelUntilMs = 0;
    uint32_t lureUntilMs = 0;
    uint32_t evolveFlashUntilMs = 0;
    int evolveFlashSpecies = -1;
    float encounterHeat = 12.0f;
    std::array<uint8_t, 32> heatHistory = {};
    char log[4][48] = {};
    char resourceRoot[96] = "/sdcard";
    bool sdMounted = false;
    int assetPngCount = 0;
    int assetGifCount = 0;
    int assetJsonCount = 0;
    int resourceJsonCount = 0;
};

Game g;
LGFX_Sprite gFrame(&M5.Display);

LGFX_Sprite& gfx()
{
    return gFrame;
}

bool file_exists(const char* path)
{
    FILE* fp = std::fopen(path, "rb");
    if (!fp) {
        return false;
    }
    std::fclose(fp);
    return true;
}

void join_resource_path(char* out, size_t outSize, const char* suffix)
{
    std::snprintf(out, outSize, "%s/%s", g.resourceRoot, suffix);
}

bool select_resource_root()
{
    constexpr const char* roots[] = {
        "/sdcard",
        "/sdcard/sdcard",
        "/sdcard/cardpokemon",
        "/sdcard/cardpokemon/assets/sdcard",
        "/sdcard/assets/sdcard",
    };
    for (const char* root : roots) {
        char path[128];
        std::snprintf(path, sizeof(path), "%s/manifest.json", root);
        const bool hasManifest = file_exists(path);
        std::snprintf(path, sizeof(path), "%s/pokemon/1.json", root);
        const bool hasPokemonJson = file_exists(path);
        if (hasManifest || hasPokemonJson) {
            std::snprintf(g.resourceRoot, sizeof(g.resourceRoot), "%s", root);
            return true;
        }
    }
    std::snprintf(g.resourceRoot, sizeof(g.resourceRoot), "%s", "/sdcard");
    return false;
}

const Species& species_at(int index)
{
    return g.species[std::clamp(index, 0, std::max(0, g.speciesCount - 1))];
}

void copy_text(char* dest, size_t size, const char* src)
{
    std::snprintf(dest, size, "%s", src ? src : "");
}

uint16_t type_color(const char* type)
{
    if (std::strcmp(type, "fire") == 0 || std::strcmp(type, "Fire") == 0) {
        return TFT_ORANGE;
    }
    if (std::strcmp(type, "water") == 0 || std::strcmp(type, "Water") == 0) {
        return TFT_CYAN;
    }
    if (std::strcmp(type, "electric") == 0 || std::strcmp(type, "Electric") == 0) {
        return TFT_YELLOW;
    }
    if (std::strcmp(type, "flying") == 0 || std::strcmp(type, "Flying") == 0 ||
        std::strcmp(type, "normal") == 0 || std::strcmp(type, "Normal") == 0) {
        return TFT_LIGHTGREY;
    }
    if (std::strcmp(type, "poison") == 0 || std::strcmp(type, "Poison") == 0) {
        return TFT_PURPLE;
    }
    return TFT_GREEN;
}

PokemonColorBadge color_badge_for_id(int id, uint16_t fallback)
{
    for (const auto& badge : kPokemonColorBadges) {
        if (badge.id == id) {
            return badge;
        }
    }
    return PokemonColorBadge{static_cast<uint16_t>(std::max(0, id)), fallback, fallback, TFT_WHITE};
}

void set_species_from_seed(int index, const SpeciesSeed& seed)
{
    Species& s = g.species[index];
    s.id = seed.id;
    copy_text(s.name, sizeof(s.name), seed.name);
    copy_text(s.type, sizeof(s.type), seed.type);
    s.baseHp = seed.baseHp;
    s.atk = seed.atk;
    s.def = seed.def;
    s.catchRate = seed.catchRate;
    s.evolveLevel = seed.evolveLevel;
    s.evolveTo = -1;
    s.evolveToId = seed.evolveToId;
    s.color = seed.color;
}

int find_species_by_id(int id)
{
    for (int i = 0; i < g.speciesCount; ++i) {
        if (g.species[i].id == id) {
            return i;
        }
    }
    return -1;
}

void resolve_evolution_indices()
{
    for (int i = 0; i < g.speciesCount; ++i) {
        g.species[i].evolveTo = g.species[i].evolveToId > 0 ? find_species_by_id(g.species[i].evolveToId) : -1;
    }
}

void init_default_catalog()
{
    g.speciesCount = kDefaultSpeciesCount;
    for (int i = 0; i < kDefaultSpeciesCount; ++i) {
        set_species_from_seed(i, kDefaultSpecies[i]);
    }
    resolve_evolution_indices();
}

const char* cjson_string(cJSON* object, const char* key, const char* fallback)
{
    cJSON* value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(value) && value->valuestring ? value->valuestring : fallback;
}

int cjson_int(cJSON* object, const char* key, int fallback)
{
    cJSON* value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(value) ? value->valueint : fallback;
}

int cjson_stat(cJSON* entry, const char* key, int fallback)
{
    cJSON* stats = cJSON_GetObjectItemCaseSensitive(entry, "stats");
    if (!cJSON_IsObject(stats)) {
        return fallback;
    }
    return cjson_int(stats, key, fallback);
}

bool set_species_from_json_entry(cJSON* entry, int index)
{
    if (!cJSON_IsObject(entry) || index < 0 || index >= kMaxSpecies) {
        return false;
    }
    const int id = cjson_int(entry, "id", -1);
    if (id <= 0) {
        return false;
    }

    Species& s = g.species[index];
    s.id = id;
    copy_text(s.name, sizeof(s.name), cjson_string(entry, "name", "Unknown"));
    cJSON* types = cJSON_GetObjectItemCaseSensitive(entry, "types");
    const char* type = "normal";
    if (cJSON_IsArray(types)) {
        cJSON* first = cJSON_GetArrayItem(types, 0);
        if (cJSON_IsString(first) && first->valuestring) {
            type = first->valuestring;
        }
    }
    copy_text(s.type, sizeof(s.type), type);

    const int hp = cjson_stat(entry, "hp", 40);
    const int atk = cjson_stat(entry, "attack", 45);
    const int def = cjson_stat(entry, "defense", 45);
    s.baseHp = std::clamp(hp / 2, 14, 80);
    s.atk = std::clamp(atk / 8, 2, 18);
    s.def = std::clamp(def / 8, 2, 18);
    s.catchRate = 18;
    s.evolveLevel = -1;
    s.evolveTo = -1;
    s.evolveToId = -1;
    s.color = type_color(type);

    cJSON* game = cJSON_GetObjectItemCaseSensitive(entry, "game");
    if (cJSON_IsObject(game)) {
        s.catchRate = cjson_int(game, "catch_rate", s.catchRate);
        s.evolveLevel = cjson_int(game, "evolve_level", s.evolveLevel);
        s.evolveToId = cjson_int(game, "evolve_to", s.evolveToId);
    }
    return true;
}

cJSON* load_json_file(const char* path, long maxSize)
{
    FILE* fp = std::fopen(path, "rb");
    if (!fp) {
        return nullptr;
    }
    std::fseek(fp, 0, SEEK_END);
    const long size = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (size <= 0 || size > maxSize) {
        std::fclose(fp);
        return nullptr;
    }

    char* text = static_cast<char*>(std::malloc(size + 1));
    if (!text) {
        std::fclose(fp);
        return nullptr;
    }
    const size_t read = std::fread(text, 1, size, fp);
    std::fclose(fp);
    text[read] = '\0';
    cJSON* root = cJSON_Parse(text);
    std::free(text);
    return root;
}

bool load_catalog_from_pokemon_jsons()
{
    if (!g.sdMounted) {
        return false;
    }

    int count = 0;
    for (int id = 1; id <= kMaxSpecies && count < kMaxSpecies; ++id) {
        char path[128];
        std::snprintf(path, sizeof(path), "%s/pokemon/%d.json", g.resourceRoot, id);
        if (!file_exists(path)) {
            continue;
        }
        cJSON* entry = load_json_file(path, 12 * 1024);
        if (!entry) {
            continue;
        }
        if (set_species_from_json_entry(entry, count)) {
            ++count;
        }
        cJSON_Delete(entry);
    }

    if (count < kStarterCount) {
        init_default_catalog();
        return false;
    }
    g.speciesCount = count;
    resolve_evolution_indices();
    return true;
}

bool load_catalog_from_manifest()
{
    char manifestPath[80];
    join_resource_path(manifestPath, sizeof(manifestPath), "manifest.json");
    if (!g.sdMounted || !file_exists(manifestPath)) {
        return false;
    }

    FILE* fp = std::fopen(manifestPath, "rb");
    if (!fp) {
        return false;
    }
    std::fseek(fp, 0, SEEK_END);
    const long size = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (size <= 0 || size > 512 * 1024) {
        std::fclose(fp);
        return false;
    }

    char* text = static_cast<char*>(std::malloc(size + 1));
    if (!text) {
        std::fclose(fp);
        return false;
    }
    const size_t read = std::fread(text, 1, size, fp);
    std::fclose(fp);
    text[read] = '\0';

    cJSON* root = cJSON_Parse(text);
    std::free(text);
    if (!root) {
        return false;
    }

    cJSON* pokemon = cJSON_GetObjectItemCaseSensitive(root, "pokemon");
    if (!cJSON_IsArray(pokemon)) {
        cJSON_Delete(root);
        return false;
    }

    int count = 0;
    cJSON* entry = nullptr;
    cJSON_ArrayForEach(entry, pokemon)
    {
        if (count >= kMaxSpecies || !cJSON_IsObject(entry)) {
            continue;
        }
        if (set_species_from_json_entry(entry, count)) {
            ++count;
        }
    }
    cJSON_Delete(root);

    if (count < kStarterCount) {
        init_default_catalog();
        return false;
    }
    g.speciesCount = count;
    resolve_evolution_indices();
    return true;
}

int count_asset_ext(const char* ext)
{
    if (!g.sdMounted) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < g.speciesCount; ++i) {
        char path[128];
        std::snprintf(path, sizeof(path), "%s/pokemon/%d.%s", g.resourceRoot, g.species[i].id, ext);
        if (file_exists(path)) {
            ++count;
        }
    }
    return count;
}

int count_resource_jsons()
{
    if (!g.sdMounted) {
        return 0;
    }
    int count = 0;
    for (int id = 1; id <= kMaxSpecies; ++id) {
        char path[128];
        std::snprintf(path, sizeof(path), "%s/pokemon/%d.json", g.resourceRoot, id);
        if (file_exists(path)) {
            ++count;
        }
    }
    return count;
}

void refresh_asset_status()
{
    g.resourceJsonCount = count_resource_jsons();
    g.assetPngCount = count_asset_ext("png");
    g.assetGifCount = count_asset_ext("gif");
    g.assetJsonCount = count_asset_ext("json");
}

const char* asset_status_label()
{
    if (!g.sdMounted) {
        return "noSD";
    }
    return g.assetPngCount == g.speciesCount ? "IMG" : "asset?";
}

int max_hp(int species, int level)
{
    return species_at(species).baseHp + level * 4;
}

int xp_to_next(int level)
{
    return 14 + level * 16;
}

uint32_t rand_u32()
{
    g.rng ^= g.rng << 13;
    g.rng ^= g.rng >> 17;
    g.rng ^= g.rng << 5;
    return g.rng;
}

int roll(int minValue, int maxValue)
{
    if (maxValue <= minValue) {
        return minValue;
    }
    return minValue + static_cast<int>(rand_u32() % static_cast<uint32_t>(maxValue - minValue + 1));
}

void add_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(g.log[g.logCursor], sizeof(g.log[g.logCursor]), fmt, args);
    va_end(args);
    g.logCursor = (g.logCursor + 1) % 4;
}

void set_flag(std::array<uint8_t, kOwnedBytes>& flags, int species, bool value = true)
{
    if (species < 0 || species >= g.speciesCount) {
        return;
    }
    const int byte = species / 8;
    const uint8_t bit = 1u << (species % 8);
    if (value) {
        flags[byte] |= bit;
    } else {
        flags[byte] &= ~bit;
    }
}

bool has_flag(const std::array<uint8_t, kOwnedBytes>& flags, int species)
{
    if (species < 0 || species >= g.speciesCount) {
        return false;
    }
    return (flags[species / 8] & (1u << (species % 8))) != 0;
}

void set_owned(int species, bool owned = true)
{
    set_flag(g.owned, species, owned);
    if (owned) {
        set_flag(g.seen, species, true);
    }
}

bool is_owned(int species)
{
    return has_flag(g.owned, species);
}

void set_seen(int species, bool seen = true)
{
    set_flag(g.seen, species, seen);
}

bool is_seen(int species)
{
    return has_flag(g.seen, species);
}

int owned_count()
{
    int count = 0;
    for (int i = 0; i < g.speciesCount; ++i) {
        if (is_owned(i)) {
            ++count;
        }
    }
    return count;
}

void clear_owned()
{
    g.owned.fill(0);
    g.seen.fill(0);
}

bool has_owned()
{
    return owned_count() > 0;
}

int distance_level_bonus()
{
    const int distanceBonus = static_cast<int>(g.sensor.distanceM / 5000.0);
    const int sessionBonus = static_cast<int>(g.sensor.sessionDistanceM / 2500.0);
    return std::clamp(distanceBonus + sessionBonus, 0, 18);
}

bool field_item_active(uint32_t untilMs)
{
    return untilMs != 0 && GetHAL().millis() < untilMs;
}

uint32_t elapsed_ms(uint32_t now, uint32_t since)
{
    return now - since;
}

bool repel_active()
{
    return field_item_active(g.repelUntilMs);
}

bool lure_active()
{
    return field_item_active(g.lureUntilMs);
}

RouteState current_route_state()
{
    if (!g.sensor.gpsFresh) {
        return RouteState::Lost;
    }
    const double speed = g.sensor.speedKmph;
    if (speed < 1.0) {
        return g.sensor.lastGpsStepM > 0.5 ? RouteState::Walk : RouteState::Lost;
    }
    if (speed < 7.0) {
        return RouteState::Walk;
    }
    if (speed < 18.0) {
        return RouteState::City;
    }
    if (g.sensor.motionScore > 3.8f) {
        return RouteState::Rough;
    }
    if (speed < 35.0) {
        return RouteState::City;
    }
    if (speed < 75.0) {
        return RouteState::Road;
    }
    return RouteState::Highway;
}

int route_limited_heat(int walkCap, int cityCap, int roadCap, int highwayCap, int roughCap, int lostCap)
{
    const int heat = std::clamp<int>(static_cast<int>(g.encounterHeat), 0, 100);
    switch (current_route_state()) {
        case RouteState::Walk:
            return std::min(heat, walkCap);
        case RouteState::City:
            return std::min(heat, cityCap);
        case RouteState::Road:
            return std::min(heat, roadCap);
        case RouteState::Highway:
            return std::min(heat, highwayCap);
        case RouteState::Rough:
            return std::min(heat, roughCap);
        case RouteState::Lost:
        default:
            return std::min(heat, lostCap);
    }
}

int rarity_heat()
{
    return route_limited_heat(45, 65, 85, 100, 100, 20);
}

int motion_level_bonus()
{
    const int heat = route_limited_heat(45, 70, 90, 100, 100, 15);
    switch (current_route_state()) {
        case RouteState::Walk:
            return std::clamp(heat / 55, 0, 1);
        case RouteState::City:
            return std::clamp(heat / 38, 0, 2);
        case RouteState::Road:
            return std::clamp(heat / 32, 0, 3);
        case RouteState::Highway:
        case RouteState::Rough:
            return std::clamp(heat / 28, 0, 4);
        case RouteState::Lost:
        default:
            return 0;
    }
}

const char* route_label(RouteState route)
{
    switch (route) {
        case RouteState::Walk:
            return "Walk";
        case RouteState::City:
            return "City";
        case RouteState::Road:
            return "Road";
        case RouteState::Highway:
            return "Highway";
        case RouteState::Rough:
            return "Rough";
        case RouteState::Lost:
        default:
            return "Lost";
    }
}

int route_weight(const Species& s, RouteState route)
{
    int weight = 8;
    const bool normal = std::strcmp(s.type, "normal") == 0 || std::strcmp(s.type, "Normal") == 0;
    const bool flying = std::strcmp(s.type, "flying") == 0 || std::strcmp(s.type, "Flying") == 0;
    const bool electric = std::strcmp(s.type, "electric") == 0 || std::strcmp(s.type, "Electric") == 0;
    const bool water = std::strcmp(s.type, "water") == 0 || std::strcmp(s.type, "Water") == 0;
    const bool grass = std::strcmp(s.type, "grass") == 0 || std::strcmp(s.type, "Grass") == 0;
    const bool fire = std::strcmp(s.type, "fire") == 0 || std::strcmp(s.type, "Fire") == 0;
    const bool poison = std::strcmp(s.type, "poison") == 0 || std::strcmp(s.type, "Poison") == 0;
    switch (route) {
        case RouteState::Walk:
            if (grass || normal || flying) weight += 12;
            if (water) weight += 4;
            break;
        case RouteState::City:
            if (normal || flying || electric || poison) weight += 10;
            break;
        case RouteState::Road:
            if (fire || electric || flying || normal) weight += 9;
            break;
        case RouteState::Highway:
            if (electric || flying || fire) weight += 12;
            weight += std::max(0, 10 - s.catchRate / 12);
            break;
        case RouteState::Rough:
            if (poison || fire || grass) weight += 11;
            weight += std::max(0, 8 - s.catchRate / 14);
            break;
        case RouteState::Lost:
            if (normal || grass || water) weight += 7;
            break;
    }
    if (lure_active()) {
        weight += std::max(1, 12 - s.catchRate / 10);
    }
    return std::clamp(weight, 1, 40);
}

bool type_eq(const char* a, const char* b)
{
    if (!a || !b) {
        return false;
    }
    while (*a && *b) {
        const char ca = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a - 'A' + 'a') : *a;
        const char cb = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b - 'A' + 'a') : *b;
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

int type_effect_percent(const Species& attacker, const Species& defender)
{
    if (type_eq(attacker.type, "Fire") && type_eq(defender.type, "Grass")) return 130;
    if (type_eq(attacker.type, "Water") && type_eq(defender.type, "Fire")) return 130;
    if (type_eq(attacker.type, "Grass") && type_eq(defender.type, "Water")) return 130;
    if (type_eq(attacker.type, "Electric") && type_eq(defender.type, "Water")) return 130;
    if (type_eq(attacker.type, "Flying") && type_eq(defender.type, "Grass")) return 125;
    if (type_eq(attacker.type, "Fire") && type_eq(defender.type, "Water")) return 75;
    if (type_eq(attacker.type, "Water") && type_eq(defender.type, "Grass")) return 75;
    if (type_eq(attacker.type, "Grass") && type_eq(defender.type, "Fire")) return 75;
    if (type_eq(attacker.type, "Electric") && type_eq(defender.type, "Grass")) return 85;
    return 100;
}

void add_travel_progress(double stepM)
{
    if (stepM <= 0.0) {
        return;
    }
    g.sensor.distanceM += stepM;
    g.sensor.sessionDistanceM += stepM;
    g.sensor.distanceSinceEventM += stepM;
    g.sensor.lastStepM += stepM;
}

double roll_next_encounter_target()
{
    const float heat = std::clamp(g.encounterHeat, 0.0f, 100.0f);
    int minM = 150;
    int maxM = 260;
    if (heat >= 80.0f) {
        minM = 55;
        maxM = 115;
    } else if (heat >= 50.0f) {
        minM = 80;
        maxM = 155;
    } else if (heat >= 20.0f) {
        minM = 115;
        maxM = 210;
    }

    switch (current_route_state()) {
        case RouteState::Walk:
            minM = minM * 8 / 10;
            maxM = maxM * 9 / 10;
            break;
        case RouteState::Highway:
            minM = minM * 13 / 10;
            maxM = maxM * 14 / 10;
            break;
        case RouteState::Rough:
            minM = minM * 7 / 10;
            maxM = maxM * 8 / 10;
            break;
        default:
            break;
    }

    if (lure_active()) {
        minM = minM * 7 / 10;
        maxM = maxM * 8 / 10;
    }
    if (repel_active()) {
        minM = minM * 18 / 10;
        maxM = maxM * 18 / 10;
    }
    return static_cast<double>(roll(std::max(35, minM), std::max(minM + 10, maxM)));
}

void adapt_encounter_target_to_heat()
{
    if (!g.sensor.gpsFresh || repel_active()) {
        return;
    }

    double desiredRemaining = 0.0;
    if (g.encounterHeat >= 80.0f) {
        desiredRemaining = 55.0;
    } else if (g.encounterHeat >= 50.0f) {
        desiredRemaining = 85.0;
    } else if (g.encounterHeat >= 20.0f) {
        desiredRemaining = 125.0;
    } else {
        return;
    }

    switch (current_route_state()) {
        case RouteState::Walk:
            desiredRemaining *= 0.85;
            break;
        case RouteState::Highway:
            desiredRemaining *= 1.35;
            break;
        case RouteState::Rough:
            desiredRemaining *= 0.75;
            break;
        default:
            break;
    }
    if (lure_active()) {
        desiredRemaining *= 0.75;
    }

    const double desiredTarget = g.sensor.distanceSinceEventM + std::max(35.0, desiredRemaining);
    if (desiredTarget < g.sensor.nextEncounterTargetM) {
        g.sensor.nextEncounterTargetM = desiredTarget;
    }
}

void update_encounter_heat()
{
    const double step = g.sensor.lastStepM;
    g.sensor.lastStepM = 0.0;
    const RouteState route = current_route_state();

    double decay = -0.55;
    double motionFactor = 1.15;
    double motionCap = 4.0;
    double gpsMovingGain = 0.25;
    switch (route) {
        case RouteState::Walk:
            decay = -0.85;
            motionFactor = 0.32;
            motionCap = 1.15;
            gpsMovingGain = 0.18;
            break;
        case RouteState::City:
            decay = -0.70;
            motionFactor = 0.62;
            motionCap = 2.2;
            gpsMovingGain = 0.22;
            break;
        case RouteState::Road:
            decay = -0.58;
            motionFactor = 1.0;
            motionCap = 3.4;
            gpsMovingGain = 0.25;
            break;
        case RouteState::Highway:
            decay = -0.55;
            motionFactor = 1.15;
            motionCap = 4.0;
            gpsMovingGain = 0.25;
            break;
        case RouteState::Rough:
            decay = -0.45;
            motionFactor = 1.35;
            motionCap = 4.8;
            gpsMovingGain = 0.3;
            break;
        case RouteState::Lost:
        default:
            decay = -1.25;
            motionFactor = 0.0;
            motionCap = 0.0;
            gpsMovingGain = 0.0;
            break;
    }

    double heatDelta = decay;
    heatDelta += std::clamp(static_cast<double>(g.sensor.motionScore) * motionFactor, 0.0, motionCap);
    if (step > 0.05 && g.sensor.gpsFresh) {
        heatDelta += gpsMovingGain;
    }
    if (repel_active()) {
        heatDelta -= 0.65;
    }
    if (lure_active()) {
        heatDelta += 1.1;
    }
    g.encounterHeat = std::clamp<float>(g.encounterHeat + heatDelta, 0.0f, 100.0f);
    adapt_encounter_target_to_heat();
    g.heatHistory[g.heatCursor] = static_cast<uint8_t>(std::clamp<int>(static_cast<int>(g.encounterHeat), 0, 100));
    g.heatCursor = (g.heatCursor + 1) % static_cast<int>(g.heatHistory.size());
}

bool should_start_wild_encounter(uint32_t now)
{
    if (elapsed_ms(now, g.lastEncounterMs) < kEncounterCooldownMs) {
        return false;
    }
    if (!g.sensor.gpsFresh || g.sensor.lastGpsStepM <= 0.0) {
        return false;
    }
    return g.sensor.distanceSinceEventM >= g.sensor.nextEncounterTargetM;
}

void save_game()
{
    nvs_handle_t handle = 0;
    if (nvs_open("cardpokemon", NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }

    nvs_set_u32(handle, "schema", kSaveSchemaVersion);
    nvs_set_blob(handle, "owned2", g.owned.data(), g.owned.size());
    nvs_set_blob(handle, "seen2", g.seen.data(), g.seen.size());
    nvs_set_i32(handle, "active", g.active);
    nvs_set_i32(handle, "scene", g.scene == Scene::Starter ? 0 : 1);
    nvs_set_u32(handle, "distm", static_cast<uint32_t>(std::lround(std::max(0.0, g.sensor.distanceM))));
    nvs_set_u16(handle, "gold", g.gold);
    nvs_set_u8(handle, "potions", g.potions);
    nvs_set_u8(handle, "balls", g.balls);
    nvs_set_u8(handle, "greatballs", g.greatBalls);
    nvs_set_u8(handle, "repels", g.repels);
    nvs_set_u8(handle, "lures", g.lures);
    std::array<uint8_t, kMaxSpecies> levels = {};
    std::array<uint16_t, kMaxSpecies> xp = {};
    std::array<int16_t, kMaxSpecies> hp = {};
    for (int i = 0; i < g.speciesCount; ++i) {
        levels[i] = g.pokemon[i].level;
        xp[i] = g.pokemon[i].xp;
        hp[i] = g.pokemon[i].hp;
    }
    nvs_set_blob(handle, "levels", levels.data(), levels.size());
    nvs_set_blob(handle, "xp2", xp.data(), xp.size() * sizeof(uint16_t));
    nvs_set_blob(handle, "hp2", hp.data(), hp.size() * sizeof(int16_t));
    nvs_commit(handle);
    nvs_close(handle);
    g.lastSaveMs = GetHAL().millis();
}

void load_game()
{
    for (int i = 0; i < g.speciesCount; ++i) {
        g.pokemon[i].level = 1;
        g.pokemon[i].xp = 0;
        g.pokemon[i].hp = max_hp(i, 1);
    }

    nvs_handle_t handle = 0;
    if (nvs_open("cardpokemon", NVS_READONLY, &handle) != ESP_OK) {
        return;
    }

    size_t ownedSize = g.owned.size();
    if (nvs_get_blob(handle, "owned2", g.owned.data(), &ownedSize) != ESP_OK) {
        uint32_t oldOwned = 0;
        if (nvs_get_u32(handle, "owned", &oldOwned) == ESP_OK) {
            clear_owned();
            for (int i = 0; i < std::min(g.speciesCount, 32); ++i) {
                if ((oldOwned & (1u << i)) != 0) {
                    set_owned(i);
                }
            }
        }
    }
    size_t seenSize = g.seen.size();
    if (nvs_get_blob(handle, "seen2", g.seen.data(), &seenSize) != ESP_OK) {
        g.seen = g.owned;
    }
    int32_t active = 0;
    if (nvs_get_i32(handle, "active", &active) == ESP_OK) {
        g.active = std::clamp(static_cast<int>(active), 0, std::max(0, g.speciesCount - 1));
    }
    int32_t savedScene = 0;
    if (nvs_get_i32(handle, "scene", &savedScene) == ESP_OK && savedScene != 0 && has_owned()) {
        g.scene = Scene::Travel;
    }
    uint32_t distm = 0;
    if (nvs_get_u32(handle, "distm", &distm) == ESP_OK) {
        g.sensor.distanceM = distm;
    }
    uint16_t gold = 0;
    if (nvs_get_u16(handle, "gold", &gold) == ESP_OK) {
        g.gold = gold;
    }
    uint8_t potions = 0;
    if (nvs_get_u8(handle, "potions", &potions) == ESP_OK) {
        g.potions = potions;
    }
    uint8_t balls = 0;
    if (nvs_get_u8(handle, "balls", &balls) == ESP_OK) {
        g.balls = balls;
    }
    uint8_t greatBalls = 0;
    if (nvs_get_u8(handle, "greatballs", &greatBalls) == ESP_OK) {
        g.greatBalls = greatBalls;
    }
    uint8_t repels = 0;
    if (nvs_get_u8(handle, "repels", &repels) == ESP_OK) {
        g.repels = repels;
    }
    uint8_t lures = 0;
    if (nvs_get_u8(handle, "lures", &lures) == ESP_OK) {
        g.lures = lures;
    }

    std::array<uint8_t, kMaxSpecies> levels = {};
    std::array<uint16_t, kMaxSpecies> xp = {};
    std::array<int16_t, kMaxSpecies> hp = {};
    size_t levelsSize = levels.size();
    size_t xpSize = xp.size() * sizeof(uint16_t);
    size_t hpSize = hp.size() * sizeof(int16_t);
    const bool hasLevelBlob = nvs_get_blob(handle, "levels", levels.data(), &levelsSize) == ESP_OK;
    const bool hasXpBlob = nvs_get_blob(handle, "xp2", xp.data(), &xpSize) == ESP_OK;
    const bool hasHpBlob = nvs_get_blob(handle, "hp2", hp.data(), &hpSize) == ESP_OK;

    for (int i = 0; i < g.speciesCount; ++i) {
        g.pokemon[i].level = hasLevelBlob ? std::clamp<int>(levels[i], 1, 99) : 1;
        g.pokemon[i].xp = hasXpBlob ? xp[i] : 0;
        g.pokemon[i].hp = hasHpBlob ? std::clamp<int>(hp[i], 1, max_hp(i, g.pokemon[i].level)) : max_hp(i, 1);
    }
    nvs_close(handle);
}

int starter_index(int slot)
{
    constexpr int starterIds[kStarterCount] = {1, 4, 7};
    const int found = find_species_by_id(starterIds[std::clamp(slot, 0, kStarterCount - 1)]);
    if (found >= 0) {
        return found;
    }
    return std::clamp(slot, 0, std::max(0, g.speciesCount - 1));
}

void choose_starter(int species)
{
    g.active = species;
    g.selected = species;
    clear_owned();
    g.gold = 20;
    g.potions = 2;
    g.balls = 8;
    g.greatBalls = 0;
    g.repels = 1;
    g.lures = 1;
    for (int i = 0; i < kStarterCount; ++i) {
        const int starter = starter_index(i);
        set_owned(starter);
        g.pokemon[starter].level = 1;
        g.pokemon[starter].xp = 0;
        g.pokemon[starter].hp = max_hp(starter, 1);
    }
    g.scene = Scene::Travel;
    add_log("%s joined you", species_at(species).name);
    add_log("Starters added");
    add_log("G20 Pot2 Ball8");
    save_game();
}

void maybe_evolve(int species)
{
    const Species& current = species_at(species);
    if (current.evolveTo < 0 || g.pokemon[species].level < current.evolveLevel) {
        return;
    }
    if (is_owned(current.evolveTo)) {
        return;
    }
    set_owned(current.evolveTo);
    set_seen(current.evolveTo);
    g.pokemon[current.evolveTo].level = g.pokemon[species].level;
    g.pokemon[current.evolveTo].xp = 0;
    g.pokemon[current.evolveTo].hp = max_hp(current.evolveTo, g.pokemon[current.evolveTo].level);
    g.active = current.evolveTo;
    g.evolveFlashSpecies = current.evolveTo;
    g.evolveFlashUntilMs = GetHAL().millis() + 2400;
    add_log("%s evolved!", current.name);
    add_log("Now %s", species_at(current.evolveTo).name);
}

void grant_xp(int species, int amount)
{
    PokemonState& p = g.pokemon[species];
    p.xp += amount;
    while (p.xp >= xp_to_next(p.level)) {
        p.xp -= xp_to_next(p.level);
        p.level = std::min<int>(99, p.level + 1);
        p.hp = max_hp(species, p.level);
        add_log("%s grew Lv%d", species_at(species).name, p.level);
        maybe_evolve(species);
    }
}

int choose_wild_species()
{
    if (g.speciesCount <= 0) {
        return 0;
    }

    const int activeLevel = g.pokemon[g.active].level;
    const int distanceUnlock = 35 + static_cast<int>(g.sensor.distanceM / 180.0) + activeLevel * 3;
    const int effectiveRarityHeat = rarity_heat();
    const int motionUnlock = effectiveRarityHeat * 45 / 100;
    const int maxDexId = std::clamp(distanceUnlock + motionUnlock, 25, 200);

    int candidates[48] = {};
    int weights[48] = {};
    int candidateCount = 0;
    int totalWeight = 0;
    const RouteState route = current_route_state();
    for (int attempts = 0; attempts < g.speciesCount * 3 &&
                           candidateCount < static_cast<int>(sizeof(candidates) / sizeof(candidates[0]));
         ++attempts) {
        const int index = roll(0, std::max(0, g.speciesCount - 1));
        const Species& s = species_at(index);
            if (s.id <= maxDexId || roll(1, 100) <= std::clamp(activeLevel + motion_level_bonus() * 4, 4, 34)) {
            candidates[candidateCount++] = index;
            weights[candidateCount - 1] = route_weight(s, route);
            totalWeight += weights[candidateCount - 1];
        }
    }

    int pick = roll(0, std::max(0, g.speciesCount - 1));
    if (candidateCount > 0) {
        int ticket = roll(1, std::max(1, totalWeight));
        for (int i = 0; i < candidateCount; ++i) {
            ticket -= weights[i];
            if (ticket <= 0) {
                pick = candidates[i];
                break;
            }
        }
    }

    const int rareBoost = lure_active() ? 10 : 0;
    if (roll(1, 100) <= std::clamp(4 + rareBoost + effectiveRarityHeat * 24 / 100 + distance_level_bonus() / 2, 4, 42)) {
        int rarePick = pick;
        for (int attempts = 0; attempts < 12; ++attempts) {
            const int index = roll(0, std::max(0, g.speciesCount - 1));
            const Species& s = species_at(index);
            if (s.id <= std::min(200, maxDexId + 30) && s.catchRate <= species_at(rarePick).catchRate) {
                rarePick = index;
            }
        }
        pick = rarePick;
    }

    return pick;
}

void start_battle()
{
    g.battle.wild = choose_wild_species();
    const int targetLevel = g.pokemon[g.active].level + distance_level_bonus() + motion_level_bonus();
    g.battle.wildLevel = std::clamp(targetLevel + roll(-2, 1), 1, 99);
    g.battle.wildMaxHp = max_hp(g.battle.wild, g.battle.wildLevel);
    g.battle.wildHp = g.battle.wildMaxHp;
    g.battle.startedMs = GetHAL().millis();
    g.battle.resolvedMs = 0;
    g.battle.resolved = false;
    g.battle.won = false;
    g.battle.caught = false;
    g.battle.ballUsed = false;
    g.battle.greatBallUsed = false;
    g.battle.typeEffect = 100;
    g.battle.xp = 0;
    g.battle.gold = 0;
    g.sensor.distanceSinceEventM = 0.0;
    g.sensor.nextEncounterTargetM = roll_next_encounter_target();
    g.lastEncounterMs = GetHAL().millis();
    set_seen(g.battle.wild);
    g.scene = Scene::Battle;
    add_log("Wild %s appeared", species_at(g.battle.wild).name);
    add_log("%s %.0fm M%.1f", route_label(current_route_state()), g.sensor.sessionDistanceM, g.sensor.motionScore);
}

void resolve_battle()
{
    PokemonState& active = g.pokemon[g.active];
    const Species& ours = species_at(g.active);
    const Species& wild = species_at(g.battle.wild);
    const int motionBonus = std::clamp(static_cast<int>(g.sensor.motionScore * 2.0f), 0, 8);
    const int hpPressure = active.hp < max_hp(g.active, active.level) / 3 ? -3 : 0;
    const int partnerEdge = is_owned(g.battle.wild) ? 0 : 2;
    const int levelGapGuard = std::max(0, g.battle.wildLevel - active.level - 1) * 2;
    const int playerType = type_effect_percent(ours, wild);
    const int wildType = type_effect_percent(wild, ours);
    const int playerBase = active.level * 6 + ours.atk + motionBonus + partnerEdge + hpPressure + roll(0, 12);
    const int wildBase = g.battle.wildLevel * 6 + wild.atk + roll(0, 9) - levelGapGuard;
    const int playerPower = playerBase * playerType / 100;
    const int wildPower = wildBase * wildType / 100;
    g.battle.typeEffect = playerType;

    g.battle.won = playerPower >= wildPower;
    g.battle.resolved = true;
    g.battle.resolvedMs = GetHAL().millis();

    if (g.battle.won) {
        g.battle.xp = 6 + g.battle.wildLevel * 4 + wild.atk;
        g.battle.gold = std::clamp(roll(1, 3 + g.battle.wildLevel / 2), 1, 80);
        g.gold = std::min<uint16_t>(9999, g.gold + g.battle.gold);
        grant_xp(g.active, g.battle.xp);
        int captureChance = std::max(2, wild.catchRate / 8);
        const bool canCatch = !is_owned(g.battle.wild) && g.balls > 0;
        const bool canGreatCatch = !is_owned(g.battle.wild) && g.greatBalls > 0;
        if (canGreatCatch) {
            --g.greatBalls;
            captureChance = captureChance * 17 / 10 + 4;
            g.battle.ballUsed = true;
            g.battle.greatBallUsed = true;
        } else if (canCatch) {
            --g.balls;
            g.battle.ballUsed = true;
        }
        g.battle.caught = (canCatch || canGreatCatch) && roll(1, 100) <= captureChance;
        if (g.battle.caught) {
            set_owned(g.battle.wild);
            g.pokemon[g.battle.wild].level = g.battle.wildLevel;
            g.pokemon[g.battle.wild].xp = 0;
            g.pokemon[g.battle.wild].hp = max_hp(g.battle.wild, g.battle.wildLevel);
            add_log("Caught %s!", wild.name);
        } else {
            add_log("Defeated %s", wild.name);
            if (g.battle.ballUsed) {
                add_log(g.battle.greatBallUsed ? "Great Ball missed" : "Ball missed");
            }
            if (!is_owned(g.battle.wild) && g.balls == 0 && g.greatBalls == 0) {
                add_log("No Ball to catch");
            }
        }
    } else {
        const int damage = std::max(1, wildPower - playerPower / 2 - ours.def / 2);
        active.hp = std::max<int>(1, active.hp - damage);
        add_log("%s hit -%dhp", wild.name, damage);
    }

    save_game();
}

double haversine_m(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double earthM = 6371000.0;
    constexpr double deg = 3.14159265358979323846 / 180.0;
    const double dLat = (lat2 - lat1) * deg;
    const double dLon = (lon2 - lon1) * deg;
    lat1 *= deg;
    lat2 *= deg;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                     std::cos(lat1) * std::cos(lat2) * std::sin(dLon / 2) * std::sin(dLon / 2);
    return earthM * 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

void update_sensors()
{
    const uint32_t now = GetHAL().millis();
    const double dt = std::max(0.1, (now - g.lastSensorMs) / 1000.0);
    g.lastSensorMs = now;
    g.sensor.lastGpsStepM = 0.0;

    if (g.sensor.imuAvailable && GetHAL().imu.update()) {
        auto imu = GetHAL().imu.getImuData();
        const float accel[3] = {imu.accel.x, imu.accel.y, imu.accel.z};
        const float gyro[3] = {imu.gyro.x, imu.gyro.y, imu.gyro.z};
        if (!g.sensor.motionBaseReady) {
            for (int i = 0; i < 3; ++i) {
                g.sensor.accelBase[i] = accel[i];
                g.sensor.gyroBase[i] = gyro[i];
            }
            g.sensor.motionBaseReady = true;
            g.sensor.motionScore = 0.0f;
        } else {
            const float accelDeltaX = accel[0] - g.sensor.accelBase[0];
            const float accelDeltaY = accel[1] - g.sensor.accelBase[1];
            const float accelDeltaZ = accel[2] - g.sensor.accelBase[2];
            const float gyroDeltaX = gyro[0] - g.sensor.gyroBase[0];
            const float gyroDeltaY = gyro[1] - g.sensor.gyroBase[1];
            const float gyroDeltaZ = gyro[2] - g.sensor.gyroBase[2];
            const float accelDeltaMag = std::sqrt(accelDeltaX * accelDeltaX + accelDeltaY * accelDeltaY + accelDeltaZ * accelDeltaZ);
            const float gyroDeltaMag = std::sqrt(gyroDeltaX * gyroDeltaX + gyroDeltaY * gyroDeltaY + gyroDeltaZ * gyroDeltaZ);
            const bool likelyStill = accelDeltaMag < 0.12f && gyroDeltaMag < 5.5f;
            const float accelAlpha = likelyStill ? 0.22f : 0.015f;
            const float gyroAlpha = likelyStill ? 0.18f : 0.01f;
            for (int i = 0; i < 3; ++i) {
                g.sensor.accelBase[i] = g.sensor.accelBase[i] * (1.0f - accelAlpha) + accel[i] * accelAlpha;
                g.sensor.gyroBase[i] = g.sensor.gyroBase[i] * (1.0f - gyroAlpha) + gyro[i] * gyroAlpha;
            }
            const float accelMotion = std::max(0.0f, accelDeltaMag - 0.055f);
            const float gyroMotion = std::max(0.0f, gyroDeltaMag - 2.8f);
            float rawMotion = accelMotion * 4.6f + gyroMotion * 0.045f;
            if (rawMotion < 0.18f) {
                rawMotion = 0.0f;
            }
            const float smooth = rawMotion > g.sensor.motionScore ? 0.45f : 0.22f;
            g.sensor.motionScore = std::clamp(g.sensor.motionScore * (1.0f - smooth) + rawMotion * smooth, 0.0f, 6.0f);
        }
    }

    bool gpsStep = false;
    g.sensor.gpsFresh = false;
    if (g.sensor.capAvailable) {
        auto gps = GetHAL().capLora868.borrowGPS();
        if (gps) {
            g.sensor.gpsValid = gps->location.isValid();
            g.sensor.gpsAgeMs = gps->location.isValid() ? gps->location.age() : UINT32_MAX;
            g.sensor.satellites = gps->satellites.isValid() ? gps->satellites.value() : 0;
            g.sensor.hdop = gps->hdop.isValid() ? gps->hdop.hdop() : 99.9;
            if (gps->speed.isValid() && gps->speed.age() < 5000) {
                g.sensor.speedKmph = std::clamp(gps->speed.kmph(), 0.0, 180.0);
            }
            g.sensor.gpsFresh = g.sensor.gpsValid && g.sensor.gpsAgeMs < 5000 && g.sensor.hdop <= 6.0;
            if (g.sensor.gpsFresh) {
                const double lat = gps->location.lat();
                const double lng = gps->location.lng();
                if (g.sensor.lat != 0.0 || g.sensor.lng != 0.0) {
                    const double step = haversine_m(g.sensor.lat, g.sensor.lng, lat, lng);
                    const double speedMps = std::max(1.0, g.sensor.speedKmph / 3.6);
                    const double maxStep = std::clamp(speedMps * dt * 2.8 + 18.0, 25.0, 180.0);
                    if (step > 0.5 && step <= maxStep) {
                        add_travel_progress(step);
                        g.sensor.lastGpsStepM = step;
                        gpsStep = true;
                    }
                }
                g.sensor.lat = lat;
                g.sensor.lng = lng;
            }
            GetHAL().capLora868.returnGPS();
        }
    }

    if (!gpsStep && !g.sensor.gpsFresh) {
        g.sensor.speedKmph *= 0.65;
    }
    update_encounter_heat();
}

void draw_bar(int x, int y, int w, int h, int value, int maxValue, uint16_t color)
{
    auto& d = gfx();
    d.drawRect(x, y, w, h, TFT_DARKGREY);
    const int fillW = std::clamp((w - 2) * value / std::max(1, maxValue), 0, w - 2);
    d.fillRect(x + 1, y + 1, fillW, h - 2, color);
}

void heal_owned_team()
{
    for (int i = 0; i < g.speciesCount; ++i) {
        if (is_owned(i)) {
            g.pokemon[i].hp = max_hp(i, g.pokemon[i].level);
        }
    }
}

bool use_potion_on_active()
{
    if (g.potions == 0) {
        add_log("No Potion");
        return false;
    }
    PokemonState& p = g.pokemon[g.active];
    const int maxHp = max_hp(g.active, p.level);
    if (p.hp >= maxHp) {
        add_log("HP already full");
        return false;
    }
    --g.potions;
    p.hp = std::min<int>(maxHp, p.hp + 20 + p.level * 2);
    add_log("Used Potion");
    save_game();
    return true;
}

void draw_menu_item(int y, int index, const char* text)
{
    auto& d = gfx();
    const bool selected = g.menuCursor == index;
    d.setTextColor(selected ? TFT_BLACK : TFT_WHITE, selected ? TFT_YELLOW : TFT_BLACK);
    if (selected) {
        d.fillRoundRect(7, y - 2, 148, 13, 3, TFT_YELLOW);
    }
    d.drawString(text, 12, y);
}

uint16_t heat_color()
{
    const int heat = std::clamp<int>(static_cast<int>(g.encounterHeat), 0, 100);
    if (heat < 25) {
        return TFT_CYAN;
    }
    if (heat < 55) {
        return TFT_GREEN;
    }
    if (heat < 78) {
        return TFT_YELLOW;
    }
    return TFT_ORANGE;
}

void draw_heat_wave(int x, int y, int w, int h)
{
    auto& d = gfx();
    const uint16_t color = heat_color();
    const int mid = y + h / 2;
    d.drawFastHLine(x, mid, w, TFT_DARKGREY);
    int lastX = x;
    int lastY = mid;
    const int count = static_cast<int>(g.heatHistory.size());
    for (int i = 0; i < count; ++i) {
        const int histIndex = (g.heatCursor + i) % count;
        const int heat = g.heatHistory[histIndex];
        const int amplitude = 1 + heat * (h / 2 - 1) / 100;
        const int phase = (i + static_cast<int>(GetHAL().millis() / 180)) % 8;
        const int wave = phase < 4 ? phase : 8 - phase;
        const int yy = mid + ((i % 2 == 0) ? 1 : -1) * (wave * amplitude / 4);
        const int xx = x + i * (w - 1) / (count - 1);
        if (i > 0) {
            d.drawLine(lastX, lastY, xx, yy, color);
        }
        lastX = xx;
        lastY = yy;
    }
}

void draw_creature(int x, int y, int species, int frame, bool mirrored)
{
    auto& d = gfx();
    const Species& s = species_at(species);
    const bool evolveFlash = species == g.evolveFlashSpecies && GetHAL().millis() < g.evolveFlashUntilMs;
    const bool flashOn = evolveFlash && ((GetHAL().millis() / 120) % 2 == 0);
    if (g.sdMounted) {
        char pngPath[128];
        std::snprintf(pngPath, sizeof(pngPath), "%s/pokemon/%d.png", g.resourceRoot, s.id);
        if (file_exists(pngPath) && d.drawPngFile(pngPath, x, y, 64, 64)) {
            if (flashOn) {
                d.drawRoundRect(x - 2, y - 2, 68, 68, 8, TFT_WHITE);
                d.drawRoundRect(x - 4, y - 4, 72, 72, 10, TFT_YELLOW);
            }
            return;
        }
    }

    const int bob = (frame / 8) % 2;
    const int cx = x + 32;
    const int cy = y + 34 + bob;
    d.fillCircle(cx, cy, 22, s.color);
    d.drawCircle(cx, cy, 23, TFT_WHITE);
    d.fillCircle(cx - 8, cy - 6, 4, TFT_BLACK);
    d.fillCircle(cx + 8, cy - 6, 4, TFT_BLACK);
    d.drawLine(cx - 8, cy + 10, cx + 8, cy + 10, TFT_BLACK);
    if (species == 8) {
        d.drawLine(cx - 18, cy - 20, cx, cy - 34, TFT_YELLOW);
        d.drawLine(cx + 18, cy - 20, cx, cy - 34, TFT_YELLOW);
    } else if (species == 9) {
        d.drawLine(cx - 16, cy + 20, cx - 8, cy + 30, TFT_BLUE);
        d.drawLine(cx + 16, cy + 20, cx + 8, cy + 30, TFT_BLUE);
    } else if (s.type[0] == 'F' || s.type[0] == 'f') {
        d.fillTriangle(cx + (mirrored ? -24 : 24), cy - 8, cx + (mirrored ? -38 : 38), cy - 18,
                       cx + (mirrored ? -28 : 28), cy + 2, TFT_RED);
    }
    if (flashOn) {
        d.drawRoundRect(x - 2, y - 2, 68, 68, 8, TFT_WHITE);
        d.drawRoundRect(x - 4, y - 4, 72, 72, 10, TFT_YELLOW);
    }
}

void draw_creature_icon(int x, int y, int species, bool seen)
{
    auto& d = gfx();
    if (!seen) {
        d.drawRoundRect(x, y, 34, 30, 4, TFT_DARKGREY);
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.setTextSize(2);
        d.drawCenterString("?", x + 17, y + 6);
        d.setTextSize(1);
        return;
    }

    const Species& s = species_at(species);
    const auto badge = color_badge_for_id(s.id, s.color);
    const int cx = x + 17;
    const int cy = y + 14;
    d.fillCircle(cx, cy, 16, badge.primary);
    d.fillArc(cx, cy, 15, 5, 205, 335, badge.secondary);
    d.fillCircle(cx, cy, 7, badge.accent);
    d.drawCircle(cx, cy, 16, is_owned(species) ? TFT_GREEN : TFT_CYAN);
    d.drawCircle(cx, cy, 10, TFT_BLACK);
    if (s.evolveTo >= 0) {
        d.fillCircle(cx + 10, cy - 10, 2, TFT_YELLOW);
    }
}

void header(const char* title)
{
    auto& d = gfx();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setTextDatum(textdatum_t::top_left);
    d.setTextColor(TFT_ORANGE, TFT_BLACK);
    d.drawString(title, 4, 3);
}

void render_starter()
{
    header("Choose starter");
    auto& d = gfx();
    for (int i = 0; i < kStarterCount; ++i) {
        const int species = starter_index(i);
        const int x = 10 + i * 76;
        const bool selected = i == g.selected;
        d.drawRoundRect(x, 20, 68, 92, 5, selected ? TFT_YELLOW : TFT_DARKGREY);
        draw_creature(x + 2, 22, species, GetHAL().millis() / 120, false);
        d.setTextColor(selected ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
        d.drawCenterString(species_at(species).name, x + 34, 92);
        d.setTextColor(TFT_CYAN, TFT_BLACK);
        d.drawCenterString(species_at(species).type, x + 34, 105);
    }
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.drawString("< > select  Enter confirm", 24, 122);
}

void render_travel()
{
    header("CardPokemon");
    auto& d = gfx();
    const PokemonState& p = g.pokemon[g.active];
    const Species& s = species_at(g.active);
    draw_creature(8, 22, g.active, GetHAL().millis() / 110, false);

    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.drawString(s.name, 88, 24);
    char line[64];
    std::snprintf(line, sizeof(line), "Lv%d  %s", p.level, s.type);
    d.drawString(line, 88, 38);
    std::snprintf(line, sizeof(line), "HP %d/%d", p.hp, max_hp(g.active, p.level));
    d.drawString(line, 88, 52);
    draw_bar(88, 65, 120, 7, p.hp, max_hp(g.active, p.level), TFT_GREEN);
    std::snprintf(line, sizeof(line), "XP %d/%d", p.xp, xp_to_next(p.level));
    d.drawString(line, 88, 75);
    draw_bar(88, 88, 120, 7, p.xp, xp_to_next(p.level), TFT_CYAN);

    std::snprintf(line, sizeof(line), "Dist %.1fkm", g.sensor.distanceM / 1000.0);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.drawString(line, 4, 104);
    draw_heat_wave(88, 102, 140, 14);
    const char* fieldItem = lure_active() ? "Lure" : (repel_active() ? "Repel" : route_label(current_route_state()));
    if (g.sensor.gpsFresh) {
        std::snprintf(line, sizeof(line), "%.1fkmh %s S%d M%.1f", g.sensor.speedKmph, fieldItem, g.sensor.satellites,
                      g.sensor.motionScore);
    } else {
        std::snprintf(line, sizeof(line), "IMU %.1f %s %s", g.sensor.motionScore,
                      g.sensor.capAvailable ? "GPS..." : "noGPS", asset_status_label());
    }
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.drawString(line, 4, 118);
    d.drawRightString("C Center  P Dex", 236, 118);
    std::snprintf(line, sizeof(line), "G%d P%d B%d Gt%d", g.gold, g.potions, g.balls, g.greatBalls);
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.drawRightString(line, 236, 3);
}

void render_battle()
{
    header("Wild encounter");
    auto& d = gfx();
    draw_creature(18, 28, g.battle.wild, GetHAL().millis() / 100, false);
    draw_creature(148, 42, g.active, GetHAL().millis() / 100, true);

    char line[64];
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    std::snprintf(line, sizeof(line), "%s Lv%d", species_at(g.battle.wild).name, g.battle.wildLevel);
    d.drawString(line, 8, 22);
    std::snprintf(line, sizeof(line), "%s Lv%d", species_at(g.active).name, g.pokemon[g.active].level);
    d.drawRightString(line, 236, 22);

    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    if (!g.battle.resolved) {
        d.drawCenterString("Battle resolving...", 120, 112);
    } else if (g.battle.won) {
        const char* ballText = g.battle.caught ? " caught" : (g.battle.ballUsed ? " missed" : "");
        if (g.evolveFlashSpecies == g.active && GetHAL().millis() < g.evolveFlashUntilMs) {
            std::snprintf(line, sizeof(line), "Evolved into %s!", species_at(g.active).name);
        } else if (g.battle.typeEffect > 110) {
            std::snprintf(line, sizeof(line), "Super! +%dxp +%dG%s", g.battle.xp, g.battle.gold, ballText);
        } else if (g.battle.typeEffect < 90) {
            std::snprintf(line, sizeof(line), "Resisted +%dxp +%dG%s", g.battle.xp, g.battle.gold, ballText);
        } else {
            std::snprintf(line, sizeof(line), "Won +%dxp +%dG%s", g.battle.xp, g.battle.gold, ballText);
        }
        d.drawCenterString(line, 120, 112);
    } else {
        d.drawCenterString("Lost ground, escaped", 120, 112);
    }
}

int seen_count()
{
    int count = 0;
    for (int i = 0; i < g.speciesCount; ++i) {
        if (is_seen(i)) {
            ++count;
        }
    }
    return count;
}

void render_pokedex_grid()
{
    header("Pokedex");
    auto& d = gfx();
    char line[64];
    std::snprintf(line, sizeof(line), "%d/%d", g.pokedexCursor + 1, g.speciesCount);
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.drawRightString(line, 236, 3);

    const int pageStart = (g.pokedexCursor / 8) * 8;
    for (int slot = 0; slot < 8; ++slot) {
        const int species = pageStart + slot;
        const int col = slot % 4;
        const int row = slot / 4;
        const int x = 6 + col * 58;
        const int y = 21 + row * 48;
        const bool exists = species < g.speciesCount;
        const bool selected = species == g.pokedexCursor;
        const bool seen = exists && is_seen(species);
        const bool owned = exists && is_owned(species);
        const uint16_t border = selected ? TFT_YELLOW : (owned ? TFT_GREEN : (seen ? TFT_CYAN : TFT_DARKGREY));
        d.drawRoundRect(x, y, 52, 43, 5, border);
        if (exists) {
            draw_creature_icon(x + 9, y + 3, species, seen);
            d.setTextColor(seen ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
            std::snprintf(line, sizeof(line), "No.%03d", species_at(species).id);
            d.drawCenterString(line, x + 26, y + 32);
            if (owned) {
                d.fillCircle(x + 46, y + 6, 3, TFT_GREEN);
            }
        }
    }

    std::snprintf(line, sizeof(line), "Seen %d Got %d", seen_count(), owned_count());
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.drawString(line, 6, 118);
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.drawRightString("Arrows  Enter  Space", 236, 118);
}

void render_pokedex_detail()
{
    header("Pokedex / Party");
    auto& d = gfx();
    const int species = g.pokedexCursor;
    const bool owned = is_owned(species);
    const bool seen = is_seen(species);
    if (seen) {
        draw_creature(8, 30, species, GetHAL().millis() / 130, false);
    } else {
        d.drawRoundRect(16, 38, 48, 48, 6, TFT_DARKGREY);
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.setTextSize(2);
        d.drawCenterString("?", 40, 52);
        d.setTextSize(1);
    }

    char line[64];
    d.setTextColor(seen ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
    std::snprintf(line, sizeof(line), "No.%03d %s", species_at(species).id, seen ? species_at(species).name : "?");
    d.drawString(line, 86, 26);
    std::snprintf(line, sizeof(line), "%s  %s", seen ? species_at(species).type : "?", owned ? "CAUGHT" : (seen ? "SEEN" : "???"));
    d.drawString(line, 86, 40);
    if (owned) {
        std::snprintf(line, sizeof(line), "Lv%d XP %d", g.pokemon[species].level, g.pokemon[species].xp);
        d.drawString(line, 86, 54);
        std::snprintf(line, sizeof(line), "HP %d/%d", g.pokemon[species].hp, max_hp(species, g.pokemon[species].level));
        d.drawString(line, 86, 68);
    }
    std::snprintf(line, sizeof(line), "Seen %d Got %d/%d", seen_count(), owned_count(), g.speciesCount);
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.drawString(line, 86, 88);
    std::snprintf(line, sizeof(line), "SD J%d P%d", g.resourceJsonCount, g.assetPngCount);
    d.setTextColor(g.resourceJsonCount >= g.speciesCount ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
    d.drawString(line, 86, 102);
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.drawString("Enter select", 8, 120);
    d.drawRightString("Space grid", 236, 120);
}

void render_pokedex()
{
    if (g.pokedexMode == PokedexMode::Grid) {
        render_pokedex_grid();
    } else {
        render_pokedex_detail();
    }
}

void render_center()
{
    header("Camp Center");
    auto& d = gfx();
    const PokemonState& p = g.pokemon[g.active];
    char line[64];
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    std::snprintf(line, sizeof(line), "%s Lv%d", species_at(g.active).name, p.level);
    d.drawString(line, 8, 22);
    std::snprintf(line, sizeof(line), "HP %d/%d  G%d", p.hp, max_hp(g.active, p.level), g.gold);
    d.drawString(line, 8, 36);
    std::snprintf(line, sizeof(line), "P%d B%d Gt%d R%d L%d", g.potions, g.balls, g.greatBalls, g.repels, g.lures);
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.drawString(line, 8, 50);
    std::snprintf(line, sizeof(line), "SD %s J%d Cat%d", g.sdMounted ? "OK" : "NO", g.resourceJsonCount, g.speciesCount);
    d.setTextColor(g.resourceJsonCount >= g.speciesCount ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
    d.drawString(line, 8, 62);
    draw_menu_item(78, 0, "Heal team");
    draw_menu_item(94, 1, "Shop");
    draw_menu_item(110, 2, "Bag");

    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.drawString("^ v choose", 8, 122);
    d.drawRightString("Enter  Space back", 236, 122);
}

void render_shop()
{
    header("Shop");
    auto& d = gfx();
    char line[64];
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    std::snprintf(line, sizeof(line), "Gold %d", g.gold);
    d.drawString(line, 8, 24);
    draw_menu_item(40, 0, "Potion 10G");
    draw_menu_item(54, 1, "Poke Ball 8G");
    draw_menu_item(68, 2, "Great Ball 18G");
    draw_menu_item(82, 3, "Repel 15G");
    draw_menu_item(96, 4, "Lure 20G");

    d.setTextColor(TFT_WHITE, TFT_BLACK);
    std::snprintf(line, sizeof(line), "P%d B%d Gt%d R%d L%d", g.potions, g.balls, g.greatBalls, g.repels, g.lures);
    d.drawString(line, 8, 110);
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.drawString("^ v choose", 8, 122);
    d.drawRightString("Enter buy  Space", 236, 122);
}

void render_bag()
{
    header("Bag");
    auto& d = gfx();
    const PokemonState& p = g.pokemon[g.active];
    char line[64];
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    std::snprintf(line, sizeof(line), "%s HP %d/%d", species_at(g.active).name, p.hp, max_hp(g.active, p.level));
    d.drawString(line, 8, 24);
    draw_menu_item(44, 0, "Use Potion");
    draw_menu_item(58, 1, "Use Repel");
    draw_menu_item(72, 2, "Use Lure");
    draw_menu_item(86, 3, "Balls auto");
    draw_menu_item(100, 4, "Back");
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    std::snprintf(line, sizeof(line), "P%d B%d Gt%d R%d L%d", g.potions, g.balls, g.greatBalls, g.repels, g.lures);
    d.drawString(line, 8, 112);
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.drawString("^ v choose", 8, 122);
    d.drawRightString("Enter  Space", 236, 122);
}

void render()
{
    switch (g.scene) {
        case Scene::Starter:
            render_starter();
            break;
        case Scene::Travel:
            render_travel();
            break;
        case Scene::Battle:
            render_battle();
            break;
        case Scene::Pokedex:
            render_pokedex();
            break;
        case Scene::Center:
            render_center();
            break;
        case Scene::Shop:
            render_shop();
            break;
        case Scene::Bag:
            render_bag();
            break;
    }
    gFrame.pushSprite(0, 0);
}

void buy_shop_item()
{
    if (g.menuCursor == 0) {
        if (g.gold < 10 || g.potions >= 9) {
            add_log(g.gold < 10 ? "Need 10G" : "Potion full");
            return;
        }
        g.gold -= 10;
        ++g.potions;
        add_log("Bought Potion");
    } else if (g.menuCursor == 1) {
        if (g.gold < 8 || g.balls >= 99) {
            add_log(g.gold < 8 ? "Need 8G" : "Ball full");
            return;
        }
        g.gold -= 8;
        ++g.balls;
        add_log("Bought Ball");
    } else if (g.menuCursor == 2) {
        if (g.gold < 18 || g.greatBalls >= 99) {
            add_log(g.gold < 18 ? "Need 18G" : "Great full");
            return;
        }
        g.gold -= 18;
        ++g.greatBalls;
        add_log("Bought Great");
    } else if (g.menuCursor == 3) {
        if (g.gold < 15 || g.repels >= 9) {
            add_log(g.gold < 15 ? "Need 15G" : "Repel full");
            return;
        }
        g.gold -= 15;
        ++g.repels;
        add_log("Bought Repel");
    } else if (g.menuCursor == 4) {
        if (g.gold < 20 || g.lures >= 9) {
            add_log(g.gold < 20 ? "Need 20G" : "Lure full");
            return;
        }
        g.gold -= 20;
        ++g.lures;
        add_log("Bought Lure");
    }
    save_game();
}

bool use_repel()
{
    if (g.repels == 0) {
        add_log("No Repel");
        return false;
    }
    --g.repels;
    g.repelUntilMs = GetHAL().millis() + kFieldItemDurationMs;
    g.lureUntilMs = 0;
    g.encounterHeat = std::min(g.encounterHeat, 22.0f);
    g.sensor.nextEncounterTargetM = std::max(g.sensor.distanceSinceEventM + 60.0, roll_next_encounter_target());
    add_log("Repel active");
    save_game();
    return true;
}

bool use_lure()
{
    if (g.lures == 0) {
        add_log("No Lure");
        return false;
    }
    --g.lures;
    g.lureUntilMs = GetHAL().millis() + kFieldItemDurationMs;
    g.repelUntilMs = 0;
    g.encounterHeat = std::max(g.encounterHeat, 45.0f);
    g.sensor.nextEncounterTargetM = std::min(g.sensor.nextEncounterTargetM, g.sensor.distanceSinceEventM + roll_next_encounter_target() * 0.75);
    add_log("Lure active");
    save_game();
    return true;
}

void handle_key(KeScanCode_t key)
{
    if (key == KEY_NONE) {
        return;
    }

    switch (g.scene) {
        case Scene::Starter:
            if (key == KEY_LEFT) {
                g.selected = (g.selected + 2) % 3;
            } else if (key == KEY_RIGHT) {
                g.selected = (g.selected + 1) % 3;
            } else if (key == KEY_ENTER) {
                choose_starter(starter_index(g.selected));
            }
            break;
        case Scene::Travel:
            if (key == KEY_P) {
                g.previousScene = g.scene;
                g.pokedexCursor = g.active;
                g.pokedexMode = PokedexMode::Grid;
                g.scene = Scene::Pokedex;
            } else if (key == KEY_C) {
                g.menuCursor = 0;
                g.scene = Scene::Center;
            }
            break;
        case Scene::Battle:
            if (g.battle.resolved && (key == KEY_ENTER || key == KEY_SPACE)) {
                g.scene = Scene::Travel;
            }
            break;
        case Scene::Pokedex:
            if (g.pokedexMode == PokedexMode::Grid) {
                if (key == KEY_LEFT) {
                    g.pokedexCursor = std::max(0, g.pokedexCursor - 1);
                } else if (key == KEY_RIGHT) {
                    g.pokedexCursor = std::min(std::max(0, g.speciesCount - 1), g.pokedexCursor + 1);
                } else if (key == KEY_UP) {
                    g.pokedexCursor = std::max(0, g.pokedexCursor - 4);
                } else if (key == KEY_DOWN) {
                    g.pokedexCursor = std::min(std::max(0, g.speciesCount - 1), g.pokedexCursor + 4);
                } else if (key == KEY_ENTER && is_seen(g.pokedexCursor)) {
                    g.pokedexMode = PokedexMode::Detail;
                } else if (key == KEY_SPACE || key == KEY_ESC) {
                    g.scene = Scene::Travel;
                }
            } else {
                if (key == KEY_ENTER && is_owned(g.pokedexCursor)) {
                    g.active = g.pokedexCursor;
                    add_log("Selected %s", species_at(g.active).name);
                    save_game();
                    g.scene = Scene::Travel;
                } else if (key == KEY_SPACE || key == KEY_ESC) {
                    g.pokedexMode = PokedexMode::Grid;
                }
            }
            break;
        case Scene::Center:
            if (key == KEY_UP) {
                g.menuCursor = (g.menuCursor + 2) % 3;
            } else if (key == KEY_DOWN) {
                g.menuCursor = (g.menuCursor + 1) % 3;
            } else if (key == KEY_ENTER) {
                if (g.menuCursor == 0) {
                    heal_owned_team();
                    add_log("Team healed");
                    save_game();
                    g.scene = Scene::Travel;
                } else if (g.menuCursor == 1) {
                    g.menuCursor = 0;
                    g.scene = Scene::Shop;
                } else {
                    g.menuCursor = 0;
                    g.scene = Scene::Bag;
                }
            } else if (key == KEY_SPACE || key == KEY_ESC) {
                g.scene = Scene::Travel;
            }
            break;
        case Scene::Shop:
            if (key == KEY_UP) {
                g.menuCursor = (g.menuCursor + 4) % 5;
            } else if (key == KEY_DOWN) {
                g.menuCursor = (g.menuCursor + 1) % 5;
            } else if (key == KEY_ENTER) {
                buy_shop_item();
            } else if (key == KEY_SPACE || key == KEY_ESC) {
                g.menuCursor = 1;
                g.scene = Scene::Center;
            }
            break;
        case Scene::Bag:
            if (key == KEY_UP) {
                g.menuCursor = (g.menuCursor + 4) % 5;
            } else if (key == KEY_DOWN) {
                g.menuCursor = (g.menuCursor + 1) % 5;
            } else if (key == KEY_ENTER) {
                if (g.menuCursor == 0) {
                    use_potion_on_active();
                } else if (g.menuCursor == 1) {
                    use_repel();
                } else if (g.menuCursor == 2) {
                    use_lure();
                } else if (g.menuCursor == 3) {
                    add_log("Best Ball after wins");
                } else {
                    g.menuCursor = 0;
                    g.scene = Scene::Center;
                }
            } else if (key == KEY_SPACE || key == KEY_ESC) {
                g.menuCursor = 2;
                g.scene = Scene::Center;
            }
            break;
    }
}

void update_game()
{
    const uint32_t now = GetHAL().millis();

    const auto key = GetHAL().keyboard.getLatestKeyEvent();
    if (key.state && !key.isModifier) {
        handle_key(key.keyCode);
        GetHAL().keyboard.clearKeyEvent();
    }

    if (g.scene == Scene::Travel && elapsed_ms(now, g.lastSensorMs) >= kSensorIntervalMs) {
        update_sensors();
        if (should_start_wild_encounter(now)) {
            start_battle();
            return;
        }
    }

    if (g.scene == Scene::Battle && !g.battle.resolved && elapsed_ms(now, g.battle.startedMs) > 1400) {
        resolve_battle();
        return;
    }
    if (g.scene == Scene::Battle && g.battle.resolved && elapsed_ms(now, g.battle.resolvedMs) > 3000) {
        g.scene = Scene::Travel;
    }

    if (elapsed_ms(now, g.lastSaveMs) > 30000 && g.scene != Scene::Starter) {
        save_game();
    }
}

void boot_game()
{
    auto& hal = GetHAL();
    hal.init();
    hal.display.setBrightness(150);
    hal.display.setTextSize(1);
    hal.display.setTextDatum(textdatum_t::top_left);
    hal.display.fillScreen(TFT_BLACK);
    const auto logoLen = static_cast<uint32_t>(cardpokemon_logo_png_end - cardpokemon_logo_png_start);
    if (!hal.display.drawPng(cardpokemon_logo_png_start, logoLen, 0, 0)) {
        hal.display.setTextColor(TFT_ORANGE, TFT_BLACK);
        hal.display.drawCenterString("CardPokemon", kScreenW / 2, 50);
        hal.display.setTextColor(TFT_WHITE, TFT_BLACK);
        hal.display.drawCenterString("Road Adventure", kScreenW / 2, 72);
    }

    gFrame.setColorDepth(16);
    gFrame.createSprite(kScreenW, kScreenH);
    gFrame.setTextSize(1);
    gFrame.setTextDatum(textdatum_t::top_left);

    g.rng ^= hal.millis();
    auto mac = hal.getDeviceMac();
    for (uint8_t b : mac) {
        g.rng = (g.rng * 33u) ^ b;
    }

    g.sensor.imuAvailable = hal.imu.begin();
    g.sensor.capAvailable = hal.capLora868.init();
    auto sd = hal.sdCardProbe();
    g.sdMounted = sd.is_mounted;
    init_default_catalog();
    const bool foundResourceRoot = g.sdMounted && select_resource_root();
    bool loadedCatalog = load_catalog_from_pokemon_jsons();
    if (!loadedCatalog) {
        loadedCatalog = load_catalog_from_manifest();
    }
    refresh_asset_status();
    load_game();
    if (has_owned() && is_owned(g.active)) {
        g.scene = Scene::Travel;
    }
    g.sensor.distanceSinceEventM = 0.0;
    g.sensor.lastGpsStepM = 0.0;
    g.sensor.lastStepM = 0.0;
    g.encounterHeat = 12.0f;
    g.sensor.nextEncounterTargetM = roll_next_encounter_target();
    g.heatHistory.fill(static_cast<uint8_t>(g.encounterHeat));
    g.heatCursor = 0;
    g.lastEncounterMs = hal.millis() - kEncounterCooldownMs;
    add_log(g.sdMounted ? "SD mounted" : "No SD resource pack");
    add_log(loadedCatalog ? "SD catalog loaded" : (foundResourceRoot ? "Catalog parse failed" : "Built-in catalog"));
    add_log("JSON %d Cat %d", g.resourceJsonCount, g.speciesCount);
    add_log("PNG %d/%d", g.assetPngCount, g.speciesCount);
    hal.delay(900);
}

}  // namespace

extern "C" void app_main(void)
{
    ESP_LOGI(kTag, "CardPokemon boot");
    boot_game();

    while (true) {
        GetHAL().feedTheDog();
        GetHAL().update();
        update_game();

        const uint32_t now = GetHAL().millis();
        if (now - g.lastRenderMs >= kRenderIntervalMs) {
            render();
            g.lastRenderMs = now;
        }
    }
}
