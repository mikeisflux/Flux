#!/usr/bin/env python3
"""Render the Flux mark into the icon files Chromium's theme expects.

The output is committed, so the user's build machine needs nothing installed;
this only runs when the mark changes. build/sync copies the results over
chrome/app/theme/chromium/ after the patch series applies, which keeps binary
files out of the patch series entirely.
"""
import pathlib
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:
    sys.exit('Pillow is required: pip install Pillow')

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / 'branding' / 'icons'

LIME = (180, 240, 60, 255)
INK = (20, 32, 10, 255)

# The mark, on the 64-unit grid the SVG uses. Kept in step with
# branding/flux-mark.svg by hand - there are two shapes and they are both here.
CORNER_RADIUS = 15 / 64
BOLT = [(37, 8), (17, 36), (28, 36), (25, 56), (47, 27), (35, 27)]

# Supersample, then downscale. A 16px icon drawn directly has stair-stepped
# diagonals, and the bolt is nothing but diagonals.
SS = 8


def render(size, padding=0.0):
    """`padding` insets the tile, for the Windows tiles that sit on a
    transparent field rather than filling their canvas."""
    big = size * SS
    img = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    inset = round(big * padding)
    box = (inset, inset, big - inset - 1, big - inset - 1)
    tile = big - 2 * inset
    draw.rounded_rectangle(box, radius=round(tile * CORNER_RADIUS), fill=LIME)

    scale = tile / 64
    draw.polygon([(inset + x * scale, inset + y * scale) for x, y in BOLT],
                 fill=INK)

    return img.resize((size, size), Image.LANCZOS)


def main():
    (OUT / 'win' / 'tiles').mkdir(parents=True, exist_ok=True)

    for size in (16, 24, 48, 64, 128, 256):
        render(size).save(OUT / f'product_logo_{size}.png')

    # Windows wants every size in one file; Explorer, the taskbar, alt-tab and
    # the shortcut all pick different ones.
    # Named for Chromium's file, not for us: the .rc and the installer both
    # reference chromium.ico by name, and copying over it is one fewer patch.
    render(256).save(OUT / 'win' / 'chromium.ico',
                     sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])

    # Start menu tiles: the mark floats on the tile's own colour, so it is
    # inset rather than bled to the edge.
    render(600, padding=0.22).save(OUT / 'win' / 'tiles' / 'Logo.png')
    render(176, padding=0.22).save(OUT / 'win' / 'tiles' / 'SmallLogo.png')

    for path in sorted(OUT.rglob('*')):
        if path.is_file():
            print(f'  {path.relative_to(ROOT)}  {path.stat().st_size} bytes')


if __name__ == '__main__':
    main()
