;==============================================================================
; tool_registry.asm - Minimal real ToolRegistry implementation (pure MASM)
;==============================================================================
; Provides the ToolRegistry entry points required by the MASM test frameworks.
; Implements a small subset of tools used by tests:
;   - file_write
;   - file_read
;   - file_delete
;   - file_exists
;   - git_status (simulated success)
;
; Also exports `szToolFileExists` to satisfy smoke_test_suite.asm.
;==============================================================================

option casemap:none

; Win32 constants
GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
FILE_SHARE_READ         EQU 1
OPEN_EXISTING           EQU 3
CREATE_ALWAYS           EQU 2
FILE_ATTRIBUTE_NORMAL   EQU 80h
INVALID_HANDLE_VALUE    EQU -1
INVALID_FILE_ATTRIBUTES EQU 0FFFFFFFFh
INFINITE                EQU 0FFFFFFFFh

; Win32 Structs
STARTUPINFOA STRUCT
    cb              DWORD ?
    lpReserved      QWORD ?
    lpDesktop       QWORD ?
    lpTitle         QWORD ?
    dwX             DWORD ?
    dwY             DWORD ?
    dwXSize         DWORD ?
    dwYSize         DWORD ?
    dwXCountChars   DWORD ?
    dwYCountChars   DWORD ?
    dwFillAttribute DWORD ?
    dwFlags         DWORD ?
    wShowWindow     WORD  ?
    cbReserved2     WORD  ?
    lpReserved2     QWORD ?
    hStdInput       QWORD ?
    hStdOutput      QWORD ?
    hStdError       QWORD ?
STARTUPINFOA ENDS

PROCESS_INFORMATION STRUCT
    hProcess    QWORD ?
    hThread     QWORD ?
    dwProcessId DWORD ?
    dwThreadId  DWORD ?
PROCESS_INFORMATION ENDS

PUBLIC ToolRegistry_QueryAvailableTools
PUBLIC ToolRegistry_InvokeToolSet
PUBLIC szToolFileExists
PUBLIC FindSubstringPtr

EXTERN lstrcmpA:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN DeleteFileA:PROC
EXTERN GetFileAttributesA:PROC
EXTERN CreateProcessA:PROC
EXTERN WaitForSingleObject:PROC

.data
szToolFileExists     BYTE "file_exists", 0

szToolsList          BYTE "file_write\nfile_read\nfile_delete\nfile_exists\ngit_status\ngit_add\ngit_commit\n", 0

szKeyPath            BYTE '"path":"', 0
szKeyContent         BYTE '"content":"', 0
szKeyMessage         BYTE '"message":"', 0

szOk                 BYTE "OK", 0
szTrue               BYTE "true", 0
szFalse              BYTE "false", 0
szGitStatusOk        BYTE "git_status: ok", 0

szToolFileWrite      BYTE "file_write", 0
szToolFileRead       BYTE "file_read", 0
szToolFileDelete     BYTE "file_delete", 0
szToolGitStatus      BYTE "git_status", 0
szToolGitAdd         BYTE "git_add", 0
szToolGitCommit      BYTE "git_commit", 0

szGitAddCmd          BYTE "git add ", 0
szGitCommitCmd       BYTE "git commit -m ", 0

.data?
pathBuf              BYTE 1024 DUP(?)
contentBuf           BYTE 4096 DUP(?)
cmdLineBuf           BYTE 2048 DUP(?)
dwBytesIO            DWORD ?

; STARTUPINFOA and PROCESS_INFORMATION for CreateProcessA
align 8
startupInfo STARTUPINFOA <?>
processInfo PROCESS_INFORMATION <?>

.code

;------------------------------------------------------------------------------
; CopyStringBounded
; rcx = dest
; rdx = src
; r8  = dest size (bytes)
; Always NUL-terminates when size > 0.
;------------------------------------------------------------------------------
CopyStringBounded PROC
    push rbx
    push rsi
    push rdi

    mov rdi, rcx
    mov rsi, rdx
    mov rbx, r8

    test rbx, rbx
    jz copy_done

    dec rbx                 ; reserve space for NUL

copy_loop2:
    cmp rbx, 0
    je force_nul
    mov al, byte ptr [rsi]
    test al, al
    je force_nul
    mov byte ptr [rdi], al
    inc rsi
    inc rdi
    dec rbx
    jmp copy_loop2

force_nul:
    mov byte ptr [rdi], 0

copy_done:
    pop rdi
    pop rsi
    pop rbx
    ret
CopyStringBounded ENDP

;------------------------------------------------------------------------------
; NormalizePathSlashes
; rcx = path buffer (in-place). Replaces '/' with '\\'.
;------------------------------------------------------------------------------
NormalizePathSlashes PROC
    push rax
    push rcx

norm_loop:
    mov al, byte ptr [rcx]
    test al, al
    jz norm_done
    cmp al, '/'
    jne norm_next
    mov byte ptr [rcx], 5Ch
norm_next:
    inc rcx
    jmp norm_loop

norm_done:
    pop rcx
    pop rax
    ret
NormalizePathSlashes ENDP

;------------------------------------------------------------------------------
; FindSubstringPtr
; rcx = haystack (LPCSTR)
; rdx = needle   (LPCSTR)
; returns rax = pointer to first match, or 0
;------------------------------------------------------------------------------
FindSubstringPtr PROC
    push rbx
    push rsi
    push rdi

    mov rsi, rcx            ; haystack
    mov rdi, rdx            ; needle

    ; Empty needle => match at start
    mov al, byte ptr [rdi]
    test al, al
    jz found_start

next_h:
    mov al, byte ptr [rsi]
    test al, al
    jz not_found

    mov rbx, rsi            ; candidate start
    mov rcx, rsi
    mov rdx, rdi

cmp_loop:
    mov al, byte ptr [rdx]
    test al, al
    jz found_candidate

    mov ah, byte ptr [rcx]
    test ah, ah
    jz not_found

    cmp ah, al
    jne advance

    inc rcx
    inc rdx
    jmp cmp_loop

advance:
    inc rsi
    jmp next_h

found_candidate:
    mov rax, rbx
    jmp done

found_start:
    mov rax, rsi
    jmp done

not_found:
    xor rax, rax

done:
    pop rdi
    pop rsi
    pop rbx
    ret
FindSubstringPtr ENDP

;------------------------------------------------------------------------------
; ExtractQuotedValue
; rcx = pointer to start of value (first char after opening quote)
; rdx = dest buffer
; r8d = dest size
; returns eax = 1 on success, 0 on failure
;------------------------------------------------------------------------------
ExtractQuotedValue PROC
    push rbx
    push rsi
    push rdi

    mov rsi, rcx
    mov rdi, rdx
    mov ebx, r8d

    cmp ebx, 1
    jb fail

    dec ebx                 ; reserve space for NUL

copy_loop:
    mov al, byte ptr [rsi]
    test al, al
    jz fail

    cmp al, '"'
    je finish

    cmp ebx, 0
    je fail

    mov byte ptr [rdi], al
    inc rdi
    inc rsi
    dec ebx
    jmp copy_loop

finish:
    mov byte ptr [rdi], 0
    mov eax, 1
    jmp done

fail:
    mov byte ptr [rdi], 0
    xor eax, eax

done:
    pop rdi
    pop rsi
    pop rbx
    ret
ExtractQuotedValue ENDP

;------------------------------------------------------------------------------
; ToolRegistry_QueryAvailableTools
; rcx = ToolRegistry* (ignored)
; rdx = output buffer
; r8  = buffer size
; returns eax = 1 on success
;------------------------------------------------------------------------------
ToolRegistry_QueryAvailableTools PROC
    sub rsp, 28h

    ; Copy tools list to output buffer (best-effort)
    mov rcx, rdx
    lea rdx, [szToolsList]
    ; r8 already = buffer size
    call CopyStringBounded

    mov eax, 1
    add rsp, 28h
    ret
ToolRegistry_QueryAvailableTools ENDP

;------------------------------------------------------------------------------
; ToolRegistry_InvokeToolSet
; rcx = ToolRegistry* (ignored)
; rdx = tool name (LPCSTR)
; r8  = params (LPCSTR)
; r9  = output buffer
; [rsp+20h] = output buffer size (QWORD)
; returns eax = 1 on success, 0 on failure
;------------------------------------------------------------------------------
ToolRegistry_InvokeToolSet PROC
    push rbx
    push rsi
    push rdi
    push r14
    sub rsp, 40h

    mov rsi, rdx            ; tool name
    mov rdi, r8             ; params
    mov rbx, r9             ; out buffer
    ; out buffer size (arg5) is inconsistently passed by existing callers.
    ; Prefer [entry+28h] (ABI-correct) but also accept [entry+20h] (legacy).
    mov rax, qword ptr [rsp + 78h]   ; entry + 20h
    mov rdx, qword ptr [rsp + 80h]   ; entry + 28h
    test rax, rax
    jz use_abi_size
    cmp rax, 100000h
    ja use_abi_size
    mov qword ptr [rsp + 38h], rax
    jmp size_done

use_abi_size:
    mov qword ptr [rsp + 38h], rdx

size_done:

    ; Dispatch by tool name
    mov rcx, rsi
    lea rdx, [szToolFileWrite]
    call lstrcmpA
    test eax, eax
    jz do_file_write

    mov rcx, rsi
    lea rdx, [szToolFileRead]
    call lstrcmpA
    test eax, eax
    jz do_file_read

    mov rcx, rsi
    lea rdx, [szToolFileDelete]
    call lstrcmpA
    test eax, eax
    jz do_file_delete

    mov rcx, rsi
    lea rdx, [szToolFileExists]
    call lstrcmpA
    test eax, eax
    jz do_file_exists

    mov rcx, rsi
    lea rdx, [szToolGitStatus]
    call lstrcmpA
    test eax, eax
    jz do_git_status

    mov rcx, rsi
    lea rdx, [szToolGitAdd]
    call lstrcmpA
    test eax, eax
    jz do_git_add

    mov rcx, rsi
    lea rdx, [szToolGitCommit]
    call lstrcmpA
    test eax, eax
    jz do_git_commit

    ; Unknown tool
    xor eax, eax
    jmp done

; ---- file_write ----
do_file_write:
    ; Extract path
    mov rcx, rdi
    lea rdx, [szKeyPath]
    call FindSubstringPtr
    test rax, rax
    jz fail
    add rax, 8              ; length of '"path":"'
    mov rcx, rax
    lea rdx, [pathBuf]
    mov r8d, 1024
    call ExtractQuotedValue
    test eax, eax
    jz fail

    lea rcx, [pathBuf]
    call NormalizePathSlashes

    ; Extract content
    mov rcx, rdi
    lea rdx, [szKeyContent]
    call FindSubstringPtr
    test rax, rax
    jz fail
    add rax, 11             ; length of '"content":"'
    mov rcx, rax
    lea rdx, [contentBuf]
    mov r8d, 4096
    call ExtractQuotedValue
    test eax, eax
    jz fail

    ; Create/open file (CREATE_ALWAYS)
    lea rcx, [pathBuf]
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    mov dword ptr [rsp+20h], CREATE_ALWAYS
    mov dword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je fail

    mov r11, rax             ; file handle

    ; compute content length
    lea rcx, [contentBuf]
    call lstrlenA
    mov r8d, eax

    ; Write content
    mov rcx, r11
    lea rdx, [contentBuf]
    lea r9, [dwBytesIO]
    mov qword ptr [rsp+20h], 0
    call WriteFile

    ; Preserve result across CloseHandle
    mov dword ptr [rsp+28h], eax

    ; Close handle
    mov rcx, r11
    call CloseHandle

    mov eax, dword ptr [rsp+28h]
    test eax, eax
    jz fail

    ; Return OK
    mov rcx, rbx
    lea rdx, [szOk]
    mov r8, qword ptr [rsp + 38h]
    call CopyStringBounded

    mov eax, 1
    jmp done

fail:
    xor eax, eax
    jmp done

; Placeholder blocks filled by a later patch

do_file_read:
    ; Extract path
    mov rcx, rdi
    lea rdx, [szKeyPath]
    call FindSubstringPtr
    test rax, rax
    jz fail
    add rax, 8
    mov rcx, rax
    lea rdx, [pathBuf]
    mov r8d, 1024
    call ExtractQuotedValue
    test eax, eax
    jz fail

    lea rcx, [pathBuf]
    call NormalizePathSlashes

    ; Open file (OPEN_EXISTING)
    lea rcx, [pathBuf]
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d
    mov dword ptr [rsp+20h], OPEN_EXISTING
    mov dword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je fail
    mov r14, rax            ; r14 = handle

    ; Read file into out buffer (up to outSize-1)
    mov rcx, r14
    mov rdx, rbx
    mov r8, qword ptr [rsp + 38h]
    cmp r8, 1
    jbe read_close_fail
    dec r8
    lea r9, [dwBytesIO]
    mov qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz read_close_fail

    ; Null-terminate
    lea r11, [dwBytesIO]
    mov eax, [r11]
    cmp rax, qword ptr [rsp + 38h]
    jae read_close_fail
    mov byte ptr [rbx + rax], 0

    mov rcx, r14
    call CloseHandle

    mov eax, 1
    jmp done

read_close_fail:
    mov rcx, r14
    call CloseHandle
    xor eax, eax
    jmp done

do_file_delete:
    ; Extract path
    mov rcx, rdi
    lea rdx, [szKeyPath]
    call FindSubstringPtr
    test rax, rax
    jz fail
    add rax, 8
    mov rcx, rax
    lea rdx, [pathBuf]
    mov r8d, 1024
    call ExtractQuotedValue
    test eax, eax
    jz fail

    lea rcx, [pathBuf]
    call NormalizePathSlashes

    lea rcx, [pathBuf]
    call DeleteFileA
    test eax, eax
    jz fail

    mov rcx, rbx
    lea rdx, [szOk]
    mov r8, qword ptr [rsp + 38h]
    call CopyStringBounded

    mov eax, 1
    jmp done

do_file_exists:
    ; Extract path
    mov rcx, rdi
    lea rdx, [szKeyPath]
    call FindSubstringPtr
    test rax, rax
    jz fail
    add rax, 8
    mov rcx, rax
    lea rdx, [pathBuf]
    mov r8d, 1024
    call ExtractQuotedValue
    test eax, eax
    jz fail

    lea rcx, [pathBuf]
    call NormalizePathSlashes

    lea rcx, [pathBuf]
    call GetFileAttributesA
    cmp eax, INVALID_FILE_ATTRIBUTES
    jne exists_true

    mov rcx, rbx
    lea rdx, [szFalse]
    mov r8, qword ptr [rsp + 38h]
    call CopyStringBounded
    mov eax, 1
    jmp done

exists_true:
    mov rcx, rbx
    lea rdx, [szTrue]
    mov r8, qword ptr [rsp + 38h]
    call CopyStringBounded
    mov eax, 1
    jmp done

do_git_status:
    mov rcx, rbx
    lea rdx, [szGitStatusOk]
    mov r8, qword ptr [rsp + 38h]
    call CopyStringBounded
    mov eax, 1
    jmp done

do_git_add:
    ; Extract path
    mov rcx, rdi
    mov rdx, OFFSET szKeyPath
    call FindSubstringPtr
    test rax, rax
    jz fail
    add rax, 8
    mov rcx, rax
    lea rdx, pathBuf
    mov r8d, 1024
    call ExtractQuotedValue
    test eax, eax
    jz fail

    ; Construct command: "git add <path>"
    lea rcx, [cmdLineBuf]
    lea rdx, [szGitAddCmd]
    call lstrcpyA
    lea rcx, [cmdLineBuf]
    lea rdx, [pathBuf]
    call lstrcatA

    call RunGitCommand
    test eax, eax
    jz fail

    mov rcx, rbx
    lea rdx, [szOk]
    mov r8, qword ptr [rsp + 38h]
    call CopyStringBounded
    mov eax, 1
    jmp done

do_git_commit:
    ; Extract message
    mov rcx, rdi
    lea rdx, [szKeyMessage]
    call FindSubstringPtr
    test rax, rax
    jz fail
    add rax, 11             ; length of '"message":"'
    mov rcx, rax
    lea rdx, [contentBuf]     ; reuse contentBuf for message
    mov r8d, 4096
    call ExtractQuotedValue
    test eax, eax
    jz fail

    ; Construct command: git commit -m "message"
    lea rcx, [cmdLineBuf]
    lea rdx, [szGitCommitCmd]
    call lstrcpyA
    
    ; Add opening quote
    lea rcx, [cmdLineBuf]
    call lstrlenA
    lea r11, [cmdLineBuf]
    mov byte ptr [r11 + rax], '"'
    mov byte ptr [r11 + rax + 1], 0

    lea rcx, [cmdLineBuf]
    lea rdx, [contentBuf]
    call lstrcatA

    ; Add closing quote
    lea rcx, [cmdLineBuf]
    call lstrlenA
    lea r11, [cmdLineBuf]
    mov byte ptr [r11 + rax], '"'
    mov byte ptr [r11 + rax + 1], 0

    call RunGitCommand
    test eax, eax
    jz fail

    mov rcx, rbx
    lea rdx, [szOk]
    mov r8, qword ptr [rsp + 38h]
    call CopyStringBounded
    mov eax, 1
    jmp done

; Helper to run git command
RunGitCommand PROC
    sub rsp, 58h

    ; Zero out startupInfo and processInfo
    lea rdi, [startupInfo]
    mov ecx, (SIZEOF STARTUPINFOA) / 8
    xor rax, rax
    rep stosq

    lea rdi, [processInfo]
    mov ecx, (SIZEOF PROCESS_INFORMATION) / 8
    xor rax, rax
    rep stosq

    lea r11, [startupInfo]
    mov [r11].STARTUPINFOA.cb, SIZEOF STARTUPINFOA

    ; CreateProcessA(NULL, cmdLineBuf, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo)
    xor rcx, rcx
    lea rdx, [cmdLineBuf]
    xor r8, r8
    xor r9, r9
    mov dword ptr [rsp + 20h], 0    ; bInheritHandles = FALSE
    mov dword ptr [rsp + 28h], 0    ; dwCreationFlags = 0
    mov qword ptr [rsp + 30h], 0    ; lpEnvironment = NULL
    mov qword ptr [rsp + 38h], 0    ; lpCurrentDirectory = NULL
    lea rax, [startupInfo]
    mov qword ptr [rsp + 40h], rax
    lea rax, [processInfo]
    mov qword ptr [rsp + 48h], rax
    call CreateProcessA

    test eax, eax
    jz run_fail

    ; Wait for process to finish
    lea r11, [processInfo]
    mov rcx, [r11].PROCESS_INFORMATION.hProcess
    mov rdx, INFINITE
    call WaitForSingleObject

    ; Close handles
    lea r11, [processInfo]
    mov rcx, [r11].PROCESS_INFORMATION.hProcess
    call CloseHandle
    mov rcx, [r11].PROCESS_INFORMATION.hThread
    call CloseHandle

    mov eax, 1
    jmp run_done

run_fail:
    xor eax, eax

run_done:
    add rsp, 58h
    ret
RunGitCommand ENDP

done:
    add rsp, 40h
    pop r14
    pop rdi
    pop rsi
    pop rbx
    ret
ToolRegistry_InvokeToolSet ENDP

end
