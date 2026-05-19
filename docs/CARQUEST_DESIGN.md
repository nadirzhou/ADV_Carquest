# CarQuest Game Design Bible

Version: 0.1  
Target hardware: M5Stack Cardputer ADV + Cap LoRa/GPS  
Design goal: a tiny, travel-driven fantasy adventure that feels like a compact tabletop RPG, but can run as an automatic car terminal game.

## 1. High Concept

CarQuest is a location-aware road fantasy RPG. The player is a roaming adventurer whose car is treated as a magical expedition wagon. Movement through the real world becomes exploration. Distance, speed, GPS state, and 6-axis motion data create encounters, treasure caches, hazards, and camp moments.

The player does not need to operate the game while moving. During travel, CarQuest runs as an automatic adventure log. When the vehicle stops, the game enters Camp mode, where the player can heal, open chests, manage loot, craft upgrades, and prepare for the next road.

The tone is "Dungeons & Dragons meets dashboard companion": monsters, relics, weapons, supplies, roadside camps, cursed signals, and strange routes, all expressed through small-screen text and later through compact icons.

## 2. Core Loop

1. Travel
   - GPS distance and IMU motion accumulate exploration progress.
   - Every fixed distance interval, the game rolls an event.
   - Strong motion, fast acceleration, braking, or rough turns increase danger and reward.

2. Encounter
   - The event may be a monster, hazard, treasure cache, shrine, merchant, road omen, or LoRa signal.
   - Moving encounters resolve automatically with player stats, equipment, motion bonus, and random rolls.

3. Reward Or Cost
   - Victory grants XP, gold, scrap, keys, potions, relic fragments, or equipment.
   - Defeat costs HP, gold, supplies, or forces an emergency camp.

4. Camp
   - Triggered after the vehicle stops for a period.
   - Player can rest, use potions, open chests, craft equipment, inspect inventory, and review the log.

5. Progression
   - XP increases level.
   - Level increases HP and base stats.
   - Gear and relics create build identity.
   - Higher levels unlock rarer monsters, better treasure, and deeper route events.

## 3. Player Character

The player is represented by a compact stat block suitable for a 204x109 display.

### Primary Stats

| Stat | Short | Meaning | Default |
|---|---:|---|---:|
| Level | L | Overall character tier | 1 |
| Hit Points | HP | Damage capacity | 30 |
| Attack | ATK | Offensive power | 6 |
| Defense | DEF | Damage reduction and survival | 3 |
| Focus | FOC | Helps against magic, fear, signal, and trap events | 3 |
| Luck | LUK | Improves treasure, crits, rare events, and chest quality | 2 |
| XP | XP | Level progress | 0 |
| Gold | G | Currency | 0 |

Current firmware already has Level, HP, ATK, DEF, XP, Gold. Focus and Luck are recommended additions once the UI has room or a detail page.

### Derived Stats

| Derived Stat | Formula | Notes |
|---|---|---|
| Max HP | `24 + Level * 6 + END bonus` | Current firmware uses +6 HP per level, which fits. |
| Power | `Level * 6 + ATK + weapon bonus` | Used for auto combat. |
| Guard | `DEF + armor bonus + shield bonus` | Reduces incoming damage. |
| Resolve | `FOC + relic bonus` | Used against curses, signal ghosts, fear, illusions. |
| Loot Rating | `LUK + route tier + chest tier` | Used for treasure quality. |
| Drive Bonus | `clamp(MotionScore * 2, 0, 8)` | Existing motion bonus can remain. |

### Character Archetypes

Archetypes are optional lightweight builds. The first version can assign no class, then later unlock one at Level 3.

| Archetype | Fantasy Role | Road Fantasy Flavor | Stat Bias |
|---|---|---|---|
| Road Knight | Fighter | Heavy vehicle guardian | +HP, +DEF |
| Signal Mage | Wizard | Uses GPS/LoRa omens | +FOC, relic effects |
| Scrap Ranger | Ranger | Scavenger and route scout | +ATK, +LUK |
| Gear Cleric | Cleric | Repairs and protects | +healing, +camp actions |
| Drift Rogue | Rogue | Risk and reward specialist | +LUK, crit chance |

## 4. Level Progression

CarQuest should use short-session friendly XP numbers. A player should level up after several meaningful trips, not after hours of grinding.

### XP Curve

Recommended formula:

```text
XP needed for next level = 16 + Level * 18 + floor(Level^2 * 2)
```

Simpler firmware formula:

```text
XP needed = Level * 18
```

The current firmware already uses `Level * 18`. That is good for early prototyping. The expanded curve should replace it once content depth increases.

### Level Table

| Level | XP To Next | Max HP | ATK | DEF | Unlock |
|---:|---:|---:|---:|---:|---|
| 1 | 18 | 30 | 6 | 3 | Basic travel events |
| 2 | 36 | 36 | 7 | 4 | Tier 1 gear crafting |
| 3 | 60 | 42 | 8 | 4 | Choose archetype |
| 4 | 84 | 48 | 9 | 5 | Relic slot 1 |
| 5 | 118 | 54 | 10 | 5 | Elite monsters |
| 6 | 156 | 60 | 11 | 6 | Camp upgrades |
| 7 | 202 | 66 | 12 | 6 | Relic slot 2 |
| 8 | 256 | 72 | 13 | 7 | Route bosses |
| 9 | 318 | 78 | 14 | 7 | Legendary loot |
| 10 | 390 | 84 | 15 | 8 | Endgame route omens |

### Level-Up Rewards

Every level:

- Max HP +6
- HP restored to full
- ATK +1

Every even level:

- DEF +1

Every third level:

- Choose one boon:
  - +1 Focus
  - +1 Luck
  - +1 Max potion capacity
  - +1 chest quality
  - +1 camp healing

## 5. Attribute And Roll System

CarQuest should feel tabletop-like, but use tiny formulas that are easy to implement.

### Core Roll

```text
Score = Stat + Gear + Level Bonus + Drive Bonus + random(0..10)
```

Recommended result bands:

| Margin | Result |
|---:|---|
| `Score >= Difficulty + 8` | Critical success |
| `Score >= Difficulty` | Success |
| `Score >= Difficulty - 5` | Partial success |
| Otherwise | Failure |

### Combat Roll

```text
PlayerPower = Level * 6 + ATK + WeaponPower + DriveBonus + random(0..10)
MonsterPower = MonsterLevel * 7 + Danger + MonsterTrait + random(0..8)
```

If `PlayerPower >= MonsterPower`, player wins.

If player loses:

```text
Damage = max(1, MonsterPower - PlayerPower / 2 - Guard / 2)
```

The current firmware already uses a close version of this and should evolve gradually.

### Criticals

Critical success triggers when:

```text
PlayerPower >= MonsterPower + 8
```

Critical rewards:

- +25% XP
- +25% gold
- Higher chance for scrap, key, or relic fragment

Critical failure triggers when:

```text
MonsterPower >= PlayerPower + 10
```

Critical failure effects:

- Extra HP loss
- Lose 1 gold bundle
- Equipment durability loss if durability is implemented

## 6. Movement And Sensor Rules

CarQuest should translate real movement into fantasy pressure.

### Travel State

| Signal | Game Meaning |
|---|---|
| GPS distance | Exploration progress |
| GPS speed | Route pace |
| IMU motion score | Road danger and combat momentum |
| Stop duration | Camp trigger |
| Satellite count | Navigation certainty |
| LoRa messages | Nearby adventurer/party signal |

### Route Danger

```text
RouteDanger = clamp(MotionScore * 10 + random(0..40), 0, 100)
```

| Danger | Event Bias |
|---:|---|
| 0-24 | Safe travel, small treasure, merchant |
| 25-44 | Cache, minor hazard, weak monster |
| 45-69 | Standard monster, trap, better treasure |
| 70-89 | Elite monster, cursed event, rare chest |
| 90-100 | Boss omen, ambush, legendary chance |

### Event Distance

Recommended:

```text
Next event distance = current distance + random(160m..360m)
```

This matches the current firmware. Later, route type can modify it:

| Route Type | Event Interval | Flavor |
|---|---:|---|
| City | 120-240m | More small events |
| Highway | 300-700m | Fewer, stronger events |
| Mountain | 180-420m | More hazards |
| Unknown road | 140-500m | More weird events |

## 7. Combat System

Combat must resolve automatically during movement. Camp can show summaries and allow preparation, but should not require real-time operation.

### Combat Phases

1. Encounter appears.
2. Game computes route danger and monster level.
3. Player power is calculated from level, ATK, weapon, relics, and drive bonus.
4. Monster power is calculated from monster level, family, danger, and traits.
5. Resolve outcome.
6. Log result in one short line.

### Damage Types

| Type | Use |
|---|---|
| Physical | Beasts, raiders, armor checks |
| Arcane | spirits, curses, relic events |
| Signal | GPS/LoRa ghosts, static entities |
| Road | hazards, traps, braking, rough motion |

Tiny firmware version can keep one HP value and one DEF value. Damage type is mainly for naming and future resistances.

### Status Effects

| Status | Effect | Duration |
|---|---|---|
| Wounded | -1 ATK | Until camp rest |
| Shaken | -1 FOC | Next 2 events |
| Jammed | LoRa/GPS event penalty | Next event |
| Inspired | +2 Power | Next event |
| Guarded | -2 incoming damage | Next event |
| Lucky | +1 loot roll | Next treasure |

## 8. Monster Families

Monster names should be short enough for the display. Full descriptions can live in design docs and later in a codex page.

### Monster Stat Template

| Field | Meaning |
|---|---|
| Name | Short display name |
| Family | Monster group |
| Level | Usually player level -1 to +2 |
| Danger | Extra threat modifier |
| Power Mod | Combat adjustment |
| Damage Type | Physical, Arcane, Signal, Road |
| Reward Type | XP, gold, scrap, key, relic |
| Trait | Special rule |

### Family 1: Road Wraiths

Ghosts of forgotten routes, wrong turns, dead GPS traces, and old mile markers.

| Monster | Level Range | Danger | Trait | Reward |
|---|---:|---:|---|---|
| Mile Wisp | 1-3 | 1 | Low damage, high escape chance | XP, small gold |
| Road Wraith | 2-6 | 5 | Ignores 1 DEF | XP, scrap |
| Fogbound Rider | 4-8 | 7 | Tests Focus on loss | XP, relic fragment |
| Dead Route King | 8-10 | 12 | Boss, causes Shaken | XP, relic |

### Family 2: Signal Imps

Small malicious entities living in static, LoRa packets, GPS drift, and bad coordinates.

| Monster | Level Range | Danger | Trait | Reward |
|---|---:|---:|---|---|
| Signal Imp | 1-5 | 2 | Weak but common | XP, gold |
| Packet Biter | 2-6 | 4 | May cause Jammed | XP, scrap |
| Static Gremlin | 3-7 | 6 | Tests Focus | XP, potion |
| LoRa Hexer | 6-10 | 10 | Elite, signal damage | XP, relic fragment |

### Family 3: Asphalt Beasts

Physical fantasy beasts born from roads, tires, engines, and heat shimmer.

| Monster | Level Range | Danger | Trait | Reward |
|---|---:|---:|---|---|
| Tar Slime | 1-3 | 2 | Reduces speed reward | XP, scrap |
| Rumble Hound | 2-5 | 4 | Extra damage at high motion | XP, gold |
| Guardrail Drake | 4-8 | 8 | High DEF | XP, key |
| Overpass Hydra | 8-10 | 13 | Boss, multiple hits | XP, relic |

### Family 4: Rust Cultists

Bandits, scavengers, and roadside machine-priests who worship broken gear.

| Monster | Level Range | Danger | Trait | Reward |
|---|---:|---:|---|---|
| Rust Acolyte | 1-4 | 3 | Drops scrap | XP, scrap |
| Wrench Raider | 3-6 | 5 | May steal gold on loss | XP, gold |
| Scrap Knight | 5-9 | 8 | High armor | XP, equipment |
| Chrome Prophet | 8-10 | 12 | Boss, curse chance | XP, relic |

## 9. Monster Scaling

Recommended monster level:

```text
MonsterLevel = clamp(PlayerLevel + random(-1..2), 1, 10)
```

Danger modifier:

```text
Danger = FamilyBaseDanger + RouteDangerBandBonus
```

Route danger band bonus:

| Route Danger | Bonus |
|---:|---:|
| 0-24 | -1 |
| 25-44 | 0 |
| 45-69 | +2 |
| 70-89 | +4 |
| 90-100 | +7 |

### Monster Rewards

```text
XP = 4 + MonsterLevel * 3 + Danger
Gold = random(1..4 + MonsterLevel * 2)
```

Elite multiplier:

- XP x1.25
- Gold x1.25
- Rare drop chance +10%

Boss multiplier:

- XP x2
- Gold x2
- Guaranteed relic fragment or unique item

## 10. Treasure System

Treasure should be exciting but compact. The firmware can store counts and item tiers instead of large inventories at first.

### Treasure Types

| Type | Use |
|---|---|
| Gold | Currency and score |
| Potion | Healing consumable |
| Scrap | Crafting and upgrades |
| Key | Opens cached chests |
| Relic Fragment | Long-term magic progression |
| Equipment | Weapon, armor, charm, module |
| Map Shard | Unlocks special route events |

### Treasure Cache Roll

```text
LootScore = Tier + Luck + random(0..100)
```

| LootScore | Reward |
|---:|---|
| 0-39 | Gold + scrap |
| 40-59 | Gold + potion |
| 60-74 | Gold + key |
| 75-89 | Gold + equipment chance |
| 90-100 | Relic fragment or rare item |
| 101+ | Unique relic or boss omen |

### Chest Types

| Chest | Key Cost | Reward |
|---|---:|---|
| Road Cache | 0 | Small gold, scrap/potion |
| Iron Cache | 1 | Medium gold, equipment chance |
| Signal-Locked Cache | 1 | Relic fragment chance |
| Dragon Trunk | 2 | High gold, rare equipment |
| Ancient Glovebox | Special | Unique relic |

## 11. Equipment System

Initial firmware can keep only `weaponTier`. Later versions can add slots.

### Equipment Slots

| Slot | Examples | Gameplay |
|---|---|---|
| Weapon | blade, wand, wrench, lance | Adds ATK and combat traits |
| Armor | coat, mail, plated jacket | Adds DEF and HP |
| Charm | compass, coin, sigil | Adds Luck/Focus |
| Module | GPS rune, LoRa crystal | Sensor and event effects |
| Relic | unique magical item | Build-defining effect |

### Weapon Tiers

| Tier | Name | ATK Bonus | Craft Cost | Notes |
|---:|---|---:|---:|---|
| 0 | Road Knife | +0 | 0 | Starter |
| 1 | Scrap Saber | +2 | 2 scrap | Current firmware equivalent |
| 2 | Signal Spear | +4 | 3 scrap | +1 vs Signal enemies |
| 3 | Asphalt Axe | +6 | 5 scrap | +1 vs Beasts |
| 4 | Dragon Torque Blade | +8 | 8 scrap + relic fragment | Rare |
| 5 | Starwheel Lance | +11 | 12 scrap + relic | Legendary |

Recommended current formula:

```text
Upgrade cost = 2 + WeaponTier
ATK gain = +2 per tier
```

This matches the current prototype.

### Armor Tiers

| Tier | Name | DEF Bonus | Extra |
|---:|---|---:|---|
| 0 | Travel Jacket | +0 | Starter |
| 1 | Reinforced Coat | +1 | -1 road damage |
| 2 | Chain Vest | +2 | +2 Max HP |
| 3 | Guardrail Mail | +3 | -1 monster damage |
| 4 | Chrome Plate | +4 | Resist critical failure |
| 5 | Dragonhide Harness | +5 | Boss resistance |

### Charms

| Charm | Effect |
|---|---|
| Copper Compass | +1 Luck on treasure |
| Silver Spark Plug | +1 Focus |
| Saint Mile Marker | Rest heals +2 |
| Soft Brake Sigil | Reduces Road hazard damage |
| Northbound Coin | Chest rare chance +5% |

### Relics

Relics should be few, memorable, and powerful.

| Relic | Effect |
|---|---|
| Crown of the Road King | +1 all rolls during highway travel |
| Lantern of Lost Turns | Prevents first Shaken effect each trip |
| Dragon Key | Opens one locked chest without consuming a key per day/session |
| LoRa Moonstone | Nearby players increase reward chance |
| Glovebox Grimoire | Focus can convert a loss into partial success |
| Starwheel | +1 event tier after every 1km of travel |

## 12. Consumables

| Item | Effect |
|---|---|
| Potion | Heal `12 + Level * 2` HP |
| Greater Potion | Heal `24 + Level * 3` HP |
| Repair Kit | Remove Wounded |
| Lucky Snack | +1 loot roll on next treasure |
| Signal Ward | Prevent Jammed |
| Campfire Token | Rest without waiting for stop timer |

Current firmware already supports potions with `12 + Level * 2` healing.

## 13. Camp System

Camp is the main interactive mode. It should feel calm and useful.

### Camp Trigger

```text
If stopped for 15 seconds, enter Camp.
```

Current firmware uses 15 seconds.

### Camp Actions

| Action | Effect | Requirement |
|---|---|---|
| Rest | Heal small HP | Camp mode |
| Use Potion | Heal large HP | Potion > 0 |
| Open Chest | Spend key, gain loot | Key > 0 |
| Upgrade Weapon | Spend scrap, +ATK | Enough scrap |
| Repair Gear | Remove Wounded/Jammed | Scrap or kit |
| Review Log | See recent events | Always |
| Manage Relics | Equip relics | Relic unlocked |
| Party Signal | View LoRa nearby players | LoRa available |

### Rest Rules

Prototype:

```text
Rest heals 4 HP.
```

Expanded:

```text
Rest heals 4 + Level + camp bonus.
```

Optional long rest:

```text
After 10 minutes stopped, full heal once per real-world session.
```

## 14. Event System

Events should be short, readable, and resolved quickly.

### Event Categories

| Category | Example | Outcome |
|---|---|---|
| Monster | Road Wraith attacks | Combat |
| Treasure | Hidden cache found | Loot roll |
| Hazard | Sudden cursed pothole | Road damage or scrap |
| Shrine | Mile marker glows | Heal or boon |
| Merchant | Wandering mechanic | Buy/sell/craft |
| Signal | LoRa/GPS anomaly | Focus check |
| Quest | Follow omen for distance | Long-term reward |
| Boss Omen | Dragon shadow over route | Multi-stage event |

### Event Selection

```text
Risk = MotionScore * 10 + random(0..40)
```

| Risk | Event |
|---:|---|
| 0-25 | Treasure, shrine, merchant |
| 26-45 | Treasure, minor hazard |
| 46-70 | Standard monster |
| 71-88 | Elite monster or curse |
| 89-100 | Boss omen or rare treasure |

### Route Quest Examples

| Quest | Trigger | Goal | Reward |
|---|---|---|---|
| The Three Mile Curse | Defeat 3 Wraiths | Travel 3 events without KO | Relic fragment |
| Rust Choir | Find 5 scrap | Upgrade at camp | Scrap charm |
| Dragon Underpass | High danger boss omen | Survive boss event | Dragon Key |
| The Lost Coordinate | GPS drift event | Travel 1km | Map shard |

## 15. LoRa Multiplayer Ideas

LoRa should feel like nearby adventurers crossing paths.

### Beacon Packet

Prototype format:

```text
CQ1|id|level|distance|mode
```

Current firmware sends a similar `SQ1` packet. It should be renamed to `CQ1`.

### Multiplayer Effects

| Nearby Signal | Effect |
|---|---|
| Same level nearby | +1 morale on next event |
| Higher level nearby | Small chance of aid |
| Lower level nearby | Player gains guardian bonus |
| Multiple signals | Party aura, +loot chance |
| Distress signal | Optional rescue event |

### Party Aura

```text
PartyBonus = min(3, nearbyPlayers)
```

Effects:

- +PartyBonus to combat rolls
- +PartyBonus to treasure rolls
- Rare chance for shared boss omen

## 16. Economy

Gold should be useful but not too complex.

### Gold Uses

| Use | Cost |
|---|---:|
| Buy potion | 8 + Level |
| Buy key | 20 + Level * 2 |
| Repair status | 6 + Level |
| Merchant reroll | 5 |
| Identify relic | 25 |

### Scrap Uses

| Use | Cost |
|---|---:|
| Weapon upgrade | 2 + WeaponTier |
| Armor upgrade | 3 + ArmorTier |
| Repair equipment | 1-3 |
| Craft signal ward | 2 |

## 17. Display Rules

CarQuest must stay readable on the Cardputer ADV display.

### Main Moving Screen

Recommended lines:

```text
CarQuest          MOVING
L3 HP 42/42 XP 12/60
A8 D4 W1 G36
820m 42.5kmh M1.8
Defeated Road Wraith
+11xp +7g Wraith
```

### Camp Screen

Recommended lines:

```text
CarQuest            CAMP
L3 HP 31/42 G36
Pot2 Key1 Scr4 Rel0
> Rest +4hp
  Open chest
  Upgrade W
```

### Text Rules

- Every visible line should fit in about 24 characters.
- Use one-line event summaries.
- Use short stat labels.
- Avoid long item names on main screens.
- Longer names can appear in future detail pages.

## 18. Save Data

Recommended persistent fields:

| Field | Type | Notes |
|---|---|---|
| level | uint8 | 1-10 initially |
| hp | int16 | Current HP |
| maxHp | int16 | Max HP |
| atk | int16 | Base attack |
| def | int16 | Base defense |
| focus | int16 | Optional |
| luck | int16 | Optional |
| xp | uint16 | Current level progress |
| gold | uint16 | Currency |
| potions | uint8 | Consumable |
| scrap | uint8 | Crafting |
| keys | uint8 | Chest keys |
| relics | uint8 | Relic fragments or count |
| weaponTier | uint8 | Current weapon |
| armorTier | uint8 | Future |
| totalDistanceM | uint32 | Lifetime travel |
| defeatedMonsters | uint16 | Lifetime stat |
| openedChests | uint16 | Lifetime stat |

## 19. Implementation Roadmap

### Phase 1: Current Prototype Stabilization

- Keep automatic movement events.
- Keep Level, HP, ATK, DEF, XP, Gold, Potion, Scrap, Key, Relic, WeaponTier.
- Rename LoRa packet prefix from `SQ1` to `CQ1`.
- Add more monster names and event names.
- Add persistent save/load.

### Phase 2: Content Expansion

- Add monster family table.
- Add treasure quality table.
- Add armor tier.
- Add Focus and Luck.
- Add status effects.
- Add route danger bands.

### Phase 3: Camp Depth

- Add inventory detail page.
- Add merchant events.
- Add relic management.
- Add quest log.
- Add longer event review.

### Phase 4: Sensor Fantasy

- Use GPS satellite count for navigation uncertainty.
- Use speed bands for route type.
- Use IMU spikes for hazards.
- Use LoRa nearby beacons for party aura.

### Phase 5: Presentation

- Add icon set.
- Add compact monster/treasure icons.
- Add event splash frames.
- Add optional sound cues.

## 20. Naming Guidelines

Names should be short, readable, and flavorful.

### Good Display Names

- Road Wraith
- Signal Imp
- Tar Slime
- Scrap Knight
- Lost Cache
- Dragon Key
- Starwheel
- Rust Charm

### Avoid

- Names longer than 18 characters on main screen.
- Dense lore text during movement.
- Rules that require manual operation while driving.

## 21. Current Firmware Mapping

| Current Field/Function | Design Meaning | Future Direction |
|---|---|---|
| `_player.level` | Character level | Keep |
| `_player.hp/maxHp` | Health | Keep |
| `_player.atk/def` | Combat stats | Keep |
| `_player.xp/gold` | Progress/economy | Keep |
| `_player.potions` | Healing inventory | Keep |
| `_player.scrap` | Craft material | Keep |
| `_player.keys` | Chest access | Keep |
| `_player.relics` | Rare progression | Expand |
| `_player.weaponTier` | Weapon upgrade | Keep |
| `_sensor.distanceM` | Exploration progress | Keep |
| `_sensor.motionScore` | Route danger / drive bonus | Keep |
| `trigger_exploration_event()` | Event roller | Expand into table |
| `encounter_monster()` | Auto combat | Expand families/traits |
| `find_treasure()` | Loot roll | Expand quality table |
| `render_camp_screen()` | Interactive camp | Add pages later |

## 22. Design Principles

- Never require interaction while driving.
- Make movement feel magical, not distracting.
- Keep every event readable in one glance.
- Let camp be the place for decisions.
- Prefer small reliable numbers over complex hidden systems.
- Add depth through loot tables, monster families, and relics, not through bloated UI.
- Treat GPS, IMU, and LoRa as fantasy inputs: distance, danger, omen, party.

