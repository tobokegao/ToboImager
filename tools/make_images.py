"""The pictures of the manual, made from what PanelShot.exe writes (size3_v0.png, size3_v1.png, size3_v2.png = the panel
at 2x with the three kinds of stereo meter).
usage: python tools/make_images.py <folder with the PanelShot pictures>"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

SRC = Path(sys.argv[1]); OUT = Path(__file__).resolve().parent.parent / 'docs' / 'img'
Z = 2                                  # the pictures are at 2x: one unit of the panel is two pixels
BG = (228, 233, 239); INK = (14, 30, 48); RED = (214, 48, 49)
FONT = ImageFont.truetype(r'C:\Windows\Fonts\arialbd.ttf', 22)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    p = [Image.open(SRC / f'size3_v{v}.png').convert('RGB') for v in range(3)]
    p[0].save(OUT / 'screenshot.png')

    # the panel with numbers in the margin and a line from each number to its part
    M = 56; w, h = p[0].size
    im = Image.new('RGB', (w + 2 * M, h + 2 * M), BG); im.paste(p[0], (M, M)); d = ImageDraw.Draw(im)
    # number: (side of the margin, position along it in units, target x, target y in units)
    marks = [(1, 'L', 100, 16, 100), (17, 'T', 70, 88, 24), (2, 'T', 118, 118, 46), (3, 'T', 184, 184, 110), (4, 'T', 262, 262, 18),
             (5, 'L', 150, 40, 150), (6, 'L', 192, 52, 192), (7, 'L', 230, 33, 230), (8, 'L', 271, 28, 271), (9, 'L', 309, 17, 309),
             (10, 'B', 199, 199, 356), (11, 'R', 292, 449, 276), (12, 'R', 328, 434, 328), (13, 'R', 23, 449, 23),
             (14, 'R', 103, 444, 103), (15, 'R', 198, 454, 208), (16, 'R', 246, 452, 257)]
    for n, side, pos, tx, ty in marks:
        cx, cy = {'L': (M // 2, M + pos * Z), 'R': (w + M + M // 2, M + pos * Z), 'T': (M + pos * Z, M // 2), 'B': (M + pos * Z, h + M + M // 2)}[side]
        d.line([(cx, cy), (M + tx * Z, M + ty * Z)], fill=RED, width=3)
        d.ellipse([M + tx * Z - 5, M + ty * Z - 5, M + tx * Z + 5, M + ty * Z + 5], fill=RED)
        d.ellipse([cx - 19, cy - 19, cx + 19, cy + 19], fill=RED)
        t = str(n); b = d.textbbox((0, 0), t, font=FONT)
        d.text((cx - (b[0] + b[2]) / 2, cy - (b[1] + b[3]) / 2), t, font=FONT, fill=(255, 255, 255))
    im.save(OUT / 'panel_map.png')

    # the three stereo meters side by side
    box = (304 * Z, 4 * Z, 460 * Z, 196 * Z); c = [q.crop(box) for q in p]; g = 16
    im = Image.new('RGB', (sum(x.width for x in c) + 4 * g, c[0].height + 2 * g), BG); x = g
    for q in c: im.paste(q, (x, g)); x += q.width + g
    im.save(OUT / 'meters.png')
    print('pictures in', OUT)


if __name__ == '__main__':
    main()
