;=============================================================================
; RawrXD_AutoHeal_GUI.asm
; GUI harness for the autonomous agentic pipeline
;=============================================================================

OPTION WIN64:3
OPTION CASEMAP:NONE

include \masm64\include64\masm64rt.inc

EXTERN MessageBoxA:PROC
EXTERN ExitProcess:PROC
EXTERN AgenticMaster_Initialize:PROC
EXTERN AgenticMaster_ProcessUserInput:PROC
EXTERN AgenticMaster_GetStatus:PROC
EXTERN HealSymbolResolution:PROC
EXTERN ValidateDMAAlignment:PROC
EXTERN RawrXD_Trigger_Chat:PROC

MB_OK              EQU 00000000h
MB_ICONINFORMATION EQU 00000040h

.data
    szGuiPrompt    db "Generate autonomous GUI-safe MASM pipeline output",0
    szSymbol       db "VirtualAlloc",0
    szTitle        db "RawrXD Amphibious GUI",0
    szBodyOk       db "GUI autonomous pipeline completed successfully.",13,10
                   db "IDE UI -> Chat -> Prompt -> LLM -> Token Stream -> Renderer",0
    szBodyFail     db "GUI autonomous pipeline failed to initialize.",0

.code
GuiMain PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    xor rcx, rcx
    xor rdx, rdx
    call AgenticMaster_Initialize
    test rax, rax
    jz GuiFail

    call ValidateDMAAlignment
    invoke HealSymbolResolution, addr szSymbol
    lea rcx, szGuiPrompt
    call AgenticMaster_ProcessUserInput
    call AgenticMaster_GetStatus
    invoke RawrXD_Trigger_Chat

    xor rcx, rcx
    lea rdx, szBodyOk
    lea r8, szTitle
    mov r9d, MB_OK or MB_ICONINFORMATION
    call MessageBoxA
    invoke ExitProcess, 0

GuiFail:
    xor rcx, rcx
    lea rdx, szBodyFail
    lea r8, szTitle
    mov r9d, MB_OK or MB_ICONINFORMATION
    call MessageBoxA
    invoke ExitProcess, 1

    add rsp, 32
    pop rbp
    ret
GuiMain ENDP

END
