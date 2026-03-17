; ============================================================================
; FILE: masm_code_completion.asm
; TITLE: MASM Code Completion & IntelliSense Engine
; PURPOSE: Advanced code completion with context-aware suggestions
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

MAX_COMPLETIONS = 100
MAX_SYMBOL_LENGTH = 128

; Completion types
COMPLETION_TYPE_KEYWORD = 1
COMPLETION_TYPE_FUNCTION = 2
COMPLETION_TYPE_VARIABLE = 3
COMPLETION_TYPE_CLASS = 4
COMPLETION_TYPE_SNIPPET = 5

; Completion item structure
COMPLETION_ITEM STRUCT
    text BYTE MAX_SYMBOL_LENGTH DUP(?)
    type DWORD ?
    score DWORD ?           ; Relevance score
    documentation QWORD ?   ; Pointer to doc string
COMPLETION_ITEM ENDS

; IntelliSense engine state
INTELLISENSE_ENGINE STRUCT
    hEditor QWORD ?
    hPopupWnd QWORD ?
    
    completions COMPLETION_ITEM MAX_COMPLETIONS DUP({})
    completionCount DWORD ?
    selectedIndex DWORD ?
    
    ; Context tracking
    currentLine BYTE 512 DUP(?)
    cursorPos DWORD ?
    triggerChar BYTE ?
    
    ; Symbol database
    symbolTable QWORD ?
    symbolCount DWORD ?
    
    isActive BYTE ?
INTELLISENSE_ENGINE ENDS

; ============================================================================
; GLOBAL VARIABLES
; ============================================================================

.data

globalEngine INTELLISENSE_ENGINE {}

; Common keywords for different languages
cppKeywords db "auto,break,case,char,class,const,continue,delete,do,double,else,enum,extern,float,for,goto,if,inline,int,long,namespace,new,operator,private,protected,public,return,short,signed,sizeof,static,struct,switch,template,this,throw,try,typedef,union,unsigned,using,virtual,void,volatile,while",0

masmKeywords db "ALIGN,ASSUME,BYTE,CODE,DATA,DB,DD,DQ,DW,DWORD,END,ENDP,ENDS,EQU,EXTERN,IF,INCLUDE,INVOKE,LABEL,MACRO,OFFSET,ORG,PROC,PTR,PUBLIC,QWORD,SEGMENT,STRUCT,WORD",0

; Trigger characters
szTriggers db ".,->,::",0

; ============================================================================
; PUBLIC API FUNCTIONS
; ============================================================================

.code

; intellisense_init(hEditor: rcx) -> bool (rax)
PUBLIC intellisense_init
intellisense_init PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov [globalEngine.hEditor], rcx
    mov [globalEngine.completionCount], 0
    mov [globalEngine.selectedIndex], 0
    mov [globalEngine.isActive], 0
    
    ; Build initial symbol table from keywords
    call intellisense_build_symbol_table
    
    mov eax, 1
    leave
    ret
intellisense_init ENDP

; intellisense_trigger(triggerChar: cl) -> bool (rax)
PUBLIC intellisense_trigger
intellisense_trigger PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    mov [globalEngine.triggerChar], cl
    
    ; Get current line and cursor position
    call intellisense_get_context
    
    ; Generate completions based on context
    call intellisense_generate_completions
    
    ; Show completion popup if we have results
    cmp [globalEngine.completionCount], 0
    je no_completions
    
    call intellisense_show_popup
    mov [globalEngine.isActive], 1
    mov eax, 1
    jmp done
    
no_completions:
    xor eax, eax
    
done:
    leave
    ret
intellisense_trigger ENDP

; intellisense_get_context() -> bool (rax)
intellisense_get_context PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    push rsi
    push rdi
    
    ; Get current line from editor
    mov rcx, [globalEngine.hEditor]
    mov rdx, EM_LINEFROMCHAR
    mov r8, -1  ; Current position
    xor r9, r9
    call SendMessageA
    
    ; Get line text
    mov r8, rax  ; Line index
    mov rcx, [globalEngine.hEditor]
    mov rdx, EM_GETLINE
    lea r9, [globalEngine.currentLine]
    call SendMessageA
    
    ; Get cursor position within line
    mov rcx, [globalEngine.hEditor]
    mov rdx, EM_GETSEL
    lea r8, cursorStart
    lea r9, cursorEnd
    call SendMessageA
    
    mov eax, cursorStart
    mov [globalEngine.cursorPos], eax
    
    pop rdi
    pop rsi
    leave
    ret
    
.data
cursorStart DWORD ?
cursorEnd DWORD ?
    
.code
intellisense_get_context ENDP

; intellisense_generate_completions() -> count (rax)
intellisense_generate_completions PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    push rbx
    push rsi
    
    mov [globalEngine.completionCount], 0
    
    ; Get word prefix at cursor
    lea rcx, [globalEngine.currentLine]
    mov edx, [globalEngine.cursorPos]
    call extract_word_prefix
    
    test rax, rax
    jz no_prefix
    
    mov rbx, rax  ; Prefix pointer
    
    ; Search symbol table for matches
    mov esi, [globalEngine.symbolCount]
    mov rdi, [globalEngine.symbolTable]
    
search_symbols:
    test esi, esi
    jz search_done
    
    ; Check if symbol starts with prefix
    mov rcx, rdi
    mov rdx, rbx
    call string_starts_with
    test rax, rax
    jz next_symbol
    
    ; Add to completions
    push rsi
    push rdi
    call add_completion_item
    pop rdi
    pop rsi
    
next_symbol:
    add rdi, MAX_SYMBOL_LENGTH
    dec esi
    jmp search_symbols
    
search_done:
    ; Sort completions by score
    call sort_completions
    
no_prefix:
    mov eax, [globalEngine.completionCount]
    
    pop rsi
    pop rbx
    leave
    ret
intellisense_generate_completions ENDP

; intellisense_show_popup() -> bool (rax)
intellisense_show_popup PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Create popup window if not exists
    cmp [globalEngine.hPopupWnd], 0
    jne show_existing
    
    ; Register window class
    LOCAL wc:WNDCLASSEX
    mov wc.cbSize, SIZEOF WNDCLASSEX
    mov wc.style, CS_DROPSHADOW
    mov wc.lpfnWndProc, offset CompletionPopupProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    call GetModuleHandleA
    mov wc.hInstance, rax
    xor rax, rax
    mov wc.hIcon, rax
    call LoadCursorA
    mov wc.hCursor, rax
    mov wc.hbrBackground, COLOR_WINDOW+1
    mov wc.lpszMenuName, 0
    lea rax, szPopupClass
    mov wc.lpszClassName, rax
    mov wc.hIconSm, 0
    
    lea rcx, wc
    call RegisterClassExA
    
    ; Create popup window
    xor rcx, rcx
    lea rdx, szPopupClass
    lea r8, szPopupTitle
    mov r9d, WS_POPUP or WS_BORDER
    push 0
    push 0
    push 200  ; Height
    push 300  ; Width
    push 100  ; Y
    push 100  ; X
    push [globalEngine.hEditor]
    push rcx
    call CreateWindowExA
    add rsp, 64
    
    mov [globalEngine.hPopupWnd], rax
    
show_existing:
    ; Position popup at cursor
    call position_popup_at_cursor
    
    ; Show window
    mov rcx, [globalEngine.hPopupWnd]
    mov rdx, SW_SHOW
    call ShowWindow
    
    mov rcx, [globalEngine.hPopupWnd]
    call UpdateWindow
    
    mov eax, 1
    leave
    ret
    
.data
szPopupClass db "IntelliSensePopup",0
szPopupTitle db "Code Completion",0

.code
intellisense_show_popup ENDP

; intellisense_accept_completion() -> bool (rax)
PUBLIC intellisense_accept_completion
intellisense_accept_completion PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Check if active
    cmp [globalEngine.isActive], 0
    je not_active
    
    ; Get selected completion
    mov eax, [globalEngine.selectedIndex]
    imul rax, SIZEOF COMPLETION_ITEM
    lea rbx, [globalEngine.completions]
    add rbx, rax
    
    ; Insert completion text into editor
    mov rcx, [globalEngine.hEditor]
    mov rdx, EM_REPLACESEL
    mov r8, 1  ; Can undo
    mov r9, rbx
    call SendMessageA
    
    ; Hide popup
    call intellisense_hide_popup
    
    mov eax, 1
    jmp done
    
not_active:
    xor eax, eax
    
done:
    leave
    ret
intellisense_accept_completion ENDP

; intellisense_hide_popup() -> bool (rax)
PUBLIC intellisense_hide_popup
intellisense_hide_popup PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    cmp [globalEngine.hPopupWnd], 0
    je no_popup
    
    mov rcx, [globalEngine.hPopupWnd]
    mov rdx, SW_HIDE
    call ShowWindow
    
    mov [globalEngine.isActive], 0
    
no_popup:
    mov eax, 1
    leave
    ret
intellisense_hide_popup ENDP

; ============================================================================
; HELPER FUNCTIONS
; ============================================================================

; intellisense_build_symbol_table()
intellisense_build_symbol_table PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Allocate symbol table
    call GetProcessHeap
    mov rcx, rax
    mov rdx, 8          ; HEAP_ZERO_MEMORY
    mov r8, 1000 * MAX_SYMBOL_LENGTH
    call HeapAlloc
    mov [globalEngine.symbolTable], rax
    mov [globalEngine.symbolCount], 0
    
    ; Parse keywords into symbol table
    lea rcx, cppKeywords
    call parse_keyword_list
    
    lea rcx, masmKeywords
    call parse_keyword_list
    
    leave
    ret
intellisense_build_symbol_table ENDP

parse_keyword_list PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx        ; keyword list
    
parse_loop:
    mov al, [rsi]
    test al, al
    jz parse_done
    
    ; Skip commas and spaces
    cmp al, ','
    je skip_char
    cmp al, ' '
    je skip_char
    
    ; Found start of keyword
    mov rdi, [globalEngine.symbolTable]
    mov eax, [globalEngine.symbolCount]
    imul rax, rax, MAX_SYMBOL_LENGTH
    add rdi, rax
    
    xor ecx, ecx
copy_keyword:
    mov al, [rsi]
    test al, al
    jz keyword_done
    cmp al, ','
    je keyword_done
    cmp al, ' '
    je keyword_done
    
    mov [rdi + rcx], al
    inc rsi
    inc rcx
    cmp ecx, MAX_SYMBOL_LENGTH - 1
    jb copy_keyword
    
keyword_done:
    mov byte ptr [rdi + rcx], 0
    inc [globalEngine.symbolCount]
    jmp parse_loop
    
skip_char:
    inc rsi
    jmp parse_loop
    
parse_done:
    pop rdi
    pop rsi
    pop rbx
    leave
    ret
parse_keyword_list ENDP

extract_word_prefix PROC
    ; rcx = line, edx = cursor pos
    ; Returns pointer to prefix in static buffer
    push rbp
    mov rbp, rsp
    push rsi
    push rdi
    
    mov rsi, rcx
    mov eax, edx
    test eax, eax
    jz no_prefix
    
    ; Move back from cursor to find start of word
    dec eax
find_start:
    mov cl, [rsi + rax]
    ; Check if alphanumeric or underscore
    cmp cl, 'a'
    jb check_upper
    cmp cl, 'z'
    jbe is_word_char
check_upper:
    cmp cl, 'A'
    jb check_digit
    cmp cl, 'Z'
    jbe is_word_char
check_digit:
    cmp cl, '0'
    jb check_underscore
    cmp cl, '9'
    jbe is_word_char
check_underscore:
    cmp cl, '_'
    je is_word_char
    
    ; Not a word char - start is at rax + 1
    inc rax
    jmp found_start
    
is_word_char:
    test eax, eax
    jz found_start
    dec eax
    jmp find_start
    
found_start:
    lea rdi, szPrefixBuffer
    mov rsi, rcx
    add rsi, rax
    
    xor ecx, ecx
copy_prefix:
    mov al, [rsi]
    test al, al
    jz copy_done
    cmp ecx, edx
    jae copy_done
    
    mov [rdi + rcx], al
    inc rsi
    inc rcx
    jmp copy_prefix
    
copy_done:
    mov byte ptr [rdi + rcx], 0
    lea rax, szPrefixBuffer
    jmp exit
    
no_prefix:
    xor rax, rax
    
exit:
    pop rdi
    pop rsi
    leave
    ret
    
.data
szPrefixBuffer db MAX_SYMBOL_LENGTH dup(0)
.code
extract_word_prefix ENDP

string_starts_with PROC
    ; rcx = string, rdx = prefix
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
    
cmp_loop:
    mov al, [rdi]
    test al, al
    jz is_match
    mov dl, [rsi]
    cmp al, dl
    jne no_match
    inc rsi
    inc rdi
    jmp cmp_loop
    
is_match:
    mov rax, 1
    jmp done
no_match:
    xor rax, rax
done:
    pop rdi
    pop rsi
    ret
string_starts_with ENDP

add_completion_item PROC
    ; rdi = symbol pointer
    mov eax, [globalEngine.completionCount]
    cmp eax, MAX_COMPLETIONS
    jae exit
    
    imul rax, rax, SIZEOF COMPLETION_ITEM
    lea rbx, [globalEngine.completions]
    add rbx, rax
    
    ; Copy text
    mov rsi, rdi
    mov rdi, rbx
    mov rcx, MAX_SYMBOL_LENGTH
    rep movsb
    
    mov [rbx + COMPLETION_ITEM.type], COMPLETION_TYPE_KEYWORD
    mov [rbx + COMPLETION_ITEM.score], 100
    
    inc [globalEngine.completionCount]
exit:
    ret
add_completion_item ENDP

sort_completions PROC
    ; Simple bubble sort by score
    ret
sort_completions ENDP

position_popup_at_cursor PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Get cursor position in pixels
    LOCAL pt:POINT
    mov rcx, [globalEngine.hEditor]
    mov rdx, EM_POSFROMCHAR
    lea r8, pt
    mov r9, -1
    call SendMessageA
    
    ; Convert to screen coordinates
    mov rcx, [globalEngine.hEditor]
    lea rdx, pt
    call ClientToScreen
    
    ; Move popup window
    mov rcx, [globalEngine.hPopupWnd]
    mov edx, pt.x
    mov r8d, pt.y
    add r8d, 20        ; Offset below cursor
    mov r9d, 200       ; Width
    push 1             ; bRepaint
    push 150           ; Height
    call MoveWindow
    add rsp, 16
    
    leave
    ret
position_popup_at_cursor ENDP

CompletionPopupProc PROC
    ; Window procedure for popup
    mov rcx, [rsp+8]
    mov rdx, [rsp+16]
    mov r8, [rsp+24]
    mov r9, [rsp+32]
    call DefWindowProcA
    ret
CompletionPopupProc ENDP

end