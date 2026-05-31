# CardPokemon Architecture

CardPokemon should be a standalone firmware, not a demo launcher app.

## Layers

```text
Hardware
  display, keyboard, speaker, IMU, GPS, SD card, power status

Resource System
  SD mount, manifest catalog, sprite/animation cache, metadata loader

Game State
  save data, owned Pokemon, active Pokemon, levels, XP, evolution, route counters

Simulation
  travel distance, wild encounters, auto battle, capture rolls, rewards

Scenes
  boot, title, starter select, travel, encounter, battle, pokedex, party, settings
```

## Key Principle

Images and animations are core to this project. The firmware should be designed around streaming or cached SD resources instead of treating sprites as tiny optional icons.

## Resource Budget

The current SD pack contains Pokemon 1-200 generated from SD `manifest.json`:

- 64x64 official-artwork PNG per Pokemon
- per-Pokemon JSON metadata
- animated GIF source when PokeAPI exposes one
- capture rate and immediate evolution metadata

The firmware consumes `64x64` PNGs, loads species names/types/stats/game metadata from `manifest.json`, and checks for matching GIF/JSON files. GIF playback is not active yet.

## Gameplay Model

Driving produces exploration progress. The player does not need to interact while moving.

- GPS distance gates wild encounters and is saved as total explored distance.
- GPS speed and fix quality are displayed in travel mode when fresh data is available.
- 6-axis IMU motion increases encounter heat, which shortens target encounter distance and raises rarity/level pressure.
- GPS distance and current partner level expand the wild Pokemon pool over time.
- Battles auto-resolve with compact formulas.
- Pokedex/party mode allows captured Pokemon selection.
- GPS speed selects the movement interpretation mode. Walk mode dampens IMU level pressure, while driving modes allow stronger IMU effects.

## Sensor Truth Model

The production firmware should reflect real device state:

- GPS position, speed, satellite count and HDOP come from the Cap LoRa-1262 GPS parser.
- Travel distance is computed from consecutive accepted GPS coordinates with `haversine_m()`.
- Accepted GPS steps must be fresh, larger than tiny noise, and below the speed-based maximum jump filter.
- IMU motion uses relative acceleration/gyro deltas from an adaptive still baseline.
- IMU motion must not add distance or trigger encounters by itself.
- At walking speed, IMU motion is treated mostly as activity for encounter pacing; level and rarity pressure are capped.
- At driving speed, IMU motion is treated as route/road intensity and can raise rarity and wild level more strongly.
- The only non-physical smoothing is display speed decay when GPS is stale; it is visual only and must not affect distance or encounters.

Known caveat:

- GPS drift may still count as small movement if coordinates move enough to pass the current filter. A future driving-integration pass should add stationary drift suppression or a stronger low-speed gate if field testing shows false distance gain.

## Current Encounter State Machine

Travel scene updates sensors once per interval. A wild encounter starts only when all of these are true:

- the scene is `Travel`
- encounter cooldown has elapsed
- GPS is fresh
- the latest sensor update accepted a real GPS step
- `distanceSinceEventM` has reached `nextEncounterTargetM`

When a battle starts, the firmware resets `distanceSinceEventM`, rolls the next target distance from the current heat/route/item state, records the wild Pokemon as seen, and switches to `Battle`. Battle resolution returns early from the game loop so the battle screen cannot be skipped in the same frame.
