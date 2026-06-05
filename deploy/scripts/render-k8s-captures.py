#!/usr/bin/env python3
"""Render terminal-style PNG from captured kubectl output."""

from __future__ import annotations

from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ModuleNotFoundError as exc:
    raise SystemExit("Missing dependency 'Pillow' (PIL). Install with: pip install Pillow") from exc

ROOT = Path(__file__).resolve().parents[2]
IMG_DIR = ROOT / "docs" / "img"
FONT = ImageFont.load_default()


def render_terminal_png(source: Path, destination: Path, title: str) -> None:
    lines = source.read_text(encoding="utf-8").splitlines()
    width = max((len(line) for line in lines), default=40) * 8 + 40
    height = len(lines) * 18 + 60
    image = Image.new("RGB", (width, height), "#0c0c0c")
    draw = ImageDraw.Draw(image)
    draw.text((16, 12), title, fill="#9cdcfe", font=FONT)
    y = 40
    for line in lines:
        draw.text((16, y), line, fill="#cccccc", font=FONT)
        y += 18
    destination.parent.mkdir(parents=True, exist_ok=True)
    image.save(destination)


def main() -> None:
    render_terminal_png(
        IMG_DIR / "k8s-pods.txt",
        IMG_DIR / "k8s-pods.png",
        "$ kubectl get pods",
    )
    render_terminal_png(
        IMG_DIR / "k8s-svc.txt",
        IMG_DIR / "k8s-svc.png",
        "$ kubectl get svc",
    )
    render_terminal_png(
        IMG_DIR / "k8s-deployments.txt",
        IMG_DIR / "k8s-deployments.png",
        "$ kubectl get deployments",
    )


if __name__ == "__main__":
    main()
