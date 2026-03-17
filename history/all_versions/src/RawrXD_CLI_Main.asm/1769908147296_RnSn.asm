; RawrXD_CLI_Main.asm - Console Entry Point for Titan Unified
OPTION CASEMAP:NONE

; External symbols from Titan Unified
EXTERNDEF Titan_LoadModel : PROC
EXTERNDEF Titan_RunInferenceStep : PROC
EXTERNDEF Titan_InferenceThread : PROC
EXTERNDEF Math_InitTables : PROC

; Import Kernel32 (Standard)
EXTERNDEF ExitProcess : PROC
EXTERNDEF GetStdHandle : PROC
EXTERNDEF WriteFile : PROC
EXTERNDEF Sleep : PROC
EXTERNDEF CreateThread : PROC

EXTERNDEF StartPipeServer : PROC
EXTERNDEF Pipe_RunServer : PROC

.data
szModelPath BYTE "models/model.gguf", 0
szStartup   BYTE "[+] RawrXD Titan Unified CLI Starting...", 13, 10, 0
szSuccess   BYTE "[+] Model Loaded Successfully. Starting Pipe Server...", 13, 10, 0
szFail      BYTE "[-] Failed to Load Model.", 13, 10, 0
szPipeFail  BYTE "[-] Failed to Create Pipe.", 13, 10, 0

ALIGN 16
TitanContext_Instance BYTE 4096 DUP(0) ; Allocate 4KB for context

.code

; ============================================================================
; Entry Point: mainCRTStartup (Console default)
; ============================================================================
PUBLIC mainCRTStartup
mainCRTStartup PROC FRAME
    sub rsp, 40h                    ; Shadow space
    .endprolog

    ; 1. Print Startup
    lea rcx, szStartup
    call PrintString

    ; 2. Initialize Math Tables
    call Math_InitTables

    ; 3. Load Model
    lea rcx, TitanContext_Instance  ; Context
    lea rdx, szModelPath            ; Path
    call Titan_LoadModel
    
    test eax, eax
    jz @@load_failed
    
    lea rcx, szSuccess
    call PrintString
    
    ; 4. Run Inference (Infinite Loop in Thread, or Step)
    ; For now, just run logic once or loop.
    ; Real logic likely needs input.
    
    ; Call Titan_InferenceThread (Blocking?)
    ; lea rcx, TitanContext_Instance
    ; call Titan_InferenceThread
    
    ; Start the Pipe Server
    lea rcx, TitanContext_Instance
    call StartPipeServer

    jmp @@exit

@@load_failed:
    lea rcx, szFail
    call PrintString
    mov ecx, 1
    call ExitProcess

@@exit:
    mov ecx, 0
    call ExitProcess

mainCRTStartup ENDP

; Helper: PrintString (RCX = String)
PrintString PROC
    push rbx
    sub rsp, 40h                    ; Align stack
    mov rbx, rcx
    
    ; Get length
    xor r10, r10                    ; Length counter
@@len:
    cmp byte ptr [rbx+r10], 0
    je @@print
    inc r10
    jmp @@len
    
@@print:
    ; Get StdOut
    mov rcx, -11                    ; STD_OUTPUT_HANDLE
    call GetStdHandle
    
    mov rcx, rax                    ; Handle
    mov rdx, rbx                    ; Buffer
    mov r8, r10                     ; Len
    lea r9, [rsp+50h]               ; BytesWritten ptr (shadow space + offset)
    mov qword ptr [rsp+20h], 0     ; Overlapped
    call WriteFile
    
    add rsp, 40h
    pop rbx
    ret
PrintString ENDP

END
