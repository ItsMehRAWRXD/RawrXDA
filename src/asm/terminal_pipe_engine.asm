; =============================================================================
; terminal_pipe_engine.asm — Sovereign Terminal Execution with Anonymous Pipes
; =============================================================================
;
; Native Win32 anonymous pipes feeding stdout/stderr directly into the
; circular chat buffer. No ConPTY dependency, no external libraries.
;
; Architecture:
;   TermPipe_Spawn   — CreatePipe + CreateProcess + capture thread
;   TermPipe_Read    — Drain read-pipe into TERM_PIPE_CTX.outputBuf ring
;   TermPipe_Kill    — TerminateProcess + handle cleanup
;   TermPipe_GetLine — Pull one line from the ring buffer (caller render)
;
; Pipe topology (per session):
;
;   [Parent]   hStdoutRd ←————————————— hStdoutWr ← child stdout
;   [Parent]   hStdinWr  ————————————→  hStdinRd  → child stdin
;
; The ring buffer (TERM_RING) uses a 64 KB power-of-2 wrapping scheme.
; Reader cursor (g_termRdCursor) and writer cursor (g_termWrCursor) are
; independent 32-bit offsets that wrap at TERM_RING_MASK.
;
; Concurrency:
;   - Main thread calls TermPipe_Spawn / TermPipe_GetLine
;   - Capture thread (TermPipe_CaptureThread) drains hStdoutRd continuously
;   - No locks needed — single-producer (capture) / single-consumer (UI)
;     ring is safe on x64 with sequentially-consistent load/store
;
; Build: ml64.exe /c /Zi terminal_pipe_engine.asm
; Link : link /subsystem:windows terminal_pipe_engine.obj ...
; =============================================================================

INCLUDE masm64_compat.inc
OPTION CASEMAP:NONE

EXTERN GetExitCodeProcess:PROC
EXTERN TerminateProcess:PROC

; ---------------------------------------------------------------------------
;  Constants
; ---------------------------------------------------------------------------
TERM_RING_SIZE      EQU 65536           ; 64 KB ring buffer per session
TERM_RING_MASK      EQU (TERM_RING_SIZE - 1)
TERM_MAX_SESSIONS   EQU 8               ; Concurrent terminal sessions
TERM_TIMEOUT_MS     EQU 30000           ; Hard process kill timeout
TERM_READ_CHUNK     EQU 4096            ; Bytes per ReadFile call
TERM_HANDLE_INH     EQU 01h             ; HANDLE_FLAG_INHERIT

; TERM_PIPE_CTX.flags bits
TERM_FLAG_ALIVE     EQU 01h             ; Process running
TERM_FLAG_OVERFLOW  EQU 02h             ; Ring wrapped (data lost)
TERM_FLAG_COMPLETE  EQU 04h             ; Process exited cleanly
TERM_FLAG_STDIN_EOF EQU 08h             ; Stdin pipe closed

; Security attribute — inheritable handles
SECURITY_ATTRIBUTES STRUCT
    nLength                 DD ?
    lpSecurityDescriptor    DQ ?
    bInheritHandle          DD ?
    _pad                    DD ?
SECURITY_ATTRIBUTES ENDS

; ---------------------------------------------------------------------------
;  TERM_PIPE_CTX — per-session state (512 bytes aligned)
; ---------------------------------------------------------------------------
TERM_PIPE_CTX STRUCT
    hProcess        DQ ?        ; Child process handle
    hThread         DQ ?        ; Child thread handle
    hStdoutRd       DQ ?        ; Parent read end of stdout pipe
    hStdoutWr       DQ ?        ; Child write end (inheritable)
    hStdinRd        DQ ?        ; Child read end (inheritable)
    hStdinWr        DQ ?        ; Parent write end of stdin pipe
    hCaptureThread  DQ ?        ; Background drain thread
    exitCode        DD ?        ; GetExitCodeProcess result
    flags           DD ?        ; TERM_FLAG_* bitmask
    wrCursor        DD ?        ; Ring buffer write pointer (bytes written mod RING_SIZE)
    rdCursor        DD ?        ; Ring buffer read pointer  (bytes consumed mod RING_SIZE)
    sessionId       DD ?        ; Index into g_termSessions
    pid             DD ?        ; Child PID (for display)
    _pad            DQ ?
    outputBuf       DB TERM_RING_SIZE DUP(?)  ; 64 KB ring
TERM_PIPE_CTX ENDS

; ---------------------------------------------------------------------------
;  Module data
; ---------------------------------------------------------------------------
.data
ALIGN 16

; Session table
g_termSessions      TERM_PIPE_CTX TERM_MAX_SESSIONS DUP(<>)
g_sessionCount      DD  0
g_saInheritable     SECURITY_ATTRIBUTES <>   ; pre-filled in TermPipe_Init

szNulDevice         DB  "NUL", 0
szDbgSpawn          DB  "[TERM] Spawned pid=%u session=%u", 0Ah, 0
szDbgCapture        DB  "[TERM] Capture thread start session=%u", 0Ah, 0
szDbgExit           DB  "[TERM] Process exited code=%u session=%u", 0Ah, 0
szDbgKill           DB  "[TERM] Kill session=%u", 0Ah, 0
szDbgOverflow       DB  "[TERM] Ring overflow session=%u", 0Ah, 0

; Scratch for sprintf_s (not ring-safe, debug only)
g_dbgBuf            DB  256 DUP(0)

; ---------------------------------------------------------------------------
;  Exports
; ---------------------------------------------------------------------------
.code
PUBLIC TermPipe_Init
PUBLIC TermPipe_Spawn
PUBLIC TermPipe_Write
PUBLIC TermPipe_Read
PUBLIC TermPipe_GetLine
PUBLIC TermPipe_Kill
PUBLIC TermPipe_GetSession

; ---------------------------------------------------------------------------
;  TermPipe_Init
;  One-time module initialization.
;  Call once at IDE startup before any TermPipe_Spawn.
; ---------------------------------------------------------------------------
TermPipe_Init PROC FRAME
    .allocstack 28h
    .endprolog
    sub     rsp, 28h

    ; Pre-fill the SECURITY_ATTRIBUTES for inheritable handles
    mov     dword ptr [g_saInheritable.nLength], SIZEOF SECURITY_ATTRIBUTES
    mov     qword ptr [g_saInheritable.lpSecurityDescriptor], 0
    mov     dword ptr [g_saInheritable.bInheritHandle], 1

    ; Zero all session slots
    mov     rcx, OFFSET g_termSessions
    mov     edx, SIZEOF(TERM_PIPE_CTX) * TERM_MAX_SESSIONS
    xor     r8d, r8d
    call    memset

    mov     g_sessionCount, 0

    add     rsp, 28h
    ret
TermPipe_Init ENDP

; ---------------------------------------------------------------------------
;  TermPipe_Spawn
;
;  Creates a new terminal session: anonymous pipes + child process.
;
;  IN:  RCX = LPSTR cmdLine   (e.g. "powershell.exe -NoProfile -Command ...")
;  OUT: EAX = sessionId (0..TERM_MAX_SESSIONS-1), or -1 on failure
; ---------------------------------------------------------------------------
TermPipe_Spawn PROC FRAME
    .pushreg    rbx
    .pushreg    rsi
    .pushreg    rdi
    .pushreg    r12
    .allocstack 0E8h
    .endprolog

    sub     rsp, 0E8h           ; shadow(32) + STARTUPINFOA(104) + PROCESS_INFORMATION(32)
    push    rbx
    push    rsi
    push    rdi
    push    r12

    mov     rsi, rcx            ; rsi = cmdLine

    ; ---- Allocate session slot ----
    mov     eax, g_sessionCount
    cmp     eax, TERM_MAX_SESSIONS
    jae     @spawn_fail

    mov     r12d, eax           ; r12d = sessionId
    inc     g_sessionCount

    ; Pointer to ctx = g_termSessions + r12 * SIZEOF TERM_PIPE_CTX
    imul    rax, r12, SIZEOF TERM_PIPE_CTX
    mov     rbx, OFFSET g_termSessions
    add     rbx, rax                   ; rbx = TERM_PIPE_CTX*

    ; ---- Zero the ctx ----
    mov     rcx, rbx
    mov     edx, SIZEOF TERM_PIPE_CTX
    xor     r8d, r8d
    call    memset

    mov     [rbx + TERM_PIPE_CTX.sessionId], r12d

    ; ---- Create stdout pipe ----
    ; hStdoutRd (non-inheritable for parent), hStdoutWr (inheritable for child)
    lea     rcx, [rbx + TERM_PIPE_CTX.hStdoutRd]
    lea     rdx, [rbx + TERM_PIPE_CTX.hStdoutWr]
    lea     r8,  g_saInheritable
    xor     r9d, r9d
    call    CreatePipe
    test    eax, eax
    jz      @spawn_fail

    ; Make hStdoutRd non-inheritable (parent side)
    mov     rcx, [rbx + TERM_PIPE_CTX.hStdoutRd]
    mov     edx, TERM_HANDLE_INH   ; HANDLE_FLAG_INHERIT
    xor     r8d, r8d               ; clear the flag
    call    SetHandleInformation

    ; ---- Create stdin pipe ----
    ; hStdinRd (inheritable for child), hStdinWr (non-inheritable for parent)
    lea     rcx, [rbx + TERM_PIPE_CTX.hStdinRd]
    lea     rdx, [rbx + TERM_PIPE_CTX.hStdinWr]
    lea     r8,  g_saInheritable
    xor     r9d, r9d
    call    CreatePipe
    test    eax, eax
    jz      @spawn_fail

    ; Make hStdinWr non-inheritable (parent side)
    mov     rcx, [rbx + TERM_PIPE_CTX.hStdinWr]
    mov     edx, TERM_HANDLE_INH
    xor     r8d, r8d
    call    SetHandleInformation

    ; ---- Build STARTUPINFOA on stack ----
    ; stack layout: rsp+20h = STARTUPINFOA (104 bytes)
    ;               rsp+88h = PROCESS_INFORMATION (32 bytes)
    lea     rdi, [rsp + 20h]        ; STARTUPINFOA*
    mov     ecx, 104 / 4
    xor     eax, eax
    rep stosd

    mov     dword ptr [rsp + 20h],          104     ; cb
    mov     dword ptr [rsp + 20h + 44h],    STARTF_USESTDHANDLES ; dwFlags
    ; hStdInput = hStdinRd, hStdOutput = hStdoutWr, hStdError = hStdoutWr
    mov     rax, [rbx + TERM_PIPE_CTX.hStdinRd]
    mov     qword ptr [rsp + 20h + 58h], rax       ; hStdInput
    mov     rax, [rbx + TERM_PIPE_CTX.hStdoutWr]
    mov     qword ptr [rsp + 20h + 60h], rax       ; hStdOutput
    mov     qword ptr [rsp + 20h + 68h], rax       ; hStdError

    ; ---- CreateProcessA ----
    lea     r8, g_saInheritable         ; lpProcessAttributes
    mov     r9d, 1                      ; bInheritHandles = TRUE
    push    0                           ; lpEnvironment
    push    0                           ; lpCurrentDirectory
    lea     rax, [rsp + 20h + 18h]     ; STARTUPINFOA*
    push    rax
    lea     rax, [rsp + 88h + 18h]     ; PROCESS_INFORMATION*
    push    rax
    sub     rsp, 20h                    ; shadow space
    mov     rcx, 0                      ; lpApplicationName (NULL)
    mov     rdx, rsi                    ; lpCommandLine
    mov     r8,  0                      ; lpProcessAttributes (NULL)
    mov     r9d, 1                      ; bInheritHandles
    mov     dword ptr [rsp+20h], CREATE_NO_WINDOW ; dwCreationFlags
    mov     qword ptr [rsp+28h], 0     ; lpEnvironment
    mov     qword ptr [rsp+30h], 0     ; lpCurrentDirectory
    lea     rax, [rsp + 20h + 38h]
    mov     qword ptr [rsp+38h], rax   ; lpStartupInfo
    lea     rax, [rsp + 20h + 38h + 68h]
    mov     qword ptr [rsp+40h], rax   ; lpProcessInformation
    call    CreateProcessA
    add     rsp, 48h

    test    eax, eax
    jz      @spawn_close_fail

    ; Store process/thread handles
    lea     rdi, [rsp + 88h]
    mov     rax, [rdi]                  ; hProcess
    mov     [rbx + TERM_PIPE_CTX.hProcess], rax
    mov     rax, [rdi + 8]             ; hThread
    mov     [rbx + TERM_PIPE_CTX.hThread], rax
    mov     eax, [rdi + 16]            ; dwProcessId
    mov     [rbx + TERM_PIPE_CTX.pid], eax

    ; Close child-side pipe ends in parent (we don't need them)
    mov     rcx, [rbx + TERM_PIPE_CTX.hStdoutWr]
    call    CloseHandle
    mov     qword ptr [rbx + TERM_PIPE_CTX.hStdoutWr], 0

    mov     rcx, [rbx + TERM_PIPE_CTX.hStdinRd]
    call    CloseHandle
    mov     qword ptr [rbx + TERM_PIPE_CTX.hStdinRd], 0

    ; Mark alive
    or      dword ptr [rbx + TERM_PIPE_CTX.flags], TERM_FLAG_ALIVE

    ; ---- Spawn capture thread ----
    ; Thread arg = (QWORD)sessionId in rcx on entry
    xor     rcx, rcx
    lea     rdx, TermPipe_CaptureThread
    xor     r8, r8
    movsxd  r9, r12d                   ; arg = sessionId
    push    0
    push    0
    call    CreateThread
    mov     [rbx + TERM_PIPE_CTX.hCaptureThread], rax

    mov     eax, r12d                  ; return sessionId
    jmp     @spawn_done

@spawn_close_fail:
    ; Close all handles we created
    mov     rcx, [rbx + TERM_PIPE_CTX.hStdoutRd]
    test    rcx, rcx
    jz      @sc_skip1
    call    CloseHandle
@sc_skip1:
    mov     rcx, [rbx + TERM_PIPE_CTX.hStdoutWr]
    test    rcx, rcx
    jz      @sc_skip2
    call    CloseHandle
@sc_skip2:
    mov     rcx, [rbx + TERM_PIPE_CTX.hStdinRd]
    test    rcx, rcx
    jz      @sc_skip3
    call    CloseHandle
@sc_skip3:
    mov     rcx, [rbx + TERM_PIPE_CTX.hStdinWr]
    test    rcx, rcx
    jz      @sc_skip4
    call    CloseHandle
@sc_skip4:

@spawn_fail:
    mov     eax, -1

@spawn_done:
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    add     rsp, 0E8h
    ret
TermPipe_Spawn ENDP

; ---------------------------------------------------------------------------
;  TermPipe_CaptureThread
;
;  Background drain thread. Runs until the child process exits.
;  Writes raw bytes into the session ring buffer.
;
;  IN:  RCX = sessionId (QWORD)
; ---------------------------------------------------------------------------
TermPipe_CaptureThread PROC FRAME
    .pushreg    rbx
    .pushreg    r12
    .allocstack 30h
    .endprolog

    sub     rsp, 30h
    push    rbx
    push    r12

    mov     r12d, ecx                   ; r12d = sessionId
    imul    rax, r12, SIZEOF TERM_PIPE_CTX
    mov     rbx, OFFSET g_termSessions
    add     rbx, rax                   ; rbx = ctx

    ; Scratch read buffer on stack (TERM_READ_CHUNK bytes)
    ; We allocate it on the ring directly: write ahead, then advance cursor
@cap_loop:
    ; Test if still alive
    test    dword ptr [rbx + TERM_PIPE_CTX.flags], TERM_FLAG_ALIVE
    jz      @cap_done

    ; Compute write position in ring and available contiguous space
    mov     eax, [rbx + TERM_PIPE_CTX.wrCursor]
    and     eax, TERM_RING_MASK         ; eax = ring offset
    
    ; dst = &outputBuf[wrCursor & MASK]
    lea     rcx, [rbx + TERM_PIPE_CTX.outputBuf]
    add     rcx, rax                    ; rcx = write ptr

    ; Max contiguous bytes to end of ring
    mov     edx, TERM_RING_SIZE
    sub     edx, eax                    ; edx = bytes to wrap
    cmp     edx, TERM_READ_CHUNK
    jbe     @cap_use_edx
    mov     edx, TERM_READ_CHUNK
@cap_use_edx:

    ; ReadFile(hStdoutRd, dst, nBytes, &bytesRead, NULL)
    mov     rcx, [rbx + TERM_PIPE_CTX.hStdoutRd]
    ; rdx = write ptr (already set via rsp scratch)
    lea     r8,  [rsp + 20h]            ; lpBuffer = stack scratch
    mov     r9d, TERM_READ_CHUNK
    push    0                           ; lpOverlapped
    lea     rax, [rsp + 28h]            ; &bytesRead
    push    rax
    sub     rsp, 20h
    call    ReadFile
    add     rsp, 30h

    test    eax, eax
    jz      @cap_pipe_broken

    ; bytesRead at [rsp+28h]
    mov     ecx, dword ptr [rsp + 28h]
    test    ecx, ecx
    jz      @cap_loop                   ; EOF but no error yet

    ; Copy from stack scratch into ring
    ; Overflow check: if (wrCursor - rdCursor) + bytesRead > RING_SIZE → overflow
    mov     eax, [rbx + TERM_PIPE_CTX.wrCursor]
    mov     edx, [rbx + TERM_PIPE_CTX.rdCursor]
    sub     eax, edx                    ; eax = used bytes
    add     eax, ecx
    cmp     eax, TERM_RING_SIZE
    jb      @cap_no_overflow

    or      dword ptr [rbx + TERM_PIPE_CTX.flags], TERM_FLAG_OVERFLOW
    ; Advance rdCursor to make space (consume oldest data)
    mov     eax, [rbx + TERM_PIPE_CTX.wrCursor]
    sub     eax, TERM_RING_SIZE
    add     eax, ecx
    mov     [rbx + TERM_PIPE_CTX.rdCursor], eax

@cap_no_overflow:
    ; Write into ring (handle wrap-around with two partial copies)
    mov     r10d, [rbx + TERM_PIPE_CTX.wrCursor]
    and     r10d, TERM_RING_MASK        ; ring write offset

    lea     rdi, [rbx + TERM_PIPE_CTX.outputBuf]
    add     rdi, r10                    ; ring write ptr

    lea     rsi, [rsp + 20h]            ; src = stack scratch

    ; First copy: min(ecx, RING_SIZE - r10d) bytes
    mov     edx, TERM_RING_SIZE
    sub     edx, r10d                   ; bytes to ring end
    cmp     ecx, edx
    jbe     @cap_single_copy

    ; Two-part copy: copy `edx` bytes first, then wrap
    mov     r11d, ecx
    mov     ecx, edx
    rep movsb                           ; first segment

    ; Wrap: rdi = start of ring
    lea     rdi, [rbx + TERM_PIPE_CTX.outputBuf]
    mov     ecx, r11d
    sub     ecx, edx                    ; remaining bytes  
    rep movsb
    mov     ecx, r11d
    jmp     @cap_advance_cursor

@cap_single_copy:
    rep movsb

@cap_advance_cursor:
    ; wrCursor += bytesRead (unwrapped, 32-bit)
    mov     edx, dword ptr [rsp + 28h]
    add     [rbx + TERM_PIPE_CTX.wrCursor], edx

    jmp     @cap_loop

@cap_pipe_broken:
    ; Process has exited. Get exit code.
    mov     rcx, [rbx + TERM_PIPE_CTX.hProcess]
    lea     rdx, [rbx + TERM_PIPE_CTX.exitCode]
    call    GetExitCodeProcess

    and     dword ptr [rbx + TERM_PIPE_CTX.flags], NOT TERM_FLAG_ALIVE
    or      dword ptr [rbx + TERM_PIPE_CTX.flags], TERM_FLAG_COMPLETE

@cap_done:
    pop     r12
    pop     rbx
    add     rsp, 30h
    xor     eax, eax
    ret
TermPipe_CaptureThread ENDP

; ---------------------------------------------------------------------------
;  TermPipe_GetLine
;
;  Pulls one LF-terminated line from the ring buffer.
;  Blocks until a newline is available or the session dies.
;
;  IN:  ECX = sessionId
;       RDX = BYTE* outBuf            (caller allocated, UTF-8 safe)
;       R8D = maxLen
;  OUT: EAX = bytes copied (0 = no data / session dead)
; ---------------------------------------------------------------------------
TermPipe_GetLine PROC FRAME
    .pushreg    rbx
    .pushreg    r12
    .pushreg    r13
    .pushreg    r14
    .allocstack 28h
    .endprolog

    sub     rsp, 28h
    push    rbx
    push    r12
    push    r13
    push    r14

    movsxd  r12, ecx                   ; r12 = sessionId
    mov     r13, rdx                   ; r13 = outBuf
    mov     r14d, r8d                  ; r14d = maxLen

    imul    rax, r12, SIZEOF TERM_PIPE_CTX
    mov     rbx, OFFSET g_termSessions
    add     rbx, rax

    xor     ecx, ecx                   ; ecx = bytes copied

@getline_wait:
    ; Check if data available: wrCursor != rdCursor
    mov     eax, [rbx + TERM_PIPE_CTX.wrCursor]
    cmp     eax, [rbx + TERM_PIPE_CTX.rdCursor]
    jne     @getline_consume

    ; No data yet — spin briefly (yield if not alive)
    test    dword ptr [rbx + TERM_PIPE_CTX.flags], TERM_FLAG_ALIVE
    jz      @getline_done              ; session dead, no more data

    push    rcx
    mov     ecx, 1
    call    Sleep                      ; yield 1ms
    pop     rcx
    jmp     @getline_wait

@getline_consume:
    ; Read one byte from ring
    mov     edx, [rbx + TERM_PIPE_CTX.rdCursor]
    and     edx, TERM_RING_MASK
    lea     rax, [rbx + TERM_PIPE_CTX.outputBuf]
    movzx   eax, byte ptr [rax + rdx]
    inc     dword ptr [rbx + TERM_PIPE_CTX.rdCursor]

    ; Store to output
    cmp     ecx, r14d
    jae     @getline_done              ; output buffer full

    mov     byte ptr [r13 + rcx], al
    inc     ecx

    ; Break on newline
    cmp     al, 0Ah
    je      @getline_done

    ; Loop if more data
    mov     edx, [rbx + TERM_PIPE_CTX.wrCursor]
    cmp     edx, [rbx + TERM_PIPE_CTX.rdCursor]
    ja      @getline_consume

    ; No more data in this call — return what we have
@getline_done:
    ; NUL-terminate
    cmp     ecx, r14d
    jae     @getline_nonul
    mov     byte ptr [r13 + rcx], 0
@getline_nonul:
    mov     eax, ecx

    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    add     rsp, 28h
    ret
TermPipe_GetLine ENDP

; ---------------------------------------------------------------------------
;  TermPipe_Write
;
;  Sends stdin bytes to child process.
;
;  IN:  ECX = sessionId
;       RDX = BYTE* buf
;       R8D = len
;  OUT: EAX = bytes written (0 = error)
; ---------------------------------------------------------------------------
TermPipe_Write PROC FRAME
    .pushreg    rbx
    .allocstack 40h
    .endprolog

    sub     rsp, 40h
    push    rbx

    movsxd  rax, ecx
    imul    rax, rax, SIZEOF TERM_PIPE_CTX
    mov     rbx, OFFSET g_termSessions
    add     rbx, rax

    ; Verify alive
    test    dword ptr [rbx + TERM_PIPE_CTX.flags], TERM_FLAG_ALIVE
    jz      @write_fail

    ; WriteFile(hStdinWr, buf, len, &written, NULL)
    mov     rcx, [rbx + TERM_PIPE_CTX.hStdinWr]
    ; rdx, r8 already set
    lea     r9,  [rsp + 28h]           ; &bytesWritten
    push    0
    sub     rsp, 20h
    call    WriteFile
    add     rsp, 28h

    test    eax, eax
    jz      @write_fail

    mov     eax, dword ptr [rsp + 28h] ; bytesWritten
    jmp     @write_done

@write_fail:
    xor     eax, eax

@write_done:
    pop     rbx
    add     rsp, 40h
    ret
TermPipe_Write ENDP

; ---------------------------------------------------------------------------
;  TermPipe_Kill
;
;  Terminates a session immediately. Closes all handles.
;
;  IN:  ECX = sessionId
;  OUT: EAX = 1 (success), 0 (invalid session)
; ---------------------------------------------------------------------------
TermPipe_Kill PROC FRAME
    .pushreg    rbx
    .allocstack 28h
    .endprolog

    sub     rsp, 28h
    push    rbx

    movsxd  rax, ecx
    imul    rax, rax, SIZEOF TERM_PIPE_CTX
    mov     rbx, OFFSET g_termSessions
    add     rbx, rax

    ; TerminateProcess if still alive
    test    dword ptr [rbx + TERM_PIPE_CTX.flags], TERM_FLAG_ALIVE
    jz      @kill_cleanup

    mov     rcx, [rbx + TERM_PIPE_CTX.hProcess]
    mov     edx, 1                     ; exit code
    call    TerminateProcess

    and     dword ptr [rbx + TERM_PIPE_CTX.flags], NOT TERM_FLAG_ALIVE

@kill_cleanup:
    ; Close all handles
    ; Inline close sequence
    mov     rcx, [rbx + TERM_PIPE_CTX.hStdoutRd]
    test    rcx, rcx
    jz      @c1
    call    CloseHandle
    mov     qword ptr [rbx + TERM_PIPE_CTX.hStdoutRd], 0
@c1:
    mov     rcx, [rbx + TERM_PIPE_CTX.hStdinWr]
    test    rcx, rcx
    jz      @c2
    call    CloseHandle
    mov     qword ptr [rbx + TERM_PIPE_CTX.hStdinWr], 0
@c2:
    mov     rcx, [rbx + TERM_PIPE_CTX.hProcess]
    test    rcx, rcx
    jz      @c3
    call    CloseHandle
    mov     qword ptr [rbx + TERM_PIPE_CTX.hProcess], 0
@c3:
    mov     rcx, [rbx + TERM_PIPE_CTX.hThread]
    test    rcx, rcx
    jz      @c4
    call    CloseHandle
    mov     qword ptr [rbx + TERM_PIPE_CTX.hThread], 0
@c4:
    ; Wait for capture thread to die
    mov     rcx, [rbx + TERM_PIPE_CTX.hCaptureThread]
    test    rcx, rcx
    jz      @kill_done
    mov     edx, 2000
    call    WaitForSingleObject
    mov     rcx, [rbx + TERM_PIPE_CTX.hCaptureThread]
    call    CloseHandle
    mov     qword ptr [rbx + TERM_PIPE_CTX.hCaptureThread], 0

@kill_done:
    mov     eax, 1
    pop     rbx
    add     rsp, 28h
    ret
TermPipe_Kill ENDP

; ---------------------------------------------------------------------------
;  TermPipe_GetSession
;
;  Returns pointer to TERM_PIPE_CTX for direct inspection.
;
;  IN:  ECX = sessionId
;  OUT: RAX = TERM_PIPE_CTX* (NULL if out of range)
; ---------------------------------------------------------------------------
TermPipe_GetSession PROC FRAME
    .allocstack 28h
    .endprolog

    sub     rsp, 28h

    cmp     ecx, TERM_MAX_SESSIONS
    jae     @gs_fail

    movsxd  rax, ecx
    imul    rax, rax, SIZEOF TERM_PIPE_CTX
    mov     rdx, OFFSET g_termSessions
    lea     rax, [rdx + rax]
    jmp     @gs_done

@gs_fail:
    xor     rax, rax

@gs_done:
    add     rsp, 28h
    ret
TermPipe_GetSession ENDP

; ---------------------------------------------------------------------------
;  TermPipe_Read  (non-blocking batch)
;
;  Drains up to maxLen bytes from the ring into outBuf. Does NOT wait.
;
;  IN:  ECX = sessionId
;       RDX = BYTE* outBuf
;       R8D = maxLen
;  OUT: EAX = bytes copied
; ---------------------------------------------------------------------------
TermPipe_Read PROC FRAME
    .pushreg    rbx
    .allocstack 28h
    .endprolog

    sub     rsp, 28h
    push    rbx

    movsxd  rax, ecx
    imul    rax, rax, SIZEOF TERM_PIPE_CTX
    mov     rbx, OFFSET g_termSessions
    add     rbx, rax

    ; Available bytes
    mov     eax, [rbx + TERM_PIPE_CTX.wrCursor]
    mov     ecx, [rbx + TERM_PIPE_CTX.rdCursor]
    sub     eax, ecx
    jz      @read_none

    cmp     eax, r8d                   ; clamp to maxLen
    jbe     @read_use_avail
    mov     eax, r8d

@read_use_avail:
    ; Ring read: two-segment copy
    mov     r9d, ecx
    and     r9d, TERM_RING_MASK        ; ring read offset
    lea     rsi, [rbx + TERM_PIPE_CTX.outputBuf]
    add     rsi, r9                    ; rsi = ring read ptr
    mov     rdi, rdx                   ; rdi = outBuf
    mov     ecx, eax                   ; ecx = total bytes

    ; Bytes to ring end
    mov     r10d, TERM_RING_SIZE
    sub     r10d, r9d

    cmp     ecx, r10d
    jbe     @read_single

    ; Two-part: copy r10d bytes, then wrap
    push    rcx
    mov     ecx, r10d
    rep movsb
    lea     rsi, [rbx + TERM_PIPE_CTX.outputBuf]   ; wrap
    pop     rcx
    sub     ecx, r10d
    rep movsb
    ; total bytes = original eax
    jmp     @read_advance

@read_single:
    rep movsb

@read_advance:
    ; Advance rdCursor
    add     [rbx + TERM_PIPE_CTX.rdCursor], eax
    jmp     @read_done

@read_none:
    xor     eax, eax

@read_done:
    pop     rbx
    add     rsp, 28h
    ret
TermPipe_Read ENDP

END
