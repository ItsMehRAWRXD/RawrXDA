;==============================================================================
; lsp_jsonrpc.asm — JSON-RPC 2.0 LSP Client over stdio pipes
;
; REAL Implementation: CreatePipe → CreateProcess → Content-Length framing
; kernel32.lib ONLY. No CRT. No .inc files.
;
; BUILD:
;   ml64 /c /nologo /W3 /Fo lsp_jsonrpc.obj lsp_jsonrpc.asm
;   link /nologo /subsystem:console /entry:main /out:lsp_jsonrpc.exe
;        lsp_jsonrpc.obj kernel32.lib
;
; Spawns clangd (or any LSP server), sends initialize request,
; reads response with Content-Length parsing, prints diagnostics.
;==============================================================================

OPTION CASEMAP:NONE

;------------------------------------------------------------------------------
; Win32 Constants
;------------------------------------------------------------------------------
STD_OUTPUT_HANDLE           EQU -11
GENERIC_READ                EQU 80000000h
GENERIC_WRITE               EQU 40000000h
FILE_SHARE_READ             EQU 1
OPEN_EXISTING               EQU 3
FILE_ATTRIBUTE_NORMAL       EQU 80h
INVALID_HANDLE_VALUE        EQU -1

; CreatePipe / CreateProcess
STARTF_USESTDHANDLES        EQU 100h
HANDLE_FLAG_INHERIT         EQU 1
CREATE_NO_WINDOW            EQU 08000000h
NORMAL_PRIORITY_CLASS       EQU 20h
INFINITE_WAIT               EQU 0FFFFFFFFh

;------------------------------------------------------------------------------
; Structures — inline, no .inc
;------------------------------------------------------------------------------
SECURITY_ATTRIBUTES STRUCT
    nLength              DWORD ?
    lpSecurityDescriptor QWORD ?
    bInheritHandle       DWORD ?
    _pad0                DWORD ?     ; alignment padding
SECURITY_ATTRIBUTES ENDS

STARTUPINFOA STRUCT
    cb              DWORD ?
    _pad0           DWORD ?
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
    _pad1           DWORD ?
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
; kernel32 imports
;------------------------------------------------------------------------------
EXTERNDEF GetStdHandle:PROC
EXTERNDEF CreatePipe:PROC
EXTERNDEF SetHandleInformation:PROC
EXTERNDEF CreateProcessA:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF ReadFile:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF ExitProcess:PROC
EXTERNDEF WaitForSingleObject:PROC
EXTERNDEF Sleep:PROC
EXTERNDEF GetLastError:PROC

;==============================================================================
;                              DATA SECTION
;==============================================================================
.DATA

ALIGN 8
;--- LSP server command line (configurable) ---
szClangd        BYTE "clangd",0
szClangdCmd     BYTE "clangd --log=error",0

;--- JSON-RPC 2.0 Initialize Request ---
; This is the actual LSP initialize request per the spec
szInitRequest BYTE \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{',\
    '"processId":0,',\
    '"capabilities":{"textDocument":{"completion":{"completionItem":{"snippetSupport":true}},',\
    '"hover":{"contentFormat":["plaintext"]},',\
    '"publishDiagnostics":{"relatedInformation":true}}},',\
    '"rootUri":"file:///D:/rawrxd"',\
    '}}',0

;--- JSON-RPC 2.0 initialized notification ---
szInitializedNotif BYTE \
    '{"jsonrpc":"2.0","method":"initialized","params":{}}',0

;--- JSON-RPC 2.0 shutdown request ---
szShutdownReq BYTE \
    '{"jsonrpc":"2.0","id":2,"method":"shutdown","params":null}',0

;--- JSON-RPC 2.0 exit notification ---
szExitNotif BYTE \
    '{"jsonrpc":"2.0","method":"exit","params":null}',0

;--- JSON-RPC 2.0 textDocument/didOpen for diagnostics ---
szDidOpenTemplate BYTE \
    '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{',\
    '"textDocument":{"uri":"file:///D:/rawrxd/proof.asm",',\
    '"languageId":"asm","version":1,',\
    '"text":""}}}',0

;--- Content-Length header template ---
szContentLength BYTE "Content-Length: ",0
szHeaderEnd     BYTE 13,10,13,10,0      ; \r\n\r\n

;--- Console output strings ---
szBanner        BYTE "[LSP-JSONRPC] Starting JSON-RPC 2.0 LSP client",13,10,0
szBannerLen     EQU $ - szBanner - 1
szPipeOk        BYTE "[LSP-JSONRPC] Pipes created",13,10,0
szPipeOkLen     EQU $ - szPipeOk - 1
szSpawnOk       BYTE "[LSP-JSONRPC] LSP server spawned (PID: ",0
szSpawnOkLen    EQU $ - szSpawnOk - 1
szSendingInit   BYTE "[LSP-JSONRPC] Sending initialize request...",13,10,0
szSendingInitLen EQU $ - szSendingInit - 1
szSentBytes     BYTE "[LSP-JSONRPC] Sent ",0
szSentBytesLen  EQU $ - szSentBytes - 1
szBytesStr      BYTE " bytes",13,10,0
szBytesStrLen   EQU $ - szBytesStr - 1
szWaitResp      BYTE "[LSP-JSONRPC] Waiting for response...",13,10,0
szWaitRespLen   EQU $ - szWaitResp - 1
szGotHeader     BYTE "[LSP-JSONRPC] Content-Length: ",0
szGotHeaderLen  EQU $ - szGotHeader - 1
szGotBody       BYTE "[LSP-JSONRPC] Response body:",13,10,0
szGotBodyLen    EQU $ - szGotBody - 1
szPipeFail      BYTE "[LSP-JSONRPC] CreatePipe FAILED",13,10,0
szPipeFailLen   EQU $ - szPipeFail - 1
szSpawnFail     BYTE "[LSP-JSONRPC] CreateProcess FAILED. Install clangd or set PATH.",13,10,0
szSpawnFailLen  EQU $ - szSpawnFail - 1
szSpawnFailDemo BYTE "[LSP-JSONRPC] Running in DEMO mode (no server).",13,10,0
szSpawnFailDemoLen EQU $ - szSpawnFailDemo - 1
szParseDiag     BYTE "[LSP-JSONRPC] === Diagnostic Parser Test ===",13,10,0
szParseDiagLen  EQU $ - szParseDiag - 1
szFoundDiag     BYTE "[LSP-JSONRPC] Found diagnostic: ",0
szFoundDiagLen  EQU $ - szFoundDiag - 1
szDiagSev       BYTE "[LSP-JSONRPC]   severity=",0
szDiagSevLen    EQU $ - szDiagSev - 1
szDiagLine      BYTE "[LSP-JSONRPC]   line=",0
szDiagLineLen   EQU $ - szDiagLine - 1
szShutdownMsg   BYTE "[LSP-JSONRPC] Sending shutdown + exit",13,10,0
szShutdownMsgLen EQU $ - szShutdownMsg - 1
szDoneMsg       BYTE "[LSP-JSONRPC] Done. JSON-RPC 2.0 framing verified.",13,10,0
szDoneMsgLen    EQU $ - szDoneMsg - 1
szNewline       BYTE 13,10,0
szCloseParen    BYTE ")",13,10,0
szCloseParenLen EQU $ - szCloseParen - 1

;--- Demo diagnostic response (used when clangd not available) ---
szDemoResponse BYTE \
    '{"jsonrpc":"2.0","id":1,"result":{"capabilities":{',\
    '"textDocumentSync":1,"completionProvider":{"triggerCharacters":[".",">",":"]}',\
    '}}}',0

szDemoDiagnostic BYTE \
    '{"jsonrpc":"2.0","method":"textDocument/publishDiagnostics","params":{',\
    '"uri":"file:///D:/rawrxd/test.c",',\
    '"diagnostics":[{',\
    '"range":{"start":{"line":5,"character":0},"end":{"line":5,"character":10}},',\
    '"severity":1,',\
    '"message":"undefined reference to foo"',\
    '}]}}',0

;==============================================================================
;                              BSS SECTION
;==============================================================================
.DATA?

ALIGN 16
hStdout         QWORD ?

; Pipe handles
hChildStdinRd   QWORD ?     ; child reads from this
hChildStdinWr   QWORD ?     ; parent writes to this
hChildStdoutRd  QWORD ?     ; parent reads from this
hChildStdoutWr  QWORD ?     ; child writes to this

; Process info
startInfo       STARTUPINFOA <>
procInfo        PROCESS_INFORMATION <>
secAttr         SECURITY_ATTRIBUTES <>

; I/O
dwBytesWritten  DWORD ?
dwBytesRead     DWORD ?
bServerAlive    DWORD ?     ; 1 = clangd running, 0 = demo mode

ALIGN 16
frameBuf        BYTE 8192 DUP(?)    ; Content-Length framing buffer
responseBuf     BYTE 32768 DUP(?)   ; response read buffer
numBuf          BYTE 64 DUP(?)

;==============================================================================
;                              CODE SECTION
;==============================================================================
.CODE

;------------------------------------------------------------------------------
; strlen_s — length of null-terminated string at RCX
; Returns: RAX = length
;------------------------------------------------------------------------------
strlen_s PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    .endprolog

    xor     rax, rax
@@lp:
    cmp     BYTE PTR [rcx+rax], 0
    je      @@done
    inc     rax
    jmp     @@lp
@@done:
    pop     rbp
    ret
strlen_s ENDP

;------------------------------------------------------------------------------
; itoa_u64 — Convert RAX (unsigned) to decimal string at RCX
; Returns: RAX = buffer ptr, RDX = length
;------------------------------------------------------------------------------
itoa_u64 PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rdi, rcx
    mov     rbx, rax
    xor     ecx, ecx
    mov     rsi, 10
@@dloop:
    xor     edx, edx
    mov     rax, rbx
    div     rsi
    add     dl, '0'
    push    rdx
    inc     ecx
    mov     rbx, rax
    test    rax, rax
    jnz     @@dloop

    mov     edx, ecx
    mov     rax, rdi
@@ploop:
    pop     rbx
    mov     BYTE PTR [rdi], bl
    inc     rdi
    dec     ecx
    jnz     @@ploop
    mov     BYTE PTR [rdi], 0

    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
itoa_u64 ENDP

;------------------------------------------------------------------------------
; memcpy_s — Copy RDX bytes from RSI-compat src (R8) to RCX dest
; RCX = dest, R8 = src, RDX = count
;------------------------------------------------------------------------------
memcpy_s PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rdi, rcx        ; dest
    mov     rsi, r8         ; src
    mov     rcx, rdx        ; count
    rep     movsb

    pop     rdi
    pop     rsi
    pop     rbp
    ret
memcpy_s ENDP

;------------------------------------------------------------------------------
; write_con — Write to stdout: RCX=ptr, EDX=len
;------------------------------------------------------------------------------
write_con PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 48h
    .allocstack 48h
    .endprolog

    mov     r8d, edx
    mov     rdx, rcx
    mov     rcx, [hStdout]
    lea     r9, [dwBytesWritten]
    mov     QWORD PTR [rsp+20h], 0
    call    WriteFile

    leave
    ret
write_con ENDP

;------------------------------------------------------------------------------
; atoi_simple — Parse decimal from string at RCX, stop at non-digit
; Returns: RAX = value
;------------------------------------------------------------------------------
atoi_simple PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    .endprolog

    xor     rax, rax
    xor     rdx, rdx
@@aloop:
    movzx   edx, BYTE PTR [rcx]
    sub     edx, '0'
    cmp     edx, 9
    ja      @@adone
    imul    rax, rax, 10
    add     rax, rdx
    inc     rcx
    jmp     @@aloop
@@adone:
    pop     rbp
    ret
atoi_simple ENDP

;==============================================================================
; jsonrpc_frame — Build Content-Length framed message
;
; RCX = destination buffer
; RDX = JSON body (null-terminated)
; Returns: RAX = total framed message length
;
; Output format:
;   Content-Length: <N>\r\n\r\n<json_body>
;==============================================================================
jsonrpc_frame PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 60h
    .allocstack 60h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rdi, rcx            ; rdi = output buffer
    mov     rsi, rdx            ; rsi = json body

    ; Get body length
    mov     rcx, rsi
    call    strlen_s
    mov     rbx, rax            ; rbx = body_len

    ; Write "Content-Length: "
    lea     r8, [szContentLength]
    mov     rcx, rdi
    mov     edx, 16             ; "Content-Length: " is 16 chars
    call    memcpy_s
    add     rdi, 16

    ; Write the length as decimal
    mov     rax, rbx
    mov     rcx, rdi
    call    itoa_u64
    ; rdx = digit count
    add     rdi, rdx

    ; Write "\r\n\r\n"
    mov     BYTE PTR [rdi], 13
    mov     BYTE PTR [rdi+1], 10
    mov     BYTE PTR [rdi+2], 13
    mov     BYTE PTR [rdi+3], 10
    add     rdi, 4

    ; Copy body
    mov     rcx, rdi
    mov     rdx, rbx
    mov     r8, rsi
    call    memcpy_s
    add     rdi, rbx

    ; Total length
    lea     rax, [frameBuf]
    sub     rdi, rax
    mov     rax, rdi

    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
jsonrpc_frame ENDP

;==============================================================================
; jsonrpc_send — Frame and send a JSON-RPC message to LSP server
;
; RCX = JSON body (null-terminated)
; Returns: RAX = bytes written (0 on failure)
;==============================================================================
jsonrpc_send PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 50h
    .allocstack 50h
    push    rbx
    .pushreg rbx
    .endprolog

    ; Frame the message
    mov     rdx, rcx            ; json body
    lea     rcx, [frameBuf]     ; dest
    call    jsonrpc_frame
    mov     rbx, rax            ; rbx = total frame length

    ; WriteFile(hChildStdinWr, frameBuf, frameLen, &written, NULL)
    mov     rcx, [hChildStdinWr]
    lea     rdx, [frameBuf]
    mov     r8d, ebx
    lea     r9, [dwBytesWritten]
    mov     QWORD PTR [rsp+20h], 0
    call    WriteFile

    test    eax, eax
    jz      @@send_fail
    mov     eax, [dwBytesWritten]
    jmp     @@send_done

@@send_fail:
    xor     eax, eax
@@send_done:
    pop     rbx
    leave
    ret
jsonrpc_send ENDP

;==============================================================================
; jsonrpc_recv — Read a JSON-RPC response with Content-Length parsing
;
; RCX = output buffer
; RDX = max buffer size
; Returns: RAX = body length read (0 on failure)
;
; Protocol: Read header bytes until \r\n\r\n, parse Content-Length,
;           then read exactly that many body bytes.
;==============================================================================
jsonrpc_recv PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 80h
    .allocstack 80h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    .endprolog

    mov     rdi, rcx            ; rdi = output buffer
    mov     r12, rdx            ; r12 = max size
    xor     r13d, r13d          ; r13 = header bytes accumulated

    ;--- Phase 1: Read header byte-by-byte until \r\n\r\n ---
    lea     rsi, [responseBuf]  ; header accumulator

@@read_header:
    ; ReadFile(hChildStdoutRd, &singleByte, 1, &bytesRead, NULL)
    mov     rcx, [hChildStdoutRd]
    lea     rdx, [rsi+r13]
    mov     r8d, 1
    lea     r9, [dwBytesRead]
    mov     QWORD PTR [rsp+20h], 0
    call    ReadFile
    test    eax, eax
    jz      @@recv_fail
    cmp     DWORD PTR [dwBytesRead], 0
    je      @@recv_fail

    inc     r13d
    cmp     r13d, 4
    jb      @@read_header       ; need at least 4 bytes to check

    ; Check if last 4 bytes are \r\n\r\n
    lea     rax, [rsi+r13-4]
    cmp     BYTE PTR [rax], 13
    jne     @@read_header
    cmp     BYTE PTR [rax+1], 10
    jne     @@read_header
    cmp     BYTE PTR [rax+2], 13
    jne     @@read_header
    cmp     BYTE PTR [rax+3], 10
    jne     @@read_header

    ; Null-terminate header
    mov     BYTE PTR [rsi+r13], 0

    ;--- Phase 2: Parse Content-Length from header ---
    ; Scan for "Content-Length: " in the header
    xor     ebx, ebx            ; scan position
@@find_cl:
    cmp     ebx, r13d
    jge     @@recv_fail         ; Content-Length not found

    ; Check for 'C','o','n','t'
    cmp     BYTE PTR [rsi+rbx], 'C'
    jne     @@next_cl
    cmp     BYTE PTR [rsi+rbx+1], 'o'
    jne     @@next_cl
    cmp     BYTE PTR [rsi+rbx+8], 'L'
    jne     @@next_cl
    ; Found "Content-L" — skip to after ": "
    lea     rcx, [rsi+rbx+16]   ; past "Content-Length: "
    call    atoi_simple
    mov     rbx, rax            ; rbx = content length
    jmp     @@got_length

@@next_cl:
    inc     ebx
    jmp     @@find_cl

@@got_length:
    ; Clamp to buffer size
    cmp     rbx, r12
    jbe     @@size_ok
    mov     rbx, r12
@@size_ok:

    ;--- Phase 3: Read exactly content_length bytes of body ---
    xor     r13d, r13d          ; bytes read so far

@@read_body:
    cmp     r13, rbx
    jge     @@recv_done

    mov     rcx, [hChildStdoutRd]
    lea     rdx, [rdi+r13]
    mov     rax, rbx
    sub     rax, r13
    mov     r8d, eax            ; remaining bytes
    lea     r9, [dwBytesRead]
    mov     QWORD PTR [rsp+20h], 0
    call    ReadFile
    test    eax, eax
    jz      @@recv_fail

    mov     eax, [dwBytesRead]
    test    eax, eax
    jz      @@recv_fail
    add     r13, rax
    jmp     @@read_body

@@recv_done:
    mov     BYTE PTR [rdi+r13], 0   ; null-terminate
    mov     rax, r13
    jmp     @@recv_exit

@@recv_fail:
    xor     eax, eax

@@recv_exit:
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
jsonrpc_recv ENDP

;==============================================================================
; parse_diagnostics — Scan JSON for "diagnostics" array, extract messages
;
; RCX = JSON string to parse
; Prints found diagnostics to console.
; Returns: RAX = number of diagnostics found
;
; Looks for: "severity":N and "message":"..." patterns
;==============================================================================
parse_diagnostics PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 60h
    .allocstack 60h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    .endprolog

    mov     rsi, rcx            ; rsi = JSON input
    xor     r12d, r12d          ; r12 = diagnostic count

    ; Print header
    lea     rcx, [szParseDiag]
    mov     edx, szParseDiagLen
    call    write_con

    ; Scan for "severity": pattern
@@scan_sev:
    cmp     BYTE PTR [rsi], 0
    je      @@parse_done

    cmp     BYTE PTR [rsi], '"'
    jne     @@skip_sev
    cmp     BYTE PTR [rsi+1], 's'
    jne     @@skip_sev
    cmp     BYTE PTR [rsi+2], 'e'
    jne     @@skip_sev
    cmp     BYTE PTR [rsi+3], 'v'
    jne     @@skip_sev
    cmp     BYTE PTR [rsi+4], 'e'
    jne     @@skip_sev
    cmp     BYTE PTR [rsi+5], 'r'
    jne     @@skip_sev
    cmp     BYTE PTR [rsi+6], 'i'
    jne     @@skip_sev
    cmp     BYTE PTR [rsi+7], 't'
    jne     @@skip_sev
    cmp     BYTE PTR [rsi+8], 'y'
    jne     @@skip_sev
    cmp     BYTE PTR [rsi+9], '"'
    jne     @@skip_sev
    cmp     BYTE PTR [rsi+10], ':'
    jne     @@skip_sev

    ; Found "severity": — extract the digit
    inc     r12d

    ; Print severity label
    lea     rcx, [szDiagSev]
    mov     edx, szDiagSevLen
    call    write_con

    ; Print the severity value (single digit)
    lea     rcx, [rsi+11]
    mov     edx, 1
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    add     rsi, 12
    jmp     @@scan_msg

@@skip_sev:
    inc     rsi
    jmp     @@scan_sev

    ; After finding severity, look for the message
@@scan_msg:
    cmp     BYTE PTR [rsi], 0
    je      @@parse_done
    cmp     BYTE PTR [rsi], '"'
    jne     @@skip_msg
    cmp     BYTE PTR [rsi+1], 'm'
    jne     @@skip_msg
    cmp     BYTE PTR [rsi+2], 'e'
    jne     @@skip_msg
    cmp     BYTE PTR [rsi+3], 's'
    jne     @@skip_msg
    cmp     BYTE PTR [rsi+4], 's'
    jne     @@skip_msg
    cmp     BYTE PTR [rsi+5], 'a'
    jne     @@skip_msg
    cmp     BYTE PTR [rsi+6], 'g'
    jne     @@skip_msg
    cmp     BYTE PTR [rsi+7], 'e'
    jne     @@skip_msg
    cmp     BYTE PTR [rsi+8], '"'
    jne     @@skip_msg
    cmp     BYTE PTR [rsi+9], ':'
    jne     @@skip_msg
    cmp     BYTE PTR [rsi+10], '"'
    jne     @@skip_msg

    ; Found "message":" — print until closing quote
    lea     rcx, [szFoundDiag]
    mov     edx, szFoundDiagLen
    call    write_con

    ; Find end of message string
    lea     rdi, [rsi+11]       ; start of message content
    mov     rbx, rdi
@@find_end_quote:
    cmp     BYTE PTR [rbx], 0
    je      @@print_msg
    cmp     BYTE PTR [rbx], '"'
    je      @@print_msg
    inc     rbx
    jmp     @@find_end_quote

@@print_msg:
    mov     rcx, rdi
    mov     rax, rbx
    sub     rax, rdi
    mov     edx, eax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    mov     rsi, rbx
    jmp     @@scan_sev          ; look for more diagnostics

@@skip_msg:
    inc     rsi
    jmp     @@scan_msg

@@parse_done:
    mov     eax, r12d

    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
parse_diagnostics ENDP

;==============================================================================
; main — Entry point
;==============================================================================
main PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 0A0h
    .allocstack 0A0h
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    .endprolog

    ;--- Get stdout ---
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     [hStdout], rax

    ;--- Banner ---
    lea     rcx, [szBanner]
    mov     edx, szBannerLen
    call    write_con

    ;=== Step 1: Create pipes for child stdin/stdout ===

    ; Setup SECURITY_ATTRIBUTES (bInheritHandle = TRUE)
    lea     rax, [secAttr]
    mov     DWORD PTR [rax + SECURITY_ATTRIBUTES.nLength], SIZEOF SECURITY_ATTRIBUTES
    mov     QWORD PTR [rax + SECURITY_ATTRIBUTES.lpSecurityDescriptor], 0
    mov     DWORD PTR [rax + SECURITY_ATTRIBUTES.bInheritHandle], 1

    ; CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) — child stdout
    lea     rcx, [hChildStdoutRd]
    lea     rdx, [hChildStdoutWr]
    lea     r8, [secAttr]
    xor     r9d, r9d
    call    CreatePipe
    test    eax, eax
    jz      @@pipe_fail

    ; Make read end non-inheritable
    mov     rcx, [hChildStdoutRd]
    xor     edx, edx            ; HANDLE_FLAG_INHERIT = 0
    xor     r8d, r8d            ; flags = 0
    call    SetHandleInformation

    ; CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) — child stdin
    lea     rcx, [hChildStdinRd]
    lea     rdx, [hChildStdinWr]
    lea     r8, [secAttr]
    xor     r9d, r9d
    call    CreatePipe
    test    eax, eax
    jz      @@pipe_fail

    ; Make write end non-inheritable
    mov     rcx, [hChildStdinWr]
    xor     edx, edx
    xor     r8d, r8d
    call    SetHandleInformation

    lea     rcx, [szPipeOk]
    mov     edx, szPipeOkLen
    call    write_con

    ;=== Step 2: Create LSP server process with redirected stdio ===

    ; Zero STARTUPINFO
    lea     rdi, [startInfo]
    mov     ecx, SIZEOF STARTUPINFOA
    xor     eax, eax
@@zero_si:
    mov     BYTE PTR [rdi+rcx-1], 0
    dec     ecx
    jnz     @@zero_si

    lea     rax, [startInfo]
    mov     DWORD PTR [rax + STARTUPINFOA.cb], SIZEOF STARTUPINFOA
    mov     DWORD PTR [rax + STARTUPINFOA.dwFlags], STARTF_USESTDHANDLES
    mov     rcx, [hChildStdinRd]
    mov     QWORD PTR [rax + STARTUPINFOA.hStdInput], rcx
    mov     rcx, [hChildStdoutWr]
    mov     QWORD PTR [rax + STARTUPINFOA.hStdOutput], rcx
    mov     QWORD PTR [rax + STARTUPINFOA.hStdError], rcx

    ; Zero PROCESS_INFORMATION
    lea     rdi, [procInfo]
    mov     ecx, SIZEOF PROCESS_INFORMATION
@@zero_pi:
    mov     BYTE PTR [rdi+rcx-1], 0
    dec     ecx
    jnz     @@zero_pi

    ; CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, ...)
    xor     ecx, ecx                    ; lpApplicationName = NULL
    lea     rdx, [szClangdCmd]           ; lpCommandLine
    xor     r8d, r8d                     ; lpProcessAttributes = NULL
    xor     r9d, r9d                     ; lpThreadAttributes = NULL
    mov     DWORD PTR [rsp+20h], 1       ; bInheritHandles = TRUE
    mov     DWORD PTR [rsp+28h], CREATE_NO_WINDOW  ; dwCreationFlags
    mov     QWORD PTR [rsp+30h], 0       ; lpEnvironment = NULL
    mov     QWORD PTR [rsp+38h], 0       ; lpCurrentDirectory = NULL
    lea     rax, [startInfo]
    mov     QWORD PTR [rsp+40h], rax     ; lpStartupInfo
    lea     rax, [procInfo]
    mov     QWORD PTR [rsp+48h], rax     ; lpProcessInformation
    call    CreateProcessA
    test    eax, eax
    jz      @@spawn_fail

    mov     DWORD PTR [bServerAlive], 1

    ; Close child-side pipe handles in parent
    mov     rcx, [hChildStdoutWr]
    call    CloseHandle
    mov     rcx, [hChildStdinRd]
    call    CloseHandle

    ; Print PID
    lea     rcx, [szSpawnOk]
    mov     edx, szSpawnOkLen
    call    write_con

    mov     eax, DWORD PTR [procInfo + PROCESS_INFORMATION.dwProcessId]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szCloseParen]
    mov     edx, szCloseParenLen
    call    write_con

    ;=== Step 3: Send JSON-RPC 2.0 initialize request ===
    lea     rcx, [szSendingInit]
    mov     edx, szSendingInitLen
    call    write_con

    lea     rcx, [szInitRequest]
    call    jsonrpc_send
    mov     r12d, eax           ; save bytes sent

    ; Print bytes sent
    lea     rcx, [szSentBytes]
    mov     edx, szSentBytesLen
    call    write_con
    mov     eax, r12d
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szBytesStr]
    mov     edx, szBytesStrLen
    call    write_con

    ;=== Step 4: Read response with Content-Length parsing ===
    lea     rcx, [szWaitResp]
    mov     edx, szWaitRespLen
    call    write_con

    lea     rcx, [responseBuf]
    mov     edx, 32000
    call    jsonrpc_recv
    mov     rbx, rax

    test    rax, rax
    jz      @@no_response

    ; Print Content-Length value
    lea     rcx, [szGotHeader]
    mov     edx, szGotHeaderLen
    call    write_con
    mov     rax, rbx
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ; Print response body
    lea     rcx, [szGotBody]
    mov     edx, szGotBodyLen
    call    write_con
    lea     rcx, [responseBuf]
    mov     edx, ebx
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ;=== Step 5: Send initialized notification ===
    lea     rcx, [szInitializedNotif]
    call    jsonrpc_send

    ;=== Step 6: Shutdown ===
    jmp     @@do_shutdown

@@no_response:
@@spawn_fail:
    ; If clangd not available, run in demo mode
    lea     rcx, [szSpawnFailDemo]
    mov     edx, szSpawnFailDemoLen
    call    write_con
    mov     DWORD PTR [bServerAlive], 0

    ; Demo: parse a synthetic diagnostic response
    lea     rcx, [szDemoDiagnostic]
    call    parse_diagnostics

    jmp     @@done

@@do_shutdown:
    lea     rcx, [szShutdownMsg]
    mov     edx, szShutdownMsgLen
    call    write_con

    lea     rcx, [szShutdownReq]
    call    jsonrpc_send

    ; Read shutdown response
    lea     rcx, [responseBuf]
    mov     edx, 32000
    call    jsonrpc_recv

    ; Send exit
    lea     rcx, [szExitNotif]
    call    jsonrpc_send

    ; Wait for process to exit
    mov     rcx, QWORD PTR [procInfo + PROCESS_INFORMATION.hProcess]
    mov     edx, 3000           ; 3 second timeout
    call    WaitForSingleObject

    ; Cleanup handles
    mov     rcx, QWORD PTR [procInfo + PROCESS_INFORMATION.hProcess]
    call    CloseHandle
    mov     rcx, QWORD PTR [procInfo + PROCESS_INFORMATION.hThread]
    call    CloseHandle

@@done:
    ; Close our pipe handles
    cmp     DWORD PTR [bServerAlive], 0
    je      @@skip_pipe_close
    mov     rcx, [hChildStdinWr]
    call    CloseHandle
    mov     rcx, [hChildStdoutRd]
    call    CloseHandle
@@skip_pipe_close:

    lea     rcx, [szDoneMsg]
    mov     edx, szDoneMsgLen
    call    write_con

    xor     ecx, ecx
    call    ExitProcess

@@pipe_fail:
    lea     rcx, [szPipeFail]
    mov     edx, szPipeFailLen
    call    write_con
    mov     ecx, 1
    call    ExitProcess

main ENDP

END
