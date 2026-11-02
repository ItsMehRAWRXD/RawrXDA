# AUDIT_SUMMARY â€” STORAGE_AUDIT_001

ASOF_UTC=2026-11-02T10:42:43.2938830Z
MODE=READ_ONLY
PROMOTE=0
CERTIFIED=0
NO_DELETIONS_PERFORMED

## Volume F:
| Metric | Value |
|--------|-------|
| Size | ~3725.9 GiB |
| Free | ~89.16 GiB |
| Used | ~3636.74 GiB |
| Files under F:\rawrxd | 1,851,573 |
| Logical sum under F:\rawrxd | ~848.94 GiB |
| Inventory rows (>=1 MiB) | 15674 |
| Files >=1 GiB | 50 |

## Dummy verdict
F:\rawrxd\dummy_200b.gguf = DELETE_CANDIDATE
- NOT sparse; allocated 400.00 GiB physical
- Not valid GGUF (zero header); evidence labels it zero placeholder
- Independent twin on G:\~dev\rawrxd\dummy_200b.gguf (not counted in F: recovery)

## LOW-risk recoverable (F: only, no double-count)
~421.51 GiB = dummy 400 + run logs ~13.51 + tmp_model 8.00

## Top recovery candidates (physical)
See TOP20_RECOVERY_CANDIDATES.csv â€” #1 is dummy @ 400 GiB.

## Deliverables
1. STORAGE_INVENTORY.csv
2. LARGEST_FILES.csv
3. DUPLICATES.csv
4. GIT_CLASSIFICATION.csv
5. CLEANUP_PLAN.md
6. RECOVERY_ESTIMATE.json
7. AUDIT_SUMMARY.md (this file)
8. DUMMY_200B_ANALYSIS.md

Also: INVENTORY_SCOPE.txt, TOP20_RECOVERY_CANDIDATES.csv, VOLUME.json, DEEPSEEK_HASH_NOTE.txt
