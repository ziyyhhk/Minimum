#!/usr/bin/env python3
"""Regenerates src/logo_data.cpp from resources/logo_round.png.

    python3 tools/embed_logo.py            # defaults: resources/logo_round.png -> src/logo_data.cpp
    python3 tools/embed_logo.py --png X --out Y

Standard library only, no Pillow. The PNG signature and the size are read from the IHDR chunk
and written into the header comment, so the comment always matches the bytes. The output is the
exact file that the mod compiles into the pause menu fallback (see src/logo.cpp).
"""

import argparse
import pathlib
import struct
import sys

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
BYTES_PER_LINE = 16


def png_size(data: bytes) -> tuple[int, int]:
    """Width and height from the IHDR chunk. Raises ValueError for anything that is not a PNG."""
    if len(data) < 24 or data[:8] != PNG_SIGNATURE or data[12:16] != b"IHDR":
        raise ValueError("not a PNG file (signature or IHDR chunk missing)")
    width, height = struct.unpack(">II", data[16:24])
    return width, height


def render(data: bytes, source: str) -> str:
    width, height = png_size(data)
    lines = []
    for start in range(0, len(data), BYTES_PER_LINE):
        chunk = data[start:start + BYTES_PER_LINE]
        lines.append("        " + " ".join(f"0x{b:02x}," for b in chunk))
    body = "\n".join(lines)
    return f"""// The round logo ({width} x {height} PNG) built into the mod, so the pause menu button can never end up
// as the pink and black "missing texture" square, even if the packaged sprite is not found.
// Generated from {source} by tools/embed_logo.py, do not edit by hand.

#include <cstddef>

namespace minimum {{

    extern unsigned char const kLogoRoundPng[];
    extern std::size_t const kLogoRoundPngSize;

    unsigned char const kLogoRoundPng[] = {{
{body}
    }};
    std::size_t const kLogoRoundPngSize = sizeof(kLogoRoundPng);

}}
"""


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description="Embed resources/logo_round.png as a C array.")
    parser.add_argument("--png", type=pathlib.Path, default=root / "resources" / "logo_round.png")
    parser.add_argument("--out", type=pathlib.Path, default=root / "src" / "logo_data.cpp")
    args = parser.parse_args()

    data = args.png.read_bytes()
    try:
        width, height = png_size(data)
    except ValueError as error:
        print(f"embed_logo: {args.png}: {error}", file=sys.stderr)
        return 1

    source = args.png.relative_to(root).as_posix() if args.png.is_relative_to(root) else args.png.name
    args.out.write_text(render(data, source), encoding="utf-8", newline="\n")
    print(f"embed_logo: {len(data)} bytes ({width} x {height}) -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
