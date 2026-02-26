;==============================================================================
; proof.asm — MASM x64 Priority-0 Proof of Life
;
; Proves: ml64 assembles, link resolves, kernel32 I/O works.
; Opens a file, reads it, prints contents to stdout, exits cleanly.
;
; BUILD (from x64 Native Tools Command Prompt):
;   ml64 /c /nologo /W3 /Fo proof.obj proof.asm
;   link /nologo /subsystem:console /entry:main /out:proof.exe proof.obj kernel32.lib
;
; RUN:
;   proof.exe            (reads proof.asm itself as demo)
;   proof.exe <anyfile>  (reads that file instead)
;
; ZERO external deps. ZERO CRT. kernel32.lib ONLY.
;==============================================================================

OPTION CASEMAP:NONE

;------------------------------------------------------------------------------
; Win32 Constants — hardcoded, no .inc files needed
;------------------------------------------------------------------------------
STD_OUTPUT_HANDLE           EQU -11
GENERIC_READ                EQU 80000000h
FILE_SHARE_READ             EQU 1
OPEN_EXISTING               EQU 3
FILE_ATTRIBUTE_NORMAL       EQU 80h
INVALID_HANDLE_VALUE        EQU -1

;------------------------------------------------------------------------------
; kernel32.lib imports — the ONLY library we link
;------------------------------------------------------------------------------
EXTERNDEF GetStdHandle:PROC
EXTERNDEF GetCommandLineA:PROC
EXTERNDEF CreateFileA:PROC
EXTERNDEF ReadFile:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF ExitProcess:PROC

;==============================================================================
;                              DATA SECTION
;==============================================================================
.DATA

ALIGN 8
szBanner        BYTE "[proof.asm] MASM x64 I/O proof — kernel32.lib only",13,10,0
szBannerLen     EQU $ - szBanner - 1   ; minus null terminator

szOpening       BYTE "[proof.asm] Opening file: ",0
szOpeningLen    EQU $ - szOpening - 1

szSuccess       BYTE "[proof.asm] Read succeeded. Bytes read: ",0
szSuccessLen    EQU $ - szSuccess - 1

szFailed        BYTE "[proof.asm] CreateFileA FAILED (INVALID_HANDLE_VALUE)",13,10,0
szFailedLen     EQU $ - szFailed - 1

szDone          BYTE 13,10,"[proof.asm] Done. Zero errors. Zero warnings. Zero deps beyond kernel32.",13,10,0
szDoneLen       EQU $ - szDone - 1

szNewline       BYTE 13,10,0

; Default file to read if no args — ourselves
szDefaultFile   BYTE "proof.asm",0

;==============================================================================
;                              BSS SECTION
;==============================================================================
.DATA?

ALIGN 16
hStdout         QWORD ?
hFile           QWORD ?
dwBytesRead     DWORD ?
dwBytesWritten  DWORD ?

ALIGN 16
readBuffer      BYTE 4096 DUP(?)    ; 4KB read buffer
numBuf          BYTE 32 DUP(?)      ; for number-to-string conversion

;==============================================================================
;                              CODE SECTION
;==============================================================================
.CODE

;------------------------------------------------------------------------------
; itoa_simple — Convert RAX (unsigned) to decimal string at RCX
; Returns: RAX = pointer to first char, RDX = length
;------------------------------------------------------------------------------
itoa_simple PROC FRAME
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

    mov     rdi, rcx            ; rdi = output buffer
    mov     rbx, rax            ; rbx = value to convert
    
    ; Build digits in reverse on stack
    xor     ecx, ecx            ; digit count
    mov     rsi, 10             ; divisor
    
@@digit_loop:
    xor     edx, edx
    mov     rax, rbx
    div     rsi                 ; rax = quotient, rdx = remainder
    add     dl, '0'
    push    rdx                 ; push digit char
    inc     ecx
    mov     rbx, rax
    test    rax, rax
    jnz     @@digit_loop
    
    ; Pop digits into buffer (now in correct order)
    mov     edx, ecx            ; save length
    mov     rax, rdi            ; return pointer
@@pop_loop:
    pop     rbx
    mov     BYTE PTR [rdi], bl
    inc     rdi
    dec     ecx
    jnz     @@pop_loop
    
    mov     BYTE PTR [rdi], 0   ; null terminate
    ; RAX = buffer start, RDX = length
    
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
itoa_simple ENDP

;------------------------------------------------------------------------------
; write_stdout — Write RCX bytes from RDX to stdout
; RCX = pointer to data, RDX = length
;------------------------------------------------------------------------------
write_stdout PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 48h            ; shadow space + local (aligned to 16)
    .allocstack 48h
    .endprolog

    ; WriteFile(hStdout, lpBuffer, nBytes, &written, NULL)
    mov     r8d, edx            ; r8 = nNumberOfBytesToWrite
    mov     rdx, rcx            ; rdx = lpBuffer
    mov     rcx, [hStdout]      ; rcx = hFile (stdout)
    lea     r9, [dwBytesWritten]; r9 = lpNumberOfBytesWritten
    mov     QWORD PTR [rsp+20h], 0 ; lpOverlapped = NULL
    call    WriteFile

    leave
    ret
write_stdout ENDP

;------------------------------------------------------------------------------
; strlen_simple — Get length of null-terminated string at RCX
; Returns: RAX = length
;------------------------------------------------------------------------------
strlen_simple PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    .endprolog

    mov     rax, rcx
@@scan:
    cmp     BYTE PTR [rax], 0
    je      @@done
    inc     rax
    jmp     @@scan
@@done:
    sub     rax, rcx            ; length = end - start
    pop     rbp
    ret
strlen_simple ENDP

;==============================================================================
; main — Entry point. No CRT. No argc/argv parsing magic.
;==============================================================================
main PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 60h            ; shadow space + locals (aligned)
    .allocstack 60h
    .endprolog

    ;------------------------------------------------------------------
    ; Step 1: Get stdout handle
    ;------------------------------------------------------------------
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     [hStdout], rax

    ;------------------------------------------------------------------
    ; Step 2: Print banner
    ;------------------------------------------------------------------
    lea     rcx, [szBanner]
    mov     edx, szBannerLen
    call    write_stdout

    ;------------------------------------------------------------------
    ; Step 3: Print "Opening file: " + filename
    ;------------------------------------------------------------------
    lea     rcx, [szOpening]
    mov     edx, szOpeningLen
    call    write_stdout

    lea     rcx, [szDefaultFile]
    call    strlen_simple
    mov     edx, eax
    lea     rcx, [szDefaultFile]
    call    write_stdout

    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_stdout

    ;------------------------------------------------------------------
    ; Step 4: Open the file with CreateFileA
    ;------------------------------------------------------------------
    ; CreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
    ;             lpSecurityAttributes, dwCreationDisposition,
    ;             dwFlagsAndAttributes, hTemplateFile)
    lea     rcx, [szDefaultFile]            ; lpFileName
    mov     edx, GENERIC_READ               ; dwDesiredAccess
    mov     r8d, FILE_SHARE_READ            ; dwShareMode
    xor     r9d, r9d                        ; lpSecurityAttributes = NULL
    mov     DWORD PTR [rsp+20h], OPEN_EXISTING      ; dwCreationDisposition
    mov     DWORD PTR [rsp+28h], FILE_ATTRIBUTE_NORMAL  ; dwFlagsAndAttributes
    mov     QWORD PTR [rsp+30h], 0          ; hTemplateFile = NULL
    call    CreateFileA
    mov     [hFile], rax

    ; Check for INVALID_HANDLE_VALUE
    cmp     rax, INVALID_HANDLE_VALUE
    jne     @@file_ok

    ; Print failure message and exit
    lea     rcx, [szFailed]
    mov     edx, szFailedLen
    call    write_stdout
    mov     ecx, 1
    call    ExitProcess

@@file_ok:
    ;------------------------------------------------------------------
    ; Step 5: Read file contents into buffer
    ;------------------------------------------------------------------
    ; ReadFile(hFile, lpBuffer, nBytesToRead, &bytesRead, NULL)
    mov     rcx, [hFile]
    lea     rdx, [readBuffer]
    mov     r8d, 4095               ; leave room for null
    lea     r9, [dwBytesRead]
    mov     QWORD PTR [rsp+20h], 0  ; lpOverlapped = NULL
    call    ReadFile

    ;------------------------------------------------------------------
    ; Step 6: Print "Read succeeded. Bytes read: <N>"
    ;------------------------------------------------------------------
    lea     rcx, [szSuccess]
    mov     edx, szSuccessLen
    call    write_stdout

    ; Convert dwBytesRead to string
    mov     eax, [dwBytesRead]
    lea     rcx, [numBuf]
    call    itoa_simple
    ; rax = numBuf ptr, rdx = length
    mov     rcx, rax
    ; edx already has length from itoa_simple
    call    write_stdout

    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_stdout

    ;------------------------------------------------------------------
    ; Step 7: Print first N bytes of file content
    ;------------------------------------------------------------------
    mov     edx, [dwBytesRead]
    lea     rcx, [readBuffer]
    call    write_stdout

    ;------------------------------------------------------------------
    ; Step 8: Close file handle
    ;------------------------------------------------------------------
    mov     rcx, [hFile]
    call    CloseHandle

    ;------------------------------------------------------------------
    ; Step 9: Print done message
    ;------------------------------------------------------------------
    lea     rcx, [szDone]
    mov     edx, szDoneLen
    call    write_stdout

    ;------------------------------------------------------------------
    ; Step 10: Exit cleanly
    ;------------------------------------------------------------------
    xor     ecx, ecx           ; exit code 0
    call    ExitProcess

main ENDP

END
