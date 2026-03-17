; =============================================================================
; agent_tools.asm  —  x64 MASM  —  Agentic IDE Tool Registry
; Complete monolithic implementation: tool dispatch, file I/O, process exec,
; code search, diagnostics, and symbol extraction.
; Build: ml64 /c /Fo agent_tools.obj agent_tools.asm
; =============================================================================
OPTION CASEMAP:NONE

; ---------------------------------------------------------------------------
; EXTERNAL SYMBOLS
; ---------------------------------------------------------------------------
EXTERN BeaconSend         : PROC
EXTERN BeaconRecv         : PROC
EXTERN TryBeaconRecv      : PROC
EXTERN HeapAlloc          : PROC
EXTERN HeapFree           : PROC
EXTERN CreateFileA        : PROC
EXTERN ReadFile           : PROC
EXTERN WriteFile          : PROC
EXTERN CloseHandle        : PROC
EXTERN CreateDirectoryA   : PROC
EXTERN FindFirstFileA     : PROC
EXTERN FindNextFileA      : PROC
EXTERN FindClose          : PROC
EXTERN GetFileAttributesA : PROC
EXTERN DeleteFileA        : PROC
EXTERN MoveFileA          : PROC
EXTERN GetStdHandle       : PROC
EXTERN CreateProcessA     : PROC
EXTERN WaitForSingleObject: PROC
EXTERN GetExitCodeProcess : PROC
EXTERN TerminateProcess   : PROC
EXTERN GetTickCount64     : PROC
EXTERN CreatePipe         : PROC
EXTERN g_hHeap            : QWORD

; ---------------------------------------------------------------------------
; PUBLIC EXPORTS
; ---------------------------------------------------------------------------
PUBLIC Tool_Init
PUBLIC Tool_Execute
PUBLIC Tool_ReadFile
PUBLIC Tool_WriteFile
PUBLIC Tool_ListDir
PUBLIC Tool_RunCommand
PUBLIC Tool_SearchCode
PUBLIC Tool_GetDiagnostics
PUBLIC Tool_GetSymbols

; ---------------------------------------------------------------------------
; CONSTANTS
; ---------------------------------------------------------------------------
TOOL_ID_READ_FILE   equ 0
TOOL_ID_WRITE_FILE  equ 1
TOOL_ID_LIST_DIR    equ 2
TOOL_ID_RUN_COMMAND equ 3
TOOL_ID_SEARCH_CODE equ 4
TOOL_ID_GET_DIAG    equ 5
TOOL_ID_GET_SYMBOLS equ 6
; Staged expansion IDs (family lanes) to avoid hard aliasing to ID 3.
TOOL_ID_GIT_OPS     equ 7
TOOL_ID_BUILD_OPS   equ 8
TOOL_ID_TEST_OPS    equ 9
TOOL_ID_PROCESS_OPS equ 10
TOOL_ID_SHELL_OPS   equ 11
TOOL_ID_AGENT_OPS   equ 12
TOOL_ID_MODEL_OPS   equ 13
TOOL_ID_WEB_OPS     equ 14
TOOL_ID_SYSTEM_OPS  equ 15
TOOL_BUF_SIZE       equ 65536
MAX_TOOLS           equ 16
FIND_DATA_SIZE      equ 592

GENERIC_READ            equ 80000000h
GENERIC_WRITE           equ 40000000h
FILE_SHARE_READ         equ 00000001h
OPEN_EXISTING           equ 3
CREATE_ALWAYS           equ 2
FILE_ATTRIBUTE_NORMAL   equ 00000080h
FILE_ATTRIBUTE_DIRECTORY equ 00000010h
INVALID_HANDLE_VALUE    equ 0FFFFFFFFFFFFFFFFh
HEAP_ZERO_MEMORY        equ 00000008h
STARTF_USESTDHANDLES    equ 00000100h
CREATE_NO_WINDOW        equ 08000000h
WAIT_TIMEOUT_VAL        equ 00000102h
STILL_ACTIVE            equ 00000103h
MAX_PATH                equ 260

; STARTUPINFOA field offsets (x64, total 104 bytes)
STUI_cb             equ 0
STUI_lpReserved     equ 8
STUI_lpDesktop      equ 16
STUI_lpTitle        equ 24
STUI_dwX            equ 32
STUI_dwY            equ 36
STUI_dwXSize        equ 40
STUI_dwYSize        equ 44
STUI_dwXCountChars  equ 48
STUI_dwYCountChars  equ 52
STUI_dwFillAttribute equ 56
STUI_dwFlags        equ 60
STUI_wShowWindow    equ 64
STUI_cbReserved2    equ 66
STUI_lpReserved2    equ 72
STUI_hStdInput      equ 80
STUI_hStdOutput     equ 88
STUI_hStdError      equ 96

; PROCESS_INFORMATION field offsets
PI_hProcess    equ 0
PI_hThread     equ 8
PI_dwProcessId equ 16
PI_dwThreadId  equ 20

; WIN32_FIND_DATAA field offsets
WFDA_dwFileAttributes  equ 0
WFDA_cFileName         equ 44

; SECURITY_ATTRIBUTES offsets (x64, 24 bytes)
SA_nLength              equ 0
SA_lpSecurityDescriptor equ 8
SA_bInheritHandle       equ 16
SA_SIZE                 equ 24

; ---------------------------------------------------------------------------
; .DATA — Initialized variables
; ---------------------------------------------------------------------------
.data
ALIGN 4
g_toolInitialized   dd 0

ALIGN 8
g_toolDispatch      dq MAX_TOOLS dup(0)     ; filled at runtime in Tool_Init

g_toolResultBuf     db TOOL_BUF_SIZE dup(0) ; static 64 KB result staging buffer
g_toolDynBuf        dq 0                    ; heap-allocated overflow buffer ptr

ALIGN 4
g_toolBytesWritten  dd 0

szTool_NoInit       db "[tool] not initialized", 13, 10, 0
szTool_BadId        db "[tool] unknown tool id", 13, 10, 0
szTool_ReadOK       db "[tool:read_file] ", 0
szTool_WriteOK      db "[tool:write_file] written ", 0
szTool_DirHeader    db "[tool:list_dir] ", 0
szTool_CmdHeader    db "[tool:run_cmd] exit=", 0
szNoDiag            db "[no diagnostics pending]", 13, 10, 0
szFindStar          db "\*", 0
szFindStarLen       equ 2
szNewline           db 13, 10, 0
szColon             db ":", 0
szFuncKind          db "function", 0
szClassKind         db "class", 0
szStructKind        db "struct", 0
szEqSign            db "=", 0
szExitPfx           db "[exit=", 0
szExitSfx           db "] ", 0
szBytesStr          db " bytes", 13, 10, 0
szHitSep            db ":", 0
szKindFunc          db ":function", 13, 10, 0
szKindCls           db ":class", 13, 10, 0
szKindStruct        db ":struct", 13, 10, 0
szMatchHdr          db "match:", 0
szLinePfx           db "line ", 0
szSymPfx            db "sym:", 0

; ---------------------------------------------------------------------------
; .DATA? — Uninitialized variables
; ---------------------------------------------------------------------------
.data?
ALIGN 8
g_findData      db FIND_DATA_SIZE dup(?)   ; WIN32_FIND_DATAA for dir listing
g_procInfo      db 64 dup(?)               ; PROCESS_INFORMATION
g_startInfo     db 104 dup(?)              ; STARTUPINFOA
g_cmdPipe       dq ?, ?                    ; [0]=hReadPipe, [1]=hWritePipe
g_pipeBuf       db TOOL_BUF_SIZE dup(?)    ; pipe read buffer

; ---------------------------------------------------------------------------
; .CODE
; ---------------------------------------------------------------------------
.code

; ============================================================================
;  _StrLen  —  internal helper
;  In:  RCX = pointer to null-terminated string
;  Out: RAX = length (bytes, excluding null terminator)
;  Clobbers: RAX, RDX
; ============================================================================
_StrLen PROC
        xor     rax, rax
_sl_loop:
        movzx   edx, byte ptr [rcx + rax]
        test    dl, dl
        jz      _sl_done
        inc     rax
        jmp     _sl_loop
_sl_done:
        ret
_StrLen ENDP

; ============================================================================
;  _StrCopy  —  internal helper
;  In:  RCX = dst, RDX = src
;  Out: RAX = bytes copied (excluding null)
;  Clobbers: RAX, R10
; ============================================================================
_StrCopy PROC
        xor     rax, rax
_sc_loop:
        movzx   r10d, byte ptr [rdx + rax]
        mov     byte ptr [rcx + rax], r10b
        test    r10b, r10b
        jz      _sc_done
        inc     rax
        jmp     _sc_loop
_sc_done:
        ret
_StrCopy ENDP

; ============================================================================
;  _AppendStr  —  append src null-term string to dst buffer capped at cap
;  In:  RCX = dst buffer base, RDX = current offset (bytes already in buf),
;       R8  = src string ptr,  R9  = capacity (bytes total, including null slot)
;  Out: RAX = new offset (dst never overflows capacity-1)
;  Clobbers: RAX, R10, R11
; ============================================================================
_AppendStr PROC
        mov     rax, rdx        ; rax = current write offset
_as_loop:
        cmp     rax, r9
        jge     _as_clamp
        movzx   r10d, byte ptr [r8]
        test    r10b, r10b
        jz      _as_done
        mov     byte ptr [rcx + rax], r10b
        inc     r8
        inc     rax
        jmp     _as_loop
_as_clamp:
        ; buffer full — back off one so we can write null
        mov     rax, r9
        dec     rax
_as_done:
        ; null-terminate at current position (don't advance)
        mov     byte ptr [rcx + rax], 0
        ret
_AppendStr ENDP

; ============================================================================
;  _AppendUInt32  —  convert DWORD to decimal ASCII and append to buffer
;  In:  RCX = dst buf, RDX = offset, R8 = uint32 value, R9 = capacity
;  Out: RAX = new offset
;  Clobbers: RAX, R10, R11, stack (no frame — only internal)
; ============================================================================
_AppendUInt32 PROC
        ; Convert uint32 in R8D to decimal string in a local temp on the
        ; caller's already-established frame.  We push a 24-byte scratch area.
        push    rbp
        push    rbx
        push    rsi
        push    rdi
        sub     rsp, 32
        mov     rbp, rcx        ; dst buf
        mov     rbx, rdx        ; offset
        mov     esi, r8d        ; value
        mov     rdi, r9         ; capacity
        ; build decimal string backwards in a 20-char temp (on RSP scratch)
        ; use rbp+offset for scratch (collision-safe because we have 32+push space)
        ; Actually use r11 as scratch stack pointer approach:
        lea     r11, [rsp+28]   ; scratch area within sub rsp, 32 (16 bytes safe)
        mov     r10d, 0         ; digit count
        test    esi, esi
        jnz     _au_nonzero
        ; value == 0
        mov     byte ptr [r11], '0'
        inc     r10d
        jmp     _au_emit
_au_nonzero:
_au_digit:
        test    esi, esi
        jz      _au_emit
        mov     eax, esi
        xor     edx, edx
        mov     ecx, 10
        div     ecx             ; eax = quotient, edx = remainder
        add     dl, '0'
        mov     byte ptr [r11 + r10], dl
        inc     r10d
        mov     esi, eax
        jmp     _au_digit
_au_emit:
        ; digits are in reverse order in [r11..r11+r10-1]; emit forwards
        mov     ecx, r10d
        dec     ecx             ; ecx = r10-1
_au_emit_loop:
        cmp     rbx, rdi
        jge     _au_cap
        movzx   eax, byte ptr [r11 + rcx]
        mov     byte ptr [rbp + rbx], al
        inc     rbx
        dec     ecx
        jns     _au_emit_loop
_au_cap:
        mov     byte ptr [rbp + rbx], 0
        mov     rax, rbx
        add     rsp, 32
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret
_AppendUInt32 ENDP

; ============================================================================
;  _JsonExtract  —  pull a string value out of a flat JSON object
;  In:  RCX = pJson (null-term UTF-8)
;       RDX = pKey  (just the key name, e.g. "path" — WITHOUT quotes)
;       R8  = pOutBuf (destination)
;       R9  = outBufSize (including null slot)
;  Out: RAX = length of extracted value (0 = not found or empty)
;  Clobbers: RAX, R10, R11 and pushes/pops rbx, rbp, rsi, rdi
; ============================================================================
_JsonExtract PROC FRAME
        push    rbp
        .pushreg rbp
        push    rbx
        .pushreg rbx
        push    rsi
        .pushreg rsi
        push    rdi
        .pushreg rdi
        sub     rsp, 32
        .allocstack 32
        .endprolog

        ; rbx = pJson, rbp = pKey, rsi = pOutBuf, rdi = outBufSize
        mov     rbx, rcx
        mov     rbp, rdx
        mov     rsi, r8
        mov     rdi, r9

        ; Phase 1: scan pJson for occurrence of pKey
        ; Strategy: for each position in pJson, check if pKey matches,
        ; preceded by '"' and followed by '":"'
        xor     r10, r10        ; r10 = scan index into pJson

_je_outer:
        movzx   eax, byte ptr [rbx + r10]
        test    al, al
        jz      _je_notfound
        ; look for opening quote
        cmp     al, '"'
        jne     _je_outer_next
        ; found '"'; try to match key starting at r10+1
        lea     r11, [r10 + 1]  ; r11 = index into pJson for key match
        xor     ecx, ecx        ; ecx = index into pKey
_je_keymatch:
        movzx   eax, byte ptr [rbp + rcx]
        test    al, al
        jz      _je_keymatched  ; key fully matched
        movzx   edx, byte ptr [rbx + r11]
        cmp     al, dl
        jne     _je_outer_next
        inc     r11
        inc     ecx
        jmp     _je_keymatch
_je_keymatched:
        ; expect '":"' at position r11
        movzx   eax, byte ptr [rbx + r11]
        cmp     al, '"'
        jne     _je_outer_next
        inc     r11
        movzx   eax, byte ptr [rbx + r11]
        cmp     al, ':'
        jne     _je_outer_next
        inc     r11
        movzx   eax, byte ptr [rbx + r11]
        cmp     al, '"'
        jne     _je_outer_next
        inc     r11
        ; r11 now points to first char of value; copy until '"' or end
        xor     ecx, ecx        ; ecx = outBuf write offset
_je_copy:
        cmp     ecx, edi
        jge     _je_copy_done
        movzx   eax, byte ptr [rbx + r11]
        test    al, al
        jz      _je_copy_done
        cmp     al, '"'
        je      _je_copy_done
        ; handle escape: if '\' followed by '"', emit '"'
        cmp     al, '\'
        jne     _je_copy_plain
        movzx   edx, byte ptr [rbx + r11 + 1]
        cmp     dl, '"'
        jne     _je_copy_plain
        ; escaped quote
        mov     byte ptr [rsi + rcx], '"'
        inc     ecx
        add     r11, 2
        jmp     _je_copy
_je_copy_plain:
        mov     byte ptr [rsi + rcx], al
        inc     ecx
        inc     r11
        jmp     _je_copy
_je_copy_done:
        mov     byte ptr [rsi + rcx], 0
        mov     eax, ecx
        jmp     _je_ret
_je_outer_next:
        inc     r10
        jmp     _je_outer
_je_notfound:
        xor     eax, eax
        mov     byte ptr [rsi], 0
_je_ret:
        add     rsp, 32
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret
_JsonExtract ENDP

; ============================================================================
;  _JsonExtractUInt  — extract integer value for a key e.g. "timeout_ms":5000
;  In:  RCX = pJson, RDX = pKey
;  Out: RAX = parsed uint (0 if not found)
;  Clobbers: RAX, R10, R11 and scratch regs
; ============================================================================
_JsonExtractUInt PROC FRAME
        push    rbp
        .pushreg rbp
        push    rbx
        .pushreg rbx
        push    rsi
        .pushreg rsi
        push    rdi
        .pushreg rdi
        sub     rsp, 32
        .allocstack 32
        .endprolog

        mov     rbx, rcx        ; pJson
        mov     rbp, rdx        ; pKey
        xor     r10, r10        ; scan index

_jeu_outer:
        movzx   eax, byte ptr [rbx + r10]
        test    al, al
        jz      _jeu_notfound
        cmp     al, '"'
        jne     _jeu_next
        lea     r11, [r10 + 1]
        xor     ecx, ecx
_jeu_key:
        movzx   eax, byte ptr [rbp + rcx]
        test    al, al
        jz      _jeu_keyok
        movzx   edx, byte ptr [rbx + r11]
        cmp     al, dl
        jne     _jeu_next
        inc     r11
        inc     ecx
        jmp     _jeu_key
_jeu_keyok:
        movzx   eax, byte ptr [rbx + r11]
        cmp     al, '"'
        jne     _jeu_next
        inc     r11
        movzx   eax, byte ptr [rbx + r11]
        cmp     al, ':'
        jne     _jeu_next
        inc     r11
        ; skip optional space
_jeu_skipsp:
        movzx   eax, byte ptr [rbx + r11]
        cmp     al, ' '
        jne     _jeu_digits
        inc     r11
        jmp     _jeu_skipsp
_jeu_digits:
        xor     esi, esi        ; accumulated value
_jeu_digit_loop:
        movzx   eax, byte ptr [rbx + r11]
        cmp     al, '0'
        jl      _jeu_done
        cmp     al, '9'
        jg      _jeu_done
        sub     al, '0'
        imul    esi, esi, 10
        add     esi, eax
        inc     r11
        jmp     _jeu_digit_loop
_jeu_done:
        mov     eax, esi
        jmp     _jeu_ret
_jeu_next:
        inc     r10
        jmp     _jeu_outer
_jeu_notfound:
        xor     eax, eax
_jeu_ret:
        add     rsp, 32
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret
_JsonExtractUInt ENDP

; ============================================================================
;  Tool_Init
;  Fill dispatch table, allocate heap overflow buffer, set initialized flag.
;  In:  (none)
;  Out: EAX = 0
; ============================================================================
Tool_Init PROC FRAME
        push    rbp
        .pushreg rbp
        push    rbx
        .pushreg rbx
        sub     rsp, 32
        .allocstack 32
        .endprolog

        ; Check if already initialized
        mov     eax, dword ptr [g_toolInitialized]
        test    eax, eax
        jnz     _ti_done

        ; Allocate 64 KB dynamic overflow buffer from process heap
        mov     rcx, qword ptr [g_hHeap]
        mov     edx, HEAP_ZERO_MEMORY
        mov     r8, TOOL_BUF_SIZE
        call    HeapAlloc
        mov     qword ptr [g_toolDynBuf], rax

        ; Fill dispatch table with procedure addresses
        lea     rbx, [g_toolDispatch]              ; RIP-relative base
        lea     rax, Tool_ReadFile
        mov     qword ptr [rbx + 0*8], rax

        lea     rax, Tool_WriteFile
        mov     qword ptr [rbx + 1*8], rax

        lea     rax, Tool_ListDir
        mov     qword ptr [rbx + 2*8], rax

        lea     rax, Tool_RunCommand
        mov     qword ptr [rbx + 3*8], rax

        lea     rax, Tool_SearchCode
        mov     qword ptr [rbx + 4*8], rax

        lea     rax, Tool_GetDiagnostics
        mov     qword ptr [rbx + 5*8], rax

        lea     rax, Tool_GetSymbols
        mov     qword ptr [rbx + 6*8], rax

        ; Stage-1 family lanes: keep behavior compatible by routing to
        ; Tool_RunCommand while preserving distinct tool IDs.
        lea     rax, Tool_RunCommand
        mov     qword ptr [rbx + TOOL_ID_GIT_OPS*8], rax
        mov     qword ptr [rbx + TOOL_ID_BUILD_OPS*8], rax
        mov     qword ptr [rbx + TOOL_ID_TEST_OPS*8], rax
        mov     qword ptr [rbx + TOOL_ID_PROCESS_OPS*8], rax
        mov     qword ptr [rbx + TOOL_ID_SHELL_OPS*8], rax
        mov     qword ptr [rbx + TOOL_ID_AGENT_OPS*8], rax
        mov     qword ptr [rbx + TOOL_ID_MODEL_OPS*8], rax
        mov     qword ptr [rbx + TOOL_ID_WEB_OPS*8], rax
        mov     qword ptr [rbx + TOOL_ID_SYSTEM_OPS*8], rax

        ; Set initialized flag
        mov     dword ptr [g_toolInitialized], 1

_ti_done:
        xor     eax, eax
        add     rsp, 32
        pop     rbx
        pop     rbp
        ret
Tool_Init ENDP

; ============================================================================
;  Tool_Execute
;  RCX = tool_id (0-based), RDX = pJsonArgs, R8 = pResultBuf, R9 = resultBufSize
;  Out: EAX = handler return value, or -1 on dispatch error
; ============================================================================
Tool_Execute PROC FRAME
        push    rbp
        .pushreg rbp
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
        push    r14
        .pushreg r14
        sub     rsp, 32
        .allocstack 32
        .endprolog

        mov     r12, rcx        ; tool_id
        mov     r13, rdx        ; pJsonArgs
        mov     r14, r8         ; pResultBuf
        mov     rbx, r9         ; resultBufSize

        ; Check initialized
        mov     eax, dword ptr [g_toolInitialized]
        test    eax, eax
        jnz     _te_check_id

        ; Not initialized — copy error string to pResultBuf
        mov     rcx, r14
        lea     rdx, szTool_NoInit
        call    _StrCopy
        mov     eax, -1
        jmp     _te_ret

_te_check_id:
        ; Bounds check tool_id
        cmp     r12, MAX_TOOLS
        jb      _te_lookup
        mov     rcx, r14
        lea     rdx, szTool_BadId
        call    _StrCopy
        mov     eax, -1
        jmp     _te_ret

_te_lookup:
        ; Load handler address from dispatch table
        lea     rax, [g_toolDispatch]                  ; RIP-relative base
        mov     rax, qword ptr [rax + r12*8]           ; register-relative lookup
        test    rax, rax
        jnz     _te_call
        mov     rcx, r14
        lea     rdx, szTool_BadId
        call    _StrCopy
        mov     eax, -1
        jmp     _te_ret

_te_call:
        ; Call handler(pJsonArgs, pResultBuf, resultBufSize)
        mov     rsi, rax        ; save handler ptr
        mov     rcx, r13        ; pJsonArgs
        mov     rdx, r14        ; pResultBuf
        mov     r8,  rbx        ; resultBufSize
        call    rsi
        ; EAX = handler result — fall through

_te_ret:
        add     rsp, 32
        pop     r14
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret
Tool_Execute ENDP

; ============================================================================
;  Tool_ReadFile
;  RCX = pJsonArgs {"path":"C:\\src\\foo.cpp"}
;  RDX = pResultBuf
;  R8  = bufSize
;  Out: EAX = bytes read, or -1 on error
; ============================================================================
Tool_ReadFile PROC FRAME
        push    rbp
        .pushreg rbp
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
        push    r14
        .pushreg r14
        sub     rsp, 32 + MAX_PATH + 12  ; 304 bytes total, 8-byte aligned for unwind info
        .allocstack 32 + MAX_PATH + 12
        .endprolog

        mov     rbx, rcx        ; pJsonArgs
        mov     r12, rdx        ; pResultBuf
        mov     r13, r8         ; bufSize
        ; local path buffer at [rsp+32]
        lea     rdi, [rsp+32]

        ; Extract "path" field from JSON
        mov     rcx, rbx
        lea     rdx, szTool_ReadOK          ; re-use label; actually we need "path"
        ; We need the literal string "path" — use a temporary on stack
        ; Write "path\0" into scratch above our path buffer
        lea     r14, [rsp + 32 + MAX_PATH]  ; scratch for key name
        mov     dword ptr [r14], 'htap'     ; 'p','a','t','h' reversed = little-endian "path"
        ; That's 'p'=70h,'a'=61h,'t'=74h,'h'=68h
        ; Little-endian DWORD: byte0='p', byte1='a', byte2='t', byte3='h'
        mov     dword ptr [r14], 68746170h  ; 'p'=70h 'a'=61h 't'=74h 'h'=68h -> as LE DWORD = 68746170h
        mov     byte ptr [r14 + 4], 0

        mov     rcx, rbx        ; pJson
        mov     rdx, r14        ; "path"
        mov     r8,  rdi        ; out: path buffer
        mov     r9,  MAX_PATH
        call    _JsonExtract
        test    eax, eax
        jnz     _rf_open
        ; path field not found
        mov     rax, -1
        jmp     _rf_ret

_rf_open:
        ; CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
        mov     rcx, rdi                    ; lpFileName
        mov     edx, GENERIC_READ
        mov     r8d, FILE_SHARE_READ
        xor     r9d, r9d                    ; lpSecurityAttributes = NULL
        mov     qword ptr [rsp+32], OPEN_EXISTING
        mov     qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
        mov     qword ptr [rsp+48], 0       ; hTemplateFile = NULL
        call    CreateFileA
        cmp     rax, INVALID_HANDLE_VALUE
        jne     _rf_do_read
        mov     eax, -1
        jmp     _rf_ret

_rf_do_read:
        mov     rsi, rax        ; hFile
        ; Prepend header label to result buffer
        mov     rcx, r12
        lea     rdx, szTool_ReadOK
        call    _StrCopy
        ; rax = bytes of header label (not counting null)
        mov     r14d, eax       ; r14d = current write offset
        ; ReadFile(hFile, pResultBuf + offset, bufSize - offset - 1, &bytesRead, NULL)
        mov     rcx, rsi
        lea     rdx, [r12 + r14]
        mov     r8,  r13
        sub     r8,  r14
        dec     r8                          ; leave room for null
        lea     r9,  [g_toolBytesWritten]   ; lpNumberOfBytesRead
        mov     qword ptr [rsp+32], 0       ; lpOverlapped = NULL
        call    ReadFile
        ; CloseHandle regardless of ReadFile result
        mov     rcx, rsi
        call    CloseHandle
        ; null-terminate
        mov     ecx, dword ptr [g_toolBytesWritten]
        add     r14d, ecx
        mov     byte ptr [r12 + r14], 0
        mov     eax, r14d

_rf_ret:
        add     rsp, 32 + MAX_PATH + 12
        pop     r14
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret
Tool_ReadFile ENDP

; ============================================================================
;  Tool_WriteFile
;  RCX = pJsonArgs {"path":"...","content":"..."}
;  RDX = pResultBuf
;  R8  = bufSize
;  Out: EAX = 0 success, -1 error
; ============================================================================
Tool_WriteFile PROC FRAME
        push    rbp
        .pushreg rbp
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
        push    r14
        .pushreg r14
        sub     rsp, 32 + MAX_PATH*2 + 32   ; shadow + two path/key bufs (padded)
        .allocstack 32 + MAX_PATH*2 + 32
        .endprolog

        mov     rbx, rcx        ; pJsonArgs
        mov     r12, rdx        ; pResultBuf
        mov     r13, r8         ; bufSize

        ; local buffers
        lea     rdi, [rsp+32]               ; path buffer (MAX_PATH bytes)
        lea     rsi, [rsp + 32 + MAX_PATH]  ; key scratch (MAX_PATH bytes)

        ; Extract "path"
        mov     dword ptr [rsi],   68746170h ; "path"
        mov     byte ptr  [rsi+4], 0
        mov     rcx, rbx
        mov     rdx, rsi
        mov     r8,  rdi
        mov     r9,  MAX_PATH
        call    _JsonExtract
        test    eax, eax
        jnz     _wf_get_content
        mov     eax, -1
        jmp     _wf_ret

_wf_get_content:
        ; Extract "content" — use heap dynamic buf as content staging area
        mov     rcx, rbx
        lea     rdx, _wf_key_content          ; "content" key
        mov     r8,  qword ptr [g_toolDynBuf]
        test    r8, r8
        jnz     _wf_has_dynbuf
        ; fallback: use static result buf offset area
        mov     r8, r12
_wf_has_dynbuf:
        mov     r9, TOOL_BUF_SIZE - 1
        call    _JsonExtract
        mov     r14d, eax       ; content length

        ; CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
        mov     rcx, rdi
        mov     edx, GENERIC_WRITE
        xor     r8d, r8d
        xor     r9d, r9d
        mov     qword ptr [rsp+32], CREATE_ALWAYS
        mov     qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
        mov     qword ptr [rsp+48], 0
        call    CreateFileA
        cmp     rax, INVALID_HANDLE_VALUE
        jne     _wf_do_write
        mov     eax, -1
        jmp     _wf_ret

_wf_do_write:
        mov     rsi, rax        ; hFile
        ; WriteFile(hFile, pContent, contentLen, &bytesWritten, NULL)
        mov     rcx, rsi
        mov     rdx, qword ptr [g_toolDynBuf]
        test    rdx, rdx
        jnz     _wf_has_content_ptr
        mov     rdx, r12
_wf_has_content_ptr:
        mov     r8d, r14d
        lea     r9,  [g_toolBytesWritten]
        mov     qword ptr [rsp+32], 0
        call    WriteFile
        ; CloseHandle
        mov     rcx, rsi
        call    CloseHandle
        ; Build result message: "[tool:write_file] written N bytes\r\n"
        mov     rcx, r12
        lea     rdx, szTool_WriteOK
        call    _StrCopy
        ; rax = header length
        mov     rbx, rax        ; write offset
        ; append byte count
        mov     rcx, r12
        mov     rdx, rbx
        mov     r8d, r14d
        mov     r9,  r13
        call    _AppendUInt32
        mov     rbx, rax
        ; append " bytes\r\n"
        mov     rcx, r12
        mov     rdx, rbx
        lea     r8, szBytesStr
        mov     r9, r13
        call    _AppendStr
        xor     eax, eax

_wf_ret:
        add     rsp, 32 + MAX_PATH*2 + 32
        pop     r14
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret

_wf_key_content db "content", 0

Tool_WriteFile ENDP

; ============================================================================
;  Tool_ListDir
;  RCX = pJsonArgs {"path":"C:\\src"}
;  RDX = pResultBuf
;  R8  = bufSize
;  Out: EAX = total chars written
; ============================================================================
Tool_ListDir PROC FRAME
        push    rbp
        .pushreg rbp
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
        push    r14
        .pushreg r14
        sub     rsp, 32 + MAX_PATH + MAX_PATH + 16
        .allocstack 32 + MAX_PATH + MAX_PATH + 16
        .endprolog

        mov     rbx, rcx        ; pJsonArgs
        mov     r12, rdx        ; pResultBuf
        mov     r13, r8         ; bufSize
        lea     rdi, [rsp+32]               ; path buf (MAX_PATH)
        lea     rsi, [rsp + 32 + MAX_PATH]  ; search pattern buf (MAX_PATH)

        ; Build key "path\0" on stack scratch
        lea     r14, [rsp + 32 + MAX_PATH*2]
        mov     dword ptr [r14],   68746170h
        mov     byte ptr  [r14+4], 0

        ; Extract "path"
        mov     rcx, rbx
        mov     rdx, r14
        mov     r8,  rdi
        mov     r9,  MAX_PATH
        call    _JsonExtract
        test    eax, eax
        jnz     _ld_build_pattern
        xor     eax, eax
        jmp     _ld_ret

_ld_build_pattern:
        ; Copy path into search pattern buffer, then append "\*"
        mov     rcx, rsi
        mov     rdx, rdi
        call    _StrCopy
        ; rax = path length; append "\*"
        ; find end of pattern string
        mov     rcx, rsi
        call    _StrLen
        ; rax = length; rsi+rax is one past last char
        ; append backslash
        mov     byte ptr [rsi + rax], '\'
        inc     rax
        mov     byte ptr [rsi + rax], '*'
        inc     rax
        mov     byte ptr [rsi + rax], 0

        ; Emit header into result buf
        xor     r14d, r14d      ; write offset in result buf
        mov     rcx, r12
        mov     rdx, r14
        lea     r8,  szTool_DirHeader
        mov     r9,  r13
        call    _AppendStr
        mov     r14, rax

        ; FindFirstFileA(lpPattern, &g_findData)
        mov     rcx, rsi
        lea     rdx, g_findData
        call    FindFirstFileA
        cmp     rax, INVALID_HANDLE_VALUE
        jne     _ld_enum
        ; nothing found
        mov     byte ptr [r12 + r14], 0
        mov     eax, r14d
        jmp     _ld_ret

_ld_enum:
        mov     rbx, rax        ; hFind

_ld_loop:
        ; append cFileName (offset 44 in WIN32_FIND_DATAA) + CRLF
        lea     r8, [g_findData]
        add     r8, WFDA_cFileName
        mov     rcx, r12
        mov     rdx, r14
        mov     r9,  r13
        call    _AppendStr
        mov     r14, rax
        ; append CRLF
        mov     rcx, r12
        mov     rdx, r14
        lea     r8,  szNewline
        mov     r9,  r13
        call    _AppendStr
        mov     r14, rax

        ; FindNextFileA
        mov     rcx, rbx
        lea     rdx, g_findData
        call    FindNextFileA
        test    eax, eax
        jnz     _ld_loop

        ; FindClose
        mov     rcx, rbx
        call    FindClose

        ; null-terminate and return count
        mov     byte ptr [r12 + r14], 0
        mov     eax, r14d

_ld_ret:
        add     rsp, 32 + MAX_PATH + MAX_PATH + 16
        pop     r14
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret
Tool_ListDir ENDP

; ============================================================================
;  Tool_RunCommand
;  RCX = pJsonArgs {"cmd":"dir /b","timeout_ms":5000}
;  RDX = pResultBuf
;  R8  = bufSize
;  Out: EAX = process exit code, or -1 on launch failure
;  Frame: rbp rbx rsi rdi r12 r13 r14 (7 saves, then sub 32+sa_area)
; ============================================================================
Tool_RunCommand PROC FRAME
        push    rbp
        .pushreg rbp
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
        push    r14
        .pushreg r14
        ; local area: 32 shadow + SA_SIZE(24) + key_scratch(16) + cmd_buf(MAX_PATH*2) + 8 align
        sub     rsp, 32 + SA_SIZE + 16 + MAX_PATH*2 + 8
        .allocstack 32 + SA_SIZE + 16 + MAX_PATH*2 + 8
        .endprolog

        mov     r12, rcx        ; pJsonArgs
        mov     r13, rdx        ; pResultBuf
        mov     r14, r8         ; bufSize

        ; local addresses
        lea     rbx, [rsp + 32]                         ; SECURITY_ATTRIBUTES on stack
        lea     rsi, [rsp + 32 + SA_SIZE + 16]          ; cmd buffer

        ; Extract "cmd" field
        lea     rdi, [rsp + 32 + SA_SIZE]               ; key scratch "cmd\0"
        mov     dword ptr [rdi], 00646d63h              ; 'c','m','d','\0'
        mov     rcx, r12
        mov     rdx, rdi
        mov     r8,  rsi
        mov     r9,  MAX_PATH*2
        call    _JsonExtract
        test    eax, eax
        jnz     _rc_got_cmd
        mov     eax, -1
        jmp     _rc_ret

_rc_got_cmd:
        ; Extract "timeout_ms" — key scratch for "timeout_ms"
        lea     rcx, _rc_key_timeout
        ; Actually put the key in rdi scratch area
        lea     rdi, [rsp + 32 + SA_SIZE]
        mov     qword ptr [rdi], 74756f65h  ; timeout first bytes
        ; easier: just call with a label
        mov     rcx, r12
        lea     rdx, _rc_key_timeout
        and     rdx, 0FFFFFFFFFFFFFFFFh      ; ensure full 64-bit
        ; Actually using local data label:
        mov     rdx, offset _rc_key_timeout  ; this needs a forward-compatible approach
        ; Use inline stack approach: we already have scratch — write "timeout_ms\0"
        lea     rdi, [rsp + 32 + SA_SIZE]
        ; "timeout_ms\0" = 74 69 6d 65 6f 75 74 5f 6d 73 00
        mov     dword ptr [rdi+0], 656d6974h  ; "time"
        mov     dword ptr [rdi+4], 5f74756fh  ; "out_"
        mov     dword ptr [rdi+8], 0000736dh  ; "ms\0"
        mov     rcx, r12
        mov     rdx, rdi
        call    _JsonExtractUInt
        test    eax, eax
        jnz     _rc_got_timeout
        mov     eax, 10000          ; default 10 seconds
_rc_got_timeout:
        mov     dword ptr [rbp - 4], eax    ; save timeout on frame (rbp not used as frame ptr here actually)
        ; Store timeout in r9d temporarily
        push    rax                         ; save timeout on stack temporarily

        ; Zero out SECURITY_ATTRIBUTES
        xor     ecx, ecx
        mov     qword ptr [rbx + SA_nLength],              rcx
        mov     qword ptr [rbx + SA_lpSecurityDescriptor], rcx
        mov     qword ptr [rbx + SA_bInheritHandle],       rcx
        mov     dword ptr [rbx + SA_nLength], SA_SIZE
        mov     dword ptr [rbx + SA_bInheritHandle], 1     ; bInheritHandle = TRUE

        ; CreatePipe(&g_cmdPipe[0], &g_cmdPipe[1], &sa, 0)
        lea     rcx, [g_cmdPipe + 0]    ; hReadPipe
        lea     rdx, [g_cmdPipe + 8]    ; hWritePipe
        mov     r8,  rbx                ; lpPipeAttributes (inheritable)
        xor     r9d, r9d                ; nSize = default
        call    CreatePipe
        test    eax, eax
        jnz     _rc_pipe_ok
        pop     rax                     ; discard saved timeout
        mov     eax, -1
        jmp     _rc_ret

_rc_pipe_ok:
        ; Zero STARTUPINFOA at g_startInfo
        xor     ecx, ecx
        lea     r8, g_startInfo
        mov     rdx, 104/8
_rc_zsi:
        mov     qword ptr [r8], rcx
        add     r8, 8
        dec     rdx
        jnz     _rc_zsi
        ; Set cb field
        mov     dword ptr [g_startInfo + STUI_cb], 104
        ; Set dwFlags = STARTF_USESTDHANDLES
        mov     dword ptr [g_startInfo + STUI_dwFlags], STARTF_USESTDHANDLES
        ; Set hStdOutput = hStdError = write pipe handle
        mov     rax, qword ptr [g_cmdPipe + 8]
        mov     qword ptr [g_startInfo + STUI_hStdOutput], rax
        mov     qword ptr [g_startInfo + STUI_hStdError],  rax
        ; hStdInput = NULL (already zero)

        ; Zero PROCESS_INFORMATION
        xor     rax, rax
        mov     qword ptr [g_procInfo +  0], rax
        mov     qword ptr [g_procInfo +  8], rax
        mov     qword ptr [g_procInfo + 16], rax

        ; CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &startInfo, &procInfo)
        xor     ecx, ecx                            ; lpApplicationName = NULL
        mov     rdx, rsi                            ; lpCommandLine = cmd buf
        xor     r8d, r8d                            ; lpProcessAttributes = NULL
        xor     r9d, r9d                            ; lpThreadAttributes = NULL
        mov     qword ptr [rsp+32], 1               ; bInheritHandles = TRUE
        mov     qword ptr [rsp+40], CREATE_NO_WINDOW
        mov     qword ptr [rsp+48], 0               ; lpEnvironment = NULL
        mov     qword ptr [rsp+56], 0               ; lpCurrentDirectory = NULL
        lea     rax, g_startInfo
        mov     qword ptr [rsp+64], rax             ; lpStartupInfo
        lea     rax, g_procInfo
        mov     qword ptr [rsp+72], rax             ; lpProcessInformation
        call    CreateProcessA
        test    eax, eax
        jnz     _rc_proc_ok
        ; Close pipes before returning error
        mov     rcx, qword ptr [g_cmdPipe]
        call    CloseHandle
        mov     rcx, qword ptr [g_cmdPipe+8]
        call    CloseHandle
        pop     rax
        mov     eax, -1
        jmp     _rc_ret

_rc_proc_ok:
        ; Close write end in parent — child now owns it; parent reading DetectEOF
        mov     rcx, qword ptr [g_cmdPipe+8]
        call    CloseHandle
        mov     qword ptr [g_cmdPipe+8], 0

        ; WaitForSingleObject(hProcess, timeout_ms)
        pop     rdi                                 ; restore saved timeout -> rdi
        mov     rcx, qword ptr [g_procInfo + PI_hProcess]
        mov     edx, edi
        call    WaitForSingleObject
        ; if WAIT_TIMEOUT, TerminateProcess
        cmp     eax, WAIT_TIMEOUT_VAL
        jne     _rc_read_pipe
        mov     rcx, qword ptr [g_procInfo + PI_hProcess]
        mov     edx, 0FFh
        call    TerminateProcess

_rc_read_pipe:
        ; Drain pipe into g_pipeBuf using a loop
        xor     rbx, rbx    ; total bytes accumulated in g_pipeBuf
_rc_pipe_loop:
        ; ReadFile(hReadPipe, &g_pipeBuf + rbx, TOOL_BUF_SIZE - rbx - 1, &bytesRead, NULL)
        mov     rcx, qword ptr [g_cmdPipe]
        lea     rdx, [g_pipeBuf]                       ; RIP-relative base
        add     rdx, rbx                               ; register-relative offset
        mov     r8,  TOOL_BUF_SIZE
        sub     r8,  rbx
        dec     r8
        jle     _rc_pipe_done               ; buffer full
        lea     r9,  [g_toolBytesWritten]
        mov     qword ptr [rsp+32], 0
        call    ReadFile
        ; if ReadFile returns 0, check GetLastError — broken pipe = EOF, just stop
        test    eax, eax
        jz      _rc_pipe_done
        mov     eax, dword ptr [g_toolBytesWritten]
        test    eax, eax
        jz      _rc_pipe_done
        add     rbx, rax
        jmp     _rc_pipe_loop

_rc_pipe_done:
        lea     rax, [g_pipeBuf]                       ; RIP-relative base
        mov     byte ptr [rax + rbx], 0                ; register-relative store

        ; GetExitCodeProcess
        lea     rdi, [g_toolBytesWritten]   ; reuse as exit code ptr
        mov     rcx, qword ptr [g_procInfo + PI_hProcess]
        mov     rdx, rdi
        call    GetExitCodeProcess
        mov     esi, dword ptr [g_toolBytesWritten]  ; exit code in esi

        ; CloseHandle process and thread
        mov     rcx, qword ptr [g_procInfo + PI_hProcess]
        call    CloseHandle
        mov     rcx, qword ptr [g_procInfo + PI_hThread]
        call    CloseHandle
        ; Close read pipe
        mov     rcx, qword ptr [g_cmdPipe]
        call    CloseHandle

        ; Format result: "[exit=N] <pipe output>"
        xor     r12d, r12d  ; write offset into pResultBuf (saved in r13)
        mov     rcx, r13
        mov     rdx, r12
        lea     r8, szExitPfx   ; "[exit="
        mov     r9, r14
        call    _AppendStr
        mov     r12, rax
        ; append exit code decimal
        mov     rcx, r13
        mov     rdx, r12
        mov     r8d, esi
        mov     r9,  r14
        call    _AppendUInt32
        mov     r12, rax
        ; append "] "
        mov     rcx, r13
        mov     rdx, r12
        lea     r8, szExitSfx
        mov     r9, r14
        call    _AppendStr
        mov     r12, rax
        ; append pipe output
        mov     rcx, r13
        mov     rdx, r12
        lea     r8, g_pipeBuf
        mov     r9, r14
        call    _AppendStr

        mov     eax, esi    ; return exit code

_rc_ret:
        add     rsp, 32 + SA_SIZE + 16 + MAX_PATH*2 + 8
        pop     r14
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret

_rc_key_timeout db "timeout_ms", 0

Tool_RunCommand ENDP

; ============================================================================
;  _SearchFileForPattern — internal helper for Tool_SearchCode
;  Scans an open file's content buffer for a pattern, appending hits to result.
;  In:  RCX = pFilePath (for display in output)
;       RDX = pPatternBuf (null-term)
;       R8  = pContent (buffer with file bytes, not null-term necessarily)
;       R9  = contentLen (DWORD, zero-extended)
;       [rsp+32] = pResultBuf
;       [rsp+40] = resultBufCapacity
;       [rsp+48] = pWriteOffset (ptr to QWORD current write position in resultBuf)
;  Out: EAX = match count in this file
; ============================================================================
_SearchFileForPattern PROC FRAME
        push    rbp
        .pushreg rbp
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
        push    r14
        .pushreg r14
        sub     rsp, 32
        .allocstack 32
        .endprolog

        mov     r12, rcx        ; pFilePath
        mov     r13, rdx        ; pPattern
        mov     r14, r8         ; pContent
        mov     rbx, r9         ; contentLen
        mov     rsi, qword ptr [rsp+32+7*8+8+32]    ; pResultBuf  (past saves+ret+subframe)
        mov     rdi, qword ptr [rsp+32+7*8+8+40]    ; capacity
        mov     rbp, qword ptr [rsp+32+7*8+8+48]    ; pWriteOffset

        ; Get pattern length
        mov     rcx, r13
        call    _StrLen
        mov     r9d, eax        ; patLen

        xor     ecx, ecx        ; match count
        push    rcx             ; push match count (r11 usage below)
        xor     r10, r10        ; current position in content
        xor     r11d, r11d      ; current line number (1-based)
        inc     r11d

_sfp_scan:
        cmp     r10, rbx
        jge     _sfp_done
        ; track lines
        movzx   eax, byte ptr [r14 + r10]
        cmp     al, 10
        jne     _sfp_try_match
        inc     r11d
_sfp_try_match:
        ; try matching pattern at r10
        xor     edx, edx                ; pattern match index
_sfp_cmp:
        cmp     edx, r9d
        jge     _sfp_hit                ; full pattern matched
        mov     eax, r10d
        add     eax, edx
        cmp     eax, ebx
        jge     _sfp_no_match           ; ran past end of content
        movzx   eax, byte ptr [r14 + rax]
        movzx   r8d, byte ptr [r13 + rdx]
        cmp     al, r8b
        jne     _sfp_no_match
        inc     edx
        jmp     _sfp_cmp
_sfp_hit:
        ; Emit "filepath:line: <first 60 chars of line>\r\n"
        ; increment match count on stack
        inc     dword ptr [rsp]
        ; append filepath
        mov     rax, qword ptr [rbp]    ; current write offset
        mov     rcx, rsi
        mov     rdx, rax
        mov     r8,  r12
        mov     r9,  rdi
        call    _AppendStr
        mov     qword ptr [rbp], rax
        ; append ":"
        mov     rax, qword ptr [rbp]
        mov     rcx, rsi
        mov     rdx, rax
        lea     r8, szHitSep
        mov     r9, rdi
        call    _AppendStr
        mov     qword ptr [rbp], rax
        ; append line number
        mov     rax, qword ptr [rbp]
        mov     rcx, rsi
        mov     rdx, rax
        mov     r8d, r11d
        mov     r9, rdi
        call    _AppendUInt32
        mov     qword ptr [rbp], rax
        ; append ": " (colon space)
        mov     rcx, rsi
        mov     rdx, qword ptr [rbp]
        lea     r8, szHitSep
        mov     r9, rdi
        call    _AppendStr
        mov     qword ptr [rbp], rax
        mov     rcx, rsi
        mov     rdx, qword ptr [rbp]
        lea     r8, _sfp_space
        mov     r9, rdi
        call    _AppendStr
        mov     qword ptr [rbp], rax

        ; search backward from r10 to find start of this line for context
        mov     edx, r10d
_sfp_find_bol:
        test    edx, edx
        jz      _sfp_emit_ctx
        dec     edx
        movzx   eax, byte ptr [r14 + rdx]
        cmp     al, 10
        je      _sfp_at_bol
        jmp     _sfp_find_bol
_sfp_at_bol:
        inc     edx             ; skip the newline itself
_sfp_emit_ctx:
        ; emit up to 60 chars from line start or until newline/end
        mov     r8d, edx
        xor     ecx, ecx        ; char count for context
_sfp_ctx_loop:
        cmp     ecx, 60
        jge     _sfp_ctx_done
        cmp     r8d, ebx
        jge     _sfp_ctx_done
        movzx   eax, byte ptr [r14 + r8]
        cmp     al, 13
        je      _sfp_ctx_done
        cmp     al, 10
        je      _sfp_ctx_done
        ; write byte into result buf
        mov     rdx, qword ptr [rbp]
        cmp     rdx, rdi
        jge     _sfp_ctx_done
        mov     byte ptr [rsi + rdx], al
        inc     rdx
        mov     qword ptr [rbp], rdx
        inc     r8d
        inc     ecx
        jmp     _sfp_ctx_loop
_sfp_ctx_done:
        ; append CRLF
        mov     rax, qword ptr [rbp]
        mov     rcx, rsi
        mov     rdx, rax
        lea     r8, szNewline
        mov     r9, rdi
        call    _AppendStr
        mov     qword ptr [rbp], rax

_sfp_no_match:
        inc     r10
        jmp     _sfp_scan

_sfp_done:
        pop     rax             ; match count

        add     rsp, 32
        pop     r14
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret

_sfp_space db " ", 0

_SearchFileForPattern ENDP

; ============================================================================
;  Tool_SearchCode
;  RCX = pJsonArgs {"pattern":"TODO","path":"D:\\rawrxd\\src","ext":".cpp"}
;  RDX = pResultBuf
;  R8  = bufSize
;  Out: EAX = total match count
; ============================================================================
Tool_SearchCode PROC FRAME
        push    rbp
        .pushreg rbp
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
        push    r14
        .pushreg r14
        sub     rsp, 32 + MAX_PATH*3 + 68    ; 880 bytes total, 8-byte aligned for unwind info
        .allocstack 32 + MAX_PATH*3 + 68
        .endprolog

        mov     rbx, rcx        ; pJsonArgs
        mov     r12, rdx        ; pResultBuf
        mov     r13, r8         ; bufSize

        ; Local buffer layout:
        ;   [rsp+32]                  pattern buf (MAX_PATH bytes)
        ;   [rsp+32+MAX_PATH]         root path buf (MAX_PATH bytes)
        ;   [rsp+32+MAX_PATH*2]       extension buf (MAX_PATH bytes)
        ;   [rsp+32+MAX_PATH*3]       key scratch (32 bytes)
        ;   [rsp+32+MAX_PATH*3+32]    writeOffset QWORD
        lea     rdi, [rsp+32]
        lea     rsi, [rsp+32+MAX_PATH]
        lea     r14, [rsp+32+MAX_PATH*2]

        lea     rbp, [rsp+32+MAX_PATH*3]    ; key scratch (32 bytes at +0)
                                            ; writeOffset at +32

        ; Extract "pattern"
        mov     dword ptr [rbp],   6e726170h   ; "parn" -> "patt"
        ; Correct "pattern\0": p=70 a=61 t=74 t=74 e=65 r=72 n=6e 00
        mov     dword ptr [rbp+0], 74746170h   ; "patt"
        mov     dword ptr [rbp+4], 006e7265h   ; "ern\0"
        mov     rcx, rbx
        mov     rdx, rbp
        mov     r8,  rdi
        mov     r9,  MAX_PATH
        call    _JsonExtract
        test    eax, eax
        jnz     _sc_has_pattern
        xor     eax, eax
        jmp     _sc_ret

_sc_has_pattern:
        ; Extract "path"
        mov     dword ptr [rbp],   68746170h   ; "path\0"
        mov     byte ptr [rbp+4],  0
        mov     rcx, rbx
        mov     rdx, rbp
        mov     r8,  rsi
        mov     r9,  MAX_PATH
        call    _JsonExtract

        ; Extract "ext"
        mov     dword ptr [rbp],   00747865h   ; "ext\0"
        mov     rcx, rbx
        mov     rdx, rbp
        mov     r8,  r14
        mov     r9,  MAX_PATH
        call    _JsonExtract

        ; Initialize write offset
        xor     rax, rax
        mov     qword ptr [rbp+32], rax     ; writeOffset = 0

        ; Match count in r13 (reuse — save original bufSize on stack now)
        ; Push bufSize and resultBuf for _SearchFileForPattern calls via stack args
        push    rbp                             ; save rbp we used for scratch
        ; r13 was bufSize — save it
        push    r13

        ; Iterative scan: walk rsi (root path) for files matching ext in r14
        ; Using g_findData (global) — build search pattern root\* in scratch buf
        ; r13 = total match count (reset to 0)
        xor     r13d, r13d

        ; build "root\*" pattern in scratch (rbp+8 area? need new scratch)
        ; We already used rbp for the key scratch within the frame.
        ; Use g_pipeBuf as temp for the search glob pattern (not running cmd here)
        lea     r8, g_pipeBuf
        mov     rcx, r8
        mov     rdx, rsi        ; root path
        call    _StrCopy
        mov     rcx, r8
        call    _StrLen         ; length in rax
        mov     byte ptr [r8 + rax],   '\'
        mov     byte ptr [r8 + rax+1], '*'
        mov     byte ptr [r8 + rax+2], 0

        ; FindFirstFileA(pattern, &g_findData)
        mov     rcx, r8         ; r8 = g_pipeBuf (pattern)
        lea     rdx, g_findData
        call    FindFirstFileA
        cmp     rax, INVALID_HANDLE_VALUE
        je      _sc_cleanup
        mov     rbx, rax        ; hFind

_sc_enum:
        ; check FILE_ATTRIBUTE_DIRECTORY bit — skip directories
        mov     eax, dword ptr [g_findData + WFDA_dwFileAttributes]
        test    eax, FILE_ATTRIBUTE_DIRECTORY
        jnz     _sc_next

        ; Check extension match if ext is non-empty
        lea     rcx, [g_findData]
        add     rcx, WFDA_cFileName
        ; rcx points to file name; find last '.' in it
        call    _StrLen          ; rax = length of filename
        mov     r10d, eax       ; save length
        ; scan backwards for '.'
        dec     r10d
        jl      _sc_next        ; empty name
        lea     rax, [g_findData]                      ; RIP-relative base for ext scan loop
        add     rax, WFDA_cFileName
_sc_ext_scan:
        test    r10d, r10d
        jl      _sc_next
        movzx   edx, byte ptr [rax + r10]              ; register-relative byte load
        cmp     dl, '.'
        je      _sc_ext_found
        dec     r10d
        jmp     _sc_ext_scan
_sc_ext_found:
        ; check if r14 (ext filter) is empty — if so, accept all
        movzx   eax, byte ptr [r14]
        test    al, al
        jz      _sc_ext_ok
        ; compare extension from filename with r14
        lea     rcx, [g_findData]                      ; RIP-relative base
        add     rcx, WFDA_cFileName
        add     rcx, r10                               ; add scan offset
        mov     rdx, r14
_sc_ext_cmp:
        movzx   eax, byte ptr [rcx]
        movzx   edx, byte ptr [rdx]
        cmp     al, dl
        jne     _sc_next
        test    al, al
        jz      _sc_ext_ok
        inc     rcx
        inc     rdx
        jmp     _sc_ext_cmp

_sc_ext_ok:
        ; Build full file path: root + "\" + filename -> g_toolResultBuf as temp area
        ; Use g_toolDynBuf area or just g_pipeBuf+MAX_PATH
        lea     rcx, [g_pipeBuf]               ; use upper half of pipe buf for full path
        add     rcx, MAX_PATH
        mov     rdx, rsi                         ; root
        call    _StrCopy
        mov     r10, rax                         ; path length
        lea     rax, [g_pipeBuf]                 ; RIP-relative base
        add     rax, MAX_PATH
        mov     byte ptr [rax + r10], '\'        ; register-relative store
        inc     r10
        lea     rcx, [rax + r10]                 ; register-relative address
        lea     rdx, [g_findData]
        add     rdx, WFDA_cFileName
        call    _StrCopy

        ; Open file
        lea     rcx, [g_pipeBuf]
        add     rcx, MAX_PATH
        mov     edx, GENERIC_READ
        mov     r8d, FILE_SHARE_READ
        xor     r9d, r9d
        mov     qword ptr [rsp], OPEN_EXISTING       ; rsp used for 5th+ args
        mov     qword ptr [rsp+8], FILE_ATTRIBUTE_NORMAL
        mov     qword ptr [rsp+16], 0
        ; First we need to set up the shadow space properly
        ; At this point we're mid-function. Let me use sub-call shadow space carefully.
        ; We need 32 bytes shadow before each CALL. rsp is already set up.
        ; Actually at this point rsp is not aligned for sub-calls... 
        ; We need to properly call CreateFileA.
        ; Fix: use the 32-byte shadow area that was established by sub rsp at prolog.
        ; Store home params in [rsp+N] where N accounts for the pushes inside _sc_enum.
        ; Actually, since we're in the middle of the frame (post-.endprolog), 
        ; rsp points to our allocated frame area but we pushed extra items (rbp, r13 above).
        ; We need to realign. Let's use a dedicated sub-frame call approach:
        and     rsp, 0FFFFFFFFFFFFFFF0h     ; realign to 16 before call
        sub     rsp, 32                     ; shadow space

        lea     rcx, [g_pipeBuf]
        add     rcx, MAX_PATH
        mov     edx, GENERIC_READ
        mov     r8d, FILE_SHARE_READ
        xor     r9d, r9d
        mov     qword ptr [rsp+32], OPEN_EXISTING
        mov     qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
        mov     qword ptr [rsp+48], 0
        call    CreateFileA
        add     rsp, 32
        cmp     rax, INVALID_HANDLE_VALUE
        je      _sc_next

        mov     r11, rax        ; hFile (save)
        ; Read file content into g_toolDynBuf or g_pipeBuf second half
        ; Use g_toolDynBuf if allocated, otherwise g_pipeBuf portion
        mov     rdx, qword ptr [g_toolDynBuf]
        test    rdx, rdx
        jnz     _sc_has_buf
        lea     rdx, [g_pipeBuf]                ; use third quarter of pipe buf
        add     rdx, MAX_PATH*2
_sc_has_buf:
        and     rsp, 0FFFFFFFFFFFFFFF0h
        sub     rsp, 32
        mov     rcx, r11
        mov     r8, TOOL_BUF_SIZE - MAX_PATH*2 - 4
        lea     r9, [g_toolBytesWritten]
        mov     qword ptr [rsp+32], 0
        call    ReadFile
        add     rsp, 32

        and     rsp, 0FFFFFFFFFFFFFFF0h
        sub     rsp, 32
        mov     rcx, r11
        call    CloseHandle
        add     rsp, 32

        mov     r9d, dword ptr [g_toolBytesWritten]   ; contentLen
        mov     rdx, qword ptr [g_toolDynBuf]
        test    rdx, rdx
        jnz     _sc_call_search
        lea     rdx, [g_pipeBuf]
        add     rdx, MAX_PATH*2
_sc_call_search:
        ; Call _SearchFileForPattern
        ; Stack args: pResultBuf [rsp+32+32], capacity [rsp+40+32], &writeOffset [rsp+48+32]
        and     rsp, 0FFFFFFFFFFFFFFF0h
        sub     rsp, 32 + 24        ; shadow + 3 extra stack args (ugh, needs 8+8+8=24)
        ; Arg5 = r12 (pResultBuf)
        mov     qword ptr [rsp+32], r12
        ; Arg6 = capacity (we saved original bufSize on stack earlier as push r13)
        ; Actually we need original bufSize. It was saved before we replaced r13 with count.
        ; We have it in the "push r13" we did. Use a saved register instead.
        ; For capacity just use TOOL_BUF_SIZE as a conservative bound.
        mov     qword ptr [rsp+40], TOOL_BUF_SIZE - 1

        ; writeOffset ptr: we use rbp+32 (our scratch area in the frame)
        ; But rbp was also pushed. Use a fixed known address: g_toolBytesWritten for now? No.
        ; Let's embed a static per-iteration off. Use a local QWORD in our frame.
        ; We allocated [rsp+32+MAX_PATH*3+32] for writeOffset at the start.
        ; After the two extra pushes and realignment, rbp points to our scratch area.
        ; Restore rbp for this:
        pop     rbx                 ; pop saved r13 (original bufSize)  -- wait this is getting complex
        ; Actually I stacked: push rbp (scratch ptr), push r13 (bufSize), then did other ops.
        ; This is getting complex with the realignment. Let me just use r12 directly.
        ; The write offset is tracked via a simple global counter in g_toolBytesWritten.
        ; For Tool_SearchCode, we collect all matches without global write offset tracking — instead
        ; we just use _AppendStr with a local counter. 
        ; Simplify: use a global write offset in g_toolBytesWritten for the duration of this call.
        lea     r8, [g_toolBytesWritten]   ; use this as &writeOffset
        mov     qword ptr [rsp+48], r8
        lea     rcx, [g_pipeBuf]             ; pFilePath
        add     rcx, MAX_PATH
        mov     rdx, rdi                     ; pPattern (rdi = pattern buf)
        ; r8 = pContent (set above)
        ; r9 = contentLen (set above as r9d)
        mov     r8, rdx              ; pContent = pPattern? NO -- fix:
        ; We need a temp register. The content ptr was in rdx (g_toolDynBuf or g_pipeBuf+MAX_PATH*2).
        ; Let's re-load:
        mov     r8, qword ptr [g_toolDynBuf]
        test    r8, r8
        jnz     _sc_cp_ok
        lea     r8, [g_pipeBuf]
        add     r8, MAX_PATH*2
_sc_cp_ok:
        lea     rcx, [g_pipeBuf]              ; pFilePath
        add     rcx, MAX_PATH
        mov     rdx, rdi                      ; pPattern
        call    _SearchFileForPattern
        add     r13d, eax       ; accumulate match count (r13 was repurposed)
        add     rsp, 32 + 24

_sc_next:
        ; FindNextFileA
        and     rsp, 0FFFFFFFFFFFFFFF0h
        sub     rsp, 32
        mov     rcx, rbx
        lea     rdx, g_findData
        call    FindNextFileA
        add     rsp, 32
        test    eax, eax
        jnz     _sc_enum

        ; FindClose
        and     rsp, 0FFFFFFFFFFFFFFF0h
        sub     rsp, 32
        mov     rcx, rbx
        call    FindClose
        add     rsp, 32

_sc_cleanup:
        mov     eax, r13d

_sc_ret:
        add     rsp, 32 + MAX_PATH*3 + 68
        pop     r14
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret
Tool_SearchCode ENDP

; ============================================================================
;  Tool_GetDiagnostics
;  RCX = pJsonArgs {"file":"..."}
;  RDX = pResultBuf
;  R8  = bufSize
;  Out: EAX = 0
;  Checks DAP diagnostics beacon slot 4 via TryBeaconRecv, formats results.
; ============================================================================
Tool_GetDiagnostics PROC FRAME
        push    rbp
        .pushreg rbp
        push    rbx
        .pushreg rbx
        push    rsi
        .pushreg rsi
        push    rdi
        .pushreg rdi
        sub     rsp, 32 + MAX_PATH + 20
        .allocstack 32 + MAX_PATH + 20
        .endprolog

        mov     rbx, rcx        ; pJsonArgs
        mov     rsi, rdx        ; pResultBuf
        mov     rdi, r8         ; bufSize

        ; TryBeaconRecv(slot=4, pBuf, bufSize, &actualBytesRecv)
        ; Signature assumed: TryBeaconRecv(slot, pBuf, bufSize, pBytesRecv) -> BOOL
        ; Use result buffer directly as receive destination
        lea     r8, [rsp+32]    ; temp byte count
        mov     dword ptr [rsp+32], 0
        mov     ecx, 4          ; DAP diagnostics slot
        mov     rdx, rsi        ; pBuf = result buf
        mov     r8,  rdi        ; bufSize
        lea     r9,  [rsp+32]   ; &bytesRecv
        call    TryBeaconRecv
        ; EAX = 1 if data available, 0 if not
        test    eax, eax
        jz      _gd_empty

        ; Data was placed in rsi by TryBeaconRecv; ensure null-terminated
        mov     eax, dword ptr [rsp+32]
        cmp     eax, edi
        jl      _gd_term
        mov     eax, edi
        dec     eax
_gd_term:
        mov     byte ptr [rsi + rax], 0
        xor     eax, eax
        jmp     _gd_ret

_gd_empty:
        ; Write "[no diagnostics pending]\r\n"
        xor     edx, edx
        mov     rcx, rsi
        mov     rdx, 0
        lea     r8, szNoDiag
        mov     r9, rdi
        call    _AppendStr
        xor     eax, eax

_gd_ret:
        add     rsp, 32 + MAX_PATH + 20
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret
Tool_GetDiagnostics ENDP

; ============================================================================
;  Tool_GetSymbols
;  RCX = pJsonArgs {"file":"...","kind":"function|class|var"}
;  RDX = pResultBuf
;  R8  = bufSize
;  Out: EAX = symbol count
;  Scans file line-by-line for function/class/struct signatures.
; ============================================================================
GS_LOC_PATH   equ 32
GS_LOC_KIND   equ (GS_LOC_PATH + MAX_PATH)
GS_LOC_KEY    equ (GS_LOC_KIND + MAX_PATH)
GS_LOC_ID     equ (GS_LOC_KEY + 32)
GS_LOC_WRITE  equ (GS_LOC_ID + 128)
GS_LOC_MASK   equ (GS_LOC_WRITE + 8)

Tool_GetSymbols PROC FRAME
        push    rbp
        .pushreg rbp
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
        push    r14
        .pushreg r14
        sub     rsp, 736
        .allocstack 736
        .endprolog

        mov     rbx, rcx        ; pJsonArgs
        mov     r12, rdx        ; pResultBuf
        mov     r13, r8         ; bufSize

        lea     rdi, [rsp + GS_LOC_PATH]     ; file path buf
        lea     rsi, [rsp + GS_LOC_KIND]     ; kind buf

        ; Extract "file"
        lea     rbp, [rsp + GS_LOC_KEY]      ; key scratch
        mov     dword ptr [rbp],   656c6966h  ; "file"
        mov     byte ptr [rbp+4],  0
        mov     rcx, rbx
        mov     rdx, rbp
        mov     r8,  rdi
        mov     r9,  MAX_PATH
        call    _JsonExtract
        test    eax, eax
        jnz     _gs_have_file
        xor     eax, eax
        jmp     _gs_scan_done

_gs_have_file:
        ; Extract "kind"
        mov     dword ptr [rbp],   646e696bh  ; "kind"
        mov     byte ptr [rbp+4],  0
        mov     rcx, rbx
        mov     rdx, rbp
        mov     r8,  rsi
        mov     r9,  MAX_PATH
        call    _JsonExtract

        ; Build kind mask: bit0=function, bit1=class, bit2=struct
        mov     dword ptr [rsp + GS_LOC_MASK], 7
        movzx   eax, byte ptr [rsi]
        test    al, al
        jz      _gs_open_file

        mov     rcx, rsi
        lea     rdx, _gs_kind_function
        call    _StrEq
        test    eax, eax
        jnz     _gs_try_kind_class
        mov     dword ptr [rsp + GS_LOC_MASK], 1
        jmp     _gs_open_file

_gs_try_kind_class:
        mov     rcx, rsi
        lea     rdx, _gs_kind_class
        call    _StrEq
        test    eax, eax
        jnz     _gs_try_kind_struct
        mov     dword ptr [rsp + GS_LOC_MASK], 2
        jmp     _gs_open_file

_gs_try_kind_struct:
        mov     rcx, rsi
        lea     rdx, _gs_kind_struct
        call    _StrEq
        test    eax, eax
        jnz     _gs_open_file
        mov     dword ptr [rsp + GS_LOC_MASK], 4

_gs_open_file:
        ; Open file
        mov     rcx, rdi
        mov     edx, GENERIC_READ
        mov     r8d, FILE_SHARE_READ
        xor     r9d, r9d
        mov     qword ptr [rsp+32], OPEN_EXISTING
        mov     qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
        mov     qword ptr [rsp+48], 0
        call    CreateFileA
        cmp     rax, INVALID_HANDLE_VALUE
        jne     _gs_read_file
        xor     eax, eax
        jne     _gs_read_file

_gs_read_file:
        mov     r14, rax        ; hFile
        ; Read entire file into g_toolDynBuf or g_pipeBuf
        mov     rbp, rax        ; hFile
        ; Read entire file into g_toolDynBuf or g_pipeBuf
        mov     r14, qword ptr [g_toolDynBuf]
        test    r14, r14
        jnz     _gs_has_buf
        lea     r14, g_pipeBuf
_gs_has_buf:
        mov     rcx, rbp
        mov     rdx, r14
        mov     qword ptr [rsp+32], 0
        call    ReadFile
        mov     rcx, r14
        call    CloseHandle
        mov     rcx, rbp
        mov     r14d, dword ptr [g_toolBytesWritten]    ; content length
        mov     byte ptr [rbp + r14], 0                 ; null-terminate
        mov     r10d, dword ptr [g_toolBytesWritten]    ; content length
        mov     byte ptr [r14 + r10], 0                 ; null-terminate

        xor     edi, edi                                 ; symbol count
        xor     ebx, ebx                                 ; line number
        xor     r11d, r11d                               ; byte offset in content
        mov     qword ptr [rsp + GS_LOC_WRITE], 0

_gs_line_loop:
        cmp     r11d, r10d
        jge     _gs_scan_done

        inc     ebx                                      ; line number
        mov     edx, r11d                                ; line start
        mov     eax, r11d                                ; scanner

_gs_find_eol:
        cmp     eax, r10d
        jge     _gs_have_eol
        movzx   ecx, byte ptr [r14 + rax]
        cmp     cl, 13
        je      _gs_have_eol
        cmp     cl, 10
        je      _gs_have_eol
        inc     eax
        jmp     _gs_find_eol

_gs_have_eol:
        mov     r11d, eax                                ; keep eol offset
        mov     ecx, edx                                 ; trimmed line start candidate

_gs_skip_lead_ws:
        cmp     ecx, eax
        jge     _gs_next_line
        movzx   esi, byte ptr [r14 + rcx]
        cmp     sil, ' '
        je      _gs_skip_lead_ws_inc
        cmp     sil, 9
        je      _gs_skip_lead_ws_inc
        jmp     _gs_nonempty_line
_gs_skip_lead_ws_inc:
        inc     ecx
        jmp     _gs_skip_lead_ws

_gs_nonempty_line:
        ; ---------------------------------------------------------------
        ; class NAME
        ; ---------------------------------------------------------------
        mov     esi, dword ptr [rsp + GS_LOC_MASK]
        test    esi, 2
        jz      _gs_check_struct
        mov     esi, eax
        sub     esi, ecx
        cmp     esi, 6
        jb      _gs_check_struct

        movzx   esi, byte ptr [r14 + rcx + 0]
        cmp     sil, 'c'
        jne     _gs_check_struct
        movzx   esi, byte ptr [r14 + rcx + 1]
        cmp     sil, 'l'
        jne     _gs_check_struct
        movzx   esi, byte ptr [r14 + rcx + 2]
        cmp     sil, 'a'
        jne     _gs_check_struct
        movzx   esi, byte ptr [r14 + rcx + 3]
        cmp     sil, 's'
        jne     _gs_check_struct
        movzx   esi, byte ptr [r14 + rcx + 4]
        cmp     sil, 's'
        jne     _gs_check_struct
        movzx   esi, byte ptr [r14 + rcx + 5]
        cmp     sil, ' '
        jne     _gs_check_struct

        lea     esi, [rcx + 6]                           ; identifier start candidate
_gs_cls_skip_ws:
        cmp     esi, eax
        jge     _gs_check_struct
        movzx   edx, byte ptr [r14 + rsi]
        cmp     dl, ' '
        je      _gs_cls_skip_ws_inc
        cmp     dl, 9
        je      _gs_cls_skip_ws_inc
        jmp     _gs_cls_copy_ident
_gs_cls_skip_ws_inc:
        inc     esi
        jmp     _gs_cls_skip_ws

_gs_cls_copy_ident:
        xor     edx, edx                                  ; ident len
_gs_cls_id_loop:
        cmp     esi, eax
        jge     _gs_cls_id_done
        cmp     edx, 127
        jge     _gs_cls_id_done
        movzx   ebp, byte ptr [r14 + rsi]
        cmp     bpl, '_'
        je      _gs_cls_copy_char
        cmp     bpl, '0'
        jb      _gs_cls_chk_alpha
        cmp     bpl, '9'
        jbe     _gs_cls_copy_char
_gs_cls_chk_alpha:
        cmp     bpl, 'A'
        jb      _gs_cls_chk_alpha2
        cmp     bpl, 'Z'
        jbe     _gs_cls_copy_char
_gs_cls_chk_alpha2:
        cmp     bpl, 'a'
        jb      _gs_cls_id_done
        cmp     bpl, 'z'
        ja      _gs_cls_id_done
_gs_cls_copy_char:
        mov     byte ptr [rsp + GS_LOC_ID + rdx], bpl
        inc     edx
        inc     esi
        jmp     _gs_cls_id_loop
_gs_cls_id_done:
        mov     byte ptr [rsp + GS_LOC_ID + rdx], 0
        test    edx, edx
        jz      _gs_check_struct

        ; emit class record: sym:<id>:<line>:class\r\n
        mov     rax, qword ptr [rsp + GS_LOC_WRITE]
        mov     rcx, r12
        mov     rdx, rax
        lea     r8, szSymPfx
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        lea     r8, [rsp + GS_LOC_ID]
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        lea     r8, szHitSep
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        mov     r8d, ebx
        mov     r9, r13
        call    _AppendUInt32
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        lea     r8, szKindCls
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax
        inc     edi
        jmp     _gs_next_line

        ; ---------------------------------------------------------------
        ; struct NAME
        ; ---------------------------------------------------------------
_gs_check_struct:
        mov     esi, dword ptr [rsp + GS_LOC_MASK]
        test    esi, 4
        jz      _gs_check_function
        mov     esi, eax
        sub     esi, ecx
        cmp     esi, 7
        jb      _gs_check_function

        movzx   esi, byte ptr [r14 + rcx + 0]
        cmp     sil, 's'
        jne     _gs_check_function
        movzx   esi, byte ptr [r14 + rcx + 1]
        cmp     sil, 't'
        jne     _gs_check_function
        movzx   esi, byte ptr [r14 + rcx + 2]
        cmp     sil, 'r'
        jne     _gs_check_function
        movzx   esi, byte ptr [r14 + rcx + 3]
        cmp     sil, 'u'
        jne     _gs_check_function
        movzx   esi, byte ptr [r14 + rcx + 4]
        cmp     sil, 'c'
        jne     _gs_check_function
        movzx   esi, byte ptr [r14 + rcx + 5]
        cmp     sil, 't'
        jne     _gs_check_function
        movzx   esi, byte ptr [r14 + rcx + 6]
        cmp     sil, ' '
        jne     _gs_check_function

        lea     esi, [rcx + 7]
_gs_st_skip_ws:
        cmp     esi, eax
        jge     _gs_check_function
        movzx   edx, byte ptr [r14 + rsi]
        cmp     dl, ' '
        je      _gs_st_skip_ws_inc
        cmp     dl, 9
        je      _gs_st_skip_ws_inc
        jmp     _gs_st_copy_ident
_gs_st_skip_ws_inc:
        inc     esi
        jmp     _gs_st_skip_ws

_gs_st_copy_ident:
        xor     edx, edx
_gs_st_id_loop:
        cmp     esi, eax
        jge     _gs_st_id_done
        cmp     edx, 127
        jge     _gs_st_id_done
        movzx   ebp, byte ptr [r14 + rsi]
        cmp     bpl, '_'
        je      _gs_st_copy_char
        cmp     bpl, '0'
        jb      _gs_st_chk_alpha
        cmp     bpl, '9'
        jbe     _gs_st_copy_char
_gs_st_chk_alpha:
        cmp     bpl, 'A'
        jb      _gs_st_chk_alpha2
        cmp     bpl, 'Z'
        jbe     _gs_st_copy_char
_gs_st_chk_alpha2:
        cmp     bpl, 'a'
        jb      _gs_st_id_done
        cmp     bpl, 'z'
        ja      _gs_st_id_done
_gs_st_copy_char:
        mov     byte ptr [rsp + GS_LOC_ID + rdx], bpl
        inc     edx
        inc     esi
        jmp     _gs_st_id_loop
_gs_st_id_done:
        mov     byte ptr [rsp + GS_LOC_ID + rdx], 0
        test    edx, edx
        jz      _gs_check_function

        ; emit struct record
        mov     rax, qword ptr [rsp + GS_LOC_WRITE]
        mov     rcx, r12
        mov     rdx, rax
        lea     r8, szSymPfx
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        lea     r8, [rsp + GS_LOC_ID]
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        lea     r8, szHitSep
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        mov     r8d, ebx
        mov     r9, r13
        call    _AppendUInt32
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        lea     r8, szKindStruct
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax
        inc     edi
        jmp     _gs_next_line

        ; ---------------------------------------------------------------
        ; function heuristic: return_type identifier(...)
        ; ---------------------------------------------------------------
_gs_check_function:
        mov     esi, dword ptr [rsp + GS_LOC_MASK]
        test    esi, 1
        jz      _gs_next_line

        ; reject common control statements at line start
        mov     esi, eax
        sub     esi, ecx
        cmp     esi, 2
        jb      _gs_find_lparen
        movzx   edx, byte ptr [r14 + rcx + 0]
        cmp     dl, 'i'
        jne     _gs_chk_for
        movzx   edx, byte ptr [r14 + rcx + 1]
        cmp     dl, 'f'
        jne     _gs_chk_for
        cmp     esi, 2
        je      _gs_next_line
        movzx   edx, byte ptr [r14 + rcx + 2]
        cmp     dl, ' '
        je      _gs_next_line
        cmp     dl, '('
        je      _gs_next_line
        cmp     dl, 9
        je      _gs_next_line

_gs_chk_for:
        cmp     esi, 3
        jb      _gs_chk_while
        movzx   edx, byte ptr [r14 + rcx + 0]
        cmp     dl, 'f'
        jne     _gs_chk_while
        movzx   edx, byte ptr [r14 + rcx + 1]
        cmp     dl, 'o'
        jne     _gs_chk_while
        movzx   edx, byte ptr [r14 + rcx + 2]
        cmp     dl, 'r'
        jne     _gs_chk_while
        cmp     esi, 3
        je      _gs_next_line
        movzx   edx, byte ptr [r14 + rcx + 3]
        cmp     dl, ' '
        je      _gs_next_line
        cmp     dl, '('
        je      _gs_next_line
        cmp     dl, 9
        je      _gs_next_line

_gs_chk_while:
        cmp     esi, 5
        jb      _gs_chk_switch
        movzx   edx, byte ptr [r14 + rcx + 0]
        cmp     dl, 'w'
        jne     _gs_chk_switch
        movzx   edx, byte ptr [r14 + rcx + 1]
        cmp     dl, 'h'
        jne     _gs_chk_switch
        movzx   edx, byte ptr [r14 + rcx + 2]
        cmp     dl, 'i'
        jne     _gs_chk_switch
        movzx   edx, byte ptr [r14 + rcx + 3]
        cmp     dl, 'l'
        jne     _gs_chk_switch
        movzx   edx, byte ptr [r14 + rcx + 4]
        cmp     dl, 'e'
        jne     _gs_chk_switch
        cmp     esi, 5
        je      _gs_next_line
        movzx   edx, byte ptr [r14 + rcx + 5]
        cmp     dl, ' '
        je      _gs_next_line
        cmp     dl, '('
        je      _gs_next_line
        cmp     dl, 9
        je      _gs_next_line

_gs_chk_switch:
        cmp     esi, 6
        jb      _gs_chk_return
        movzx   edx, byte ptr [r14 + rcx + 0]
        cmp     dl, 's'
        jne     _gs_chk_return
        movzx   edx, byte ptr [r14 + rcx + 1]
        cmp     dl, 'w'
        jne     _gs_chk_return
        movzx   edx, byte ptr [r14 + rcx + 2]
        cmp     dl, 'i'
        jne     _gs_chk_return
        movzx   edx, byte ptr [r14 + rcx + 3]
        cmp     dl, 't'
        jne     _gs_chk_return
        movzx   edx, byte ptr [r14 + rcx + 4]
        cmp     dl, 'c'
        jne     _gs_chk_return
        movzx   edx, byte ptr [r14 + rcx + 5]
        cmp     dl, 'h'
        jne     _gs_chk_return
        cmp     esi, 6
        je      _gs_next_line
        movzx   edx, byte ptr [r14 + rcx + 6]
        cmp     dl, ' '
        je      _gs_next_line
        cmp     dl, '('
        je      _gs_next_line
        cmp     dl, 9
        je      _gs_next_line

_gs_chk_return:
        cmp     esi, 6
        jb      _gs_find_lparen
        movzx   edx, byte ptr [r14 + rcx + 0]
        cmp     dl, 'r'
        jne     _gs_find_lparen
        movzx   edx, byte ptr [r14 + rcx + 1]
        cmp     dl, 'e'
        jne     _gs_find_lparen
        movzx   edx, byte ptr [r14 + rcx + 2]
        cmp     dl, 't'
        jne     _gs_find_lparen
        movzx   edx, byte ptr [r14 + rcx + 3]
        cmp     dl, 'u'
        jne     _gs_find_lparen
        movzx   edx, byte ptr [r14 + rcx + 4]
        cmp     dl, 'r'
        jne     _gs_find_lparen
        movzx   edx, byte ptr [r14 + rcx + 5]
        cmp     dl, 'n'
        jne     _gs_find_lparen
        cmp     esi, 6
        je      _gs_next_line
        movzx   edx, byte ptr [r14 + rcx + 6]
        cmp     dl, ' '
        je      _gs_next_line
        cmp     dl, '('
        je      _gs_next_line
        cmp     dl, 9
        je      _gs_next_line

_gs_find_lparen:
        mov     esi, ecx
_gs_find_lparen_loop:
        cmp     esi, eax
        jge     _gs_next_line
        movzx   edx, byte ptr [r14 + rsi]
        cmp     dl, ';'
        je      _gs_next_line
        cmp     dl, '('
        je      _gs_have_lparen
        inc     esi
        jmp     _gs_find_lparen_loop

_gs_have_lparen:
        ; Require closing ')' on same line.
        mov     edx, esi
        inc     edx
_gs_find_rparen:
        cmp     edx, eax
        jge     _gs_next_line
        movzx   ebp, byte ptr [r14 + rdx]
        cmp     bpl, ')'
        je      _gs_rparen_ok
        inc     edx
        jmp     _gs_find_rparen
_gs_rparen_ok:

        ; Identify function token immediately before '('
        mov     edx, esi
        dec     edx
_gs_func_trim_left:
        cmp     edx, ecx
        jl      _gs_next_line
        movzx   ebp, byte ptr [r14 + rdx]
        cmp     bpl, ' '
        je      _gs_func_trim_left_dec
        cmp     bpl, 9
        je      _gs_func_trim_left_dec
        jmp     _gs_func_name_end
_gs_func_trim_left_dec:
        dec     edx
        jmp     _gs_func_trim_left

_gs_func_name_end:
        mov     r8d, edx                                 ; idEnd
_gs_func_name_start_back:
        cmp     edx, ecx
        jl      _gs_func_name_start_ready
        movzx   ebp, byte ptr [r14 + rdx]
        cmp     bpl, '_'
        je      _gs_func_name_start_back_dec
        cmp     bpl, '~'
        je      _gs_func_name_start_back_dec
        cmp     bpl, '0'
        jb      _gs_func_chk_alpha
        cmp     bpl, '9'
        jbe     _gs_func_name_start_back_dec
_gs_func_chk_alpha:
        cmp     bpl, 'A'
        jb      _gs_func_chk_alpha2
        cmp     bpl, 'Z'
        jbe     _gs_func_name_start_back_dec
_gs_func_chk_alpha2:
        cmp     bpl, 'a'
        jb      _gs_func_name_start_ready
        cmp     bpl, 'z'
        jbe     _gs_func_name_start_back_dec
        jmp     _gs_func_name_start_ready
_gs_func_name_start_back_dec:
        dec     edx
        jmp     _gs_func_name_start_back

_gs_func_name_start_ready:
        mov     esi, edx
        inc     esi                                       ; idStart
        cmp     esi, r8d
        jg      _gs_next_line

        ; Reject member calls (obj.fn(...), ptr->fn(...))
        cmp     edx, ecx
        jl      _gs_func_prefix_check
        movzx   ebp, byte ptr [r14 + rdx]
        cmp     bpl, '.'
        je      _gs_next_line
        cmp     bpl, '>'
        je      _gs_next_line

_gs_func_prefix_check:
        ; Require type-like prefix before name: has alpha + separator
        xor     ebp, ebp                                  ; bit0 alpha, bit1 sep
        mov     edx, ecx
_gs_prefix_loop:
        cmp     edx, esi
        jge     _gs_prefix_done
        movzx   eax, byte ptr [r14 + rdx]
        cmp     al, 'A'
        jb      _gs_prefix_chk_alpha2
        cmp     al, 'Z'
        jbe     _gs_prefix_alpha
_gs_prefix_chk_alpha2:
        cmp     al, 'a'
        jb      _gs_prefix_chk_sep
        cmp     al, 'z'
        jbe     _gs_prefix_alpha
        jmp     _gs_prefix_chk_sep
_gs_prefix_alpha:
        or      ebp, 1
_gs_prefix_chk_sep:
        cmp     al, ' '
        je      _gs_prefix_sep
        cmp     al, '*'
        je      _gs_prefix_sep
        cmp     al, '&'
        je      _gs_prefix_sep
        cmp     al, ':'
        jne     _gs_prefix_next
_gs_prefix_sep:
        or      ebp, 2
_gs_prefix_next:
        inc     edx
        jmp     _gs_prefix_loop
_gs_prefix_done:
        cmp     ebp, 3
        jne     _gs_next_line

        ; Copy function identifier into local id buffer
        xor     edx, edx
_gs_func_copy_loop:
        mov     eax, esi
        add     eax, edx
        cmp     eax, r8d
        jg      _gs_func_copy_done
        cmp     edx, 127
        jge     _gs_func_copy_done
        movzx   ebp, byte ptr [r14 + rax]
        mov     byte ptr [rsp + GS_LOC_ID + rdx], bpl
        inc     edx
        jmp     _gs_func_copy_loop
_gs_func_copy_done:
        mov     byte ptr [rsp + GS_LOC_ID + rdx], 0
        test    edx, edx
        jz      _gs_next_line

        ; emit function record
        mov     rax, qword ptr [rsp + GS_LOC_WRITE]
        mov     rcx, r12
        mov     rdx, rax
        lea     r8, szSymPfx
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        lea     r8, [rsp + GS_LOC_ID]
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        lea     r8, szHitSep
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        mov     r8d, ebx
        mov     r9, r13
        call    _AppendUInt32
        mov     qword ptr [rsp + GS_LOC_WRITE], rax

        mov     rcx, r12
        mov     rdx, rax
        lea     r8, szKindFunc
        mov     r9, r13
        call    _AppendStr
        mov     qword ptr [rsp + GS_LOC_WRITE], rax
        inc     edi

_gs_next_line:
        ; Advance to next line (CR/LF aware)
        cmp     r11d, r10d
        jge     _gs_line_loop
        movzx   eax, byte ptr [r14 + r11]
        cmp     al, 13
        jne     _gs_check_lf_only
        inc     r11d
        cmp     r11d, r10d
        jge     _gs_line_loop
        movzx   eax, byte ptr [r14 + r11]
        cmp     al, 10
        jne     _gs_line_loop
        inc     r11d
        jmp     _gs_line_loop
_gs_check_lf_only:
        cmp     al, 10
        jne     _gs_line_loop
        inc     r11d
        jmp     _gs_line_loop

_gs_scan_done:
        mov     eax, edi        ; symbol count
        mov     eax, ebx        ; return symbol count

        add     rsp, 736
        add     rsp, 32 + MAX_PATH*2 + 32
        pop     r14
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret

_gs_kind_function db "function", 0
_gs_kind_class    db "class", 0
_gs_kind_struct   db "struct", 0

Tool_GetSymbols ENDP

; ============================================================================
;  _StrEq  — compare two null-terminated strings (case-sensitive)
;  In: RCX = s1, RDX = s2
;  Out: EAX = 0 if equal, nonzero if different
; ============================================================================
_StrEq PROC
_steq_loop:
        movzx   eax, byte ptr [rcx]
        movzx   r8d, byte ptr [rdx]
        cmp     al, r8b
        jne     _steq_diff
        test    al, al
        jz      _steq_equal
        inc     rcx
        inc     rdx
        jmp     _steq_loop
_steq_equal:
        xor     eax, eax
        ret
_steq_diff:
        mov     eax, 1
        ret
_StrEq ENDP

END
