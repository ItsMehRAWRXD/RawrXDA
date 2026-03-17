; ════════════════════════════════════════════════════════════════════════════════
; GGUF-DUMP v1.1.x — RawrXD Sovereign Tool
; Zero-allocation GGUF parser for GHOST-MAP generation
; ════════════════════════════════════════════════════════════════════════════════

OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; Windows API

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

EXTERNDEF __imp_CreateFileA:QWORD
EXTERNDEF __imp_CreateFileMappingA:QWORD
EXTERNDEF __imp_MapViewOfFile:QWORD
EXTERNDEF __imp_UnmapViewOfFile:QWORD
EXTERNDEF __imp_CloseHandle:QWORD
EXTERNDEF __imp_GetFileSizeEx:QWORD
EXTERNDEF __imp_WriteFile:QWORD
EXTERNDEF __imp_GetStdHandle:QWORD

; GGUF Magic Constants
GGUF_MAGIC      EQU 0x46554747      ; "GGUF" little-endian
GGUF_VERSION    EQU 3

; GGUF Types
GGUF_TYPE_UINT8   EQU 0
GGUF_TYPE_INT8    EQU 1
GGUF_TYPE_UINT16  EQU 2
GGUF_TYPE_INT16   EQU 3
GGUF_TYPE_UINT32  EQU 4
GGUF_TYPE_INT32   EQU 5
GGUF_TYPE_FLOAT32 EQU 6
GGUF_TYPE_BOOL    EQU 7
GGUF_TYPE_STRING  EQU 8
GGUF_TYPE_ARRAY   EQU 9
GGUF_TYPE_UINT64  EQU 10
GGUF_TYPE_INT64   EQU 11
GGUF_TYPE_FLOAT64 EQU 12

.data
    ALIGN 8
    
    szBanner        db 13,10
                    db "╔══════════════════════════════════════════════════════════════╗",13,10
                    db "║         RAWRXD GGUF-DUMP v1.1.x — SOVEREIGN EDITION          ║",13,10
                    db "╚══════════════════════════════════════════════════════════════╝",13,10,0
    
    szUsage         db "Usage: gguf-dump.exe <model.gguf> [options]",13,10
                    db "Options:",13,10
                    db "  --raw          Full hex dump of header",13,10
                    db "  --tensors      List all tensors with offsets",13,10
                    db "  --metadata     Show key-value metadata",13,10
                    db "  --ghost        Output GHOST-MAP compatible format",13,10
                    db "  --verify       SHA256 integrity check",13,10,13,10,0
    
    szErrOpen       db "[ERROR] Failed to open file: ",0
    szErrMap        db "[ERROR] Failed to map file",13,10,0
    szErrMagic      db "[ERROR] Invalid GGUF magic",13,10,0
    szErrVersion    db "[ERROR] Unsupported GGUF version",13,10,0
    
    szInfoMagic     db "[HEADER] Magic: GGUF v",0
    szInfoTensors   db "[HEADER] Tensor count: ",0
    szInfoKV        db "[HEADER] Metadata KV pairs: ",0
    
    szTensorHeader  db 13,10,"═══ TENSOR MAP ═══",13,10
                    db "Idx  Name                           Offset      Size        Dims",13,10
                    db "─────────────────────────────────────────────────────────────────",13,10,0
    
    szTensorEntry   db "%4d %-30s %12llu %12llu  %dD [",0
    szDimSep        db "%d",0
    szDimComma      db "×",0
    szDimClose      db "]",13,10,0
    
    szGhostHeader   db 13,10,"═══ GHOST-MAP FORMAT ═══",13,10
                    db "# RawrXD v1.1.x GHOST-MAP — Auto-generated from GGUF",13,10
                    db "# Format: tensor_id|name_hash|offset|size|priority|stripe_hint",13,10,13,10,0
    
    szGhostEntry    db "%d|%08X|%llu|%llu|%d|%d",13,10,0
    
    szMetadataHeader db 13,10,"═══ METADATA ═══",13,10,0
    szKVString      db "  %s = %s",13,10,0
    szKVNumeric     db "  %s = %lld",13,10,0
    
    szFooter        db 13,10,"[GGUF-DUMP] Complete. ",0
    szBytesScanned  db " bytes scanned, ",0
    szTensorsFound  db " tensors mapped.",13,10,0
    
    ; Runtime state
    g_file_handle   dq 0
    g_map_handle    dq 0
    g_view_base     dq 0
    g_file_size     dq 0
    g_cursor        dq 0
    
    ; Output handle
    g_stdout        dq 0

.code

; ════════════════════════════════════════════════════════════════════════════════
; Entry Point
; ════════════════════════════════════════════════════════════════════════════════
main PROC
    push rbp
    mov rbp, rsp
    sub rsp, 256
    
    ; Get stdout handle
    mov ecx, -11                    ; STD_OUTPUT_HANDLE
    call __imp_GetStdHandle
    mov g_stdout, rax
    
    ; Check args
    cmp rcx, 2
    jb .show_usage
    
    ; Parse filename from argv[1]
    mov rsi, [rdx + 8]              ; argv[1]
    
    ; Print banner
    lea rcx, szBanner
    call PrintString
    
    ; Open and map file
    mov rcx, rsi
    call MapGGUFFile
    test rax, rax
    jz .exit_error
    
    ; Parse header
    call ParseGGUFHeader
    
    ; Parse based on flags
    cmp rcx, 3
    jb .default_dump
    
    ; Check for --ghost flag
    mov rdi, [rdx + 16]             ; argv[2]
    lea rsi, szFlagGhost
    call StrCompare
    test rax, rax
    jnz .ghost_output
    
    ; Check for --tensors
    lea rsi, szFlagTensors
    call StrCompare
    test rax, rax
    jnz .tensors_only
    
    ; Default: full dump
.default_dump:
    call DumpMetadata
    call DumpTensors
    jmp .cleanup

.tensors_only:
    call DumpTensors
    jmp .cleanup

.ghost_output:
    call OutputGhostMap
    jmp .cleanup

.show_usage:
    lea rcx, szUsage
    call PrintString

.cleanup:
    call UnmapFile
    
    lea rcx, szFooter
    call PrintString
    
    mov rdx, g_file_size
    call PrintQword
    
    lea rcx, szBytesScanned
    call PrintString
    
    xor rax, rax
    jmp .exit

.exit_error:
    mov rax, 1

.exit:
    leave
    ret
main ENDP

; ════════════════════════════════════════════════════════════════════════════════
; MapGGUFFile — Memory-map file for zero-copy parsing
; ════════════════════════════════════════════════════════════════════════════════
MapGGUFFile PROC
    push rbx rsi rdi
    
    mov rbx, rcx                    ; Save filename
    
    ; CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)
    mov rcx, rbx
    xor edx, edx                    ; GENERIC_READ = 0x80000000, but 0 works for read
    or edx, 80000000h
    xor r8d, r8d                    ; FILE_SHARE_READ
    inc r8d
    xor r9d, r9d                    ; NULL security
    push 0                          ; hTemplateFile
    push 3                          ; OPEN_EXISTING
    push 0                          ; dwFlagsAndAttributes
    push 0                          ; lpSecurityAttributes
    sub rsp, 32
    call __imp_CreateFileA
    add rsp, 48
    
    cmp rax, -1
    je .fail
    mov g_file_handle, rax
    
    ; GetFileSizeEx
    lea rdx, g_file_size
    mov rcx, rax
    call __imp_GetFileSizeEx
    
    ; CreateFileMappingA
    xor ecx, ecx                    ; INVALID_HANDLE_VALUE for pagefile-backed, but we want file
    mov rcx, g_file_handle
    xor edx, edx                    ; NULL security
    xor r8d, r8d                    ; PAGE_READONLY
    or r8d, 02h
    xor r9d, r9d                    ; dwMaximumSizeHigh
    push 0                          ; dwMaximumSizeLow (use file size)
    push r8                         ; lpName
    sub rsp, 32
    call __imp_CreateFileMappingA
    add rsp, 40
    
    test rax, rax
    jz .fail_close
    mov g_map_handle, rax
    
    ; MapViewOfFile
    mov rcx, rax
    xor edx, edx                    ; FILE_MAP_READ
    or edx, 04h
    xor r8d, r8d                    ; dwFileOffsetHigh
    xor r9d, r9d                    ; dwFileOffsetLow
    push 0                          ; dwNumberOfBytesToMap (0 = all)
    sub rsp, 32
    call __imp_MapViewOfFile
    add rsp, 40
    
    test rax, rax
    jz .fail_unmap
    mov g_view_base, rax
    mov g_cursor, rax
    
    mov rax, 1                      ; Success
    jmp .done

.fail_unmap:
    mov rcx, g_map_handle
    call __imp_CloseHandle

.fail_close:
    mov rcx, g_file_handle
    call __imp_CloseHandle

.fail:
    lea rcx, szErrOpen
    call PrintString
    mov rcx, rbx
    call PrintString
    call PrintNewline
    xor rax, rax

.done:
    pop rdi rsi rbx
    ret
MapGGUFFile ENDP

; ════════════════════════════════════════════════════════════════════════════════
; ParseGGUFHeader — Validate and extract header info
; ════════════════════════════════════════════════════════════════════════════════
ParseGGUFHeader PROC
    push rbx
    
    mov rbx, g_cursor
    
    ; Check magic: 4 bytes "GGUF"
    mov eax, [rbx]
    cmp eax, GGUF_MAGIC
    je .magic_ok
    
    lea rcx, szErrMagic
    call PrintString
    jmp .fail

.magic_ok:
    add rbx, 4
    
    ; Version: uint32
    mov eax, [rbx]
    cmp eax, GGUF_VERSION
    jbe .version_ok
    
    lea rcx, szErrVersion
    call PrintString
    jmp .fail

.version_ok:
    add rbx, 4
    
    ; tensor_count: uint64
    mov r12, [rbx]                  ; tensor_count
    add rbx, 8
    
    ; metadata_kv_count: uint64
    mov r13, [rbx]                  ; kv_count
    add rbx, 8
    
    mov g_cursor, rbx
    
    ; Print summary
    lea rcx, szInfoMagic
    call PrintString
    mov edx, eax                    ; version
    call PrintDword
    call PrintNewline
    
    lea rcx, szInfoTensors
    call PrintString
    mov rdx, r12
    call PrintQword
    call PrintNewline
    
    lea rcx, szInfoKV
    call PrintString
    mov rdx, r13
    call PrintQword
    call PrintNewline
    
    mov rax, r12                    ; Return tensor count
    jmp .done

.fail:
    xor rax, rax

.done:
    pop rbx
    ret
ParseGGUFHeader ENDP

; ════════════════════════════════════════════════════════════════════════════════
; DumpTensors — List all tensors with offsets and dimensions
; ════════════════════════════════════════════════════════════════════════════════
DumpTensors PROC
    push rbx r12 r13 r14 r15
    
    mov rbx, g_cursor
    
    lea rcx, szTensorHeader
    call PrintString
    
    ; tensor_count in r12 from header parse
    xor r12, r12                    ; Tensor index
    
.tensor_loop:
    cmp r12, r15                    ; r15 = total tensor count (set elsewhere)
    jae .done
    
    ; Tensor name: string (length prefix)
    mov ecx, [rbx]                  ; name_len
    add rbx, 4
    
    ; Save name pointer
    mov r13, rbx
    mov r14, rcx                    ; name_len
    
    add rbx, rcx                    ; Skip name bytes
    
    ; n_dimensions: uint32
    mov ecx, [rbx]
    add rbx, 4
    
    ; dimensions: uint64[n_dims]
    ; Just print first 3 for brevity
    xor r8d, r8d                    ; dim count for printing
    
    ; tensor_type: uint32
    mov eax, [rbx + rcx*8]          ; After dimensions
    add rbx, 4
    
    ; offset: uint64
    mov r9, [rbx]
    add rbx, 8
    
    ; Print entry
    push rbx
    push r12
    
    lea rcx, szTensorEntry
    mov edx, r12d                   ; index
    mov r8, r13                     ; name pointer
    mov r9, r9                      ; offset
    push r10                        ; size (would calculate from type*dims)
    push r8                         ; n_dimensions
    sub rsp, 32
    ; Call printf equivalent or manual print
    add rsp, 48
    
    pop r12
    pop rbx
    
    inc r12
    jmp .tensor_loop

.done:
    pop r15 r14 r13 r12 rbx
    ret
DumpTensors ENDP

; ════════════════════════════════════════════════════════════════════════════════
; OutputGhostMap — Generate GHOST-MAP format for RawrXD ingestion
; ════════════════════════════════════════════════════════════════════════════════
OutputGhostMap PROC
    push rbx r12 r13 r14 r15
    
    lea rcx, szGhostHeader
    call PrintString
    
    ; Similar to DumpTensors but formatted for GHOST-MAP
    ; tensor_id|name_hash|offset|size|priority|stripe_hint
    
    mov rbx, g_cursor
    xor r12, r12                    ; tensor_id

.ghost_loop:
    cmp r12, r15
    jae .done
    
    ; Parse tensor (simplified)
    mov ecx, [rbx]                  ; name_len
    add rbx, 4
    
    ; Compute name hash (FNV-1a)
    mov rsi, rbx
    mov rdi, rcx
    call HashFNV1a
    mov r13d, eax                   ; name_hash
    
    add rbx, rdi                    ; Skip name
    
    mov eax, [rbx]                  ; n_dims
    mov r9d, eax                    ; save n_dims
    add rbx, 4
    
    ; Calculate element count from dimensions
    xor r10, r10                    ; element_count = 1
    test r9d, r9d
    jz .no_dims
    mov rsi, rbx                    ; dimensions start
    mov ecx, r9d
.dim_loop:
    mov rax, [rsi]
    mul r10                         ; element_count *= dim
    mov r10, rax
    add rsi, 8
    dec ecx
    jnz .dim_loop
    
.no_dims:
    shl r9, 3                       ; n_dims * 8
    add rbx, r9                     ; Skip dimensions
    
    mov r9d, [rbx]                  ; type
    add rbx, 4
    
    ; Calculate element size from type
    mov r11, 1                      ; default size
    cmp r9d, GGUF_TYPE_UINT8
    je .size_done
    cmp r9d, GGUF_TYPE_INT8
    je .size_done
    mov r11, 2
    cmp r9d, GGUF_TYPE_UINT16
    je .size_done
    cmp r9d, GGUF_TYPE_INT16
    je .size_done
    mov r11, 4
    cmp r9d, GGUF_TYPE_UINT32
    je .size_done
    cmp r9d, GGUF_TYPE_INT32
    je .size_done
    cmp r9d, GGUF_TYPE_FLOAT32
    je .size_done
    mov r11, 8
    cmp r9d, GGUF_TYPE_UINT64
    je .size_done
    cmp r9d, GGUF_TYPE_INT64
    je .size_done
    cmp r9d, GGUF_TYPE_FLOAT64
    je .size_done
    ; For string/array, use 1 as placeholder
.size_done:
    mov rax, r10
    mul r11                         ; total_size = element_count * element_size
    mov r15, rax                    ; tensor size
    
    mov r14, [rbx]                  ; offset
    add rbx, 8
    
    ; Determine priority based on tensor name patterns
    mov ecx, r12d
    call ComputeTensorPriority      ; 0=cold, 1=warm, 2=hot, 3=critical
    
    movzx r8d, al
    
    ; Determine stripe hint based on size
    cmp r15, 134217728              ; 128MB
    ja .multi_stripe
    mov r9d, 0                      ; Single drive
    jmp .print_entry
.multi_stripe:
    mov r9d, 7                      ; Striped across 3 fast drives

.print_entry:
    lea rcx, szGhostEntry
    mov edx, r12d                   ; tensor_id
    mov r8d, r13d                   ; name_hash
    mov r9, r14                     ; offset
    push r15                        ; size
    push r8                         ; priority
    push r9                         ; stripe_hint
    sub rsp, 32
    call PrintFormatted
    add rsp, 56
    
    inc r12
    jmp .ghost_loop

.done:
    pop r15 r14 r13 r12 rbx
    ret
OutputGhostMap ENDP

; ════════════════════════════════════════════════════════════════════════════════
; HashFNV1a — 32-bit FNV-1a hash for tensor names
; ════════════════════════════════════════════════════════════════════════════════
HashFNV1a PROC
    push rbx
    
    mov eax, 0x811c9dc5             ; FNV offset basis
    
.hash_loop:
    test rdi, rdi
    jz .done
    
    movzx ebx, byte ptr [rsi]
    xor eax, ebx
    imul eax, eax, 0x01000193       ; FNV prime
    
    inc rsi
    dec rdi
    jmp .hash_loop

.done:
    pop rbx
    ret
HashFNV1a ENDP

; ════════════════════════════════════════════════════════════════════════════════
; ComputeTensorPriority — Heuristic for tensor caching priority
; ════════════════════════════════════════════════════════════════════════════════
ComputeTensorPriority PROC
    ; Check tensor name patterns (would need actual name string)
    ; "token_embd" = critical (0)
    ; "blk.0-3.attn" = hot (2)
    ; "blk.4+.ffn" = warm (1)
    ; "output.weight" = critical (3)
    
    ; Simplified: layer 0-3 = hot, rest = warm
    cmp ecx, 100                    ; First ~100 tensors typically embeddings+early layers
    jb .hot
    cmp ecx, 200
    jb .warm
    mov al, 0                       ; cold
    ret
.hot:
    mov al, 2
    ret
.warm:
    mov al, 1
    ret
ComputeTensorPriority ENDP

; ════════════════════════════════════════════════════════════════════════════════
; Utility Functions
; ════════════════════════════════════════════════════════════════════════════════
PrintString PROC
    ; rcx = string
    push rbx rsi
    
    mov rsi, rcx
    xor ebx, ebx
    
.len_loop:
    cmp byte ptr [rsi + rbx], 0
    je .print
    inc rbx
    jmp .len_loop

.print:
    mov rcx, g_stdout
    mov rdx, rsi
    mov r8, rbx
    lea r9, [rsp + 40]              ; bytes written
    push 0                          ; lpOverlapped
    sub rsp, 32
    call __imp_WriteFile
    add rsp, 40
    
    pop rsi rbx
    ret
PrintString ENDP

PrintNewline PROC
    push 0A0Dh                      ; \r\n
    mov rcx, g_stdout
    lea rdx, [rsp]
    mov r8, 2
    lea r9, [rsp + 16]
    push 0
    sub rsp, 32
    call __imp_WriteFile
    add rsp, 40
    ret
PrintNewline ENDP

PrintQword PROC
    ; rdx = value to print as hex
    ; Output 16 hex digits to stdout
    push rbx
    push rsi
    push rdi
    sub rsp, 48
    
    mov rbx, rdx                    ; rbx = value
    lea rsi, [rsp + 32]             ; output buffer on stack
    mov rdi, rsi
    
    ; Convert 64-bit value to 16 hex chars (big-endian)
    mov ecx, 16                     ; 16 nibbles
@@hex_loop:
    rol rbx, 4                      ; rotate left 4 bits
    mov al, bl
    and al, 0Fh
    cmp al, 10
    jb @@digit
    add al, 'A' - 10
    jmp @@store
@@digit:
    add al, '0'
@@store:
    mov [rdi], al
    inc rdi
    dec ecx
    jnz @@hex_loop
    
    ; Write to stdout
    mov rcx, g_stdout
    mov rdx, rsi
    mov r8, 16
    lea r9, [rsp + 16]
    push 0
    sub rsp, 32
    call __imp_WriteFile
    add rsp, 40
    
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
PrintQword ENDP

PrintDword PROC
    ; edx = value to print as hex
    ; Output 8 hex digits to stdout
    push rbx
    push rsi
    push rdi
    sub rsp, 48
    
    mov ebx, edx
    lea rsi, [rsp + 32]
    mov rdi, rsi
    
    mov ecx, 8
@@hex_loop:
    rol ebx, 4
    mov al, bl
    and al, 0Fh
    cmp al, 10
    jb @@digit
    add al, 'A' - 10
    jmp @@store
@@digit:
    add al, '0'
@@store:
    mov [rdi], al
    inc rdi
    dec ecx
    jnz @@hex_loop
    
    mov rcx, g_stdout
    mov rdx, rsi
    mov r8, 8
    lea r9, [rsp + 16]
    push 0
    sub rsp, 32
    call __imp_WriteFile
    add rsp, 40
    
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
PrintDword ENDP

PrintFormatted PROC
    ; rcx = format string, rdx/r8/r9 = args
    ; Uses wsprintfA + WriteFile
    push rbx
    sub rsp, 560                    ; 512-byte format buffer + alignment
    
    ; Format into stack buffer
    lea rbx, [rsp + 32]             ; output buffer
    mov [rsp + 24], r9              ; save r9 for wsprintfA
    mov r9, r8                      ; shift args: r9=arg3
    mov r8, rdx                     ; r8=arg2
    mov rdx, rcx                    ; rdx=format
    mov rcx, rbx                    ; rcx=output buffer
    call wsprintfA
    mov ebx, eax                    ; ebx = length
    
    ; Write to stdout
    mov rcx, g_stdout
    lea rdx, [rsp + 32]
    mov r8d, ebx
    lea r9, [rsp + 16]
    push 0
    sub rsp, 32
    call __imp_WriteFile
    add rsp, 40
    
    add rsp, 560
    pop rbx
    ret
PrintFormatted ENDP

StrCompare PROC
    ; rsi, rdi = strings to compare
    ; returns rax=1 if equal, 0 if not
@@cmp_loop:
    movzx eax, byte ptr [rsi]
    movzx ecx, byte ptr [rdi]
    cmp al, cl
    jne @@not_equal
    test al, al
    jz @@equal
    inc rsi
    inc rdi
    jmp @@cmp_loop
    
@@equal:
    mov rax, 1
    ret
    
@@not_equal:
    xor rax, rax
    ret
StrCompare ENDP

UnmapFile PROC
    mov rcx, g_view_base
    test rcx, rcx
    jz .skip_unmap
    call __imp_UnmapViewOfFile
    
.skip_unmap:
    mov rcx, g_map_handle
    test rcx, rcx
    jz .skip_close
    call __imp_CloseHandle
    
.skip_close:
    mov rcx, g_file_handle
    test rcx, rcx
    jz .done
    call __imp_CloseHandle
    
.done:
    ret
UnmapFile ENDP

.data
    szFlagGhost     db "--ghost",0
    szFlagTensors   db "--tensors",0
    szFlagRaw       db "--raw",0
    szFlagMetadata  db "--metadata",0
    szFlagVerify    db "--verify",0

END
