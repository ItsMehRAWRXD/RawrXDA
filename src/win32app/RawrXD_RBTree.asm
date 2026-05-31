; RawrXD_RBTree.asm — MASM x64 Red-Black Tree Core v2
; Instance-based, thread-safe-ready, overwrite-on-duplicate
; Assemble: ml64.exe /c /Zi /Fo$(OutDir)RawrXD_RBTree.obj RawrXD_RBTree.asm
;
; Node Layout (48 bytes, 8-byte aligned):
;   +0x00  Parent   dq ?
;   +0x08  Left     dq ?
;   +0x10  Right    dq ?
;   +0x18  Key      dq ?
;   +0x20  Value    dq ?
;   +0x28  Color    db ?   (0=Black, 1=Red)
;   +0x29  Padding  db 7 dup(?)
;   +0x30  (end)

; Tree Context Layout (16 bytes):
;   +0x00  Root    dq ?
;   +0x08  Count   dq ?

; ============================================================================
; External Windows APIs
; ============================================================================

extern GetProcessHeap : proc
extern HeapAlloc : proc
extern HeapFree : proc

; ============================================================================
; Data Section
; ============================================================================

.data
    g_hHeap     dq 0

; Color constants
BLACK       equ 0
RED         equ 1

; Node field offsets
OFF_PARENT  equ 0
OFF_LEFT    equ 8
OFF_RIGHT   equ 16
OFF_KEY     equ 24
OFF_VALUE   equ 32
OFF_COLOR   equ 40

; Tree context offsets
OFF_ROOT    equ 0
OFF_COUNT   equ 8

; ============================================================================
; Code Section
; ============================================================================

.code

; ----------------------------------------------------------------------------
; RB_Init — Initialize the global heap allocator (call once at process start)
; ----------------------------------------------------------------------------
RB_Init proc
    sub     rsp, 40
    call    GetProcessHeap
    mov     g_hHeap, rax
    add     rsp, 40
    ret
RB_Init endp

; ----------------------------------------------------------------------------
; RB_TreeInit — Initialize a tree context instance
;   RCX = RB_TREE* (16 bytes: Root, Count)
; ----------------------------------------------------------------------------
RB_TreeInit proc
    mov     qword ptr [rcx + OFF_ROOT], 0
    mov     qword ptr [rcx + OFF_COUNT], 0
    ret
RB_TreeInit endp

; ----------------------------------------------------------------------------
; RB_CreateNode — Allocate and initialize a new node
;   RCX = Key, RDX = Value
;   Returns: RAX = pointer to new node (or NULL on failure)
; ----------------------------------------------------------------------------
RB_CreateNode proc
    push    rbx
    push    rdi
    sub     rsp, 40

    mov     rbx, rcx            ; Save key
    mov     rdi, rdx            ; Save value

    ; HeapAlloc(g_hHeap, HEAP_ZERO_MEMORY, 48)
    mov     rcx, g_hHeap
    mov     rdx, 8              ; HEAP_ZERO_MEMORY
    mov     r8, 48              ; dwBytes
    call    HeapAlloc

    test    rax, rax
    jz      @alloc_fail

    ; Initialize node fields (HeapAlloc zeroed most, but be explicit)
    mov     qword ptr [rax + OFF_PARENT], 0
    mov     qword ptr [rax + OFF_LEFT], 0
    mov     qword ptr [rax + OFF_RIGHT], 0
    mov     [rax + OFF_KEY], rbx
    mov     [rax + OFF_VALUE], rdi
    mov     byte ptr [rax + OFF_COLOR], RED

@alloc_fail:
    add     rsp, 40
    pop     rdi
    pop     rbx
    ret
RB_CreateNode endp

; ----------------------------------------------------------------------------
; RB_LeftRotate — Standard RB-tree left rotation
;   RCX = RB_TREE* (tree context)
;   RDX = Pivot node (RB_NODE*)
; ----------------------------------------------------------------------------
RB_LeftRotate proc
    push    rbx
    push    rsi
    sub     rsp, 40

    mov     rbx, rcx            ; rbx = tree context
    mov     rsi, rdx            ; rsi = pivot node

    mov     r8, [rsi + OFF_RIGHT]       ; r8 = Pivot->Right (Y)
    mov     rax, [r8 + OFF_LEFT]        ; rax = Y->Left
    mov     [rsi + OFF_RIGHT], rax      ; Pivot->Right = Y->Left

    test    rax, rax
    jz      @skip_parent
    mov     [rax + OFF_PARENT], rsi     ; Y->Left->Parent = Pivot
@skip_parent:

    mov     rax, [rsi + OFF_PARENT]     ; rax = Pivot->Parent
    mov     [r8 + OFF_PARENT], rax      ; Y->Parent = Pivot->Parent

    test    rax, rax
    jnz     @check_side
    mov     [rbx + OFF_ROOT], r8        ; tree->Root = Y
    jmp     @link_y
@check_side:
    cmp     rsi, [rax + OFF_LEFT]       ; Is Pivot the Left child?
    jne     @is_right
    mov     [rax + OFF_LEFT], r8        ; Parent->Left = Y
    jmp     @link_y
@is_right:
    mov     [rax + OFF_RIGHT], r8       ; Parent->Right = Y

@link_y:
    mov     [r8 + OFF_LEFT], rsi        ; Y->Left = Pivot
    mov     [rsi + OFF_PARENT], r8      ; Pivot->Parent = Y

    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
RB_LeftRotate endp

; ----------------------------------------------------------------------------
; RB_RightRotate — Standard RB-tree right rotation
;   RCX = RB_TREE* (tree context)
;   RDX = Pivot node (RB_NODE*)
; ----------------------------------------------------------------------------
RB_RightRotate proc
    push    rbx
    push    rsi
    sub     rsp, 40

    mov     rbx, rcx            ; rbx = tree context
    mov     rsi, rdx            ; rsi = pivot node

    mov     r8, [rsi + OFF_LEFT]        ; r8 = Pivot->Left (Y)
    mov     rax, [r8 + OFF_RIGHT]       ; rax = Y->Right
    mov     [rsi + OFF_LEFT], rax       ; Pivot->Left = Y->Right

    test    rax, rax
    jz      @skip_parent
    mov     [rax + OFF_PARENT], rsi     ; Y->Right->Parent = Pivot
@skip_parent:

    mov     rax, [rsi + OFF_PARENT]     ; rax = Pivot->Parent
    mov     [r8 + OFF_PARENT], rax      ; Y->Parent = Pivot->Parent

    test    rax, rax
    jnz     @check_side
    mov     [rbx + OFF_ROOT], r8        ; tree->Root = Y
    jmp     @link_y
@check_side:
    cmp     rsi, [rax + OFF_RIGHT]      ; Is Pivot the Right child?
    jne     @is_left
    mov     [rax + OFF_RIGHT], r8       ; Parent->Right = Y
    jmp     @link_y
@is_left:
    mov     [rax + OFF_LEFT], r8        ; Parent->Left = Y

@link_y:
    mov     [r8 + OFF_RIGHT], rsi       ; Y->Right = Pivot
    mov     [rsi + OFF_PARENT], r8      ; Pivot->Parent = Y

    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
RB_RightRotate endp

; ----------------------------------------------------------------------------
; RB_InsertFixup — Restore RB properties after insertion
;   RCX = RB_TREE* (tree context)
;   RDX = Newly inserted node (RB_NODE*)
; ----------------------------------------------------------------------------
RB_InsertFixup proc
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 40

    mov     rbx, rcx                    ; rbx = tree context
    mov     rsi, rdx                    ; rsi = new node

@loop:
    ; while (node != tree->root && node->parent->color == RED)
    mov     rax, [rbx + OFF_ROOT]       ; rax = tree->root
    cmp     rsi, rax
    je      @done                       ; node == root, done

    mov     rdi, [rsi + OFF_PARENT]     ; rdi = parent
    cmp     byte ptr [rdi + OFF_COLOR], RED
    jne     @done                       ; parent is BLACK, done

    ; Determine if parent is left or right child of grandparent
    mov     rax, [rdi + OFF_PARENT]     ; rax = grandparent
    cmp     rdi, [rax + OFF_LEFT]
    jne     @parent_is_right

    ; Parent is LEFT child
    mov     rcx, [rax + OFF_RIGHT]      ; rcx = uncle (right child of grandparent)

    ; Case 1: Uncle is RED
    test    rcx, rcx
    jz      @case1_skip
    cmp     byte ptr [rcx + OFF_COLOR], RED
    jne     @case1_skip

    mov     byte ptr [rdi + OFF_COLOR], BLACK
    mov     byte ptr [rcx + OFF_COLOR], BLACK
    mov     byte ptr [rax + OFF_COLOR], RED
    mov     rsi, rax                    ; node = grandparent
    jmp     @loop

@case1_skip:
    ; Case 2: Uncle is BLACK, node is right child
    cmp     rsi, [rdi + OFF_RIGHT]
    jne     @case2_skip

    mov     rsi, rdi                    ; node = parent
    mov     rdx, rsi
    mov     rcx, rbx
    call    RB_LeftRotate
    mov     rdi, [rsi + OFF_PARENT]     ; update parent

@case2_skip:
    ; Case 3: Uncle is BLACK, node is left child
    mov     byte ptr [rdi + OFF_COLOR], BLACK
    mov     rax, [rdi + OFF_PARENT]
    mov     byte ptr [rax + OFF_COLOR], RED
    mov     rdx, rax
    mov     rcx, rbx
    call    RB_RightRotate
    jmp     @loop

@parent_is_right:
    ; Parent is RIGHT child (mirror of above)
    mov     rcx, [rax + OFF_LEFT]       ; rcx = uncle (left child of grandparent)

    ; Case 1: Uncle is RED
    test    rcx, rcx
    jz      @case1_skip_r
    cmp     byte ptr [rcx + OFF_COLOR], RED
    jne     @case1_skip_r

    mov     byte ptr [rdi + OFF_COLOR], BLACK
    mov     byte ptr [rcx + OFF_COLOR], BLACK
    mov     byte ptr [rax + OFF_COLOR], RED
    mov     rsi, rax
    jmp     @loop

@case1_skip_r:
    ; Case 2: Uncle is BLACK, node is left child
    cmp     rsi, [rdi + OFF_LEFT]
    jne     @case2_skip_r

    mov     rsi, rdi
    mov     rdx, rsi
    mov     rcx, rbx
    call    RB_RightRotate
    mov     rdi, [rsi + OFF_PARENT]

@case2_skip_r:
    ; Case 3: Uncle is BLACK, node is right child
    mov     byte ptr [rdi + OFF_COLOR], BLACK
    mov     rax, [rdi + OFF_PARENT]
    mov     byte ptr [rax + OFF_COLOR], RED
    mov     rdx, rax
    mov     rcx, rbx
    call    RB_LeftRotate
    jmp     @loop

@done:
    ; Ensure root is BLACK
    mov     rax, [rbx + OFF_ROOT]
    mov     byte ptr [rax + OFF_COLOR], BLACK

    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RB_InsertFixup endp

; ----------------------------------------------------------------------------
; RB_Insert — Insert a key-value pair into the tree
;   RCX = RB_TREE* (tree context)
;   RDX = Key
;   R8  = Value
;   Returns: RAX = 1 on success, 0 on failure
; ----------------------------------------------------------------------------
RB_Insert proc
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 40

    mov     rbx, rcx                    ; rbx = tree context
    mov     r12, rdx                    ; r12 = Key
    mov     r13, r8                     ; r13 = Value

    ; Standard BST insert
    xor     rdi, rdi                    ; rdi = parent (null)
    mov     rsi, [rbx + OFF_ROOT]       ; rsi = current

@bst_loop:
    test    rsi, rsi
    jz      @bst_done

    mov     rdi, rsi
    cmp     r12, [rsi + OFF_KEY]
    je      @overwrite                 ; Duplicate key: overwrite value
    jb      @go_left
    mov     rsi, [rsi + OFF_RIGHT]
    jmp     @bst_loop
@go_left:
    mov     rsi, [rsi + OFF_LEFT]
    jmp     @bst_loop

@overwrite:
    ; Overwrite existing value, free the new node we would have allocated
    mov     [rsi + OFF_VALUE], r13
    mov     rax, 1
    jmp     @insert_done

@bst_done:
    ; Create new node
    mov     rcx, r12
    mov     rdx, r13
    call    RB_CreateNode
    test    rax, rax
    jz      @insert_fail

    mov     rsi, rax                    ; rsi = new node

    mov     [rsi + OFF_PARENT], rdi

    test    rdi, rdi
    jnz     @has_parent
    ; Tree was empty
    mov     [rbx + OFF_ROOT], rsi
    jmp     @fixup
@has_parent:
    cmp     r12, [rdi + OFF_KEY]
    jb      @insert_left
    mov     [rdi + OFF_RIGHT], rsi
    jmp     @fixup
@insert_left:
    mov     [rdi + OFF_LEFT], rsi

@fixup:
    ; Fixup RB properties
    mov     rcx, rbx
    mov     rdx, rsi
    call    RB_InsertFixup

    inc     qword ptr [rbx + OFF_COUNT]
    mov     rax, 1
    jmp     @insert_done

@insert_fail:
    xor     rax, rax

@insert_done:
    add     rsp, 40
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RB_Insert endp

; ----------------------------------------------------------------------------
; RB_Find — Search for a key in the tree
;   RCX = RB_TREE* (tree context)
;   RDX = Key
;   Returns: RAX = pointer to node (or NULL if not found)
; ----------------------------------------------------------------------------
RB_Find proc
    push    rbx
    sub     rsp, 40

    mov     rbx, [rcx + OFF_ROOT]       ; rbx = tree->root
    mov     rcx, rdx                    ; rcx = key

@find_loop:
    test    rbx, rbx
    jz      @find_fail

    cmp     rcx, [rbx + OFF_KEY]
    je      @find_done
    jb      @go_left
    mov     rbx, [rbx + OFF_RIGHT]
    jmp     @find_loop
@go_left:
    mov     rbx, [rbx + OFF_LEFT]
    jmp     @find_loop

@find_fail:
    xor     rax, rax
    add     rsp, 40
    pop     rbx
    ret
@find_done:
    mov     rax, rbx
    add     rsp, 40
    pop     rbx
    ret
RB_Find endp

; ----------------------------------------------------------------------------
; RB_GetValue — Get value for a key (0 if not found)
;   RCX = RB_TREE* (tree context)
;   RDX = Key
;   Returns: RAX = Value
; ----------------------------------------------------------------------------
RB_GetValue proc
    push    rbx
    sub     rsp, 40

    mov     rbx, rdx                    ; rbx = key
    call    RB_Find
    test    rax, rax
    jz      @get_fail
    mov     rax, [rax + OFF_VALUE]
    add     rsp, 40
    pop     rbx
    ret
@get_fail:
    xor     rax, rax
    add     rsp, 40
    pop     rbx
    ret
RB_GetValue endp

; ----------------------------------------------------------------------------
; RB_Contains — Check if key exists
;   RCX = RB_TREE* (tree context)
;   RDX = Key
;   Returns: RAX = 1 if found, 0 if not
; ----------------------------------------------------------------------------
RB_Contains proc
    push    rbx
    sub     rsp, 40

    mov     rbx, rdx                    ; rbx = key
    call    RB_Find
    test    rax, rax
    setnz   al
    movzx   rax, al

    add     rsp, 40
    pop     rbx
    ret
RB_Contains endp

; ----------------------------------------------------------------------------
; RB_GetCount — Return number of nodes in tree
;   RCX = RB_TREE* (tree context)
; ----------------------------------------------------------------------------
RB_GetCount proc
    mov     rax, [rcx + OFF_COUNT]
    ret
RB_GetCount endp

; ----------------------------------------------------------------------------
; RB_GetRoot — Return root node pointer
;   RCX = RB_TREE* (tree context)
; ----------------------------------------------------------------------------
RB_GetRoot proc
    mov     rax, [rcx + OFF_ROOT]
    ret
RB_GetRoot endp

; ----------------------------------------------------------------------------
; RB_ClearNode — Delete all nodes (iterative to avoid stack blowout)
;   RCX = RB_TREE* (tree context)
; ----------------------------------------------------------------------------
RB_ClearNode proc
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 40

    mov     rbx, rcx                    ; rbx = tree context
    mov     rsi, [rbx + OFF_ROOT]       ; rsi = root

    test    rsi, rsi
    jz      @clear_done

    ; Iterative post-order traversal using parent pointers
    ; Stackless: use parent pointers to walk back up

@traverse:
    ; Go to leftmost node
    mov     rdi, [rsi + OFF_LEFT]
    test    rdi, rdi
    jz      @check_right
    mov     rsi, rdi
    jmp     @traverse

@check_right:
    ; Check if we can go right
    mov     rdi, [rsi + OFF_RIGHT]
    test    rdi, rdi
    jz      @visit_node
    mov     rsi, rdi
    jmp     @traverse

@visit_node:
    ; Save parent before freeing
    mov     rdi, [rsi + OFF_PARENT]

    ; Free this node
    push    rdi
    mov     rcx, g_hHeap
    xor     rdx, rdx
    mov     r8, rsi
    call    HeapFree
    pop     rdi

    ; If parent is null, we're done
    test    rdi, rdi
    jz      @clear_done

    ; Determine if we came from left or right
    cmp     rsi, [rdi + OFF_LEFT]
    je      @came_from_left

    ; Came from right: parent's right is now null, visit parent
    mov     qword ptr [rdi + OFF_RIGHT], 0
    mov     rsi, rdi
    jmp     @check_right

@came_from_left:
    ; Came from left: parent's left is now null
    mov     qword ptr [rdi + OFF_LEFT], 0

    ; Check if parent has right child
    mov     rsi, [rdi + OFF_RIGHT]
    test    rsi, rsi
    jnz     @traverse

    ; No right child, visit parent
    mov     rsi, rdi
    jmp     @check_right

@clear_done:
    mov     qword ptr [rbx + OFF_ROOT], 0
    mov     qword ptr [rbx + OFF_COUNT], 0

    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RB_ClearNode endp

; ----------------------------------------------------------------------------
; RB_Clear — Clear entire tree
;   RCX = RB_TREE* (tree context)
; ----------------------------------------------------------------------------
RB_Clear proc
    jmp     RB_ClearNode
RB_Clear endp

; ============================================================================
; Export Table
; ============================================================================

public RB_Init
public RB_TreeInit
public RB_CreateNode
public RB_LeftRotate
public RB_RightRotate
public RB_InsertFixup
public RB_Insert
public RB_Find
public RB_GetValue
public RB_Contains
public RB_GetCount
public RB_GetRoot
public RB_ClearNode
public RB_Clear

end
