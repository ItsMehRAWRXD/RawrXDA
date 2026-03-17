; =============================================================================
; Win32IDE_Sidebar_Core.asm - Pure x64 ASM Sidebar Engine
; Replaces: Qt Debug/Logging, Lazy Tree, DWM Dark Mode, Debug API
; Exports: C-callable (undecorated, extern "C")
; Assemble: ml64 /c /Cp /Cx /Zi /Zd Win32IDE_Sidebar_Core.asm
; =============================================================================
OPTION DOTNAME
OPTION CASEMAP:NONE

; =============================================================================
; DATA SECTION
; =============================================================================
.data
ALIGN 16
; Handles & Globals
g_hTreeView             dq 0
g_hDebugProcess         dq 0
g_hDebugThread          dq 0
g_hLogFile              dq 0
g_DarkModeEnabled       dd 0
g_DebugActive           dd 0

; Strings (null-terminated)
szKernel32              db "kernel32.dll", 0
szUser32                db "user32.dll", 0
szDwmApi                db "dwmapi.dll", 0
szNtdll                 db "ntdll.dll", 0

szWaitForDebugEventEx   db "WaitForDebugEventEx", 0
szContinueDebugEvent    db "ContinueDebugEvent", 0
szDebugActiveProcess    db "DebugActiveProcess", 0
szDebugActiveProcessStop db "DebugActiveProcessStop", 0
szOutputDebugStringA    db "OutputDebugStringA", 0
szCreateFileA           db "CreateFileA", 0
szWriteFile             db "WriteFile", 0
szCloseHandle           db "CloseHandle", 0
szGetCurrentProcessId   db "GetCurrentProcessId", 0
szPostMessageA          db "PostMessageA", 0
szSendMessageA          db "SendMessageA", 0
szDwmSetWindowAttribute db "DwmSetWindowAttribute", 0

szLogPath               db "rawrxd_sidebar.log", 0
szDebugPrefix           db "[RAWRXD DEBUG] ", 0
szCRLF                  db 0Dh, 0Ah, 0

; TVM Constants (for TreeView)
TVM_INSERTITEMA         equ 1100h + 0
TVM_DELETEITEM          equ 1100h + 1
TVM_EXPAND              equ 1100h + 2
TVM_GETITEMRECT         equ 1100h + 4
TVM_GETCOUNT            equ 1100h + 5
TVM_GETNEXTITEM         equ 1100h + 0Ah
TV_FIRST                equ 1100h
TVM_SETEXTENDEDSTYLE    equ TV_FIRST + 44
TVS_EX_DOUBLEBUFFER     equ 00004h
TVS_EX_FADEINOUTEXPAND  equ 00040h
WM_USER                 equ 0400h

; DWM Constants
DWMWA_USE_IMMERSIVE_DARK_MODE equ 20

; DEBUG_EVENT Codes
CREATE_PROCESS_DEBUG_EVENT equ 3
CREATE_THREAD_DEBUG_EVENT  equ 2
EXCEPTION_DEBUG_EVENT      equ 1
EXIT_PROCESS_DEBUG_EVENT   equ 5
EXIT_THREAD_DEBUG_EVENT    equ 4

; Struct sizes
SIZEOF_DEBUG_EVENT      equ 24 + 64 ; Simplified size
SIZEOF_CONTEXT          equ 1232    ; x64 CONTEXT size
SIZEOF_TVITEM           equ 56      ; TVITEMEXA x64

; =============================================================================
; STRUCTURE DEFINITIONS (offsets for manual access)
; =============================================================================

; DEBUG_EVENT offsets (packed)
DE_dwDebugEventCode     equ 0
DE_dwProcessId          equ 4
DE_dwThreadId           equ 8
DE_u                    equ 16      ; Union starts here

; EXCEPTION_DEBUG_INFO offsets within union
EDI_ExceptionCode       equ 0
EDI_ExceptionFlags      equ 4
EDI_ExceptionAddress    equ 16      ; 64-bit ptr

; TVINSERTSTRUCTA offsets
TIS_hParent             equ 0
TIS_hInsertAfter        equ 8
TIS_item                equ 16
TIS_cchTextMax          equ 24

; =============================================================================
; UNINITIALIZED DATA
; =============================================================================
.data?
ALIGN 16
g_DebugEvent            db SIZEOF_DEBUG_EVENT dup(?)
g_Context               db SIZEOF_CONTEXT dup(?)
g_TvItem                db SIZEOF_TVITEM dup(?)
g_Written               dd ?
g_PidCache              dd ?

; =============================================================================
; CODE SECTION
; =============================================================================
.code
ALIGN 16

; =============================================================================
; INTERNAL: GetProcAddress64 (manual, no imports)
; RCX = Module base, RDX = Proc name ASCII
; Returns: RAX = function address or 0
; =============================================================================
GetProcAddr64 PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov r12, rdx            ; Save proc name
    mov rbx, rcx            ; Save module base
    
    ; PE Header traversal
    mov eax, [rbx+3Ch]      ; e_lfanew
    mov esi, [rbx+rax+88h]  ; Export table RVA (PE32+ offset)
    test esi, esi
    jz fail_gpa
    lea rsi, [rbx+rsi]      ; Export table VA
    
    mov ecx, [rsi+18h]      ; NumberOfNames
    mov r8d, [rsi+20h]      ; AddressOfNames RVA
    lea r8, [rbx+r8]        ; Names table
    
    xor rdi, rdi            ; Index counter

search_loop:
    cmp rdi, rcx
    jae fail_gpa
    
    mov eax, [r8+rdi*4]     ; Name RVA
    lea rax, [rbx+rax]      ; Name string
    
    ; String compare (rsi = table entry, r12 = target)
    push rcx
    push rsi
    push rdi
    mov rsi, rax
    mov rdi, r12
    
compare_byte:
    lodsb
    mov dl, [rdi]
    cmp al, dl
    jne not_match
    test al, al
    jz found_it
    inc rdi
    jmp compare_byte
    
not_match:
    pop rdi
    pop rsi
    pop rcx
    inc rdi
    jmp search_loop
    
found_it:
    pop rdi
    pop rsi
    pop rcx
    
    ; Get ordinal
    mov r8d, [rsi+24h]      ; AddressOfNameOrdinals
    lea r8, [rbx+r8]
    movzx eax, word ptr [r8+rdi*2]
    
    ; Get function address
    mov r8d, [rsi+1Ch]      ; AddressOfFunctions
    lea r8, [rbx+r8]
    mov eax, [r8+rax*4]
    lea rax, [rbx+rax]      ; Final address
    jmp exit_gpa
    
fail_gpa:
    xor rax, rax
    
exit_gpa:
    add rsp, 32
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
GetProcAddr64 ENDP

; =============================================================================
; EXPORT: Sidebar_InitLogging
; RCX = lpFilePath (null = default)
; Returns: RAX = 1 success, 0 fail
; =============================================================================
PUBLIC Sidebar_InitLogging
Sidebar_InitLogging PROC FRAME
    push rbx
    push rsi
    sub rsp, 40h
    .allocstack 40h
    .endprolog
    
    mov rsi, rcx
    test rcx, rcx
    jnz use_custom_path
    lea rsi, szLogPath
    
use_custom_path:
    ; Get CreateFileA from kernel32
    lea rcx, szKernel32
    call qword ptr [__imp_GetModuleHandleA]
    test rax, rax
    jz init_fail
    
    mov rcx, rax
    lea rdx, szCreateFileA
    call GetProcAddr64
    test rax, rax
    jz init_fail
    mov rbx, rax            ; Save CreateFileA
    
    ; CreateFileA(szPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
    sub rsp, 48             ; Shadow space + 3 stack args
    mov qword ptr [rsp+40], 0  ; hTemplateFile = NULL
    mov dword ptr [rsp+32], 80h ; FILE_ATTRIBUTE_NORMAL
    mov r9d, 2              ; CREATE_ALWAYS
    xor r8, r8              ; lpSecurityAttributes = NULL
    mov edx, 1              ; FILE_SHARE_READ
    shl edx, 30             ; GENERIC_WRITE = 0x40000000 ... nah, set properly:
    mov edx, 40000000h      ; GENERIC_WRITE
    mov rcx, rsi            ; lpFileName
    call rbx
    add rsp, 48
    mov g_hLogFile, rax
    cmp rax, -1
    je init_fail
    
    mov eax, 1
    jmp init_exit
    
init_fail:
    xor eax, eax
    
init_exit:
    add rsp, 40h
    pop rsi
    pop rbx
    ret
Sidebar_InitLogging ENDP

; =============================================================================
; EXPORT: Sidebar_LogWrite
; RCX = lpMessage (null-terminated ASCII)
; =============================================================================
PUBLIC Sidebar_LogWrite
Sidebar_LogWrite PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 48h
    .allocstack 48h
    .endprolog
    
    mov rsi, rcx            ; Save message pointer
    
    ; OutputDebugStringA first (always works even without log file)
    mov rcx, rsi
    call qword ptr [__imp_OutputDebugStringA]
    
    ; File logging
    mov rdi, g_hLogFile
    test rdi, rdi
    jz log_exit
    cmp rdi, -1
    je log_exit
    
    ; Get WriteFile address
    lea rcx, szKernel32
    call qword ptr [__imp_GetModuleHandleA]
    mov rcx, rax
    lea rdx, szWriteFile
    call GetProcAddr64
    mov rbx, rax            ; Save WriteFile ptr
    test rax, rax
    jz log_exit
    
    ; Write prefix "[RAWRXD DEBUG] "
    sub rsp, 40
    mov qword ptr [rsp+32], 0   ; lpOverlapped = NULL
    lea r9, g_Written           ; lpNumberOfBytesWritten
    mov r8d, 15                 ; nNumberOfBytesToWrite (prefix len)
    lea rdx, szDebugPrefix      ; lpBuffer
    mov rcx, rdi                ; hFile
    call rbx
    add rsp, 40
    
    ; Calculate message length
    mov rcx, rsi
    xor rax, rax
strlen_loop:
    cmp byte ptr [rcx+rax], 0
    je got_len
    inc rax
    jmp strlen_loop
got_len:
    mov r12, rax            ; Save length
    
    ; Write message body
    sub rsp, 40
    mov qword ptr [rsp+32], 0
    lea r9, g_Written
    mov r8, r12             ; message length
    mov rdx, rsi            ; message buffer
    mov rcx, rdi            ; hFile
    call rbx
    add rsp, 40
    
    ; Write CRLF
    sub rsp, 40
    mov qword ptr [rsp+32], 0
    lea r9, g_Written
    mov r8d, 2
    lea rdx, szCRLF
    mov rcx, rdi
    call rbx
    add rsp, 40
    
log_exit:
    add rsp, 48h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Sidebar_LogWrite ENDP

; =============================================================================
; EXPORT: Sidebar_SetTreeVirtualMode
; RCX = hTreeView (HWND)
; Returns: RAX = 1
; =============================================================================
PUBLIC Sidebar_SetTreeVirtualMode
Sidebar_SetTreeVirtualMode PROC FRAME
    push rbx
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov g_hTreeView, rcx
    
    ; SendMessage(hWnd, TVM_SETEXTENDEDSTYLE, mask, styles)
    mov r9d, TVS_EX_DOUBLEBUFFER or TVS_EX_FADEINOUTEXPAND
    mov r8d, TVS_EX_DOUBLEBUFFER or TVS_EX_FADEINOUTEXPAND
    mov edx, TVM_SETEXTENDEDSTYLE
    ; RCX already has hWnd
    call qword ptr [__imp_SendMessageA]
    
    mov eax, 1
    add rsp, 28h
    pop rbx
    ret
Sidebar_SetTreeVirtualMode ENDP

; =============================================================================
; EXPORT: Sidebar_SetDarkMode
; RCX = hWnd, RDX = bEnable (0 or 1)
; Returns: RAX = DWM result (HRESULT)
; =============================================================================
PUBLIC Sidebar_SetDarkMode
Sidebar_SetDarkMode PROC FRAME
    push rbx
    push rsi
    sub rsp, 38h
    .allocstack 38h
    .endprolog
    
    mov rbx, rcx            ; Save hWnd
    mov g_DarkModeEnabled, edx
    
    ; Load dwmapi.dll
    lea rcx, szDwmApi
    call qword ptr [__imp_LoadLibraryA]
    test rax, rax
    jz dark_fail
    
    mov rcx, rax
    lea rdx, szDwmSetWindowAttribute
    call GetProcAddr64
    test rax, rax
    jz dark_fail
    mov rsi, rax            ; Save DwmSetWindowAttribute ptr
    
    ; DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &bEnable, 4)
    mov r9d, 4                          ; cbAttribute
    lea r8, g_DarkModeEnabled           ; pvAttribute
    mov edx, DWMWA_USE_IMMERSIVE_DARK_MODE ; dwAttribute
    mov rcx, rbx                        ; hWnd
    call rsi
    
    ; Force redraw via InvalidateRect(hWnd, NULL, TRUE) + UpdateWindow(hWnd)
    mov r8d, 1              ; bErase = TRUE
    xor rdx, rdx            ; lpRect = NULL (entire window)
    mov rcx, rbx            ; hWnd
    call qword ptr [__imp_InvalidateRect]
    
    mov eax, 1
    jmp dark_exit

dark_fail:
    xor eax, eax
    
dark_exit:
    add rsp, 38h
    pop rsi
    pop rbx
    ret
Sidebar_SetDarkMode ENDP

; =============================================================================
; EXPORT: Sidebar_DebugAttach
; RCX = dwProcessId
; Returns: RAX = 1 success, 0 fail
; =============================================================================
PUBLIC Sidebar_DebugAttach
Sidebar_DebugAttach PROC FRAME
    push rbx
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov ebx, ecx
    
    ; Get DebugActiveProcess from kernel32
    lea rcx, szKernel32
    call qword ptr [__imp_GetModuleHandleA]
    mov rcx, rax
    lea rdx, szDebugActiveProcess
    call GetProcAddr64
    test rax, rax
    jz dbg_fail
    
    mov ecx, ebx
    call rax
    
    test eax, eax
    jz dbg_fail
    
    mov g_DebugActive, 1
    mov eax, 1
    jmp dbg_exit
    
dbg_fail:
    xor eax, eax
    
dbg_exit:
    add rsp, 28h
    pop rbx
    ret
Sidebar_DebugAttach ENDP

; =============================================================================
; EXPORT: Sidebar_DebugLoopIteration
; Single iteration of debug loop (non-blocking with timeout)
; RCX = dwMilliseconds (timeout for WaitForDebugEventEx)
; Returns: RAX = dwDebugEventCode or 0 (timeout/no event)
; =============================================================================
PUBLIC Sidebar_DebugLoopIteration
Sidebar_DebugLoopIteration PROC FRAME
    push rbx
    push rsi
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov esi, ecx            ; Save timeout
    
    ; Get WaitForDebugEventEx
    lea rcx, szKernel32
    call qword ptr [__imp_GetModuleHandleA]
    mov rcx, rax
    lea rdx, szWaitForDebugEventEx
    call GetProcAddr64
    test rax, rax
    jz no_event
    mov rbx, rax            ; Save WaitForDebugEventEx
    
    ; WaitForDebugEventEx(&g_DebugEvent, dwMilliseconds)
    lea rcx, g_DebugEvent
    mov edx, esi
    call rbx
    
    test eax, eax
    jz no_event
    
    ; Got event — read event code
    mov eax, dword ptr [g_DebugEvent + DE_dwDebugEventCode]
    mov ebx, eax            ; Save event code
    
    ; If exception, auto-continue with DBG_CONTINUE
    cmp eax, EXCEPTION_DEBUG_EVENT
    jne return_event
    
    ; Get ContinueDebugEvent
    lea rcx, szKernel32
    call qword ptr [__imp_GetModuleHandleA]
    mov rcx, rax
    lea rdx, szContinueDebugEvent
    call GetProcAddr64
    test rax, rax
    jz return_event
    
    ; ContinueDebugEvent(pid, tid, DBG_CONTINUE)
    mov ecx, dword ptr [g_DebugEvent + DE_dwProcessId]
    mov edx, dword ptr [g_DebugEvent + DE_dwThreadId]
    mov r8d, 00010002h      ; DBG_CONTINUE
    call rax
    
return_event:
    mov eax, ebx            ; Return event code
    jmp dbgloop_exit
    
no_event:
    xor eax, eax
    
dbgloop_exit:
    add rsp, 28h
    pop rsi
    pop rbx
    ret
Sidebar_DebugLoopIteration ENDP

; =============================================================================
; EXPORT: Sidebar_LazyLoadTreeCallback
; RCX = hTree (HWND), RDX = hParent (HTREEITEM), R8 = lpPathString (LPCSTR)
; Returns: RAX = New HTREEITEM handle
; =============================================================================
PUBLIC Sidebar_LazyLoadTreeCallback
Sidebar_LazyLoadTreeCallback PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 60h
    .allocstack 60h
    .endprolog
    
    mov rbx, rcx            ; hTree
    mov rsi, rdx            ; hParent  
    mov rdi, r8             ; lpPath
    
    ; Build TVINSERTSTRUCT on stack
    ; Offset 0: hParent
    mov qword ptr [rsp+80h], rsi
    ; Offset 8: hInsertAfter (TVI_LAST = 0xFFFF0002)
    mov qword ptr [rsp+88h], 0FFFF0002h
    ; Offset 16+0: mask (TVIF_TEXT = 0x0001)
    mov dword ptr [rsp+90h], 1
    ; Offset 16+8: pszText
    mov qword ptr [rsp+98h], rdi
    ; Offset 16+16: cchTextMax
    mov dword ptr [rsp+0A0h], 260

    ; SendMessage(hTree, TVM_INSERTITEMA, 0, &tvis)
    lea r9, [rsp+80h]      ; lParam = &TVINSERTSTRUCT
    xor r8d, r8d            ; wParam = 0
    mov edx, TVM_INSERTITEMA ; msg
    mov rcx, rbx            ; hWnd = hTree
    call qword ptr [__imp_SendMessageA]
    
    add rsp, 60h
    pop rdi
    pop rsi
    pop rbx
    ret
Sidebar_LazyLoadTreeCallback ENDP

; =============================================================================
; EXPORT: Sidebar_CloseLogging
; Closes log file handle, zeroes global
; =============================================================================
PUBLIC Sidebar_CloseLogging
Sidebar_CloseLogging PROC FRAME
    push rbx
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rbx, g_hLogFile
    test rbx, rbx
    jz close_exit
    cmp rbx, -1
    je close_exit
    
    ; CloseHandle
    mov rcx, rbx
    call qword ptr [__imp_CloseHandle]
    
    mov g_hLogFile, 0
    
close_exit:
    add rsp, 28h
    pop rbx
    ret
Sidebar_CloseLogging ENDP

; =============================================================================
; IMPORTS — linked from kernel32.lib / user32.lib / gdi32.lib
; =============================================================================
EXTERN __imp_GetModuleHandleA:QWORD
EXTERN __imp_LoadLibraryA:QWORD
EXTERN __imp_SendMessageA:QWORD
EXTERN __imp_OutputDebugStringA:QWORD
EXTERN __imp_CloseHandle:QWORD
EXTERN __imp_InvalidateRect:QWORD

; =============================================================================
END
