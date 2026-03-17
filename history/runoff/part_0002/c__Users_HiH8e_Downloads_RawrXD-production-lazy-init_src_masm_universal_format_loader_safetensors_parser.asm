; =============================================================================
; SafeTensors Parser - Pure MASM x64
; =============================================================================
; Parses SafeTensors format files and converts to GGUF-compatible output
; 
; SafeTensors Format:
;   [8 bytes: little-endian uint64] = size of JSON metadata
;   [N bytes: UTF-8 JSON] = {"__metadata__": {...}, "tensor_name": {...}}
;   [variable: tensor data] = concatenated tensor blocks
;
; Output: GGUF format file that can be loaded by existing pipeline
; =============================================================================

.data
    ; SafeTensors tensor metadata JSON keys
    str_data_offsets    db "data_offsets", 0
    str_shape           db "shape", 0
    str_dtype           db "dtype", 0
    str_metadata        db "__metadata__", 0
    
    ; GGUF magic and version
    gguf_magic          db "GGUF", 0
    gguf_version        dd 3
    gguf_arch_name      db "llama", 0
    
    ; Data type mappings: SafeTensors -> GGUF
    ; SafeTensors types: "F32", "F16", "BF16", "I32", "I16", "I8", "U8", "BOOL"
    dt_f32_str          db "F32", 0
    dt_f16_str          db "F16", 0
    dt_i32_str          db "I32", 0
    dt_i8_str           db "I8", 0
    
    ; GGUF tensor type constants
    gguf_f32            equ 0
    gguf_f16            equ 1
    gguf_i32            equ 2
    gguf_i16            equ 3
    gguf_i8             equ 4
    
    ; Buffer for JSON parsing
    json_buffer_size    equ 65536
    
.code

; =============================================================================
; PUBLIC: ParseSafeTensorsFile
; Input:  RCX = pointer to SafeTensors file path (wide char)
; Output: RAX = pointer to GGUF buffer (or NULL on error)
;         RDX = GGUF buffer size
; Allocates memory that must be freed by caller
; =============================================================================
PUBLIC ParseSafeTensorsFile
ParseSafeTensorsFile PROC
    push rbx
    push r12
    push r13
    sub rsp, 48 + 512   ; Local variables + shadow space
    
    ; rcx = file path
    mov r12, rcx        ; r12 = file path
    
    ; Open file
    mov rdx, 80000000h  ; GENERIC_READ
    mov r8, 1           ; FILE_SHARE_READ
    xor r9, r9          ; NULL security
    mov rax, 3          ; OPEN_EXISTING
    mov qword ptr [rsp + 32], rax
    mov qword ptr [rsp + 40], 0
    mov qword ptr [rsp + 48], 0
    
    mov rcx, r12
    extern CreateFileW : proc
    call CreateFileW
    
    cmp rax, -1
    je @parse_safetensors_error
    
    mov rbx, rax        ; rbx = file handle
    
    ; Get file size
    lea r9, [rsp + 64]  ; Pointer to file size output
    mov rcx, rbx
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    
    extern GetFileSize : proc
    call GetFileSize
    
    cmp rax, -1
    je @close_and_error_safetensors
    
    mov r13, rax        ; r13 = file size
    
    ; Allocate buffer for entire file
    mov rcx, r13
    extern malloc : proc
    call malloc
    
    test rax, rax
    jz @close_and_error_safetensors
    
    mov r12, rax        ; r12 = file buffer
    
    ; Read file
    mov rcx, rbx
    mov rdx, r12
    mov r8, r13
    lea r9, [rsp + 80]  ; bytes read pointer
    mov qword ptr [rsp + 32], 0
    
    extern ReadFile : proc
    call ReadFile
    
    test eax, eax
    jz @free_and_error_safetensors
    
    ; Now parse SafeTensors header
    ; First 8 bytes = metadata size
    mov rax, qword ptr [r12]
    mov rbx, rax        ; rbx = metadata size
    
    ; Validate: metadata size should be reasonable (< 1MB)
    cmp rbx, 1000000h
    jg @free_and_error_safetensors
    
    ; JSON starts at offset 8, size is rbx bytes
    mov rcx, r12
    add rcx, 8
    mov rdx, rbx
    
    ; Parse JSON to extract tensor info
    ; For now, create simplified GGUF with basic structure
    
    ; Allocate output GGUF buffer (estimate: file_size + 10KB overhead)
    mov rcx, r13
    add rcx, 10240
    extern malloc : proc
    call malloc
    
    test rax, rax
    jz @free_and_error_safetensors
    
    mov r13, rax        ; r13 = output GGUF buffer
    
    ; Write GGUF header
    mov rcx, r13
    mov rdx, offset gguf_magic
    mov r8, 4
    ; memcpy(output, "GGUF", 4)
    mov eax, dword ptr [rdx]
    mov dword ptr [rcx], eax
    add rcx, 4
    
    ; Version (little-endian uint32)
    mov eax, gguf_version
    mov dword ptr [rcx], eax
    add rcx, 4
    
    ; Number of tensors (placeholder for now)
    mov eax, 0  ; Will be filled after parsing
    mov dword ptr [rcx], eax
    add rcx, 4
    
    ; KV pair count (placeholder)
    mov eax, 1  ; At least "general.architecture"
    mov dword ptr [rcx], eax
    add rcx, 4
    
    ; TODO: Parse JSON and extract tensors
    ; For now, return the buffer
    mov rax, r13
    mov rdx, r13
    add rdx, 16  ; Current position
    
    add rsp, 48 + 512
    pop r13
    pop r12
    pop rbx
    ret
    
@free_and_error_safetensors:
    mov rcx, r12
    extern free : proc
    call free
    
@close_and_error_safetensors:
    mov rcx, rbx
    extern CloseHandle : proc
    call CloseHandle
    
@parse_safetensors_error:
    xor rax, rax
    xor rdx, rdx
    add rsp, 48 + 512
    pop r13
    pop r12
    pop rbx
    ret
    
ParseSafeTensorsFile ENDP

; =============================================================================
; PUBLIC: ParseSafeTensorsBuffer
; Input:  RCX = pointer to SafeTensors data buffer
;         RDX = buffer size
; Output: RAX = pointer to parsed tensor info array
;         RBX = number of tensors
; =============================================================================
PUBLIC ParseSafeTensorsBuffer
ParseSafeTensorsBuffer PROC
    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx        ; r12 = buffer
    mov rbx, rdx        ; rbx = size
    
    ; Extract metadata size from first 8 bytes
    mov rax, qword ptr [r12]
    mov r8, rax         ; r8 = metadata size
    
    ; Validate
    cmp r8, 1000000h
    jg @parse_buf_error
    cmp r8, 0
    je @parse_buf_error
    
    ; JSON metadata starts at offset 8
    mov rcx, r12
    add rcx, 8
    
    ; Parse JSON manually (no external library)
    ; Extract tensor names and their data_offsets
    
    ; For now, allocate space for results
    mov rcx, 1024  ; Space for tensor info
    extern malloc : proc
    call malloc
    
    test rax, rax
    jz @parse_buf_error
    
    mov rbx, 0  ; tensor count = 0
    
    add rsp, 32
    pop r12
    pop rbx
    ret
    
@parse_buf_error:
    xor rax, rax
    xor rbx, rbx
    add rsp, 32
    pop r12
    pop rbx
    ret
    
ParseSafeTensorsBuffer ENDP

; =============================================================================
; INTERNAL: JsonParseMetadata
; Parses the JSON metadata section of SafeTensors file
; Input:  RCX = JSON buffer pointer
;         RDX = JSON size in bytes
; Output: RAX = pointer to tensor metadata array (malloc'd)
;         RBX = number of tensors found
; =============================================================================
JsonParseMetadata PROC
    push rbx
    push r12
    sub rsp, 64
    
    ; Simple state machine for JSON parsing
    mov r12, rcx        ; r12 = json buffer
    mov rbx, rdx        ; rbx = size
    mov rsi, 0          ; rsi = current offset
    mov r8, 0           ; r8 = tensor count
    
@parse_json_loop:
    cmp rsi, rbx
    jge @parse_json_done
    
    ; Look for tensor_name patterns
    ; In SafeTensors JSON: "tensor_name": {"data_offsets": [start, end], "shape": [...], "dtype": "..."}
    
    mov al, byte ptr [r12 + rsi]
    
    ; Look for opening quote (tensor name start)
    cmp al, '"'
    jne @json_next_char
    
    ; Check if this is a tensor name (not __metadata__)
    ; Advance and collect name
    inc rsi
    mov rcx, rsi
    
    ; Count characters until closing quote
@json_count_name:
    cmp rsi, rbx
    jge @json_next_char
    mov al, byte ptr [r12 + rsi]
    cmp al, '"'
    je @json_found_name
    inc rsi
    jmp @json_count_name
    
@json_found_name:
    ; We have a name from rcx to rsi
    ; Check if it's __metadata__
    mov al, byte ptr [r12 + rcx]
    cmp al, '_'
    je @json_skip_metadata
    
    ; This is a tensor name
    inc r8
    
@json_skip_metadata:
    inc rsi
    jmp @parse_json_loop
    
@json_next_char:
    inc rsi
    jmp @parse_json_loop
    
@parse_json_done:
    mov rax, 0  ; For now, return NULL (placeholder)
    mov rbx, r8 ; Return tensor count
    
    add rsp, 64
    pop r12
    pop rbx
    ret
    
JsonParseMetadata ENDP

END
