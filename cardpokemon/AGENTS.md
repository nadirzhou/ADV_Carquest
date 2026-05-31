# AGENTS.md

## Project

CardPokemon is an isolated ESP-IDF firmware project for M5Stack Cardputer ADV. The device should boot directly into this game rather than loading it as part of the original demo launcher.

The game direction is a Pokemon-inspired driving RPG:

- choose one of three starter Pokemon
- drive/explore with the active Pokemon
- encounter wild Pokemon while moving
- resolve simple battles using the CarQuest-style compact loop
- gain XP, level up, and evolve
- rarely capture defeated wild Pokemon
- choose captured Pokemon from the Pokedex for future exploration
- use Cap LoRa-1262 GPS movement as the encounter gate, with Cardputer 6-axis IMU heat tuning encounter pacing, rarity and wild level pressure

## Hardware And Build

- Target chip: ESP32-S3
- Development serial port: `COM10`
- ESP-IDF installation: `C:\esp-idf-5.4.2-ascii`

Build from this directory:

```powershell
$env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; $env:IDF_CCACHE_ENABLE='0'; chcp 65001 | Out-Null; & 'C:\esp-idf-5.4.2-ascii\export.ps1'; idf.py -DCCACHE_ENABLE=0 build
```

Flash from this directory, preserving NVS save data:

```powershell
$env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; chcp 65001 | Out-Null; & 'C:\esp-idf-5.4.2-ascii\export.ps1'; idf.py -p COM10 -b 1500000 flash
```

Only erase flash when the user explicitly asks to clear the device.

## Resource Workflow

The SD resource pack lives under `assets/sdcard/` and is intended to be copied to the SD card manually by the user.

Do not add serial file-transfer, SD formatting, or upload helpers unless the user explicitly asks to revisit that approach.

The firmware reads `/sdcard/manifest.json` for the species catalog. The active resource pack targets Pokemon 1-200. It falls back to the built-in starter catalog if the manifest is missing or invalid.

Current displayed art is 64x64 PNG from `/sdcard/pokemon/{id}.png`. GIF files may be present in the pack, but playback is a later phase.

The Pokedex grid thumbnails use a firmware-embedded 1-200 RGB565 color badge table generated from the SD PNG artwork, so grid browsing should not depend on SD image reads.

The startup logo is firmware-embedded at `main/assets/cardpokemon_logo.png`; it should remain available without an SD card.

## Important Files

- `main/cardpokemon_main.cpp`: game loop, rendering, SD manifest catalog, input, save/load.
- `main/assets/cardpokemon_logo.png`: embedded startup splash logo.
- `main/pokemon_color_badges.h`: embedded color badge table for Pokedex grid thumbnails.
- `tools/fetch_pokemon_assets.py`: downloads and preprocesses PokeAPI JSON/PNG/GIF files into `assets/sdcard/`.
- `docs/GAME_DESIGN.md`: complete gameplay design bible for progression, battle, capture, items, shop, center, UI and implementation phases.
- `docs/ROADMAP.md`: phased project plan.
- `docs/RESOURCE_PACK.md`: SD card layout and manifest contract.
- `docs/ARCHITECTURE.md`: firmware architecture notes.

## Development Notes

Keep CardPokemon isolated inside this directory. Avoid changes to the legacy launcher or CarQuest app unless the user explicitly asks for integration work.

The compact display needs stable, low-noise screens. Prefer paging or replacing existing rows over adding crowded text.

Current encounter rule:

- Do not add simulated GPS, simulated distance, or debug keys for forced encounters unless the user explicitly asks for a test build.
- GPS is the movement gate: encounters require fresh GPS and a real accepted GPS step.
- IMU is heat only: it changes encounter target distance, wild rarity and wild level pressure, but it must not add travel distance by itself.
- Scale IMU effects by GPS speed. Walking should keep encounters winnable near the active Pokemon level; driving can preserve stronger road-intensity rarity/level pressure.
- Be careful with timer math on ESP32. `elapsed_ms(now, since)` should preserve unsigned wrap-around behavior.
- The current build was flashed to `COM10` after the GPS-first encounter refactor, preserving NVS save data.
