; ============================================================================
; RawrXD_TextEditorGUI_AI_INTEGRATION.asm - AI Completion Bridge
; ============================================================================
; Integration layer: Hooks into EditorWindow message handlers
; Triggers tokenization, inference, and ghost text rendering on keystrokes
; ============================================================================

OPTION CASEMAP:NONE

; External C++ bridge functions
EXTERN Tokenize_Phi3Mini:PROC
EXTERN Detokenize_Phi3Mini:PROC
EXTERN Bridge_SubmitCompletion:PROC
EXTERN Bridge_OnSuggestionComplete:PROC
EXTERN Bridge_GetSuggestionText:PROC
EXTERN Bridge_ClearSuggestion:PROC
EXTERN KVCache_UpdateIncremental:PROC

; External Win32 API functions
EXTERN PostMessageA:PROC
EXTERN InvalidateRect:PROC
EXTERN SetTextColor:PROC
EXTERN TextOutA:PROC

; Windows message constants
WM_USER                 EQU 0x0400
WM_USER_COMPLETION      EQU WM_USER + 200       ; Async completion trigger
WM_USER_SUGGESTION      EQU WM_USER + 201       ; Suggestion tokens arrived

.CODE

; ============================================================================
; EditorWindow_OnChar_WithAICompletion(rcx = context, edx = char_code)
; Hook after character insertion: trigger AI completion if user presses Ctrl+Space
; ============================================================================
EditorWindow_OnChar_WithAICompletion PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG
    
    ; Check for Ctrl+Space trigger (ASCII 32 = space)
    cmp edx, 32
    je @OnSpace
    
    ret

@OnSpace:
    ; Quick check: is Ctrl held? (would require GetAsyncKeyState call)
    ; For now: trigger on space (Ctrl+Space = implicit)
    
    ; Post async message to start completion in background
    mov rax, [rcx]                      ; rax = hwnd
    mov ecx, 1224                       ; ecx = WM_USER + 200 (0x04C8)
    xor edx, edx                        ; wparam = 0
    xor r8d, r8d                        ; lparam = 0
    call PostMessageA
    
    ret
EditorWindow_OnChar_WithAICompletion ENDP


; ============================================================================
; AICompletion_ExtractBufferContext(rcx = context_ptr)
; Extract text buffer + cursor position for tokenizer
; Returns: rbx = buffer_text, r8d = buffer_len, r9d = cursor_offset
; ============================================================================
AICompletion_ExtractBufferContext PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG
    
    ; Get buffer pointer from context
    mov rbx, [rcx + 32]                 ; rbx = text buffer pointer
    test rbx, rbx
    jz @ExtractFail
    
    ; Extract buffer length (from buffer struct offset 20)
    mov r8d, [rbx + 20]                 ; r8d = buffer length
    
    ; Get cursor offset from cursor struct
    mov rax, [rcx + 24]                 ; rax = cursor_ptr
    mov r9d, [rax + 0]                  ; r9d = cursor offset
    
    ret

@ExtractFail:
    xor ebx, ebx
    xor r8d, r8d
    xor r9d, r9d
    ret
AICompletion_ExtractBufferContext ENDP


; ============================================================================
; AICompletion_TokenizeBuffer(rcx = text, edx = text_len)
; Call bridge_layer::Tokenize_Phi3Mini
; Returns: rax = token_count, tokens stored in g_tokenBuffer
; ============================================================================

; Global token buffer (shared with bridge_layer.cpp)
g_tokenBuffer QWORD 0                  ; Pointer to token array (allocated in C++)
g_tokenCount DWORD 0                   ; Number of tokens

AICompletion_TokenizeBuffer PROC FRAME
    .ALLOCSTACK 40                     ; Space for local variables
    .ENDPROLOG
    
    mov r8, rsp                        ; r8 = out_tokens array (4096 * 4 bytes on stack)
    mov r9d, 4096                      ; max_tokens = 4096
    
    ; Call Tokenize_Phi3Mini(text, text_len, out_tokens, max_tokens)
    ; rcx = text, edx = text_len already set
    mov edx, edx                       ; Ensure edx is zero-extended
    call Tokenize_Phi3Mini             ; Returns token count in eax
    
    mov [g_tokenCount], eax            ; Store token count
    mov [g_tokenBuffer], r8            ; Store token buffer pointer
    
    ret
AICompletion_TokenizeBuffer ENDP


; ============================================================================
; AICompletion_SubmitRequest(rcx = context_ptr)
; Async handler for WM_USER_COMPLETION message
; Extracts buffer, tokenizes, submits to model router
; ============================================================================
AICompletion_SubmitRequest PROC FRAME
    .ALLOCSTACK 128                    ; CompletionRequest struct
    .ENDPROLOG
    
    mov r12, rcx                       ; r12 = context_ptr
    
    ; Extract buffer context
    mov rcx, r12
    call AICompletion_ExtractBufferContext
    
    ; Build CompletionRequest on stack
    lea rax, [rsp]
    mov [rax + 0], rbx                 ; buffer_text
    mov [rax + 8], r8d                 ; buffer_len
    mov [rax + 12], r9d                ; cursor_offset (encode as line/col later)
    mov dword ptr [rax + 16], 5        ; context_lines = 5
    
    ; Call Bridge_SubmitCompletion
    mov rcx, rax                       ; rcx = CompletionRequest*
    mov rdx, [r12 + 0]                 ; rdx = hwnd (for callbacks)
    call Bridge_SubmitCompletion
    
    ret
AICompletion_SubmitRequest ENDP


; ============================================================================
; AICompletion_ShowGhostText(rcx = context_ptr)
; Handler for WM_USER_SUGGESTION message
; Retrieves suggestion text from bridge and triggers repaint
; ============================================================================
AICompletion_ShowGhostText PROC FRAME
    .ALLOCSTACK 256                    ; Suggestion text buffer
    .ENDPROLOG
    
    ; Get suggestion text from bridge layer
    lea rcx, [rsp]                     ; rcx = output buffer
    mov edx, 256                       ; max size
    call Bridge_GetSuggestionText      ; Returns suggestion string in rax
    
    ; Trigger repaint to show ghost text
    mov rcx, [rsp - 8]                 ; rcx = hwnd (from context)
    xor edx, edx                       ; lpRect = NULL (full window)
    call InvalidateRect
    
    ret
AICompletion_ShowGhostText ENDP


; ============================================================================
; EditorWindow_OnKeyDown_ClearSuggestion
; Clear ghost text when user presses any non-suggestion key
; ============================================================================
EditorWindow_OnKeyDown_ClearSuggestion PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    sub rsp, 32
    
    ; On any key except arrows/modifiers: clear suggestion
    cmp edx, 37                        ; VK_LEFT
    je @SkipClear
    cmp edx, 39                        ; VK_RIGHT
    je @SkipClear
    cmp edx, 38                        ; VK_UP
    je @SkipClear
    cmp edx, 40                        ; VK_DOWN
    je @SkipClear
    cmp edx, 17                        ; VK_CONTROL
    je @SkipClear
    cmp edx, 16                        ; VK_SHIFT
    je @SkipClear
    
    ; Clear ghost text for any other key
    call Bridge_ClearSuggestion
    
@SkipClear:
    ret
EditorWindow_OnKeyDown_ClearSuggestion ENDP


; ============================================================================
; AICompletion_AcceptSuggestion
; Called when user presses Tab to accept ghost text
; ============================================================================
AICompletion_AcceptSuggestion PROC FRAME
    .ALLOCSTACK 256
    .ENDPROLOG
    
    ; Get current suggestion text
    lea rcx, [rsp]
    mov edx, 256
    call Bridge_GetSuggestionText      ; rax = suggestion string
    
    ; Insert suggestion text into buffer
    ; (Similar to OnChar logic, but for entire string)
    ; Pseudo: Insert(rax, length, at_cursor)
    
    ; Clear the suggestion after insertion
    call Bridge_ClearSuggestion
    
    ret
AICompletion_AcceptSuggestion ENDP


; ============================================================================
; AICompletion_RenderGhostText(rcx = hdc, rdx = context_ptr, r8 = suggestion_text)
; Render ghost text in gray after cursor position
; ============================================================================
AICompletion_RenderGhostText PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG
    
    ; Input: rcx = hdc, rdx = context_ptr, r8 = suggestion text
    test r8, r8
    jz @RenderSkip
    
    ; Get cursor position from context
    mov r9, [rdx + 24]                 ; r9 = cursor_ptr
    mov r10d, [r9 + 16]                ; r10d = cursor column
    mov r11d, [r9 + 8]                 ; r11d = cursor line
    
    ; Get font metrics from context
    mov eax, [rdx + 44]                ; eax = char_height
    mov r12d, [rdx + 40]               ; r12d = char_width
    
    ; Calculate pixel position: x = col * char_width, y = line * char_height
    imul r10d, r12d                    ; r10d = x position
    imul r11d, eax                     ; r11d = y position
    
    ; Set text color to gray (128, 128, 128) for ghost text
    mov edx, 08080080h                 ; RGB(128, 128, 128)
    call SetTextColor
    
    ; TextOutA(hdc, x, y, lpString, nCount)
    mov edx, r10d                      ; edx = x
    mov r8d, r11d                      ; r8d = y
    mov r9, r8                         ; r9 = suggestion text
    mov r10d, 256                      ; max length (or calculate strlen)
    
    ; Would call TextOutA with these parameters
    ; invoke TextOutA, rcx, edx, r8d, r9, r10d
    
@RenderSkip:
    ret
AICompletion_RenderGhostText ENDP


; ============================================================================
; Message Router Hook - Insert into EditorWindow_WNDPROC
; ============================================================================
; This code snippet should be inserted into the main WNDPROC message routing:
;
; In EditorWindow_WNDPROC, add:
;
;    cmp ecx, WM_USER_COMPLETION    ; 0x0400 + 200
;    je @OnAICompletion
;    cmp ecx, WM_USER_SUGGESTION    ; 0x0400 + 201
;    je @OnAISuggestion
;
; @OnAICompletion:
;    mov rcx, r10                   ; context_ptr
;    call AICompletion_SubmitRequest
;    xor eax, eax
;    jmp @WNDPROCExit
;
; @OnAISuggestion:
;    mov rcx, r10                   ; context_ptr
;    call AICompletion_ShowGhostText
;    xor eax, eax
;    jmp @WNDPROCExit
;
; ============================================================================

END
