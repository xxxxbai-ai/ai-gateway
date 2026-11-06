"""Build the native vector brand mark into a Windows ICO (Pillow optional tooling)."""
from pathlib import Path
from PIL import Image, ImageDraw

size = 512
scale = size / 64
image = Image.new("RGBA", (size, size))
draw = ImageDraw.Draw(image)
box = lambda values: tuple(round(value * scale) for value in values)
draw.rounded_rectangle(box((1, 1, 63, 63)), round(17 * scale), fill="#426650")
for points in (((18, 45), (31, 17), (44, 45)), ((23, 35), (39, 35))):
    pixels = [(round(x * scale), round(y * scale)) for x, y in points]
    draw.line(pixels, fill="#f9f5eb", width=round(3.8 * scale), joint="curve")
    for x, y in pixels:
        radius = 1.9 * scale
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill="#f9f5eb")
draw.ellipse(box((45, 13, 53, 21)), fill="#d2dfb8")
image.save(Path(__file__).resolve().parent.parent / "app.ico", sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
print("Windows icon prepared")
