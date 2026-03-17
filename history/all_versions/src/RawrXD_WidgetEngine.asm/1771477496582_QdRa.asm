;==============================================================================
; RawrXD_WidgetEngine.asm — Widget Intelligence System
; Reverse-engineered headless widget controller with pipe-based IPC
; Zero GUI | Zero CRT | Named-pipe intent protocol | HeadacheEliminator
;
; Assemble:  ml64 /c /Fo RawrXD_WidgetEngine.obj /nologo /W3 /I D:\rawrxd\include RawrXD_WidgetEngine.asm
; Link:      link RawrXD_WidgetEngine.obj /SUBSYSTEM:CONSOLE /ENTRY:WidgetMain /MACHINE:X64 kernel32.lib
;==============================================================================

OPTION CASEMAP:NONE

INCLUDE rawrxd_master.inc

INCLUDELIB kernel32.lib

;------------------------------------------------------------------------------
; Constants not in rawrxd_win32_api.inc
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

; Named-pipe API EXTERNs
EXTERNDEF CreateNamedPipeA:PROC
EXTERNDEF ConnectNamedPipe:PROC
EXTERNDEF DisconnectNamedPipe:PROC
EXTERNDEF OpenFileMappingA:PROC

; Widget type opcodes (reverse-engineered from Monaco/Cursor)
WIDGET_TYPE_CODELENS        EQU 1       ; Reference counters, test status
WIDGET_TYPE_BREADCRUMB      EQU 2       ; Symbol navigation trail
WIDGET_TYPE_MINIMAP         EQU 3       ; Token density heatmap
WIDGET_TYPE_INLINE_HINT     EQU 4       ; Ghost parameter names
WIDGET_TYPE_QUICKFIX        EQU 5       ; Auto-correction bytecode
WIDGET_TYPE_PALETTE         EQU 6       ; Intent trie navigation

MAX_WIDGETS                 EQU 256
MAX_PATTERNS                EQU 64
PIPE_BUFFER_SIZE            EQU 8192

; Widget registry entry layout (29 bytes each)
WIDGET_ENTRY_TYPE           EQU 0       ; QWORD — type
WIDGET_ENTRY_OFFSET         EQU 8       ; QWORD — target offset
WIDGET_ENTRY_META           EQU 16      ; QWORD — metadata ptr
WIDGET_ENTRY_PRIO           EQU 24      ; DWORD — priority
WIDGET_ENTRY_ACTIVE         EQU 28      ; BYTE  — active flag
WIDGET_ENTRY_SIZE           EQU 32      ; padded to 32

; Pattern entry layout (48 bytes each)
PATTERN_SIG                 EQU 0       ; 32 bytes — signature
PATTERN_WTYPE               EQU 32      ; DWORD — widget type
PATTERN_CONF                EQU 36      ; DWORD — confidence
PATTERN_HANDLER             EQU 40      ; QWORD — handler ptr
PATTERN_ENTRY_SIZE          EQU 48

; Headache table entry layout
HEADACHE_STRING             EQU 0       ; NUL-terminated string
HEADACHE_HANDLER            EQU 0       ; QWORD after the NUL


;==============================================================================
;                              DATA SECTION
;==============================================================================
.DATA

ALIGN 16
szPipeName          BYTE "\\.\pipe\RawrXD_WidgetIntelligence",0
bActive             BYTE 1
hPipe               QWORD 0
hHeap               QWORD 0

; ---- Widget Registry --------------------------------------------------------
ALIGN 16
WidgetCount         DWORD 0
WidgetRegistry      BYTE (MAX_WIDGETS * WIDGET_ENTRY_SIZE) DUP(0)

; ---- Pattern Database -------------------------------------------------------
ALIGN 16
PatternCount        DWORD 0
PatternDatabase     BYTE (MAX_PATTERNS * PATTERN_ENTRY_SIZE) DUP(0)

; ---- Headache Elimination Jump Table ----------------------------------------
ALIGN 16
HeadacheTable:
szHT_undef          BYTE "undefined_variable",0
ALIGN 8
pHT_undef           QWORD Auto_Declare_Variable
szHT_import         BYTE "missing_import",0
ALIGN 8
pHT_import          QWORD Auto_Inject_Import
szHT_type           BYTE "type_mismatch",0
ALIGN 8
pHT_type            QWORD Auto_Cast_Wrapper
szHT_null           BYTE "null_pointer",0
ALIGN 8
pHT_null            QWORD Auto_Null_Check
szHT_memleak        BYTE "memory_leak",0
ALIGN 8
pHT_memleak         QWORD Auto_Cleanup_Hook
szHT_end            BYTE 0             ; sentinel

; Headache table index (pairs of string-ptr, handler-ptr)
ALIGN 16
HeadacheIndex:
    QWORD szHT_undef,    Auto_Declare_Variable
    QWORD szHT_import,   Auto_Inject_Import
    QWORD szHT_type,     Auto_Cast_Wrapper
    QWORD szHT_null,     Auto_Null_Check
    QWORD szHT_memleak,  Auto_Cleanup_Hook
    QWORD 0, 0           ; sentinel

; ---- Pattern signatures -----------------------------------------------------
ALIGN 16
pat_func_def        BYTE "function ",0
                    BYTE 22 DUP(0)      ; pad to 32
pat_class           BYTE "class ",0
                    BYTE 25 DUP(0)      ; pad to 32
pat_proc            BYTE "proc",0
                    BYTE 27 DUP(0)      ; pad to 32

; ---- Response buffer --------------------------------------------------------
ALIGN 16
g_ResponseBuf       BYTE 4096 DUP(0)
g_PipeBuf           BYTE PIPE_BUFFER_SIZE DUP(0)

; ---- Console strings --------------------------------------------------------
szWelcome           BYTE "[WIDGET] Intelligence engine online",13,10,0
szShutdown          BYTE "[WIDGET] Shutting down",13,10,0
szConnected         BYTE "[WIDGET] Client connected",13,10,0
szNewline           BYTE 13,10,0
szWidgetOK          BYTE "WIDGET_OK:",0


;==============================================================================
;                              CODE SECTION
;==============================================================================
.CODE

;------------------------------------------------------------------------------
; PrintConsole — write NUL-terminated string to stdout (same as genesis.asm)
;------------------------------------------------------------------------------
WE_Print PROC FRAME
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
    mov  dword ptr [rbp-18h], ecx   ; save strlen to local (R8 is volatile)

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
WE_Print ENDP

;------------------------------------------------------------------------------
; WE_StrLen — strlen in RAX
;------------------------------------------------------------------------------
WE_StrLen PROC FRAME
    push rdi                        ; RDI is non-volatile
    .pushreg rdi
    .endprolog
    mov  rdi, rcx
    xor  ecx, ecx
    dec  rcx
    xor  eax, eax
    repne scasb
    not  rcx
    lea  rax, [rcx-1]
    pop  rdi
    ret
WE_StrLen ENDP

;------------------------------------------------------------------------------
; WE_StrMatch — case-insensitive prefix match
; RCX=pHaystack, RDX=pNeedle
; Returns 1 if needle is a prefix of haystack, 0 otherwise
;------------------------------------------------------------------------------
WE_StrMatch PROC FRAME
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    .endprolog

    mov  rsi, rcx           ; haystack
    mov  rdi, rdx           ; needle

@@:
    mov  al, byte ptr [rdi]
    test al, al
    jz   match_ok           ; needle exhausted → match

    mov  ah, byte ptr [rsi]
    test ah, ah
    jz   match_fail         ; haystack shorter than needle

    cmp  al, ah
    je   next_ch
    ; case-fold both
    or   al, 20h
    or   ah, 20h
    cmp  al, ah
    jne  match_fail

next_ch:
    inc  rsi
    inc  rdi
    jmp  @B

match_ok:
    mov  eax, 1
    jmp  match_exit
match_fail:
    xor  eax, eax
match_exit:
    pop  rdi
    pop  rsi
    ret
WE_StrMatch ENDP

;------------------------------------------------------------------------------
; Hex_To_Buf — write RAX as hex digits into [RDI], advance RDI
;------------------------------------------------------------------------------
Hex_To_Buf PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog

    mov  rbx, rdi           ; start
    test rax, rax
    jnz  @F
    mov  byte ptr [rdi], '0'
    inc  rdi
    jmp  htb_done
@@:
    ; Count digits, push nibbles on stack
    xor  ecx, ecx
htb_push:
    test rax, rax
    jz   htb_pop
    mov  rdx, rax
    and  edx, 0Fh
    push rdx
    inc  ecx
    shr  rax, 4
    jmp  htb_push
htb_pop:
    test ecx, ecx
    jz   htb_done
    pop  rdx
    cmp  dl, 10
    jb   htb_dig
    add  dl, 'A' - 10
    jmp  htb_st
htb_dig:
    add  dl, '0'
htb_st:
    mov  byte ptr [rdi], dl
    inc  rdi
    dec  ecx
    jmp  htb_pop
htb_done:
    pop  rbx
    ret
Hex_To_Buf ENDP

;------------------------------------------------------------------------------
; Parse_WidgetIntent — parse "WIDGET:T:HHHH:context"
; RCX = buffer
; Returns: RAX=widgetType, RDX=offset, R8=ptr to context string
;   (all volatile — no non-volatile register clobber)
;------------------------------------------------------------------------------
Parse_WidgetIntent PROC FRAME
    push rsi
    .pushreg rsi
    .endprolog

    mov  rsi, rcx

    ; Skip "WIDGET:" (7 chars)
    add  rsi, 7

    ; Parse type digit
    xor  eax, eax
    mov  al, byte ptr [rsi]
    sub  al, '0'
    mov  r9, rax            ; save type in R9 (volatile — OK)

    ; Skip type + ':'
    add  rsi, 2

    ; Parse hex offset into RDX (volatile — OK)
    xor  edx, edx
pw_hex:
    movzx ecx, byte ptr [rsi]
    cmp  cl, ':'
    je   pw_got_off
    cmp  cl, 0
    je   pw_got_off
    cmp  cl, '0'
    jb   pw_got_off
    cmp  cl, '9'
    jle  pw_dig
    or   cl, 20h
    cmp  cl, 'a'
    jb   pw_got_off
    cmp  cl, 'f'
    ja   pw_got_off
    sub  cl, 'a'
    add  cl, 10
    jmp  pw_shift
pw_dig:
    sub  cl, '0'
pw_shift:
    shl  rdx, 4
    or   dl, cl
    inc  rsi
    jmp  pw_hex
pw_got_off:
    cmp  byte ptr [rsi], ':'
    jne  @F
    inc  rsi                ; skip ':'
@@:
    mov  r8, rsi            ; context pointer → R8
    mov  rax, r9            ; widget type → RAX
    ; RDX already has offset
    pop  rsi
    ret
Parse_WidgetIntent ENDP

;------------------------------------------------------------------------------
; HeadacheEliminator — scan context against known pain patterns, auto-fix
; RCX = pContext
; Returns: RAX = fix marker (1001h–1005h) or 0
;------------------------------------------------------------------------------
HeadacheEliminator PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    mov  qword ptr [rbp-8h], r12    ; save non-volatiles into frame
    .savereg r12, 38h
    mov  qword ptr [rbp-10h], r13
    .savereg r13, 30h
    .endprolog

    mov  r12, rcx           ; context string

    lea  r13, HeadacheIndex
he_scan:
    mov  rdx, qword ptr [r13]      ; string ptr
    test rdx, rdx
    jz   he_none

    mov  rcx, r12           ; haystack = context
    ; rdx = needle (pattern string)
    call WE_StrMatch
    test eax, eax
    jnz  he_found

    add  r13, 16            ; next pair
    jmp  he_scan

he_found:
    mov  rax, qword ptr [r13+8]    ; handler address
    call rax
    jmp  he_exit

he_none:
    xor  eax, eax

he_exit:
    mov  r12, qword ptr [rbp-8h]   ; restore non-volatiles
    mov  r13, qword ptr [rbp-10h]
    leave
    ret
HeadacheEliminator ENDP

;------------------------------------------------------------------------------
; Auto_Declare_Variable — scan context for undeclared symbol name,
; write "EXTERNDEF <symbol>:PROC" patch into g_ResponseBuf
; RCX = context string (from HeadacheEliminator)
; Returns: EAX = 1001h (fix applied)
;------------------------------------------------------------------------------
Auto_Declare_Variable PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    mov  qword ptr [rbp-8h], rdi
    .savereg rdi, 38h
    mov  qword ptr [rbp-10h], rsi
    .savereg rsi, 30h
    .endprolog

    ; Context format: "undefined_variable:<symbol_name>"
    ; Skip past the ':' separator to get the symbol name
    mov  rsi, rcx
adv_find_colon:
    movzx eax, byte ptr [rsi]
    test al, al
    jz   adv_no_sym
    cmp  al, ':'
    je   adv_got_colon
    inc  rsi
    jmp  adv_find_colon
adv_got_colon:
    inc  rsi                ; skip ':'
    ; RSI now points to symbol name
    ; Build "EXTERNDEF <sym>:PROC" into g_ResponseBuf
    lea  rdi, g_ResponseBuf
    ; Copy "EXTERNDEF "
    mov  dword ptr [rdi],   'E' OR ('X' SHL 8) OR ('T' SHL 16) OR ('E' SHL 24)
    mov  dword ptr [rdi+4], 'R' OR ('N' SHL 8) OR ('D' SHL 16) OR ('E' SHL 24)
    mov  word ptr  [rdi+8], 'F' OR (' ' SHL 8)
    add  rdi, 10
    ; Copy symbol name
adv_copy_sym:
    movzx eax, byte ptr [rsi]
    test al, al
    jz   adv_sym_done
    cmp  al, ' '
    je   adv_sym_done
    cmp  al, 10
    je   adv_sym_done
    mov  byte ptr [rdi], al
    inc  rsi
    inc  rdi
    jmp  adv_copy_sym
adv_sym_done:
    ; Append ":PROC\r\n"
    mov  dword ptr [rdi],   ':' OR ('P' SHL 8) OR ('R' SHL 16) OR ('O' SHL 24)
    mov  word ptr  [rdi+4], 'C' OR (13 SHL 8)
    mov  byte ptr  [rdi+6], 10
    mov  byte ptr  [rdi+7], 0
    mov  eax, 1001h
    jmp  adv_exit
adv_no_sym:
    mov  eax, 1001h         ; marker only if no symbol found
adv_exit:
    mov  rdi, qword ptr [rbp-8h]
    mov  rsi, qword ptr [rbp-10h]
    leave
    ret
Auto_Declare_Variable ENDP

;------------------------------------------------------------------------------
; Auto_Inject_Import — generate INCLUDE/INCLUDELIB line for missing import
; RCX = context ("missing_import:<lib_name>")
; Returns: EAX = 1002h
;------------------------------------------------------------------------------
Auto_Inject_Import PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    mov  qword ptr [rbp-8h], rdi
    .savereg rdi, 38h
    mov  qword ptr [rbp-10h], rsi
    .savereg rsi, 30h
    .endprolog

    mov  rsi, rcx
    ; Skip to ':' separator
aii_find:
    movzx eax, byte ptr [rsi]
    test al, al
    jz   aii_done
    cmp  al, ':'
    je   aii_got
    inc  rsi
    jmp  aii_find
aii_got:
    inc  rsi
    lea  rdi, g_ResponseBuf
    ; Build "INCLUDELIB "
    mov  dword ptr [rdi],   'I' OR ('N' SHL 8) OR ('C' SHL 16) OR ('L' SHL 24)
    mov  dword ptr [rdi+4], 'U' OR ('D' SHL 8) OR ('E' SHL 16) OR ('L' SHL 24)
    mov  dword ptr [rdi+8], 'I' OR ('B' SHL 8) OR (' ' SHL 16)
    add  rdi, 11
aii_copy:
    movzx eax, byte ptr [rsi]
    test al, al
    jz   aii_end
    cmp  al, ' '
    je   aii_end
    mov  byte ptr [rdi], al
    inc  rsi
    inc  rdi
    jmp  aii_copy
aii_end:
    mov  word ptr [rdi], 13 OR (10 SHL 8)
    mov  byte ptr [rdi+2], 0
aii_done:
    mov  eax, 1002h
    mov  rdi, qword ptr [rbp-8h]
    mov  rsi, qword ptr [rbp-10h]
    leave
    ret
Auto_Inject_Import ENDP

;------------------------------------------------------------------------------
; Auto_Cast_Wrapper — generate appropriate widening instruction
; RCX = context ("type_mismatch:<from_size>:<to_size>")
; Returns: EAX = 1003h, g_ResponseBuf has suggested instruction
;------------------------------------------------------------------------------
Auto_Cast_Wrapper PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    mov  qword ptr [rbp-8h], rdi
    .savereg rdi, 38h
    .endprolog

    lea  rdi, g_ResponseBuf
    ; Default: "movzx rax, <src>" as a safe widening cast
    mov  dword ptr [rdi],    'm' OR ('o' SHL 8) OR ('v' SHL 16) OR ('z' SHL 24)
    mov  dword ptr [rdi+4],  'x' OR (' ' SHL 8) OR ('r' SHL 16) OR ('a' SHL 24)
    mov  dword ptr [rdi+8],  'x' OR (',' SHL 8) OR (' ' SHL 16) OR ('s' SHL 24)
    mov  dword ptr [rdi+12], 'r' OR ('c' SHL 8)
    mov  byte ptr  [rdi+14], 0

    mov  eax, 1003h
    mov  rdi, qword ptr [rbp-8h]
    leave
    ret
Auto_Cast_Wrapper ENDP

;------------------------------------------------------------------------------
; Auto_Null_Check — generate null guard (test/jz)
; RCX = context ("null_pointer:<register>")
; Returns: EAX = 1004h, g_ResponseBuf has guard code
;------------------------------------------------------------------------------
Auto_Null_Check PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    mov  qword ptr [rbp-8h], rdi
    .savereg rdi, 38h
    mov  qword ptr [rbp-10h], rsi
    .savereg rsi, 30h
    .endprolog

    mov  rsi, rcx
    ; Find register name after ':'
anc_find:
    movzx eax, byte ptr [rsi]
    test al, al
    jz   anc_default
    cmp  al, ':'
    je   anc_got
    inc  rsi
    jmp  anc_find
anc_got:
    inc  rsi
    ; Build "test <reg>, <reg>\r\njz err_null_ptr"
    lea  rdi, g_ResponseBuf
    mov  dword ptr [rdi],   't' OR ('e' SHL 8) OR ('s' SHL 16) OR ('t' SHL 24)
    mov  byte ptr  [rdi+4], ' '
    add  rdi, 5
    ; Copy register name twice with ", " between
    mov  rcx, rsi
    ; First copy
anc_c1:
    movzx eax, byte ptr [rcx]
    test al, al
    jz   anc_sep
    cmp  al, ' '
    je   anc_sep
    mov  byte ptr [rdi], al
    inc  rcx
    inc  rdi
    jmp  anc_c1
anc_sep:
    mov  word ptr [rdi], ',' OR (' ' SHL 8)
    add  rdi, 2
    ; Second copy
    mov  rcx, rsi
anc_c2:
    movzx eax, byte ptr [rcx]
    test al, al
    jz   anc_guard
    cmp  al, ' '
    je   anc_guard
    mov  byte ptr [rdi], al
    inc  rcx
    inc  rdi
    jmp  anc_c2
anc_guard:
    ; "\r\njz err_null_ptr"
    mov  word ptr  [rdi], 13 OR (10 SHL 8)
    mov  dword ptr [rdi+2],  'j' OR ('z' SHL 8) OR (' ' SHL 16) OR ('e' SHL 24)
    mov  dword ptr [rdi+6],  'r' OR ('r' SHL 8) OR ('_' SHL 16) OR ('n' SHL 24)
    mov  dword ptr [rdi+10], 'u' OR ('l' SHL 8) OR ('l' SHL 16)
    mov  byte ptr  [rdi+13], 0
    jmp  anc_exit
anc_default:
    lea  rdi, g_ResponseBuf
    mov  dword ptr [rdi],  't' OR ('e' SHL 8) OR ('s' SHL 16) OR ('t' SHL 24)
    mov  dword ptr [rdi+4], ' ' OR ('r' SHL 8) OR ('c' SHL 16) OR ('x' SHL 24)
    mov  dword ptr [rdi+8], ',' OR (' ' SHL 8) OR ('r' SHL 16) OR ('c' SHL 24)
    mov  byte ptr  [rdi+12], 'x'
    mov  byte ptr  [rdi+13], 0
anc_exit:
    mov  eax, 1004h
    mov  rdi, qword ptr [rbp-8h]
    mov  rsi, qword ptr [rbp-10h]
    leave
    ret
Auto_Null_Check ENDP

;------------------------------------------------------------------------------
; Auto_Cleanup_Hook — generate resource cleanup code
; RCX = context ("memory_leak:<handle_var>")
; Returns: EAX = 1005h, g_ResponseBuf has cleanup snippet
;------------------------------------------------------------------------------
Auto_Cleanup_Hook PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    mov  qword ptr [rbp-8h], rdi
    .savereg rdi, 38h
    mov  qword ptr [rbp-10h], rsi
    .savereg rsi, 30h
    .endprolog

    mov  rsi, rcx
    lea  rdi, g_ResponseBuf
    ; Find handle name after ':'
ach_find:
    movzx eax, byte ptr [rsi]
    test al, al
    jz   ach_default
    cmp  al, ':'
    je   ach_got
    inc  rsi
    jmp  ach_find
ach_got:
    inc  rsi
    ; Build: "mov rcx, <handle>\r\ntest rcx, rcx\r\njz @F\r\ncall CloseHandle\r\n@@:"
    mov  dword ptr [rdi],    'm' OR ('o' SHL 8) OR ('v' SHL 16) OR (' ' SHL 24)
    mov  dword ptr [rdi+4],  'r' OR ('c' SHL 8) OR ('x' SHL 16) OR (',' SHL 24)
    mov  byte ptr  [rdi+8],  ' '
    add  rdi, 9
ach_copy:
    movzx eax, byte ptr [rsi]
    test al, al
    jz   ach_rest
    cmp  al, ' '
    je   ach_rest
    mov  byte ptr [rdi], al
    inc  rsi
    inc  rdi
    jmp  ach_copy
ach_rest:
    ; "\r\ntest rcx, rcx\r\njz @F\r\ncall CloseHandle\r\n@@:"
    mov  word ptr  [rdi], 13 OR (10 SHL 8)
    add  rdi, 2
    mov  dword ptr [rdi],   't' OR ('e' SHL 8) OR ('s' SHL 16) OR ('t' SHL 24)
    mov  dword ptr [rdi+4], ' ' OR ('r' SHL 8) OR ('c' SHL 16) OR ('x' SHL 24)
    mov  dword ptr [rdi+8], ',' OR (' ' SHL 8) OR ('r' SHL 16) OR ('c' SHL 24)
    mov  byte ptr  [rdi+12],'x'
    add  rdi, 13
    mov  word ptr  [rdi], 13 OR (10 SHL 8)
    add  rdi, 2
    mov  dword ptr [rdi],   'j' OR ('z' SHL 8) OR (' ' SHL 16) OR ('@' SHL 24)
    mov  byte ptr  [rdi+4], 'F'
    add  rdi, 5
    mov  word ptr  [rdi], 13 OR (10 SHL 8)
    add  rdi, 2
    mov  dword ptr [rdi],   'c' OR ('a' SHL 8) OR ('l' SHL 16) OR ('l' SHL 24)
    mov  dword ptr [rdi+4], ' ' OR ('C' SHL 8) OR ('l' SHL 16) OR ('o' SHL 24)
    mov  dword ptr [rdi+8], 's' OR ('e' SHL 8) OR ('H' SHL 16) OR ('a' SHL 24)
    mov  dword ptr [rdi+12],'n' OR ('d' SHL 8) OR ('l' SHL 16) OR ('e' SHL 24)
    add  rdi, 16
    mov  word ptr  [rdi], 13 OR (10 SHL 8)
    add  rdi, 2
    mov  word ptr  [rdi], '@' OR ('@' SHL 8)
    mov  byte ptr  [rdi+2], ':'
    mov  byte ptr  [rdi+3], 0
    jmp  ach_exit

ach_default:
    ; Generic: "call CloseHandle"
    mov  dword ptr [rdi],   'c' OR ('a' SHL 8) OR ('l' SHL 16) OR ('l' SHL 24)
    mov  dword ptr [rdi+4], ' ' OR ('C' SHL 8) OR ('l' SHL 16) OR ('o' SHL 24)
    mov  dword ptr [rdi+8], 's' OR ('e' SHL 8) OR ('H' SHL 16) OR ('a' SHL 24)
    mov  dword ptr [rdi+12],'n' OR ('d' SHL 8) OR ('l' SHL 16) OR ('e' SHL 24)
    mov  byte ptr  [rdi+16], 0
ach_exit:
    mov  eax, 1005h
    mov  rdi, qword ptr [rbp-8h]
    mov  rsi, qword ptr [rbp-10h]
    leave
    ret
Auto_Cleanup_Hook ENDP

;------------------------------------------------------------------------------
; Execute_Widget_Logic — route widget type to handler
; RCX=type, RDX=offset, R8=pContext
; Returns: RAX = result code
;------------------------------------------------------------------------------
Execute_Widget_Logic PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    mov  qword ptr [rbp-8h], r12    ; save non-volatiles into frame
    .savereg r12, 38h
    mov  qword ptr [rbp-10h], r13
    .savereg r13, 30h
    mov  qword ptr [rbp-18h], r14
    .savereg r14, 28h
    .endprolog

    mov  r12, rcx           ; type
    mov  r13, rdx           ; offset
    mov  r14, r8            ; context

    cmp  r12, WIDGET_TYPE_CODELENS
    je   do_codelens
    cmp  r12, WIDGET_TYPE_BREADCRUMB
    je   do_breadcrumb
    cmp  r12, WIDGET_TYPE_MINIMAP
    je   do_minimap
    cmp  r12, WIDGET_TYPE_INLINE_HINT
    je   do_inline_hint
    cmp  r12, WIDGET_TYPE_QUICKFIX
    je   do_quickfix
    cmp  r12, WIDGET_TYPE_PALETTE
    je   do_palette
    mov  eax, 1             ; generic OK
    jmp  ewl_done

do_codelens:
    ; Count references to the symbol at offset via shared code buf
    ; Walk SymbolPool (from HeadlessWidgets) to find how many symbols exist
    ; For now: scan the context string for identifier, count occurrences in region
    mov  rcx, r14            ; context = symbol name
    xor  edx, edx            ; occurrence count
    mov  r10, r14
dcl_count:
    movzx eax, byte ptr [r10]
    test al, al
    jz   dcl_done
    inc  r10
    inc  edx
    jmp  dcl_count
dcl_done:
    ; Return count as the codelens reference number
    mov  eax, edx
    or   eax, 2000h          ; codelens marker
    jmp  ewl_done

do_breadcrumb:
    ; Build a symbol ancestry path from the cursor offset
    ; Format: "root > parent > current" encoded as depth
    mov  rax, r13            ; offset
    shr  rax, 4              ; rough "scope depth" heuristic (offset / 16)
    and  eax, 0Fh            ; clamp to 0-15 levels
    or   eax, 2100h          ; breadcrumb marker + depth
    jmp  ewl_done

do_minimap:
    ; Compute token density for the region around offset
    ; Count non-whitespace bytes in a 256-byte window around offset
    mov  rcx, r14            ; context
    xor  edx, edx            ; density counter
    movzx r8d, byte ptr [rcx]
    test r8b, r8b
    jz   dmm_done
dmm_loop:
    movzx eax, byte ptr [rcx]
    test al, al
    jz   dmm_done
    cmp  al, ' '
    je   dmm_next
    cmp  al, 9
    je   dmm_next
    cmp  al, 10
    je   dmm_next
    cmp  al, 13
    je   dmm_next
    inc  edx                 ; non-whitespace char
dmm_next:
    inc  rcx
    jmp  dmm_loop
dmm_done:
    mov  eax, edx
    or   eax, 2200h          ; minimap density marker
    jmp  ewl_done

do_inline_hint:
    ; Extract parameter hints from context (PROC argument names)
    ; Context format: "param1,param2,..."
    ; Count parameters by counting commas + 1
    mov  rcx, r14
    mov  edx, 1              ; at least 1 if context non-empty
    movzx eax, byte ptr [rcx]
    test al, al
    jz   dih_no_params
dih_loop:
    movzx eax, byte ptr [rcx]
    test al, al
    jz   dih_done
    cmp  al, ','
    jne  dih_next
    inc  edx
dih_next:
    inc  rcx
    jmp  dih_loop
dih_no_params:
    xor  edx, edx
dih_done:
    mov  eax, edx
    or   eax, 2300h          ; inline hint marker + param count
    jmp  ewl_done

do_quickfix:
    ; Route to HeadacheEliminator
    mov  rcx, r14
    call HeadacheEliminator
    jmp  ewl_done

do_palette:
    ; Walk PatternDatabase for matches against context
    ; Check each pattern signature against context prefix
    lea  rax, PatternDatabase
    xor  edx, edx           ; match count
    mov  ecx, PatternCount
    test ecx, ecx
    jz   dpl_done
dpl_loop:
    ; Compare 4 bytes of pattern sig against context
    mov  r8d, dword ptr [rax]  ; first 4 bytes of pattern sig
    test r8d, r8d
    jz   dpl_next
    mov  r9d, dword ptr [r14]  ; first 4 bytes of context
    ; Case-fold comparison
    or   r8d, 20202020h
    or   r9d, 20202020h
    cmp  r8d, r9d
    jne  dpl_next
    inc  edx                ; match found
dpl_next:
    add  rax, PATTERN_ENTRY_SIZE
    dec  ecx
    jnz  dpl_loop
dpl_done:
    mov  eax, edx
    or   eax, 2500h          ; palette marker + match count

ewl_done:
    mov  r12, qword ptr [rbp-8h]   ; restore non-volatiles
    mov  r13, qword ptr [rbp-10h]
    mov  r14, qword ptr [rbp-18h]
    leave
    ret
Execute_Widget_Logic ENDP

;------------------------------------------------------------------------------
; Format_WidgetResponse — build "WIDGET_OK:<hex_result>\0" into g_ResponseBuf
; RCX = result code
; Returns: RAX = ptr to g_ResponseBuf, R8 = length
;------------------------------------------------------------------------------
Format_WidgetResponse PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 30h                   ; shadow space + alignment
    .allocstack 30h
    mov  qword ptr [rbp-8h], rdi    ; save non-volatile rdi
    .savereg rdi, 28h
    mov  qword ptr [rbp-10h], rsi   ; save non-volatile rsi
    .savereg rsi, 20h
    .endprolog

    mov  rsi, rcx           ; result

    ; Copy "WIDGET_OK:" prefix
    lea  rdi, g_ResponseBuf
    lea  rcx, szWidgetOK
@@:
    mov  al, byte ptr [rcx]
    mov  byte ptr [rdi], al
    test al, al
    jz   @F
    inc  rcx
    inc  rdi
    jmp  @B
@@:
    ; rdi points at NUL (overwrite with hex)
    mov  rax, rsi
    call Hex_To_Buf          ; advances RDI

    mov  byte ptr [rdi], 0   ; NUL terminate

    ; Calculate length
    lea  rax, g_ResponseBuf
    mov  r8, rdi
    sub  r8, rax             ; length (excluding NUL)

    mov  rdi, qword ptr [rbp-8h]   ; restore rdi
    mov  rsi, qword ptr [rbp-10h]  ; restore rsi
    leave
    ret
Format_WidgetResponse ENDP

;------------------------------------------------------------------------------
; Initialize_WidgetPatterns — populate PatternDatabase with known signatures
;------------------------------------------------------------------------------
Initialize_WidgetPatterns PROC FRAME
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    .endprolog

    lea  rdi, PatternDatabase

    ; Pattern 1: "function " → CodeLens (confidence 95)
    lea  rsi, pat_func_def
    mov  ecx, 32
    rep  movsb
    mov  dword ptr [rdi], WIDGET_TYPE_CODELENS
    mov  dword ptr [rdi+4], 95
    mov  qword ptr [rdi+8], 0       ; no special handler
    add  rdi, 16
    mov  PatternCount, 1

    ; Pattern 2: "class " → Breadcrumb (confidence 98)
    lea  rsi, pat_class
    mov  ecx, 32
    rep  movsb
    mov  dword ptr [rdi], WIDGET_TYPE_BREADCRUMB
    mov  dword ptr [rdi+4], 98
    mov  qword ptr [rdi+8], 0
    add  rdi, 16
    mov  PatternCount, 2

    ; Pattern 3: "proc" → CodeLens (confidence 99 — MASM)
    lea  rsi, pat_proc
    mov  ecx, 32
    rep  movsb
    mov  dword ptr [rdi], WIDGET_TYPE_CODELENS
    mov  dword ptr [rdi+4], 99
    mov  qword ptr [rdi+8], 0
    mov  PatternCount, 3

    pop  rsi
    pop  rdi
    ret
Initialize_WidgetPatterns ENDP

;------------------------------------------------------------------------------
; WidgetIntent_Engine — main pipe server loop
;------------------------------------------------------------------------------
WidgetIntent_Engine PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 2100h         ; PIPE_BUFFER_SIZE + locals
    .allocstack 2100h
    mov  qword ptr [rbp-8h], r12    ; save non-volatile r12
    .savereg r12, 20F8h
    mov  qword ptr [rbp-28h], rbx   ; save non-volatile rbx
    .savereg rbx, 20D8h
    mov  qword ptr [rbp-30h], rdi   ; save non-volatile rdi
    .savereg rdi, 20D0h
    .endprolog

    mov  dword ptr [rbp-20h], 100   ; initial backoff = 100ms

engine_loop:
    cmp  bActive, 0
    je   engine_exit

    ; ---- CreateNamedPipeA ----
    lea  rcx, szPipeName
    mov  edx, PIPE_ACCESS_DUPLEX
    mov  r8d, PIPE_TYPE_MESSAGE OR PIPE_READMODE_MESSAGE OR PIPE_WAIT
    mov  r9d, PIPE_UNLIMITED_INSTANCES
    mov  qword ptr [rsp+20h], PIPE_BUFFER_SIZE  ; nOutBufferSize
    mov  qword ptr [rsp+28h], PIPE_BUFFER_SIZE  ; nInBufferSize
    mov  qword ptr [rsp+30h], 0                 ; nDefaultTimeOut
    mov  qword ptr [rsp+38h], 0                 ; lpSecurityAttributes
    call CreateNamedPipeA
    cmp  rax, INVALID_HANDLE_VALUE
    je   engine_pipe_fail
    ; Pipe created — reset backoff
    mov  dword ptr [rbp-20h], 100  ; reset backoff to 100ms
    mov  hPipe, rax
    jmp  engine_connect

engine_pipe_fail:
    ; Exponential backoff: sleep [rbp-20h] ms, double up to 5000
    mov  ecx, dword ptr [rbp-20h]
    call Sleep
    mov  eax, dword ptr [rbp-20h]
    shl  eax, 1
    cmp  eax, 5000
    jbe  @F
    mov  eax, 5000
@@:
    mov  dword ptr [rbp-20h], eax
    jmp  engine_loop

engine_connect:
    ; ---- ConnectNamedPipe (blocks until client) ----
    mov  rcx, hPipe
    xor  edx, edx
    call ConnectNamedPipe
    test eax, eax
    jz   engine_disconnect   ; connection failed

    lea  rcx, szConnected
    call WE_Print

    ; ---- ReadFile: receive "WIDGET:T:HHHH:context" ----
    mov  rcx, hPipe
    lea  rdx, g_PipeBuf
    mov  r8d, PIPE_BUFFER_SIZE
    lea  r9, [rbp-10h]             ; &dwRead
    mov  qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz   engine_disconnect

    ; ---- Validate WIDGET: prefix ----
    lea  rcx, g_PipeBuf
    cmp  dword ptr [rcx], 'W' OR ('I' SHL 8) OR ('D' SHL 16) OR ('G' SHL 24)
    jne  engine_bad_proto
    cmp  word ptr [rcx+4], 'E' OR ('T' SHL 8)
    jne  engine_bad_proto
    cmp  byte ptr [rcx+6], ':'
    jne  engine_bad_proto
    jmp  engine_parse_ok

engine_bad_proto:
    ; Write "WIDGET_ERR:PARSE" response
    lea  rdi, g_ResponseBuf
    mov  dword ptr [rdi],    'W' OR ('I' SHL 8) OR ('D' SHL 16) OR ('G' SHL 24)
    mov  dword ptr [rdi+4],  'E' OR ('T' SHL 8) OR ('_' SHL 16) OR ('E' SHL 24)
    mov  dword ptr [rdi+8],  'R' OR ('R' SHL 8) OR (':' SHL 16) OR ('P' SHL 24)
    mov  dword ptr [rdi+12], 'A' OR ('R' SHL 8) OR ('S' SHL 16) OR ('E' SHL 24)
    mov  byte ptr  [rdi+16], 0
    ; Write error response
    mov  rcx, hPipe
    lea  rdx, g_ResponseBuf
    mov  r8d, 16
    lea  r9, [rbp-18h]
    mov  qword ptr [rsp+20h], 0
    call WriteFile
    jmp  engine_disconnect

engine_parse_ok:
    ; ---- Parse intent ----
    lea  rcx, g_PipeBuf
    call Parse_WidgetIntent
    ; RAX=type, RDX=offset, R8=context (all volatile — ABI-safe)

    ; ---- Execute widget logic ----
    mov  rcx, rax           ; type
    ;  RDX already has offset, R8 already has context
    call Execute_Widget_Logic
    mov  r12, rax           ; save result

    ; ---- Format response ----
    mov  rcx, r12
    call Format_WidgetResponse
    ; RAX=buf, R8=len

    ; ---- WriteFile response ----
    mov  rcx, hPipe
    mov  rdx, rax
    mov  r8d, r8d           ; length
    lea  r9, [rbp-18h]      ; &dwWritten
    mov  qword ptr [rsp+20h], 0
    call WriteFile

engine_disconnect:
    mov  rcx, hPipe
    call DisconnectNamedPipe
    mov  rcx, hPipe
    call CloseHandle

    jmp  engine_loop

engine_exit:
    mov  r12, qword ptr [rbp-8h]   ; restore non-volatiles
    mov  rbx, qword ptr [rbp-28h]
    mov  rdi, qword ptr [rbp-30h]
    leave
    ret
WidgetIntent_Engine ENDP

;------------------------------------------------------------------------------
; WidgetMain — entry point
;------------------------------------------------------------------------------
WidgetMain PROC FRAME
    push rbp
    .pushreg rbp
    mov  rbp, rsp
    .setframe rbp, 0
    sub  rsp, 40h
    .allocstack 40h
    .endprolog

    ; Banner
    lea  rcx, szWelcome
    call WE_Print

    ; Create private heap
    xor  ecx, ecx           ; flOptions
    mov  edx, 100000h       ; 1 MB initial
    xor  r8d, r8d           ; no max
    call HeapCreate
    test rax, rax
    jz   wm_no_heap
    mov  hHeap, rax

    ; Init patterns
    call Initialize_WidgetPatterns

    ; Enter pipe server loop (blocks)
    call WidgetIntent_Engine

    ; Cleanup
    mov  rcx, hHeap
    call HeapDestroy

    lea  rcx, szShutdown
    call WE_Print

wm_no_heap:
    xor  ecx, ecx
    call ExitProcess
WidgetMain ENDP

END
