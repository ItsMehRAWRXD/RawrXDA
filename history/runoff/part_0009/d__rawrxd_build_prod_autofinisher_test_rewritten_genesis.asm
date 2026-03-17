;==============================================================================
; RawrXD Genesis Engine v1.0.0
; Autonomous MASM64 Code Generation System
; No CRT | No Dependencies | Self-Bootstrapping | Production-Grade
;
; Assemble:
;   ml64.exe genesis.asm /c /Fo genesis.obj /nologo /W3
; Link:
;   link.exe genesis.obj /subsystem:console /entry:GenesisMain /out:genesis.exe ^
;       kernel32.lib winhttp.lib user32.lib
;
; Or combined:
;   ml64.exe genesis.asm /link /subsystem:console /entry:GenesisMain ^
;       /out:genesis.exe kernel32.lib winhttp.lib
;==============================================================================

OPTION CASEMAP:NONE

; [AutoFinisher] Auto-injected extrn declarations
extrn CloseHandle:proc
extrn CreateDirectoryA:proc
extrn CreateFileA:proc
extrn CreateProcessA:proc
extrn CreateThread:proc
extrn ExitProcess:proc
extrn GetStdHandle:proc
extrn ReadFile:proc
extrn WaitForSingleObject:proc
extrn WinHttpCloseHandle:proc
extrn WinHttpConnect:proc
extrn WinHttpOpen:proc
extrn WinHttpOpenRequest:proc
extrn WinHttpQueryDataAvailable:proc
extrn WinHttpReadData:proc
extrn WinHttpReceiveResponse:proc
extrn WinHttpSendRequest:proc
extrn WriteFile:proc


;==============================================================================
; INCLUDE ? project master header gives us all EXTERN + constants
;==============================================================================
INCLUDE rawrxd_master.inc

;------------------------------------------------------------------------------
; INCLUDELIB ? pull these at link time
;------------------------------------------------------------------------------
INCLUDELIB kernel32.lib
INCLUDELIB winhttp.lib
INCLUDELIB user32.lib

;------------------------------------------------------------------------------
; Constants not yet in rawrxd_win32_api.inc
;------------------------------------------------------------------------------
IFNDEF WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
WINHTTP_ACCESS_TYPE_DEFAULT_PROXY EQU 0
ENDIF

IFNDEF FILE_LIST_DIRECTORY
FILE_LIST_DIRECTORY EQU 1
ENDIF

IFNDEF FILE_SHARE_READ
FILE_SHARE_READ  EQU 1
ENDIF
IFNDEF FILE_SHARE_WRITE
FILE_SHARE_WRITE EQU 2
ENDIF

IFNDEF FILE_FLAG_BACKUP_SEMANTICS
FILE_FLAG_BACKUP_SEMANTICS EQU 02000000h
ENDIF
IFNDEF FILE_FLAG_OVERLAPPED
FILE_FLAG_OVERLAPPED       EQU 40000000h
ENDIF

IFNDEF FILE_NOTIFY_CHANGE_LAST_WRITE
FILE_NOTIFY_CHANGE_LAST_WRITE EQU 10h
ENDIF

IFNDEF NORMAL_PRIORITY_CLASS
NORMAL_PRIORITY_CLASS EQU 20h
ENDIF

;------------------------------------------------------------------------------
; Structures (inline ? not in the project-wide .inc yet)
;------------------------------------------------------------------------------
STARTUPINFOA STRUCT
    cb              DWORD ?
    lpReserved      QWORD ?
    lpDesktop       QWORD ?
    lpTitle         QWORD ?
    dwX             DWORD ?
    dwY             DWORD ?
    dwXSize         DWORD ?
    dwYSize         DWORD ?
    dwXCountChars   DWORD ?
    dwYCountChars   DWORD ?
    dwFillAttribute DWORD ?
    dwFlags         DWORD ?
    wShowWindow     WORD  ?
    cbReserved2     WORD  ?
    lpReserved2     QWORD ?
    hStdInput       QWORD ?
    hStdOutput      QWORD ?
    hStdError       QWORD ?
STARTUPINFOA ENDS

PROCESS_INFORMATION STRUCT
    hProcess    QWORD ?
    hThread     QWORD ?
    dwProcessId DWORD ?
    dwThreadId  DWORD ?
PROCESS_INFORMATION ENDS

;------------------------------------------------------------------------------
; EXTERN ? WinHTTP calls not yet in rawrxd_win32_api.inc
;------------------------------------------------------------------------------
IFNDEF _GENESIS_WINHTTP_EXTRAS
_GENESIS_WINHTTP_EXTRAS EQU 1
EXTERNDEF WinHttpAddRequestHeaders:PROC
EXTERNDEF ReadDirectoryChangesW:PROC
EXTERNDEF Sleep:PROC
ENDIF

;==============================================================================
;                              DATA SECTION
;==============================================================================
.DATA

; ---- System Configuration (paths wired to D:\rawrxd) -----------------------
ALIGN 16
g_OllamaHost        BYTE "localhost",0
g_OllamaPath        BYTE "/api/generate",0
g_ModelName          BYTE "phi-3-mini",0
g_SrcDirectory       BYTE "D:\rawrxd\src\",0
g_BuildDirectory     BYTE "D:\rawrxd\build_prod\",0
g_ML64Path           BYTE "ml64.exe",0     ; rely on PATH from VS env

; ---- JSON template fragments ------------------------------------------------
ALIGN 16
g_JsonModelStart     BYTE '{"model":"',0
g_JsonPromptStart    BYTE '","prompt":"',0
g_JsonOptions        BYTE '","stream":false,"options":{"temperature":0.1}}',0

; ---- Prompt Engineering Context ---------------------------------------------
ALIGN 16
g_PromptContext BYTE \
    "You are the RawrXD Genesis AI. Generate production MASM64 assembly code. ",\
    "Requirements: 1) Use FRAME prologues with .ENDPROLOG 2) AVX-512 for SIMD ",\
    "operations 3) Fastcall ABI compliance 4) No external CRT calls, use ",\
    "WinAPI only 5) Include error handling 6) Stack aligned to 16 bytes. ",\
    "Generate implementation for: ",0

; ---- Pattern Templates: 7 AI Systems (stub skeletons) -----------------------
ALIGN 16
g_Pattern_CompletionEngine BYTE \
    "; CompletionEngine.asm [GENESIS AUTO-GENERATED]",13,10,\
    "OPTION CASEMAP:NONE",13,10,\
    "INCLUDE rawrxd_master.inc",13,10,\
    "public CompletionEngine_Initialize",13,10,\
    "public CompletionEngine_GetSuggestions",13,10,\
    "public CompletionEngine_Shutdown",13,10,13,10,\
    ".CODE",13,10,13,10,\
    "CompletionEngine_Initialize PROC FRAME",13,10,\
    "    push rbp",13,10,\
    "    .pushreg rbp",13,10,\
    "    mov rbp, rsp",13,10,\
    "    .setframe rbp, 0",13,10,\
    "    sub rsp, 40h",13,10,\
    "    .allocstack 40h",13,10,\
    "    .endprolog",13,10,\
    "    ; [GENESIS:AI_CONTEXT_START]",13,10,\
    "    ; [GENESIS:AI_CONTEXT_END]",13,10,\
    "    xor eax, eax",13,10,\
    "    leave",13,10,\
    "    ret",13,10,\
    "CompletionEngine_Initialize ENDP",13,10,13,10,\
    "CompletionEngine_GetSuggestions PROC FRAME",13,10,\
    "    push rbp",13,10,\
    "    .pushreg rbp",13,10,\
    "    mov rbp, rsp",13,10,\
    "    .setframe rbp, 0",13,10,\
    "    sub rsp, 40h",13,10,\
    "    .allocstack 40h",13,10,\
    "    .endprolog",13,10,\
    "    mov eax, 1",13,10,\
    "    leave",13,10,\
    "    ret",13,10,\
    "CompletionEngine_GetSuggestions ENDP",13,10,13,10,\
    "CompletionEngine_Shutdown PROC FRAME",13,10,\
    "    .endprolog",13,10,\
    "    ret",13,10,\
    "CompletionEngine_Shutdown ENDP",13,10,13,10,\
    "END",13,10,0

g_Pattern_ContextAnalyzer BYTE \
    "; ContextAnalyzer.asm [GENESIS]",13,10,\
    "OPTION CASEMAP:NONE",13,10,\
    "INCLUDE rawrxd_master.inc",13,10,\
    "public ContextAnalyzer_BuildIndex",13,10,\
    ".CODE",13,10,\
    "ContextAnalyzer_BuildIndex PROC FRAME",13,10,\
    "    .endprolog",13,10,\
    "    ; [GENESIS:BUILD_SYMBOL_INDEX]",13,10,0
g_Pattern_ContextAnalyzer_p1 BYTE \
    "    xor eax, eax",13,10,\
    "    ret",13,10,\
    "ContextAnalyzer_BuildIndex ENDP",13,10,\
    "END",13,10,0

g_Pattern_RewriteEngine BYTE \
    "; SmartRewriteEngine.asm [GENESIS]",13,10,\
    "OPTION CASEMAP:NONE",13,10,\
    "INCLUDE rawrxd_master.inc",13,10,\
    "public RewriteEngine_Transform",13,10,\
    ".CODE",13,10,\
    "RewriteEngine_Transform PROC FRAME",13,10,\
    "    .endprolog",13,10,\
    "    ; [GENESIS:REFACTOR_LOGIC]",13,10,0
g_Pattern_RewriteEngine_p1 BYTE \
    "    xor eax, eax",13,10,\
    "    ret",13,10,\
    "RewriteEngine_Transform ENDP",13,10,\
    "END",13,10,0

g_Pattern_ModelRouter BYTE \
    "; MultiModalModelRouter.asm [GENESIS]",13,10,\
    "OPTION CASEMAP:NONE",13,10,\
    "INCLUDE rawrxd_master.inc",13,10,\
    "public ModelRouter_SelectBackend",13,10,\
    ".CODE",13,10,\
    "ModelRouter_SelectBackend PROC FRAME",13,10,\
    "    .endprolog",13,10,\
    "    ; [GENESIS:ROUTING_LOGIC]",13,10,0
g_Pattern_ModelRouter_p1 BYTE \
    "    xor eax, eax",13,10,\
    "    ret",13,10,\
    "ModelRouter_SelectBackend ENDP",13,10,\
    "END",13,10,0

g_Pattern_LSPIntegration BYTE \
    "; LanguageServerIntegration.asm [GENESIS]",13,10,\
    "OPTION CASEMAP:NONE",13,10,\
    "INCLUDE rawrxd_master.inc",13,10,\
    "public LSP_InitializeServer",13,10,\
    "public LSP_HandleRequest",13,10,\
    ".CODE",13,10,\
    "LSP_InitializeServer PROC FRAME",13,10,\
    "    .endprolog",13,10,\
    "    ; [GENESIS:LSP_CAPS]",13,10,\
    "    xor eax, eax",13,10,\
    "    ret",13,10,\
    "LSP_InitializeServer ENDP",13,10,13,10,\
    "LSP_HandleRequest PROC FRAME",13,10,\
    "    .endprolog",13,10,\
    "    xor eax, eax",13,10,\
    "    ret",13,10,\
    "LSP_HandleRequest ENDP",13,10,\
    "END",13,10,0

g_Pattern_PerformanceOptimizer BYTE \
    "; PerformanceOptimizer.asm [GENESIS]",13,10,\
    "OPTION CASEMAP:NONE",13,10,\
    "INCLUDE rawrxd_master.inc",13,10,\
    "public PerfOpt_CacheWarmup",13,10,\
    ".CODE",13,10,\
    "PerfOpt_CacheWarmup PROC FRAME",13,10,\
    "    .endprolog",13,10,\
    "    ; [GENESIS:CACHE_STRAT]",13,10,0
g_Pattern_PerformanceOptimizer_p1 BYTE \
    "    xor eax, eax",13,10,\
    "    ret",13,10,\
    "PerfOpt_CacheWarmup ENDP",13,10,\
    "END",13,10,0

g_Pattern_AdvancedAgent BYTE \
    "; AdvancedCodingAgent.asm [GENESIS]",13,10,\
    "OPTION CASEMAP:NONE",13,10,\
    "INCLUDE rawrxd_master.inc",13,10,\
    "public Agent_ExecuteTask",13,10,\
    "public Agent_SecurityScan",13,10,\
    ".CODE",13,10,\
    "Agent_ExecuteTask PROC FRAME",13,10,\
    "    .endprolog",13,10,\
    "    ; [GENESIS:AGENT_LOOP]",13,10,\
    "    xor eax, eax",13,10,\
    "    ret",13,10,\
    "Agent_ExecuteTask ENDP",13,10,13,10,\
    "Agent_SecurityScan PROC FRAME",13,10,\
    "    .endprolog",13,10,\
    "    xor eax, eax",13,10,\
    "    ret",13,10,\
    "Agent_SecurityScan ENDP",13,10,\
    "END",13,10,0

; ---- Buffers ----------------------------------------------------------------
ALIGN 16
g_JsonBuffer        BYTE 16384 DUP(0)
g_ResponseBuffer    BYTE 32768 DUP(0)
g_CodeBuffer        BYTE 65536 DUP(0)
g_FilePath          BYTE 512 DUP(0)
g_CmdLine           BYTE 2048 DUP(0)

; ---- Synchronization --------------------------------------------------------
g_ThreadHandle      QWORD 0
g_WatcherHandle     QWORD 0
g_RunAgent          QWORD 1

; ---- Console Strings --------------------------------------------------------
szGenStart          BYTE "[GENESIS] Autonomous code generation engine online",13,10,0
szGenComplete       BYTE "[GENESIS] Code synthesis complete",13,10,0
szAssembling        BYTE "[GENESIS] Assembling: ",0
szOllamaError       BYTE "[GENESIS] Ollama connection failed",13,10,0
szFileCreated       BYTE "[GENESIS] Generated: ",0
szHotpatch          BYTE "[GENESIS] Hot-patch rebuild triggered",13,10,0
szShutdown          BYTE "[GENESIS] Shutting down...",13,10,0
szNewline           BYTE 13,10,0

; HTTP strings
szPost              BYTE "POST",0
szContentType       BYTE "Content-Type: application/json",0

; Task descriptions for agent thread
szTaskCompletion    BYTE "CompletionEngine with sub-50ms latency using ",\
    "AVX-512 string matching and hash table symbol cache",0
szFileCompletion    BYTE "CompletionEngine.asm",0


;==============================================================================
;                              CODE SECTION
;==============================================================================
.CODE

;------------------------------------------------------------------------------
; PrintConsole ? write NUL-terminated string to stdout
; RCX = pString
;------------------------------------------------------------------------------
PrintConsole PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 60h                   ; shadow + locals
    .allocstack 60h
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    .endprolog

    mov  rsi, rcx                   ; pString

    ; strlen
    mov  rdi, rsi
    xor  ecx, ecx
    dec  rcx
    xor  eax, eax
    repne scasb
    not  rcx
    dec  rcx                        ; length in rcx
    mov  r8d, ecx                   ; cbToWrite

    ; GetStdHandle
    mov  ecx, STD_OUTPUT_HANDLE
    call GetStdHandle

    ; WriteFile(hConsole, pString, len, &written, NULL)
    mov  rcx, rax                   ; hConsole
    mov  rdx, rsi                   ; pString
    ;  r8d already set
    lea  r9, [rbp-50h]              ; local written
    mov  qword ptr [rsp+20h], 0
    call WriteFile

    pop  rdi
    pop  rsi
    leave
    ret
PrintConsole ENDP

;------------------------------------------------------------------------------
; StringLength ? return strlen in RAX
; RCX = pString
;------------------------------------------------------------------------------
StringLength PROC FRAME
    .endprolog
    mov  rdi, rcx
    xor  ecx, ecx
    dec  rcx
    xor  eax, eax
    repne scasb
    not  rcx
    lea  rax, [rcx-1]
    ret
StringLength ENDP

;------------------------------------------------------------------------------
; StringCopy ? copy NUL-terminated src?dest, return dest in RAX
; RCX=pDest, RDX=pSrc
;------------------------------------------------------------------------------
StringCopy PROC FRAME
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    .endprolog

    mov  rdi, rcx                   ; dest
    mov  rsi, rdx                   ; src
    mov  rax, rcx                   ; return dest
@@:
    lodsb
    stosb
    test al, al
    jnz  @B

    pop  rsi
    pop  rdi
    ret
StringCopy ENDP

;------------------------------------------------------------------------------
; StringConcat ? append NUL-terminated src to end of dest
; RCX=pDest, RDX=pSrc, returns pDest in RAX
;------------------------------------------------------------------------------
StringConcat PROC FRAME
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    .endprolog

    mov  rdi, rcx
    mov  rsi, rdx
    mov  rax, rcx                   ; return value

    ; seek to NUL in dest
    xor  ecx, ecx
    dec  rcx
    xor  eax, eax
    repne scasb
    dec  rdi                        ; back up over NUL

    ; copy src in
@@:
    lodsb
    stosb
    test al, al
    jnz  @B

    mov  rax, rcx                   ; (clobber ok, caller ignores)
    pop  rsi
    pop  rdi
    ret
StringConcat ENDP

;------------------------------------------------------------------------------
; HttpPostOllama ? POST JSON to localhost:11434/api/generate via WinHTTP
; RCX=pPrompt (NUL term), RDX=pResponseOut (buffer to receive response body)
; Returns bytes read in RAX, 0 on failure
;------------------------------------------------------------------------------
HttpPostOllama PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 0C0h
    .allocstack 0C0h
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .endprolog

    mov  r12, rcx                   ; pPrompt
    mov  r13, rdx                   ; pResponseOut

    ; ---- Build JSON payload into g_JsonBuffer ----
    lea  rcx, g_JsonBuffer
    lea  rdx, g_JsonModelStart
    call StringCopy
    lea  rcx, g_JsonBuffer
    lea  rdx, g_ModelName
    call StringConcat
    lea  rcx, g_JsonBuffer
    lea  rdx, g_JsonPromptStart
    call StringConcat
    lea  rcx, g_JsonBuffer
    mov  rdx, r12
    call StringConcat
    lea  rcx, g_JsonBuffer
    lea  rdx, g_JsonOptions
    call StringConcat

    ; ---- JSON payload length ----
    lea  rcx, g_JsonBuffer
    call StringLength
    mov  rbx, rax                   ; payload length

    ; ---- WinHttpOpen ----
    xor  ecx, ecx                   ; user-agent = NULL
    mov  edx, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
    xor  r8, r8
    xor  r9, r9
    mov  qword ptr [rsp+20h], 0
    call WinHttpOpen
    test rax, rax
    jz   HttpFail
    mov  [rbp-30h], rax             ; hSession

    ; ---- WinHttpSetTimeouts — Ollama model gen can take minutes ----
    mov  rcx, rax                   ; hSession
    xor  edx, edx                   ; resolve = 0 (infinite)
    mov  r8d, 30000                 ; connect = 30s
    mov  r9d, 60000                 ; send    = 60s
    mov  dword ptr [rsp+20h], 300000 ; receive = 300s (5 min)
    call WinHttpSetTimeouts

    ; ---- WinHttpConnect(hSession, "localhost", 11434, 0) ----
    mov  rcx, rax
    lea  rdx, g_OllamaHost
    mov  r8d, 11434
    xor  r9d, r9d
    call WinHttpConnect
    test rax, rax
    jz   HttpCloseSession
    mov  [rbp-38h], rax             ; hConnect

    ; ---- WinHttpOpenRequest(hConnect, "POST", "/api/generate", ...) ----
    mov  rcx, rax
    lea  rdx, szPost
    lea  r8,  g_OllamaPath
    xor  r9, r9                     ; HTTP/1.1
    mov  qword ptr [rsp+20h], 0     ; accept types
    mov  qword ptr [rsp+28h], 0     ; referrer
    mov  qword ptr [rsp+30h], 0     ; flags
    call WinHttpOpenRequest
    test rax, rax
    jz   HttpCloseConnect
    mov  [rbp-40h], rax             ; hRequest

    ; ---- Add Content-Type header ----
    mov  rcx, rax
    lea  rdx, szContentType
    mov  r8d, -1                    ; auto-length
    mov  r9d, 0                     ; modifiers
    mov  qword ptr [rsp+20h], 0
    call WinHttpAddRequestHeaders

    ; ---- WinHttpSendRequest ----
    mov  rcx, [rbp-40h]
    xor  edx, edx                   ; additional headers
    xor  r8d, r8d                   ; headers length
    lea  r9,  g_JsonBuffer          ; optional data
    mov  qword ptr [rsp+20h], rbx   ; optional length
    mov  qword ptr [rsp+28h], rbx   ; total length
    mov  qword ptr [rsp+30h], 0     ; context
    call WinHttpSendRequest
    test eax, eax
    jz   HttpCloseRequest

    ; ---- WinHttpReceiveResponse ----
    mov  rcx, [rbp-40h]
    xor  edx, edx
    call WinHttpReceiveResponse
    test eax, eax
    jz   HttpCloseRequest

    ; ---- Read response loop ----
    mov  rdi, r13                   ; write cursor
    xor  esi, esi                   ; total bytes

ReadLoop:
    lea  rdx, [rbp-48h]            ; &dwSize
    mov  rcx, [rbp-40h]
    call WinHttpQueryDataAvailable
    test eax, eax
    jz   ReadDone
    mov  eax, dword ptr [rbp-48h]
    test eax, eax
    jz   ReadDone
    cmp  eax, 4096
    jle  @F
    mov  eax, 4096
@@:
    mov  [rbp-4Ch], eax            ; clamp

    ; WinHttpReadData(hRequest, pBuf, toRead, &bytesRead)
    mov  rcx, [rbp-40h]
    mov  rdx, rdi
    mov  r8d, eax
    lea  r9,  [rbp-50h]
    call WinHttpReadData
    test eax, eax
    jz   ReadDone

    mov  eax, dword ptr [rbp-50h]
    add  esi, eax
    add  rdi, rax                  ; advance write cursor
    jmp  ReadLoop

ReadDone:
    mov  byte ptr [rdi], 0         ; NUL terminate
    mov  eax, esi                  ; return total bytes
    jmp  HttpCleanupAll

HttpCloseRequest:
    mov  rcx, [rbp-40h]
    call WinHttpCloseHandle
HttpCloseConnect:
    mov  rcx, [rbp-38h]
    call WinHttpCloseHandle
HttpCloseSession:
    mov  rcx, [rbp-30h]
    call WinHttpCloseHandle
HttpFail:
    xor  eax, eax
    jmp  HttpExit

HttpCleanupAll:
    push rax                       ; save return value
    mov  rcx, [rbp-40h]
    call WinHttpCloseHandle
    mov  rcx, [rbp-38h]
    call WinHttpCloseHandle
    mov  rcx, [rbp-30h]
    call WinHttpCloseHandle
    pop  rax

HttpExit:
    pop  r13
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    leave
    ret
HttpPostOllama ENDP

;------------------------------------------------------------------------------
; ExtractJSONResponse ? find "response":"..." in JSON, copy value to pOutput
; RCX=pJson, RDX=pOutput
; Returns pOutput in RAX, or 0 on failure
;------------------------------------------------------------------------------
ExtractJSONResponse PROC FRAME
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    .endprolog

    mov  rsi, rcx                   ; pJson
    mov  rdi, rdx                   ; pOutput
    mov  rbx, rdx                   ; save for return

    ; Find the substring "response" in the JSON
ScanJson:
    mov  al, byte ptr [rsi]
    test al, al
    jz   ExtractFail
    cmp  al, 'r'
    jne  ScanNext
    ; check "response"
    cmp  byte ptr [rsi+1], 'e'
    jne  ScanNext
    cmp  byte ptr [rsi+2], 's'
    jne  ScanNext
    cmp  byte ptr [rsi+3], 'p'
    jne  ScanNext
    cmp  byte ptr [rsi+4], 'o'
    jne  ScanNext
    cmp  byte ptr [rsi+5], 'n'
    jne  ScanNext
    cmp  byte ptr [rsi+6], 's'
    jne  ScanNext
    cmp  byte ptr [rsi+7], 'e'
    jne  ScanNext
    ; Found "response" ? skip past ":"
    add  rsi, 8
    jmp  SkipToColon

ScanNext:
    inc  rsi
    jmp  ScanJson

SkipToColon:
    mov  al, byte ptr [rsi]
    test al, al
    jz   ExtractFail
    cmp  al, ':'
    je   FoundColon
    inc  rsi
    jmp  SkipToColon

FoundColon:
    inc  rsi                        ; skip ':'
SkipWhitespace:
    mov  al, byte ptr [rsi]
    cmp  al, ' '
    je   SkipWS
    cmp  al, 9                      ; tab
    je   SkipWS
    cmp  al, '"'
    je   BeginCopy
    jmp  ExtractFail
SkipWS:
    inc  rsi
    jmp  SkipWhitespace

BeginCopy:
    inc  rsi                        ; skip opening quote
CopyLoop:
    mov  al, byte ptr [rsi]
    test al, al
    jz   CopyDone
    cmp  al, '"'
    je   CopyDone
    cmp  al, '\'
    jne  StoreChar
    inc  rsi                        ; skip backslash
    mov  al, byte ptr [rsi]
    cmp  al, 'n'
    jne  @F
    mov  al, 10                     ; \n ? LF
@@:
    cmp  al, 'r'
    jne  @F
    mov  al, 13                     ; \r ? CR
@@:
StoreChar:
    mov  byte ptr [rdi], al
    inc  rdi
    inc  rsi
    jmp  CopyLoop

CopyDone:
    mov  byte ptr [rdi], 0
    mov  rax, rbx                   ; return pOutput
    jmp  ExtractExit

ExtractFail:
    xor  eax, eax

ExtractExit:
    pop  rbx
    pop  rdi
    pop  rsi
    ret
ExtractJSONResponse ENDP

;------------------------------------------------------------------------------
; GenerateCodeFile ? write template + AI fill to disk
; RCX=pTemplate, RDX=pAIResponse, R8=pOutputPath
; Returns 1 success, 0 failure
;------------------------------------------------------------------------------
GenerateCodeFile PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 80h
    .allocstack 80h
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    .endprolog

    mov  rsi, rcx                   ; pTemplate
    mov  rbx, rdx                   ; pAIResponse
    mov  r12, r8                    ; pOutputPath

    ; Clear code buffer
    lea  rdi, g_CodeBuffer
    mov  ecx, 65536/8
    xor  eax, eax
    rep  stosq

    ; Copy template into g_CodeBuffer
    lea  rcx, g_CodeBuffer
    mov  rdx, rsi
    call StringCopy

    ; ---- CreateFileA for output ----
    mov  rcx, r12                   ; lpFileName
    mov  edx, GENERIC_WRITE
    xor  r8d, r8d                   ; no sharing
    xor  r9, r9                     ; no security
    mov  qword ptr [rsp+20h], CREATE_ALWAYS
    mov  qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov  qword ptr [rsp+30h], 0
    call CreateFileA
    cmp  rax, INVALID_HANDLE_VALUE
    je   GenFail
    mov  [rbp-30h], rax             ; hFile

    ; ---- Get length of buffer ----
    lea  rcx, g_CodeBuffer
    call StringLength
    mov  r8d, eax                   ; bytes to write

    ; ---- WriteFile ----
    mov  rcx, [rbp-30h]
    lea  rdx, g_CodeBuffer
    ; r8d set
    lea  r9, [rbp-38h]             ; &written
    mov  qword ptr [rsp+20h], 0
    call WriteFile

    ; ---- CloseHandle ----
    mov  rcx, [rbp-30h]
    call CloseHandle

    ; Print confirmation
    lea  rcx, szFileCreated
    call PrintConsole
    mov  rcx, r12
    call PrintConsole
    lea  rcx, szNewline
    call PrintConsole

    mov  eax, 1
    jmp  GenExit

GenFail:
    xor  eax, eax

GenExit:
    pop  r12
    pop  rbx
    pop  rdi
    pop  rsi
    leave
    ret
GenerateCodeFile ENDP

;------------------------------------------------------------------------------
; AssembleFile ? spawn ml64.exe /c /Fo<obj> <asm>
; RCX=pAsmPath, RDX=pObjPath
; Returns 1 success, 0 failure
;------------------------------------------------------------------------------
AssembleFile PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 200h                  ; locals: si, pi, cmdLine
    .allocstack 200h
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .endprolog

    mov  r12, rcx                   ; pAsmPath
    mov  r13, rdx                   ; pObjPath

    ; ---- Build command line: "ml64.exe /c /Fo<obj> <asm> /nologo /W3" ----
    lea  rcx, g_CmdLine
    lea  rdx, g_ML64Path
    call StringCopy

    lea  rcx, g_CmdLine
    lea  rdx, szML64Arg_c
    call StringConcat

    lea  rcx, g_CmdLine
    lea  rdx, szML64Arg_Fo
    call StringConcat

    lea  rcx, g_CmdLine
    mov  rdx, r13
    call StringConcat

    lea  rcx, g_CmdLine
    lea  rdx, szML64Arg_space
    call StringConcat

    lea  rcx, g_CmdLine
    mov  rdx, r12
    call StringConcat

    lea  rcx, g_CmdLine
    lea  rdx, szML64Arg_flags
    call StringConcat

    ; ---- Zero STARTUPINFOA ----
    lea  rdi, [rbp-0A0h]           ; si at rbp-0A0h
    mov  ecx, SIZEOF STARTUPINFOA
    xor  eax, eax
    rep  stosb
    mov  dword ptr [rbp-0A0h], SIZEOF STARTUPINFOA   ; si.cb

    ; ---- Zero PROCESS_INFORMATION ----
    lea  rdi, [rbp-0C0h]           ; pi at rbp-0C0h
    mov  ecx, SIZEOF PROCESS_INFORMATION
    xor  eax, eax
    rep  stosb

    ; ---- CreateProcessA ----
    xor  ecx, ecx                   ; lpApplicationName = NULL
    lea  rdx, g_CmdLine             ; lpCommandLine
    xor  r8, r8                     ; lpProcessAttributes
    xor  r9, r9                     ; lpThreadAttributes
    mov  qword ptr [rsp+20h], 0     ; bInheritHandles
    mov  qword ptr [rsp+28h], NORMAL_PRIORITY_CLASS
    mov  qword ptr [rsp+30h], 0     ; lpEnvironment
    mov  qword ptr [rsp+38h], 0     ; lpCurrentDirectory
    lea  rax, [rbp-0A0h]
    mov  qword ptr [rsp+40h], rax   ; lpStartupInfo
    lea  rax, [rbp-0C0h]
    mov  qword ptr [rsp+48h], rax   ; lpProcessInformation
    call CreateProcessA
    test eax, eax
    jz   AsmFail

    ; ---- Wait for ml64 to finish (60s timeout) ----
    mov  rcx, qword ptr [rbp-0C0h] ; pi.hProcess
    mov  edx, 60000
    call WaitForSingleObject

    ; ---- Close handles ----
    mov  rcx, qword ptr [rbp-0C0h] ; pi.hProcess
    call CloseHandle
    mov  rcx, qword ptr [rbp-0B8h] ; pi.hThread
    call CloseHandle

    ; Print success
    lea  rcx, szAssembling
    call PrintConsole
    mov  rcx, r12
    call PrintConsole
    lea  rcx, szNewline
    call PrintConsole

    mov  eax, 1
    jmp  AsmExit

AsmFail:
    xor  eax, eax

AsmExit:
    pop  r13
    pop  r12
    leave
    ret

; Local string constants
szML64Arg_c     BYTE " /c",0
szML64Arg_Fo    BYTE " /Fo",0
szML64Arg_space BYTE " ",0
szML64Arg_flags BYTE " /nologo /W3",0

AssembleFile ENDP

;------------------------------------------------------------------------------
; AgentThread ? autonomous loop: prompt Ollama ? generate ? assemble
; lpParam in RCX (unused)
;------------------------------------------------------------------------------
AgentThread PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 100h
    .allocstack 100h
    .endprolog

AgentLoop:
    cmp  g_RunAgent, 0
    je   AgentExit

    ; ---- Build prompt: context + task description ----
    lea  rcx, g_ResponseBuffer
    xor  eax, eax
    mov  byte ptr [rcx], al         ; clear buffer for prompt construction

    lea  rcx, g_ResponseBuffer
    lea  rdx, g_PromptContext
    call StringCopy

    lea  rcx, g_ResponseBuffer
    lea  rdx, szTaskCompletion
    call StringConcat

    ; ---- HTTP POST to Ollama ----
    lea  rcx, g_ResponseBuffer      ; prompt
    lea  rdx, g_CodeBuffer          ; response output
    call HttpPostOllama
    test eax, eax
    jz   WaitAndRetry

    ; ---- Extract "response" field from JSON ----
    lea  rcx, g_CodeBuffer
    lea  rdx, g_ResponseBuffer     ; reuse buffer for extracted code
    call ExtractJSONResponse
    test rax, rax
    jz   WaitAndRetry

    ; ---- Build output file path: D:\rawrxd\src\CompletionEngine.asm ----
    lea  rcx, g_FilePath
    lea  rdx, g_SrcDirectory
    call StringCopy
    lea  rcx, g_FilePath
    lea  rdx, szFileCompletion
    call StringConcat

    ; ---- Write template + AI output to file ----
    lea  rcx, g_Pattern_CompletionEngine  ; template
    lea  rdx, g_ResponseBuffer            ; AI response
    lea  r8,  g_FilePath                  ; output path
    call GenerateCodeFile
    test eax, eax
    jz   WaitAndRetry

    ; ---- Assemble the generated file ----
    ; Build obj path: D:\rawrxd\build_prod\CompletionEngine.obj
    lea  rcx, g_CmdLine             ; reuse for obj path
    lea  rdx, g_BuildDirectory
    call StringCopy
    lea  rcx, g_CmdLine
    lea  rdx, szObjCompletion
    call StringConcat

    lea  rcx, g_FilePath            ; asm path
    lea  rdx, g_CmdLine             ; obj path
    call AssembleFile

    lea  rcx, szGenComplete
    call PrintConsole

WaitAndRetry:
    mov  ecx, 5000                  ; 5 second delay
    call Sleep
    jmp  AgentLoop

AgentExit:
    xor  eax, eax
    leave
    ret

szObjCompletion BYTE "CompletionEngine.obj",0
AgentThread ENDP

;------------------------------------------------------------------------------
; WatcherThread ? monitor D:\rawrxd\src\ for changes, print notification
; lpParam in RCX (unused)
;------------------------------------------------------------------------------
WatcherThread PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 1100h                 ; 4096-byte buffer + locals
    .allocstack 1100h
    .endprolog

    ; ---- Open directory for watching ----
    lea  rcx, g_SrcDirectory
    mov  edx, FILE_LIST_DIRECTORY
    mov  r8d, FILE_SHARE_READ OR FILE_SHARE_WRITE
    xor  r9, r9                     ; no security
    mov  qword ptr [rsp+20h], OPEN_EXISTING
    mov  qword ptr [rsp+28h], FILE_FLAG_BACKUP_SEMANTICS
    mov  qword ptr [rsp+30h], 0
    call CreateFileA
    cmp  rax, INVALID_HANDLE_VALUE
    je   WatchFail
    mov  [rbp-8h], rax              ; hDir

WatchLoop:
    cmp  g_RunAgent, 0
    je   WatchCleanup

    ; ReadDirectoryChangesW(hDir, buf, 4096, TRUE, filter, &bytesRet, NULL, NULL)
    mov  rcx, [rbp-8h]
    lea  rdx, [rbp-1008h]          ; 4096 byte buffer
    mov  r8d, 4096
    mov  r9d, 1                     ; bWatchSubtree = TRUE
    mov  qword ptr [rsp+20h], FILE_NOTIFY_CHANGE_LAST_WRITE
    lea  rax, [rbp-10h]
    mov  qword ptr [rsp+28h], rax   ; &bytesReturned
    mov  qword ptr [rsp+30h], 0     ; lpOverlapped
    mov  qword ptr [rsp+38h], 0     ; completion routine
    call ReadDirectoryChangesW
    test eax, eax
    jz   WatchLoop                  ; error ? retry

    ; Change detected ? print notification
    lea  rcx, szHotpatch
    call PrintConsole

    ; Small debounce
    mov  ecx, 500
    call Sleep

    jmp  WatchLoop

WatchCleanup:
    mov  rcx, [rbp-8h]
    call CloseHandle
WatchFail:
    xor  eax, eax
    leave
    ret
WatcherThread ENDP

;------------------------------------------------------------------------------
; GenesisMain ? entry point
;------------------------------------------------------------------------------
GenesisMain PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 80h
    .allocstack 80h
    .endprolog

    ; ---- Banner ----
    lea  rcx, szGenStart
    call PrintConsole

    ; ---- Ensure directories exist ----
    lea  rcx, g_SrcDirectory
    xor  edx, edx
    call CreateDirectoryA

    lea  rcx, g_BuildDirectory
    xor  edx, edx
    call CreateDirectoryA

    ; ---- Start Agent thread ----
    xor  ecx, ecx                   ; lpThreadAttributes
    xor  edx, edx                   ; dwStackSize = default
    lea  r8,  AgentThread            ; lpStartAddress
    xor  r9, r9                     ; lpParameter
    mov  qword ptr [rsp+20h], 0     ; dwCreationFlags
    lea  rax, [rbp-10h]
    mov  qword ptr [rsp+28h], rax   ; lpThreadId
    call CreateThread
    mov  g_ThreadHandle, rax

    ; ---- Start Watcher thread ----
    xor  ecx, ecx
    xor  edx, edx
    lea  r8,  WatcherThread
    xor  r9, r9
    mov  qword ptr [rsp+20h], 0
    lea  rax, [rbp-18h]
    mov  qword ptr [rsp+28h], rax
    call CreateThread
    mov  g_WatcherHandle, rax

    ; ---- Main loop: wait for 'q' on stdin ----
MainLoop:
    mov  ecx, 200
    call Sleep

    ; Try reading a byte from stdin
    mov  ecx, STD_INPUT_HANDLE
    call GetStdHandle
    mov  [rbp-20h], rax             ; hStdin

    mov  rcx, rax
    lea  rdx, [rbp-28h]            ; 1-byte buffer
    mov  r8d, 1
    lea  r9, [rbp-30h]             ; &bytesRead
    mov  qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz   MainLoop

    mov  al, byte ptr [rbp-28h]
    cmp  al, 'q'
    je   Shutdown
    cmp  al, 'Q'
    je   Shutdown
    jmp  MainLoop

Shutdown:
    lea  rcx, szShutdown
    call PrintConsole

    mov  g_RunAgent, 0

    ; Wait for agent thread
    mov  rcx, g_ThreadHandle
    mov  edx, 10000
    call WaitForSingleObject

    ; Wait for watcher thread
    mov  rcx, g_WatcherHandle
    mov  edx, 5000
    call WaitForSingleObject

    ; Close thread handles
    mov  rcx, g_ThreadHandle
    call CloseHandle
    mov  rcx, g_WatcherHandle
    call CloseHandle

    xor  ecx, ecx
    call ExitProcess

    ; unreachable
    leave
    ret
GenesisMain ENDP

END

