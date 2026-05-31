# CardPokemon SD Resource Pack

CardPokemon loads Pokemon resources from SD card.

## SD Layout

```text
/pokemon/
  1.json
  1.png
  1.gif
  4.json
  4.png
  4.gif
manifest.json
```

The firmware loads species catalog data from `/sdcard/manifest.json`, then reads `/sdcard/pokemon/{pokedex_id}.png` as a `64x64` transparent PNG. It currently supports a 1-200 Pokemon SD pack and falls back to a built-in 10-Pokemon catalog plus placeholder drawings when the SD card or files are missing. GIF files are imported into the pack now; runtime GIF playback is the next integration step.

Each manifest entry should include:

- `id`
- `name`
- `types`
- `stats.hp`, `stats.attack`, `stats.defense`
- `png`
- `gif`
- `game.catch_rate`
- `game.evolve_level`
- `game.evolve_to`

## Import Tool

From the repository root:

```powershell
python cardpokemon/tools/fetch_pokemon_assets.py --ids 1-10
```

For the current full resource pack:

```powershell
python cardpokemon/tools/fetch_pokemon_assets.py --ids 1-200
```

For a smaller Kanto-only pack, use `--ids 1-151`. The tool follows the same PokeAPI resource model used by `hunglin59638/pokedex`: compact JSON metadata, official artwork PNG, generation-v animated GIF when available, capture rate, and the immediate evolution target from the PokeAPI evolution chain. It preprocesses the firmware PNG to exactly `64x64` for stable small-screen rendering.

## Image Strategy

Original PNG/GIF assets should be preprocessed on a PC.

Recommended runtime sizes:

- icon: `32x32`
- travel/party sprite: `64x64`
- battle sprite: `80x80` or `96x96`
- Pokedex feature art: up to `112x96`

Recommended animation limits:

- 4 to 8 fps
- 4 to 12 frames per Pokemon
- 16 or 32 color indexed palette
- frame delta or RLE compression

## Animation Format Direction

Long term, use a custom lightweight format rather than decoding arbitrary GIFs on-device.

```text
PKA1
width
height
frame_count
fps
palette
frame table
RLE frame data
```

The visual source can still be GIF. The SD card resource pack should store the converted runtime format.
