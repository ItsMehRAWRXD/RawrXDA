# CMakeLists Integration Guide for Stub Completion
**File**: CMakeLists.txt
**Action**: Add stub completion module
**Lines to modify**: ~15 (straightforward additions)

---

## Step 1: Locate MASM Sources Section

Find the line in CMakeLists.txt that lists MASM source files:

```cmake
# Around line 250-300, find:
set(MASM_SOURCES
    src/masm/final-ide/main_window_masm.asm
    src/masm/final-ide/ide_components.asm
    src/masm/final-ide/ide_pane_system.asm
    src/masm/final-ide/agent_chat_modes.asm
    # ... other files
)
```

## Step 2: Add Stub Completion Module

Insert this line after the existing MASM sources:

```cmake
set(MASM_SOURCES
    src/masm/final-ide/main_window_masm.asm
    src/masm/final-ide/ide_components.asm
    src/masm/final-ide/ide_pane_system.asm
    src/masm/final-ide/agent_chat_modes.asm
    src/masm/final-ide/masm_terminal_integration.asm
    src/masm/final-ide/masm_qt_bridge.asm
    src/masm/final-ide/masm_thread_coordinator.asm
    src/masm/final-ide/masm_memory_bridge.asm
    src/masm/final-ide/masm_io_reactor.asm
    src/masm/final-ide/masm_agent_integration.asm
    src/masm/final-ide/stub_completion_comprehensive.asm  # <-- ADD THIS LINE
    # ... other files
)
```

## Step 3: Verify MASM Compiler Configuration

Ensure the MASM compiler settings include:

```cmake
# Find lines around 150-200 with MASM configuration
if(MSVC)
    enable_language(ASM_MASM)
    set(CMAKE_ASM_MASM_COMPILER ml.exe)
    
    # Compiler flags for MASM
    set(CMAKE_ASM_MASM_FLAGS_INIT "/nologo /Zf")
    
    # Should already have these settings
    # If not, add them
endif()
```

## Step 4: Link to Main Executable

Ensure stub_completion_comprehensive.asm is linked to RawrXD-QtShell target:

Find the add_executable or target_sources line:

```cmake
# Around line 400-450, find:
target_sources(RawrXD-QtShell PRIVATE
    ${MASM_SOURCES}
    ${CPP_SOURCES}
    ${HEADER_FILES}
)

# This should automatically include your new MASM file
# since it's added to ${MASM_SOURCES}
```

## Step 5: Build and Test

After modifying CMakeLists.txt:

```powershell
# In project root
cd build
cmake ..
cmake --build . --config Release --target RawrXD-QtShell
```

## Expected Output

When build completes successfully:

```
Compiling MASM: src/masm/final-ide/stub_completion_comprehensive.asm
Linking RawrXD-QtShell...
Build successful!
Output: build/bin/Release/RawrXD-QtShell.exe
```

## Troubleshooting

### Error: "ml.exe not found"
**Cause**: MASM compiler path not in PATH
**Fix**: Ensure Visual Studio 2022 is installed or add ml.exe path to CMakeLists.txt:

```cmake
set(CMAKE_ASM_MASM_COMPILER "C:/Program Files (x86)/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/ml64.exe")
```

### Error: "unresolved external symbol"
**Cause**: Stub function not exported properly
**Fix**: Verify all PUBLIC declarations at top of stub_completion_comprehensive.asm:

```asm
PUBLIC StartAnimationTimer
PUBLIC UpdateAnimation
PUBLIC ui_create_mode_combo
PUBLIC ui_open_file_dialog
# ... all 51 functions
```

### Error: "symbol already defined"
**Cause**: Function defined in multiple files
**Fix**: Check that these functions don't exist in other files:
- ui_create_mode_combo (should only be in stub_completion_comprehensive.asm)
- ui_open_file_dialog (should only be in stub_completion_comprehensive.asm)
- ml_masm_get_tensor (should only be in stub_completion_comprehensive.asm)
- etc.

If they exist elsewhere, comment out the duplicate definitions.

---

## File Sizes After Build

Expected object file sizes:

```
stub_completion_comprehensive.obj  ~85 KB
Total RawrXD-QtShell.exe           ~1.55 MB (up from 1.49 MB)
```

---

## Verification Steps

After successful build:

1. **Check executable exists**:
   ```powershell
   Test-Path build/bin/Release/RawrXD-QtShell.exe
   ```

2. **Check file size reasonable**:
   ```powershell
   (Get-Item build/bin/Release/RawrXD-QtShell.exe).Length
   # Should be ~1.5 MB
   ```

3. **Run quick test**:
   ```powershell
   cd build/bin/Release
   ./RawrXD-QtShell.exe --help
   # Should display help or start normally
   ```

4. **Run full test suite**:
   ```powershell
   cd build
   cmake --build . --config Release --target self_test_gate
   # Should show all 274 tests passing
   ```

---

**Status**: Ready for integration into CMakeLists.txt
