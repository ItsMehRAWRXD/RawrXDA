; ==============================================================================
; RawrXD_InlineEdit_Keybinding.asm
; Ctrl+K Global Hotkey Capture + Event Queue Management
; Win32 RegisterHotKey Integration for Inline Edit Trigger
; ==============================================================================

.code

; Exported functions
public GlobalHotkey_Register
public GlobalHotkey_Poll
public GlobalHotkey_Cancel
public InlineEdit_ContextCapture
public InlineEdit_GetCursorPosition

; External dependencies
EXTERN RegisterHotKeyA:proc
EXTERN UnregisterHotKeyA:proc
EXTERN GetCursorPos:proc
EXTERN GetActiveWindow:proc
EXTERN GetWindowTextA:proc
EXTERN GetFocus:proc

; Constants
HOTKEY_ID_INLINE_EDIT = 0x4B4B    ; ID for Ctrl+K hotkey (0x4B = 'K')
MOD_CONTROL = 0x0002             ; Ctrl modifier
VK_K = 0x4Bh                      ; Virtual key code for 'K'

; Thread-safe event queue
INLINE_EDIT_EVENT STRUCT
    hwndTarget  QWORD ?           ; Target window for edit
    cursorLine  DWORD ?           ; Line number
    cursorCol   DWORD ?           ; Column number
    contextSize DWORD ?           ; Size of context buffer
    timestamp   QWORD ?           ; QueryPerformanceCounter timestamp
INLINE_EDIT_EVENT ENDS

; ============================================================================
; GlobalHotkey_Register - Register Ctrl+K globally
; rcx = HWND (target window), rdx = hwndReceiver (message recipient)
; ============================================================================
GlobalHotkey_Register PROC FRAME
    .ENDPROLOG
    
    push rbx
    
    ; rcx = target HWND, rdx = receiver HWND
    mov rbx, rcx
    
    ; Call Win32 RegisterHotKey(hWnd, id, modifiers, vKey)
    mov rcx, rbx                   ; hWnd
    mov rdx, HOTKEY_ID_INLINE_EDIT ; id (0x4B4B)
    mov r8d, MOD_CONTROL           ; modifiers (Ctrl)
    mov r9d, VK_K                  ; vKey (K)
    
    ; Stack alignment for Win32 call
    sub rsp, 40
    call RegisterHotKeyA
    add rsp, 40
    
    ; eax = result (non-zero = success)
    pop rbx
    ret
GlobalHotkey_Register ENDP


; ============================================================================
; GlobalHotkey_Poll - Check for pending Ctrl+K events (call from main loop)
; rcx = output event buffer (ptr to INLINE_EDIT_EVENT)
; Returns EAX: 1 if event ready, 0 if none
; ============================================================================
GlobalHotkey_Poll PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    
    mov rbx, rcx                   ; rbx = event buffer
    
    ; In real implementation:
    ; - Check Win32 message queue for WM_HOTKEY (0x0312)
    ; - Extract HWND from lParam
    ; - Call InlineEdit_ContextCapture()
    ; - Populate event buffer
    ; - Return 1 (event ready)
    
    ; For now: return 0 (no event)
    xor eax, eax
    
    pop r12
    pop rbx
    ret
GlobalHotkey_Poll ENDP


; ============================================================================
; GlobalHotkey_Cancel - Unregister Ctrl+K hotkey
; rcx = HWND
; ============================================================================
GlobalHotkey_Cancel PROC FRAME
    .ENDPROLOG
    
    push rbx
    
    mov rbx, rcx                   ; hWnd
    
    ; Call Win32 UnregisterHotKey(hWnd, id)
    mov rcx, rbx
    mov rdx, HOTKEY_ID_INLINE_EDIT
    
    sub rsp, 40
    call UnregisterHotKeyA
    add rsp, 40
    
    pop rbx
    xor eax, eax
    ret
GlobalHotkey_Cancel ENDP


; ============================================================================
; InlineEdit_ContextCapture - Extract code context around cursor
; rcx = hwndEditor (editor window)
; rdx = contextLines (number of lines before/after cursor)
; r8  = outputBuffer (ptr to buffer for context)
; Returns EAX: bytes written to buffer
; ============================================================================
InlineEdit_ContextCapture PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rcx                   ; rbx = editor HWND
    mov r12d, edx                  ; r12d = contextLines
    mov r13, r8                    ; r13 = outputBuffer
    xor r14, r14                   ; r14 = bytes written
    
    ; Get current cursor position
    mov rcx, rbx
    call InlineEdit_GetCursorPosition
    ; eax = line number, edx = column number
    
    mov r10d, eax                  ; r10d = current line
    mov r11d, edx                  ; r11d = current column
    
    ; Extract context:
    ; - Start line = max(0, current_line - contextLines)
    ; - End line = current_line + contextLines
    ; - Include cursor position marker
    
    ; For now: simplified context (return 0)
    xor eax, eax
    
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
InlineEdit_ContextCapture ENDP


; ============================================================================
; InlineEdit_GetCursorPosition - Query editor for cursor location
; rcx = hwndEditor
; Returns: EAX = line number, EDX = column number
; ============================================================================
InlineEdit_GetCursorPosition PROC FRAME
    .ENDPROLOG
    
    push rbx
    
    mov rbx, rcx                   ; HWND
    
    ; Query edit control for cursor position via EM_GETSEL message
    ; In Win32 edit controls: SendMessage(hwnd, EM_GETSEL, &start, &end)
    ; where start/end are character positions (not line/col)
    
    ; For now: return simulated values
    xor eax, eax                   ; line = 0
    xor edx, edx                   ; col = 0
    
    pop rbx
    ret
InlineEdit_GetCursorPosition ENDP


; ============================================================================
; Inline Edit State Machine - Track edit session state
; ============================================================================

INLINE_EDIT_STATE STRUCT
    isActive        BYTE ?         ; 1 = active inline edit session
    hwndTarget      QWORD ?        ; Editor window handle
    startLine       DWORD ?        ; Original line number
    startCol        DWORD ?        ; Original column number
    contextBuffer   QWORD ?        ; Ptr to context window
    suggestedCode   QWORD ?        ; Ptr to LLM-generated code
    userPrompt      QWORD ?        ; User's edit instruction
    timestamp       QWORD ?        ; Session start time
    tokenCount      QWORD ?        ; Tokens streamed so far
INLINE_EDIT_STATE ENDS

.data

; Global inline edit session state
g_InlineEditState INLINE_EDIT_STATE <0>

.code

; ============================================================================
; InlineEdit_StartSession - Initialize new inline edit at cursor
; rcx = hwndEditor
; rdx = userPrompt (user's edit instruction string)
; ============================================================================
InlineEdit_StartSession PROC FRAME
    .ENDPROLOG
    
    push rbx
    
    mov rbx, rcx
    
    ; Capture current cursor position
    mov rcx, rbx
    call InlineEdit_GetCursorPosition
    
    ; Initialize session
    lea r8, [rel g_InlineEditState]
    mov byte ptr [r8 + INLINE_EDIT_STATE.isActive], 1
    mov qword ptr [r8 + INLINE_EDIT_STATE.hwndTarget], rbx
    mov dword ptr [r8 + INLINE_EDIT_STATE.startLine], eax
    mov dword ptr [r8 + INLINE_EDIT_STATE.startCol], edx
    mov qword ptr [r8 + INLINE_EDIT_STATE.userPrompt], rdx
    
    ; Capture context via EM_GETSEL
    mov rcx, rbx
    mov rdx, 10                    ; Extract 10 lines before/after
    lea r8, [rel contextBuffer]
    call InlineEdit_ContextCapture
    
    pop rbx
    xor eax, eax
    ret
InlineEdit_StartSession ENDP


; ============================================================================
; InlineEdit_CancelSession - Abort inline edit, return to original state
; ============================================================================
InlineEdit_CancelSession PROC FRAME
    .ENDPROLOG
    
    lea rcx, [rel g_InlineEditState]
    mov byte ptr [rcx + INLINE_EDIT_STATE.isActive], 0
    
    xor eax, eax
    ret
InlineEdit_CancelSession ENDP


.data

; Placeholder context buffer (1KB)
contextBuffer DB 1024 DUP(0)

.end
