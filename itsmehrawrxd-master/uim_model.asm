; uim_model.asm
; Universal Intermediate Model (UIM) implementation in assembly
; Defines data structures for cross-language code generation

section .data
    ; UIM node types
    %define UIM_NODE_SYMBOL       1
    %define UIM_NODE_FUNCTION     2
    %define UIM_NODE_MODULE       3
    %define UIM_NODE_PROJECT      4
    %define UIM_NODE_PARAMETER    5
    %define UIM_NODE_TYPE         6

    ; UIM node structure (64 bytes)
    %define UIM_NODE_TYPE_FIELD       0
    %define UIM_NODE_NAME_PTR         8
    %define UIM_NODE_TYPE_NAME_PTR    16
    %define UIM_NODE_RETURN_TYPE_PTR  24
    %define UIM_NODE_CHILDREN_PTR     32
    %define UIM_NODE_NEXT_PTR         40
    %define UIM_NODE_DATA_PTR         48
    %define UIM_NODE_SIZE             56

    ; String constants
    string_type        db "String", 0
    int32_type         db "int32", 0
    int64_type         db "int64", 0
    float_type         db "float", 0
    double_type        db "double", 0
    bool_type          db "bool", 0
    void_type          db "void", 0

section .bss
    uim_root_node      resq 1      ; Root of UIM tree
    uim_node_count     resq 1      ; Total number of nodes
    uim_string_pool    resb 65536  ; String pool for names and types
    uim_string_pos     resq 1      ; Current position in string pool

section .text
    global uim_init
    global uim_create_symbol
    global uim_create_function
    global uim_create_module
    global uim_create_project
    global uim_add_child
    global uim_find_node
    global uim_get_node_type
    global uim_get_node_name
    global uim_cleanup

; Initialize UIM system
uim_init:
    mov     qword [uim_root_node], 0
    mov     qword [uim_node_count], 0
    mov     qword [uim_string_pos], 0
    ret

; Create a new UIM node
; Args: rdi = node type, rsi = name pointer, rdx = type name pointer
; Returns: rax = pointer to new node
uim_create_node:
    push    rbx
    push    rcx
    push    rdx
    
    ; Allocate memory for node
    mov     rax, UIM_NODE_SIZE
    call    allocate_memory
    
    ; Initialize node
    mov     qword [rax + UIM_NODE_TYPE_FIELD], rdi
    mov     qword [rax + UIM_NODE_NAME_PTR], rsi
    mov     qword [rax + UIM_NODE_TYPE_NAME_PTR], rdx
    mov     qword [rax + UIM_NODE_RETURN_TYPE_PTR], 0
    mov     qword [rax + UIM_NODE_CHILDREN_PTR], 0
    mov     qword [rax + UIM_NODE_NEXT_PTR], 0
    mov     qword [rax + UIM_NODE_DATA_PTR], 0
    
    ; Increment node count
    inc     qword [uim_node_count]
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Create a symbol node
; Args: rdi = name pointer, rsi = type name pointer
; Returns: rax = pointer to symbol node
uim_create_symbol:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rdi  ; name
    mov     rcx, rsi  ; type
    
    mov     rdi, UIM_NODE_SYMBOL
    mov     rsi, rbx
    mov     rdx, rcx
    call    uim_create_node
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Create a function node
; Args: rdi = name pointer, rsi = return type pointer
; Returns: rax = pointer to function node
uim_create_function:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rdi  ; name
    mov     rcx, rsi  ; return type
    
    mov     rdi, UIM_NODE_FUNCTION
    mov     rsi, rbx
    mov     rdx, 0    ; function type name
    call    uim_create_node
    
    ; Set return type
    mov     qword [rax + UIM_NODE_RETURN_TYPE_PTR], rcx
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Create a module node
; Args: rdi = name pointer
; Returns: rax = pointer to module node
uim_create_module:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rdi  ; name
    
    mov     rdi, UIM_NODE_MODULE
    mov     rsi, rbx
    mov     rdx, 0    ; module type name
    call    uim_create_node
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Create a project node
; Args: rdi = name pointer
; Returns: rax = pointer to project node
uim_create_project:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rdi  ; name
    
    mov     rdi, UIM_NODE_PROJECT
    mov     rsi, rbx
    mov     rdx, 0    ; project type name
    call    uim_create_node
    
    ; Set as root if first project
    cmp     qword [uim_root_node], 0
    jne     .not_root
    mov     qword [uim_root_node], rax
    
.not_root:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Add child node to parent
; Args: rdi = parent node, rsi = child node
uim_add_child:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rdi  ; parent
    mov     rcx, rsi  ; child
    
    ; Get current children
    mov     rdx, qword [rbx + UIM_NODE_CHILDREN_PTR]
    cmp     rdx, 0
    je      .set_first_child
    
    ; Find last child
.find_last_child:
    mov     rdi, qword [rdx + UIM_NODE_NEXT_PTR]
    cmp     rdi, 0
    je      .set_next
    mov     rdx, rdi
    jmp     .find_last_child
    
.set_next:
    mov     qword [rdx + UIM_NODE_NEXT_PTR], rcx
    jmp     .done
    
.set_first_child:
    mov     qword [rbx + UIM_NODE_CHILDREN_PTR], rcx
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Find node by name in tree
; Args: rdi = root node, rsi = name pointer
; Returns: rax = found node or 0
uim_find_node:
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    
    mov     rbx, rdi  ; current node
    mov     rcx, rsi  ; name to find
    
    cmp     rbx, 0
    je      .not_found
    
    ; Check current node
    mov     rdx, qword [rbx + UIM_NODE_NAME_PTR]
    cmp     rdx, 0
    je      .check_children
    
    ; Compare names
    push    rbx
    push    rcx
    mov     rdi, rdx
    mov     rsi, rcx
    call    compare_strings
    pop     rcx
    pop     rbx
    
    cmp     rax, 1
    je      .found
    
.check_children:
    ; Check children
    mov     rdx, qword [rbx + UIM_NODE_CHILDREN_PTR]
    cmp     rdx, 0
    je      .check_siblings
    
    push    rbx
    push    rcx
    mov     rdi, rdx
    mov     rsi, rcx
    call    uim_find_node
    pop     rcx
    pop     rbx
    
    cmp     rax, 0
    jne     .found
    
.check_siblings:
    ; Check siblings
    mov     rdx, qword [rbx + UIM_NODE_NEXT_PTR]
    cmp     rdx, 0
    je      .not_found
    
    push    rbx
    push    rcx
    mov     rdi, rdx
    mov     rsi, rcx
    call    uim_find_node
    pop     rcx
    pop     rbx
    
    cmp     rax, 0
    jne     .found
    
.not_found:
    mov     rax, 0
    jmp     .done
    
.found:
    mov     rax, rbx
    
.done:
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Get node type
; Args: rdi = node pointer
; Returns: rax = node type
uim_get_node_type:
    mov     rax, qword [rdi + UIM_NODE_TYPE_FIELD]
    ret

; Get node name
; Args: rdi = node pointer
; Returns: rax = name pointer
uim_get_node_name:
    mov     rax, qword [rdi + UIM_NODE_NAME_PTR]
    ret

; Add string to string pool
; Args: rdi = string pointer
; Returns: rax = pointer to string in pool
uim_add_string:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rdi  ; source string
    mov     rcx, qword [uim_string_pos]  ; current position
    
    ; Copy string to pool
.string_copy_loop:
    mov     al, byte [rbx]
    mov     byte [uim_string_pool + rcx], al
    cmp     al, 0
    je      .string_copy_done
    inc     rbx
    inc     rcx
    jmp     .string_copy_loop
    
.string_copy_done:
    ; Return pointer to string in pool
    mov     rax, uim_string_pool
    add     rax, qword [uim_string_pos]
    
    ; Update position
    mov     qword [uim_string_pos], rcx
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Compare strings
; Args: rdi = string1, rsi = string2
; Returns: rax = 1 if equal, 0 if not
compare_strings:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rdi  ; string1
    mov     rcx, rsi  ; string2
    
.compare_loop:
    mov     al, byte [rbx]
    mov     dl, byte [rcx]
    
    cmp     al, dl
    jne     .not_equal
    
    cmp     al, 0
    je      .equal
    
    inc     rbx
    inc     rcx
    jmp     .compare_loop
    
.equal:
    mov     rax, 1
    jmp     .done
    
.not_equal:
    mov     rax, 0
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Allocate memory (placeholder - should be implemented)
allocate_memory:
    ; This should be implemented with proper memory allocation
    ; For now, return a dummy pointer
    mov     rax, 0x2000
    ret

; Cleanup UIM system
uim_cleanup:
    push    rbx
    push    rcx
    
    ; Free all nodes
    mov     rbx, qword [uim_root_node]
    cmp     rbx, 0
    je      .cleanup_done
    
    call    free_uim_tree
    mov     qword [uim_root_node], 0
    
.cleanup_done:
    mov     qword [uim_node_count], 0
    mov     qword [uim_string_pos], 0
    
    pop     rcx
    pop     rbx
    ret

; Free UIM tree recursively
; Args: rbx = node to free
free_uim_tree:
    push    rbx
    push    rcx
    push    rdx
    
    ; Free children
    mov     rcx, qword [rbx + UIM_NODE_CHILDREN_PTR]
    cmp     rcx, 0
    je      .free_siblings
    
    call    free_uim_tree
    
.free_siblings:
    ; Free siblings
    mov     rcx, qword [rbx + UIM_NODE_NEXT_PTR]
    cmp     rcx, 0
    je      .free_current
    
    push    rbx
    mov     rbx, rcx
    call    free_uim_tree
    pop     rbx
    
.free_current:
    ; Free current node
    mov     rdi, rbx
    call    free_memory
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Free memory (placeholder - should be implemented)
free_memory:
    ; This should be implemented with proper memory deallocation
    ret
