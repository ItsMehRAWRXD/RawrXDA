SkipMetadataKV PROC
    ; rsi = file base + header end
    ; rcx = metadata_kv_count

    test rcx, rcx
    jz SkipMetadataDone

SkipMetadataLoop:
    ; Read key length (u32)
    mov eax, dword ptr [rsi]
    add rsi, 4
    add rsi, rax          ; Skip key bytes

    ; Read value type (u32)
    mov eax, dword ptr [rsi]
    add rsi, 4

    ; Skip value based on type
    cmp eax, 1            ; String
    je SkipString
    cmp eax, 2            ; Array
    je SkipArray
    cmp eax, 3            ; Scalar
    je SkipScalar
    jmp SkipMetadataNext

SkipString:
    ; Read string length (u32)
    mov eax, dword ptr [rsi]
    add rsi, 4
    add rsi, rax
    jmp SkipMetadataNext

SkipArray:
    ; Read array length (u32)
    mov eax, dword ptr [rsi]
    add rsi, 4
    imul rax, 8           ; Assume 8-byte elements
    add rsi, rax
    jmp SkipMetadataNext

SkipScalar:
    add rsi, 8            ; Assume 8-byte scalar

SkipMetadataNext:
    dec rcx
    jnz SkipMetadataLoop

SkipMetadataDone:
    ret
SkipMetadataKV ENDP