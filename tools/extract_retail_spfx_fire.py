#!/usr/bin/env python3
"""Extract retail SPFX fire TIMs as exact RGBA PNG references.

Point this tool at a user-owned COMMON/SPFX.HOG extraction. Generated ROM
pixels belong under ``out`` and are intentionally not bundled by this project.
The PNG encoder uses only the Python standard library for deterministic output.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


@dataclass(frozen=True)
class HogEntry:
    name: str
    offset: int
    size: int


@dataclass(frozen=True)
class TimBlock:
    x: int
    y: int
    width_words: int
    height: int
    words: tuple[int, ...]


@dataclass(frozen=True)
class TimImage:
    name: str
    mode: int
    clut: TimBlock
    pixels: TimBlock

    @property
    def width(self) -> int:
        return self.pixels.width_words * 2

    @property
    def height(self) -> int:
        return self.pixels.height


def le_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def parse_hog(data: bytes) -> tuple[int, dict[str, HogEntry]]:
    if len(data) < 20:
        raise ValueError("HOG header is truncated")
    identifier, count, _, names_offset, data_offset = struct.unpack_from(
        "<IIIII", data, 0
    )
    if not count or 20 + count * 4 > names_offset or not (
        names_offset < data_offset <= len(data)
    ):
        raise ValueError("Invalid HOG header")
    relative = [le_u32(data, 20 + index * 4) for index in range(count)]
    if relative[0] != 0 or relative != sorted(relative):
        raise ValueError("Invalid HOG entry offsets")
    names: list[str] = []
    cursor = names_offset
    for _ in range(count):
        end = data.find(b"\0", cursor, data_offset)
        if end <= cursor:
            raise ValueError("Invalid HOG name table")
        names.append(data[cursor:end].decode("ascii"))
        cursor = end + 1
    entries: dict[str, HogEntry] = {}
    for index, name in enumerate(names):
        begin = data_offset + relative[index]
        end = data_offset + relative[index + 1] if index + 1 < count else len(data)
        if not (data_offset <= begin < end <= len(data)):
            raise ValueError(f"Invalid HOG range for {name}")
        entries[name.upper()] = HogEntry(name, begin, end - begin)
    return identifier, entries


def parse_tim(name: str, data: bytes) -> TimImage:
    if len(data) < 20 or le_u32(data, 0) != 0x10:
        raise ValueError(f"{name}: missing TIM signature")
    flags = le_u32(data, 4)
    mode = flags & 7
    if mode != 1 or not flags & 8:
        raise ValueError(f"{name}: expected indexed8+CLUT TIM, flags={flags:#x}")

    def block(offset: int, label: str) -> tuple[TimBlock, int]:
        size = le_u32(data, offset)
        if size < 12 or offset + size > len(data):
            raise ValueError(f"{name}: invalid {label} block")
        x, y, width_words, height = struct.unpack_from("<HHHH", data, offset + 4)
        word_count = width_words * height
        if 12 + word_count * 2 > size:
            raise ValueError(f"{name}: truncated {label} words")
        words = struct.unpack_from(f"<{word_count}H", data, offset + 12)
        return TimBlock(x, y, width_words, height, tuple(words)), offset + size

    clut, cursor = block(8, "CLUT")
    pixels, _ = block(cursor, "pixel")
    if clut.width_words < 256 or not clut.height:
        raise ValueError(f"{name}: indexed8 TIM has no 256-color CLUT")
    return TimImage(name, mode, clut, pixels)


def psx_rgb5(value: int) -> tuple[int, int, int]:
    def expand(component: int) -> int:
        return (component << 3) | (component >> 2)

    return (
        expand(value & 31),
        expand((value >> 5) & 31),
        expand((value >> 10) & 31),
    )


def rgba_pixels(image: TimImage) -> tuple[bytes, list[int]]:
    indices: list[int] = []
    for word in image.pixels.words:
        indices.extend((word & 0xFF, word >> 8))
    indices = indices[: image.width * image.height]
    rgba = bytearray()
    palette = image.clut.words[:256]
    for index in indices:
        word = palette[index]
        red, green, blue = psx_rgb5(word)
        # PS1: 0x0000 is transparent; bit 15 marks the texel for ABR blending.
        alpha = 0 if word == 0 else (128 if word & 0x8000 else 255)
        rgba.extend((red, green, blue, alpha))
    return bytes(rgba), indices


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def write_rgba_png(path: Path, width: int, height: int, rgba: bytes) -> None:
    if len(rgba) != width * height * 4:
        raise ValueError(f"{path}: invalid RGBA byte count")
    scanlines = b"".join(
        b"\0" + rgba[row * width * 4 : (row + 1) * width * 4]
        for row in range(height)
    )
    output = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(scanlines, 9))
        + png_chunk(b"IEND", b"")
    )
    path.write_bytes(output)


def alpha_bounds(indices: Iterable[int], palette: tuple[int, ...], width: int):
    visible = [
        (position % width, position // width)
        for position, index in enumerate(indices)
        if palette[index] != 0
    ]
    if not visible:
        return None
    xs = [point[0] for point in visible]
    ys = [point[1] for point in visible]
    return [min(xs), min(ys), max(xs), max(ys)]


def contact_sheet(
    frames: list[tuple[TimImage, bytes]], columns: int, padding: int = 4
) -> tuple[int, int, bytes]:
    frame_width = max(image.width for image, _ in frames)
    frame_height = max(image.height for image, _ in frames)
    rows = (len(frames) + columns - 1) // columns
    width = columns * frame_width + (columns - 1) * padding
    height = rows * frame_height + (rows - 1) * padding
    sheet = bytearray(width * height * 4)
    for index, (image, rgba) in enumerate(frames):
        left = (index % columns) * (frame_width + padding)
        top = (index // columns) * (frame_height + padding)
        for row in range(image.height):
            source = row * image.width * 4
            target = ((top + row) * width + left) * 4
            sheet[target : target + image.width * 4] = rgba[
                source : source + image.width * 4
            ]
    return width, height, bytes(sheet)


def checker_preview(width: int, height: int, rgba: bytes, scale: int = 4):
    output_width = width * scale
    output_height = height * scale
    output = bytearray(output_width * output_height * 4)
    for output_y in range(output_height):
        source_y = output_y // scale
        for output_x in range(output_width):
            source_x = output_x // scale
            source = (source_y * width + source_x) * 4
            red, green, blue, alpha = rgba[source : source + 4]
            checker = 38 if ((output_x // 16) + (output_y // 16)) & 1 else 68
            inverse = 255 - alpha
            target = (output_y * output_width + output_x) * 4
            output[target : target + 4] = bytes(
                (
                    (red * alpha + checker * inverse) // 255,
                    (green * alpha + checker * inverse) // 255,
                    (blue * alpha + checker * inverse) // 255,
                    255,
                )
            )
    return output_width, output_height, bytes(output)


def frame_names(prefix: str, count: int, digits: int) -> list[str]:
    return [f"{prefix}{index:0{digits}d}.TIM" for index in range(count)]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="User-owned COMMON/SPFX.HOG")
    parser.add_argument("output", type=Path, help="Output reference directory")
    args = parser.parse_args()

    source = args.source.resolve()
    output = args.output.resolve()
    data = source.read_bytes()
    identifier, entries = parse_hog(data)
    output.mkdir(parents=True, exist_ok=True)

    families = (
        ("EXPL", 12, 3, 32, 32, 4),
        ("FIRE", 16, 4, 32, 64, 4),
    )
    metadata: dict[str, object] = {
        "source": str(source),
        "source_sha256": hashlib.sha256(data).hexdigest(),
        "hog_identifier": f"0x{identifier:08x}",
        "ps1_transparency": {
            "transparent_clut_word": "0x0000",
            "stp_bit": "0x8000",
            "png_preview_alpha": "0 for 0x0000, 128 for STP, 255 otherwise",
        },
        "families": {},
    }
    shared_clut_sha = None

    for prefix, count, digits, expected_width, expected_height, columns in families:
        directory = output / prefix.lower()
        directory.mkdir(exist_ok=True)
        frames: list[tuple[TimImage, bytes]] = []
        frame_metadata: list[dict[str, object]] = []
        for index, name in enumerate(frame_names(prefix, count, digits)):
            entry = entries.get(name)
            if entry is None:
                raise ValueError(f"SPFX.HOG has no {name}")
            image = parse_tim(name, data[entry.offset : entry.offset + entry.size])
            if (image.width, image.height) != (expected_width, expected_height):
                raise ValueError(
                    f"{name}: expected {expected_width}x{expected_height}, "
                    f"got {image.width}x{image.height}"
                )
            rgba, indices = rgba_pixels(image)
            png_name = f"{prefix.lower()}_{index:02d}.png"
            write_rgba_png(directory / png_name, image.width, image.height, rgba)
            frames.append((image, rgba))
            palette_bytes = struct.pack(f"<{len(image.clut.words)}H", *image.clut.words)
            clut_sha = hashlib.sha256(palette_bytes).hexdigest()
            if shared_clut_sha is None:
                shared_clut_sha = clut_sha
            elif clut_sha != shared_clut_sha:
                raise ValueError(f"{name}: SPFX fire families do not share a CLUT")
            bounds = alpha_bounds(indices, image.clut.words, image.width)
            visible_count = sum(image.clut.words[value] != 0 for value in indices)
            stp_count = sum(bool(image.clut.words[value] & 0x8000) for value in indices)
            frame_metadata.append(
                {
                    "index": index,
                    "tim_name": name,
                    "hog_offset": entry.offset,
                    "hog_size": entry.size,
                    "png": str((directory / png_name).relative_to(output)),
                    "display_size": [image.width, image.height],
                    "pixel_block": {
                        "vram_x_words": image.pixels.x,
                        "vram_y": image.pixels.y,
                        "width_words": image.pixels.width_words,
                        "height": image.pixels.height,
                    },
                    "clut_block": {
                        "vram_x_words": image.clut.x,
                        "vram_y": image.clut.y,
                        "width_words": image.clut.width_words,
                        "height": image.clut.height,
                        "sha256_le16": clut_sha,
                    },
                    "visible_bounds_inclusive": bounds,
                    "visible_pixel_count": visible_count,
                    "visible_coverage": round(visible_count / (image.width * image.height), 6),
                    "stp_pixel_count": stp_count,
                    "indexed_pixel_sha256": hashlib.sha256(bytes(indices)).hexdigest(),
                }
            )
        width, height, rgba = contact_sheet(frames, columns)
        raw_sheet = output / f"{prefix.lower()}_sheet_rgba.png"
        write_rgba_png(raw_sheet, width, height, rgba)
        preview_width, preview_height, preview = checker_preview(width, height, rgba)
        preview_sheet = output / f"{prefix.lower()}_sheet_preview.png"
        write_rgba_png(preview_sheet, preview_width, preview_height, preview)
        metadata["families"][prefix] = {
            "frame_count": count,
            "display_size": [expected_width, expected_height],
            "sheet_rgba": raw_sheet.name,
            "sheet_preview": preview_sheet.name,
            "frames": frame_metadata,
        }

    # CFIREA/B/C uses the attached EXPL controller: frames 0..7.
    expl_frames = []
    for index in range(8):
        name = f"EXPL{index:03d}.TIM"
        entry = entries[name]
        image = parse_tim(name, data[entry.offset : entry.offset + entry.size])
        rgba, _ = rgba_pixels(image)
        expl_frames.append((image, rgba))
    width, height, rgba = contact_sheet(expl_frames, 4)
    cfire_sheet = output / "cfire_attached_expl_0_7_rgba.png"
    write_rgba_png(cfire_sheet, width, height, rgba)
    preview_width, preview_height, preview = checker_preview(width, height, rgba)
    cfire_preview = output / "cfire_attached_expl_0_7_preview.png"
    write_rgba_png(cfire_preview, preview_width, preview_height, preview)
    metadata["cfire_attached_runtime"] = {
        "sprite_family": "EXPL",
        "frame_range_inclusive": [0, 7],
        "texture_size": [32, 32],
        "sprite_pivot": "center (mapping_x=16, mapping_y=16 in regular GsSortSprite)",
        "authored_spawn": "(transform.x, -transform.y - 0x70, transform.z)",
        "native_scale_byte": 57,
        "world_size": [456, 456],
        "world_size_formula": "scale_byte * 8 world units",
        "orientation": "camera-facing square billboard",
        "blend": "semi-transparent textured primitive, ABR=1 additive",
        "uv": "full inclusive 32x32 frame; resident atlas uses u0/v0..u0+31/v0+31",
        "sheet_rgba": cfire_sheet.name,
        "sheet_preview": cfire_preview.name,
    }
    metadata["shared_clut_sha256_le16"] = shared_clut_sha
    (output / "metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(f"Extracted retail SPFX reference to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
