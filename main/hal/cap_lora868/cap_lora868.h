/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "TinyGPSPlus/TinyGPS++.h"
#include <mooncake_log_signal.h>
#include <M5Unified.h>
#include <string>
#include <cstdint>

class CapLoRa868 {
public:
    bool init();
    void update();

    /* ---------------------------------- LoRa ---------------------------------- */
    struct lora_config {
        static constexpr float ferq              = 868.0f;
        static constexpr float bw                = 500.0f;
        static constexpr uint8_t sf              = 7;
        static constexpr uint8_t cr              = 5;
        static constexpr uint8_t syncWord        = 0x34;
        static constexpr int8_t power            = 10;
        static constexpr uint16_t preambleLength = 10;
    };

    struct P2PMessage {
        char type            = 'M';
        std::string src;
        std::string dst;
        uint32_t id          = 0;
        uint8_t flags        = 0;
        std::string payload;
        float rssi           = 0.0f;
        float snr            = 0.0f;
    };

    bool loraSendMsg(const std::string& msg);
    bool loraSendP2PText(const std::string& src, const std::string& dst, const std::string& text);
    mclog::Signal<const std::string&> onLoraMsg;
    mclog::Signal<const P2PMessage&> onP2PMsg;

    /* ----------------------------------- GPS ---------------------------------- */
    TinyGPSPlus* borrowGPS();
    void returnGPS();

private:
    bool _is_inited = false;
    uint32_t _next_msg_id = 1;

    bool enable_rf_switch();
    bool lora_init();
    void lora_update();
    bool gps_init();
    std::string encode_p2p_packet(const P2PMessage& packet);
    bool decode_p2p_packet(const std::string& raw, P2PMessage& packet);
};
