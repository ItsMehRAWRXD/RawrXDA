;==========================================================================
; masm_hotpatch_coordinator.asm - Pure MASM Hotpatch Coordinator
; ==========================================================================
; Replaces unified_hotpatch_manager.cpp.
; Coordinates Memory, Byte, and Server hotpatching layers.
;==========================================================================

option casemap:none

include windows.inc

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN hpatch_apply_memory:PROC
EXTERN hpatch_apply_byte:PROC
EXTERN hpatch_apply_server:PROC
EXTERN console_log:PROC

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    szCoordInit     BYTE "HotpatchCoord: Initializing unified system...", 0
    szCoordApply    BYTE "HotpatchCoord: Applying multi-layer patch: %s", 0
    
    ; State
    g_coord_init    DWORD 0

.code

;==========================================================================
; hotpatch_coord_init()
;==========================================================================
PUBLIC hotpatch_coord_init
hotpatch_coord_init PROC
    sub rsp, 32
    
    lea rcx, szCoordInit
    call console_log
    
    mov g_coord_init, 1
    
    add rsp, 32
    ret
hotpatch_coord_init ENDP

;==========================================================================
; hotpatch_coord_apply(patch_name: rcx, layer_mask: rdx)
;==========================================================================
PUBLIC hotpatch_coord_apply
hotpatch_coord_apply PROC
    push rbx
    push rsi
    sub rsp, 32
    
    mov rsi, rcx        ; patch_name
    mov rbx, rdx        ; layer_mask
    
    lea rcx, szCoordApply
    mov rdx, rsi
    call console_log
    
    ; 1. Memory Layer (Bit 0)
    test rbx, 1
    jz @F
    mov rcx, rsi
    call hpatch_apply_memory
@@:
    ; 2. Byte Layer (Bit 1)
    test rbx, 2
    jz @F
    mov rcx, rsi
    call hpatch_apply_byte
@@:
    ; 3. Server Layer (Bit 2)
    test rbx, 4
    jz @F
    mov rcx, rsi
    call hpatch_apply_server
@@:
    
    add rsp, 32
    pop rsi
    pop rbx
    ret
hotpatch_coord_apply ENDP

END
