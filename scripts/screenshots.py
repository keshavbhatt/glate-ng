#!/usr/bin/env python3
"""Composes the store screenshots and the banner from raw window grabs.

    scripts/screenshots.py <raw-dir>   # writes screenshots/*.png and screenshots/store/*.png

Run through scripts/screenshots.sh, which makes the raw grabs (GLATE_DEBUG_GRAB) in <raw-dir>:
    main.png  offline.png  languages.png  linebyline.png  history.png  share.png  settings.png
Style, as in playlist-dl: the icon's blue gradient with soft circles, a headline, a one-line
subtitle, the window framed with rounded corners and a shadow.
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT_BOLD = "/usr/share/fonts/noto/NotoSans-Bold.ttf"
FONT_REG = "/usr/share/fonts/noto/NotoSans-Regular.ttf"
FONT_MED = "/usr/share/fonts/noto/NotoSans-Medium.ttf"
LOGO = ROOT / "icons/icon-512.png"

TOP = (0x3E, 0x74, 0xBF)  # the icon's deep blue
BOTTOM = (0x0E, 0x1A, 0x2E)  # towards a dark navy ground
SUBTITLE = (0xD6, 0xE6, 0xF8)


def font(path, size):
    return ImageFont.truetype(path, size)


def background(w, h):
    """Vertical blue gradient with two soft, translucent circles."""
    img = Image.new("RGB", (w, h), TOP)
    draw = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        draw.line([(0, y), (w, y)], fill=tuple(int(TOP[i] * (1 - t) + BOTTOM[i] * t) for i in range(3)))
    overlay = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    r = int(h * 0.55)
    od.ellipse((w - r * 1.1, -r * 0.5, w - r * 1.1 + r * 1.6, -r * 0.5 + r * 1.6), fill=(255, 255, 255, 14))
    od.ellipse((-r * 0.6, h - r * 0.9, -r * 0.6 + r * 1.4, h - r * 0.9 + r * 1.4), fill=(255, 255, 255, 12))
    return Image.alpha_composite(img.convert("RGBA"), overlay)


def framed(shot, width, radius=14):
    """The grab scaled to `width`, rounded, on a soft shadow (RGBA)."""
    scale = width / shot.width
    shot = shot.convert("RGB").resize((width, int(shot.height * scale)), Image.LANCZOS)
    mask = Image.new("L", shot.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, shot.width - 1, shot.height - 1), radius=radius, fill=255)
    pad = 60
    canvas = Image.new("RGBA", (shot.width + pad * 2, shot.height + pad * 2), (0, 0, 0, 0))
    shadow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        (pad, pad + 18, pad + shot.width, pad + shot.height + 18), radius=radius, fill=(0, 0, 0, 150)
    )
    canvas = Image.alpha_composite(canvas, shadow.filter(ImageFilter.GaussianBlur(28)))
    rounded = Image.new("RGBA", shot.size, (0, 0, 0, 0))
    rounded.paste(shot, (0, 0), mask)
    # A hairline so the dark window separates from the dark ground.
    ImageDraw.Draw(rounded).rounded_rectangle(
        (0, 0, shot.width - 1, shot.height - 1), radius=radius, outline=(255, 255, 255, 40)
    )
    canvas.alpha_composite(rounded, (pad, pad))
    return canvas


def text_centered(draw, y, text, fnt, fill, w):
    draw.text(((w - draw.textlength(text, font=fnt)) / 2, y), text, font=fnt, fill=fill)


def compose(shot, headline, subtitle, size=(1280, 800), top=56):
    w, h = size
    img = background(w, h)
    d = ImageDraw.Draw(img)
    text_centered(d, top, headline, font(FONT_BOLD, int(h * 0.058)), (255, 255, 255), w)
    text_centered(d, top + int(h * 0.085), subtitle, font(FONT_REG, int(h * 0.026)), SUBTITLE, w)
    # Fit the window into the box under the text, keeping its aspect ratio.
    box_top = top + int(h * 0.15)
    box_w, box_h = int(w * 0.80), h - box_top - int(h * 0.05)
    scale = min(box_w / shot.width, box_h / shot.height, 1.25)
    fr = framed(shot, int(shot.width * scale))
    inner_h = int(shot.height * scale)
    y = box_top + (box_h - inner_h) // 2 - 60  # 60 = frame padding
    img.alpha_composite(fr, ((w - fr.width) // 2, y))
    return img.convert("RGB")


def banner(window, size=(2160, 720)):
    w, h = size
    img = background(w, h)
    d = ImageDraw.Draw(img)
    logo = Image.open(LOGO).convert("RGBA").resize((150, 150), Image.LANCZOS)
    x0 = 150
    img.alpha_composite(logo, (x0, 112))
    d.text((x0 + 180, 100), "Glate", font=font(FONT_BOLD, 104), fill=(255, 255, 255))
    d.text((x0, 300), "Translate and listen, online or offline.", font=font(FONT_REG, 54), fill=(255, 255, 255))
    d.text((x0, 380), "A desktop translator with text-to-speech for Linux and Windows.",
           font=font(FONT_REG, 30), fill=SUBTITLE)
    # Translucent pills need their own layer: ImageDraw writes alpha, it does not blend.
    pf = font(FONT_MED, 27)
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    x = x0
    for pill in ("100+ languages", "Offline mode", "Read aloud", "Line by line"):
        tw = ld.textlength(pill, font=pf)
        ld.rounded_rectangle((x, 470, x + tw + 56, 528), radius=29, fill=(255, 255, 255, 40))
        ld.text((x + 28, 481), pill, font=pf, fill=(255, 255, 255, 255))
        x += tw + 56 + 22
    img = Image.alpha_composite(img, layer)
    fr = framed(window, 820, radius=16)
    img.alpha_composite(fr, (w - fr.width - 30, (h - fr.height) // 2))
    return img.convert("RGB")


PLAN = [
    ("00-hero", "main", "Translate between 100+ languages",
     "Type or paste text, pick the languages, and read the translation side by side."),
    ("01-offline", "offline", "Translate offline, privately",
     "Download a language once and translate without internet. Your text never leaves your device."),
    ("02-languages", "languages", "Nearly 60 languages offline",
     "Mozilla's Firefox translation models, downloaded only for the languages you pick."),
    ("03-linebyline", "linebyline", "Check it line by line",
     "Each sentence next to its translation, with copy and read aloud on both sides."),
    ("04-history", "history", "Every translation, kept",
     "Search your history, load an earlier translation again, or remove it."),
    ("05-share", "share", "Share it, or listen to it",
     "Send by email, create a share link, save as text, or export the speech as audio."),
    ("06-settings", "settings", "Make it yours",
     "Light or dark, two themes, your accent colour, a quick-translate shortcut."),
]


def main(raw):
    raw = Path(raw)
    out = ROOT / "screenshots"
    store = out / "store"
    store.mkdir(parents=True, exist_ok=True)
    for name, key, head, sub in PLAN:
        shot = Image.open(raw / f"{key}.png")
        compose(shot, head, sub).save(out / f"{name}.png", optimize=True)
        compose(shot, head, sub, size=(1600, 1000)).save(store / f"{name}.png", optimize=True)
    banner(Image.open(raw / "main.png")).save(out / "banner.png", optimize=True)
    for f in sorted(out.glob("*.png")) + sorted(store.glob("*.png")):
        print(f.relative_to(ROOT), Image.open(f).size, f.stat().st_size // 1024, "KB")


if __name__ == "__main__":
    main(sys.argv[1])
