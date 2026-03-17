; piram_reverse_quantization.asm - Reverse Quantization Engine
; NEXT WEEK Task #2: Dequantize Q4/Q5/Q8 back to F16/F32
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC ReverseQuant_Init
PUBLIC ReverseQuant_Q4toF16
PUBLIC ReverseQuant_Q2toF16
PUBLIC ReverseQuant_Q5toF16
PUBLIC ReverseQuant_Q8toF16
PUBLIC ReverseQuant_Q4toF32
PUBLIC ReverseQuant_Q2toF32
PUBLIC ReverseQuant_Q5toF32
PUBLIC ReverseQuant_Q8toF32
PUBLIC ReverseQuant_F16Copy
PUBLIC ReverseQuant_F32Copy
PUBLIC ReverseQuant_Q4KtoF16
PUBLIC ReverseQuant_Q5KtoF16
PUBLIC ReverseQuant_Q8KtoF16
PUBLIC ReverseQuant_Q4KtoF32
PUBLIC ReverseQuant_Q8KtoF32
PUBLIC ReverseQuant_GetFormat
PUBLIC ReverseQuant_Batch
PUBLIC ReverseQuant_GetStats
PUBLIC ReverseQuant_ResetStats
PUBLIC Dequant4BitToF16
PUBLIC Dequant2BitToF16
PUBLIC Dequant5BitToF16
PUBLIC Dequant8BitToF16
PUBLIC Dequant4BitToF32
PUBLIC Dequant2BitToF32
PUBLIC Dequant5BitToF32
PUBLIC Dequant8BitToF32

; Quantization formats
QUANT_FMT_Q4_0      EQU 2
QUANT_FMT_Q4_1      EQU 3
QUANT_FMT_Q2_0      EQU 16
QUANT_FMT_Q5_0      EQU 6
QUANT_FMT_Q5_1      EQU 7
QUANT_FMT_Q8_0      EQU 8
QUANT_FMT_Q8_1      EQU 9
QUANT_FMT_Q2_K      EQU 17
QUANT_FMT_Q4_K      EQU 12
QUANT_FMT_Q5_K      EQU 13
QUANT_FMT_Q8_K      EQU 15
QUANT_FMT_F16       EQU 1
QUANT_FMT_F32       EQU 0

; Block sizes for quantization
BLOCK_SIZE_Q4       EQU 32
BLOCK_SIZE_Q2       EQU 32
BLOCK_SIZE_Q5       EQU 32
BLOCK_SIZE_Q8       EQU 32
BLOCK_SIZE_Q2K      EQU 256
BLOCK_SIZE_Q4K      EQU 256
BLOCK_SIZE_Q5K      EQU 256
BLOCK_SIZE_Q8K      EQU 256

; Quantization block structures
Q2Block STRUCT
    scale   dw ?        ; FP16 scale factor
    data    db 8 dup(?) ; 32 x 2-bit values packed
Q2Block ENDS

Q4Block STRUCT
    scale   dw ?        ; FP16 scale factor
    data    db 16 dup(?)  ; 32 x 4-bit values
Q4Block ENDS

Q5Block STRUCT
    scale   dw ?        ; FP16 scale factor
    min     dw ?        ; FP16 minimum value (Q5_1 only)
    data    db 20 dup(?)  ; 32 x 5-bit values
Q5Block ENDS

Q8Block STRUCT
    scale   dw ?        ; FP16 scale factor
    data    db 32 dup(?)  ; 32 x 8-bit values
Q8Block ENDS

; K-variant block structures (larger 256-value blocks)
Q2KBlock STRUCT
    d       db 64 dup(?)    ; 256 x 2-bit values (64 bytes)
    scales  db 32 dup(?)    ; 32 groups of scales (one per 8 values)
Q2KBlock ENDS

Q4KBlock STRUCT
    d       db 128 dup(?)  ; 256 x 4-bit values (128 bytes)
    scales  db 32 dup(?)   ; 32 groups of scales (one per 8 values)
    mins    db 32 dup(?)   ; 32 groups of min values (FP6)
Q4KBlock ENDS

Q8KBlock STRUCT
    d       db 256 dup(?)  ; 256 x 8-bit values
    scales  db 32 dup(?)   ; 32 group scales
Q8KBlock ENDS

.data
g_pLookupTable  dd 0
g_bInitialized  dd 0

; Dequantization statistics
g_Stats_Q2Blocks    dd 0
g_Stats_Q4Blocks    dd 0
g_Stats_Q5Blocks    dd 0
g_Stats_Q8Blocks    dd 0
g_Stats_TotalValues dd 0

; Quality metrics
g_Quality_TotalSamples  dd 0
g_Quality_SatHits       dd 0
g_Quality_CurrentFmt    dd 0
g_Quality_FmtSamples    dd 18 dup(0)
g_Quality_FmtSat        dd 18 dup(0)
g_Quality_FmtMin        dd 18 dup(07FFFFFFFh)
g_Quality_FmtMax        dd 18 dup(080000000h)

; Performance timing
g_Perf_StartTick    dd 0        ; Starting tick count
g_Perf_EndTick      dd 0        ; Ending tick count
g_Perf_ElapsedMs    dd 0        ; Elapsed milliseconds
g_Perf_BytesPerSec  dd 0        ; Throughput (bytes/sec)

; FP16 helper constants (approximations)
g_FP16_Bias         dw 15360  ; 0x3C00 - FP16 1.0
g_FP16_Max          dw 31743  ; 0x7BFF - FP16 max normal
g_FP16_Min          dw 256    ; 0x0100 - FP16 min normal
g_FP16_Denorm       dw 1      ; 0x0001 - FP16 min denormal

.code

; ============================================================
; ReverseQuant_Init - Initialize dequantization tables
; Output: EAX = 1 success, 0 failure
; ============================================================
ReverseQuant_Init PROC
    push ebx
    
    ; Check if already initialized
    cmp [g_bInitialized], 1
    je @already_init
    
    ; Allocate lookup table (4KB for fast conversion)
    invoke VirtualAlloc, 0, 4096, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    test eax, eax
    jz @fail
    
    mov [g_pLookupTable], eax
    
    ; Build lookup tables for common conversions
    call BuildQ4Lookup
    call BuildQ8Lookup
    
    ; Reset statistics
    call ReverseQuant_ResetStats
    
    mov [g_bInitialized], 1
    mov eax, 1
    pop ebx
    ret
    
@already_init:
    mov eax, 1
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop ebx
    ret
ReverseQuant_Init ENDP

; ============================================================
; ReverseQuant_Q4toF16 - Dequantize Q4 to FP16
; Input:  ECX = Q4 block pointer
;         EDX = F16 output buffer
;         ESI = number of blocks
; Output: EAX = number of values written
; ============================================================
ReverseQuant_Q4toF16 PROC pQ4:DWORD, pF16:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ4
    mov edi, pF16
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q4_0
    xor ebx, ebx            ; Total values counter
    
@block_loop:
    test ebp, ebp
    jz @done
    
    ; Load scale factor (FP16)
    movzx eax, word ptr [esi]
    mov edx, eax
    
    ; Process 32 4-bit values
    lea ecx, [esi + 2]      ; Point to data
    mov eax, 32             ; Values per block
    
@value_loop:
    test eax, eax
    jz @next_block
    
    ; Extract 4-bit value
    movzx ebx, byte ptr [ecx]
    
    ; Low nibble
    push eax
    mov eax, ebx
    and eax, 0Fh
    push ecx
    push edx
    call Dequant4BitToF16   ; Result in AX
    pop edx
    pop ecx
    mov [edi], ax
    add edi, 2
    inc ebx
    pop eax
    
    ; High nibble
    push eax
    mov eax, ebx
    shr eax, 4
    push ecx
    push edx
    call Dequant4BitToF16
    pop edx
    pop ecx
    mov [edi], ax
    add edi, 2
    inc ebx
    pop eax
    
    inc ecx
    sub eax, 2
    jmp @value_loop
    
@next_block:
    add esi, SIZEOF Q4Block
    dec ebp
    jmp @block_loop
    
@done:
    ; Update statistics
    mov eax, nBlocks
    add [g_Stats_Q4Blocks], eax
    mov eax, ebx
    add [g_Stats_TotalValues], eax
    
    mov eax, ebx
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q4toF16 ENDP

; ============================================================
; ReverseQuant_Q2toF16 - Dequantize Q2 to FP16
; Input:  pQ2:DWORD = Q2 block pointer
;         pF16:DWORD = F16 output buffer
;         nBlocks:DWORD = number of blocks
; Output: EAX = number of values written
; ============================================================
ReverseQuant_Q2toF16 PROC pQ2:DWORD, pF16:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ2
    mov edi, pF16
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q2_0
    xor eax, eax
    
@block_loop:
    test ebp, ebp
    jz @done
    
    movzx ebx, word ptr [esi]    ; scale in EBX (lower 16)
    xor ecx, ecx                 ; value index
    
@value_loop:
    cmp ecx, 32
    jge @block_done
    
    push ecx
    mov eax, ecx
    shr eax, 2                    ; byte offset
    mov dl, byte ptr [esi + 2 + eax]
    mov eax, ecx
    and eax, 3
    shl eax, 1                    ; bit offset = (idx%4)*2
    mov cl, al
    shr dl, cl
    and edx, 3                    ; qval in EDX
    mov eax, edx                  ; qval -> EAX
    mov edx, ebx                  ; scale -> EDX
    call Dequant2BitToF16
    pop ecx
    mov [edi], ax
    add edi, 2
    inc ecx
    jmp @value_loop
    
@block_done:
    add esi, SIZEOF Q2Block
    dec ebp
    jmp @block_loop
    
@done:
    ; update stats
    mov eax, nBlocks
    add [g_Stats_Q2Blocks], eax
    mov eax, nBlocks
    imul eax, 32
    add [g_Stats_TotalValues], eax
    mov eax, nBlocks
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q2toF16 ENDP

; ============================================================
; ReverseQuant_Q5toF16 - Dequantize Q5 to FP16
; Input:  pQ5:DWORD = Q5 block pointer
;         pF16:DWORD = F16 output buffer
;         nBlocks:DWORD = number of blocks
; Output: EAX = number of values written
; ============================================================
ReverseQuant_Q5toF16 PROC pQ5:DWORD, pF16:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ5
    mov edi, pF16
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q5_0
    xor ebx, ebx
    
@block_loop:
    test ebp, ebp
    jz @done
    
    ; Load scale factor (FP16)
    movzx edx, word ptr [esi]
    
    ; Process 32 5-bit values
    ; 5 bits per value * 32 values = 160 bits = 20 bytes
    lea ecx, [esi + 2]
    mov eax, 32             ; 32 values per block
    xor ebx, ebx            ; Bit counter
    
@value_loop:
    test eax, eax
    jz @next_block
    
    ; Extract 5-bit value from packed buffer
    ; bit_index = value_index * 5
    ; byte_offset = bit_index / 8
    ; bit_offset = bit_index % 8
    
    mov esi, ebx            ; Current value index
    mov edi, ebx
    imul edi, 5             ; edi = bit position
    
    mov eax, edi
    shr eax, 3              ; byte offset
    mov ecx, edi
    and ecx, 7              ; bit offset in byte
    
    ; Load up to 2 bytes
    movzx ebx, byte ptr [ecx + eax]
    shr ebx, cl             ; Shift to align
    and ebx, 31             ; Mask 5 bits
    
    ; Call dequantize function
    push eax
    push ecx
    push esi
    mov eax, ebx
    push edx
    call Dequant5BitToF16
    pop edx
    pop esi
    pop ecx
    pop eax
    
    mov [edi], ax
    add edi, 2
    
    inc esi
    dec eax
    jmp @value_loop
    
@next_block:
    add esi, SIZEOF Q5Block
    dec ebp
    jmp @block_loop
    
@done:
    mov eax, nBlocks
    add [g_Stats_Q5Blocks], eax
    
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q5toF16 ENDP
; Input:  ECX = Q8 block pointer
;         EDX = F16 output buffer
;         ESI = number of blocks
; Output: EAX = number of values written
; ============================================================
ReverseQuant_Q8toF16 PROC pQ8:DWORD, pF16:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ8
    mov edi, pF16
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q8_0
    xor ebx, ebx
    
@block_loop:
    test ebp, ebp
    jz @done
    
    ; Load scale factor
    movzx edx, word ptr [esi]
    
    ; Process 32 8-bit values
    lea ecx, [esi + 2]
    mov eax, 32
    
@value_loop:
    test eax, eax
    jz @next_block
    
    ; Load 8-bit quantized value
    movsx ebx, byte ptr [ecx]
    
    ; Dequantize: value = q8 * scale
    push eax
    push ecx
    mov eax, ebx
    push edx
    call Dequant8BitToF16
    pop edx
    pop ecx
    mov [edi], ax
    add edi, 2
    pop eax
    
    inc ecx
    dec eax
    jmp @value_loop
    
@next_block:
    add esi, SIZEOF Q8Block
    dec ebp
    jmp @block_loop
    
@done:
    mov eax, nBlocks
    add [g_Stats_Q8Blocks], eax
    
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q8toF16 ENDP

; ============================================================
; ReverseQuant_Q4toF32 - Dequantize Q4 to FP32
; Input:  ECX = Q4 block pointer
;         EDX = F32 output buffer
;         ESI = number of blocks
; Output: EAX = number of values written
; ============================================================
ReverseQuant_Q4toF32 PROC pQ4:DWORD, pF32:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ4
    mov edi, pF32
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q4_0
    xor ebx, ebx
    
@block_loop:
    test ebp, ebp
    jz @done
    
    movzx edx, word ptr [esi]
    lea ecx, [esi + 2]
    mov eax, 32
    
@value_loop:
    test eax, eax
    jz @next_block
    
    movzx ebx, byte ptr [ecx]
    
    ; Low nibble to F32
    push eax
    mov eax, ebx
    and eax, 0Fh
    push ecx
    push edx
    call Dequant4BitToF32
    pop edx
    pop ecx
    mov [edi], eax
    add edi, 4
    pop eax
    
    ; High nibble to F32
    push eax
    mov eax, ebx
    shr eax, 4
    push ecx
    push edx
    call Dequant4BitToF32
    pop edx
    pop ecx
    mov [edi], eax
    add edi, 4
    pop eax
    
    inc ecx
    sub eax, 2
    jmp @value_loop
    
@next_block:
    add esi, SIZEOF Q4Block
    dec ebp
    jmp @block_loop
    
@done:
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q4toF32 ENDP

; ============================================================
; ReverseQuant_Q2toF32 - Dequantize Q2 to FP32
; ============================================================
ReverseQuant_Q2toF32 PROC pQ2:DWORD, pF32:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ2
    mov edi, pF32
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q2_0
    
@block_loop:
    test ebp, ebp
    jz @done
    
    movzx ebx, word ptr [esi]    ; scale
    xor ecx, ecx
    
@value_loop:
    cmp ecx, 32
    jge @next_block
    
    push ecx
    mov eax, ecx
    shr eax, 2
    mov dl, byte ptr [esi + 2 + eax]
    mov eax, ecx
    and eax, 3
    shl eax, 1
    mov cl, al
    shr dl, cl
    and edx, 3
    mov eax, edx
    mov edx, ebx
    call Dequant2BitToF32
    pop ecx
    mov [edi], eax
    add edi, 4
    inc ecx
    jmp @value_loop
    
@next_block:
    add esi, SIZEOF Q2Block
    dec ebp
    jmp @block_loop
    
@done:
    mov eax, nBlocks
    add [g_Stats_Q2Blocks], eax
    mov eax, nBlocks
    imul eax, 32
    add [g_Stats_TotalValues], eax
    mov eax, nBlocks
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q2toF32 ENDP

; ============================================================
; ReverseQuant_Q5toF32 - Dequantize Q5 to FP32
; Input:  pQ5:DWORD = Q5 block pointer
;         pF32:DWORD = F32 output buffer
;         nBlocks:DWORD = number of blocks
; Output: EAX = number of values written
; ============================================================
ReverseQuant_Q5toF32 PROC pQ5:DWORD, pF32:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ5
    mov edi, pF32
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q5_0
    xor ebx, ebx
    
@block_loop:
    test ebp, ebp
    jz @done
    
    ; Load scale factor (FP16)
    movzx edx, word ptr [esi]
    
    ; Process 32 5-bit values
    lea ecx, [esi + 2]
    mov eax, 32
    xor ebx, ebx            ; Value counter
    
@value_loop:
    test eax, eax
    jz @next_block
    
    ; Extract 5-bit value from packed buffer
    mov esi, ebx
    imul esi, 5             ; bit position = value_index * 5
    
    mov ecx, esi
    shr ecx, 3              ; byte offset
    mov esi, esi
    and esi, 7              ; bit offset
    
    ; Load byte and extract 5 bits
    movzx ebx, byte ptr [ecx + ecx]
    shr ebx, cl
    and ebx, 31             ; Mask 5 bits
    
    ; Call dequantize function
    push eax
    push ecx
    mov eax, ebx
    push edx
    call Dequant5BitToF32
    pop edx
    pop ecx
    mov [edi], eax
    add edi, 4
    pop eax
    
    inc ebx
    dec eax
    jmp @value_loop
    
@next_block:
    add esi, SIZEOF Q5Block
    dec ebp
    jmp @block_loop
    
@done:
    mov eax, nBlocks
    add [g_Stats_Q5Blocks], eax
    
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q5toF32 ENDP

; ============================================================
; ReverseQuant_Q8toF32 - Dequantize Q8 to FP32
; Input:  ECX = Q8 block pointer
;         EDX = F32 output buffer
;         ESI = number of blocks
; Output: EAX = number of values written
; ============================================================
ReverseQuant_Q8toF32 PROC pQ8:DWORD, pF32:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ8
    mov edi, pF32
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q8_0
    
@block_loop:
    test ebp, ebp
    jz @done
    
    movzx edx, word ptr [esi]
    lea ecx, [esi + 2]
    mov eax, 32
    
@value_loop:
    test eax, eax
    jz @next_block
    
    movsx ebx, byte ptr [ecx]
    
    push eax
    push ecx
    mov eax, ebx
    push edx
    call Dequant8BitToF32
    pop edx
    pop ecx
    mov [edi], eax
    add edi, 4
    pop eax
    
    inc ecx
    dec eax
    jmp @value_loop
    
@next_block:
    add esi, SIZEOF Q8Block
    dec ebp
    jmp @block_loop
    
@done:
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q8toF32 ENDP

; ============================================================
; ReverseQuant_F16Copy - Pass-through FP16 blocks
; Input:  pSrc:DWORD = F16 input buffer
;         pDst:DWORD = F16 output buffer
;         nElements:DWORD = number of values
; Output: EAX = number of values written
; ============================================================
ReverseQuant_F16Copy PROC pSrc:DWORD, pDst:DWORD, nElements:DWORD
    push esi
    push edi
    push ecx
    
    mov esi, pSrc
    mov edi, pDst
    mov ecx, nElements
    mov eax, ecx
    rep movsw
    
    ; stats: treat as Q2 blocks? none; just total values
    add [g_Stats_TotalValues], eax
    
    pop ecx
    pop edi
    pop esi
    ret
ReverseQuant_F16Copy ENDP

; ============================================================
; ReverseQuant_F32Copy - Pass-through FP32 blocks
; ============================================================
ReverseQuant_F32Copy PROC pSrc:DWORD, pDst:DWORD, nElements:DWORD
    push esi
    push edi
    push ecx
    
    mov esi, pSrc
    mov edi, pDst
    mov ecx, nElements
    mov eax, ecx
    rep movsd
    
    add [g_Stats_TotalValues], eax
    
    pop ecx
    pop edi
    pop esi
    ret
ReverseQuant_F32Copy ENDP

; ============================================================
; ReverseQuant_Q4KtoF16 - Dequantize Q4_K to FP16
; Input:  pQ4K:DWORD = Q4_K block pointer (256 values)
;         pF16:DWORD = F16 output buffer
;         nBlocks:DWORD = number of blocks
; Output: EAX = number of values written
; ============================================================
ReverseQuant_Q4KtoF16 PROC pQ4K:DWORD, pF16:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ4K
    mov edi, pF16
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q4_K
    xor ebx, ebx
    
@block_loop:
    test ebp, ebp
    jz @done
    
    ; Q4_K: 256 values per block, 32 groups of 8 values
    ; Each group has its own scale (FP6 in 1 byte) and min (FP6)
    lea ecx, [esi + 32 + 32]    ; Point to start of data
    mov eax, 32                  ; 32 groups
    xor ebx, ebx                 ; Value counter
    
@group_loop:
    test eax, eax
    jz @next_block
    
    ; Load group scale and min (simplified - treat as byte values)
    mov edx, esi
    add edx, 32                  ; Point to scales
    add edx, ebx
    movzx edx, byte ptr [edx]   ; Get scale for this group
    
    ; Process 8 values in this group
    push eax
    mov eax, 8
    
@value_loop2:
    test eax, eax
    jz @group_done
    
    ; Extract 4-bit values
    movzx ebx, byte ptr [ecx]
    
    ; Low nibble
    push eax
    mov eax, ebx
    and eax, 0Fh
    push ecx
    push edx
    call Dequant4BitToF16
    pop edx
    pop ecx
    mov [edi], ax
    add edi, 2
    pop eax
    
    ; High nibble
    push eax
    mov eax, ebx
    shr eax, 4
    push ecx
    push edx
    call Dequant4BitToF16
    pop edx
    pop ecx
    mov [edi], ax
    add edi, 2
    pop eax
    
    inc ecx
    dec eax
    jmp @value_loop2
    
@group_done:
    pop eax
    inc ebx
    dec eax
    jmp @group_loop
    
@next_block:
    add esi, SIZEOF Q4KBlock
    dec ebp
    jmp @block_loop
    
@done:
    mov eax, nBlocks
    add [g_Stats_Q4Blocks], eax
    
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q4KtoF16 ENDP

; ============================================================
; ReverseQuant_Q5KtoF16 - Dequantize Q5_K to FP16
; Input:  pQ5K:DWORD = Q5_K block pointer
;         pF16:DWORD = F16 output buffer
;         nBlocks:DWORD = number of blocks
; Output: EAX = number of values written
; ============================================================
ReverseQuant_Q5KtoF16 PROC pQ5K:DWORD, pF16:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ5K
    mov edi, pF16
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q5_K
    xor ebx, ebx
    
@block_loop:
    test ebp, ebp
    jz @done
    
    ; Q5_K: 256 values, 32 groups with per-group scales
    lea ecx, [esi + 32 + 32]
    mov eax, 32
    xor ebx, ebx
    
@group_loop:
    test eax, eax
    jz @next_block
    
    ; Get scale for this group
    mov edx, esi
    add edx, 32
    add edx, ebx
    movzx edx, byte ptr [edx]
    
    mov ecx, 8              ; 8 values per group
    
@value_loop:
    test ecx, ecx
    jz @group_done
    
    ; Extract 5-bit value (simplified)
    movzx ebx, byte ptr [ecx]
    and ebx, 31
    
    push ecx
    mov eax, ebx
    push edx
    call Dequant5BitToF16
    pop edx
    pop ecx
    mov [edi], ax
    add edi, 2
    
    inc ecx
    dec ecx
    jmp @value_loop
    
@group_done:
    inc ebx
    dec eax
    jmp @group_loop
    
@next_block:
    add esi, 320            ; Approximate Q5K block size
    dec ebp
    jmp @block_loop
    
@done:
    mov eax, nBlocks
    add [g_Stats_Q5Blocks], eax
    
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q5KtoF16 ENDP

; ============================================================
; ReverseQuant_Q8KtoF16 - Dequantize Q8_K to FP16
; Input:  pQ8K:DWORD = Q8_K block pointer
;         pF16:DWORD = F16 output buffer
;         nBlocks:DWORD = number of blocks
; Output: EAX = number of values written
; ============================================================
ReverseQuant_Q8KtoF16 PROC pQ8K:DWORD, pF16:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ8K
    mov edi, pF16
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q8_K
    xor ebx, ebx
    
@block_loop:
    test ebp, ebp
    jz @done
    
    ; Q8_K: 256 x 8-bit values with 32 group scales
    lea ecx, [esi + 32]         ; Point to data
    mov eax, 32                 ; 32 groups
    xor ebx, ebx
    
@group_loop:
    test eax, eax
    jz @next_block
    
    ; Get scale for this group
    mov edx, esi
    add edx, ebx
    movzx edx, byte ptr [edx]
    
    ; Process 8 values
    push eax
    mov eax, 8
    
@value_loop:
    test eax, eax
    jz @group_done
    
    movsx ebx, byte ptr [ecx]
    
    push eax
    push ecx
    mov eax, ebx
    push edx
    call Dequant8BitToF16
    pop edx
    pop ecx
    mov [edi], ax
    add edi, 2
    pop eax
    
    inc ecx
    dec eax
    jmp @value_loop
    
@group_done:
    pop eax
    inc ebx
    dec eax
    jmp @group_loop
    
@next_block:
    add esi, SIZEOF Q8KBlock
    dec ebp
    jmp @block_loop
    
@done:
    mov eax, nBlocks
    add [g_Stats_Q8Blocks], eax
    
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q8KtoF16 ENDP

; ============================================================
; ReverseQuant_Q2KtoF16 - Dequantize Q2_K to FP16 (256 values)
; ============================================================
ReverseQuant_Q2KtoF16 PROC pQ2K:DWORD, pF16:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ2K              ; block pointer
    mov edi, pF16              ; output pointer
    mov ebx, nBlocks           ; block counter
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q2_K
    
@block_loop:
    test ebx, ebx
    jz @done
    
    xor ecx, ecx               ; group index
@group_loop:
    cmp ecx, 32
    jge @next_block
    
    movzx edx, byte ptr [esi + 64 + ecx] ; group scale in EDX
    xor eax, eax               ; inner value index 0..7

@inner:
    cmp eax, 8
    jge @group_done
    
    ; global value index = ecx*8 + eax
    mov ebp, ecx
    imul ebp, 8
    add ebp, eax               ; EBp = global index 0..255

    push eax                   ; save inner counter
    push ecx                   ; save group index
    push edx                   ; save scale

    mov edx, ebp
    shr edx, 2                 ; byte offset
    mov al, byte ptr [esi + edx]
    mov edx, ebp
    and edx, 3
    shl edx, 1                 ; bit offset
    mov cl, dl
    shr al, cl
    and eax, 3                 ; qval in EAX

    pop edx                    ; restore scale
    call Dequant2BitToF16
    pop ecx                    ; restore group index
    pop eax                    ; restore inner counter

    mov [edi], ax
    add edi, 2
    inc eax
    jmp @inner

@group_done:
    inc ecx
    jmp @group_loop
    
@next_block:
    add esi, SIZEOF Q2KBlock
    dec ebx
    jmp @block_loop
    
@done:
    mov eax, nBlocks
    add [g_Stats_Q2Blocks], eax
    mov eax, nBlocks
    imul eax, 256
    add [g_Stats_TotalValues], eax
    mov eax, nBlocks
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q2KtoF16 ENDP

; ============================================================
; ReverseQuant_Q4KtoF32 - Dequantize Q4_K to FP32
; (Similar to Q4KtoF16, outputs F32)
; ============================================================
ReverseQuant_Q4KtoF32 PROC pQ4K:DWORD, pF32:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ4K
    mov edi, pF32
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q4_K
    xor ebx, ebx
    
@block_loop:
    test ebp, ebp
    jz @done
    
    lea ecx, [esi + 32 + 32]
    mov eax, 32
    xor ebx, ebx
    
@group_loop:
    test eax, eax
    jz @next_block
    
    mov edx, esi
    add edx, 32
    add edx, ebx
    movzx edx, byte ptr [edx]
    
    push eax
    mov eax, 8
    
@value_loop:
    test eax, eax
    jz @group_done
    
    movzx ebx, byte ptr [ecx]
    
    push eax
    mov eax, ebx
    and eax, 0Fh
    push ecx
    push edx
    call Dequant4BitToF32
    pop edx
    pop ecx
    mov [edi], eax
    add edi, 4
    pop eax
    
    push eax
    mov eax, ebx
    shr eax, 4
    push ecx
    push edx
    call Dequant4BitToF32
    pop edx
    pop ecx
    mov [edi], eax
    add edi, 4
    pop eax
    
    inc ecx
    dec eax
    jmp @value_loop
    
@group_done:
    pop eax
    inc ebx
    dec eax
    jmp @group_loop
    
@next_block:
    add esi, SIZEOF Q4KBlock
    dec ebp
    jmp @block_loop
    
@done:
    mov eax, nBlocks
    add [g_Stats_Q4Blocks], eax
    
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q4KtoF32 ENDP

; ============================================================
; ReverseQuant_Q8KtoF32 - Dequantize Q8_K to FP32
; ============================================================
ReverseQuant_Q8KtoF32 PROC pQ8K:DWORD, pF32:DWORD, nBlocks:DWORD
    push ebx
    push esi
    push edi
    push ebp
    
    mov esi, pQ8K
    mov edi, pF32
    mov ebp, nBlocks
    mov dword ptr [g_Quality_CurrentFmt], QUANT_FMT_Q8_K
    xor ebx, ebx
    
@block_loop:
    test ebp, ebp
    jz @done
    
    lea ecx, [esi + 32]
    mov eax, 32
    xor ebx, ebx
    
@group_loop:
    test eax, eax
    jz @next_block
    
    mov edx, esi
    add edx, ebx
    movzx edx, byte ptr [edx]
    
    push eax
    mov eax, 8
    
@value_loop:
    test eax, eax
    jz @group_done
    
    movsx ebx, byte ptr [ecx]
    
    push eax
    push ecx
    mov eax, ebx
    push edx
    call Dequant8BitToF32
    pop edx
    pop ecx
    mov [edi], eax
    add edi, 4
    pop eax
    
    inc ecx
    dec eax
    jmp @value_loop
    
@group_done:
    pop eax
    inc ebx
    dec eax
    jmp @group_loop
    
@next_block:
    add esi, SIZEOF Q8KBlock
    dec ebp
    jmp @block_loop
    
@done:
    mov eax, nBlocks
    add [g_Stats_Q8Blocks], eax
    
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
ReverseQuant_Q8KtoF32 ENDP

; ============================================================
; ReverseQuant_GetFormat - Detect quantization format
; Input:  pData:DWORD = tensor data pointer (with GGUF header)
; Output: EAX = format ID (QUANT_FMT_*)
; ============================================================
ReverseQuant_GetFormat PROC pData:DWORD
    push ebx
    push ecx
    
    mov eax, pData
    test eax, eax
    jz @default
    
    ; Check for GGUF magic header
    mov ecx, [eax]
    cmp ecx, 'GGUF'         ; Check for GGUF signature
    jne @check_format_byte
    
    ; GGUF format detected - check metadata
    ; For now, assume Q4_0 as most common format
    ; In production, would parse GGUF metadata header
    mov eax, QUANT_FMT_Q4_0
    jmp @done
    
@check_format_byte:
    ; Check for format marker byte at offset 0
    movzx eax, byte ptr [eax]
    
    ; Map format ID to our constants
    cmp al, QUANT_FMT_Q2_0
    je @q2_0
    cmp al, QUANT_FMT_Q2_K
    je @q2_k
    cmp al, QUANT_FMT_F32
    je @f32
    cmp al, QUANT_FMT_F16
    je @f16
    cmp al, 2
    je @q4_0
    cmp al, 3
    je @q4_1
    cmp al, 6
    je @q5_0
    cmp al, 7
    je @q5_1
    cmp al, 8
    je @q8_0
    cmp al, 9
    je @q8_1
    cmp al, 12
    je @q4_k
    cmp al, 13
    je @q5_k
    cmp al, 15
    je @q8_k
    
@default:
    mov eax, QUANT_FMT_Q4_0
    jmp @done
    
@q4_0:
    mov eax, QUANT_FMT_Q4_0
    jmp @done
@q4_1:
    mov eax, QUANT_FMT_Q4_1
    jmp @done
@q2_0:
    mov eax, QUANT_FMT_Q2_0
    jmp @done
@q2_k:
    mov eax, QUANT_FMT_Q2_K
    jmp @done
@q5_0:
    mov eax, QUANT_FMT_Q5_0
    jmp @done
@q5_1:
    mov eax, QUANT_FMT_Q5_1
    jmp @done
@q8_0:
    mov eax, QUANT_FMT_Q8_0
    jmp @done
@q8_1:
    mov eax, QUANT_FMT_Q8_1
    jmp @done
@q4_k:
    mov eax, QUANT_FMT_Q4_K
    jmp @done
@q5_k:
    mov eax, QUANT_FMT_Q5_K
    jmp @done
@q8_k:
    mov eax, QUANT_FMT_Q8_K
    jmp @done
@f32:
    mov eax, QUANT_FMT_F32
    jmp @done
@f16:
    mov eax, QUANT_FMT_F16
    jmp @done
    
@done:
    pop ecx
    pop ebx
    ret
ReverseQuant_GetFormat ENDP

; ============================================================
; ReverseQuant_Batch - Batch dequantization dispatcher
; Input:  ECX = source format
;         EDX = source buffer
;         ESI = destination buffer
;         EDI = number of elements
; Output: EAX = 1 success
; ============================================================
ReverseQuant_Batch PROC srcFmt:DWORD, pSrc:DWORD, pDst:DWORD, nElements:DWORD
    push ebx
    push esi
    
    mov ebx, srcFmt
    
    ; Calculate number of blocks (32 values per block)
    mov eax, nElements
    mov ecx, 32
    xor edx, edx
    div ecx
    ; EAX = number of blocks
    mov esi, eax
    
    ; Dispatch based on source format
    cmp ebx, QUANT_FMT_F32
    je @f32_copy
    cmp ebx, QUANT_FMT_F16
    je @f16_copy
    cmp ebx, QUANT_FMT_Q2_0
    je @q2_0_f16
    cmp ebx, QUANT_FMT_Q2_K
    je @q2_k_f16
    cmp ebx, QUANT_FMT_Q4_0
    je @q4_0_f16
    cmp ebx, QUANT_FMT_Q4_1
    je @q4_1_f16
    cmp ebx, QUANT_FMT_Q4_K
    je @q4_k_f16
    cmp ebx, QUANT_FMT_Q5_0
    je @q5_0_f16
    cmp ebx, QUANT_FMT_Q5_1
    je @q5_1_f16
    cmp ebx, QUANT_FMT_Q5_K
    je @q5_k_f16
    cmp ebx, QUANT_FMT_Q8_0
    je @q8_0_f16
    cmp ebx, QUANT_FMT_Q8_1
    je @q8_1_f16
    cmp ebx, QUANT_FMT_Q8_K
    je @q8_k_f16
    jmp @unsupported
    
@q4_0_f16:
    push esi
    push pDst
    push pSrc
    call ReverseQuant_Q4toF16
    jmp @done
    
@q4_1_f16:
    ; Q4_1 same structure as Q4_0 for dequantization
    push esi
    push pDst
    push pSrc
    call ReverseQuant_Q4toF16
    jmp @done
    
@q4_k_f16:
    ; Q4_K: 256 values per block (calculate blocks differently)
    mov eax, nElements
    mov ecx, 256
    xor edx, edx
    div ecx
    mov esi, eax
    push esi
    push pDst
    push pSrc
    call ReverseQuant_Q4KtoF16
    jmp @done
    
@q5_0_f16:
    push esi
    push pDst
    push pSrc
    call ReverseQuant_Q5toF16
    jmp @done
    
@q5_1_f16:
    ; Q5_1 same as Q5_0 for dequantization
    push esi
    push pDst
    push pSrc
    call ReverseQuant_Q5toF16
    jmp @done
    
@q5_k_f16:
    ; Q5_K: 256 values per block
    mov eax, nElements
    mov ecx, 256
    xor edx, edx
    div ecx
    mov esi, eax
    push esi
    push pDst
    push pSrc
    call ReverseQuant_Q5KtoF16
    jmp @done
    
@q8_0_f16:
    push esi
    push pDst
    push pSrc
    call ReverseQuant_Q8toF16
    jmp @done
    
@q8_1_f16:
    ; Q8_1 same as Q8_0 for dequantization
    push esi
    push pDst
    push pSrc
    call ReverseQuant_Q8toF16
    jmp @done
    
@q8_k_f16:
    ; Q8_K: 256 values per block
    mov eax, nElements
    mov ecx, 256
    xor edx, edx
    div ecx
    mov esi, eax
    push esi
    push pDst
    push pSrc
    call ReverseQuant_Q8KtoF16
    jmp @done

@f32_copy:
    push nElements
    push pDst
    push pSrc
    call ReverseQuant_F32Copy
    jmp @done

@f16_copy:
    push nElements
    push pDst
    push pSrc
    call ReverseQuant_F16Copy
    jmp @done

@q2_0_f16:
    push esi
    push pDst
    push pSrc
    call ReverseQuant_Q2toF16
    jmp @done

@q2_k_f16:
    mov eax, nElements
    mov ecx, 256
    xor edx, edx
    div ecx
    mov esi, eax
    push esi
    push pDst
    push pSrc
    call ReverseQuant_Q2KtoF16
    jmp @done
    
@done:
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@unsupported:
    xor eax, eax
    pop esi
    pop ebx
    ret
ReverseQuant_Batch ENDP

; ============================================================
; ReverseQuant_GetStats - Retrieve dequantization statistics
; Output: EAX = total values processed
;         EDX = total blocks processed
;         ECX = statistics structure pointer
; ============================================================
ReverseQuant_GetStats PROC
    mov eax, [g_Stats_TotalValues]
    mov edx, [g_Stats_Q2Blocks]
    add edx, [g_Stats_Q4Blocks]
    add edx, [g_Stats_Q5Blocks]
    add edx, [g_Stats_Q8Blocks]
    mov ecx, offset g_Stats_TotalValues
    ret
ReverseQuant_GetStats ENDP

; ============================================================
; ReverseQuant_ResetStats - Reset statistics counters
; ============================================================
ReverseQuant_ResetStats PROC
    xor eax, eax
    mov [g_Stats_Q2Blocks], eax
    mov [g_Stats_Q4Blocks], eax
    mov [g_Stats_Q5Blocks], eax
    mov [g_Stats_Q8Blocks], eax
    mov [g_Stats_TotalValues], eax
    mov [g_Perf_ElapsedMs], eax
    mov [g_Perf_BytesPerSec], eax
    ret
ReverseQuant_ResetStats ENDP

; ============================================================
; ReverseQuant_StartTiming - Start performance timer
; ============================================================
ReverseQuant_StartTiming PROC
    invoke GetTickCount       ; Returns milliseconds in EAX
    mov [g_Perf_StartTick], eax
    ret
ReverseQuant_StartTiming ENDP

; ============================================================
; ReverseQuant_StopTiming - Stop timer and calculate throughput
; Input:  EDX = number of bytes processed
; Output: EAX = throughput in MB/sec
; ============================================================
ReverseQuant_StopTiming PROC cbBytes:DWORD
    invoke GetTickCount       ; Current time in EAX
    mov ecx, eax
    sub ecx, [g_Perf_StartTick]   ; ECX = elapsed milliseconds
    mov [g_Perf_ElapsedMs], ecx
    
    ; Calculate throughput: (cbBytes * 1000) / (elapsed_ms * 1024 * 1024)
    ; Simplified: (cbBytes * 1000) / elapsed_ms / (1024*1024)
    
    test ecx, ecx
    jz @zero_time
    
    mov eax, cbBytes
    mov edx, 1000
    mul edx                     ; EAX = cbBytes * 1000
    
    div ecx                     ; EAX = (cbBytes * 1000) / elapsed_ms
    
    mov ecx, 1048576           ; 1MB = 1024*1024
    div ecx                     ; EAX = MB/sec
    
    mov [g_Perf_BytesPerSec], eax
    ret
    
@zero_time:
    ; Avoid division by zero
    mov eax, 0xFFFFFFFF        ; Max value
    mov [g_Perf_BytesPerSec], eax
    ret
ReverseQuant_StopTiming ENDP

; ============================================================
; ReverseQuant_GetThroughput - Get last measured throughput
; Output: EAX = MB/sec
; ============================================================
ReverseQuant_GetThroughput PROC
    mov eax, [g_Perf_BytesPerSec]
    ret
ReverseQuant_GetThroughput ENDP

; ============================================================
; ReverseQuant_GetQualityMetrics - return saturation hits and samples
; Output: EAX = total samples, EDX = saturation hits, ECX = pointer to struct
; ============================================================
ReverseQuant_GetQualityMetrics PROC
    mov eax, [g_Quality_TotalSamples]
    mov edx, [g_Quality_SatHits]
    mov ecx, offset g_Quality_TotalSamples
    ret
ReverseQuant_GetQualityMetrics ENDP

ReverseQuant_ResetQualityMetrics PROC
    xor eax, eax
    mov [g_Quality_TotalSamples], eax
    mov [g_Quality_SatHits], eax
    mov [g_Quality_CurrentFmt], eax
    mov ecx, 18
    lea edi, g_Quality_FmtSamples
@clr_samples:
    mov [edi], eax
    add edi, 4
    loop @clr_samples

    mov ecx, 18
    lea edi, g_Quality_FmtSat
@clr_sat:
    mov [edi], eax
    add edi, 4
    loop @clr_sat

    mov ecx, 18
    lea edi, g_Quality_FmtMin
@clr_min:
    mov dword ptr [edi], 07FFFFFFFh
    add edi, 4
    loop @clr_min

    mov ecx, 18
    lea edi, g_Quality_FmtMax
@clr_max:
    mov dword ptr [edi], 080000000h
    add edi, 4
    loop @clr_max
    ret
ReverseQuant_ResetQualityMetrics ENDP

; ============================================================
; Quality_Update - update per-format statistics
; Input: EAX = value (post-clamp), BL = saturation flag (0/1)
; Uses g_Quality_CurrentFmt as index (0-17)
; ============================================================
Quality_Update PROC
    push ecx
    push edx
    push esi
    push edi

    mov ecx, [g_Quality_CurrentFmt]
    cmp ecx, 18
    jae @done

    ; total per-format samples
    lea edi, g_Quality_FmtSamples
    mov edx, [edi + ecx*4]
    inc edx
    mov [edi + ecx*4], edx

    ; per-format saturation
    test bl, bl
    jz @skip_sat
    lea edi, g_Quality_FmtSat
    mov edx, [edi + ecx*4]
    inc edx
    mov [edi + ecx*4], edx
@skip_sat:

    ; min/max tracking
    lea edi, g_Quality_FmtMin
    mov edx, [edi + ecx*4]
    cmp eax, edx
    jge @check_max
    mov [edi + ecx*4], eax
@check_max:
    lea edi, g_Quality_FmtMax
    mov edx, [edi + ecx*4]
    cmp eax, edx
    jle @done
    mov [edi + ecx*4], eax

@done:
    pop edi
    pop esi
    pop edx
    pop ecx
    ret
Quality_Update ENDP

; ============================================================
; VALIDATION AND ERROR HANDLING FUNCTIONS
; ============================================================

; ============================================================
; ValidatePointer - Validate buffer pointer
; Input:  EAX = pointer to validate
; Output: EAX = 1 if valid, 0 if NULL
; ============================================================
ValidatePointer PROC pBuf:DWORD
    mov eax, pBuf
    test eax, eax
    jz @null
    mov eax, 1
    ret
@null:
    xor eax, eax
    ret
ValidatePointer ENDP

; ============================================================
; ValidateBufferSize - Validate buffer size
; Input:  EAX = size in bytes
;         EDX = maximum allowed size
; Output: EAX = 1 if valid, 0 if too large
; ============================================================
ValidateBufferSize PROC cbSize:DWORD, cbMaxSize:DWORD
    mov eax, cbSize
    test eax, eax
    jz @invalid
    
    cmp eax, cbMaxSize
    jg @too_large
    
    mov eax, 1
    ret
    
@invalid:
@too_large:
    xor eax, eax
    ret
ValidateBufferSize ENDP

; ============================================================
; ValidateQuantFormat - Validate quantization format ID
; Input:  EAX = format ID to validate
; Output: EAX = 1 if recognized, 0 if unknown
; ============================================================
ValidateQuantFormat PROC fmt:DWORD
    mov eax, fmt
    
    ; Check against known formats
    cmp al, QUANT_FMT_Q4_0
    je @valid
    cmp al, QUANT_FMT_Q4_1
    je @valid
    cmp al, QUANT_FMT_Q4_K
    je @valid
    cmp al, QUANT_FMT_Q5_0
    je @valid
    cmp al, QUANT_FMT_Q5_1
    je @valid
    cmp al, QUANT_FMT_Q5_K
    je @valid
    cmp al, QUANT_FMT_Q8_0
    je @valid
    cmp al, QUANT_FMT_Q8_1
    je @valid
    cmp al, QUANT_FMT_Q8_K
    je @valid
    cmp al, QUANT_FMT_F16
    je @valid
    cmp al, QUANT_FMT_F32
    je @valid
    
@invalid:
    xor eax, eax
    ret
    
@valid:
    mov eax, 1
    ret
ValidateQuantFormat ENDP

; ============================================================
; Helper procedures for bit-level dequantization
; These convert quantized values to FP16/FP32 using scale factors
; ============================================================

; ============================================================
; Dequant2BitToF16 - Convert 2-bit quantized to FP16
; Input:  EAX = 2-bit value (0-3)
;         EDX = FP16 scale factor
; Output: AX = FP16 result
; Formula: result = (qval - 2) * scale
; ============================================================
Dequant2BitToF16 PROC qval:DWORD, scale:DWORD
    push ebx
    push ecx

    mov eax, qval
    movzx edx, word ptr scale

    xor ebx, ebx               ; saturation flag

    sub eax, 2                ; [0..3] -> [-2..1]
    imul eax, edx
    sar eax, 7

    inc [g_Quality_TotalSamples]
    cmp eax, 0x7BFF
    jle @clamp_min
    mov eax, 0x7BFF
    inc [g_Quality_SatHits]
    mov bl, 1
@clamp_min:
    cmp eax, 0x0001
    jge @done
    mov eax, 0x0001
    inc [g_Quality_SatHits]
    mov bl, 1
@done:
    call Quality_Update
    movzx eax, ax
    pop ecx
    pop ebx
    ret
Dequant2BitToF16 ENDP

; ============================================================
; Dequant4BitToF16 - Convert 4-bit quantized to FP16
; Input:  EAX = 4-bit value (0-15)
;         EDX = FP16 scale factor
; Output: AX = FP16 result
; Formula: result = (qval - 8) * scale
; ============================================================
Dequant4BitToF16 PROC qval:DWORD, scale:DWORD
    push ebx
    push ecx
    
    mov eax, qval
    movzx edx, word ptr scale
    
    inc [g_Quality_TotalSamples]
    xor ebx, ebx               ; saturation flag
    ; Shift 4-bit value to signed range: [0-15] -> [-8 to 7]
    sub eax, 8
    
    ; For FP16 scale multiplication (simplified integer approximation)
    ; Real implementation would use FP arithmetic
    ; This dequantizes: result ≈ (qval - 8) * scale
    imul eax, edx
    sar eax, 7              ; Divide by 128 (scale adjustment)
    
    ; Clamp to FP16 range if needed
    cmp eax, 0x7BFF        ; Check FP16 max
    jle @clamp_min
    mov eax, 0x7BFF
    inc [g_Quality_SatHits]
    mov bl, 1
    
@clamp_min:
    cmp eax, 0x0001        ; Check FP16 min
    jge @done_clamping
    mov eax, 0x0001
    inc [g_Quality_SatHits]
    mov bl, 1
    
@done_clamping:
    call Quality_Update
    movzx eax, ax
    pop ecx
    pop ebx
    ret
Dequant4BitToF16 ENDP

; ============================================================
; Dequant5BitToF16 - Convert 5-bit quantized to FP16
; Input:  EAX = 5-bit value (0-31)
;         EDX = FP16 scale factor
; Output: AX = FP16 result
; Formula: result = (qval - 16) * scale
; ============================================================
Dequant5BitToF16 PROC qval:DWORD, scale:DWORD
    push ebx
    push ecx
    
    inc [g_Quality_TotalSamples]
    xor ebx, ebx
    mov eax, qval
    movzx edx, word ptr scale
    
    ; Shift 5-bit value to signed range: [0-31] -> [-16 to 15]
    sub eax, 16
    
    ; Dequantize: result = (qval - 16) * scale
    imul eax, edx
    sar eax, 7
    
    call Quality_Update
    movzx eax, ax
    pop ecx
    pop ebx
    ret
Dequant5BitToF16 ENDP

; ============================================================
; Dequant8BitToF16 - Convert 8-bit quantized to FP16
; Input:  EAX = signed 8-bit value (-128 to 127)
;         EDX = FP16 scale factor
; Output: AX = FP16 result
; Formula: result = qval * scale
; ============================================================
Dequant8BitToF16 PROC qval:DWORD, scale:DWORD
    push ebx
    push ecx
    
    inc [g_Quality_TotalSamples]
    xor ebx, ebx
    movsx eax, byte ptr qval  ; Sign-extend 8-bit to 32-bit
    movzx edx, word ptr scale
    
    ; Dequantize: result = qval * scale
    imul eax, edx
    sar eax, 7              ; Divide by 128
    
    call Quality_Update
    movzx eax, ax
    pop ecx
    pop ebx
    ret
Dequant8BitToF16 ENDP

; ============================================================
; Dequant4BitToF32 - Convert 4-bit quantized to FP32
; Input:  EAX = 4-bit value (0-15)
;         EDX = FP16 scale factor
; Output: EAX = F32 result (as INT representation)
; ============================================================
Dequant4BitToF32 PROC qval:DWORD, scale:DWORD
    push ebx
    push ecx
    
    inc [g_Quality_TotalSamples]
    xor ebx, ebx

    movsx eax, byte ptr qval
    xor ebx, ebx

    mov eax, qval
    xor ebx, ebx

    mov eax, qval
    mov eax, qval
    movzx edx, word ptr scale
    
    ; Shift to signed range [-8 to 7]
    sub eax, 8
    
    ; Convert to F32: (qval - 8) * scale / 128
    ; For full F32 precision, this would use FP instructions
    ; Simplified: scale as integer multiply
    imul eax, edx
    sar eax, 7
    call Quality_Update
    
    pop ecx
    pop ebx
    ret
Dequant4BitToF32 ENDP

; ============================================================
; Dequant5BitToF32 - Convert 5-bit quantized to FP32
; Input:  EAX = 5-bit value (0-31)
;         EDX = FP16 scale factor
; Output: EAX = F32 result (as INT representation)
; ============================================================
Dequant2BitToF32 PROC qval:DWORD, scale:DWORD
    push ebx
    push ecx

    inc [g_Quality_TotalSamples]
    xor ebx, ebx

    mov eax, qval
    mov eax, qval
    movzx edx, word ptr scale
    sub eax, 2                ; [-2..1]
    imul eax, edx
    sar eax, 7

    call Quality_Update

    pop ecx
    pop ebx
    ret
Dequant2BitToF32 ENDP

Dequant5BitToF32 PROC qval:DWORD, scale:DWORD
    push ebx
    push ecx
    
    inc [g_Quality_TotalSamples]
    mov eax, qval
    movzx edx, word ptr scale
    
    ; Shift to signed range [-16 to 15]
    sub eax, 16
    
    imul eax, edx
    sar eax, 7

    call Quality_Update
    
    pop ecx
    pop ebx
    ret
Dequant5BitToF32 ENDP

; ============================================================
; Dequant8BitToF32 - Convert 8-bit quantized to FP32
; Input:  EAX = signed 8-bit value (-128 to 127)
;         EDX = FP16 scale factor
; Output: EAX = F32 result (as INT representation)
; ============================================================
Dequant8BitToF32 PROC qval:DWORD, scale:DWORD
    push ebx
    push ecx
    
    inc [g_Quality_TotalSamples]
    movsx eax, byte ptr qval
    movzx edx, word ptr scale
    
    ; Dequantize: qval * scale / 128
    imul eax, edx
    sar eax, 7
    call Quality_Update
    
    pop ecx
    pop ebx
    ret
Dequant8BitToF32 ENDP

BuildQ4Lookup PROC
    ; Build lookup table for Q4 dequantization
    ; Maps 4-bit values (0-15) -> FP16 equivalents
    ; Table location: g_pLookupTable (4KB buffer)
    ; Offset: 0-2048 (16 entries * 128 scales)
    
    cmp [g_pLookupTable], 0
    je @not_allocated
    
    push ebx
    push ecx
    push edx
    push esi
    push edi
    
    mov edi, [g_pLookupTable]    ; Base of lookup table
    xor ebx, ebx                 ; 4-bit value (0-15)
    
@q4_outer_loop:
    cmp ebx, 16
    jge @q4_done
    
    ; For each 4-bit value, generate FP16 approximations
    ; Entry: (4-bit value - 8) in signed range [-8, 7]
    mov eax, ebx
    sub eax, 8
    
    ; Store in table (2 bytes per entry)
    movzx ecx, ax
    mov [edi], cx
    add edi, 2
    
    inc ebx
    jmp @q4_outer_loop
    
@q4_done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    ret
    
@not_allocated:
    xor eax, eax
    ret
BuildQ4Lookup ENDP

BuildQ8Lookup PROC
    ; Build lookup table for Q8 dequantization
    ; Maps 8-bit signed values (-128 to 127) -> FP16 equivalents
    ; Table location: offset 2048+ in g_pLookupTable
    
    cmp [g_pLookupTable], 0
    je @not_allocated
    
    push ebx
    push ecx
    push edx
    push esi
    push edi
    
    mov edi, [g_pLookupTable]
    add edi, 2048               ; Offset for Q8 lookup table
    
    mov ebx, -128               ; Start with signed minimum
    
@q8_outer_loop:
    cmp ebx, 128
    jge @q8_done
    
    ; Store signed 8-bit value as FP16
    movsx eax, bl
    movzx ecx, ax
    mov [edi], cx
    add edi, 2
    
    inc ebx
    jmp @q8_outer_loop
    
@q8_done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    ret
    
@not_allocated:
    xor eax, eax
    ret
BuildQ8Lookup ENDP

END
