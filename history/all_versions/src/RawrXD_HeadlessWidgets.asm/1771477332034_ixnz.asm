;==============================================================================
; RawrXD_HeadlessWidgets.asm — Headless Widget Data Transformers
; Completion | Diagnostics | Semantic (AVX-512) | Symbol | Folding | References
; Zero GUI | Zero CRT | Shared-memory IPC | Pipe command dispatch
;
; Assemble:  ml64 /c /Fo RawrXD_HeadlessWidgets.obj /nologo /W3 /I D:\rawrxd\include RawrXD_HeadlessWidgets.asm
; Link:      link RawrXD_HeadlessWidgets.obj /SUBSYSTEM:CONSOLE /ENTRY:HeadlessMain /MACHINE:X64 kernel32.lib
;==============================================================================

OPTION CASEMAP:NONE

INCLUDE rawrxd_master.inc

INCLUDELIB kernel32.lib

;------------------------------------------------------------------------------
; Inline EQUs not in rawrxd_win32_api.inc
;------------------------------------------------------------------------------
IFNDEF PIPE_ACCESS_DUPLEX
PIPE_ACCESS_DUPLEX          EQU 3
ENDIF
IFNDEF PIPE_TYPE_MESSAGE
PIPE_TYPE_MESSAGE           EQU 4
ENDIF
IFNDEF PIPE_READMODE_MESSAGE
PIPE_READMODE_MESSAGE       EQU 2
ENDIF
IFNDEF PIPE_WAIT
PIPE_WAIT                   EQU 0
ENDIF
IFNDEF PIPE_UNLIMITED_INSTANCES
PIPE_UNLIMITED_INSTANCES    EQU 255
ENDIF
IFNDEF FILE_MAP_ALL_ACCESS
FILE_MAP_ALL_ACCESS         EQU 0F001Fh
ENDIF
IFNDEF FILE_MAP_WRITE
FILE_MAP_WRITE              EQU 2
ENDIF

; Pipe/shared-memory API EXTERNs
EXTERNDEF CreateNamedPipeA:PROC
EXTERNDEF ConnectNamedPipe:PROC
EXTERNDEF DisconnectNamedPipe:PROC
EXTERNDEF OpenFileMappingA:PROC

;------------------------------------------------------------------------------
; Command opcodes (pipe protocol)
;------------------------------------------------------------------------------
CMD_COMPLETION              EQU 1
CMD_DIAGNOSTICS             EQU 2
CMD_SEMANTIC_TOKENS         EQU 3
CMD_SYMBOLS                 EQU 4
CMD_FOLDING                 EQU 5
CMD_REFERENCES              EQU 6
CMD_SHUTDOWN                EQU 0FFh

;------------------------------------------------------------------------------
; Token types (LSP encoding)
;------------------------------------------------------------------------------
TT_KEYWORD                  EQU 0
TT_IDENTIFIER               EQU 1
TT_NUMLIT                   EQU 2
TT_STRLIT                   EQU 3
TT_OPERATOR                 EQU 4
TT_COMMENT                  EQU 5
TT_DIRECTIVE                EQU 6
TT_REGISTER                 EQU 7

;------------------------------------------------------------------------------
; Diagnostic severity (LSP)
;------------------------------------------------------------------------------
DIAG_ERROR                  EQU 1
DIAG_WARNING                EQU 2
DIAG_INFO                   EQU 3
DIAG_HINT                   EQU 4

;------------------------------------------------------------------------------
; Layout constants
;------------------------------------------------------------------------------
MAX_COMPLETIONS             EQU 128
MAX_DIAGNOSTICS             EQU 256
MAX_SYMBOLS                 EQU 512
MAX_FOLDING_RANGES          EQU 256
MAX_REFERENCES              EQU 256
MAX_SEMANTIC_TOKENS         EQU 4096
CODE_BUFFER_SIZE            EQU 65536   ; 64 KB
PIPE_BUF_SIZE               EQU 8192
SHARED_STATE_SIZE           EQU 1048576 ; 1 MB
SHARED_CODE_SIZE            EQU 262144  ; 256 KB

;------------------------------------------------------------------------------
; Completion entry — 72 bytes
;------------------------------------------------------------------------------
COMPLETION_ENTRY STRUCT
    label_          BYTE 32 DUP(0)      ; display text
    insertText      BYTE 32 DUP(0)      ; insertion text
    kind            DWORD 0             ; CompletionItemKind
    score           DWORD 0             ; relevance 0-100
COMPLETION_ENTRY ENDS

;------------------------------------------------------------------------------
; Diagnostic entry — 80 bytes
;------------------------------------------------------------------------------
DIAGNOSTIC_ENTRY STRUCT
    line_           DWORD 0
    col_            DWORD 0
    endLine_        DWORD 0
    endCol_         DWORD 0
    severity_       DWORD 0
    pad             DWORD 0
    message_        BYTE 56 DUP(0)
DIAGNOSTIC_ENTRY ENDS

;------------------------------------------------------------------------------
; Symbol entry — 64 bytes
;------------------------------------------------------------------------------
SYMBOL_ENTRY STRUCT
    sname           BYTE 32 DUP(0)
    kind            DWORD 0             ; SymbolKind
    startLine       DWORD 0
    endLine         DWORD 0
    containerIdx    DWORD 0
    detail          BYTE 16 DUP(0)
SYMBOL_ENTRY ENDS

;------------------------------------------------------------------------------
; Folding range — 16 bytes
;------------------------------------------------------------------------------
FOLDING_RANGE STRUCT
    startLn         DWORD 0
    endLn           DWORD 0
    kind            DWORD 0             ; 1=comment, 2=import, 3=region
    pad0            DWORD 0
FOLDING_RANGE ENDS

;------------------------------------------------------------------------------
; Reference entry — 16 bytes
;------------------------------------------------------------------------------
REFERENCE_ENTRY STRUCT
    refLine         DWORD 0
    refCol          DWORD 0
    refLen          DWORD 0
    isDecl          DWORD 0
REFERENCE_ENTRY ENDS

;------------------------------------------------------------------------------
; Semantic token (packed LSP delta format) — 20 bytes
;------------------------------------------------------------------------------
SEMANTIC_TOKEN STRUCT
    deltaLine       DWORD 0
    deltaStart      DWORD 0
    length_         DWORD 0
    tokenType       DWORD 0
    tokenMods       DWORD 0
SEMANTIC_TOKEN ENDS


;==============================================================================
;                              DATA SECTION
;==============================================================================
.DATA

ALIGN 16
szPipeName          BYTE "\\.\pipe\RawrXD_WidgetPipe",0
szStateName         BYTE "Global\RawrXD_WidgetState_v1",0
szCodeBufName       BYTE "Global\RawrXD_CodeBuffer",0
bActive             BYTE 1

hPipe               QWORD 0
hHeap               QWORD 0
hStateMapping       QWORD 0
pStateView          QWORD 0
hCodeMapping        QWORD 0
pCodeView           QWORD 0

; --- Completion pool --------------------------------------------------------
ALIGN 16
CompletionCount     DWORD 0
CompletionPool      COMPLETION_ENTRY MAX_COMPLETIONS DUP(<>)

; --- Diagnostic pool --------------------------------------------------------
ALIGN 16
DiagnosticCount     DWORD 0
DiagnosticPool      DIAGNOSTIC_ENTRY MAX_DIAGNOSTICS DUP(<>)

; --- Symbol pool ------------------------------------------------------------
ALIGN 16
SymbolCount         DWORD 0
SymbolPool          SYMBOL_ENTRY MAX_SYMBOLS DUP(<>)

; --- Folding ranges ---------------------------------------------------------
ALIGN 16
FoldingCount        DWORD 0
FoldingPool         FOLDING_RANGE MAX_FOLDING_RANGES DUP(<>)

; --- Reference pool ---------------------------------------------------------
ALIGN 16
ReferenceCount      DWORD 0
ReferencePool       REFERENCE_ENTRY MAX_REFERENCES DUP(<>)

; --- Semantic tokens --------------------------------------------------------
ALIGN 16
SemanticCount       DWORD 0
SemanticPool        SEMANTIC_TOKEN MAX_SEMANTIC_TOKENS DUP(<>)

; --- Pipe I/O buffers -------------------------------------------------------
ALIGN 16
g_PipeBuf           BYTE PIPE_BUF_SIZE DUP(0)
g_ResponseBuf       BYTE 4096 DUP(0)

; --- Known keywords (used by completion & semantic engine) ------------------
ALIGN 16
kw_proc             BYTE "PROC",0
kw_endp             BYTE "ENDP",0
kw_mov              BYTE "mov",0
kw_call             BYTE "call",0
kw_push             BYTE "push",0
kw_pop              BYTE "pop",0
kw_lea              BYTE "lea",0
kw_ret              BYTE "ret",0
kw_jmp              BYTE "jmp",0
kw_je               BYTE "je",0
kw_jne              BYTE "jne",0
kw_jz               BYTE "jz",0
kw_jnz              BYTE "jnz",0
kw_cmp              BYTE "cmp",0
kw_test             BYTE "test",0
kw_xor              BYTE "xor",0
kw_and_             BYTE "and",0
kw_or_              BYTE "or",0
kw_sub_             BYTE "sub",0
kw_add_             BYTE "add",0
kw_include          BYTE "INCLUDE",0

; Keyword table: array of QWORD pointers, NUL-terminated
ALIGN 16
KeywordTable:
    QWORD kw_proc, kw_endp, kw_mov, kw_call, kw_push, kw_pop
    QWORD kw_lea, kw_ret, kw_jmp, kw_je, kw_jne, kw_jz, kw_jnz
    QWORD kw_cmp, kw_test, kw_xor, kw_and_, kw_or_, kw_sub_, kw_add_
    QWORD kw_include
    QWORD 0                 ; sentinel

; --- Console strings --------------------------------------------------------
szWelcome           BYTE "[HEADLESS] Widget data transformers online",13,10,0
szShutdown          BYTE "[HEADLESS] Shutting down",13,10,0
szConnected         BYTE "[HEADLESS] Pipe client connected",13,10,0
szDispatch          BYTE "[HEADLESS] Dispatching cmd ",0
szOK                BYTE "OK",0
szERR               BYTE "ERR",0
szNewline           BYTE 13,10,0


;==============================================================================
;                              CODE SECTION
;==============================================================================
.CODE

;------------------------------------------------------------------------------
; HL_Print — write NUL-terminated string to stdout
;------------------------------------------------------------------------------
HL_Print PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 60h
    .allocstack 60h
    mov  qword ptr [rbp-8h], rsi    ; save non-volatile rsi
    .savereg rsi, 58h
    mov  qword ptr [rbp-10h], rdi   ; save non-volatile rdi
    .savereg rdi, 50h
    .endprolog

    mov  rsi, rcx
    mov  rdi, rsi
    xor  ecx, ecx
    dec  rcx
    xor  eax, eax
    repne scasb
    not  rcx
    dec  rcx
    mov  dword ptr [rbp-18h], ecx   ; save strlen (R8 is volatile!)

    mov  ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov  rcx, rax
    mov  rdx, rsi
    mov  r8d, dword ptr [rbp-18h]   ; restore strlen from local
    lea  r9, [rbp-20h]
    mov  qword ptr [rsp+20h], 0
    call WriteFile

    mov  rsi, qword ptr [rbp-8h]    ; restore rsi
    mov  rdi, qword ptr [rbp-10h]   ; restore rdi
    leave
    ret
HL_Print ENDP

;------------------------------------------------------------------------------
; HL_StrCmp — simple NUL-terminated byte comparison
; RCX=a, RDX=b → RAX=0 if equal
;------------------------------------------------------------------------------
HL_StrCmp PROC FRAME
    .endprolog
@@:
    mov  al, byte ptr [rcx]
    mov  ah, byte ptr [rdx]
    cmp  al, ah
    jne  sc_ne
    test al, al
    jz   sc_eq
    inc  rcx
    inc  rdx
    jmp  @B
sc_eq:
    xor  eax, eax
    ret
sc_ne:
    mov  eax, 1
    ret
HL_StrCmp ENDP

;------------------------------------------------------------------------------
; HL_StrNCopy — copy up to ECX bytes from RSI→RDI, NUL-pad remainder
;------------------------------------------------------------------------------
HL_StrNCopy PROC FRAME
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    .endprolog
    ; RCX=maxLen, RDX=pSrc, R8=pDst
    mov  rsi, rdx
    mov  rdi, r8
    mov  edx, ecx           ; save max
@@:
    test ecx, ecx
    jz   snc_done
    mov  al, byte ptr [rsi]
    mov  byte ptr [rdi], al
    test al, al
    jz   snc_pad
    inc  rsi
    inc  rdi
    dec  ecx
    jmp  @B
snc_pad:
    ; NUL-fill remainder
    rep  stosb
snc_done:
    pop  rsi
    pop  rdi
    ret
HL_StrNCopy ENDP

;------------------------------------------------------------------------------
; IsIdentifierChar — test if AL is [a-zA-Z0-9_]
; AL = char → AL = 1 if ident, 0 if not
;------------------------------------------------------------------------------
IsIdentifierChar PROC FRAME
    .endprolog
    cmp  al, '_'
    je   iic_yes
    cmp  al, '0'
    jb   iic_no
    cmp  al, '9'
    jbe  iic_yes
    cmp  al, 'A'
    jb   iic_no
    cmp  al, 'Z'
    jbe  iic_yes
    cmp  al, 'a'
    jb   iic_no
    cmp  al, 'z'
    jbe  iic_yes
iic_no:
    xor  eax, eax
    ret
iic_yes:
    mov  eax, 1
    ret
IsIdentifierChar ENDP

;------------------------------------------------------------------------------
; Widget_CreateSharedMemory — create or open shared-memory regions
;------------------------------------------------------------------------------
Widget_CreateSharedMemory PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    .endprolog

    ; ---- State region (1 MB) ----
    ; CreateFileMappingA(hFile, lpAttr, flProtect, dwMaxHigh, dwMaxLow, lpName)
    mov  rcx, INVALID_HANDLE_VALUE      ; hFile = page file
    xor  edx, edx                       ; lpAttributes = NULL
    mov  r8d, PAGE_READWRITE            ; flProtect
    xor  r9d, r9d                       ; dwMaximumSizeHigh = 0
    mov  qword ptr [rsp+20h], SHARED_STATE_SIZE  ; dwMaximumSizeLow (QWORD!)
    lea  rax, szStateName
    mov  qword ptr [rsp+28h], rax       ; lpName
    call CreateFileMappingA
    test rax, rax
    jz   csm_fail
    mov  hStateMapping, rax

    ; Map view
    mov  rcx, rax
    mov  edx, FILE_MAP_ALL_ACCESS
    xor  r8d, r8d                       ; dwFileOffsetHigh
    xor  r9d, r9d                       ; dwFileOffsetLow
    mov  qword ptr [rsp+20h], 0         ; dwNumberOfBytesToMap (0=all)
    call MapViewOfFile
    test rax, rax
    jz   csm_fail
    mov  pStateView, rax

    ; ---- Code buffer region (256 KB) ----
    mov  rcx, INVALID_HANDLE_VALUE
    xor  edx, edx
    mov  r8d, PAGE_READWRITE
    xor  r9d, r9d
    mov  qword ptr [rsp+20h], SHARED_CODE_SIZE   ; QWORD write
    lea  rax, szCodeBufName
    mov  qword ptr [rsp+28h], rax
    call CreateFileMappingA
    test rax, rax
    jz   csm_fail
    mov  hCodeMapping, rax

    mov  rcx, rax
    mov  edx, FILE_MAP_ALL_ACCESS
    xor  r8d, r8d
    xor  r9d, r9d
    mov  qword ptr [rsp+20h], 0
    call MapViewOfFile
    test rax, rax
    jz   csm_fail
    mov  pCodeView, rax

    mov  eax, 1
    jmp  csm_exit

csm_fail:
    ; Partial-failure cleanup — close/unmap anything already opened
    mov  rcx, pCodeView
    test rcx, rcx
    jz   csm_c1
    call UnmapViewOfFile
    mov  pCodeView, 0
csm_c1:
    mov  rcx, hCodeMapping
    test rcx, rcx
    jz   csm_c2
    call CloseHandle
    mov  hCodeMapping, 0
csm_c2:
    mov  rcx, pStateView
    test rcx, rcx
    jz   csm_c3
    call UnmapViewOfFile
    mov  pStateView, 0
csm_c3:
    mov  rcx, hStateMapping
    test rcx, rcx
    jz   csm_c4
    call CloseHandle
    mov  hStateMapping, 0
csm_c4:
    xor  eax, eax

csm_exit:
    leave
    ret
Widget_CreateSharedMemory ENDP

;------------------------------------------------------------------------------
; Widget_GetCodeBuffer — return ptr to shared code buffer in RAX
;------------------------------------------------------------------------------
Widget_GetCodeBuffer PROC FRAME
    .endprolog
    mov  rax, pCodeView
    ret
Widget_GetCodeBuffer ENDP

;------------------------------------------------------------------------------
; Widget_CompletionEngine — scan current cursor context, produce completions
; RCX = ptr to code buffer,  RDX = cursor offset
;------------------------------------------------------------------------------
Widget_CompletionEngine PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 100h
    .allocstack 100h
    mov  qword ptr [rbp-8h], r12
    .savereg r12, 0F8h
    mov  qword ptr [rbp-10h], r13
    .savereg r13, 0F0h
    mov  qword ptr [rbp-18h], r14
    .savereg r14, 0E8h
    mov  qword ptr [rbp-20h], rsi
    .savereg rsi, 0E0h
    mov  qword ptr [rbp-28h], rdi
    .savereg rdi, 0D8h
    .endprolog

    mov  r12, rcx           ; code buffer base
    mov  r13, rdx           ; cursor offset

    ; Reset completions
    mov  CompletionCount, 0

    ; Walk backwards from cursor to find identifier prefix
    lea  rsi, [r12 + r13]   ; cursor ptr
    mov  rdi, rsi            ; end of prefix
    cmp  r13, 0
    je   ce_scan_kw          ; nothing to scan

ce_back:
    dec  rsi
    cmp  rsi, r12
    jb   ce_got_prefix
    mov  al, byte ptr [rsi]
    call IsIdentifierChar
    test eax, eax
    jnz  ce_back
    inc  rsi                 ; first char of prefix

ce_got_prefix:
    ; RSI = start of prefix, RDI = end
    ; Compute prefix length
    mov  r14, rdi
    sub  r14, rsi            ; prefix len

ce_scan_kw:
    ; Scan KeywordTable for matches
    lea  rcx, KeywordTable
    xor  edx, edx           ; completion index

ce_kw_loop:
    mov  rax, qword ptr [rcx]
    test rax, rax
    jz   ce_done

    ; If no prefix, add all keywords (triggered by empty context)
    cmp  r14, 0
    je   ce_add_kw

    ; Match prefix against keyword
    push rcx
    push rdx
    mov  rdi, rax            ; keyword ptr
    mov  r8, rsi             ; prefix ptr
    mov  r9, r14             ; prefix len
    xor  eax, eax

ce_match:
    cmp  rax, r9
    jge  ce_matched
    mov  cl, byte ptr [rdi + rax]
    test cl, cl
    jz   ce_no_match         ; keyword shorter than prefix
    mov  ch, byte ptr [r8 + rax]
    ; case-fold
    or   cl, 20h
    or   ch, 20h
    cmp  cl, ch
    jne  ce_no_match
    inc  rax
    jmp  ce_match

ce_matched:
    pop  rdx
    pop  rcx
    jmp  ce_add_kw

ce_no_match:
    pop  rdx
    pop  rcx
    add  rcx, 8
    jmp  ce_kw_loop

ce_add_kw:
    ; Add to CompletionPool[edx]
    cmp  edx, MAX_COMPLETIONS
    jge  ce_done

    push rcx
    push rdx

    ; Calculate pool entry address
    mov  eax, edx
    imul eax, SIZEOF COMPLETION_ENTRY
    lea  rdi, CompletionPool
    add  rdi, rax

    ; Copy keyword to label_ and insertText
    mov  rax, qword ptr [rcx]  ; keyword string ptr
    mov  rsi, rax

    ; Copy to label_ (first 32 bytes)
    push rdi
    mov  ecx, 32
ce_copy_label:
    mov  al, byte ptr [rsi]
    mov  byte ptr [rdi], al
    test al, al
    jz   ce_pad_label
    inc  rsi
    inc  rdi
    dec  ecx
    jmp  ce_copy_label
ce_pad_label:
    rep  stosb               ; NUL-pad
    pop  rdi

    ; Copy to insertText (offset +32)
    mov  rax, qword ptr [rsp+8]  ; re-get keyword from stack's rcx
    mov  rax, qword ptr [rax]
    mov  rsi, rax
    lea  rdi, [rdi + 32]
    mov  ecx, 32
ce_copy_insert:
    mov  al, byte ptr [rsi]
    mov  byte ptr [rdi], al
    test al, al
    jz   ce_pad_insert
    inc  rsi
    inc  rdi
    dec  ecx
    jmp  ce_copy_insert
ce_pad_insert:
    rep  stosb
    sub  rdi, 32             ; back to insertText start
    sub  rdi, 32             ; back to entry start

    ; Set kind=14 (keyword), score=80
    mov  dword ptr [rdi + 64], 14
    mov  dword ptr [rdi + 68], 80

    pop  rdx
    pop  rcx
    inc  edx
    add  rcx, 8
    jmp  ce_kw_loop

ce_done:
    mov  CompletionCount, edx
    mov  eax, edx

    mov  rdi, qword ptr [rbp-28h]
    mov  rsi, qword ptr [rbp-20h]
    mov  r14, qword ptr [rbp-18h]
    mov  r13, qword ptr [rbp-10h]
    mov  r12, qword ptr [rbp-8h]
    leave
    ret
Widget_CompletionEngine ENDP

;------------------------------------------------------------------------------
; Widget_AddDiagnostic — add diagnostic to pool
; RCX=line, RDX=col, R8=severity, R9=pMessage
;------------------------------------------------------------------------------
Widget_AddDiagnostic PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    mov  qword ptr [rbp-8h], rdi    ; save non-volatile rdi
    .savereg rdi, 38h
    mov  qword ptr [rbp-10h], rsi   ; save non-volatile rsi
    .savereg rsi, 30h
    .endprolog

    mov  eax, DiagnosticCount
    cmp  eax, MAX_DIAGNOSTICS
    jge  ad_full

    ; Calculate entry address
    imul eax, SIZEOF DIAGNOSTIC_ENTRY
    lea  rdi, DiagnosticPool
    add  rdi, rax

    ; Fill fields
    mov  dword ptr [rdi + DIAGNOSTIC_ENTRY.line_], ecx
    mov  dword ptr [rdi + DIAGNOSTIC_ENTRY.col_], edx
    mov  dword ptr [rdi + DIAGNOSTIC_ENTRY.endLine_], ecx   ; same line
    mov  dword ptr [rdi + DIAGNOSTIC_ENTRY.endCol_], edx
    mov  dword ptr [rdi + DIAGNOSTIC_ENTRY.severity_], r8d
    mov  dword ptr [rdi + DIAGNOSTIC_ENTRY.pad], 0

    ; Copy message (up to 55 chars + NUL)
    lea  rdi, [rdi + DIAGNOSTIC_ENTRY.message_]
    mov  rsi, r9
    mov  ecx, 55
@@:
    mov  al, byte ptr [rsi]
    mov  byte ptr [rdi], al
    test al, al
    jz   ad_pad
    inc  rsi
    inc  rdi
    dec  ecx
    jz   ad_trunc
    jmp  @B
ad_trunc:
    mov  byte ptr [rdi], 0
ad_pad:
    inc  DiagnosticCount
    mov  eax, 1
    jmp  ad_exit
ad_full:
    xor  eax, eax
ad_exit:
    mov  rdi, qword ptr [rbp-8h]    ; restore rdi
    mov  rsi, qword ptr [rbp-10h]   ; restore rsi
    leave
    ret
Widget_AddDiagnostic ENDP

;------------------------------------------------------------------------------
; Widget_GetDiagnosticCount — return count in EAX
;------------------------------------------------------------------------------
Widget_GetDiagnosticCount PROC FRAME
    .endprolog
    mov  eax, DiagnosticCount
    ret
Widget_GetDiagnosticCount ENDP

;------------------------------------------------------------------------------
; Widget_DiagnosticsEngine — scan code buffer for common issues
; RCX = code buffer, RDX = buffer length
;------------------------------------------------------------------------------
Widget_DiagnosticsEngine PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 100h
    .allocstack 100h
    mov  qword ptr [rbp-8h], r12
    .savereg r12, 0F8h
    mov  qword ptr [rbp-10h], r13
    .savereg r13, 0F0h
    mov  qword ptr [rbp-18h], r14
    .savereg r14, 0E8h
    mov  qword ptr [rbp-20h], rsi      ; save non-volatile rsi
    .savereg rsi, 0E0h
    .endprolog

    mov  r12, rcx           ; code buf
    mov  r13, rdx           ; length

    ; Reset diagnostics
    mov  DiagnosticCount, 0

    ; State tracking
    mov  dword ptr [rbp-30h], 0     ; proc_depth (open PROC count)
    mov  dword ptr [rbp-34h], 0     ; push_count (in current proc)
    mov  dword ptr [rbp-38h], 0     ; pop_count (in current proc)

    ; Scan line by line
    xor  r14d, r14d         ; line counter
    mov  rsi, r12

de_line:
    mov  rax, rsi
    sub  rax, r12
    cmp  rax, r13
    jge  de_check_balance

    inc  r14d               ; line number (1-based)

    ; --- Skip leading whitespace, find first token ---
    mov  qword ptr [rbp-48h], rsi   ; save line start
de_skip_ws:
    mov  al, byte ptr [rsi]
    cmp  al, ' '
    je   de_skip_ws2
    cmp  al, 9               ; tab
    je   de_skip_ws2
    jmp  de_got_token
de_skip_ws2:
    inc  rsi
    jmp  de_skip_ws

de_got_token:
    ; --- Check for PROC keyword (anywhere on line) ---
    ; Scan line for " PROC" or "\tPROC" (keyword after identifier)
    mov  qword ptr [rbp-50h], rsi   ; save token start
    ; First, measure line length and find EOL
    xor  ecx, ecx           ; char count
    mov  rax, rsi
de_measure:
    mov  dl, byte ptr [rax]
    test dl, dl
    jz   de_eol_found
    cmp  dl, 10             ; LF
    je   de_eol_found
    inc  ecx
    inc  rax
    jmp  de_measure
de_eol_found:
    mov  dword ptr [rbp-3Ch], ecx   ; line_length
    mov  qword ptr [rbp-58h], rax   ; save EOL pointer

    ; --- Scan line for PROC/ENDP/push/pop keywords ---
    mov  rax, qword ptr [rbp-48h]   ; line start
de_kw_scan:
    cmp  rax, qword ptr [rbp-58h]
    jge  de_after_kw

    ; Skip whitespace
    mov  dl, byte ptr [rax]
    cmp  dl, ' '
    je   de_kw_next
    cmp  dl, 9
    je   de_kw_next

    ; Check "PROC" (case-sensitive, preceded or followed by whitespace/EOL)
    cmp  dword ptr [rax], 'P' OR ('R' SHL 8) OR ('O' SHL 16) OR ('C' SHL 24)
    jne  de_check_endp
    ; Verify followed by whitespace/EOL/NUL
    mov  dl, byte ptr [rax+4]
    cmp  dl, ' '
    je   de_found_proc
    cmp  dl, 9
    je   de_found_proc
    cmp  dl, 10
    je   de_found_proc
    cmp  dl, 13
    je   de_found_proc
    test dl, dl
    jz   de_found_proc
    jmp  de_check_endp

de_found_proc:
    inc  dword ptr [rbp-30h]        ; proc_depth++
    mov  dword ptr [rbp-34h], 0     ; reset push_count
    mov  dword ptr [rbp-38h], 0     ; reset pop_count
    jmp  de_after_kw

de_check_endp:
    cmp  dword ptr [rax], 'E' OR ('N' SHL 8) OR ('D' SHL 16) OR ('P' SHL 24)
    jne  de_check_push
    mov  dl, byte ptr [rax+4]
    cmp  dl, ' '
    je   de_found_endp
    cmp  dl, 9
    je   de_found_endp
    cmp  dl, 10
    je   de_found_endp
    cmp  dl, 13
    je   de_found_endp
    test dl, dl
    jz   de_found_endp
    jmp  de_check_push

de_found_endp:
    ; Check push/pop balance before closing this PROC scope
    mov  eax, dword ptr [rbp-34h]   ; push_count
    cmp  eax, dword ptr [rbp-38h]   ; pop_count
    je   de_balance_ok

    ; Unbalanced push/pop → emit warning
    mov  qword ptr [rbp-28h], rsi
    mov  ecx, r14d
    mov  edx, 1
    mov  r8d, DIAG_WARNING
    lea  r9, [rbp-80h]
    mov  dword ptr [r9],    'U' OR ('n' SHL 8) OR ('b' SHL 16) OR ('a' SHL 24)
    mov  dword ptr [r9+4],  'l' OR ('a' SHL 8) OR ('n' SHL 16) OR ('c' SHL 24)
    mov  dword ptr [r9+8],  'e' OR ('d' SHL 8) OR (' ' SHL 16) OR ('p' SHL 24)
    mov  dword ptr [r9+12], 'u' OR ('s' SHL 8) OR ('h' SHL 16) OR ('/' SHL 24)
    mov  dword ptr [r9+16], 'p' OR ('o' SHL 8) OR ('p' SHL 16)
    mov  byte ptr  [r9+19], 0
    call Widget_AddDiagnostic
    mov  rsi, qword ptr [rbp-28h]

de_balance_ok:
    dec  dword ptr [rbp-30h]        ; proc_depth--
    jns  de_after_kw
    ; Negative depth → ENDP without PROC
    mov  dword ptr [rbp-30h], 0
    mov  qword ptr [rbp-28h], rsi
    mov  ecx, r14d
    mov  edx, 1
    mov  r8d, DIAG_ERROR
    lea  r9, [rbp-80h]
    mov  dword ptr [r9],    'E' OR ('N' SHL 8) OR ('D' SHL 16) OR ('P' SHL 24)
    mov  dword ptr [r9+4],  ' ' OR ('w' SHL 8) OR ('i' SHL 16) OR ('t' SHL 24)
    mov  dword ptr [r9+8],  'h' OR ('o' SHL 8) OR ('u' SHL 16) OR ('t' SHL 24)
    mov  dword ptr [r9+12], ' ' OR ('P' SHL 8) OR ('R' SHL 16) OR ('O' SHL 24)
    mov  byte ptr  [r9+16], 'C'
    mov  byte ptr  [r9+17], 0
    call Widget_AddDiagnostic
    mov  rsi, qword ptr [rbp-28h]
    jmp  de_after_kw

de_check_push:
    ; "push" — 4 chars
    cmp  dword ptr [rax], 'p' OR ('u' SHL 8) OR ('s' SHL 16) OR ('h' SHL 24)
    jne  de_check_pop
    mov  dl, byte ptr [rax+4]
    cmp  dl, ' '
    je   de_inc_push
    cmp  dl, 9
    je   de_inc_push
    jmp  de_check_pop
de_inc_push:
    inc  dword ptr [rbp-34h]
    jmp  de_kw_next

de_check_pop:
    ; "pop " — 3+whitespace
    cmp  word ptr [rax], 'p' OR ('o' SHL 8)
    jne  de_kw_next
    cmp  byte ptr [rax+2], 'p'
    jne  de_kw_next
    mov  dl, byte ptr [rax+3]
    cmp  dl, ' '
    je   de_inc_pop
    cmp  dl, 9
    je   de_inc_pop
    jmp  de_kw_next
de_inc_pop:
    inc  dword ptr [rbp-38h]

de_kw_next:
    inc  rax
    jmp  de_kw_scan

de_after_kw:
    ; --- Line length check ---
    mov  ecx, dword ptr [rbp-3Ch]
    cmp  ecx, 120
    jle  de_advance

    mov  qword ptr [rbp-28h], rsi
    mov  ecx, r14d
    mov  edx, 121
    mov  r8d, DIAG_WARNING
    lea  r9, [rbp-80h]
    mov  dword ptr [r9],    'L' OR ('i' SHL 8) OR ('n' SHL 16) OR ('e' SHL 24)
    mov  dword ptr [r9+4],  ' ' OR ('t' SHL 8) OR ('o' SHL 16) OR ('o' SHL 24)
    mov  dword ptr [r9+8],  ' ' OR ('l' SHL 8) OR ('o' SHL 16) OR ('n' SHL 24)
    mov  word ptr  [r9+12], 'g'
    mov  byte ptr  [r9+14], 0
    call Widget_AddDiagnostic
    mov  rsi, qword ptr [rbp-28h]

de_advance:
    ; Move to next line
    mov  rsi, qword ptr [rbp-58h]   ; EOL pointer
    cmp  byte ptr [rsi], 10
    jne  de_skip_cr
    inc  rsi                        ; skip LF
de_skip_cr:
    jmp  de_line

de_check_balance:
    ; End of file — check if any PROCs are still open
    mov  eax, dword ptr [rbp-30h]
    test eax, eax
    jz   de_done

    ; Unclosed PROC(s) → emit error
    mov  qword ptr [rbp-28h], rsi
    mov  ecx, r14d          ; last line
    mov  edx, 1
    mov  r8d, DIAG_ERROR
    lea  r9, [rbp-80h]
    mov  dword ptr [r9],    'U' OR ('n' SHL 8) OR ('c' SHL 16) OR ('l' SHL 24)
    mov  dword ptr [r9+4],  'o' OR ('s' SHL 8) OR ('e' SHL 16) OR ('d' SHL 24)
    mov  dword ptr [r9+8],  ' ' OR ('P' SHL 8) OR ('R' SHL 16) OR ('O' SHL 24)
    mov  byte ptr  [r9+12], 'C'
    mov  byte ptr  [r9+13], 0
    call Widget_AddDiagnostic
    mov  rsi, qword ptr [rbp-28h]

de_done:
    mov  eax, DiagnosticCount

    mov  rsi, qword ptr [rbp-20h]    ; restore rsi
    mov  r14, qword ptr [rbp-18h]
    mov  r13, qword ptr [rbp-10h]
    mov  r12, qword ptr [rbp-8h]
    leave
    ret
Widget_DiagnosticsEngine ENDP

;------------------------------------------------------------------------------
; Widget_SemanticEngine — AVX-512 accelerated token classifier
; Scans code buffer byte-parallel using ZMM registers
; RCX = code buffer, RDX = length
;------------------------------------------------------------------------------
Widget_SemanticEngine PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 200h
    .allocstack 200h
    mov  qword ptr [rbp-8h], r12
    .savereg r12, 1F8h
    mov  qword ptr [rbp-10h], r13
    .savereg r13, 1F0h
    mov  qword ptr [rbp-18h], r14
    .savereg r14, 1E8h
    mov  qword ptr [rbp-20h], r15
    .savereg r15, 1E0h
    mov  qword ptr [rbp-28h], rsi      ; save non-volatile rsi
    .savereg rsi, 1D8h
    mov  qword ptr [rbp-30h], rdi      ; save non-volatile rdi
    .savereg rdi, 1D0h
    mov  qword ptr [rbp-38h], rbx      ; save non-volatile rbx
    .savereg rbx, 1C8h
    .endprolog

    mov  r12, rcx           ; buffer
    mov  r13, rdx           ; length

    ; Reset
    mov  SemanticCount, 0
    xor  r15d, r15d         ; current token index

    ; Check for AVX-512 support (CPUID.7.0:EBX.AVX512F [bit 16])
    ; RBX saved in prolog — safe to clobber for CPUID
    mov  eax, 7
    xor  ecx, ecx
    cpuid
    bt   ebx, 16
    ; RBX restored in epilog
    jnc  se_scalar           ; no AVX-512 → scalar fallback

    ;----------------------------------------------------------------------
    ; AVX-512 path: broadcast space (0x20), semicolon (';'), newline (0x0A)
    ; Scan 64 bytes at a time for comment markers
    ;----------------------------------------------------------------------
    vpbroadcastb zmm1, byte ptr [se_space]      ; space pattern
    vpbroadcastb zmm2, byte ptr [se_semicolon]  ; ';' pattern
    vpbroadcastb zmm3, byte ptr [se_newline]    ; newline pattern

    mov  r14, r12            ; scan pointer
    mov  rax, r13
    and  rax, 0FFFFFFFFFFFFFFC0h  ; align to 64-byte blocks

    xor  ecx, ecx           ; line counter

se_avx_loop:
    cmp  r14, r12
    jb   se_avx_done
    mov  rax, r14
    sub  rax, r12
    add  rax, 64
    cmp  rax, r13
    ja   se_scalar_tail

    ; Load 64 bytes
    vmovdqu8 zmm0, zmmword ptr [r14]

    ; Compare for semicolons → comment tokens
    vpcmpeqb k1, zmm0, zmm2
    kmovq rax, k1
    test  rax, rax
    jz    se_avx_no_semi

    ; Found semicolons — emit comment tokens
    ; Count trailing zeros to find first match position
    bsf   rdx, rax
    mov   ecx, r15d
    cmp   ecx, MAX_SEMANTIC_TOKENS
    jge   se_avx_done

    ; deltaLine=0, deltaStart=offset, length=1, type=COMMENT
    imul  eax, ecx, SIZEOF SEMANTIC_TOKEN
    lea   rdi, SemanticPool
    add   rdi, rax
    mov   dword ptr [rdi + SEMANTIC_TOKEN.deltaLine], 0
    mov   dword ptr [rdi + SEMANTIC_TOKEN.deltaStart], edx
    mov   dword ptr [rdi + SEMANTIC_TOKEN.length_], 1
    mov   dword ptr [rdi + SEMANTIC_TOKEN.tokenType], TT_COMMENT
    mov   dword ptr [rdi + SEMANTIC_TOKEN.tokenMods], 0
    inc   r15d

se_avx_no_semi:
    ; Compare for newlines → increment line counter
    vpcmpeqb k2, zmm0, zmm3
    kmovq rax, k2
    popcnt rax, rax          ; count newlines in block
    add  ecx, eax            ; accumulate line count

    add  r14, 64
    jmp  se_avx_loop

se_avx_done:
se_scalar_tail:
    ; Handle remaining bytes via scalar path

se_scalar:
    ;----------------------------------------------------------------------
    ; Scalar fallback: simple token classifier
    ;----------------------------------------------------------------------
    mov  rsi, r12
    xor  r14d, r14d         ; line
    xor  ecx, ecx           ; column

se_sc_loop:
    mov  rax, rsi
    sub  rax, r12
    cmp  rax, r13
    jge  se_sc_done

    movzx eax, byte ptr [rsi]

    cmp  al, 10              ; LF
    jne  se_not_nl
    inc  r14d
    xor  ecx, ecx
    inc  rsi
    jmp  se_sc_loop

se_not_nl:
    cmp  al, ';'
    jne  se_not_comment
    ; Emit comment token to end of line
    mov  edx, ecx            ; startCol
    mov  qword ptr [rbp-40h], rsi    ; save comment start
se_skip_comment:
    inc  rsi
    mov  rax, rsi
    sub  rax, r12
    cmp  rax, r13
    jge  se_end_comment
    cmp  byte ptr [rsi], 10
    jne  se_skip_comment
se_end_comment:
    mov  rax, qword ptr [rbp-40h]    ; retrieve comment start
    mov  r8, rsi
    sub  r8, rax             ; length of comment

    cmp  r15d, MAX_SEMANTIC_TOKENS
    jge  se_sc_done

    imul eax, r15d, SIZEOF SEMANTIC_TOKEN
    lea  rdi, SemanticPool
    add  rdi, rax
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaLine], r14d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaStart], edx
    mov  dword ptr [rdi + SEMANTIC_TOKEN.length_], r8d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenType], TT_COMMENT
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenMods], 0
    inc  r15d
    jmp  se_sc_loop

se_not_comment:
    ; --- Check for string literals (quoted) ---
    cmp  al, '"'
    jne  se_not_string
    mov  edx, ecx            ; startCol
    mov  qword ptr [rbp-40h], rsi
    inc  rsi
    inc  ecx
se_skip_str:
    mov  rax, rsi
    sub  rax, r12
    cmp  rax, r13
    jge  se_end_str
    movzx eax, byte ptr [rsi]
    cmp  al, '"'
    je   se_end_str_q
    cmp  al, 10
    je   se_end_str
    inc  rsi
    inc  ecx
    jmp  se_skip_str
se_end_str_q:
    inc  rsi                 ; skip closing quote
    inc  ecx
se_end_str:
    mov  rax, qword ptr [rbp-40h]
    mov  r8, rsi
    sub  r8, rax             ; string length
    cmp  r15d, MAX_SEMANTIC_TOKENS
    jge  se_sc_done
    imul eax, r15d, SIZEOF SEMANTIC_TOKEN
    lea  rdi, SemanticPool
    add  rdi, rax
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaLine], r14d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaStart], edx
    mov  dword ptr [rdi + SEMANTIC_TOKEN.length_], r8d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenType], TT_STRLIT
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenMods], 0
    inc  r15d
    jmp  se_sc_loop

se_not_string:
    ; --- Check for numeric literals (0-9 at start) ---
    cmp  al, '0'
    jb   se_not_number
    cmp  al, '9'
    ja   se_not_number
    mov  edx, ecx            ; startCol
    mov  qword ptr [rbp-40h], rsi
se_skip_num:
    inc  rsi
    inc  ecx
    mov  rax, rsi
    sub  rax, r12
    cmp  rax, r13
    jge  se_end_num
    movzx eax, byte ptr [rsi]
    cmp  al, '0'
    jb   se_end_num
    cmp  al, '9'
    jle  se_skip_num
    ; Allow hex digits (a-f, A-F) and 'h' suffix and 'x' prefix
    cmp  al, 'a'
    jb   se_try_upper_hex
    cmp  al, 'f'
    jle  se_skip_num
    cmp  al, 'h'
    je   se_skip_num_last
    jmp  se_end_num
se_try_upper_hex:
    cmp  al, 'A'
    jb   se_end_num
    cmp  al, 'F'
    jle  se_skip_num
    cmp  al, 'H'
    je   se_skip_num_last
    cmp  al, 'x'
    je   se_skip_num
    cmp  al, 'X'
    je   se_skip_num
    jmp  se_end_num
se_skip_num_last:
    inc  rsi
    inc  ecx
se_end_num:
    mov  rax, qword ptr [rbp-40h]
    mov  r8, rsi
    sub  r8, rax
    cmp  r15d, MAX_SEMANTIC_TOKENS
    jge  se_sc_done
    imul eax, r15d, SIZEOF SEMANTIC_TOKEN
    lea  rdi, SemanticPool
    add  rdi, rax
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaLine], r14d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaStart], edx
    mov  dword ptr [rdi + SEMANTIC_TOKEN.length_], r8d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenType], TT_NUMLIT
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenMods], 0
    inc  r15d
    jmp  se_sc_loop

se_not_number:
    ; --- Check for directives (. at start — .DATA, .CODE, .endprolog, etc) ---
    cmp  al, '.'
    jne  se_not_directive
    mov  edx, ecx
    mov  qword ptr [rbp-40h], rsi
se_skip_dir:
    inc  rsi
    inc  ecx
    mov  rax, rsi
    sub  rax, r12
    cmp  rax, r13
    jge  se_end_dir
    movzx eax, byte ptr [rsi]
    call IsIdentifierChar
    test eax, eax
    jnz  se_skip_dir
se_end_dir:
    mov  rax, qword ptr [rbp-40h]
    mov  r8, rsi
    sub  r8, rax
    cmp  r15d, MAX_SEMANTIC_TOKENS
    jge  se_sc_done
    imul eax, r15d, SIZEOF SEMANTIC_TOKEN
    lea  rdi, SemanticPool
    add  rdi, rax
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaLine], r14d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaStart], edx
    mov  dword ptr [rdi + SEMANTIC_TOKEN.length_], r8d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenType], TT_DIRECTIVE
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenMods], 0
    inc  r15d
    jmp  se_sc_loop

se_not_directive:
    ; --- Check for identifiers/keywords/registers (a-z, A-Z, _) ---
    call IsIdentifierChar
    test eax, eax
    jz   se_operator

    ; Collect the full identifier
    movzx eax, byte ptr [rsi]       ; reload (IsIdentifierChar clobbered eax)
    mov  edx, ecx            ; startCol
    mov  qword ptr [rbp-40h], rsi
    xor  ebx, ebx           ; ident length
se_skip_ident:
    inc  rsi
    inc  ecx
    inc  ebx
    mov  rax, rsi
    sub  rax, r12
    cmp  rax, r13
    jge  se_classify_ident
    movzx eax, byte ptr [rsi]
    call IsIdentifierChar
    test eax, eax
    jnz  se_skip_ident

se_classify_ident:
    ; EBX = ident length, [rbp-40h] = start ptr, edx = startCol
    mov  r8d, ebx            ; length
    
    ; --- Check against KeywordTable ---
    mov  qword ptr [rbp-60h], rdx   ; save startCol
    lea  rax, KeywordTable
    mov  qword ptr [rbp-68h], rax   ; kw_table ptr
se_kw_chk:
    mov  rax, qword ptr [rbp-68h]
    mov  rax, qword ptr [rax]
    test rax, rax
    jz   se_not_kw
    ; Compare ident against keyword (case-insensitive, length must match)
    mov  qword ptr [rbp-70h], rax   ; save keyword ptr
    ; Get keyword length
    xor  ecx, ecx
se_kw_len:
    cmp  byte ptr [rax + rcx], 0
    je   se_kw_len_done
    inc  ecx
    jmp  se_kw_len
se_kw_len_done:
    cmp  ecx, ebx
    jne  se_kw_next          ; length mismatch
    ; Compare chars
    mov  rax, qword ptr [rbp-70h]
    mov  rcx, qword ptr [rbp-40h]
    xor  edi, edi
se_kw_cmp:
    cmp  edi, ebx
    jge  se_is_keyword
    movzx r9d, byte ptr [rax + rdi]
    movzx r10d, byte ptr [rcx + rdi]
    or   r9d, 20h
    or   r10d, 20h
    cmp  r9d, r10d
    jne  se_kw_next
    inc  edi
    jmp  se_kw_cmp
se_is_keyword:
    ; It's a keyword → emit TT_KEYWORD
    mov  edx, dword ptr [rbp-60h]
    cmp  r15d, MAX_SEMANTIC_TOKENS
    jge  se_sc_done
    imul eax, r15d, SIZEOF SEMANTIC_TOKEN
    lea  rdi, SemanticPool
    add  rdi, rax
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaLine], r14d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaStart], edx
    mov  dword ptr [rdi + SEMANTIC_TOKEN.length_], r8d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenType], TT_KEYWORD
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenMods], 0
    inc  r15d
    jmp  se_sc_loop
se_kw_next:
    add  qword ptr [rbp-68h], 8
    jmp  se_kw_chk

se_not_kw:
    ; --- Check for common registers (rax-r15, eax-r15d, al-r15b, etc) ---
    ; Simple heuristic: 2-4 char idents starting with r/e/a-d and ending with x/i/p/l/d/w/b
    mov  edx, dword ptr [rbp-60h]   ; restore startCol
    mov  rax, qword ptr [rbp-40h]
    movzx ecx, byte ptr [rax]
    or   ecx, 20h
    ; r-prefix registers (rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8-r15)
    cmp  cl, 'r'
    je   se_maybe_reg
    ; e-prefix registers (eax, ebx, ecx, edx, esi, edi, ebp, esp)
    cmp  cl, 'e'
    je   se_maybe_reg
    ; a-d single byte: al, ah, bl, bh, cl, ch, dl, dh
    cmp  ebx, 2
    jne  se_is_ident
    cmp  cl, 'a'
    jb   se_is_ident
    cmp  cl, 'd'
    ja   se_check_si_di
    movzx ecx, byte ptr [rax+1]
    or   ecx, 20h
    cmp  cl, 'l'
    je   se_emit_reg
    cmp  cl, 'h'
    je   se_emit_reg
    cmp  cl, 'x'
    je   se_emit_reg
    jmp  se_is_ident
se_check_si_di:
    ; si, di, sp, bp (2-char)
    cmp  cl, 's'
    je   se_maybe_2reg
    cmp  cl, 'd'
    je   se_maybe_2reg
    cmp  cl, 'b'
    je   se_maybe_2reg
    jmp  se_is_ident
se_maybe_2reg:
    movzx ecx, byte ptr [rax+1]
    or   ecx, 20h
    cmp  cl, 'i'
    je   se_emit_reg
    cmp  cl, 'p'
    je   se_emit_reg
    jmp  se_is_ident

se_maybe_reg:
    ; r/e prefix — check length 2-5 and common suffixes
    cmp  ebx, 2
    jb   se_is_ident
    cmp  ebx, 5
    ja   se_is_ident
    ; If starts with 'r' and second char is digit → r8-r15
    movzx ecx, byte ptr [rax+1]
    cmp  cl, '0'
    jb   se_check_rname
    cmp  cl, '9'
    jle  se_emit_reg
se_check_rname:
    or   ecx, 20h
    cmp  cl, 'a'
    je   se_emit_reg
    cmp  cl, 'b'
    je   se_emit_reg
    cmp  cl, 'c'
    je   se_emit_reg
    cmp  cl, 'd'
    je   se_emit_reg
    cmp  cl, 's'
    je   se_emit_reg
    jmp  se_is_ident

se_emit_reg:
    mov  edx, dword ptr [rbp-60h]
    cmp  r15d, MAX_SEMANTIC_TOKENS
    jge  se_sc_done
    imul eax, r15d, SIZEOF SEMANTIC_TOKEN
    lea  rdi, SemanticPool
    add  rdi, rax
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaLine], r14d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaStart], edx
    mov  dword ptr [rdi + SEMANTIC_TOKEN.length_], r8d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenType], TT_REGISTER
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenMods], 0
    inc  r15d
    jmp  se_sc_loop

se_is_ident:
    ; Generic identifier
    mov  edx, dword ptr [rbp-60h]
    cmp  r15d, MAX_SEMANTIC_TOKENS
    jge  se_sc_done
    imul eax, r15d, SIZEOF SEMANTIC_TOKEN
    lea  rdi, SemanticPool
    add  rdi, rax
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaLine], r14d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaStart], edx
    mov  dword ptr [rdi + SEMANTIC_TOKEN.length_], r8d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenType], TT_IDENTIFIER
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenMods], 0
    inc  r15d
    jmp  se_sc_loop

se_operator:
    ; Anything else that isn't whitespace → operator
    cmp  al, ' '
    je   se_default_advance
    cmp  al, 9
    je   se_default_advance
    cmp  al, 13
    je   se_default_advance

    cmp  r15d, MAX_SEMANTIC_TOKENS
    jge  se_sc_done
    imul eax, r15d, SIZEOF SEMANTIC_TOKEN
    lea  rdi, SemanticPool
    add  rdi, rax
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaLine], r14d
    mov  dword ptr [rdi + SEMANTIC_TOKEN.deltaStart], ecx
    mov  dword ptr [rdi + SEMANTIC_TOKEN.length_], 1
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenType], TT_OPERATOR
    mov  dword ptr [rdi + SEMANTIC_TOKEN.tokenMods], 0
    inc  r15d

se_default_advance:
    inc  ecx
    inc  rsi
    jmp  se_sc_loop

se_sc_done:
    mov  SemanticCount, r15d
    mov  eax, r15d

    ; Clean up AVX state
    vzeroupper

    mov  rbx, qword ptr [rbp-38h]    ; restore rbx
    mov  rdi, qword ptr [rbp-30h]    ; restore rdi
    mov  rsi, qword ptr [rbp-28h]    ; restore rsi
    mov  r15, qword ptr [rbp-20h]
    mov  r14, qword ptr [rbp-18h]
    mov  r13, qword ptr [rbp-10h]
    mov  r12, qword ptr [rbp-8h]
    leave
    ret

; Inline broadcast constants (1-byte each)
ALIGN 16
se_space        BYTE 20h
se_semicolon    BYTE ';'
se_newline      BYTE 0Ah

Widget_SemanticEngine ENDP

;------------------------------------------------------------------------------
; Widget_SymbolEngine — extract PROC/ENDP/STRUCT symbols from code buffer
; RCX = code buffer, RDX = length
;------------------------------------------------------------------------------
Widget_SymbolEngine PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 100h
    .allocstack 100h
    mov  qword ptr [rbp-8h], r12
    .savereg r12, 0F8h
    mov  qword ptr [rbp-10h], r13
    .savereg r13, 0F0h
    mov  qword ptr [rbp-18h], r14
    .savereg r14, 0E8h
    mov  qword ptr [rbp-20h], rsi      ; save non-volatile rsi
    .savereg rsi, 0E0h
    mov  qword ptr [rbp-28h], rdi      ; save non-volatile rdi
    .savereg rdi, 0D8h
    .endprolog

    mov  r12, rcx           ; code buffer
    mov  r13, rdx           ; length
    mov  SymbolCount, 0

    mov  rsi, r12
    xor  r14d, r14d         ; line counter

sy_line:
    mov  rax, rsi
    sub  rax, r12
    cmp  rax, r13
    jge  sy_done

    inc  r14d

    ; Skip leading whitespace
sy_ws:
    cmp  byte ptr [rsi], ' '
    je   sy_skip_ws
    cmp  byte ptr [rsi], 9   ; tab
    je   sy_skip_ws
    jmp  sy_check
sy_skip_ws:
    inc  rsi
    jmp  sy_ws

sy_check:
    ; Read identifier
    mov  rdi, rsi            ; start of token
    xor  ecx, ecx           ; token length

sy_ident:
    movzx eax, byte ptr [rsi + rcx]
    cmp  al, ' '
    je   sy_end_ident
    cmp  al, 9
    je   sy_end_ident
    cmp  al, 13
    je   sy_end_ident
    cmp  al, 10
    je   sy_end_ident
    cmp  al, 0
    je   sy_end_ident
    inc  ecx
    cmp  ecx, 32
    jge  sy_end_ident
    jmp  sy_ident

sy_end_ident:
    ; Check if next non-ws word is "PROC", "ENDP", "STRUCT"
    push rdi
    push rcx

    ; Advance rsi past identifier + whitespace
    lea  rax, [rdi + rcx]
    mov  rsi, rax
sy_skip2:
    cmp  byte ptr [rsi], ' '
    je   sy_skip2b
    cmp  byte ptr [rsi], 9
    je   sy_skip2b
    jmp  sy_check_kw
sy_skip2b:
    inc  rsi
    jmp  sy_skip2

sy_check_kw:
    ; Compare with "PROC" (4 chars)
    cmp  dword ptr [rsi], 'P' OR ('R' SHL 8) OR ('O' SHL 16) OR ('C' SHL 24)
    je   sy_found_proc
    ; Compare with "ENDP" (4 chars)
    cmp  dword ptr [rsi], 'E' OR ('N' SHL 8) OR ('D' SHL 16) OR ('P' SHL 24)
    je   sy_found_endp
    ; Compare with "STRU" (first 4 of STRUCT)
    cmp  dword ptr [rsi], 'S' OR ('T' SHL 8) OR ('R' SHL 16) OR ('U' SHL 24)
    jne  sy_no_match

    ; Verify "CT" follows
    cmp  word ptr [rsi+4], 'C' OR ('T' SHL 8)
    jne  sy_no_match
    ; STRUCT found → add symbol kind=23 (Struct)
    pop  rcx
    pop  rdi
    mov  edx, 23
    jmp  sy_add_symbol

sy_found_proc:
    pop  rcx
    pop  rdi
    mov  edx, 12             ; SymbolKind.Function
    jmp  sy_add_symbol

sy_found_endp:
    ; ENDP → skip, don't double-add
    pop  rcx
    pop  rdi
    jmp  sy_next_line

sy_add_symbol:
    ; RDI=name start, ECX=name len, EDX=kind, R14D=line
    mov  eax, SymbolCount
    cmp  eax, MAX_SYMBOLS
    jge  sy_done

    push rdx
    imul eax, SIZEOF SYMBOL_ENTRY
    lea  r8, SymbolPool
    add  r8, rax

    ; Copy name (up to 31 chars + NUL)
    push rdi
    push rcx
    mov  rsi, rdi
    lea  rdi, [r8 + SYMBOL_ENTRY.sname]
    cmp  ecx, 31
    jle  @F
    mov  ecx, 31
@@:
    rep  movsb
    mov  byte ptr [rdi], 0
    pop  rcx
    pop  rdi
    pop  rdx

    mov  dword ptr [r8 + SYMBOL_ENTRY.kind], edx
    mov  dword ptr [r8 + SYMBOL_ENTRY.startLine], r14d

    ; --- Compute endLine by scanning for matching ENDP ---
    ; Save current scan state
    push rsi
    push r14
    mov  dword ptr [r8 + SYMBOL_ENTRY.endLine], r14d  ; default: same line

    ; Scan forward line-by-line for "<name> ENDP"
sy_find_endp:
    mov  rax, rsi
    sub  rax, r12
    cmp  rax, r13
    jge  sy_endp_notfound
    cmp  byte ptr [rsi], 10
    jne  sy_fe_skip
    inc  rsi
    inc  r14d
    ; Check if this line starts with the symbol name followed by ENDP
    ; Skip whitespace
    push rsi
sy_fe_ws:
    cmp  byte ptr [rsi], ' '
    je   sy_fe_ws2
    cmp  byte ptr [rsi], 9
    je   sy_fe_ws2
    jmp  sy_fe_check
sy_fe_ws2:
    inc  rsi
    jmp  sy_fe_ws
sy_fe_check:
    ; Compare first token with our symbol name (pointer in [r8+sname])
    lea  rax, [r8 + SYMBOL_ENTRY.sname]
    xor  ecx, ecx
sy_fe_name_cmp:
    mov  dl, byte ptr [rax + rcx]
    test dl, dl
    jz   sy_fe_name_end  ; name exhausted → check separator
    cmp  dl, byte ptr [rsi + rcx]
    jne  sy_fe_nomatch
    inc  ecx
    cmp  ecx, 31
    jge  sy_fe_nomatch
    jmp  sy_fe_name_cmp
sy_fe_name_end:
    ; After name, skip whitespace, then check for "ENDP"
    add  rsi, rcx
sy_fe_ws3:
    cmp  byte ptr [rsi], ' '
    je   sy_fe_ws4
    cmp  byte ptr [rsi], 9
    je   sy_fe_ws4
    jmp  sy_fe_endp_check
sy_fe_ws4:
    inc  rsi
    jmp  sy_fe_ws3
sy_fe_endp_check:
    cmp  dword ptr [rsi], 'E' OR ('N' SHL 8) OR ('D' SHL 16) OR ('P' SHL 24)
    jne  sy_fe_nomatch
    ; Found matching ENDP
    pop  rsi   ; discard saved rsi
    mov  dword ptr [r8 + SYMBOL_ENTRY.endLine], r14d
    jmp  sy_endp_found
sy_fe_nomatch:
    pop  rsi   ; restore line-start rsi
    jmp  sy_find_endp
sy_fe_skip:
    inc  rsi
    jmp  sy_find_endp

sy_endp_notfound:
sy_endp_found:
    pop  r14           ; restore original line counter
    pop  rsi           ; restore original scan position

    ; --- Populate detail field with kind string ---
    push rdi
    push rcx
    lea  rdi, [r8 + SYMBOL_ENTRY.detail]
    cmp  dword ptr [r8 + SYMBOL_ENTRY.kind], 12
    jne  sy_detail_struct
    ; "PROC" + NUL
    mov  dword ptr [rdi], 'P' OR ('R' SHL 8) OR ('O' SHL 16) OR ('C' SHL 24)
    mov  byte ptr [rdi+4], 0
    jmp  sy_detail_done
sy_detail_struct:
    cmp  dword ptr [r8 + SYMBOL_ENTRY.kind], 23
    jne  sy_detail_other
    mov  dword ptr [rdi],   'S' OR ('T' SHL 8) OR ('R' SHL 16) OR ('U' SHL 24)
    mov  word ptr  [rdi+4], 'C' OR ('T' SHL 8)
    mov  byte ptr  [rdi+6], 0
    jmp  sy_detail_done
sy_detail_other:
    mov  byte ptr [rdi], 0
sy_detail_done:
    pop  rcx
    pop  rdi

    mov  dword ptr [r8 + SYMBOL_ENTRY.containerIdx], -1

    inc  SymbolCount
    jmp  sy_next_line

sy_no_match:
    pop  rcx
    pop  rdi

sy_next_line:
    ; Advance to next line
sy_skip_line:
    mov  rax, rsi
    sub  rax, r12
    cmp  rax, r13
    jge  sy_done
    cmp  byte ptr [rsi], 10
    je   sy_got_nl
    inc  rsi
    jmp  sy_skip_line
sy_got_nl:
    inc  rsi
    jmp  sy_line

sy_done:
    mov  eax, SymbolCount

    mov  rdi, qword ptr [rbp-28h]    ; restore rdi
    mov  rsi, qword ptr [rbp-20h]    ; restore rsi
    mov  r14, qword ptr [rbp-18h]
    mov  r13, qword ptr [rbp-10h]
    mov  r12, qword ptr [rbp-8h]
    leave
    ret
Widget_SymbolEngine ENDP

;------------------------------------------------------------------------------
; Widget_FoldingEngine — detect foldable regions (PROC→ENDP, STRUCT→ENDS)
; RCX = code buffer, RDX = length
;------------------------------------------------------------------------------
Widget_FoldingEngine PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 100h
    .allocstack 100h
    mov  qword ptr [rbp-8h], r12
    .savereg r12, 0F8h
    mov  qword ptr [rbp-10h], r13
    .savereg r13, 0F0h
    .endprolog

    mov  r12, rcx
    mov  r13, rdx
    mov  FoldingCount, 0

    ; Use symbol engine results as basis for folding
    ; Walk SymbolPool: every Function (kind=12) at startLine → fold to next ENDP line
    xor  ecx, ecx           ; symbol index

fe_loop:
    cmp  ecx, SymbolCount
    jge  fe_done

    mov  eax, ecx
    imul eax, SIZEOF SYMBOL_ENTRY
    lea  rdx, SymbolPool
    add  rdx, rax

    cmp  dword ptr [rdx + SYMBOL_ENTRY.kind], 12    ; Function?
    jne  fe_next

    ; Add folding range
    mov  eax, FoldingCount
    cmp  eax, MAX_FOLDING_RANGES
    jge  fe_done

    imul eax, SIZEOF FOLDING_RANGE
    lea  r8, FoldingPool
    add  r8, rax

    mov  eax, dword ptr [rdx + SYMBOL_ENTRY.startLine]
    mov  dword ptr [r8 + FOLDING_RANGE.startLn], eax
    ; Use real endLine from SymbolEngine (computed by ENDP scan)
    mov  eax, dword ptr [rdx + SYMBOL_ENTRY.endLine]
    cmp  eax, dword ptr [rdx + SYMBOL_ENTRY.startLine]
    jg   fe_endline_ok
    ; Fallback: if endLine == startLine (ENDP not found), use startLine+10
    mov  eax, dword ptr [rdx + SYMBOL_ENTRY.startLine]
    add  eax, 10
fe_endline_ok:
    mov  dword ptr [r8 + FOLDING_RANGE.endLn], eax
    mov  dword ptr [r8 + FOLDING_RANGE.kind], 3     ; region
    mov  dword ptr [r8 + FOLDING_RANGE.pad0], 0

    inc  FoldingCount

fe_next:
    inc  ecx
    jmp  fe_loop

fe_done:
    mov  eax, FoldingCount

    mov  r13, qword ptr [rbp-10h]
    mov  r12, qword ptr [rbp-8h]
    leave
    ret
Widget_FoldingEngine ENDP

;------------------------------------------------------------------------------
; Widget_ReferenceEngine — find all occurrences of an identifier
; RCX = code buffer, RDX = length, R8 = pIdentifier
;------------------------------------------------------------------------------
Widget_ReferenceEngine PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 100h
    .allocstack 100h
    mov  qword ptr [rbp-8h], r12
    .savereg r12, 0F8h
    mov  qword ptr [rbp-10h], r13
    .savereg r13, 0F0h
    mov  qword ptr [rbp-18h], r14
    .savereg r14, 0E8h
    mov  qword ptr [rbp-20h], r15
    .savereg r15, 0E0h
    mov  qword ptr [rbp-28h], rsi      ; save non-volatile rsi
    .savereg rsi, 0D8h
    mov  qword ptr [rbp-30h], rdi      ; save non-volatile rdi
    .savereg rdi, 0D0h
    .endprolog

    mov  r12, rcx           ; code buffer
    mov  r13, rdx           ; length
    mov  r14, r8            ; identifier to find
    mov  ReferenceCount, 0

    ; Get identifier length
    mov  rcx, r14
    xor  r15d, r15d
re_idlen:
    cmp  byte ptr [rcx + r15], 0
    je   re_idlen_done
    inc  r15d
    jmp  re_idlen
re_idlen_done:

    mov  rsi, r12
    xor  ecx, ecx           ; line (1-based)
    xor  edx, edx           ; column (0-based)

re_scan:
    mov  rax, rsi
    sub  rax, r12
    cmp  rax, r13
    jge  re_done

    cmp  byte ptr [rsi], 10
    jne  re_not_nl
    inc  ecx
    xor  edx, edx
    inc  rsi
    jmp  re_scan
re_not_nl:
    ; Try to match identifier at this position
    push rcx
    push rdx
    push rsi

    xor  edi, edi
re_cmp:
    cmp  edi, r15d
    jge  re_match
    movzx eax, byte ptr [rsi + rdi]
    movzx r8d, byte ptr [r14 + rdi]
    cmp  al, r8b
    jne  re_no_match
    inc  edi
    jmp  re_cmp

re_match:
    ; Verify not part of larger identifier — word boundary check
    pop  rsi
    pop  rdx
    pop  rcx

    ; Check char BEFORE match (must be non-ident or at buffer start)
    cmp  rsi, r12
    je   re_boundary_ok       ; at buffer start → OK
    movzx eax, byte ptr [rsi - 1]
    ; Is it alphanumeric or underscore?
    cmp  al, '_'
    je   re_boundary_fail
    cmp  al, '0'
    jb   re_chk_before_alpha
    cmp  al, '9'
    jle  re_boundary_fail     ; digit before → part of larger token
re_chk_before_alpha:
    or   al, 20h
    cmp  al, 'a'
    jb   re_chk_after
    cmp  al, 'z'
    jle  re_boundary_fail     ; letter before → part of larger ident

re_chk_after:
    ; Check char AFTER match (must be non-ident or at buffer end)
    lea  rax, [rsi + r15]
    sub  rax, r12
    cmp  rax, r13
    jge  re_boundary_ok       ; at buffer end → OK
    movzx eax, byte ptr [rsi + r15]
    cmp  al, '_'
    je   re_boundary_fail
    cmp  al, '0'
    jb   re_chk_after_alpha
    cmp  al, '9'
    jle  re_boundary_fail
re_chk_after_alpha:
    or   al, 20h
    cmp  al, 'a'
    jb   re_boundary_ok
    cmp  al, 'z'
    jle  re_boundary_fail

re_boundary_ok:
    mov  eax, ReferenceCount
    cmp  eax, MAX_REFERENCES
    jge  re_done

    imul eax, SIZEOF REFERENCE_ENTRY
    lea  rdi, ReferencePool
    add  rdi, rax

    mov  dword ptr [rdi + REFERENCE_ENTRY.refLine], ecx
    mov  dword ptr [rdi + REFERENCE_ENTRY.refCol], edx
    mov  dword ptr [rdi + REFERENCE_ENTRY.refLen], r15d

    ; --- Detect if this is a declaration ---
    ; Check if "PROC" or "ENDP" follows the identifier on this line
    mov  dword ptr [rdi + REFERENCE_ENTRY.isDecl], 0
    push rsi
    lea  rax, [rsi + r15]    ; position after the match
re_decl_ws:
    movzx r8d, byte ptr [rax]
    cmp  r8b, ' '
    je   re_decl_ws2
    cmp  r8b, 9
    je   re_decl_ws2
    jmp  re_decl_check
re_decl_ws2:
    inc  rax
    jmp  re_decl_ws
re_decl_check:
    cmp  dword ptr [rax], 'P' OR ('R' SHL 8) OR ('O' SHL 16) OR ('C' SHL 24)
    je   re_is_decl
    cmp  dword ptr [rax], 'S' OR ('T' SHL 8) OR ('R' SHL 16) OR ('U' SHL 24)
    je   re_is_decl
    cmp  dword ptr [rax], 'E' OR ('N' SHL 8) OR ('D' SHL 16) OR ('P' SHL 24)
    je   re_is_decl
    jmp  re_not_decl
re_is_decl:
    mov  dword ptr [rdi + REFERENCE_ENTRY.isDecl], 1
re_not_decl:
    pop  rsi

    inc  ReferenceCount

    add  rsi, r15            ; skip past match
    add  edx, r15d
    jmp  re_scan

re_boundary_fail:
    inc  rsi
    inc  edx
    jmp  re_scan

re_no_match:
    pop  rsi
    pop  rdx
    pop  rcx
    inc  rsi
    inc  edx
    jmp  re_scan

re_done:
    mov  eax, ReferenceCount

    mov  rdi, qword ptr [rbp-30h]    ; restore rdi
    mov  rsi, qword ptr [rbp-28h]    ; restore rsi
    mov  r15, qword ptr [rbp-20h]
    mov  r14, qword ptr [rbp-18h]
    mov  r13, qword ptr [rbp-10h]
    mov  r12, qword ptr [rbp-8h]
    leave
    ret
Widget_ReferenceEngine ENDP

;------------------------------------------------------------------------------
; Widget_DispatchCommand — route pipe command byte to engine
; RCX = command buffer (first byte = opcode, rest = payload)
; RDX = bytes read
; Returns: RAX = result count, g_ResponseBuf filled
;------------------------------------------------------------------------------
Widget_DispatchCommand PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 80h
    .allocstack 80h
    mov  qword ptr [rbp-8h], r12
    .savereg r12, 78h
    .endprolog

    movzx r12d, byte ptr [rcx]   ; opcode
    mov  qword ptr [rbp-10h], rcx ; save buffer ptr to local
    mov  qword ptr [rbp-18h], rdx ; save length to local

    ; Print dispatch info
    lea  rcx, szDispatch
    call HL_Print

    mov  rdx, qword ptr [rbp-18h] ; restore length
    mov  rcx, qword ptr [rbp-10h] ; restore buffer ptr

    ; Route based on opcode
    cmp  r12d, CMD_COMPLETION
    je   dc_completion
    cmp  r12d, CMD_DIAGNOSTICS
    je   dc_diagnostics
    cmp  r12d, CMD_SEMANTIC_TOKENS
    je   dc_semantic
    cmp  r12d, CMD_SYMBOLS
    je   dc_symbols
    cmp  r12d, CMD_FOLDING
    je   dc_folding
    cmp  r12d, CMD_REFERENCES
    je   dc_references
    cmp  r12d, CMD_SHUTDOWN
    je   dc_shutdown
    jmp  dc_unknown

dc_completion:
    mov  rcx, pCodeView
    test rcx, rcx
    jz   dc_err
    xor  edx, edx           ; cursor offset 0
    call Widget_CompletionEngine
    jmp  dc_ok

dc_diagnostics:
    mov  rcx, pCodeView
    test rcx, rcx
    jz   dc_err
    mov  rdx, SHARED_CODE_SIZE
    call Widget_DiagnosticsEngine
    jmp  dc_ok

dc_semantic:
    mov  rcx, pCodeView
    test rcx, rcx
    jz   dc_err
    mov  rdx, SHARED_CODE_SIZE
    call Widget_SemanticEngine
    jmp  dc_ok

dc_symbols:
    mov  rcx, pCodeView
    test rcx, rcx
    jz   dc_err
    mov  rdx, SHARED_CODE_SIZE
    call Widget_SymbolEngine
    jmp  dc_ok

dc_folding:
    mov  rcx, pCodeView
    test rcx, rcx
    jz   dc_err
    mov  rdx, SHARED_CODE_SIZE
    call Widget_FoldingEngine
    jmp  dc_ok

dc_references:
    ; Payload after opcode = identifier string
    mov  rcx, pCodeView
    test rcx, rcx
    jz   dc_err
    mov  rdx, SHARED_CODE_SIZE
    ; Format: [opcode:1 byte][identifier:null-terminated string]
    mov  rax, qword ptr [rbp-10h]    ; saved buffer ptr
    lea  r8, [rax+1]                  ; skip opcode byte -> identifier
    call Widget_ReferenceEngine
    jmp  dc_ok

dc_shutdown:
    mov  bActive, 0
    xor  eax, eax
    jmp  dc_exit

dc_ok:
    ; Write "OK" to response
    lea  rcx, g_ResponseBuf
    mov  word ptr [rcx], 'O' OR ('K' SHL 8)
    mov  byte ptr [rcx+2], 0
    jmp  dc_exit

dc_err:
    lea  rcx, g_ResponseBuf
    mov  dword ptr [rcx], 'E' OR ('R' SHL 8) OR ('R' SHL 16)
    mov  byte ptr [rcx+3], 0
    xor  eax, eax

dc_exit:
    mov  r12, qword ptr [rbp-8h]     ; restore r12
    leave
    ret

dc_unknown:
    ; Unknown command → write error response
    lea  rcx, g_ResponseBuf
    mov  dword ptr [rcx],   'E' OR ('R' SHL 8) OR ('R' SHL 16) OR (':' SHL 24)
    mov  dword ptr [rcx+4], 'U' OR ('N' SHL 8) OR ('K' SHL 16) OR ('N' SHL 24)
    mov  dword ptr [rcx+8], 'O' OR ('W' SHL 8) OR ('N' SHL 16)
    mov  byte ptr  [rcx+11], 0
    xor  eax, eax
    mov  r12, qword ptr [rbp-8h]
    leave
    ret
Widget_DispatchCommand ENDP

;------------------------------------------------------------------------------
; Widget_ServiceLoop — main pipe service loop
;------------------------------------------------------------------------------
Widget_ServiceLoop PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 2100h
    .allocstack 2100h
    mov  qword ptr [rbp-8h], r12
    .savereg r12, 20F8h
    mov  qword ptr [rbp-20h], rdi      ; save non-volatile rdi
    .savereg rdi, 20E0h
    .endprolog

    mov  dword ptr [rbp-28h], 100      ; retry backoff (ms)

sl_loop:
    cmp  bActive, 0
    je   sl_exit

    ; Create named pipe
    lea  rcx, szPipeName
    mov  edx, PIPE_ACCESS_DUPLEX
    mov  r8d, PIPE_TYPE_MESSAGE OR PIPE_READMODE_MESSAGE OR PIPE_WAIT
    mov  r9d, PIPE_UNLIMITED_INSTANCES
    mov  qword ptr [rsp+20h], PIPE_BUF_SIZE
    mov  qword ptr [rsp+28h], PIPE_BUF_SIZE
    mov  qword ptr [rsp+30h], 0
    mov  qword ptr [rsp+38h], 0
    call CreateNamedPipeA
    cmp  rax, INVALID_HANDLE_VALUE
    jne  sl_pipe_ok

    ; Pipe creation failed — backoff then retry
    mov  ecx, dword ptr [rbp-28h]     ; current backoff
    call Sleep
    ; Double backoff up to 5000ms
    mov  eax, dword ptr [rbp-28h]
    shl  eax, 1
    cmp  eax, 5000
    jle  @F
    mov  eax, 5000
@@:
    mov  dword ptr [rbp-28h], eax
    jmp  sl_loop

sl_pipe_ok:
    mov  dword ptr [rbp-28h], 100     ; reset backoff on success
    mov  hPipe, rax

    ; Wait for client
    mov  rcx, rax
    xor  edx, edx
    call ConnectNamedPipe
    test eax, eax
    jz   sl_disconnect         ; ConnectNamedPipe failed → cleanup

    lea  rcx, szConnected
    call HL_Print

    ; Read command
    mov  rcx, hPipe
    lea  rdx, g_PipeBuf
    mov  r8d, PIPE_BUF_SIZE
    lea  r9, [rbp-10h]
    mov  qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz   sl_disconnect

    ; Dispatch
    lea  rcx, g_PipeBuf
    mov  edx, dword ptr [rbp-10h]    ; bytes read
    call Widget_DispatchCommand

    ; Send response
    lea  rcx, g_ResponseBuf
    mov  rdi, rcx
    xor  ecx, ecx
    dec  rcx
    xor  eax, eax
    repne scasb
    not  rcx
    dec  rcx
    mov  r8d, ecx           ; response length

    mov  rcx, hPipe
    lea  rdx, g_ResponseBuf
    ; r8d already set
    lea  r9, [rbp-18h]
    mov  qword ptr [rsp+20h], 0
    call WriteFile

sl_disconnect:
    mov  rcx, hPipe
    call DisconnectNamedPipe
    mov  rcx, hPipe
    call CloseHandle

    jmp  sl_loop

sl_exit:
    mov  rdi, qword ptr [rbp-20h]    ; restore rdi
    mov  r12, qword ptr [rbp-8h]     ; restore r12
    leave
    ret
Widget_ServiceLoop ENDP

;------------------------------------------------------------------------------
; HeadlessMain — entry point
;------------------------------------------------------------------------------
HeadlessMain PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    .endprolog

    ; Banner
    lea  rcx, szWelcome
    call HL_Print

    ; Create private heap
    xor  ecx, ecx
    mov  edx, 100000h       ; 1 MB
    xor  r8d, r8d
    call HeapCreate
    test rax, rax
    jz   hm_no_shm           ; heap creation failed → skip
    mov  hHeap, rax

    ; Set up shared memory regions
    call Widget_CreateSharedMemory
    test eax, eax
    jz   hm_no_shm

    ; Enter service loop (blocks)
    call Widget_ServiceLoop

hm_no_shm:
    ; Cleanup — unmap views
    mov  rcx, pCodeView
    test rcx, rcx
    jz   @F
    call UnmapViewOfFile
@@:
    mov  rcx, pStateView
    test rcx, rcx
    jz   @F
    call UnmapViewOfFile
@@:
    mov  rcx, hCodeMapping
    test rcx, rcx
    jz   @F
    call CloseHandle
@@:
    mov  rcx, hStateMapping
    test rcx, rcx
    jz   @F
    call CloseHandle
@@:
    mov  rcx, hHeap
    call HeapDestroy

    lea  rcx, szShutdown
    call HL_Print

    xor  ecx, ecx
    call ExitProcess
    ; unreachable — ExitProcess does not return
HeadlessMain ENDP

END
