; ==========================================================================
; MASM Qt6 Component: Syntax Highlighter
; ==========================================================================
; Lexer for .asm, .c, .cpp files with keyword/string/comment colorization.
;
; Features:
;   - Lexical analysis (tokenize source code)
;   - Keyword highlighting (MASM, C/C++ keywords)
;   - String/char literal highlighting
;   - Comment highlighting (single/multi-line)
;   - Preprocessor directive highlighting
;   - Number/constant highlighting
;
; Architecture:
;   - SYNTAX_HIGHLIGHTER structure
;   - Token array (type_id, start offset, length, color)
;   - Language-specific keyword tables
;   - Lazy re-highlighting (only affected lines)
;
; ==========================================================================

option casemap:none

; External memory functions (provided by malloc_wrapper.asm)
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN realloc:PROC
EXTERN memset:PROC

include windows.inc
includelib kernel32.lib
includelib user32.lib

; Define OBJECT_BASE structure (from qt6_foundation)
OBJECT_BASE STRUCT
    obj_vmt          QWORD ?
    obj_hwnd         QWORD ?
    obj_parent       QWORD ?
    obj_children     QWORD ?
    obj_child_count  DWORD ?
    obj_flags        DWORD ?
    obj_user_data    QWORD ?
OBJECT_BASE ENDS

FLAG_VISIBLE         EQU 00000001h
FLAG_DIRTY           EQU 00000008h

;==========================================================================
; STRUCTURES
;=========================================================================

; Syntax token
SYNTAX_TOKEN STRUCT
    start_offset    QWORD ?         ; Byte offset in_val source
    token_length    DWORD ?         ; Token length
    type_id         DWORD ?         ; TOKEN_KEYWORD, TOKEN_STRING, etc.
    color           DWORD ?         ; RGB color
SYNTAX_TOKEN ENDS

; Syntax highlighter
SYNTAX_HIGHLIGHTER STRUCT
    ; OBJECT_BASE fields
    obj_vmt          QWORD ?
    obj_hwnd         QWORD ?
    obj_parent       QWORD ?
    obj_children     QWORD ?
    obj_child_count  DWORD ?
    obj_flags        DWORD ?
    obj_user_data    QWORD ?
    
    ; Source code
    source_ptr      QWORD ?         ; Pointer to source text
    source_len      QWORD ?         ; Total length
    
    ; Tokens
    tokens_ptr      QWORD ?         ; Array of SYNTAX_TOKEN
    token_count     DWORD ?         ; Number of tokens
    max_tokens      DWORD ?         ; Allocated slots
    
    ; File type_id
    file_ext        DWORD ?         ; EXT_ASM, EXT_C, EXT_CPP
    
    ; State
    dirty_start     QWORD ?         ; Start of dirty region
    dirty_end       QWORD ?         ; End of dirty region
    last_error_pos  QWORD ?         ; Position of last syntax error (if any)
    
    ; Flags
    flags           DWORD ?         ; FLAG_VISIBLE, FLAG_DIRTY
SYNTAX_HIGHLIGHTER ENDS

;==========================================================================
; CONSTANTS
;==========================================================================

; Token types
TOKEN_KEYWORD       EQU 1
TOKEN_STRING        EQU 2
TOKEN_COMMENT       EQU 3
TOKEN_NUMBER        EQU 4
TOKEN_PREPROCESSOR  EQU 5
TOKEN_IDENTIFIER    EQU 6
TOKEN_OPERATOR      EQU 7
TOKEN_WHITESPACE    EQU 8

; File types
EXT_ASM             EQU 1
EXT_C               EQU 2
EXT_CPP             EQU 3
EXT_HEADER          EQU 4

; Colors
COLOR_KEYWORD       EQU 0000FFh    ; Blue
COLOR_STRING        EQU 008000h    ; Green
COLOR_COMMENT       EQU 808080h    ; Gray
COLOR_NUMBER        EQU 0FF0000h   ; Red
COLOR_PREPROC       EQU 800080h    ; Purple
COLOR_TEXT          EQU 000000h    ; Black

;==========================================================================
; DATA SECTION - Keyword Tables
;==========================================================================

.DATA
ALIGN 8
; MASM keywords (common)
MASM_KEYWORDS:
    db "MOV", 0
    db "ADD", 0
    db "SUB", 0
    db "MUL", 0
    db "DIV", 0
    db "PUSH", 0
    db "POP", 0
    db "CALL", 0
    db "RET", 0
    db "JMP", 0
    db "JE", 0
    db "JNE", 0
    db "CMP", 0
    db "TEST", 0
    db "XOR", 0
    db "AND", 0
    db "OR", 0
    db "STRUCT", 0
    db "ENDS", 0
    db "PROC", 0
    db "ENDP", 0
    db "PUBLIC", 0
    db "PRIVATE", 0
    db "QWORD", 0
    db "DWORD", 0
    db "WORD", 0
    db "BYTE", 0
    db 0

; C/C++ keywords
C_KEYWORDS:
    db "int", 0
    db "float", 0
    db "double", 0
    db "char", 0
    db "void", 0
    db "bool", 0
    db "struct", 0
    db "class", 0
    db "if", 0
    db "else", 0
    db "for", 0
    db "while", 0
    db "return", 0
    db "const", 0
    db "static", 0
    db "extern", 0
    db "typedef", 0
    db "template", 0
    db "namespace", 0
    db "public", 0
    db "private", 0
    db "protected", 0
    db 0

;==========================================================================
; PUBLIC FUNCTIONS
;==========================================================================

PUBLIC syntax_highlighter_create
PUBLIC syntax_highlighter_destroy
PUBLIC syntax_highlighter_tokenize
PUBLIC syntax_highlighter_get_color
PUBLIC syntax_highlighter_update_dirty_region
PUBLIC syntax_highlighter_detect_language

;==========================================================================
; IMPLEMENTATION
;==========================================================================

.code

; =============== syntax_highlighter_create ===============
; Create a syntax highlighter instance
; Inputs:  rcx = source text ptr, rdx = length, r8 = file extension
; Outputs: rax = SYNTAX_HIGHLIGHTER ptr
syntax_highlighter_create PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; TODO: Allocate SYNTAX_HIGHLIGHTER structure (~192 bytes)
    ; TODO: Detect language from file extension
    ; TODO: Call syntax_highlighter_tokenize to parse source
    ; TODO: Build token array
    
    xor rax, rax
    add rsp, 32
    pop rbp
    ret
syntax_highlighter_create ENDP

; =============== syntax_highlighter_destroy ===============
; Destroy highlighter and free resources
; Inputs:  rcx = SYNTAX_HIGHLIGHTER ptr
; Outputs: rax = success (1) or failure (0)
syntax_highlighter_destroy PROC
    push rbp
    mov rbp, rsp
    
    ; TODO: Free token array
    ; TODO: Free SYNTAX_HIGHLIGHTER structure
    
    mov rax, 1
    pop rbp
    ret
syntax_highlighter_destroy ENDP

; =============== syntax_highlighter_tokenize ===============
; Tokenize source code and build token array
; Inputs:  rcx = SYNTAX_HIGHLIGHTER ptr
; Outputs: rax = number of tokens, or error code
syntax_highlighter_tokenize PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; TODO: Walk through source text
    ; TODO: For each position:
    ;   - Check if keyword (match MASM_KEYWORDS or C_KEYWORDS)
    ;   - Check if string/char literal (" or ')
    ;   - Check if comment (// or /* */)
    ;   - Check if preprocessor (#)
    ;   - Check if number (0-9)
    ;   - Check if identifier (letter or _)
    ;   - Check if operator (+ - * / etc)
    ; TODO: Create SYNTAX_TOKEN for each
    ; TODO: Store in_val tokens array
    ; TODO: Return token count
    
    xor rax, rax
    add rsp, 32
    pop rbp
    ret
syntax_highlighter_tokenize ENDP

; =============== syntax_highlighter_get_color ===============
; Get color for a token at given offset
; Inputs:  rcx = SYNTAX_HIGHLIGHTER ptr, rdx = offset
; Outputs: rax = RGB color
syntax_highlighter_get_color PROC
    push rbp
    mov rbp, rsp
    
    ; TODO: Binary search tokens array to find token at offset
    ; TODO: Return token.color
    
    mov rax, COLOR_TEXT             ; Default to black
    pop rbp
    ret
syntax_highlighter_get_color ENDP

; =============== syntax_highlighter_update_dirty_region ===============
; Mark region for re-highlighting
; Inputs:  rcx = SYNTAX_HIGHLIGHTER ptr, rdx = start offset, r8 = end offset
; Outputs: rax = success (1) or failure (0)
syntax_highlighter_update_dirty_region PROC
    push rbp
    mov rbp, rsp
    
    ; TODO: Set dirty_start = min(dirty_start, rdx)
    ; TODO: Set dirty_end = max(dirty_end, r8)
    ; TODO: Re-tokenize affected region on next paint
    
    mov rax, 1
    pop rbp
    ret
syntax_highlighter_update_dirty_region ENDP

; =============== syntax_highlighter_detect_language ===============
; Detect programming language from file extension
; Inputs:  rcx = file extension (LPSTR, e.g. "asm", "c", "cpp")
; Outputs: rax = EXT_ASM, EXT_C, EXT_CPP, or 0 (unknown)
syntax_highlighter_detect_language PROC
    push rbp
    mov rbp, rsp
    
    ; TODO: Compare extension string
    ;   - "asm" → EXT_ASM
    ;   - "c" → EXT_C
    ;   - "cpp", "cc", "cxx" → EXT_CPP
    ;   - "h", "hpp" → EXT_HEADER
    
    xor rax, rax
    pop rbp
    ret
syntax_highlighter_detect_language ENDP

; =============== Helper: is_keyword ===============
; Check if identifier is keyword
; Inputs:  rcx = identifier ptr, rdx = length, r8 = keyword table
; Outputs: rax = 1 (is keyword) or 0 (not keyword)
is_keyword PROC
    push rbp
    mov rbp, rsp
    
    ; TODO: Walk through keyword table
    ; TODO: Compare each keyword to identifier
    ; TODO: Return 1 if match, 0 if no match
    
    xor rax, rax
    pop rbp
    ret
is_keyword ENDP

; =============== Helper: is_digit ===============
; Check if character is digit
; Inputs:  al = character
; Outputs: rax = 1 (is digit) or 0 (not digit)
is_digit PROC
    cmp al, '0'
    jb notdigit
    cmp al, '9'
    ja notdigit
    mov rax, 1
    ret
notdigit:
    xor rax, rax
    ret
is_digit ENDP

; =============== Helper: is_alpha ===============
; Check if character is letter or underscore
; Inputs:  al = character
; Outputs: rax = 1 (is alpha) or 0 (not alpha)
is_alpha PROC
    cmp al, 'a'
    jl check_upper
    cmp al, 'z'
    jle isalpha
check_upper:
    cmp al, 'A'
    jl check_underscore
    cmp al, 'Z'
    jle isalpha
check_underscore:
    cmp al, '_'
    jne notalpha
isalpha:
    mov rax, 1
    ret
notalpha:
    xor rax, rax
    ret
is_alpha ENDP

END
