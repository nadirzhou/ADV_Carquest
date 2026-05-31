/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "cap_lora868.h"
#include "../hal_config.h"
#include "uart/uart_helper.h"
#include "radio_lib/EspHal.h"
#include <utility/PI4IOE5V6408_Class.hpp>
#include <driver/sdspi_host.h>
#include <mooncake_log.h>
#include <RadioLib.h>
#include <algorithm>
#include <charconv>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

static const std::string _tag = "Cap-LoRa1262";

bool CapLoRa868::init()
{
    mclog::tagInfo(_tag, "init");

    if (_is_inited) {
        return true;
    }

    const bool lora_ok = lora_init();
    const bool gps_ok  = gps_init();

    _is_inited = lora_ok || gps_ok;
    return _is_inited;
}

void CapLoRa868::update()
{
    if (!_is_inited) {
        return;
    }

    lora_update();
}

/* -------------------------------------------------------------------------- */
/*                                    LoRa                                    */
/* -------------------------------------------------------------------------- */
// https://github.com/jgromes/RadioLib/blob/master/examples/NonArduino/ESP-IDF
// https://github.com/jgromes/RadioLib/blob/master/examples/SX126x

struct RadioLibData_t {
    std::unique_ptr<EspHal> hal;
    std::unique_ptr<Module> module;
    std::unique_ptr<SX1262> sx1262;

    void reset()
    {
        this->hal.reset();
        this->module.reset();
        this->sx1262.reset();
    }
};

static RadioLibData_t _radio_lib;
static volatile bool _lora_rx_flag = false;
static volatile bool _lora_tx_flag = false;
static bool _lora_tx_active        = false;

IRAM_ATTR void _lora_set_rx_flag(void)
{
    _lora_rx_flag = true;
}

IRAM_ATTR void _lora_set_tx_flag(void)
{
    _lora_tx_flag = true;
}

bool CapLoRa868::lora_init()
{
    mclog::tagInfo(_tag, "lora init");

    enable_rf_switch();

    // Create hal
    spi_host_device_t spi_host = SDSPI_DEFAULT_HOST;
    _radio_lib.hal = std::make_unique<EspHal>(HAL_PIN_SPI_SCLK, HAL_PIN_SPI_MISO, HAL_PIN_SPI_MOSI, spi_host, true);

    // Create module
    _radio_lib.module = std::make_unique<Module>(_radio_lib.hal.get(), HAL_PIN_LORA_NSS_GPIO, HAL_PIN_LORA_DIO1_GPIO,
                                                 HAL_PIN_LORA_RST_GPIO, HAL_PIN_LORA_BUSY_GPIO);

    // Create sx1262
    _radio_lib.sx1262 = std::make_unique<SX1262>(_radio_lib.module.get());
    auto state =
        _radio_lib.sx1262->begin(lora_config::ferq, lora_config::bw, lora_config::sf, lora_config::cr,
                                 lora_config::syncWord, lora_config::power, lora_config::preambleLength, 3.0, false);
    if (state == RADIOLIB_ERR_NONE) {
        mclog::tagInfo(_tag, "sx1262 init success");
    } else {
        mclog::tagError(_tag, "sx1262 init failed, code {}", state);
        goto HANDLE_ERROR;
    }

    // Setup
    _radio_lib.sx1262->setDio2AsRfSwitch(true);
    _radio_lib.sx1262->setCurrentLimit(140);

    // Start receiving
    _radio_lib.sx1262->setPacketReceivedAction(_lora_set_rx_flag);
    state = _radio_lib.sx1262->startReceive();
    if (state == RADIOLIB_ERR_NONE) {
        mclog::tagInfo(_tag, "sx1262 start receive success");
    } else {
        mclog::tagError(_tag, "sx1262 start receive failed, code {}", state);
        goto HANDLE_ERROR;
    }

    return true;

HANDLE_ERROR:
    _radio_lib.reset();
    return false;
}

bool CapLoRa868::enable_rf_switch()
{
    mclog::tagInfo(_tag, "enable rf switch");

    m5::PI4IOE5V6408_Class ioe(0x43, 400000, &m5::In_I2C);
    if (!ioe.begin()) {
        mclog::tagWarn(_tag, "PI4IOE5V6408 not found, continue without rf switch setup");
        return false;
    }

    ioe.setDirection(0, true);
    ioe.setHighImpedance(0, false);
    ioe.digitalWrite(0, true);
    mclog::tagInfo(_tag, "rf switch enabled on PI4IOE P0");
    return true;
}

void CapLoRa868::lora_update()
{
    if (!_radio_lib.sx1262) {
        return;
    }

    // mclog::tagDebug(_tag, "{} {}", _lora_rx_flag, _lora_tx_flag);

    if (_lora_rx_flag) {
        _lora_rx_flag = false;

        size_t len = _radio_lib.sx1262->getPacketLength();
        std::string msg;
        msg.resize(len);
        int state = _radio_lib.sx1262->readData(reinterpret_cast<uint8_t*>(&msg[0]), len);
        if (state == RADIOLIB_ERR_NONE) {
            mclog::tagDebug(_tag, "lora receive msg: {} | len: {} | RSSI: {}dBm | SNR: {}dB", msg, len,
                            _radio_lib.sx1262->getRSSI(), _radio_lib.sx1262->getSNR());
        } else {
            mclog::tagError(_tag, "lora read data failed, code {}", state);
        }

        P2PMessage packet;
        if (state == RADIOLIB_ERR_NONE && decode_p2p_packet(msg, packet)) {
            packet.rssi = _radio_lib.sx1262->getRSSI();
            packet.snr  = _radio_lib.sx1262->getSNR();
            onP2PMsg.emit(packet);
        } else if (state == RADIOLIB_ERR_NONE) {
            onLoraMsg.emit(msg);
        }

        _radio_lib.sx1262->setPacketReceivedAction(_lora_set_rx_flag);
        state = _radio_lib.sx1262->startReceive();
        if (state != RADIOLIB_ERR_NONE) {
            mclog::tagError(_tag, "sx1262 restart receive failed, code {}", state);
        }
    }

    if (_lora_tx_flag) {
        _lora_tx_flag = false;
        _lora_tx_active = false;

        mclog::tagDebug(_tag, "lora send msg success");
        _radio_lib.sx1262->setPacketReceivedAction(_lora_set_rx_flag);
        _radio_lib.sx1262->startReceive();
    }
}

bool CapLoRa868::loraSendMsg(const std::string& msg)
{
    if (!_is_inited || !_radio_lib.sx1262) {
        return false;
    }

    if (_lora_tx_active) {
        mclog::tagWarn(_tag, "lora tx busy, drop message");
        return false;
    }

    mclog::tagDebug(_tag, "lora send msg: {}", msg);

    _radio_lib.sx1262->setPacketSentAction(_lora_set_tx_flag);
    int state = _radio_lib.sx1262->startTransmit(msg.c_str());
    if (state != RADIOLIB_ERR_NONE) {
        mclog::tagError(_tag, "lora send msg failed, code {}", state);
        return false;
    }
    _lora_tx_active = true;
    return true;
}

bool CapLoRa868::loraSendP2PText(const std::string& src, const std::string& dst, const std::string& text)
{
    P2PMessage packet;
    packet.type    = 'M';
    packet.src     = src;
    packet.dst     = dst;
    packet.id      = _next_msg_id++;
    packet.flags   = 0;
    packet.payload = text;

    return loraSendMsg(encode_p2p_packet(packet));
}

static std::string _hex_encode(const std::string& input)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string output;
    output.reserve(input.size() * 2);
    for (uint8_t c : input) {
        output.push_back(hex[(c >> 4) & 0x0F]);
        output.push_back(hex[c & 0x0F]);
    }
    return output;
}

static bool _hex_decode(const std::string& input, std::string& output)
{
    auto value_of = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    if (input.size() % 2 != 0) {
        return false;
    }

    output.clear();
    output.reserve(input.size() / 2);
    for (size_t i = 0; i < input.size(); i += 2) {
        int hi = value_of(input[i]);
        int lo = value_of(input[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        output.push_back(static_cast<char>((hi << 4) | lo));
    }
    return true;
}

static std::vector<std::string> _split_fields(const std::string& input, char delim)
{
    std::vector<std::string> fields;
    size_t start = 0;
    while (true) {
        size_t end = input.find(delim, start);
        if (end == std::string::npos) {
            fields.push_back(input.substr(start));
            break;
        }
        fields.push_back(input.substr(start, end - start));
        start = end + 1;
    }
    return fields;
}

std::string CapLoRa868::encode_p2p_packet(const P2PMessage& packet)
{
    return fmt::format("CP2P1|{}|{}|{}|{:08X}|{:02X}|{}", packet.type, packet.src, packet.dst, packet.id,
                       packet.flags, _hex_encode(packet.payload));
}

bool CapLoRa868::decode_p2p_packet(const std::string& raw, P2PMessage& packet)
{
    auto fields = _split_fields(raw, '|');
    if (fields.size() != 7 || fields[0] != "CP2P1" || fields[1].size() != 1) {
        return false;
    }

    uint32_t id = 0;
    uint32_t flags = 0;
    auto id_result = std::from_chars(fields[4].data(), fields[4].data() + fields[4].size(), id, 16);
    auto flags_result = std::from_chars(fields[5].data(), fields[5].data() + fields[5].size(), flags, 16);
    if (id_result.ec != std::errc() || flags_result.ec != std::errc() || flags > 0xFF) {
        return false;
    }

    std::string payload;
    if (!_hex_decode(fields[6], payload)) {
        return false;
    }

    packet.type    = fields[1][0];
    packet.src     = fields[2];
    packet.dst     = fields[3];
    packet.id      = id;
    packet.flags   = static_cast<uint8_t>(flags);
    packet.payload = payload;
    return true;
}

/* -------------------------------------------------------------------------- */
/*                                     GPS                                    */
/* -------------------------------------------------------------------------- */
// https://github.com/mikalhart/TinyGPSPlus/blob/master/examples/BasicExample

static std::unique_ptr<TinyGPSPlus> _gps;
static std::mutex _gps_mutex;

void _handle_gps_msg(const char* msg)
{
    // mclog::tagDebug(_tag, "on gps msg:\n{}", msg);

    std::lock_guard<std::mutex> lock(_gps_mutex);

    const char* p = msg;
    while (*p) {
        if (_gps->encode(*p++)) {
            // mclog::tagDebug(_tag, "gps parse ready");
        }
    }
}

bool CapLoRa868::gps_init()
{
    mclog::tagInfo(_tag, "gps init");

    _gps = std::make_unique<TinyGPSPlus>();

    gps_uart_helper_init();
    gps_uart_helper_set_on_msg_callback(_handle_gps_msg);

    return true;
}

TinyGPSPlus* CapLoRa868::borrowGPS()
{
    if (!_gps) {
        mclog::tagError(_tag, "gps not initialized");
        return nullptr;
    }
    _gps_mutex.lock();
    return _gps.get();
}

void CapLoRa868::returnGPS()
{
    _gps_mutex.unlock();
}
