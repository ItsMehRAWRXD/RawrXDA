; ============================================================================
; File 30: completions_popup.asm - Autocomplete popup with fuzzy filtering
; ============================================================================
; Purpose: Ctrl+Space trigger, fuzzy filter items, overlay window, snippet processing
; Uses: LSP completions, custom overlay window, keyboard navigation, snippet insertion
; Functions: Init, Show, Filter, Navigate, InsertSnippet, Hide
; ============================================================================

.code

; CONSTANTS
; ============================================================================

COMPLETIONS_MAX_ITEMS   equ 500
COMPLETION_ITEM_HEIGHT  equ 20
COMPLETION_POPUP_WIDTH  equ 400
POPUP_BUFFER_SIZE       equ 1048576  ; 1MB

; INITIALIZATION
; ============================================================================

CompletionsPopup_Init PROC USES rbx rcx rdx rsi rdi
    ; Returns: CompletionsPopup* in rax
    ; { hwnd, items[], itemCount, filteredItems[], filteredCount, selectedIndex, visible, mutex }
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    
    ; Allocate main struct (128 bytes)
    mov rdx, 0
    mov r8, 128
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rbx, rax  ; rbx = CompletionsPopup*
    
    ; Allocate items array (500 × 128 bytes = 64KB)
    mov r8, 65536
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rbx + 0], rax  ; items
    
    ; Allocate filtered items array (pointers, 500 × 8 bytes)
    mov r8, 4000
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rbx + 8], rax  ; filteredItems
    
    ; Initialize fields
    mov qword ptr [rbx + 16], 0     ; hwnd
    mov qword ptr [rbx + 24], 0     ; itemCount
    mov qword ptr [rbx + 32], 0     ; filteredCount
    mov qword ptr [rbx + 40], 0     ; selectedIndex
    mov qword ptr [rbx + 48], 0     ; visible
    
    ; Initialize CRITICAL_SECTION
    lea rdx, [rbx + 56]
    sub rsp, 40
    mov rcx, rdx
    call InitializeCriticalSection
    add rsp, 40
    
    mov rax, rbx
    ret
CompletionsPopup_Init ENDP

; SHOW POPUP (Triggered by Ctrl+Space)
; ============================================================================

CompletionsPopup_Show PROC USES rbx rcx rdx rsi rdi r8 r9 popup:PTR DWORD, cursorX:QWORD, cursorY:QWORD
    ; popup = CompletionsPopup*
    ; cursorX, cursorY = screen coordinates
    ; Returns: 1 if successful
    
    mov rcx, popup
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, popup
    mov r8, cursorX
    mov r9, cursorY
    
    ; Request completions from LSP
    ; Call LSPClient_GetCompletions(lspClient, filePath)
    
    ; For now, populate with dummy data
    ; In real implementation, would load from LSP response
    
    mov qword ptr [rcx + 48], 1  ; visible = true
    
    ; Create popup window (custom class)
    ; RegisterClass, CreateWindowEx, ShowWindow
    
    ; Position popup below cursor
    mov r10, r8
    mov r11, r9
    add r11, 20  ; offset below cursor
    
    ; TODO: Implement popup window creation
    
    mov rcx, popup
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 1
    ret
CompletionsPopup_Show ENDP

; FILTER ITEMS (Incremental Fuzzy Match)
; ============================================================================

CompletionsPopup_Filter PROC USES rbx rcx rdx rsi rdi r8 r9 popup:PTR DWORD, prefix:PTR BYTE
    ; popup = CompletionsPopup*
    ; prefix = user-typed text (e.g., "myFunc")
    ; Updates filteredItems array with matches
    ; Returns: filtered count in rax
    
    mov rcx, popup
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, popup
    mov rsi, prefix
    
    mov rdi, [rcx + 8]   ; filteredItems
    mov r8, [rcx + 24]   ; itemCount
    xor r9, r9           ; filtered index
    
@filter_loop:
    cmp r8, COMPLETIONS_MAX_ITEMS
    jge @filter_done
    cmp r9, COMPLETIONS_MAX_ITEMS
    jge @filter_done
    
    ; Get item at index r8
    mov r10, [rcx + 0]   ; items
    lea r11, [r10 + r8*128]  ; item at index
    
    ; Fuzzy match: check if prefix chars are in item label
    mov r12, rsi         ; r12 = prefix pointer
    mov r13b, byte ptr [r12]  ; first prefix char
    cmp r13b, 0
    je @filter_match     ; empty prefix matches all
    
    ; Search for prefix in item label
    mov r10, [r11 + 0]   ; item.label
    call CompletionsPopup_FuzzyMatch
    cmp rax, 1
    jne @filter_no_match
    
@filter_match:
    ; Add to filtered list
    mov [rdi + r9*8], r11  ; filtered item pointer
    inc r9
    
@filter_no_match:
    inc r8
    jmp @filter_loop
    
@filter_done:
    mov rcx, popup
    mov [rcx + 32], r9   ; filteredCount
    mov qword ptr [rcx + 40], 0  ; selectedIndex = 0
    
    mov rax, r9  ; return filtered count
    
    mov rcx, popup
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    ret
CompletionsPopup_Filter ENDP

; FUZZY MATCH (Case-Insensitive)
; ============================================================================

CompletionsPopup_FuzzyMatch PROC USES rbx rcx rdx rsi rdi r8 r9 itemLabel:PTR BYTE, prefix:PTR BYTE
    ; itemLabel = completion item label
    ; prefix = filter string
    ; Returns: 1 if matches, 0 if not
    
    mov rsi, prefix
    mov rdi, itemLabel
    
    xor r8, r8  ; position in itemLabel
    
@match_loop:
    mov al, byte ptr [rsi]
    cmp al, 0
    je @match_success  ; entire prefix matched
    
    ; Convert to uppercase for comparison
    cmp al, 'a'
    jl @match_upper_p
    cmp al, 'z'
    jg @match_upper_p
    sub al, 32
    
@match_upper_p:
    mov cl, byte ptr [rdi + r8]
    cmp cl, 0
    je @match_fail  ; end of itemLabel before prefix
    
    ; Convert to uppercase
    cmp cl, 'a'
    jl @match_upper_l
    cmp cl, 'z'
    jg @match_upper_l
    sub cl, 32
    
@match_upper_l:
    cmp al, cl
    je @match_next_char
    
    ; Not matching at this position, try next position in itemLabel
    inc r8
    cmp r8, 256  ; max length
    jge @match_fail
    jmp @match_loop
    
@match_next_char:
    inc rsi
    inc r8
    jmp @match_loop
    
@match_success:
    mov rax, 1
    ret
    
@match_fail:
    mov rax, 0
    ret
CompletionsPopup_FuzzyMatch ENDP

; NAVIGATE POPUP (Arrow Keys)
; ============================================================================

CompletionsPopup_Navigate PROC USES rbx rcx rdx popup:PTR DWORD, direction:DWORD
    ; popup = CompletionsPopup*
    ; direction: 0=Up, 1=Down, 2=Home, 3=End
    ; Returns: 1 if successful
    
    mov rcx, popup
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, popup
    mov eax, [direction]
    mov r8, [rcx + 40]   ; selectedIndex
    mov r9, [rcx + 32]   ; filteredCount
    
    cmp eax, 0
    je @nav_up
    cmp eax, 1
    je @nav_down
    cmp eax, 2
    je @nav_home
    cmp eax, 3
    je @nav_end
    
    jmp @nav_done
    
@nav_up:
    cmp r8, 0
    je @nav_done
    dec r8
    jmp @nav_update
    
@nav_down:
    inc r8
    cmp r8, r9
    jge @nav_clamp
    jmp @nav_update
    
@nav_home:
    mov r8, 0
    jmp @nav_update
    
@nav_end:
    mov r8, r9
    dec r8
    
@nav_clamp:
    cmp r8, r9
    jl @nav_update
    mov r8, r9
    dec r8
    
@nav_update:
    mov rcx, popup
    mov [rcx + 40], r8   ; selectedIndex
    
    ; Trigger repaint
    
@nav_done:
    mov rcx, popup
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 1
    ret
CompletionsPopup_Navigate ENDP

; INSERT SELECTED COMPLETION
; ============================================================================

CompletionsPopup_InsertSelection PROC USES rbx rcx rdx rsi rdi r8 r9 popup:PTR DWORD, editor:PTR DWORD
    ; popup = CompletionsPopup*
    ; editor = text editor context
    ; Returns: 1 if inserted
    
    mov rcx, popup
    mov r8 = [rcx + 40]   ; selectedIndex
    mov r9 = [rcx + 8]    ; filteredItems
    
    mov rsi = [r9 + r8*8]  ; selected item
    
    ; Get item text and kind
    mov rax = [rsi + 0]   ; label
    mov rdx = [rsi + 8]   ; kind (1=Text, 2=Method, 3=Function, etc.)
    mov r10 = [rsi + 16]  ; insertText or insertSnippet
    
    ; Check if snippet
    cmp rdx, 10  ; Snippet kind
    je @insert_snippet
    
    ; Insert plain text
    mov rcx = editor
    mov rdx = rax  ; label/insertText
    
    ; call TabBuffer_InsertText(editor, text)
    
    jmp @insert_done
    
@insert_snippet:
    ; Process snippet syntax: ${1:param}, ${2:param}, etc.
    mov rcx = editor
    mov rdx = r10  ; insertText with snippet syntax
    call CompletionsPopup_ProcessSnippet
    
@insert_done:
    ; Close popup
    mov rcx = popup
    mov qword ptr [rcx + 48], 0  ; visible = false
    call CompletionsPopup_Hide
    
    mov rax, 1
    ret
CompletionsPopup_InsertSelection ENDP

; PROCESS SNIPPET
; ============================================================================

CompletionsPopup_ProcessSnippet PROC USES rbx rcx rdx rsi rdi r8 r9 editor:PTR DWORD, snippet:PTR BYTE
    ; editor = text editor
    ; snippet = snippet string with ${1:param} placeholders
    ; Expands snippet and sets up cursor positions
    ; Returns: 1 if processed
    
    mov rsi = snippet
    mov rdi = 0  ; output position
    xor r8, r8   ; placeholder count
    
@snippet_loop:
    mov al = byte ptr [rsi]
    cmp al, 0
    je @snippet_done
    
    cmp al, '$'
    jne @snippet_copy
    
    ; Check for ${N:
    mov bl = byte ptr [rsi + 1]
    cmp bl, '{'
    jne @snippet_copy
    
    ; Parse ${N:param}
    ; Extract N and param
    mov ecx = 0
    mov r9b = byte ptr [rsi + 2]
    cmp r9b, '0'
    jl @snippet_copy
    cmp r9b, '9'
    jg @snippet_copy
    
    mov ecx = r9 - '0'  ; placeholder number
    
    ; Find closing }
    mov r10 = 3
@find_close:
    mov r9b = byte ptr [rsi + r10]
    cmp r9b, 0
    je @snippet_copy
    cmp r9b, '}'
    je @close_found
    inc r10
    jmp @find_close
    
@close_found:
    ; Extract param text between : and }
    mov r11 = 4  ; start of param (after ${N:)
    mov r12 = r10
    sub r12, r11  ; param length
    
    ; Insert param into editor
    ; (would call TabBuffer_InsertText)
    
    ; Track placeholder position for later cursor jumping
    inc r8
    
    add rsi, r10
    inc rsi  ; skip past }
    jmp @snippet_loop
    
@snippet_copy:
    ; Copy character as-is
    mov [rdi], al
    inc rsi
    inc rdi
    jmp @snippet_loop
    
@snippet_done:
    mov rax, 1
    ret
CompletionsPopup_ProcessSnippet ENDP

; HIDE POPUP
; ============================================================================

CompletionsPopup_Hide PROC USES rbx rcx rdx popup:PTR DWORD
    ; Hide and clean up popup
    
    mov rcx = popup
    lea rdx = [rcx + 56]
    sub rsp, 40
    mov rcx = rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx = popup
    mov qword ptr [rcx + 48], 0  ; visible = false
    mov qword ptr [rcx + 24], 0  ; itemCount = 0
    mov qword ptr [rcx + 32], 0  ; filteredCount = 0
    
    ; DestroyWindow if hwnd exists
    
    mov rcx = popup
    lea rdx = [rcx + 56]
    sub rsp, 40
    mov rcx = rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 1
    ret
CompletionsPopup_Hide ENDP

; COMPLETION ITEM STRUCTURE
; ============================================================================
; {
;   label[64],       ; "myFunction"
;   kind[4],         ; 1-20 (Text, Method, Function, Variable, Class, Interface, Module, Property, Unit, Value, Enum, Keyword, Snippet, Color, File, Reference, Folder, EnumMember, Constant)
;   insertText[64],  ; what to insert (may include ${1:param} syntax)
;   documentation[256],  ; tooltip
; }

end
