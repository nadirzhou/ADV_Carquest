from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT / "main" / "apps" / "app_signal_quest" / "assets"
SHEET = ASSET_ROOT / "monster_line_src" / "carquest_monster_line_contact_sheet.png"
PNG_DIR = ASSET_ROOT / "monster_line_png"
TRACE_DIR = ASSET_ROOT / "monster_line_trace"
HEADER = ASSET_ROOT / "monster_line_sprites.h"
PREVIEW = ASSET_ROOT / "monster_line_src" / "carquest_monster_line_64_preview.png"
SPRITE_SIZE = 64


@dataclass(frozen=True)
class Monster:
    slug: str
    label: str


MONSTERS = [
    Monster("mile_wisp", "Mile Wisp"),
    Monster("road_wraith", "Road Wraith"),
    Monster("fog_rider", "Fog Rider"),
    Monster("route_king", "Route King"),
    Monster("signal_imp", "Signal Imp"),
    Monster("packet_biter", "Packet Biter"),
    Monster("static_hexer", "Static Hexer"),
    Monster("lora_hexer", "LoRa Hexer"),
    Monster("tar_slime", "Tar Slime"),
    Monster("rumble_hound", "Rumble Hound"),
    Monster("guard_drake", "Guard Drake"),
    Monster("overpass_hydra", "Overpass Hydra"),
    Monster("rust_acolyte", "Rust Acolyte"),
    Monster("wrench_raider", "Wrench Raider"),
    Monster("scrap_knight", "Scrap Knight"),
    Monster("chrome_prophet", "Chrome Prophet"),
]


def main() -> None:
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    TRACE_DIR.mkdir(parents=True, exist_ok=True)
    sheet = Image.open(SHEET).convert("RGB")
    sprites: list[tuple[Monster, list[int], list[int]]] = []

    for idx, monster in enumerate(MONSTERS):
        cell = crop_cell(sheet, idx)
        sprite = normalize_line_sprite(cell)
        bits = list(sprite.getdata())
        packed = pack_bits(bits)
        sprites.append((monster, bits, packed))
        write_display_png(sprite, PNG_DIR / f"{monster.slug}.png")
        write_trace_png(sprite, TRACE_DIR / f"{monster.slug}.png")

    write_preview()
    write_header(sprites)
    print(f"Wrote {len(sprites)} monochrome sprites to {PNG_DIR}")
    print(f"Wrote Potrace inputs to {TRACE_DIR}")
    print(f"Wrote firmware header to {HEADER}")


def crop_cell(sheet: Image.Image, idx: int) -> Image.Image:
    col = idx % 4
    row = idx // 4
    cell_w = sheet.width / 4
    cell_h = sheet.height / 4
    return sheet.crop((
        round(col * cell_w),
        round(row * cell_h),
        round((col + 1) * cell_w),
        round((row + 1) * cell_h),
    ))


def normalize_line_sprite(cell: Image.Image) -> Image.Image:
    gray = cell.convert("L")
    bbox = threshold_bbox(gray, 54)
    if bbox is None:
        return Image.new("1", (SPRITE_SIZE, SPRITE_SIZE), 0)

    cropped = gray.crop(expand_bbox(bbox, gray.size, 18))
    scale = min((SPRITE_SIZE - 6) / cropped.width, (SPRITE_SIZE - 6) / cropped.height)
    resized = cropped.resize((max(1, round(cropped.width * scale)), max(1, round(cropped.height * scale))),
                             Image.Resampling.LANCZOS)
    canvas = Image.new("L", (SPRITE_SIZE, SPRITE_SIZE), 0)
    canvas.paste(resized, ((SPRITE_SIZE - resized.width) // 2, (SPRITE_SIZE - resized.height) // 2))

    return canvas.point(lambda p: 1 if p >= 70 else 0, mode="1")


def threshold_bbox(gray: Image.Image, threshold: int) -> tuple[int, int, int, int] | None:
    mask = gray.point(lambda p: 255 if p >= threshold else 0, mode="L")
    return mask.getbbox()


def expand_bbox(bbox: tuple[int, int, int, int], size: tuple[int, int], pad: int) -> tuple[int, int, int, int]:
    return (
        max(0, bbox[0] - pad),
        max(0, bbox[1] - pad),
        min(size[0], bbox[2] + pad),
        min(size[1], bbox[3] + pad),
    )


def write_display_png(sprite: Image.Image, path: Path) -> None:
    rgba = Image.new("RGBA", sprite.size, (0, 0, 0, 255))
    white = Image.new("RGBA", sprite.size, (255, 255, 255, 255))
    rgba.paste(white, mask=sprite.convert("L").point(lambda p: 255 if p else 0))
    rgba.save(path)


def write_trace_png(sprite: Image.Image, path: Path) -> None:
    # Potrace traces dark foreground, so invert to black linework on white.
    trace = Image.new("L", sprite.size, 255)
    draw = ImageDraw.Draw(trace)
    pix = sprite.load()
    for y in range(sprite.height):
        for x in range(sprite.width):
            if pix[x, y]:
                draw.point((x, y), fill=0)
    trace.save(path)


def pack_bits(bits: list[int]) -> list[int]:
    packed: list[int] = []
    for i in range(0, len(bits), 8):
        value = 0
        for bit in range(8):
            if i + bit < len(bits) and bits[i + bit]:
                value |= 1 << (7 - bit)
        packed.append(value)
    return packed


def write_preview() -> None:
    cell = 88
    label_h = 12
    sheet = Image.new("RGBA", (4 * cell, 4 * (cell + label_h)), (0, 0, 0, 255))
    draw = ImageDraw.Draw(sheet)
    for idx, monster in enumerate(MONSTERS):
        im = Image.open(PNG_DIR / f"{monster.slug}.png").convert("RGBA").resize((64, 64), Image.Resampling.NEAREST)
        x = (idx % 4) * cell + 12
        y = (idx // 4) * (cell + label_h) + 4
        sheet.alpha_composite(im, (x, y))
        draw.text(((idx % 4) * cell + 2, y + 66), monster.slug[:13], fill=(220, 220, 220, 255))
    sheet.save(PREVIEW)


def write_header(sprites: list[tuple[Monster, list[int], list[int]]]) -> None:
    lines = [
        "/*",
        " * Generated by tools/monster_assets/generate_monster_line_assets.py.",
        " * 64x64 monochrome line sprites packed as 1 bit per pixel.",
        " * Bits are row-major, most-significant bit first. Bit 0 is background, bit 1 is line.",
        " */",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "struct MonsterLineSpriteAsset {",
        "    const char* name;",
        "    uint8_t width;",
        "    uint8_t height;",
        "    const uint8_t* data;",
        "    uint16_t dataLength;",
        "};",
        "",
        f"static constexpr uint8_t kMonsterLineSpriteWidth = {SPRITE_SIZE};",
        f"static constexpr uint8_t kMonsterLineSpriteHeight = {SPRITE_SIZE};",
        "",
        "static inline void decode_monster_line_sprite_rgb565(const MonsterLineSpriteAsset& sprite, uint16_t* out,",
        "                                                      uint16_t backgroundColor, uint16_t lineColor)",
        "{",
        "    const uint16_t total = static_cast<uint16_t>(sprite.width) * static_cast<uint16_t>(sprite.height);",
        "    for (uint16_t pos = 0; pos < total; ++pos) {",
        "        const uint16_t byteIndex = pos >> 3;",
        "        const uint8_t mask = static_cast<uint8_t>(0x80 >> (pos & 0x07));",
        "        const bool line = byteIndex < sprite.dataLength && (sprite.data[byteIndex] & mask);",
        "        out[pos] = line ? lineColor : backgroundColor;",
        "    }",
        "}",
        "",
    ]

    for monster, _, packed in sprites:
        lines.append(f"static const uint8_t kMonsterLineBits_{monster.slug}[{len(packed)}] = {{")
        lines.append(format_values(packed))
        lines.append("};")
        lines.append("")

    lines.append(f"static const MonsterLineSpriteAsset kMonsterLineSprites[{len(sprites)}] = {{")
    for monster, _, packed in sprites:
        lines.append(
            f'    {{"{monster.label}", kMonsterLineSpriteWidth, kMonsterLineSpriteHeight, '
            f"kMonsterLineBits_{monster.slug}, {len(packed)}}},"
        )
    lines.append("};")
    lines.append("")
    HEADER.write_text("\n".join(lines), encoding="utf-8")


def format_values(values: list[int]) -> str:
    return "\n".join(
        "    " + ", ".join(str(v) for v in values[i:i + 20]) + ","
        for i in range(0, len(values), 20)
    )


if __name__ == "__main__":
    main()
