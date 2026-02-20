; RawrXD_CLI_Titan.asm - Headless Titan Ring Consumer (Option A)
; Assemble: ml64 /c /Zi RawrXD_CLI_Titan.asm
; Link: link /SUBSYSTEM:CONSOLE /OUT:RawrXD-Agent.exe RawrXD_CLI_Titan.obj Titan_Streaming_Orchestrator_Fixed.obj kernel32.lib

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; ============================================================================
; EXTERNAL IMPORTS - Win32 API
; ============================================================================
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN GetCommandLineA:PROC
EXTERN ExitProcess:PROC
EXTERN Sleep:PROC
EXTERN lstrlenA:PROC

; ============================================================================
; EXTERNAL IMPORTS - Titan High-Level API (from Orchestrator)
; ============================================================================
EXTERN Titan_InitOrchestrator:PROC
EXTERN Titan_CreateContext:PROC
EXTERN Titan_LoadModel_GGUF:PROC
EXTERN Titan_BeginStreamingInference:PROC
EXTERN Titan_ConsumeToken:PROC
EXTERN Titan_Shutdown:PROC

; ============================================================================
; CONSTANTS
; ============================================================================
STD_OUTPUT_HANDLE   EQU -11
STD_INPUT_HANDLE    EQU -10
TOKEN_BUF_SIZE      EQU 4096
PROMPT_BUF_SIZE     EQU 8192

; ============================================================================
; DATA SECTION
; ============================================================================
.DATA
    szBanner        BYTE 13,10
                    BYTE "======================================",13,10
                    BYTE " RawrXD Agent [Titan DMA Stream v5.0]",13,10
                    BYTE " 64MB Ring | AVX-512 | Zero-Copy",13,10
                    BYTE "======================================",13,10,13,10,0
    szBannerLen     EQU $ - szBanner - 1
    
    szInitOK        BYTE "[OK] Titan Orchestrator initialized",13,10,0
    szInitFail      BYTE "[FAIL] Titan Orchestrator init failed",13,10,0
    szCtxOK         BYTE "[OK] Context created",13,10,0
    szCtxFail       BYTE "[FAIL] Context creation failed",13,10,0
    szModelOK       BYTE "[OK] Model loaded",13,10,0
    szModelFail     BYTE "[FAIL] Model load failed",13,10,0
    szStreamStart   BYTE "[OK] Streaming started",13,10,0
    szStreamFail    BYTE "[FAIL] Streaming start failed",13,10,0
    szStreamDone    BYTE 13,10,"[DONE] Inference complete",13,10,0
    szPromptHdr     BYTE 13,10,"Prompt> ",0
    szNewline       BYTE 13,10,0
    
    ; Default model path (override via command line)
    szDefaultModel  BYTE "D:\rawrxd\models\phi-3-mini.gguf",0
    
    ; Default prompt for testing
    szDefaultPrompt BYTE "Hello, I am RawrXD Agent. How can I help you today?",0

.DATA?
    hStdOut         QWORD ?
    hStdIn          QWORD ?
    hContext        QWORD ?
    dwWritten       DWORD ?
    dwRead          DWORD ?
    tokenBuf        BYTE TOKEN_BUF_SIZE DUP(?)
    promptBuf       BYTE PROMPT_BUF_SIZE DUP(?)
    modelPath       BYTE 260 DUP(?)

; ============================================================================
; CODE SECTION
; ============================================================================
.CODE

; ----------------------------------------------------------------------------
; PrintString - Write null-terminated string to stdout
; RCX = string pointer
; ----------------------------------------------------------------------------
PrintString PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rsi, rcx
    
    ; Get string length
    mov rcx, rsi
    call lstrlenA
    mov rbx, rax            ; Length
    
    ; WriteFile(hStdOut, string, len, &written, NULL)
    mov rcx, hStdOut
    mov rdx, rsi
    mov r8, rbx
    lea r9, dwWritten
    mov QWORD PTR [rsp+32], 0
    call WriteFile
    
    add rsp, 40
    pop rsi
    pop rbx
    ret
PrintString ENDP

; ----------------------------------------------------------------------------
; main - Entry point
; ----------------------------------------------------------------------------
main PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    ; Get console handles
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov hStdOut, rax
    
    mov ecx, STD_INPUT_HANDLE
    call GetStdHandle
    mov hStdIn, rax
    
    ; Print banner
    lea rcx, szBanner
    call PrintString
    
    ; ========================================
    ; Step 1: Initialize Titan Orchestrator
    ; ========================================
    call Titan_InitOrchestrator
    test rax, rax
    jnz @@init_failed
    
    lea rcx, szInitOK
    call PrintString
    jmp @@init_ok
    
@@init_failed:
    lea rcx, szInitFail
    call PrintString
    mov eax, 1
    jmp @@exit
    
@@init_ok:
    ; ========================================
    ; Step 2: Create Context
    ; ========================================
    call Titan_CreateContext
    test rax, rax
    jz @@ctx_failed
    
    mov hContext, rax
    lea rcx, szCtxOK
    call PrintString
    jmp @@ctx_ok
    
@@ctx_failed:
    lea rcx, szCtxFail
    call PrintString
    jmp @@shutdown
    
@@ctx_ok:
    ; ========================================
    ; Step 3: Load Model (use default or argv[1])
    ; ========================================
    ; For now, use default model path
    mov rcx, hContext
    lea rdx, szDefaultModel
    call Titan_LoadModel_GGUF
    test eax, eax
    jz @@model_failed
    
    lea rcx, szModelOK
    call PrintString
    jmp @@model_ok
    
@@model_failed:
    lea rcx, szModelFail
    call PrintString
    jmp @@shutdown
    
@@model_ok:
    ; ========================================
    ; Step 4: Read prompt from stdin or use default
    ; ========================================
    lea rcx, szPromptHdr
    call PrintString
    
    ; Read from stdin
    mov rcx, hStdIn
    lea rdx, promptBuf
    mov r8d, PROMPT_BUF_SIZE - 1
    lea r9, dwRead
    mov QWORD PTR [rsp+32], 0
    call ReadFile
    
    ; Check if we got input
    mov eax, dwRead
    test eax, eax
    jz @@use_default_prompt
    
    ; Null-terminate
    lea rcx, promptBuf
    add rcx, rax
    mov BYTE PTR [rcx], 0
    
    ; Use user prompt
    lea r12, promptBuf
    mov r13d, dwRead
    jmp @@start_inference
    
@@use_default_prompt:
    lea r12, szDefaultPrompt
    lea rcx, szDefaultPrompt
    call lstrlenA
    mov r13d, eax
    
@@start_inference:
    ; ========================================
    ; Step 5: Begin Streaming Inference
    ; ========================================
    mov rcx, hContext
    mov rdx, r12            ; Prompt
    mov r8, r13             ; PromptLen
    call Titan_BeginStreamingInference
    test eax, eax
    jz @@stream_failed
    
    lea rcx, szStreamStart
    call PrintString
    
    lea rcx, szNewline
    call PrintString
    jmp @@consume_loop
    
@@stream_failed:
    lea rcx, szStreamFail
    call PrintString
    jmp @@shutdown
    
@@consume_loop:
    ; ========================================
    ; Step 6: Consume Tokens Until Complete
    ; ========================================
    mov rcx, hContext
    lea rdx, tokenBuf
    mov r8d, TOKEN_BUF_SIZE - 1
    call Titan_ConsumeToken
    
    test rax, rax
    jz @@check_done         ; 0 = empty or complete
    
    mov r14, rax            ; Bytes read
    
    ; Null-terminate token
    lea rcx, tokenBuf
    add rcx, rax
    mov BYTE PTR [rcx], 0
    
    ; Write token to stdout
    mov rcx, hStdOut
    lea rdx, tokenBuf
    mov r8, r14
    lea r9, dwWritten
    mov QWORD PTR [rsp+32], 0
    call WriteFile
    
    jmp @@consume_loop
    
@@check_done:
    ; Small delay before re-checking (cooperative yield)
    mov ecx, 10
    call Sleep
    
    ; Check if context state is COMPLETE (CTX_STATE_COMPLETE = 3)
    mov rax, hContext
    cmp DWORD PTR [rax+4], 3     ; Context.State == COMPLETE?
    jne @@consume_loop
    
    ; Done!
    lea rcx, szStreamDone
    call PrintString
    
@@shutdown:
    ; ========================================
    ; Step 7: Cleanup
    ; ========================================
    call Titan_Shutdown
    xor eax, eax
    
@@exit:
    add rsp, 64
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    
    mov ecx, eax
    call ExitProcess
    ret
main ENDP

END
