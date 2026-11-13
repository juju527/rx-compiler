#!/usr/bin/env python3
"""Render the final PDF and make contact sheets for visual review.

Requires pdftoppm on PATH, plus Python packages pdfplumber and Pillow.
All outputs are written below docs/LLVM/build/qa.
"""
from pathlib import Path
import json
import subprocess

import pdfplumber
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
QA = ROOT / "build" / "qa"
QA.mkdir(parents=True, exist_ok=True)
for pattern in ("page-*.png", "page-*.txt", "contact-*.png"):
    for old in QA.glob(pattern):
        old.unlink()

subprocess.run(
    ["pdftoppm", "-scale-to", "1400", "-png",
     str(ROOT / "llvm-ir-guide.pdf"), str(QA / "page")],
    check=True,
)
pages = []
with pdfplumber.open(ROOT / "llvm-ir-guide.pdf") as pdf:
    for i, page in enumerate(pdf.pages, 1):
        text = page.extract_text() or ""
        (QA / f"page-{i:02d}.txt").write_text(text)
        edge = [
            char["text"] for char in page.chars
            if char["x0"] < 40 or char["x1"] > page.width - 35
            or char["top"] < 20 or char["bottom"] > page.height - 20
        ]
        pages.append({
            "page": i,
            "characters": len(page.chars),
            "edge_characters": "".join(edge),
            "replacement_character_found": "\ufffd" in text,
            "first_lines": text.splitlines()[:4],
        })

images = sorted(QA.glob("page-*.png"))
for start in range(0, len(images), 6):
    canvas = Image.new("RGB", (1000, 2210), "#d8dfe5")
    draw = ImageDraw.Draw(canvas)
    for j, path in enumerate(images[start:start + 6]):
        with Image.open(path) as original:
            thumb = original.convert("RGB")
        thumb.thumbnail((476, 690))
        x, y = 12 + (j % 2) * 500, 26 + (j // 2) * 736
        canvas.paste(thumb, (x, y))
        draw.text((x, y - 17), f"PDF page {start + j + 1}", fill="black")
    canvas.save(QA / f"contact-{start // 6 + 1:02d}.png")

report = {
    "page_count": len(pages),
    "automated_edge_check": "pass" if all(not p["edge_characters"] for p in pages) else "review",
    "pages": pages,
    "note": "Automated checks supplement, but do not replace, visual review of the rendered pages.",
}
(QA / "text-inspection.json").write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + "\n"
)
print(f"Rendered {len(pages)} pages; edge check: {report['automated_edge_check']}")
for page in pages:
    print(f"{page['page']:02d}  {page['characters']:4d} chars  {page['first_lines'][1:3]}")
