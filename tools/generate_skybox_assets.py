#!/usr/bin/env python3
"""Normalize generated panoramas into seamless, runtime-ready sky BMPs."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageFilter, ImageOps


WIDTH = 1024
HEIGHT = 512
SEAM_BAND = 48
SEAM_BLUR_RADIUS = 8
POLE_BAND = 24
ASSETS = (
    "dc_night",
    "park_storm",
    "kazakhstan_night",
    "stronghold_dawn",
    "almaty_industrial",
)


def smooth_edge_weight(distance: int, band: int) -> float:
    if band <= 1:
        return 1.0
    position = min(max(distance / (band - 1), 0.0), 1.0)
    smoothstep = position * position * (3.0 - 2.0 * position)
    return 1.0 - smoothstep


def close_horizontal_seam(image: Image.Image) -> Image.Image:
    # Feather a circular blur into the wrap. Mirrored edge averaging creates
    # a visible V-shaped fold even when the first/last texels are identical.
    tiled = Image.new(image.mode, (image.width * 3, image.height))
    for copy in range(3):
        tiled.paste(image, (copy * image.width, 0))
    periodic_blur = tiled.filter(
        ImageFilter.GaussianBlur(radius=SEAM_BLUR_RADIUS)
    ).crop((image.width, 0, image.width * 2, image.height))

    mask = Image.new("L", image.size, 0)
    mask_pixels = mask.load()
    for distance in range(SEAM_BAND):
        left = distance
        right = image.width - 1 - distance
        alpha = round(255 * smooth_edge_weight(distance, SEAM_BAND))
        for y in range(image.height):
            mask_pixels[left, y] = alpha
            mask_pixels[right, y] = alpha
    return Image.composite(periodic_blur, image, mask)


def close_poles(image: Image.Image) -> Image.Image:
    # All longitudes converge at a pole. Smoothly collapse the last rows to
    # one colour so looking up/down cannot expose a radial pinwheel.
    result = image.copy()
    source = image.load()
    target = result.load()
    for distance in range(POLE_BAND):
        weight = smooth_edge_weight(distance, POLE_BAND)
        for y in {distance, image.height - 1 - distance}:
            average = tuple(
                round(
                    sum(source[x, y][channel] for x in range(image.width))
                    / image.width
                )
                for channel in range(3)
            )
            for x in range(image.width):
                target[x, y] = tuple(
                    round(value + (mean - value) * weight)
                    for value, mean in zip(source[x, y], average)
                )
    return result


def build(source_dir: Path, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    previews: list[Image.Image] = []
    for name in ASSETS:
        source_path = source_dir / f"{name}.png"
        source = Image.open(source_path).convert("RGB")
        panorama = ImageOps.fit(
            source,
            (WIDTH, HEIGHT),
            method=Image.Resampling.LANCZOS,
            centering=(0.5, 0.5),
        )
        panorama = close_poles(close_horizontal_seam(panorama))
        # BMP keeps SDL loading dependency-free. Metadata is deliberately
        # stripped and all files share one audited 2:1 format.
        panorama.save(output_dir / f"{name}.bmp", format="BMP")
        previews.append(
            panorama.resize((384, 192), Image.Resampling.LANCZOS)
        )

    contact = Image.new("RGB", (384, 192 * len(previews)), (0, 0, 0))
    for row, preview in enumerate(previews):
        contact.paste(preview, (0, row * 192))
    contact.save(output_dir / "preview.png", optimize=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_dir", type=Path)
    parser.add_argument("output_dir", type=Path)
    arguments = parser.parse_args()
    build(arguments.source_dir, arguments.output_dir)


if __name__ == "__main__":
    main()
