#!/usr/bin/env python3
"""
Generate a Stripe/Anthropic-inspired PDF deck from JSON inputs:
  - logs/command_usage_runtime.json (usage[] histograms)
  - harness *_summary.json (rates, latency)
  - optional competitor_positioning.json (narrative + matrix)

Usage:
  py -3 -m venv .venv && .venv\\Scripts\\pip install -r requirements.txt
  py -3 generate_pdf_deck.py --inputs ..\\..\\logs\\command_usage_runtime.json examples\\competitor_positioning.json --out ..\\..\\reports\\metrics_deck.pdf
"""

from __future__ import annotations

import argparse
import base64
import json
import tempfile
from html import escape as html_escape
from pathlib import Path
from typing import Any

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import LETTER
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    Image,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)

# Stripe/Anthropic-adjacent palette (restrained, high-contrast)
BG = colors.HexColor("#0c0c0e")
SURFACE = colors.HexColor("#16161a")
TEXT = colors.HexColor("#f4f4f5")
MUTED = colors.HexColor("#a1a1aa")
ACCENT = colors.HexColor("#6366f1")
ACCENT2 = colors.HexColor("#22d3ee")
RULE = colors.HexColor("#27272a")


def _load_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def _merge_inputs(paths: list[Path]) -> dict[str, Any]:
    merged: dict[str, Any] = {}
    for p in paths:
        data = _load_json(p)
        if not isinstance(data, dict):
            continue
        for k, v in data.items():
            if k == "usage" and isinstance(v, list) and isinstance(merged.get("usage"), list):
                merged["usage"] = merged["usage"] + v
            elif k in merged and isinstance(merged[k], dict) and isinstance(v, dict):
                merged[k] = {**merged[k], **v}
            else:
                merged[k] = v
    return merged


def _chart_command_usage(usage: list[dict[str, Any]], out_dir: Path) -> Path | None:
    if not usage:
        return None
    rows = sorted(
        usage,
        key=lambda r: int(r.get("attempts") or 0),
        reverse=True,
    )[:18]
    labels = [str(r.get("canonical") or r.get("id"))[:28] for r in rows]
    vals = [int(r.get("attempts") or 0) for r in rows]
    fig, ax = plt.subplots(figsize=(10.5, 6.2), facecolor="#0c0c0e")
    ax.set_facecolor("#16161a")
    y = range(len(labels))
    ax.barh(y, vals, color="#6366f1", height=0.65, alpha=0.92)
    ax.set_yticks(list(y))
    ax.set_yticklabels(labels, color="#e4e4e7", fontsize=8)
    ax.tick_params(axis="x", colors="#a1a1aa")
    ax.set_xlabel("Attempts", color="#a1a1aa", fontsize=9)
    ax.set_title("Command usage (top by attempts)", color="#fafafa", fontsize=12, pad=12)
    for s in ax.spines.values():
        s.set_color("#3f3f46")
    ax.grid(axis="x", color="#3f3f46", alpha=0.35)
    fig.tight_layout()
    png = out_dir / "chart_command_usage.png"
    fig.savefig(png, dpi=160, facecolor=fig.get_facecolor())
    plt.close(fig)
    return png


def _chart_harness_summary(data: dict[str, Any], out_dir: Path) -> Path | None:
    keys = [
        ("pass@1", "pass@1"),
        ("task_completion_rate", "Task completion"),
        ("patch_correctness", "Patch correctness"),
        ("test_pass_rate", "Test pass rate"),
    ]
    pairs: list[tuple[str, float]] = []
    for json_key, label in keys:
        v = data.get(json_key)
        if v is None:
            continue
        try:
            pairs.append((label, float(v)))
        except (TypeError, ValueError):
            pass
    if not pairs:
        return None
    labels = [p[0] for p in pairs]
    vals = [max(0.0, min(1.0, p[1])) for p in pairs]
    fig, ax = plt.subplots(figsize=(8.5, 4.8), facecolor="#0c0c0e")
    ax.set_facecolor("#16161a")
    x = range(len(labels))
    ax.bar(x, vals, color="#22d3ee", alpha=0.88, width=0.55)
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels, rotation=15, ha="right", color="#e4e4e7", fontsize=8)
    ax.set_ylim(0, 1.05)
    ax.set_ylabel("Rate", color="#a1a1aa", fontsize=9)
    ax.tick_params(axis="y", colors="#a1a1aa")
    for s in ax.spines.values():
        s.set_color("#3f3f46")
    ax.grid(axis="y", color="#3f3f46", alpha=0.35)
    ax.set_title("Harness summary (key rates)", color="#fafafa", fontsize=12, pad=12)
    fig.tight_layout()
    png = out_dir / "chart_harness.png"
    fig.savefig(png, dpi=160, facecolor=fig.get_facecolor())
    plt.close(fig)
    return png


def _styles() -> dict[str, ParagraphStyle]:
    base = getSampleStyleSheet()
    return {
        "title": ParagraphStyle(
            name="T",
            parent=base["Heading1"],
            fontName="Helvetica-Bold",
            fontSize=22,
            textColor=TEXT,
            alignment=TA_LEFT,
            spaceAfter=8,
        ),
        "subtitle": ParagraphStyle(
            name="S",
            parent=base["Normal"],
            fontName="Helvetica",
            fontSize=11,
            textColor=MUTED,
            alignment=TA_LEFT,
            spaceAfter=18,
        ),
        "h2": ParagraphStyle(
            name="H2",
            parent=base["Heading2"],
            fontName="Helvetica-Bold",
            fontSize=14,
            textColor=TEXT,
            spaceAfter=10,
            spaceBefore=6,
        ),
        "body": ParagraphStyle(
            name="B",
            parent=base["Normal"],
            fontName="Helvetica",
            fontSize=10,
            textColor=TEXT,
            alignment=TA_LEFT,
            leading=14,
        ),
        "small": ParagraphStyle(
            name="Sm",
            parent=base["Normal"],
            fontName="Helvetica",
            fontSize=8,
            textColor=MUTED,
            alignment=TA_CENTER,
            leading=11,
        ),
        "coverTitle": ParagraphStyle(
            name="CT",
            parent=base["Title"],
            fontName="Helvetica-Bold",
            fontSize=26,
            textColor=TEXT,
            alignment=TA_CENTER,
            spaceAfter=12,
        ),
        "coverSub": ParagraphStyle(
            name="CS",
            parent=base["Normal"],
            fontName="Helvetica",
            fontSize=12,
            textColor=MUTED,
            alignment=TA_CENTER,
        ),
    }


def _cover_flow(st: dict[str, ParagraphStyle], title: str, subtitle: str) -> list[Any]:
    story: list[Any] = []
    story.append(Spacer(1, 2.2 * inch))
    story.append(Paragraph(title.replace("&", "&amp;"), st["coverTitle"]))
    story.append(Spacer(1, 0.15 * inch))
    story.append(Paragraph(subtitle.replace("&", "&amp;"), st["coverSub"]))
    story.append(Spacer(1, 0.4 * inch))
    story.append(
        Paragraph(
            "Auto-generated from JSON · charts + narrative · competitor-aware",
            st["small"],
        )
    )
    story.append(PageBreak())
    return story


def _competitor_section(st: dict[str, ParagraphStyle], data: dict[str, Any]) -> list[Any]:
    story: list[Any] = []
    comps = data.get("competitors") or []
    if not comps:
        return story
    story.append(Paragraph("Competitive framing", st["h2"]))
    line = data.get("positioningLine") or ""
    if line:
        story.append(Paragraph(line.replace("&", "&amp;"), st["body"]))
        story.append(Spacer(1, 0.12 * inch))

    # Matrix: dimensions scored 1–5 (qualitative; edit JSON to tune)
    headers = ["Product", "Desktop IDE UX", "Local packaging", "Serving throughput", "Agent+terminal loop"]
    ollama = [4, 5, 2, 2]
    vllm = [1, 2, 5, 2]
    rawrxd = [5, 3, 3, 5]
    # Allow override from JSON
    ov = data.get("matrixScores")
    if isinstance(ov, dict):
        ollama = ov.get("ollama", ollama)
        vllm = ov.get("vllm", vllm)
        rawrxd = ov.get("rawrxd", rawrxd)
    mat = [headers]
    names = ["Ollama", "vLLM", "RawrXD"]
    for name, row in zip(names, [ollama, vllm, rawrxd]):
        mat.append([name, *[str(x) for x in row]])

    t = Table(mat, colWidths=[1.35 * inch, 1.15 * inch, 1.15 * inch, 1.15 * inch, 1.45 * inch])
    t.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), SURFACE),
                ("TEXTCOLOR", (0, 0), (-1, 0), TEXT),
                ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
                ("FONTSIZE", (0, 0), (-1, -1), 8),
                ("TEXTCOLOR", (0, 1), (-1, -1), TEXT),
                ("GRID", (0, 0), (-1, -1), 0.25, RULE),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.HexColor("#121214"), colors.HexColor("#0e0e10")]),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("LEFTPADDING", (0, 0), (-1, -1), 6),
                ("RIGHTPADDING", (0, 0), (-1, -1), 6),
                ("TOPPADDING", (0, 0), (-1, -1), 5),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
            ]
        )
    )
    story.append(t)
    story.append(Spacer(1, 0.18 * inch))

    for c in comps:
        if not isinstance(c, dict):
            continue
        name = str(c.get("name", ""))
        tag = str(c.get("tagline", ""))
        story.append(Paragraph(f"<b>{name}</b> — {tag.replace('&', '&amp;')}", st["body"]))
        strengths = c.get("strengths") or []
        gaps = c.get("gaps") or []
        if strengths:
            story.append(
                Paragraph(
                    "<font color='#a1a1aa'>Strengths:</font> "
                    + " · ".join(str(s).replace("&", "&amp;") for s in strengths),
                    st["body"],
                )
            )
        if gaps:
            story.append(
                Paragraph(
                    "<font color='#a1a1aa'>Gaps:</font> "
                    + " · ".join(str(s).replace("&", "&amp;") for s in gaps),
                    st["body"],
                )
            )
        story.append(Spacer(1, 0.1 * inch))

    story.append(PageBreak())
    return story


def _chart_pages(st: dict[str, ParagraphStyle], paths: dict[str, Path | None]) -> list[Any]:
    story: list[Any] = []
    story.append(Paragraph("Telemetry charts", st["h2"]))
    story.append(
        Paragraph(
            "Rendered from your JSON inputs (matplotlib). "
            "Regenerate after runs to keep investor / internal decks fresh.",
            st["body"],
        )
    )
    story.append(Spacer(1, 0.12 * inch))
    for key, label in (("usage", "Command usage"), ("harness", "Harness summary")):
        p = paths.get(key)
        if p and p.exists():
            story.append(Paragraph(label, st["h2"]))
            story.append(Image(str(p), width=6.8 * inch, height=(4.0 if key == "usage" else 3.2) * inch))
            story.append(Spacer(1, 0.15 * inch))
    story.append(PageBreak())
    return story


def build_pdf(
    merged: dict[str, Any],
    out_pdf: Path,
    tmp: Path,
) -> None:
    st = _styles()
    title = str(merged.get("deckTitle") or "RawrXD · metrics deck")
    subtitle = str(merged.get("subtitle") or "JSON-driven charts & positioning")

    charts: dict[str, Path | None] = {}
    usage = merged.get("usage")
    if isinstance(usage, list):
        charts["usage"] = _chart_command_usage(usage, tmp)
    # Harness shape: flat keys at root
    if any(k in merged for k in ("pass@1", "task_completion_rate", "patch_correctness")):
        charts["harness"] = _chart_harness_summary(merged, tmp)

    story: list[Any] = []
    story.extend(_cover_flow(st, title, subtitle))
    story.extend(_competitor_section(st, merged))
    story.extend(_chart_pages(st, charts))

    # Appendix: raw JSON excerpt
    story.append(Paragraph("Appendix · source keys", st["h2"]))
    slim = {k: merged[k] for k in list(merged.keys())[:24]}
    blob = html_escape(json.dumps(slim, indent=2)[:12000])
    story.append(Paragraph(f"<font face='Courier' size='7'>{blob}</font>", st["body"]))

    def _draw_bg(canvas, doc) -> None:
        canvas.saveState()
        canvas.setFillColor(BG)
        canvas.rect(0, 0, doc.pagesize[0], doc.pagesize[1], fill=1, stroke=0)
        canvas.setStrokeColor(RULE)
        canvas.setLineWidth(0.5)
        canvas.line(36, 36, doc.pagesize[0] - 36, 36)
        canvas.restoreState()

    out_pdf.parent.mkdir(parents=True, exist_ok=True)
    doc = SimpleDocTemplate(
        str(out_pdf),
        pagesize=LETTER,
        leftMargin=42,
        rightMargin=42,
        topMargin=48,
        bottomMargin=48,
    )
    doc.build(story, onFirstPage=_draw_bg, onLaterPages=_draw_bg)


def build_html(
    merged: dict[str, Any],
    out_html: Path,
    tmp: Path,
) -> None:
    """Self-contained HTML deck (print to PDF from browser if needed)."""
    title = str(merged.get("deckTitle") or "RawrXD · metrics deck")
    subtitle = str(merged.get("subtitle") or "")
    charts: dict[str, Path | None] = {}
    usage = merged.get("usage")
    if isinstance(usage, list):
        charts["usage"] = _chart_command_usage(usage, tmp)
    if any(k in merged for k in ("pass@1", "task_completion_rate", "patch_correctness")):
        charts["harness"] = _chart_harness_summary(merged, tmp)

    def b64(p: Path | None) -> str:
        if not p or not p.exists():
            return ""
        return base64.standard_b64encode(p.read_bytes()).decode("ascii")

    imgs_html = ""
    if charts.get("usage"):
        imgs_html += f'<section class="slide"><h2>Command usage</h2><img alt="chart" src="data:image/png;base64,{b64(charts["usage"])}" /></section>'
    if charts.get("harness"):
        imgs_html += f'<section class="slide"><h2>Harness summary</h2><img alt="chart" src="data:image/png;base64,{b64(charts["harness"])}" /></section>'

    comps = merged.get("competitors") or []
    comp_html = ""
    for c in comps:
        if not isinstance(c, dict):
            continue
        name = html_escape(str(c.get("name", "")))
        tag = html_escape(str(c.get("tagline", "")))
        sth = html_escape(" · ".join(str(s) for s in (c.get("strengths") or [])))
        gap = html_escape(" · ".join(str(s) for s in (c.get("gaps") or [])))
        comp_html += f'<article class="card"><h3>{name}</h3><p class="tag">{tag}</p><p><span class="lbl">Strengths</span> {sth}</p><p><span class="lbl">Gaps</span> {gap}</p></article>'

    pos = html_escape(str(merged.get("positioningLine") or ""))
    css = """
    :root { --bg:#0c0c0e; --surface:#16161a; --text:#f4f4f5; --muted:#a1a1aa; --accent:#6366f1; --rule:#27272a; }
    * { box-sizing: border-box; }
    body { margin:0; font-family: ui-sans-serif, system-ui, "Segoe UI", Helvetica, Arial, sans-serif; background:var(--bg); color:var(--text); line-height:1.5; }
    .cover { min-height: 100vh; display:flex; flex-direction:column; justify-content:center; align-items:center; padding:3rem; text-align:center;
      background: radial-gradient(ellipse 80% 60% at 50% -20%, rgba(99,102,241,0.15), transparent), var(--bg); }
    .cover h1 { font-size: clamp(1.75rem, 4vw, 2.25rem); font-weight: 600; letter-spacing: -0.02em; margin:0 0 0.5rem; }
    .cover p { color: var(--muted); max-width: 36rem; margin:0; font-size: 0.95rem; }
    .deck { max-width: 960px; margin: 0 auto; padding: 2.5rem 1.5rem 4rem; }
    .slide { margin-bottom: 3rem; }
    .slide h2 { font-size: 1rem; font-weight: 600; color: var(--muted); text-transform: uppercase; letter-spacing: 0.12em; margin-bottom: 1rem; }
    .slide img { width: 100%; height: auto; border-radius: 8px; border: 1px solid var(--rule); }
    .grid { display: grid; gap: 1rem; }
    @media (min-width: 720px) { .grid { grid-template-columns: repeat(3, 1fr); } }
    .card { background: var(--surface); border: 1px solid var(--rule); border-radius: 10px; padding: 1.25rem; }
    .card h3 { margin: 0 0 0.35rem; font-size: 1.05rem; }
    .tag { color: var(--muted); font-size: 0.85rem; margin: 0 0 0.75rem; }
    .lbl { color: var(--accent); font-size: 0.7rem; text-transform: uppercase; letter-spacing: 0.08em; }
    .position { font-size: 1.05rem; margin: 1.5rem 0 2rem; padding: 1rem 1.25rem; border-left: 3px solid var(--accent); background: var(--surface); border-radius: 0 8px 8px 0; }
    footer { text-align: center; color: var(--muted); font-size: 0.75rem; padding: 2rem; border-top: 1px solid var(--rule); }
    """
    html = f"""<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"/><meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>{html_escape(title)}</title>
<style>{css}</style></head><body>
<div class="cover"><h1>{html_escape(title)}</h1><p>{html_escape(subtitle)}</p></div>
<div class="deck">
  <p class="position">{pos}</p>
  <h2 class="slide" style="margin-bottom:1rem">Ollama · vLLM · RawrXD</h2>
  <div class="grid">{comp_html}</div>
  {imgs_html}
</div>
<footer>Generated from JSON · print to PDF via browser (Ctrl+P)</footer>
</body></html>"""
    out_html.parent.mkdir(parents=True, exist_ok=True)
    out_html.write_text(html, encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser(description="JSON → charts → PDF deck")
    ap.add_argument(
        "--inputs",
        nargs="+",
        type=Path,
        required=True,
        help="One or more JSON files (merged: usage[], harness keys, competitor block)",
    )
    ap.add_argument("--out", type=Path, default=Path("reports/metrics_deck.pdf"))
    ap.add_argument("--html", type=Path, default=None, help="Optional self-contained HTML deck path")
    args = ap.parse_args()
    merged = _merge_inputs(args.inputs)
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        build_pdf(merged, args.out, tmp)
        print(f"Wrote {args.out.resolve()}")
        if args.html:
            build_html(merged, args.html, tmp)
            print(f"Wrote {args.html.resolve()}")


if __name__ == "__main__":
    main()
