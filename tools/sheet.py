"""Contact sheet: crop the same region of several screenshots, label them, tile 2 per row.
usage: sheet.py out.png x y w h label1=img1 label2=img2 ..."""
import sys
from PIL import Image, ImageDraw

out, x, y, w, h = sys.argv[1], *map(int, sys.argv[2:6])
items = [a.split('=', 1) for a in sys.argv[6:]]
cols = 2
rows = (len(items) + cols - 1) // cols
sheet = Image.new('RGB', (w * cols, h * rows), (0, 0, 0))
for i, (label, path) in enumerate(items):
    im = Image.open(path).convert('RGB').crop((x, y, x + w, y + h))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 8 * len(label) + 8, 16), fill=(0, 0, 0))
    d.text((4, 2), label, fill=(255, 255, 0))
    sheet.paste(im, ((i % cols) * w, (i // cols) * h))
sheet.save(out)
