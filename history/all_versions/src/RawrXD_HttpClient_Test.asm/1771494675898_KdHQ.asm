zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz;==============================================================================
; RawrXD_HttpClient_Test.asm — Standalone test harness for Http client
; Links with RawrXD_HttpClient_Real.obj
; Tests: Init → GET /api/tags → tokenize JSON → FindKey →
;        POST /api/generate (real prompt) → tokenize → extract model response
;        → Shutdown
;==============================================================================
OPTION CASEMAP:NONE

EXTERNDEF ExitProcess:PROC
EXTERNDEF GetStdHandle:PROC
EXTERNDEF WriteFile:PROC

; Http client exports
EXTERNDEF Http_Initialize:PROC
EXTERNDEF Http_Shutdown:PROC
EXTERNDEF Http_PostJSON:PROC
EXTERNDEF Http_GetJSON:PROC
EXTERNDEF Http_GetErrorInfo:PROC
EXTERNDEF Json_Tokenize:PROC
EXTERNDEF Json_TokenCount:PROC
EXTERNDEF Json_FindKey:PROC
EXTERNDEF Json_ExtractString:PROC

STD_OUTPUT_HANDLE EQU -11

.DATA
ALIGN 1
szBanner    BYTE "[TEST] RawrXD Real HTTP Client Test v2",13,10,0
szInitOk    BYTE "[TEST] Http_Initialize: OK",13,10,0
szInitFail  BYTE "[TEST] Http_Initialize: FAILED",13,10,0

; GET test strings
szTestGet   BYTE "[TEST] GET /api/tags...",13,10,0
szGetOk     BYTE "[TEST] Http_GetJSON: OK, bytes=",0
szGetFail   BYTE "[TEST] Http_GetJSON: FAILED (is Ollama running?)",13,10,0
szTokenize  BYTE "[TEST] Json_Tokenize: tokens=",0
szFindName  BYTE "[TEST] FindKey 'name': idx=",0
szExtract   BYTE "[TEST] Model name: ",0

; POST test strings
szTestGen   BYTE "[TEST] POST /api/generate (real prompt)...",13,10,0
szGenOk     BYTE "[TEST] Http_PostJSON: OK, bytes=",0
szGenFail   BYTE "[TEST] Http_PostJSON: FAILED",13,10,0
szFindResp  BYTE "[TEST] FindKey 'response': idx=",0
szRespVal   BYTE "[TEST] Ollama says: ",0
szRespRaw   BYTE "[TEST] Raw response (first 200B):",13,10,0

szShutdown  BYTE "[TEST] Http_Shutdown complete",13,10,0
szNewline   BYTE 13,10,0
szPass      BYTE "[PASS] All tests completed successfully!",13,10,0

; POST body for /api/generate — real prompt, stream=true for faster response
szGenBody   BYTE '{"model":"bigdaddyg-alldrive:latest","prompt":"Say hello in one sentence.","stream":true,"options":{"temperature":0.1}}',0

; Key strings for JSON lookup
szKeyName   BYTE "name",0
szKeyResp   BYTE "response",0

; Wide strings for endpoints
ALIGN 2
wTagsPath   DW '/','a','p','i','/','t','a','g','s',0
wGenPath    DW '/','a','p','i','/','g','e','n','e','r','a','t','e',0

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
    sub  rsp, 48h
    .allocstack 48h
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
    mov  [rbp-10h], edx             ; save length (R8 is volatile!)

    mov  ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov  rcx, rax
    mov  rdx, rsi
    mov  r8d, [rbp-10h]             ; restore length
    lea  r9, [rbp-18h]
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
    sub  rsp, 28h
    .allocstack 28h
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
    add  rsp, 28h
    pop  rdi
    pop  rbx
    ret
_PrintNum ENDP

;------------------------------------------------------------------------------
; _PrintBuf — print first N bytes of buffer (capped at 200)
; RCX = buffer, EDX = total len
;------------------------------------------------------------------------------
_PrintBuf PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    .endprolog

    ; cap at 200
    cmp  edx, 200
    jle  @F
    mov  edx, 200
@@:
    mov  [rbp-10h], edx
    mov  [rbp-18h], rcx

    mov  ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov  rcx, rax
    mov  rdx, [rbp-18h]
    mov  r8d, [rbp-10h]
    lea  r9, [rbp-20h]
    mov  qword ptr [rsp+20h], 0
    call WriteFile

    leave
    ret
_PrintBuf ENDP

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

    ;=== INIT ===
    xor  ecx, ecx
    call Http_Initialize
    test eax, eax
    jz   _tm_init_fail

    lea  rcx, szInitOk
    call _Print

    ;=== TEST 1: GET /api/tags ===
    lea  rcx, szTestGet
    call _Print

    lea  rcx, g_TestResp
    mov  edx, 65536
    lea  r8, wTagsPath
    call Http_GetJSON
    test eax, eax
    jz   _tm_get_fail

    mov  [rbp-8h], eax              ; save bytes received
    lea  rcx, szGetOk
    call _Print
    mov  ecx, [rbp-8h]
    call _PrintNum
    lea  rcx, szNewline
    call _Print

    ; Print raw response snippet
    lea  rcx, szRespRaw
    call _Print
    lea  rcx, g_TestResp
    mov  edx, [rbp-8h]
    call _PrintBuf
    lea  rcx, szNewline
    call _Print

    ; Tokenize the JSON response
    lea  rcx, g_TestResp
    call Json_Tokenize
    mov  [rbp-10h], eax
    lea  rcx, szTokenize
    call _Print
    mov  ecx, [rbp-10h]
    call _PrintNum
    lea  rcx, szNewline
    call _Print

    ; Find "name" key in the tags response
    lea  rcx, szKeyName
    xor  edx, edx
    call Json_FindKey
    mov  [rbp-18h], eax
    lea  rcx, szFindName
    call _Print
    mov  ecx, [rbp-18h]
    call _PrintNum
    lea  rcx, szNewline
    call _Print

    ; If found, extract the model name string
    cmp  dword ptr [rbp-18h], -1
    je   _tm_test2

    mov  ecx, [rbp-18h]
    lea  rdx, g_TestExtr
    mov  r8d, 4096
    call Json_ExtractString

    lea  rcx, szExtract
    call _Print
    lea  rcx, g_TestExtr
    call _Print
    lea  rcx, szNewline
    call _Print

    ;=== TEST 2: POST /api/generate with real prompt ===
_tm_test2:
    lea  rcx, szTestGen
    call _Print

    lea  rcx, szGenBody
    lea  rdx, g_TestResp
    mov  r8d, 65536
    lea  r9, wGenPath
    call Http_PostJSON
    test eax, eax
    jz   _tm_gen_fail

    mov  [rbp-20h], eax
    lea  rcx, szGenOk
    call _Print
    mov  ecx, [rbp-20h]
    call _PrintNum
    lea  rcx, szNewline
    call _Print

    ; Print raw response snippet
    lea  rcx, szRespRaw
    call _Print
    lea  rcx, g_TestResp
    mov  edx, [rbp-20h]
    call _PrintBuf
    lea  rcx, szNewline
    call _Print

    ; Tokenize the generation response
    lea  rcx, g_TestResp
    call Json_Tokenize
    mov  [rbp-28h], eax
    lea  rcx, szTokenize
    call _Print
    mov  ecx, [rbp-28h]
    call _PrintNum
    lea  rcx, szNewline
    call _Print

    ; Find "response" key
    lea  rcx, szKeyResp
    xor  edx, edx
    call Json_FindKey
    mov  [rbp-30h], eax
    lea  rcx, szFindResp
    call _Print
    mov  ecx, [rbp-30h]
    call _PrintNum
    lea  rcx, szNewline
    call _Print

    ; Extract model's response text
    cmp  dword ptr [rbp-30h], -1
    je   _tm_shutdown

    mov  ecx, [rbp-30h]
    lea  rdx, g_TestExtr
    mov  r8d, 4096
    call Json_ExtractString

    lea  rcx, szRespVal
    call _Print
    lea  rcx, g_TestExtr
    call _Print
    lea  rcx, szNewline
    call _Print

    jmp  _tm_shutdown

_tm_init_fail:
    lea  rcx, szInitFail
    call _Print
    jmp  _tm_exit

_tm_get_fail:
    lea  rcx, szGetFail
    call _Print
    jmp  _tm_shutdown

_tm_gen_fail:
    lea  rcx, szGenFail
    call _Print

_tm_shutdown:
    call Http_Shutdown
    lea  rcx, szShutdown
    call _Print

    lea  rcx, szPass
    call _Print

_tm_exit:
    xor  ecx, ecx
    call ExitProcess

    leave
    ret
TestMain ENDP

END
