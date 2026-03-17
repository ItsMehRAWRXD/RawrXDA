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
; Ranges derived from COMMAND_TABLE in command_registry.hpp (SSOT)
; ============================================================================

; File/Edit/View/Git/Terminal/Help core ranges (< 5000)
CMD_IDE_CORE_MIN    EQU 100       ; ide_constants.h IDs (105, 106, 108-110)
CMD_IDE_CORE_MAX    EQU 699       ; through 601, 603

CMD_FILE_MIN        EQU 1000
CMD_FILE_MAX        EQU 1099
CMD_EDIT_MIN        EQU 2000
CMD_EDIT_MAX        EQU 2029
CMD_VIEW_THEME_MIN  EQU 3000      ; View 3020-3024 Git, 3100-3117 Theme, 3200-3211 Trans
CMD_VIEW_THEME_MAX  EQU 3299
CMD_TERMINAL_MIN    EQU 4000
CMD_TERMINAL_MAX    EQU 4010
CMD_AGENT_MIN       EQU 4100
CMD_AGENT_MAX       EQU 4155      ; Agent + Autonomy
CMD_AI_MODE_MIN     EQU 4200
CMD_AI_MODE_MAX     EQU 4216      ; AI Mode + Context Window
CMD_RE_MIN          EQU 4300
CMD_RE_MAX          EQU 4319      ; Reverse Engineering

; Extended feature surface (5000-5999)
CMD_BACKEND_MIN     EQU 5037
CMD_BACKEND_MAX     EQU 5047
CMD_ROUTER_MIN      EQU 5048
CMD_ROUTER_MAX      EQU 5081
CMD_LSP_MIN         EQU 5058      ; overlaps router, handled by C++ dispatch
CMD_ASM_SEM_MIN     EQU 5082
CMD_ASM_SEM_MAX     EQU 5093
CMD_HYBRID_MIN      EQU 5094
CMD_HYBRID_MAX      EQU 5105
CMD_MULTI_MIN       EQU 5106
CMD_MULTI_MAX       EQU 5117
CMD_GOV_MIN         EQU 5118
CMD_GOV_MAX         EQU 5121
CMD_SAFETY_MIN      EQU 5122
CMD_SAFETY_MAX      EQU 5131
CMD_SWARM_MIN       EQU 5132
CMD_SWARM_MAX       EQU 5156
CMD_DEBUG_MIN       EQU 5157
CMD_DEBUG_MAX       EQU 5184
CMD_PLUGIN_MIN      EQU 5200
CMD_PLUGIN_MAX      EQU 5208

; Decompiler context menu
CMD_DECOMP_MIN      EQU 8001
CMD_DECOMP_MAX      EQU 8006

; System panels (9000-9999)
CMD_HOTPATCH_MIN    EQU 9001
CMD_HOTPATCH_MAX    EQU 9017
CMD_MONACO_MIN      EQU 9100
CMD_MONACO_MAX      EQU 9105
CMD_LSPSRV_MIN     EQU 9200
CMD_LSPSRV_MAX     EQU 9208
CMD_EDITOR_MIN      EQU 9300
CMD_EDITOR_MAX      EQU 9304
CMD_PDB_MIN         EQU 9400
CMD_PDB_MAX         EQU 9412
CMD_AUDIT_MIN       EQU 9500
CMD_AUDIT_MAX       EQU 9506
CMD_GAUNTLET_MIN    EQU 9600
CMD_GAUNTLET_MAX    EQU 9601
CMD_VOICE_MIN       EQU 9700
CMD_VOICE_MAX       EQU 9709
CMD_QW_MIN          EQU 9800
CMD_QW_MAX          EQU 9830
CMD_TELEM_MIN       EQU 9900
CMD_TELEM_MAX       EQU 9905

; VSCode Extension API + Voice Automation (10000+)
CMD_VSCEXT_MIN      EQU 10000
CMD_VSCEXT_MAX      EQU 10009
CMD_VAUTO_MIN       EQU 10200
CMD_VAUTO_MAX       EQU 10206

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

    ; ── Strategy: merge contiguous/overlapping ranges into super-ranges ──
    ; Rather than checking 30+ individual ranges, we check 6 super-ranges
    ; and let the C++ SSOT dispatch do the final exact-match.

    ; ── Super-range A: IDE core 100-699 ──────────────────────────────────
    cmp     ebx, CMD_IDE_CORE_MIN
    jb      .not_handled
    cmp     ebx, CMD_IDE_CORE_MAX
    jbe     .dispatch_it

    ; ── Super-range B: File/Edit/View/Theme/Trans 1000-3299 ──────────────
    cmp     ebx, CMD_FILE_MIN
    jb      .not_handled
    cmp     ebx, CMD_VIEW_THEME_MAX
    jbe     .dispatch_it

    ; ── Super-range C: Terminal/Agent/Autonomy/AI/RE 4000-4319 ───────────
    cmp     ebx, CMD_TERMINAL_MIN
    jb      .not_handled
    cmp     ebx, CMD_RE_MAX
    jbe     .dispatch_it

    ; ── Super-range D: Extended features 5037-5208 ───────────────────────
    cmp     ebx, CMD_BACKEND_MIN
    jb      .not_handled
    cmp     ebx, CMD_PLUGIN_MAX
    jbe     .dispatch_it

    ; ── Super-range E: Decompiler + Hotpatch-to-Telemetry 8001-9905 ─────
    cmp     ebx, CMD_DECOMP_MIN
    jb      .not_handled
    cmp     ebx, CMD_TELEM_MAX
    jbe     .dispatch_it

    ; ── Super-range F: VSCode Ext + Voice Automation 10000-10206 ─────────
    cmp     ebx, CMD_VSCEXT_MIN
    jb      .not_handled
    cmp     ebx, CMD_VAUTO_MAX
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
    
    ; Super-range A: IDE core 100-699
    cmp     eax, CMD_IDE_CORE_MIN
    jb      .invalid
    cmp     eax, CMD_IDE_CORE_MAX
    jbe     .valid

    ; Super-range B: File/Edit/View/Theme/Trans 1000-3299
    cmp     eax, CMD_FILE_MIN
    jb      .invalid
    cmp     eax, CMD_VIEW_THEME_MAX
    jbe     .valid

    ; Super-range C: Terminal/Agent/Autonomy/AI/RE 4000-4319
    cmp     eax, CMD_TERMINAL_MIN
    jb      .invalid
    cmp     eax, CMD_RE_MAX
    jbe     .valid

    ; Super-range D: Extended features 5037-5208
    cmp     eax, CMD_BACKEND_MIN
    jb      .invalid
    cmp     eax, CMD_PLUGIN_MAX
    jbe     .valid

    ; Super-range E: Decomp + Hotpatch-to-Telemetry 8001-9905
    cmp     eax, CMD_DECOMP_MIN
    jb      .invalid
    cmp     eax, CMD_TELEM_MAX
    jbe     .valid

    ; Super-range F: VSCode + Voice Automation 10000-10206
    cmp     eax, CMD_VSCEXT_MIN
    jb      .invalid
    cmp     eax, CMD_VAUTO_MAX
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
