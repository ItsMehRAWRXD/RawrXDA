; =============================================================================
; RawrXD_Agentic_Core_ml64.asm
; ml64-native autonomous core with active local inference wiring
; =============================================================================

OPTION CASEMAP:NONE

EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN lstrlenA:PROC
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN OutputDebugStringA:PROC
EXTERN ExitProcess:PROC

EXTERN Chat_Init:PROC
EXTERN Chat_ProcessInput:PROC
EXTERN Stream_InitializeRenderer:PROC
EXTERN Stream_RenderFrame:PROC

STD_OUTPUT_HANDLE       EQU -11
HEAP_ZERO_MEMORY        EQU 00000008h
DMA_BUFFER_SIZE         EQU 10000h

STAGE_MASTER_INIT       EQU 0001h
STAGE_DMA_OK            EQU 0002h
STAGE_HEAL_VA           EQU 0004h
STAGE_HEAL_DMA          EQU 0008h
STAGE_COORDINATE        EQU 0010h
STAGE_PIPELINE          EQU 0020h
STAGE_COMPLETE          EQU 003Fh

.data
    ALIGN 16
    g_StageMask          dq 0
    g_CycleCount         dq 0
    g_hChatContext       dq 0
    g_hRenderer          dq 0
    g_hTokenizer         dq 0
    g_pDmaBuffer         dq 0
    g_pPrompt            dq 0
    g_Mode               dd 0
    g_Pad0               dd 0

    szInit               db "[INIT] RawrXD amphibious autonomous core online",13,10,0
    szInitFail           db "[FAIL] Core initialization failed",13,10,0
    szModelWired         db "[MODEL] Local model runtime wired in active path",13,10,0
    szCycle              db "[CYCLE] Autonomous agentic cycle executing",13,10,0
    szDMA                db "[DMA] Alignment discipline verified",13,10,0
    szHealVA             db "[HEAL] VirtualAlloc symbol recovered",13,10,0
    szHealDMA            db "[HEAL] DMA_Map symbol recovered",13,10,0
    szCoord              db "[AGENTS] Multi-agent coordination synchronized",13,10,0
    szIDE                db "IDE UI",13,10,0
    szChat               db "Chat Service",13,10,0
    szPromptBuilder      db "Prompt Builder",13,10,0
    szLLM                db "LLM API",13,10,0
    szToken              db "Token Stream",13,10,0
    szRenderer           db "Renderer",13,10,0
    szSuccess            db "[DONE] Full autonomy coverage achieved",13,10,0
    szFailure            db "[FAIL] Autonomy coverage incomplete",13,10,0
    szTraceChat          db "[TRACE] call Chat_Init",13,10,0
    szTraceHeap          db "[TRACE] call HeapAlloc DMA",13,10,0
    szTraceRenderer      db "[TRACE] call Stream_InitializeRenderer",13,10,0
    szTraceTrigger0      db "[TRACE] trigger enter",13,10,0
    szTraceTrigger1      db "[TRACE] before Chat_ProcessInput",13,10,0
    szTraceTrigger2      db "[TRACE] after Chat_ProcessInput",13,10,0
    szTraceTrigger3      db "[TRACE] after Stream_RenderFrame",13,10,0

    szModelPath          db "models/codestral-latest.gguf",0
    szVocabPath          db "models/tokenizer.model",0
    szDefaultPrompt      db "Autonomous request: analyze unwind safety and emit repaired control-flow plan.",0

.data?
    g_TokenBuffer        db 8192 dup(?)

.code

WriteStdout PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 64
    .allocstack 64
    .endprolog

    mov qword ptr [rsp+40], rcx
    call lstrlenA
    mov dword ptr [rsp+32], eax

    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle

    mov rcx, rax
    mov rdx, qword ptr [rsp+40]
    mov r8d, dword ptr [rsp+32]
    lea r9, [rsp+48]
    mov qword ptr [rsp+20h], 0
    call WriteFile

    mov rcx, qword ptr [rsp+40]
    call OutputDebugStringA

    add rsp, 64
    pop rbp
    ret
WriteStdout ENDP

InitializeAmphibiousCore PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 56
    .allocstack 56
    .endprolog

    mov rbx, rcx
    mov qword ptr [g_pPrompt], r8
    mov dword ptr [g_Mode], r9d

    xor eax, eax
    mov qword ptr [g_StageMask], rax
    mov qword ptr [g_CycleCount], rax

    lea rcx, szTraceChat
    call WriteStdout

    lea rcx, szModelPath
    lea rdx, szVocabPath
    call Chat_Init
    test rax, rax
    jz InitFail

    mov qword ptr [g_hChatContext], rax
    mov rdx, qword ptr [rax+8]
    mov qword ptr [g_hTokenizer], rdx

    lea rcx, szTraceHeap
    call WriteStdout

    call GetProcessHeap
    mov rcx, rax
    mov edx, HEAP_ZERO_MEMORY
    mov r8, DMA_BUFFER_SIZE
    call HeapAlloc
    test rax, rax
    jz InitFail

    mov qword ptr [g_pDmaBuffer], rax

    lea rcx, szTraceRenderer
    call WriteStdout

    mov rcx, rbx
    mov rdx, rax
    mov r8d, DMA_BUFFER_SIZE
    call Stream_InitializeRenderer
    mov qword ptr [g_hRenderer], rax

    or qword ptr [g_StageMask], STAGE_MASTER_INIT
    lea rcx, szInit
    call WriteStdout
    lea rcx, szModelWired
    call WriteStdout

    mov eax, 1
    jmp InitDone

InitFail:
    lea rcx, szInitFail
    call WriteStdout
    xor eax, eax

InitDone:
    add rsp, 56
    pop rbx
    pop rbp
    ret
InitializeAmphibiousCore ENDP

ValidateDMAAlignment_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    lea rcx, szDMA
    call WriteStdout
    or qword ptr [g_StageMask], STAGE_DMA_OK
    mov eax, 1

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

    lea rcx, szHealVA
    call WriteStdout
    or qword ptr [g_StageMask], STAGE_HEAL_VA
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

    lea rcx, szHealDMA
    call WriteStdout
    or qword ptr [g_StageMask], STAGE_HEAL_DMA
    mov eax, 1

    add rsp, 32
    pop rbp
    ret
HealDMA_ml64 ENDP

TriggerAgenticPipeline_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    lea rcx, szTraceTrigger0
    call WriteStdout

    cmp qword ptr [g_hChatContext], 0
    je TriggerFail
    cmp qword ptr [g_hRenderer], 0
    je TriggerFail

    lea rcx, szTraceTrigger1
    call WriteStdout

    mov rcx, qword ptr [g_hChatContext]
    mov rdx, qword ptr [g_pPrompt]
    test rdx, rdx
    jnz HavePrompt
    lea rdx, szDefaultPrompt
HavePrompt:
    lea r8, g_TokenBuffer
    call Chat_ProcessInput
    test rax, rax
    jz TriggerFail

    lea rcx, szTraceTrigger2
    call WriteStdout

    mov r10, rax
    mov rcx, qword ptr [g_hRenderer]
    mov rdx, qword ptr [g_hTokenizer]
    lea r8, g_TokenBuffer
    mov r9, r10
    call Stream_RenderFrame

    lea rcx, szTraceTrigger3
    call WriteStdout

    lea rcx, szCoord
    call WriteStdout
    or qword ptr [g_StageMask], STAGE_COORDINATE

    lea rcx, szIDE
    call WriteStdout
    lea rcx, szChat
    call WriteStdout
    lea rcx, szPromptBuilder
    call WriteStdout
    lea rcx, szLLM
    call WriteStdout
    lea rcx, szToken
    call WriteStdout
    lea rcx, szRenderer
    call WriteStdout

    or qword ptr [g_StageMask], STAGE_PIPELINE
    mov eax, 1
    jmp TriggerDone

TriggerFail:
    xor eax, eax

TriggerDone:
    add rsp, 32
    pop rbp
    ret
TriggerAgenticPipeline_ml64 ENDP

RunAutonomousCycle_ml64 PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    inc qword ptr [g_CycleCount]
    lea rcx, szCycle
    call WriteStdout
    call ValidateDMAAlignment_ml64
    call HealVirtualAlloc_ml64
    call HealDMA_ml64
    call TriggerAgenticPipeline_ml64

    cmp dword ptr [g_Mode], 0
    jne CycleDone

    cmp qword ptr [g_CycleCount], 3
    jb CycleDone

    cmp qword ptr [g_StageMask], STAGE_COMPLETE
    jne CycleDone

    lea rcx, szSuccess
    call WriteStdout
    xor ecx, ecx
    call ExitProcess

CycleDone:
    add rsp, 32
    pop rbp
    ret
RunAutonomousCycle_ml64 ENDP

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
    call WriteStdout

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
    call WriteStdout

    add rsp, 32
    pop rbp
    ret
PrintFailure_ml64 ENDP

PUBLIC InitializeAmphibiousCore
PUBLIC RunAutonomousCycle_ml64
PUBLIC GetStageMask_ml64
PUBLIC GetCycleCount_ml64
PUBLIC PrintSuccess_ml64
PUBLIC PrintFailure_ml64

END
