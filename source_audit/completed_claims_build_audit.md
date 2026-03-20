# Completed Claims vs Current Build Audit

Generated: 2026-03-20 19:37:46

## Scope

- docs/FULL_AUDIT_MASTER.md
- UNFINISHED_FEATURES.md

- WIN32IDE_SOURCES entries parsed: **529**
- ASM_KERNEL_SOURCES entries parsed: **47**
- Active CMakeLists scanned: **103**
- Claims scanned: **37**

## Mismatch Summary

| Mismatch Type | Count |
|---------------|-------|
| claimed_file_missing | 0 |
| claimed_file_missing_but_referenced_by_win32ide_sources | 0 |
| claimed_file_not_in_expected_build_graph | 0 |
| claimed_symbol_missing | 0 |
| claimed_completed_with_not_implemented_markers | 0 |

## Claimed file missing

- None

## Claimed file missing but still referenced by WIN32IDE_SOURCES

- None

## Claimed complete but not in expected build graph

- None

## Claimed symbol missing

- None

## Claimed complete but file still contains 'not implemented'

- None

## Notes

- This audit compares current repository state, not historical build artifacts.
- Inclusion checks are against current root `CMakeLists.txt` WIN32IDE source graph.
- Marker hits are textual signals and should be manually triaged before code changes.
