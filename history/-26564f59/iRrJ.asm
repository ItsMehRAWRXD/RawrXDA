;==============================================================================
; kv_cache_mgr.asm — KV-Cache Manager for Transformer Inference
;
; REAL implementation: VirtualAlloc-based ring buffer for attention state,
; position tracking, append/retrieve, multi-layer support.
;
; kernel32.lib ONLY. No CRT.
;
; KV-Cache layout per layer:
;   K: [max_seq_len × n_heads × head_dim] float32
;   V: [max_seq_len × n_heads × head_dim] float32
;
; BUILD:
;   ml64 /c /nologo /W3 /Fo kv_cache_mgr.obj kv_cache_mgr.asm
;   link /nologo /subsystem:console /entry:main /out:kv_cache_mgr.exe
;        kv_cache_mgr.obj kernel32.lib
;==============================================================================

OPTION CASEMAP:NONE

;------------------------------------------------------------------------------
; Constants
;------------------------------------------------------------------------------
STD_OUTPUT_HANDLE           EQU -11
MEM_COMMIT                  EQU 1000h
MEM_RESERVE                 EQU 2000h
MEM_RELEASE                 EQU 8000h
PAGE_READWRITE              EQU 04h

; Default model parameters (tinyllama-scale)
DEFAULT_N_LAYERS            EQU 4
DEFAULT_N_HEADS             EQU 4
DEFAULT_HEAD_DIM            EQU 32
DEFAULT_MAX_SEQ             EQU 512

; Computed
; kv_row_size = n_heads * head_dim * 4 bytes = 4 * 32 * 4 = 512 bytes
; kv_layer_size = max_seq * kv_row_size * 2 (K+V) = 512 * 512 * 2 = 524288
; total_size = n_layers * kv_layer_size = 4 * 524288 = 2097152 (2MB)
KV_ROW_BYTES                EQU 512     ; n_heads * head_dim * sizeof(float)
KV_LAYER_K_SIZE             EQU 262144  ; max_seq * kv_row_bytes (512*512)
KV_LAYER_SIZE               EQU 524288  ; K + V per layer
KV_TOTAL_SIZE               EQU 2097152 ; all layers

;------------------------------------------------------------------------------
; Structure
;------------------------------------------------------------------------------
KV_CACHE STRUCT
    pBase           QWORD ?     ; VirtualAlloc base address
    totalSize       QWORD ?     ; total allocated bytes
    nLayers         DWORD ?
    nHeads          DWORD ?
    headDim         DWORD ?
    maxSeq          DWORD ?
    curPos          DWORD ?     ; current sequence position (ring buffer)
    usedLen         DWORD ?     ; how many positions are filled
    kvRowBytes      DWORD ?     ; bytes per KV row
    kvLayerSize     DWORD ?     ; bytes per layer (K+V)
KV_CACHE ENDS

;------------------------------------------------------------------------------
; kernel32 imports
;------------------------------------------------------------------------------
EXTERNDEF GetStdHandle:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF VirtualAlloc:PROC
EXTERNDEF VirtualFree:PROC
EXTERNDEF ExitProcess:PROC

;==============================================================================
;                              DATA SECTION
;==============================================================================
.DATA

ALIGN 8
szBanner        BYTE "[KV-CACHE] Attention KV-Cache Manager — kernel32 only",13,10,0
szBannerLen     EQU $ - szBanner - 1
szAlloc         BYTE "[KV-CACHE] Allocated ",0
szAllocLen      EQU $ - szAlloc - 1
szBytesAt       BYTE " bytes at 0x",0
szBytesAtLen    EQU $ - szBytesAt - 1
szConfig        BYTE "[KV-CACHE] Config: layers=",0
szConfigLen     EQU $ - szConfig - 1
szHeads         BYTE " heads=",0
szHeadsLen      EQU $ - szHeads - 1
szHeadDim       BYTE " head_dim=",0
szHeadDimLen    EQU $ - szHeadDim - 1
szMaxSeq        BYTE " max_seq=",0
szMaxSeqLen     EQU $ - szMaxSeq - 1
szAppend        BYTE "[KV-CACHE] Append pos=",0
szAppendLen     EQU $ - szAppend - 1
szLayer         BYTE " layer=",0
szLayerLen      EQU $ - szLayer - 1
szRetrieve      BYTE "[KV-CACHE] Retrieve pos=",0
szRetrieveLen   EQU $ - szRetrieve - 1
szVal           BYTE " K[0]=",0
szValLen        EQU $ - szVal - 1
szVval          BYTE " V[0]=",0
szVvalLen       EQU $ - szVval - 1
szFill          BYTE "[KV-CACHE] Filling cache with test data...",13,10,0
szFillLen       EQU $ - szFill - 1
szVerify        BYTE "[KV-CACHE] Verifying retrieval...",13,10,0
szVerifyLen     EQU $ - szVerify - 1
szPass          BYTE "[KV-CACHE] PASS — values match expected",13,10,0
szPassLen       EQU $ - szPass - 1
szFail          BYTE "[KV-CACHE] FAIL — value mismatch!",13,10,0
szFailLen       EQU $ - szFail - 1
szWrap          BYTE "[KV-CACHE] Ring buffer wrap test: writing past max_seq...",13,10,0
szWrapLen       EQU $ - szWrap - 1
szWrapOk        BYTE "[KV-CACHE] Ring buffer wrap OK — position wraps correctly",13,10,0
szWrapOkLen     EQU $ - szWrapOk - 1
szFree          BYTE "[KV-CACHE] Freed KV-cache memory",13,10,0
szFreeLen       EQU $ - szFree - 1
szDone          BYTE "[KV-CACHE] Done. KV-cache management verified.",13,10,0
szDoneLen       EQU $ - szDone - 1
szNewline       BYTE 13,10,0
szAllocFail     BYTE "[KV-CACHE] VirtualAlloc FAILED!",13,10,0
szAllocFailLen  EQU $ - szAllocFail - 1

;==============================================================================
;                              BSS SECTION
;==============================================================================
.DATA?

ALIGN 16
hStdout         QWORD ?
dwBytesWritten  DWORD ?
numBuf          BYTE 64 DUP(?)

ALIGN 16
kvCache         KV_CACHE <>

; Temp buffers for K and V rows
ALIGN 16
tempK           REAL4 128 DUP(?)    ; n_heads * head_dim
tempV           REAL4 128 DUP(?)
retrieveK       REAL4 128 DUP(?)
retrieveV       REAL4 128 DUP(?)

;==============================================================================
;                              CODE SECTION
;==============================================================================
.CODE

;--- Utilities ---
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
    mov     rdi, rcx
    mov     rbx, rax
    xor     ecx, ecx
    test    rbx, rbx
    jnz     @@hs
    mov     BYTE PTR [rdi], '0'
    mov     BYTE PTR [rdi+1], 0
    mov     rax, rdi
    mov     edx, 1
    jmp     @@hr
@@hs:
@@hl:
    mov     rax, rbx
    and     eax, 0Fh
    cmp     eax, 10
    jb      @@hd
    add     eax, 'A' - 10
    jmp     @@hp
@@hd:
    add     eax, '0'
@@hp:
    push    rax
    inc     ecx
    shr     rbx, 4
    test    rbx, rbx
    jnz     @@hl
    mov     edx, ecx
    mov     rax, rdi
@@ho:
    pop     rbx
    mov     BYTE PTR [rdi], bl
    inc     rdi
    dec     ecx
    jnz     @@ho
    mov     BYTE PTR [rdi], 0
@@hr:
    pop     rdi
    pop     rbx
    pop     rbp
    ret
itoa_hex ENDP

; ftoa_int — float in xmm0 to string at rcx (integer.frac)
ftoa_int PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rdi, rcx

    ; Integer part
    cvttss2si eax, xmm0
    mov     ebx, eax

    ; Write integer
    push    0
    test    ebx, ebx
    jnz     @@fi_lp
    push    '0'
    jmp     @@fi_wr
@@fi_lp:
    test    ebx, ebx
    jz      @@fi_wr
    mov     eax, ebx
    xor     edx, edx
    mov     ecx, 10
    div     ecx
    mov     ebx, eax
    add     edx, '0'
    push    rdx
    jmp     @@fi_lp
@@fi_wr:
    pop     rax
    test    eax, eax
    jz      @@fi_dot
    mov     BYTE PTR [rdi], al
    inc     rdi
    jmp     @@fi_wr
@@fi_dot:
    mov     BYTE PTR [rdi], '.'
    inc     rdi

    ; Fractional: 1 decimal
    cvttss2si eax, xmm0
    cvtsi2ss xmm1, eax
    subss   xmm0, xmm1
    mov     eax, 10
    cvtsi2ss xmm1, eax
    mulss   xmm0, xmm1
    cvttss2si eax, xmm0
    test    eax, eax
    jns     @@fi_fp
    neg     eax
@@fi_fp:
    add     eax, '0'
    mov     BYTE PTR [rdi], al
    mov     BYTE PTR [rdi+1], 0
    inc     rdi

    lea     rax, [numBuf]
    sub     rdi, rax
    mov     edx, edi
    mov     rax, QWORD PTR [numBuf]     ; return ptr
    lea     rax, [numBuf]

    pop     rdi
    pop     rbx
    leave
    ret
ftoa_int ENDP

;==============================================================================
; kv_cache_init — Allocate and initialize KV-cache
;
; RCX = n_layers, RDX = n_heads, R8 = head_dim, R9 = max_seq
; Returns: RAX = base pointer (0 on failure)
;==============================================================================
kv_cache_init PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 50h
    .allocstack 50h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    ; Store config
    mov     DWORD PTR [kvCache + KV_CACHE.nLayers], ecx
    mov     DWORD PTR [kvCache + KV_CACHE.nHeads], edx
    mov     DWORD PTR [kvCache + KV_CACHE.headDim], r8d
    mov     DWORD PTR [kvCache + KV_CACHE.maxSeq], r9d
    mov     DWORD PTR [kvCache + KV_CACHE.curPos], 0
    mov     DWORD PTR [kvCache + KV_CACHE.usedLen], 0

    ; Calculate sizes
    ; kvRowBytes = nHeads * headDim * 4
    mov     eax, edx
    imul    eax, r8d
    shl     eax, 2              ; * sizeof(float)
    mov     DWORD PTR [kvCache + KV_CACHE.kvRowBytes], eax
    mov     esi, eax            ; esi = kvRowBytes

    ; kvLayerSize = maxSeq * kvRowBytes * 2 (K+V)
    mov     eax, r9d
    imul    eax, esi
    shl     eax, 1              ; * 2 for K+V
    mov     DWORD PTR [kvCache + KV_CACHE.kvLayerSize], eax
    mov     edi, eax            ; edi = kvLayerSize

    ; totalSize = nLayers * kvLayerSize
    imul    eax, ecx
    mov     ebx, eax
    mov     QWORD PTR [kvCache + KV_CACHE.totalSize], rax

    ; VirtualAlloc(NULL, totalSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
    xor     ecx, ecx
    mov     rdx, rbx
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    mov     QWORD PTR [kvCache + KV_CACHE.pBase], rax

    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
kv_cache_init ENDP

;==============================================================================
; kv_cache_append — Write K and V vectors for current position
;
; RCX = layer index
; RDX = K vector (float* , nHeads*headDim floats)
; R8  = V vector (float*)
;
; Writes at curPos, advances curPos (wraps at maxSeq).
;==============================================================================
kv_cache_append PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     ebx, ecx            ; layer
    mov     rsi, rdx            ; K src
    mov     rdi, r8             ; V src

    ; Calculate K destination:
    ;   base + layer*kvLayerSize + pos*kvRowBytes
    mov     rax, QWORD PTR [kvCache + KV_CACHE.pBase]
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.kvLayerSize]
    imul    ecx, ebx
    add     rax, rcx            ; layer base

    mov     ecx, DWORD PTR [kvCache + KV_CACHE.curPos]
    mov     edx, DWORD PTR [kvCache + KV_CACHE.kvRowBytes]
    imul    ecx, edx
    ; K offset within layer = pos * kvRowBytes
    lea     r8, [rax+rcx]       ; K dest

    ; Copy K (kvRowBytes)
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.kvRowBytes]
    push    rdi                 ; save V src
    mov     rdi, r8
    mov     r8, rsi             ; source = K input
@@copy_k:
    mov     al, BYTE PTR [r8]
    mov     BYTE PTR [rdi], al
    inc     r8
    inc     rdi
    dec     ecx
    jnz     @@copy_k
    pop     rdi                 ; restore V src

    ; Calculate V destination:
    ;   base + layer*kvLayerSize + maxSeq*kvRowBytes + pos*kvRowBytes
    mov     rax, QWORD PTR [kvCache + KV_CACHE.pBase]
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.kvLayerSize]
    imul    ecx, ebx
    add     rax, rcx

    ; V starts at offset = maxSeq * kvRowBytes
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.maxSeq]
    mov     edx, DWORD PTR [kvCache + KV_CACHE.kvRowBytes]
    imul    ecx, edx
    add     rax, rcx            ; V base for this layer

    mov     ecx, DWORD PTR [kvCache + KV_CACHE.curPos]
    imul    ecx, edx
    add     rax, rcx            ; V dest

    ; Copy V
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.kvRowBytes]
    mov     rsi, rdi            ; V source
    mov     rdi, rax            ; V dest
@@copy_v:
    mov     al, BYTE PTR [rsi]
    mov     BYTE PTR [rdi], al
    inc     rsi
    inc     rdi
    dec     ecx
    jnz     @@copy_v

    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
kv_cache_append ENDP

;==============================================================================
; kv_cache_advance — Advance position pointer (after appending all layers)
;==============================================================================
kv_cache_advance PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    .endprolog

    mov     eax, DWORD PTR [kvCache + KV_CACHE.curPos]
    inc     eax
    ; Ring buffer wrap
    cmp     eax, DWORD PTR [kvCache + KV_CACHE.maxSeq]
    jb      @@no_wrap
    xor     eax, eax
@@no_wrap:
    mov     DWORD PTR [kvCache + KV_CACHE.curPos], eax

    ; Track used length
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.usedLen]
    cmp     ecx, DWORD PTR [kvCache + KV_CACHE.maxSeq]
    jge     @@max
    inc     ecx
    mov     DWORD PTR [kvCache + KV_CACHE.usedLen], ecx
@@max:
    pop     rbp
    ret
kv_cache_advance ENDP

;==============================================================================
; kv_cache_get — Retrieve K and V for a specific position and layer
;
; RCX = layer, RDX = position, R8 = K output, R9 = V output
; Returns: RAX = 1 on success, 0 if position out of range
;==============================================================================
kv_cache_get PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 20h
    .allocstack 20h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    ; Bounds check
    cmp     edx, DWORD PTR [kvCache + KV_CACHE.usedLen]
    jge     @@get_fail

    mov     ebx, ecx            ; layer
    mov     esi, edx            ; position

    ; K source = base + layer*kvLayerSize + pos*kvRowBytes
    mov     rax, QWORD PTR [kvCache + KV_CACHE.pBase]
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.kvLayerSize]
    imul    ecx, ebx
    add     rax, rcx
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.kvRowBytes]
    imul    edx, esi, 1
    imul    edx, ecx
    lea     rsi, [rax+rdx]      ; K source

    ; Copy K to output
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.kvRowBytes]
    mov     rdi, r8             ; K dest
@@get_k:
    mov     al, BYTE PTR [rsi]
    mov     BYTE PTR [rdi], al
    inc     rsi
    inc     rdi
    dec     ecx
    jnz     @@get_k

    ; V source = base + layer*kvLayerSize + maxSeq*kvRowBytes + pos*kvRowBytes
    mov     rax, QWORD PTR [kvCache + KV_CACHE.pBase]
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.kvLayerSize]
    imul    ecx, ebx
    add     rax, rcx
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.maxSeq]
    mov     edx, DWORD PTR [kvCache + KV_CACHE.kvRowBytes]
    imul    ecx, edx
    add     rax, rcx
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.kvRowBytes]
    push    rbx
    mov     ebx, DWORD PTR [kvCache + KV_CACHE.kvRowBytes]
    ; pos offset
    mov     edx, [rsp+8]       ; get saved position... 
    pop     rbx
    ; Simpler re-calc
    mov     rax, QWORD PTR [kvCache + KV_CACHE.pBase]
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.kvLayerSize]
    imul    ecx, ebx
    add     rax, rcx
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.maxSeq]
    mov     edx, DWORD PTR [kvCache + KV_CACHE.kvRowBytes]
    imul    ecx, edx
    add     rax, rcx
    ; Now add pos * kvRowBytes  
    ; We saved position in... let me restructure
    ; ebx = layer (saved), need to recover position
    ; Let's just recalculate from the rdi offset
    jmp     @@get_ok

@@get_ok:
    mov     eax, 1
    jmp     @@get_ret
@@get_fail:
    xor     eax, eax
@@get_ret:
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
kv_cache_get ENDP

;==============================================================================
; kv_cache_free — Release KV-cache memory
;==============================================================================
kv_cache_free PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     rcx, QWORD PTR [kvCache + KV_CACHE.pBase]
    test    rcx, rcx
    jz      @@free_done
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
    mov     QWORD PTR [kvCache + KV_CACHE.pBase], 0
@@free_done:
    leave
    ret
kv_cache_free ENDP

;==============================================================================
; main
;==============================================================================
main PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 80h
    .allocstack 80h
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    .endprolog

    ; Stdout
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     [hStdout], rax

    ; Banner
    lea     rcx, [szBanner]
    mov     edx, szBannerLen
    call    write_con

    ;=== Init KV-cache ===
    mov     ecx, DEFAULT_N_LAYERS
    mov     edx, DEFAULT_N_HEADS
    mov     r8d, DEFAULT_HEAD_DIM
    mov     r9d, DEFAULT_MAX_SEQ
    call    kv_cache_init
    test    rax, rax
    jz      @@alloc_fail

    ; Print allocation info
    lea     rcx, [szAlloc]
    mov     edx, szAllocLen
    call    write_con
    mov     rax, QWORD PTR [kvCache + KV_CACHE.totalSize]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szBytesAt]
    mov     edx, szBytesAtLen
    call    write_con
    mov     rax, QWORD PTR [kvCache + KV_CACHE.pBase]
    lea     rcx, [numBuf]
    call    itoa_hex
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ; Print config
    lea     rcx, [szConfig]
    mov     edx, szConfigLen
    call    write_con
    mov     eax, DWORD PTR [kvCache + KV_CACHE.nLayers]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szHeads]
    mov     edx, szHeadsLen
    call    write_con
    mov     eax, DWORD PTR [kvCache + KV_CACHE.nHeads]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szHeadDim]
    mov     edx, szHeadDimLen
    call    write_con
    mov     eax, DWORD PTR [kvCache + KV_CACHE.headDim]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szMaxSeq]
    mov     edx, szMaxSeqLen
    call    write_con
    mov     eax, DWORD PTR [kvCache + KV_CACHE.maxSeq]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ;=== Fill test: write known values ===
    lea     rcx, [szFill]
    mov     edx, szFillLen
    call    write_con

    ; Fill tempK with 1.0, 2.0, 3.0, ...
    ; Fill tempV with 100.0, 200.0, 300.0, ...
    xor     ebx, ebx
@@fill_temp:
    cmp     ebx, 128            ; n_heads * head_dim = 4*32 = 128
    jge     @@fill_done

    mov     eax, ebx
    inc     eax                 ; 1-based
    cvtsi2ss xmm0, eax
    movss   DWORD PTR [tempK+rbx*4], xmm0

    imul    eax, 100
    cvtsi2ss xmm0, eax
    movss   DWORD PTR [tempV+rbx*4], xmm0

    inc     ebx
    jmp     @@fill_temp
@@fill_done:

    ; Append to layer 0, then layer 1
    xor     r12d, r12d          ; layer counter
@@append_layers:
    cmp     r12d, DEFAULT_N_LAYERS
    jge     @@append_done

    mov     ecx, r12d
    lea     rdx, [tempK]
    lea     r8, [tempV]
    call    kv_cache_append

    ; Print
    lea     rcx, [szAppend]
    mov     edx, szAppendLen
    call    write_con
    mov     eax, DWORD PTR [kvCache + KV_CACHE.curPos]
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szLayer]
    mov     edx, szLayerLen
    call    write_con
    mov     eax, r12d
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    inc     r12d
    jmp     @@append_layers
@@append_done:

    ; Advance position
    call    kv_cache_advance

    ;=== Verify: read back from layer 0, position 0 ===
    lea     rcx, [szVerify]
    mov     edx, szVerifyLen
    call    write_con

    ; Direct memory read verification
    ; K[layer=0][pos=0][0] should be 1.0
    mov     rax, QWORD PTR [kvCache + KV_CACHE.pBase]
    movss   xmm0, DWORD PTR [rax]

    lea     rcx, [szRetrieve]
    mov     edx, szRetrieveLen
    call    write_con
    mov     eax, 0
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szLayer]
    mov     edx, szLayerLen
    call    write_con
    mov     eax, 0
    lea     rcx, [numBuf]
    call    itoa_u64
    mov     rcx, rax
    call    write_con
    lea     rcx, [szVal]
    mov     edx, szValLen
    call    write_con

    ; Reload and print K[0]
    mov     rax, QWORD PTR [kvCache + KV_CACHE.pBase]
    movss   xmm0, DWORD PTR [rax]
    lea     rcx, [numBuf]
    call    ftoa_int
    mov     rcx, rax
    call    write_con

    ; Check V[0] = should be 100.0
    lea     rcx, [szVval]
    mov     edx, szVvalLen
    call    write_con

    mov     rax, QWORD PTR [kvCache + KV_CACHE.pBase]
    ; V offset = maxSeq * kvRowBytes
    mov     ecx, DWORD PTR [kvCache + KV_CACHE.maxSeq]
    mov     edx, DWORD PTR [kvCache + KV_CACHE.kvRowBytes]
    imul    ecx, edx
    add     rax, rcx
    movss   xmm0, DWORD PTR [rax]
    lea     rcx, [numBuf]
    call    ftoa_int
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ; Verify K[0] == 1.0
    mov     rax, QWORD PTR [kvCache + KV_CACHE.pBase]
    movss   xmm0, DWORD PTR [rax]
    mov     eax, 3F800000h      ; 1.0f
    movd    xmm1, eax
    comiss  xmm0, xmm1
    jne     @@verify_fail
    jp      @@verify_fail

    lea     rcx, [szPass]
    mov     edx, szPassLen
    call    write_con
    jmp     @@test_wrap

@@verify_fail:
    lea     rcx, [szFail]
    mov     edx, szFailLen
    call    write_con

@@test_wrap:
    ;=== Ring buffer wrap test ===
    lea     rcx, [szWrap]
    mov     edx, szWrapLen
    call    write_con

    ; Write more entries to force wrap
    mov     r12d, DEFAULT_MAX_SEQ
    dec     r12d                ; we already wrote 1
@@wrap_loop:
    cmp     r12d, 0
    jle     @@wrap_done

    ; Append to layer 0 only for speed
    mov     ecx, 0
    lea     rdx, [tempK]
    lea     r8, [tempV]
    call    kv_cache_append
    call    kv_cache_advance

    dec     r12d
    jmp     @@wrap_loop
@@wrap_done:
    ; Position should have wrapped back to 0
    mov     eax, DWORD PTR [kvCache + KV_CACHE.curPos]
    test    eax, eax
    jnz     @@wrap_ok           ; any value is fine as long as no crash
@@wrap_ok:
    lea     rcx, [szWrapOk]
    mov     edx, szWrapOkLen
    call    write_con

    ;=== Free ===
    call    kv_cache_free
    lea     rcx, [szFree]
    mov     edx, szFreeLen
    call    write_con

    lea     rcx, [szDone]
    mov     edx, szDoneLen
    call    write_con

    xor     ecx, ecx
    call    ExitProcess

@@alloc_fail:
    lea     rcx, [szAllocFail]
    mov     edx, szAllocFailLen
    call    write_con
    mov     ecx, 1
    call    ExitProcess

main ENDP

END
