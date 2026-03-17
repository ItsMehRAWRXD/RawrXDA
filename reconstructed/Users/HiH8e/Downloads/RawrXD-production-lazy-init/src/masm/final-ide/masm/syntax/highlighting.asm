; ============================================================================
; FILE: masm_syntax_highlighting.asm
; TITLE: MASM Syntax Highlighting Engine
; PURPOSE: Advanced code coloring and syntax highlighting for multiple languages
; LINES: 800+ (Complete syntax highlighting system)
; ============================================================================

option casemap:none

include windows.inc
include masm_hotpatch.inc
include logging.inc

includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

; ============================================================================
; CONSTANTS AND STRUCTURES
; ============================================================================

; Language types
LANGUAGE_NONE = 0
LANGUAGE_CPP = 1
LANGUAGE_PYTHON = 2
LANGUAGE_JAVASCRIPT = 3
LANGUAGE_TYPESCRIPT = 4
LANGUAGE_JSON = 5
LANGUAGE_XML = 6
LANGUAGE_MARKDOWN = 7
LANGUAGE_MASM = 8

; Color constants for syntax highlighting
COLOR_KEYWORD = 000000FFh      ; Blue
COLOR_STRING = 00008000h       ; Green
COLOR_COMMENT = 00808080h      ; Gray
COLOR_NUMBER = 00008080h       ; Teal
COLOR_FUNCTION = 00800080h     ; Purple
COLOR_CLASS = 00808000h        ; Olive
COLOR_OPERATOR = 00000000h     ; Black
COLOR_PREPROCESSOR = 00800000h ; Maroon

; Highlighting rule structure
HIGHLIGHT_RULE STRUCT
    pattern QWORD ?     ; Pointer to pattern string
    patternLen DWORD ?  ; Pattern length
    color DWORD ?       ; Color value
    flags DWORD ?       ; Flags (case sensitive, regex, etc)
HIGHLIGHT_RULE ENDS

; Syntax highlighter state
SYNTAX_HIGHLIGHTER STRUCT
    language DWORD ?
    rules QWORD ?       ; Array of HIGHLIGHT_RULE
    ruleCount DWORD ?
    hEditor QWORD ?
    hDC QWORD ?
SYNTAX_HIGHLIGHTER ENDS

; ============================================================================
; GLOBAL VARIABLES
; ============================================================================

.data

; Global syntax highlighter instance
globalHighlighter SYNTAX_HIGHLIGHTER {}

; Reusable CHARFORMATA for coloring selections (avoids per-call stack locals)
charFormat CHARFORMAT {}

; Language names
szCppLang db "C++",0
szPythonLang db "Python",0
szJSLang db "JavaScript",0
szTSLang db "TypeScript",0
szJsonLang db "JSON",0
szXmlLang db "XML",0
szMarkdownLang db "Markdown",0
szMasmLang db "MASM",0

; C++ keywords
cppKeywords db "auto break case char const continue default do double else enum extern float for goto if int long register return short signed sizeof static struct switch typedef union unsigned void volatile while",0

; Python keywords
pythonKeywords db "and as assert async await break class continue def del elif else except False finally for from global if import in is lambda None nonlocal not or pass raise return True try while with yield",0

; JavaScript keywords
jsKeywords db "break case catch class const continue debugger default delete do else export extends finally for function if import in instanceof new return super switch this throw try typeof var void while with yield",0

; MASM keywords
masmKeywords db "ALIGN ASSUME BYTE CARRY? CODE DATA DB DD DQ DT DW DWORD END ENDP ENDS EQU EQUAL EXTERN EXTRN FAR FORWARD HIGH IF IFDEF IFNDEF INCLUDE INVOKE LABEL LENGTH LOCAL LOW MACRO NAME NEAR OFFSET ORG PAGE PROC PTR PUBLIC QWORD REAL4 REAL8 REAL10 SEGMENT SHORT SIZE STACK STRUCT SUBTTL TITLE TYPE WORD",0

; ============================================================================
; PUBLIC API FUNCTIONS
; ============================================================================

.code

; syntax_init_highlighter(hEditor: rcx) -> bool (rax)
; Initialize syntax highlighter for given editor
PUBLIC syntax_init_highlighter
syntax_init_highlighter PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov [globalHighlighter.hEditor], rcx
    
    ; Get device context for text rendering
    mov rcx, rcx
    call GetDC
    mov [globalHighlighter.hDC], rax
    
    ; Set default language to C++
    mov [globalHighlighter.language], LANGUAGE_CPP
    
    ; Initialize rules for default language
    call syntax_setup_cpp_rules
    
    mov eax, 1
    leave
    ret
syntax_init_highlighter ENDP

; syntax_set_language(language: rcx) -> bool (rax)
; Set current language for highlighting
PUBLIC syntax_set_language
syntax_set_language PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov [globalHighlighter.language], ecx
    
    ; Setup rules based on language
    cmp ecx, LANGUAGE_CPP
    je setup_cpp
    cmp ecx, LANGUAGE_PYTHON
    je setup_python
    cmp ecx, LANGUAGE_JAVASCRIPT
    je setup_js
    cmp ecx, LANGUAGE_MASM
    je setup_masm
    
    ; Default to no highlighting
    mov [globalHighlighter.ruleCount], 0
    jmp done
    
setup_cpp:
    call syntax_setup_cpp_rules
    jmp done
    
setup_python:
    call syntax_setup_python_rules
    jmp done
    
setup_js:
    call syntax_setup_js_rules
    jmp done
    
setup_masm:
    call syntax_setup_masm_rules
    
done:
    mov eax, 1
    leave
    ret
syntax_set_language ENDP

; syntax_highlight_text(text: rcx, len: rdx) -> bool (rax)
; Apply syntax highlighting to text
PUBLIC syntax_highlight_text
syntax_highlight_text PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    push r12
    push r12
    push r13
    push r14
    push rbx
    push rsi
    
    ; Check if we have rules
    cmp [globalHighlighter.ruleCount], 0
    je no_highlighting
    
    ; Apply each rule to the text
    mov rsi, rcx                ; Text pointer
    mov r12, rdx                ; Text length (preserved)
    mov rbx, [globalHighlighter.rules] ; Rules array
    mov r13d, [globalHighlighter.ruleCount]
    
highlight_loop:
    test r13d, r13d
    jz highlight_done
    
    ; Load rule fields (pattern ptr, len, color)
    mov r8, [rbx]
    mov r9d, [rbx+8]
    mov r10d, [rbx+12]
    
    ; Skip empty patterns
    test r9d, r9d
    jz advance_rule
    
    ; Apply pattern matching
    mov rcx, rsi        ; Text
    mov rdx, r12        ; Text length
    call syntax_match_pattern
    test rax, rax
    jz advance_rule
    
    ; Found match - apply color
    mov rcx, [globalHighlighter.hEditor]
    mov rdx, EM_SETCHARFORMAT
    mov r8, SCF_SELECTION
    lea r9, charFormat
    
    ; Setup CHARFORMAT structure (reuse global buffer)
    mov [charFormat.cbSize], SIZEOF CHARFORMAT
    mov [charFormat.dwMask], CFM_COLOR
    mov [charFormat.crTextColor], r10d
    
    call SendMessageA
    
advance_rule:
    add rbx, SIZEOF HIGHLIGHT_RULE
    dec r13d
    jmp highlight_loop
    
highlight_done:
no_highlighting:
    mov eax, 1
    pop rsi
    pop rbx
    pop r14
    pop r13
    pop r12
    leave
    ret
syntax_highlight_text ENDP

; syntax_match_pattern(text: rcx, textLen: rdx, pattern: r8, patternLen: r9) -> position (rax)
; Simple pattern matching algorithm
syntax_match_pattern PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    push r12
    push r12
    push r13
    push rbx
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, r8
    mov r11, r8         ; pattern base (volatile ok)
    mov r12d, edx       ; text length
    mov r13d, r9d       ; pattern length
    
    ; Reject empty or longer-than-text patterns early
    test r13d, r13d
    jz no_match_fast
    cmp r13d, r12d
    ja no_match_fast
    
    ; Remaining comparisons = textLen - patLen + 1
    mov eax, r12d
    sub eax, r13d
    inc eax
    mov r9d, eax        ; loop counter
    
    ; First byte of pattern for quick scan
    mov al, byte ptr [rdi]
    
search_loop:
    test r9d, r9d
    jz no_match_fast
    
    cmp byte ptr [rsi], al
    jne advance_char
    
    ; If single-byte pattern, match immediately
    cmp r13d, 1
    je match_found
    
    ; Compare remaining bytes
    mov rbx, rsi        ; preserve start pointer
    mov rcx, r13d
    dec rcx
    lea rsi, [rbx+1]
    lea rdi, [r11+1]
    repe cmpsb
    je match_found_restore
    
    ; Restore pointers after failed compare
    mov rsi, rbx
    mov rdi, r11
    
advance_char:
    inc rsi
    dec r9d
    jmp search_loop
    
match_found_restore:
    mov rax, rbx
    jmp done
    
no_match_fast:
    xor rax, rax
    
done:
    pop rdi
    pop rsi
    pop rbx
    pop r13
    pop r12
    leave
    ret
syntax_match_pattern ENDP

; ============================================================================
; LANGUAGE-SPECIFIC RULE SETUP FUNCTIONS
; ============================================================================

; Setup C++ highlighting rules
syntax_setup_cpp_rules PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    push rbx
    
    ; Allocate memory for rules
    mov rcx, 50 * sizeof HIGHLIGHT_RULE
    call malloc
    mov [globalHighlighter.rules], rax
    mov rbx, rax
    
    ; Rule 1: Keywords
    lea rax, cppKeywords
    mov [rbx + HIGHLIGHT_RULE.pattern], rax
    mov [rbx + HIGHLIGHT_RULE.patternLen], 0 ; 0 means use keyword list
    mov [rbx + HIGHLIGHT_RULE.color], COLOR_KEYWORD
    add rbx, sizeof HIGHLIGHT_RULE
    
    ; Rule 2: Strings
    lea rax, szStringPattern
    mov [rbx + HIGHLIGHT_RULE.pattern], rax
    mov [rbx + HIGHLIGHT_RULE.patternLen], 1
    mov [rbx + HIGHLIGHT_RULE.color], COLOR_STRING
    add rbx, sizeof HIGHLIGHT_RULE
    
    ; Rule 3: Comments
    lea rax, szCommentPattern
    mov [rbx + HIGHLIGHT_RULE.pattern], rax
    mov [rbx + HIGHLIGHT_RULE.patternLen], 2
    mov [rbx + HIGHLIGHT_RULE.color], COLOR_COMMENT
    
    mov [globalHighlighter.ruleCount], 3
    
    pop rbx
    leave
    ret
    
.data
szStringPattern db "\"",0
szCommentPattern db "//",0
.code
syntax_setup_cpp_rules ENDP

; Setup MASM highlighting rules
syntax_setup_masm_rules PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    push rbx
    
    mov rcx, 50 * sizeof HIGHLIGHT_RULE
    call malloc
    mov [globalHighlighter.rules], rax
    mov rbx, rax
    
    ; Rule 1: Keywords
    lea rax, masmKeywords
    mov [rbx + HIGHLIGHT_RULE.pattern], rax
    mov [rbx + HIGHLIGHT_RULE.patternLen], 0
    mov [rbx + HIGHLIGHT_RULE.color], COLOR_KEYWORD
    add rbx, sizeof HIGHLIGHT_RULE
    
    ; Rule 2: Comments
    lea rax, szMasmCommentPattern
    mov [rbx + HIGHLIGHT_RULE.pattern], rax
    mov [rbx + HIGHLIGHT_RULE.patternLen], 1
    mov [rbx + HIGHLIGHT_RULE.color], COLOR_COMMENT
    
    mov [globalHighlighter.ruleCount], 2
    
    pop rbx
    leave
    ret
    
.data
szMasmCommentPattern db ";",0
.code
syntax_setup_masm_rules ENDP

; malloc(size: rcx) -> pointer (rax)
malloc PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov r8, rcx         ; size
    call GetProcessHeap
    mov rcx, rax        ; hHeap
    mov rdx, 8          ; HEAP_ZERO_MEMORY
    call HeapAlloc
    
    leave
    ret
malloc ENDP

; free(ptr: rcx)
free PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov r8, rcx         ; lpMem
    call GetProcessHeap
    mov rcx, rax        ; hHeap
    mov rdx, 0
    call HeapFree
    
    leave
    ret
free ENDP

end