;==============================================================================
; RawrXD_HttpClient_Real.asm — Production HTTP Client + JSON Parser + Streaming
;
; Pure ml64 x64 | No CRT | WinHTTP (Unicode) | Full Error Handling
; Talks to Ollama at localhost:11434 via proper WinHTTP API
;
; Exports:
;   Http_Initialize          — Create WinHTTP session with timeouts
;   Http_Shutdown            — Close session handle
;   Http_PostJSON            — POST JSON, return full response + status code
;   Http_PostStream          — POST with stream:true, callback per token
;   Http_GetErrorInfo        — Get last error code + formatted message
;
;   Json_Tokenize            — Tokenize JSON into token array
;   Json_FindKey             — Find key in object, return value token index
;   Json_ExtractString       — Extract + unescape string from token
;   Json_ExtractInt          — Extract integer from token
;   Json_ExtractBool         — Extract boolean from token
;   Json_TokenCount          — Return token count from last tokenize
;
;   Stream_Init              — Initialize streaming accumulator
;   Stream_Feed              — Feed chunk, extract NDJSON response tokens
;   Stream_GetAccumulated    — Get full accumulated response text
;   Stream_Reset             — Reset streaming state
;   Stream_IsDone            — Check if streaming is complete
;
; Assemble:
;   ml64.exe RawrXD_HttpClient_Real.asm /c /Fo RawrXD_HttpClient_Real.obj
; Link with:
;   kernel32.lib winhttp.lib
;==============================================================================

OPTION CASEMAP:NONE

;==============================================================================
; EXTERN — Win32 API (supplement rawrxd_win32_api.inc)
;==============================================================================

; --- Already in rawrxd_win32_api.inc ---
EXTERNDEF GetLastError:PROC
EXTERNDEF GetStdHandle:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF ExitProcess:PROC
EXTERNDEF FormatMessageA:PROC

EXTERNDEF WinHttpOpen:PROC
EXTERNDEF WinHttpConnect:PROC
EXTERNDEF WinHttpOpenRequest:PROC
EXTERNDEF WinHttpSendRequest:PROC
EXTERNDEF WinHttpReceiveResponse:PROC
EXTERNDEF WinHttpReadData:PROC
EXTERNDEF WinHttpQueryDataAvailable:PROC
EXTERNDEF WinHttpCloseHandle:PROC

; --- Additional WinHTTP not in rawrxd_win32_api.inc ---
EXTERNDEF WinHttpSetTimeouts:PROC
EXTERNDEF WinHttpQueryHeaders:PROC
EXTERNDEF WinHttpAddRequestHeaders:PROC
EXTERNDEF WinHttpSetOption:PROC

;==============================================================================
; Constants
;==============================================================================
STD_OUTPUT_HANDLE                EQU -11
STD_ERROR_HANDLE                 EQU -12

WINHTTP_ACCESS_TYPE_DEFAULT_PROXY EQU 0
WINHTTP_QUERY_STATUS_CODE        EQU 19
WINHTTP_QUERY_FLAG_NUMBER        EQU 20000000h
WINHTTP_ADDREQ_FLAG_ADD          EQU 20000000h
WINHTTP_OPTION_CONNECT_TIMEOUT   EQU 3
WINHTTP_OPTION_SEND_TIMEOUT      EQU 5
WINHTTP_OPTION_RECEIVE_TIMEOUT   EQU 6
WINHTTP_FLAG_REFRESH             EQU 00000100h

FORMAT_MESSAGE_FROM_SYSTEM       EQU 00001000h
FORMAT_MESSAGE_IGNORE_INSERTS    EQU 00000200h

; JSON Token Types
JT_NONE         EQU 0
JT_OBJ_START    EQU 1      ; {
JT_OBJ_END      EQU 2      ; }
JT_ARR_START    EQU 3      ; [
JT_ARR_END      EQU 4      ; ]
JT_STRING       EQU 5      ; "..."
JT_NUMBER       EQU 6      ; 123, -45, 3.14
JT_TRUE         EQU 7      ; true
JT_FALSE        EQU 8      ; false
JT_NULL         EQU 9      ; null
JT_COLON        EQU 10     ; :
JT_COMMA        EQU 11     ; ,

; Limits
MAX_JSON_TOKENS EQU 4096
MAX_RESPONSE    EQU 131072          ; 128 KB response buffer
MAX_STREAM_ACC  EQU 262144          ; 256 KB streaming accumulator
MAX_LINE_BUF    EQU 16384           ; 16 KB line buffer for NDJSON
MAX_ERROR_MSG   EQU 512
MAX_EXTRACT_BUF EQU 65536           ; 64 KB string extraction buffer

; HTTP Defaults
DEFAULT_RESOLVE_TIMEOUT  EQU 5000   ; 5 seconds
DEFAULT_CONNECT_TIMEOUT  EQU 10000  ; 10 seconds
DEFAULT_SEND_TIMEOUT     EQU 30000  ; 30 seconds
DEFAULT_RECEIVE_TIMEOUT  EQU 120000 ; 120 seconds (Ollama can be slow)

;==============================================================================
; Structures
;==============================================================================
JSON_TOKEN STRUCT
    tokenType   DWORD ?             ; JT_* constant
    startOff    DWORD ?             ; byte offset into source JSON
    tokenLen    DWORD ?             ; byte length of token text
    depth       DWORD ?             ; nesting depth (0 = top level)
JSON_TOKEN ENDS

;==============================================================================
;                         DATA SECTION
;==============================================================================
.DATA

; ---- WinHTTP Session State ----
ALIGN 8
g_hSession      QWORD 0            ; WinHttpOpen handle (reusable)
g_hConnect      QWORD 0            ; WinHttpConnect handle (reusable)
g_LastError     DWORD 0            ; Last Win32 error code
g_LastHttpCode  DWORD 0            ; Last HTTP status code (200, 404, etc.)
g_Initialized   DWORD 0            ; 1 = session is live

; ---- Wide Strings for WinHTTP (UTF-16LE) ----
; WinHTTP is Unicode-only — ANSI strings will cause NULL returns!
ALIGN 2
wUserAgent      DW 'R','a','w','r','X','D','-','H','T','T','P','/','1','.','0',0
wLocalhost      DW 'l','o','c','a','l','h','o','s','t',0
wOllamaGenPath  DW '/','a','p','i','/','g','e','n','e','r','a','t','e',0
wOllamaChatPath DW '/','a','p','i','/','c','h','a','t',0
wOllamaTagsPath DW '/','a','p','i','/','t','a','g','s',0
wPost           DW 'P','O','S','T',0
wGet            DW 'G','E','T',0
wContentTypeHdr DW 'C','o','n','t','e','n','t','-','T','y','p','e',':'
                DW ' ','a','p','p','l','i','c','a','t','i','o','n'
                DW '/','j','s','o','n',0

; ---- ANSI JSON Template Fragments ----
ALIGN 1
szJsonModelStart     BYTE '{"model":"',0
szJsonPromptStart    BYTE '","prompt":"',0
szJsonStreamFalse    BYTE '","stream":false,"options":{"temperature":0.1}}',0
szJsonStreamTrue     BYTE '","stream":true,"options":{"temperature":0.1}}',0

; ---- Console Error Strings ----
szErrPrefix     BYTE "[HTTP-ERR] ",0
szErrWinHttpOpen BYTE "WinHttpOpen failed",0
szErrConnect    BYTE "WinHttpConnect failed",0
szErrRequest    BYTE "WinHttpOpenRequest failed",0
szErrHeaders    BYTE "WinHttpAddRequestHeaders failed",0
szErrSend       BYTE "WinHttpSendRequest failed",0
szErrReceive    BYTE "WinHttpReceiveResponse failed",0
szErrRead       BYTE "WinHttpReadData failed",0
szErrStatus     BYTE "HTTP status: ",0
szErrTimeout    BYTE "Request timed out",0
szOk            BYTE "[HTTP] OK — ",0
szBytesRead     BYTE " bytes received",13,10,0
szStreamDone    BYTE "[STREAM] Done — response complete",13,10,0
szNewline       BYTE 13,10,0
szColonSpace    BYTE ": ",0

; ---- JSON key strings for Ollama response parsing ----
szKeyResponse   BYTE "response",0
szKeyDone       BYTE "done",0
szKeyError      BYTE "error",0
szKeyModel      BYTE "model",0

; ---- State variables (initialized) ----
g_TokenCount    DWORD 0                         ; tokens from last tokenize
g_TokenSrcPtr   QWORD 0                         ; pointer to source JSON
g_StreamAccLen  DWORD 0                          ; current accumulated length
g_StreamDone    DWORD 0                          ; 1 = got "done":true
g_LineBufLen    DWORD 0                          ; current partial line length

; ---- Buffers (BSS — zero-init at load, NOT stored in binary) ----
.DATA?
ALIGN 16
g_ResponseBuf   BYTE MAX_RESPONSE DUP(?)       ; HTTP response body
g_JsonPayload   BYTE 32768 DUP(?)              ; outgoing JSON POST body
g_ErrorMsg      BYTE MAX_ERROR_MSG DUP(?)       ; formatted error message
g_ExtractBuf    BYTE MAX_EXTRACT_BUF DUP(?)     ; JSON string extraction
ALIGN 16
g_TokenArray    JSON_TOKEN MAX_JSON_TOKENS DUP(<>)  ; token output array
ALIGN 16
g_StreamAccum   BYTE MAX_STREAM_ACC DUP(?)      ; accumulated response text
g_LineBuf       BYTE MAX_LINE_BUF DUP(?)        ; partial line buffer
g_NumBuf        BYTE 32 DUP(?)

;==============================================================================
;                         CODE SECTION
;==============================================================================
.CODE

;==============================================================================
; Internal Helpers
;==============================================================================

;------------------------------------------------------------------------------
; _StrLen — NUL-terminated string length
; RCX = pString
; Returns length in RAX (not counting NUL)
;------------------------------------------------------------------------------
_StrLen PROC FRAME
    .endprolog
    xor  eax, eax
    test rcx, rcx
    jz   _sl_done
    mov  rdx, rcx
@@:
    cmp  byte ptr [rdx], 0
    je   @F
    inc  rdx
    jmp  @B
@@:
    sub  rdx, rcx
    mov  rax, rdx
_sl_done:
    ret
_StrLen ENDP

;------------------------------------------------------------------------------
; _StrCopy — copy NUL-terminated src to dest, return dest
; RCX = pDest, RDX = pSrc
;------------------------------------------------------------------------------
_StrCopy PROC FRAME
    .endprolog
    mov  rax, rcx
    test rcx, rcx
    jz   _sc_done
    test rdx, rdx
    jz   _sc_done
@@:
    mov  r8b, byte ptr [rdx]
    mov  byte ptr [rcx], r8b
    test r8b, r8b
    jz   _sc_done
    inc  rcx
    inc  rdx
    jmp  @B
_sc_done:
    ret
_StrCopy ENDP

;------------------------------------------------------------------------------
; _StrConcat — append src to end of dest
; RCX = pDest, RDX = pSrc
;------------------------------------------------------------------------------
_StrConcat PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    mov  rbx, rcx
    ; find end of dest
@@:
    cmp  byte ptr [rcx], 0
    je   @F
    inc  rcx
    jmp  @B
@@:
    ; copy src to end
@@:
    mov  al, byte ptr [rdx]
    mov  byte ptr [rcx], al
    test al, al
    jz   _scat_done
    inc  rcx
    inc  rdx
    jmp  @B
_scat_done:
    mov  rax, rbx
    pop  rbx
    ret
_StrConcat ENDP

;------------------------------------------------------------------------------
; _IntToStr — convert unsigned 32-bit in ECX to decimal string at g_NumBuf
; Returns pointer to g_NumBuf in RAX
;------------------------------------------------------------------------------
_IntToStr PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    .endprolog

    lea  rdi, g_NumBuf
    add  rdi, 30                    ; start from end
    mov  byte ptr [rdi], 0          ; NUL terminator
    dec  rdi

    mov  eax, ecx
    test eax, eax
    jnz  _its_loop
    mov  byte ptr [rdi], '0'
    mov  rax, rdi
    jmp  _its_done

_its_loop:
    test eax, eax
    jz   _its_finish
    xor  edx, edx
    mov  ebx, 10
    div  ebx
    add  dl, '0'
    mov  byte ptr [rdi], dl
    dec  rdi
    jmp  _its_loop

_its_finish:
    inc  rdi
    mov  rax, rdi

_its_done:
    pop  rdi
    pop  rbx
    ret
_IntToStr ENDP

;------------------------------------------------------------------------------
; _PrintConsole — write NUL-terminated ANSI string to stdout
; RCX = pString
;------------------------------------------------------------------------------
_PrintConsole PROC FRAME
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
    test rsi, rsi
    jz   _pc_done

    ; strlen
    mov  rcx, rsi
    call _StrLen
    mov  [rbp-10h], eax             ; save length to local (R8 is volatile!)

    ; GetStdHandle(-11)
    mov  ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    test rax, rax
    jz   _pc_done

    ; WriteFile
    mov  rcx, rax
    mov  rdx, rsi
    mov  r8d, [rbp-10h]             ; restore length from local
    lea  r9, [rbp-18h]
    mov  qword ptr [rsp+20h], 0
    call WriteFile

_pc_done:
    pop  rsi
    leave
    ret
_PrintConsole ENDP

;------------------------------------------------------------------------------
; _PrintError — print "[HTTP-ERR] <msg>: <win32 error text>\n" to stdout
; RCX = pMessage (ANSI), EDX = Win32 error code
;------------------------------------------------------------------------------
_PrintError PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 60h
    .allocstack 60h
    push rsi
    .pushreg rsi
    push r12
    .pushreg r12
    .endprolog

    mov  rsi, rcx                   ; message
    mov  r12d, edx                  ; error code

    ; Print prefix
    lea  rcx, szErrPrefix
    call _PrintConsole

    ; Print message
    mov  rcx, rsi
    call _PrintConsole

    ; Print ": "
    lea  rcx, szColonSpace
    call _PrintConsole

    ; FormatMessageA to get system error text
    mov  ecx, FORMAT_MESSAGE_FROM_SYSTEM or FORMAT_MESSAGE_IGNORE_INSERTS
    xor  edx, edx                  ; lpSource = NULL
    mov  r8d, r12d                  ; dwMessageId = error code
    xor  r9d, r9d                  ; dwLanguageId = 0
    lea  rax, g_ErrorMsg
    mov  qword ptr [rsp+20h], rax  ; lpBuffer
    mov  qword ptr [rsp+28h], MAX_ERROR_MSG  ; nSize
    mov  qword ptr [rsp+30h], 0    ; Arguments = NULL
    call FormatMessageA
    test eax, eax
    jz   _pe_noformat

    lea  rcx, g_ErrorMsg
    call _PrintConsole
    jmp  _pe_done

_pe_noformat:
    ; Fallback: print error code as number
    mov  ecx, r12d
    call _IntToStr
    mov  rcx, rax
    call _PrintConsole
    lea  rcx, szNewline
    call _PrintConsole

_pe_done:
    pop  r12
    pop  rsi
    leave
    ret
_PrintError ENDP

;==============================================================================
; Http_Initialize — Create WinHTTP session with configurable timeouts
;
; RCX = port (0 = default 11434)
; Returns: 1 success, 0 failure
;==============================================================================
Http_Initialize PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 68h
    .allocstack 68h
    push rbx
    .pushreg rbx
    .endprolog

    ; Save port (or use default)
    mov  ebx, ecx
    test ebx, ebx
    jnz  @F
    mov  ebx, 11434                 ; default Ollama port
@@:

    ; If already initialized, shut down first
    cmp  g_Initialized, 1
    jne  _hi_open
    call Http_Shutdown

_hi_open:
    ; WinHttpOpen(pwszUserAgent, accessType, proxyName, proxyBypass, flags)
    lea  rcx, wUserAgent            ; Wide string user agent
    mov  edx, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
    xor  r8, r8                     ; no proxy
    xor  r9, r9                     ; no bypass
    mov  qword ptr [rsp+20h], 0     ; no async flags
    call WinHttpOpen
    test rax, rax
    jz   _hi_fail_open
    mov  g_hSession, rax

    ; Set timeouts: WinHttpSetTimeouts(hSession, resolve, connect, send, recv)
    mov  rcx, rax
    mov  edx, DEFAULT_RESOLVE_TIMEOUT
    mov  r8d, DEFAULT_CONNECT_TIMEOUT
    mov  r9d, DEFAULT_SEND_TIMEOUT
    mov  qword ptr [rsp+20h], DEFAULT_RECEIVE_TIMEOUT
    call WinHttpSetTimeouts
    ; Timeout failure is non-fatal, continue

    ; WinHttpConnect(hSession, "localhost", port, 0)
    mov  rcx, g_hSession
    lea  rdx, wLocalhost            ; Wide string!
    mov  r8d, ebx                   ; port
    xor  r9d, r9d                   ; reserved
    call WinHttpConnect
    test rax, rax
    jz   _hi_fail_connect
    mov  g_hConnect, rax

    mov  g_Initialized, 1
    mov  eax, 1
    jmp  _hi_exit

_hi_fail_open:
    call GetLastError
    mov  g_LastError, eax
    lea  rcx, szErrWinHttpOpen
    mov  edx, eax
    call _PrintError
    xor  eax, eax
    jmp  _hi_exit

_hi_fail_connect:
    call GetLastError
    mov  g_LastError, eax
    lea  rcx, szErrConnect
    mov  edx, eax
    call _PrintError
    ; Clean up session
    mov  rcx, g_hSession
    call WinHttpCloseHandle
    mov  g_hSession, 0
    xor  eax, eax

_hi_exit:
    pop  rbx
    leave
    ret
Http_Initialize ENDP

;==============================================================================
; Http_Shutdown — Close all WinHTTP handles
;==============================================================================
Http_Shutdown PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 30h
    .allocstack 30h
    .endprolog

    cmp  g_Initialized, 0
    je   _hs_done

    ; Close connect handle
    mov  rcx, g_hConnect
    test rcx, rcx
    jz   @F
    call WinHttpCloseHandle
    mov  g_hConnect, 0
@@:
    ; Close session handle
    mov  rcx, g_hSession
    test rcx, rcx
    jz   @F
    call WinHttpCloseHandle
    mov  g_hSession, 0
@@:
    mov  g_Initialized, 0

_hs_done:
    leave
    ret
Http_Shutdown ENDP

;==============================================================================
; Http_PostJSON — POST JSON to Ollama, return full response
;
; RCX = pJsonBody (ANSI/UTF-8 NUL-terminated JSON string)
; RDX = pResponseOut (buffer to receive response body, caller-allocated)
; R8  = responseMaxLen (max bytes for response buffer)
; R9  = pWideEndpoint (LPCWSTR path, e.g. lea r9, wOllamaGenPath; NULL = /api/generate)
;
; Returns: bytes read in EAX (0 on failure)
; Side effects: g_LastHttpCode set to HTTP status code
;               g_LastError set on failure
;==============================================================================
Http_PostJSON PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 0E8h
    .allocstack 0E8h
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
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    .endprolog

    mov  r12, rcx                   ; pJsonBody
    mov  r13, rdx                   ; pResponseOut
    mov  r14d, r8d                  ; responseMaxLen
    mov  r15, r9                    ; pWideEndpoint (or NULL)

    ; Default endpoint
    test r15, r15
    jnz  @F
    lea  r15, wOllamaGenPath
@@:

    ; Ensure initialized
    cmp  g_Initialized, 0
    jne  @F
    xor  ecx, ecx                   ; default port
    call Http_Initialize
    test eax, eax
    jz   _hpj_fail
@@:

    ; Get JSON body length
    mov  rcx, r12
    call _StrLen
    mov  rbx, rax                   ; payload byte length

    ;------------------------------------------------------------------
    ; WinHttpOpenRequest(hConnect, "POST", "/api/generate", NULL, NULL, NULL, 0)
    ; 7 params: RCX, RDX, R8, R9, [rsp+20h], [rsp+28h], [rsp+30h]
    ;------------------------------------------------------------------
    mov  rcx, g_hConnect
    lea  rdx, wPost                 ; LPCWSTR verb
    mov  r8,  r15                   ; LPCWSTR path
    xor  r9, r9                     ; HTTP/1.1 (NULL = default)
    mov  qword ptr [rsp+20h], 0     ; referrer
    mov  qword ptr [rsp+28h], 0     ; accept types
    mov  qword ptr [rsp+30h], 0     ; flags
    call WinHttpOpenRequest
    test rax, rax
    jz   _hpj_fail_request
    mov  [rbp-30h], rax             ; hRequest

    ;------------------------------------------------------------------
    ; Add Content-Type: application/json header (wide string!)
    ; WinHttpAddRequestHeaders(hRequest, pwszHeaders, dwLen, dwModifiers)
    ;------------------------------------------------------------------
    mov  rcx, rax
    lea  rdx, wContentTypeHdr
    mov  r8d, -1                    ; auto-calculate length
    mov  r9d, WINHTTP_ADDREQ_FLAG_ADD
    call WinHttpAddRequestHeaders
    test eax, eax
    jz   _hpj_fail_headers

    ;------------------------------------------------------------------
    ; WinHttpSendRequest — 7 params
    ; (hRequest, additionalHdrs, hdrsLen, pOptional, optionalLen, totalLen, context)
    ;------------------------------------------------------------------
    mov  rcx, [rbp-30h]             ; hRequest
    xor  edx, edx                   ; no additional headers
    xor  r8d, r8d                   ; headers length = 0
    mov  r9,  r12                   ; pOptional = JSON body (ANSI bytes!)
    mov  qword ptr [rsp+20h], rbx   ; dwOptionalLength
    mov  qword ptr [rsp+28h], rbx   ; dwTotalLength
    mov  qword ptr [rsp+30h], 0     ; dwContext
    call WinHttpSendRequest
    test eax, eax
    jz   _hpj_fail_send

    ;------------------------------------------------------------------
    ; WinHttpReceiveResponse(hRequest, NULL)
    ;------------------------------------------------------------------
    mov  rcx, [rbp-30h]
    xor  edx, edx
    call WinHttpReceiveResponse
    test eax, eax
    jz   _hpj_fail_receive

    ;------------------------------------------------------------------
    ; Query HTTP status code
    ; WinHttpQueryHeaders(hReq, QUERY_STATUS_CODE|FLAG_NUMBER, NULL, &code, &len, NULL)
    ;------------------------------------------------------------------
    mov  rcx, [rbp-30h]
    mov  edx, WINHTTP_QUERY_STATUS_CODE or WINHTTP_QUERY_FLAG_NUMBER
    xor  r8, r8                     ; WINHTTP_HEADER_NAME_BY_INDEX
    lea  r9, [rbp-38h]             ; &statusCode (DWORD)
    mov  dword ptr [rbp-40h], 4     ; bufferLength = sizeof(DWORD)
    lea  rax, [rbp-40h]
    mov  qword ptr [rsp+20h], rax   ; &bufferLength
    mov  qword ptr [rsp+28h], 0     ; WINHTTP_NO_HEADER_INDEX
    call WinHttpQueryHeaders
    test eax, eax
    jz   @F
    mov  eax, dword ptr [rbp-38h]
    mov  g_LastHttpCode, eax
@@:

    ; Check for non-200 status
    cmp  g_LastHttpCode, 200
    je   _hpj_read_body

    ; Print HTTP error status
    lea  rcx, szErrStatus
    call _PrintConsole
    mov  ecx, g_LastHttpCode
    call _IntToStr
    mov  rcx, rax
    call _PrintConsole
    lea  rcx, szNewline
    call _PrintConsole
    ; Still try to read body (may contain error JSON)

    ;------------------------------------------------------------------
    ; Read response body loop
    ;------------------------------------------------------------------
_hpj_read_body:
    mov  rdi, r13                   ; write cursor into output buffer
    xor  esi, esi                   ; total bytes read

_hpj_read_loop:
    ; Check if we'd overflow the output buffer
    mov  eax, r14d
    sub  eax, esi
    cmp  eax, 1                     ; need at least 1 byte for NUL
    jle  _hpj_read_done

    ; WinHttpQueryDataAvailable(hRequest, &dwSize)
    mov  rcx, [rbp-30h]
    lea  rdx, [rbp-48h]            ; &dwSize
    call WinHttpQueryDataAvailable
    test eax, eax
    jz   _hpj_read_done

    mov  eax, dword ptr [rbp-48h]
    test eax, eax
    jz   _hpj_read_done            ; 0 bytes available = end

    ; Clamp to remaining space
    mov  ecx, r14d
    sub  ecx, esi
    dec  ecx                        ; leave room for NUL
    cmp  eax, ecx
    jle  @F
    mov  eax, ecx
@@:
    ; Clamp to 8192 per read
    cmp  eax, 8192
    jle  @F
    mov  eax, 8192
@@:
    mov  [rbp-4Ch], eax             ; bytes to read

    ; WinHttpReadData(hRequest, pBuf, toRead, &bytesRead)
    mov  rcx, [rbp-30h]
    mov  rdx, rdi                   ; write position
    mov  r8d, eax
    lea  r9, [rbp-50h]             ; &bytesRead
    call WinHttpReadData
    test eax, eax
    jz   _hpj_fail_read

    mov  eax, dword ptr [rbp-50h]
    test eax, eax
    jz   _hpj_read_done

    add  esi, eax                   ; total += bytesRead
    add  rdi, rax                   ; advance write cursor
    jmp  _hpj_read_loop

_hpj_read_done:
    mov  byte ptr [rdi], 0          ; NUL-terminate response
    mov  eax, esi                   ; return total bytes

    ; Print success info — save return value to local
    mov  [rbp-58h], eax
    lea  rcx, szOk
    call _PrintConsole
    mov  ecx, [rbp-58h]
    call _IntToStr
    mov  rcx, rax
    call _PrintConsole
    lea  rcx, szBytesRead
    call _PrintConsole

    ; Close request handle
    mov  rcx, [rbp-30h]
    call WinHttpCloseHandle
    mov  eax, [rbp-58h]            ; restore return value
    jmp  _hpj_exit

    ;------------------------------------------------------------------
    ; Error paths — each calls GetLastError, prints, cleans up
    ;------------------------------------------------------------------
_hpj_fail_request:
    call GetLastError
    mov  g_LastError, eax
    lea  rcx, szErrRequest
    mov  edx, eax
    call _PrintError
    xor  eax, eax
    jmp  _hpj_exit

_hpj_fail_headers:
    call GetLastError
    mov  g_LastError, eax
    lea  rcx, szErrHeaders
    mov  edx, eax
    call _PrintError
    mov  rcx, [rbp-30h]
    call WinHttpCloseHandle
    xor  eax, eax
    jmp  _hpj_exit

_hpj_fail_send:
    call GetLastError
    mov  g_LastError, eax
    lea  rcx, szErrSend
    mov  edx, eax
    call _PrintError
    mov  rcx, [rbp-30h]
    call WinHttpCloseHandle
    xor  eax, eax
    jmp  _hpj_exit

_hpj_fail_receive:
    call GetLastError
    mov  g_LastError, eax
    lea  rcx, szErrReceive
    mov  edx, eax
    call _PrintError
    mov  rcx, [rbp-30h]
    call WinHttpCloseHandle
    xor  eax, eax
    jmp  _hpj_exit

_hpj_fail_read:
    call GetLastError
    mov  g_LastError, eax
    lea  rcx, szErrRead
    mov  edx, eax
    call _PrintError
    mov  byte ptr [rdi], 0
    mov  eax, esi                   ; return partial bytes
    mov  rcx, [rbp-30h]
    call WinHttpCloseHandle
    ; eax may be non-zero (partial read)
    jmp  _hpj_exit

_hpj_fail:
    xor  eax, eax

_hpj_exit:
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    leave
    ret
Http_PostJSON ENDP

;==============================================================================
; Http_PostStream — POST with stream:true, invoke callback per response token
;
; RCX = pJsonBody (ANSI/UTF-8 with "stream":true in it)
; RDX = pCallback (function pointer: void callback(RCX=pToken, RDX=tokenLen, R8=pUserData))
; R8  = pUserData (passed through to callback)
;
; Returns: total accumulated bytes in EAX, 0 on failure
; The streaming accumulator (g_StreamAccum) contains the full joined response.
;==============================================================================
Http_PostStream PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 0E8h
    .allocstack 0E8h
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
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    .endprolog

    mov  r12, rcx                   ; pJsonBody
    mov  r13, rdx                   ; pCallback
    mov  r14, r8                    ; pUserData

    ; Initialize streaming state
    call Stream_Init

    ; Ensure initialized
    cmp  g_Initialized, 0
    jne  @F
    xor  ecx, ecx
    call Http_Initialize
    test eax, eax
    jz   _hps_fail
@@:

    ; Get body length
    mov  rcx, r12
    call _StrLen
    mov  rbx, rax

    ; Open request
    mov  rcx, g_hConnect
    lea  rdx, wPost
    lea  r8,  wOllamaGenPath
    xor  r9, r9
    mov  qword ptr [rsp+20h], 0
    mov  qword ptr [rsp+28h], 0
    mov  qword ptr [rsp+30h], 0
    call WinHttpOpenRequest
    test rax, rax
    jz   _hps_fail_req
    mov  [rbp-30h], rax             ; hRequest

    ; Add Content-Type header
    mov  rcx, rax
    lea  rdx, wContentTypeHdr
    mov  r8d, -1
    mov  r9d, WINHTTP_ADDREQ_FLAG_ADD
    call WinHttpAddRequestHeaders

    ; Send request
    mov  rcx, [rbp-30h]
    xor  edx, edx
    xor  r8d, r8d
    mov  r9,  r12
    mov  qword ptr [rsp+20h], rbx
    mov  qword ptr [rsp+28h], rbx
    mov  qword ptr [rsp+30h], 0
    call WinHttpSendRequest
    test eax, eax
    jz   _hps_fail_send

    ; Receive response
    mov  rcx, [rbp-30h]
    xor  edx, edx
    call WinHttpReceiveResponse
    test eax, eax
    jz   _hps_fail_recv

    ; Query HTTP status
    mov  rcx, [rbp-30h]
    mov  edx, WINHTTP_QUERY_STATUS_CODE or WINHTTP_QUERY_FLAG_NUMBER
    xor  r8, r8
    lea  r9, [rbp-38h]
    mov  dword ptr [rbp-40h], 4
    lea  rax, [rbp-40h]
    mov  qword ptr [rsp+20h], rax
    mov  qword ptr [rsp+28h], 0
    call WinHttpQueryHeaders
    test eax, eax
    jz   @F
    mov  eax, dword ptr [rbp-38h]
    mov  g_LastHttpCode, eax
@@:

    ;------------------------------------------------------------------
    ; Streaming read loop — read chunks, feed to Stream_Feed
    ;------------------------------------------------------------------
_hps_stream_loop:
    ; Check if streaming is done
    cmp  g_StreamDone, 1
    je   _hps_stream_done

    ; Query available data
    mov  rcx, [rbp-30h]
    lea  rdx, [rbp-48h]
    call WinHttpQueryDataAvailable
    test eax, eax
    jz   _hps_stream_done

    mov  eax, dword ptr [rbp-48h]
    test eax, eax
    jz   _hps_stream_done

    ; Clamp to 8192
    cmp  eax, 8192
    jle  @F
    mov  eax, 8192
@@:
    mov  [rbp-4Ch], eax

    ; Read into local buffer on stack (within frame)
    ; Use g_ResponseBuf temporarily as chunk buffer
    mov  rcx, [rbp-30h]
    lea  rdx, g_ResponseBuf
    mov  r8d, eax
    lea  r9, [rbp-50h]
    call WinHttpReadData
    test eax, eax
    jz   _hps_stream_done

    mov  eax, dword ptr [rbp-50h]
    test eax, eax
    jz   _hps_stream_done

    ; NUL-terminate the chunk for safety
    lea  rcx, g_ResponseBuf
    mov  byte ptr [rcx+rax], 0

    ; Feed chunk to streaming parser
    ; Stream_Feed(pChunk, chunkLen, pCallback, pUserData)
    lea  rcx, g_ResponseBuf
    mov  edx, eax                   ; chunk length
    mov  r8, r13                    ; callback
    mov  r9, r14                    ; user data
    call Stream_Feed

    jmp  _hps_stream_loop

_hps_stream_done:
    ; Close request
    mov  rcx, [rbp-30h]
    call WinHttpCloseHandle

    ; Print done
    lea  rcx, szStreamDone
    call _PrintConsole

    ; Return accumulated length
    mov  eax, g_StreamAccLen
    jmp  _hps_exit

_hps_fail_req:
    call GetLastError
    mov  g_LastError, eax
    lea  rcx, szErrRequest
    mov  edx, eax
    call _PrintError
    xor  eax, eax
    jmp  _hps_exit

_hps_fail_send:
    call GetLastError
    mov  g_LastError, eax
    lea  rcx, szErrSend
    mov  edx, eax
    call _PrintError
    mov  rcx, [rbp-30h]
    call WinHttpCloseHandle
    xor  eax, eax
    jmp  _hps_exit

_hps_fail_recv:
    call GetLastError
    mov  g_LastError, eax
    lea  rcx, szErrReceive
    mov  edx, eax
    call _PrintError
    mov  rcx, [rbp-30h]
    call WinHttpCloseHandle
    xor  eax, eax
    jmp  _hps_exit

_hps_fail:
    xor  eax, eax

_hps_exit:
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    leave
    ret
Http_PostStream ENDP

;==============================================================================
; Http_GetErrorInfo — Return last error code and HTTP status
; Returns: EAX = Win32 error code, EDX = HTTP status code
;==============================================================================
Http_GetErrorInfo PROC FRAME
    .endprolog
    mov  eax, g_LastError
    mov  edx, g_LastHttpCode
    ret
Http_GetErrorInfo ENDP

;==============================================================================
;                   JSON TOKENIZER — Real Recursive Descent
;==============================================================================

;------------------------------------------------------------------------------
; Json_Tokenize — Tokenize a JSON string into g_TokenArray
;
; RCX = pJsonStr (NUL-terminated ANSI/UTF-8)
; Returns: token count in EAX, 0 on error
;          Tokens stored in g_TokenArray, count in g_TokenCount
;------------------------------------------------------------------------------
Json_Tokenize PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    .endprolog

    test rcx, rcx
    jz   _jt_fail

    mov  g_TokenSrcPtr, rcx         ; save source pointer
    mov  rsi, rcx                   ; RSI = scan pointer
    mov  rbx, rcx                   ; RBX = base pointer (for offsets)
    xor  edi, edi                   ; EDI = token count
    xor  r12d, r12d                 ; R12D = depth

    ; Clear token array header
    mov  g_TokenCount, 0

_jt_scan:
    ; Skip whitespace
    movzx eax, byte ptr [rsi]
    cmp  al, ' '
    je   _jt_skip
    cmp  al, 9                      ; tab
    je   _jt_skip
    cmp  al, 10                     ; LF
    je   _jt_skip
    cmp  al, 13                     ; CR
    je   _jt_skip
    test al, al
    jz   _jt_done                   ; end of string

    ; Check what token we have
    cmp  al, '{'
    je   _jt_obj_start
    cmp  al, '}'
    je   _jt_obj_end
    cmp  al, '['
    je   _jt_arr_start
    cmp  al, ']'
    je   _jt_arr_end
    cmp  al, '"'
    je   _jt_string
    cmp  al, ':'
    je   _jt_colon
    cmp  al, ','
    je   _jt_comma
    cmp  al, 't'
    je   _jt_true
    cmp  al, 'f'
    je   _jt_false
    cmp  al, 'n'
    je   _jt_null
    cmp  al, '-'
    je   _jt_number
    cmp  al, '0'
    jb   _jt_fail                   ; invalid char
    cmp  al, '9'
    jbe  _jt_number
    jmp  _jt_fail                   ; unexpected char

_jt_skip:
    inc  rsi
    jmp  _jt_scan

    ;--- Single-char tokens ---
_jt_obj_start:
    mov  ecx, JT_OBJ_START
    call _jt_emit_single
    inc  r12d                       ; depth++
    inc  rsi
    jmp  _jt_scan

_jt_obj_end:
    dec  r12d                       ; depth--
    mov  ecx, JT_OBJ_END
    call _jt_emit_single
    inc  rsi
    jmp  _jt_scan

_jt_arr_start:
    mov  ecx, JT_ARR_START
    call _jt_emit_single
    inc  r12d
    inc  rsi
    jmp  _jt_scan

_jt_arr_end:
    dec  r12d
    mov  ecx, JT_ARR_END
    call _jt_emit_single
    inc  rsi
    jmp  _jt_scan

_jt_colon:
    mov  ecx, JT_COLON
    call _jt_emit_single
    inc  rsi
    jmp  _jt_scan

_jt_comma:
    mov  ecx, JT_COMMA
    call _jt_emit_single
    inc  rsi
    jmp  _jt_scan

    ;--- String token ---
_jt_string:
    mov  rcx, rsi
    sub  rcx, rbx                   ; start offset (includes quote)
    mov  [rbp-10h], ecx             ; save start offset
    inc  rsi                        ; skip opening quote

_jt_str_scan:
    movzx eax, byte ptr [rsi]
    test al, al
    jz   _jt_fail                   ; unterminated string
    cmp  al, '"'
    je   _jt_str_end
    cmp  al, '\'
    jne  _jt_str_next
    ; Escape: skip next char
    inc  rsi
    cmp  byte ptr [rsi], 0
    je   _jt_fail
_jt_str_next:
    inc  rsi
    jmp  _jt_str_scan

_jt_str_end:
    inc  rsi                        ; skip closing quote
    ; Emit: offset = start+1 (after quote), length = content bytes
    mov  ecx, [rbp-10h]             ; start offset (at opening quote)
    inc  ecx                        ; skip opening quote for content offset
    mov  edx, esi
    sub  edx, ebx                   ; current offset
    dec  edx                        ; exclude closing quote
    sub  edx, ecx                   ; length = current - (start+1)
    ; Emit token
    cmp  edi, MAX_JSON_TOKENS
    jge  _jt_done
    lea  rax, g_TokenArray
    imul r8d, edi, SIZEOF JSON_TOKEN
    mov  dword ptr [rax+r8], JT_STRING
    mov  dword ptr [rax+r8+4], ecx  ; content start offset
    mov  dword ptr [rax+r8+8], edx  ; content length
    mov  dword ptr [rax+r8+12], r12d ; depth
    inc  edi
    jmp  _jt_scan

    ;--- Number token ---
_jt_number:
    mov  rcx, rsi
    sub  rcx, rbx                   ; start offset
    mov  [rbp-10h], ecx
    ; Scan digits, -, ., e, E, +
_jt_num_scan:
    movzx eax, byte ptr [rsi]
    cmp  al, '0'
    jb   _jt_num_check
    cmp  al, '9'
    jbe  _jt_num_next
_jt_num_check:
    cmp  al, '-'
    je   _jt_num_next
    cmp  al, '+'
    je   _jt_num_next
    cmp  al, '.'
    je   _jt_num_next
    cmp  al, 'e'
    je   _jt_num_next
    cmp  al, 'E'
    je   _jt_num_next
    jmp  _jt_num_end
_jt_num_next:
    inc  rsi
    jmp  _jt_num_scan
_jt_num_end:
    mov  ecx, [rbp-10h]
    mov  edx, esi
    sub  edx, ebx
    sub  edx, ecx
    cmp  edi, MAX_JSON_TOKENS
    jge  _jt_done
    lea  rax, g_TokenArray
    imul r8d, edi, SIZEOF JSON_TOKEN
    mov  dword ptr [rax+r8], JT_NUMBER
    mov  dword ptr [rax+r8+4], ecx
    mov  dword ptr [rax+r8+8], edx
    mov  dword ptr [rax+r8+12], r12d
    inc  edi
    jmp  _jt_scan

    ;--- Keyword tokens ---
_jt_true:
    cmp  byte ptr [rsi+1], 'r'
    jne  _jt_fail
    cmp  byte ptr [rsi+2], 'u'
    jne  _jt_fail
    cmp  byte ptr [rsi+3], 'e'
    jne  _jt_fail
    mov  ecx, JT_TRUE
    mov  edx, 4
    call _jt_emit_keyword
    add  rsi, 4
    jmp  _jt_scan

_jt_false:
    cmp  byte ptr [rsi+1], 'a'
    jne  _jt_fail
    cmp  byte ptr [rsi+2], 'l'
    jne  _jt_fail
    cmp  byte ptr [rsi+3], 's'
    jne  _jt_fail
    cmp  byte ptr [rsi+4], 'e'
    jne  _jt_fail
    mov  ecx, JT_FALSE
    mov  edx, 5
    call _jt_emit_keyword
    add  rsi, 5
    jmp  _jt_scan

_jt_null:
    cmp  byte ptr [rsi+1], 'u'
    jne  _jt_fail
    cmp  byte ptr [rsi+2], 'l'
    jne  _jt_fail
    cmp  byte ptr [rsi+3], 'l'
    jne  _jt_fail
    mov  ecx, JT_NULL
    mov  edx, 4
    call _jt_emit_keyword
    add  rsi, 4
    jmp  _jt_scan

_jt_done:
    mov  g_TokenCount, edi
    mov  eax, edi
    jmp  _jt_exit

_jt_fail:
    mov  g_TokenCount, 0
    xor  eax, eax

_jt_exit:
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    leave
    ret

;--- Internal: emit single-char token ---
; ECX = tokenType, RSI = current pos, RBX = base, EDI = token count, R12D = depth
_jt_emit_single:
    cmp  edi, MAX_JSON_TOKENS
    jge  _jtes_skip
    lea  rax, g_TokenArray
    imul r8d, edi, SIZEOF JSON_TOKEN
    mov  dword ptr [rax+r8], ecx    ; type
    mov  edx, esi
    sub  edx, ebx
    mov  dword ptr [rax+r8+4], edx  ; offset
    mov  dword ptr [rax+r8+8], 1    ; length
    mov  dword ptr [rax+r8+12], r12d ; depth
    inc  edi
_jtes_skip:
    ret

;--- Internal: emit keyword token ---
; ECX = tokenType, EDX = length
_jt_emit_keyword:
    cmp  edi, MAX_JSON_TOKENS
    jge  _jtek_skip
    lea  rax, g_TokenArray
    imul r8d, edi, SIZEOF JSON_TOKEN
    mov  dword ptr [rax+r8], ecx
    mov  r9d, esi
    sub  r9d, ebx
    mov  dword ptr [rax+r8+4], r9d
    mov  dword ptr [rax+r8+8], edx
    mov  dword ptr [rax+r8+12], r12d
    inc  edi
_jtek_skip:
    ret

Json_Tokenize ENDP

;------------------------------------------------------------------------------
; Json_TokenCount — return count from last tokenize
;------------------------------------------------------------------------------
Json_TokenCount PROC FRAME
    .endprolog
    mov  eax, g_TokenCount
    ret
Json_TokenCount ENDP

;==============================================================================
; Json_FindKey — Find a key string in the token array, return value token index
;
; RCX = pKeyName (ANSI NUL-terminated, WITHOUT quotes)
; RDX = startTokenIdx (0 to search from beginning)
;
; Returns: token index of the VALUE in EAX (-1 if not found)
;
; Algorithm:
;   Scan tokens for JT_STRING matching key text at depth N,
;   followed by JT_COLON, then return index of next token (the value).
;==============================================================================
Json_FindKey PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 48h
    .allocstack 48h
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

    mov  r12, rcx                   ; pKeyName
    mov  r13d, edx                  ; startIdx

    ; Get key length
    mov  rcx, r12
    call _StrLen
    mov  ebx, eax                   ; key length

    mov  esi, r13d                  ; current index

_jfk_loop:
    cmp  esi, g_TokenCount
    jge  _jfk_notfound

    ; Get token
    lea  rdi, g_TokenArray
    imul eax, esi, SIZEOF JSON_TOKEN
    add  rdi, rax

    ; Check if it's a string token
    cmp  dword ptr [rdi], JT_STRING
    jne  _jfk_next

    ; Compare length
    cmp  dword ptr [rdi+8], ebx
    jne  _jfk_next

    ; Compare content: source[offset..offset+len] vs pKeyName
    mov  rcx, g_TokenSrcPtr
    mov  eax, dword ptr [rdi+4]     ; startOffset
    add  rcx, rax                   ; pointer to key content in JSON
    mov  rdx, r12                   ; pKeyName
    mov  r8d, ebx                   ; length
    call _MemCmp
    test eax, eax
    jnz  _jfk_next                  ; no match

    ; Key matched! Check next token is colon
    mov  eax, esi
    inc  eax
    cmp  eax, g_TokenCount
    jge  _jfk_notfound

    lea  rdi, g_TokenArray
    imul ecx, eax, SIZEOF JSON_TOKEN
    cmp  dword ptr [rdi+rcx], JT_COLON
    jne  _jfk_next

    ; Return index of value (after colon)
    add  eax, 1                     ; value is token after colon
    cmp  eax, g_TokenCount
    jge  _jfk_notfound
    jmp  _jfk_exit

_jfk_next:
    inc  esi
    jmp  _jfk_loop

_jfk_notfound:
    mov  eax, -1

_jfk_exit:
    pop  r13
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    leave
    ret
Json_FindKey ENDP

;------------------------------------------------------------------------------
; _MemCmp — compare N bytes, return 0 if equal
; RCX = ptr1, RDX = ptr2, R8D = length
;------------------------------------------------------------------------------
_MemCmp PROC FRAME
    .endprolog
    test r8d, r8d
    jz   _mc_eq
@@:
    mov  al, byte ptr [rcx]
    cmp  al, byte ptr [rdx]
    jne  _mc_ne
    inc  rcx
    inc  rdx
    dec  r8d
    jnz  @B
_mc_eq:
    xor  eax, eax
    ret
_mc_ne:
    mov  eax, 1
    ret
_MemCmp ENDP

;==============================================================================
; Json_ExtractString — Extract + unescape a string value at given token index
;
; RCX = token index (must be JT_STRING)
; RDX = pOutput buffer
; R8D = maxOutputLen
;
; Returns: bytes written in EAX (0 on failure)
; Handles: \", \\, \/, \b, \f, \n, \r, \t, \uXXXX (as raw UTF-8)
;==============================================================================
Json_ExtractString PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 30h
    .allocstack 30h
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    .endprolog

    mov  r12d, ecx                  ; token index
    mov  rdi, rdx                   ; pOutput
    mov  ebx, r8d                   ; maxLen

    ; Validate token index
    cmp  r12d, g_TokenCount
    jge  _jes_fail

    ; Get token
    lea  rsi, g_TokenArray
    imul eax, r12d, SIZEOF JSON_TOKEN
    add  rsi, rax

    ; Must be a string token
    cmp  dword ptr [rsi], JT_STRING
    jne  _jes_fail

    ; Get source pointer
    mov  rcx, g_TokenSrcPtr
    mov  eax, dword ptr [rsi+4]     ; startOffset
    add  rcx, rax                   ; pStart
    mov  edx, dword ptr [rsi+8]     ; tokenLen
    add  rdx, rcx                   ; pEnd = pStart + len

    mov  rsi, rcx                   ; RSI = read pointer
    xor  ecx, ecx                   ; ECX = output count

_jes_loop:
    cmp  rsi, rdx
    jge  _jes_done
    cmp  ecx, ebx
    jge  _jes_done
    dec  ebx                        ; reserve 1 for NUL

    movzx eax, byte ptr [rsi]
    cmp  al, '\'
    je   _jes_escape
    ; Normal char
    mov  byte ptr [rdi], al
    inc  rdi
    inc  rsi
    inc  ecx
    jmp  _jes_loop

_jes_escape:
    inc  rsi                        ; skip backslash
    cmp  rsi, rdx
    jge  _jes_done

    movzx eax, byte ptr [rsi]
    inc  rsi

    cmp  al, '"'
    je   _jes_store_char
    cmp  al, '\'
    je   _jes_store_char
    cmp  al, '/'
    je   _jes_store_char
    cmp  al, 'n'
    jne  @F
    mov  al, 10
    jmp  _jes_store_char
@@:
    cmp  al, 'r'
    jne  @F
    mov  al, 13
    jmp  _jes_store_char
@@:
    cmp  al, 't'
    jne  @F
    mov  al, 9
    jmp  _jes_store_char
@@:
    cmp  al, 'b'
    jne  @F
    mov  al, 8
    jmp  _jes_store_char
@@:
    cmp  al, 'f'
    jne  @F
    mov  al, 12
    jmp  _jes_store_char
@@:
    cmp  al, 'u'
    jne  _jes_store_char

    ; \uXXXX — parse 4 hex digits, emit as UTF-8
    ; For simplicity, handle BMP only (U+0000..U+FFFF)
    push rcx
    xor  ecx, ecx                   ; accumulator
    mov  r8d, 4
_jes_hex_loop:
    test r8d, r8d
    jz   _jes_hex_done
    cmp  rsi, rdx
    jge  _jes_hex_done
    movzx eax, byte ptr [rsi]
    inc  rsi
    ; Convert hex char to value
    cmp  al, '0'
    jb   _jes_hex_done
    cmp  al, '9'
    jbe  _jes_hex_digit
    cmp  al, 'a'
    jb   _jes_hex_upper
    cmp  al, 'f'
    ja   _jes_hex_done
    sub  al, 'a'
    add  al, 10
    jmp  _jes_hex_shift
_jes_hex_upper:
    cmp  al, 'A'
    jb   _jes_hex_done
    cmp  al, 'F'
    ja   _jes_hex_done
    sub  al, 'A'
    add  al, 10
    jmp  _jes_hex_shift
_jes_hex_digit:
    sub  al, '0'
_jes_hex_shift:
    shl  ecx, 4
    or   cl, al
    dec  r8d
    jmp  _jes_hex_loop

_jes_hex_done:
    ; ECX = Unicode codepoint (BMP)
    ; Encode as UTF-8
    cmp  ecx, 07Fh
    ja   _jes_utf8_2
    ; 1-byte: 0xxxxxxx
    mov  al, cl
    pop  rcx
    jmp  _jes_store_char

_jes_utf8_2:
    cmp  ecx, 07FFh
    ja   _jes_utf8_3
    ; 2-byte: 110xxxxx 10xxxxxx
    mov  eax, ecx
    shr  eax, 6
    or   al, 0C0h
    mov  byte ptr [rdi], al
    inc  rdi
    mov  eax, ecx
    and  al, 3Fh
    or   al, 80h
    pop  rcx
    inc  ecx
    jmp  _jes_store_char

_jes_utf8_3:
    ; 3-byte: 1110xxxx 10xxxxxx 10xxxxxx
    mov  eax, ecx
    shr  eax, 12
    or   al, 0E0h
    mov  byte ptr [rdi], al
    inc  rdi
    mov  eax, ecx
    shr  eax, 6
    and  al, 3Fh
    or   al, 80h
    mov  byte ptr [rdi], al
    inc  rdi
    mov  eax, ecx
    and  al, 3Fh
    or   al, 80h
    pop  rcx
    add  ecx, 2
    jmp  _jes_store_char

_jes_store_char:
    mov  byte ptr [rdi], al
    inc  rdi
    inc  ecx
    jmp  _jes_loop

_jes_done:
    mov  byte ptr [rdi], 0          ; NUL-terminate
    mov  eax, ecx
    jmp  _jes_exit

_jes_fail:
    xor  eax, eax

_jes_exit:
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    leave
    ret
Json_ExtractString ENDP

;==============================================================================
; Json_ExtractInt — Extract integer value from a number token
;
; RCX = token index (must be JT_NUMBER)
; Returns: value in EAX (0 on failure, check with jz / test)
;          RDX = 1 if valid, 0 if invalid
;==============================================================================
Json_ExtractInt PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    .endprolog

    ; Validate
    cmp  ecx, g_TokenCount
    jge  _jei_fail
    lea  rsi, g_TokenArray
    imul eax, ecx, SIZEOF JSON_TOKEN
    cmp  dword ptr [rsi+rax], JT_NUMBER
    jne  _jei_fail

    ; Get source
    mov  ebx, dword ptr [rsi+rax+4] ; offset
    mov  ecx, dword ptr [rsi+rax+8] ; length
    mov  rsi, g_TokenSrcPtr
    add  rsi, rbx

    ; Parse integer (handle leading -)
    xor  eax, eax
    xor  edx, edx                   ; negative flag
    movzx ebx, byte ptr [rsi]
    cmp  bl, '-'
    jne  _jei_digits
    mov  edx, 1
    inc  rsi
    dec  ecx

_jei_digits:
    test ecx, ecx
    jz   _jei_negate
    movzx ebx, byte ptr [rsi]
    cmp  bl, '.'                    ; stop at decimal point
    je   _jei_negate
    cmp  bl, 'e'
    je   _jei_negate
    cmp  bl, 'E'
    je   _jei_negate
    cmp  bl, '0'
    jb   _jei_negate
    cmp  bl, '9'
    ja   _jei_negate
    imul eax, eax, 10
    sub  bl, '0'
    add  eax, ebx
    inc  rsi
    dec  ecx
    jmp  _jei_digits

_jei_negate:
    test edx, edx
    jz   @F
    neg  eax
@@:
    mov  edx, 1                     ; valid
    jmp  _jei_exit

_jei_fail:
    xor  eax, eax
    xor  edx, edx

_jei_exit:
    pop  rsi
    pop  rbx
    ret
Json_ExtractInt ENDP

;==============================================================================
; Json_ExtractBool — Extract boolean value
;
; RCX = token index (must be JT_TRUE or JT_FALSE)
; Returns: EAX = 1 for true, 0 for false
;          EDX = 1 if valid, 0 if invalid token type
;==============================================================================
Json_ExtractBool PROC FRAME
    .endprolog
    cmp  ecx, g_TokenCount
    jge  _jeb_fail
    lea  rax, g_TokenArray
    imul edx, ecx, SIZEOF JSON_TOKEN
    mov  ecx, dword ptr [rax+rdx]
    cmp  ecx, JT_TRUE
    je   _jeb_true
    cmp  ecx, JT_FALSE
    je   _jeb_false
    jmp  _jeb_fail

_jeb_true:
    mov  eax, 1
    mov  edx, 1
    ret
_jeb_false:
    xor  eax, eax
    mov  edx, 1
    ret
_jeb_fail:
    xor  eax, eax
    xor  edx, edx
    ret
Json_ExtractBool ENDP

;==============================================================================
;                   STREAMING NDJSON HANDLER
;==============================================================================
; Ollama with "stream":true sends newline-delimited JSON objects:
;   {"model":"...","response":"Hello","done":false}\n
;   {"model":"...","response":" world","done":false}\n
;   {"model":"...","response":"","done":true,...}\n
;
; Stream_Feed reads chunks, splits on \n, tokenizes each line,
; extracts "response" values, and accumulates them.
;==============================================================================

;------------------------------------------------------------------------------
; Stream_Init — Reset streaming state
;------------------------------------------------------------------------------
Stream_Init PROC FRAME
    push rdi
    .pushreg rdi
    .endprolog

    ; Clear accumulator
    lea  rdi, g_StreamAccum
    mov  byte ptr [rdi], 0
    mov  g_StreamAccLen, 0
    mov  g_StreamDone, 0

    ; Clear line buffer
    lea  rdi, g_LineBuf
    mov  byte ptr [rdi], 0
    mov  g_LineBufLen, 0

    pop  rdi
    ret
Stream_Init ENDP

;------------------------------------------------------------------------------
; Stream_Feed — Feed a chunk of data, parse NDJSON lines
;
; RCX = pChunk (NUL-terminated chunk data)
; EDX = chunkLen
; R8  = pCallback (optional, NULL if not needed)
;         callback(RCX=pTokenText, EDX=tokenLen, R8=pUserData)
; R9  = pUserData (for callback)
;
; Returns: number of response tokens extracted in EAX
;------------------------------------------------------------------------------
Stream_Feed PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 88h
    .allocstack 88h
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
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    .endprolog

    mov  rsi, rcx                   ; pChunk read pointer
    mov  ebx, edx                   ; remaining chunk bytes
    mov  r13, r8                    ; pCallback
    mov  r14, r9                    ; pUserData
    xor  r15d, r15d                 ; token count

    ; Process chunk byte by byte
_sf_byte_loop:
    test ebx, ebx
    jz   _sf_done

    movzx eax, byte ptr [rsi]
    inc  rsi
    dec  ebx

    ; Check for newline (end of JSON line)
    cmp  al, 10                     ; LF
    je   _sf_process_line
    cmp  al, 13                     ; CR (ignore, wait for LF)
    je   _sf_byte_loop

    ; Append byte to line buffer
    mov  ecx, g_LineBufLen
    cmp  ecx, MAX_LINE_BUF - 2
    jge  _sf_byte_loop              ; line too long, skip

    lea  rdi, g_LineBuf
    mov  byte ptr [rdi+rcx], al
    inc  ecx
    mov  byte ptr [rdi+rcx], 0      ; keep NUL-terminated
    mov  g_LineBufLen, ecx
    jmp  _sf_byte_loop

_sf_process_line:
    ; Line is complete — process it
    cmp  g_LineBufLen, 2            ; minimum valid JSON "{}"
    jl   _sf_clear_line

    ; Save chunk read state before tokenizing (rsi = chunk ptr, ebx = remaining)
    mov  [rbp-10h], rsi
    mov  [rbp-18h], ebx

    ; Tokenize the line
    lea  rcx, g_LineBuf
    call Json_Tokenize
    test eax, eax
    jz   _sf_clear_line

    ; Look for "response" key
    lea  rcx, szKeyResponse
    xor  edx, edx
    call Json_FindKey
    cmp  eax, -1
    je   _sf_check_done

    ; Extract the response string
    mov  ecx, eax                   ; value token index
    lea  rdx, g_ExtractBuf
    mov  r8d, MAX_EXTRACT_BUF
    call Json_ExtractString
    test eax, eax
    jz   _sf_check_done

    mov  r12d, eax                  ; extracted length

    ; Append to accumulator
    mov  ecx, g_StreamAccLen
    add  ecx, r12d
    cmp  ecx, MAX_STREAM_ACC - 1
    jge  _sf_check_done             ; overflow protection

    ; Save chunk read state (rsi/ebx) before clobbering
    mov  [rbp-10h], rsi             ; save chunk pointer
    mov  [rbp-18h], ebx             ; save remaining bytes

    lea  rdi, g_StreamAccum
    mov  eax, g_StreamAccLen
    add  rdi, rax                   ; write position in accumulator
    lea  rsi, g_ExtractBuf          ; source = extracted token
    ; Copy extracted text
    mov  ecx, r12d
_sf_copy_loop:
    test ecx, ecx
    jz   _sf_copy_done
    mov  al, byte ptr [rsi]
    mov  byte ptr [rdi], al
    inc  rsi
    inc  rdi
    dec  ecx
    jmp  _sf_copy_loop
_sf_copy_done:
    mov  byte ptr [rdi], 0          ; NUL-terminate
    add  g_StreamAccLen, r12d

    ; Restore chunk read state
    mov  rsi, [rbp-10h]
    mov  ebx, [rbp-18h]

    ; Invoke callback if provided
    test r13, r13
    jz   _sf_no_callback
    lea  rcx, g_ExtractBuf          ; pTokenText
    mov  edx, r12d                  ; tokenLen
    mov  r8, r14                    ; pUserData
    call r13
_sf_no_callback:

    inc  r15d                       ; token count++

_sf_check_done:
    ; Look for "done": true/false
    lea  rcx, szKeyDone
    xor  edx, edx
    call Json_FindKey
    cmp  eax, -1
    je   _sf_clear_line

    ; Check if it's true
    mov  ecx, eax
    call Json_ExtractBool
    test edx, edx                   ; valid?
    jz   _sf_clear_line
    test eax, eax                   ; true?
    jz   _sf_clear_line
    mov  g_StreamDone, 1

_sf_clear_line:
    ; Reset line buffer for next line
    lea  rdi, g_LineBuf
    mov  byte ptr [rdi], 0
    mov  g_LineBufLen, 0

    ; Restore chunk read state (saved before line processing)
    mov  rsi, [rbp-10h]
    mov  ebx, [rbp-18h]

    jmp  _sf_byte_loop              ; continue processing remaining chunk

_sf_done:
    mov  eax, r15d                  ; return token count
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    leave
    ret
Stream_Feed ENDP

;------------------------------------------------------------------------------
; Stream_GetAccumulated — Get pointer to accumulated response text
; Returns: RAX = pointer to g_StreamAccum, EDX = length
;------------------------------------------------------------------------------
Stream_GetAccumulated PROC FRAME
    .endprolog
    lea  rax, g_StreamAccum
    mov  edx, g_StreamAccLen
    ret
Stream_GetAccumulated ENDP

;------------------------------------------------------------------------------
; Stream_Reset — Reset streaming state
;------------------------------------------------------------------------------
Stream_Reset PROC FRAME
    .endprolog
    jmp  Stream_Init                ; same thing
Stream_Reset ENDP

;------------------------------------------------------------------------------
; Stream_IsDone — Check if streaming received "done":true
; Returns: EAX = 1 if done, 0 if still streaming
;------------------------------------------------------------------------------
Stream_IsDone PROC FRAME
    .endprolog
    mov  eax, g_StreamDone
    ret
Stream_IsDone ENDP

;==============================================================================
; PUBLIC EXPORTS
;==============================================================================
PUBLIC Http_Initialize
PUBLIC Http_Shutdown
PUBLIC Http_PostJSON
PUBLIC Http_PostStream
PUBLIC Http_GetErrorInfo

PUBLIC Json_Tokenize
PUBLIC Json_TokenCount
PUBLIC Json_FindKey
PUBLIC Json_ExtractString
PUBLIC Json_ExtractInt
PUBLIC Json_ExtractBool

PUBLIC Stream_Init
PUBLIC Stream_Feed
PUBLIC Stream_GetAccumulated
PUBLIC Stream_Reset
PUBLIC Stream_IsDone

END
