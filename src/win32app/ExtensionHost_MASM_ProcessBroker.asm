; ExtensionHost_MASM_ProcessBroker.asm
; Phase 2 Day 7: MASM Process Isolation & IPC Gate
; x64 Assembly - Secure process creation with memory isolation and job object management

extern CreateJobObjectA:ptr
extern SetInformationJobObject:ptr
extern AssignProcessToJobObject:ptr
extern CreateProcessA:ptr
extern CloseHandle:ptr
extern GetLastError:ptr
extern SetEvent:ptr
extern ResetEvent:ptr
extern ResumeThread:ptr

; Constants
PROCESS_CREATE_SUSPENDED        EQU 00000004h
CREATE_NEW_CONSOLE              EQU 00000010h
JOB_OBJECT_LIMIT_MEMORY         EQU 00000100h
JOB_OBJECT_LIMIT_PROCESS_TIME   EQU 00000002h
JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE EQU 00002000h
JobObjectExtendedLimitInformation EQU 9

.code

;;; MASM_CreateIsolatedProcess
;;; RCX = AppName, RDX = CmdLine, R8 = WorkDir, R9D = MemLimitMB
MASM_CreateIsolatedProcess PROC
    push rbp
    mov rbp, rsp
    sub rsp, 200h

    ; Local Storage
    mov qword ptr [rbp-08h], rcx  ; AppName
    mov qword ptr [rbp-10h], rdx ; CmdLine
    mov qword ptr [rbp-18h], r8  ; WorkDir
    mov dword ptr [rbp-20h], r9d; MemLimitMB

    ; 1. Create Job Object
    xor rcx, rcx
    xor rdx, rdx
    call CreateJobObjectA
    test rax, rax
    jz fail_no_cleanup
    mov qword ptr [rbp-28h], rax

    ; 2. Set Job Limits (144 bytes for JOBOBJECT_EXTENDED_LIMIT_INFORMATION)
    lea rdi, [rbp-0C0h]
    mov rcx, 144
    xor al, al
    rep stosb

    mov rax, 0
    mov eax, dword ptr [rbp-20h]
    shl rax, 20                 ; * 1024 * 1024
    mov [rbp-0C0h + 112], rax   ; JobMemoryLimit (ExtendedLimitInfo offset)
    mov dword ptr [rbp-0C0h + 16], JOB_OBJECT_LIMIT_MEMORY
    or dword ptr [rbp-0C0h + 16], JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE

    mov rcx, qword ptr [rbp-28h]
    mov rdx, JobObjectExtendedLimitInformation
    lea r8, [rbp-0C0h]
    mov r9, 144
    call SetInformationJobObject
    test rax, rax
    jz fail_close_job

    ; 3. CreateProcess Suspended
    ; STARTUPINFOA (104 bytes)
    lea rdi, [rbp-130h]
    mov rcx, 104
    xor al, al
    rep stosb
    mov dword ptr [rbp-130h], 104

    mov rcx, qword ptr [rbp-08h]
    mov rdx, qword ptr [rbp-10h]
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+32], 0
    mov r10d, PROCESS_CREATE_SUSPENDED
    or r10d, CREATE_NEW_CONSOLE
    mov dword ptr [rsp+40], r10d
    xor r10, r10
    mov qword ptr [rsp+48], r10
    mov r10, qword ptr [rbp-18h]
    mov qword ptr [rsp+56], r10
    lea r10, [rbp-130h]
    mov qword ptr [rsp+64], r10
    lea r10, [rbp-150h]         ; PROCESS_INFORMATION (24 bytes)
    mov qword ptr [rsp+72], r10
    call CreateProcessA
    test rax, rax
    jz fail_close_job

    ; 4. Assign to Job
    mov rcx, qword ptr [rbp-28h]
    mov rdx, qword ptr [rbp-150h]
    call AssignProcessToJobObject
    test rax, rax
    jz fail_close_job

    ; 5. Resume
    mov rcx, qword ptr [rbp-150h + 8] ; hThread
    call ResumeThread

    mov rax, qword ptr [rbp-150h]     ; hProcess
    jmp done

fail_close_job:
    mov rcx, qword ptr [rbp-28h]
    call CloseHandle
fail_no_cleanup:
    xor rax, rax
done:
    lea rsp, [rbp]
    pop rbp
    ret
MASM_CreateIsolatedProcess ENDP

END