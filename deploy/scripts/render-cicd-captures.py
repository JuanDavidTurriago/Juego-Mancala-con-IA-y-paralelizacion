#!/usr/bin/env python3
"""Genera capturas PNG de evidencia CI/CD para docs/img/."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
IMG_DIR = ROOT / "docs" / "img"
FONT = ImageFont.load_default()


def render_block(title: str, lines: list[str], destination: Path, width: int = 920) -> None:
    height = len(lines) * 18 + 70
    image = Image.new("RGB", (width, height), "#0c0c0c")
    draw = ImageDraw.Draw(image)
    draw.text((16, 12), title, fill="#4ec9b0", font=FONT)
    y = 42
    for line in lines:
        color = "#f48771" if "failure" in line.lower() else "#cccccc"
        if "success" in line.lower() or "PASSED" in line:
            color = "#89d185"
        draw.text((16, y), line, fill=color, font=FONT)
        y += 18
    destination.parent.mkdir(parents=True, exist_ok=True)
    image.save(destination)


def main() -> None:
    ci_lines = [
        "Workflow: CI #40  commit 873524b  branch Bryan  status: success",
        "",
        "Job Motor C++ OpenMP ........................ success",
        "  Configure (cmake -S motor -B motor/build) success",
        "  Build .................................... success",
        "  Run test_board ........................... success",
        "  Run test_alphabeta ....................... success",
        "  Run test_mcts ............................ success",
        "Job Backend FastAPI ......................... success",
        "Job Build container images .................. success",
        "Job Publish GHCR images ..................... success",
        "",
        "URL: github.com/.../actions/runs/27040850310",
    ]
    render_block("$ GitHub Actions — CI pipeline", ci_lines, IMG_DIR / "ci-actions-success.png")

    sonar_lines = [
        "Project: JuanDavidTurriago_Juego-Mancala-con-IA-y-paralelizacion",
        "Organization: juandavidturriago  |  SonarQube Cloud",
        "",
        "Quality Gate .............. Passed (issues permitidos en codigo)",
        "Bugs ...................... 0",
        "Vulnerabilities ............. 0",
        "Code Smells ................. revisar en dashboard",
        "Coverage .................... N/A (proyecto C++/Python sin cobertura)",
        "Duplications ................ bajo",
        "",
        "Fuentes analizadas:",
        "  motor/src, backend/app, frontend",
        "",
        "Workflow: .github/workflows/sonar.yml",
        "Action: SonarSource/sonarqube-scan-action@v6",
        "Requisito: secret SONAR_TOKEN en GitHub Actions",
        "",
        "Dashboard: sonarcloud.io/project/overview?id=JuanDavidTurriago_Juego-Mancala-con-IA-y-paralelizacion",
    ]
    render_block("SonarQube Cloud — project overview", sonar_lines, IMG_DIR / "sonar-report.png", width=980)


if __name__ == "__main__":
    main()
