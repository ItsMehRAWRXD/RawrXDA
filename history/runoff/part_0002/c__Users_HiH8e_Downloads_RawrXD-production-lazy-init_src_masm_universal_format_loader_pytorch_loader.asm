; =============================================================================
; PyTorch Model Loader - Pure MASM x64
; =============================================================================
; Loads PyTorch .pt files (ZIP-based with pickle tensors)
; Unpickles Python objects and extracts state_dict
; Converts to GGUF-compatible format
;
; PyTorch .pt file structure:
;   - ZIP archive with specific layout
;   - archive/ directory containing:
;     - data.pkl (pickled state_dict)
;     - data/ (binary tensor data)
;     - version (format version)
; =============================================================================

.data
    ; ZIP format constants
    zip_local_file_sig  dd 04034b50h  ; "PK\x03\x04"
    zip_central_dir_sig dd 02014b50h  ; "PK\x01\x02"
    zip_end_of_cd_sig   dd 06054b50h  ; "PK\x05\x06"
    
    ; Python pickle constants
    pickle_proto_2      equ 80h
    pickle_proto_3      equ 03h
    pickle_proto_4      equ 04h
    
    ; Pickle opcodes
    pkl_frame           equ 95h  ; Frame opcode (protocol 4+)
    pkl_proto           equ 80h  ; Protocol opcode
    pkl_reduce          equ 52h  ; Reduce opcode 'R'
    pkl_mark            equ 28h  ; Mark opcode '('
    pkl_stop            equ 2Eh  ; Stop opcode '.'
    pkl_tuple           equ 29h  ; Tuple opcode ')'
    pkl_dict            equ 7Dh  ; Dict opcode '}'
    pkl_setitems        equ 75h  ; Setitems opcode 'u'
    pkl_binunicode4     equ 8Ch  ; Binary unicode (4-byte length)
    pkl_binstring       equ 54h  ; Binary string
    pkl_short_binstring equ 55h  ; Short binary string
    pkl_long_binstring  equ 43h  ; Long binary string
    pkl_binfloat        equ 47h  ; Binary float
    pkl_binbytes        equ 43h  ; Binary bytes
    pkl_empty_dict      equ 7Dh  ; Empty dict '}'
    pkl_empty_tuple     equ 29h  ; Empty tuple ')'
    
    ; PyTorch tensor class names (in pickle)
    torch_tensor_str    db "torch._utils._rebuild.rebuild_tensor_v2", 0
    torch_storage_str   db "torch.storage.TypedStorage", 0
    torch_parameter_str db "torch.nn.parameter.Parameter", 0
    
    ; Output buffer markers
    pytorch_gguf_sig    db "GGUF", 0
    
.code

; =============================================================================
; PUBLIC: ParsePyTorchFile
; Input:  RCX = pointer to .pt file path (wide char)
; Output: RAX = pointer to GGUF buffer
;         RDX = GGUF buffer size
; =============================================================================
PUBLIC ParsePyTorchFile
ParsePyTorchFile PROC
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 48 + 512
    
    mov r12, rcx        ; r12 = file path
    
    ; Step 1: Verify it's a valid ZIP (PyTorch .pt files are ZIPs)
    mov rdx, 80000000h  ; GENERIC_READ
    mov r8, 1           ; FILE_SHARE_READ
    xor r9, r9
    mov rax, 3          ; OPEN_EXISTING
    mov qword ptr [rsp + 32], rax
    mov qword ptr [rsp + 40], 0
    mov qword ptr [rsp + 48], 0
    
    mov rcx, r12
    extern CreateFileW : proc
    call CreateFileW
    
    cmp rax, -1
    je @pytorch_error
    
    mov rbx, rax        ; rbx = file handle
    
    ; Get file size
    mov rcx, rbx
    extern GetFileSize : proc
    call GetFileSize
    
    cmp rax, -1
    je @pytorch_close_error
    
    mov r13, rax        ; r13 = file size
    
    ; Allocate buffer for entire file
    mov rcx, r13
    extern malloc : proc
    call malloc
    
    test rax, rax
    jz @pytorch_close_error
    
    mov r14, rax        ; r14 = file buffer
    
    ; Read entire file
    mov rcx, rbx
    mov rdx, r14
    mov r8, r13
    lea r9, [rsp + 80]
    mov qword ptr [rsp + 32], 0
    
    extern ReadFile : proc
    call ReadFile
    
    test eax, eax
    jz @pytorch_free_error
    
    ; Step 2: Verify ZIP signature
    mov eax, dword ptr [r14]
    cmp eax, 04034b50h  ; "PK\x03\x04"
    jne @pytorch_free_error
    
    ; Step 3: Parse ZIP to find 'archive/data.pkl'
    mov rcx, r14
    mov rdx, r13
    call FindZipEntry
    
    ; rax now points to pickle data (or NULL if not found)
    test rax, rax
    jz @pytorch_free_error
    
    mov r12, rax        ; r12 = pickle data
    mov r13, rdx        ; r13 = pickle size
    
    ; Step 4: Unpickle the state_dict
    mov rcx, r12
    mov rdx, r13
    call UnpickleStateDictFromBuffer
    
    ; rax = pointer to parsed tensors
    ; rbx = number of tensors
    
    test rax, rax
    jz @pytorch_free_error
    
    mov r12, rax        ; r12 = tensor array
    mov r13, rbx        ; r13 = tensor count
    
    ; Step 5: Convert to GGUF format
    mov rcx, r12
    mov rdx, r13
    call ConvertPyTorchToGGUF
    
    ; rax = GGUF buffer
    ; rdx = GGUF size
    
    ; Save result before cleanup
    mov r12, rax
    mov r13, rdx
    
    ; Cleanup
    mov rcx, r14
    extern free : proc
    call free
    
    mov rcx, rbx
    call free
    
    mov rax, r12
    mov rdx, r13
    
    add rsp, 48 + 512
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
@pytorch_free_error:
    mov rcx, r14
    extern free : proc
    call free
    
@pytorch_close_error:
    mov rcx, rbx
    extern CloseHandle : proc
    call CloseHandle
    
@pytorch_error:
    xor rax, rax
    xor rdx, rdx
    add rsp, 48 + 512
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
ParsePyTorchFile ENDP

; =============================================================================
; INTERNAL: FindZipEntry
; Finds a file entry in ZIP archive
; Input:  RCX = ZIP buffer
;         RDX = buffer size
; Output: RAX = pointer to entry data (or NULL)
;         RDX = data size
; =============================================================================
FindZipEntry PROC
    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx        ; r12 = ZIP buffer
    mov rbx, rdx        ; rbx = size
    mov rsi, 0          ; rsi = current offset
    
@find_entry_loop:
    cmp rsi, rbx
    jge @find_entry_notfound
    
    ; Look for local file header signature
    mov eax, dword ptr [r12 + rsi]
    cmp eax, 04034b50h  ; PK\x03\x04
    jne @find_entry_next
    
    ; Found a local file header
    ; Structure:
    ;   +0: signature (4 bytes)
    ;   +4: version (2 bytes)
    ;   +6: flags (2 bytes)
    ;   +8: compression (2 bytes)
    ;   +10: mod time (2 bytes)
    ;   +12: mod date (2 bytes)
    ;   +14: CRC-32 (4 bytes)
    ;   +18: compressed size (4 bytes)
    ;   +22: uncompressed size (4 bytes)
    ;   +26: filename length (2 bytes)
    ;   +28: extra field length (2 bytes)
    ;   +30: filename
    
    movzx eax, word ptr [r12 + rsi + 26]  ; filename length
    mov rcx, rsi
    add rcx, 30         ; Start of filename
    
    ; Check if filename contains "data.pkl"
    ; For now, simplified check
    mov r8, rcx
    add r8, rax
    
    ; Look for "data.pkl" string
    mov r9, 0
@check_filename:
    cmp r9, rax
    jge @find_entry_next
    mov bl, byte ptr [rcx + r9]
    cmp bl, 'd'
    jne @check_next_char
    cmp byte ptr [rcx + r9 + 1], 'a'
    jne @check_next_char
    cmp byte ptr [rcx + r9 + 2], 't'
    jne @check_next_char
    
    ; Likely found "data.pkl"
    jmp @found_data_pkl
    
@check_next_char:
    inc r9
    jmp @check_filename
    
@found_data_pkl:
    ; Extract data offset and size
    movzx eax, word ptr [r12 + rsi + 28]  ; extra field length
    mov rcx, rsi
    add rcx, 30
    movzx edx, word ptr [r12 + rsi + 26]  ; filename length
    add rcx, rdx
    add rcx, rax        ; Add extra field length (eax already has it)
    
    mov eax, dword ptr [r12 + rsi + 22]  ; Uncompressed size
    mov rdx, rax
    mov rax, rcx        ; Return pointer to data
    
    add rsp, 32
    pop r12
    pop rbx
    ret
    
@find_entry_next:
    inc rsi
    jmp @find_entry_loop
    
@find_entry_notfound:
    xor rax, rax
    xor rdx, rdx
    add rsp, 32
    pop r12
    pop rbx
    ret
    
FindZipEntry ENDP

; =============================================================================
; INTERNAL: UnpickleStateDictFromBuffer
; Unpickles Python state_dict from pickle data
; Input:  RCX = pickle buffer
;         RDX = buffer size
; Output: RAX = pointer to tensor metadata array (malloc'd)
;         RBX = number of tensors
; =============================================================================
UnpickleStateDictFromBuffer PROC
    push rbx
    push r12
    sub rsp, 64
    
    ; This is a simplified unpickler for torch state_dict
    ; Real implementation would need full pickle protocol support
    
    mov r12, rcx        ; r12 = pickle buffer
    mov rbx, rdx        ; rbx = size
    
    ; For now, allocate space for tensor metadata
    mov rcx, 4096
    extern malloc : proc
    call malloc
    
    test rax, rax
    jz @unpickle_error
    
    mov r12, rax        ; r12 = output tensor array
    mov rbx, 0          ; tensor count = 0
    
    ; TODO: Implement full pickle unpickling
    
    mov rax, r12
    
    add rsp, 64
    pop r12
    pop rbx
    ret
    
@unpickle_error:
    xor rax, rax
    xor rbx, rbx
    add rsp, 64
    pop r12
    pop rbx
    ret
    
UnpickleStateDictFromBuffer ENDP

; =============================================================================
; INTERNAL: ConvertPyTorchToGGUF
; Converts PyTorch tensor array to GGUF format
; Input:  RCX = tensor array
;         RDX = tensor count
; Output: RAX = GGUF buffer
;         RDX = GGUF buffer size
; =============================================================================
ConvertPyTorchToGGUF PROC
    push rbx
    push r12
    sub rsp, 48
    
    mov r12, rcx        ; r12 = tensor array
    mov rbx, rdx        ; rbx = tensor count
    
    ; Allocate GGUF output (estimate)
    mov rcx, 1048576    ; 1MB initial
    extern malloc : proc
    call malloc
    
    test rax, rax
    jz @convert_error
    
    mov r12, rax        ; r12 = GGUF buffer
    
    ; Write GGUF header
    mov eax, 46554747h  ; "GGUF" (little-endian)
    mov dword ptr [r12], eax
    
    mov eax, 3
    mov dword ptr [r12 + 4], eax  ; Version
    
    mov eax, ebx
    mov dword ptr [r12 + 8], eax  ; Tensor count
    
    mov eax, 1
    mov dword ptr [r12 + 12], eax  ; KV pair count
    
    ; TODO: Add tensors to GGUF
    
    mov rax, r12
    mov rdx, 16  ; For now, just header size
    
    add rsp, 48
    pop r12
    pop rbx
    ret
    
@convert_error:
    xor rax, rax
    xor rdx, rdx
    add rsp, 48
    pop r12
    pop rbx
    ret
    
ConvertPyTorchToGGUF ENDP

END
