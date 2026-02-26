# RawrXD Report Schema v1.0 — Frozen Specification

**Status:** FROZEN  
**Date:** 2026-02-10  
**Binary:** RawrXD Unified MASM64 IDE v2.0  
**Schema Version:** 1  

All RawrXD analysis modes produce structured JSON reports. This document **freezes** the schema for external consumption. Any field additions in future versions MUST be backward-compatible (additive only). Existing fields MUST NOT change type, name, or semantics.

---

## Common Rules

| Rule | Spec |
|------|------|
| **Encoding** | UTF-8, no BOM |
| **Block identity** | RVA-only (relative virtual address from `.text` base) |
| **Numeric offsets** | Hex string `"0xHHHH"` for addresses, decimal for counts |
| **Version field** | `"schema_version": 1` present in every report root |
| **ASLR invariance** | All addresses are RVA — ASLR-independent by design |
| **Max buffer** | 256KB per report (JSON_REPORT_MAX = 262144) |
| **Null safety** | Missing optional fields omitted, not set to null |

---

## 1. bbcov_report.json (Mode 12: BBCov)

**Producer:** `BasicBlockCovMode PROC` → `BBGenerateJSON PROC`  
**Purpose:** Static basic block discovery via PE .text disassembly

```json
{
  "basic_block_coverage": {
    "schema_version": 1,
    "version": 1,
    "summary": {
      "total_blocks": 2835,
      "suspect_blocks": 1,
      "data_regions": 0,
      "text_base": "0x1000",
      "text_size": 29256
    },
    "blocks": [
      {
        "id": 0,
        "offset": "0x0",
        "size": 16,
        "type": "entry",
        "flags": 0
      }
    ]
  }
}
```

### Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `schema_version` | int | Always `1` for this spec |
| `version` | int | Report format version (currently `1`) |
| `summary.total_blocks` | int | Total basic blocks discovered |
| `summary.suspect_blocks` | int | Blocks flagged by validation (zero-size, oversized, padding, data-in-code) |
| `summary.data_regions` | int | Skip regions detected (jump tables, embedded data) |
| `summary.text_base` | string | RVA of `.text` section (hex) |
| `summary.text_size` | int | Size of `.text` section in bytes |
| `blocks[].id` | int | Sequential block index (0-based) |
| `blocks[].offset` | string | RVA offset within `.text` (hex) |
| `blocks[].size` | int | Block size in bytes |
| `blocks[].type` | string | Block type: `"entry"`, `"fall"`, `"jcc"`, `"jmp"`, `"call"`, `"ret"`, `"data"`, `"?"` |
| `blocks[].flags` | int | Bitmask: `0x1`=zero-size, `0x2`=oversized, `0x4`=in-data-region, `0x8`=all-padding |

---

## 2. dyntrace_report.json (Mode 14: DynTrace)

**Producer:** `DynTraceMode PROC`  
**Purpose:** Dynamic trace via debug API (breakpoint injection + single-step)

```json
{
  "dyntrace_report": {
    "schema_version": 1,
    "version": 1,
    "summary": {
      "total_blocks": 2835,
      "hit_blocks": 412,
      "miss_blocks": 2423,
      "coverage_pct": 14,
      "total_hits": 1847,
      "target_pid": 12345,
      "text_base": "0x1000",
      "text_size": 29256
    },
    "blocks": [
      {
        "id": 0,
        "offset": "0x0",
        "size": 16,
        "type": "entry",
        "hits": 3,
        "status": "hot"
      }
    ]
  }
}
```

### Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `summary.hit_blocks` | int | Blocks executed at least once |
| `summary.miss_blocks` | int | Blocks never executed |
| `summary.coverage_pct` | int | `hit_blocks * 100 / total_blocks` |
| `summary.total_hits` | int | Sum of all block hit counts |
| `summary.target_pid` | int | Traced process ID |
| `blocks[].hits` | int | Execution count for this block |
| `blocks[].status` | string | `"hot"` (>1), `"warm"` (==1), `"cold"` (==0) |

---

## 3. gapfuzz_report.json (Mode 16: GapFuzz)

**Producer:** `GapFuzzMode PROC`  
**Purpose:** Coverage gap analysis + fuzz target prioritization

```json
{
  "mode": "GapFuzz",
  "schema_version": 1,
  "target_pid": 0,
  "total_blocks": 2835,
  "hit_blocks": 412,
  "gap_count": 2423,
  "gap_pct": 85,
  "branch_gaps": 150,
  "deep_gaps": 89,
  "edge_gaps": 42,
  "fuzz_targets": [
    {
      "rank": 1,
      "block_id": 145,
      "rva": "0x2A80",
      "score": 95,
      "type": "branch",
      "priority": "critical"
    }
  ],
  "analysis_duration_ms": 12,
  "status": "complete"
}
```

### Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `gap_count` | int | Blocks with zero coverage |
| `gap_pct` | int | `gap_count * 100 / total_blocks` |
| `branch_gaps` | int | Gaps at untaken conditional branches |
| `deep_gaps` | int | Gaps in deeply nested code paths |
| `edge_gaps` | int | Gaps in error/exception handlers |
| `fuzz_targets[].rank` | int | Priority rank (1 = highest) |
| `fuzz_targets[].score` | int | Fuzzing priority score (0-100) |
| `fuzz_targets[].type` | string | Gap classification: `"branch"`, `"deep"`, `"edge"`, `"unknown"` |
| `fuzz_targets[].priority` | string | `"critical"`, `"high"`, `"medium"`, `"low"` |
| `analysis_duration_ms` | int | Wall-clock analysis time |

---

## 4. covfusion_report.json (Mode 13: CovFusion v2)

**Producer:** `CovFusionMode PROC` → `CovFuseV2_Engine`  
**Purpose:** Weighted synthesis of static + dynamic + gap analysis

```json
{
  "covfusion_report": {
    "schema_version": 1,
    "fusion_version": "2.0",
    "summary": {
      "total_blocks": 2835,
      "hit_blocks": 412,
      "missed_blocks": 2423,
      "partial_blocks": 0,
      "coverage_pct": 14,
      "fuzz_targets": 50,
      "trace_targets": 100,
      "inject_targets": 2273,
      "ignore_count": 12
    },
    "recommendations": []
  }
}
```

### Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `fusion_version` | string | CovFusion engine version (`"2.0"`) |
| `summary.fuzz_targets` | int | Blocks recommended for fuzzing |
| `summary.trace_targets` | int | Blocks recommended for deeper tracing |
| `summary.inject_targets` | int | Blocks needing breakpoint injection |
| `summary.ignore_count` | int | Blocks classified as noise/padding |
| `recommendations[]` | array | Scored action list (block_id, action, score) |

### Weight Table (frozen)

| Weight | Value | Purpose |
|--------|-------|---------|
| `hit_count` | 30 | DynTrace hit frequency influence |
| `gap_priority` | 50 | GapFuzz priority influence |
| `static_size` | 10 | Block size heuristic |
| `type_bonus` | 10 | Block type bonus (call/jcc preferred) |

### Action Thresholds (frozen)

| Action | Score Range | Meaning |
|--------|------------|---------|
| `fuzz` | ≥ 80 | High-priority fuzzing target |
| `trace` | 50-79 | Needs dynamic instrumentation |
| `inject` | 10-49 | Basic breakpoint coverage needed |
| `ignore` | < 10 | Noise (padding, data, trivial) |

---

## 5. diffcov_report.json (Mode 18: DiffCov)

**Producer:** `DiffCovMode PROC`  
**Purpose:** Behavioral drift detection between two dyntrace snapshots

```json
{
  "diffcov_report": {
    "schema_version": 1,
    "version": "1.0",
    "summary": {
      "old_blocks": 2835,
      "new_blocks": 2840,
      "drift_score": 12,
      "new_count": 5,
      "removed_count": 0,
      "changed_count": 340,
      "stable_count": 2495
    },
    "deltas": [
      {
        "block_id": 42,
        "old_hits": 3,
        "new_hits": 0,
        "delta": -3,
        "status": "removed"
      }
    ]
  }
}
```

### Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `summary.old_blocks` | int | Block count from old snapshot |
| `summary.new_blocks` | int | Block count from new snapshot |
| `summary.drift_score` | int | `((new + removed + changed) * 100) / total` — 0=identical, 100=total divergence |
| `summary.new_count` | int | Blocks present only in new snapshot |
| `summary.removed_count` | int | Blocks present only in old snapshot |
| `summary.changed_count` | int | Blocks with different hit counts |
| `summary.stable_count` | int | Blocks with identical behavior |
| `deltas[].delta` | int | `new_hits - old_hits` (signed) |
| `deltas[].status` | string | `"new"`, `"removed"`, `"changed"`, `"stable"` |

### Drift Score Thresholds

| Score | Classification | Meaning |
|-------|---------------|---------|
| 0 | Identical | No behavioral change |
| 1-39 | Low drift | Minor code path changes |
| 40-74 | Moderate drift | Significant behavioral shift |
| 75-100 | High drift | Major regression / mutation |

---

## ASLR Invariance Guarantee

All addresses in every report are **RVA-only** (offsets from `.text` section base). This means:

- Reports are deterministic across ASLR randomizations
- Reports are diffable across builds (same source → same RVAs)
- Block identity is address-space independent
- No runtime `ImageBase` appears in any report field

The `text_base` field records the PE section RVA (typically `0x1000`), not the loaded virtual address.

---

## Versioning Policy

| Rule | Spec |
|------|------|
| `schema_version` | Monotonically increasing integer. Current: `1` |
| Additive changes | New optional fields may be added without version bump |
| Breaking changes | Require `schema_version` increment to `2` |
| Removal | Fields will never be removed, only deprecated |
| External consumers | MUST check `schema_version` and handle unknown fields gracefully |

---

## File Naming Convention (Frozen)

| Mode | Output File | Schema |
|------|-------------|--------|
| 12 (BBCov) | `bbcov_report.json` | §1 |
| 13 (CovFusion) | `covfusion_report.json` | §4 |
| 14 (DynTrace) | `dyntrace_report.json` | §2 |
| 16 (GapFuzz) | `gapfuzz_report.json` | §3 |
| 18 (DiffCov) | `diffcov_report.json` | §5 |

---

*This schema is frozen as of v2.0-tier2-complete. External tools, CI pipelines, and dashboards may depend on this contract.*
