/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "../../app_launcher.h"
#include <mooncake_log.h>
#include <apps/utils/theme.h>
#include <apps/utils/common.h>

#include "assets/wifi1.h"
#include "assets/wifi2.h"
#include "assets/wifi3.h"
#include "assets/wifi4.h"
#include "assets/wifi5.h"

void Launcher::start_system_bar()
{
    render_system_bar();
}

void Launcher::update_system_bar()
{
    if ((GetHAL().millis() - _data.system_bar_update_count) > _data.system_bar_update_preiod) {
        render_system_bar();
        _data.system_bar_update_count = GetHAL().millis();
    }
}

void Launcher::render_system_bar()
{
    // Update state
    _data.system_state.wifi_state = GetHAL().isWifiConnected() ? 1 : 4;

    // Time
    static time_t now;
    static struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    _data.system_state.time = fmt::format("{:02d}:{:02d}", timeinfo.tm_hour, timeinfo.tm_min);

    // Bat
    if ((GetHAL().millis() - _data.bat_update_time_count) > 5000 || _data.bat_update_time_count == 0) {
        auto power                         = GetHAL().getPowerStatus();
        _data.system_state.bat_level_valid = power.batteryLevelValid;
        _data.system_state.bat_charging    = power.chargeStateKnown && power.isCharging;
        _data.system_state.bat_voltage_valid = power.batteryVoltageMv > 0;
        _data.system_state.bat_voltage_mv    = power.batteryVoltageMv;
        _data.system_state.bat_level =
            power.batteryLevelValid ? fmt::format("{}", power.batteryLevel) : std::string("--");

        if (!power.batteryLevelValid) {
            _data.system_state.bat_state = 4;
        } else if (power.batteryLevel >= 100) {
            _data.system_state.bat_state = 1;
        } else if (power.batteryLevel >= 75) {
            _data.system_state.bat_state = 2;
        } else if (power.batteryLevel >= 50) {
            _data.system_state.bat_state = 3;
        } else {
            _data.system_state.bat_state = 4;
        }

        _data.bat_update_time_count = GetHAL().millis();
    }

    // Backgound
    int margin_x = 5;
    int margin_y = 4;

    GetHAL().canvasSystemBar.fillScreen(THEME_COLOR_BG);
    GetHAL().canvasSystemBar.fillSmoothRoundRect(margin_x, margin_y, GetHAL().canvasSystemBar.width() - margin_x * 2,
                                                 GetHAL().canvasSystemBar.height() - margin_y * 2,
                                                 (GetHAL().canvasSystemBar.height() - margin_y * 2) / 2,
                                                 THEME_COLOR_SYSTEM_BAR);

    GetHAL().canvasSystemBar.setFont(FONT_BASIC);

    // Time
    GetHAL().canvasSystemBar.setTextColor(THEME_COLOR_SYSTEM_BAR_TEXT);
    GetHAL().canvasSystemBar.drawCenterString(_data.system_state.time.c_str(), GetHAL().canvasSystemBar.width() / 2,
                                              GetHAL().canvasSystemBar.height() / 2 - FONT_HEIGHT / 2);

    // Wifi
    int x = 15;
    int y = 5;

    if (_data.system_state.wifi_state == 1) {
        GetHAL().canvasSystemBar.pushImage(x, y, 16, 16, image_data_wifi1);
    } else if (_data.system_state.wifi_state == 2) {
        GetHAL().canvasSystemBar.pushImage(x, y, 16, 16, image_data_wifi2);
    } else if (_data.system_state.wifi_state == 3) {
        GetHAL().canvasSystemBar.pushImage(x, y, 16, 16, image_data_wifi3);
    } else if (_data.system_state.wifi_state == 4) {
        GetHAL().canvasSystemBar.pushImage(x, y, 16, 16, image_data_wifi4);
    } else if (_data.system_state.wifi_state == 5) {
        GetHAL().canvasSystemBar.pushImage(x, y, 16, 16, image_data_wifi5);
    }

    // Battery level and voltage. Cardputer-Adv cannot report charge state reliably.
    GetHAL().canvasSystemBar.setFont(&fonts::Font0);
    GetHAL().canvasSystemBar.setTextColor((uint32_t)0x000000);
    std::string bat_label = _data.system_state.bat_level_valid ? fmt::format("{}%", _data.system_state.bat_level)
                                                               : std::string("--%");
    if (_data.system_state.bat_voltage_valid) {
        bat_label += fmt::format(" {:.2f}V", _data.system_state.bat_voltage_mv / 1000.0f);
    } else {
        bat_label += " --V";
    }
    GetHAL().canvasSystemBar.drawRightString(bat_label.c_str(), GetHAL().canvasSystemBar.width() - 12,
                                             GetHAL().canvasSystemBar.height() / 2 - 3);

    // Push
    GetHAL().pushCanvasSystemBar();
}
