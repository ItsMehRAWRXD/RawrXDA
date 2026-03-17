;=============================================================================
; rawrxd_slice.asm  
; Binary-safe line range extractor (for analyzing specific source sections)
; Usage: rawrxd_slice.exe <file> <start_line> <end_line>
;=============================================================================
INCLUDE \masm64\include64\masm64rt.inc

.const
CHUNK_SIZE      EQU 1048576    ; 1MB read buffer
MAX_LINE_LEN    EQU 8192       ; Safety cap

.data
hInput          QWORD ?
hOutput         QWORD ?        ; stdout handle
pBuffer         QWORD ?
pLineBuf        QWORD ?
lineNum         QWORD 1
targetStart     QWORD ?
targetEnd       QWORD ?
inRange         BYTE ?
bytesRead       QWORD ?
fmt_range       BYTE "[Lines %llu-%llu]",13,10,0
fmt_line        BYTE "%4llu | %s",13,10,0
err_args        BYTE "Usage: rawrxd_slice <file> <start> <end>",13,10,0
err_open        BYTE "ERROR: Cannot open input",13,10,0
err_mem         BYTE "ERROR: Allocation failed",13,10,0

.code
;----------------------------------------------------------------------
; ExtractRange - Main worker
;----------------------------------------------------------------------
ExtractRange PROC
    LOCAL   hFile:QWORD
    LOCAL   bufPos:QWORD
    LOCAL   linePos:QWORD
    
    invoke  CreateFileA, pBuffer, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0
    mov     hFile, rax
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@error_open
    
    ; Allocate buffers
    invoke  VirtualAlloc, 0, CHUNK_SIZE, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    mov     pBuffer, rax
    test    rax, rax
    jz      @@error_mem
    
    invoke  VirtualAlloc, 0, MAX_LINE_LEN, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    mov     pLineBuf, rax
    test    rax, rax
    jz      @@error_mem
    
    mov     rsi, rax        ; pLineBuf cursor
    xor     rbx, rbx        ; line buffer position
    mov     lineNum, 1
    
    ; Print range header
    invoke  printf, ADDR fmt_range, targetStart, targetEnd
    
@@read_loop:
    invoke  ReadFile, hFile, pBuffer, CHUNK_SIZE, ADDR bytesRead, 0
    test    eax, eax
    jz      @@done
    
    mov     rcx, bytesRead
    test    rcx, rcx
    jz      @@done
    
    mov     rdi, pBuffer
    mov     rdx, rcx        ; remaining bytes in chunk
    
@@byte_loop:
    mov     al, [rdi]
    inc     rdi
    dec     rdx
    
    ; Check range
    mov     r8, lineNum
    cmp     r8, targetStart
    setae   al
    cmp     r8, targetEnd
    setbe   bl
    and     al, bl
    mov     inRange, al
    
    .IF al == 1
        ; Store in line buffer
        mov     [rsi+rbx], byte ptr [rdi-1]
        inc     rbx
        cmp     rbx, MAX_LINE_LEN-2
        jb      @@check_newline
        
        ; Line too long, force print
        mov     byte ptr [rsi+rbx], 0
        invoke  printf, ADDR fmt_line, lineNum, pLineBuf
        xor     ebx, ebx
        jmp     @@check_newline
    .ENDIF
    
@@check_newline:
    cmp     byte ptr [rdi-1], 10    ; LF
    jne     @@next_byte
    
    ; End of line
    .IF inRange == 1
        ; Null terminate (strip CRLF)
        cmp     rbx, 0
        je      @@print_empty
        
        .IF byte ptr [rsi+rbx-1] == 13  ; CR
            dec     rbx
        .ENDIF
        
        mov     byte ptr [rsi+rbx], 0
        
        ; Print line
        invoke  printf, ADDR fmt_line, lineNum, pLineBuf
        
    .ELSEIF lineNum > targetEnd
        jmp     @@done
    .ENDIF
    
    xor     ebx, ebx
    inc     lineNum
    
@@next_byte:
    test    rdx, rdx
    jnz     @@byte_loop
    jmp     @@read_loop
    
@@print_empty:
    mov     byte ptr [rsi], 0
    invoke  printf, ADDR fmt_line, lineNum, pLineBuf
    inc     lineNum
    jmp     @@read_loop
    
@@done:
    invoke  CloseHandle, hFile
    invoke  VirtualFree, pBuffer, 0, MEM_RELEASE
    invoke  VirtualFree, pLineBuf, 0, MEM_RELEASE
    xor     eax, eax
    ret
    
@@error_open:
    invoke  printf, ADDR err_open
    mov     eax, 1
    ret
    
@@error_mem:
    invoke  printf, ADDR err_mem
    mov     eax, 1
    ret
    
ExtractRange ENDP

;----------------------------------------------------------------------
; Main - Parse args and dispatch
;----------------------------------------------------------------------
Main PROC
    LOCAL   argc:QWORD, argv:QWORD
    LOCAL   filename[260]:BYTE
    
    invoke  GetCommandLineW
    lea     rcx, argc
    invoke  CommandLineToArgvW, rax, rcx
    mov     argv, rax
    
    cmp     argc, 4
    jne     @@usage
    
    ; Get filename (convert to ANSI)
    mov     rsi, argv
    mov     rsi, [rsi+16]   ; argv[1]
    invoke  WideCharToMultiByte, CP_ACP, 0, rsi, -1, ADDR filename, 260, 0, 0
    lea     rax, filename
    mov     pBuffer, rax
    
    ; Parse start line
    mov     rsi, argv
    mov     rsi, [rsi+24]   ; argv[2]
    invoke  wcstoull, rsi, 0, 10
    mov     targetStart, rax
    
    ; Parse end line
    mov     rsi, argv
    mov     rsi, [rsi+32]   ; argv[3]
    invoke  wcstoull, rsi, 0, 10
    mov     targetEnd, rax
    
    ; Validate
    cmp     targetStart, 0
    je      @@usage
    cmp     targetEnd, 0
    je      @@usage
    mov     rax, targetStart
    cmp     targetEnd, rax
    jb      @@usage
    
    call    ExtractRange
    invoke  ExitProcess, eax
    
@@usage:
    invoke  printf, ADDR err_args
    invoke  ExitProcess, 1

Main ENDP
END
