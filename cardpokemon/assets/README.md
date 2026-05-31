# CardPokemon Assets

Runtime Pokemon images and animations should not be embedded in the firmware by default.

Use the SD card resource pack layout described in `../docs/RESOURCE_PACK.md`.

Firmware-embedded assets should be limited to:

- boot logo or loading screen
- small UI icons
- fallback missing-resource image
- compact font or palette tables if needed
