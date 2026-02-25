; RawrXD_CLI_Main.asm - Console Entry Point for Titan Unified
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


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
    sub rsp, 28h                    ; Shadow space (Aligned)
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
    
    ; 4. Start Pipe Server
    call StartPipeServer
    test rax, rax
    jz @@pipe_fail
    
    ; 5. Start Inference Thread
    sub rsp, 48 
    mov qword ptr [rsp+40], 0 ; lpThreadId
    mov dword ptr [rsp+32], 0 ; dwCreationFlags
    lea r9, TitanContext_Instance ; lpParameter (Pass Context)
    lea r8, Titan_InferenceThread ; lpStartAddress
    xor rdx, rdx              ; dwStackSize
    xor rcx, rcx              ; lpThreadAttributes
    call CreateThread
    add rsp, 48
    
    ; 6. Run Pipe Server Loop (Blocks Main Thread)
    call Pipe_RunServer
    
    jmp @@exit

RunCLLoop:
    ; Simple read-eval-print loop placeholder replacement
    ; Check if model loaded
    ; If not, print warning
    ; Else call RunInferenceStep with dummy input
    
    mov cx, 5
    call Sleep ; Yield
    
    jmp RunCLLoop

@@load_failed:
    lea rcx, szFail
    call PrintString
    mov ecx, 1
    call ExitProcess

@@pipe_fail:
    lea rcx, szPipeFail
    call PrintString
    mov ecx, 2
    call ExitProcess

@@exit:
    mov ecx, 0
    call ExitProcess
    ret

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
