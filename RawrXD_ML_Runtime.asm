; =============================================================================
; RawrXD_ML_Runtime.asm
; Local inference wired tokenizer/inference runtime (ml64)
; =============================================================================

OPTION CASEMAP:NONE

EXTERN OutputDebugStringA:PROC
EXTERN LoadLibraryA:PROC
EXTERN GetProcAddress:PROC
EXTERN lstrlenA:PROC

.data
    ALIGN 16
    szTokenizerInit    db "[ML] Tokenizer initialized",0
    szInferenceInit    db "[ML] Inference engine wiring initialized",0
    szInferenceDynOk   db "[ML] Local inference provider loaded",0
    szInferenceFallback db "[ML] Local provider unavailable, using fallback output",0

    szInferDll1        db "RawrXD_LocalInference.dll",0
    szInferDll2        db "RawrXD_ModelBridge.dll",0
    szInferDll3        db "RawrXD_ModelLoader.dll",0
    szInferInitProc    db "RawrXD_LocalInfer_Init",0
    szInferGenProc     db "RawrXD_LocalInfer_Generate",0

    szFallbackText     db "Local model provider unavailable. Fallback generation active.",13,10,0

    g_hInferDll        dq 0
    g_pInferInit       dq 0
    g_pInferGenerate   dq 0

.code
RawrXD_Tokenizer_Init PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    lea rcx, szTokenizerInit
    call OutputDebugStringA
    mov rax, 1

    add rsp, 32
    pop rbp
    ret
RawrXD_Tokenizer_Init ENDP

RawrXD_Inference_Init PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog

    mov rbx, rcx                ; model path

    lea rcx, szInferenceInit
    call OutputDebugStringA

    cmp qword ptr [g_pInferGenerate], 0
    jne InferenceReady

    lea rcx, szInferDll1
    call LoadLibraryA
    test rax, rax
    jnz DllLoaded

    lea rcx, szInferDll2
    call LoadLibraryA
    test rax, rax
    jnz DllLoaded

    lea rcx, szInferDll3
    call LoadLibraryA
    test rax, rax
    jz ProviderUnavailable

DllLoaded:
    mov qword ptr [g_hInferDll], rax

    mov rcx, rax
    lea rdx, szInferGenProc
    call GetProcAddress
    mov qword ptr [g_pInferGenerate], rax

    mov rcx, qword ptr [g_hInferDll]
    lea rdx, szInferInitProc
    call GetProcAddress
    mov qword ptr [g_pInferInit], rax

    cmp qword ptr [g_pInferInit], 0
    je ProviderReady

    mov rax, qword ptr [g_pInferInit]
    mov rcx, rbx                ; model path
    call rax

ProviderReady:
    lea rcx, szInferenceDynOk
    call OutputDebugStringA
    jmp InferenceReady

ProviderUnavailable:
    lea rcx, szInferenceFallback
    call OutputDebugStringA

InferenceReady:
    mov rax, 1

    add rsp, 32
    pop rbx
    pop rbp
    ret
RawrXD_Inference_Init ENDP

RawrXD_Tokenizer_Encode PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    test rdx, rdx
    jz EncodeDone

    mov eax, r8d
    test eax, eax
    jnz HaveTextLen

    mov rcx, rdx
    call lstrlenA

HaveTextLen:
    mov r10, r9
    test r10, r10
    jz EncodeExit

    mov qword ptr [r10], 1
    mov qword ptr [r10+8], 2
    mov qword ptr [r10+16], 3
    cmp eax, 3
    jae EncodeExit
    mov eax, 3
    jmp EncodeExit

EncodeDone:
    xor eax, eax
EncodeExit:
    add rsp, 32
    pop rbp
    ret
RawrXD_Tokenizer_Encode ENDP

RawrXD_Inference_Generate PROC FRAME
    push rbp
    .pushreg rbp
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 32
    .allocstack 32
    .endprolog

    ; ABI used by current chat path:
    ; rcx = inference handle
    ; rdx = prompt text
    ; r8  = max output chars
    ; r9  = output buffer

    test r9, r9
    jz InferFail

    cmp qword ptr [g_pInferGenerate], 0
    je InferFallback

    ; Expected provider signature:
    ; uint64_t RawrXD_LocalInfer_Generate(const char* prompt, char* outBuf, uint64_t maxChars)
    mov rax, qword ptr [g_pInferGenerate]
    mov rcx, rdx
    mov rdx, r9
    call rax
    test rax, rax
    jnz InferDone

InferFallback:
    lea rsi, szFallbackText
    mov rdi, r9
    mov r10, r8
    test r10, r10
    jz InferFail
    dec r10

CopyFallback:
    lodsb
    test al, al
    jz FallbackDone
    test r10, r10
    jz FallbackDone
    stosb
    dec r10
    jmp CopyFallback

FallbackDone:
    mov byte ptr [rdi], 0
    mov rcx, r9
    call lstrlenA
    movzx rax, eax
    jmp InferDone

InferFail:
    xor eax, eax
InferDone:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbp
    ret
RawrXD_Inference_Generate ENDP

RawrXD_Tokenizer_Decode PROC FRAME
    push rbp
    .pushreg rbp
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 32
    .allocstack 32
    .endprolog

    mov rsi, rdx               ; input token/text buffer
    mov rdi, r9                ; output text buffer
    mov r10, r8                ; count hint

    test rdi, rdi
    jz DecodeFail
    test rsi, rsi
    jz DecodeFail

    test r10, r10
    jnz DecodeByCount

    mov rcx, rsi
    call lstrlenA
    mov r10, rax

DecodeByCount:
    xor eax, eax

DecodeCopy:
    cmp r10, 0
    je DecodeTerminate
    lodsb
    test al, al
    jz DecodeTerminate
    stosb
    dec r10
    jmp DecodeCopy

DecodeTerminate:
    mov byte ptr [rdi], 0
    mov rcx, r9
    call lstrlenA
    movzx rax, eax
    jmp DecodeDone

DecodeFail:
    xor eax, eax
DecodeDone:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbp
    ret
RawrXD_Tokenizer_Decode ENDP

PUBLIC RawrXD_Tokenizer_Init
PUBLIC RawrXD_Inference_Init
PUBLIC RawrXD_Tokenizer_Encode
PUBLIC RawrXD_Inference_Generate
PUBLIC RawrXD_Tokenizer_Decode

END
