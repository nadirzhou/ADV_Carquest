/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_signal_quest.h"
#include "assets/codex_big.h"
#include "assets/codex_small.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/common.h>
#include <apps/utils/theme.h>
#include <hal.h>
#include <mooncake_log.h>
#include <algorithm>
#include <cmath>

using namespace mooncake;

namespace {
constexpr uint32_t kSensorIntervalMs = 1000;
constexpr uint32_t kRenderIntervalMs = 350;
constexpr uint32_t kHelpHoldMs       = 650;
constexpr uint32_t kStoppedToCampMs  = 15000;
constexpr double kMoveSpeedKmph      = 2.0;
constexpr double kStopSpeedKmph      = 0.8;
constexpr float kMoveMotionScore     = 2.2f;
constexpr float kStopMotionScore     = 1.0f;
constexpr double kMaxGpsStepM        = 350.0;
constexpr double kMinGpsStepM        = 1.2;
constexpr int kMaxLogLines           = 7;
constexpr int kExploreX              = 2;
constexpr int kExploreTopY           = 2;
constexpr int kExploreLineH          = 17;
constexpr int kExploreLogRow         = 4;
constexpr uint32_t kCampToastMs      = 2000;

enum StatusFlag {
    STATUS_WOUNDED = 1 << 0,
    STATUS_SHAKEN  = 1 << 1,
    STATUS_JAMMED  = 1 << 2,
    STATUS_INSPIRED = 1 << 3,
    STATUS_GUARDED = 1 << 4,
    STATUS_LUCKY   = 1 << 5,
};

enum QuestFlag {
    QUEST_MILE_REWARD  = 1 << 0,
    QUEST_CACHE_REWARD = 1 << 1,
    QUEST_HUNT_REWARD  = 1 << 2,
};

constexpr MonsterTemplate kRoadWraiths[] = {
    {"Mile Wisp", 1, 3, 1, 0, 0},
    {"Road Wraith", 2, 6, 5, 1, 1},
    {"Fog Rider", 4, 8, 7, 2, 2},
    {"Route King", 8, 10, 12, 4, 3},
};

constexpr MonsterTemplate kSignalImps[] = {
    {"Signal Imp", 1, 5, 2, 0, 0},
    {"Packet Biter", 2, 6, 4, 1, 1},
    {"Static Hexer", 3, 7, 6, 2, 2},
    {"LoRa Hexer", 6, 10, 10, 3, 3},
};

constexpr MonsterTemplate kAsphaltBeasts[] = {
    {"Tar Slime", 1, 3, 2, 0, 1},
    {"Rumble Hound", 2, 5, 4, 2, 0},
    {"Guard Drake", 4, 8, 8, 3, 2},
    {"Overpass Hydra", 8, 10, 13, 5, 3},
};

constexpr MonsterTemplate kRustCultists[] = {
    {"Rust Acolyte", 1, 4, 3, 0, 1},
    {"Wrench Raider", 3, 6, 5, 1, 0},
    {"Scrap Knight", 5, 9, 8, 3, 2},
    {"Chrome Prophet", 8, 10, 12, 4, 3},
};
}  // namespace

AppSignalQuest::AppSignalQuest()
{
    setAppInfo().name     = "CarQuest";
    setAppInfo().userData = new AppIcon_t(image_data_codex_big, image_data_codex_small);
}

AppSignalQuest::~AppSignalQuest()
{
    delete static_cast<AppIcon_t*>(getAppInfo().userData);
}

void AppSignalQuest::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    GetHAL().canvas.setBaseColor(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_REPL);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setTextScroll(false);
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 0);
    GetHAL().canvas.setTextColor(TFT_ORANGE);
    GetHAL().canvas.println("CarQuest");
    GetHAL().canvas.setTextColor(TFT_WHITE);
    GetHAL().canvas.println("Init sensors...");
    GetHAL().pushCanvas();

    _sensor.capAvailable = GetHAL().capLora868.init();
    _sensor.imuAvailable = GetHAL().imu.begin();
    _tick_ms             = GetHAL().millis();
    _last_sensor_ms      = 0;
    _render_ms           = 0;
    _last_beacon_ms      = 0;
    _rng ^= GetHAL().millis();
    auto mac = GetHAL().getDeviceMac();
    _rng ^= (static_cast<uint32_t>(mac[3]) << 16) | (static_cast<uint32_t>(mac[4]) << 8) | mac[5];
    load_save_data();

    add_log("Camp ready. Move to explore.");
    add_log("Hold OPT for controls.");

    _key_slot_id = GetHAL().keyboard.onKeyEvent.connect([this](const Keyboard::KeyEvent_t& keyEvent) {
        if (keyEvent.isModifier || keyEvent.state == false) {
            return;
        }
        if (_help_visible) {
            handle_help_key(keyEvent.keyCode);
            return;
        }
        handle_key(keyEvent.keyCode, keyEvent.keyName);
    });
}

void AppSignalQuest::onRunning()
{
    const auto now = GetHAL().millis();
    update_help_state();

    if (now - _last_sensor_ms >= kSensorIntervalMs) {
        update_sensors();
        update_game();
        _last_sensor_ms = now;
    }

    if (now - _render_ms >= kRenderIntervalMs) {
        render();
        _render_ms = now;
    }

    if (GetHAL().homeButton.wasClicked()) {
        audio::play_random_tone();
        close();
    }
}

void AppSignalQuest::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    if (_key_slot_id >= 0) {
        GetHAL().keyboard.onKeyEvent.disconnect(_key_slot_id);
        _key_slot_id = -1;
    }
    save_data();
}

void AppSignalQuest::update_sensors()
{
    const auto now = GetHAL().millis();
    const auto dt  = _last_sensor_ms == 0 ? kSensorIntervalMs : now - _last_sensor_ms;

    _sensor.motionScore = 0.0f;
    if (_sensor.imuAvailable && GetHAL().imu.update()) {
        auto imu = GetHAL().imu.getImuData();
        float ax = imu.accel.x;
        float ay = imu.accel.y;
        float az = imu.accel.z;
        const auto accel_mag = std::sqrt(ax * ax + ay * ay + az * az);
        if (accel_mag > 3.0f) {
            ax /= 9.80665f;
            ay /= 9.80665f;
            az /= 9.80665f;
        }

        if (!_sensor.gravityReady) {
            _sensor.gravityX     = ax;
            _sensor.gravityY     = ay;
            _sensor.gravityZ     = az;
            _sensor.gravityReady = true;
        } else {
            constexpr float alpha = 0.08f;
            _sensor.gravityX = _sensor.gravityX * (1.0f - alpha) + ax * alpha;
            _sensor.gravityY = _sensor.gravityY * (1.0f - alpha) + ay * alpha;
            _sensor.gravityZ = _sensor.gravityZ * (1.0f - alpha) + az * alpha;
        }

        const auto linear_x   = ax - _sensor.gravityX;
        const auto linear_y   = ay - _sensor.gravityY;
        const auto linear_z   = az - _sensor.gravityZ;
        const auto linear_mag = std::sqrt(linear_x * linear_x + linear_y * linear_y + linear_z * linear_z);
        const auto gyro_mag   = std::sqrt(imu.gyro.x * imu.gyro.x + imu.gyro.y * imu.gyro.y + imu.gyro.z * imu.gyro.z);
        _sensor.motionScore  = std::clamp(linear_mag * 12.0f + std::min(gyro_mag, 250.0f) * 0.006f, 0.0f, 9.9f);
    }

    bool moving         = false;
    bool clearlyStopped = _sensor.motionScore < kStopMotionScore;
    if (_sensor.capAvailable) {
        auto gps = GetHAL().capLora868.borrowGPS();
        if (gps) {
            _sensor.gpsValid = gps->location.isValid();
            _sensor.speedKmph = gps->speed.isValid() ? gps->speed.kmph() : 0.0;
            _sensor.sats      = gps->satellites.isValid() ? gps->satellites.value() : 0;

            if (_sensor.gpsValid) {
                _sensor.lat = gps->location.lat();
                _sensor.lng = gps->location.lng();
                if (_has_last_fix) {
                    const double step =
                        TinyGPSPlus::distanceBetween(_last_lat, _last_lng, _sensor.lat, _sensor.lng);
                    if (step >= kMinGpsStepM && step <= kMaxGpsStepM) {
                        _sensor.distanceM += step;
                        _session_distance_m += step;
                        _player.totalDistanceM += static_cast<int>(step);
                    }
                }
                _last_lat      = _sensor.lat;
                _last_lng      = _sensor.lng;
                _has_last_fix  = true;
            }

            moving         = _sensor.speedKmph >= kMoveSpeedKmph || _sensor.motionScore > kMoveMotionScore;
            clearlyStopped = _sensor.speedKmph <= kStopSpeedKmph && _sensor.motionScore < kStopMotionScore;
            GetHAL().capLora868.returnGPS();
        }
    } else if (_sensor.motionScore > kMoveMotionScore) {
        const double simulated_step = std::min(12.0, static_cast<double>(_sensor.motionScore) * 1.4);
        _sensor.distanceM += simulated_step;
        _session_distance_m += simulated_step;
        _player.totalDistanceM += static_cast<int>(simulated_step);
        _sensor.speedKmph = simulated_step * 3600.0 / static_cast<double>(dt);
        moving            = true;
    }

    if (moving) {
        _sensor.stoppedMs = 0;
        _mode             = Mode::Moving;
    } else if (clearlyStopped) {
        _sensor.stoppedMs += dt;
        if (_sensor.stoppedMs >= kStoppedToCampMs) {
            _mode = Mode::Camp;
        }
    } else {
        _sensor.stoppedMs = 0;
    }
}

void AppSignalQuest::update_game()
{
    if (_mode == Mode::Moving) {
        if (_sensor.distanceM >= _next_event_distance_m) {
            trigger_exploration_event();
            _next_event_distance_m = _sensor.distanceM + static_cast<double>(roll(160, 360));
        }
        send_lora_beacon();
    }
}

void AppSignalQuest::trigger_exploration_event()
{
    const int risk = std::clamp(static_cast<int>(_sensor.motionScore * 10.0f) + roll(0, 40), 0, 100);
    if (risk >= 89) {
        const auto& monster = kRoadWraiths[3];
        encounter_monster(monster, std::clamp(_player.level + roll(1, 2), monster.minLevel, monster.maxLevel),
                          monster.baseDanger + 7);
    } else if (risk >= 71) {
        const MonsterTemplate* families[] = {kRoadWraiths, kSignalImps, kAsphaltBeasts, kRustCultists};
        const auto& monster = families[roll(0, 3)][roll(1, 3)];
        encounter_monster(monster, std::clamp(_player.level + roll(0, 2), monster.minLevel, monster.maxLevel),
                          monster.baseDanger + 4);
    } else if (risk >= 46) {
        const MonsterTemplate* families[] = {kRoadWraiths, kSignalImps, kAsphaltBeasts, kRustCultists};
        const auto& monster = families[roll(0, 3)][roll(0, 2)];
        encounter_monster(monster, std::clamp(_player.level + roll(-1, 1), monster.minLevel, monster.maxLevel),
                          monster.baseDanger + 2);
    } else if (risk >= 26) {
        if (roll(0, 100) < 45) {
            trigger_hazard(risk);
        } else {
            find_treasure(std::max(1, _player.level + (_sensor.motionScore > 2.0f ? 1 : 0)));
        }
    } else {
        if (roll(0, 100) < 30) {
            trigger_shrine(std::max(1, _player.level));
        } else {
            find_treasure(std::max(1, _player.level));
        }
    }
}

void AppSignalQuest::encounter_monster(const MonsterTemplate& monster, int level, int danger)
{
    const int inspired = (_player.statusFlags & STATUS_INSPIRED) ? 2 : 0;
    const int weapon_power = _player.weaponTier * 2;
    const int player_power =
        _player.level * 6 + _player.atk + weapon_power + drive_bonus() + inspired + roll(0, 10);
    const int monster_power = level * 7 + danger + monster.traitPower + roll(0, 8);

    if (player_power >= monster_power) {
        int xp         = 4 + level * 3 + danger;
        int gold       = roll(1, 4 + level * 2);
        const bool crit = player_power >= monster_power + 8;
        if (crit) {
            xp += std::max(1, xp / 4);
            gold += std::max(1, gold / 4);
        }
        _player.xp += xp;
        _player.gold += gold;
        if (monster.rewardType == 1 || roll(0, 100) + _player.luck > 78) {
            _player.scrap++;
        }
        if (monster.rewardType >= 2 && roll(0, 100) + _player.luck > 82) {
            _player.keys++;
        }
        if (monster.rewardType >= 3 && roll(0, 100) + _player.luck > 86) {
            _player.relics++;
        }
        _player.statusFlags &= ~(STATUS_INSPIRED | STATUS_GUARDED | STATUS_LUCKY);
        _player.defeatedMonsters++;
        _last_event = fmt::format("{} {}", crit ? "CRIT" : "Defeated", monster.name);
        add_log(fmt::format("+{}xp +{}g {}", xp, gold, monster.name));
        level_up_if_needed();
        save_data();
    } else {
        const bool critical_failure = monster_power >= player_power + 10;
        int damage = std::max(1, monster_power - player_power / 2 - guard_value() / 2);
        if (critical_failure) {
            damage += std::max(1, level / 2);
            _player.statusFlags |= STATUS_WOUNDED;
        }
        _player.hp       = std::max(0, _player.hp - damage);
        if (monster.rewardType == 2 || monster.rewardType == 3) {
            _player.statusFlags |= STATUS_SHAKEN;
        }
        _last_event      = fmt::format("{} hit you", monster.name);
        add_log(fmt::format("-{}hp vs {}", damage, monster.name));
        if (_player.hp == 0) {
            _player.hp      = std::max(1, _player.maxHp / 2);
            _player.gold    = std::max(0, _player.gold - 5);
            _last_event     = "Rescued at roadside camp";
            _mode           = Mode::Camp;
            _sensor.stoppedMs = kStoppedToCampMs;
            add_log("KO: lost 5g, camp forced");
        }
        save_data();
    }
}

void AppSignalQuest::find_treasure(int tier)
{
    const int score  = loot_score(tier);
    const int reward = roll(2 + tier, 6 + tier * 3);
    _player.gold += reward;
    if (score >= 101) {
        _player.relics++;
        _player.keys++;
        add_log(fmt::format("Legend cache +{}g", reward));
        _last_event = "Ancient glovebox found";
    } else if (score >= 90) {
        _player.relics++;
        add_log(fmt::format("Relic cache +{}g", reward));
        _last_event = "Relic fragment found";
    } else if (score >= 75) {
        _player.scrap += 2;
        if (roll(0, 100) < 35) {
            _player.armorTier++;
            _player.def++;
            add_log(fmt::format("Armor tier {}", _player.armorTier));
        } else {
            add_log(fmt::format("Gear cache +{}g +scrap", reward));
        }
        _last_event = "Gear cache found";
    } else if (score >= 60) {
        _player.keys++;
        add_log(fmt::format("Found cache: +{}g +key", reward));
        _last_event = "Iron cache found";
    } else if (score >= 40) {
        _player.potions++;
        add_log(fmt::format("Found cache: +{}g +potion", reward));
        _last_event = "Potion cache found";
    } else {
        _player.scrap++;
        add_log(fmt::format("Found cache: +{}g +scrap", reward));
        _last_event = "Road cache found";
    }
    save_data();
}

void AppSignalQuest::trigger_hazard(int risk)
{
    const int difficulty = 12 + risk / 6;
    const int score = _player.def + _player.focus + guard_value() + drive_bonus() + roll(0, 10);
    if (score >= difficulty + 8) {
        _player.scrap++;
        _last_event = "Hazard salvaged";
        add_log("+scrap from road omen");
    } else if (score >= difficulty) {
        _last_event = "Hazard avoided";
        add_log("Clean pass through danger");
    } else {
        const int damage = std::max(1, difficulty - score);
        _player.hp = std::max(0, _player.hp - damage);
        _player.statusFlags |= STATUS_JAMMED;
        _last_event = "Cursed pothole";
        add_log(fmt::format("-{}hp road hazard", damage));
        if (_player.hp == 0) {
            _player.hp = std::max(1, _player.maxHp / 2);
            _player.gold = std::max(0, _player.gold - 5);
            _mode = Mode::Camp;
            _sensor.stoppedMs = kStoppedToCampMs;
            add_log("KO: lost 5g, camp forced");
        }
    }
    save_data();
}

void AppSignalQuest::trigger_shrine(int tier)
{
    const int roll_value = loot_score(tier);
    if (roll_value > 82) {
        _player.statusFlags |= STATUS_LUCKY;
        _last_event = "Mile shrine: Lucky";
        add_log("Next loot roll blessed");
    } else if (roll_value > 55) {
        const int heal = 4 + _player.level;
        _player.hp = std::min(_player.maxHp, _player.hp + heal);
        _last_event = "Mile shrine healed";
        add_log(fmt::format("Shrine +{}hp", heal));
    } else {
        _player.statusFlags |= STATUS_INSPIRED;
        _last_event = "Road omen inspires";
        add_log("Next battle +2 power");
    }
    save_data();
}

void AppSignalQuest::level_up_if_needed()
{
    while (_player.level < 10) {
        const int need = xp_to_next_level();
        if (_player.xp < need) {
            break;
        }
        _player.xp -= need;
        _player.level++;
        _player.maxHp += 6;
        _player.hp = _player.maxHp;
        _player.atk++;
        if (_player.level % 2 == 0) {
            _player.def++;
        }
        if (_player.level % 3 == 0) {
            if (_player.luck <= _player.focus) {
                _player.luck++;
            } else {
                _player.focus++;
            }
        }
        add_log(fmt::format("LEVEL UP -> {}", _player.level));
    }
}

int AppSignalQuest::xp_to_next_level() const
{
    if (_player.level <= 2) {
        return _player.level * 18;
    }
    return 16 + _player.level * 18 + (_player.level * _player.level * 2);
}

int AppSignalQuest::drive_bonus() const
{
    const int relic_bonus = _player.equippedRelic == 3 ? 1 : 0;
    return std::clamp(static_cast<int>(_sensor.motionScore * 2.0f) + relic_bonus, 0, 9);
}

int AppSignalQuest::guard_value() const
{
    const int guarded = (_player.statusFlags & STATUS_GUARDED) ? 2 : 0;
    const int relic_bonus = _player.equippedRelic == 2 ? 1 : 0;
    return _player.def + _player.armorTier + guarded + relic_bonus;
}

int AppSignalQuest::loot_score(int tier)
{
    const int lucky = (_player.statusFlags & STATUS_LUCKY) ? 10 : 0;
    const int relic_bonus = _player.equippedRelic == 1 ? 4 : 0;
    _player.statusFlags &= ~STATUS_LUCKY;
    return tier + _player.luck + lucky + relic_bonus + roll(0, 100);
}


void AppSignalQuest::handle_key(int key_code, const char* key_name)
{
    if (_mode == Mode::Camp && (key_code == KEY_UP || key_code == KEY_DOWN || key_code == KEY_LEFT || key_code == KEY_RIGHT ||
                                key_code == KEY_ENTER)) {
        handle_camp_key(key_code);
    } else if (key_code == KEY_I) {
        use_potion();
    } else if (key_code == KEY_R) {
        rest();
    } else if (key_code == KEY_U) {
        craft_upgrade();
    } else if (key_code == KEY_O) {
        open_cached_chest();
    } else if (key_code == KEY_T) {
        trigger_exploration_event();
    } else if (key_code == KEY_M) {
        _sensor.distanceM += 50.0;
        _session_distance_m += 50.0;
        _mode             = Mode::Moving;
        _sensor.stoppedMs = 0;
        add_log("Simulated +50m movement");
    } else if (key_code == KEY_C) {
        _mode             = Mode::Camp;
        _sensor.stoppedMs = kStoppedToCampMs;
        add_log("Manual camp mode");
    } else if (key_code == KEY_SPACE) {
        _mode = _mode == Mode::Camp ? Mode::Moving : Mode::Camp;
        add_log(fmt::format("Mode -> {}", mode_label()));
    }

    (void)key_name;
    render();
}

void AppSignalQuest::use_potion()
{
    if (_player.potions <= 0) {
        add_log("No potion.");
        return;
    }
    _player.potions--;
    const int heal = 12 + _player.level * 2;
    _player.hp     = std::min(_player.maxHp, _player.hp + heal);
    add_log(fmt::format("Potion +{}hp", heal));
    save_data();
}

void AppSignalQuest::rest()
{
    if (_mode != Mode::Camp) {
        add_log("Rest only at camp.");
        return;
    }
    const int heal = 4 + _player.level;
    _player.hp = std::min(_player.maxHp, _player.hp + heal);
    _player.statusFlags &= ~(STATUS_WOUNDED | STATUS_SHAKEN | STATUS_JAMMED);
    add_log(fmt::format("Camp rest +{}hp", heal));
    save_data();
}

void AppSignalQuest::craft_upgrade()
{
    const int cost = 2 + _player.weaponTier + (_player.armorTier > _player.weaponTier ? 1 : 0);
    if (_player.scrap < cost) {
        add_log(fmt::format("Need {} scrap for upgrade", cost));
        return;
    }
    _player.scrap -= cost;
    if (_player.weaponTier <= _player.armorTier) {
        _player.weaponTier++;
        _player.atk += 2;
        add_log(fmt::format("Weapon tier {}", _player.weaponTier));
    } else {
        _player.armorTier++;
        _player.def++;
        add_log(fmt::format("Armor tier {}", _player.armorTier));
    }
    save_data();
}

void AppSignalQuest::open_cached_chest()
{
    if (_player.keys <= 0) {
        add_log("No key.");
        return;
    }
    _player.keys--;
    _player.openedChests++;
    const int gold = roll(8, 20 + _player.level * 4);
    _player.gold += gold;
    const int score = loot_score(_player.level + 2);
    if (score >= 92) {
        _player.relics++;
        add_log(fmt::format("Chest +{}g +relic", gold));
    } else if (score >= 72) {
        _player.scrap += 2;
        add_log(fmt::format("Chest +{}g +2scrap", gold));
    } else if (score >= 46) {
        _player.potions++;
        add_log(fmt::format("Chest +{}g +potion", gold));
    } else {
        _player.scrap++;
        add_log(fmt::format("Chest +{}g +scrap", gold));
    }
    save_data();
}

void AppSignalQuest::send_lora_beacon()
{
    if (!_sensor.capAvailable || GetHAL().millis() - _last_beacon_ms < 15000) {
        return;
    }
    const auto mac = GetHAL().getDeviceMac();
    const auto id  = fmt::format("{:02X}{:02X}{:02X}", mac[3], mac[4], mac[5]);
    const auto msg = fmt::format("CQ1|{}|L{}|{:.0f}m|{}", id, _player.level, _sensor.distanceM, mode_label());
    GetHAL().capLora868.loraSendP2PText(id, "FFFFFF", msg);
    _last_beacon_ms = GetHAL().millis();
}

void AppSignalQuest::render()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_REPL);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setTextDatum(textdatum_t::top_left);

    if (_help_visible) {
        render_help_page();
        GetHAL().pushCanvas();
        return;
    }

    if (_mode == Mode::Camp) {
        render_camp_screen();
        GetHAL().pushCanvas();
        return;
    }

    render_hud();
    render_log(kExploreTopY + kExploreLineH * kExploreLogRow);
    GetHAL().pushCanvas();
}

void AppSignalQuest::render_hud()
{
    GetHAL().canvas.setTextColor(TFT_ORANGE);
    GetHAL().canvas.drawString("CarQuest", kExploreX, kExploreTopY);
    GetHAL().canvas.setTextColor(_mode == Mode::Moving ? TFT_GREEN : TFT_CYAN);
    GetHAL().canvas.drawRightString(mode_label(), 202, kExploreTopY);

    GetHAL().canvas.setTextColor(TFT_WHITE);
    GetHAL().canvas.drawString(fit_text(fmt::format("L{} HP {}/{} XP {}/{}", _player.level, _player.hp,
                                                    _player.maxHp, _player.xp, xp_to_next_level()),
                                        24)
                                   .c_str(),
                               kExploreX, kExploreTopY + kExploreLineH);
    GetHAL().canvas.drawString(fit_text(fmt::format("A{} D{} F{} L{} G{}", _player.atk, guard_value(),
                                                    _player.focus, _player.luck, _player.gold),
                                        24)
                                   .c_str(),
                               kExploreX, kExploreTopY + kExploreLineH * 2);

    GetHAL().canvas.setTextColor((uint32_t)0x9BE7FF);
    GetHAL().canvas.drawString(fit_text(fmt::format("{:.0f}m {:.1f}kmh M{:.1f}", _sensor.distanceM,
                                                    _sensor.speedKmph, _sensor.motionScore),
                                        24)
                                   .c_str(),
                               kExploreX, kExploreTopY + kExploreLineH * 3);
}

void AppSignalQuest::render_log(int y)
{
    GetHAL().canvas.setTextColor(TFT_YELLOW);
    GetHAL().canvas.drawString(fit_text(_last_event, 24).c_str(), kExploreX, y);
    GetHAL().canvas.setTextColor(TFT_WHITE);

    int line_y = y + kExploreLineH;
    int count  = 0;
    for (const auto& line : _log) {
        GetHAL().canvas.drawString(fit_text(line, 24).c_str(), kExploreX, line_y);
        line_y += kExploreLineH;
        count++;
        if (count >= 1 || line_y > 108) {
            break;
        }
    }
}

void AppSignalQuest::render_camp_screen()
{
    constexpr int title_y       = 2;
    constexpr int content_y     = title_y + kExploreLineH;
    constexpr int body_y        = title_y + kExploreLineH * 2;
    const bool show_toast = _camp_page == CampPage::Actions && !_camp_toast.empty() &&
                            GetHAL().millis() - _camp_toast_ms < kCampToastMs;
    const bool toast_failure =
        show_toast &&
        (_camp_toast.find("No ") == 0 || _camp_toast.find("Need ") == 0 || _camp_toast.find("Rest only") == 0);

    GetHAL().canvas.setFont(FONT_REPL);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setTextColor(TFT_ORANGE);
    GetHAL().canvas.drawString("CarQuest", kExploreX, title_y);
    GetHAL().canvas.setTextColor(TFT_CYAN);
    GetHAL().canvas.drawRightString(fmt::format("{}/{}", camp_page_index() + 1, camp_page_count()).c_str(), 202,
                                    title_y);

    GetHAL().canvas.setTextColor(show_toast ? (toast_failure ? TFT_ORANGE : TFT_GREEN) : TFT_WHITE);
    GetHAL().canvas.drawString(
        fit_text(show_toast ? _camp_toast
                            : fmt::format("{} HP {}/{} G{}", camp_page_title(), _player.hp, _player.maxHp,
                                          _player.gold),
                 24)
            .c_str(),
        kExploreX, content_y);

    switch (_camp_page) {
        case CampPage::Actions:
            render_camp_actions(body_y);
            break;
        case CampPage::Inventory:
            render_camp_inventory(body_y);
            break;
        case CampPage::Merchant:
            render_camp_merchant(body_y);
            break;
        case CampPage::Relics:
            render_camp_relics(body_y);
            break;
        case CampPage::Quests:
            render_camp_quests(body_y);
            break;
        case CampPage::Log:
            render_camp_event_log(body_y);
            break;
    }
}

void AppSignalQuest::render_camp_actions(int body_y)
{
    const int heal = 4 + _player.level;
    const int upgrade_cost = 2 + _player.weaponTier + (_player.armorTier > _player.weaponTier ? 1 : 0);
    const std::string actions[] = {
        fmt::format("Rest +{}hp", heal),
        fmt::format("Potion x{}", _player.potions),
        fmt::format("Chest key x{}", _player.keys),
        fmt::format("Upgrade S{}", upgrade_cost),
    };
    for (int i = 0; i < 4; ++i) {
        const int row_y = body_y + i * kExploreLineH;
        if (i == _selected_inventory_line) {
            GetHAL().canvas.fillRoundRect(1, row_y - 1, 202, 16, 3, (uint32_t)0x334155);
            GetHAL().canvas.setTextColor(TFT_YELLOW);
            GetHAL().canvas.drawString(">", 4, row_y);
        } else {
            GetHAL().canvas.setTextColor(TFT_WHITE);
        }
        GetHAL().canvas.drawString(fit_text(actions[i], 22).c_str(), 20, row_y);
    }
}

void AppSignalQuest::render_camp_inventory(int body_y)
{
    const std::string lines[] = {
        fmt::format("L{} XP {}/{}", _player.level, _player.xp, xp_to_next_level()),
        fmt::format("ATK{} DEF{}", _player.atk, guard_value()),
        fmt::format("Focus{} Luck{}", _player.focus, _player.luck),
        fmt::format("Weapon{} Armor{}", _player.weaponTier, _player.armorTier),
        fmt::format("Potion{} Key{}", _player.potions, _player.keys),
        fmt::format("Scrap{} Relic{}", _player.scrap, _player.relics),
        fmt::format("Distance {}m", _player.totalDistanceM),
        fmt::format("Kills{} Chests{}", _player.defeatedMonsters, _player.openedChests),
    };
    constexpr int line_count   = sizeof(lines) / sizeof(lines[0]);
    constexpr int visible_rows = 4;
    _camp_scroll              = std::clamp(_camp_scroll, 0, std::max(0, line_count - visible_rows));

    GetHAL().canvas.setTextColor(TFT_WHITE);
    for (int i = 0; i < visible_rows; ++i) {
        GetHAL().canvas.drawString(fit_text(lines[_camp_scroll + i], 24).c_str(), 4, body_y + i * kExploreLineH);
    }
}

void AppSignalQuest::render_camp_merchant(int body_y)
{
    const std::string actions[] = {
        fmt::format("Buy potion {}g", 8 + _player.level),
        fmt::format("Buy key {}g", 20 + _player.level * 2),
        fmt::format("Repair {}g", 6 + _player.level),
        "Leave merchant",
    };
    for (int i = 0; i < 4; ++i) {
        const int row_y = body_y + i * kExploreLineH;
        if (i == _selected_inventory_line) {
            GetHAL().canvas.fillRoundRect(1, row_y - 1, 202, 16, 3, (uint32_t)0x334155);
            GetHAL().canvas.setTextColor(TFT_YELLOW);
            GetHAL().canvas.drawString(">", 4, row_y);
        } else {
            GetHAL().canvas.setTextColor(TFT_WHITE);
        }
        GetHAL().canvas.drawString(fit_text(actions[i], 22).c_str(), 20, row_y);
    }
}

void AppSignalQuest::render_camp_relics(int body_y)
{
    const std::string relics[] = {
        fmt::format("{} Copper Compass", _player.equippedRelic == 1 ? "*" : " "),
        fmt::format("{} Guardrail Crown", _player.equippedRelic == 2 ? "*" : " "),
        fmt::format("{} Starwheel", _player.equippedRelic == 3 ? "*" : " "),
        fmt::format("Fragments {}", _player.relics),
    };
    for (int i = 0; i < 4; ++i) {
        const int row_y = body_y + i * kExploreLineH;
        if (i < 3 && i == _selected_inventory_line) {
            GetHAL().canvas.fillRoundRect(1, row_y - 1, 202, 16, 3, (uint32_t)0x334155);
            GetHAL().canvas.setTextColor(TFT_YELLOW);
            GetHAL().canvas.drawString(">", 4, row_y);
        } else {
            GetHAL().canvas.setTextColor(i == 3 ? TFT_CYAN : TFT_WHITE);
        }
        GetHAL().canvas.drawString(fit_text(relics[i], 22).c_str(), i < 3 ? 20 : 4, row_y);
    }
}

void AppSignalQuest::render_camp_quests(int body_y)
{
    const int miles_progress = std::min(_player.totalDistanceM, 1000);
    const int cache_progress = std::min(_player.openedChests, 3);
    const int hunt_progress  = std::min(_player.defeatedMonsters, 5);
    const std::string quests[] = {
        fmt::format("{} Road Pilgrim {}/1000m", quest_reward_claimed(0) ? "*" : " ", miles_progress),
        fmt::format("{} Cache Seeker {}/3", quest_reward_claimed(1) ? "*" : " ", cache_progress),
        fmt::format("{} Wraith Hunt {}/5", quest_reward_claimed(2) ? "*" : " ", hunt_progress),
        "Enter: claim ready",
    };
    for (int i = 0; i < 4; ++i) {
        const int row_y = body_y + i * kExploreLineH;
        if (i < 3 && i == _selected_inventory_line) {
            GetHAL().canvas.fillRoundRect(1, row_y - 1, 202, 16, 3, (uint32_t)0x334155);
            GetHAL().canvas.setTextColor(TFT_YELLOW);
            GetHAL().canvas.drawString(">", 4, row_y);
        } else {
            GetHAL().canvas.setTextColor(i == 3 ? TFT_CYAN : TFT_WHITE);
        }
        GetHAL().canvas.drawString(fit_text(quests[i], 22).c_str(), i < 3 ? 20 : 4, row_y);
    }
}

void AppSignalQuest::render_camp_event_log(int body_y)
{
    const int visible_rows = 3;
    _camp_scroll = std::clamp(_camp_scroll, 0, std::max(0, static_cast<int>(_log.size()) - visible_rows));
    GetHAL().canvas.setTextColor(TFT_YELLOW);
    GetHAL().canvas.drawString(fit_text(_last_event, 24).c_str(), 4, body_y);
    GetHAL().canvas.setTextColor(TFT_WHITE);
    for (int i = 0; i < visible_rows && _camp_scroll + i < static_cast<int>(_log.size()); ++i) {
        GetHAL().canvas.drawString(fit_text(_log[_camp_scroll + i], 24).c_str(), 4, body_y + (i + 1) * kExploreLineH);
    }
}

void AppSignalQuest::render_help_page()
{
    constexpr int title_y      = 4;
    constexpr int body_y       = title_y + 18;
    constexpr int help_line_h  = kExploreLineH;
    static const char* help_lines[] = {
        "Drive: auto explore",
        "Stop: camp after 15s",
        "Camp L/R: pages",
        "Camp U/D: select",
        "Enter: do action",
        "I: potion",
        "R: rest",
        "O: open chest",
        "U: upgrade weapon",
        "T: test event",
        "M: simulate +50m",
        "Space: mode toggle",
        "Hold OPT to exit",
    };
    constexpr int line_count   = sizeof(help_lines) / sizeof(help_lines[0]);
    constexpr int visible_rows = 5;
    _help_scroll              = std::clamp(_help_scroll, 0, std::max(0, line_count - visible_rows));

    GetHAL().canvas.setFont(FONT_REPL);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.fillScreen((uint32_t)0x101420);
    GetHAL().canvas.drawRect(0, 0, GetHAL().canvas.width(), GetHAL().canvas.height(), TFT_DARKGREY);
    GetHAL().canvas.setTextColor(TFT_ORANGE);
    GetHAL().canvas.drawString("Controls", 4, title_y);
    GetHAL().canvas.setTextColor(TFT_CYAN);
    GetHAL().canvas.drawRightString(fmt::format("{}/{}", _help_scroll + 1, line_count - visible_rows + 1).c_str(),
                                    200, title_y);

    GetHAL().canvas.setTextColor(TFT_WHITE);
    for (int i = 0; i < visible_rows; ++i) {
        GetHAL().canvas.drawString(help_lines[_help_scroll + i], 4, body_y + i * help_line_h);
    }
}

void AppSignalQuest::handle_help_key(int key_code)
{
    if (key_code == KEY_UP || key_code == KEY_LEFT) {
        _help_scroll = std::max(0, _help_scroll - 1);
        render();
    } else if (key_code == KEY_DOWN || key_code == KEY_RIGHT) {
        _help_scroll++;
        render();
    }
}

void AppSignalQuest::handle_camp_key(int key_code)
{
    if (key_code == KEY_LEFT || key_code == KEY_RIGHT) {
        int page = camp_page_index();
        page += key_code == KEY_RIGHT ? 1 : -1;
        page = (page + camp_page_count()) % camp_page_count();
        _camp_page = static_cast<CampPage>(page);
        _selected_inventory_line = 0;
        _camp_scroll = 0;
        return;
    }

    if (_camp_page == CampPage::Inventory || _camp_page == CampPage::Log) {
        if (key_code == KEY_UP) {
            _camp_scroll = std::max(0, _camp_scroll - 1);
        } else if (key_code == KEY_DOWN) {
            _camp_scroll++;
        }
        return;
    }

    const int selectable = camp_selectable_count();
    if (selectable > 0 && key_code == KEY_UP) {
        _selected_inventory_line = (_selected_inventory_line + selectable - 1) % selectable;
    } else if (selectable > 0 && key_code == KEY_DOWN) {
        _selected_inventory_line = (_selected_inventory_line + 1) % selectable;
    } else if (key_code == KEY_ENTER) {
        handle_camp_selection();
    }
}

void AppSignalQuest::handle_camp_selection()
{
    switch (_camp_page) {
        case CampPage::Actions:
            if (_selected_inventory_line == 0) {
                rest();
            } else if (_selected_inventory_line == 1) {
                use_potion();
            } else if (_selected_inventory_line == 2) {
                open_cached_chest();
            } else {
                craft_upgrade();
            }
            break;
        case CampPage::Merchant:
            if (_selected_inventory_line == 0) {
                buy_potion();
            } else if (_selected_inventory_line == 1) {
                buy_key();
            } else if (_selected_inventory_line == 2) {
                merchant_repair();
            } else {
                add_log("Merchant packs up.");
            }
            break;
        case CampPage::Relics:
            equip_relic(_selected_inventory_line + 1);
            break;
        case CampPage::Quests:
            claim_quest_reward(_selected_inventory_line);
            break;
        case CampPage::Inventory:
        case CampPage::Log:
            break;
    }
}

int AppSignalQuest::camp_page_index() const
{
    return static_cast<int>(_camp_page);
}

int AppSignalQuest::camp_page_count() const
{
    return 6;
}

int AppSignalQuest::camp_selectable_count() const
{
    switch (_camp_page) {
        case CampPage::Actions:
        case CampPage::Merchant:
            return 4;
        case CampPage::Relics:
        case CampPage::Quests:
            return 3;
        case CampPage::Inventory:
        case CampPage::Log:
            return 0;
    }
    return 0;
}

const char* AppSignalQuest::camp_page_title() const
{
    switch (_camp_page) {
        case CampPage::Actions:
            return "Actions";
        case CampPage::Inventory:
            return "Inventory";
        case CampPage::Merchant:
            return "Merchant";
        case CampPage::Relics:
            return "Relics";
        case CampPage::Quests:
            return "Quests";
        case CampPage::Log:
            return "Event Log";
    }
    return "Camp";
}

void AppSignalQuest::buy_potion()
{
    const int cost = 8 + _player.level;
    if (_player.gold < cost) {
        add_log(fmt::format("Need {}g for potion", cost));
        return;
    }
    _player.gold -= cost;
    _player.potions++;
    add_log(fmt::format("Bought potion -{}g", cost));
    save_data();
}

void AppSignalQuest::buy_key()
{
    const int cost = 20 + _player.level * 2;
    if (_player.gold < cost) {
        add_log(fmt::format("Need {}g for key", cost));
        return;
    }
    _player.gold -= cost;
    _player.keys++;
    add_log(fmt::format("Bought key -{}g", cost));
    save_data();
}

void AppSignalQuest::merchant_repair()
{
    const int cost = 6 + _player.level;
    if (_player.gold < cost) {
        add_log(fmt::format("Need {}g repair", cost));
        return;
    }
    _player.gold -= cost;
    _player.statusFlags &= ~(STATUS_WOUNDED | STATUS_SHAKEN | STATUS_JAMMED);
    add_log(fmt::format("Gear repaired -{}g", cost));
    save_data();
}

void AppSignalQuest::equip_relic(int relic_id)
{
    if (_player.relics <= 0) {
        add_log("No relic fragments.");
        return;
    }
    _player.equippedRelic = relic_id;
    if (relic_id == 1) {
        add_log("Relic: Copper Compass");
    } else if (relic_id == 2) {
        add_log("Relic: Guardrail Crown");
    } else {
        add_log("Relic: Starwheel");
    }
    save_data();
}

bool AppSignalQuest::quest_reward_claimed(int quest_id) const
{
    return (_player.questFlags & (1 << quest_id)) != 0;
}

void AppSignalQuest::claim_quest_reward(int quest_id)
{
    if (quest_reward_claimed(quest_id)) {
        add_log("Quest already claimed");
        return;
    }

    bool ready = false;
    if (quest_id == 0) {
        ready = _player.totalDistanceM >= 1000;
    } else if (quest_id == 1) {
        ready = _player.openedChests >= 3;
    } else {
        ready = _player.defeatedMonsters >= 5;
    }

    if (!ready) {
        add_log("Quest not ready");
        return;
    }

    _player.questFlags |= (1 << quest_id);
    if (quest_id == 0) {
        _player.luck++;
        _player.gold += 25;
        add_log("Quest reward: +L +25g");
    } else if (quest_id == 1) {
        _player.keys++;
        _player.relics++;
        add_log("Quest reward: +key +relic");
    } else {
        _player.atk++;
        _player.scrap += 3;
        add_log("Quest reward: +A +3scrap");
    }
    save_data();
}

void AppSignalQuest::update_help_state()
{
    const bool opt_down = (GetHAL().keyboard.getModifierMask() & KEY_MOD_LMETA) != 0;
    if (opt_down) {
        if (_opt_hold_start_ms == 0) {
            _opt_hold_start_ms = GetHAL().millis();
        } else if (!_opt_toggle_latched && GetHAL().millis() - _opt_hold_start_ms >= kHelpHoldMs) {
            _help_visible       = !_help_visible;
            _opt_toggle_latched = true;
            render();
        }
    } else {
        _opt_hold_start_ms  = 0;
        _opt_toggle_latched = false;
    }
}

void AppSignalQuest::load_save_data()
{
    Settings settings("cardputer");
    auto get_int = [&settings](const char* key, int fallback) {
        return static_cast<int>(settings.GetInt(key, fallback));
    };
    const int version = settings.GetInt("cq_version", 0);
    if (version <= 0) {
        return;
    }
    _player.level = std::clamp(get_int("cq_level", _player.level), 1, 10);
    _player.maxHp = std::max(1, get_int("cq_max_hp", 24 + _player.level * 6));
    _player.hp = std::clamp(get_int("cq_hp", _player.maxHp), 1, _player.maxHp);
    _player.atk = std::max(1, get_int("cq_atk", _player.atk));
    _player.def = std::max(0, get_int("cq_def", _player.def));
    _player.focus = std::max(0, get_int("cq_focus", _player.focus));
    _player.luck = std::max(0, get_int("cq_luck", _player.luck));
    _player.xp = std::max(0, get_int("cq_xp", _player.xp));
    _player.gold = std::max(0, get_int("cq_gold", _player.gold));
    _player.potions = std::max(0, get_int("cq_potions", _player.potions));
    _player.scrap = std::max(0, get_int("cq_scrap", _player.scrap));
    _player.keys = std::max(0, get_int("cq_keys", _player.keys));
    _player.relics = std::max(0, get_int("cq_relics", _player.relics));
    _player.weaponTier = std::clamp(get_int("cq_weapon", _player.weaponTier), 0, 5);
    _player.armorTier = std::clamp(get_int("cq_armor", _player.armorTier), 0, 5);
    _player.statusFlags = get_int("cq_status", 0);
    _player.equippedRelic = std::clamp(get_int("cq_relic_eq", _player.equippedRelic), 0, 3);
    _player.questFlags = get_int("cq_quests", _player.questFlags);
    _player.totalDistanceM = std::max(0, get_int("cq_total_m", _player.totalDistanceM));
    _player.defeatedMonsters = std::max(0, get_int("cq_kills", _player.defeatedMonsters));
    _player.openedChests = std::max(0, get_int("cq_chests", _player.openedChests));
}

void AppSignalQuest::save_data()
{
    Settings settings("cardputer", true);
    settings.SetInt("cq_version", 1);
    settings.SetInt("cq_level", _player.level);
    settings.SetInt("cq_hp", _player.hp);
    settings.SetInt("cq_max_hp", _player.maxHp);
    settings.SetInt("cq_atk", _player.atk);
    settings.SetInt("cq_def", _player.def);
    settings.SetInt("cq_focus", _player.focus);
    settings.SetInt("cq_luck", _player.luck);
    settings.SetInt("cq_xp", _player.xp);
    settings.SetInt("cq_gold", _player.gold);
    settings.SetInt("cq_potions", _player.potions);
    settings.SetInt("cq_scrap", _player.scrap);
    settings.SetInt("cq_keys", _player.keys);
    settings.SetInt("cq_relics", _player.relics);
    settings.SetInt("cq_weapon", _player.weaponTier);
    settings.SetInt("cq_armor", _player.armorTier);
    settings.SetInt("cq_status", _player.statusFlags);
    settings.SetInt("cq_relic_eq", _player.equippedRelic);
    settings.SetInt("cq_quests", _player.questFlags);
    settings.SetInt("cq_total_m", _player.totalDistanceM);
    settings.SetInt("cq_kills", _player.defeatedMonsters);
    settings.SetInt("cq_chests", _player.openedChests);
}

std::string AppSignalQuest::fit_text(const std::string& text, std::size_t max_chars) const
{
    if (text.size() <= max_chars) {
        return text;
    }
    if (max_chars <= 3) {
        return text.substr(0, max_chars);
    }
    return text.substr(0, max_chars - 3) + "...";
}

void AppSignalQuest::add_log(const std::string& line)
{
    _log.insert(_log.begin(), line);
    if (_mode == Mode::Camp) {
        _camp_toast    = line;
        _camp_toast_ms = GetHAL().millis();
    }
    if (_log.size() > kMaxLogLines) {
        _log.pop_back();
    }
}

int AppSignalQuest::roll(int min_value, int max_value)
{
    if (max_value <= min_value) {
        return min_value;
    }
    return min_value + static_cast<int>(rand_u32() % static_cast<uint32_t>(max_value - min_value + 1));
}

uint32_t AppSignalQuest::rand_u32()
{
    _rng ^= _rng << 13;
    _rng ^= _rng >> 17;
    _rng ^= _rng << 5;
    return _rng;
}

const char* AppSignalQuest::mode_label() const
{
    return _mode == Mode::Moving ? "EXPLORE" : "CAMP";
}
