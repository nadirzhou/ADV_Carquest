# AGENTS.md

## Project

This repository is the M5Stack Cardputer ADV firmware workspace customized for:

- M5Stack Cardputer ADV
- M5Stack Cap LoRa-1262 / GPS
- CarQuest, a travel-driven fantasy RPG app
- LoRa P2P experimentation
- Cardputer ADV power/status display tuning

The current active app is `CarQuest`, implemented in `main/apps/app_signal_quest`.

## Hardware Assumptions

- Target chip: ESP32-S3
- Device serial port used during development: `COM10`
- ESP-IDF installation: `C:\esp-idf-5.4.2-ascii`
- Power switch must be ON for normal battery charging behavior.
- Cardputer ADV battery status is ADC-style and does not expose a reliable charging-state UI flag, so the firmware displays percent and voltage text instead of a battery icon.

## Build

Use PowerShell from the repository root:

```powershell
$env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; $env:IDF_CCACHE_ENABLE='0'; chcp 65001 | Out-Null; & 'C:\esp-idf-5.4.2-ascii\export.ps1'; idf.py -DCCACHE_ENABLE=0 build
```

## Flash

Normal flash, preserving NVS/player save data:

```powershell
$env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; chcp 65001 | Out-Null; & 'C:\esp-idf-5.4.2-ascii\export.ps1'; idf.py -p COM10 -b 1500000 flash
```

Only erase flash when the user explicitly asks to clear the device.

## Current CarQuest State

CarQuest currently includes:

- Automatic movement/exploration events
- Camp mode after stopped detection
- Long-press OPT help overlay
- Unified small-screen text layout
- Level, HP, ATK, DEF, Focus, Luck
- XP curve and level-up rewards
- Gold, potions, scrap, keys, relic count
- Weapon tier and armor tier
- Monster family tables
- Treasure quality table
- Hazard and shrine events
- Status effects
- NVS save/load for player state
- LoRa beacon prefix `CQ1`

The roadmap status is:

- Phase 1: complete
- Phase 2: mostly complete
- Phase 3: next major focus
- Phase 4: early partial work
- Phase 5: not started

See `docs/CARQUEST_DESIGN.md` and `docs/FIRMWARE_STATUS.md`.

## Important Files

- `main/apps/app_signal_quest/app_signal_quest.cpp`: CarQuest gameplay, rendering, input, save/load.
- `main/apps/app_signal_quest/app_signal_quest.h`: CarQuest state and method declarations.
- `main/apps/app_signal_quest/assets/`: CarQuest app icon assets.
- `main/hal/cap_lora868/`: Cap LoRa/GPS HAL.
- `main/apps/app_launcher/view/system_bar/system_bar.cpp`: top system bar battery/voltage display.
- `docs/`: design and project documentation.

## Cleanup Policy

Keep the repository focused on firmware and curated documentation. Temporary generated asset/vector experiments should not live at the repo root after review. If generated assets become part of firmware, copy only the final headers or source assets into the owning app folder and document the workflow in `docs/`.

Do not revert unrelated user changes. This workspace may contain local firmware experiments that are not yet committed.

