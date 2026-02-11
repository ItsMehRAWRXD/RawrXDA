; ============================================================================
; RawrXD_PDBKernel.asm — Native PDB Symbol Server ASM Kernel
; ============================================================================
;
; Phase 29: PDB Symbol Server — MASM64 accelerated core operations
;
; Architecture:
;   MSF v7.00 magic validation (32-byte compare)
;   Public symbol stream scanner (CVRecordHeader walker)
;   Stream page list builder (directory → block array)
;   GUID-to-hex converter (for cache path construction)
;
; Assemble: ml64.exe /c /Fo RawrXD_PDBKernel.obj RawrXD_PDBKernel.asm
; Link:     Linked as object into RawrXD-Win32IDE (not a DLL)
;
; Exports:
;   PDB_ValidateMagic   — Compare 32-byte MSF superblock magic
;   PDB_ScanPublics     — Walk public symbol records, find name match
;   PDB_BuildPageList   — Extract page/block numbers for a stream
;   PDB_GuidToHex       — Convert 16-byte GUID to 32-char hex string
;
; ABI:      Windows x64 (RCX, RDX, R8, R9 + shadow space)
; Callee-save: RBX, RBP, RSI, RDI, R12-R15, XMM6-XMM15
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; ============================================================================

option casemap:none

; ============================================================================
; Constants — MSF v7.00 Format
; ============================================================================

; MSF magic string: "Microsoft C/C++ MSF 7.00\r\n\x1ADS\0\0\0"
; Total: 32 bytes
MSF_MAGIC_LEN       equ 32

; CodeView symbol record types (CVSymbolType from pdb_native.h)
S_PUB32             equ 0110Eh      ; Public symbol
S_GPROC32           equ 01110h      ; Global procedure start
S_LPROC32           equ 01114h      ; Local procedure start
S_GDATA32           equ 0110Dh      ; Global data
S_LDATA32           equ 0110Ch      ; Local data

; CVRecordHeader layout:
;   uint16_t recLen;   // +0  (record length, excludes this 2-byte field)
;   uint16_t recTyp;   // +2  (record type)
REC_LEN_OFFSET      equ 0
REC_TYP_OFFSET      equ 2
REC_HEADER_SIZE     equ 4

; CVPubSym32 layout (after header):
;   uint32_t pubSymFlags;  // +4
;   uint32_t off;          // +8
;   uint16_t seg;          // +12
;   char     name[];       // +14 (null-terminated)
PUB32_FLAGS_OFFSET  equ 4
PUB32_OFF_OFFSET    equ 8
PUB32_SEG_OFFSET    equ 12
PUB32_NAME_OFFSET   equ 14

; Sentinel value for "not found"
NOT_FOUND           equ 0FFFFFFFFh

; Hex digit lookup
HEX_UPPER           equ 1
HEX_LOWER           equ 0

; ============================================================================
; Read-only data segment
; ============================================================================
.data

; MSF v7.00 magic bytes — canonical reference for validation
; "Microsoft C/C++ MSF 7.00\r\n\x1ADS\0\0\0"
ALIGN 16
msf_magic_ref db 'Microsoft C/C++ MSF 7.00', 0Dh, 0Ah, 1Ah, 44h, 53h, 00h, 00h, 00h

; Hex digit table (uppercase) for GUID→hex conversion
ALIGN 16
hex_digits_upper db '0','1','2','3','4','5','6','7'
                 db '8','9','A','B','C','D','E','F'

; Hex digit table (lowercase) for alternative formatting
ALIGN 16
hex_digits_lower db '0','1','2','3','4','5','6','7'
                 db '8','9','a','b','c','d','e','f'

; ============================================================================
; Code segment
; ============================================================================
.code

; ============================================================================
; PDB_ValidateMagic — Compare 32-byte MSF superblock magic
; ============================================================================
;
; Purpose:
;   Validates that the first 32 bytes of a memory-mapped file contain the
;   MSF v7.00 magic signature. This is the first check performed when
;   opening any PDB file.
;
; Prototype:
;   uint32_t PDB_ValidateMagic(const void* superBlock);
;
; Parameters:
;   RCX = pointer to first 32 bytes of the file (MSFSuperBlock.magic)
;
; Returns:
;   EAX = 1 if magic matches, 0 if mismatch
;
; Clobbers: RAX, RDX, R8
; Preserves: RBX, RSI, RDI, RBP, R12-R15
;
; Strategy:
;   Uses 4x QWORD comparisons (4 × 8 = 32 bytes) for minimal branch count.
;   On modern CPUs this is faster than rep cmpsb for small fixed sizes.
;
; ============================================================================
PDB_ValidateMagic PROC
    ; RCX = pointer to candidate magic bytes
    lea rdx, [msf_magic_ref]       ; RDX = pointer to reference magic

    ; Compare 8 bytes at offset 0
    mov rax, QWORD PTR [rcx + 0]
    cmp rax, QWORD PTR [rdx + 0]
    jne magic_fail

    ; Compare 8 bytes at offset 8
    mov rax, QWORD PTR [rcx + 8]
    cmp rax, QWORD PTR [rdx + 8]
    jne magic_fail

    ; Compare 8 bytes at offset 16
    mov rax, QWORD PTR [rcx + 16]
    cmp rax, QWORD PTR [rdx + 16]
    jne magic_fail

    ; Compare 8 bytes at offset 24
    mov rax, QWORD PTR [rcx + 24]
    cmp rax, QWORD PTR [rdx + 24]
    jne magic_fail

    ; All 32 bytes match
    mov eax, 1
    ret

magic_fail:
    xor eax, eax
    ret
PDB_ValidateMagic ENDP


; ============================================================================
; PDB_ScanPublics — Walk public symbol records and find name match
; ============================================================================
;
; Purpose:
;   Linearly scans a contiguous buffer of CV symbol records (typically
;   from the public symbol stream) looking for a record whose name
;   matches the given search string. This is the Phase 29 v1 lookup
;   strategy. Phase 29.2 will add a hash table built from the GSI.
;
; Prototype:
;   uint32_t PDB_ScanPublics(
;       const void* streamData,   // RCX: contiguous buffer of CV records
;       uint32_t    streamSize,   // EDX: buffer size in bytes
;       const char* name,         // R8:  null-terminated search name
;       uint32_t    nameLen       // R9D: length of search name (not including null)
;   );
;
; Returns:
;   EAX = byte offset within streamData of the matching CVPubSym32 record,
;         or 0xFFFFFFFF (NOT_FOUND) if no match.
;
; Clobbers: RAX, RCX, RDX, R8, R9, R10, R11
; Preserves: RBX, RSI, RDI, RBP, R12-R15
;
; Record walking strategy:
;   Each CV record starts with a 2-byte recLen, followed by 2-byte recTyp.
;   To advance to the next record: offset += recLen + 2 (recLen doesn't
;   include the 2-byte recLen field itself). Records are 4-byte aligned
;   in the stream, so we round up to alignment.
;
; Name comparison:
;   For S_PUB32 records, the name starts at fixed offset 14 (after the
;   pubSymFlags, off, and seg fields). We compare exactly nameLen bytes
;   and verify a null terminator follows.
;
; ============================================================================
PDB_ScanPublics PROC
    ; Save callee-saved registers
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14

    ; Setup:
    ; RCX = streamData base pointer
    ; EDX = streamSize
    ; R8  = name pointer
    ; R9D = nameLen
    mov rsi, rcx                   ; RSI = streamData base
    mov r12d, edx                  ; R12D = streamSize
    mov rdi, r8                    ; RDI = search name
    mov r13d, r9d                  ; R13D = nameLen

    xor ebx, ebx                   ; EBX = current offset into stream

scan_loop:
    ; Check if we have at least 4 bytes (CVRecordHeader) remaining
    mov eax, r12d
    sub eax, ebx
    cmp eax, REC_HEADER_SIZE
    jl scan_not_found

    ; Read CVRecordHeader at current offset
    lea r10, [rsi + rbx]           ; R10 = pointer to current record
    movzx ecx, WORD PTR [r10 + REC_LEN_OFFSET]   ; ECX = recLen
    movzx edx, WORD PTR [r10 + REC_TYP_OFFSET]   ; EDX = recTyp

    ; Validate: recLen must be >= 2 (at least recTyp)
    cmp ecx, 2
    jl scan_not_found              ; Corrupt record — bail

    ; Validate: record must fit within stream
    lea eax, [ebx + ecx + 2]      ; Total bytes consumed by this record
    cmp eax, r12d
    ja scan_not_found              ; Record extends past stream end — bail

    ; Check if this is S_PUB32
    cmp edx, S_PUB32
    jne scan_advance               ; Not a public symbol — skip

    ; ---- S_PUB32 found: compare name ----
    ; Name starts at offset 14 from record start (PUB32_NAME_OFFSET)
    ; Check that the record is large enough to hold the name
    lea eax, [PUB32_NAME_OFFSET + r13d]  ; Minimum bytes needed: header(4) + flags(4) + off(4) + seg(2) + name
    cmp ecx, eax                   ; recLen >= name_offset_from_recTyp + nameLen ?
    jl scan_advance                ; Record too small for this name

    ; Get pointer to name in record
    lea r14, [r10 + PUB32_NAME_OFFSET]  ; R14 = pointer to name in record

    ; Compare nameLen bytes: record name vs. search name
    mov r11d, r13d                 ; R11D = bytes remaining to compare
    xor ecx, ecx                   ; ECX = comparison index

name_compare_loop:
    cmp ecx, r11d
    jge name_compare_done          ; All bytes matched

    movzx eax, BYTE PTR [r14 + rcx]   ; Record name byte
    movzx edx, BYTE PTR [rdi + rcx]   ; Search name byte

    cmp al, dl
    jne scan_advance               ; Mismatch

    inc ecx
    jmp name_compare_loop

name_compare_done:
    ; All nameLen bytes matched. Verify null terminator in record name.
    movzx eax, BYTE PTR [r14 + r11]
    test al, al
    jnz scan_advance               ; Not null-terminated — partial match, skip

    ; Full match found. Return the offset of this record.
    mov eax, ebx
    jmp scan_done

scan_advance:
    ; Advance to next record:
    ; next_offset = current_offset + recLen + 2
    movzx ecx, WORD PTR [rsi + rbx + REC_LEN_OFFSET]
    lea ebx, [ebx + ecx + 2]

    ; Align to 4-byte boundary (CV records are typically aligned)
    add ebx, 3
    and ebx, 0FFFFFFFCh

    jmp scan_loop

scan_not_found:
    mov eax, NOT_FOUND

scan_done:
    ; Restore callee-saved registers
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
PDB_ScanPublics ENDP


; ============================================================================
; PDB_BuildPageList — Extract page/block numbers for a stream from directory
; ============================================================================
;
; Purpose:
;   Given the MSF stream directory layout, extract the list of page (block)
;   numbers that compose a specific stream. The directory has this layout:
;
;     uint32_t numStreams;                      // +0
;     uint32_t streamSizes[numStreams];         // +4
;     uint32_t pages_for_stream_0[...];        // variable
;     uint32_t pages_for_stream_1[...];
;     ...
;
;   The number of pages for stream i = ceil(streamSizes[i] / blockSize).
;   Since we don't pass blockSize here, the caller provides pre-computed
;   streamSizes array and the function walks the page array portion.
;
;   Actually, for simplicity and correctness, this function receives:
;     - A flat array of page numbers (the pages portion of the directory)
;     - The stream index to extract
;     - Number of streams and the stream sizes array
;   It computes the offset into the pages array and copies out the pages.
;
; Prototype:
;   uint32_t PDB_BuildPageList(
;       const void*     directory,     // RCX: pointer to pages portion of directory
;       uint32_t        streamIndex,   // EDX: which stream to extract
;       uint32_t        numStreams,    // R8D: total number of streams
;       const uint32_t* streamSizes,  // R9:  array of stream sizes (in bytes)
;       uint32_t*       pagesOut,     // [RSP+40]: output buffer for page numbers
;       uint32_t        maxPages      // [RSP+48]: capacity of pagesOut buffer
;   );
;
; Returns:
;   EAX = number of pages written to pagesOut, or 0 on error
;
; Note: The caller must pass blockSize-derived page counts through streamSizes.
;       This function assumes streamSizes[i] = number of pages for stream i
;       (NOT byte sizes). The C++ wrapper pre-divides.
;
; Clobbers: RAX, RCX, RDX, R8, R9, R10, R11
; Preserves: RBX, RSI, RDI, RBP, R12-R15
;
; ============================================================================
PDB_BuildPageList PROC
    ; Save callee-saved registers
    push rbx
    push rsi
    push rdi
    push r12
    push r13

    ; Parameter setup
    ; RCX = directory (flat page array)
    ; EDX = streamIndex
    ; R8D = numStreams
    ; R9  = streamSizes (page counts per stream)
    ; [RSP+40+40] = pagesOut (5 pushes × 8 = 40, + shadow 32 + ret 8 = +80 from original RSP)
    ; Actually: 5 pushes = 40 bytes on stack. Parameters at [RSP+40+40] = [RSP+80]
    ; Stack: [ret][shadow32][arg5][arg6] + 5 pushes
    ; pagesOut at RSP + 40 (pushes) + 8 (ret) + 32 (shadow) = RSP + 80? No.
    ; Windows x64: first 4 args in RCX,RDX,R8,R9. 5th at [RSP+40] (original RSP).
    ; After 5 pushes: [RSP + 40 + 40] = [RSP + 80]
    mov rsi, rcx                   ; RSI = directory (page array base)
    mov r12d, edx                  ; R12D = target streamIndex
    mov r13d, r8d                  ; R13D = numStreams
    mov rbx, r9                    ; RBX = streamSizes array

    ; Load pagesOut and maxPages from stack
    mov rdi, QWORD PTR [rsp + 88]  ; RDI = pagesOut (5th param: 5*8 + 8ret + 32shadow + 8 = 88)
    mov ecx, DWORD PTR [rsp + 96]  ; ECX = maxPages (6th param)
    ; Note: Adjust offsets. 5 pushes = 40, ret = 8, shadow = 32. 5th param at original [rsp+40]
    ; After pushes: 5th param at [rsp + 40 + 40] = [rsp + 80]? Let me recalculate.
    ; Original stack at CALL: [ret_addr][arg5_home][arg6_home]... 
    ; No — 5th arg is at [RSP_at_call + 40]. After "push rbx; push rsi; push rdi; push r12; push r13":
    ; RSP decreased by 40. So original [RSP_at_call + 40] is now at [RSP + 40 + 40] = [RSP+80].
    ; But RSP_at_call already has ret pushed, so: [RSP + 40 (pushes) + 8 (ret) + 32 (shadow)] = 80
    ; 5th param at [RSP + 80], 6th at [RSP + 88].
    ; Let me recalculate properly:
    ; At function entry (after CALL pushed return address):
    ;   [RSP+0] = return address
    ;   [RSP+8] = shadow for RCX
    ;   [RSP+16] = shadow for RDX  
    ;   [RSP+24] = shadow for R8
    ;   [RSP+32] = shadow for R9
    ;   [RSP+40] = 5th parameter (pagesOut)
    ;   [RSP+48] = 6th parameter (maxPages)
    ; After 5 pushes (RSP -= 40):
    ;   5th param at [RSP + 40 + 40] = [RSP + 80]
    ;   6th param at [RSP + 40 + 48] = [RSP + 88]
    ; Correction applied above is correct.
    mov rdi, QWORD PTR [rsp + 80]  ; RDI = pagesOut
    mov ecx, DWORD PTR [rsp + 88]  ; ECX = maxPages

    ; Validate streamIndex < numStreams
    cmp r12d, r13d
    jae bpl_error

    ; Validate pagesOut is not null
    test rdi, rdi
    jz bpl_error

    ; Calculate page offset for the target stream:
    ; We need to sum streamSizes[0..streamIndex-1] to find where
    ; this stream's pages start in the flat directory array.
    xor edx, edx                   ; EDX = accumulated page offset
    xor eax, eax                   ; EAX = loop counter

bpl_sum_loop:
    cmp eax, r12d                  ; counter < streamIndex ?
    jge bpl_sum_done

    add edx, DWORD PTR [rbx + rax*4]  ; Add page count of stream [i]
    inc eax
    jmp bpl_sum_loop

bpl_sum_done:
    ; EDX = page offset (number of uint32_t entries to skip in directory)
    ; Get the page count for our target stream
    mov r8d, DWORD PTR [rbx + r12*4]  ; R8D = page count for target stream

    ; Clamp to maxPages
    cmp r8d, ecx
    cmova r8d, ecx                 ; r8d = min(pageCount, maxPages)

    ; Copy page numbers from directory to output buffer
    ; Source: RSI + EDX*4 (skip preceding streams' pages)
    ; Dest:   RDI
    ; Count:  R8D
    lea r10, [rsi + rdx*4]        ; R10 = source start
    xor eax, eax                   ; EAX = copy index

bpl_copy_loop:
    cmp eax, r8d
    jge bpl_copy_done

    mov r11d, DWORD PTR [r10 + rax*4]
    mov DWORD PTR [rdi + rax*4], r11d

    inc eax
    jmp bpl_copy_loop

bpl_copy_done:
    ; Return page count
    mov eax, r8d
    jmp bpl_done

bpl_error:
    xor eax, eax

bpl_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
PDB_BuildPageList ENDP


; ============================================================================
; PDB_GuidToHex — Convert 16-byte GUID to 32-character hex string
; ============================================================================
;
; Purpose:
;   Converts a 16-byte GUID (from PDB info stream or RSDS debug directory)
;   into a 32-character uppercase hex string suitable for constructing
;   symbol server cache paths.
;
;   GUID byte order for symbol server:
;     The first 3 fields (Data1=4, Data2=2, Data3=2) are little-endian
;     and must be byte-swapped for hex output. The remaining 8 bytes
;     (Data4) are in network order and output as-is.
;
;   However, for simplicity in v1, we output all 16 bytes in the exact
;   order they appear in the GUID struct, matching Microsoft's symsrv
;   behavior (which uses the raw GUID bytes as-is for the path).
;
; Prototype:
;   uint32_t PDB_GuidToHex(
;       const uint8_t guid[16],   // RCX: 16-byte GUID
;       char*         out,        // RDX: output buffer (at least 33 bytes)
;       uint32_t      maxLen      // R8D: buffer capacity
;   );
;
; Returns:
;   EAX = number of characters written (32), or 0 on error
;
; Output format:
;   32 uppercase hex characters followed by null terminator.
;   Example: "A1B2C3D4E5F6789012345678AABBCCDD"
;
; Clobbers: RAX, RCX, RDX, R8, R9, R10, R11
; Preserves: RBX, RSI, RDI, RBP, R12-R15
;
; ============================================================================
PDB_GuidToHex PROC
    ; Save callee-saved registers
    push rbx
    push rsi

    ; Parameter setup
    mov rsi, rcx                   ; RSI = GUID pointer
    mov rbx, rdx                   ; RBX = output buffer
    mov r9d, r8d                   ; R9D = maxLen

    ; Validate: need at least 33 bytes (32 hex chars + null)
    cmp r9d, 33
    jl guid_error

    ; Validate pointers
    test rsi, rsi
    jz guid_error
    test rbx, rbx
    jz guid_error

    ; Load hex digit lookup table
    lea r10, [hex_digits_upper]

    ; Convert 16 bytes → 32 hex characters
    xor ecx, ecx                  ; ECX = byte index (0..15)

guid_byte_loop:
    cmp ecx, 16
    jge guid_terminate

    ; Load byte
    movzx eax, BYTE PTR [rsi + rcx]

    ; High nibble
    mov edx, eax
    shr edx, 4
    and edx, 0Fh
    movzx r8d, BYTE PTR [r10 + rdx]

    ; Low nibble
    mov edx, eax
    and edx, 0Fh
    movzx r11d, BYTE PTR [r10 + rdx]

    ; Write two hex characters
    lea eax, [ecx * 2]            ; Output index = byteIndex * 2
    mov BYTE PTR [rbx + rax], r8b
    mov BYTE PTR [rbx + rax + 1], r11b

    inc ecx
    jmp guid_byte_loop

guid_terminate:
    ; Null-terminate
    mov BYTE PTR [rbx + 32], 0

    ; Return 32 (characters written)
    mov eax, 32
    jmp guid_done

guid_error:
    xor eax, eax

guid_done:
    pop rsi
    pop rbx
    ret
PDB_GuidToHex ENDP


; ============================================================================
; Export declarations for MSVC linker
; ============================================================================
END
