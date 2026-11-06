#!/usr/bin/env python3
"""Generate Sovel's built-in Normal Mode city wallpaper and Private emblem wallpaper.

    python3 tools/make-wallpapers.py [--emblem path/to/emblem.png] [--lab path/to/lab.png]

The city is drawn procedurally (deterministic seed), so it is an original
project asset. The emblem and lab wallpapers are composed from images the
owner supplied; pass them explicitly to rebuild those two files.
"""
import argparse
import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

W, H = 2560, 1440
ROOT = Path(__file__).resolve().parent.parent / 'assets'


def gradient(stops, height):
    """Vertical gradient from [(position, (r, g, b)), ...]."""
    ys = np.linspace(0, 1, height)
    channels = []
    for c in range(3):
        channels.append(np.interp(ys, [p for p, _ in stops], [col[c] for _, col in stops]))
    return np.stack(channels, axis=1)


def city(seed=20261106):
    rng = random.Random(seed)
    horizon = int(H * 0.76)
    sky = gradient([(0.0, (6, 10, 26)), (0.40, (14, 24, 56)), (0.66, (40, 52, 96)),
                    (0.84, (104, 88, 132)), (0.94, (196, 138, 128)), (1.0, (238, 176, 134))], horizon)
    canvas = np.zeros((H, W, 3), dtype=np.float32)
    canvas[:horizon] = sky[:, None, :]
    for _ in range(220):
        x, y = rng.randrange(W), rng.randrange(int(horizon * 0.5))
        canvas[y, x] = np.minimum(255, canvas[y, x] + rng.uniform(30, 110) * (1 - y / (horizon * 0.5)))
    image = Image.fromarray(canvas.clip(0, 255).astype(np.uint8))
    windows = Image.new('RGB', (W, H), (0, 0, 0))
    draw, light = ImageDraw.Draw(image), ImageDraw.Draw(windows)

    def tower(x, width, top, color, brightness, warmth):
        draw.rectangle([x, top, x + width, horizon], fill=color)
        # A lit edge on the side facing the dusk glow gives each tower volume.
        edge = tuple(min(255, int(c * 1.55 + 10)) for c in color)
        draw.line([x, top, x, horizon], fill=edge, width=2)
        if rng.random() < 0.3:
            crown = rng.randint(10, max(11, width // 3))
            draw.rectangle([x + width // 2 - crown // 2, top - rng.randint(18, 70), x + width // 2 + crown // 2, top], fill=color)
        if rng.random() < 0.14:
            spire = top - rng.randint(70, 180)
            draw.line([x + width // 2, top, x + width // 2, spire], fill=color, width=3)
            light.ellipse([x + width // 2 - 3, spire - 3, x + width // 2 + 3, spire + 3], fill=(255, 64, 64))
        lit = rng.uniform(0.04, 0.42)
        column, row = rng.choice([(9, 13), (11, 15), (14, 19)])
        floors = list(range(top + 12, horizon - 8, row))
        for wy in floors:
            floor_lit = lit * (2.2 if rng.random() < 0.12 else 1.0)
            for wx in range(x + 7, x + width - 7, column):
                if rng.random() < floor_lit:
                    tone = (255, 212, 156) if rng.random() < warmth else (178, 216, 255)
                    fade = rng.uniform(0.45, 1.0) * brightness
                    light.rectangle([wx, wy, wx + 3, wy + 6], fill=tuple(int(c * fade) for c in tone))

    def skyline(low, high, color, brightness, warmth, widths, gap):
        x = -60
        while x < W + 60:
            width = rng.randint(*widths)
            tower(x, width, horizon - rng.randint(low, high), color, brightness, warmth)
            x += width + rng.randint(*gap)

    skyline(150, 380, (58, 58, 96), 0.30, 0.45, (60, 150), (-10, 10))
    skyline(120, 470, (26, 31, 56), 0.65, 0.55, (70, 170), (0, 18))
    skyline(60, 300, (10, 12, 22), 1.00, 0.68, (90, 230), (0, 26))
    lights = np.asarray(windows, dtype=np.float32)
    glow = np.asarray(windows.filter(ImageFilter.GaussianBlur(10)), dtype=np.float32)
    base = np.asarray(image, dtype=np.float32)
    # Atmospheric haze: distant structure dissolves into the dusk band.
    rows = np.arange(H, dtype=np.float32)
    haze = np.where(rows < horizon, np.exp(-(horizon - rows) / 150.0), 0)[:, None, None]
    base = base * (1 - 0.55 * haze) + np.array([168, 122, 134], dtype=np.float32) * 0.55 * haze
    final = np.minimum(255, base + lights + 0.85 * glow)
    # Water: a calm, darkened, softly rippled reflection.
    depth = H - horizon
    mirrored = final[horizon - depth:horizon][::-1]
    water = np.asarray(Image.fromarray(mirrored.astype(np.uint8)).filter(ImageFilter.GaussianBlur(4)), dtype=np.float32)
    shift = (np.sin(np.arange(depth) / 3.1) * 3 + np.sin(np.arange(depth) / 11.0) * 5).astype(int)
    for y in range(depth):
        water[y] = np.roll(water[y], shift[y], axis=0)
    falloff = np.linspace(0.55, 0.16, depth)[:, None, None]
    water = water * falloff + np.array([8, 13, 30], dtype=np.float32) * (1 - falloff)
    final[horizon:] = water
    final[horizon - 1:horizon + 1] = final[horizon - 1:horizon + 1] * 0.5 + np.array([246, 186, 148]) * 0.5
    yy, xx = np.mgrid[0:H, 0:W]
    vignette = 1 - 0.34 * (((xx - W / 2) / (W / 2)) ** 2 * 0.55 + ((yy - H * 0.45) / (H / 2)) ** 2 * 0.45)
    final *= vignette.clip(0.58, 1)[..., None]
    return Image.fromarray(final.clip(0, 255).astype(np.uint8))


def emblem(source):
    art = Image.open(source).convert('RGB')
    scale = min(W * 0.80 / art.width, H * 0.80 / art.height)
    art = art.resize((int(art.width * scale), int(art.height * scale)), Image.LANCZOS)
    yy, xx = np.mgrid[0:H, 0:W]
    radius = np.sqrt(((xx - W / 2) / (W * 0.55)) ** 2 + ((yy - H * 0.52) / (H * 0.62)) ** 2)
    glow = np.clip(1 - radius, 0, 1) ** 2.2
    base = np.array([4, 6, 7], dtype=np.float32) + glow[..., None] * np.array([38, 14, 58], dtype=np.float32)
    canvas = Image.fromarray(base.clip(0, 255).astype(np.uint8))
    left, top = (W - art.width) // 2, int(H * 0.52 - art.height / 2)
    # Screen-blend the black-backed emblem so its violet halo merges into the glow.
    region = np.asarray(canvas.crop((left, top, left + art.width, top + art.height)), dtype=np.float32)
    blended = 255 - (255 - region) * (255 - np.asarray(art, dtype=np.float32)) / 255
    canvas.paste(Image.fromarray(blended.clip(0, 255).astype(np.uint8)), (left, top))
    return canvas


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--emblem', type=Path)
    parser.add_argument('--lab', type=Path)
    args = parser.parse_args()
    city().save(ROOT / 'city.png', optimize=True)
    print('wrote', ROOT / 'city.png')
    if args.emblem:
        emblem(args.emblem).save(ROOT / 'emblem.png', optimize=True)
        print('wrote', ROOT / 'emblem.png')
    if args.lab:
        Image.open(args.lab).convert('RGB').save(ROOT / 'lab.png', optimize=True)
        print('wrote', ROOT / 'lab.png')


if __name__ == '__main__':
    main()
