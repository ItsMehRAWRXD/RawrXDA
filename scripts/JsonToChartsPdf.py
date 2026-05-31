#!/usr/bin/env python3
"""
JsonToChartsPdf — turn structured JSON metrics into matplotlib charts + a multi-page PDF.

Dependencies (pip install matplotlib numpy):
    pip install matplotlib numpy

Usage:
    python JsonToChartsPdf.py path/to/metrics.json -o report.pdf
    python JsonToChartsPdf.py path/to/metrics.json --stdout-json   # echo normalized schema

Input JSON (flexible):
{
  "title": "RawrXD Inference Report",
  "series": [
    { "name": "Latency p50 (ms)", "values": [12, 11, 10, 9], "labels": ["w1","w2","w3","w4"] },
    { "name": "Throughput (tok/s)", "values": [120, 135, 140], "kind": "bar" }
  ],
  "tables": [
    { "title": "Competitors", "columns": ["Product", "Role"], "rows": [["Ollama", "Local dev"], ["vLLM", "Serving"]] }
  ]
}

If "series" is omitted but top-level numeric keys exist, they are charted as line series.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

import numpy as np


def _load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def _normalize_payload(raw: dict[str, Any]) -> dict[str, Any]:
    """Coerce minimal / legacy shapes into {title, series, tables}."""
    out: dict[str, Any] = {"title": raw.get("title", "Metrics"), "series": [], "tables": raw.get("tables", [])}
    if "series" in raw and isinstance(raw["series"], list):
        out["series"] = raw["series"]
        return out
    # Heuristic: chart any list of numbers
    for k, v in raw.items():
        if k in ("title", "tables"):
            continue
        if isinstance(v, list) and v and all(isinstance(x, (int, float)) for x in v):
            out["series"].append({"name": k, "values": [float(x) for x in v], "kind": "line"})
    return out


def _render_pdf(payload: dict[str, Any], out_path: Path) -> None:
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages

    # Stripe-adjacent neutrals (not generic purple)
    plt.rcParams.update(
        {
            "font.family": "sans-serif",
            "font.sans-serif": ["Segoe UI", "Helvetica Neue", "Arial", "DejaVu Sans"],
            "axes.facecolor": "#fafafa",
            "figure.facecolor": "#ffffff",
            "axes.edgecolor": "#e6e6e6",
            "axes.labelcolor": "#1a1a1a",
            "text.color": "#1a1a1a",
            "xtick.color": "#525252",
            "ytick.color": "#525252",
            "grid.color": "#ebebeb",
            "grid.linestyle": "-",
            "grid.linewidth": 0.8,
        }
    )

    title = str(payload.get("title", "Report"))
    series_list = payload.get("series") or []
    tables = payload.get("tables") or []

    with PdfPages(out_path) as pdf:
        # Cover
        fig = plt.figure(figsize=(8.27, 11.69))  # A4 portrait
        fig.text(0.12, 0.82, title, fontsize=22, weight="600", color="#0d0d0d")
        fig.text(0.12, 0.76, "Auto-generated charts + tables", fontsize=11, color="#666666")
        fig.text(0.12, 0.10, "RawrXD · JsonToChartsPdf.py", fontsize=9, color="#999999")
        pdf.savefig(fig)
        plt.close(fig)

        for idx, s in enumerate(series_list):
            name = str(s.get("name", f"series_{idx}"))
            values = s.get("values") or []
            if not isinstance(values, list) or not values:
                continue
            y = np.array([float(x) for x in values], dtype=float)
            labels = s.get("labels")
            x = np.arange(len(y))
            kind = str(s.get("kind", "line")).lower()

            fig, ax = plt.subplots(figsize=(8.27, 5.5))
            ax.set_title(name, fontsize=14, pad=12, color="#111111")
            if kind == "bar":
                w = 0.65
                ax.bar(x, y, width=w, color="#0f766e", edgecolor="#115e59", linewidth=0.5)
            else:
                ax.plot(x, y, color="#c2410c", linewidth=2.2, marker="o", markersize=4)
            if isinstance(labels, list) and len(labels) == len(y):
                ax.set_xticks(x)
                ax.set_xticklabels([str(l) for l in labels], rotation=25, ha="right", fontsize=8)
            ax.grid(True, alpha=0.9)
            ax.spines["top"].set_visible(False)
            ax.spines["right"].set_visible(False)
            plt.tight_layout()
            pdf.savefig(fig)
            plt.close(fig)

        for t in tables:
            if not isinstance(t, dict):
                continue
            t_title = str(t.get("title", "Table"))
            cols = t.get("columns") or []
            rows = t.get("rows") or []
            fig, ax = plt.subplots(figsize=(8.27, min(11.0, 3 + 0.35 * max(1, len(rows)))))
            ax.axis("off")
            ax.set_title(t_title, fontsize=13, loc="left", pad=8)
            if cols and rows:
                table = ax.table(
                    cellText=rows,
                    colLabels=[str(c) for c in cols],
                    loc="center",
                    cellLoc="left",
                )
                table.auto_set_font_size(False)
                table.set_fontsize(9)
                table.scale(1, 1.35)
                for (r, c), cell in table.get_celld().items():
                    if r == 0:
                        cell.set_facecolor("#f4f4f5")
                        cell.set_text_props(weight="600")
                    else:
                        cell.set_facecolor("#ffffff")
            plt.tight_layout()
            pdf.savefig(fig)
            plt.close(fig)


def main() -> int:
    ap = argparse.ArgumentParser(description="JSON metrics → charts + PDF")
    ap.add_argument("input_json", type=Path, help="Input .json file")
    ap.add_argument("-o", "--output", type=Path, default=None, help="Output .pdf path")
    ap.add_argument("--stdout-json", action="store_true", help="Print normalized JSON to stdout")
    args = ap.parse_args()

    if not args.input_json.is_file():
        print(f"Not found: {args.input_json}", file=sys.stderr)
        return 2

    raw = _load_json(args.input_json)
    payload = _normalize_payload(raw)
    if args.stdout_json:
        json.dump(payload, sys.stdout, indent=2)
        sys.stdout.write("\n")

    out = args.output or args.input_json.with_suffix(".pdf")
    try:
        _render_pdf(payload, out)
    except ImportError as e:
        print(
            "Missing dependency. Install with:\n  pip install matplotlib numpy\n",
            file=sys.stderr,
        )
        print(str(e), file=sys.stderr)
        return 3

    print(f"Wrote: {out.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
