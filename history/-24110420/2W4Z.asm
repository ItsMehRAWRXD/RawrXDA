;==============================================================================
; RawrXD_HttpClient_Test.asm — Standalone test harness for Http client
; Links with RawrXD_HttpClient_Real.obj
; Tests: Init → POST /api/tags → parse response → shutdown
;==============================================================================
OPTION CASEMAP:NONE

EXTERNDEF ExitProcess:PROC
EXTERNDEF GetStdHandle:PROC
EXTERNDEF WriteFile:PROC

; Http client exports
EXTERNDEF Http_Initialize:PROC
EXTERNDEF Http_Shutdown:PROC
EXTERNDEF Http_PostJSON:PROC
EXTERNDEF Http_GetErrorInfo:PROC
EXTERNDEF Json_Tokenize:PROC
EXTERNDEF Json_TokenCount:PROC
EXTERNDEF Json_FindKey:PROC
EXTERNDEF Json_ExtractString:PROC

STD_OUTPUT_HANDLE EQU -11

.DATA
ALIGN 1
szBanner    BYTE "[TEST] RawrXD Real HTTP Client Test",13,10,0
szInitOk    BYTE "[TEST] Http_Initialize: OK",13,10,0
szInitFail  BYTE "[TEST] Http_Initialize: FAILED",13,10,0
szTestPost  BYTE "[TEST] Posting to Ollama /api/tags...",13,10,0
szPostOk    BYTE "[TEST] Http_PostJSON: OK, bytes=",0
szPostFail  BYTE "[TEST] Http_PostJSON: FAILED (is Ollama running?)",13,10,0
szTokenize  BYTE "[TEST] Json_Tokenize: tokens=",0
szShutdown  BYTE "[TEST] Http_Shutdown complete",13,10,0
szNewline   BYTE 13,10,0

; Simple test payload — GET equivalent via POST to /api/tags
szTestBody  BYTE "{}",0

; Wide strings for alternate endpoints
ALIGN 2
wTagsPath   DW '/','a','p','i','/','t','a','g','s',0

.DATA?
ALIGN 16
g_TestResp  BYTE 65536 DUP(?)
g_TestExtr  BYTE 4096 DUP(?)
g_TestNum   BYTE 32 DUP(?)

.CODE

;------------------------------------------------------------------------------
; _Print — write string to stdout
;------------------------------------------------------------------------------
_Print PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    push rsi
    .pushreg rsi
    .endprolog

    mov  rsi, rcx
    ; strlen
    xor  eax, eax
    mov  rdx, rsi
@@:
    cmp  byte ptr [rdx], 0
    je   @F
    inc  rdx
    jmp  @B
@@:
    sub  rdx, rsi
    mov  r8d, edx

    mov  ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov  rcx, rax
    mov  rdx, rsi
    lea  r9, [rbp-30h]
    mov  qword ptr [rsp+20h], 0
    call WriteFile

    pop  rsi
    leave
    ret
_Print ENDP

;------------------------------------------------------------------------------
; _PrintNum — print unsigned 32-bit number
;------------------------------------------------------------------------------
_PrintNum PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    .endprolog

    lea  rdi, g_TestNum
    add  rdi, 30
    mov  byte ptr [rdi], 0
    dec  rdi
    mov  eax, ecx
    test eax, eax
    jnz  @F
    mov  byte ptr [rdi], '0'
    mov  rcx, rdi
    call _Print
    jmp  _pn_done
@@:
    xor  edx, edx
    mov  ebx, 10
    div  ebx
    add  dl, '0'
    mov  byte ptr [rdi], dl
    dec  rdi
    test eax, eax
    jnz  @B
    inc  rdi
    mov  rcx, rdi
    call _Print

_pn_done:
    pop  rdi
    pop  rbx
    ret
_PrintNum ENDP

;------------------------------------------------------------------------------
; TestMain — entry point
;------------------------------------------------------------------------------
TestMain PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 60h
    .allocstack 60h
    .endprolog

    ; Banner
    lea  rcx, szBanner
    call _Print

    ; Initialize HTTP (default port 11434)
    xor  ecx, ecx
    call Http_Initialize
    test eax, eax
    jz   _tm_init_fail

    lea  rcx, szInitOk
    call _Print

    ; POST test
    lea  rcx, szTestPost
    call _Print

    lea  rcx, szTestBody
    lea  rdx, g_TestResp
    mov  r8d, 65536
    lea  r9, wTagsPath
    call Http_PostJSON
    test eax, eax
    jz   _tm_post_fail

    ; Print bytes received
    push rax
    lea  rcx, szPostOk
    call _Print
    pop  rcx
    call _PrintNum
    lea  rcx, szNewline
    call _Print

    ; Tokenize the response
    lea  rcx, g_TestResp
    call Json_Tokenize
    push rax
    lea  rcx, szTokenize
    call _Print
    pop  rcx
    call _PrintNum
    lea  rcx, szNewline
    call _Print

    jmp  _tm_shutdown

_tm_init_fail:
    lea  rcx, szInitFail
    call _Print
    jmp  _tm_exit

_tm_post_fail:
    lea  rcx, szPostFail
    call _Print

_tm_shutdown:
    call Http_Shutdown
    lea  rcx, szShutdown
    call _Print

_tm_exit:
    xor  ecx, ecx
    call ExitProcess

    leave
    ret
TestMain ENDP

END
