# Firmware Status

Last updated: 2026-05-19

## Summary

The current firmware is past the initial CarQuest prototype and is now roughly at:

```text
Phase 1 complete
Phase 2 mostly complete
Phase 3 initial implementation complete
Phase 4 partially started
Phase 5 not started
```

## Phase 1: Current Prototype Stabilization

Status: complete

- Automatic movement events: implemented.
- Core player stats and resources: implemented.
- LoRa packet prefix renamed to `CQ1`: implemented.
- More monster names and event names: implemented through family tables and event branches.
- Persistent save/load: implemented through NVS.

## Phase 2: Content Expansion

Status: mostly complete

- Monster family table: implemented.
- Treasure quality table: implemented.
- Armor tier: implemented.
- Focus and Luck: implemented.
- Status effects: implemented.
- Route danger bands: implemented as risk bands, but route-type identity is not implemented yet.

Remaining polish:

- More careful balancing of monster power, rewards, and treasure odds.
- Better display of status effects and equipment details.
- Route-type names such as city/highway/mountain/unknown.

## Phase 3: Camp Depth

Status: initial implementation complete

Implemented:

- Inventory detail page.
- Merchant events.
- Relic management.
- Quest log.
- Longer event review.

Camp mode now has multiple pages:

- Rest
- Use potion
- Open chest
- Upgrade gear
- Inventory detail page
- Merchant page with potion/key/repair
- Relic attunement page
- Quest reward page
- Event log page

Remaining polish:

- Better quest variety and named quest chains.
- More interesting merchant inventory rotation.
- Relics are currently represented by three simple attunement choices plus fragment count.
- Event log is still session-local, not a persistent journal.

## Phase 4: Sensor Fantasy

Status: partially started

Implemented:

- IMU motion influences route danger and simulated movement.
- IMU stop detection now learns the current gravity vector, so Camp does not require the device to be flat.
- GPS speed contributes to moving/stopped state.

Missing:

- GPS satellite count affecting navigation uncertainty.
- Speed bands creating route types.
- IMU spikes creating specific hazard flavors.
- LoRa nearby beacons creating party aura or rescue events.

## Phase 5: Presentation

Status: not started

Missing:

- In-game icon set.
- Monster and treasure icons.
- Event splash frames.
- Dedicated CarQuest sound cues.

## Recent Important Behavior

Camp entry is based on clear stopped detection:

- GPS speed at or below `0.8km/h`
- motion score below `1.0`
- stopped for 15 seconds

Motion scoring uses a learned gravity vector, so holding the device at a different angle from boot should not prevent Camp mode.
