# Arabic build: app logo (design "A": 3D neon Ra). Source art is rendered from
# a 512px SVG with headless Chrome to tools/ar/logoA.png (1024px, transparent).
# Run from repo root: python tools/ar/make_logo.py   (needs Pillow)
from PIL import Image, ImageDraw, ImageFont
import pathlib

root = pathlib.Path(__file__).resolve().parents[2]
res = root / "resources"
src = Image.open(root / "tools" / "ar" / "logoA.png").convert("RGBA")


def sized(n):
    return src.resize((n, n), Image.LANCZOS)


sized(256).save(res / "icon.png")
sized(256).save(res / "icon.ico",
                sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
sized(64).save(res / "settings" / "logo.png")

# About page banner: logo + name
w, h = 1261, 264
banner = Image.new("RGBA", (w, h), (0, 0, 0, 0))
banner.alpha_composite(sized(236), (w - 250, 14))
d = ImageDraw.Draw(banner)
black = str(res / "fonts" / "thmanyahsans-Black.otf")
medium = str(res / "fonts" / "thmanyahsans-Medium.otf")
d.text((w - 280, h / 2 - 30), "Chatterino AR", font=ImageFont.truetype(black, 92),
       fill=(255, 255, 255, 255), anchor="rm")
d.text((w - 280, h / 2 + 56), "Made By RayanAthlawi", font=ImageFont.truetype(medium, 44),
       fill=(125, 255, 196, 255), anchor="rm")
banner.save(res / "settings" / "aboutlogo.png")
print("ok")
