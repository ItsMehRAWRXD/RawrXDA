;=====================================================================
; unified_hotpatch_manager.asm - Three-Layer Hotpatch Coordinator (Pure MASM x64)
; ZERO-DEPENDENCY UNIFIED HOTPATCH ORCHESTRATION
;=====================================================================
; Coordinates all three hotpatch layers:
;  - Memory layer (model_memory_hotpatch.asm)
;  - Byte layer (byte_level_hotpatcher.asm)
;  - Server layer (gguf_server_hotpatch.asm)
;
; Provides unified API with event-based coordination.
;
; UnifiedHotpatchManager Structure (1024 bytes):
;   [+0]:  event_loop_handle (qword)
;   [+8]:  memory_patches_ptr (qword) - array of memory patches
;   [+16]: memory_patch_count (qword)
;   [+24]: byte_patches_ptr (qword) - array of byte patches
;   [+32]: byte_patch_count (qword)
;   [+40]: server_hotpatch_count (qword)
;   [+48]: mutex_handle (qword)
;   [+56]: preset_registry_ptr (qword) - saved presets
;   [+64]: preset_count (qword)
;   [+72]: total_operations (qword)
;   [+80]: successful_operations (qword)
;   [+88]: failed_operations (qword)
;   [+96]: reserved[116] (qword[116])
;
; UnifiedResult Structure (128 bytes):
;   [+0]:  success (qword)
;   [+8]:  operation_name_ptr (qword)
;   [+16]: operation_name_len (qword)
;   [+24]: detail_ptr (qword)
;   [+32]: detail_len (qword)
;   [+40]: patch_layer (qword) - 0=memory, 1=byte, 2=server
;   [+48]: error_code (qword)
;   [+56]: reserved[9] (qword[9])
;=====================================================================

; Public exports
PUBLIC masm_unified_manager_create
PUBLIC masm_unified_apply_memory_patch
PUBLIC masm_unified_apply_byte_patch
PUBLIC masm_unified_add_server_hotpatch
PUBLIC masm_unified_process_events
PUBLIC masm_unified_get_stats
PUBLIC masm_unified_destroy

; External dependencies - ALL required functions
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_mutex_create:PROC
EXTERN asm_mutex_destroy:PROC
EXTERN asm_mutex_lock:PROC
EXTERN asm_mutex_unlock:PROC
EXTERN asm_event_loop_create:PROC
EXTERN asm_event_loop_destroy:PROC
EXTERN asm_event_loop_emit:PROC
EXTERN asm_event_loop_process_all:PROC
EXTERN masm_byte_patch_find_pattern:PROC
EXTERN masm_byte_patch_apply:PROC
EXTERN masm_server_hotpatch_add:PROC
EXTERN masm_server_hotpatch_cleanup:PROC
EXTERN masm_server_hotpatch_init:PROC
EXTERN asm_event_loop_register_signal:PROC
EXTERN masm_hotpatch_apply_memory:PROC
EXTERN asm_log:PROC
.data

; Global unified manager instance
g_unified_manager_ptr   QWORD 0

; Logging messages
msg_unified_mem_enter   DB "UNIFIED mem apply enter",0
msg_unified_mem_fail    DB "UNIFIED mem apply fail",0
msg_unified_mem_exit    DB "UNIFIED mem apply exit",0
msg_unified_byte_enter  DB "UNIFIED byte apply enter",0
msg_unified_byte_fail   DB "UNIFIED byte apply fail",0
msg_unified_byte_exit   DB "UNIFIED byte apply exit",0

; Event handler log format strings
szLogPatchApplied       DB "PATCH applied",0
szLogPatchFailed        DB "PATCH failed",0

; Simple metrics block used by handlers
; [0] patches_applied (qword)
; [8] patches_failed  (qword)
g_hotpatch_metrics      QWORD 0, 0

; Error recovery feature toggle (0=off, nonzero=on)
g_error_recovery_enabled DWORD 0

; Signal IDs for event coordination
SIGNAL_PATCH_APPLIED        EQU 1000
SIGNAL_PATCH_FAILED         EQU 1001
SIGNAL_OPTIMIZATION_COMPLETE EQU 1002
SIGNAL_ERROR_OCCURRED       EQU 1003

.code

;=====================================================================
; masm_unified_manager_create(event_queue_size: rcx) -> rax (manager handle or NULL)
;
; Creates unified hotpatch manager with event loop.
;=====================================================================

ALIGN 16
masm_unified_manager_create PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; r12 = event_queue_size
    
    ; Allocate manager structure
    mov rcx, 1024
    mov rdx, 64
    call asm_malloc
    test rax, rax
    jz create_fail
    
    mov rbx, rax            ; rbx = manager ptr
    
    ; Create event loop
    mov rcx, r12
    call asm_event_loop_create
    test rax, rax
    jz create_fail_free_manager
    
    mov [rbx], rax          ; event_loop_handle
    
    ; Create mutex
    call asm_mutex_create
    test rax, rax
    jz create_fail_free_loop
    
    mov [rbx + 48], rax     ; mutex_handle
    
    ; Initialize arrays
    mov rcx, 64
    imul rcx, 128           ; 64 slots * 128 bytes per MemoryPatch
    mov rdx, 64
    call asm_malloc
    test rax, rax
    jz create_fail_free_mutex
    
    mov [rbx + 8], rax      ; memory_patches_ptr
    mov qword ptr [rbx + 16], 0  ; memory_patch_count = 0
    
    ; Allocate byte patches array
    mov rcx, 64
    imul rcx, 256           ; 64 slots * 256 bytes per BytePatch
    mov rdx, 64
    call asm_malloc
    test rax, rax
    jz create_fail_free_memory_array
    
    mov [rbx + 24], rax     ; byte_patches_ptr
    mov qword ptr [rbx + 32], 0  ; byte_patch_count = 0
    
    ; Initialize server hotpatch layer
    mov rcx, 64             ; capacity
    call masm_server_hotpatch_init
    test rax, rax
    jz create_fail_free_byte_array
    
    ; Initialize statistics
    mov qword ptr [rbx + 72], 0  ; total_operations
    mov qword ptr [rbx + 80], 0  ; successful_operations
    mov qword ptr [rbx + 88], 0  ; failed_operations
    
    ; Store global instance
    mov [g_unified_manager_ptr], rbx
    
    ; Register event handlers
    mov rcx, [rbx]          ; event_loop_handle
    mov edx, SIGNAL_PATCH_APPLIED
    lea r8, [unified_handler_patch_applied]
    call asm_event_loop_register_signal
    
    mov rcx, [rbx]
    mov edx, SIGNAL_PATCH_FAILED
    lea r8, [unified_handler_patch_failed]
    call asm_event_loop_register_signal
    
    mov rax, rbx            ; Return manager handle
    jmp create_exit

create_fail_free_byte_array:
    mov rcx, [rbx + 24]
    call asm_free
    
create_fail_free_memory_array:
    mov rcx, [rbx + 8]
    call asm_free
    
create_fail_free_mutex:
    mov rcx, [rbx + 48]
    call asm_mutex_destroy
    
create_fail_free_loop:
    mov rcx, [rbx]
    call asm_event_loop_destroy
    
create_fail_free_manager:
    mov rcx, rbx
    call asm_free
    
create_fail:
    xor rax, rax

create_exit:
    add rsp, 32
    pop r12
    pop rbx
    ret

masm_unified_manager_create ENDP

;=====================================================================
; masm_unified_apply_memory_patch(manager: rcx, name_ptr: rdx, patch_ptr: r8, 
;                                result_ptr: r9) -> void
;
; Applies memory layer hotpatch through unified manager.
; Emits SIGNAL_PATCH_APPLIED or SIGNAL_PATCH_FAILED.
;=====================================================================

ALIGN 16
masm_unified_apply_memory_patch PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = manager
    mov r12, rdx            ; r12 = name_ptr
    mov r13, r8             ; r13 = patch_ptr
    mov r14, r9             ; r14 = result_ptr
    lea rcx, msg_unified_mem_enter
    call asm_log
    
    test rbx, rbx
    jz apply_mem_exit
    
    ; Lock manager
    mov rcx, [rbx + 48]
    call asm_mutex_lock
    
    ; Increment total operations
    inc qword ptr [rbx + 72]
    
    ; Unlock (operations are independent)
    mov rcx, [rbx + 48]
    call asm_mutex_unlock
    
    ; Apply memory patch
    mov rcx, r13            ; patch_ptr
    mov rdx, r14            ; result_ptr
    call masm_hotpatch_apply_memory
    
    ; Check result
    mov rax, [r14]          ; rax = success flag
    test rax, rax
    jz apply_mem_failed
    
    ; Success: emit SIGNAL_PATCH_APPLIED
    lock inc qword ptr [rbx + 80]  ; successful_operations++
    
    mov rcx, [rbx]          ; event_loop_handle
    mov edx, SIGNAL_PATCH_APPLIED
    mov r8, r12             ; p1 = name_ptr
    xor r9, r9              ; p2 = 0 (memory layer)
    mov qword ptr [rsp + 40], r13  ; p3 = patch_ptr
    
    call asm_event_loop_emit
    jmp apply_mem_exit

apply_mem_failed:
    lea rcx, msg_unified_mem_fail
    call asm_log
    lock inc qword ptr [rbx + 88]  ; failed_operations++
    
    mov rcx, [rbx]
    mov edx, SIGNAL_PATCH_FAILED
    mov r8, r12
    xor r9, r9
    mov qword ptr [rsp + 40], r14  ; p3 = result_ptr
    
    call asm_event_loop_emit

apply_mem_exit:
    lea rcx, msg_unified_mem_exit
    call asm_log
    add rsp, 32
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_unified_apply_memory_patch ENDP

;=====================================================================
; masm_unified_apply_byte_patch(manager: rcx, name_ptr: rdx, patch_ptr: r8, 
;                              result_ptr: r9) -> void
;
; Applies byte layer hotpatch through unified manager.
;=====================================================================

ALIGN 16
masm_unified_apply_byte_patch PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = manager
    mov r12, rdx            ; r12 = name_ptr
    mov r13, r8             ; r13 = patch_ptr
    mov r14, r9             ; r14 = result_ptr (unused for byte patches)
    lea rcx, msg_unified_byte_enter
    call asm_log
    
    test rbx, rbx
    jz apply_byte_exit
    
    ; Lock manager
    mov rcx, [rbx + 48]
    call asm_mutex_lock
    
    inc qword ptr [rbx + 72]
    
    mov rcx, [rbx + 48]
    call asm_mutex_unlock
    
    ; Find pattern in file
    mov rcx, r13
    call masm_byte_patch_find_pattern
    
    cmp rax, -1
    je apply_byte_failed
    
    ; Apply byte patch
    mov rcx, r13
    call masm_byte_patch_apply
    
    test rax, rax
    jz apply_byte_failed
    
    ; Success
    lock inc qword ptr [rbx + 80]
    
    mov rcx, [rbx]
    mov edx, SIGNAL_PATCH_APPLIED
    mov r8, r12
    mov r9, 1               ; p2 = 1 (byte layer)
    mov qword ptr [rsp + 40], r13
    
    call asm_event_loop_emit
    jmp apply_byte_exit

apply_byte_failed:
    lea rcx, msg_unified_byte_fail
    call asm_log
    lock inc qword ptr [rbx + 88]
    
    mov rcx, [rbx]
    mov edx, SIGNAL_PATCH_FAILED
    mov r8, r12
    mov r9, 1
    mov qword ptr [rsp + 40], 0
    
    call asm_event_loop_emit

apply_byte_exit:
    lea rcx, msg_unified_byte_exit
    call asm_log
    add rsp, 32
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_unified_apply_byte_patch ENDP

;=====================================================================
; masm_unified_add_server_hotpatch(manager: rcx, hotpatch_ptr: rdx) -> rax (hotpatch_id or -1)
;
; Adds server layer hotpatch through unified manager.
;=====================================================================

ALIGN 16
masm_unified_add_server_hotpatch PROC

    push rbx
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = manager
    
    test rbx, rbx
    jz add_server_fail
    
    ; Lock manager
    mov rcx, [rbx + 48]
    call asm_mutex_lock
    
    inc qword ptr [rbx + 72]
    
    mov rcx, [rbx + 48]
    call asm_mutex_unlock
    
    ; Add server hotpatch
    mov rcx, rdx
    call masm_server_hotpatch_add
    
    cmp rax, -1
    je add_server_fail
    
    ; Success
    lock inc qword ptr [rbx + 80]
    lock inc qword ptr [rbx + 40]  ; server_hotpatch_count++
    
    jmp add_server_exit

add_server_fail:
    lock inc qword ptr [rbx + 88]
    mov rax, -1

add_server_exit:
    add rsp, 32
    pop rbx
    ret

masm_unified_add_server_hotpatch ENDP

;=====================================================================
; masm_unified_process_events(manager: rcx) -> rax (events processed)
;
; Processes all pending events in the event loop.
;=====================================================================

ALIGN 16
masm_unified_process_events PROC

    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    test rbx, rbx
    jz process_fail
    
    mov rcx, [rbx]          ; event_loop_handle
    call asm_event_loop_process_all
    
    jmp process_exit

process_fail:
    xor rax, rax

process_exit:
    add rsp, 32
    pop rbx
    ret

masm_unified_process_events ENDP

;=====================================================================
; masm_unified_get_stats(manager: rcx, stats_ptr: rdx) -> void
;
; Fills unified statistics structure:
;   [0]: total_operations (qword)
;   [8]: successful_operations (qword)
;   [16]: failed_operations (qword)
;   [24]: memory_patch_count (qword)
;   [32]: byte_patch_count (qword)
;   [40]: server_hotpatch_count (qword)
;=====================================================================

ALIGN 16
masm_unified_get_stats PROC

    test rcx, rcx
    jz stats_exit
    
    test rdx, rdx
    jz stats_exit
    
    mov rax, [rcx + 72]
    mov [rdx], rax
    
    mov rax, [rcx + 80]
    mov [rdx + 8], rax
    
    mov rax, [rcx + 88]
    mov [rdx + 16], rax
    
    mov rax, [rcx + 16]
    mov [rdx + 24], rax
    
    mov rax, [rcx + 32]
    mov [rdx + 32], rax
    
    mov rax, [rcx + 40]
    mov [rdx + 40], rax

stats_exit:
    ret

masm_unified_get_stats ENDP

;=====================================================================
; Event Handlers (called by event loop)
;=====================================================================

ALIGN 16
unified_handler_patch_applied PROC

    ; rcx = p1 (name_ptr)
    ; rdx = p2 (layer)
    ; r8 = p3 (patch_ptr)
    
    push rbx
    push rsi
    sub rsp, 40h
    
    mov rbx, rcx            ; Save name_ptr
    mov rsi, rdx            ; Save layer
    
    ; Log patch application success
    lea rcx, [szLogPatchApplied]
    mov rdx, rbx            ; patch name
    mov r8, rsi             ; layer
    call asm_log
    
    ; Update metrics - increment successful patch count
    ; Assume g_hotpatch_metrics structure exists
    lea rax, [g_hotpatch_metrics]
    lock inc qword ptr [rax]     ; patches_applied counter
    
    add rsp, 40h
    pop rsi
    pop rbx
    ret

unified_handler_patch_applied ENDP

ALIGN 16
unified_handler_patch_failed PROC

    ; rcx = p1 (name_ptr)
    ; rdx = p2 (layer)
    ; r8 = p3 (result_ptr or 0)
    
    push rbx
    push rsi
    push rdi
    sub rsp, 40h
    
    mov rbx, rcx            ; Save name_ptr
    mov rsi, rdx            ; Save layer
    mov rdi, r8             ; Save result_ptr
    
    ; Log error with details
    lea rcx, [szLogPatchFailed]
    mov rdx, rbx            ; patch name
    mov r8, rsi             ; layer
    mov r9, rdi             ; error code/result
    call asm_log
    
    ; Update error metrics
    lea rax, [g_hotpatch_metrics]
    lock inc qword ptr [rax + 8]     ; patches_failed counter
    
    ; Trigger error recovery if enabled
    cmp dword ptr [g_error_recovery_enabled], 0
    je skip_recovery
    
    ; Attempt recovery procedure
    mov rcx, rbx            ; patch name
    mov rdx, rsi            ; layer
    call unified_attempt_patch_recovery
    
skip_recovery:
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    ret

unified_handler_patch_failed ENDP

;=====================================================================
; Recovery hook used by unified_handler_patch_failed
;=====================================================================

ALIGN 16
unified_attempt_patch_recovery PROC
    ; rcx = patch name, rdx = layer
    ; Minimal recovery hook: currently no-op, returns success
    mov eax, 1
    ret
unified_attempt_patch_recovery ENDP

;=====================================================================
; masm_unified_destroy(manager: rcx) -> void
;
; Destroys unified manager and all resources.
;=====================================================================

ALIGN 16
masm_unified_destroy PROC

    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    test rbx, rbx
    jz destroy_exit
    
    ; Destroy event loop
    mov rcx, [rbx]
    call asm_event_loop_destroy
    
    ; Free arrays
    mov rcx, [rbx + 8]
    call asm_free
    
    mov rcx, [rbx + 24]
    call asm_free
    
    ; Cleanup server hotpatches
    call masm_server_hotpatch_cleanup
    
    ; Destroy mutex
    mov rcx, [rbx + 48]
    call asm_mutex_destroy
    
    ; Free manager structure
    mov rcx, rbx
    call asm_free
    
    mov qword ptr [g_unified_manager_ptr], 0

destroy_exit:
    add rsp, 32
    pop rbx
    ret

masm_unified_destroy ENDP

END


