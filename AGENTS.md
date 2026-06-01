# AGENTS.md

## Project

This repository is the M5Stack Cardputer ADV firmware workspace customized for:

- M5Stack Cardputer ADV
- M5Stack Cap LoRa-1262 / GPS
- CarQuest, a travel-driven fantasy RPG app
- LoRa P2P experimentation
- Cardputer ADV power/status display tuning

The current legacy app is `CarQuest`, implemented in `main/apps/app_signal_quest`.

The current standalone game track is `CardPokemon`, implemented as an isolated ESP-IDF project under `cardpokemon/`. It should boot directly into the game and should not be added to the official demo launcher.

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

For CardPokemon, build from the isolated project directory:

```powershell
cd cardpokemon
$env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; $env:IDF_CCACHE_ENABLE='0'; chcp 65001 | Out-Null; & 'C:\esp-idf-5.4.2-ascii\export.ps1'; idf.py -DCCACHE_ENABLE=0 build
```

## Flash

Normal flash, preserving NVS/player save data:

```powershell
$env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; chcp 65001 | Out-Null; & 'C:\esp-idf-5.4.2-ascii\export.ps1'; idf.py -p COM10 -b 1500000 flash
```

For CardPokemon, flash from `cardpokemon/` with the same port and baud rate:

```powershell
cd cardpokemon
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

CarQuest display rule:

- The usable game area should be treated as exactly six text rows.
- Any screen with more than six rows must use paging or scrolling.
- Temporary feedback should replace an existing row instead of adding a seventh row.

The roadmap status is:

- Phase 1: complete
- Phase 2: mostly complete
- Phase 3: initial implementation complete
- Phase 4: early partial work
- Phase 5: not started

See `docs/CARQUEST_DESIGN.md` and `docs/FIRMWARE_STATUS.md`.

## Important Files

- `cardpokemon/`: standalone CardPokemon firmware, docs, SD resource pack tooling and generated SD assets.
- `cardpokemon/main/cardpokemon_main.cpp`: CardPokemon gameplay, rendering, SD catalog loading, input, save/load.
- `cardpokemon/tools/fetch_pokemon_assets.py`: downloads and preprocesses PokeAPI JSON/PNG/GIF resources for the SD card.
- `cardpokemon/docs/AGENT_WORKFLOW.md`: CardPokemon sub-agent workflow, roles, file ownership, handoff format and verification rules.
- `main/apps/app_signal_quest/app_signal_quest.cpp`: CarQuest gameplay, rendering, input, save/load.
- `main/apps/app_signal_quest/app_signal_quest.h`: CarQuest state and method declarations.
- `main/apps/app_signal_quest/assets/`: CarQuest app icon assets.
- `main/hal/cap_lora868/`: Cap LoRa/GPS HAL.
- `main/apps/app_launcher/view/system_bar/system_bar.cpp`: top system bar battery/voltage display.
- `docs/`: design and project documentation.

## Cleanup Policy

Keep the repository focused on firmware and curated documentation. Temporary generated asset/vector experiments should not live at the repo root after review. If generated assets become part of firmware, copy only the final headers or source assets into the owning app folder and document the workflow in `docs/`.

CardPokemon generated SD resource packs may live under `cardpokemon/assets/sdcard/` while they are part of the active asset workflow. Do not add serial upload helpers unless the user explicitly asks to revisit that approach.

CardPokemon current rule of record:

- The firmware should use real Cap LoRa-1262 GPS movement as the only travel-distance source.
- IMU should be treated as relative activity heat, not as fake movement.
- IMU effects should be scaled by GPS speed: walking should dampen wild level pressure, while driving can use stronger road-intensity effects.
- Do not reintroduce debug controls such as key-driven distance gain or forced wild encounters unless the user explicitly asks for a test build.
- Preserve unsigned timer wrap-around behavior for encounter and battle timers.

Do not revert unrelated user changes. This workspace may contain local firmware experiments that are not yet committed.
