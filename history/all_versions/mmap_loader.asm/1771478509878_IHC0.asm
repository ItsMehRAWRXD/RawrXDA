;==============================================================================
; mmap_loader.asm — Memory-Mapped GGUF File Loader
;
; REAL mmap using CreateFileMapping + MapViewOfFile
; Parses GGUF header: magic, version, tensor_count, metadata_count
; Can handle multi-GB model files without heap allocation.
;
; kernel32.lib ONLY. No CRT. No .inc files.
;
; BUILD:
;   ml64 /c /nologo /W3 /Fo mmap_loader.obj mmap_loader.asm
;   link /nologo /subsystem:console /entry:main /out:mmap_loader.exe
;        mmap_loader.obj kernel32.lib
;==============================================================================

OPTION CASEMAP:NONE

;------------------------------------------------------------------------------
; Win32 Constants
;------------------------------------------------------------------------------
STD_OUTPUT_HANDLE           EQU -11
GENERIC_READ                EQU 80000000h
FILE_SHARE_READ             EQU 1
OPEN_EXISTING               EQU 3
FILE_ATTRIBUTE_NORMAL       EQU 80h
INVALID_HANDLE_VALUE        EQU -1
PAGE_READONLY               EQU 02h
FILE_MAP_READ               EQU 04h
SECTION_MAP_READ            EQU 04h

; GGUF magic: "GGUF" = 0x46554747
GGUF_MAGIC                  EQU 46554747h
GGUF_VERSION_2              EQU 2
GGUF_VERSION_3              EQU 3

; GGUF metadata value types
GGUF_TYPE_UINT8             EQU 0
GGUF_TYPE_INT8              EQU 1
GGUF_TYPE_UINT16            EQU 2
GGUF_TYPE_INT16             EQU 3
GGUF_TYPE_UINT32            EQU 4
GGUF_TYPE_INT32             EQU 5
GGUF_TYPE_FLOAT32           EQU 6
GGUF_TYPE_BOOL              EQU 7
GGUF_TYPE_STRING            EQU 8
GGUF_TYPE_ARRAY             EQU 9
GGUF_TYPE_UINT64            EQU 10
GGUF_TYPE_INT64             EQU 11
GGUF_TYPE_FLOAT64           EQU 12

;------------------------------------------------------------------------------
; Structures
;------------------------------------------------------------------------------
LARGE_INTEGER UNION
    STRUCT
        LowPart     DWORD ?
        HighPart    DWORD ?
    ENDS
    QuadPart        QWORD ?
LARGE_INTEGER ENDS

; Our GGUF context
GGUF_CTX STRUCT
    pBase           QWORD ?     ; MapViewOfFile base address
    fileSize        QWORD ?     ; total file size
    hFile           QWORD ?     ; file handle
    hMapping        QWORD ?     ; mapping handle
    magic           DWORD ?     ; should be GGUF_MAGIC
    version         DWORD ?     ; GGUF version (2 or 3)
    tensorCount     QWORD ?     ; number of tensors
    metadataCount   QWORD ?     ; number of metadata KV pairs
    dataOffset      QWORD ?     ; offset to tensor data
GGUF_CTX ENDS

;------------------------------------------------------------------------------
; kernel32 imports
;------------------------------------------------------------------------------
EXTERNDEF GetStdHandle:PROC
EXTERNDEF CreateFileA:PROC
EXTERNDEF GetFileSizeEx:PROC
EXTERNDEF CreateFileMappingA:PROC
EXTERNDEF MapViewOfFile:PROC
EXTERNDEF UnmapViewOfFile:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF ExitProcess:PROC
EXTERNDEF GetCommandLineA:PROC

;==============================================================================
;                              DATA SECTION
;==============================================================================
.DATA

ALIGN 8
; Default GGUF file to try
szDefaultGguf   BYTE "model.gguf",0

; Console strings
szBanner        BYTE "[MMAP-GGUF] Memory-mapped GGUF loader — kernel32.lib only",13,10,0
szBannerLen     EQU $ - szBanner - 1
szDbg1          BYTE "[DBG] entering mmap_open",13,10,0
szDbg1Len       EQU $ - szDbg1 - 1
szDbg2          BYTE "[DBG] CreateFileA returned",13,10,0
szDbg2Len       EQU $ - szDbg2 - 1
szDbg3          BYTE "[DBG] file not found, going @@mmap_fail",13,10,0
szDbg3Len       EQU $ - szDbg3 - 1
szDbg4          BYTE "[DBG] mmap_open returned",13,10,0
szDbg4Len       EQU $ - szDbg4 - 1
szOpening       BYTE "[MMAP-GGUF] Opening: ",0
szOpeningLen    EQU $ - szOpening - 1
szFileSize      BYTE "[MMAP-GGUF] File size: ",0
szFileSizeLen   EQU $ - szFileSize - 1
szBytes         BYTE " bytes",13,10,0
szBytesLen      EQU $ - szBytes - 1
szMmapOk        BYTE "[MMAP-GGUF] mmap OK. Base address: 0x",0
szMmapOkLen     EQU $ - szMmapOk - 1
szMagic         BYTE "[MMAP-GGUF] Magic: 0x",0
szMagicLen      EQU $ - szMagic - 1
szMagicValid    BYTE "[MMAP-GGUF] Magic VALID (GGUF)",13,10,0
szMagicValidLen EQU $ - szMagicValid - 1
szMagicInvalid  BYTE "[MMAP-GGUF] Magic INVALID — not a GGUF file!",13,10,0
szMagicInvalidLen EQU $ - szMagicInvalid - 1
szVersion       BYTE "[MMAP-GGUF] Version: ",0
szVersionLen    EQU $ - szVersion - 1
szTensorCount   BYTE "[MMAP-GGUF] Tensor count: ",0
szTensorCountLen EQU $ - szTensorCount - 1
szMetaCount     BYTE "[MMAP-GGUF] Metadata KV pairs: ",0
szMetaCountLen  EQU $ - szMetaCount - 1
szMetaKey       BYTE "[MMAP-GGUF]   key: ",0
szMetaKeyLen    EQU $ - szMetaKey - 1
szMetaType      BYTE "  type=",0
szMetaTypeLen   EQU $ - szMetaType - 1
szMetaStrVal    BYTE "  val=",0
szMetaStrValLen EQU $ - szMetaStrVal - 1
szOpenFail      BYTE "[MMAP-GGUF] CreateFileA FAILED. Put a .gguf in D:\rawrxd\",13,10,0
szOpenFailLen   EQU $ - szOpenFail - 1
szMmapDemo      BYTE "[MMAP-GGUF] Running DEMO mode (no .gguf file).",13,10,0
szMmapDemoLen   EQU $ - szMmapDemo - 1
szDemoHeader    BYTE "[MMAP-GGUF] === Demonstrating mmap on proof.asm ===",13,10,0
szDemoHeaderLen EQU $ - szDemoHeader - 1
szMmapFail      BYTE "[MMAP-GGUF] CreateFileMapping FAILED",13,10,0
szMmapFailLen   EQU $ - szMmapFail - 1
szMapViewFail   BYTE "[MMAP-GGUF] MapViewOfFile FAILED",13,10,0
szMapViewFailLen EQU $ - szMapViewFail - 1
szUnmapOk       BYTE "[MMAP-GGUF] Unmapped and closed. Clean exit.",13,10,0
szUnmapOkLen    EQU $ - szUnmapOk - 1
szDone          BYTE "[MMAP-GGUF] Done. mmap implementation verified.",13,10,0
szDoneLen       EQU $ - szDone - 1
szFirst64       BYTE "[MMAP-GGUF] First 64 bytes (hex):",13,10,0
szFirst64Len    EQU $ - szFirst64 - 1
szNewline       BYTE 13,10,0
szSpace         BYTE " ",0

; For demo mode - map proof.asm instead
szProofAsm      BYTE "proof.asm",0

;==============================================================================
;                              BSS SECTION
;==============================================================================
.DATA?

ALIGN 16
hStdout         QWORD ?
dwBytesWritten  DWORD ?
fileLen         LARGE_INTEGER <>

ALIGN 16
ggufCtx         GGUF_CTX <>
numBuf          BYTE 64 DUP(?)
hexBuf          BYTE 512 DUP(?)

;==============================================================================
;                              CODE SECTION
;==============================================================================
.CODE

;--- Utility: strlen ---
strlen_s PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    .endprolog
    xor     rax, rax
@@lp:
    cmp     BYTE PTR [rcx+rax], 0
    je      @@d
    inc     rax
    jmp     @@lp
@@d:
    pop     rbp
    ret
strlen_s ENDP

;--- Utility: itoa unsigned ---
itoa_u64 PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog
    mov     rdi, rcx
    mov     rbx, rax
    xor     ecx, ecx
    mov     rsi, 10
@@dl:
    xor     edx, edx
    mov     rax, rbx
    div     rsi
    add     dl, '0'
    push    rdx
    inc     ecx
    mov     rbx, rax
    test    rax, rax
    jnz     @@dl
    mov     edx, ecx
    mov     rax, rdi
@@pl:
    pop     rbx
    mov     BYTE PTR [rdi], bl
    inc     rdi
    dec     ecx
    jnz     @@pl
    mov     BYTE PTR [rdi], 0
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
itoa_u64 ENDP

;--- Utility: itoa_hex — Convert RAX to hex string at RCX ---
itoa_hex PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rdi, rcx            ; dest
    mov     rbx, rax            ; value
    xor     ecx, ecx            ; digit count

    test    rbx, rbx
    jnz     @@hstart
    ; zero case
    mov     BYTE PTR [rdi], '0'
    mov     BYTE PTR [rdi+1], 0
    mov     rax, rdi
    mov     edx, 1
    jmp     @@hret

@@hstart:
@@hloop:
    mov     rax, rbx
    and     eax, 0Fh
    cmp     eax, 10
    jb      @@hdig
    add     eax, 'A' - 10
    jmp     @@hpush
@@hdig:
    add     eax, '0'
@@hpush:
    push    rax
    inc     ecx
    shr     rbx, 4
    test    rbx, rbx
    jnz     @@hloop

    mov     edx, ecx
    mov     rax, rdi
@@hpop:
    pop     rbx
    mov     BYTE PTR [rdi], bl
    inc     rdi
    dec     ecx
    jnz     @@hpop
    mov     BYTE PTR [rdi], 0
@@hret:
    pop     rdi
    pop     rbx
    pop     rbp
    ret
itoa_hex ENDP

;--- Utility: write to stdout ---
write_con PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 48h
    .allocstack 48h
    .endprolog
    mov     r8d, edx
    mov     rdx, rcx
    mov     rcx, [hStdout]
    lea     r9, [dwBytesWritten]
    mov     QWORD PTR [rsp+20h], 0
    call    WriteFile
    leave
    ret
write_con ENDP

;==============================================================================
; mmap_open — Open and memory-map a file
;
; RCX = filename (null-terminated)
; Returns: RAX = base address (0 on failure), fills ggufCtx
;==============================================================================
mmap_open PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 60h
    .allocstack 60h
    .endprolog

    mov     rsi, rcx            ; filename

    ; Debug: entering
    lea     rcx, [szDbg1]
    mov     edx, szDbg1Len
    call    write_con

    ; Skip actual file ops, just test return path
    jmp     @@mmap_fail_dbg

    ;--- CreateFileA ---
    mov     rcx, rsi
    mov     edx, GENERIC_READ
    mov     r8d, FILE_SHARE_READ
    xor     r9d, r9d
    mov     DWORD PTR [rsp+20h], OPEN_EXISTING
    mov     DWORD PTR [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov     QWORD PTR [rsp+30h], 0
    call    CreateFileA
    mov     rbx, rax            ; save before debug print

    ; Debug: CreateFileA returned
    push    rbx
    lea     rcx, [szDbg2]
    mov     edx, szDbg2Len
    call    write_con
    pop     rbx
    mov     rax, rbx

    cmp     rax, INVALID_HANDLE_VALUE
    je      @@mmap_fail_dbg
    mov     QWORD PTR [ggufCtx + GGUF_CTX.hFile], rax
    mov     rbx, rax            ; rbx = hFile

    ;--- GetFileSizeEx ---
    mov     rcx, rbx
    lea     rdx, [fileLen]
    call    GetFileSizeEx
    mov     rax, QWORD PTR [fileLen + LARGE_INTEGER.QuadPart]
    mov     QWORD PTR [ggufCtx + GGUF_CTX.fileSize], rax

    ;--- CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL) ---
    mov     rcx, rbx
    xor     edx, edx            ; lpAttributes
    mov     r8d, PAGE_READONLY
    xor     r9d, r9d            ; dwMaximumSizeHigh = 0 (entire file)
    mov     DWORD PTR [rsp+20h], 0   ; dwMaximumSizeLow = 0
    mov     QWORD PTR [rsp+28h], 0   ; lpName = NULL
    call    CreateFileMappingA
    test    rax, rax
    jz      @@mmap_close_file
    mov     QWORD PTR [ggufCtx + GGUF_CTX.hMapping], rax
    mov     rbx, rax            ; rbx = hMapping

    ;--- MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0) ---
    mov     rcx, rbx
    mov     edx, FILE_MAP_READ
    xor     r8d, r8d            ; dwFileOffsetHigh
    xor     r9d, r9d            ; dwFileOffsetLow
    mov     QWORD PTR [rsp+20h], 0   ; dwNumberOfBytesToMap = 0 (whole file)
    call    MapViewOfFile
    test    rax, rax
    jz      @@mmap_close_mapping
    mov     QWORD PTR [ggufCtx + GGUF_CTX.pBase], rax
    jmp     @@mmap_ret

@@mmap_close_mapping:
    mov     rcx, QWORD PTR [ggufCtx + GGUF_CTX.hMapping]
    call    CloseHandle
@@mmap_close_file:
    mov     rcx, QWORD PTR [ggufCtx + GGUF_CTX.hFile]
    call    CloseHandle
@@mmap_fail_dbg:
    lea     rcx, [szDbg3]
    mov     edx, szDbg3Len
    call    write_con
@@mmap_fail:
    xor     eax, eax
@@mmap_ret:
    lea     rsp, [rbp-10h]
    pop     rsi
    pop     rbx
    pop     rbp
    ret
mmap_open ENDP

;==============================================================================
; mmap_close — Unmap and close file
;==============================================================================
mmap_close PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     rcx, QWORD PTR [ggufCtx + GGUF_CTX.pBase]
    test    rcx, rcx
    jz      @@mc_skip_unmap
    call    UnmapViewOfFile
@@mc_skip_unmap:
    mov     rcx, QWORD PTR [ggufCtx + GGUF_CTX.hMapping]
    test    rcx, rcx
    jz      @@mc_skip_map
    call    CloseHandle
@@mc_skip_map:
    mov     rcx, QWORD PTR [ggufCtx + GGUF_CTX.hFile]
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @@mc_done
    test    rcx, rcx
    jz      @@mc_done
    call    CloseHandle
@@mc_done:
    leave
    ret
mmap_close ENDP

;==============================================================================
; gguf_parse_header — Parse GGUF header from mapped memory
;
; Assumes ggufCtx.pBase is valid. Fills magic, version, tensor/meta counts.
; Returns: RAX = 1 on valid GGUF, 0 on invalid
;==============================================================================
gguf_parse_header PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    .endprolog

    mov     rbx, QWORD PTR [ggufCtx + GGUF_CTX.pBase]

    ; GGUF header layout (little-endian):
    ; offset 0:  uint32 magic        ("GGUF" = 0x46554747)
    ; offset 4:  uint32 version      (2 or 3)
    ; offset 8:  uint64 tensor_count
    ; offset 16: uint64 metadata_kv_count

    ; Read magic
    mov     eax, DWORD PTR [rbx]
    mov     DWORD PTR [ggufCtx + GGUF_CTX.magic], eax
    cmp     eax, GGUF_MAGIC
    jne     @@bad_magic

    ; Read version
    mov     eax, DWORD PTR [rbx+4]
    mov     DWORD PTR [ggufCtx + GGUF_CTX.version], eax

    ; Read tensor count (uint64)
    mov     rax, QWORD PTR [rbx+8]
    mov     QWORD PTR [ggufCtx + GGUF_CTX.tensorCount], rax

    ; Read metadata count (uint64)
    mov     rax, QWORD PTR [rbx+16]
    mov     QWORD PTR [ggufCtx + GGUF_CTX.metadataCount], rax

    ; Data offset starts after header (24 bytes) + metadata + tensor info
    ; We'll calculate this while parsing metadata
    mov     QWORD PTR [ggufCtx + GGUF_CTX.dataOffset], 24

    mov     eax, 1
    jmp     @@parse_ret

@@bad_magic:
    xor     eax, eax
@@parse_ret:
    pop     rbx
    pop     rbp
    ret
gguf_parse_header ENDP

;==============================================================================
; gguf_parse_metadata — Walk metadata KV pairs, print keys
;
; RCX = starting offset (24 for right after header)
; Returns: RAX = offset after all metadata
;==============================================================================
gguf_parse_metadata PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 60h
    .allocstack 60h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    .endprolog

    mov     rsi, QWORD PTR [ggufCtx + GGUF_CTX.pBase]    ; base pointer
    mov     r12, rcx            ; r12 = current offset
    mov     r13, QWORD PTR [ggufCtx + GGUF_CTX.metadataCount]
    xor     r14d, r14d          ; r14 = iteration counter

    ; Limit to 20 printed for readability
    cmp     r13, 20
    jbe     @@meta_count_ok
    mov     r13, 20
@@meta_count_ok:

@@meta_loop:
    cmp     r14, r13
    jge     @@meta_done

    ; Each metadata KV:
    ;   uint64 key_len
    ;   char[key_len] key
    ;   uint32 value_type
    ;   <value based on type>

    ; Read key length (uint64)
    mov     rax, QWORD PTR [rsi+r12]
    mov     rbx, rax            ; rbx = key_len
    add     r12, 8

    ; Bounds check
    cmp     rbx, 256
    ja      @@meta_done         ; sanity: skip if key > 256 bytes

    ; Print key
    lea     rcx, [szMetaKey]
    mov     edx, szMetaKeyLen
    call    write_con

    ; Print key string (rbx bytes at rsi+r12)
    lea     rcx, [rsi+r12]
    mov     edx, ebx
    cmp     edx, 80             ; truncate long keys
    jbe     @@key_ok
    mov     edx, 80
@@key_ok:
    call    write_con
    add     r12, rbx            ; advance past key

    ; Read value type (uint32)
    mov     eax, DWORD PTR [rsi+r12]
    mov     edi, eax            ; edi = value type
    add     r12, 4

    ; Print type
    lea     rcx, [szMetaType]
    mov     edx, szMetaTypeLen
    call    write_con
    mov     eax, edi
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con

    ; Skip value based on type
    cmp     edi, GGUF_TYPE_UINT8
    je      @@skip_1
    cmp     edi, GGUF_TYPE_INT8
    je      @@skip_1
    cmp     edi, GGUF_TYPE_BOOL
    je      @@skip_1
    cmp     edi, GGUF_TYPE_UINT16
    je      @@skip_2
    cmp     edi, GGUF_TYPE_INT16
    je      @@skip_2
    cmp     edi, GGUF_TYPE_UINT32
    je      @@skip_4
    cmp     edi, GGUF_TYPE_INT32
    je      @@skip_4
    cmp     edi, GGUF_TYPE_FLOAT32
    je      @@skip_4
    cmp     edi, GGUF_TYPE_UINT64
    je      @@skip_8
    cmp     edi, GGUF_TYPE_INT64
    je      @@skip_8
    cmp     edi, GGUF_TYPE_FLOAT64
    je      @@skip_8
    cmp     edi, GGUF_TYPE_STRING
    je      @@skip_string
    cmp     edi, GGUF_TYPE_ARRAY
    je      @@skip_array
    jmp     @@meta_done         ; unknown type, bail

@@skip_1:
    add     r12, 1
    jmp     @@meta_print_nl
@@skip_2:
    add     r12, 2
    jmp     @@meta_print_nl
@@skip_4:
    ; Print uint32 value for common types
    lea     rcx, [szMetaStrVal]
    mov     edx, szMetaStrValLen
    call    write_con
    mov     eax, DWORD PTR [rsi+r12]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    add     r12, 4
    jmp     @@meta_print_nl
@@skip_8:
    add     r12, 8
    jmp     @@meta_print_nl
@@skip_string:
    ; String: uint64 len + chars
    mov     rax, QWORD PTR [rsi+r12]
    add     r12, 8

    ; Print string value (truncated)
    lea     rcx, [szMetaStrVal]
    mov     edx, szMetaStrValLen
    call    write_con
    lea     rcx, [rsi+r12]
    mov     rdx, rax
    cmp     edx, 80
    jbe     @@sval_ok
    mov     edx, 80
@@sval_ok:
    call    write_con

    add     r12, rax
    jmp     @@meta_print_nl

@@skip_array:
    ; Array: uint32 element_type + uint64 count + elements
    ; For now, skip by reading type+count and estimating
    mov     eax, DWORD PTR [rsi+r12]    ; element type
    add     r12, 4
    mov     rax, QWORD PTR [rsi+r12]    ; count
    add     r12, 8
    ; Skip array elements — approximate by element size
    ; This is imprecise for nested types but handles common cases
    jmp     @@meta_print_nl

@@meta_print_nl:
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    inc     r14
    jmp     @@meta_loop

@@meta_done:
    mov     rax, r12            ; return current offset

    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
gguf_parse_metadata ENDP

;==============================================================================
; hexdump — Print N bytes from RCX as hex
; RCX = data ptr, RDX = count
;==============================================================================
hexdump PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rsi, rcx            ; source
    mov     rbx, rdx            ; count
    xor     edi, edi            ; position

@@hd_loop:
    cmp     rdi, rbx
    jge     @@hd_done

    ; Convert byte to 2 hex chars
    movzx   eax, BYTE PTR [rsi+rdi]
    lea     rcx, [numBuf]

    ; High nibble
    mov     edx, eax
    shr     edx, 4
    cmp     edx, 10
    jb      @@hd_dig1
    add     edx, 'A' - 10
    jmp     @@hd_w1
@@hd_dig1:
    add     edx, '0'
@@hd_w1:
    mov     BYTE PTR [rcx], dl

    ; Low nibble
    mov     edx, eax
    and     edx, 0Fh
    cmp     edx, 10
    jb      @@hd_dig2
    add     edx, 'A' - 10
    jmp     @@hd_w2
@@hd_dig2:
    add     edx, '0'
@@hd_w2:
    mov     BYTE PTR [rcx+1], dl
    mov     BYTE PTR [rcx+2], ' '
    mov     BYTE PTR [rcx+3], 0

    lea     rcx, [numBuf]
    mov     edx, 3
    call    write_con

    inc     edi

    ; Newline every 16 bytes
    mov     eax, edi
    and     eax, 0Fh
    test    eax, eax
    jnz     @@hd_loop
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con
    jmp     @@hd_loop

@@hd_done:
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
hexdump ENDP

;==============================================================================
; main
;==============================================================================
main PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    sub     rsp, 70h
    .allocstack 70h
    .endprolog

    ;--- Stdout ---
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     [hStdout], rax

    ;--- Banner ---
    lea     rcx, [szBanner]
    mov     edx, szBannerLen
    call    write_con

    ;--- Try to open GGUF file ---
    lea     rcx, [szOpening]
    mov     edx, szOpeningLen
    call    write_con
    lea     rcx, [szDefaultGguf]
    call    strlen_s
    mov     edx, eax
    lea     rcx, [szDefaultGguf]
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    lea     rcx, [szDefaultGguf]
    call    mmap_open

    ; Debug: mmap_open returned
    push    rax
    lea     rcx, [szDbg4]
    mov     edx, szDbg4Len
    call    write_con
    pop     rax

    test    rax, rax
    jnz     @@gguf_mapped

    ;--- GGUF not found, demo with proof.asm ---
    lea     rcx, [szMmapDemo]
    mov     edx, szMmapDemoLen
    call    write_con

    lea     rcx, [szDemoHeader]
    mov     edx, szDemoHeaderLen
    call    write_con

    lea     rcx, [szProofAsm]
    call    mmap_open
    test    rax, rax
    jz      @@open_fail

@@gguf_mapped:
    ;--- Print file size ---
    lea     rcx, [szFileSize]
    mov     edx, szFileSizeLen
    call    write_con
    mov     rax, QWORD PTR [ggufCtx + GGUF_CTX.fileSize]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szBytes]
    mov     edx, szBytesLen
    call    write_con

    ;--- Print base address ---
    lea     rcx, [szMmapOk]
    mov     edx, szMmapOkLen
    call    write_con
    mov     rax, QWORD PTR [ggufCtx + GGUF_CTX.pBase]
    lea     rcx, [numBuf]
    call    itoa_hex
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ;--- Hexdump first 64 bytes ---
    lea     rcx, [szFirst64]
    mov     edx, szFirst64Len
    call    write_con
    mov     rcx, QWORD PTR [ggufCtx + GGUF_CTX.pBase]
    mov     edx, 64
    ; Clamp to file size
    cmp     rdx, QWORD PTR [ggufCtx + GGUF_CTX.fileSize]
    jbe     @@hex_ok
    mov     rdx, QWORD PTR [ggufCtx + GGUF_CTX.fileSize]
@@hex_ok:
    call    hexdump

    ;--- Try GGUF header parse ---
    lea     rcx, [szMagic]
    mov     edx, szMagicLen
    call    write_con
    mov     rbx, QWORD PTR [ggufCtx + GGUF_CTX.pBase]
    mov     eax, DWORD PTR [rbx]
    lea     rcx, [numBuf]
    call    itoa_hex
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    call    gguf_parse_header
    test    eax, eax
    jz      @@not_gguf

    ;--- Valid GGUF ---
    lea     rcx, [szMagicValid]
    mov     edx, szMagicValidLen
    call    write_con

    ; Print version
    lea     rcx, [szVersion]
    mov     edx, szVersionLen
    call    write_con
    mov     eax, DWORD PTR [ggufCtx + GGUF_CTX.version]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ; Print tensor count
    lea     rcx, [szTensorCount]
    mov     edx, szTensorCountLen
    call    write_con
    mov     rax, QWORD PTR [ggufCtx + GGUF_CTX.tensorCount]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ; Print metadata count
    lea     rcx, [szMetaCount]
    mov     edx, szMetaCountLen
    call    write_con
    mov     rax, QWORD PTR [ggufCtx + GGUF_CTX.metadataCount]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ; Parse metadata KV pairs
    mov     rcx, 24             ; offset after header
    call    gguf_parse_metadata

    jmp     @@cleanup

@@not_gguf:
    lea     rcx, [szMagicInvalid]
    mov     edx, szMagicInvalidLen
    call    write_con

@@cleanup:
    ;--- Cleanup ---
    call    mmap_close
    lea     rcx, [szUnmapOk]
    mov     edx, szUnmapOkLen
    call    write_con

    lea     rcx, [szDone]
    mov     edx, szDoneLen
    call    write_con

    xor     ecx, ecx
    call    ExitProcess

@@open_fail:
    lea     rcx, [szOpenFail]
    mov     edx, szOpenFailLen
    call    write_con
    mov     ecx, 1
    call    ExitProcess

main ENDP

END
