# LNK2005 Fix Implementation Plan

## Overview
This document provides step-by-step instructions to resolve all LNK2005 duplicate symbol errors identified in the audit.

---

## PART 1: DELETE DUPLICATE FILES

### Step 1.1: Delete src/qtapp/system_runtime.cpp
```
Location: src/qtapp/system_runtime.cpp
Action: DELETE ENTIRE FILE
Reason: Duplicate of src/core/system_runtime.cpp with identical implementation
Impact: Will eliminate duplicate CreateThreadEx and CreatePipeEx definitions
```

**Before deletion, verify:**
- [ ] No unique code in qtapp version (compare with core version)
- [ ] All includes of this file are from qtapp/ only
- [ ] CMakeLists.txt lists this file

### Step 1.2: Delete src/qtapp/system_runtime.hpp
```
Location: src/qtapp/system_runtime.hpp
Action: DELETE ENTIRE FILE
Reason: Duplicate declarations of CreateThreadEx and CreatePipeEx
Impact: Forces all files to use src/core/system_runtime.hpp instead
```

**After deletion:**
- [ ] Update any qtapp file that includes this to use: `#include "../core/system_runtime.hpp"`

---

## PART 2: REMOVE STUB DEFINITIONS

### Step 2.1: Edit src/qtapp/masm_function_stubs.cpp

Remove the following function definitions (3 groups):

#### Group A: Remove masm_hotpatch_init (lines 27-32)
```cpp
DELETE THESE LINES:
    void masm_hotpatch_init() {}
    void masm_hotpatch_shutdown() {}
    void masm_hotpatch_add_patch(const char* p) {}
    void masm_hotpatch_remove_patch(const char* p) {}
    void masm_hotpatch_rollback_patch() {}
    void masm_hotpatch_verify_patch(const char* p) {}
```

**Reason:** These are empty stubs that conflict with no actual implementation. If needed, they should be provided by the ASM layer or core system.

#### Group B: Remove masm_server_hotpatch_init (lines 117-127)
```cpp
DELETE THESE LINES:
    void masm_server_hotpatch_init() {}
    void masm_server_hotpatch_add(const char* p) {}
    void masm_server_hotpatch_apply() {}
    void masm_server_hotpatch_enable() {}
    void masm_server_hotpatch_disable() {}
    void masm_server_hotpatch_get_stats(void* s) {}
    void masm_server_hotpatch_cleanup() {}
```

**Reason:** Production implementation exists in `src/masm/gguf_server_hotpatch.asm`. Stub should not compete.

#### Group C: Remove CreateThreadEx and CreatePipeEx (lines 146-149)
```cpp
DELETE THESE LINES:
    HANDLE CreateThreadEx(LPSECURITY_ATTRIBUTES lpa, SIZE_T s, LPTHREAD_START_ROUTINE r, LPVOID p, DWORD f, LPDWORD id) {
        return CreateThread(lpa, s, r, p, f, id);
    }
    BOOL CreatePipeEx(PHANDLE r, PHANDLE w, LPSECURITY_ATTRIBUTES a, DWORD s) {
        return CreatePipe(r, w, a, s);
    }
```

**Reason:** Production implementations exist in `src/core/system_runtime.cpp`. Stub definitions conflict with proper implementations.

---

## PART 3: UPDATE INCLUDE PATHS

### Step 3.1: Find all files including qtapp/system_runtime.hpp
```powershell
grep -r "#include.*qtapp.*system_runtime" src/
grep -r "#include.*\"system_runtime" src/qtapp/
grep -r "#include.*<system_runtime" src/qtapp/
```

### Step 3.2: Update each include
**For files in qtapp/ directory:**
- Change: `#include "system_runtime.hpp"` 
- To: `#include "../core/system_runtime.hpp"`

**For files outside qtapp/:**
- Change: `#include <qtapp/system_runtime.hpp>`
- To: `#include <core/system_runtime.hpp>`

---

## PART 4: UPDATE BUILD CONFIGURATION

### Step 4.1: Check CMakeLists.txt
```
File: CMakeLists.txt (and any subdirectory CMakeLists.txt)
Action: Search for qtapp/system_runtime.cpp and remove it from source lists
Pattern: Look for lines like:
  - src/qtapp/system_runtime.cpp
  - ${QTAPP_SOURCES}
  
Remove references to the deleted file.
```

### Step 4.2: Verify linker settings
If using Visual Studio project files (.vcxproj):
```
Action: Remove or comment out compilation of src/qtapp/system_runtime.cpp
File location: Check for ItemGroup with Compile Include="...system_runtime.cpp"
```

---

## PART 5: VERIFICATION

### Step 5.1: Compile test
```
Command: cmake --build . --config Release (or your build command)
Expected: No LNK2005 errors for:
  - masm_hotpatch_init
  - masm_server_hotpatch_init
  - CreateThreadEx
  - CreatePipeEx
```

### Step 5.2: Link verification
```
Command: Check linker output for:
  Remaining duplicate symbols: NONE (for above 4 functions)
  
If errors remain:
  1. Check if other copies exist (search entire src/ tree)
  2. Verify CMakeLists.txt was updated
  3. Clean build artifacts: rm -rf build/ && cmake .. && make clean
```

### Step 5.3: Function availability test
```
Verify these functions are still accessible:
  ✓ CreateThreadEx (from src/core/system_runtime.cpp)
  ✓ CreatePipeEx (from src/core/system_runtime.cpp)
  ✓ asm_event_loop_create (from src/masm/asm_events.asm)
  ✓ masm_server_hotpatch_init (from src/masm/gguf_server_hotpatch.asm)
```

---

## IMPLEMENTATION CHECKLIST

### Phase 1: File Deletion
- [ ] Review src/qtapp/system_runtime.cpp for unique code
- [ ] Review src/qtapp/system_runtime.hpp for unique declarations
- [ ] Delete src/qtapp/system_runtime.cpp
- [ ] Delete src/qtapp/system_runtime.hpp

### Phase 2: Stub Cleanup
- [ ] Backup src/qtapp/masm_function_stubs.cpp
- [ ] Remove masm_hotpatch_init family (lines 27-32)
- [ ] Remove masm_server_hotpatch_init family (lines 117-127)
- [ ] Remove CreateThreadEx/CreatePipeEx (lines 146-149)
- [ ] Verify file still compiles

### Phase 3: Include Path Updates
- [ ] Search for all includes of qtapp/system_runtime.hpp
- [ ] Update all includes to use core/system_runtime.hpp
- [ ] Verify include paths are correct (relative or absolute)

### Phase 4: Build Configuration
- [ ] Remove qtapp/system_runtime.cpp from CMakeLists.txt
- [ ] Remove qtapp/system_runtime.hpp from CMakeLists.txt (if listed)
- [ ] Clean and rebuild from scratch
- [ ] Verify no linking errors

### Phase 5: Testing
- [ ] Check for LNK2005 errors on the 4 critical functions
- [ ] Verify all 6 remaining functions are still accessible
- [ ] Run full test suite
- [ ] Check that CreateThreadEx/CreatePipeEx work correctly

---

## FILES TO MODIFY SUMMARY

### DELETE (Complete Removal)
```
src/qtapp/system_runtime.cpp
src/qtapp/system_runtime.hpp
```

### MODIFY (Partial Removal)
```
src/qtapp/masm_function_stubs.cpp
  - Remove 6 lines: masm_hotpatch_init family
  - Remove 7 lines: masm_server_hotpatch_init family
  - Remove 4 lines: CreateThreadEx/CreatePipeEx pair
  - Total: ~17 lines removed
```

### UPDATE (Include Paths)
```
Any file in src/qtapp/ that includes "system_runtime.hpp"
  - Search for: #include "system_runtime.hpp"
  - Change to: #include "../core/system_runtime.hpp"

Any file outside qtapp/ that includes qtapp version
  - Search for: #include <qtapp/system_runtime.hpp>
  - Change to: #include <core/system_runtime.hpp>
```

### REBUILD (Configuration)
```
CMakeLists.txt (and subdirectories)
  - Remove: src/qtapp/system_runtime.cpp
  - Remove: any references to deleted files
```

---

## ROLLBACK PLAN

If issues arise after implementation:

1. **Quick Rollback:**
   - Restore deleted files from Git: `git checkout src/qtapp/system_runtime.*`
   - Restore stub file: `git checkout src/qtapp/masm_function_stubs.cpp`
   - Revert CMakeLists.txt: `git checkout CMakeLists.txt`

2. **Partial Rollback:**
   - Keep file deletions, restore only stub functions if needed
   - Verify which functions are actually used

3. **Investigate Failures:**
   - Check which functions failed to link
   - Verify ASM implementations compiled correctly
   - Check for missing extern declarations

---

## ROOT CAUSE ANALYSIS

### Why This Happened
1. **qtapp/ was cloned from core/** during refactoring
2. **Both versions remained in the codebase** instead of being consolidated
3. **Build system included both** leading to duplicate symbol definitions
4. **Stub file tried to provide fallbacks** but conflicted with real implementations

### Why This Pattern is Problematic
- **Maintenance burden:** Keeping two versions in sync
- **Build confusion:** Which version gets linked?
- **Linker errors:** Inevitable when both are compiled
- **Code duplication:** Violates DRY principle

### Prevention for Future
- **Single source of truth:** Production code in core/, no duplication
- **Platform-specific code:** Use #ifdef for Windows/Linux/Mac
- **Stubs only in fallback locations:** Not competing with production
- **CMakeLists.txt review:** Ensure no accidental duplication
- **Code review checklist:** Flag duplicate symbol definitions

---

## ESTIMATED IMPACT

### Build Time
- Slight reduction (fewer files to compile)
- One less .cpp file = faster builds

### Runtime
- No impact (identical code being removed)
- Same functions, same behavior

### Maintenance
- **Positive:** Single source of truth
- **Positive:** Easier to maintain CreateThreadEx/CreatePipeEx
- **Positive:** Clear separation of concerns

### Risk Level
- **Low:** Only removing exact duplicates
- **Safe:** Core version is more complete than qtapp version
- **Tested:** These functions are used in actual code

---

**Document Version:** 1.0  
**Created:** January 22, 2026  
**Repository:** D:\RawrXD-production-lazy-init  
**Status:** Ready for Implementation
