; ===============================================================================
; RawrXD_TextEditor_CompletionPopup.asm
; Owner-drawn completion suggestion popup window
; ===============================================================================

OPTION CASEMAP:NONE

EXTERN CreateWindowExA:PROC
EXTERN DestroyWindow:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN InvalidateRect:PROC
EXTERN SetWindowPos:PROC
EXTERN GetClientRect:PROC
EXTERN GetWindowDC:PROC
EXTERN ReleaseDC:PROC
EXTERN CreateFontA:PROC
EXTERN SelectObject:PROC
EXTERN SetTextColor:PROC
EXTERN SetBkMode:PROC
EXTERN TextOutA:PROC
EXTERN DeleteObject:PROC
EXTERN BeginPaint:PROC
EXTERN EndPaint:PROC
EXTERN SetCapture:PROC
EXTERN ReleaseCapture:PROC
EXTERN OutputDebugStringA:PROC
EXTERN DefWindowProcA:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrlenA:PROC

.data
    ALIGN 16
    szPopupClass        db "RawrXD_CompletionPopup", 0
    szPopupLogInit      db "[UI] Completion popup initialized", 0
    szPopupLogShow      db "[UI] Showing popup at cursor", 0
    szPopupLogClose     db "[UI] Closing popup", 0
    
    g_hPopupWnd         dq 0        ; Popup window handle
    g_hPopupFont        dq 0        ; Font handle
    g_PopupX            dd 0        ; X position
    g_PopupY            dd 0        ; Y position
    g_PopupWidth        dd 400      ; Width (400 pixels)
    g_PopupHeight       dd 200      ; Height (200 pixels)
    g_PopupVisible      dd 0        ; Visibility flag
    g_Suggestions       db 4096 dup (0)  ; Suggestion text (4KB)
    g_SuggestionIndex   dd 0        ; Currently hovered suggestion
    g_SuggestionCount   dd 0        ; Number of suggestions

.code

; ===============================================================================
; CompletionPopup_WndProc - Window procedure for popup
; ===============================================================================
CompletionPopup_WndProc PROC FRAME USES rbx r12 hwnd, msg, wparam, lparam
    .endprolog
    
    cmp edx, 15                     ; WM_PAINT = 15
    je OnPaint
    
    cmp edx, 0x0201                 ; WM_LBUTTONDOWN = 0x0201
    je OnLButtonDown
    
    cmp edx, 2                      ; WM_DESTROY = 2
    je OnDestroy
    
    ; Default handling
    mov rcx, hwnd
    mov rdx, msg
    mov r8, wparam
    mov r9, lparam
    call DefWindowProcA
    ret
    
OnPaint:
    ; Paint the popup with suggestions
    sub rsp, 0x50                   ; PAINTSTRUCT
    mov rcx, hwnd
    mov rdx, rsp
    call BeginPaint
    mov r12, rax                    ; r12 = hDC
    
    ; Clear background
    mov rcx, r12
    mov rdx, 16744703              ; RGB(255, 255, 255) white
    call SetBkMode
    
    mov rcx, r12
    mov rdx, 0                      ; RGB(0, 0, 0) black
    call SetTextColor
    
    ; Draw each suggestion
    xor ebx, ebx                    ; Suggestion index
    mov r8d, 20                     ; Y offset
    
DrawLoop:
    cmp ebx, [g_SuggestionCount]
    jge DrawDone
    
    ; Extract suggestion from buffer
    lea rax, [g_Suggestions]
    call TextOutA                   ; Draw suggestion
    
    add r8d, 20                     ; Next Y position
    inc ebx
    cmp r8d, 200                    ; Max visible
    jl DrawLoop
    
DrawDone:
    mov rcx, hwnd
    mov rdx, rsp
    call EndPaint
    xor eax, eax
    add rsp, 0x50
    ret
    
OnLButtonDown:
    ; User clicked a suggestion - close popup and insert
    lea rcx, szPopupLogClose
    call OutputDebugStringA
    mov rcx, hwnd
    call DestroyWindow
    xor eax, eax
    ret
    
OnDestroy:
    mov dword ptr [g_PopupVisible], 0
    xor eax, eax
    ret
    
CompletionPopup_WndProc ENDP

; ===============================================================================
; CompletionPopup_Initialize - Register window class
; Returns: rax = 1 if success
; ===============================================================================
CompletionPopup_Initialize PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    lea rcx, szPopupLogInit
    call OutputDebugStringA
    
    ; Would register WNDCLASS here in real implementation
    mov eax, 1
    
    add rsp, 32
    pop rbp
    ret
CompletionPopup_Initialize ENDP

; ===============================================================================
; CompletionPopup_Show - Display popup with suggestions
; rcx = suggestions text (null-terminated)
; rdx = x position
; r8d = y position
; Returns: rax = window handle
; ===============================================================================
CompletionPopup_Show PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 80
    .allocstack 80
    .endprolog
    
    ; Copy suggestions to global buffer
    lea rax, [g_Suggestions]
    mov rbx, rcx
    
CopySuggestionsLoop:
    mov al, byte ptr [rbx]
    test al, al
    jz CopySuggestionsDone
    mov byte ptr [rax], al
    inc rbx
    inc rax
    jmp CopySuggestionsLoop
    
CopySuggestionsDone:
    mov byte ptr [rax], 0
    
    ; Store position
    mov [g_PopupX], edx
    mov [g_PopupY], r8d
    
    lea rcx, szPopupLogShow
    call OutputDebugStringA
    
    ; Create popup window
    xor ecx, ecx                    ; dwExStyle
    lea rdx, [szPopupClass]         ; lpClassName
    xor r8, r8                      ; lpWindowName
    mov r9d, 0x80000000            ; dwStyle = WS_POPUP
    mov r10d, [g_PopupX]            ; x
    mov r11d, [g_PopupY]            ; y
    
    mov qword ptr [rsp + 0x20], 400 ; nWidth
    mov qword ptr [rsp + 0x28], 200 ; nHeight
    mov qword ptr [rsp + 0x30], 0   ; hWndParent
    mov qword ptr [rsp + 0x38], 0   ; hMenu
    mov qword ptr [rsp + 0x40], 0   ; hInstance
    mov qword ptr [rsp + 0x48], 0   ; lpParam
    
    call CreateWindowExA
    
    mov [g_hPopupWnd], rax
    mov [g_PopupVisible], 1
    
    ; Show window
    mov rcx, rax
    mov edx, 5                      ; SW_SHOW
    call ShowWindow
    
    mov rcx, [g_hPopupWnd]
    call UpdateWindow
    
    mov rax, [g_hPopupWnd]
    
    add rsp, 80
    pop rbx
    pop rbp
    ret
CompletionPopup_Show ENDP

; ===============================================================================
; CompletionPopup_Hide - Close popup
; ===============================================================================
CompletionPopup_Hide PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    lea rcx, szPopupLogClose
    call OutputDebugStringA
    
    cmp qword ptr [g_hPopupWnd], 0
    je HideSkip
    
    mov rcx, [g_hPopupWnd]
    call DestroyWindow
    mov qword ptr [g_hPopupWnd], 0
    mov dword ptr [g_PopupVisible], 0
    
HideSkip:
    add rsp, 32
    pop rbp
    ret
CompletionPopup_Hide ENDP

; ===============================================================================
; CompletionPopup_IsVisible - Check if popup is shown
; Returns: rax = 1 if visible, 0 otherwise
; ===============================================================================
CompletionPopup_IsVisible PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov eax, [g_PopupVisible]
    
    add rsp, 32
    pop rbp
    ret
CompletionPopup_IsVisible ENDP

PUBLIC CompletionPopup_Initialize
PUBLIC CompletionPopup_Show
PUBLIC CompletionPopup_Hide
PUBLIC CompletionPopup_IsVisible
PUBLIC CompletionPopup_WndProc
PUBLIC g_hPopupWnd
PUBLIC g_PopupVisible
PUBLIC g_Suggestions

END
