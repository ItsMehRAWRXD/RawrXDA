; ============================================================================
; RawrXD_SourceEdit_Kernel.asm — MASM64 Atomic Source File Replacement
; ============================================================================
;
; Provides two exported procedures for safe source-level self-modification:
;
;   SourceEdit_AtomicReplace:
;     Atomically replaces a source file's contents with backup guarantee.
;     Transaction: backup → write temp → rename → cleanup
;     On any failure, the original file is guaranteed untouched.
;
;   SourceEdit_GitCommand:
;     Launches git.exe with given arguments via CreateProcessW,
;     waits for completion, captures stdout.
;
; Calling Convention: Microsoft x64 (RCX, RDX, R8, R9 + stack)
; Error Model: RAX = 0 on success, GetLastError() on failure
; Threading: Not thread-safe — caller must serialize (orchestrator mutex)
;
; Integration:
;   - Called by AutonomousRecoveryOrchestrator::atomicWriteSourceFile()
;   - Called by AutonomousRecoveryOrchestrator::gitAdd/gitCommit()
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
; ============================================================================

OPTION CASEMAP:NONE

; ============================================================================
; Win32 API imports
; ============================================================================
EXTERN CreateFileW:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN DeleteFileW:PROC
EXTERN MoveFileExW:PROC
EXTERN ReplaceFileW:PROC
EXTERN GetLastError:PROC
EXTERN GetFileSize:PROC
EXTERN FlushFileBuffers:PROC
EXTERN SetFileAttributesW:PROC
EXTERN GetFileAttributesW:PROC
EXTERN CreateProcessW:PROC
EXTERN WaitForSingleObject:PROC
EXTERN GetExitCodeProcess:PROC
EXTERN TerminateProcess:PROC
EXTERN CreatePipe:PROC
EXTERN SetHandleInformation:PROC

; ============================================================================
; Constants
; ============================================================================
GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
CREATE_ALWAYS           EQU 2
OPEN_EXISTING           EQU 3
FILE_ATTRIBUTE_NORMAL   EQU 80h
INVALID_HANDLE_VALUE    EQU -1
MOVEFILE_REPLACE_EXISTING EQU 1h
MOVEFILE_WRITE_THROUGH  EQU 8h
FILE_ATTRIBUTE_NORMAL_VAL EQU 80h
FILE_ATTRIBUTE_READONLY EQU 1h
INFINITE_WAIT           EQU 0FFFFFFFFh
CREATE_NO_WINDOW        EQU 08000000h
STARTF_USESTDHANDLES    EQU 00000100h
HANDLE_FLAG_INHERIT     EQU 00000001h
WAIT_OBJECT_0           EQU 0

; STARTUPINFOW size = 104 bytes on x64
STARTUPINFOW_SIZE       EQU 104
; PROCESS_INFORMATION size = 24 bytes on x64
PROCINFO_SIZE           EQU 24
; SECURITY_ATTRIBUTES size = 24 bytes on x64
SECATTR_SIZE            EQU 24

; ============================================================================
; Data segment
; ============================================================================
.DATA

    ALIGN 16
    szTmpSuffix     DW '.', 't', 'm', 'p', 0     ; L".tmp"
    szOldSuffix     DW '.', 'o', 'l', 'd', 0     ; L".old"

; ============================================================================
; Code segment
; ============================================================================
.CODE

; ============================================================================
; SourceEdit_AtomicReplace PROC
; ============================================================================
;
; Atomically replaces a source file with new content, creating a backup.
;
; Parameters (Microsoft x64 ABI):
;   RCX = originalPath   (const wchar_t*) — file to modify
;   RDX = backupPath     (const wchar_t*) — where to write backup
;   R8  = newContent     (const void*)    — new file content bytes
;   R9  = newContentLen  (uint64_t)       — length of new content
;
; Returns:
;   RAX = 0 on success, GetLastError() on failure
;
; Transaction steps:
;   1. Read original file into stack buffer
;   2. Write original content to backupPath
;   3. Write newContent to originalPath.tmp
;   4. MoveFileEx(originalPath → originalPath.old)
;   5. MoveFileEx(originalPath.tmp → originalPath)
;   6. DeleteFile(originalPath.old)
;
; On failure at step 4+, reverse the rename to restore original.
;
; Stack frame layout (large — uses 64KB buffer for file read):
;   [RSP+0]     shadow space (32 bytes)
;   [RSP+32]    local variables
;   [RSP+??]    temp path buffers (1024 bytes each)
;   [RSP+??]    file read buffer (up to 64KB)
;
; ============================================================================

SourceEdit_AtomicReplace PROC PUBLIC FRAME

    ; Prologue
    push    rbp
    .PUSHREG rbp
    push    rbx
    .PUSHREG rbx
    push    rsi
    .PUSHREG rsi
    push    rdi
    .PUSHREG rdi
    push    r12
    .PUSHREG r12
    push    r13
    .PUSHREG r13
    push    r14
    .PUSHREG r14
    push    r15
    .PUSHREG r15

    ; Reserve stack: 32 shadow + 80 locals + 2048 path bufs + 65536 read buf
    ;              = 67696 → round to 67712 (aligned 16)
    FRAME_SIZE EQU 67712
    sub     rsp, FRAME_SIZE
    .ALLOCSTACK FRAME_SIZE
    mov     rbp, rsp
    .SETFRAME rbp, 0
    .ENDPROLOG

    ; Save parameters
    mov     r12, rcx                    ; r12 = originalPath
    mov     r13, rdx                    ; r13 = backupPath
    mov     r14, r8                     ; r14 = newContent
    mov     r15, r9                     ; r15 = newContentLen

    ; Local variable offsets from RBP
    ; [rbp + 32]   = hOriginal (HANDLE)
    ; [rbp + 40]   = hBackup (HANDLE)
    ; [rbp + 48]   = hTemp (HANDLE)
    ; [rbp + 56]   = originalFileSize (DWORD)
    ; [rbp + 64]   = bytesReadWritten (DWORD)
    ; [rbp + 72]   = tmpPathBuf start (1024 wchar = 2048 bytes)
    ; [rbp + 2120] = oldPathBuf start (1024 wchar = 2048 bytes)
    ; [rbp + 4168] = readBuffer start (65536 bytes)

    TMP_PATH_OFF EQU 72
    OLD_PATH_OFF EQU 2120
    READ_BUF_OFF EQU 4168

    ; ---- Build temp path: originalPath + ".tmp" ----
    lea     rdi, [rbp + TMP_PATH_OFF]
    mov     rsi, r12
    xor     rcx, rcx
@@copy_tmp:
    mov     ax, WORD PTR [rsi + rcx*2]
    mov     WORD PTR [rdi + rcx*2], ax
    inc     rcx
    test    ax, ax
    jnz     @@copy_tmp
    ; rcx-1 = string length in wchar, append ".tmp"
    dec     rcx
    lea     rax, [szTmpSuffix]
    xor     rdx, rdx
@@append_tmp:
    mov     bx, WORD PTR [rax + rdx*2]
    mov     WORD PTR [rdi + rcx*2], bx
    inc     rcx
    inc     rdx
    test    bx, bx
    jnz     @@append_tmp

    ; ---- Build old path: originalPath + ".old" ----
    lea     rdi, [rbp + OLD_PATH_OFF]
    mov     rsi, r12
    xor     rcx, rcx
@@copy_old:
    mov     ax, WORD PTR [rsi + rcx*2]
    mov     WORD PTR [rdi + rcx*2], ax
    inc     rcx
    test    ax, ax
    jnz     @@copy_old
    dec     rcx
    lea     rax, [szOldSuffix]
    xor     rdx, rdx
@@append_old:
    mov     bx, WORD PTR [rax + rdx*2]
    mov     WORD PTR [rdi + rcx*2], bx
    inc     rcx
    inc     rdx
    test    bx, bx
    jnz     @@append_old

    ; ======== STEP 1: Read original file ========
    mov     rcx, r12                    ; lpFileName = originalPath
    mov     edx, GENERIC_READ           ; dwDesiredAccess
    mov     r8d, 1                      ; dwShareMode = FILE_SHARE_READ
    xor     r9, r9                      ; lpSecurityAttributes = NULL
    mov     DWORD PTR [rsp + 32], OPEN_EXISTING ; dwCreationDisposition
    mov     DWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov     QWORD PTR [rsp + 48], 0     ; hTemplateFile = NULL
    call    CreateFileW
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@fail_getlasterror
    mov     [rbp + 32], rax             ; hOriginal

    ; Get file size
    mov     rcx, rax                    ; hFile
    xor     rdx, rdx                    ; lpFileSizeHigh = NULL
    call    GetFileSize
    cmp     eax, 0FFFFFFFFh
    je      @@fail_close_orig
    mov     [rbp + 56], eax             ; originalFileSize
    ; Cap at 64KB for stack buffer
    cmp     eax, 65536
    ja      @@fail_close_orig           ; File too large for stack buffer

    ; Read original content
    mov     rcx, [rbp + 32]             ; hFile
    lea     rdx, [rbp + READ_BUF_OFF]  ; lpBuffer
    mov     r8d, [rbp + 56]             ; nNumberOfBytesToRead
    lea     r9, [rbp + 64]              ; lpNumberOfBytesRead
    mov     QWORD PTR [rsp + 32], 0     ; lpOverlapped = NULL
    call    ReadFile
    test    eax, eax
    jz      @@fail_close_orig

    ; Close original
    mov     rcx, [rbp + 32]
    call    CloseHandle

    ; ======== STEP 2: Write backup ========
    mov     rcx, r13                    ; lpFileName = backupPath
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d                    ; dwShareMode = 0
    xor     r9, r9                      ; lpSecurityAttributes = NULL
    mov     DWORD PTR [rsp + 32], CREATE_ALWAYS
    mov     DWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov     QWORD PTR [rsp + 48], 0
    call    CreateFileW
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@fail_getlasterror
    mov     [rbp + 40], rax             ; hBackup

    ; Write original content to backup
    mov     rcx, rax                    ; hFile
    lea     rdx, [rbp + READ_BUF_OFF]  ; lpBuffer (original content)
    mov     r8d, [rbp + 56]             ; nNumberOfBytesToWrite
    lea     r9, [rbp + 64]              ; lpNumberOfBytesWritten
    mov     QWORD PTR [rsp + 32], 0
    call    WriteFile
    test    eax, eax
    jz      @@fail_close_backup

    ; Close backup
    mov     rcx, [rbp + 40]
    call    CloseHandle

    ; ======== STEP 3: Write new content to temp file ========
    lea     rcx, [rbp + TMP_PATH_OFF]   ; lpFileName = tmpPath
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d
    xor     r9, r9
    mov     DWORD PTR [rsp + 32], CREATE_ALWAYS
    mov     DWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov     QWORD PTR [rsp + 48], 0
    call    CreateFileW
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@fail_getlasterror
    mov     [rbp + 48], rax             ; hTemp

    ; Write new content
    mov     rcx, rax                    ; hFile
    mov     rdx, r14                    ; lpBuffer = newContent
    mov     r8, r15                     ; nNumberOfBytesToWrite = newContentLen
    lea     r9, [rbp + 64]              ; lpNumberOfBytesWritten
    mov     QWORD PTR [rsp + 32], 0
    call    WriteFile
    test    eax, eax
    jz      @@fail_close_temp

    ; Close temp
    mov     rcx, [rbp + 48]
    call    CloseHandle

    ; ======== STEP 4: Rename original → .old ========
    mov     rcx, r12                    ; lpExistingFileName = originalPath
    lea     rdx, [rbp + OLD_PATH_OFF]   ; lpNewFileName = oldPath
    mov     r8d, MOVEFILE_REPLACE_EXISTING OR MOVEFILE_WRITE_THROUGH
    call    MoveFileExW
    test    eax, eax
    jz      @@fail_getlasterror         ; Original untouched on failure

    ; ======== STEP 5: Rename .tmp → original ========
    lea     rcx, [rbp + TMP_PATH_OFF]   ; lpExistingFileName = tmpPath
    mov     rdx, r12                    ; lpNewFileName = originalPath
    mov     r8d, MOVEFILE_REPLACE_EXISTING OR MOVEFILE_WRITE_THROUGH
    call    MoveFileExW
    test    eax, eax
    jz      @@fail_reverse_rename       ; Must restore original from .old

    ; ======== STEP 6: Delete .old (cleanup, non-fatal) ========
    lea     rcx, [rbp + OLD_PATH_OFF]
    call    DeleteFileW
    ; Ignore failure — .old is just cleanup

    ; ======== SUCCESS ========
    xor     rax, rax                    ; return 0
    jmp     @@epilogue

@@fail_reverse_rename:
    ; Restore: .old → original
    call    GetLastError
    push    rax                         ; save error code
    lea     rcx, [rbp + OLD_PATH_OFF]
    mov     rdx, r12
    mov     r8d, MOVEFILE_REPLACE_EXISTING
    call    MoveFileExW
    pop     rax                         ; restore error code
    jmp     @@epilogue

@@fail_close_temp:
    mov     rcx, [rbp + 48]
    call    CloseHandle
    jmp     @@fail_getlasterror

@@fail_close_backup:
    mov     rcx, [rbp + 40]
    call    CloseHandle
    jmp     @@fail_getlasterror

@@fail_close_orig:
    mov     rcx, [rbp + 32]
    call    CloseHandle
    ; Fall through to GetLastError

@@fail_getlasterror:
    call    GetLastError
    ; RAX = error code (nonzero)

@@epilogue:
    add     rsp, FRAME_SIZE
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret

SourceEdit_AtomicReplace ENDP

; ============================================================================
; SourceEdit_GitCommand PROC
; ============================================================================
;
; Launches git.exe with given arguments and captures stdout.
;
; Parameters (Microsoft x64 ABI):
;   RCX = gitExePath    (const wchar_t*) — path to git.exe
;   RDX = workingDir    (const wchar_t*) — repo root directory
;   R8  = args          (const wchar_t*) — command line arguments
;   R9  = stdoutBuf     (wchar_t*)       — output buffer (nullable)
;   [RSP+40] = stdoutBufLen (uint64_t)   — buffer size in wchar_t
;
; Returns:
;   RAX = process exit code (0 = success), -1 on launch failure
;
; Implementation:
;   1. Build command line: "gitExePath args"
;   2. Create pipe for stdout capture (if stdoutBuf != NULL)
;   3. CreateProcessW with working directory
;   4. WaitForSingleObject (30s timeout)
;   5. Read stdout pipe into buffer
;   6. Return exit code
;
; ============================================================================

SourceEdit_GitCommand PROC PUBLIC FRAME

    push    rbp
    .PUSHREG rbp
    push    rbx
    .PUSHREG rbx
    push    rsi
    .PUSHREG rsi
    push    rdi
    .PUSHREG rdi
    push    r12
    .PUSHREG r12
    push    r13
    .PUSHREG r13
    push    r14
    .PUSHREG r14
    push    r15
    .PUSHREG r15

    ; Stack: 32 shadow + locals + STARTUPINFOW(104) + PROCINFO(24) +
    ;        SECATTR(24) + cmdline buf(4096) + pipe handles(16)
    ;      = 32 + 16 + 104 + 24 + 24 + 4096 + 16 = 4312 → 4320 (align 16)
    GIT_FRAME EQU 4320
    sub     rsp, GIT_FRAME
    .ALLOCSTACK GIT_FRAME
    mov     rbp, rsp
    .SETFRAME rbp, 0
    .ENDPROLOG

    ; Save parameters
    mov     r12, rcx                    ; gitExePath
    mov     r13, rdx                    ; workingDir
    mov     r14, r8                     ; args
    mov     r15, r9                     ; stdoutBuf
    mov     rbx, [rbp + GIT_FRAME + 64 + 40]  ; stdoutBufLen (stack param after 8 pushes)

    ; Layout offsets:
    ; [rbp+32]    = hReadPipe (HANDLE, 8 bytes)
    ; [rbp+40]    = hWritePipe (HANDLE, 8 bytes)
    ; [rbp+48]    = SECURITY_ATTRIBUTES (24 bytes)
    ; [rbp+72]    = STARTUPINFOW (104 bytes)
    ; [rbp+176]   = PROCESS_INFORMATION (24 bytes)
    ; [rbp+200]   = command line buffer (4096 bytes, wchar_t)

    HREAD_OFF   EQU 32
    HWRITE_OFF  EQU 40
    SA_OFF      EQU 48
    SI_OFF      EQU 72
    PI_OFF      EQU 176
    CMD_OFF     EQU 200

    ; Initialize handles to NULL
    mov     QWORD PTR [rbp + HREAD_OFF], 0
    mov     QWORD PTR [rbp + HWRITE_OFF], 0

    ; ---- Build command line: "\"gitExePath\" args" ----
    lea     rdi, [rbp + CMD_OFF]
    ; Write opening quote
    mov     WORD PTR [rdi], '"'
    add     rdi, 2
    ; Copy gitExePath
    mov     rsi, r12
@@copy_exe:
    lodsw
    stosw
    test    ax, ax
    jnz     @@copy_exe
    ; Back up over null, write closing quote + space
    sub     rdi, 2
    mov     WORD PTR [rdi], '"'
    add     rdi, 2
    mov     WORD PTR [rdi], ' '
    add     rdi, 2
    ; Copy args
    mov     rsi, r14
@@copy_args:
    lodsw
    stosw
    test    ax, ax
    jnz     @@copy_args
    ; Command line is null-terminated

    ; ---- Create pipe for stdout (if buffer provided) ----
    test    r15, r15
    jz      @@skip_pipe

    ; Set up SECURITY_ATTRIBUTES
    lea     rcx, [rbp + SA_OFF]
    mov     DWORD PTR [rcx], SECATTR_SIZE       ; nLength
    mov     QWORD PTR [rcx + 8], 0              ; lpSecurityDescriptor = NULL
    mov     DWORD PTR [rcx + 16], 1             ; bInheritHandle = TRUE

    ; CreatePipe(&hRead, &hWrite, &sa, 0)
    lea     rcx, [rbp + HREAD_OFF]
    lea     rdx, [rbp + HWRITE_OFF]
    lea     r8, [rbp + SA_OFF]
    xor     r9d, r9d
    call    CreatePipe
    test    eax, eax
    jz      @@fail_launch

    ; Make read end non-inheritable
    mov     rcx, [rbp + HREAD_OFF]
    mov     edx, HANDLE_FLAG_INHERIT
    xor     r8d, r8d                            ; flags = 0 (clear inherit)
    call    SetHandleInformation

@@skip_pipe:

    ; ---- Zero STARTUPINFOW ----
    lea     rdi, [rbp + SI_OFF]
    xor     eax, eax
    mov     ecx, STARTUPINFOW_SIZE / 8
    rep     stosq
    ; Remaining bytes (if any)
    mov     ecx, STARTUPINFOW_SIZE AND 7
    rep     stosb

    ; Set cb
    lea     rax, [rbp + SI_OFF]
    mov     DWORD PTR [rax], STARTUPINFOW_SIZE  ; cb

    ; If capturing stdout, set handles
    test    r15, r15
    jz      @@skip_handles

    lea     rax, [rbp + SI_OFF]
    mov     DWORD PTR [rax + 60], STARTF_USESTDHANDLES  ; dwFlags at offset 60
    mov     rcx, [rbp + HWRITE_OFF]
    mov     [rax + 80], rcx                     ; hStdOutput at offset 80
    mov     [rax + 88], rcx                     ; hStdError at offset 88

@@skip_handles:

    ; ---- Zero PROCESS_INFORMATION ----
    lea     rdi, [rbp + PI_OFF]
    xor     eax, eax
    mov     ecx, PROCINFO_SIZE / 8
    rep     stosq

    ; ---- CreateProcessW ----
    xor     rcx, rcx                            ; lpApplicationName = NULL
    lea     rdx, [rbp + CMD_OFF]                ; lpCommandLine
    xor     r8, r8                              ; lpProcessAttributes = NULL
    xor     r9, r9                              ; lpThreadAttributes = NULL
    ; Stack params:
    mov     DWORD PTR [rsp + 32], 1             ; bInheritHandles = TRUE
    mov     DWORD PTR [rsp + 40], CREATE_NO_WINDOW ; dwCreationFlags
    mov     QWORD PTR [rsp + 48], 0             ; lpEnvironment = NULL
    mov     [rsp + 56], r13                     ; lpCurrentDirectory = workingDir
    lea     rax, [rbp + SI_OFF]
    mov     [rsp + 64], rax                     ; lpStartupInfo
    lea     rax, [rbp + PI_OFF]
    mov     [rsp + 72], rax                     ; lpProcessInformation
    call    CreateProcessW
    test    eax, eax
    jz      @@fail_launch

    ; Close write end of pipe in parent
    mov     rcx, [rbp + HWRITE_OFF]
    test    rcx, rcx
    jz      @@skip_close_write
    call    CloseHandle
    mov     QWORD PTR [rbp + HWRITE_OFF], 0
@@skip_close_write:

    ; ---- Wait for process (30 second timeout) ----
    mov     rcx, [rbp + PI_OFF]                 ; hProcess
    mov     edx, 30000                          ; dwMilliseconds = 30s
    call    WaitForSingleObject
    cmp     eax, WAIT_OBJECT_0
    jne     @@timeout

    ; Get exit code
    mov     rcx, [rbp + PI_OFF]                 ; hProcess
    lea     rdx, [rbp + 32]                     ; reuse hRead slot for exit code temp
    push    rdx                                 ; save pointer
    call    GetExitCodeProcess
    pop     rdx
    mov     eax, DWORD PTR [rdx]                ; exit code
    mov     esi, eax                            ; save in esi

    jmp     @@read_stdout

@@timeout:
    ; Kill the process
    mov     rcx, [rbp + PI_OFF]
    mov     edx, 1
    call    TerminateProcess
    mov     esi, -1                             ; exit code = -1

@@read_stdout:
    ; ---- Read stdout pipe into buffer ----
    test    r15, r15
    jz      @@cleanup
    mov     rcx, [rbp + HREAD_OFF]
    test    rcx, rcx
    jz      @@cleanup

    ; Simple read loop — read bytes, store as wchar (ASCII assumption)
    xor     edi, edi                            ; bytes written to output
@@read_loop:
    ; Check buffer space
    mov     rax, rbx
    sub     rax, 1                              ; leave room for null
    cmp     rdi, rax
    jge     @@done_read

    ; ReadFile(hRead, tempBuf, 1, &bytesRead, NULL)
    ; We read one byte at a time for simplicity (not performance-critical)
    sub     rsp, 8                              ; align
    mov     rcx, [rbp + HREAD_OFF]
    lea     rdx, [rbp + CMD_OFF + 4090]         ; temp byte slot
    mov     r8d, 1
    lea     r9, [rbp + CMD_OFF + 4092]          ; bytesRead slot
    mov     QWORD PTR [rsp + 32], 0
    call    ReadFile
    add     rsp, 8

    test    eax, eax
    jz      @@done_read
    mov     eax, DWORD PTR [rbp + CMD_OFF + 4092]
    test    eax, eax
    jz      @@done_read

    ; Store byte as wchar in output buffer
    movzx   eax, BYTE PTR [rbp + CMD_OFF + 4090]
    mov     WORD PTR [r15 + rdi*2], ax
    inc     rdi
    jmp     @@read_loop

@@done_read:
    ; Null-terminate output
    mov     WORD PTR [r15 + rdi*2], 0

@@cleanup:
    ; Close pipe read handle
    mov     rcx, [rbp + HREAD_OFF]
    test    rcx, rcx
    jz      @@skip_close_read
    call    CloseHandle
@@skip_close_read:

    ; Close process and thread handles
    mov     rcx, [rbp + PI_OFF]                 ; hProcess
    test    rcx, rcx
    jz      @@skip_close_proc
    call    CloseHandle
@@skip_close_proc:
    mov     rcx, [rbp + PI_OFF + 8]             ; hThread
    test    rcx, rcx
    jz      @@skip_close_thread
    call    CloseHandle
@@skip_close_thread:

    ; Return exit code
    movsxd  rax, esi
    jmp     @@git_epilogue

@@fail_launch:
    ; Close any open pipe handles
    mov     rcx, [rbp + HREAD_OFF]
    test    rcx, rcx
    jz      @@fail_no_read
    call    CloseHandle
@@fail_no_read:
    mov     rcx, [rbp + HWRITE_OFF]
    test    rcx, rcx
    jz      @@fail_no_write
    call    CloseHandle
@@fail_no_write:
    mov     rax, -1                             ; return -1

@@git_epilogue:
    add     rsp, GIT_FRAME
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret

SourceEdit_GitCommand ENDP

END
