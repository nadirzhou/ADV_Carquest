# CarQuest Monster Asset Pipeline

CarQuest monster portraits use a compact firmware format intended to scale from the current 16 monsters to larger bestiaries.

The current preferred art direction is monochrome bestiary line art: white monster linework on a black background. This matches Potrace well and produces small firmware assets.

## Chosen Firmware Format

- Sprite size: `64x64`
- Color format: 1-bit monochrome line art
- Packed format: row-major, most-significant bit first
- Bit meaning: `0` background, `1` line
- Firmware header: `main/apps/app_signal_quest/assets/monster_line_sprites.h`

This keeps each monster at exactly 512 bytes. The current 16 monsters use 8192 bytes of packed sprite data. A future 200-monster bestiary would use about 100 KB before optional metadata.

## Source And Generated Files

- Source contact sheet: `main/apps/app_signal_quest/assets/monster_line_src/carquest_monster_line_contact_sheet.png`
- Firmware PNG previews: `main/apps/app_signal_quest/assets/monster_line_png/*.png`
- Potrace input PNGs: `main/apps/app_signal_quest/assets/monster_line_trace/*.png`
- Potrace SVGs: `main/apps/app_signal_quest/assets/monster_line_svg/*.svg`
- Preview sheet: `main/apps/app_signal_quest/assets/monster_line_src/carquest_monster_line_64_preview.png`
- Vector comparison preview: `main/apps/app_signal_quest/assets/monster_line_src/line_vector_compare_preview.png`
- Conversion tools: `tools/monster_assets/`

## Regeneration

From the repository root:

```powershell
python tools\monster_assets\generate_monster_line_assets.py
Push-Location tools\monster_assets
npm install
$files = (Get-ChildItem ..\..\main\apps\app_signal_quest\assets\monster_line_trace\*.png | ForEach-Object { $_.FullName }) -join ';'
node trace_line_sprites.js --files $files --outDir ..\..\main\apps\app_signal_quest\assets\monster_line_svg
Pop-Location
```

## Vectorizer Notes

Potrace is the preferred vectorizer for this version because the source art is black-background white linework. The SVGs use a single filled `currentColor` path with `fill-rule="evenodd"` and `clip-rule="evenodd"`, preserving line interiors as traced filled outlines.

Earlier color/vector experiments are intentionally separate from the `monster_line_*` assets. Keep `monster_line_sprites.h` as the source of truth for firmware rendering.

## Rendering In Firmware

`monster_line_sprites.h` provides:

- `MonsterLineSpriteAsset`
- `kMonsterLineSprites`
- `decode_monster_line_sprite_rgb565(...)`

Decode into a temporary `uint16_t buffer[64 * 64]`, then draw with M5GFX `pushImage`.
