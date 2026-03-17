; RawrXD_Sidebar_x64.asm
; Pure Win64 ABI — Zero Qt, Zero CRT, Zero masm64 SDK
; Sidebar subsystems: Logger, Debug Engine, Tree Virtualization, DWM Dark Mode
;
; Assemble: ml64 /c /FoRawrXD_Sidebar_x64.obj RawrXD_Sidebar_x64.asm
; Link:     link RawrXD_Sidebar_x64.obj kernel32.lib user32.lib dwmapi.lib
;
; Exports (extern "C" from C++):
;   RawrXD_Logger_Init      — Open log file + capture tick baseline
;   RawrXD_Logger_Write     — Format & emit to OutputDebugString + file
;   RawrXD_Debug_Attach     — DebugActiveProcess wrapper
;   RawrXD_Debug_Wait       — WaitForDebugEvent wrapper
;   RawrXD_Debug_Step       — Single-step via trap flag (hardware)
;   RawrXD_Tree_LazyLoad    — Set TVS_HASBUTTONS|LINES|LINESATROOT + EX_DOUBLEBUFFER
;   RawrXD_DarkMode_Force   — DwmSetWindowAttribute IMMERSIVE_DARK_MODE

extrn OutputDebugStringA:proc
extrn WriteFile:proc
extrn CreateFileA:proc
extrn CloseHandle:proc
extrn GetCurrentProcessId:proc
extrn GetCurrentThreadId:proc
extrn GetTickCount64:proc
extrn DebugActiveProcess:proc
extrn WaitForDebugEvent:proc
extrn GetThreadContext:proc
extrn SetThreadContext:proc
extrn ResumeThread:proc
extrn SendMessageA:proc
extrn SetWindowLongPtrA:proc
extrn GetWindowLongPtrA:proc
extrn DwmSetWindowAttribute:proc

public RawrXD_Logger_Init
public RawrXD_Logger_Write
public RawrXD_Debug_Attach
public RawrXD_Debug_Wait
public RawrXD_Debug_Step
public RawrXD_Tree_LazyLoad
public RawrXD_DarkMode_Force

.data
    align 8
    hLogFile        dq 0
    qwTickBase      dq 0
    szLogFile       db "rawrxd_sidebar.log",0
    szPrefix        db "[RAWRXD]",0
    szNewLine       db 13,10

    CONTEXT_CONTROL equ 10001h
    DWMWA_IMMERSIVE equ 20

.code

; =========================================================
; QWORD Hex Conversion (Manual, no CRT)
; RCX = value, RDX = output buffer
; Returns RAX = 16 (chars written)
; =========================================================
QwordToHex proc
    push rbx
    push rdi
    mov rbx, rcx
    mov rdi, rdx
    mov r8, 16
@@: rol rbx, 4
    movzx eax, bl
    and al, 0Fh
    cmp al, 9
    jbe @F
    add al, 7
@@: add al, '0'
    mov [rdi], al
    inc rdi
    dec r8
    jnz @B
    mov rax, 16
    pop rdi
    pop rbx
    ret
QwordToHex endp

; =========================================================
; DWORD Decimal Conversion (Manual)
; RCX = value, RDX = output buffer
; Returns RAX = chars written
; =========================================================
DwordToDec proc
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    xor r8, r8
    mov rax, rbx
@@: xor rdx, rdx
    mov rcx, 10
    div rcx
    add dl, '0'
    push rdx
    inc r8
    test rax, rax
    jnz @B
    mov rax, r8
@@: pop rdx
    mov [rsi], dl
    inc rsi
    dec r8
    jnz @B
    pop rsi
    pop rbx
    ret
DwordToDec endp

; =========================================================
; RawrXD_Logger_Init
; RCX = filename (or NULL for default "rawrxd_sidebar.log")
; Opens log file with OPEN_ALWAYS + GENERIC_WRITE
; =========================================================
RawrXD_Logger_Init proc
    sub rsp, 40h
    test rcx, rcx
    jnz @open
    lea rcx, szLogFile
@open:
    mov rdx, 40100000h          ; GENERIC_WRITE | FILE_APPEND_DATA
    mov r8, 1                   ; FILE_SHARE_READ
    xor r9, r9                  ; lpSecurityAttributes = NULL
    mov qword ptr [rsp+20h], 4  ; dwCreationDisposition = OPEN_ALWAYS
    mov qword ptr [rsp+28h], 80h; dwFlagsAndAttributes  = FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0  ; hTemplateFile = NULL
    call CreateFileA
    mov hLogFile, rax
    call GetTickCount64
    mov qwTickBase, rax
    add rsp, 40h
    ret
RawrXD_Logger_Init endp

; =========================================================
; RawrXD_Logger_Write
; RCX = Level string ("INFO","WARN","ERROR","CRIT")
; RDX = Source file string
; R8  = Line number
; R9  = Message string
;
; Format: [RAWRXD] <hex_tick_delta> <pid> <level> <message>\r\n
; Output: OutputDebugStringA + WriteFile to hLogFile
; =========================================================
RawrXD_Logger_Write proc
    push rbx
    push rsi
    push rdi
    sub rsp, 200h

    mov [rsp+28h], rcx          ; Level
    mov [rsp+30h], rdx          ; File
    mov [rsp+38h], r8           ; Line
    mov [rsp+40h], r9           ; Message

    lea rdi, [rsp+50h]          ; Build output buffer on stack

    ; ── Prefix "[RAWRXD] " ──
    lea rsi, szPrefix
@@: mov al, [rsi]
    test al, al
    jz @pfxdone
    mov [rdi], al
    inc rdi
    inc rsi
    jmp @B
@pfxdone:
    mov byte ptr [rdi], ' '
    inc rdi

    ; ── Tick delta (16-char hex) ──
    call GetTickCount64
    sub rax, qwTickBase
    mov rcx, rax
    mov rdx, rdi
    call QwordToHex
    add rdi, rax
    mov byte ptr [rdi], ' '
    inc rdi

    ; ── PID (decimal) ──
    call GetCurrentProcessId
    mov rcx, rax
    mov rdx, rdi
    call DwordToDec
    add rdi, rax
    mov byte ptr [rdi], ' '
    inc rdi

    ; ── Level string ──
    mov rsi, [rsp+28h]
@@: mov al, [rsi]
    test al, al
    jz @lvldone
    mov [rdi], al
    inc rdi
    inc rsi
    jmp @B
@lvldone:
    mov byte ptr [rdi], ' '
    inc rdi

    ; ── Message string ──
    mov rsi, [rsp+40h]
@@: mov al, [rsi]
    test al, al
    jz @msgdone
    mov [rdi], al
    inc rdi
    inc rsi
    jmp @B
@msgdone:
    mov ax, 0A0Dh               ; CRLF
    mov [rdi], ax
    add rdi, 2

    ; ── Calculate total length ──
    lea rax, [rsp+50h]
    sub rdi, rax
    mov rbx, rdi                ; rbx = byte count

    ; ── OutputDebugStringA (always) ──
    lea rcx, [rsp+50h]
    call OutputDebugStringA

    ; ── WriteFile to log (if open) ──
    mov rax, hLogFile
    cmp rax, -1
    je @skip
    mov rcx, hLogFile           ; hFile
    lea rdx, [rsp+50h]         ; lpBuffer
    mov r8, rbx                 ; nNumberOfBytesToWrite
    lea r9, [rsp+48h]          ; lpNumberOfBytesWritten
    mov qword ptr [rsp+20h], 0 ; lpOverlapped = NULL
    call WriteFile
@skip:
    add rsp, 200h
    pop rdi
    pop rsi
    pop rbx
    ret
RawrXD_Logger_Write endp

; =========================================================
; Debug Engine — Real Hardware Stepping
; =========================================================

; RawrXD_Debug_Attach
; RCX = dwProcessId
; Returns: BOOL (nonzero = success)
RawrXD_Debug_Attach proc
    sub rsp, 28h
    call DebugActiveProcess
    add rsp, 28h
    ret
RawrXD_Debug_Attach endp

; RawrXD_Debug_Wait
; RCX = lpDebugEvent (pointer to DEBUG_EVENT)
; RDX = dwMilliseconds
; Returns: BOOL
RawrXD_Debug_Wait proc
    sub rsp, 28h
    mov r8, rdx                 ; timeout → r8 (WaitForDebugEvent param 2)
    mov rdx, rcx                ; lpDebugEvent → rdx (param 1)
    call WaitForDebugEvent
    add rsp, 28h
    ret
RawrXD_Debug_Wait endp

; RawrXD_Debug_Step
; RCX = hThread
; RDX = pContext (CONTEXT struct, caller-allocated)
; Sets trap flag (TF=1) in EFLAGS for single-step
RawrXD_Debug_Step proc
    push rbx
    push r12
    sub rsp, 40h
    mov rbx, rcx               ; hThread
    mov r12, rdx                ; pContext

    ; Request CONTEXT_CONTROL
    mov dword ptr [r12+30h], CONTEXT_CONTROL
    mov rcx, rbx
    mov rdx, r12
    call GetThreadContext

    ; Set Trap Flag (bit 8 of EFlags at CONTEXT+44h)
    mov eax, [r12+44h]
    or eax, 100h                ; TF = 1
    mov [r12+44h], eax

    mov rcx, rbx
    mov rdx, r12
    call SetThreadContext

    ; Resume execution — will break after one instruction
    mov rcx, rbx
    call ResumeThread

    add rsp, 40h
    pop r12
    pop rbx
    ret
RawrXD_Debug_Step endp

; =========================================================
; Tree Virtualization — Lazy Load Style Setup
; RCX = hWndTree (TreeView control handle)
; =========================================================
RawrXD_Tree_LazyLoad proc
    push rbx
    sub rsp, 40h
    mov rbx, rcx

    ; Get current style, add TVS flags
    xor rdx, rdx
    mov r8, -20                 ; GWL_STYLE
    call GetWindowLongPtrA
    or rax, 27h                 ; TVS_HASBUTTONS(1)|TVS_HASLINES(2)|TVS_LINESATROOT(4)|TVS_SHOWSELALWAYS(20)
    mov rcx, rbx
    xor rdx, rdx
    mov r8, -20                 ; GWL_STYLE
    mov r9, rax                 ; new style
    call SetWindowLongPtrA

    ; Set extended style: TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS
    mov rcx, rbx
    mov rdx, 1100h + 44        ; TVM_SETEXTENDEDSTYLE = TVM_FIRST (0x1100) + 44
    xor r8, r8                  ; mask = 0 (set all)
    mov r9, 44h                 ; TVS_EX_DOUBLEBUFFER(4) | TVS_EX_FADEINOUTEXPANDOS(40)
    call SendMessageA

    add rsp, 40h
    pop rbx
    ret
RawrXD_Tree_LazyLoad endp

; =========================================================
; Dark Mode Force — DWM Immersive Dark Mode Attribute
; RCX = hWnd
; =========================================================
RawrXD_DarkMode_Force proc
    push rbx
    sub rsp, 40h
    mov rbx, rcx

    ; DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(DWORD))
    mov edx, DWMWA_IMMERSIVE    ; dwAttribute = 20
    mov dword ptr [rsp+20h], 1  ; pvAttribute value = TRUE
    lea r8, [rsp+20h]           ; pvAttribute pointer
    mov r9d, 4                  ; cbAttribute = sizeof(DWORD)
    call DwmSetWindowAttribute

    add rsp, 40h
    pop rbx
    ret
RawrXD_DarkMode_Force endp

end
