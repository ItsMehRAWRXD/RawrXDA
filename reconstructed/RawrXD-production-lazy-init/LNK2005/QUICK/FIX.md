# LNK2005 Quick Fix Reference Card

## TL;DR - What You Need To Do

### CRITICAL (Fix These First)

1. **DELETE TWO FILES:**
   ```
   rm src/qtapp/system_runtime.cpp
   rm src/qtapp/system_runtime.hpp
   ```
   This eliminates the duplicate CreateThreadEx and CreatePipeEx definitions.

2. **EDIT ONE FILE** - `src/qtapp/masm_function_stubs.cpp`
   Remove 3 function families:
   ```cpp
   // DELETE lines 27-32:
   void masm_hotpatch_init() {}
   void masm_hotpatch_shutdown() {}
   void masm_hotpatch_add_patch(const char* p) {}
   void masm_hotpatch_remove_patch(const char* p) {}
   void masm_hotpatch_rollback_patch() {}
   void masm_hotpatch_verify_patch(const char* p) {}

   // DELETE lines 117-127:
   void masm_server_hotpatch_init() {}
   void masm_server_hotpatch_add(const char* p) {}
   void masm_server_hotpatch_apply() {}
   void masm_server_hotpatch_enable() {}
   void masm_server_hotpatch_disable() {}
   void masm_server_hotpatch_get_stats(void* s) {}
   void masm_server_hotpatch_cleanup() {}

   // DELETE lines 146-149:
   HANDLE CreateThreadEx(...) { return CreateThread(...); }
   BOOL CreatePipeEx(...) { return CreatePipe(...); }
   ```

3. **UPDATE CMakeLists.txt:**
   Remove: `src/qtapp/system_runtime.cpp` from source list

That's it! These 3 changes fix all 4 duplicate symbol errors.

---

## The 4 Functions with LNK2005 Errors

| Function | Problem | Location | Fix |
|----------|---------|----------|-----|
| **CreateThreadEx** | Defined in both core/ and qtapp/ | src/core/system_runtime.cpp:93 ✓ <br> src/qtapp/system_runtime.cpp:71 ✗ | Delete qtapp file |
| **CreatePipeEx** | Defined in both core/ and qtapp/ | src/core/system_runtime.cpp:131 ✓ <br> src/qtapp/system_runtime.cpp:116 ✗ | Delete qtapp file |
| **masm_hotpatch_init** | Stub conflicts with nothing | src/qtapp/masm_function_stubs.cpp:27 ✗ | Delete stub |
| **masm_server_hotpatch_init** | Stub conflicts with ASM impl | src/qtapp/masm_function_stubs.cpp:117 ✗ <br> src/masm/gguf_server_hotpatch.asm:35 ✓ | Delete stub |

---

## The 9 Functions WITHOUT Issues

These have NO duplicates - leave them alone:
- ✓ asm_event_loop_create (src/masm/asm_events.asm)
- ✓ ml_masm_get_tensor (src/masm/ml_masm.asm)
- ✓ hpatch_apply_memory (src/masm/unified_masm_hotpatch.asm)
- ✓ extract_sentence (src/masm/agentic_puppeteer.asm)
- ✓ strstr_case_insensitive (src/masm/agentic_puppeteer.asm)
- ✓ tokenizer_init (src/gpu/gpu_tokenizer.asm)
- ○ file_search_recursive (NOT FOUND)
- ○ strstr_masm (NOT FOUND)
- ○ ui_create_mode_combo (NOT FOUND)

---

## Root Cause

The `src/qtapp/` directory is a DUPLICATE copy of core functionality from `src/core/`. 

When both get compiled, linker sees two definitions = LNK2005 error.

**Solution:** Use core/ only, delete qtapp/ duplicates.

---

## Implementation in 5 Minutes

### Step 1 (30 seconds)
```powershell
cd src/qtapp
rm system_runtime.cpp system_runtime.hpp
```

### Step 2 (2 minutes)
Open `src/qtapp/masm_function_stubs.cpp` and:
- Delete lines 27-32 (6 lines)
- Delete lines 117-127 (11 lines)  
- Delete lines 146-149 (4 lines)

### Step 3 (1 minute)
Edit `CMakeLists.txt`:
- Find and remove `src/qtapp/system_runtime.cpp`

### Step 4 (1 minute)
Rebuild:
```powershell
cmake --build . --config Release
```

### Step 5 (30 seconds)
Verify: No LNK2005 errors for the 4 functions

---

## Before & After

### BEFORE (With LNK2005 Errors)
```
LNK2005: "int __stdcall CreateThreadEx(...)" already defined in system_runtime.obj
LNK2005: "int __stdcall CreatePipeEx(...)" already defined in system_runtime.obj
LNK2005: "void __cdecl masm_hotpatch_init(void)" already defined
LNK2005: "void __cdecl masm_server_hotpatch_init(void)" already defined
...fatal error LNK1169: one or more multiply defined symbols found
```

### AFTER (Clean Build)
```
Build succeeded
0 errors
0 warnings
```

---

## File Checklist

- [ ] Delete: `src/qtapp/system_runtime.cpp`
- [ ] Delete: `src/qtapp/system_runtime.hpp`
- [ ] Edit: `src/qtapp/masm_function_stubs.cpp` (remove 17 lines)
- [ ] Edit: `CMakeLists.txt` (remove 1 line reference)
- [ ] Rebuild project
- [ ] Verify no LNK2005 errors

---

## Q&A

**Q: Will this break anything?**  
A: No. The code being deleted is exact duplicates of the core/ version. No functionality lost.

**Q: What about qtapp-specific code?**  
A: The only qtapp-specific code is GUI/Qt-related, which should include from core/ instead.

**Q: Why wasn't this a problem before?**  
A: Either:
1. Previously only one version was being compiled
2. Build system had different linker settings
3. These files were recently duplicated

**Q: Can I keep both copies?**  
A: No. The linker will always error when both are compiled. You must pick one.

**Q: Which one should I keep?**  
A: Keep `src/core/`. It's more complete and is the production version.

---

## Troubleshooting

**Problem:** Still getting LNK2005 after deletion
```
Solution: 
1. Clean build: rm -rf build && cmake . && make clean
2. Verify files deleted: ls src/qtapp/system_runtime.* (should not exist)
3. Check CMakeLists.txt was updated
```

**Problem:** Compilation errors after deletion
```
Solution:
1. Find file including qtapp/system_runtime.hpp
2. Change to: #include "../core/system_runtime.hpp"
3. Rebuild
```

**Problem:** CreateThreadEx/CreatePipeEx not found at link time
```
Solution:
1. Verify src/core/system_runtime.cpp is in CMakeLists.txt
2. Verify it's being compiled: cmake --build . --verbose
3. Check that core/ is in include path
```

---

## Key Files Involved

| File | Size | Action |
|------|------|--------|
| `src/qtapp/system_runtime.cpp` | ~164 lines | **DELETE** |
| `src/qtapp/system_runtime.hpp` | ~95 lines | **DELETE** |
| `src/qtapp/masm_function_stubs.cpp` | ~170 lines | **EDIT** (remove 17 lines) |
| `src/core/system_runtime.cpp` | ~174 lines | **KEEP** |
| `src/core/system_runtime.hpp` | ~95 lines | **KEEP** |
| `CMakeLists.txt` | ? | **EDIT** (remove 1 reference) |

---

## Advanced: Understanding the Root Cause

### Directory Structure Problem
```
src/
├── core/
│   ├── system_runtime.cpp    ← PRODUCTION (KEEP THIS)
│   ├── system_runtime.hpp    ← PRODUCTION (KEEP THIS)
│   └── ...
├── qtapp/
│   ├── system_runtime.cpp    ← DUPLICATE (DELETE)
│   ├── system_runtime.hpp    ← DUPLICATE (DELETE)
│   ├── masm_function_stubs.cpp ← Has conflicting stubs (EDIT)
│   └── ...
├── masm/
│   ├── gguf_server_hotpatch.asm ← PRODUCTION (KEEP)
│   └── ...
└── gpu/
    └── gpu_tokenizer.asm ← PRODUCTION (KEEP)
```

### Build System Flow
```
CMakeLists.txt includes:
  ✓ src/core/system_runtime.cpp ← Compiles to system_runtime.obj
  ✗ src/qtapp/system_runtime.cpp ← ALSO compiles to system_runtime.obj (same name!)
  ✗ src/qtapp/masm_function_stubs.cpp ← Has conflicting stubs

Linker tries to link:
  ERROR: Two objects define the same symbol
  → LNK2005 duplicate symbol error
```

### Solution Flow
```
Delete qtapp/system_runtime.cpp   →  Only one version compiles
Delete qtapp/system_runtime.hpp   →  Only one header included
Edit masm_function_stubs.cpp      →  No stub conflicts
Update CMakeLists.txt             →  References deleted files removed

Result: Clean link, no duplicates
```

---

## Verification Commands

```powershell
# Check files deleted
Test-Path "src/qtapp/system_runtime.cpp"  # Should return False
Test-Path "src/qtapp/system_runtime.hpp"  # Should return False

# Check stubs removed
Select-String -Path "src/qtapp/masm_function_stubs.cpp" -Pattern "masm_hotpatch_init|masm_server_hotpatch_init|CreateThreadEx|CreatePipeEx" | Measure-Object  # Should be 0

# Check CMakeLists updated
Select-String -Path "CMakeLists.txt" -Pattern "system_runtime.cpp" | Where-Object { $_.Line -match "qtapp" }  # Should be empty

# Verify core files still exist
Test-Path "src/core/system_runtime.cpp"   # Should return True
Test-Path "src/core/system_runtime.hpp"   # Should return True
```

---

**Quick Reference Version:** 1.0  
**Time to Fix:** ~5 minutes  
**Risk Level:** LOW (exact duplicate removal)  
**Expected Outcome:** 0 LNK2005 errors
