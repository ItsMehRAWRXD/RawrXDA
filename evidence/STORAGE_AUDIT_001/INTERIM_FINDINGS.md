# STORAGE_AUDIT_001 — Interim findings (read-only)

ASOF_UTC=2026-11-02T10:16:00Z  
MODE=READ_ONLY  
PROMOTE=0  
CERTIFIED=0  
NO_DELETIONS_PERFORMED

## Volume F:

| Metric | Value |
|--------|-------|
| Size | ~3726 GiB (4000650883072 bytes) |
| Free | **~89.16 GiB** |
| Used | **~3636.74 GiB** |
| FS | NTFS |

## Priority finding: `dummy_200b.gguf`

| Path | File ID | Sparse? | Logical | Allocated |
|------|---------|---------|---------|-----------|
| `F:\rawrxd\dummy_200b.gguf` | `0x0005000000026c94` | **NO** | 400.0 GiB | **400.0 GiB** |
| `G:\~dev\rawrxd\dummy_200b.gguf` | `0x0001000000056a33` | (separate volume) | 400.0 GiB | **400.0 GiB** |

- These are **two independent files** on two volumes (not hardlinks).
- F: copy Valid Data Length = full size; `fsutil sparse` = not sparse; `$DATA` Allocated Size = 400.0 GB.
- **Expected F: recovery if deleted after approval: ~400 GiB physical** (would raise free from ~89 GiB toward ~489 GiB).
- G: copy deletion would recover space on **G:** only (~1.47 TB free already observed on G:).
- Classification: **DELETE_CANDIDATE** after dependency sign-off; until then **REVIEW**.
- Name strongly indicates synthetic fixture; full code/test reference scan still completing with inventory.

## Audit status

Chunked enumerator running: `run_audit_chunked.ps1`  
Output directory: `F:\rawrxd\evidence\STORAGE_AUDIT_001\`

Required deliverables (populate as run completes):

- STORAGE_INVENTORY.csv
- LARGEST_FILES.csv
- DUPLICATES.csv
- GIT_CLASSIFICATION.csv
- CLEANUP_PLAN.md
- RECOVERY_ESTIMATE.json
- DUMMY_200B_ANALYSIS.md
- AUDIT_SUMMARY.md

## Policy reminder

- Option 1 Git staging: **no force-add** of ignored models.
- No blind deletion of 675 untracked entries.
- KEEP sealed certification evidence and unique models until dependencies verified.
- Approve cleanup **by batch** from CLEANUP_PLAN.md only.
