# CardPokemon

CardPokemon is a standalone Cardputer ADV firmware concept for a travel-driven monster collecting game.

This directory is intentionally isolated from the existing demo launcher and CarQuest app. The goal is for the device to boot directly into this game rather than running as one app inside the official demo firmware.

## Target

- Hardware: M5Stack Cardputer ADV
- Chip: ESP32-S3
- Storage model: firmware for game logic, SD card for Pokemon data and image/animation assets
- Sensors: GPS movement gates exploration; IMU motion heat tunes encounter frequency, rarity and level
- Save data: NVS for player progress, owned Pokemon, active partner, XP and levels

## Current Scope

This directory contains a buildable standalone game firmware. It boots directly into CardPokemon and uses the parent Cardputer ADV HAL without registering as a Mooncake launcher app.

Implemented:

- Boot directly into CardPokemon
- Choose one of three level-1 starter Pokemon
- Travel mode driven by GPS/IMU
- Wild encounters require fresh GPS movement; 6-axis motion heat changes encounter distance, rarity and level pressure
- Auto-resolved battle inspired by CarQuest
- XP, level-up and evolution
- Low-probability capture after victory
- Gold, Potion and Poke Ball economy
- Camp Center with heal, shop and bag pages
- Pokedex grid browser with firmware-embedded color badges and detail page for selecting captured Pokemon
- NVS save data for seen/owned Pokemon, active partner, XP, level, HP, gold and items
- NVS save data for total explored distance
- SD `manifest.json` driven species catalog with built-in fallback
- SD resource pack status check for PNG/GIF/JSON assets
- 64x64 Pokemon PNG rendering from SD card with built-in placeholder fallback
- Embedded Cardputer-style startup logo PNG
- Embedded 1-200 Pokemon color badge table for SD-independent Pokedex thumbnails
- 1-200 Pokemon SD resource pack generated from PokeAPI

Current firmware notes:

- The latest flashed build uses real Cap LoRa-1262 GPS data only for movement and distance. There is no simulated GPS, simulated distance, or debug key that forces encounters.
- IMU data is real and relative-calibrated. It affects heat, rarity and level pressure, but it does not create travel distance by itself.
- GPS speed now changes how IMU is interpreted: walking dampens level pressure, while driving keeps stronger road-intensity effects.
- A valid encounter requires fresh GPS, a real GPS step, the 45-second cooldown to be over, and `distanceSinceEventM >= nextEncounterTargetM`.
- GPS drift can still appear as small movement if the module reports changing coordinates; current filtering rejects very large impossible jumps but does not yet do advanced stationary drift suppression.
- When GPS is stale, the displayed speed decays visually, but that decay does not add distance or trigger encounters.

Controls:

- Arrow keys: choose starter or move in Pokedex/menus
- Enter: confirm or close a resolved battle
- `P`: open Pokedex/party
- `C`: open Camp Center from Travel
- Up/Down: move Center, Shop and Bag menus
- Space: return from Pokedex detail/grid or battle result

## SD Card Assets

The firmware expects this layout on a FAT32 SD card:

```text
pokemon/
  1.png
  1.gif
  1.json
  ...
manifest.json
```

Generate the current 200-Pokemon resource pack with:

```powershell
python cardpokemon/tools/fetch_pokemon_assets.py --ids 1-200
```

For a tiny desk-test pack, use a small range such as `--ids 1-10`.

Then copy the contents of `cardpokemon/assets/sdcard/` to the SD card root.

## Directory Layout

```text
cardpokemon/
  CMakeLists.txt
  partitions.csv
  sdkconfig.defaults
  dependencies.lock
  main/
    CMakeLists.txt
    assets/
      cardpokemon_logo.png
    cardpokemon_main.cpp
    pokemon_color_badges.h
  assets/
    README.md
  docs/
    ARCHITECTURE.md
    GAME_DESIGN.md
    RESOURCE_PACK.md
    ROADMAP.md
  tools/
    README.md
```

## Build Direction

Build from `cardpokemon/` as its own ESP-IDF project, not through the repository root demo app.

```powershell
cd cardpokemon
$env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; $env:IDF_CCACHE_ENABLE='0'; chcp 65001 | Out-Null; & 'C:\esp-idf-5.4.2-ascii\export.ps1'; idf.py -DCCACHE_ENABLE=0 build
```

The firmware has been verified to build for ESP32-S3 with ESP-IDF 5.4.2.
