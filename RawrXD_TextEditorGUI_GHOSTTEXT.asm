; ============================================================================
; RawrXD_TextEditorGUI_GHOSTTEXT.asm - Ghost Text Rendering (Suggestion Display)
; ============================================================================
; Extends EditorWindow_OnPaint_Impl with inline suggestion text rendering
; Called after cursor drawing in paint loop
; ============================================================================

OPTION CASEMAP:NONE

; External bridge layer
EXTERN Bridge_GetSuggestionText:PROC
EXTERN SetTextColor:PROC
EXTERN TextOutA:PROC

; Notification from bridge_layer
EXTERN Bridge_GetSuggestionText:PROC

; Ghost text color constants
GHOST_TEXT_COLOR_RGB EQU 808080h       ; RGB(128, 128, 128) - gray

.CODE

; ============================================================================
; EditorWindow_RenderGhostText(rcx = hdc, rdx = context_ptr)
; Render suggestion text in gray after cursor position
; Call this from EditorWindow_OnPaint_Impl, after RenderCursor
; ============================================================================
EditorWindow_RenderGhostText PROC FRAME
    .PUSHREG r12
    .PUSHREG r13
    .ALLOCSTACK 256
    .ENDPROLOG

    sub rsp, 256
    push r12
    push r13
    
    mov r12, rcx                       ; r12 = hdc (device context)
    mov r13, rdx                       ; r13 = context_ptr
    
    ; Get suggestion text from bridge layer
    lea rcx, [rsp + 32]                ; rcx = suggestion_buffer (local)
    mov edx, 200                       ; max size
    call Bridge_GetSuggestionText      ; rax points to suggestion text
    
    ; Check if suggestion is empty
    test rax, rax
    jz @RenderGhostExit
    
    cmp byte ptr [rax], 0
    je @RenderGhostExit
    
    ; Get cursor position from context
    mov r10, [r13 + 24]                ; r10 = cursor_ptr
    mov r11d, [r10 + 16]               ; r11d = cursor_col
    mov r9d, [r10 + 8]                 ; r9d = cursor_line
    
    ; Get font metrics
    mov r8d, [r13 + 44]                ; r8d = char_height
    mov ecx, [r13 + 40]                ; ecx = char_width
    
    ; Calculate pixel position
    ; x = cursor_col * char_width + MARGIN_LEFT (assume 5)
    mov eax, r11d
    imul eax, ecx
    add eax, 5                         ; Left margin
    mov ebx, eax                       ; ebx = x position
    
    ; y = cursor_line * char_height + MARGIN_TOP (assume 5)
    mov eax, r9d
    imul eax, r8d
    add eax, 5                         ; Top margin
    mov ecx, eax                       ; ecx = y position
    
    ; Set text color to gray (semi-transparent style)
    mov rdx, r12                       ; rdx = hdc
    mov eax, GHOST_TEXT_COLOR_RGB      ; eax = RGB(128,128,128)
    mov rcx, rdx
    mov edx, eax
    call SetTextColor
    
    ; Get suggestion text length (assume null-terminated, max 200)
    lea rsi, [rsp + 32]                ; rsi = suggestion_buffer
    xor ecx, ecx                       ; ecx = length counter
@LenLoop:
    cmp byte ptr [rsi + rcx], 0
    je @LenDone
    inc ecx
    cmp ecx, 200
    jl @LenLoop
@LenDone:
    
    ; TextOutA(hdc, x, y, lpString, nCount)
    mov rdx, r12                       ; rdx = hdc
    mov r8d, ebx                       ; r8d = x
    mov r9d, ecx                       ; r9d = y
    ; Push: lpString, nCount
    push rcx                           ; nCount
    lea rax, [rsp + 32 + 8]            ; rax = suggestion_buffer
    push rax                           ; lpString
    
    call TextOutA
    
    add rsp, 16                        ; Cleanup push params

@RenderGhostExit:
    pop r13
    pop r12
    add rsp, 256
    ret
EditorWindow_RenderGhostText ENDP


; ============================================================================
; EditorWindow_OnPaint_WithGhostText (Replacement OnPaint handler)
; ============================================================================
; This is the updated OnPaint that includes ghost text rendering
; Replace your current EditorWindow_OnPaint_Impl with this, or call this after
;
; Hook point in EditorWindow_OnPaint_Impl (after EndPaint):
;
;     mov rcx, r12                    ; rcx = hdc
;     mov rdx, r10                    ; rdx = context_ptr
;     call EditorWindow_RenderGhostText
;
; ============================================================================

; ============================================================================
; Helper: Get cursor pixel position (for ghost text start)
; ============================================================================
EditorWindow_GetCursorPixelPos PROC FRAME
    .ALLOCSTACK 64
    .ENDPROLOG

    sub rsp, 64
    
    ; Input: rcx = context_ptr
    ; Output: eax = x_pixel, edx = y_pixel
    
    mov r10, [rcx + 24]                ; r10 = cursor_ptr
    mov r11d, [r10 + 16]               ; r11d = cursor_col
    mov r9d, [r10 + 8]                 ; r9d = cursor_line
    
    mov r8d, [rcx + 44]                ; r8d = char_height
    mov eax, [rcx + 40]                ; eax = char_width
    
    mov edx, r11d
    imul edx, eax                      ; edx = col * char_width
    add edx, 5                         ; Add left margin
    mov eax, edx                       ; eax = x_pixel
    
    mov edx, r9d
    imul edx, r8d                      ; edx = line * char_height
    add edx, 5                         ; Add top margin
    ; edx = y_pixel
    
    add rsp, 64
    ret
EditorWindow_GetCursorPixelPos ENDP


; ============================================================================
; Suggestion Management
; ============================================================================

; Global suggestion buffer (alternative: fetch from bridge_layer)
g_ghostTextBuffer DB 256 dup(0)
g_ghostTextActive DB 0

; ============================================================================
; EditorWindow_ShowGhostText - Public API to show ghost text suggestion
; ============================================================================
EditorWindow_ShowGhostText PROC
    push rbp
    mov rbp, rsp
    sub rsp, 16
    
    ; rcx = suggestion_text
    test rcx, rcx
    jz @ShowGhostExit
    
    ; Copy text to g_ghostTextBuffer
    mov rsi, rcx                       ; rsi = source (suggestion_text)
    lea rdi, [g_ghostTextBuffer]       ; rdi = destination
    mov rcx, 256                       ; maximum length
    
@CopyLoop:
    test rcx, rcx
    jz @CopyDone
    
    mov al, [rsi]
    mov [rdi], al
    test al, al
    jz @CopyDone
    
    inc rsi
    inc rdi
    dec rcx
    jmp @CopyLoop
    
@CopyDone:
    mov byte ptr [rdi], 0              ; Null terminate
    mov byte ptr [g_ghostTextActive], 1  ; Set active flag
    
@ShowGhostExit:
    add rsp, 16
    pop rbp
    ret
EditorWindow_ShowGhostText ENDP

; ============================================================================
; EditorWindow_HideGhostText - Public API to hide ghost text
; ============================================================================
EditorWindow_HideGhostText PROC
    mov byte ptr [g_ghostTextActive], 0
    ret
EditorWindow_HideGhostText ENDP

; ============================================================================
; EditorWindow_HasGhostText - Check if ghost text is active
; ============================================================================
EditorWindow_HasGhostText PROC
    mov al, [g_ghostTextActive]
    ret
EditorWindow_HasGhostText ENDP

END
