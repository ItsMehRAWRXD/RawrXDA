# Test Suite Naming Standard (2026-03-27)

Purpose: Eliminate ambiguity like `6/6` vs `11/11` by naming suites explicitly.

## Canonical Suite IDs

- `ZW-CORE-6`
  - Script: `d:\scripts\directvalidate.ps1`
  - Scope: core zero-wrapper checks
  - Expected summary format: `ZW-CORE-6: 6/6 PASS`

- `ZW-SMOKE-BUILD`
  - Script: `d:\scripts\smoke_test_build_verification.ps1`
  - Scope: clean build + linker/binary checks
  - Expected summary format: `ZW-SMOKE-BUILD: PASS|FAIL`

- `ZW-SMOKE-HTTP` (optional, requires running server)
  - Script: `d:\scripts\smoke_test_zero_wrapper.ps1`
  - Scope: HTTP route/tool endpoint behavior
  - Expected summary format: `ZW-SMOKE-HTTP: X/Y PASS`

## Reporting Rules

1. Never report a bare fraction (example: `11/11`) without suite id.
2. Always include script path and timestamp in release notes.
3. If multiple suites are combined, report per-suite first, then rollup:
   - Example: `ROLLUP: 2/2 suites passing`
4. If prerequisites are missing (e.g. HTTP server), mark suite `NOT RUN` instead of failing the rollup.

## Recommended Release Summary Block

```text
Validation Summary
- ZW-CORE-6: 6/6 PASS (d:\scripts\directvalidate.ps1)
- ZW-SMOKE-BUILD: PASS (d:\scripts\smoke_test_build_verification.ps1)
- ZW-SMOKE-HTTP: NOT RUN (server not started)
- ROLLUP: 2/2 runnable suites passing
```

## Transition Note

Legacy statements like `11/11 tests passing` are deprecated unless they identify the exact suite composition. Use canonical suite ids above for all future reports.
