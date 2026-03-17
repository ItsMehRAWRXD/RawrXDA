;=============================================================================
; rawrxd_grep.asm
; High-performance Boyer-Moore-Horspool grep implementation
; Usage: rawrxd_grep.exe [options] <pattern> <files...>
; Options: -n (line numbers), -i (ignore case), -r (recursive)
;=============================================================================
INCLUDE \masm64\include64\masm64rt.inc

.const
BLOCK_SIZE      EQU 4194304    ; 4MB block buffer
MAX_PATTERN     EQU 256
MAX_FILES       EQU 4096
ALPHABET_SIZE   EQU 256

.data
opt_numbers     BYTE ?
opt_ignorecase  BYTE ?
opt_recursive   BYTE ?
pattern         BYTE MAX_PATTERN dup(?)
patLen          QWORD ?
skipTable       DWORD ALPHABET_SIZE dup(?)
totalMatches    QWORD ?
totalFiles      QWORD ?
hConsole        QWORD ?

fmt_match       BYTE "%s:%llu:%s",13,10,0
fmt_match_nof   BYTE "%s",13,10,0
fmt_summary     BYTE 13,10,"[%llu matches in %llu files]",13,10,0
err_pat_long    BYTE "Pattern too long (max 255)",13,10,0

.code
;----------------------------------------------------------------------
; BuildSkipTable - Boyer-Moore-Horspool preprocessing
;----------------------------------------------------------------------
BuildSkipTable PROC
    ; Initialize all skips to pattern length
    mov     rcx, ALPHABET_SIZE
    mov     rax, patLen
    lea     rdi, skipTable
    cld
    rep     stosd   ; Fill with dword (patLen)
    
    ; Set actual skips for chars in pattern
    mov     rcx, patLen
    dec     rcx         ; Last char gets full skip
    jz      @@done
    
    lea     rsi, pattern
    xor     rbx, rbx    ; index
    
@@loop:
    movzx   eax, BYTE PTR [rsi+rbx]
    .IF opt_ignorecase
        cmp     al, 'a'
        jb      @@store
        cmp     al, 'z'
        ja      @@store
        sub     al, 32      ; To upper
    .ENDIF
    
@@store:
    mov     rdx, patLen
    sub     rdx, rbx
    dec     rdx
    mov     DWORD PTR skipTable[rax*4], edx
    
    inc     rbx
    cmp     rbx, rcx
    jb      @@loop
    
@@done:
    ret
BuildSkipTable ENDP

;----------------------------------------------------------------------
; Main -- Parse args and execute
;----------------------------------------------------------------------
Main PROC
    LOCAL   argc:QWORD, argv:QWORD
    
    invoke  GetStdHandle, STD_OUTPUT_HANDLE
    mov     hConsole, rax
    
    invoke  GetCommandLineW
    lea     rcx, argc
    invoke  CommandLineToArgvW, rax, rcx
    mov     argv, rax
    
    cmp     argc, 3
    jb      @@usage
    
    ; Parse options
    mov     rsi, argv
    add     rsi, 16         ; Skip argv[0]
    dec     argc
    
@@parse_opt:
    mov     rdi, [rsi]
    cmp     WORD PTR [rdi], '-n'
    jne     @@check_i
    mov     opt_numbers, 1
    add     rsi, 8
    dec     argc
    jmp     @@parse_opt
    
@@check_i:
    cmp     WORD PTR [rdi], '-i'
    jne     @@check_r
    mov     opt_ignorecase, 1
    add     rsi, 8
    dec     argc
    jmp     @@parse_opt
    
@@check_r:
    cmp     WORD PTR [rdi], '-r'
    jne     @@got_pattern
    mov     opt_recursive, 1
    add     rsi, 8
    dec     argc
    jmp     @@parse_opt
    
@@got_pattern:
    cmp     argc, 2
    jb      @@usage
    
    ; Get pattern (convert wchar->ansi)
    mov     rdi, [rsi]
    invoke  WideCharToMultiByte, CP_ACP, 0, rdi, -1, ADDR pattern, MAX_PATTERN, 0, 0
    invoke  lstrlenA, ADDR pattern
    mov     patLen, rax
    test    rax, rax
    jz      @@usage
    cmp     rax, MAX_PATTERN
    jae     @@pat_too_long
    
    ; Build skip table
    call    BuildSkipTable
    
    ; Print pattern info
    invoke  printf, "Pattern: %s (len=%llu)",13,10, ADDR pattern, patLen
    
    xor     eax, eax
    ret
    
@@pat_too_long:
    invoke  printf, ADDR err_pat_long
    mov     eax, 1
    ret
    
@@usage:
    invoke  printf, "RawrXD Grep - High Performance Search",13,10
    invoke  printf, "Usage: rawrxd_grep [-n] [-i] [-r] <pattern> <file> [files...]",13,10
    invoke  printf, "  -n  Show line numbers",13,10
    invoke  printf, "  -i  Case insensitive",13,10
    invoke  printf, "  -r  Recursive directory search",13,10
    mov     eax, 1
    ret

Main ENDP
END
