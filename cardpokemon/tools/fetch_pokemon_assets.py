#!/usr/bin/env python3
"""Build a CardPokemon SD-card resource pack from PokeAPI sprites.

Output layout:

  assets/sdcard/pokemon/1.json
  assets/sdcard/pokemon/1.png   # 64x64 transparent PNG for firmware
  assets/sdcard/pokemon/1.gif   # animated sprite when available
"""

from __future__ import annotations

import argparse
import io
import json
import os
import shutil
import subprocess
from pathlib import Path

import requests
from PIL import Image
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry


DEFAULT_IDS = [1, 4, 7, 2, 5, 8, 16, 19, 25, 129]
FIRST_200_IDS = list(range(1, 201))
GAME_DEFAULTS = {
    1: {"catch_rate": 42, "evolve_level": 5, "evolve_to": 2},
    4: {"catch_rate": 38, "evolve_level": 5, "evolve_to": 5},
    7: {"catch_rate": 40, "evolve_level": 5, "evolve_to": 8},
    2: {"catch_rate": 25, "evolve_level": -1, "evolve_to": -1},
    5: {"catch_rate": 23, "evolve_level": -1, "evolve_to": -1},
    8: {"catch_rate": 25, "evolve_level": -1, "evolve_to": -1},
    16: {"catch_rate": 70, "evolve_level": -1, "evolve_to": -1},
    19: {"catch_rate": 72, "evolve_level": -1, "evolve_to": -1},
    25: {"catch_rate": 20, "evolve_level": -1, "evolve_to": -1},
    129: {"catch_rate": 80, "evolve_level": -1, "evolve_to": -1},
}
EVOLUTION_CACHE: dict[str, dict] = {}


def make_session() -> requests.Session:
    session = requests.Session()
    retries = Retry(total=5, backoff_factor=1, status_forcelist=[429, 500, 502, 503, 504])
    adapter = HTTPAdapter(max_retries=retries)
    session.mount("https://", adapter)
    session.mount("http://", adapter)
    session.headers.update({"User-Agent": "cardpokemon-asset-fetcher/0.1"})
    return session


def get_json(session: requests.Session, url: str) -> dict:
    response = session.get(url, timeout=30)
    response.raise_for_status()
    return response.json()


def pokemon_id_from_url(url: str | None) -> int:
    if not url:
        return -1
    parts = [part for part in url.rstrip("/").split("/") if part]
    try:
        return int(parts[-1])
    except (ValueError, IndexError):
        return -1


def evolution_target_from_chain(chain_node: dict, species_id: int) -> tuple[int, int]:
    node_species = chain_node.get("species", {})
    node_id = pokemon_id_from_url(node_species.get("url"))
    if node_id == species_id:
        evolves_to = chain_node.get("evolves_to", [])
        if not evolves_to:
            return -1, -1
        target = evolves_to[0]
        target_id = pokemon_id_from_url(target.get("species", {}).get("url"))
        min_level = -1
        details = target.get("evolution_details", [])
        if details:
            value = details[0].get("min_level")
            if isinstance(value, int):
                min_level = value
        return min_level, target_id

    for child in chain_node.get("evolves_to", []):
        found_level, found_id = evolution_target_from_chain(child, species_id)
        if found_id > 0:
            return found_level, found_id
    return -1, -1


def game_metadata(session: requests.Session, pokemon_id: int, species: dict) -> dict:
    fallback = GAME_DEFAULTS.get(pokemon_id, {"catch_rate": 18, "evolve_level": -1, "evolve_to": -1})
    capture_rate = species.get("capture_rate")
    catch_rate = max(2, min(95, int(capture_rate))) if isinstance(capture_rate, int) else fallback["catch_rate"]

    chain_url = species.get("evolution_chain", {}).get("url")
    evolve_level = fallback["evolve_level"]
    evolve_to = fallback["evolve_to"]
    if chain_url:
        chain = EVOLUTION_CACHE.get(chain_url)
        if chain is None:
            chain = get_json(session, chain_url)
            EVOLUTION_CACHE[chain_url] = chain
        found_level, found_id = evolution_target_from_chain(chain.get("chain", {}), pokemon_id)
        if found_id > 0:
            evolve_level = found_level if found_level > 0 else fallback["evolve_level"]
            evolve_to = found_id

    return {"catch_rate": catch_rate, "evolve_level": evolve_level, "evolve_to": evolve_to}


def best_sprite_urls(pokemon: dict) -> tuple[str | None, str | None]:
    sprites = pokemon.get("sprites", {})
    png = (
        sprites.get("other", {}).get("official-artwork", {}).get("front_default")
        or sprites.get("other", {}).get("home", {}).get("front_default")
        or sprites.get("front_default")
    )
    try:
        gif = sprites["versions"]["generation-v"]["black-white"]["animated"]["front_default"]
    except Exception:
        gif = None
    return png, gif


def localized_names(pokemon: dict, species: dict) -> dict:
    names = {"en": pokemon.get("name", "")}
    for item in species.get("names", []):
        lang = item.get("language", {}).get("name")
        if lang in ("zh-Hant", "zh-Hans"):
            names["zh"] = item.get("name", "")
        elif lang in ("ja", "ko"):
            names[lang] = item.get("name", "")
    return names


def save_64_png(session: requests.Session, url: str, path: Path) -> None:
    response = session.get(url, timeout=30)
    response.raise_for_status()
    source = Image.open(io.BytesIO(response.content)).convert("RGBA")
    source.thumbnail((64, 64), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    x = (64 - source.width) // 2
    y = (64 - source.height) // 2
    canvas.alpha_composite(source, (x, y))
    canvas.save(path, "PNG", optimize=True)


def save_gif(session: requests.Session, url: str, path: Path) -> None:
    response = session.get(url, timeout=30)
    response.raise_for_status()
    path.write_bytes(response.content)

    gifsicle = shutil.which("gifsicle")
    if not gifsicle:
        return
    if path.stat().st_size <= 110 * 1024:
        return
    subprocess.run([gifsicle, "--resize-fit", "64x64", "-O2", "-o", str(path), str(path)], check=False)


def fetch_one(session: requests.Session, pokemon_id: int, out_dir: Path) -> dict:
    pokemon = get_json(session, f"https://pokeapi.co/api/v2/pokemon/{pokemon_id}")
    species = get_json(session, pokemon.get("species", {}).get("url"))
    names = localized_names(pokemon, species)
    png_url, gif_url = best_sprite_urls(pokemon)

    entry = {
        "id": pokemon.get("id"),
        "name": names.get("zh") or names.get("en"),
        "names": names,
        "types": [item["type"]["name"] for item in pokemon.get("types", [])],
        "stats": {item["stat"]["name"]: item["base_stat"] for item in pokemon.get("stats", [])},
        "height": pokemon.get("height"),
        "weight": pokemon.get("weight"),
        "png": f"{pokemon_id}.png" if png_url else None,
        "gif": f"{pokemon_id}.gif" if gif_url else None,
        "game": game_metadata(session, pokemon_id, species),
    }

    (out_dir / f"{pokemon_id}.json").write_text(json.dumps(entry, ensure_ascii=False, indent=2), encoding="utf-8")
    if png_url:
        save_64_png(session, png_url, out_dir / f"{pokemon_id}.png")
    if gif_url:
        save_gif(session, gif_url, out_dir / f"{pokemon_id}.gif")
    return entry


def parse_ids(value: str) -> list[int]:
    if value == "mvp":
        return DEFAULT_IDS
    if value in ("200", "first-200"):
        return FIRST_200_IDS
    if value == "151":
        return list(range(1, 152))
    ids: list[int] = []
    for part in value.split(","):
        part = part.strip()
        if "-" in part:
            start, end = [int(x) for x in part.split("-", 1)]
            ids.extend(range(start, end + 1))
        elif part:
            ids.append(int(part))
    return ids


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ids", default="mvp", help="mvp, comma list, or range such as 1-151")
    parser.add_argument("--out", default="../assets/sdcard/pokemon", help="Output pokemon folder")
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    out_dir = (script_dir / args.out).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    session = make_session()
    manifest = []
    for pokemon_id in parse_ids(args.ids):
        print(f"Fetching {pokemon_id}...")
        manifest.append(fetch_one(session, pokemon_id, out_dir))

    manifest_path = out_dir.parent / "manifest.json"
    manifest_path.write_text(json.dumps({"pokemon": manifest}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Wrote {manifest_path}")
    print(f"Copy the contents of {out_dir.parent} to the SD card root.")


if __name__ == "__main__":
    main()
