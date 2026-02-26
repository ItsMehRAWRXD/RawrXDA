# Copilot Instructions Automation Guide

This guide explains how to keep `.github/copilot-instructions.md` automatically synchronized with the actual codebase.

---

## 🤖 Automated Validation Systems

Three validation systems work together to ensure instructions stay current:

### 1. GitHub Actions CI/CD (Cloud - Automatic)
**File**: `.github/workflows/validate-copilot.yml`

Runs automatically on:
- Any PR that modifies:
  - `.github/copilot-instructions.md`
  - `src/**/*.hpp` or `src/**/*.cpp`
  - `CMakeLists.txt`
- Push to `main` or `develop` branches

**Checks**:
- ✓ All referenced files exist in codebase
- ✓ Documented directories are present
- ✓ Build targets defined in CMakeLists.txt match instructions
- ✓ Qt configuration (MOC, C++ standard) matches CMakeLists.txt
- ✓ All hotpatch files are enabled (not disabled)
- ✓ Class patterns (PatchResult, etc.) defined correctly
- ✓ Build commands are achievable

**When it fails**:
```
PR Status: ❌ validate-copilot-instructions (Validate Copilot Instructions)
Your PR will be blocked until instructions are corrected.
```

---

### 2. Local Pre-Commit Hook (Developer Machine - Local)
**File**: `scripts/pre-commit-copilot-check.sh` (bash)

**Installation**:
```bash
# One-time setup
chmod +x scripts/pre-commit-copilot-check.sh
cp scripts/pre-commit-copilot-check.sh .git/hooks/pre-commit
```

**How it works**:
- Runs automatically before every `git commit`
- Fails commit if instructions reference non-existent files
- Warns about disabled hotpatch files
- Verifies CMakeLists.txt matches instructions

**Skip if needed**:
```bash
git commit --no-verify  # Bypass hook (use carefully!)
```

---

### 3. Manual Validation (Developer Command Line)

**Python (Windows/Mac/Linux)**:
```bash
python scripts/validate-copilot-instructions.py
```

Output:
```
============================================================
COPILOT INSTRUCTIONS VALIDATION REPORT
============================================================
✅ All validations passed!
============================================================
```

**PowerShell (Windows)**:
```powershell
.\scripts\validate-copilot-instructions.ps1 -Verbose
```

Output:
```
🔍 Validating Copilot Instructions...
✓ Found .github/copilot-instructions.md

Checking file references...
  ✓ Found: src/qtapp/model_memory_hotpatch.hpp
  ✓ Found: src/qtapp/byte_level_hotpatcher.hpp
  ...
```

---

## 📋 What Gets Checked

### Category 1: File References
✓ All `src/` paths mentioned in backticks actually exist
✓ No `.disabled` file references

### Category 2: Directory Structure
✓ `src/qtapp/` - hotpatch implementations
✓ `src/agent/` - agentic systems
✓ `.github/` - CI/CD workflows

### Category 3: Build Configuration (CMakeLists.txt)
✓ `add_executable(RawrXD-QtShell)` defined
✓ `add_library(self_test_gate)` defined
✓ `add_library(quant_utils)` defined
✓ `CMAKE_AUTOMOC ON` if referenced
✓ `CMAKE_CXX_STANDARD 20` if mentioned

### Category 4: Hotpatch Systems
✓ All 10 hotpatch files present:
  - model_memory_hotpatch.hpp/cpp
  - byte_level_hotpatcher.hpp/cpp
  - gguf_server_hotpatch.hpp/cpp
  - unified_hotpatch_manager.hpp/cpp
  - proxy_hotpatcher.hpp/cpp
✓ None are commented out in CMakeLists.txt

### Category 5: Agentic Systems
✓ All 4 agentic files present:
  - agentic_failure_detector.hpp/cpp
  - agentic_puppeteer.hpp/cpp

### Category 6: Class Patterns
✓ `struct PatchResult` defined in model_memory_hotpatch.hpp
✓ `struct UnifiedResult` defined in unified_hotpatch_manager.hpp
✓ `struct CorrectionResult` defined in agentic_puppeteer.hpp
✓ Factory methods (::ok(), ::error(), etc.) exist

### Category 7: Build Commands
✓ `cmake --build . --config Release --target RawrXD-QtShell` achievable
✓ `cmake --build . --config Release --target self_test_gate` achievable
✓ `cmake --build . --config Release --target quant_utils` achievable

---

## 🔧 Typical Workflow

### Scenario 1: Add a New Hotpatch File
```
1. Create: src/qtapp/new_hotpatcher.hpp/cpp
2. Add to CMakeLists.txt RawrXD-QtShell target
3. Update .github/copilot-instructions.md with new file reference
4. Run: python scripts/validate-copilot-instructions.py
5. Commit when validation passes
```

### Scenario 2: Disable a Component
```
1. Comment out in CMakeLists.txt: # src/qtapp/experimental_patch.hpp
2. Update .github/copilot-instructions.md "Known Constraints" section
3. Run pre-commit validation: git commit (will fail if inconsistent)
4. Fix instructions, then commit
```

### Scenario 3: Change C++ Standard
```
1. Update CMakeLists.txt: set(CMAKE_CXX_STANDARD 23)
2. Update .github/copilot-instructions.md
3. Validation will catch if they don't match
4. Commit when both are synchronized
```

### Scenario 4: Update Build Instructions
```
1. Create new task in "Common Tasks for AI Agents"
2. Run validation: .\scripts\validate-copilot-instructions.ps1
3. Validation checks build commands are achievable
4. If command fails → CI will catch before merge
```

---

## ⚠️ Common Validation Failures

### Error: "Referenced file not found: src/qtapp/new_feature.hpp"
**Fix**: Either create the file or remove it from instructions

### Error: "Target not found in CMakeLists.txt: RawrXD-QtShell"
**Fix**: Ensure `add_executable(RawrXD-QtShell ...)` exists in CMakeLists.txt

### Warning: "Hotpatch file may be disabled: src/qtapp/proxy_hotpatcher.hpp"
**Fix**: Either:
  - Uncomment in CMakeLists.txt if you want it enabled, or
  - Update instructions to document why it's disabled

### Error: "CMAKE_AUTOMOC ON not found in CMakeLists.txt"
**Fix**: If instructions say it's enabled, add it; or remove from instructions if not using Qt MOC

---

## 🚀 GitHub Actions Integration

### PR Status Check
When you create a PR:
```
✅ Tests passed
✅ Build succeeded
❌ validate-copilot-instructions — Copilot instructions out of sync
```

### Auto-Fix with GitHub Actions
Instructions cannot auto-fix yet, but the workflow will:
1. ✓ Identify exactly what's out of sync
2. ✓ Provide clear error messages
3. ✓ Link to the validation report in PR

### View Detailed Report
On PR page → Checks tab → validate-copilot-instructions → Details

---

## 📊 Validation Output Examples

### ✅ Successful Validation
```
============================================================
COPILOT INSTRUCTIONS VALIDATION REPORT
============================================================
✅ All validations passed!
============================================================
Summary: 0 errors, 0 warnings
```

### ⚠️ Warnings Only
```
⚠️  WARNINGS (review):
  ⚠️  Instructions mention CMAKE_AUTOMOC ON but not found in CMakeLists.txt
  ⚠️  Pattern 'PatchResult' mentioned in instructions but may not be defined

Summary: 0 errors, 2 warnings
✅ Build will succeed (no errors), but review warnings
```

### ❌ Errors (Blocks Merge)
```
❌ ERRORS (must fix):
  ❌ Referenced file not found: src/qtapp/missing_file.hpp
  ❌ Build target not found in CMakeLists.txt: UnknownTarget
  ❌ Hotpatch file missing: src/qtapp/byte_level_hotpatcher.cpp

Summary: 3 errors, 0 warnings
❌ Commit/merge blocked until errors are fixed
```

---

## 🔄 Keeping Instructions Current

### Monthly Review
```bash
# Run validation manually
python scripts/validate-copilot-instructions.py

# If errors appear, update instructions and rebuild
```

### When Adding Major Features
```bash
1. Implement new feature (file + code)
2. Update CMakeLists.txt
3. Document in .github/copilot-instructions.md
4. Run validation before committing
5. Verify CI passes on PR
```

### When Removing Features
```bash
1. Remove files from CMakeLists.txt (comment or delete)
2. Remove from .github/copilot-instructions.md
3. Validation ensures they're in sync
4. Commit when clean
```

---

## 🎯 Benefits

| Benefit | How It Helps | When Needed |
|---------|-------------|------------|
| **Catch Stale Docs** | Validation fails before merge | Every change to src/ |
| **Sync Multiple Authors** | Instructions stay in sync across team | Collaborative development |
| **Onboard AI Agents** | Instructions always reflect reality | Using GitHub Copilot/Cursor |
| **Prevent Regressions** | Files can't be orphaned without update | Long-term project health |
| **CI/CD Peace of Mind** | Build can't break from doc inconsistency | Production deployments |

---

## 🔗 Related Files

- `.github/copilot-instructions.md` - The instructions being validated
- `.github/workflows/validate-copilot.yml` - GitHub Actions workflow
- `scripts/validate-copilot-instructions.py` - Python validator
- `scripts/validate-copilot-instructions.ps1` - PowerShell validator
- `scripts/pre-commit-copilot-check.sh` - Git hook
- `BUILD_COMPLETE.md` - Latest build status
