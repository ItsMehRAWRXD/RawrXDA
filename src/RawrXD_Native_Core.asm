; RawrXD Native Core - v15.0.0 (x64 Optimized)
; Replaces PowerShell logic (Log, Error Handling, File IO)
OPTION CASEMAP:NONE

PUBLIC RawrXD_Native_Log

includelib kernel32.lib
includelib user32.lib

.data
    g_hLogFile      QWORD 0
    szLogPath       DB "D:\rawrxd\logs\sovereign_trace.log", 0
    szCRLF          DB 13, 10, 0

.code

; =============================================================================
; Internal: OpenLog (Ensures g_hLogFile is valid)
; =============================================================================
OpenLog PROC
    push rbx
    cmp g_hLogFile, 0
    jne @@done
    
    sub rsp, 40
    lea rcx, szLogPath
    mov rdx, 40000000h      ; GENERIC_WRITE
    mov r8, 1               ; FILE_SHARE_READ
    xor r9, r9              ; NULL Security
    mov QWORD PTR [rsp+32], 4 ; OPEN_ALWAYS
    mov QWORD PTR [rsp+40], 80h ; FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp+48], 0 ; NULL Template
    
    EXTERN CreateFileA : PROC
    call CreateFileA
    add rsp, 40
    
    mov g_hLogFile, rax
    
@@done:
    pop rbx
    ret
OpenLog ENDP

; =============================================================================
; RawrXD_Native_Log
; Replaces PS: Write-StartupLog
; =============================================================================
RawrXD_Native_Log PROC
    ; RCX = LPCSTR message
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    
    mov rsi, rcx            ; Save message pointer
    
    call OpenLog
    cmp g_hLogFile, -1
    je @@exit
    
    ; 1. Get length of message
    xor rbx, rbx
@@len_loop:
    cmp BYTE PTR [rsi + rbx], 0
    je @@len_done
    inc rbx
    jmp @@len_loop
@@len_done:

    ; 2. Write message to file
    sub rsp, 48
    mov rcx, g_hLogFile
    mov rdx, rsi            ; Message buffer
    mov r8, rbx             ; Length
    lea r9, [rbp-8]         ; bytesWritten (dummy)
    mov QWORD PTR [rsp+32], 0 ; Overlapped
    EXTERN WriteFile : PROC
    call WriteFile
    
    ; 3. Write CRLF
    mov rcx, g_hLogFile
    lea rdx, szCRLF
    mov r8, 2
    lea r9, [rbp-8]
    mov QWORD PTR [rsp+32], 0
    call WriteFile
    add rsp, 48

@@exit:
    pop rsi
    pop rbx
    pop rbp
    ret
RawrXD_Native_Log ENDP

Internal_StrStrCase proc
    push rbx
    push rsi
    push rdi
    mov rsi, rcx
@outer:
    mov rdi, rdx
    mov rax, rsi
@inner:
    movzx ebx, byte ptr [rdi]
    test bl, bl
    jz @match_found
    movzx ecx, byte ptr [rsi]
    test cl, cl
    jz @no_match
    cmp cl, 'A'
    jl @c1
    cmp cl, 'Z'
    jg @c1
    add cl, 32
@c1:
    cmp bl, 'A'
    jl @c2
    cmp bl, 'Z'
    jg @c2
    add bl, 32
@c2:
    cmp cl, bl
    jne @next
    inc rsi
    inc rdi
    jmp @inner
@next:
    inc rax
    mov rsi, rax
    jmp @outer
@match_found:
    jmp @exit
@no_match:
    xor rax, rax
@exit:
    pop rdi
    pop rsi
    pop rbx
    ret
Internal_StrStrCase endp

;Core_TestInputSafety proc
;    push rbp
;    mov rbp, rsp
;    sub rsp, 32
;    mov rsi, rcx
;    test rsi, rsi
;    jz @ok
;    lea rdx, patExec
;    call Internal_StrStrCase
;    test rax, rax
;    jnz @fail
;    lea rdx, patEval
;    call Internal_StrStrCase
;    test rax, rax
;    jnz @fail
;    lea rdx, patPS
;    call Internal_StrStrCase
;    test rax, rax
;    jnz @fail
;@ok:
;    mov eax, 1
;    jmp @done
;@fail:
;    xor eax, eax
;@done:
;    add rsp, 32
;    pop rbp
;    ret
;Core_TestInputSafety endp

;Core_FastFileRead proc
;    push rbp
;    mov rbp, rsp
;    sub rsp, 64
;    mov rdx, 80000000h
;    mov r8, 1
;    xor r9, r9
;    mov qword ptr [rsp+32], 3
;    mov qword ptr [rsp+40], 80h
;    call CreateFileA
;    cmp rax, -1
;    je @err
;    mov rbx, rax
;    mov rcx, rbx
;    xor rdx, rdx
;    call GetFileSize
;    mov r12, rax
;    call GetProcessHeap
;    mov rcx, rax
;    mov rdx, 8
;    mov r8, r12
;    inc r8
;    call HeapAlloc
;    mov r13, rax
;    mov rcx, rbx
;    mov rdx, r13
;    mov r8, r12
;    lea r9, [rsp+48]
;    mov qword ptr [rsp+32], 0
;    call ReadFile
;    mov rcx, rbx
;    call CloseHandle
;    mov rax, r13
;    jmp @done
;@err:
;    xor rax, rax
;@done:
;    add rsp, 64
;    pop rbp
;    ret
;Core_FastFileRead endp

End
