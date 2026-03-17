;=============================================================================
; rawrxd_probe.asm
; GGUF Metadata Diagnostic Tool - Prevents bad_alloc by analyzing structure
; Usage: rawrxd_probe.exe <model.gguf>
;=============================================================================
INCLUDE \masm64\include64\masm64rt.inc

.const
BUF_SIZE        EQU 65536
GGUF_MAGIC      EQU 0x46554747    ; "GGUF" little-endian
MAX_META_KEY    EQU 1024
MAX_STRING_LEN  EQU 10000000      ; 10MB safety threshold

.data
hHeap           QWORD ?
hFile           QWORD ?
hMap            QWORD ?
pMap            QWORD ?
fileSize        QWORD ?
fmt_header      BYTE 13,10,"[RawrXD Probe] GGUF Structure Analysis",13,10
                BYTE "========================================",13,10,0
fmt_magic       BYTE "Magic:          0x%08X (%s)",13,10,0
fmt_version     BYTE "Version:        %u",13,10,0
fmt_tensors     BYTE "Tensor Count:   %llu",13,10,0
fmt_kv          BYTE "KV Pairs:       %llu",13,10,10,0
fmt_key         BYTE "[0x%04X] Key: %s | Type: %s",0
fmt_skip        BYTE " [SKIPPED %llu bytes - exceeds safety]",13,10,0
fmt_string      BYTE " | Value: %.*s...",13,10,0
fmt_array       BYTE " | Array[%llu] <%s>",13,10,0
newline         BYTE 13,10,0

.align 8
gguf_types      QWORD offset type_u8, offset type_i8, offset type_u16, offset type_i16
                QWORD offset type_u32, offset type_i32, offset type_f32, offset type_bool
                QWORD offset type_str, offset type_arr, offset type_u64, offset type_i64
                QWORD offset type_f64
type_u8         BYTE "U8",0
type_i8         BYTE "I8",0
type_u16        BYTE "U16",0
type_i16        BYTE "I16",0
type_u32        BYTE "U32",0
type_i32        BYTE "I32",0
type_f32        BYTE "F32",0
type_bool       BYTE "BOOL",0
type_str        BYTE "STRING",0
type_arr        BYTE "ARRAY",0
type_u64        BYTE "U64",0
type_i64        BYTE "I64",0
type_f64        BYTE "F64",0

.data?
readPtr         QWORD ?
tmpKey          BYTE MAX_META_KEY dup(?)

.code
;----------------------------------------------------------------------
; ReadLE64 - Read 8 bytes little-endian from mapped memory
;----------------------------------------------------------------------
ReadLE64 PROC
    mov     rax, QWORD PTR [r12]
    add     r12, 8
    xchg    al, ah
    rol     eax, 16
    xchg    al, ah
    rol     rax, 32
    xchg    al, ah
    rol     eax, 16
    xchg    al, ah
    ret
ReadLE64 ENDP

;----------------------------------------------------------------------
; ReadString - Read GGUF string (len:u64, data[..]) 
; Returns: RAX=length, RDX=data pointer (or NULL if skipped)
;----------------------------------------------------------------------
ReadString PROC 
    push    rbx
    push    rsi
    
    call    ReadLE64        ; RAX = length
    mov     rbx, rax
    
    .IF rax > MAX_STRING_LEN
        ; Skip oversized - advance pointer but return NULL
        add     r12, rbx
        xor     rdx, rdx
        mov     rax, rbx    ; Return original size for diagnostics
        jmp     @F
    .ENDIF
    
    mov     rdx, r12        ; Return data pointer
    add     r12, rbx        ; Advance read pointer
    mov     rax, rbx        ; Return length
    
@@: pop     rsi
    pop     rbx
    ret
ReadString ENDP

;----------------------------------------------------------------------
; PrintType - Get type name from index
;----------------------------------------------------------------------
PrintType PROC idx:QWORD
    mov     rcx, idx
    cmp     rcx, 12
    jae     @@unknown
    
    mov     rax, gguf_types[rcx*8]
    ret
    
@@unknown:
    mov     rax, offset type_str    ; Default to STRING for unknown
    ret
PrintType ENDP

;----------------------------------------------------------------------
; Main Entry
;----------------------------------------------------------------------
Main PROC
    LOCAL   argc:QWORD, argv:QWORD
    
    ; Get process heap
    invoke  GetProcessHeap
    mov     hHeap, rax
    
    ; Parse command line
    invoke  GetCommandLineW
    lea     rcx, argc
    invoke  CommandLineToArgvW, rax, rcx
    mov     argv, rax
    
    .IF argc < 2
        invoke  MessageBoxA, 0, "Usage: rawrxd_probe <model.gguf>", "RawrXD Probe", MB_OK
        jmp     Cleanup
    .ENDIF
    
    ; Convert wide to ANSI for CreateFileA
    mov     rsi, argv
    mov     rsi, [rsi+16]       ; argv[1]
    invoke  WideCharToMultiByte, CP_ACP, 0, rsi, -1, ADDR tmpKey, MAX_META_KEY, 0, 0
    
    ; Open file
    invoke  CreateFileA, ADDR tmpKey, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    mov     hFile, rax
    cmp     rax, INVALID_HANDLE_VALUE
    je      FileError
    
    ; Get file size
    invoke  GetFileSizeEx, hFile, ADDR fileSize
    
    ; Memory map for fast access
    invoke  CreateFileMappingA, hFile, 0, PAGE_READONLY, 0, 0, 0
    mov     hMap, rax
    invoke  MapViewOfFile, rax, FILE_MAP_READ, 0, 0, 0
    mov     pMap, rax
    mov     r12, rax            ; r12 = read pointer
    
    ; Validate GGUF magic
    mov     eax, DWORD PTR [r12]
    cmp     eax, GGUF_MAGIC
    jne     InvalidFormat
    add     r12, 4
    
    ; Print header
    invoke  printf, ADDR fmt_header
    
    ; Read version
    mov     eax, DWORD PTR [r12]
    add     r12, 4
    invoke  printf, ADDR fmt_magic, GGUF_MAGIC, ADDR type_str
    invoke  printf, ADDR fmt_version, eax
    
    ; Read tensor count
    call    ReadLE64
    push    rax
    invoke  printf, ADDR fmt_tensors, rax
    
    ; Read metadata count
    call    ReadLE64
    push    rax
    invoke  printf, ADDR fmt_kv, rax
    
    pop     rcx                 ; Metadata count
    mov     r13, rcx            ; Loop counter
    
    ; Process each KV pair
@@metadata_loop:
    dec     r13
    js      @@done_meta
    
    ; Read key (string: len + data)
    call    ReadString          ; RAX=len, RDX=data
    .IF rdx == 0
        ; Oversized key name - critical corruption
        invoke  printf, "ERROR: Corrupted key name (len=%llu)", rax
        jmp     Cleanup
    .ENDIF
    
    ; Copy key to buffer (null terminate)
    mov     rsi, rdx
    lea     rdi, tmpKey
    mov     rcx, rax
    cmp     rax, MAX_META_KEY-1
    jbe     @@copy_ok
    mov     rcx, MAX_META_KEY-1
@@copy_ok:
    rep     movsb
    mov     BYTE PTR [rdi], 0
    
    ; Read value type
    movzx   eax, WORD PTR [r12]
    add     r12, 2
    push    rax                 ; Type index
    
    ; Print key info
    mov     rcx, [rsp]
    call    PrintType
    invoke  printf, ADDR fmt_key, r13, ADDR tmpKey, rax
    pop     rax
    
    ; Handle by type
    cmp     eax, 8              ; STRING
    je      @@handle_string
    cmp     eax, 9              ; ARRAY
    je      @@handle_array
    cmp     eax, 10             ; U64
    jbe     @@scalar
    
    ; Unknown/skip
    add     r12, 8
    invoke  printf, ADDR newline
    jmp     @@metadata_loop
    
@@scalar:
    ; Skip scalar (8 bytes max)
    add     r12, 8
    invoke  printf, ADDR newline
    jmp     @@metadata_loop
    
@@handle_string:
    push    r13
    call    ReadString          ; RAX=len, RDX=data
    pop     r13
    
    .IF rdx == 0
        ; Skipped - print warning
        invoke  printf, ADDR fmt_skip, rax
        ; Check if this is the problematic chat_template
        invoke  strstr, ADDR tmpKey, "chat_template"
        .IF rax != 0
            invoke  printf, " ^^^ CRITICAL: chat_template oversized!",13,10
        .ENDIF
    .ELSE
        ; Print preview (first 80 chars)
        mov     rcx, rax
        cmp     rcx, 80
        jbe     @@len_ok
        mov     rcx, 80
@@len_ok:
        invoke  printf, ADDR fmt_string, eax, rdx
    .ENDIF
    jmp     @@metadata_loop
    
@@handle_array:
    ; Array: type (2) + count (8) + data
    movzx   ebx, WORD PTR [r12]     ; Element type
    add     r12, 2
    call    ReadLE64                ; Count
    mov     r14, rax
    
    ; Calculate skip size
    mov     r15, rax
    cmp     ebx, 0                  ; U8
    je      @@arr_u8
    cmp     ebx, 8                  ; String array (complex)
    je      @@arr_string
    cmp     ebx, 10                 ; U64/I64/F64
    je      @@arr_8
    cmp     ebx, 4                  ; U32/I32/F32
    je      @@arr_4
    cmp     ebx, 2                  ; U16/I16
    je      @@arr_2
    jmp     @@arr_default
    
@@arr_u8:
    invoke  printf, ADDR fmt_array, r14, ADDR type_u8
    jmp     @@arr_skip
    
@@arr_string:
    ; String array - dangerous, skip carefully
    invoke  printf, ADDR fmt_array, r14, ADDR type_str
    invoke  printf, " [SKIPPING STRING ARRAY - MANUAL REVIEW REQUIRED]",13,10
    ; Must iterate to skip properly
    mov     r8, r14
@@skip_str_loop:
    dec     r8
    js      @@metadata_loop
    call    ReadString          ; Skip each string
    jmp     @@skip_str_loop
    
@@arr_8:
    shl     r15, 3
    jmp     @@arr_skip
@@arr_4:
    shl     r15, 2
    jmp     @@arr_skip
@@arr_2:
    shl     r15, 1
    jmp     @@arr_skip
@@arr_default:
    shl     r15, 3
    
@@arr_skip:
    add     r12, r15
    jmp     @@metadata_loop
    
@@done_meta:
    invoke  printf, 13,10,"[Probe] Metadata scan complete. File pointer at 0x%llX", r12
    invoke  printf, ADDR newline
    
Cleanup:
    .IF pMap
        invoke  UnmapViewOfFile, pMap
    .ENDIF
    .IF hMap
        invoke  CloseHandle, hMap
    .ENDIF
    .IF hFile
        invoke  CloseHandle, hFile
    .ENDIF
    invoke  ExitProcess, 0
    
FileError:
    invoke  printf, "ERROR: Cannot open file",13,10
    jmp     Cleanup
    
InvalidFormat:
    invoke  printf, "ERROR: Invalid GGUF magic",13,10
    jmp     Cleanup

Main ENDP
END
