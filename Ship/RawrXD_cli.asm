; RawrXD_CLI.asm - Minimal Entry Point with File Loading
; Links with Titan_InferenceCore.obj

OPTION CASEMAP:NONE

EXTERN GGUF_LoadFile : PROC
EXTERN Titan_RunInference : PROC
EXTERN GetCommandLineA : PROC
EXTERN ExitProcess : PROC
EXTERN WriteConsoleA : PROC
EXTERN GetStdHandle : PROC

.code

TransformerCtx STRUCT
    pFileBase          QWORD ?
    cbFileSize         QWORD ?
    pTensorIndex       QWORD ?
    Architecture       DWORD ?
    nLayers            DWORD ?
    nHeads             DWORD ?
    nEmbed             DWORD ?
    nCtx              DWORD ?
TransformerCtx ENDS

PUBLIC main
main PROC
    sub rsp, 128            ; Reserve stack for context and shadow space
    
    ; 1. Get command line
    call GetCommandLineA
    mov rcx, rax
    
    ; 2. Skip executable name (scan for space)
    ; Input: "RawrXD-Agent.exe model.gguf"
    ; This is a very naive parser
    mov rdx, rcx
@scan:
    cmp BYTE PTR [rdx], 0
    je @no_args
    cmp BYTE PTR [rdx], 32 ; Space
    je @found_space
    inc rdx
    jmp @scan
    
@found_space:
    inc rdx              ; Skip space
    
    ; 3. Load Model
    ; RCX = Path (now in RDX), RDX = Context (stack)
    mov rcx, rdx
    lea rdx, [rsp+64]    ; Context struct on stack
    call GGUF_LoadFile
    
    test eax, eax
    jz @load_fail
    
    ; 4. Success?
    ; Just hardcode success logic or print "Loaded"
    ; We need GetStdHandle(-11) -> WriteConsoleA
    
    mov rcx, -11         ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx, rax
    lea rdx, [szSuccess]
    mov r8d, 15          ; Length "Model Loaded!\r\n"
    lea r9, [rsp+56]     ; Bytes written
    mov QWORD PTR [rsp+32], 0
    call WriteConsoleA
    
    ; 5. Run dummy inference step
    lea rcx, [rsp+64]    ; pCtx
    xor edx, edx
    xor r8, r8
    call Titan_RunInference
    
    jmp @exit
    
@load_fail:
    ; Print Fail
    mov rcx, -11
    call GetStdHandle
    mov rcx, rax
    lea rdx, [szFail]
    mov r8d, 12
    lea r9, [rsp+56]
    mov QWORD PTR [rsp+32], 0
    call WriteConsoleA

@no_args:
@exit:
    xor ecx, ecx
    call ExitProcess
main ENDP

.data
szSuccess DB "Model Loaded!", 13, 10, 0
szFail    DB "Load Failed!", 13, 10, 0

END
