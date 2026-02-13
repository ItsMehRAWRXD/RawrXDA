# Pocket Lab: Enterprise Tier Validation

## Status
- **Kernel**: `pocket_lab.asm` (MASM64)
- **Dependency**: 0 (Native NT Syscalls)
- **Build**: Success (`build_pocket.ps1`)
- **Validation**: PASSED (`CI_TIER_PASS`)

## Technical Details
- **Syscalls**: `NtQuerySystemInformation` (Detect RAM), `NtOpenFile` (Map Model), `NtMapViewOfSection`.
- **Logic**: Auto-scales based on RAM (Mobile < 12GB, Enterprise > 50GB).
- **Correctness**: Fixed x64 ABI stack alignment issues in `PrintSz` and `MapGGUF`.

## Validation Output
```text
POCKET-LAB: auto-scaling GGUF runner (70-800 B Q4)
Physical RAM detected: ENTERPRISE
```

## Next Steps
- Integrate Ghost Paging with Model Weights.
- Implement full GGUF parsing logic.
