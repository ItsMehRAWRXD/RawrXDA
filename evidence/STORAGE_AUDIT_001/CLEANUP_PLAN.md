# CLEANUP_PLAN â€” STORAGE_AUDIT_001

ASOF_UTC=2026-11-02T10:42:43.2938830Z
MODE=READ_ONLY_AUDIT
PROMOTE=0
CERTIFIED=0
NO DELETION WITHOUT EXPLICIT BATCH APPROVAL

## Sampling rule
See INVENTORY_SCOPE.txt. Full tree enumerated (1,851,573 files). Inventory CSV = files >= 1 MiB only.

## Policy
- KEEP: unique source, configs, manifests, unique models, sealed certification evidence
- REVIEW: duplicate models/fixtures, obsolete builds, superseded evidence, large dumps
- DELETE_CANDIDATE: verified disposable dummy, redundant temps, rebuildable outputs without evidentiary purpose
- Git: never force-add ignored *.gguf / models

## F: vs G: overlap
- F:\rawrxd and G:\~dev\rawrxd are separate NTFS volumes (serials 0x66a37175 vs 0x8ae8e0db)
- Both hold independent dummy_200b.gguf @ 400 GiB allocated â€” deleting F: does not free G:

## Batch 1 â€” dummy first (LOW)
| Path | Allocated | Risk | Deps | Justification |
|------|-----------|------|------|---------------|
| F:\rawrxd\dummy_200b.gguf | 400.00 GiB | LOW | Benchmark/smoke refs only; evidence already calls it zero placeholder | Synthetic all-zero non-GGUF; gitignored; not sparse |

Action: PENDING APPROVAL. See DUMMY_200B_ANALYSIS.md.

## Batch 2 â€” disposable run logs (LOW)
| Path | Size | Risk | Justification |
|------|------|------|---------------|
| F:\rawrxd\__run_output30.txt .. __run_output35.txt | ~2.25 GiB each (~13.5 GiB) | LOW | Unique content but disposable run dumps; not sealed evidence |

## Batch 3 â€” duplicate model working copies (LOW to MED)
| Path | Size | Risk | Justification |
|------|------|------|---------------|
| F:\rawrxd\tmp_model-00001-of-000004.safetensors | 8.00 GiB | LOW | QUICK fingerprint == git LFS object a314b79a...; keep LFS |
| F:\rawrxd\tmp_safetensors\ | ~29 GiB logical | MED | Overlaps LFS shards; confirm all 4 shards mapped before delete |

## Batch 4 â€” duplicate fixtures (MED; preserve one)
| Group | Paths | Extra bytes | Note |
|-------|-------|-------------|------|
| llama3.2-3b-Q2_K identical | root + evidence\reap_calibration\roundtrip* (5 copies) | ~5.08 GiB extras | Keep one authoritative; do not wipe sealed receipt dirs blindly |
| corrupted_test.gguf | gguf_lifecycle_test{,3,4} | ~1.25 GiB extras | Keep one fixture |

## Batch 5 â€” old builds / redundant evidence (MED)
- Local build*, out, bin trees: REVIEW then DELETE_CANDIDATE if rebuildable
- Do not bulk-delete evidence\ sealed certification trees

## Batch 6 â€” KEEP
- F:\rawrxd\models\*.gguf unique weights (many sparse)
- nemotron-bolt.gguf, DeepSeek-V2-Lite-Chat.Q4_K_M.gguf (different hash from DIAGNOSTIC_REPAIRED)
- Source under asm\, lavapath\, manifests, sealed evidence

## RawrXD-owned dirs on F:\ (outside F:\rawrxd focus)
50 top-level *rawrxd* directories on F:\ (archives, builds, preservations, WebView2). Separate follow-up after Batch 1.

## Git staging
See GIT_CLASSIFICATION.csv. SAFE_TO_STAGE only for intended source/config. NO_FORCE_ADD for ignored models.
