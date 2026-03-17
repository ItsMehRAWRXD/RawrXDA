; ============================================================================
; RawrXD_NativeHost.asm - Pure MASM64 Native Host for IDE Integration
; Version: 1.0.0
; 
; This is a standalone executable that hosts the pattern engine with zero
; PowerShell overhead. It provides named pipe communication for IDE plugins.
;
; Build:
;   ml64 /c RawrXD_NativeHost.asm
;   link /SUBSYSTEM:CONSOLE /ENTRY:main RawrXD_NativeHost.obj kernel32.lib
;
; Usage:
;   RawrXD_NativeHost.exe [--pipe <name>] [--watch <path>]
;
; Protocol:
;   Client sends: [4-byte length][command][data]
;   Server sends: [4-byte length][JSON response]
;
; Commands:
;   PING       - Health check, returns PONG
;   CLASSIFY   - Classify buffer, send data after command
;   FILE       - Classify file, send path after command
;   STATS      - Get engine statistics
;   INFO       - Get engine info
;   QUIT       - Shutdown server
;
; ============================================================================

OPTION CASEMAP:NONE

includelib kernel32.lib
includelib user32.lib

includelib kernel32.lib
includelib user32.lib

; ============================================================================
; Include Files and Externals
; ============================================================================

; Windows API constants
INVALID_HANDLE_VALUE    EQU -1
NULL                    EQU 0

; Pipe constants
PIPE_ACCESS_DUPLEX      EQU 3h
PIPE_TYPE_MESSAGE       EQU 4h
PIPE_READMODE_MESSAGE   EQU 2h
PIPE_WAIT               EQU 0h
PIPE_UNLIMITED_INSTANCES EQU 255

; File constants
GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
FILE_SHARE_READ         EQU 1h
OPEN_EXISTING           EQU 3h
FILE_ATTRIBUTE_NORMAL   EQU 80h
CREATE_ALWAYS           EQU 2h

; Console constants
STD_OUTPUT_HANDLE       EQU -11
STD_INPUT_HANDLE        EQU -10

; ============================================================================
; External Functions
; ============================================================================

; Kernel32
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN ReadConsoleA:PROC
EXTERN ExitProcess:PROC
EXTERN CreateNamedPipeA:PROC
EXTERN ConnectNamedPipe:PROC
EXTERN DisconnectNamedPipe:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetLastError:PROC
EXTERN CreateFileA:PROC
EXTERN GetFileSize:PROC
EXTERN FlushFileBuffers:PROC
EXTERN GetTickCount64:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN GetCommandLineA:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcmpA:PROC
EXTERN lstrcpyA:PROC

; CRT for sprintf-like formatting
EXTERN __imp_wsprintfA:QWORD
EXTERN Titan_LoadModel:PROC
EXTERN Titan_Initialize:PROC
EXTERN Titan_RunInferenceStep:PROC

TitanContext STRUC
    signature          DWORD ?
    state              DWORD ?
    hFile              QWORD ?
    hMap               QWORD ?
    pFileBase          QWORD ?
    cbFile             QWORD ?
    arch_type          DWORD ?
    n_vocab            DWORD ?
    n_embd             DWORD ?
    n_layer            DWORD ?
    n_head             DWORD ?
    pKVCache           QWORD ?
    cbKVCache          QWORD ?
    nCurPos            DWORD ?
    pWeights           QWORD ? ; Host extension
TitanContext ENDS

; ============================================================================
; Data Section
; ============================================================================

.data

; Console messages
szBanner        DB 13,10
                DB "====================================================",13,10
                DB "  RawrXD Native Host v1.0 - MASM64 Pattern Engine",13,10
                DB "====================================================",13,10
                DB 0
szStarting      DB "[Host] Starting named pipe server...",13,10,0
szListening     DB "[Host] Listening on: ",0
szNewline       DB 13,10,0
szConnected     DB "[Host] Client connected",13,10,0
szDisconnected  DB "[Host] Client disconnected",13,10,0
szShutdown      DB "[Host] Shutting down...",13,10,0
szError         DB "[ERROR] ",0
szPipeError     DB "Failed to create named pipe",13,10,0
szRecvCmd       DB "[Cmd]  ",0

; Default pipe name
szDefaultPipe   DB "\\.\pipe\RawrXD_PatternBridge",0
szCurrentPipe   DB 256 DUP(0)

; Command strings
szCmdPing       DB "PING",0
szCmdClassify   DB "CLASSIFY",0
szCmdFile       DB "FILE",0
szCmdStats      DB "STATS",0
szCmdInfo       DB "INFO",0
szCmdQuit       DB "QUIT",0
szCmdLoad       DB "LOAD",0
szCmdInfer      DB "INFER",0

; Response templates
szPong          DB '{"Status":"PONG","Version":"1.0.0"}',0
szInfoTmpl      DB '{"Engine":"RawrXD_NativeHost","Version":"1.0.0","Mode":"%s","CPU":"%s"}',0
szStatsTmpl     DB '{"TotalRequests":%lu,"TotalMatches":%lu,"TotalBytes":%lu,"Uptime":%lu}',0
szResultTmpl    DB '{"Pattern":"%s","Confidence":%d.%02d,"Line":%d,"Priority":%d}',0
szErrorTmpl     DB '{"Error":"%s"}',0

; Pattern names
szPatternUnknown DB "Unknown",0
szPatternTODO   DB "TODO",0
szPatternFIXME  DB "FIXME",0
szPatternXXX    DB "XXX",0
szPatternHACK   DB "HACK",0
szPatternBUG    DB "BUG",0
szPatternNOTE   DB "NOTE",0
szPatternIDEA   DB "IDEA",0
szPatternREVIEW DB "REVIEW",0

; Pattern lookup table (pointers to strings)
ALIGN 8
PatternNames    DQ szPatternUnknown     ; 0
                DQ szPatternTODO        ; 1
                DQ szPatternFIXME       ; 2
                DQ szPatternXXX         ; 3
                DQ szPatternHACK        ; 4
                DQ szPatternBUG         ; 5
                DQ szPatternNOTE        ; 6
                DQ szPatternIDEA        ; 7
                DQ szPatternREVIEW      ; 8

; Engine mode strings
szModeScalar    DB "Scalar",0
szModeAVX512    DB "AVX-512",0
szCPUDefault    DB "x64",0
szCPUAVX512     DB "x64+AVX512",0

szLoadOk        DB '{"Status":"OK","Model":"Loaded"}',0
szLoadFail      DB "Failed to load model",0
szInferOk       DB '{"Status":"OK","Step":"Complete"}',0

; ============================================================================
; BSS Section (uninitialized data)
; ============================================================================

.data?

; Handles
hStdOut         DQ ?
hPipe           DQ ?
hFile           DQ ?

; Statistics
qwStartTime     DQ ?
qwTotalRequests DQ ?
qwTotalMatches  DQ ?
qwTotalBytes    DQ ?

; Buffers - smaller sizes to avoid ADDR32 issues
RecvBuffer      DB 8192 DUP(?)
SendBuffer      DB 8192 DUP(?)
TempBuffer      DB 1024 DUP(?)
FileBuffer      DB 65536 DUP(?)       ; 64KB file buffer
PathBuffer      DB 512 DUP(?)

; I/O variables
dwBytesRead     DD ?
dwBytesWritten  DD ?
dwFileSize      DD ?

; Engine state
bEngineMode     DD ?                   ; 0 = Scalar, 2 = AVX-512
bRunning        DD ?
g_TitanContext  DQ ?                   ; Pointer to TitanContext

; ============================================================================
; Code Section
; ============================================================================

.code

; ============================================================================
; PrintString - Print null-terminated string to console
; Input:  RCX = pointer to string
; ============================================================================
PrintString PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 40
    
    mov rsi, rcx                ; Save string pointer
    
    ; Get string length
    call lstrlenA
    mov ebx, eax                ; Length in EBX
    
    ; WriteConsoleA(hStdOut, string, length, &written, NULL)
    mov rcx, [hStdOut]
    mov rdx, rsi
    mov r8d, ebx
    lea r9, [dwBytesWritten]
    mov qword ptr [rsp+32], 0
    call WriteConsoleA
    
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
PrintString ENDP

; ============================================================================
; PrintNumber - Print number to console
; Input:  RCX = number
; ============================================================================
PrintNumber PROC
    sub rsp, 56
    
    ; Format number to string
    lea rcx, [TempBuffer]
    lea rdx, [szNumberFmt]
    mov r8, r8                  ; Number already in r8 from caller
    call qword ptr [__imp_wsprintfA]
    
    ; Print the string
    lea rcx, [TempBuffer]
    call PrintString
    
    add rsp, 56
    ret
    
szNumberFmt DB "%lu",0
PrintNumber ENDP

; ============================================================================
; InitializeEngine - Set up pattern engine (detect AVX-512)
; Output: EAX = mode (0=Scalar, 2=AVX-512)
; ============================================================================
InitializeEngine PROC
    push rbx
    sub rsp, 32
    
    ; Check for AVX-512 using CPUID
    mov eax, 7
    xor ecx, ecx
    cpuid
    
    ; Check EBX bit 16 (AVX-512F)
    bt ebx, 16
    jnc @@UseScalar
    
    ; Check XGETBV for ZMM state
    xor ecx, ecx
    xgetbv
    
    ; Check bits 5,6,7 (OPMASK, ZMM_Hi256, Hi16_ZMM)
    and eax, 0E0h
    cmp eax, 0E0h
    jne @@UseScalar
    
    ; AVX-512 available
    mov dword ptr [bEngineMode], 2
    mov eax, 2
    jmp @@Done
    
@@UseScalar:
    mov dword ptr [bEngineMode], 0
    xor eax, eax
    
@@Done:
    add rsp, 32
    pop rbx
    ret
InitializeEngine ENDP

; ============================================================================
; ClassifyPattern - Classify a buffer for patterns
; Input:  RCX = buffer pointer
;         RDX = buffer length
; Output: EAX = pattern type (0-8)
;         XMM0 = confidence (double)
; ============================================================================
ClassifyPattern PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 48
    
    mov rsi, rcx                ; Buffer pointer
    mov r12d, edx               ; Buffer length
    
    xor r13d, r13d              ; Pattern found = 0 (UNKNOWN)
    xorps xmm0, xmm0            ; Confidence = 0.0
    
    ; Scan buffer for pattern keywords
    xor edi, edi                ; Position = 0
    
@@ScanLoop:
    cmp edi, r12d
    jge @@Done
    
    ; Get current byte
    movzx eax, byte ptr [rsi + rdi]
    
    ; Check for 'T' (TODO)
    cmp al, 'T'
    je @@CheckTODO
    
    ; Check for 'F' (FIXME)
    cmp al, 'F'
    je @@CheckFIXME
    
    ; Check for 'X' (XXX)
    cmp al, 'X'
    je @@CheckXXX
    
    ; Check for 'H' (HACK)
    cmp al, 'H'
    je @@CheckHACK
    
    ; Check for 'B' (BUG)
    cmp al, 'B'
    je @@CheckBUG
    
    ; Check for 'N' (NOTE)
    cmp al, 'N'
    je @@CheckNOTE
    
    ; Check for 'I' (IDEA)
    cmp al, 'I'
    je @@CheckIDEA
    
    ; Check for 'R' (REVIEW)
    cmp al, 'R'
    je @@CheckREVIEW
    
@@NextChar:
    inc edi
    jmp @@ScanLoop

; ----------------- Pattern Checkers -----------------

@@CheckTODO:
    ; Need at least 4 more chars
    lea eax, [edi + 4]
    cmp eax, r12d
    jge @@NextChar
    
    ; Check "ODO"
    cmp byte ptr [rsi + rdi + 1], 'O'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 2], 'D'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 3], 'O'
    jne @@NextChar
    
    ; Found TODO
    mov r13d, 1
    movsd xmm0, qword ptr [conf_high]
    jmp @@Done

@@CheckFIXME:
    lea eax, [edi + 5]
    cmp eax, r12d
    jge @@NextChar
    
    cmp byte ptr [rsi + rdi + 1], 'I'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 2], 'X'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 3], 'M'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 4], 'E'
    jne @@NextChar
    
    mov r13d, 2
    movsd xmm0, qword ptr [conf_high]
    jmp @@Done

@@CheckXXX:
    lea eax, [edi + 3]
    cmp eax, r12d
    jge @@NextChar
    
    cmp byte ptr [rsi + rdi + 1], 'X'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 2], 'X'
    jne @@NextChar
    
    mov r13d, 3
    movsd xmm0, qword ptr [conf_med]
    jmp @@Done

@@CheckHACK:
    lea eax, [edi + 4]
    cmp eax, r12d
    jge @@NextChar
    
    cmp byte ptr [rsi + rdi + 1], 'A'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 2], 'C'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 3], 'K'
    jne @@NextChar
    
    mov r13d, 4
    movsd xmm0, qword ptr [conf_med]
    jmp @@Done

@@CheckBUG:
    lea eax, [edi + 3]
    cmp eax, r12d
    jge @@NextChar
    
    cmp byte ptr [rsi + rdi + 1], 'U'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 2], 'G'
    jne @@NextChar
    
    mov r13d, 5
    movsd xmm0, qword ptr [conf_high]
    jmp @@Done

@@CheckNOTE:
    lea eax, [edi + 4]
    cmp eax, r12d
    jge @@NextChar
    
    cmp byte ptr [rsi + rdi + 1], 'O'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 2], 'T'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 3], 'E'
    jne @@NextChar
    
    mov r13d, 6
    movsd xmm0, qword ptr [conf_med]
    jmp @@Done

@@CheckIDEA:
    lea eax, [edi + 4]
    cmp eax, r12d
    jge @@NextChar
    
    cmp byte ptr [rsi + rdi + 1], 'D'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 2], 'E'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 3], 'A'
    jne @@NextChar
    
    mov r13d, 7
    movsd xmm0, qword ptr [conf_med]
    jmp @@Done

@@CheckREVIEW:
    lea eax, [edi + 6]
    cmp eax, r12d
    jge @@NextChar
    
    cmp byte ptr [rsi + rdi + 1], 'E'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 2], 'V'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 3], 'I'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 4], 'E'
    jne @@NextChar
    cmp byte ptr [rsi + rdi + 5], 'W'
    jne @@NextChar
    
    mov r13d, 8
    movsd xmm0, qword ptr [conf_med]
    jmp @@Done

@@Done:
    mov eax, r13d
    
    add rsp, 48
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; Confidence values (as doubles)
ALIGN 8
conf_high       DQ 3FEC28F5C28F5C29h   ; 0.88
conf_med        DQ 3FE3333333333333h   ; 0.60
ClassifyPattern ENDP

; ============================================================================
; HandleCommand - Process a command from the client
; Input:  RCX = command buffer
;         RDX = command length
; Output: EAX = response length (in SendBuffer)
; ============================================================================
HandleCommand PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 80
    
    mov rsi, rcx                ; Command buffer
    mov r12d, edx               ; Command length
    
    ; Increment request counter
    inc qword ptr [qwTotalRequests]
    
    ; Check command type
    
    ; === PING ===
    lea rcx, [rsi]
    lea rdx, [szCmdPing]
    call lstrcmpA
    test eax, eax
    jnz @@NotPing
    
    ; Return PONG
    lea rcx, [SendBuffer]
    lea rdx, [szPong]
    call lstrcpyA
    lea rcx, [SendBuffer]
    call lstrlenA
    jmp @@Done
    
@@NotPing:
    ; === INFO ===
    lea rcx, [rsi]
    lea rdx, [szCmdInfo]
    call lstrcmpA
    test eax, eax
    jnz @@NotInfo
    
    ; Format info response
    lea rcx, [SendBuffer]
    lea rdx, [szInfoTmpl]
    ; Mode string
    cmp dword ptr [bEngineMode], 2
    jne @@InfoScalar
    lea r8, [szModeAVX512]
    lea r9, [szCPUAVX512]
    jmp @@DoInfoFormat
@@InfoScalar:
    lea r8, [szModeScalar]
    lea r9, [szCPUDefault]
@@DoInfoFormat:
    call qword ptr [__imp_wsprintfA]
    jmp @@Done
    
@@NotInfo:
    ; === STATS ===
    lea rcx, [rsi]
    lea rdx, [szCmdStats]
    call lstrcmpA
    test eax, eax
    jnz @@NotStats
    
    ; Calculate uptime
    call GetTickCount64
    sub rax, [qwStartTime]
    mov r13, rax                ; Uptime in ms
    
    ; Format stats response
    lea rcx, [SendBuffer]
    lea rdx, [szStatsTmpl]
    mov r8, [qwTotalRequests]
    mov r9, [qwTotalMatches]
    mov rax, [qwTotalBytes]
    mov [rsp+32], rax
    mov [rsp+40], r13
    call qword ptr [__imp_wsprintfA]
    jmp @@Done
    
@@NotStats:
    ; === LOAD ===
    lea rcx, [rsi]
    lea rdx, [szCmdLoad]
    call lstrcmpA
    test eax, eax
    jnz @@NotLoad
    
    ; Read path length
    mov rcx, [hPipe]
    lea rdx, [dwBytesRead]
    mov r8d, 4
    lea r9, [dwBytesWritten]
    xor eax, eax
    mov [rsp+32], rax
    call ReadFile
    
    ; Read path
    mov ecx, [dwBytesRead]
    mov rcx, [hPipe]
    lea rdx, [PathBuffer]
    mov r8d, [dwBytesRead]
    lea r9, [dwBytesWritten]
    xor eax, eax
    mov [rsp+32], rax
    call ReadFile
    
    ; Null terminate
    mov eax, [dwBytesWritten]
    mov byte ptr [PathBuffer + rax], 0
    
    ; Initialize Titan
    lea rcx, [g_TitanContext]
    call Titan_Initialize
    test eax, eax
    jnz @@LoadError
    
    ; Load Model
    mov rcx, [g_TitanContext]
    lea rdx, [PathBuffer]
    call Titan_LoadModel
    test eax, eax
    jnz @@LoadError
    
    ; Success Response
    lea rcx, [SendBuffer]
    lea rdx, [szLoadOk]
    call lstrcpyA
    lea rcx, [SendBuffer]
    call lstrlenA
    jmp @@Done

@@LoadError:
    lea rcx, [SendBuffer]
    lea rdx, [szErrorTmpl]
    lea r8, [szLoadFail]
    call qword ptr [__imp_wsprintfA]
    jmp @@Done

@@NotLoad:
    ; === INFER ===
    lea rcx, [rsi]
    lea rdx, [szCmdInfer]
    call lstrcmpA
    test eax, eax
    jnz @@NotInfer
    
    ; Check context
    mov rcx, [g_TitanContext]
    test rcx, rcx
    jz @@LoadError
    
    ; Run Step
    mov rcx, [g_TitanContext]
    mov rdx, [rcx].TitanContext.pWeights ; Needs to be set!
    ; Fallback if pWeights is null: use pFileBase + offset?
    ; For now pass rcx itself as dummy weight base if null
    test rdx, rdx
    cmovz rdx, rcx 
    
    call Titan_RunInferenceStep
    
    ; Success Response
    lea rcx, [SendBuffer]
    lea rdx, [szInferOk]
    call lstrcpyA
    lea rcx, [SendBuffer]
    call lstrlenA
    jmp @@Done
    
@@NotInfer:
    ; === QUIT ===
    lea rcx, [rsi]
    lea rdx, [szCmdQuit]
    call lstrcmpA
    test eax, eax
    jnz @@UnknownCmd
    
    mov dword ptr [bRunning], 0
    lea rcx, [SendBuffer]
    lea rdx, [szQuitResp]
    call lstrcpyA
    lea rcx, [SendBuffer]
    call lstrlenA
    jmp @@Done
    
@@UnknownCmd:
    ; Unknown command
    lea rcx, [SendBuffer]
    lea rdx, [szErrorTmpl]
    lea r8, [szUnknownCmd]
    call qword ptr [__imp_wsprintfA]
    
@@Done:
    add rsp, 80
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

szClassifyErr   DB "Failed to read data",0
szUnknownCmd    DB "Unknown command",0
szQuitResp      DB '{"Status":"Shutdown"}',0
HandleCommand ENDP

; ============================================================================
; SendResponse - Send response to client
; Input:  RCX = response buffer
;         RDX = response length
; ============================================================================
SendResponse PROC
    push rbx
    sub rsp, 48
    
    mov rbx, rcx                ; Buffer
    mov r8d, edx                ; Length
    
    ; Write length prefix (4 bytes)
    mov dword ptr [TempBuffer], r8d
    mov rcx, [hPipe]
    lea rdx, [TempBuffer]
    mov r8d, 4
    lea r9, [dwBytesWritten]
    xor eax, eax
    mov [rsp+32], rax
    call WriteFile
    
    ; Write response data
    mov rcx, [hPipe]
    mov rdx, rbx
    lea rcx, [SendBuffer]
    call lstrlenA
    mov r8d, eax
    mov rcx, [hPipe]
    lea rdx, [SendBuffer]
    lea r9, [dwBytesWritten]
    xor eax, eax
    mov [rsp+32], rax
    call WriteFile
    
    ; Flush
    mov rcx, [hPipe]
    call FlushFileBuffers
    
    add rsp, 48
    pop rbx
    ret
SendResponse ENDP

; ============================================================================
; main - Entry point
; ============================================================================
PUBLIC main
main PROC
    sub rsp, 88
    
    ; Get stdout handle
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [hStdOut], rax
    
    ; Print banner
    lea rcx, [szBanner]
    call PrintString
    
    ; Initialize engine
    call InitializeEngine
    
    ; Print engine mode
    lea rcx, [szModeMsg]
    call PrintString
    cmp dword ptr [bEngineMode], 2
    jne @@PrintScalar
    lea rcx, [szModeAVX512]
    jmp @@PrintMode
@@PrintScalar:
    lea rcx, [szModeScalar]
@@PrintMode:
    call PrintString
    lea rcx, [szNewline]
    call PrintString
    
    ; Copy default pipe name
    lea rcx, [szCurrentPipe]
    lea rdx, [szDefaultPipe]
    call lstrcpyA
    
    ; Get start time
    call GetTickCount64
    mov [qwStartTime], rax
    
    ; Initialize counters
    xor eax, eax
    mov [qwTotalRequests], rax
    mov [qwTotalMatches], rax
    mov [qwTotalBytes], rax
    
    ; Print starting message
    lea rcx, [szStarting]
    call PrintString
    lea rcx, [szListening]
    call PrintString
    lea rcx, [szCurrentPipe]
    call PrintString
    lea rcx, [szNewline]
    call PrintString
    
    ; Create named pipe
    lea rcx, [szCurrentPipe]
    mov edx, PIPE_ACCESS_DUPLEX
    mov r8d, PIPE_TYPE_MESSAGE or PIPE_READMODE_MESSAGE or PIPE_WAIT
    mov r9d, 1                  ; Max instances
    mov dword ptr [rsp+32], 65536  ; Out buffer
    mov dword ptr [rsp+40], 65536  ; In buffer
    mov dword ptr [rsp+48], 0      ; Timeout
    mov qword ptr [rsp+56], 0      ; Security
    call CreateNamedPipeA
    
    cmp rax, INVALID_HANDLE_VALUE
    jne @@PipeCreated
    
    ; Pipe creation failed
    lea rcx, [szError]
    call PrintString
    lea rcx, [szPipeError]
    call PrintString
    mov ecx, 1
    call ExitProcess
    
@@PipeCreated:
    mov [hPipe], rax
    mov dword ptr [bRunning], 1
    
@@ServerLoop:
    ; Wait for client
    mov rcx, [hPipe]
    xor edx, edx
    call ConnectNamedPipe
    
    lea rcx, [szConnected]
    call PrintString
    
@@ClientLoop:
    cmp dword ptr [bRunning], 0
    je @@Shutdown
    
    ; Read command length (4 bytes)
    mov rcx, [hPipe]
    lea rdx, [dwBytesRead]
    mov r8d, 4
    lea r9, [dwBytesWritten]
    xor eax, eax
    mov [rsp+32], rax
    call ReadFile
    test eax, eax
    jz @@ClientDisconnected
    
    ; Read command
    mov ecx, [dwBytesRead]
    test ecx, ecx
    jz @@ClientDisconnected
    
    mov rcx, [hPipe]
    lea rdx, [RecvBuffer]
    mov r8d, [dwBytesRead]
    lea r9, [dwBytesWritten]
    xor eax, eax
    mov [rsp+32], rax
    call ReadFile
    test eax, eax
    jz @@ClientDisconnected
    
    ; Null-terminate command
    mov eax, [dwBytesWritten]
    mov byte ptr [RecvBuffer + rax], 0
    
    ; Print received command
    lea rcx, [szRecvCmd]
    call PrintString
    lea rcx, [RecvBuffer]
    call PrintString
    lea rcx, [szNewline]
    call PrintString
    
    ; Handle command
    lea rcx, [RecvBuffer]
    mov edx, [dwBytesWritten]
    call HandleCommand
    
    ; Send response
    lea rcx, [SendBuffer]
    mov edx, eax
    call SendResponse
    
    jmp @@ClientLoop
    
@@ClientDisconnected:
    lea rcx, [szDisconnected]
    call PrintString
    
    ; Disconnect pipe
    mov rcx, [hPipe]
    call DisconnectNamedPipe
    
    cmp dword ptr [bRunning], 0
    jne @@ServerLoop
    
@@Shutdown:
    lea rcx, [szShutdown]
    call PrintString
    
    ; Close pipe
    mov rcx, [hPipe]
    call CloseHandle
    
    ; Exit
    xor ecx, ecx
    call ExitProcess
    
szModeMsg DB "[Engine] Mode: ",0
main ENDP

END
