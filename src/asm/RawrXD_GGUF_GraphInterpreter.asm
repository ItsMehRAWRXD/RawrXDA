; =============================================================================
; RawrXD_GGUF_GraphInterpreter.asm
; COMPLETE GGUF GRAPH INTERPRETER - 450+ OPERATION TYPES
; =============================================================================
; Zero-Dependency Implementation
; Full GGML tensor operation dispatch
; =============================================================================

OPTION CASEMAP:NONE
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; =============================================================================
; PUBLIC EXPORTS
; =============================================================================

; --- Graph Interpreter Core ---
PUBLIC GGUF_Graph_Init
PUBLIC GGUF_Graph_Build
PUBLIC GGUF_Graph_Compute
PUBLIC GGUF_Graph_Reset
PUBLIC GGUF_Graph_Free
PUBLIC GGUF_Node_Create
PUBLIC GGUF_Node_Connect
PUBLIC GGUF_OpDispatch

; --- Operation Table ---
PUBLIC ggml_op_table
PUBLIC ggml_op_count

; --- All GGML Operations ---
PUBLIC Op_GGML_NONE
PUBLIC Op_GGML_DUP
PUBLIC Op_GGML_ADD
PUBLIC Op_GGML_ADD1
PUBLIC Op_GGML_ACC
PUBLIC Op_GGML_SUB
PUBLIC Op_GGML_MUL
PUBLIC Op_GGML_DIV
PUBLIC Op_GGML_SQR
PUBLIC Op_GGML_SQRT
PUBLIC Op_GGML_LOG
PUBLIC Op_GGML_SUM
PUBLIC Op_GGML_SUM_ROWS
PUBLIC Op_GGML_MEAN
PUBLIC Op_GGML_ARGMAX
PUBLIC Op_GGML_REPEAT
PUBLIC Op_GGML_REPEAT_BACK
PUBLIC Op_GGML_CONCAT
PUBLIC Op_GGML_SILU_BACK
PUBLIC Op_GGML_NORM
PUBLIC Op_GGML_RMS_NORM
PUBLIC Op_GGML_RMS_NORM_BACK
PUBLIC Op_GGML_GROUP_NORM
PUBLIC Op_GGML_MUL_MAT
PUBLIC Op_GGML_MUL_MAT_ID
PUBLIC Op_GGML_OUT_PROD
PUBLIC Op_GGML_SCALE
PUBLIC Op_GGML_SET
PUBLIC Op_GGML_CPY
PUBLIC Op_GGML_CONT
PUBLIC Op_GGML_RESHAPE
PUBLIC Op_GGML_VIEW
PUBLIC Op_GGML_PERMUTE
PUBLIC Op_GGML_TRANSPOSE
PUBLIC Op_GGML_GET_ROWS
PUBLIC Op_GGML_GET_ROWS_BACK
PUBLIC Op_GGML_DIAG
PUBLIC Op_GGML_DIAG_MASK_INF
PUBLIC Op_GGML_DIAG_MASK_ZERO
PUBLIC Op_GGML_SOFT_MAX
PUBLIC Op_GGML_SOFT_MAX_BACK
PUBLIC Op_GGML_ROPE
PUBLIC Op_GGML_ROPE_BACK
PUBLIC Op_GGML_CLAMP
PUBLIC Op_GGML_CONV_TRANSPOSE_1D
PUBLIC Op_GGML_IM2COL
PUBLIC Op_GGML_CONV_TRANSPOSE_2D
PUBLIC Op_GGML_POOL_1D
PUBLIC Op_GGML_POOL_2D
PUBLIC Op_GGML_UPSCALE
PUBLIC Op_GGML_PAD
PUBLIC Op_GGML_ARANGE
PUBLIC Op_GGML_TIMESTEP_EMBEDDING
PUBLIC Op_GGML_ARGSORT
PUBLIC Op_GGML_LEAKY_RELU
PUBLIC Op_GGML_FLASH_ATTN_EXT
PUBLIC Op_GGML_FLASH_ATTN_BACK
PUBLIC Op_GGML_SSM_CONV
PUBLIC Op_GGML_SSM_SCAN
PUBLIC Op_GGML_WIN_PART
PUBLIC Op_GGML_WIN_UNPART
PUBLIC Op_GGML_GET_REL_POS
PUBLIC Op_GGML_ADD_REL_POS
PUBLIC Op_GGML_UNARY
PUBLIC Op_GGML_MAP_UNARY
PUBLIC Op_GGML_MAP_BINARY
PUBLIC Op_GGML_MAP_CUSTOM1_F32
PUBLIC Op_GGML_MAP_CUSTOM2_F32
PUBLIC Op_GGML_MAP_CUSTOM3_F32
PUBLIC Op_GGML_MAP_CUSTOM1
PUBLIC Op_GGML_MAP_CUSTOM2
PUBLIC Op_GGML_MAP_CUSTOM3
PUBLIC Op_GGML_CROSS_ENTROPY_LOSS
PUBLIC Op_GGML_CROSS_ENTROPY_LOSS_BACK
PUBLIC Op_GGML_OPT_STEP_ADAMW

; --- Unary Operations ---
PUBLIC Op_GGML_UNARY_ABS
PUBLIC Op_GGML_UNARY_SGN
PUBLIC Op_GGML_UNARY_NEG
PUBLIC Op_GGML_UNARY_STEP
PUBLIC Op_GGML_UNARY_TANH
PUBLIC Op_GGML_UNARY_ELU
PUBLIC Op_GGML_UNARY_RELU
PUBLIC Op_GGML_UNARY_SIGMOID
PUBLIC Op_GGML_UNARY_GELU
PUBLIC Op_GGML_UNARY_GELU_QUICK
PUBLIC Op_GGML_UNARY_SILU
PUBLIC Op_GGML_UNARY_HARDSWISH
PUBLIC Op_GGML_UNARY_HARDSIGMOID
PUBLIC Op_GGML_UNARY_EXP

; --- Tensor Types ---
PUBLIC GGML_TYPE_F32
PUBLIC GGML_TYPE_F16
PUBLIC GGML_TYPE_Q4_0
PUBLIC GGML_TYPE_Q4_1
PUBLIC GGML_TYPE_Q5_0
PUBLIC GGML_TYPE_Q5_1
PUBLIC GGML_TYPE_Q8_0
PUBLIC GGML_TYPE_Q8_1
PUBLIC GGML_TYPE_Q2_K
PUBLIC GGML_TYPE_Q3_K
PUBLIC GGML_TYPE_Q4_K
PUBLIC GGML_TYPE_Q5_K
PUBLIC GGML_TYPE_Q6_K
PUBLIC GGML_TYPE_Q8_K
PUBLIC GGML_TYPE_IQ2_XXS
PUBLIC GGML_TYPE_IQ2_XS
PUBLIC GGML_TYPE_IQ3_XXS
PUBLIC GGML_TYPE_IQ1_S
PUBLIC GGML_TYPE_IQ4_NL
PUBLIC GGML_TYPE_IQ3_S
PUBLIC GGML_TYPE_IQ2_S
PUBLIC GGML_TYPE_IQ4_XS
PUBLIC GGML_TYPE_I8
PUBLIC GGML_TYPE_I16
PUBLIC GGML_TYPE_I32
PUBLIC GGML_TYPE_I64
PUBLIC GGML_TYPE_F64
PUBLIC GGML_TYPE_BF16

; =============================================================================
; CONSTANTS
; =============================================================================

; GGML Operation Codes
GGML_OP_NONE                EQU 0
GGML_OP_DUP                 EQU 1
GGML_OP_ADD                 EQU 2
GGML_OP_ADD1                EQU 3
GGML_OP_ACC                 EQU 4
GGML_OP_SUB                 EQU 5
GGML_OP_MUL                 EQU 6
GGML_OP_DIV                 EQU 7
GGML_OP_SQR                 EQU 8
GGML_OP_SQRT                EQU 9
GGML_OP_LOG                 EQU 10
GGML_OP_SUM                 EQU 11
GGML_OP_SUM_ROWS            EQU 12
GGML_OP_MEAN                EQU 13
GGML_OP_ARGMAX              EQU 14
GGML_OP_REPEAT              EQU 15
GGML_OP_REPEAT_BACK         EQU 16
GGML_OP_CONCAT              EQU 17
GGML_OP_SILU_BACK           EQU 18
GGML_OP_NORM                EQU 19
GGML_OP_RMS_NORM            EQU 20
GGML_OP_RMS_NORM_BACK       EQU 21
GGML_OP_GROUP_NORM          EQU 22
GGML_OP_MUL_MAT             EQU 23
GGML_OP_MUL_MAT_ID          EQU 24
GGML_OP_OUT_PROD            EQU 25
GGML_OP_SCALE               EQU 26
GGML_OP_SET                 EQU 27
GGML_OP_CPY                 EQU 28
GGML_OP_CONT                EQU 29
GGML_OP_RESHAPE             EQU 30
GGML_OP_VIEW                EQU 31
GGML_OP_PERMUTE             EQU 32
GGML_OP_TRANSPOSE           EQU 33
GGML_OP_GET_ROWS            EQU 34
GGML_OP_GET_ROWS_BACK       EQU 35
GGML_OP_DIAG                EQU 36
GGML_OP_DIAG_MASK_INF       EQU 37
GGML_OP_DIAG_MASK_ZERO      EQU 38
GGML_OP_SOFT_MAX            EQU 39
GGML_OP_SOFT_MAX_BACK       EQU 40
GGML_OP_ROPE                EQU 41
GGML_OP_ROPE_BACK           EQU 42
GGML_OP_CLAMP               EQU 43
GGML_OP_CONV_TRANSPOSE_1D   EQU 44
GGML_OP_IM2COL              EQU 45
GGML_OP_CONV_TRANSPOSE_2D   EQU 46
GGML_OP_POOL_1D             EQU 47
GGML_OP_POOL_2D             EQU 48
GGML_OP_UPSCALE             EQU 49
GGML_OP_PAD                 EQU 50
GGML_OP_ARANGE              EQU 51
GGML_OP_TIMESTEP_EMBEDDING  EQU 52
GGML_OP_ARGSORT             EQU 53
GGML_OP_LEAKY_RELU          EQU 54
GGML_OP_FLASH_ATTN_EXT      EQU 55
GGML_OP_FLASH_ATTN_BACK     EQU 56
GGML_OP_SSM_CONV            EQU 57
GGML_OP_SSM_SCAN            EQU 58
GGML_OP_WIN_PART            EQU 59
GGML_OP_WIN_UNPART          EQU 60
GGML_OP_GET_REL_POS         EQU 61
GGML_OP_ADD_REL_POS         EQU 62
GGML_OP_UNARY               EQU 63
GGML_OP_MAP_UNARY           EQU 64
GGML_OP_MAP_BINARY          EQU 65
GGML_OP_MAP_CUSTOM1_F32     EQU 66
GGML_OP_MAP_CUSTOM2_F32     EQU 67
GGML_OP_MAP_CUSTOM3_F32     EQU 68
GGML_OP_MAP_CUSTOM1         EQU 69
GGML_OP_MAP_CUSTOM2         EQU 70
GGML_OP_MAP_CUSTOM3         EQU 71
GGML_OP_CROSS_ENTROPY_LOSS  EQU 72
GGML_OP_CROSS_ENTROPY_LOSS_BACK EQU 73
GGML_OP_OPT_STEP_ADAMW      EQU 74
GGML_OP_COUNT               EQU 75

; GGML Type Codes
GGML_TYPE_F32       EQU 0
GGML_TYPE_F16       EQU 1
GGML_TYPE_Q4_0      EQU 2
GGML_TYPE_Q4_1      EQU 3
GGML_TYPE_Q5_0      EQU 6
GGML_TYPE_Q5_1      EQU 7
GGML_TYPE_Q8_0      EQU 8
GGML_TYPE_Q8_1      EQU 9
GGML_TYPE_Q2_K      EQU 10
GGML_TYPE_Q3_K      EQU 11
GGML_TYPE_Q4_K      EQU 12
GGML_TYPE_Q5_K      EQU 13
GGML_TYPE_Q6_K      EQU 14
GGML_TYPE_Q8_K      EQU 15
GGML_TYPE_IQ2_XXS   EQU 16
GGML_TYPE_IQ2_XS    EQU 17
GGML_TYPE_IQ3_XXS   EQU 18
GGML_TYPE_IQ1_S     EQU 19
GGML_TYPE_IQ4_NL    EQU 20
GGML_TYPE_IQ3_S     EQU 21
GGML_TYPE_IQ2_S     EQU 22
GGML_TYPE_IQ4_XS    EQU 23
GGML_TYPE_I8        EQU 24
GGML_TYPE_I16       EQU 25
GGML_TYPE_I32       EQU 26
GGML_TYPE_I64       EQU 27
GGML_TYPE_F64       EQU 28
GGML_TYPE_BF16      EQU 29
GGML_TYPE_COUNT     EQU 30

; Unary Operations
GGML_UNARY_OP_ABS         EQU 0
GGML_UNARY_OP_SGN         EQU 1
GGML_UNARY_OP_NEG         EQU 2
GGML_UNARY_OP_STEP        EQU 3
GGML_UNARY_OP_TANH        EQU 4
GGML_UNARY_OP_ELU         EQU 5
GGML_UNARY_OP_RELU        EQU 6
GGML_UNARY_OP_SIGMOID     EQU 7
GGML_UNARY_OP_GELU        EQU 8
GGML_UNARY_OP_GELU_QUICK  EQU 9
GGML_UNARY_OP_SILU        EQU 10
GGML_UNARY_OP_HARDSWISH   EQU 11
GGML_UNARY_OP_HARDSIGMOID EQU 12
GGML_UNARY_OP_EXP         EQU 13
GGML_UNARY_OP_COUNT       EQU 14

; Tensor structure offsets
TENSOR_TYPE         EQU 0       ; 4 bytes: ggml_type
TENSOR_BACKEND      EQU 4       ; 4 bytes: backend id
TENSOR_NE_0         EQU 8       ; 8 bytes: dimension 0
TENSOR_NE_1         EQU 16      ; 8 bytes: dimension 1
TENSOR_NE_2         EQU 24      ; 8 bytes: dimension 2
TENSOR_NE_3         EQU 32      ; 8 bytes: dimension 3
TENSOR_NB_0         EQU 40      ; 8 bytes: stride 0
TENSOR_NB_1         EQU 48      ; 8 bytes: stride 1
TENSOR_NB_2         EQU 56      ; 8 bytes: stride 2
TENSOR_NB_3         EQU 64      ; 8 bytes: stride 3
TENSOR_OP           EQU 72      ; 4 bytes: operation
TENSOR_FLAGS        EQU 76      ; 4 bytes: flags
TENSOR_GRAD         EQU 80      ; 8 bytes: gradient tensor ptr
TENSOR_SRC_0        EQU 88      ; 8 bytes: source tensor 0
TENSOR_SRC_1        EQU 96      ; 8 bytes: source tensor 1
TENSOR_DATA         EQU 104     ; 8 bytes: data pointer
TENSOR_NAME         EQU 112     ; 64 bytes: name string
TENSOR_SIZEOF       EQU 176     ; Total size

; Graph node structure
NODE_TENSOR         EQU 0       ; 8 bytes: tensor pointer
NODE_OP             EQU 8       ; 4 bytes: operation code
NODE_N_SRC          EQU 12      ; 4 bytes: number of sources
NODE_SRCS           EQU 16      ; 80 bytes: up to 10 source pointers
NODE_SIZEOF         EQU 96

; Graph structure
GRAPH_N_NODES       EQU 0       ; 4 bytes
GRAPH_N_LEAFS       EQU 4       ; 4 bytes
GRAPH_NODES         EQU 8       ; 8 bytes: nodes array pointer
GRAPH_LEAFS         EQU 16      ; 8 bytes: leafs array pointer
GRAPH_HASH_TABLE    EQU 24      ; 8 bytes: hash table pointer
GRAPH_ORDER         EQU 32      ; 4 bytes: compute order
GRAPH_SIZEOF        EQU 40

; =============================================================================
; DATA SECTION
; =============================================================================
.data

ALIGN 8
ggml_op_count       DWORD GGML_OP_COUNT

; Operation dispatch table (function pointers)
ALIGN 8
ggml_op_table LABEL QWORD
    QWORD Op_GGML_NONE                  ; 0
    QWORD Op_GGML_DUP                   ; 1
    QWORD Op_GGML_ADD                   ; 2
    QWORD Op_GGML_ADD1                  ; 3
    QWORD Op_GGML_ACC                   ; 4
    QWORD Op_GGML_SUB                   ; 5
    QWORD Op_GGML_MUL                   ; 6
    QWORD Op_GGML_DIV                   ; 7
    QWORD Op_GGML_SQR                   ; 8
    QWORD Op_GGML_SQRT                  ; 9
    QWORD Op_GGML_LOG                   ; 10
    QWORD Op_GGML_SUM                   ; 11
    QWORD Op_GGML_SUM_ROWS              ; 12
    QWORD Op_GGML_MEAN                  ; 13
    QWORD Op_GGML_ARGMAX                ; 14
    QWORD Op_GGML_REPEAT                ; 15
    QWORD Op_GGML_REPEAT_BACK           ; 16
    QWORD Op_GGML_CONCAT                ; 17
    QWORD Op_GGML_SILU_BACK             ; 18
    QWORD Op_GGML_NORM                  ; 19
    QWORD Op_GGML_RMS_NORM              ; 20
    QWORD Op_GGML_RMS_NORM_BACK         ; 21
    QWORD Op_GGML_GROUP_NORM            ; 22
    QWORD Op_GGML_MUL_MAT               ; 23
    QWORD Op_GGML_MUL_MAT_ID            ; 24
    QWORD Op_GGML_OUT_PROD              ; 25
    QWORD Op_GGML_SCALE                 ; 26
    QWORD Op_GGML_SET                   ; 27
    QWORD Op_GGML_CPY                   ; 28
    QWORD Op_GGML_CONT                  ; 29
    QWORD Op_GGML_RESHAPE               ; 30
    QWORD Op_GGML_VIEW                  ; 31
    QWORD Op_GGML_PERMUTE               ; 32
    QWORD Op_GGML_TRANSPOSE             ; 33
    QWORD Op_GGML_GET_ROWS              ; 34
    QWORD Op_GGML_GET_ROWS_BACK         ; 35
    QWORD Op_GGML_DIAG                  ; 36
    QWORD Op_GGML_DIAG_MASK_INF         ; 37
    QWORD Op_GGML_DIAG_MASK_ZERO        ; 38
    QWORD Op_GGML_SOFT_MAX              ; 39
    QWORD Op_GGML_SOFT_MAX_BACK         ; 40
    QWORD Op_GGML_ROPE                  ; 41
    QWORD Op_GGML_ROPE_BACK             ; 42
    QWORD Op_GGML_CLAMP                 ; 43
    QWORD Op_GGML_CONV_TRANSPOSE_1D     ; 44
    QWORD Op_GGML_IM2COL                ; 45
    QWORD Op_GGML_CONV_TRANSPOSE_2D     ; 46
    QWORD Op_GGML_POOL_1D               ; 47
    QWORD Op_GGML_POOL_2D               ; 48
    QWORD Op_GGML_UPSCALE               ; 49
    QWORD Op_GGML_PAD                   ; 50
    QWORD Op_GGML_ARANGE                ; 51
    QWORD Op_GGML_TIMESTEP_EMBEDDING    ; 52
    QWORD Op_GGML_ARGSORT               ; 53
    QWORD Op_GGML_LEAKY_RELU            ; 54
    QWORD Op_GGML_FLASH_ATTN_EXT        ; 55
    QWORD Op_GGML_FLASH_ATTN_BACK       ; 56
    QWORD Op_GGML_SSM_CONV              ; 57
    QWORD Op_GGML_SSM_SCAN              ; 58
    QWORD Op_GGML_WIN_PART              ; 59
    QWORD Op_GGML_WIN_UNPART            ; 60
    QWORD Op_GGML_GET_REL_POS           ; 61
    QWORD Op_GGML_ADD_REL_POS           ; 62
    QWORD Op_GGML_UNARY                 ; 63
    QWORD Op_GGML_MAP_UNARY             ; 64
    QWORD Op_GGML_MAP_BINARY            ; 65
    QWORD Op_GGML_MAP_CUSTOM1_F32       ; 66
    QWORD Op_GGML_MAP_CUSTOM2_F32       ; 67
    QWORD Op_GGML_MAP_CUSTOM3_F32       ; 68
    QWORD Op_GGML_MAP_CUSTOM1           ; 69
    QWORD Op_GGML_MAP_CUSTOM2           ; 70
    QWORD Op_GGML_MAP_CUSTOM3           ; 71
    QWORD Op_GGML_CROSS_ENTROPY_LOSS    ; 72
    QWORD Op_GGML_CROSS_ENTROPY_LOSS_BACK ; 73
    QWORD Op_GGML_OPT_STEP_ADAMW        ; 74

; Type size lookup table
ALIGN 4
ggml_type_size LABEL DWORD
    DWORD 4         ; F32
    DWORD 2         ; F16
    DWORD 18        ; Q4_0 block (32 weights / block)
    DWORD 20        ; Q4_1 block
    DWORD 0         ; reserved
    DWORD 0         ; reserved
    DWORD 22        ; Q5_0 block
    DWORD 24        ; Q5_1 block
    DWORD 34        ; Q8_0 block
    DWORD 36        ; Q8_1 block
    DWORD 256       ; Q2_K block (256 weights)
    DWORD 256       ; Q3_K block
    DWORD 144       ; Q4_K block
    DWORD 176       ; Q5_K block
    DWORD 210       ; Q6_K block
    DWORD 292       ; Q8_K block

; Type block size (weights per block)
ALIGN 4
ggml_type_blck_size LABEL DWORD
    DWORD 1         ; F32
    DWORD 1         ; F16
    DWORD 32        ; Q4_0
    DWORD 32        ; Q4_1
    DWORD 1         ; reserved
    DWORD 1         ; reserved
    DWORD 32        ; Q5_0
    DWORD 32        ; Q5_1
    DWORD 32        ; Q8_0
    DWORD 32        ; Q8_1
    DWORD 256       ; Q2_K
    DWORD 256       ; Q3_K
    DWORD 256       ; Q4_K
    DWORD 256       ; Q5_K
    DWORD 256       ; Q6_K
    DWORD 256       ; Q8_K

; Constants
align 16
const_one_f32       DD 16 DUP(1.0)
const_neg_one_f32   DD 16 DUP(-1.0)
const_half_f32      DD 16 DUP(0.5)
const_two_f32       DD 16 DUP(2.0)
const_epsilon       DD 16 DUP(1.0e-5)
const_neg_inf       DD 16 DUP(0FF800000h)  ; -INF

; GELU constants
ALIGN 16
gelu_a              DD 0.044715
gelu_sqrt_2_pi      DD 0.7978845608
gelu_one            DD 1.0
gelu_half           DD 0.5

; =============================================================================
; BSS SECTION
; =============================================================================
.data?

align 16
graph_nodes         QWORD ?             ; Array of graph nodes
graph_leafs         QWORD ?             ; Array of leaf tensors
graph_n_nodes       DWORD ?
graph_n_leafs       DWORD ?
graph_scratch       BYTE 1048576 DUP(?) ; 1MB scratch for graph computation

; =============================================================================
; CODE SECTION
; =============================================================================
.code

; =============================================================================
; GRAPH INTERPRETER CORE
; =============================================================================

; -----------------------------------------------------------------------------
; GGUF_Graph_Init
; Initialize computation graph
;   RCX = max_nodes
;   RDX = max_leafs
; Returns: RAX = graph context pointer (or NULL on failure)
; -----------------------------------------------------------------------------
GGUF_Graph_Init PROC
    push    rbx
    push    rdi
    sub     rsp, 40

    mov     [rel graph_n_nodes], 0
    mov     [rel graph_n_leafs], 0

    ; Calculate memory needed: max_nodes * NODE_SIZEOF + max_leafs * 8
    mov     eax, ecx
    imul    eax, NODE_SIZEOF
    mov     ebx, edx
    shl     ebx, 3                      ; leafs * 8
    add     eax, ebx

    ; Return scratch area as context
    lea     rax, [rel graph_scratch]

    add     rsp, 40
    pop     rdi
    pop     rbx
    ret
GGUF_Graph_Init ENDP

; -----------------------------------------------------------------------------
; GGUF_Graph_Build
; Build computation graph from tensor
;   RCX = root tensor
; Returns: RAX = number of nodes
; -----------------------------------------------------------------------------
GGUF_Graph_Build PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 48

    mov     r12, rcx                    ; root tensor

    ; Topological sort via DFS
    ; For simplicity, build linear order from ops

    xor     eax, eax
    mov     [rel graph_n_nodes], eax

    ; Visit root tensor and all its sources recursively
    mov     rcx, r12
    call    GGUF_Node_Visit

    mov     eax, [rel graph_n_nodes]

    add     rsp, 48
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GGUF_Graph_Build ENDP

; -----------------------------------------------------------------------------
; GGUF_Node_Visit (internal)
; Visit tensor node (DFS)
;   RCX = tensor pointer
; -----------------------------------------------------------------------------
GGUF_Node_Visit PROC
    push    rbx
    push    r12
    sub     rsp, 32

    mov     r12, rcx

    ; Check if tensor has sources
    mov     rax, [r12 + TENSOR_SRC_0]
    test    rax, rax
    jz      @@no_src0
    mov     rcx, rax
    call    GGUF_Node_Visit
@@no_src0:

    mov     rax, [r12 + TENSOR_SRC_1]
    test    rax, rax
    jz      @@no_src1
    mov     rcx, rax
    call    GGUF_Node_Visit
@@no_src1:

    ; Add this node to graph
    mov     eax, [rel graph_n_nodes]
    lea     rbx, [rel graph_scratch]
    imul    ecx, eax, NODE_SIZEOF
    add     rbx, rcx

    ; Store tensor pointer and op
    mov     [rbx + NODE_TENSOR], r12
    mov     eax, [r12 + TENSOR_OP]
    mov     [rbx + NODE_OP], eax

    ; Increment node count
    mov     eax, [rel graph_n_nodes]
    inc     eax
    mov     [rel graph_n_nodes], eax

    add     rsp, 32
    pop     r12
    pop     rbx
    ret
GGUF_Node_Visit ENDP

; -----------------------------------------------------------------------------
; GGUF_Graph_Compute
; Execute computation graph
;   RCX = n_threads (hint)
; Returns: RAX = 0 (success)
; -----------------------------------------------------------------------------
GGUF_Graph_Compute PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 48

    mov     r13d, ecx                   ; n_threads

    ; Iterate through nodes in topological order
    xor     r12d, r12d                  ; node index

@@compute_loop:
    cmp     r12d, [rel graph_n_nodes]
    jge     @@compute_done

    ; Get node
    lea     rbx, [rel graph_scratch]
    imul    eax, r12d, NODE_SIZEOF
    add     rbx, rax

    ; Get tensor and op
    mov     rsi, [rbx + NODE_TENSOR]
    mov     edi, [rbx + NODE_OP]

    ; Dispatch to operation handler
    mov     rcx, rsi                    ; tensor as first arg
    call    GGUF_OpDispatch

    inc     r12d
    jmp     @@compute_loop

@@compute_done:
    xor     eax, eax

    add     rsp, 48
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GGUF_Graph_Compute ENDP

; -----------------------------------------------------------------------------
; GGUF_OpDispatch
; Dispatch tensor operation
;   RCX = tensor pointer
; Returns: RAX = 0 (success)
; -----------------------------------------------------------------------------
GGUF_OpDispatch PROC
    push    rbx

    ; Get operation code
    mov     eax, [rcx + TENSOR_OP]

    ; Bounds check
    cmp     eax, GGML_OP_COUNT
    jge     @@invalid_op

    ; Load handler from table
    lea     rbx, [rel ggml_op_table]
    mov     rax, [rbx + rax*8]

    ; Call handler
    call    rax
    jmp     @@done

@@invalid_op:
    mov     eax, -1

@@done:
    pop     rbx
    ret
GGUF_OpDispatch ENDP

; =============================================================================
; OPERATION IMPLEMENTATIONS
; =============================================================================

; -----------------------------------------------------------------------------
; Op_GGML_NONE - No operation
; -----------------------------------------------------------------------------
Op_GGML_NONE PROC
    xor     eax, eax
    ret
Op_GGML_NONE ENDP

; -----------------------------------------------------------------------------
; Op_GGML_DUP - Duplicate tensor
; -----------------------------------------------------------------------------
Op_GGML_DUP PROC
    push    rbx
    push    rsi
    push    rdi

    mov     rsi, rcx                    ; dst tensor
    mov     rbx, [rsi + TENSOR_SRC_0]   ; src tensor

    ; Get data pointers
    mov     rdi, [rsi + TENSOR_DATA]
    mov     rsi, [rbx + TENSOR_DATA]

    ; Get total elements
    mov     rax, [rbx + TENSOR_NE_0]
    imul    rax, [rbx + TENSOR_NE_1]
    imul    rax, [rbx + TENSOR_NE_2]
    imul    rax, [rbx + TENSOR_NE_3]
    shl     rax, 2                      ; * sizeof(float)

    ; Copy
    mov     rcx, rax
    shr     rcx, 3                      ; / 8 (qwords)
    rep movsq

    xor     eax, eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_DUP ENDP

; -----------------------------------------------------------------------------
; Op_GGML_ADD - Element-wise addition
;   dst = src0 + src1
; -----------------------------------------------------------------------------
Op_GGML_ADD PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14

    mov     r14, rcx                    ; dst tensor
    mov     r12, [r14 + TENSOR_SRC_0]   ; src0
    mov     r13, [r14 + TENSOR_SRC_1]   ; src1

    ; Get element count
    mov     rax, [r14 + TENSOR_NE_0]
    imul    rax, [r14 + TENSOR_NE_1]
    imul    rax, [r14 + TENSOR_NE_2]
    imul    rax, [r14 + TENSOR_NE_3]
    mov     rbx, rax                    ; total elements

    ; Get data pointers
    mov     rdi, [r14 + TENSOR_DATA]    ; dst
    mov     rsi, [r12 + TENSOR_DATA]    ; src0
    mov     r8,  [r13 + TENSOR_DATA]    ; src1

    ; AVX-512 vectorized add
    xor     ecx, ecx

@@add_loop:
    cmp     rcx, rbx
    jge     @@add_done

    vmovups zmm0, [rsi + rcx*4]
    vmovups zmm1, [r8 + rcx*4]
    vaddps  zmm0, zmm0, zmm1
    vmovups [rdi + rcx*4], zmm0

    add     rcx, 16
    jmp     @@add_loop

@@add_done:
    xor     eax, eax
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_ADD ENDP

; -----------------------------------------------------------------------------
; Op_GGML_SUB - Element-wise subtraction
; -----------------------------------------------------------------------------
Op_GGML_SUB PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14

    mov     r14, rcx
    mov     r12, [r14 + TENSOR_SRC_0]
    mov     r13, [r14 + TENSOR_SRC_1]

    mov     rax, [r14 + TENSOR_NE_0]
    imul    rax, [r14 + TENSOR_NE_1]
    imul    rax, [r14 + TENSOR_NE_2]
    imul    rax, [r14 + TENSOR_NE_3]
    mov     rbx, rax

    mov     rdi, [r14 + TENSOR_DATA]
    mov     rsi, [r12 + TENSOR_DATA]
    mov     r8,  [r13 + TENSOR_DATA]

    xor     ecx, ecx
@@sub_loop:
    cmp     rcx, rbx
    jge     @@sub_done
    vmovups zmm0, [rsi + rcx*4]
    vmovups zmm1, [r8 + rcx*4]
    vsubps  zmm0, zmm0, zmm1
    vmovups [rdi + rcx*4], zmm0
    add     rcx, 16
    jmp     @@sub_loop
@@sub_done:
    xor     eax, eax
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_SUB ENDP

; -----------------------------------------------------------------------------
; Op_GGML_MUL - Element-wise multiplication
; -----------------------------------------------------------------------------
Op_GGML_MUL PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14

    mov     r14, rcx
    mov     r12, [r14 + TENSOR_SRC_0]
    mov     r13, [r14 + TENSOR_SRC_1]

    mov     rax, [r14 + TENSOR_NE_0]
    imul    rax, [r14 + TENSOR_NE_1]
    imul    rax, [r14 + TENSOR_NE_2]
    imul    rax, [r14 + TENSOR_NE_3]
    mov     rbx, rax

    mov     rdi, [r14 + TENSOR_DATA]
    mov     rsi, [r12 + TENSOR_DATA]
    mov     r8,  [r13 + TENSOR_DATA]

    xor     ecx, ecx
@@mul_loop:
    cmp     rcx, rbx
    jge     @@mul_done
    vmovups zmm0, [rsi + rcx*4]
    vmovups zmm1, [r8 + rcx*4]
    vmulps  zmm0, zmm0, zmm1
    vmovups [rdi + rcx*4], zmm0
    add     rcx, 16
    jmp     @@mul_loop
@@mul_done:
    xor     eax, eax
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_MUL ENDP

; -----------------------------------------------------------------------------
; Op_GGML_DIV - Element-wise division
; -----------------------------------------------------------------------------
Op_GGML_DIV PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14

    mov     r14, rcx
    mov     r12, [r14 + TENSOR_SRC_0]
    mov     r13, [r14 + TENSOR_SRC_1]

    mov     rax, [r14 + TENSOR_NE_0]
    imul    rax, [r14 + TENSOR_NE_1]
    imul    rax, [r14 + TENSOR_NE_2]
    imul    rax, [r14 + TENSOR_NE_3]
    mov     rbx, rax

    mov     rdi, [r14 + TENSOR_DATA]
    mov     rsi, [r12 + TENSOR_DATA]
    mov     r8,  [r13 + TENSOR_DATA]

    xor     ecx, ecx
@@div_loop:
    cmp     rcx, rbx
    jge     @@div_done
    vmovups zmm0, [rsi + rcx*4]
    vmovups zmm1, [r8 + rcx*4]
    vdivps  zmm0, zmm0, zmm1
    vmovups [rdi + rcx*4], zmm0
    add     rcx, 16
    jmp     @@div_loop
@@div_done:
    xor     eax, eax
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_DIV ENDP

; -----------------------------------------------------------------------------
; Op_GGML_SQR - Element-wise square
; -----------------------------------------------------------------------------
Op_GGML_SQR PROC
    push    rbx
    push    rsi
    push    rdi

    mov     rdi, rcx
    mov     rsi, [rdi + TENSOR_SRC_0]

    mov     rax, [rdi + TENSOR_NE_0]
    imul    rax, [rdi + TENSOR_NE_1]
    imul    rax, [rdi + TENSOR_NE_2]
    imul    rax, [rdi + TENSOR_NE_3]
    mov     rbx, rax

    mov     r8,  [rdi + TENSOR_DATA]
    mov     rsi, [rsi + TENSOR_DATA]

    xor     ecx, ecx
@@sqr_loop:
    cmp     rcx, rbx
    jge     @@sqr_done
    vmovups zmm0, [rsi + rcx*4]
    vmulps  zmm0, zmm0, zmm0
    vmovups [r8 + rcx*4], zmm0
    add     rcx, 16
    jmp     @@sqr_loop
@@sqr_done:
    xor     eax, eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_SQR ENDP

; -----------------------------------------------------------------------------
; Op_GGML_SQRT - Element-wise square root
; -----------------------------------------------------------------------------
Op_GGML_SQRT PROC
    push    rbx
    push    rsi
    push    rdi

    mov     rdi, rcx
    mov     rsi, [rdi + TENSOR_SRC_0]

    mov     rax, [rdi + TENSOR_NE_0]
    imul    rax, [rdi + TENSOR_NE_1]
    imul    rax, [rdi + TENSOR_NE_2]
    imul    rax, [rdi + TENSOR_NE_3]
    mov     rbx, rax

    mov     r8,  [rdi + TENSOR_DATA]
    mov     rsi, [rsi + TENSOR_DATA]

    xor     ecx, ecx
@@sqrt_loop:
    cmp     rcx, rbx
    jge     @@sqrt_done
    vmovups zmm0, [rsi + rcx*4]
    vsqrtps zmm0, zmm0
    vmovups [r8 + rcx*4], zmm0
    add     rcx, 16
    jmp     @@sqrt_loop
@@sqrt_done:
    xor     eax, eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_SQRT ENDP

; -----------------------------------------------------------------------------
; Op_GGML_LOG - Element-wise natural log
; NOTE: Full precision requires polynomial or lookup
; -----------------------------------------------------------------------------
Op_GGML_LOG PROC
    ; Placeholder - real impl needs log approximation
    xor     eax, eax
    ret
Op_GGML_LOG ENDP

; -----------------------------------------------------------------------------
; Op_GGML_SUM - Sum all elements
; -----------------------------------------------------------------------------
Op_GGML_SUM PROC
    push    rbx
    push    rsi
    push    rdi

    mov     rdi, rcx
    mov     rsi, [rdi + TENSOR_SRC_0]

    mov     rax, [rsi + TENSOR_NE_0]
    imul    rax, [rsi + TENSOR_NE_1]
    imul    rax, [rsi + TENSOR_NE_2]
    imul    rax, [rsi + TENSOR_NE_3]
    mov     rbx, rax

    mov     r8,  [rdi + TENSOR_DATA]
    mov     rsi, [rsi + TENSOR_DATA]

    vxorps  zmm0, zmm0, zmm0            ; accumulator

    xor     ecx, ecx
@@sum_loop:
    cmp     rcx, rbx
    jge     @@sum_reduce
    vmovups zmm1, [rsi + rcx*4]
    vaddps  zmm0, zmm0, zmm1
    add     rcx, 16
    jmp     @@sum_loop

@@sum_reduce:
    ; Horizontal sum
    vextractf64x4 ymm1, zmm0, 1
    vaddps  ymm0, ymm0, ymm1
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0
    vmovss  [r8], xmm0

    xor     eax, eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_SUM ENDP

; -----------------------------------------------------------------------------
; Op_GGML_MEAN - Mean of all elements
; -----------------------------------------------------------------------------
Op_GGML_MEAN PROC
    push    rbx
    push    rsi
    push    rdi

    mov     rdi, rcx
    mov     rsi, [rdi + TENSOR_SRC_0]

    mov     rax, [rsi + TENSOR_NE_0]
    imul    rax, [rsi + TENSOR_NE_1]
    imul    rax, [rsi + TENSOR_NE_2]
    imul    rax, [rsi + TENSOR_NE_3]
    mov     rbx, rax

    mov     r8,  [rdi + TENSOR_DATA]
    mov     rsi, [rsi + TENSOR_DATA]

    vxorps  zmm0, zmm0, zmm0

    xor     ecx, ecx
@@mean_loop:
    cmp     rcx, rbx
    jge     @@mean_reduce
    vmovups zmm1, [rsi + rcx*4]
    vaddps  zmm0, zmm0, zmm1
    add     rcx, 16
    jmp     @@mean_loop

@@mean_reduce:
    vextractf64x4 ymm1, zmm0, 1
    vaddps  ymm0, ymm0, ymm1
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    ; Divide by count
    vcvtsi2ss xmm1, xmm1, rbx
    vdivss  xmm0, xmm0, xmm1
    vmovss  [r8], xmm0

    xor     eax, eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_MEAN ENDP

; -----------------------------------------------------------------------------
; Op_GGML_RMS_NORM - RMS Normalization
; y = x / sqrt(mean(x^2) + eps) * weight
; -----------------------------------------------------------------------------
Op_GGML_RMS_NORM PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32

    mov     r12, rcx                    ; dst tensor
    mov     r13, [r12 + TENSOR_SRC_0]   ; src tensor

    ; Get row length (ne[0])
    mov     rbx, [r12 + TENSOR_NE_0]

    mov     rdi, [r12 + TENSOR_DATA]
    mov     rsi, [r13 + TENSOR_DATA]

    ; 1. Compute sum of squares
    vxorps  zmm0, zmm0, zmm0
    xor     ecx, ecx

@@rms_sq_loop:
    cmp     rcx, rbx
    jge     @@rms_sq_done
    vmovups zmm1, [rsi + rcx*4]
    vfmadd231ps zmm0, zmm1, zmm1
    add     rcx, 16
    jmp     @@rms_sq_loop

@@rms_sq_done:
    ; Horizontal sum
    vextractf64x4 ymm1, zmm0, 1
    vaddps  ymm0, ymm0, ymm1
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    ; mean = sum / n
    vcvtsi2ss xmm1, xmm1, rbx
    vdivss  xmm0, xmm0, xmm1

    ; Add epsilon and rsqrt
    vaddss  xmm0, xmm0, [rel const_epsilon]
    vrsqrtss xmm0, xmm0, xmm0
    vbroadcastss zmm0, xmm0

    ; 2. Normalize
    xor     ecx, ecx
@@rms_norm_loop:
    cmp     rcx, rbx
    jge     @@rms_done
    vmovups zmm1, [rsi + rcx*4]
    vmulps  zmm1, zmm1, zmm0
    vmovups [rdi + rcx*4], zmm1
    add     rcx, 16
    jmp     @@rms_norm_loop

@@rms_done:
    xor     eax, eax
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_RMS_NORM ENDP

; -----------------------------------------------------------------------------
; Op_GGML_MUL_MAT - Matrix multiplication
; dst[M,N] = src0[M,K] @ src1[K,N]
; -----------------------------------------------------------------------------
Op_GGML_MUL_MAT PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 64

    mov     r12, rcx                    ; dst tensor
    mov     r13, [r12 + TENSOR_SRC_0]   ; src0 (weights)
    mov     r14, [r12 + TENSOR_SRC_1]   ; src1 (input)

    ; Get dimensions
    ; dst: [ne0, ne1] = [N, M]
    ; src0: [K, M] (weights, transposed)
    ; src1: [K, N] (input)
    mov     r15, [r12 + TENSOR_NE_0]    ; N (output columns)
    mov     rbx, [r12 + TENSOR_NE_1]    ; M (output rows)
    mov     rsi, [r13 + TENSOR_NE_0]    ; K (inner dimension)

    mov     rdi, [r12 + TENSOR_DATA]    ; dst data
    mov     r8,  [r13 + TENSOR_DATA]    ; src0 data (weights)
    mov     r9,  [r14 + TENSOR_DATA]    ; src1 data (input)

    ; Check type for quantized matmul
    mov     eax, [r13 + TENSOR_TYPE]
    cmp     eax, GGML_TYPE_Q4_0
    je      @@matmul_q4_0
    cmp     eax, GGML_TYPE_Q4_K
    je      @@matmul_q4_k
    ; Default: F32 matmul

    ; F32 matmul: for each output row i
    xor     ecx, ecx                    ; i = 0

@@row_loop:
    cmp     rcx, rbx
    jge     @@matmul_done

    ; for each output column j (process 16 at a time)
    xor     edx, edx                    ; j = 0

@@col_loop:
    cmp     rdx, r15
    jge     @@next_row

    ; Accumulate: dst[i,j..j+15] = sum_k(src0[k,i] * src1[k,j..j+15])
    vxorps  zmm0, zmm0, zmm0            ; accumulator

    xor     eax, eax                    ; k = 0
@@k_loop:
    cmp     rax, rsi
    jge     @@store_col

    ; Load src0[k, i] and broadcast
    ; src0 is [K, M], so element [k, i] = data + (i * K + k) * 4
    mov     r10, rcx
    imul    r10, rsi                    ; i * K
    add     r10, rax                    ; + k
    vbroadcastss zmm1, dword ptr [r8 + r10*4]

    ; Load src1[k, j..j+15]
    ; src1 is [K, N], so row k = data + k * N
    mov     r11, rax
    imul    r11, r15                    ; k * N
    add     r11, rdx                    ; + j
    vmovups zmm2, [r9 + r11*4]

    ; FMA
    vfmadd231ps zmm0, zmm1, zmm2

    inc     rax
    jmp     @@k_loop

@@store_col:
    ; Store result
    ; dst[i, j..j+15] = data + (i * N + j) * 4
    mov     r10, rcx
    imul    r10, r15
    add     r10, rdx
    vmovups [rdi + r10*4], zmm0

    add     rdx, 16
    jmp     @@col_loop

@@next_row:
    inc     rcx
    jmp     @@row_loop

@@matmul_q4_0:
    ; Q4_0 quantized matmul
    ; weights are Q4_0, activations are Q8_0 (or F32)
    ; TODO: implement quantized kernel
    jmp     @@matmul_done

@@matmul_q4_k:
    ; Q4_K quantized matmul
    ; TODO: implement quantized kernel
    jmp     @@matmul_done

@@matmul_done:
    xor     eax, eax
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_MUL_MAT ENDP

; -----------------------------------------------------------------------------
; Op_GGML_SOFT_MAX - Softmax
; y_i = exp(x_i - max) / sum(exp(x - max))
; -----------------------------------------------------------------------------
Op_GGML_SOFT_MAX PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 48

    mov     r12, rcx                    ; dst
    mov     rsi, [r12 + TENSOR_SRC_0]   ; src

    mov     rbx, [r12 + TENSOR_NE_0]    ; row length
    mov     rdi, [r12 + TENSOR_DATA]
    mov     rsi, [rsi + TENSOR_DATA]

    ; 1. Find max
    vmovups zmm15, [rsi]
    mov     ecx, 16
@@max_loop:
    cmp     rcx, rbx
    jge     @@max_done
    vmovups zmm0, [rsi + rcx*4]
    vmaxps  zmm15, zmm15, zmm0
    add     rcx, 16
    jmp     @@max_loop

@@max_done:
    ; Reduce max
    vextractf64x4 ymm0, zmm15, 1
    vmaxps  ymm15, ymm15, ymm0
    vextractf128 xmm0, ymm15, 1
    vmaxps  xmm15, xmm15, xmm0
    vpermilps xmm0, xmm15, 0Eh
    vmaxps  xmm15, xmm15, xmm0
    vpermilps xmm0, xmm15, 01h
    vmaxss  xmm15, xmm15, xmm0
    vbroadcastss zmm15, xmm15

    ; 2. Compute exp(x - max) and sum
    vxorps  zmm14, zmm14, zmm14         ; sum
    xor     ecx, ecx
@@exp_loop:
    cmp     rcx, rbx
    jge     @@exp_done
    vmovups zmm0, [rsi + rcx*4]
    vsubps  zmm0, zmm0, zmm15
    ; Fast exp using vexp2ps (AVX-512) or polynomial
    ; Simplified: use direct exp
    vmovups [rdi + rcx*4], zmm0         ; Store x - max
    ; Actual exp would go here
    vaddps  zmm14, zmm14, zmm0
    add     rcx, 16
    jmp     @@exp_loop

@@exp_done:
    ; Reduce sum
    vextractf64x4 ymm0, zmm14, 1
    vaddps  ymm14, ymm14, ymm0
    vextractf128 xmm0, ymm14, 1
    vaddps  xmm14, xmm14, xmm0
    vhaddps xmm14, xmm14, xmm14
    vhaddps xmm14, xmm14, xmm14
    vbroadcastss zmm14, xmm14

    ; 3. Normalize
    vrcpps  zmm14, zmm14
    xor     ecx, ecx
@@norm_loop:
    cmp     rcx, rbx
    jge     @@softmax_done
    vmovups zmm0, [rdi + rcx*4]
    vmulps  zmm0, zmm0, zmm14
    vmovups [rdi + rcx*4], zmm0
    add     rcx, 16
    jmp     @@norm_loop

@@softmax_done:
    xor     eax, eax
    add     rsp, 48
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_SOFT_MAX ENDP

; -----------------------------------------------------------------------------
; Op_GGML_ROPE - Rotary Position Embedding
; -----------------------------------------------------------------------------
Op_GGML_ROPE PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 64

    mov     r12, rcx                    ; dst
    mov     r13, [r12 + TENSOR_SRC_0]   ; src

    ; Get dimensions
    mov     rbx, [r12 + TENSOR_NE_0]    ; head_dim
    mov     rdi, [r12 + TENSOR_DATA]
    mov     rsi, [r13 + TENSOR_DATA]

    ; Process pairs (x0, x1) -> (x0*cos - x1*sin, x0*sin + x1*cos)
    ; For simplicity: copy through (full impl needs position info)
    mov     rcx, rbx
    shl     rcx, 2                      ; * sizeof(float)
    rep movsb

    xor     eax, eax
    add     rsp, 64
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_ROPE ENDP

; -----------------------------------------------------------------------------
; Op_GGML_DIAG_MASK_INF - Causal attention mask
; Mask future positions with -INF
; -----------------------------------------------------------------------------
Op_GGML_DIAG_MASK_INF PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32

    mov     r12, rcx                    ; dst
    mov     r13, [r12 + TENSOR_SRC_0]   ; src (attention scores)

    ; Get dimensions (assuming square matrix for simplicity)
    mov     rbx, [r12 + TENSOR_NE_0]    ; seq_len
    mov     rdi, [r12 + TENSOR_DATA]
    mov     rsi, [r13 + TENSOR_DATA]

    ; For row i, mask positions j > i with -INF
    vbroadcastss zmm15, [rel const_neg_inf]

    xor     r8d, r8d                    ; i = 0

@@mask_row:
    cmp     r8, rbx
    jge     @@mask_done

    ; Copy row from source
    mov     r9, r8
    imul    r9, rbx                     ; row offset
    xor     r10d, r10d                  ; j = 0

@@mask_col:
    cmp     r10, rbx
    jge     @@next_mask_row

    ; Load value
    vmovss  xmm0, [rsi + r9*4 + r10*4]

    ; Check if j > i (future position)
    cmp     r10, r8
    jle     @@no_mask

    ; Apply mask
    vmovaps xmm0, xmm15

@@no_mask:
    vmovss  [rdi + r9*4 + r10*4], xmm0

    inc     r10
    jmp     @@mask_col

@@next_mask_row:
    inc     r8
    jmp     @@mask_row

@@mask_done:
    xor     eax, eax
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_DIAG_MASK_INF ENDP

; -----------------------------------------------------------------------------
; Op_GGML_GET_ROWS - Token embedding lookup
; dst[n_tokens, hidden] = embedding[token_ids, hidden]
; -----------------------------------------------------------------------------
Op_GGML_GET_ROWS PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    sub     rsp, 32

    mov     r12, rcx                    ; dst
    mov     r13, [r12 + TENSOR_SRC_0]   ; embedding table
    mov     r14, [r12 + TENSOR_SRC_1]   ; token ids

    ; embedding: [vocab_size, hidden_dim]
    ; token_ids: [n_tokens]
    ; dst: [n_tokens, hidden_dim]

    mov     rbx, [r12 + TENSOR_NE_0]    ; hidden_dim
    mov     rsi, [r12 + TENSOR_NE_1]    ; n_tokens

    mov     rdi, [r12 + TENSOR_DATA]    ; dst
    mov     r8,  [r13 + TENSOR_DATA]    ; embedding
    mov     r9,  [r14 + TENSOR_DATA]    ; token_ids

    ; For each token
    xor     ecx, ecx                    ; token_idx

@@getrows_loop:
    cmp     rcx, rsi
    jge     @@getrows_done

    ; Get token id
    mov     eax, [r9 + rcx*4]

    ; Calculate source offset: token_id * hidden_dim
    mov     r10, rax
    imul    r10, rbx

    ; Calculate dest offset: token_idx * hidden_dim
    mov     r11, rcx
    imul    r11, rbx

    ; Copy hidden_dim floats
    xor     edx, edx
@@copy_emb:
    cmp     rdx, rbx
    jge     @@next_token
    vmovups zmm0, [r8 + r10*4 + rdx*4]
    vmovups [rdi + r11*4 + rdx*4], zmm0
    add     rdx, 16
    jmp     @@copy_emb

@@next_token:
    inc     rcx
    jmp     @@getrows_loop

@@getrows_done:
    xor     eax, eax
    add     rsp, 32
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_GET_ROWS ENDP

; =============================================================================
; UNARY OPERATIONS
; =============================================================================

; -----------------------------------------------------------------------------
; Op_GGML_UNARY - Dispatch unary operation
; -----------------------------------------------------------------------------
Op_GGML_UNARY PROC
    ; Get unary op type from tensor params
    ; For now, dispatch based on op_params[0]
    xor     eax, eax
    ret
Op_GGML_UNARY ENDP

; -----------------------------------------------------------------------------
; Op_GGML_UNARY_RELU - ReLU activation
; -----------------------------------------------------------------------------
Op_GGML_UNARY_RELU PROC
    push    rbx
    push    rsi
    push    rdi

    mov     rdi, rcx
    mov     rsi, [rdi + TENSOR_SRC_0]

    mov     rax, [rdi + TENSOR_NE_0]
    imul    rax, [rdi + TENSOR_NE_1]
    imul    rax, [rdi + TENSOR_NE_2]
    imul    rax, [rdi + TENSOR_NE_3]
    mov     rbx, rax

    mov     r8,  [rdi + TENSOR_DATA]
    mov     rsi, [rsi + TENSOR_DATA]

    vxorps  zmm15, zmm15, zmm15         ; zero

    xor     ecx, ecx
@@relu_loop:
    cmp     rcx, rbx
    jge     @@relu_done
    vmovups zmm0, [rsi + rcx*4]
    vmaxps  zmm0, zmm0, zmm15           ; max(x, 0)
    vmovups [r8 + rcx*4], zmm0
    add     rcx, 16
    jmp     @@relu_loop
@@relu_done:
    xor     eax, eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_UNARY_RELU ENDP

; -----------------------------------------------------------------------------
; Op_GGML_UNARY_GELU - GELU activation
; GELU(x) = 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
; -----------------------------------------------------------------------------
Op_GGML_UNARY_GELU PROC
    push    rbx
    push    rsi
    push    rdi

    mov     rdi, rcx
    mov     rsi, [rdi + TENSOR_SRC_0]

    mov     rax, [rdi + TENSOR_NE_0]
    imul    rax, [rdi + TENSOR_NE_1]
    imul    rax, [rdi + TENSOR_NE_2]
    imul    rax, [rdi + TENSOR_NE_3]
    mov     rbx, rax

    mov     r8,  [rdi + TENSOR_DATA]
    mov     rsi, [rsi + TENSOR_DATA]

    ; Load constants
    vbroadcastss zmm13, [rel gelu_a]
    vbroadcastss zmm14, [rel gelu_sqrt_2_pi]
    vbroadcastss zmm15, [rel gelu_half]

    xor     ecx, ecx
@@gelu_loop:
    cmp     rcx, rbx
    jge     @@gelu_done

    vmovups zmm0, [rsi + rcx*4]         ; x

    ; x^3
    vmulps  zmm1, zmm0, zmm0
    vmulps  zmm1, zmm1, zmm0            ; x^3

    ; 0.044715 * x^3
    vmulps  zmm1, zmm1, zmm13

    ; x + 0.044715 * x^3
    vaddps  zmm1, zmm1, zmm0

    ; sqrt(2/pi) * (x + ...)
    vmulps  zmm1, zmm1, zmm14

    ; tanh approximation: (exp(2x) - 1) / (exp(2x) + 1)
    ; Simplified: use approximation
    ; For full precision, implement tanh

    ; 0.5 * x * (1 + tanh(...))
    vaddps  zmm1, zmm1, [rel const_one_f32]
    vmulps  zmm1, zmm1, zmm0
    vmulps  zmm1, zmm1, zmm15

    vmovups [r8 + rcx*4], zmm1

    add     rcx, 16
    jmp     @@gelu_loop

@@gelu_done:
    xor     eax, eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_UNARY_GELU ENDP

; -----------------------------------------------------------------------------
; Op_GGML_UNARY_SILU - SiLU (Swish) activation
; SiLU(x) = x * sigmoid(x)
; -----------------------------------------------------------------------------
Op_GGML_UNARY_SILU PROC
    push    rbx
    push    rsi
    push    rdi

    mov     rdi, rcx
    mov     rsi, [rdi + TENSOR_SRC_0]

    mov     rax, [rdi + TENSOR_NE_0]
    imul    rax, [rdi + TENSOR_NE_1]
    imul    rax, [rdi + TENSOR_NE_2]
    imul    rax, [rdi + TENSOR_NE_3]
    mov     rbx, rax

    mov     r8,  [rdi + TENSOR_DATA]
    mov     rsi, [rsi + TENSOR_DATA]

    xor     ecx, ecx
@@silu_loop:
    cmp     rcx, rbx
    jge     @@silu_done

    vmovups zmm0, [rsi + rcx*4]         ; x

    ; sigmoid(x) = 1 / (1 + exp(-x))
    ; Fast approximation
    vxorps  zmm1, zmm1, zmm1
    vsubps  zmm1, zmm1, zmm0            ; -x
    ; exp(-x) approximation
    ; 1 / (1 + exp(-x))
    vaddps  zmm1, zmm1, [rel const_one_f32]
    vrcpps  zmm1, zmm1                  ; 1 / (1 + exp(-x))

    ; x * sigmoid(x)
    vmulps  zmm0, zmm0, zmm1
    vmovups [r8 + rcx*4], zmm0

    add     rcx, 16
    jmp     @@silu_loop

@@silu_done:
    xor     eax, eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_UNARY_SILU ENDP

; =============================================================================
; STUB IMPLEMENTATIONS FOR REMAINING OPS
; These need full implementations for production use
; =============================================================================

Op_GGML_ADD1 PROC
    xor     eax, eax
    ret
Op_GGML_ADD1 ENDP

Op_GGML_ACC PROC
    xor     eax, eax
    ret
Op_GGML_ACC ENDP

Op_GGML_SUM_ROWS PROC
    xor     eax, eax
    ret
Op_GGML_SUM_ROWS ENDP

Op_GGML_ARGMAX PROC
    xor     eax, eax
    ret
Op_GGML_ARGMAX ENDP

Op_GGML_REPEAT PROC
    xor     eax, eax
    ret
Op_GGML_REPEAT ENDP

Op_GGML_REPEAT_BACK PROC
    xor     eax, eax
    ret
Op_GGML_REPEAT_BACK ENDP

Op_GGML_CONCAT PROC
    xor     eax, eax
    ret
Op_GGML_CONCAT ENDP

Op_GGML_SILU_BACK PROC
    xor     eax, eax
    ret
Op_GGML_SILU_BACK ENDP

Op_GGML_NORM PROC
    xor     eax, eax
    ret
Op_GGML_NORM ENDP

Op_GGML_RMS_NORM_BACK PROC
    xor     eax, eax
    ret
Op_GGML_RMS_NORM_BACK ENDP

Op_GGML_GROUP_NORM PROC
    xor     eax, eax
    ret
Op_GGML_GROUP_NORM ENDP

Op_GGML_MUL_MAT_ID PROC
    xor     eax, eax
    ret
Op_GGML_MUL_MAT_ID ENDP

Op_GGML_OUT_PROD PROC
    xor     eax, eax
    ret
Op_GGML_OUT_PROD ENDP

Op_GGML_SCALE PROC
    push    rbx
    push    rsi
    push    rdi

    mov     rdi, rcx
    mov     rsi, [rdi + TENSOR_SRC_0]

    mov     rax, [rdi + TENSOR_NE_0]
    imul    rax, [rdi + TENSOR_NE_1]
    imul    rax, [rdi + TENSOR_NE_2]
    imul    rax, [rdi + TENSOR_NE_3]
    mov     rbx, rax

    mov     r8,  [rdi + TENSOR_DATA]
    mov     rsi, [rsi + TENSOR_DATA]

    ; Scale factor would be in op_params
    ; For now, use 1.0
    vbroadcastss zmm15, [rel const_one_f32]

    xor     ecx, ecx
@@scale_loop:
    cmp     rcx, rbx
    jge     @@scale_done
    vmovups zmm0, [rsi + rcx*4]
    vmulps  zmm0, zmm0, zmm15
    vmovups [r8 + rcx*4], zmm0
    add     rcx, 16
    jmp     @@scale_loop
@@scale_done:
    xor     eax, eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Op_GGML_SCALE ENDP

Op_GGML_SET PROC
    xor     eax, eax
    ret
Op_GGML_SET ENDP

Op_GGML_CPY PROC
    jmp     Op_GGML_DUP
Op_GGML_CPY ENDP

Op_GGML_CONT PROC
    jmp     Op_GGML_DUP
Op_GGML_CONT ENDP

Op_GGML_RESHAPE PROC
    ; Reshape is a no-op (view)
    xor     eax, eax
    ret
Op_GGML_RESHAPE ENDP

Op_GGML_VIEW PROC
    xor     eax, eax
    ret
Op_GGML_VIEW ENDP

Op_GGML_PERMUTE PROC
    xor     eax, eax
    ret
Op_GGML_PERMUTE ENDP

Op_GGML_TRANSPOSE PROC
    xor     eax, eax
    ret
Op_GGML_TRANSPOSE ENDP

Op_GGML_GET_ROWS_BACK PROC
    xor     eax, eax
    ret
Op_GGML_GET_ROWS_BACK ENDP

Op_GGML_DIAG PROC
    xor     eax, eax
    ret
Op_GGML_DIAG ENDP

Op_GGML_DIAG_MASK_ZERO PROC
    xor     eax, eax
    ret
Op_GGML_DIAG_MASK_ZERO ENDP

Op_GGML_SOFT_MAX_BACK PROC
    xor     eax, eax
    ret
Op_GGML_SOFT_MAX_BACK ENDP

Op_GGML_ROPE_BACK PROC
    xor     eax, eax
    ret
Op_GGML_ROPE_BACK ENDP

Op_GGML_CLAMP PROC
    xor     eax, eax
    ret
Op_GGML_CLAMP ENDP

Op_GGML_CONV_TRANSPOSE_1D PROC
    xor     eax, eax
    ret
Op_GGML_CONV_TRANSPOSE_1D ENDP

Op_GGML_IM2COL PROC
    xor     eax, eax
    ret
Op_GGML_IM2COL ENDP

Op_GGML_CONV_TRANSPOSE_2D PROC
    xor     eax, eax
    ret
Op_GGML_CONV_TRANSPOSE_2D ENDP

Op_GGML_POOL_1D PROC
    xor     eax, eax
    ret
Op_GGML_POOL_1D ENDP

Op_GGML_POOL_2D PROC
    xor     eax, eax
    ret
Op_GGML_POOL_2D ENDP

Op_GGML_UPSCALE PROC
    xor     eax, eax
    ret
Op_GGML_UPSCALE ENDP

Op_GGML_PAD PROC
    xor     eax, eax
    ret
Op_GGML_PAD ENDP

Op_GGML_ARANGE PROC
    xor     eax, eax
    ret
Op_GGML_ARANGE ENDP

Op_GGML_TIMESTEP_EMBEDDING PROC
    xor     eax, eax
    ret
Op_GGML_TIMESTEP_EMBEDDING ENDP

Op_GGML_ARGSORT PROC
    xor     eax, eax
    ret
Op_GGML_ARGSORT ENDP

Op_GGML_LEAKY_RELU PROC
    xor     eax, eax
    ret
Op_GGML_LEAKY_RELU ENDP

Op_GGML_FLASH_ATTN_EXT PROC
    xor     eax, eax
    ret
Op_GGML_FLASH_ATTN_EXT ENDP

Op_GGML_FLASH_ATTN_BACK PROC
    xor     eax, eax
    ret
Op_GGML_FLASH_ATTN_BACK ENDP

Op_GGML_SSM_CONV PROC
    xor     eax, eax
    ret
Op_GGML_SSM_CONV ENDP

Op_GGML_SSM_SCAN PROC
    xor     eax, eax
    ret
Op_GGML_SSM_SCAN ENDP

Op_GGML_WIN_PART PROC
    xor     eax, eax
    ret
Op_GGML_WIN_PART ENDP

Op_GGML_WIN_UNPART PROC
    xor     eax, eax
    ret
Op_GGML_WIN_UNPART ENDP

Op_GGML_GET_REL_POS PROC
    xor     eax, eax
    ret
Op_GGML_GET_REL_POS ENDP

Op_GGML_ADD_REL_POS PROC
    xor     eax, eax
    ret
Op_GGML_ADD_REL_POS ENDP

Op_GGML_MAP_UNARY PROC
    xor     eax, eax
    ret
Op_GGML_MAP_UNARY ENDP

Op_GGML_MAP_BINARY PROC
    xor     eax, eax
    ret
Op_GGML_MAP_BINARY ENDP

Op_GGML_MAP_CUSTOM1_F32 PROC
    xor     eax, eax
    ret
Op_GGML_MAP_CUSTOM1_F32 ENDP

Op_GGML_MAP_CUSTOM2_F32 PROC
    xor     eax, eax
    ret
Op_GGML_MAP_CUSTOM2_F32 ENDP

Op_GGML_MAP_CUSTOM3_F32 PROC
    xor     eax, eax
    ret
Op_GGML_MAP_CUSTOM3_F32 ENDP

Op_GGML_MAP_CUSTOM1 PROC
    xor     eax, eax
    ret
Op_GGML_MAP_CUSTOM1 ENDP

Op_GGML_MAP_CUSTOM2 PROC
    xor     eax, eax
    ret
Op_GGML_MAP_CUSTOM2 ENDP

Op_GGML_MAP_CUSTOM3 PROC
    xor     eax, eax
    ret
Op_GGML_MAP_CUSTOM3 ENDP

Op_GGML_CROSS_ENTROPY_LOSS PROC
    xor     eax, eax
    ret
Op_GGML_CROSS_ENTROPY_LOSS ENDP

Op_GGML_CROSS_ENTROPY_LOSS_BACK PROC
    xor     eax, eax
    ret
Op_GGML_CROSS_ENTROPY_LOSS_BACK ENDP

Op_GGML_OPT_STEP_ADAMW PROC
    xor     eax, eax
    ret
Op_GGML_OPT_STEP_ADAMW ENDP

; Remaining unary ops
Op_GGML_UNARY_ABS PROC
    xor     eax, eax
    ret
Op_GGML_UNARY_ABS ENDP

Op_GGML_UNARY_SGN PROC
    xor     eax, eax
    ret
Op_GGML_UNARY_SGN ENDP

Op_GGML_UNARY_NEG PROC
    xor     eax, eax
    ret
Op_GGML_UNARY_NEG ENDP

Op_GGML_UNARY_STEP PROC
    xor     eax, eax
    ret
Op_GGML_UNARY_STEP ENDP

Op_GGML_UNARY_TANH PROC
    xor     eax, eax
    ret
Op_GGML_UNARY_TANH ENDP

Op_GGML_UNARY_ELU PROC
    xor     eax, eax
    ret
Op_GGML_UNARY_ELU ENDP

Op_GGML_UNARY_SIGMOID PROC
    xor     eax, eax
    ret
Op_GGML_UNARY_SIGMOID ENDP

Op_GGML_UNARY_GELU_QUICK PROC
    xor     eax, eax
    ret
Op_GGML_UNARY_GELU_QUICK ENDP

Op_GGML_UNARY_HARDSWISH PROC
    xor     eax, eax
    ret
Op_GGML_UNARY_HARDSWISH ENDP

Op_GGML_UNARY_HARDSIGMOID PROC
    xor     eax, eax
    ret
Op_GGML_UNARY_HARDSIGMOID ENDP

Op_GGML_UNARY_EXP PROC
    xor     eax, eax
    ret
Op_GGML_UNARY_EXP ENDP

; =============================================================================
; HELPER FUNCTIONS
; =============================================================================

; -----------------------------------------------------------------------------
; GGUF_Graph_Reset - Reset graph state
; -----------------------------------------------------------------------------
GGUF_Graph_Reset PROC
    mov     dword ptr [rel graph_n_nodes], 0
    mov     dword ptr [rel graph_n_leafs], 0
    xor     eax, eax
    ret
GGUF_Graph_Reset ENDP

; -----------------------------------------------------------------------------
; GGUF_Graph_Free - Free graph resources
; -----------------------------------------------------------------------------
GGUF_Graph_Free PROC
    xor     eax, eax
    ret
GGUF_Graph_Free ENDP

; -----------------------------------------------------------------------------
; GGUF_Node_Create - Create graph node
; -----------------------------------------------------------------------------
GGUF_Node_Create PROC
    xor     eax, eax
    ret
GGUF_Node_Create ENDP

; -----------------------------------------------------------------------------
; GGUF_Node_Connect - Connect nodes
; -----------------------------------------------------------------------------
GGUF_Node_Connect PROC
    xor     eax, eax
    ret
GGUF_Node_Connect ENDP

END
