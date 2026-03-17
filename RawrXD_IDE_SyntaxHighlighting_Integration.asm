; =============================================================================
; RawrXD_IDE_SyntaxHighlighting_Integration.asm
; Integration layer: Wire MASM syntax highlighter into editor surface
; =============================================================================

OPTION CASEMAP:NONE

; ============================================================================
; Monaco Editor Tokenization Bridge (for Monaco-based editors)
; ============================================================================

EXTERN GetMonacoTokenizer:PROC
EXTERN ApplyTokenColors:PROC
EXTERN RenderEditorLine:PROC

; ============================================================================
; INIT_MASM_SYNTAX
; Initialize MASM syntax highlighting in editor
; RCX = editor handle
; RDX = config path ("D:\rawrxd\config\masm_syntax_highlighting.json")
; ============================================================================
INIT_MASM_SYNTAX PROC FRAME
    .pushreg rbp
    .pushreg rdi
    push rbp
    push rdi
    .endprolog
    
    mov rdi, rcx                ; Save editor handle
    
    ; Load color configuration from JSON
    mov rcx, rdx                ; Config path
    call LoadSyntaxConfig
    test rax, rax
    jz .init_fail
    
    ; Register tokenizer for .asm files
    mov rcx, rdi                ; Editor handle
    mov rdx, rax                ; Config
    lea r8, szAsmExt            ; File extension ".asm"
    call RegisterFileExtension
    
    ; Enable syntax highlighting in editor
    mov rcx, rdi
    mov edx, 1                  ; Enable flag
    call EnableSyntaxHighlighting
    
    mov rax, 1                  ; Success
    jmp .init_done
    
.init_fail:
    xor rax, rax
    
.init_done:
    pop rdi
    pop rbp
    ret
INIT_MASM_SYNTAX ENDP

; ============================================================================
; HIGHLIGHT_MASM_LINE
; Apply syntax highlighting to a single line of MASM code
; RCX = editor handle
; RDX = line number
; R8  = line text (null-terminated)
; ============================================================================
HIGHLIGHT_MASM_LINE PROC FRAME
    .pushreg rbp
    .pushreg rdi
    .pushreg rsi
    .pushreg rbx
    push rbp
    push rdi
    push rsi
    push rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rsi, r8                 ; rsi = line text
    
    ; Allocate token buffer (max 256 tokens per line)
    mov rcx, 256 * 16           ; 256 tokens × 16 bytes each
    sub rsp, rcx
    mov rdi, rsp                ; rdi = token buffer
    
    ; Tokenize line
    mov rcx, rsi                ; source
    mov rdx, rdi                ; output tokens
    mov r8, 256                 ; max tokens
    call TOKENIZE_MASM_LINE
    mov rbx, rax                ; rbx = token count
    
    ; Apply each token's color to display
    xor r9, r9                  ; r9 = token index
    
.apply_tokens:
    cmp r9, rbx
    jge .highlight_done
    
    ; Get token info
    mov eax, r9d
    mov ecx, 16
    imul eax, ecx
    add rax, rdi
    
    mov edx, [rax]              ; token start
    mov r10d, [rax + 4]         ; token length
    mov r11b, [rax + 8]         ; token type
    mov r14d, [rax + 12]        ; token color
    
    ; Apply color to editor display
    ; Editor API call: SetLineTokenColor(lineNum, startCol, endCol, color)
    mov rcx, rdx                ; RCX = start column
    mov rdx, r10d
    add rdx, rcx                ; RDX = end column
    mov r8, r14                 ; R8 = color
    mov r9, [rsp + 32 * 2]      ; R9 = line number (from prologue)
    call ApplyTokenColors
    
    inc r9
    jmp .apply_tokens
    
.highlight_done:
    add rsp, 256 * 16
    add rsp, 32
    pop rbx
    pop rsi
    pop rdi
    pop rbp
    ret
HIGHLIGHT_MASM_LINE ENDP

; ============================================================================
; HIGHLIGHT_VISIBLE_MASM_BUFFER
; Apply syntax highlighting to all visible lines in editor
; RCX = editor handle
; ============================================================================
HIGHLIGHT_VISIBLE_MASM_BUFFER PROC FRAME
    .pushreg rbp
    .pushreg rdi
    .pushreg rsi
    push rbp
    push rdi
    push rsi
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rdi, rcx                ; rdi = editor handle
    
    ; Get visible range (line numbers)
    mov rcx, rdi
    call GetVisibleLineRange
    ; Returns: RAX = first line, RDX = last line
    
    mov rsi, rax                ; rsi = current line
    mov rbx, rdx                ; rbx = last line
    
.highlight_loop:
    cmp rsi, rbx
    jg .highlight_all_done
    
    ; Get line text
    mov rcx, rdi
    mov rdx, rsi
    call GetLineText
    ; Returns: RAX = line text (null-terminated)
    
    ; Highlight this line
    mov rcx, rdi
    mov rdx, rsi
    mov r8, rax
    call HIGHLIGHT_MASM_LINE
    
    ; Render the highlighted line
    mov rcx, rdi
    mov rdx, rsi
    call RenderEditorLine
    
    inc rsi
    jmp .highlight_loop
    
.highlight_all_done:
    add rsp, 32
    pop rsi
    pop rdi
    pop rbp
    ret
HIGHLIGHT_VISIBLE_MASM_BUFFER ENDP

; ============================================================================
; ON_MASM_BUFFER_CHANGED
; Event handler: Syntax highlight when buffer changes
; RCX = editor handle
; RDX = change type (INSERT=1, DELETE=2, REPLACE=3)
; R8  = start line
; R9  = end line
; ============================================================================
ON_MASM_BUFFER_CHANGED PROC FRAME
    .pushreg rbp
    push rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; For efficiency, only highlight changed lines + context (±1 line)
    mov eax, r8d
    test eax, eax
    jz .from_top
    dec eax
    
.from_top:
    mov esi, eax                ; esi = start highlight
    
    mov eax, r9d
    add eax, 1
    mov edi, eax                ; edi = end highlight
    
    ; Highlight range
.highlight_changed_range:
    cmp esi, edi
    jg .change_done
    
    mov rcx, rdx                ; editor handle (preserved from prologue)
    mov rdx, rsi                ; line number
    call GetLineText
    
    mov rcx, rdx
    mov rdx, rsi
    mov r8, rax
    call HIGHLIGHT_MASM_LINE
    
    mov rcx, rdx
    mov rdx, rsi
    call RenderEditorLine
    
    inc rsi
    jmp .highlight_changed_range
    
.change_done:
    add rsp, 32
    pop rbp
    ret
ON_MASM_BUFFER_CHANGED ENDP

; ============================================================================
; REGISTER_SYNTAX_HANDLERS
; Hook syntax highlighting into editor event system
; RCX = editor handle
; ============================================================================
REGISTER_SYNTAX_HANDLERS PROC FRAME
    .pushreg rbp
    push rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Register change event handler
    mov rcx, rcx                ; editor handle
    lea rdx, szOnBufferChanged
    lea r8, ON_MASM_BUFFER_CHANGED
    call RegisterEventHandler
    
    ; Initial highlighting of visible buffer
    mov rcx, rcx                ; editor handle
    call HIGHLIGHT_VISIBLE_MASM_BUFFER
    
    add rsp, 32
    pop rbp
    ret
REGISTER_SYNTAX_HANDLERS ENDP

; ============================================================================
; DATA
; ============================================================================

.data
    ALIGN 8
    
    szAsmExt            DB ".asm",0
    szOnBufferChanged   DB "onBufferChange",0
    
    ; Color palette (can be customized)
    g_ColorKeyword      DD 0569CD6h     ; Blue
    g_ColorRegister     DD 04EC9B0h     ; Cyan
    g_ColorComment      DD 06A9955h     ; Green (gray-green)
    g_ColorString       DD 0CE9178h     ; Orange
    g_ColorNumber       DD 0B5CEA8h     ; Light green
    g_ColorDirective    DD 0C586C0h     ; Purple
    g_ColorLabel        DD 0D7BA7Dh     ; Tan
    g_ColorInstruction  DD 0569CD6h     ; Blue (same as keyword)

END
