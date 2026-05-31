# CardPokemon Tools

Tools in this directory prepare SD card resource packs.

Available tools:

- `fetch_pokemon_assets.py`: downloads compact JSON, `64x64` PNGs, and source GIFs from PokeAPI into `cardpokemon/assets/sdcard/`

Planned tools:

- convert GIFs into lightweight animation packs
- emit `index.dat` and per-species data records

The firmware should consume generated files instead of doing expensive image processing on-device.
