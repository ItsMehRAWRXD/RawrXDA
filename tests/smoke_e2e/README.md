# RawrXD Chat Subsystem E2E Smoke Test Suite

## Overview

This directory contains comprehensive end-to-end smoke tests for the RawrXD Chat Subsystem integration into Win32IDE. The suite validates three critical architectural components:

1. **Deferred Initialization Verification** (`1_DeferredInitVerification.ps1`)
2. **Subclass Keyboard Trapping & GDI Overlay** (`2_SubclassKeyboardTrapping.ps1`)
3. **IPC Frame Constraining** (`3_IpcFrameConstraining.ps1`)

## Test Execution

### Quick Start

```powershell
# Run entire smoke test suite
.\0_SmokeTestCoordinator.ps1 -BinaryPath "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe" -Verbose
```

### Individual Scenario Tests

```powershell
# Test 1: Deferred initialization timing
.\1_DeferredInitVerification.ps1 -BinaryPath "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe" -Verbose

# Test 2: Keyboard subclass and GDI overlay
.\2_SubclassKeyboardTrapping.ps1 -BinaryPath "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe" -Verbose

# Test 3: IPC boundary validation
.\3_IpcFrameConstraining.ps1 -BinaryPath "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe" -Verbose
```

## Scenario Details

### Scenario 1: Deferred Initialization Verification

**Validates**: `WM_APP + 1002` asynchronous message routing

**Expected Behavior**:
- Primary editor window appears within 1500ms (responsive boot)
- Chat panel creation deferred to 2000-8000ms window
- Process remains responsive throughout boot sequence
- No synchronous hangs or blocking operations

**Success Criteria**:
- ✓ Primary window responsive (< 2000ms)
- ✓ Chat panel async deferred (2000-8000ms)
- ✓ Total boot < 15s
- ✓ Process never hangs

**Architecture Validated**:
```
WM_CREATE → Primary window (instant)
         ↓ (PostMessage WM_APP+1002)
         → Deferred chat panel creation
```

---

### Scenario 2: Subclass Keyboard Trapping & GDI Overlay

**Validates**: Win32 subclassing and GDI resource management

**Expected Behavior**:
- Tab key intercepted by `ChatInputSubclassProc`
- Ghost text rendered via `SaveDC`/`RestoreDC` without leaks
- Control focus handled correctly
- No unmatched GDI state saves

**Success Criteria**:
- ✓ Chat input HWND located and validated
- ✓ Tab key messages intercepted correctly
- ✓ GDI SaveDC/RestoreDC pairs balanced
- ✓ Extended stability test (5 iterations, no crashes)

**GDI Pairing Pattern**:
```
SaveDC(hdc) → [overlay render ops] → RestoreDC(hdc, saved_state)
```

**Resource Leak Prevention**:
- Every `SaveDC()` paired with exactly one `RestoreDC()`
- No DC leaks, no unmatched save/restore calls
- Process DC handle count stable before/after overlay

---

### Scenario 3: IPC Frame Constraining (>64KB Rejection)

**Validates**: `CChatIpcDelegation` security boundary enforcement

**Expected Behavior**:
- Payloads ≤ 64KB forwarded normally
- Payloads > 64KB rejected with `[RAWR_IPC_SECURITY]` marker
- Zero-allocation rejection (no heap corruption)
- Graceful failure without process crash

**Success Criteria**:
- ✓ Valid payload (16KB) passes
- ✓ Boundary payload (64KB-14) passes
- ✓ Oversized payload (65KB) rejected
- ✓ No buffer overflow vectors
- ✓ Memory safety gates active

**Frame Structure**:
```
[Magic(4) | Length(4) | Type(1) | Flags(1) | Pad(4) | Body(...) | CRC32(4)]
                                      ↑
                         Frame total must be ≤ 65536 bytes
```

**Security Boundary**:
- Max payload = 65536 - 14 (header) - 4 (CRC) = 65518 bytes
- Any payload > 65518 triggers rejection
- Backend isolation maintained

---

## Log Output

Test logs are written to `d:\rxdn_smoke_tests\logs\`:

- `scenario1_deferredinit.log` — Deferred init timing and boot phases
- `scenario2_subclass.log` — Keyboard interception and GDI operations
- `scenario3_ipc_frame.log` — IPC boundary validation results

Example log entry:
```
[14:23:45.123] [INFO]  Boot started at: 14:23:45.123
[14:23:45.456] [SUCCESS] Primary window created at 333ms
[14:23:47.890] [DEBUG]  Chat panel detection window active at 2767ms
[14:23:47.892] [SUCCESS] SCENARIO 1 PASSED ✓
```

---

## Interpreting Results

### All Scenarios PASS ✓

**Meaning**: Chat subsystem integration is production-ready.

**Next Step**: Proceed to full integration testing and performance benchmarking.

### One or More Scenarios FAIL ✗

**Troubleshooting**:
1. Review the detailed log file in `d:\rxdn_smoke_tests\logs\`
2. Check for specific error messages (`[✗]` markers)
3. Verify binary is recent build (compare timestamp)
4. Ensure no other RawrXD processes running (port conflicts)

**Common Issues**:
- **Deferred init timeout**: Primary window not appearing → check WM_CREATE handler
- **Subclass failures**: HWND lookup issues → verify chat panel creation fires
- **IPC boundary false positives**: Frame size calculation errors → validate CRC32 math

---

## Architecture Checkpoints

This test suite validates the following architectural decisions:

| Checkpoint | Test | Status |
|-----------|------|--------|
| **Message Loop Exclusivity** | Scenario 1 | ✓ No synchronous `createChatPanel()` in layout paths |
| **Subclass Safety Boundary** | Scenario 2 | ✓ Deferred HWND lifecycle prevents race conditions |
| **IPC Bridge Readiness** | Scenario 3 | ✓ Named Pipe loop awaits configuration frames |
| **GDI Resource Safety** | Scenario 2 | ✓ SaveDC/RestoreDC balanced pairing |
| **Zero-Allocation Security** | Scenario 3 | ✓ No heap allocation on rejection path |

---

## Performance Benchmarks

Expected timing for reference builds:

```
Boot Timeline (Deferred Init):
├─ WM_CREATE → Primary window: 300-500ms
├─ PostMessage WM_APP+1002: immediate
├─ Deferred loop → createChatPanel: 2000-4000ms
└─ Total boot: <6000ms (6 seconds)

GDI Operations (Subclass Trapping):
├─ Tab key intercept: <1ms
├─ SaveDC/RestoreDC cycle: <2ms
└─ Ghost text render: <5ms per frame

IPC Validation (Frame Constraining):
├─ Valid payload parse: <0.5ms
├─ Oversized rejection: <0.1ms (no alloc)
└─ CRC32 compute: <1ms for 64KB frame
```

---

## Development Notes

### Adding New Smoke Tests

To add a new test scenario:

1. Create `N_ScenarioName.ps1` in this directory
2. Follow the log/level pattern from existing tests
3. Return exit code 0 (pass) or non-zero (fail)
4. Add entry to `0_SmokeTestCoordinator.ps1`

### Continuous Integration

These tests are designed to be CI-friendly:

```yaml
# GitHub Actions example
- name: E2E Smoke Tests
  run: |
    cd d:\rxdn_smoke_tests
    .\0_SmokeTestCoordinator.ps1 -BinaryPath "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe"
  timeout-minutes: 5
```

---

## Success Criteria Summary

The chat subsystem integration is **production-ready** when:

- ✓ **Scenario 1**: Deferred initialization verified (responsive boot)
- ✓ **Scenario 2**: Subclass trapping and GDI pairing validated
- ✓ **Scenario 3**: IPC security boundary enforced (no overflows)
- ✓ **No process crashes** during any scenario
- ✓ **All logs clean** (no unexpected error markers)

---

**Test Suite Version**: 1.0  
**Last Updated**: May 16, 2026  
**Validated Against**: RawrXD-Win32IDE chat integration (Phase 1 completion)
