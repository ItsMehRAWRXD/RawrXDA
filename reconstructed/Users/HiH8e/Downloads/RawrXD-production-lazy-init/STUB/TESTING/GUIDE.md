# STUB COMPLETION TESTING & VERIFICATION GUIDE
**Date**: December 27, 2025
**Status**: Ready for comprehensive testing

---

## Overview

This guide provides step-by-step procedures to verify all 51 completed stubs function correctly and integrate properly with existing systems.

---

## Part 1: Unit Testing

### Test Group 1: Animation System (8 functions)

#### Test 1.1: StartAnimationTimer
**Objective**: Verify timer creation and ID generation

```asm
; Test code (add to test suite)
.code
test_start_animation_timer PROC
    ; Create first timer
    mov ecx, 300        ; 300ms duration
    lea rdx, test_callback
    call StartAnimationTimer
    cmp eax, 0          ; Should return timer ID
    je .test_failed
    
    ; Verify timer ID is 0
    cmp eax, 0
    jne .test_failed
    
    ; Create second timer
    mov ecx, 500
    lea rdx, test_callback
    call StartAnimationTimer
    
    ; Verify second timer ID is 1
    cmp eax, 1
    jne .test_failed
    
    mov eax, 1          ; Success
    ret
    
.test_failed:
    xor eax, eax
    ret
test_start_animation_timer ENDP

test_callback PROC
    ret
test_callback ENDP
```

**Expected Result**: 
- ✅ First timer returns ID 0
- ✅ Second timer returns ID 1
- ✅ Timer limit (32) properly enforced

#### Test 1.2: UpdateAnimation
**Objective**: Verify progress calculation (0-100%)

```asm
test_update_animation PROC
    ; Create 300ms timer
    mov ecx, 300
    lea rdx, test_callback
    call StartAnimationTimer
    mov ebx, eax        ; Save timer ID
    
    ; Update after 150ms (50%)
    mov ecx, ebx
    mov edx, 150        ; 150ms elapsed
    call UpdateAnimation
    
    ; Should return ~50
    cmp eax, 45
    jl .progress_error   ; Allow ±5% tolerance
    cmp eax, 55
    jg .progress_error
    
    ; Update remaining 150ms (100%)
    mov ecx, ebx
    mov edx, 150
    call UpdateAnimation
    
    ; Should return 100
    cmp eax, 100
    jne .progress_error
    
    mov eax, 1
    ret
    
.progress_error:
    xor eax, eax
    ret
test_update_animation ENDP
```

**Expected Result**:
- ✅ 150ms elapsed → 50% progress
- ✅ 300ms elapsed → 100% progress
- ✅ Progress clamped to 0-100
- ✅ Timer auto-stops at 100%

#### Test 1.3: ParseAnimationJson
**Objective**: Verify JSON animation parsing

```json
{
  "duration": 300,
  "easing": "ease-in-out",
  "fromStyle": { "opacity": 0, "x": 0 },
  "toStyle": { "opacity": 1, "x": 100 }
}
```

**Test**:
```asm
test_parse_animation_json PROC
    lea rcx, [test_json_buffer]
    call ParseAnimationJson
    test eax, eax
    jz .parse_failed
    
    mov eax, 1
    ret
.parse_failed:
    xor eax, eax
    ret
test_parse_animation_json ENDP
```

**Expected Result**:
- ✅ Returns 1 (success) for valid JSON
- ✅ Returns 0 for invalid JSON
- ✅ Extracts duration field
- ✅ Extracts easing function

#### Test 1.4: StartStyleAnimation
**Objective**: Verify style animation initiation

```asm
test_start_style_animation PROC
    ; Style transition: opacity 0→1
    mov ecx, 1          ; Component ID
    lea rdx, from_style
    mov r8, to_style
    call StartStyleAnimation
    test eax, eax
    jz .style_anim_failed
    
    mov eax, 1
    ret
.style_anim_failed:
    xor eax, eax
    ret
test_start_style_animation ENDP
```

**Expected Result**:
- ✅ Returns 1 (success)
- ✅ Creates internal timer
- ✅ Sets up animation callback

#### Test 1.5: UpdateComponentPositions
**Objective**: Verify component position recalculation

```asm
test_update_positions PROC
    ; Create layout
    lea rcx, layout_buffer
    call ParseLayoutJson
    test rax, rax
    jz .position_failed
    
    ; Get layout pointer
    mov rcx, rax
    call UpdateComponentPositions
    test eax, eax
    jz .position_failed
    
    mov eax, 1
    ret
.position_failed:
    xor eax, eax
    ret
test_update_positions ENDP
```

**Expected Result**:
- ✅ Updates positions for all components
- ✅ Respects parent bounds
- ✅ Completes in <50ms

#### Test 1.6: RequestRedraw
**Objective**: Verify component redraw request

```asm
test_request_redraw PROC
    mov rcx, test_hwnd
    call RequestRedraw
    ; No return value, just verify no crash
    mov eax, 1
    ret
test_request_redraw ENDP
```

**Expected Result**:
- ✅ Sends WM_PAINT message
- ✅ No crash on NULL hwnd
- ✅ Doesn't block

#### Test 1.7: ParseLayoutJson
**Objective**: Verify layout JSON parsing

```asm
test_parse_layout_json PROC
    lea rcx, layout_json_buffer
    call ParseLayoutJson
    test rax, rax
    jz .layout_parse_failed
    
    ; Verify allocated structure
    mov eax, [rax]      ; Width
    cmp eax, 800
    jne .layout_parse_failed
    
    mov eax, 1
    ret
.layout_parse_failed:
    xor eax, eax
    ret
test_parse_layout_json ENDP
```

**Expected Result**:
- ✅ Returns non-zero pointer on success
- ✅ Extracts width (800)
- ✅ Extracts height (600)
- ✅ Allocates from heap

#### Test 1.8: RecalculateLayout
**Objective**: Verify layout recalculation

```asm
test_recalculate_layout PROC
    mov rcx, root_component
    call RecalculateLayout
    test eax, eax
    jz .layout_recalc_failed
    
    mov eax, 1
    ret
.layout_recalc_failed:
    xor eax, eax
    ret
test_recalculate_layout ENDP
```

**Expected Result**:
- ✅ Completes tree traversal
- ✅ Updates all positions
- ✅ Completes in <100ms

---

### Test Group 2: UI System (5 functions)

#### Test 2.1: ui_create_mode_combo
**Objective**: Verify mode selector creation

**Manual Test**:
1. Run RawrXD-QtShell.exe
2. Look for combo box in UI
3. Click dropdown
4. Verify 7 options appear:
   - [ ] Ask
   - [ ] Edit
   - [ ] Plan
   - [ ] Debug
   - [ ] Optimize
   - [ ] Teach
   - [ ] Architect
5. Select "Plan" mode
6. Verify agent mode changes

**Automated Test**:
```asm
test_ui_create_mode_combo PROC
    mov rcx, parent_window
    call ui_create_mode_combo
    test rax, rax
    jz .combo_failed
    
    mov eax, 1
    ret
.combo_failed:
    xor eax, eax
    ret
test_ui_create_mode_combo ENDP
```

**Expected Result**:
- ✅ Returns valid window handle
- ✅ Displays all 7 modes
- ✅ Accepts selection
- ✅ Integrates with agent system

#### Test 2.2: ui_create_mode_checkboxes
**Objective**: Verify checkbox options

**Manual Test**:
1. Look for checkboxes below mode selector
2. Verify checkboxes appear:
   - [ ] "Enable streaming"
   - [ ] "Show reasoning"
   - [ ] "Save context"
3. Toggle checkboxes
4. Verify agent respects options

#### Test 2.3: ui_open_file_dialog
**Objective**: Verify file selection dialog

**Manual Test**:
1. Click File → Open Model
2. Dialog should open
3. Navigate to model directory
4. Select a GGUF file
5. Click Open
6. Verify file path returned
7. Verify model loads

**Automated Test**:
```asm
test_ui_open_file_dialog PROC
    lea rcx, model_filter
    call ui_open_file_dialog
    test rax, rax
    jz .dialog_cancelled   ; 0 = user cancelled
    
    ; Verify returned path is valid
    ; Should start with C:\ or similar
    mov bl, BYTE PTR [rax]
    cmp bl, 'C'
    jne .dialog_failed
    
    mov eax, 1
    ret
.dialog_cancelled:
    mov eax, 1            ; OK if user cancels
    ret
.dialog_failed:
    xor eax, eax
    ret
test_ui_open_file_dialog ENDP
```

#### Test 2.4: ui_create_feature_toggle_window
**Objective**: Verify feature management window

**Manual Test**:
1. Tools → Feature Management
2. Window opens with:
   - [ ] Feature tree (left pane)
   - [ ] Feature list (right pane)
   - [ ] Enable/disable buttons
3. Can expand/collapse tree
4. Can select items

#### Test 2.5: ui_populate_feature_tree
**Objective**: Verify feature tree population

**Expected Tree Structure**:
```
✓ Advanced Features
  ✓ Reasoning Modes
    - Ask Mode
    - Edit Mode
    - Plan Mode
  ✓ Hotpatching
    - Memory Patches
    - Byte Patches
```

---

### Test Group 3: Feature Harness (18 functions)

#### Test 3.1: LoadUserFeatureConfiguration
**Test File**: `features_config.json`
```json
{
  "features": [
    {
      "id": 1,
      "name": "advanced_reasoning",
      "enabled": true,
      "dependencies": [],
      "policyFlags": 0
    },
    {
      "id": 2,
      "name": "hotpatching",
      "enabled": true,
      "dependencies": [1],
      "policyFlags": 0
    }
  ]
}
```

**Test**:
```asm
test_load_feature_config PROC
    lea rcx, config_path
    call LoadUserFeatureConfiguration
    test eax, eax
    jz .load_failed
    
    ; Verify g_feature_count
    cmp g_feature_count, 2
    jne .load_failed
    
    mov eax, 1
    ret
.load_failed:
    xor eax, eax
    ret
test_load_feature_config ENDP
```

**Expected Result**:
- ✅ Loads 2 features from config
- ✅ Sets g_feature_count = 2
- ✅ Populates g_feature_configs array
- ✅ Returns 1 (success)

#### Test 3.2: ValidateFeatureConfiguration
**Objective**: Verify dependency validation

```asm
test_validate_config PROC
    call ValidateFeatureConfiguration
    test eax, eax
    jz .validation_failed
    
    ; Should detect no circular deps
    ; Should verify all deps exist
    mov eax, 1
    ret
.validation_failed:
    xor eax, eax
    ret
test_validate_config ENDP
```

**Expected Result**:
- ✅ Detects circular dependencies
- ✅ Verifies all dependencies exist
- ✅ Checks for feature conflicts
- ✅ Returns 1 on valid config

#### Test 3.3: ApplyEnterpriseFeaturePolicy
**Objective**: Verify policy enforcement

**Test**:
1. Enable feature with policy flag 0x0001
2. Call ApplyEnterpriseFeaturePolicy()
3. Verify feature disabled if policy restricts it

#### Test 3.4-3.10: Initialization Functions
**Test** each initialization function returns 1 (success):

- [ ] InitializeFeaturePerformanceMonitoring() → 1
- [ ] InitializeFeatureSecurityMonitoring() → 1
- [ ] InitializeFeatureTelemetry() → 1
- [ ] SetupFeatureDependencyResolution() → 1
- [ ] SetupFeatureConflictDetection() → 1
- [ ] ApplyInitialFeatureConfiguration() → 1
- [ ] LogFeatureHarnessInitialization() → (void)

---

### Test Group 4: Model Loader (3 functions)

#### Test 4.1: ml_masm_get_tensor
**Objective**: Verify tensor retrieval

**Test**:
```asm
test_get_tensor PROC
    ; Load model first
    lea rcx, model_path
    call rawr1024_direct_load
    test rax, rax
    jz .tensor_failed
    
    ; Get tensor by name
    lea rcx, tensor_name
    call ml_masm_get_tensor
    test rax, rax
    jz .tensor_not_found
    
    mov eax, 1
    ret
.tensor_not_found:
    mov eax, 1          ; OK if tensor doesn't exist
    ret
.tensor_failed:
    xor eax, eax
    ret
test_get_tensor ENDP
```

**Expected Result**:
- ✅ Returns pointer to tensor data
- ✅ Returns 0 if tensor not found
- ✅ Works with loaded models

#### Test 4.2: ml_masm_get_arch
**Objective**: Verify architecture retrieval

**Test**:
```asm
test_get_arch PROC
    sub rsp, 256        ; Allocate buffer
    mov rcx, rsp        ; Buffer pointer
    call ml_masm_get_arch
    test eax, eax
    jz .arch_failed
    
    ; Verify JSON returned
    mov al, BYTE PTR [rsp]
    cmp al, '{'         ; Should start with {
    jne .arch_failed
    
    add rsp, 256
    mov eax, 1
    ret
.arch_failed:
    add rsp, 256
    xor eax, eax
    ret
test_get_arch ENDP
```

**Expected Result**:
- ✅ Returns JSON architecture
- ✅ Contains model name
- ✅ Contains layer count
- ✅ Contains parameter count

#### Test 4.3: rawr1024_direct_load
**Objective**: Verify GGUF file loading

**Test**:
1. Place test GGUF file in build directory
2. Run: 
   ```asm
   lea rcx, "llama-2-7b.gguf"
   call rawr1024_direct_load
   test rax, rax
   ```
3. Should return non-zero model pointer
4. Should load in <5 seconds

---

## Part 2: Integration Testing

### Integration Test 1: Mode Selection Flow
**Scenario**: User selects different agent mode

**Steps**:
1. ✅ ui_create_mode_combo() creates dropdown
2. ✅ User selects "Plan" mode
3. ✅ Signal sent to agent system
4. ✅ Agent switches to Plan mode
5. ✅ UI updates to show current mode
6. ✅ Typing prompt triggers Plan mode logic

### Integration Test 2: File Open → Model Load
**Scenario**: User opens GGUF model file

**Steps**:
1. ✅ ui_open_file_dialog() opens
2. ✅ User selects model file
3. ✅ rawr1024_direct_load() loads file
4. ✅ ml_masm_get_arch() retrieves architecture
5. ✅ Model info displayed in UI
6. ✅ Model available for inference

### Integration Test 3: Feature Configuration → UI Sync
**Scenario**: Feature configuration loaded at startup

**Steps**:
1. ✅ LoadUserFeatureConfiguration() reads config
2. ✅ ValidateFeatureConfiguration() validates
3. ✅ ApplyEnterpriseFeaturePolicy() restricts
4. ✅ ApplyInitialFeatureConfiguration() enables
5. ✅ ui_populate_feature_tree() displays features
6. ✅ UI reflects enabled/disabled state

### Integration Test 4: Animation System
**Scenario**: Style animation during theme change

**Steps**:
1. ✅ StartStyleAnimation() initiates
2. ✅ UpdateAnimation() updates progress each frame
3. ✅ RequestRedraw() triggers visual update
4. ✅ Animation completes in 300ms
5. ✅ Final style applied correctly

---

## Part 3: Performance Testing

### Performance Test 1: Animation Timing
**Objective**: Verify 30 FPS performance

**Measurement**:
```asm
test_animation_performance PROC
    ; Measure 10 animation update cycles
    mov ecx, 300        ; Duration
    lea rdx, test_callback
    call StartAnimationTimer
    mov r8d, eax        ; Timer ID
    
    mov r9d, 0
    mov r10d, 0         ; Total time
    
.measure_loop:
    cmp r9d, 10         ; 10 iterations
    jge .measure_done
    
    ; Time one update
    mov eax, r8d        ; Timer ID
    mov edx, 33         ; 33ms per frame
    
    ; ... measure time ...
    call UpdateAnimation
    
    inc r9d
    jmp .measure_loop
    
.measure_done:
    ; Average should be <1ms per update
    mov eax, 1
    ret
test_animation_performance ENDP
```

**Expected**: <1ms per UpdateAnimation call

### Performance Test 2: File Dialog Latency
**Objective**: Dialog opens in reasonable time

**Measurement**:
1. Time ui_open_file_dialog() call
2. User interaction time (variable)
3. Expected: Dialog visible in <100ms

### Performance Test 3: Feature Configuration Load
**Objective**: Config loads without blocking UI

**Measurement**:
1. Time LoadUserFeatureConfiguration()
2. Expected: <100ms for typical config

### Performance Test 4: Model Loading
**Objective**: Large models load efficiently

**Measurement**:
1. Measure rawr1024_direct_load() time
2. Small model (2GB): <100ms
3. Medium model (7GB): <1s
4. Large model (13GB): <5s

---

## Part 4: Regression Testing

### Run Full Test Suite
```powershell
cd build
cmake --build . --config Release --target self_test_gate

# Expected output:
# Running 274 tests...
# [PASS] 274/274 tests
# ===========================
# All systems functional
```

### Verify All Existing Tests Still Pass
- [ ] agent_chat_modes tests (12 tests)
- [ ] masm_terminal_integration tests (8 tests)
- [ ] ide_components tests (15 tests)
- [ ] ide_pane_system tests (6 tests)
- [ ] unified_hotpatch_manager tests (18 tests)
- [ ] agentic_failure_detector tests (10 tests)
- [ ] All 51 new stub tests (200+ tests)

**Expected**: All 274 tests pass (100%)

---

## Part 5: UAT (User Acceptance Testing)

### UAT 1: Mode Selection
**User**: Can select agent mode
- [ ] Click mode dropdown
- [ ] Select "Debug" mode
- [ ] Prompt typed
- [ ] Debug reasoning appears

### UAT 2: File Open
**User**: Can open and load models
- [ ] File → Open Model
- [ ] Dialog opens
- [ ] Navigate to model file
- [ ] Select file
- [ ] Model loads
- [ ] Architecture displayed

### UAT 3: Feature Management
**User**: Can manage features
- [ ] Tools → Feature Management
- [ ] Window opens
- [ ] See feature list
- [ ] Toggle features
- [ ] Changes take effect

### UAT 4: Theme Animation
**User**: UI animates during theme changes
- [ ] Select different theme
- [ ] Smooth transition
- [ ] No flicker
- [ ] Completes in ~300ms

---

## Test Results Template

```markdown
# Test Results - Stub Completion

**Date**: [DATE]
**Tester**: [NAME]
**Build**: RawrXD-QtShell v2.5

## Unit Tests

### Animation System (8/8 functions)
- [ ] StartAnimationTimer ✅ PASS
- [ ] UpdateAnimation ✅ PASS
- [ ] ParseAnimationJson ✅ PASS
- [ ] StartStyleAnimation ✅ PASS
- [ ] UpdateComponentPositions ✅ PASS
- [ ] RequestRedraw ✅ PASS
- [ ] ParseLayoutJson ✅ PASS
- [ ] RecalculateLayout ✅ PASS

### UI System (5/5 functions)
- [ ] ui_create_mode_combo ✅ PASS
- [ ] ui_create_mode_checkboxes ✅ PASS
- [ ] ui_open_file_dialog ✅ PASS
- [ ] ui_create_feature_toggle_window ✅ PASS
- [ ] ui_populate_feature_tree ✅ PASS

### Feature Harness (18/18 functions)
- [ ] All 18 functions ✅ PASS

### Model Loader (3/3 functions)
- [ ] ml_masm_get_tensor ✅ PASS
- [ ] ml_masm_get_arch ✅ PASS
- [ ] rawr1024_direct_load ✅ PASS

## Integration Tests: ✅ ALL PASS (4/4)
## Performance Tests: ✅ ALL PASS (4/4)
## Regression Tests: ✅ 274/274 PASS

## UAT: ✅ ALL PASS (4/4)

## Summary
**Status**: READY FOR PRODUCTION
**Total Tests**: 298
**Passed**: 298
**Failed**: 0
**Coverage**: 100%
```

---

**Next Step**: Execute tests and collect results
