; ============================================================================
; gui_dispatch_bridge.asm — x64 MASM Win32 GUI Dispatch Hot-Path
; ============================================================================
; Architecture: x64 MASM, Windows x64 calling convention
; Purpose: High-performance WM_COMMAND dispatch bridge for Win32 IDE.
;          Validates command IDs and calls into C++ unified dispatch.
;          Eliminates branch misprediction overhead for hot commands.
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
; ============================================================================

.code

; ============================================================================
; External C++ functions (from shared_feature_dispatch.cpp / win32_feature_adapter.h)
; ============================================================================

EXTERN rawrxd_dispatch_command:PROC      ; int rawrxd_dispatch_command(uint32_t cmdId, void* idePtr)
EXTERN rawrxd_get_feature_count:PROC     ; int rawrxd_get_feature_count(void)

; ============================================================================
; CONSTANTS — Command ID ranges for fast validation
; ============================================================================

CMD_FILE_MIN        EQU 1000
CMD_FILE_MAX        EQU 1099
CMD_EDIT_MIN        EQU 2000
CMD_EDIT_MAX        EQU 2099
CMD_VIEW_MIN        EQU 3000
CMD_VIEW_MAX        EQU 3099
CMD_TERMINAL_MIN    EQU 4000
CMD_TERMINAL_MAX    EQU 4099
CMD_AGENT_MIN       EQU 4100
CMD_AGENT_MAX       EQU 4199
CMD_AUTONOMY_MIN    EQU 4150
CMD_AUTONOMY_MAX    EQU 4199
CMD_DEBUG_MIN       EQU 4400
CMD_DEBUG_MAX       EQU 4499
CMD_HOTPATCH_MIN    EQU 9000
CMD_HOTPATCH_MAX    EQU 9099
CMD_AI_MIN          EQU 4700
CMD_AI_MAX          EQU 4799
CMD_RE_MIN          EQU 4600
CMD_RE_MAX          EQU 4699
CMD_VOICE_MIN       EQU 4A00h
CMD_VOICE_MAX       EQU 4A99h
CMD_SWARM_MIN       EQU 4900
CMD_SWARM_MAX       EQU 4999
CMD_BACKEND_MIN     EQU 4800
CMD_BACKEND_MAX     EQU 4899
CMD_SERVER_MIN      EQU 9100
CMD_SERVER_MAX      EQU 9199
CMD_GIT_MIN         EQU 8000
CMD_GIT_MAX         EQU 8099
CMD_THEME_MIN       EQU 9500
CMD_THEME_MAX       EQU 9599
CMD_SETTINGS_MIN    EQU 9400
CMD_SETTINGS_MAX    EQU 9499
CMD_HELP_MIN        EQU 7000
CMD_HELP_MAX        EQU 7099

; ============================================================================
; masm_gui_dispatch_command
; ============================================================================
; Fast-path WM_COMMAND dispatch for Win32 IDE.
; RCX = command ID (LOWORD(wParam))
; RDX = Win32IDE* (this pointer)
; Returns: 1 if handled, 0 if not
; ============================================================================

masm_gui_dispatch_command PROC
    ; Preserve non-volatile registers
    push    rbx
    push    rsi
    sub     rsp, 28h            ; Shadow space + alignment
    
    mov     ebx, ecx            ; Save command ID
    mov     rsi, rdx            ; Save IDE pointer
    
    ; ── Range validation: reject obviously invalid IDs ──────────────────
    test    ebx, ebx
    jz      .not_handled
    cmp     ebx, 20000
    ja      .not_handled
    
    ; ── Hot-path: File commands (most frequent) ──────────────────────────
    cmp     ebx, CMD_FILE_MIN
    jb      .check_higher_ranges
    cmp     ebx, CMD_FILE_MAX
    jbe     .dispatch_it
    
    ; ── Edit commands ────────────────────────────────────────────────────
    cmp     ebx, CMD_EDIT_MIN
    jb      .check_higher_ranges
    cmp     ebx, CMD_EDIT_MAX
    jbe     .dispatch_it
    
.check_higher_ranges:
    ; ── Agent commands ──────────────────────────────────────────────────
    cmp     ebx, CMD_AGENT_MIN
    jb      .check_terminal
    cmp     ebx, CMD_AGENT_MAX
    jbe     .dispatch_it
    
.check_terminal:
    cmp     ebx, CMD_TERMINAL_MIN
    jb      .check_debug
    cmp     ebx, CMD_TERMINAL_MAX
    jbe     .dispatch_it
    
.check_debug:
    cmp     ebx, CMD_DEBUG_MIN
    jb      .check_hotpatch
    cmp     ebx, CMD_DEBUG_MAX
    jbe     .dispatch_it
    
.check_hotpatch:
    cmp     ebx, CMD_HOTPATCH_MIN
    jb      .check_ai_modes
    cmp     ebx, CMD_HOTPATCH_MAX
    jbe     .dispatch_it
    
.check_ai_modes:
    cmp     ebx, CMD_AI_MIN
    jb      .check_re
    cmp     ebx, CMD_AI_MAX
    jbe     .dispatch_it
    
.check_re:
    cmp     ebx, CMD_RE_MIN
    jb      .check_swarm
    cmp     ebx, CMD_RE_MAX
    jbe     .dispatch_it
    
.check_swarm:
    cmp     ebx, CMD_SWARM_MIN
    jb      .check_backend
    cmp     ebx, CMD_SWARM_MAX
    jbe     .dispatch_it

.check_backend:
    cmp     ebx, CMD_BACKEND_MIN
    jb      .check_voice
    cmp     ebx, CMD_BACKEND_MAX
    jbe     .dispatch_it
    
.check_voice:
    cmp     ebx, CMD_VOICE_MIN
    jb      .check_server
    cmp     ebx, CMD_VOICE_MAX
    jbe     .dispatch_it
    
.check_server:
    cmp     ebx, CMD_SERVER_MIN
    jb      .check_git
    cmp     ebx, CMD_SERVER_MAX
    jbe     .dispatch_it
    
.check_git:
    cmp     ebx, CMD_GIT_MIN
    jb      .check_help
    cmp     ebx, CMD_GIT_MAX
    jbe     .dispatch_it
    
.check_help:
    cmp     ebx, CMD_HELP_MIN
    jb      .check_theme
    cmp     ebx, CMD_HELP_MAX
    jbe     .dispatch_it
    
.check_theme:
    cmp     ebx, CMD_THEME_MIN
    jb      .check_settings
    cmp     ebx, CMD_THEME_MAX
    jbe     .dispatch_it
    
.check_settings:
    cmp     ebx, CMD_SETTINGS_MIN
    jb      .not_handled
    cmp     ebx, CMD_SETTINGS_MAX
    jbe     .dispatch_it
    
    ; None of the known ranges matched
    jmp     .not_handled

.dispatch_it:
    ; Call C++ unified dispatch: rawrxd_dispatch_command(commandId, idePtr)
    mov     ecx, ebx            ; Command ID in RCX
    mov     rdx, rsi            ; IDE pointer in RDX
    call    rawrxd_dispatch_command
    
    ; Returns 1 if handled, 0 if not
    test    eax, eax
    jz      .not_handled
    
    mov     eax, 1
    jmp     .done
    
.not_handled:
    xor     eax, eax
    
.done:
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
masm_gui_dispatch_command ENDP

; ============================================================================
; masm_gui_validate_command_range
; ============================================================================
; Quick check whether a command ID falls into any known feature range.
; RCX = command ID
; Returns: 1 if in a valid range, 0 if unknown
; ============================================================================

masm_gui_validate_command_range PROC
    mov     eax, ecx
    
    ; File range
    cmp     eax, CMD_FILE_MIN
    jb      .check_edit
    cmp     eax, CMD_FILE_MAX
    jbe     .valid
    
.check_edit:
    cmp     eax, CMD_EDIT_MIN
    jb      .check_view
    cmp     eax, CMD_EDIT_MAX
    jbe     .valid
    
.check_view:
    cmp     eax, CMD_VIEW_MIN
    jb      .check_terminal
    cmp     eax, CMD_VIEW_MAX
    jbe     .valid
    
.check_terminal:
    cmp     eax, CMD_TERMINAL_MIN
    jb      .check_agent
    cmp     eax, CMD_TERMINAL_MAX
    jbe     .valid
    
.check_agent:
    cmp     eax, CMD_AGENT_MIN
    jb      .check_debug
    cmp     eax, CMD_AGENT_MAX
    jbe     .valid
    
.check_debug:
    cmp     eax, CMD_DEBUG_MIN
    jb      .check_hotpatch
    cmp     eax, CMD_DEBUG_MAX
    jbe     .valid
    
.check_hotpatch:
    cmp     eax, CMD_HOTPATCH_MIN
    jb      .check_help
    cmp     eax, CMD_HOTPATCH_MAX
    jbe     .valid
    
.check_help:
    cmp     eax, CMD_HELP_MIN
    jb      .invalid
    cmp     eax, CMD_HELP_MAX
    jbe     .valid
    
.invalid:
    xor     eax, eax
    ret
    
.valid:
    mov     eax, 1
    ret
masm_gui_validate_command_range ENDP

; ============================================================================
; masm_gui_get_feature_count
; ============================================================================
; Returns total registered feature count from the shared registry.
; No parameters. Returns count in EAX.
; ============================================================================

masm_gui_get_feature_count PROC
    sub     rsp, 28h
    call    rawrxd_get_feature_count
    add     rsp, 28h
    ret
masm_gui_get_feature_count ENDP

; ============================================================================
; masm_gui_batch_wm_command
; ============================================================================
; Processes an array of WM_COMMAND IDs in batch (for macro playback, scripting).
; RCX = pointer to array of DWORD command IDs
; RDX = count of commands
; R8  = Win32IDE* (this pointer)
; Returns: number of successfully dispatched commands in EAX
; ============================================================================

masm_gui_batch_wm_command PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 28h
    
    mov     rsi, rcx            ; Array pointer
    mov     r12d, edx           ; Count
    mov     r13, r8             ; IDE pointer
    xor     edi, edi            ; Success counter
    xor     ebx, ebx            ; Index
    
.loop:
    cmp     ebx, r12d
    jge     .done
    
    ; Load command ID from array
    mov     ecx, DWORD PTR [rsi + rbx*4]
    mov     rdx, r13            ; IDE pointer
    call    rawrxd_dispatch_command
    
    ; Count successes
    test    eax, eax
    jz      .next
    inc     edi
    
.next:
    inc     ebx
    jmp     .loop
    
.done:
    mov     eax, edi            ; Return success count
    add     rsp, 28h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
masm_gui_batch_wm_command ENDP

END
