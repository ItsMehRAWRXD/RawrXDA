; =============================================================================
; gguf_parser.asm — Real GGUF Header Parser (Pure x64 MASM)
; Reads GGUF magic, version, tensor count, metadata from memory-mapped file
; =============================================================================
; Exports:
;   int32_t GGUF_ParseHeader(void* mappedBase, uint64_t fileSize, GGUF_Info* out);
;   int32_t GGUF_GetTensorInfo(void* mappedBase, uint32_t tensorIdx, GGUF_Tensor* out);
; =============================================================================

include rawrxd_win64.inc

; ---------------------------------------------------------------------------
; GGUF format constants
; ---------------------------------------------------------------------------
GGUF_MAGIC              EQU 46554747h   ; "GGUF" little-endian
GGUF_DEFAULT_ALIGNMENT  EQU 32

; GGML types (subset)
GGML_TYPE_F32           EQU 0
GGML_TYPE_F16           EQU 1
GGML_TYPE_Q4_0          EQU 2
GGML_TYPE_Q4_1          EQU 3
GGML_TYPE_Q5_0          EQU 6
GGML_TYPE_Q5_1          EQU 7
GGML_TYPE_Q8_0          EQU 8
GGML_TYPE_Q8_1          EQU 9

; ---------------------------------------------------------------------------
; C-compatible structures (packed)
; ---------------------------------------------------------------------------
; GGUF_Info (48 bytes)
GINFO_MAGIC             EQU 0
GINFO_VERSION           EQU 4
GINFO_TENSOR_COUNT      EQU 8
GINFO_METADATA_COUNT    EQU 12
GINFO_HEADER_SIZE       EQU 16
GINFO_TENSOR_OFFSET     EQU 24
GINFO_METADATA_OFFSET   EQU 32
GINFO_ALIGNMENT         EQU 40

; GGUF_Tensor (80 bytes)
GTENSOR_NAME_LEN        EQU 0
GTENSOR_NAME_PTR        EQU 8
GTENSOR_NDIMS           EQU 16
GTENSOR_DIMS            EQU 20
GTENSOR_TYPE            EQU 52
GTENSOR_OFFSET          EQU 56
GTENSOR_SIZE            EQU 64
GTENSOR_DATA_PTR        EQU 72

; ---------------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------------
.data
align 8
sz_gguf_ok              db "[GGUF] Parsed: %u tensors, %u metadata, align=%u", 10, 0
sz_gguf_bad_magic       db "[GGUF] Bad magic: expected 0x%08X, got 0x%08X", 10, 0
sz_gguf_too_small       db "[GGUF] File too small for header", 10, 0
sz_gguf_bad_version     db "[GGUF] Unsupported version: %u", 10, 0

; ---------------------------------------------------------------------------
; Code
; ---------------------------------------------------------------------------
.code

; =============================================================================
; GGUF_ParseHeader
;   RCX = void* mappedBase
;   RDX = uint64_t fileSize
;   R8  = GGUF_Info* out
;   Returns EAX = 0 on success, -1 on error
; =============================================================================
align 16
PUBLIC GGUF_ParseHeader
GGUF_ParseHeader PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 40
    .allocstack 40
    .endprolog

    mov rbx, rcx                        ; RBX = mappedBase
    mov r12, rdx                        ; R12 = fileSize
    mov r13, r8                         ; R13 = out

    ; Validate minimum size (magic + version + counts = 24 bytes)
    cmp r12, 24
    jb _gguf_too_small

    ; Check magic
    mov eax, DWORD PTR [rbx]
    cmp eax, GGUF_MAGIC
    je _gguf_magic_ok

    ; Log bad magic
    lea rcx, sz_gguf_bad_magic
    call OutputDebugStringA
    mov eax, -1
    jmp _gguf_done

_gguf_magic_ok:
    ; Read version (offset 4) — little-endian uint32
    mov eax, DWORD PTR [rbx+4]
    cmp eax, 3                          ; Support v3
    jbe _gguf_version_ok

    lea rcx, sz_gguf_bad_version
    call OutputDebugStringA
    mov eax, -1
    jmp _gguf_done

_gguf_version_ok:
    ; Read tensor count (offset 8)
    mov esi, DWORD PTR [rbx+8]
    ; Read metadata count (offset 12)
    mov edi, DWORD PTR [rbx+12]

    ; Store to output struct
    mov DWORD PTR [r13+GINFO_MAGIC], GGUF_MAGIC
    mov DWORD PTR [r13+GINFO_VERSION], eax
    mov DWORD PTR [r13+GINFO_TENSOR_COUNT], esi
    mov DWORD PTR [r13+GINFO_METADATA_COUNT], edi
    mov DWORD PTR [r13+GINFO_ALIGNMENT], GGUF_DEFAULT_ALIGNMENT

    ; Header = magic(4) + version(4) + tensor_count(4) + metadata_count(4) = 16
    mov rax, 16
    mov QWORD PTR [r13+GINFO_HEADER_SIZE], rax
    mov QWORD PTR [r13+GINFO_TENSOR_OFFSET], rax

    ; Approximate metadata offset (after tensor info array)
    mov rcx, rsi
    imul rcx, rcx, 64
    add rax, rcx
    mov QWORD PTR [r13+GINFO_METADATA_OFFSET], rax

    ; Log success
    lea rcx, sz_gguf_ok
    call OutputDebugStringA

    xor eax, eax                        ; Success

_gguf_done:
    add rsp, 40
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

_gguf_too_small:
    lea rcx, sz_gguf_too_small
    call OutputDebugStringA
    mov eax, -1
    jmp _gguf_done
GGUF_ParseHeader ENDP


; =============================================================================
; GGUF_GetTensorInfo
;   RCX = void* mappedBase
;   RDX = uint32_t tensorIdx
;   R8  = GGUF_Info* info (from ParseHeader)
;   R9  = GGUF_Tensor* out
;   Returns EAX = 0 on success, -1 on error
; =============================================================================
align 16
PUBLIC GGUF_GetTensorInfo
GGUF_GetTensorInfo PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 40
    .allocstack 40
    .endprolog

    mov rbx, rcx                        ; RBX = mappedBase
    mov r12d, edx                       ; R12 = tensorIdx
    mov r13, r8                         ; R13 = info
    mov r14, r9                         ; R14 = out

    ; Validate index
    cmp r12d, DWORD PTR [r13+GINFO_TENSOR_COUNT]
    jae _tensor_bad_idx

    ; Walk tensor infos to find offset of requested tensor
    ; Start at tensor_offset
    mov rsi, QWORD PTR [r13+GINFO_TENSOR_OFFSET]
    add rsi, rbx                        ; RSI = current pointer in mapped file

    xor rdi, rdi                        ; RDI = current tensor index

_tensor_walk:
    cmp edi, r12d
    jae _tensor_found

    ; Skip this tensor's info
    ; name_len (uint64)
    mov rcx, QWORD PTR [rsi]
    add rsi, 8
    ; name string (name_len bytes)
    add rsi, rcx
    ; n_dims (uint32)
    mov eax, DWORD PTR [rsi]
    add rsi, 4
    ; dims (n_dims * uint64)
    mov rcx, rax
    shl rax, 3                          ; *8
    add rsi, rax
    ; type (uint32)
    add rsi, 4
    ; offset (uint64)
    add rsi, 8

    inc edi
    jmp _tensor_walk

_tensor_found:
    ; RSI now points to start of requested tensor info
    ; name_len
    mov rcx, QWORD PTR [rsi]
    mov QWORD PTR [r14+GTENSOR_NAME_LEN], rcx
    add rsi, 8
    ; name_ptr (offset into mapped file)
    mov rax, rsi
    sub rax, rbx
    mov QWORD PTR [r14+GTENSOR_NAME_PTR], rax
    add rsi, rcx                        ; Skip name string
    ; n_dims
    mov eax, DWORD PTR [rsi]
    mov DWORD PTR [r14+GTENSOR_NDIMS], eax
    add rsi, 4
    ; dims[0..3]
    mov rcx, 4
    xor rdx, rdx
_tensor_dims:
    cmp edx, eax
    jae _tensor_dims_pad
    mov r8, QWORD PTR [rsi + rdx*8]
    mov QWORD PTR [r14+GTENSOR_DIMS + rdx*8], r8
    inc edx
    jmp _tensor_dims
_tensor_dims_pad:
    cmp edx, 4
    jae _tensor_type
    mov QWORD PTR [r14+GTENSOR_DIMS + rdx*8], 0
    inc edx
    jmp _tensor_dims_pad
_tensor_type:
    ; type
    mov eax, DWORD PTR [rsi]
    mov DWORD PTR [r14+GTENSOR_TYPE], eax
    add rsi, 4
    ; offset
    mov rax, QWORD PTR [rsi]
    mov QWORD PTR [r14+GTENSOR_OFFSET], rax
    add rsi, 8

    ; Calculate data pointer = mappedBase + tensor_offset + alignment_pad
    mov rax, QWORD PTR [r14+GTENSOR_OFFSET]
    add rax, rbx
    mov QWORD PTR [r14+GTENSOR_DATA_PTR], rax

    ; Calculate size (simplified: product of dims * type_size)
    ; For F32: 4 bytes per element
    mov eax, DWORD PTR [r14+GTENSOR_TYPE]
    cmp eax, GGML_TYPE_F32
    jne _tensor_size_f16
    mov ecx, 4
    jmp _tensor_calc_size
_tensor_size_f16:
    cmp eax, GGML_TYPE_F16
    jne _tensor_size_q8
    mov ecx, 2
    jmp _tensor_calc_size
_tensor_size_q8:
    cmp eax, GGML_TYPE_Q8_0
    jne _tensor_size_q4
    mov ecx, 1
    jmp _tensor_calc_size
_tensor_size_q4:
    mov ecx, 1                          ; Approximate

_tensor_calc_size:
    ; size = dims[0] * dims[1] * dims[2] * dims[3] * type_size
    mov rax, QWORD PTR [r14+GTENSOR_DIMS+0]
    imul rax, QWORD PTR [r14+GTENSOR_DIMS+8]
    imul rax, QWORD PTR [r14+GTENSOR_DIMS+16]
    imul rax, QWORD PTR [r14+GTENSOR_DIMS+24]
    mul rcx
    mov QWORD PTR [r14+GTENSOR_SIZE], rax

    xor eax, eax
    jmp _tensor_done

_tensor_bad_idx:
    mov eax, -1

_tensor_done:
    add rsp, 40
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
GGUF_GetTensorInfo ENDP

END
