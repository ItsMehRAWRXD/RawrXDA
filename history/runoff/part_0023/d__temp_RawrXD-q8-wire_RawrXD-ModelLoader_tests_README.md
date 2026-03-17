# GUI Component Test Suite

## Overview
Comprehensive test harness for all RawrXD GUI components with startup crash protection.

## Test Files Created

1. **`tests/test_gui_components.cpp`** - Main test harness (40+ tests)
2. **`tests/CMakeLists.txt`** - Test build configuration  
3. **`Run-GUI-Tests.ps1`** - PowerShell test runner
4. **`Run-Full-Validation.ps1`** - Complete validation suite
5. **`Quick-Test.ps1`** - Quick validation runner

## Running Tests

### Option 1: Full Validation (Recommended)
```powershell
.\Run-Full-Validation.ps1
```

Tests:
- ✅ Build validation
- ✅ Component tests (isolated)
- ✅ Runtime test (minimal mode)

### Option 2: Component Tests Only
```powershell
.\Run-GUI-Tests.ps1 -Build
```

### Option 3: Quick Test
```powershell
.\Quick-Test.ps1
```

## What Gets Tested

### Agentic Mode Switcher (7 tests)
- Component creation
- Default mode (ASK_MODE)
- Mode changes with signal emission  
- All three modes (Ask/Plan/Agent)
- Activity indicator/spinner
- Progress display

### Model Selector (6 tests)
- Component creation
- Default state (IDLE)
- Adding models
- Status transitions (IDLE → LOADING → LOADED → ERROR)
- All signals (modelSelected, loadNewModelRequested, etc.)
- Clear models

### AI Chat Panel (6 tests)
- Component creation
- User messages
- Assistant messages
- Streaming messages (token-by-token)
- Clear messages
- Context setting (code + file path)
- Signal emissions

### Command Palette (6 tests)
- Component creation
- Register single command
- Register 50 commands
- Show/hide
- Search functionality
- Signal emissions

### Activity Bar (2 tests)
- Component creation
- Fixed width (50px)

### Integration Tests (3 tests)
- Mode switcher + Model selector coexistence
- Chat panel + Command palette coexistence  
- All components together

### Stress Tests (3 tests)
- Rapid mode changes (100 iterations)
- Many messages (100 messages)
- Rapid show/hide (50 cycles)

### Memory Tests (2 tests)
- Create/destroy cycles (100 iterations)
- Signal connection cleanup

## Total: 35+ Tests

## Test Output

Results are written to:
- **Console**: Color-coded PASS/FAIL
- **`gui_component_tests.log`**: Detailed log file

## Environment Variables

For full GUI testing:
```powershell
# Don't set RAWRXD_MINIMAL_GUI or set to 0
$env:RAWRXD_MINIMAL_GUI = "0"  # Full IDE mode

# Enable GGUF server
$env:QTAPP_DISABLE_GGUF = "0"
```

For minimal testing:
```powershell
$env:RAWRXD_MINIMAL_GUI = "1"  # Minimal mode
$env:QTAPP_DISABLE_GGUF = "1"  # Disable server
```

## Startup Crash Protection

The test harness creates components in **isolation** without full MainWindow startup:
- ✅ No application crash affects test results
- ✅ Each component tested independently
- ✅ Memory leaks detected
- ✅ Signal/slot connections verified

## Build Requirements

- Qt 6.7.3 MSVC x64
- Visual Studio 2022
- CMake 3.20+
- Qt Test module

## Success Criteria

All tests must pass for 100% verification:
- Component creation
- Signal emissions
- UI responsiveness
- Memory management
- Integration compatibility

## CI/CD Integration

Add to CI pipeline:
```yaml
- name: Run GUI Tests
  run: |
    powershell -File Run-Full-Validation.ps1
```

Exit code 0 = all tests passed
Exit code 1 = one or more tests failed
