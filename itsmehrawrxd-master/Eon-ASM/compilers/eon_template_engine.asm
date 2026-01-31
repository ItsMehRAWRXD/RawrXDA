; eon_template_engine.asm
; Template engine for processing template AST and generating Eon source code
; Handles data binding, control flow, and code generation

section .data
    ; Data types for template data
    %define DATA_TYPE_OBJECT       1
    %define DATA_TYPE_ARRAY        2
    %define DATA_TYPE_STRING       3
    %define DATA_TYPE_NUMBER       4
    %define DATA_TYPE_BOOLEAN      5

    ; Template data structure
    %define DATA_NODE_TYPE         0
    %define DATA_NODE_VALUE        8
    %define DATA_NODE_CHILDREN     16
    %define DATA_NODE_NEXT         24
    %define DATA_NODE_SIZE         32

    ; Output buffer
    output_buffer         resb 65536  ; 64KB output buffer
    output_pos            resq 1      ; Current position in output buffer
    output_size           resq 1      ; Size of output buffer

section .bss
    template_data_root    resq 1      ; Root of template data
    current_scope         resq 1      ; Current data scope
    scope_stack           resq 100    ; Stack for nested scopes
    scope_stack_top       resq 1      ; Top of scope stack

section .text
    global template_engine_init
    global template_engine_set_data
    global template_engine_generate
    global template_engine_get_output
    global template_engine_cleanup

; Initialize template engine
template_engine_init:
    mov     qword [template_data_root], 0
    mov     qword [current_scope], 0
    mov     qword [scope_stack_top], 0
    mov     qword [output_pos], 0
    mov     qword [output_size], 65536
    ret

; Set template data
; Args: rdi = data root node
template_engine_set_data:
    mov     qword [template_data_root], rdi
    mov     qword [current_scope], rdi
    ret

; Generate Eon source from template AST
; Args: rdi = template AST root
template_engine_generate:
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    
    ; Reset output buffer
    mov     qword [output_pos], 0
    
    ; Generate from AST
    call    generate_from_ast
    
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Generate code from AST node
; Args: rdi = AST node
generate_from_ast:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rdi
    mov     rcx, qword [rbx + TEMPLATE_NODE_TYPE]
    
    ; Process based on node type
    cmp     rcx, TEMPLATE_AST_TEXT
    je      .process_text
    
    cmp     rcx, TEMPLATE_AST_PLACEHOLDER
    je      .process_placeholder
    
    cmp     rcx, TEMPLATE_AST_IF
    je      .process_if
    
    cmp     rcx, TEMPLATE_AST_ELSE
    je      .process_else
    
    cmp     rcx, TEMPLATE_AST_FOR
    je      .process_for
    
    cmp     rcx, TEMPLATE_AST_EACH
    je      .process_each
    
    jmp     .process_children
    
.process_text:
    call    process_text_node
    jmp     .process_children
    
.process_placeholder:
    call    process_placeholder_node
    jmp     .process_children
    
.process_if:
    call    process_if_node
    jmp     .done
    
.process_else:
    call    process_else_node
    jmp     .done
    
.process_for:
    call    process_for_node
    jmp     .done
    
.process_each:
    call    process_each_node
    jmp     .done
    
.process_children:
    ; Process children
    mov     rcx, qword [rbx + TEMPLATE_NODE_CHILDREN]
    cmp     rcx, 0
    je      .process_siblings
    
    call    generate_from_ast
    
.process_siblings:
    ; Process siblings
    mov     rcx, qword [rbx + TEMPLATE_NODE_NEXT]
    cmp     rcx, 0
    je      .done
    
    mov     rdi, rcx
    call    generate_from_ast
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Process text node
; Args: rbx = text node
process_text_node:
    push    rbx
    push    rcx
    push    rdx
    
    ; Get text value
    mov     rcx, qword [rbx + TEMPLATE_NODE_VALUE]
    cmp     rcx, 0
    je      .done
    
    ; Copy text to output buffer
    call    copy_text_to_output
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Process placeholder node
; Args: rbx = placeholder node
process_placeholder_node:
    push    rbx
    push    rcx
    push    rdx
    
    ; Get placeholder expression
    mov     rcx, qword [rbx + TEMPLATE_NODE_VALUE]
    cmp     rcx, 0
    je      .done
    
    ; Evaluate expression and output result
    call    evaluate_expression
    call    output_evaluated_value
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Process if node
; Args: rbx = if node
process_if_node:
    push    rbx
    push    rcx
    push    rdx
    
    ; Get condition
    mov     rcx, qword [rbx + TEMPLATE_NODE_CONDITION]
    cmp     rcx, 0
    je      .done
    
    ; Evaluate condition
    call    evaluate_condition
    cmp     rax, 1
    jne     .check_else
    
    ; Process if body
    mov     rcx, qword [rbx + TEMPLATE_NODE_CHILDREN]
    cmp     rcx, 0
    je      .check_else
    
    mov     rdi, rcx
    call    generate_from_ast
    
.check_else:
    ; Check for else clause
    mov     rcx, qword [rbx + TEMPLATE_NODE_NEXT]
    cmp     rcx, 0
    je      .done
    
    mov     rdx, qword [rcx + TEMPLATE_NODE_TYPE]
    cmp     rdx, TEMPLATE_AST_ELSE
    jne     .done
    
    ; Process else body
    mov     rcx, qword [rcx + TEMPLATE_NODE_CHILDREN]
    cmp     rcx, 0
    je      .done
    
    mov     rdi, rcx
    call    generate_from_ast
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Process else node
; Args: rbx = else node
process_else_node:
    ; Else nodes are handled by if nodes
    ret

; Process for node
; Args: rbx = for node
process_for_node:
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    
    ; Get iterator expression
    mov     rcx, qword [rbx + TEMPLATE_NODE_ITERATOR]
    cmp     rcx, 0
    je      .done
    
    ; Evaluate iterator to get array
    call    evaluate_expression
    mov     rsi, rax
    
    ; Check if it's an array
    mov     rdx, qword [rsi + DATA_NODE_TYPE]
    cmp     rdx, DATA_TYPE_ARRAY
    jne     .done
    
    ; Iterate through array
    mov     rcx, qword [rsi + DATA_NODE_CHILDREN]
    cmp     rcx, 0
    je      .done
    
.for_loop:
    ; Push current scope
    call    push_scope
    
    ; Set current item as scope
    mov     qword [current_scope], rcx
    
    ; Process for body
    mov     rdx, qword [rbx + TEMPLATE_NODE_CHILDREN]
    cmp     rdx, 0
    je      .next_iteration
    
    push    rcx
    mov     rdi, rdx
    call    generate_from_ast
    pop     rcx
    
.next_iteration:
    ; Pop scope
    call    pop_scope
    
    ; Move to next item
    mov     rcx, qword [rcx + DATA_NODE_NEXT]
    cmp     rcx, 0
    jne     .for_loop
    
.done:
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Process each node
; Args: rbx = each node
process_each_node:
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    
    ; Get iterator expression
    mov     rcx, qword [rbx + TEMPLATE_NODE_ITERATOR]
    cmp     rcx, 0
    je      .done
    
    ; Evaluate iterator to get array
    call    evaluate_expression
    mov     rsi, rax
    
    ; Check if it's an array
    mov     rdx, qword [rsi + DATA_NODE_TYPE]
    cmp     rdx, DATA_TYPE_ARRAY
    jne     .done
    
    ; Iterate through array
    mov     rcx, qword [rsi + DATA_NODE_CHILDREN]
    cmp     rcx, 0
    je      .done
    
.each_loop:
    ; Push current scope
    call    push_scope
    
    ; Set current item as scope
    mov     qword [current_scope], rcx
    
    ; Process each body
    mov     rdx, qword [rbx + TEMPLATE_NODE_CHILDREN]
    cmp     rdx, 0
    je      .next_iteration
    
    push    rcx
    mov     rdi, rdx
    call    generate_from_ast
    pop     rcx
    
.next_iteration:
    ; Pop scope
    call    pop_scope
    
    ; Move to next item
    mov     rcx, qword [rcx + DATA_NODE_NEXT]
    cmp     rcx, 0
    jne     .each_loop
    
.done:
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Copy text to output buffer
; Args: rcx = text pointer
copy_text_to_output:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, qword [output_pos]
    mov     rdx, qword [output_size]
    
.text_copy_loop:
    ; Check if we have space
    cmp     rbx, rdx
    jge     .text_copy_done
    
    ; Copy character
    mov     al, byte [rcx]
    cmp     al, 0
    je      .text_copy_done
    
    mov     byte [output_buffer + rbx], al
    inc     rbx
    inc     rcx
    jmp     .text_copy_loop
    
.text_copy_done:
    mov     qword [output_pos], rbx
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Evaluate expression
; Args: rcx = expression pointer
; Returns: rax = data node
evaluate_expression:
    push    rbx
    push    rcx
    push    rdx
    
    ; Simple expression evaluation
    ; For now, just look up variable in current scope
    call    lookup_variable
    mov     rax, rbx
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Look up variable in current scope
; Args: rcx = variable name
; Returns: rbx = data node
lookup_variable:
    push    rcx
    push    rdx
    
    mov     rbx, qword [current_scope]
    cmp     rbx, 0
    je      .not_found
    
    ; Search in current scope
    call    search_in_scope
    cmp     rax, 0
    jne     .found
    
    ; Search in parent scopes
    call    search_parent_scopes
    cmp     rax, 0
    jne     .found
    
.not_found:
    mov     rbx, 0
    jmp     .done
    
.found:
    mov     rbx, rax
    
.done:
    pop     rdx
    pop     rcx
    ret

; Search for variable in scope
; Args: rbx = scope node, rcx = variable name
; Returns: rax = data node
search_in_scope:
    push    rbx
    push    rcx
    push    rdx
    
    ; Check if scope is an object
    mov     rdx, qword [rbx + DATA_NODE_TYPE]
    cmp     rdx, DATA_TYPE_OBJECT
    jne     .not_found
    
    ; Search children
    mov     rbx, qword [rbx + DATA_NODE_CHILDREN]
    cmp     rbx, 0
    je      .not_found
    
.search_loop:
    ; Compare variable name
    call    compare_strings
    cmp     rax, 1
    je      .found
    
    ; Move to next
    mov     rbx, qword [rbx + DATA_NODE_NEXT]
    cmp     rbx, 0
    jne     .search_loop
    
.not_found:
    mov     rax, 0
    jmp     .done
    
.found:
    mov     rax, rbx
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Search parent scopes
; Args: rcx = variable name
; Returns: rax = data node
search_parent_scopes:
    push    rbx
    push    rcx
    push    rdx
    
    ; Get parent scope from stack
    mov     rbx, qword [scope_stack_top]
    cmp     rbx, 0
    je      .not_found
    
    dec     rbx
    mov     rdx, qword [scope_stack + rbx * 8]
    cmp     rdx, 0
    je      .not_found
    
    ; Search in parent scope
    call    search_in_scope
    cmp     rax, 0
    jne     .found
    
    ; Recursively search parent scopes
    call    search_parent_scopes
    jmp     .done
    
.not_found:
    mov     rax, 0
    jmp     .done
    
.found:
    mov     rax, rbx
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Compare strings
; Args: rbx = string1, rcx = string2
; Returns: rax = 1 if equal, 0 if not
compare_strings:
    push    rbx
    push    rcx
    push    rdx
    
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

; Output evaluated value
; Args: rax = data node
output_evaluated_value:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rax
    mov     rcx, qword [rbx + DATA_NODE_TYPE]
    
    ; Output based on data type
    cmp     rcx, DATA_TYPE_STRING
    je      .output_string
    
    cmp     rcx, DATA_TYPE_NUMBER
    je      .output_number
    
    cmp     rcx, DATA_TYPE_BOOLEAN
    je      .output_boolean
    
    jmp     .done
    
.output_string:
    mov     rcx, qword [rbx + DATA_NODE_VALUE]
    call    copy_text_to_output
    jmp     .done
    
.output_number:
    ; Convert number to string and output
    call    number_to_string
    jmp     .done
    
.output_boolean:
    ; Convert boolean to string and output
    call    boolean_to_string
    jmp     .done
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Push scope to stack
push_scope:
    push    rbx
    push    rcx
    
    mov     rbx, qword [scope_stack_top]
    mov     rcx, qword [current_scope]
    mov     qword [scope_stack + rbx * 8], rcx
    inc     rbx
    mov     qword [scope_stack_top], rbx
    
    pop     rcx
    pop     rbx
    ret

; Pop scope from stack
pop_scope:
    push    rbx
    push    rcx
    
    mov     rbx, qword [scope_stack_top]
    dec     rbx
    mov     qword [scope_stack_top], rbx
    mov     rcx, qword [scope_stack + rbx * 8]
    mov     qword [current_scope], rcx
    
    pop     rcx
    pop     rbx
    ret

; Evaluate condition
; Args: rcx = condition expression
; Returns: rax = 1 if true, 0 if false
evaluate_condition:
    push    rbx
    push    rcx
    push    rdx
    
    ; Simple condition evaluation
    ; For now, just check if variable exists and is truthy
    call    evaluate_expression
    mov     rbx, rax
    
    cmp     rbx, 0
    je      .false
    
    ; Check if it's a boolean
    mov     rcx, qword [rbx + DATA_NODE_TYPE]
    cmp     rcx, DATA_TYPE_BOOLEAN
    je      .check_boolean
    
    ; Check if it's a number
    cmp     rcx, DATA_TYPE_NUMBER
    je      .check_number
    
    ; Check if it's a string
    cmp     rcx, DATA_TYPE_STRING
    je      .check_string
    
    ; Default to true for objects/arrays
    mov     rax, 1
    jmp     .done
    
.check_boolean:
    mov     rcx, qword [rbx + DATA_NODE_VALUE]
    cmp     rcx, 0
    je      .false
    mov     rax, 1
    jmp     .done
    
.check_number:
    mov     rcx, qword [rbx + DATA_NODE_VALUE]
    cmp     rcx, 0
    je      .false
    mov     rax, 1
    jmp     .done
    
.check_string:
    mov     rcx, qword [rbx + DATA_NODE_VALUE]
    cmp     rcx, 0
    je      .false
    mov     rax, 1
    jmp     .done
    
.false:
    mov     rax, 0
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Convert number to string
; Args: rbx = number data node
number_to_string:
    ; Placeholder - should implement number to string conversion
    ret

; Convert boolean to string
; Args: rbx = boolean data node
boolean_to_string:
    ; Placeholder - should implement boolean to string conversion
    ret

; Get generated output
template_engine_get_output:
    mov     rax, output_buffer
    ret

; Cleanup template engine
template_engine_cleanup:
    mov     qword [template_data_root], 0
    mov     qword [current_scope], 0
    mov     qword [scope_stack_top], 0
    mov     qword [output_pos], 0
    ret
