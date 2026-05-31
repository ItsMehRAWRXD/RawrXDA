# RawrXD Release Validation Harness

## Overview

Automated validation system for RawrXD release tags. Ensures all critical components are present and functional before promotion.

## Usage

### Local Validation
```powershell
# Validate a specific tag
.\ci\release_validation_harness.ps1 -Tag v1.0.0-gold

# Skip build (use existing binary)
.\ci\release_validation_harness.ps1 -Tag v1.0.0-gold -SkipBuild

# Skip tests (faster validation)
.\ci\release_validation_harness.ps1 -Tag v1.0.0-gold -SkipTests
```

### CI/CD Integration

The harness runs automatically on GitHub Actions for every release tag:

```yaml
on:
  push:
    tags:
      - 'v*'
```

## Validation Checks

### 1. Tag Integrity
- Verifies tag exists in git
- Confirms commit hash matches

### 2. Build Verification (optional)
- Clean CMake configuration
- Full ninja build
- Binary size validation (~14.7MB expected)

### 3. AST Scope-Awareness Tests
- Access modifier sovereignty
- Template parameter deduction
- CRTP pattern recognition
- Concept constraints (C++20)
- Nested class scope resolution
- Lambda capture analysis

### 4. Inference Loop Sanity
- Critical component presence check
- AST Graph Engine
- Execution Scheduler Integration
- KV FP8 Quantization
- Token Pipeline Double-Buffer
- AST Completion Bridge

### 5. Thread Safety Audit
- Lock-free pattern verification
- Atomic operations
- Concurrent queue usage
- Shared mutex patterns

### 6. Memory Safety Audit
- Smart pointer usage ratio
- RAII compliance check
- Raw pointer detection

## Exit Codes

- `0` - All validations passed, release cleared for promotion
- `1` - One or more validations failed

## Output Format

```
========================================
VALIDATION SUMMARY
========================================
Tag: v1.0.0-gold
Total Tests: 5
Passed: 5
Failed: 0
Warnings: 0
Duration: 239.61ms

✅ RELEASE VALIDATION PASSED
Tag v1.0.0-gold is cleared for promotion
```

## Integration with GitHub Actions

The `.github/workflows/release_validation.yml` workflow:

1. Triggers on release tags
2. Sets up MSVC and Ninja
3. Caches build artifacts
4. Runs validation harness
5. Uploads validation report
6. Creates GitHub release on success

## Manual Release Creation

If CI/CD is not available, create release manually:

```powershell
# 1. Validate
.\ci\release_validation_harness.ps1 -Tag v1.0.0-gold

# 2. Create release notes
git log --oneline v0.9.0..v1.0.0-gold > RELEASE_NOTES.md

# 3. Tag and push
git tag -a v1.0.0-gold -m "Release v1.0.0-gold"
git push origin v1.0.0-gold
```

## Troubleshooting

### Test Executable Not Found
The harness will attempt to compile `tests\ast_scope_validation_real.cpp` if `tests\ast_test.exe` is missing.

### Build Failures
Check that Visual Studio 2022 is installed with C++ workload and that `cl.exe` is in PATH.

### Component Missing
Verify file paths in the `$components` array match actual source tree structure.

## Validation History

| Tag | Date | Result | Duration |
|-----|------|--------|----------|
| v1.0.0-gold | 2026-05-02 | ✅ PASS | 239ms |
