# CardPokemon Roadmap

This plan treats CardPokemon as a standalone Cardputer ADV game, not a launcher app.

For the full gameplay specification, see `GAME_DESIGN.md`.

## Phase 0: Isolation And First Playable Build

Status: complete.

- Isolated `cardpokemon/` ESP-IDF project
- Direct boot into the game
- Starter selection, travel, encounter, auto battle and capture loop
- NVS save/load
- SD card PNG rendering fallback path

## Phase 1: Playable Foundation

Status: mostly complete.

- SD `manifest.json` driven species catalog
- 1-200 Pokemon resource pack generated from PokeAPI
- Stable small-screen UI with resource status
- Save format that can survive catalog growth
- Manual SD card copy workflow
- GPS-first movement gate and 6-axis heat hooked into exploration

Exit criteria:

- Game runs with or without SD card
- With SD card, species names/stats/types come from manifest data
- No hard dependency on a fixed 10-entry C++ species table
- Encounters require fresh GPS movement, while IMU heat changes frequency, rarity and level pressure

## Phase 2: Center And Economy

Status: in progress.

- Camp Center with heal, shop and bag pages
- Gold, Potion and Poke Ball economy
- Seen/caught Pokedex distinction
- Captures now consume Poke Balls
- First UI layout pass for Travel, Battle, Pokedex, Center, Shop and Bag

## Phase 3: Better Pokemon Feel

Status: initial implementation complete.

- Type effectiveness in auto battle
- Route-weighted wild encounters for Walk, City, Road, Highway, Rough and Lost states
- Great Ball, Repel and Lure items
- Evolution flash feedback
- Pokedex 2x4 grid browser with detail page
- Refactored encounter logic: GPS movement gates encounters; IMU heat tunes encounter distance, rarity and wild level
- GPS speed-aware IMU scaling: walking dampens level pressure, driving preserves stronger road-intensity effects

## Phase 4: Visual Identity

Status: planned.

- GIF-to-runtime animation conversion pipeline
- Animated travel/battle/Pokedex sprites
- Pokedex feature screen with larger art
- Resource validation tool for missing or oversized assets

## Phase 5: Game Depth

Status: planned.

- Deeper encounter tables by biome, distance, motion and rarity
- Evolution chains from resource data
- Party management and simple healing/rest mode
- Balanced XP/capture formulas

## Phase 6: Driving Integration

Status: planned.

- Field-test GPS-first encounter reliability after the encounter refactor
- Field-test walking balance after speed-aware IMU scaling; walking should produce winnable encounters near the active Pokemon level
- Add stationary GPS drift suppression if real-world tests show distance gain while parked
- Consider stronger low-speed gating for accepted GPS steps
- Trip/session summaries and GPS quality feedback
- Stop/camp detection
- Optional LoRa beacons after the core loop is stable

## Current Field-Test Notes

- Latest build was flashed to `COM10` after the GPS-first encounter refactor, preserving NVS.
- There is no simulated GPS, simulated movement, key-driven distance gain or key-driven forced encounter in the production build.
- Required encounter conditions are: Travel scene, cooldown elapsed, GPS fresh, accepted GPS step, and distance since last event reaching the current target distance.
- IMU heat now changes target distance, wild rarity and wild level pressure, but cannot trigger an encounter without GPS movement.
- Watch for GPS drift in parked tests; current filtering handles impossible jumps but not all small coordinate wander.

## Phase 7: Scale-Up

Status: planned.

- Tune the 1-200 pack and decide whether to grow beyond 200
- Compact binary catalog if JSON parse cost becomes too high
- Indexed animation format if GIF decode is too heavy
- Save migration and compatibility tests
