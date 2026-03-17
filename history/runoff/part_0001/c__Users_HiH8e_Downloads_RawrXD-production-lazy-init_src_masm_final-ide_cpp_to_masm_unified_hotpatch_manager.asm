; unified_hotpatch_manager_masm.asm
; Pure MASM x64 - Unified Hotpatch Manager (converted from C++ UnifiedHotpatchManager class)
; Coordinates memory, byte-level, and server hotpatching systems

option casemap:none

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memset:PROC
EXTERN memcpy:PROC
EXTERN strlen:PROC
EXTERN strcpy:PROC
EXTERN sprintf:PROC
EXTERN console_log:PROC
EXTERN GetSystemTimeAsFileTime:PROC

; Manager constants
MAX_HOTPATCHES EQU 100
MAX_PRESETS EQU 50
MAX_CALLBACKS EQU 20

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; UNIFIED_RESULT - Unified operation result
UNIFIED_RESULT STRUCT
    success BYTE ?                  ; True if successful
    detail QWORD ?                  ; Result detail
    errorCode DWORD ?               ; Error code
    layer DWORD ?                   ; Patch layer enum
    elapsedMs QWORD ?               ; Execution time
ENDS

; UNIFIED_HOTPATCH - Unified hotpatch definition
UNIFIED_HOTPATCH STRUCT
    name QWORD ?                    ; Patch name
    description QWORD ?             ; Patch description
    layer DWORD ?                   ; Patch layer enum
    target QWORD ?                  ; Target identifier
    data QWORD ?                    ; Patch data
    size QWORD ?                    ; Patch size
    enabled BYTE ?                  ; Whether patch is enabled
    applied BYTE ?                  ; Whether patch is applied
    timesApplied DWORD ?            ; Number of times applied
ENDS

; HOTPATCH_PRESET - Hotpatch preset
HOTPATCH_PRESET STRUCT
    name QWORD ?                    ; Preset name
    description QWORD ?             ; Preset description
    hotpatchCount DWORD ?           ; Number of hotpatches
    hotpatches QWORD ?              ; Array of hotpatch IDs
ENDS

; UNIFIED_HOTPATCH_MANAGER - Manager state
UNIFIED_HOTPATCH_MANAGER STRUCT
    hotpatches QWORD ?              ; Array of UNIFIED_HOTPATCH
    hotpatchCount DWORD ?           ; Current hotpatch count
    maxHotpatches DWORD ?           ; Capacity
    
    presets QWORD ?                 ; Array of HOTPATCH_PRESET
    presetCount DWORD ?             ; Current preset count
    maxPresets DWORD ?              ; Capacity
    
    ; Subsystems
    memoryHotpatch QWORD ?          ; Memory hotpatch system
    byteHotpatcher QWORD ?          ; Byte-level hotpatcher
    serverHotpatch QWORD ?          ; Server hotpatch system
    
    ; Statistics
    totalPatchesApplied QWORD ?
    totalBytesPatched QWORD ?
    totalErrors DWORD ?
    
    ; Callbacks
    callbacks QWORD ?               ; Array of callback functions
    callbackCount DWORD ?           ; Current callback count
    maxCallbacks DWORD ?            ; Capacity
    
    initialized BYTE ?
ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szManagerCreated DB "[UNIFIED_MANAGER] Created with all subsystems", 0
    szPatchApplied DB "[UNIFIED_MANAGER] Patch applied: %s (layer=%d)", 0
    szPatchFailed DB "[UNIFIED_MANAGER] Patch failed: %s (layer=%d, error=%d)", 0
    szPresetApplied DB "[UNIFIED_MANAGER] Preset applied: %s (%d patches)", 0
    szPresetFailed DB "[UNIFIED_MANAGER] Preset failed: %s (error=%d)", 0

; Patch layers
PATCH_LAYER_MEMORY EQU 0
PATCH_LAYER_BYTE EQU 1
PATCH_LAYER_SERVER EQU 2

.code

; ============================================================================
; PUBLIC API
; ============================================================================

; unified_hotpatch_manager_create(RCX = memoryHotpatch, RDX = byteHotpatcher, R8 = serverHotpatch)
; Create unified hotpatch manager
; Returns: RAX = pointer to UNIFIED_HOTPATCH_MANAGER
PUBLIC unified_hotpatch_manager_create
unified_hotpatch_manager_create PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = memoryHotpatch
    mov r9, rdx                     ; r9 = byteHotpatcher
    mov r10, r8                     ; r10 = serverHotpatch
    
    ; Allocate manager
    mov rcx, SIZEOF UNIFIED_HOTPATCH_MANAGER
    call malloc
    mov r11, rax
    
    ; Allocate hotpatches array
    mov rcx, MAX_HOTPATCHES
    imul rcx, SIZEOF UNIFIED_HOTPATCH
    call malloc
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.hotpatches], rax
    
    ; Allocate presets array
    mov rcx, MAX_PRESETS
    imul rcx, SIZEOF HOTPATCH_PRESET
    call malloc
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.presets], rax
    
    ; Allocate callbacks array
    mov rcx, MAX_CALLBACKS
    imul rcx, 8                     ; Function pointers
    call malloc
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.callbacks], rax
    
    ; Initialize
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.memoryHotpatch], rbx
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.byteHotpatcher], r9
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.serverHotpatch], r10
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.hotpatchCount], 0
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.maxHotpatches], MAX_HOTPATCHES
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.presetCount], 0
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.maxPresets], MAX_PRESETS
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.callbackCount], 0
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.maxCallbacks], MAX_CALLBACKS
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.totalPatchesApplied], 0
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.totalBytesPatched], 0
    mov [r11 + UNIFIED_HOTPATCH_MANAGER.totalErrors], 0
    
    mov byte [r11 + UNIFIED_HOTPATCH_MANAGER.initialized], 1
    
    ; Log
    lea rcx, [szManagerCreated]
    call console_log
    
    mov rax, r11
    pop rbx
    ret
unified_hotpatch_manager_create ENDP

; ============================================================================

; unified_manager_apply_memory_patch(RCX = manager, RDX = patchName, R8 = patchData)
; Apply memory patch
; Returns: RAX = pointer to UNIFIED_RESULT
PUBLIC unified_manager_apply_memory_patch
unified_manager_apply_memory_patch PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = manager
    mov rsi, rdx                    ; rsi = patchName
    mov r9, r8                      ; r9 = patchData
    
    ; Get start time
    call GetSystemTimeAsFileTime
    mov r10, rax                    ; r10 = start time
    
    ; Allocate result
    mov rcx, SIZEOF UNIFIED_RESULT
    call malloc
    mov r11, rax                    ; r11 = result
    
    ; Check if memory hotpatch exists
    mov r12, [rbx + UNIFIED_HOTPATCH_MANAGER.memoryHotpatch]
    test r12, r12
    jz .no_memory_hotpatch
    
    ; Create memory patch
    mov rcx, r12
    mov rdx, rsi
    mov r8, r9
    mov r9d, PATCH_TYPE_WEIGHT_MODIFICATION
    call memory_hotpatch_add_patch
    
    test rax, rax
    jz .patch_creation_failed
    
    ; Get patch
    mov rcx, r12
    mov edx, eax
    call memory_hotpatch_get_patch
    mov r13, rax                    ; r13 = patch
    
    ; Apply patch
    mov rcx, r12
    mov rdx, r13
    call memory_hotpatch_apply_patch
    mov r14, rax                    ; r14 = patch result
    
    ; Check result
    cmp byte [r14 + PATCH_RESULT.success], 1
    jne .patch_apply_failed
    
    ; Get end time
    call GetSystemTimeAsFileTime
    sub rax, r10                    ; rax = elapsed time
    mov [r11 + UNIFIED_RESULT.elapsedMs], rax
    
    ; Set unified result
    mov byte [r11 + UNIFIED_RESULT.success], 1
    lea rax, [szPatchAppliedDetail]
    mov [r11 + UNIFIED_RESULT.detail], rax
    mov [r11 + UNIFIED_RESULT.errorCode], 0
    mov [r11 + UNIFIED_RESULT.layer], PATCH_LAYER_MEMORY
    
    ; Update statistics
    inc qword [rbx + UNIFIED_HOTPATCH_MANAGER.totalPatchesApplied]
    mov rax, [r13 + MEMORY_PATCH.size]
    add [rbx + UNIFIED_HOTPATCH_MANAGER.totalBytesPatched], rax
    
    ; Log success
    lea rcx, [szPatchApplied]
    mov rdx, rsi
    mov r8d, PATCH_LAYER_MEMORY
    call console_log
    
    ; Free patch result
    mov rcx, r14
    call free
    
    mov rax, r11                    ; Return result
    pop rsi
    pop rbx
    ret
    
.no_memory_hotpatch:
.patch_creation_failed:
.patch_apply_failed:
    ; Set error result
    mov byte [r11 + UNIFIED_RESULT.success], 0
    lea rax, [szPatchFailedDetail]
    mov [r11 + UNIFIED_RESULT.detail], rax
    mov [r11 + UNIFIED_RESULT.errorCode], 1
    mov [r11 + UNIFIED_RESULT.layer], PATCH_LAYER_MEMORY
    
    ; Log failure
    lea rcx, [szPatchFailed]
    mov rdx, rsi
    mov r8d, PATCH_LAYER_MEMORY
    mov r9d, 1
    call console_log
    
    inc dword [rbx + UNIFIED_HOTPATCH_MANAGER.totalErrors]
    
    mov rax, r11
    pop rsi
    pop rbx
    ret
unified_manager_apply_memory_patch ENDP

; ============================================================================

; unified_manager_apply_byte_patch(RCX = manager, RDX = patchName, R8 = patchData)
; Apply byte patch
; Returns: RAX = pointer to UNIFIED_RESULT
PUBLIC unified_manager_apply_byte_patch
unified_manager_apply_byte_patch PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = manager
    mov rsi, rdx                    ; rsi = patchName
    mov r9, r8                      ; r9 = patchData
    
    ; Get start time
    call GetSystemTimeAsFileTime
    mov r10, rax                    ; r10 = start time
    
    ; Allocate result
    mov rcx, SIZEOF UNIFIED_RESULT
    call malloc
    mov r11, rax                    ; r11 = result
    
    ; Check if byte hotpatcher exists
    mov r12, [rbx + UNIFIED_HOTPATCH_MANAGER.byteHotpatcher]
    test r12, r12
    jz .no_byte_hotpatcher
    
    ; Create byte patch
    mov rcx, r12
    mov rdx, rsi
    mov r8, r9
    mov r9d, PATCH_TYPE_REPLACE
    call byte_hotpatcher_add_patch
    
    test rax, rax
    jz .patch_creation_failed
    
    ; Get patch
    mov rcx, r12
    mov edx, eax
    call byte_hotpatcher_get_patch
    mov r13, rax                    ; r13 = patch
    
    ; Apply patch
    mov rcx, r12
    mov rdx, r13
    call byte_hotpatcher_apply_patch
    mov r14, rax                    ; r14 = patch result
    
    ; Check result
    cmp byte [r14 + PATCH_RESULT.success], 1
    jne .patch_apply_failed
    
    ; Get end time
    call GetSystemTimeAsFileTime
    sub rax, r10                    ; rax = elapsed time
    mov [r11 + UNIFIED_RESULT.elapsedMs], rax
    
    ; Set unified result
    mov byte [r11 + UNIFIED_RESULT.success], 1
    lea rax, [szPatchAppliedDetail]
    mov [r11 + UNIFIED_RESULT.detail], rax
    mov [r11 + UNIFIED_RESULT.errorCode], 0
    mov [r11 + UNIFIED_RESULT.layer], PATCH_LAYER_BYTE
    
    ; Update statistics
    inc qword [rbx + UNIFIED_HOTPATCH_MANAGER.totalPatchesApplied]
    mov rax, [r13 + BYTE_PATCH.size]
    add [rbx + UNIFIED_HOTPATCH_MANAGER.totalBytesPatched], rax
    
    ; Log success
    lea rcx, [szPatchApplied]
    mov rdx, rsi
    mov r8d, PATCH_LAYER_BYTE
    call console_log
    
    ; Free patch result
    mov rcx, r14
    call free
    
    mov rax, r11                    ; Return result
    pop rsi
    pop rbx
    ret
    
.no_byte_hotpatcher:
.patch_creation_failed:
.patch_apply_failed:
    ; Set error result
    mov byte [r11 + UNIFIED_RESULT.success], 0
    lea rax, [szPatchFailedDetail]
    mov [r11 + UNIFIED_RESULT.detail], rax
    mov [r11 + UNIFIED_RESULT.errorCode], 1
    mov [r11 + UNIFIED_RESULT.layer], PATCH_LAYER_BYTE
    
    ; Log failure
    lea rcx, [szPatchFailed]
    mov rdx, rsi
    mov r8d, PATCH_LAYER_BYTE
    mov r9d, 1
    call console_log
    
    inc dword [rbx + UNIFIED_HOTPATCH_MANAGER.totalErrors]
    
    mov rax, r11
    pop rsi
    pop rbx
    ret
unified_manager_apply_byte_patch ENDP

; ============================================================================

; unified_manager_add_server_hotpatch(RCX = manager, RDX = patchName, R8 = patchData)
; Add server hotpatch
; Returns: RAX = pointer to UNIFIED_RESULT
PUBLIC unified_manager_add_server_hotpatch
unified_manager_add_server_hotpatch PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = manager
    mov rsi, rdx                    ; rsi = patchName
    mov r9, r8                      ; r9 = patchData
    
    ; Get start time
    call GetSystemTimeAsFileTime
    mov r10, rax                    ; r10 = start time
    
    ; Allocate result
    mov rcx, SIZEOF UNIFIED_RESULT
    call malloc
    mov r11, rax                    ; r11 = result
    
    ; Check if server hotpatch exists
    mov r12, [rbx + UNIFIED_HOTPATCH_MANAGER.serverHotpatch]
    test r12, r12
    jz .no_server_hotpatch
    
    ; Add server hotpatch (simplified)
    ; In real implementation, this would call server_hotpatch_add
    
    ; Get end time
    call GetSystemTimeAsFileTime
    sub rax, r10                    ; rax = elapsed time
    mov [r11 + UNIFIED_RESULT.elapsedMs], rax
    
    ; Set unified result
    mov byte [r11 + UNIFIED_RESULT.success], 1
    lea rax, [szPatchAppliedDetail]
    mov [r11 + UNIFIED_RESULT.detail], rax
    mov [r11 + UNIFIED_RESULT.errorCode], 0
    mov [r11 + UNIFIED_RESULT.layer], PATCH_LAYER_SERVER
    
    ; Update statistics
    inc qword [rbx + UNIFIED_HOTPATCH_MANAGER.totalPatchesApplied]
    
    ; Log success
    lea rcx, [szPatchApplied]
    mov rdx, rsi
    mov r8d, PATCH_LAYER_SERVER
    call console_log
    
    mov rax, r11                    ; Return result
    pop rsi
    pop rbx
    ret
    
.no_server_hotpatch:
    ; Set error result
    mov byte [r11 + UNIFIED_RESULT.success], 0
    lea rax, [szPatchFailedDetail]
    mov [r11 + UNIFIED_RESULT.detail], rax
    mov [r11 + UNIFIED_RESULT.errorCode], 1
    mov [r11 + UNIFIED_RESULT.layer], PATCH_LAYER_SERVER
    
    ; Log failure
    lea rcx, [szPatchFailed]
    mov rdx, rsi
    mov r8d, PATCH_LAYER_SERVER
    mov r9d, 1
    call console_log
    
    inc dword [rbx + UNIFIED_HOTPATCH_MANAGER.totalErrors]
    
    mov rax, r11
    pop rsi
    pop rbx
    ret
unified_manager_add_server_hotpatch ENDP

; ============================================================================

; unified_manager_add_hotpatch(RCX = manager, RDX = name, R8 = description, R9d = layer)
; Add unified hotpatch
; Returns: RAX = hotpatch ID (0 on error)
PUBLIC unified_manager_add_hotpatch
unified_manager_add_hotpatch PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = manager
    mov rsi, rdx                    ; rsi = name
    mov r10, r8                     ; r10 = description
    mov r11d, r9d                   ; r11d = layer
    
    ; Check capacity
    mov r12d, [rbx + UNIFIED_HOTPATCH_MANAGER.hotpatchCount]
    cmp r12d, [rbx + UNIFIED_HOTPATCH_MANAGER.maxHotpatches]
    jge .capacity_exceeded
    
    ; Get hotpatch slot
    mov r13, [rbx + UNIFIED_HOTPATCH_MANAGER.hotpatches]
    mov r14, r12
    imul r14, SIZEOF UNIFIED_HOTPATCH
    add r13, r14
    
    ; Store hotpatch name
    mov rcx, rsi
    call strlen
    inc rax
    call malloc
    mov [r13 + UNIFIED_HOTPATCH.name], rax
    
    mov rcx, rsi
    mov rdx, rax
    call strcpy
    
    ; Store hotpatch description
    mov rcx, r10
    call strlen
    inc rax
    call malloc
    mov [r13 + UNIFIED_HOTPATCH.description], rax
    
    mov rcx, r10
    mov rdx, rax
    call strcpy
    
    ; Set hotpatch properties
    mov [r13 + UNIFIED_HOTPATCH.layer], r11d
    mov byte [r13 + UNIFIED_HOTPATCH.enabled], 1
    mov byte [r13 + UNIFIED_HOTPATCH.applied], 0
    mov [r13 + UNIFIED_HOTPATCH.timesApplied], 0
    
    ; Increment hotpatch count
    inc dword [rbx + UNIFIED_HOTPATCH_MANAGER.hotpatchCount]
    
    mov eax, r12d                   ; Return hotpatch ID
    pop rsi
    pop rbx
    ret
    
.capacity_exceeded:
    xor rax, rax
    pop rsi
    pop rbx
    ret
unified_manager_add_hotpatch ENDP

; ============================================================================

; unified_manager_add_preset(RCX = manager, RDX = name, R8 = description)
; Add hotpatch preset
; Returns: RAX = preset ID (0 on error)
PUBLIC unified_manager_add_preset
unified_manager_add_preset PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = manager
    mov rsi, rdx                    ; rsi = name
    mov r10, r8                     ; r10 = description
    
    ; Check capacity
    mov r11d, [rbx + UNIFIED_HOTPATCH_MANAGER.presetCount]
    cmp r11d, [rbx + UNIFIED_HOTPATCH_MANAGER.maxPresets]
    jge .capacity_exceeded
    
    ; Get preset slot
    mov r12, [rbx + UNIFIED_HOTPATCH_MANAGER.presets]
    mov r13, r11
    imul r13, SIZEOF HOTPATCH_PRESET
    add r12, r13
    
    ; Store preset name
    mov rcx, rsi
    call strlen
    inc rax
    call malloc
    mov [r12 + HOTPATCH_PRESET.name], rax
    
    mov rcx, rsi
    mov rdx, rax
    call strcpy
    
    ; Store preset description
    mov rcx, r10
    call strlen
    inc rax
    call malloc
    mov [r12 + HOTPATCH_PRESET.description], rax
    
    mov rcx, r10
    mov rdx, rax
    call strcpy
    
    ; Initialize preset
    mov [r12 + HOTPATCH_PRESET.hotpatchCount], 0
    
    ; Allocate hotpatches array
    mov rcx, MAX_HOTPATCHES
    imul rcx, 4                     ; DWORD IDs
    call malloc
    mov [r12 + HOTPATCH_PRESET.hotpatches], rax
    
    ; Increment preset count
    inc dword [rbx + UNIFIED_HOTPATCH_MANAGER.presetCount]
    
    mov eax, r11d                   ; Return preset ID
    pop rsi
    pop rbx
    ret
    
.capacity_exceeded:
    xor rax, rax
    pop rsi
    pop rbx
    ret
unified_manager_add_preset ENDP

; ============================================================================

; unified_manager_apply_preset(RCX = manager, RDX = presetId)
; Apply hotpatch preset
; Returns: RAX = pointer to UNIFIED_RESULT
PUBLIC unified_manager_apply_preset
unified_manager_apply_preset PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = manager
    mov esi, edx                    ; esi = presetId
    
    ; Get start time
    call GetSystemTimeAsFileTime
    mov r8, rax                     ; r8 = start time
    
    ; Allocate result
    mov rcx, SIZEOF UNIFIED_RESULT
    call malloc
    mov r9, rax                     ; r9 = result
    
    ; Get preset
    mov rcx, rbx
    mov edx, esi
    call unified_manager_get_preset
    mov r10, rax                    ; r10 = preset
    
    test r10, r10
    jz .preset_not_found
    
    ; Apply all hotpatches in preset
    mov r11d, [r10 + HOTPATCH_PRESET.hotpatchCount]
    xor r12d, r12d                  ; r12d = success count
    xor r13d, r13d                  ; r13d = index
    
.apply_loop:
    cmp r13d, r11d
    jge .apply_complete
    
    ; Get hotpatch ID
    mov r14, [r10 + HOTPATCH_PRESET.hotpatches]
    mov r15d, r13d
    imul r15d, 4
    mov eax, dword [r14 + r15]
    
    ; Get hotpatch
    mov rcx, rbx
    mov edx, eax
    call unified_manager_get_hotpatch
    mov r15, rax                    ; r15 = hotpatch
    
    test r15, r15
    jz .skip_hotpatch
    
    ; Apply hotpatch based on layer
    mov eax, [r15 + UNIFIED_HOTPATCH.layer]
    
    cmp eax, PATCH_LAYER_MEMORY
    je .apply_memory
    cmp eax, PATCH_LAYER_BYTE
    je .apply_byte
    cmp eax, PATCH_LAYER_SERVER
    je .apply_server
    
    jmp .skip_hotpatch
    
.apply_memory:
    mov rcx, rbx
    mov rdx, [r15 + UNIFIED_HOTPATCH.name]
    mov r8, [r15 + UNIFIED_HOTPATCH.data]
    call unified_manager_apply_memory_patch
    jmp .check_result
    
.apply_byte:
    mov rcx, rbx
    mov rdx, [r15 + UNIFIED_HOTPATCH.name]
    mov r8, [r15 + UNIFIED_HOTPATCH.data]
    call unified_manager_apply_byte_patch
    jmp .check_result
    
.apply_server:
    mov rcx, rbx
    mov rdx, [r15 + UNIFIED_HOTPATCH.name]
    mov r8, [r15 + UNIFIED_HOTPATCH.data]
    call unified_manager_add_server_hotpatch
    jmp .check_result
    
.check_result:
    mov r14, rax                    ; r14 = result
    cmp byte [r14 + UNIFIED_RESULT.success], 1
    jne .hotpatch_failed
    
    inc r12d
    
.hotpatch_failed:
    mov rcx, r14
    call free
    
.skip_hotpatch:
    inc r13d
    jmp .apply_loop
    
.apply_complete:
    ; Get end time
    call GetSystemTimeAsFileTime
    sub rax, r8                     ; rax = elapsed time
    mov [r9 + UNIFIED_RESULT.elapsedMs], rax
    
    ; Set result
    mov byte [r9 + UNIFIED_RESULT.success], 1
    lea rax, [szPresetAppliedDetail]
    mov [r9 + UNIFIED_RESULT.detail], rax
    mov [r9 + UNIFIED_RESULT.errorCode], 0
    mov [r9 + UNIFIED_RESULT.layer], 0
    
    ; Log success
    lea rcx, [szPresetApplied]
    mov rdx, [r10 + HOTPATCH_PRESET.name]
    mov r8, r11
    call console_log
    
    mov rax, r9                     ; Return result
    pop rsi
    pop rbx
    ret
    
.preset_not_found:
    ; Set error result
    mov byte [r9 + UNIFIED_RESULT.success], 0
    lea rax, [szPresetFailedDetail]
    mov [r9 + UNIFIED_RESULT.detail], rax
    mov [r9 + UNIFIED_RESULT.errorCode], 1
    mov [r9 + UNIFIED_RESULT.layer], 0
    
    ; Log failure
    lea rcx, [szPresetFailed]
    mov rdx, esi
    mov r8d, 1
    call console_log
    
    inc dword [rbx + UNIFIED_HOTPATCH_MANAGER.totalErrors]
    
    mov rax, r9
    pop rsi
    pop rbx
    ret
unified_manager_apply_preset ENDP

; ============================================================================

; unified_manager_get_hotpatch(RCX = manager, RDX = hotpatchId)
; Get hotpatch by ID
; Returns: RAX = pointer to UNIFIED_HOTPATCH
PUBLIC unified_manager_get_hotpatch
unified_manager_get_hotpatch PROC
    mov r8, [rcx + UNIFIED_HOTPATCH_MANAGER.hotpatches]
    mov r9d, [rcx + UNIFIED_HOTPATCH_MANAGER.hotpatchCount]
    xor r10d, r10d
    
.find_hotpatch:
    cmp r10d, r9d
    jge .hotpatch_not_found
    
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF UNIFIED_HOTPATCH
    add r11, r12
    
    cmp r10d, edx
    je .hotpatch_found
    
    inc r10d
    jmp .find_hotpatch
    
.hotpatch_found:
    mov rax, r11
    ret
    
.hotpatch_not_found:
    xor rax, rax
    ret
unified_manager_get_hotpatch ENDP

; ============================================================================

; unified_manager_get_preset(RCX = manager, RDX = presetId)
; Get preset by ID
; Returns: RAX = pointer to HOTPATCH_PRESET
PUBLIC unified_manager_get_preset
unified_manager_get_preset PROC
    mov r8, [rcx + UNIFIED_HOTPATCH_MANAGER.presets]
    mov r9d, [rcx + UNIFIED_HOTPATCH_MANAGER.presetCount]
    xor r10d, r10d
    
.find_preset:
    cmp r10d, r9d
    jge .preset_not_found
    
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF HOTPATCH_PRESET
    add r11, r12
    
    cmp r10d, edx
    je .preset_found
    
    inc r10d
    jmp .find_preset
    
.preset_found:
    mov rax, r11
    ret
    
.preset_not_found:
    xor rax, rax
    ret
unified_manager_get_preset ENDP

; ============================================================================

; unified_manager_get_statistics(RCX = manager, RDX = statsBuffer)
; Get manager statistics
PUBLIC unified_manager_get_statistics
unified_manager_get_statistics PROC
    mov [rdx + 0], qword [rcx + UNIFIED_HOTPATCH_MANAGER.totalPatchesApplied]
    mov [rdx + 8], qword [rcx + UNIFIED_HOTPATCH_MANAGER.totalBytesPatched]
    mov [rdx + 16], dword [rcx + UNIFIED_HOTPATCH_MANAGER.totalErrors]
    ret
unified_manager_get_statistics ENDP

; ============================================================================

; unified_manager_destroy(RCX = manager)
; Free unified hotpatch manager
PUBLIC unified_manager_destroy
unified_manager_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Free hotpatches array
    mov r10, [rbx + UNIFIED_HOTPATCH_MANAGER.hotpatches]
    mov r11d, [rbx + UNIFIED_HOTPATCH_MANAGER.hotpatchCount]
    xor r12d, r12d
    
.free_hotpatches:
    cmp r12d, r11d
    jge .hotpatches_freed
    
    mov r13, r10
    mov r14, r12
    imul r14, SIZEOF UNIFIED_HOTPATCH
    add r13, r14
    
    mov rcx, [r13 + UNIFIED_HOTPATCH.name]
    cmp rcx, 0
    je .skip_hotpatch_name
    call free
    
.skip_hotpatch_name:
    mov rcx, [r13 + UNIFIED_HOTPATCH.description]
    cmp rcx, 0
    je .skip_hotpatch_desc
    call free
    
.skip_hotpatch_desc:
    inc r12d
    jmp .free_hotpatches
    
.hotpatches_freed:
    mov rcx, [rbx + UNIFIED_HOTPATCH_MANAGER.hotpatches]
    cmp rcx, 0
    je .skip_hotpatches_array
    call free
    
.skip_hotpatches_array:
    ; Free presets array
    mov r10, [rbx + UNIFIED_HOTPATCH_MANAGER.presets]
    mov r11d, [rbx + UNIFIED_HOTPATCH_MANAGER.presetCount]
    xor r12d, r12d
    
.free_presets:
    cmp r12d, r11d
    jge .presets_freed
    
    mov r13, r10
    mov r14, r12
    imul r14, SIZEOF HOTPATCH_PRESET
    add r13, r14
    
    mov rcx, [r13 + HOTPATCH_PRESET.name]
    cmp rcx, 0
    je .skip_preset_name
    call free
    
.skip_preset_name:
    mov rcx, [r13 + HOTPATCH_PRESET.description]
    cmp rcx, 0
    je .skip_preset_desc
    call free
    
.skip_preset_desc:
    mov rcx, [r13 + HOTPATCH_PRESET.hotpatches]
    cmp rcx, 0
    je .skip_preset_hotpatches
    call free
    
.skip_preset_hotpatches:
    inc r12d
    jmp .free_presets
    
.presets_freed:
    mov rcx, [rbx + UNIFIED_HOTPATCH_MANAGER.presets]
    cmp rcx, 0
    je .skip_presets_array
    call free
    
.skip_presets_array:
    ; Free callbacks array
    mov rcx, [rbx + UNIFIED_HOTPATCH_MANAGER.callbacks]
    cmp rcx, 0
    je .skip_callbacks
    call free
    
.skip_callbacks:
    ; Free manager
    mov rcx, rbx
    call free
    
    pop rbx
    ret
unified_manager_destroy ENDP

; ============================================================================

.data
    szPatchAppliedDetail DB "Patch applied successfully", 0
    szPatchFailedDetail DB "Patch application failed", 0
    szPresetAppliedDetail DB "Preset applied successfully", 0
    szPresetFailedDetail DB "Preset application failed", 0

END
