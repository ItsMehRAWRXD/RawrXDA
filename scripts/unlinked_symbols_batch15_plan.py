#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--audit-json", default="/workspace/source_audit/unlinked_symbols_production_audit.json")
    ap.add_argument("--output-md", default="/workspace/source_audit/unlinked_symbols_batches_15.md")
    ap.add_argument("--batch-size", type=int, default=15)
    args = ap.parse_args()

    data = json.loads(Path(args.audit_json).read_text(encoding="utf-8"))
    symbols = data.get("symbols", [])

    # "No impact" ordering: use first appearance in linker log.
    def first_line(sym: dict) -> int:
        lines = sym.get("reference_lines") or []
        return min(lines) if lines else 10**9

    ordered = sorted(symbols, key=lambda s: (first_line(s), s["symbol"]))
    total = len(ordered)
    batch_size = max(1, args.batch_size)
    batches = math.ceil(total / batch_size)

    out = []
    out.append("# Unlinked Symbol Batches (15 each)")
    out.append("")
    out.append(f"- Source audit: `{args.audit_json}`")
    out.append(f"- Total symbols: **{total}**")
    out.append(f"- Batch size: **{batch_size}**")
    out.append(f"- Total batches: **{batches}**")
    out.append("")

    for i in range(batches):
        start = i * batch_size
        end = min(start + batch_size, total)
        batch = ordered[start:end]
        out.append(f"## Batch {i+1:03d} ({start+1}-{end})")
        out.append("")
        out.append("| # | Symbol | Classification | Occurrences | First log line |")
        out.append("|---:|---|---|---:|---:|")
        for n, sym in enumerate(batch, start=start + 1):
            out.append(
                f"| {n} | `{sym['symbol']}` | `{sym['classification']}` | "
                f"{sym.get('occurrences', 0)} | {first_line(sym)} |"
            )
        out.append("")

    Path(args.output_md).write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"Wrote: {args.output_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
