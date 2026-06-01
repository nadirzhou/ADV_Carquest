# CardPokemon Agent Workflow

This document defines how future CardPokemon work should be split across a main agent and focused sub-agents.

The goal is to keep firmware, gameplay, UI, assets and documentation moving quickly without mixing concerns or accidentally breaking field-tested behavior.

## Coordination Model

The main agent owns the final result.

Main agent responsibilities:

- interpret the user's request and decide which sub-agent roles are needed
- keep the active branch and working tree clean
- assign scoped tasks to sub-agents
- merge or reject sub-agent output
- run the final build before any flash
- flash only when the user asks for it
- update docs when behavior, workflow or field-test expectations change
- preserve unrelated local user changes

Sub-agents provide focused analysis or patches. They should not independently change project direction.

## Sub-Agent Roles

### Firmware Agent

Scope:

- ESP-IDF project structure, CMake, sdkconfig and partitions
- Cardputer ADV HAL use
- Cap LoRa-1262 GPS integration
- IMU reading and filtering
- SD/FATFS, NVS and boot behavior
- build and flash diagnostics

Primary files:

- `cardpokemon/CMakeLists.txt`
- `cardpokemon/main/CMakeLists.txt`
- `cardpokemon/main/cardpokemon_main.cpp`
- `cardpokemon/sdkconfig.defaults`
- `cardpokemon/partitions.csv`
- `main/hal/cap_lora868/`

Boundaries:

- Do not rebalance gameplay formulas unless asked by the main agent.
- Do not redesign UI beyond fixing rendering or hardware display bugs.
- Do not reintroduce simulated GPS, simulated distance, or forced-encounter debug keys in production builds.

### Gameplay Agent

Scope:

- travel loop and encounter conditions
- GPS/IMU gameplay interpretation
- wild Pokemon rarity and level pressure
- auto-battle formula
- XP, level-up, evolution and capture
- shop, bag, healing and economy balance

Primary files:

- `cardpokemon/main/cardpokemon_main.cpp`
- `cardpokemon/docs/GAME_DESIGN.md`
- `cardpokemon/docs/ROADMAP.md`

Boundaries:

- Do not change HAL or hardware drivers.
- Do not change SD asset schema without involving the Assets Agent.
- Explain expected player-facing behavior for every balance change.

### UI Agent

Scope:

- 240x135 travel, battle, Pokedex, center, shop and bag layout
- readability, overlap, row count and small-screen ergonomics
- waveform, bars, badges, menus and navigation
- startup screen and visual polish

Primary files:

- `cardpokemon/main/cardpokemon_main.cpp`
- `cardpokemon/main/assets/`
- `cardpokemon/docs/GAME_DESIGN.md`

Boundaries:

- Do not change game balance formulas.
- Do not change sensor truth or encounter gates.
- Keep screens dense but readable; avoid explanatory in-app text that crowds the display.

### Assets Agent

Scope:

- SD resource pack generation and validation
- PNG/GIF processing and runtime conversion options
- `manifest.json` and per-Pokemon JSON data
- firmware-embedded color badges
- resource size and loading constraints

Primary files:

- `cardpokemon/assets/`
- `cardpokemon/main/assets/`
- `cardpokemon/main/pokemon_color_badges.h`
- `cardpokemon/tools/fetch_pokemon_assets.py`
- `cardpokemon/docs/RESOURCE_PACK.md`

Boundaries:

- Keep temporary experiments out of the repository root.
- Do not add serial upload or SD formatting helpers unless the user explicitly asks to revisit that workflow.
- Document any generated asset workflow that becomes part of the firmware.

### Docs/QA Agent

Scope:

- documentation consistency
- field-test checklist and results
- regression risks and test gaps
- AGENTS instructions and workflow updates
- review for stale debug code or behavior-doc mismatch

Primary files:

- `AGENTS.md`
- `cardpokemon/AGENTS.md`
- `cardpokemon/README.md`
- `cardpokemon/docs/*.md`

Boundaries:

- Do not implement feature logic unless explicitly asked.
- Prefer finding mismatches and handing them back to the main agent or the owning sub-agent.

## Task Routing

Use the smallest set of sub-agents that covers the task.

Examples:

| User request | Suggested routing |
|---|---|
| "Walking encounters are too hard" | Gameplay Agent, Docs/QA Agent |
| "GPS distance is not increasing" | Firmware Agent, Docs/QA Agent |
| "Pokedex layout feels bad" | UI Agent, Docs/QA Agent |
| "Use GIF on the home screen" | Assets Agent, Firmware Agent, UI Agent |
| "Pokemon data is missing from SD" | Assets Agent, Firmware Agent |
| "Write a full design for the next phase" | Gameplay Agent, UI Agent, Docs/QA Agent |

## File Ownership Rules

- `cardpokemon/main/cardpokemon_main.cpp` is shared. The main agent must review all patches to this file.
- `main/hal/` is firmware-owned. Gameplay/UI/Assets agents should not edit it directly.
- `cardpokemon/assets/sdcard/` is asset-owned. Do not manually patch generated Pokemon files unless the user requests a one-off fix.
- `cardpokemon/docs/` is shared, but Docs/QA should keep it coherent after feature changes.
- Root-level files should be changed only for repository-wide rules or shared hardware/build assumptions.

## Required Handoff Format

Each sub-agent should return:

```text
Role:
Task:
Files touched:
Behavior impact:
Risks:
Verification:
Follow-up:
```

For code patches, include the exact build or test command that should be run by the main agent.

## Build And Flash Rules

Build CardPokemon from `cardpokemon/`:

```powershell
$env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; $env:IDF_CCACHE_ENABLE='0'; chcp 65001 | Out-Null; & 'C:\esp-idf-5.4.2-ascii\export.ps1'; idf.py -DCCACHE_ENABLE=0 build
```

Flash only after a successful build, and only when the user asks:

```powershell
$env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; chcp 65001 | Out-Null; & 'C:\esp-idf-5.4.2-ascii\export.ps1'; idf.py -p COM10 -b 1500000 flash
```

Do not erase flash unless the user explicitly asks to clear NVS/player save data.

## Documentation Rules

Update documentation in the same task when changes affect:

- encounter conditions
- GPS/IMU interpretation
- battle, capture, XP, evolution or economy formulas
- SD card layout or asset generation
- controls or UI navigation
- build, flash or hardware assumptions
- field-test expectations

At minimum, update one of:

- `cardpokemon/README.md` for current user-facing state
- `cardpokemon/docs/GAME_DESIGN.md` for gameplay rules
- `cardpokemon/docs/ARCHITECTURE.md` for system model
- `cardpokemon/docs/RESOURCE_PACK.md` for asset contracts
- `cardpokemon/docs/ROADMAP.md` for phase status and field-test notes
- `cardpokemon/AGENTS.md` for future agent instructions

## Field-Test Discipline

When a change is meant to affect real-world behavior, record what the user should test.

Useful test dimensions:

- indoor no-GPS behavior
- outdoor stationary GPS behavior
- walking encounter frequency
- walking wild level range
- driving encounter frequency
- driving wild level range
- GPS drift while parked
- battle screen timing and auto-return
- SD resource loading after boot

If repeated field notes grow beyond `ROADMAP.md`, create `cardpokemon/docs/FIELD_TEST.md` and move current notes there.

## Current Guardrails

- CardPokemon remains isolated under `cardpokemon/`.
- The production build should use real GPS movement only for travel distance.
- IMU is activity heat and must not create fake distance.
- Walking should keep wild encounters near the active Pokemon's level.
- Driving may use stronger IMU-driven rarity and level pressure.
- Pokedex grid thumbnails use firmware-embedded RGB565 color badges.
- Pokemon art/data remains SD-card based, with built-in fallback behavior.
