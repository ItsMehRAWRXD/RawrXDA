; ============================================================================
; RawrXD_NativeHost.asm
; ============================================================================
OPTION CASEMAP:NONE
OPTION DOTNAME

includelib kernel32.lib
includelib user32.lib

; Helper for wsprintfA (CDECL)
; EXTERN wsprintfA : PROC

; Titan API
EXTERN Titan_Initialize:PROC
EXTERN Titan_RunInferenceStep:PROC
EXTERN Titan_LoadModel:PROC
EXTERN Titan_Shutdown:PROC

; Win32 API
EXTERN ExitProcess:PROC
EXTERN CreateNamedPipeA:PROC
EXTERN ConnectNamedPipe:PROC
EXTERN DisconnectNamedPipe:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetLastError:PROC
EXTERN Sleep:PROC
EXTERN FlushFileBuffers:PROC

.data
szPipeName      DB "\\.\pipe\RawrXD_PatternBridge",0
szPing          DB "PING",0
szPong          DB '{"Status":"PONG"}',0
szQuit          DB "QUIT",0
szInfer         DB "INFER",0
szLoad          DB "LOAD",0
szOk            DB '{"Status":"OK"}',0
szError         DB '{"Status":"ERROR"}',0
szFmtDebug      DB "[Host] Values: %d %d",13,10,0

hPipe           DQ -1
hContext        DQ 0
ReadBuf         DB 1024 DUP(0)
WriteBuf        DB 1024 DUP(0)
BytesRead       DWORD 0
BytesWritten    DWORD 0

.code

main PROC
    sub rsp, 88 ; Shadow space + Locals
    
    ; Initialize Titan
    lea rcx, hContext
    call Titan_Initialize
    test rax, rax
    jnz @exit_err
    
@server_loop:
    ; Create Pipe
    mov rcx, OFFSET szPipeName
    mov rdx, 3 ; PIPE_ACCESS_DUPLEX
    mov r8, 4 ; PIPE_TYPE_MESSAGE
    mov r9, 255
    mov qword ptr [rsp+32], 4096 ; OutBuf
    mov qword ptr [rsp+40], 4096 ; InBuf
    mov qword ptr [rsp+48], 0 ; Timeout
    mov qword ptr [rsp+56], 0 ; Security
    call CreateNamedPipeA
    
    cmp rax, -1
    je @exit_err
    mov hPipe, rax
    
    ; Connect
    mov rcx, hPipe
    xor rdx, rdx
    call ConnectNamedPipe
    
    ; Read Loop
@client_loop:
    mov rcx, hPipe
    lea rdx, ReadBuf
    mov r8, 1024
    lea r9, BytesRead
    mov qword ptr [rsp+32], 0
    call ReadFile
    
    test rax, rax
    jz @client_disconnect
    
    ; Process Command (Simple String Match)
    ; PING
    mov rcx, OFFSET ReadBuf
    mov rdx, OFFSET szPing
    call StrCmp
    test eax, eax
    jz @cmd_ping
    
    ; INFER
    mov rcx, OFFSET ReadBuf
    mov rdx, OFFSET szInfer
    call StrCmp
    test eax, eax
    jz @cmd_infer
    
    ; LOAD
    mov rcx, OFFSET ReadBuf
    mov rdx, OFFSET szLoad
    call StrCmp
    test eax, eax
    jz @cmd_load
    
    ; QUIT
    mov rcx, OFFSET ReadBuf
    mov rdx, OFFSET szQuit
    call StrCmp
    test eax, eax
    jz @exit_ok
    
    jmp @client_loop
    
@cmd_ping:
    mov rcx, hPipe
    lea rdx, szPong
    mov r8, 17 ; Len
    lea r9, BytesWritten
    mov qword ptr [rsp+32], 0
    call WriteFile
    jmp @client_loop

@cmd_infer:
    ; Titan Inference
    mov rcx, hContext
    xor rdx, rdx ; Layer 0
    call Titan_RunInferenceStep
    
    mov rcx, hPipe
    lea rdx, szOk
    mov r8, 15
    lea r9, BytesWritten
    mov qword ptr [rsp+32], 0
    call WriteFile
    jmp @client_loop

@cmd_load:
    ; Titan Load
    mov rcx, hContext
    lea rdx, szPing ; Dummy path
    call Titan_LoadModel
    
    mov rcx, hPipe
    lea rdx, szOk
    mov r8, 15
    lea r9, BytesWritten
    mov qword ptr [rsp+32], 0
    call WriteFile
    jmp @client_loop

@client_disconnect:
    mov rcx, hPipe
    call DisconnectNamedPipe
    mov rcx, hPipe
    call CloseHandle
    jmp @server_loop
    
@exit_err:
    mov ecx, 1
    call ExitProcess
    
@exit_ok:
    mov rcx, hContext
    call Titan_Shutdown
    mov ecx, 0
    call ExitProcess

main ENDP

; Simple String Compare
StrCmp PROC
    ; RCX=Str1, RDX=Str2
    ; Return 0 if match
    ; Only checks first 4 chars for now for speed/simplicity
    mov eax, [rcx]
    mov r8d, [rdx]
    sub eax, r8d
    ret
StrCmp ENDP

END
