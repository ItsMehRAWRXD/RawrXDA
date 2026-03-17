# Phase 4 Implementation Code Examples & Patterns

## Table of Contents
1. Control Loading Pattern
2. Control Saving Pattern  
3. Registry Operations
4. Validation Patterns
5. Tab Management
6. Error Handling
7. Memory Management

---

## 1. Control Loading Pattern

### LoadSettingsToUI Implementation Example

```asm
; Load checkbox from settings data
; Pattern: movzx to convert BYTE → DWORD for SetDlgItemInt

; Load auto_save checkbox
mov rcx, dialog_hwnd              ; hwnd parameter (rcx)
mov edx, IDC_AUTO_SAVE            ; control ID (rdx as 32-bit)
movzx r8d, [rsi+SETTINGS_DATA.auto_save_enabled]  ; BYTE → DWORD in r8d
call CheckDlgButton               ; Windows API call
```

### LoadSettingsToUI Full Example

```asm
LoadSettingsToUI PROC
    push rbx
    push rsi
    push rdi
    mov rbx, rcx  ; settings_dialog
    
    ; Get settings data with null check
    mov rsi, [rbx+SETTINGS_DIALOG.settings_data]
    test rsi, rsi
    jz load_defaults
    
    mov rdi, [rbx+SETTINGS_DIALOG.hwnd]  ; dialog hwnd
    
    ; --- Load checkbox controls ---
    mov rcx, rdi
    mov edx, IDC_AUTO_SAVE
    movzx r8d, [rsi+SETTINGS_DATA.auto_save_enabled]
    call CheckDlgButton
    
    ; --- Load numeric controls ---
    mov rcx, rdi
    mov edx, IDC_FONT_SIZE
    mov r8d, [rsi+SETTINGS_DATA.font_size]
    xor r9, r9  ; unsigned
    call SetDlgItemInt
    
    ; --- Load text controls (with null check) ---
    mov rcx, rdi
    mov edx, IDC_MODEL_PATH
    mov r8, [rsi+SETTINGS_DATA.model_path]
    test r8, r8         ; Check for NULL pointer
    jz skip_model_path
    call SetDlgItemTextW
skip_model_path:
    
    jmp load_done
    
load_defaults:
    ; Set sensible defaults if no data
    mov rdi, [rbx+SETTINGS_DIALOG.hwnd]
    mov rcx, rdi
    mov edx, IDC_AUTO_SAVE
    mov r8d, 1          ; Default: enabled
    call CheckDlgButton
    
load_done:
    pop rdi
    pop rsi
    pop rbx
    mov rax, 1
    ret
LoadSettingsToUI ENDP
```

---

## 2. Control Saving Pattern

### SaveSettingsFromUI Implementation Example

```asm
; Read checkbox from dialog
; Pattern: IsDlgButtonChecked returns 0 or 1 in eax

mov rcx, dialog_hwnd              ; hwnd
mov edx, IDC_AUTO_SAVE            ; control ID
call IsDlgButtonChecked           ; Returns eax = 0 or 1
mov [rsi+SETTINGS_DATA.auto_save_enabled], al  ; Store as BYTE
```

### SaveSettingsFromUI Full Example

```asm
SaveSettingsFromUI PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rbx, rcx  ; settings_dialog
    mov rsi, [rbx+SETTINGS_DIALOG.settings_data]
    test rsi, rsi
    jz save_complete
    
    mov rdi, [rbx+SETTINGS_DIALOG.hwnd]
    
    ; --- Save checkbox controls ---
    mov rcx, rdi
    mov edx, IDC_AUTO_SAVE
    call IsDlgButtonChecked      ; eax = 0 or 1
    mov [rsi+SETTINGS_DATA.auto_save_enabled], al
    
    ; --- Save numeric controls ---
    mov rcx, rdi
    mov edx, IDC_FONT_SIZE
    lea r8, [rsp+18h]            ; address for result flag
    xor r9, r9                   ; unsigned
    call GetDlgItemInt           ; eax = value
    mov [rsi+SETTINGS_DATA.font_size], eax
    
    ; --- Save to registry ---
    mov rcx, rsi
    call SaveSettingsToRegistry
    
    ; Mark as clean
    mov [rbx+SETTINGS_DIALOG.is_dirty], 0
    
save_complete:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    mov rax, 1
    ret
SaveSettingsFromUI ENDP
```

---

## 3. Registry Operations

### Registry Get Pattern

```asm
; Get DWORD with default value
mov rcx, registry_key_handle      ; Key handle
lea rdx, REG_FONT_SIZE            ; Value name (wide string)
mov r8d, 10                       ; Default value if not found
call RegistryGetDWORD             ; Returns eax = value
mov [rsi+SETTINGS_DATA.font_size], eax

; Get String value
mov rcx, registry_key_handle
lea rdx, REG_MODEL_PATH_VAL
xor r8, r8                        ; No default for strings
mov r9, [rbx+SETTINGS_DATA.model_path]  ; Output buffer
call RegistryGetString
```

### Registry Set Pattern

```asm
; Set DWORD value
mov rcx, registry_key_handle
lea rdx, REG_FONT_SIZE
mov r8d, [rsi+SETTINGS_DATA.font_size]
call RegistrySetDWORD

; Set string value (with null check)
mov rcx, registry_key_handle
lea rdx, REG_MODEL_PATH_VAL
mov r8, [rsi+SETTINGS_DATA.model_path]
test r8, r8           ; Check for NULL pointer
jz skip_save
call RegistrySetString
skip_save:
```

### Full Registry Implementation

```asm
LoadSettingsFromRegistry PROC
    push rbx
    push rsi
    mov rbx, rcx        ; settings_data
    
    ; Open registry key
    lea rcx, REG_PATH_GENERAL
    call RegistryOpenKey
    mov rsi, rax        ; registry key handle
    test rsi, rsi
    jz load_error
    
    ; Load General settings
    mov rcx, rsi
    lea rdx, REG_AUTO_SAVE
    mov r8d, 1          ; default = TRUE
    call RegistryGetDWORD
    mov [rbx+SETTINGS_DATA.auto_save_enabled], al
    
    mov rcx, rsi
    lea rdx, REG_STARTUP_FULLSCREEN
    mov r8d, 0          ; default = FALSE
    call RegistryGetDWORD
    mov [rbx+SETTINGS_DATA.startup_fullscreen], al
    
    mov rcx, rsi
    lea rdx, REG_FONT_SIZE
    mov r8d, 10         ; default = 10
    call RegistryGetDWORD
    mov [rbx+SETTINGS_DATA.font_size], eax
    
    ; Close registry key
    mov rcx, rsi
    call RegistryCloseKey
    
    mov rax, 1
    jmp load_done
    
load_error:
    mov rax, 0
    
load_done:
    pop rsi
    pop rbx
    ret
LoadSettingsFromRegistry ENDP
```

---

## 4. Validation Patterns

### Range Validation Pattern

```asm
; Validate font size (8-72)
mov rcx, dialog_hwnd
mov edx, IDC_FONT_SIZE
lea r8, [rsp+18h]      ; address for result
xor r9, r9
call GetDlgItemInt     ; eax = value

cmp eax, 8             ; Check minimum
jl font_invalid        ; Jump if less than 8
cmp eax, 72            ; Check maximum
jg font_invalid        ; Jump if greater than 72
jmp next_validation    ; Value is valid

font_invalid:
    ; Correct to default
    mov rcx, dialog_hwnd
    mov edx, IDC_FONT_SIZE
    mov r8d, 10        ; Default value
    xor r9, r9
    call SetDlgItemInt
```

### Full Validation Implementation

```asm
HandleControlChange PROC
    push rbx
    push rsi
    sub rsp, 20h
    
    mov rbx, rcx  ; dialog_hwnd
    mov esi, edx  ; control_id
    
    ; Dispatch validation based on control ID
    cmp esi, IDC_FONT_SIZE
    je validate_font_size
    cmp esi, IDC_TEMPERATURE
    je validate_temperature
    cmp esi, IDC_MAX_TOKENS
    je validate_tokens
    jmp validation_done
    
validate_font_size:
    mov rcx, rbx
    mov edx, IDC_FONT_SIZE
    lea r8, [rsp+18h]
    xor r9, r9
    call GetDlgItemInt
    
    cmp eax, 8
    jl font_invalid
    cmp eax, 72
    jg font_invalid
    jmp validation_done
    
font_invalid:
    mov rcx, rbx
    mov edx, IDC_FONT_SIZE
    mov r8d, 10
    xor r9, r9
    call SetDlgItemInt
    
    ; Similar patterns for temperature and tokens...
    
validation_done:
    add rsp, 20h
    pop rsi
    pop rbx
    mov rax, 1
    ret
HandleControlChange ENDP
```

---

## 5. Tab Management Pattern

### Tab Switching Implementation

```asm
OnTabSelectionChanged PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rbx, rcx  ; tab_control
    mov esi, edx  ; new_page_index
    
    ; --- Hide all pages ---
    xor edi, edi
hide_all_pages:
    mov eax, [rbx+TAB_CONTROL.page_count]
    cmp edi, eax
    jge show_active_page
    
    ; Get page pointer from array
    mov rax, [rbx+TAB_CONTROL.pages]
    test rax, rax
    jz show_active_page
    
    mov r8, [rax+rdi*8]  ; Get page at index edi
    test r8, r8
    jz skip_hide
    
    ; Get page HWND
    mov rcx, [r8+TAB_PAGE.hwnd]
    test rcx, rcx
    jz skip_hide
    
    ; Hide window
    mov rdx, SW_HIDE     ; 0
    call ShowWindow
    
skip_hide:
    inc edi
    jmp hide_all_pages
    
    ; --- Show active page ---
show_active_page:
    mov rax, [rbx+TAB_CONTROL.pages]
    mov r8, [rax+rsi*8]  ; Get page at active index
    test r8, r8
    jz switching_done
    
    mov rcx, [r8+TAB_PAGE.hwnd]
    test rcx, rcx
    jz switching_done
    
    ; Show window
    mov rdx, SW_SHOW     ; 5
    call ShowWindow
    
    ; Update active page index
    mov [rbx+TAB_CONTROL.active_page], esi
    
switching_done:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    mov rax, 1
    ret
OnTabSelectionChanged ENDP
```

---

## 6. Error Handling Pattern

### Null Pointer Checks

```asm
; Check settings_data pointer
mov rsi, [rbx+SETTINGS_DIALOG.settings_data]
test rsi, rsi
jz error_no_data        ; Jump if NULL

; Check string pointer before setting text
mov r8, [rsi+SETTINGS_DATA.model_path]
test r8, r8
jz skip_set_text        ; Jump if NULL
call SetDlgItemTextW
skip_set_text:

; Check return value from registry operation
mov rcx, registry_key
call RegistryOpenKey
mov rsi, rax
test rsi, rsi
jz registry_error       ; Jump if NULL (failed to open)
```

### Graceful Degradation

```asm
; If registry unavailable, use defaults
mov rsi, [rsi+SETTINGS_DIALOG.settings_data]
test rsi, rsi
jz use_defaults

; Try to load from registry
mov rcx, rsi
call LoadSettingsFromRegistry
test rax, rax
jz use_defaults        ; If load failed, use defaults

jmp load_complete

use_defaults:
    ; Set sensible default values
    mov [rsi+SETTINGS_DATA.auto_save_enabled], 1
    mov [rsi+SETTINGS_DATA.font_size], 10
    mov [rsi+SETTINGS_DATA.temperature], 100
    mov [rsi+SETTINGS_DATA.max_tokens], 2048

load_complete:
```

---

## 7. Memory Management Pattern

### Stack Frame Management

```asm
; Allocate space for local variables
sub rsp, 20h            ; Allocate 32 bytes (shadow space + locals)

; Use allocated space
mov [rsp+18h], rbx      ; Save at offset 18h
lea r8, [rsp+10h]       ; Get address of local variable

; Code using locals...

; Deallocate before return
add rsp, 20h
ret
```

### Register Preservation

```asm
; Save non-volatile registers used
push rbx
push rsi
push rdi
push r12

; ... Function code ...

; Restore in reverse order
pop r12
pop rdi
pop rsi
pop rbx
ret
```

### Parameter Passing Pattern

```asm
; x64 calling convention (Windows)
; rcx = first parameter (hwnd)
; rdx = second parameter (control_id)
; r8  = third parameter (value or pointer)
; r9  = fourth parameter
; [rsp+28h] onwards = additional parameters (stack)

MyFunction PROC
    ; rcx is first param
    mov rbx, rcx        ; Save rcx if needed
    
    ; rdx is second param
    cmp edx, expected_value
    je process
    
    ; Additional params on stack
    mov rax, [rsp+28h]  ; Fifth parameter
    mov rbx, [rsp+30h]  ; Sixth parameter
    
    ; Call another function
    ; Setup parameters for callee:
    mov rcx, param1     ; First parameter
    mov rdx, param2     ; Second parameter
    mov r8, param3      ; Third parameter
    mov r9, param4      ; Fourth parameter
    mov [rsp+28h], param5  ; Fifth on stack (must maintain 32-byte shadow)
    call AnotherFunction
    
    ret
MyFunction ENDP
```

---

## Common Windows API Patterns

### Dialog Control Operations

```asm
; Template: CheckDlgButton(HWND hwnd, int nIDDlgItem, UINT uCheck)
; Parameters: rcx=hwnd, rdx=id, r8=check (0 or 1)
mov rcx, dialog_hwnd
mov edx, IDC_CHECKBOX
mov r8d, 1        ; Checked
call CheckDlgButton

; Template: IsDlgButtonChecked(HWND hwnd, int nIDDlgItem)
; Parameters: rcx=hwnd, rdx=id
; Returns: eax = 0 (unchecked) or 1 (checked)
mov rcx, dialog_hwnd
mov edx, IDC_CHECKBOX
call IsDlgButtonChecked
; eax now contains result
test eax, eax
jz checkbox_unchecked
```

### Control Creation

```asm
; Template: CreateWindowExW(...)
; This is complex - encapsulate in helper functions
; Example: CreateCheckbox PROC (see qt6_settings_dialog.asm)

CreateCheckbox PROC
    sub rsp, 68h
    ; Setup parameters for CreateWindowExW
    mov rax, [rsp+90h]  ; id parameter
    mov [rsp+58h], rax  ; lpParam
    ; ... more setup ...
    lea rdx, WC_BUTTON_W
    mov r9d, WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX
    xor rcx, rcx        ; dwExStyle
    call CreateWindowExW
    add rsp, 68h
    ret
CreateCheckbox ENDP
```

---

## Best Practices Summary

### 1. **Always Null-Check Pointers**
```asm
mov rax, [rsi+offset]
test rax, rax
jz handle_null
```

### 2. **Use Stack-Based Local Variables**
```asm
sub rsp, 20h
lea r8, [rsp+10h]  ; Get address of local
call Function
add rsp, 20h
```

### 3. **Preserve Non-Volatile Registers**
```asm
push rbx    ; Save if using
mov rbx, rcx
; ... code ...
pop rbx     ; Restore
```

### 4. **Validate Return Values**
```asm
call FunctionThatReturns
test rax, rax
jz error_handler
```

### 5. **Use Conditional Jumps for Validation**
```asm
cmp eax, minimum
jl invalid
cmp eax, maximum
jg invalid
; Valid value, continue
```

### 6. **Type Conversion When Needed**
```asm
movzx r8d, [rsi]   ; BYTE → DWORD (zero-extend)
mov r8d, [rsi]     ; Already DWORD
```

---

## Performance Considerations

### O(1) Operations
- All checkbox/numeric control reads/writes
- All registry value operations
- Validation range checks

### O(n) Operations
- LoadSettingsToUI: n = number of controls (~13)
- SaveSettingsFromUI: n = number of controls (~13)
- OnTabSelectionChanged: n = number of tabs (7)
- LoadSettingsFromRegistry: n = number of fields (12)
- SaveSettingsToRegistry: n = number of fields (12)

### Optimization Strategies
1. Minimize function calls in tight loops
2. Use registers instead of stack variables when possible
3. Avoid redundant null checks
4. Cache frequently accessed pointers

---

## Testing Examples

### Unit Test for LoadSettingsToUI
```asm
; Test 1: Load valid data
mov rcx, dialog_pointer
mov [rcx+SETTINGS_DIALOG.settings_data], settings_data_ptr
call LoadSettingsToUI
; Verify: Check that controls are populated with settings_data values

; Test 2: Load with NULL data
mov rcx, dialog_pointer
mov [rcx+SETTINGS_DIALOG.settings_data], 0  ; NULL
call LoadSettingsToUI
; Verify: Check that controls have default values
```

### Unit Test for Validation
```asm
; Test 1: Font size = 5 (invalid)
mov rcx, dialog_hwnd
mov edx, IDC_FONT_SIZE
mov r8d, 5
xor r9, r9
call SetDlgItemInt
mov rcx, dialog_hwnd
mov edx, IDC_FONT_SIZE
call HandleControlChange
; Verify: Font size should be corrected to 10

; Test 2: Font size = 50 (valid)
mov rcx, dialog_hwnd
mov edx, IDC_FONT_SIZE
mov r8d, 50
xor r9, r9
call SetDlgItemInt
call HandleControlChange
; Verify: Font size should remain 50
```

---

## Conclusion

These patterns form the foundation of the Phase 4 implementation. They represent best practices for:
- Dialog control management
- Registry persistence
- Input validation
- Tab management
- Memory safety
- Error handling
- Performance optimization

All implementations in qt6_settings_dialog.asm and registry_persistence.asm follow these patterns consistently.

---

*Last Updated: December 4, 2025*  
*Reference: Phase 4 Critical Items Implementation*
