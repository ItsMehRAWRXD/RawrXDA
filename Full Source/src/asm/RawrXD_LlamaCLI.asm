;==============================================================================
; RawrXD_LlamaCLI.asm
; Minimal console REPL to verify RawrXD_LlamaBridge + llama.dll works end-to-end.
;
; Usage:
;   Place `llama.dll` (and if required by your build, `ggml.dll`) next to the EXE.
;   Put a GGUF model at `models\\model.gguf` (or edit `szModelPath` below).
;
; Build:
;   ml64.exe /nologo /c /Fo RawrXD_LlamaCLI.obj /I . RawrXD_LlamaCLI.asm
;   link.exe /nologo /SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup /OUT:RawrXD_LlamaCLI.exe ^
;       RawrXD_LlamaCLI.obj RawrXD_LlamaBridge.obj kernel32.lib
;==============================================================================

OPTION CASEMAP:NONE

include rawrxd_win64.inc

EXTERN LlamaBridge_CreateA:PROC
EXTERN LlamaBridge_Destroy:PROC
EXTERN LlamaBridge_GenerateStreamA:PROC

;------------------------------------------------------------------------------
; Data
;------------------------------------------------------------------------------
.data
align 16

szBanner        BYTE "RawrXD Llama CLI (no HTTP). Type a prompt, or 'exit'.", 13, 10, 0
szPrompt        BYTE "> ", 0
szNewline       BYTE 13, 10, 0
szExit          BYTE "exit", 0

; Default model path (ASCII)
szModelPath     BYTE "models\\model.gguf", 0

.data?
align 16
g_hStdout       QWORD ?
g_hStdin        QWORD ?
g_pBridge       QWORD ?
g_inbuf         BYTE 4096 DUP(?)
g_bytesRead     QWORD ?

;------------------------------------------------------------------------------
; Helpers
;------------------------------------------------------------------------------
.code

; Write N bytes to stdout: RCX=ptr, EDX=len
WriteStdoutN PROC FRAME
    ; Win64 ABI: reserve 32 bytes shadow + 8 bytes for 5th arg, keep 16-byte align.
    sub rsp, 30h
    .endprolog
    mov r8d, edx
    mov rdx, rcx
    mov rcx, g_hStdout
    lea r9, [rsp+28h]                ; LPDWORD lpNumberOfBytesWritten
    mov qword ptr [rsp+20h], 0        ; lpOverlapped (5th arg)
    call WriteFile
    add rsp, 30h
    ret
WriteStdoutN ENDP

; Write NUL-terminated string to stdout: RCX=ptr
WriteStdoutA PROC FRAME
    push rbx
    sub rsp, 20h
    .endprolog
    mov rbx, rcx
    invoke lstrlenA, rbx
    mov edx, eax
    mov rcx, rbx
    call WriteStdoutN
    add rsp, 20h
    pop rbx
    ret
WriteStdoutA ENDP

; Callback for streaming pieces: (piece,len,user)
; RCX=piece, EDX=len, R8=user
TokenCallback PROC
    ; RCX=piece, EDX=len, R8=user (ignored)
    jmp WriteStdoutN
TokenCallback ENDP

;------------------------------------------------------------------------------
; Minimal CRT stubs
; Some MASM modules declare these as EXTERN; for the CLI verifier we link
; kernel32-only, so provide tiny implementations to satisfy the linker.
;------------------------------------------------------------------------------
PUBLIC sprintf_s
sprintf_s PROC
    ; RCX=dest, RDX=destsz, R8=format, R9=...
    ; Best-effort: write empty string.
    test rcx, rcx
    jz  @@done
    mov byte ptr [rcx], 0
@@done:
    xor eax, eax
    ret
sprintf_s ENDP

PUBLIC strcpy_s
strcpy_s PROC
    ; RCX=dest, RDX=destsz, R8=src
    ; Best-effort bounded copy, always NUL-terminates if destsz > 0.
    test rcx, rcx
    jz @@err
    test rdx, rdx
    jz @@err
    test r8, r8
    jz @@term

    mov r9, rcx             ; dst
    mov r10, r8             ; src
    mov r11, rdx
    dec r11                 ; max bytes to copy excluding NUL
    xor eax, eax
@@cpy:
    test r11, r11
    jz @@term
    mov al, byte ptr [r10]
    test al, al
    jz @@term
    mov byte ptr [r9], al
    inc r9
    inc r10
    dec r11
    jmp @@cpy

@@term:
    mov byte ptr [r9], 0
    xor eax, eax            ; 0 = success
    ret

@@err:
    mov eax, 22             ; EINVAL
    ret
strcpy_s ENDP

; Read a line into g_inbuf. Returns EAX=1 if non-empty, 0 otherwise.
ReadLine PROC FRAME
    ; Win64 ABI: reserve 32-byte shadow + space for 5th arg, keep alignment.
    sub rsp, 30h
    .endprolog
    
    ; ReadFile(stdin, buf, sizeof-1, &bytes, 0)
    mov rcx, g_hStdin
    lea rdx, g_inbuf
    mov r8d, 4095
    lea r9, g_bytesRead
    mov qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz @@empty

    mov rax, g_bytesRead
    test rax, rax
    jz @@empty

    ; NUL terminate, strip \r\n
    lea rcx, g_inbuf
    mov rdx, rax
    add rcx, rdx
    mov byte ptr [rcx], 0

    ; Strip trailing CR/LF
    lea rcx, g_inbuf
    mov r8, g_bytesRead
    test r8, r8
    jz @@empty
    dec r8
@@strip:
    cmp r8, 0
    jl @@done
    mov al, byte ptr [rcx+r8]
    cmp al, 13
    je @@zap
    cmp al, 10
    je @@zap
    jmp @@done
@@zap:
    mov byte ptr [rcx+r8], 0
    dec r8
    jmp @@strip

@@done:
    ; Non-empty?
    cmp byte ptr [g_inbuf], 0
    je @@empty
    mov eax, 1
    add rsp, 30h
    ret
    
@@empty:
    xor eax, eax
    add rsp, 30h
    ret
ReadLine ENDP

; Compare g_inbuf with "exit" (case-sensitive). Returns EAX=1 if match.
IsExit PROC FRAME
    push rsi
    push rdi
    sub rsp, 20h
    .endprolog

    lea rsi, g_inbuf
    lea rdi, szExit
@@cmp:
    mov al, [rsi]
    mov dl, [rdi]
    cmp al, dl
    jne @@no
    test al, al
    je @@yes
    inc rsi
    inc rdi
    jmp @@cmp
@@yes:
    mov eax, 1
    jmp @@done
@@no:
    xor eax, eax
@@done:
    add rsp, 20h
    pop rdi
    pop rsi
    ret
IsExit ENDP

;------------------------------------------------------------------------------
; Entry point
;------------------------------------------------------------------------------
PUBLIC mainCRTStartup
mainCRTStartup PROC FRAME
    sub rsp, 28h
    .endprolog

    ; Std handles
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov g_hStdout, rax
    invoke GetStdHandle, -10
    mov g_hStdin, rax

    invoke WriteStdoutA, OFFSET szBanner

    ; Create bridge (loads llama.dll + model)
    invoke LlamaBridge_CreateA, OFFSET szModelPath
    test rax, rax
    jz @@exit_fail
    mov g_pBridge, rax

@@repl:
    invoke WriteStdoutA, OFFSET szPrompt
    call ReadLine
    test eax, eax
    jz @@repl

    call IsExit
    test eax, eax
    jnz @@shutdown

    ; Generate streaming response
    mov rcx, g_pBridge
    lea rdx, g_inbuf
    lea r8, TokenCallback
    xor r9, r9
    call LlamaBridge_GenerateStreamA

    invoke WriteStdoutA, OFFSET szNewline
    jmp @@repl

@@shutdown:
    mov rcx, g_pBridge
    call LlamaBridge_Destroy
    xor ecx, ecx
    call ExitProcess

@@exit_fail:
    mov ecx, 1
    call ExitProcess

mainCRTStartup ENDP

END
