# DUMMY_200B_ANALYSIS

ASOF_UTC=2026-11-02T10:42:43.2938830Z
PROMOTE=0
CERTIFIED=0
MODE=READ_ONLY_AUDIT

## Path
- F:\rawrxd\dummy_200b.gguf
- LogicalBytes=429496729600 (400.00 GiB)
- AllocatedBytes=429496729600 (FileStandardInfo + GetCompressedFileSize + fsutil)
- SparseFlag=NOT set as sparse (fsutil sparse queryflag / queryrange)
- ValidDataLength=429496729600 (fsutil file queryvaliddata)
- NumberOfLinks=1
- FileId=0x0005000000026c94

## Synthetic verdict
- GGUF magic at offset 0: absent (bytes 00 00 00 00; real GGUF begins with ASCII GGUF)
- Name dummy_200b indicates fixture, not production 200B weights
- Sealed evidence text: dummy_200b.gguf = zero placeholder

## Code / test / evidence references
- F:\rawrxd\benchmark_results\benchmark_20260719_162222.csv
- F:\rawrxd\evidence\gate5_head_repro_2026-10-14\PARITY_RECEIPTS.txt (explicit zero placeholder)
- F:\rawrxd\evidence\LARGE_MODEL_SMOKE_20261020\...\models.json
- F:\rawrxd\RECOVERY_BACKUP_MANIFEST_2026-10-12.md
- Additional G:\~dev\rawrxd\test_40gb_models\* and headless tip logs

Git: ignored via .gitignore *.gguf; not tracked; never force-add.

## Sibling on G: (separate volume)
- G:\~dev\rawrxd\dummy_200b.gguf also 400 GiB fully allocated, FileId=0x0001000000056a33
- F: serial 0x66a37175 vs G: serial 0x8ae8e0db â€” independent physical copies
- Deleting F: recovers F: only

## Classification
DELETE_CANDIDATE (LOW risk for F: space recovery after dependency sign-off).
NO DELETION PERFORMED THIS AUDIT.
