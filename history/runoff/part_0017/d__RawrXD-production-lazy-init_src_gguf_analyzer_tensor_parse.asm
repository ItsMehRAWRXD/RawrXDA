; ════════════════════════════════════════════════════════════════════════════════
; Tensor Parsing - Tensor Table Only
; ════════════════════════════════════════════════════════════════════════════════

.data
    g_Tensors           TensorMetadata MAX_TENSORS dup(<>)
    g_TensorCount       dq 0

.code

ParseTensorTable PROC
    ; Shadow space + local variables + alignment
    sub rsp, 40h

    ; Parse tensor table
TensorLoop:
    ; Check tensor count limit
    cmp g_TensorCount, MAX_TENSORS
    jae TensorLimitExceeded

    ; Read tensor metadata
    ; ... (tensor metadata parsing logic here) ...

    ; Increment tensor count
    inc g_TensorCount

    ; Loop to next tensor
    ; ... (loop logic here) ...

TensorLimitExceeded:
    ; Handle tensor limit exceeded error
    ; ... (error handling logic here) ...

    add rsp, 40h
    ret
ParseTensorTable ENDP

ParseTensorMetadata PROC
    ; rsi = pointer to tensor metadata
    ; rcx = tensor index

    ; Read tensor name length
    mov eax, dword ptr [rsi + TENSOR_NAME_LEN_OFF]
    mov [g_Tensors + rcx * SIZEOF TensorMetadata].name_len, rax

    ; Copy tensor name
    lea rdi, [g_Tensors + rcx * SIZEOF TensorMetadata].tensor_name
    lea rsi, [rsi + TENSOR_NAME_OFF]
    mov ecx, MAX_NAME_LEN
    rep movsb

    ; Read number of dimensions
    mov eax, dword ptr [rsi + TENSOR_NDIMS_OFF]
    mov [g_Tensors + rcx * SIZEOF TensorMetadata].n_dimensions, rax

    ; Copy dimensions
    lea rdi, [g_Tensors + rcx * SIZEOF TensorMetadata].dimensions
    lea rsi, [rsi + TENSOR_DIMS_OFF]
    mov ecx, 8
    rep movsq

    ; Read data type
    mov eax, dword ptr [rsi + TENSOR_DTYPE_OFF]
    mov [g_Tensors + rcx * SIZEOF TensorMetadata].dtype, rax

    ; Read tensor offset
    mov rax, qword ptr [rsi + TENSOR_OFFSET_OFF]
    mov [g_Tensors + rcx * SIZEOF TensorMetadata].tensor_offset, rax

    ret
ParseTensorMetadata ENDP

END ParseTensorTable