.686P
.XMM
.model flat, c
OPTION CASEMAP:NONE

;============================================================================
; RawrXD Native Core - v15.0.0-AUTONOMOUS
; Built via Titan JIT Stage 13+ (Self-Hosting Bootstrap)
; No .NET dependencies. Pure Win32/x64.
;============================================================================

extern OutputDebugStringA: proc
extern GetStdHandle: proc
extern WriteFile: proc
extern CreateFileA: proc
extern CloseHandle: proc
extern ReadFile: proc
extern GetFileSize: proc
extern HeapAlloc: proc
extern GetProcessHeap: proc
extern HeapFree: proc
extern ExitProcess: proc

.data
; Logs/Debug
msgInit             db "[RAWR_CORE] Bootstrapping Native Core DLL...", 0
msgSafetyPass       db "[RAWR_CORE] Security: Input Safety CHECK: OK", 0
msgSafetyFail       db "[RAWR_CORE] Security: Input Safety CHECK: BLOCKED", 0
msgFileLoadOK       db "[RAWR_CORE] FILE: Loaded successfully (%d bytes)", 0
msgFileLoadErr      db "[RAWR_CORE] FILE: Load failed", 0

; Security Patterns (Test-InputSafety)
patExec             db "exec", 0
patEval             db "eval", 0
patSystem           db "system", 0
patPS               db "powershell", 0
patNet              db "dotnet", 0

.code

;----------------------------------------------------------------------------
; Core_Initialize
; DLL Entry point equivalent for specialized initialization
;----------------------------------------------------------------------------
Core_Initialize proc
    sub rsp, 40
    lea rcx, msgInit
    call OutputDebugStringA
    mov eax, 1                      ; Return SUCCESS
    add rsp, 40
    ret
Core_Initialize endp

;----------------------------------------------------------------------------
; Internal_StrStrCase (Case-insensitive)
; rcx = haystack, rdx = needle
;----------------------------------------------------------------------------
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
    ; Upper to Lower
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

;----------------------------------------------------------------------------
; Core_TestInputSafety (Native Replacement)
; Returns 1 (safe) or 0 (dangerous)
;----------------------------------------------------------------------------
Core_TestInputSafety proc
    push rbp
    mov rbp, rsp
    push rsi
    mov rsi, rcx                    ; input string
    
    test rsi, rsi
    jz @ok
    
    ; Check Patterns
    lea rdx, patExec
    call Internal_StrStrCase
    test rax, rax
    jnz @fail
    
    lea rdx, patEval
    call Internal_StrStrCase
    test rax, rax
    jnz @fail
    
    lea rdx, patPS
    call Internal_StrStrCase
    test rax, rax
    jnz @fail

@ok:
    lea rcx, msgSafetyPass
    call OutputDebugStringA
    mov eax, 1
    jmp @done
@fail:
    lea rcx, msgSafetyFail
    call OutputDebugStringA
    xor eax, eax
@done:
    pop rsi
    pop rbp
    ret
Core_TestInputSafety endp

;----------------------------------------------------------------------------
; Core_FastFileRead (Native Performance)
; rcx = filePath
; Returns Buffer Pointer or 0
;----------------------------------------------------------------------------
Core_FastFileRead proc
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; CreateFileA
    mov rdx, 80000000h              ; GENERIC_READ
    mov r8, 1                       ; SHARE_READ
    xor r9, r9
    mov qword ptr [rsp+32], 3       ; OPEN_EXISTING
    mov qword ptr [rsp+40], 80h     ; NORMAL
    call CreateFileA
    
    cmp rax, -1
    je @err
    mov rbx, rax                    ; Handle
    
    ; GetSize
    mov rcx, rbx
    xor rdx, rdx
    call GetFileSize
    mov r12, rax                    ; Size in r12
    
    ; Alloc
    call GetProcessHeap
    mov rcx, rax
    mov rdx, 8                      ; ZERO_INIT
    mov r8, r12
    inc r8                          ; +1 for null term
    call HeapAlloc
    mov r13, rax                    ; Buffer in r13
    
    ; Read
    mov rcx, rbx
    mov rdx, r13
    mov r8, r12
    lea r9, [rsp+48]                ; BytesRead
    mov qword ptr [rsp+32], 0
    call ReadFile
    
    ; Close
    mov rcx, rbx
    call CloseHandle
    
    mov rax, r13
    jmp @done

@err:
    lea rcx, msgFileLoadErr
    call OutputDebugStringA
    xor rax, rax
@done:
    add rsp, 64
    pop rbp
    ret
Core_FastFileRead endp

END
