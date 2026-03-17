; ═══════════════════════════════════════════════════════════════════
; PE Writer Test Harness — Minimal entry point
; Calls WritePEFile + SavePEToDisk, then exits.
; Provides stub symbols for all EXTERNs ui.asm needs.
; Assembles: poasm /AAMD64 pe_test_harness.asm
; Links: polink /SUBSYSTEM:CONSOLE pe_test_harness.obj ui.obj ...
; ═══════════════════════════════════════════════════════════════════

EXTERN WritePEFile:PROC
EXTERN SavePEToDisk:PROC
EXTERN ExitProcess:PROC
EXTERN GetModuleHandleW:PROC

PUBLIC WinMainCRTStartup
PUBLIC g_hInstance

; Bridge stubs referenced by ui.asm
PUBLIC Bridge_SubmitCompletion
PUBLIC Bridge_GetSuggestionText
PUBLIC Bridge_ClearSuggestion
PUBLIC Bridge_RequestSuggestion

.data
align 8
g_hInstance   dq 0

.code

WinMainCRTStartup PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Get module handle
    xor     ecx, ecx
    call    GetModuleHandleW
    mov     g_hInstance, rax

    ; Generate PE in memory
    call    WritePEFile
    test    rax, rax
    jz      @fail

    ; Save to output.exe
    call    SavePEToDisk
    test    rax, rax
    jz      @fail

    ; Success: exit code 0
    xor     ecx, ecx
    call    ExitProcess

@fail:
    mov     ecx, 1
    call    ExitProcess
WinMainCRTStartup ENDP

; ── Bridge stubs (no-op) ────────────────────────────────────────
Bridge_SubmitCompletion PROC
    ret
Bridge_SubmitCompletion ENDP

Bridge_GetSuggestionText PROC
    xor     eax, eax
    ret
Bridge_GetSuggestionText ENDP

Bridge_ClearSuggestion PROC
    ret
Bridge_ClearSuggestion ENDP

Bridge_RequestSuggestion PROC
    ret
Bridge_RequestSuggestion ENDP

END
