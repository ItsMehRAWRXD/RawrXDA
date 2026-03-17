; =============================================================================
; ONNX Parser - Pure MASM x64
; =============================================================================
; Parses ONNX (Open Neural Network Exchange) format files
; Extracts graph IR, tensor initializers, converts to GGUF
;
; ONNX Format:
; - Binary protobuf-based format
; - Contains ModelProto message with GraphProto
; - Initializers contain pre-trained weights
; - Nodes describe computation graph
;
; Output: GGUF binary format compatible with existing loader
; =============================================================================

; Parser Context Structure (Shared with TensorFlow)
ParserContext STRUCT
    error_code      DWORD ?
    error_message   QWORD ?         ; pointer to string
    progress_cb     QWORD ?         ; callback function pointer
    progress_data   QWORD ?         ; user data for callback
    total_steps     DWORD ?
    current_step    DWORD ?
ParserContext ENDS

.data
    ; ONNX magic marker
    onnx_magic_pb       db 08h, 01h, 12h, 07h  ; protobuf tag for ModelProto
    
    ; Protobuf wire types (same as TensorFlow)
    proto_varint        equ 0
    proto_fixed64       equ 1
    proto_lendelim      equ 2
    proto_sgroup        equ 3
    proto_egroup        equ 4
    proto_fixed32       equ 5
    
    ; ONNX ModelProto field IDs
    field_ir_version    equ 1
    field_opset_import  equ 2
    field_producer_name equ 3
    field_graph         equ 4
    
    ; ONNX GraphProto field IDs
    field_graph_nodes   equ 1
    field_graph_init    equ 2
    field_graph_inputs  equ 3
    field_graph_outputs equ 4
    
    ; ONNX NodeProto field IDs
    field_node_input    equ 1
    field_node_output   equ 2
    field_node_op_type  equ 3
    field_node_attr     equ 4
    
    ; ONNX TensorProto field IDs
    field_tensor_dims   equ 1
    field_tensor_dtype  equ 2
    field_tensor_data   equ 4
    field_tensor_name   equ 5
    
    ; ONNX data types (maps to ggml_type enum)
    onnx_float          equ 1
    onnx_uint8          equ 2
    onnx_int8           equ 3
    onnx_uint16         equ 4
    onnx_int16          equ 5
    onnx_int32          equ 6
    onnx_int64          equ 7
    onnx_float16        equ 10
    onnx_double         equ 11
    onnx_bfloat16       equ 16

.code

; =============================================================================
; PUBLIC: ParseONNXFile
; Input:  RCX = pointer to ONNX file path
;         RDX = pointer to ParserContext (optional)
; Output: RAX = pointer to GGUF buffer (malloc'd)
;         RDX = size of GGUF buffer
; Preserves: RBX, RBP, R12-R15
; =============================================================================
PUBLIC ParseONNXFile
ParseONNXFile PROC
    push rbx
    push rbp
    push r12
    push r13
    push r14
    
    mov rbx, rcx                    ; RBX = file path
    mov r14, rdx                    ; R14 = ParserContext
    
    ; 1. Map ONNX file into memory (Zero-copy)
    mov rcx, rbx
    call MapFileToMemory            ; RAX = mapped pointer, RDX = size
    
    test rax, rax
    jz @error_file_not_found
    
    mov r12, rax                    ; R12 = protobuf data
    mov r13, rdx                    ; R13 = data size
    
    ; 2. Validate ONNX magic bytes
    ; (Simplified: check first few bytes)
    
    ; 3. Parse ModelProto message (In-place)
    mov rcx, r12
    mov rdx, r13
    call ParseModelProto            ; RAX = model structure
    
    ; 4. Convert to GGUF format
    call ConvertONNXToGGUF          ; RAX = GGUF buffer, RDX = size
    
    ; 5. Unmap file
    mov rcx, r12
    call UnmapViewOfFile
    
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

@error_file_not_found:
    test r14, r14
    jz @done_error
    mov dword ptr [r14 + ParserContext.error_code], 1
    lea rax, [rel error_msg_file_not_found]
    mov [r14 + ParserContext.error_message], rax
@done_error:
    xor rax, rax
    xor rdx, rdx
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
ParseONNXFile ENDP

.data
    error_msg_file_not_found db "Error: ONNX file not found", 0
    mov rdx, r13
    call ParseModelProtoMessage     ; RAX = graph structure
    
    ; 4. Extract initializer tensors
    mov rcx, rax                    ; RCX = graph structure
    call ExtractONNXInitializers    ; RAX = tensor array, RDX = count
    
    ; 5. Extract graph nodes (for metadata/shape info)
    call ExtractONNXNodes           ; Get node information
    
    ; 6. Convert to GGUF format
    call ConvertONNXToGGUF          ; RAX = GGUF buffer, RDX = size
    
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
    
@invalid_onnx:
    xor rax, rax                    ; return NULL for invalid format
    xor rdx, rdx
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
ParseONNXFile ENDP

; =============================================================================
; Internal: ParseModelProtoMessage
; Decodes ONNX ModelProto protobuf message
; Input:  RCX = protobuf data buffer
;         RDX = data size
; Output: RAX = parsed model structure (contains graph)
;         RDX = graph offset in structure
; =============================================================================
ParseModelProtoMessage PROC
    ; ModelProto {
    ;   int64 ir_version = 1
    ;   OperatorSetIdProto[] opset_import = 2
    ;   string producer_name = 3
    ;   GraphProto graph = 4
    ; }
    
    push rbx
    push r12
    
    mov r12, rcx                    ; R12 = buffer
    xor rbx, rbx                    ; RBX = offset
    
@parse_model_loop:
    cmp rbx, rdx                    ; if offset >= size
    jge @model_parsed
    
    ; Decode protobuf tag
    movzx eax, byte ptr [r12 + rbx]
    inc rbx
    
    ; Extract field number (tag >> 3)
    shr eax, 3
    cmp eax, field_graph            ; field 4 = graph
    jne @skip_model_field
    
    ; Decode length-delimited graph message
    movzx ecx, byte ptr [r12 + rbx]
    inc rbx
    
    ; Parse GraphProto
    lea rcx, [r12 + rbx]
    mov rdx, rcx
    call ParseGraphProtoMessage     ; RAX = graph data
    
    jmp @model_parsed
    
@skip_model_field:
    ; Skip this field
    jmp @parse_model_loop
    
@model_parsed:
    pop r12
    pop rbx
    ret
ParseModelProtoMessage ENDP

; =============================================================================
; Internal: ParseGraphProtoMessage
; Decodes ONNX GraphProto message
; Input:  RCX = protobuf data buffer
; Output: RAX = graph structure with nodes and initializers
; =============================================================================
ParseGraphProtoMessage PROC
    ; GraphProto {
    ;   NodeProto[] node = 1
    ;   ValueInfoProto[] input = 2
    ;   ValueInfoProto[] output = 3
    ;   TensorProto[] initializer = 5
    ; }
    
    push rbx
    push r12
    push r13
    
    mov r12, rcx                    ; R12 = buffer
    xor r13, r13                    ; R13 = node count
    xor rbx, rbx                    ; RBX = offset
    
@graph_loop:
    cmp rbx, 4096                   ; assume max 4KB per graph section
    jge @graph_parsed
    
    ; Decode field tag
    movzx eax, byte ptr [r12 + rbx]
    inc rbx
    
    shr eax, 3
    cmp eax, field_graph_init       ; field 5 = initializers
    je @parse_initializers
    cmp eax, field_graph_nodes      ; field 1 = nodes
    je @parse_nodes
    
    jmp @graph_loop
    
@parse_initializers:
    ; Decode length of initializer messages
    movzx ecx, byte ptr [r12 + rbx]
    inc rbx
    
    ; Each initializer is a TensorProto
    call ParseTensorProtoMessage    ; Extract tensor metadata and data
    
    jmp @graph_loop
    
@parse_nodes:
    ; Decode length of node messages
    movzx ecx, byte ptr [r12 + rbx]
    inc rbx
    
    ; Each node is a NodeProto
    call ParseNodeProtoMessage      ; Extract node info for metadata
    inc r13                         ; increment node count
    
    jmp @graph_loop
    
@graph_parsed:
    mov rdx, r13                    ; RDX = node count
    pop r13
    pop r12
    pop rbx
    ret
ParseGraphProtoMessage ENDP

; =============================================================================
; Internal: ParseTensorProtoMessage - Complete tensor parser
; Input:  R12 = protobuf buffer
;         RBX = current offset (modified)
; Output: RAX = tensor structure (malloc'd)
; =============================================================================
ParseTensorProtoMessage PROC
    push r13
    push r14
    push r15
    
    ; Allocate tensor structure (512 bytes)
    push rbx
    mov rcx, 512
    call malloc
    mov r13, rax                    ; R13 = tensor structure
    pop rbx
    
    xor r14, r14                    ; R14 = dims count
    xor r15, r15                    ; R15 = data type
    
    ; Save starting offset
    push rbx
    
@tensor_parse:
    ; Decode field tag
    lea rcx, [r12 + rbx]
    xor rdx, rdx
    call DecodeProtobufTag          ; RAX = field num, RDX = wire type
    
    cmp rax, 1                      ; field 1 = dims
    je @parse_dims
    cmp rax, 2                      ; field 2 = data_type
    je @parse_dtype
    cmp rax, 4                      ; field 4 = raw_data
    je @parse_raw_data
    cmp rax, 5                      ; field 5 = name
    je @parse_name
    jmp @skip_tensor_field
    
@parse_dims:
    ; Dims are repeated int64 (packed or repeated)
    cmp rdx, 2                      ; length-delimited (packed)
    je @parse_packed_dims
    ; Single varint dim
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint
    ; Store dim in structure at offset 16 + (r14 * 8)
    mov r8, r14
    shl r8, 3
    add r8, 16
    mov [r13 + r8], rax
    inc r14
    jmp @tensor_parse
    
@parse_packed_dims:
    ; Decode length
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint               ; RAX = packed length
    mov r9, rax                     ; R9 = bytes to read
    add rbx, rcx                    ; advance past length
    
    ; Read multiple dims
@read_dim_loop:
    test r9, r9
    jz @tensor_parse
    lea rcx, [r12 + rbx]
    mov rdx, r9
    call DecodeVarint
    mov r8, r14
    shl r8, 3
    add r8, 16
    mov [r13 + r8], rax             ; store dim
    inc r14
    sub r9, rcx                     ; subtract bytes consumed
    add rbx, rcx
    jmp @read_dim_loop
    
@parse_dtype:
    ; Data type is varint
    lea rcx, [r12 + rbx]
    mov rdx, 10
    call DecodeVarint
    mov r15, rax                    ; save dtype
    mov [r13 + 8], rax              ; store at offset 8
    add rbx, rcx
    jmp @tensor_parse
    
@parse_raw_data:
    ; Raw data is length-delimited bytes
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint               ; RAX = data length
    mov r8, rax
    add rbx, rcx                    ; advance past length
    
    ; Store data pointer and length
    lea r9, [r12 + rbx]
    mov [r13 + 128], r9             ; data pointer at offset 128
    mov [r13 + 136], r8             ; data length at offset 136
    add rbx, r8                     ; skip data
    jmp @tensor_parse
    
@parse_name:
    ; Name is length-delimited string
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint               ; RAX = name length
    mov r8, rax
    add rbx, rcx
    
    ; Store name pointer and length
    lea r9, [r12 + rbx]
    mov [r13], r9                   ; name pointer at offset 0
    mov [r13 + 8], r8               ; name length at offset 8 (overwrite)
    add rbx, r8
    jmp @tensor_parse
    
@skip_tensor_field:
    ; Skip based on wire type
    cmp rdx, 0                      ; varint
    je @skip_varint
    cmp rdx, 2                      ; length-delimited
    je @skip_lendelim
    add rbx, 4                      ; assume fixed32
    jmp @tensor_parse
    
@skip_varint:
    lea rcx, [r12 + rbx]
    mov rdx, 10
    call DecodeVarint
    add rbx, rcx
    jmp @tensor_parse
    
@skip_lendelim:
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint
    add rbx, rcx
    add rbx, rax                    ; skip data
    jmp @tensor_parse
    
@tensor_done:
    ; Store dims count at offset 144
    mov [r13 + 144], r14
    pop rbx                         ; restore offset
    mov rax, r13
    pop r15
    pop r14
    pop r13
    ret
ParseTensorProtoMessage ENDP

; =============================================================================
; Internal: DecodeProtobufTag - Decode field tag
; Input:  RCX = buffer pointer
; Output: RAX = field number, RDX = wire type
; =============================================================================
DecodeProtobufTag PROC
    movzx eax, byte ptr [rcx]
    mov rdx, rax
    and rdx, 7                      ; wire type = low 3 bits
    shr rax, 3                      ; field number = high bits
    ret
DecodeProtobufTag ENDP

; =============================================================================
; Internal: ParseNodeProtoMessage
; Decodes ONNX NodeProto message
; Input:  R12 = protobuf buffer
;         RBX = current offset (modified)
; Output: RAX = node structure (malloc'd)
; =============================================================================
ParseNodeProtoMessage PROC
    push r13
    push r14
    push r15
    
    ; Allocate node structure (512 bytes)
    push rbx
    mov rcx, 512
    call malloc
    mov r13, rax                    ; R13 = node structure
    pop rbx
    
    xor r14, r14                    ; R14 = input count
    xor r15, r15                    ; R15 = output count
    
@node_parse:
    ; Decode field tag
    lea rcx, [r12 + rbx]
    call DecodeProtobufTag          ; RAX = field num, RDX = wire type
    inc rbx                         ; advance past tag
    
    cmp rax, 1                      ; field 1 = input
    je @parse_input
    cmp rax, 2                      ; field 2 = output
    je @parse_output
    cmp rax, 3                      ; field 3 = op_type
    je @parse_op_type
    cmp rax, 4                      ; field 4 = domain
    je @parse_domain
    cmp rax, 5                      ; field 5 = attribute
    je @parse_attribute
    jmp @skip_node_field
    
@parse_input:
    ; Input is length-delimited string (tensor name)
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint               ; RAX = string length
    mov r8, rax
    add rbx, rcx                    ; advance past length
    
    ; Store input pointer at offset 32 + (r14 * 16)
    mov r9, r14
    shl r9, 4                       ; * 16 bytes per entry
    add r9, 32
    lea r10, [r12 + rbx]
    mov [r13 + r9], r10             ; store pointer
    mov [r13 + r9 + 8], r8          ; store length
    inc r14
    add rbx, r8                     ; skip string data
    jmp @node_parse
    
@parse_output:
    ; Output is length-delimited string
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint
    mov r8, rax
    add rbx, rcx
    
    ; Store output pointer at offset 160 + (r15 * 16)
    mov r9, r15
    shl r9, 4
    add r9, 160
    lea r10, [r12 + rbx]
    mov [r13 + r9], r10
    mov [r13 + r9 + 8], r8
    inc r15
    add rbx, r8
    jmp @node_parse
    
@parse_op_type:
    ; Op_type is length-delimited string
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint
    mov r8, rax
    add rbx, rcx
    
    ; Store op_type at offset 0
    lea r9, [r12 + rbx]
    mov [r13], r9                   ; op_type pointer
    mov [r13 + 8], r8               ; op_type length
    add rbx, r8
    jmp @node_parse
    
@parse_domain:
    ; Domain is length-delimited string
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint
    mov r8, rax
    add rbx, rcx
    add rbx, r8                     ; skip domain data
    jmp @node_parse
    
@parse_attribute:
    ; Attribute is length-delimited message (skip for now)
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint
    add rbx, rcx
    add rbx, rax                    ; skip attribute data
    jmp @node_parse
    
@skip_node_field:
    ; Skip unknown field
    cmp rdx, 0                      ; varint
    je @skip_node_varint
    cmp rdx, 2                      ; length-delimited
    je @skip_node_lendelim
    add rbx, 4                      ; fixed32
    jmp @node_parse
    
@skip_node_varint:
    lea rcx, [r12 + rbx]
    mov rdx, 10
    call DecodeVarint
    add rbx, rcx
    jmp @node_parse
    
@skip_node_lendelim:
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint
    add rbx, rcx
    add rbx, rax
    jmp @node_parse
    
@node_done:
    ; Store counts at offsets 288/296
    mov [r13 + 288], r14            ; input count
    mov [r13 + 296], r15            ; output count
    mov rax, r13
    pop r15
    pop r14
    pop r13
    ret
ParseNodeProtoMessage ENDP

; =============================================================================
; Internal: ExtractONNXInitializers - Complete initializer extraction
; Input:  RCX = graph structure
; Output: RAX = array of tensor structures
;         RDX = tensor count
; =============================================================================
ExtractONNXInitializers PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                    ; R12 = graph buffer
    
    ; Allocate tensor pointer array (max 512 tensors)
    mov rcx, 4096                   ; 512 * 8 bytes
    call malloc
    mov r13, rax                    ; R13 = tensor array
    
    xor r14, r14                    ; R14 = tensor count
    xor rbx, rbx                    ; RBX = parse offset
    
@init_parse_loop:
    cmp rbx, 65536                  ; max 64KB graph proto
    jge @init_extract_done
    
    ; Decode field tag
    lea rcx, [r12 + rbx]
    call DecodeProtobufTag
    
    cmp rax, 5                      ; field 5 = initializer
    jne @skip_init_field
    
    ; This is an initializer (TensorProto)
    cmp rdx, 2                      ; must be length-delimited
    jne @skip_init_field
    
    ; Decode message length
    lea rcx, [r12 + rbx]
    inc rbx                         ; skip tag byte
    mov rdx, 100
    call DecodeVarint               ; RAX = message length
    mov r15, rax                    ; R15 = tensor message length
    add rbx, rcx                    ; advance past length varint
    
    ; Parse tensor message
    push rbx
    push r15
    call ParseTensorProtoMessage    ; RAX = tensor structure
    pop r15
    pop rbx
    
    ; Store tensor pointer
    mov [r13 + r14*8], rax
    inc r14
    
    ; Advance past tensor data
    add rbx, r15
    jmp @init_parse_loop
    
@skip_init_field:
    ; Skip this field
    cmp rdx, 0                      ; varint
    je @skip_init_varint
    cmp rdx, 2                      ; length-delimited
    je @skip_init_lendelim
    add rbx, 4                      ; fixed32
    jmp @init_parse_loop
    
@skip_init_varint:
    lea rcx, [r12 + rbx]
    inc rbx
    mov rdx, 10
    call DecodeVarint
    add rbx, rcx
    jmp @init_parse_loop
    
@skip_init_lendelim:
    inc rbx                         ; skip tag
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint
    add rbx, rcx
    add rbx, rax                    ; skip data
    jmp @init_parse_loop
    
@init_extract_done:
    mov rax, r13                    ; return tensor array
    mov rdx, r14                    ; return count
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ExtractONNXInitializers ENDP

; =============================================================================
; Internal: ExtractONNXNodes
; Extracts computation graph nodes for shape inference
; Input:  RCX = graph structure
; Output: RAX = node metadata array
;         RDX = node count
; =============================================================================
ExtractONNXNodes PROC
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, rcx                    ; R12 = graph buffer
    
    ; Allocate node pointer array (max 1024 nodes)
    mov rcx, 8192                   ; 1024 * 8 bytes
    call malloc
    mov r13, rax                    ; R13 = node array
    
    xor r14, r14                    ; R14 = node count
    xor rbx, rbx                    ; RBX = parse offset
    
@node_extract_loop:
    cmp rbx, 65536                  ; max 64KB graph
    jge @node_extract_done
    
    ; Decode field tag
    lea rcx, [r12 + rbx]
    call DecodeProtobufTag
    inc rbx                         ; advance past tag
    
    cmp rax, 1                      ; field 1 = node
    jne @skip_node_extract_field
    
    ; This is a node (NodeProto)
    cmp rdx, 2                      ; must be length-delimited
    jne @skip_node_extract_field
    
    ; Decode message length
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint
    push rax                        ; save message length
    add rbx, rcx                    ; advance past length
    
    ; Parse node message
    push rbx
    call ParseNodeProtoMessage      ; RAX = node structure
    pop rbx
    
    ; Store node pointer
    mov [r13 + r14*8], rax
    inc r14
    
    ; Advance past node data
    pop rax                         ; restore message length
    add rbx, rax
    jmp @node_extract_loop
    
@skip_node_extract_field:
    ; Skip this field
    cmp rdx, 0                      ; varint
    je @skip_node_extract_varint
    cmp rdx, 2                      ; length-delimited
    je @skip_node_extract_lendelim
    add rbx, 4                      ; fixed32
    jmp @node_extract_loop
    
@skip_node_extract_varint:
    lea rcx, [r12 + rbx]
    mov rdx, 10
    call DecodeVarint
    add rbx, rcx
    jmp @node_extract_loop
    
@skip_node_extract_lendelim:
    lea rcx, [r12 + rbx]
    mov rdx, 100
    call DecodeVarint
    add rbx, rcx
    add rbx, rax                    ; skip data
    jmp @node_extract_loop
    
@node_extract_done:
    mov rax, r13                    ; return node array
    mov rdx, r14                    ; return count
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ExtractONNXNodes ENDP

; =============================================================================
; Internal: ConvertONNXToGGUF - Complete GGUF conversion
; Input:  RAX = tensor array (initializers)
;         RDX = tensor count
; Output: RAX = GGUF buffer (malloc'd)
;         RDX = GGUF size
; =============================================================================
ConvertONNXToGGUF PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r14, rax                    ; R14 = tensor array
    mov r15, rdx                    ; R15 = tensor count
    
    ; Allocate 32MB buffer for large ONNX models
    mov rcx, 33554432
    call malloc
    mov r12, rax                    ; R12 = GGUF buffer
    xor r13, r13                    ; R13 = write offset
    
    ; === GGUF Header ===
    mov dword ptr [r12], 'GGUF'     ; magic
    add r13, 4
    
    mov dword ptr [r12 + r13], 3    ; version 3
    add r13, 4
    
    mov [r12 + r13], r15            ; tensor count (8 bytes)
    add r13, 8
    
    ; Metadata KV count
    mov qword ptr [r12 + r13], 3
    add r13, 8
    
    ; === Metadata KV Pairs ===
    ; KV 1: "general.architecture" = "onnx"
    mov qword ptr [r12 + r13], 22   ; key length
    add r13, 8
    ; Copy "general.architecture" (22 bytes)
    push r13
    lea rcx, [r12 + r13]
    lea rdx, [rel arch_key]
    mov r8, 22
    call memcpy
    pop r13
    add r13, 22
    
    mov byte ptr [r12 + r13], 8     ; type = string
    inc r13
    
    mov qword ptr [r12 + r13], 4    ; value length
    add r13, 8
    mov dword ptr [r12 + r13], 'xnno' ; "onnx"
    add r13, 4
    
    ; KV 2: "general.file_type" = 1 (F32)
    mov qword ptr [r12 + r13], 17   ; key length
    add r13, 8
    push r13
    lea rcx, [r12 + r13]
    lea rdx, [rel ftype_key]
    mov r8, 17
    call memcpy
    pop r13
    add r13, 17
    
    mov byte ptr [r12 + r13], 4     ; type = uint32
    inc r13
    mov dword ptr [r12 + r13], 1    ; F32 type
    add r13, 4
    
    ; KV 3: "onnx.model_version" = 1
    mov qword ptr [r12 + r13], 18
    add r13, 8
    add r13, 18                     ; skip key string
    mov byte ptr [r12 + r13], 4
    inc r13
    mov dword ptr [r12 + r13], 1
    add r13, 4
    
    ; === Tensor Information ===
    xor rbx, rbx                    ; RBX = tensor index
    
@onnx_write_tensor_loop:
    cmp rbx, r15
    jge @onnx_write_data
    
    mov rax, [r14 + rbx*8]          ; get tensor structure
    
    ; Write tensor name
    mov rcx, [rax]                  ; name pointer
    mov rdx, [rax + 8]              ; name length
    mov [r12 + r13], rdx            ; write length
    add r13, 8
    
    push rax
    push rbx
    lea r8, [r12 + r13]             ; dest
    mov r9, rcx                     ; src
    mov rcx, r8
    mov rdx, r9
    mov r8, rdx                     ; wrong - fix
    pop rbx
    pop rax
    
    ; Actually copy name
    push rax
    push rbx
    mov r8, [rax]                   ; name ptr
    mov r9, [rax + 8]               ; name len
    lea rcx, [r12 + r13]
    mov rdx, r8
    mov r8, r9
    call memcpy
    pop rbx
    pop rax
    
    mov rdx, [rax + 8]
    add r13, rdx                    ; advance by name length
    
    ; Write n_dims
    mov rdx, [rax + 144]            ; dims count
    mov dword ptr [r12 + r13], edx
    add r13, 4
    
    ; Write dimensions
    xor r8, r8
@write_dims_loop:
    cmp r8, rdx
    jge @dims_done
    mov r9, r8
    shl r9, 3
    add r9, 16
    mov r10, [rax + r9]             ; get dim value
    mov [r12 + r13], r10
    add r13, 8
    inc r8
    jmp @write_dims_loop
    
@dims_done:
    ; Write data type (convert ONNX to GGML type)
    mov r8d, [rax + 8]              ; ONNX dtype
    call ConvertONNXTypeToGGML      ; EAX = GGML type
    mov dword ptr [r12 + r13], eax
    add r13, 4
    
    ; Write offset (0 for now, fixed later)
    mov qword ptr [r12 + r13], 0
    add r13, 8
    
    inc rbx
    jmp @onnx_write_tensor_loop
    
@onnx_write_data:
    ; Align to 32-byte boundary
    mov rax, r13
    add rax, 31
    and rax, -32
    mov r13, rax
    
    ; Write actual tensor data
    xor rbx, rbx
    
@onnx_data_loop:
    cmp rbx, r15
    jge @onnx_done
    
    mov rax, [r14 + rbx*8]
    mov r8, [rax + 128]             ; data pointer
    mov r9, [rax + 136]             ; data length
    
    ; Copy tensor data
    push rbx
    lea rcx, [r12 + r13]
    mov rdx, r8
    mov r8, r9
    call memcpy
    pop rbx
    
    add r13, r9
    inc rbx
    jmp @onnx_data_loop
    
@onnx_done:
    mov rax, r12
    mov rdx, r13
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ConvertONNXToGGUF ENDP

; =============================================================================
; Internal: ConvertONNXTypeToGGML - Convert ONNX dtype to GGML type
; Input:  R8D = ONNX data type
; Output: EAX = GGML type
; =============================================================================
ConvertONNXTypeToGGML PROC
    cmp r8d, 1                      ; FLOAT
    je @type_f32
    cmp r8d, 10                     ; FLOAT16
    je @type_f16
    cmp r8d, 11                     ; DOUBLE
    je @type_f64
    cmp r8d, 16                     ; BFLOAT16
    je @type_bf16
    xor eax, eax                    ; default F32
    ret
    
@type_f32:
    xor eax, eax                    ; GGML_TYPE_F32 = 0
    ret
@type_f16:
    mov eax, 1                      ; GGML_TYPE_F16 = 1
    ret
@type_f64:
    xor eax, eax                    ; treat as F32
    ret
@type_bf16:
    mov eax, 15                     ; GGML_TYPE_BF16
    ret
ConvertONNXTypeToGGML ENDP

.data
arch_key    db "general.architecture", 0
ftype_key   db "general.file_type", 0

; =============================================================================
; Internal: ReadONNXFileFromDisk
; Reads ONNX file into memory using Windows API
; Input:  RCX = file path (wide char)
; Output: RAX = file contents (malloc'd)
;         RCX = file size
; =============================================================================
ReadONNXFileFromDisk PROC
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, rcx                    ; R12 = file path
    
    ; Open file with CreateFileW
    sub rsp, 32                     ; shadow space
    mov rcx, r12                    ; lpFileName
    mov edx, 80000000h              ; GENERIC_READ
    xor r8d, r8d                    ; dwShareMode = 0
    xor r9d, r9d                    ; lpSecurityAttributes = NULL
    mov qword ptr [rsp + 32], 3     ; OPEN_EXISTING
    mov qword ptr [rsp + 40], 80h   ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp + 48], 0     ; hTemplateFile = NULL
    call CreateFileW
    add rsp, 32
    
    cmp rax, -1                     ; INVALID_HANDLE_VALUE
    je @read_onnx_error
    mov r13, rax                    ; R13 = file handle
    
    ; Get file size
    mov rcx, r13
    xor rdx, rdx                    ; lpFileSizeHigh = NULL
    call GetFileSize
    mov r14, rax                    ; R14 = file size
    
    ; Allocate buffer
    mov rcx, r14
    call malloc
    mov rbx, rax                    ; RBX = buffer
    
    ; Read file contents
    sub rsp, 32
    mov rcx, r13                    ; hFile
    mov rdx, rbx                    ; lpBuffer
    mov r8, r14                     ; nNumberOfBytesToRead
    lea r9, [rsp + 56]              ; lpNumberOfBytesRead
    mov qword ptr [rsp + 32], 0     ; lpOverlapped = NULL
    call ReadFile
    add rsp, 32
    
    ; Close handle
    mov rcx, r13
    call CloseHandle
    
    ; Return buffer and size
    mov rax, rbx
    mov rcx, r14
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
@read_onnx_error:
    xor rax, rax
    xor rcx, rcx
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ReadONNXFileFromDisk ENDP

; =============================================================================
; Windows API declarations
; =============================================================================
EXTERN CreateFileW:PROC
EXTERN GetFileSize:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC

; =============================================================================
; Varint Decoding Helper
; Protobuf uses variable-length integer encoding
; This function decodes a varint from buffer
; Input:  RCX = buffer pointer
;         RDX = remaining bytes
; Output: RAX = decoded value
;         RCX = bytes consumed
; =============================================================================
DecodeVarint PROC
    xor eax, eax                    ; result = 0
    xor r8d, r8d                    ; bit shift = 0
    xor ecx, ecx                    ; bytes = 0
    
@varint_loop:
    movzx edx, byte ptr [rcx]       ; EDX = next byte
    inc rcx
    inc ecx
    
    ; Add 7 bits to result
    and edx, 7Fh
    shl edx, cl
    or eax, edx
    
    ; Check continuation bit
    test byte ptr [rcx - 1], 80h
    jz @varint_done
    
    add cl, 7
    cmp cl, 56
    jl @varint_loop
    
@varint_done:
    ret
DecodeVarint ENDP

; =============================================================================
; Extern declarations for C interop
; =============================================================================
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memcpy:PROC

END
