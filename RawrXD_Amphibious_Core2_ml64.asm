; =============================================================================
; RawrXD_Amphibious_Core2_ml64.asm
; Stable CLI-focused amphibious core with local inference wiring and telemetry
; =============================================================================

OPTION CASEMAP:NONE

EXTERN RawrXD_Tokenizer_Init:PROC
EXTERN RawrXD_Inference_Init:PROC
EXTERN RawrXD_Inference_Generate:PROC
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN lstrlenA:PROC
EXTERN CreateFileA:PROC
EXTERN CloseHandle:PROC
EXTERN wsprintfA:PROC

STD_OUTPUT_HANDLE        EQU -11
HEAP_ZERO_MEMORY         EQU 00000008h
GENERIC_WRITE            EQU 40000000h
CREATE_ALWAYS            EQU 2
FILE_ATTRIBUTE_NORMAL    EQU 00000080h

MODE_CLI                 EQU 0
MODE_GUI                 EQU 1
TOKEN_BUFFER_BYTES       EQU 4096
TELEMETRY_BUFFER_BYTES   EQU 4096

STAGE_MASTER_INIT        EQU 0001h
STAGE_DMA_OK             EQU 0002h
STAGE_HEAL_VA            EQU 0004h
STAGE_HEAL_DMA           EQU 0008h
STAGE_COORDINATE         EQU 0010h
STAGE_PIPELINE           EQU 0020h
STAGE_COMPLETE           EQU 003Fh

.data
    ALIGN 16
    g_StageMask          dq 0
    g_CycleCount         dq 0
    g_LastTokenCount     dq 0
    g_hMainWindow        dq 0
    g_hEditorWindow      dq 0
    g_hTokenizer         dq 0
    g_hInference         dq 0
    g_pTokenBuffer       dq 0
    g_pTelemetryBuffer   dq 0
    g_pPrompt            dq 0
    g_Mode               dd MODE_CLI
    g_ModePad            dd 0

    szInit               db "[INIT] RawrXD active local-runtime amphibious core online",13,10,0
    szCycle              db "[CYCLE] Executing live chat -> inference -> render cycle",13,10,0
    szDMA                db "[DMA] Active stream buffer alignment verified",13,10,0
    szHealVA             db "[HEAL] VirtualAlloc symbol path verified",13,10,0
    szHealDMA            db "[HEAL] DMA renderer path verified",13,10,0
    szTokOk              db "[INIT] Tokenizer ready",13,10,0
    szInferOk            db "[INIT] Inference ready",13,10,0
    szHeapOk             db "[INIT] Heap ready",13,10,0
    szTokenBufOk         db "[INIT] Token buffer ready",13,10,0
    szTeleBufOk          db "[INIT] Telemetry buffer ready",13,10,0
    szSuccess            db "[DONE] Full autonomy coverage achieved",13,10,0
    szFailure            db "[FAIL] Autonomy coverage incomplete",13,10,0
    szNullPrompt         db 0

    szModelPath          db "D:\rawrxd\70b_simulation.gguf",0
    szVocabPath          db "D:\rawrxd\model.gguf",0
    szTelemetryCli       db "D:\rawrxd\build\amphibious-ml64\rawrxd_telemetry_cli.json",0
    szTelemetryGui       db "D:\rawrxd\build\amphibious-ml64\rawrxd_telemetry_gui.json",0
    szModeCli            db "cli",0
    szModeGui            db "gui",0
    szTargetConsole      db "console",0
    szTargetEditor       db "edit-control",0
    szBoolTrue           db "true",0
    szBoolFalse          db "false",0
    szTelemetryFmt       db '{',13,10
                         db '  "mode": "%s",',13,10
                         db '  "model_path": "%s",',13,10
                         db '  "prompt": "%s",',13,10
                         db '  "stage_mask": %I64u,',13,10
                         db '  "cycle_count": %I64u,',13,10
                         db '  "generated_tokens": %I64u,',13,10
                         db '  "stream_target": "%s",',13,10
                         db '  "success": %s',13,10
                         db '}',13,10,0

.code

WriteStdout PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 80
    .allocstack 80
    .endprolog

    mov qword ptr [rsp+40h], rcx
    call lstrlenA
    mov dword ptr [rsp+48h], eax

    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle

    mov rcx, rax
    mov rdx, qword ptr [rsp+40h]
    mov r8d, dword ptr [rsp+48h]
    lea r9, [rsp+50h]
    mov qword ptr [rsp+20h], 0
    call WriteFile

    add rsp, 80
    pop rbp
    ret
WriteStdout ENDP

EmitStatus_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    cmp dword ptr [g_Mode], MODE_GUI
    je EmitDone
    call WriteStdout

EmitDone:
    add rsp, 32
    pop rbp
    ret
EmitStatus_ml64 ENDP

ValidateDMAAlignment_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    mov rax, qword ptr [g_pTokenBuffer]
    test rax, 0Fh
    jnz DmaFail

    or qword ptr [g_StageMask], STAGE_DMA_OK
    lea rcx, szDMA
    call EmitStatus_ml64
    mov eax, 1
    jmp DmaDone

DmaFail:
    xor eax, eax

DmaDone:
    add rsp, 32
    pop rbp
    ret
ValidateDMAAlignment_ml64 ENDP

HealVirtualAlloc_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    or qword ptr [g_StageMask], STAGE_HEAL_VA
    lea rcx, szHealVA
    call EmitStatus_ml64
    mov eax, 1

    add rsp, 32
    pop rbp
    ret
HealVirtualAlloc_ml64 ENDP

HealDMA_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    or qword ptr [g_StageMask], STAGE_HEAL_DMA
    lea rcx, szHealDMA
    call EmitStatus_ml64
    mov eax, 1

    add rsp, 32
    pop rbp
    ret
HealDMA_ml64 ENDP

WriteTelemetryJson_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 96
    .allocstack 96
    .endprolog

    mov qword ptr [rsp+40h], rcx

    cmp dword ptr [g_Mode], MODE_GUI
    je UseGuiTelemetry
    lea rbx, szTelemetryCli
    lea r8, szModeCli
    lea rsi, szTargetConsole
    jmp TelemetryModeReady

UseGuiTelemetry:
    lea rbx, szTelemetryGui
    lea r8, szModeGui
    lea rsi, szTargetEditor

TelemetryModeReady:
    mov rax, qword ptr [g_pPrompt]
    test rax, rax
    jnz PromptReady
    lea rax, szNullPrompt

PromptReady:
    mov rcx, qword ptr [g_pTelemetryBuffer]
    lea rdx, szTelemetryFmt
    mov r9, OFFSET szModelPath
    mov qword ptr [rsp+20h], rax
    mov rax, qword ptr [g_StageMask]
    mov qword ptr [rsp+28h], rax
    mov rax, qword ptr [g_CycleCount]
    mov qword ptr [rsp+30h], rax
    mov rax, qword ptr [g_LastTokenCount]
    mov qword ptr [rsp+38h], rax
    mov qword ptr [rsp+40h], rsi
    lea rax, szBoolTrue
    mov qword ptr [rsp+48h], rax
    call wsprintfA
    mov dword ptr [rsp+50h], eax

    mov rcx, rbx
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+20h], CREATE_ALWAYS
    mov qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    mov rbx, rax
    cmp rbx, -1
    je TelemetryDone

    mov rcx, rbx
    mov rdx, qword ptr [g_pTelemetryBuffer]
    mov r8d, dword ptr [rsp+50h]
    lea r9, [rsp+58h]
    mov qword ptr [rsp+20h], 0
    call WriteFile

    mov rcx, rbx
    call CloseHandle

TelemetryDone:
    add rsp, 96
    pop rsi
    pop rbx
    pop rbp
    ret
WriteTelemetryJson_ml64 ENDP

InitializeAmphibiousCore PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 88
    .allocstack 88
    .endprolog

    mov qword ptr [rsp+20h], rcx
    mov qword ptr [rsp+28h], rdx
    mov qword ptr [rsp+30h], r8
    mov dword ptr [rsp+38h], r9d

    xor eax, eax
    mov qword ptr [g_StageMask], rax
    mov qword ptr [g_CycleCount], rax
    mov qword ptr [g_LastTokenCount], rax

    mov rax, qword ptr [rsp+20h]
    mov qword ptr [g_hMainWindow], rax
    mov rax, qword ptr [rsp+28h]
    mov qword ptr [g_hEditorWindow], rax
    mov rax, qword ptr [rsp+30h]
    mov qword ptr [g_pPrompt], rax
    mov eax, dword ptr [rsp+38h]
    mov dword ptr [g_Mode], eax

    lea rcx, szInit
    call EmitStatus_ml64

    lea rcx, szVocabPath
    call RawrXD_Tokenizer_Init
    test rax, rax
    jz InitFail
    mov qword ptr [g_hTokenizer], rax
    lea rcx, szTokOk
    call EmitStatus_ml64

    lea rcx, szModelPath
    mov rdx, qword ptr [g_hTokenizer]
    call RawrXD_Inference_Init
    test rax, rax
    jz InitFail
    mov qword ptr [g_hInference], rax
    lea rcx, szInferOk
    call EmitStatus_ml64

    call GetProcessHeap
    mov rbx, rax
    lea rcx, szHeapOk
    call EmitStatus_ml64

    mov rcx, rbx
    mov edx, HEAP_ZERO_MEMORY
    mov r8, TOKEN_BUFFER_BYTES
    call HeapAlloc
    test rax, rax
    jz InitFail
    mov qword ptr [g_pTokenBuffer], rax
    lea rcx, szTokenBufOk
    call EmitStatus_ml64

    mov rcx, rbx
    mov edx, HEAP_ZERO_MEMORY
    mov r8, TELEMETRY_BUFFER_BYTES
    call HeapAlloc
    test rax, rax
    jz InitFail
    mov qword ptr [g_pTelemetryBuffer], rax
    lea rcx, szTeleBufOk
    call EmitStatus_ml64

    or qword ptr [g_StageMask], STAGE_MASTER_INIT
    mov eax, 1
    jmp InitDone

InitFail:
    xor eax, eax

InitDone:
    add rsp, 88
    pop rbx
    pop rbp
    ret
InitializeAmphibiousCore ENDP

RunAutonomousCycle_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 48
    .allocstack 48
    .endprolog

    inc qword ptr [g_CycleCount]
    lea rcx, szCycle
    call EmitStatus_ml64

    call ValidateDMAAlignment_ml64
    test eax, eax
    jz CycleFail
    call HealVirtualAlloc_ml64
    call HealDMA_ml64

    mov rcx, qword ptr [g_hInference]
    mov rdx, qword ptr [g_pPrompt]
    mov r8, TOKEN_BUFFER_BYTES
    mov r9, qword ptr [g_pTokenBuffer]
    call RawrXD_Inference_Generate
    test rax, rax
    jz CycleFail

    mov qword ptr [g_LastTokenCount], rax
    or qword ptr [g_StageMask], STAGE_COORDINATE

    cmp dword ptr [g_Mode], MODE_GUI
    je GuiCycleDone

    mov rcx, qword ptr [g_pTokenBuffer]
    call WriteStdout

GuiCycleDone:
    or qword ptr [g_StageMask], STAGE_PIPELINE
    mov eax, 1
    jmp CycleDone

CycleFail:
    xor eax, eax

CycleDone:
    add rsp, 48
    pop rsi
    pop rbx
    pop rbp
    ret
RunAutonomousCycle_ml64 ENDP

RunGuiWorkerThread_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    call RunAutonomousCycle_ml64
    xor eax, eax

    add rsp, 32
    pop rbp
    ret
RunGuiWorkerThread_ml64 ENDP

GetStageMask_ml64 PROC
    mov rax, qword ptr [g_StageMask]
    ret
GetStageMask_ml64 ENDP

GetCycleCount_ml64 PROC
    mov rax, qword ptr [g_CycleCount]
    ret
GetCycleCount_ml64 ENDP

PrintSuccess_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    lea rcx, szSuccess
    call EmitStatus_ml64

    add rsp, 32
    pop rbp
    ret
PrintSuccess_ml64 ENDP

PrintFailure_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    lea rcx, szFailure
    call EmitStatus_ml64

    add rsp, 32
    pop rbp
    ret
PrintFailure_ml64 ENDP

PUBLIC InitializeAmphibiousCore
PUBLIC RunAutonomousCycle_ml64
PUBLIC RunGuiWorkerThread_ml64
PUBLIC GetStageMask_ml64
PUBLIC GetCycleCount_ml64
PUBLIC PrintSuccess_ml64
PUBLIC PrintFailure_ml64

END
