#!/usr/bin/env python3
"""Render font glyphs to extract pixel patterns for block graphics."""

from PIL import Image, ImageDraw, ImageFont
import sys

FONT_PATH = "/nix/store/4fgk3gfhw5hzsc36d8h5wikyqkbcciis-noto-fonts-2026.01.01/share/fonts/noto/NotoSansSymbols2-Regular.otf"

# Render at size that gives us approximately 8x12 pixels per character
RENDER_SIZE = 48
TARGET_W = 8
TARGET_H = 12

font = ImageFont.truetype(FONT_PATH, RENDER_SIZE)


def render_char(char):
    """Render a character and return its bitmap sampled to 8x12"""
    try:
        bbox = font.getbbox(char)
        if bbox is None or bbox[2] <= bbox[0] or bbox[3] <= bbox[1]:
            return None

        # Render to image
        w = bbox[2] - bbox[0]
        h = bbox[3] - bbox[1]
        img = Image.new("L", (w, h), 0)
        draw = ImageDraw.Draw(img)
        draw.text((-bbox[0], -bbox[1]), char, font=font, fill=255)

        # Resize to 8x12 using high quality resampling
        img_resized = img.resize((TARGET_W, TARGET_H), Image.Resampling.LANCZOS)

        # Extract pattern - threshold at 128
        pattern = []
        for y in range(TARGET_H):
            for x in range(TARGET_W):
                p = img_resized.getpixel((x, y))
                pattern.append(1 if p > 128 else 0)

        return pattern
    except Exception as e:
        print(f"Error rendering U+{ord(char):04X}: {e}", file=sys.stderr)
        return None


def show_pattern(pattern, char):
    """Display pattern as ASCII art"""
    print(f"// {char} (U+{ord(char):04X})")
    for y in range(TARGET_H):
        line = "// "
        for x in range(TARGET_W):
            idx = y * TARGET_W + x
            line += "█" if pattern[idx] else "·"
        print(line)
    print()


def main():
    # Characters to render
    chars_to_render = []

    # Sextants U+1FB00 - U+1FB3B (60 chars)
    for cp in range(0x1FB00, 0x1FB3C):
        chars_to_render.append(chr(cp))

    # Smooth diagonals U+1FB3C - U+1FB6F (52 chars)
    for cp in range(0x1FB3C, 0x1FB70):
        chars_to_render.append(chr(cp))

    # Note: Standard block elements (▀▄▌▐█ etc.) are NOT in Noto Sans Symbols 2
    # We'll use geometric patterns for those in C++

    print("// Auto-generated character patterns from font rendering")
    print("// Font: Noto Sans Symbols 2")
    print(f"// Grid: {TARGET_W}x{TARGET_H} subpixels")
    print()
    print("namespace tempura::block_gfx {")
    print()
    print("struct RenderedChar {")
    print("  const char* utf8;")
    print("  uint64_t pattern_lo;  // bits 0-63")
    print("  uint32_t pattern_hi;  // bits 64-95")
    print("};")
    print()
    print("inline constexpr RenderedChar kRenderedChars[] = {")

    for char in chars_to_render:
        pattern = render_char(char)
        if pattern:
            # Pack into lo (64 bits) and hi (32 bits)
            lo = 0
            hi = 0
            for i, bit in enumerate(pattern):
                if bit:
                    if i < 64:
                        lo |= 1 << i
                    else:
                        hi |= 1 << (i - 64)

            # Escape the char for C++
            utf8_bytes = char.encode("utf-8")
            utf8_escaped = "".join(f"\\x{b:02x}" for b in utf8_bytes)

            lo_hex = f"0x{lo:016x}ULL"
            hi_hex = f"0x{hi:08x}U"
            print(f'  {{"{utf8_escaped}", {lo_hex}, {hi_hex}}},')

    print("};")
    print()
    print("}  // namespace tempura::block_gfx")
    print()

    # Also show a few examples visually to stderr
    print("// Visual examples:", file=sys.stderr)
    examples = [
        "\U0001FB00",  # Sextant-1
        "\U0001FB02",  # Sextant-12
        "\U0001FB0B",  # Sextant-34
        "\U0001FB3C",  # First diagonal
        "\U0001FB40",  # Diagonal
        "\U0001FB51",  # Full diagonal
        "\U0001FB46",  # Another diagonal
        "▀",
        "▄",
        "▌",
        "▐",
    ]
    for char in examples:
        pattern = render_char(char)
        if pattern:
            show_pattern_stderr(pattern, char)


def show_pattern_stderr(pattern, char):
    """Display pattern as ASCII art to stderr"""
    print(f"// {char} (U+{ord(char):04X})", file=sys.stderr)
    for y in range(TARGET_H):
        line = "// "
        for x in range(TARGET_W):
            idx = y * TARGET_W + x
            line += "█" if pattern[idx] else "·"
        print(line, file=sys.stderr)
    print(file=sys.stderr)


if __name__ == "__main__":
    main()
