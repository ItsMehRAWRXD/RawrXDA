; ==============================================================================
; Hook_Simulator_Test.asm - Live Message Loop Latency Validation
; ==============================================================================
; Creates a minimal window, installs GetMessageW hook, pumps messages,
; and reports RDTSC latency statistics.
;
; Acceptance: Hook overhead < 0.5ms (~1.5M cycles at 3GHz)
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

EXTERN ExitProcess : PROC
EXTERN GetModuleHandleA : PROC
EXTERN RegisterClassExA : PROC
EXTERN CreateWindowExA : PROC
EXTERN ShowWindow : PROC
EXTERN UpdateWindow : PROC
EXTERN GetMessageA : PROC
EXTERN TranslateMessage : PROC
EXTERN DispatchMessageA : PROC
EXTERN PostQuitMessage : PROC
EXTERN DefWindowProcA : PROC
EXTERN Sleep : PROC
EXTERN GetTickCount : PROC

; Hook Simulator APIs
EXTERN INSTALL_HOOK : PROC
EXTERN UNINSTALL_HOOK : PROC
EXTERN GET_LATENCY_STATS : PROC
EXTERN HOOK_HANDLER : PROC
EXTERN GetProcAddress : PROC

; Ghost Engine APIs
EXTERN INIT_GHOST_BUFFER : PROC
EXTERN GET_GHOST_STATS : PROC

; ==============================================================================
; Data Section
; ==============================================================================
.data
ALIGN 16

; Window class
wc              db 80 dup(0)            ; WNDCLASSEXA structure (80 bytes)
class_name      db "HookTestClass", 0
window_title    db "Sovereign Hook Simulator Test", 0

; Message structure
msg             db 48 dup(0)            ; MSG structure (48 bytes)

; Stats buffer (32 bytes for GET_LATENCY_STATS)
ALIGN 16
stats_buffer    dq 4 dup(0)

; Ghost stats buffer
ALIGN 16
ghost_stats     dq 4 dup(0)

; Test configuration
TEST_ITERATIONS equ 1000
TEST_DELAY_MS   equ 1

; Performance threshold
LATENCY_BUDGET_CYCLES equ 1500000       ; 0.5ms at 3GHz

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; WndProc - Minimal window procedure
; ==============================================================================
WndProc PROC
    ; Check for WM_DESTROY
    cmp edx, 2              ; WM_DESTROY = 0x0002
    jne wndproc_default
    
    ; Post quit message
    xor ecx, ecx
    call PostQuitMessage
    xor eax, eax
    ret
    
wndproc_default:
    ; Call DefWindowProcA
    ; rcx = hwnd, rdx = uMsg, r8 = wParam, r9 = lParam
    jmp DefWindowProcA
WndProc ENDP

; ==============================================================================
; main - Test entry point
; ==============================================================================
main PROC
    sub rsp, 40
    
    ; Initialize Ghost Engine
    call INIT_GHOST_BUFFER
    test eax, eax
    jz test_fail
    
    ; Get GetMessageA address from user32
    xor ecx, ecx
    call GetModuleHandleA   ; Get user32 handle
    mov rcx, rax
    lea rdx, szGetMessageA
    call GetProcAddress
    test rax, rax
    jz test_fail
    mov r13, rax            ; r13 = GetMessageA address
    
    ; Install hook
    mov rcx, r13
    lea rdx, HOOK_HANDLER
    call INSTALL_HOOK
    test eax, eax
    jz test_fail
    
    ; Create window class
    xor ecx, ecx
    call GetModuleHandleA
    mov r14, rax            ; r14 = hInstance
    
    ; Fill WNDCLASSEXA
    lea rdi, wc
    mov dword ptr [rdi + 0], 80     ; cbSize
    mov dword ptr [rdi + 4], 3      ; style (CS_HREDRAW | CS_VREDRAW)
    lea rax, WndProc
    mov [rdi + 8], rax              ; lpfnWndProc
    mov dword ptr [rdi + 16], 0   ; cbClsExtra
    mov dword ptr [rdi + 20], 0   ; cbWndExtra
    mov [rdi + 24], r14             ; hInstance
    mov qword ptr [rdi + 32], 0   ; hIcon
    mov qword ptr [rdi + 40], 0   ; hCursor
    mov qword ptr [rdi + 48], 0   ; hbrBackground
    lea rax, class_name
    mov [rdi + 56], rax             ; lpszMenuName
    lea rax, class_name
    mov [rdi + 64], rax             ; lpszClassName
    mov qword ptr [rdi + 72], 0   ; hIconSm
    
    ; Register class
    mov rcx, rdi
    call RegisterClassExA
    test eax, eax
    jz test_fail
    
    ; Create window
    xor ecx, ecx                    ; dwExStyle
    lea rdx, class_name             ; lpClassName
    lea r8, window_title            ; lpWindowName
    mov r9d, 0CF0000h               ; dwStyle (WS_OVERLAPPEDWINDOW)
    mov dword ptr [rsp + 32], 80000000h ; CW_USEDEFAULT (X)
    mov dword ptr [rsp + 40], 80000000h ; CW_USEDEFAULT (Y)
    mov dword ptr [rsp + 48], 80000000h ; CW_USEDEFAULT (Width)
    mov dword ptr [rsp + 56], 80000000h ; CW_USEDEFAULT (Height)
    mov qword ptr [rsp + 64], 0   ; hWndParent
    mov qword ptr [rsp + 72], 0   ; hMenu
    mov [rsp + 80], r14           ; hInstance
    mov qword ptr [rsp + 88], 0   ; lpParam
    call CreateWindowExA
    test rax, rax
    jz test_fail
    mov r15, rax                    ; r15 = hwnd
    
    ; Show window
    mov rcx, r15
    mov edx, 1                      ; SW_SHOWNORMAL
    call ShowWindow
    
    ; Update window
    mov rcx, r15
    call UpdateWindow
    
    ; Message loop (TEST_ITERATIONS)
    mov r14d, TEST_ITERATIONS
    
msg_loop:
    ; GetMessageA (hooked)
    lea rcx, msg
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call GetMessageA
    
    test eax, eax
    jz msg_done                     ; WM_QUIT
    
    ; Translate and dispatch
    lea rcx, msg
    call TranslateMessage
    lea rcx, msg
    call DispatchMessageA
    
    ; Small delay to simulate realistic message rate
    mov ecx, TEST_DELAY_MS
    call Sleep
    
    dec r14d
    jnz msg_loop
    
msg_done:
    ; Destroy window
    mov rcx, r15
    mov edx, 2                      ; WM_DESTROY
    xor r8d, r8d
    xor r9d, r9d
    call DefWindowProcA
    
    ; Uninstall hook
    call UNINSTALL_HOOK
    test eax, eax
    jz test_fail
    
    ; Get latency statistics
    lea rcx, stats_buffer
    call GET_LATENCY_STATS
    
    ; Validate results
    ; stats_buffer[0] = Total cycles
    ; stats_buffer[8] = Call count
    ; stats_buffer[16] = Max latency
    ; stats_buffer[24] = Min latency
    
    mov rax, stats_buffer[8]        ; Call count
    cmp rax, 0
    je test_fail                    ; No calls measured
    
    mov rax, stats_buffer[16]       ; Max latency
    cmp rax, LATENCY_BUDGET_CYCLES
    ja test_fail                    ; Exceeded budget
    
    ; Get Ghost Engine stats
    lea rcx, ghost_stats
    call GET_GHOST_STATS
    
    ; All tests passed
    xor ecx, ecx
    call ExitProcess
    
test_fail:
    mov ecx, 1
    call ExitProcess
    
main ENDP

; ==============================================================================
; Data for GetProcAddress
; ==============================================================================
.data
szGetMessageA   db "GetMessageA", 0

; ==============================================================================
; End
; ==============================================================================
end
