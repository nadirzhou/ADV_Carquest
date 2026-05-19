/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <mooncake.h>
#include <cstdint>
#include <string>
#include <vector>

struct MonsterTemplate {
    const char* name;
    int minLevel;
    int maxLevel;
    int baseDanger;
    int traitPower;
    int rewardType;
};

class AppSignalQuest : public mooncake::AppAbility {
public:
    AppSignalQuest();
    ~AppSignalQuest();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    enum class Mode {
        Moving,
        Camp,
    };

    struct Player {
        int level      = 1;
        int hp         = 30;
        int maxHp      = 30;
        int atk        = 6;
        int def        = 3;
        int focus      = 3;
        int luck       = 2;
        int xp         = 0;
        int gold       = 0;
        int potions    = 2;
        int scrap      = 0;
        int keys       = 0;
        int relics     = 0;
        int weaponTier = 0;
        int armorTier  = 0;
        int statusFlags = 0;
        int totalDistanceM = 0;
        int defeatedMonsters = 0;
        int openedChests = 0;
    };

    struct SensorState {
        bool gpsValid       = false;
        double lat          = 0.0;
        double lng          = 0.0;
        double speedKmph    = 0.0;
        double distanceM    = 0.0;
        float motionScore   = 0.0f;
        uint32_t sats       = 0;
        uint32_t stoppedMs  = 0;
        bool capAvailable   = false;
        bool imuAvailable   = false;
        bool gravityReady    = false;
        float gravityX       = 0.0f;
        float gravityY       = 0.0f;
        float gravityZ       = 1.0f;
    };

    Player _player;
    SensorState _sensor;
    Mode _mode = Mode::Camp;

    uint32_t _tick_ms               = 0;
    uint32_t _render_ms             = 0;
    uint32_t _last_sensor_ms        = 0;
    uint32_t _last_beacon_ms        = 0;
    int _key_slot_id                = -1;
    uint32_t _opt_hold_start_ms     = 0;
    bool _help_visible              = false;
    bool _opt_toggle_latched        = false;
    int _help_scroll                = 0;
    bool _has_last_fix              = false;
    double _last_lat                = 0.0;
    double _last_lng                = 0.0;
    double _next_event_distance_m   = 180.0;
    double _session_distance_m      = 0.0;
    uint32_t _rng                   = 0x5EED1234;
    int _selected_inventory_line    = 0;
    std::vector<std::string> _log;
    std::string _last_event = "Awaiting signal...";

    void update_sensors();
    void update_game();
    void render();
    void render_hud();
    void render_log(int y);
    void render_camp_screen();
    void render_camp_panel();
    void render_help_page();
    void handle_key(int key_code, const char* key_name);
    void handle_help_key(int key_code);
    void handle_camp_selection();
    void update_help_state();
    void load_save_data();
    void save_data();
    std::string fit_text(const std::string& text, std::size_t max_chars) const;
    void add_log(const std::string& line);
    void trigger_exploration_event();
    void encounter_monster(const MonsterTemplate& monster, int level, int danger);
    void find_treasure(int tier);
    void trigger_hazard(int risk);
    void trigger_shrine(int tier);
    void level_up_if_needed();
    int xp_to_next_level() const;
    int drive_bonus() const;
    int guard_value() const;
    int loot_score(int tier);
    void use_potion();
    void rest();
    void craft_upgrade();
    void open_cached_chest();
    void send_lora_beacon();
    int roll(int min_value, int max_value);
    uint32_t rand_u32();
    const char* mode_label() const;
};
