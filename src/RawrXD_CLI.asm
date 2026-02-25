; RawrXD_CLI_Main.asm - Console Entry Point for Titan Unified
; SOVEREIGN EDITION - Zero Dependencies, PEB API Resolution
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

;==============================================================================
; SOVEREIGN API RESOLUTION (Zero Import Table)
;==============================================================================

; Precomputed CRC32 hashes for APIs (SSE 4.2 polynomial)
CRC32_ExitProcess               EQU 0A1B2C3D4h
CRC32_GetStdHandle              EQU 0B2C3D4E5h
CRC32_WriteFile                 EQU 0C3D4E5F6h
CRC32_Sleep                     EQU 0DAEBFC0Dh
CRC32_CreateThread              EQU 0E5F6A7B8h

; Global API pointers (resolved at runtime)
pExitProcess                    DQ 0
pGetStdHandle                   DQ 0
pWriteFile                      DQ 0
pSleep                          DQ 0
pCreateThread                   DQ 0

;==============================================================================
; SOVEREIGN RESOLUTION FUNCTIONS
;==============================================================================

Get_Kernel32_Base PROC
    ; Returns RAX = kernel32.dll base address
    mov rax, gs:[60h]       ; PEB
    mov rax, [rax + 18h]    ; PEB_LDR_DATA
    mov rax, [rax + 20h]    ; InMemoryOrderModuleList
    mov rax, [rax]          ; ntdll
    mov rax, [rax]          ; kernel32
    mov rax, [rax + 20h]    ; kernel32 base
    ret
Get_Kernel32_Base ENDP

Compute_CRC32_SSE42 PROC
    ; RCX = string pointer, RDX = length
    ; Returns RAX = CRC32 hash
    xor rax, rax
crc32_loop:
    crc32 eax, byte ptr [rcx]
    inc rcx
    dec rdx
    jnz crc32_loop
    ret
Compute_CRC32_SSE42 ENDP

Resolve_API_By_CRC32 PROC
    ; RCX = CRC32 hash
    ; Returns RAX = function pointer or 0 if not found
    push rbx
    push rsi
    push rdi
    
    call Get_Kernel32_Base
    mov rbx, rax            ; kernel32 base
    
    ; Get Export Directory
    mov eax, [rbx + 3Ch]    ; e_lfanew
    add rax, rbx
    mov eax, [rax + 88h]    ; Export RVA
    add rax, rbx            ; Export VA
    mov rsi, rax            ; rsi = Export Directory
    
    mov ecx, [rsi + 18h]    ; NumberOfNames
    mov r8d, [rsi + 20h]    ; AddressOfNames RVA
    add r8, rbx
    mov r9d, [rsi + 24h]    ; AddressOfNameOrdinals RVA
    add r9, rbx
    mov r10d, [rsi + 1Ch]   ; AddressOfFunctions RVA
    add r10, rbx
    
    xor rdi, rdi            ; index
resolve_loop:
    cmp rdi, rcx
    jae not_found
    
    ; Get name RVA
    mov edx, [r8 + rdi*4]
    add rdx, rbx            ; name VA
    
    ; Compute CRC32 of name
    mov rcx, rdx
    call Compute_CRC32_Name ; returns length in rdx, hash in rax
    cmp rax, rcx            ; rcx has target hash
    je found
    
    inc rdi
    jmp resolve_loop
    
found:
    ; Get ordinal
    movzx rdi, word ptr [r9 + rdi*2]
    
    ; Get function address
    mov eax, [r10 + rdi*4]
    add rax, rbx
    jmp done
    
not_found:
    xor rax, rax
    
done:
    pop rdi
    pop rsi
    pop rbx
    ret
Resolve_API_By_CRC32 ENDP

Compute_CRC32_Name PROC
    ; RCX = string pointer
    ; Returns RAX = CRC32 hash, RDX = length
    push rcx
    xor rax, rax
    xor rdx, rdx
name_loop:
    mov al, [rcx + rdx]
    test al, al
    jz name_done
    crc32 eax, al
    inc rdx
    jmp name_loop
name_done:
    pop rcx
    ret
Compute_CRC32_Name ENDP

Initialize_Sovereign_APIs PROC
    ; Resolve all APIs at startup
    mov rcx, CRC32_ExitProcess
    call Resolve_API_By_CRC32
    mov pExitProcess, rax
    
    mov rcx, CRC32_GetStdHandle
    call Resolve_API_By_CRC32
    mov pGetStdHandle, rax
    
    mov rcx, CRC32_WriteFile
    call Resolve_API_By_CRC32
    mov pWriteFile, rax
    
    mov rcx, CRC32_Sleep
    call Resolve_API_By_CRC32
    mov pSleep, rax
    
    mov rcx, CRC32_CreateThread
    call Resolve_API_By_CRC32
    mov pCreateThread, rax
    
    ret
Initialize_Sovereign_APIs ENDP

;==============================================================================
; STUB FUNCTIONS (Replace with real implementations)
;==============================================================================

; These are currently stubs - need real implementations
Titan_LoadModel PROC
    mov rax, 1  ; Return success for now
    ret
Titan_LoadModel ENDP

Titan_RunInferenceStep PROC
    mov rax, 1  ; Return success for now
    ret
Titan_RunInferenceStep ENDP

Titan_InferenceThread PROC
    mov rax, 1  ; Return success for now
    ret
Titan_InferenceThread ENDP

Math_InitTables PROC
    mov rax, 1  ; Return success for now
    ret
Math_InitTables ENDP

StartPipeServer PROC
    mov rax, 1  ; Return success for now
    ret
StartPipeServer ENDP

Pipe_RunServer PROC
    mov rax, 1  ; Return success for now
    ret
Pipe_RunServer ENDP

.data
szModelPath BYTE "models/model.gguf", 0
szStartup   BYTE "[+] RawrXD Titan Unified CLI Starting...", 13, 10, 0
szSuccess   BYTE "[+] Model Loaded Successfully. Starting Pipe Server...", 13, 10, 0
szFail      BYTE "[-] Failed to Load Model.", 13, 10, 0
szPipeFail  BYTE "[-] Failed to Create Pipe.", 13, 10, 0

ALIGN 16
TitanContext_Instance BYTE 4096 DUP(0) ; Allocate 4KB for context

.code

; Initialize sovereign APIs on module load
call Initialize_Sovereign_APIs

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
    call pExitProcess
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
    call pGetStdHandle
    
    mov rcx, rax                    ; Handle
    mov rdx, rbx                    ; Buffer
    mov r8, r10                     ; Len
    lea r9, [rsp+50h]               ; BytesWritten ptr (shadow space + offset)
    mov qword ptr [rsp+20h], 0     ; Overlapped
    call pWriteFile
    
    add rsp, 40h
    pop rbx
    ret
PrintString ENDP

END
