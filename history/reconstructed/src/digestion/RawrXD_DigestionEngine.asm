; RawrXD_DigestionEngine.asm
; Minimal MASM64 stub for RunDigestionEngine used by RawrXD IDE
; Assemble with: ml64 /c /FoRawrXD_DigestionEngine.obj RawrXD_DigestionEngine.asm

.code

; RawrXD_DigestionEngine.asm
; Minimal MASM64 stub for RunDigestionEngine used by RawrXD IDE
; Assemble with: ml64 /c /FoRawrXD_DigestionEngine.obj RawrXD_DigestionEngine.asm

.code

; DWORD RunDigestionEngine(
;   LPCWSTR szSource,   ; RCX
;   LPCWSTR szOutput,   ; RDX
;   DWORD   dwChunk,    ; R8D
;   DWORD   dwThreads,  ; R9D
;   DWORD   dwFlags,    ; [RSP+40]
;   LPVOID  pCtx        ; [RSP+48]
; );
RunDigestionEngine PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; Validate arguments
    mov     r12, rcx        ; szSource
    mov     r13, rdx        ; szOutput
    test    r12, r12
    jz      invalid_arg
    test    r13, r13
    jz      invalid_arg

    ; Open source file
    mov     rcx, r12
    mov     rdx, GENERIC_READ
    xor     r8, r8
    xor     r9, r9
    mov     qword ptr [rsp+32], OPEN_EXISTING
    mov     qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov     qword ptr [rsp+48], 0
    call    CreateFileW
    cmp     rax, INVALID_HANDLE_VALUE
    je      file_error
    mov     rbx, rax        ; hSource

    ; Open output file
    mov     rcx, r13
    mov     rdx, GENERIC_WRITE
    xor     r8, r8
    xor     r9, r9
    mov     qword ptr [rsp+32], CREATE_ALWAYS
    mov     qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov     qword ptr [rsp+48], 0
    call    CreateFileW
    cmp     rax, INVALID_HANDLE_VALUE
    je      close_source
    mov     rsi, rax        ; hOutput

    ; Allocate buffer
    mov     rcx, 65536      ; 64KB buffer
    call    GetProcessHeap
    mov     rcx, rax
    mov     rdx, HEAP_ZERO_MEMORY
    mov     r8, 65536
    call    HeapAlloc
    test    rax, rax
    jz      close_files
    mov     rdi, rax        ; buffer

copy_loop:
    ; Read from source
    mov     rcx, rbx
    mov     rdx, rdi
    mov     r8, 65536
    lea     r9, [rsp+24]
    call    ReadFile
    test    eax, eax
    jz      free_buffer
    mov     r14, [rsp+24]   ; bytes read
    test    r14, r14
    jz      free_buffer

    ; Write to output
    mov     rcx, rsi
    mov     rdx, rdi
    mov     r8, r14
    lea     r9, [rsp+24]
    call    WriteFile
    test    eax, eax
    jz      free_buffer

    jmp     copy_loop

free_buffer:
    mov     rcx, GetProcessHeap
    call    rcx
    mov     rcx, rax
    mov     rdx, 0
    mov     r8, rdi
    call    HeapFree

close_files:
    mov     rcx, rsi
    call    CloseHandle

close_source:
    mov     rcx, rbx
    call    CloseHandle

    xor     eax, eax        ; S_DIGEST_OK = 0
    jmp     done

file_error:
    mov     eax, 2          ; E_DIGEST_FILE_ERROR

invalid_arg:
    mov     eax, 87         ; E_DIGEST_INVALIDARG / ERROR_INVALID_PARAMETER

done:
    add     rsp, 32
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

RunDigestionEngine ENDP

END
