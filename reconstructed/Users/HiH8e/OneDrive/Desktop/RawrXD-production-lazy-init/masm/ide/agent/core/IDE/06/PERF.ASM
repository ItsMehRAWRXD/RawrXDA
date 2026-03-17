;==============================================================================
; RawrXD Performance Optimizations - Pure MASM Implementation
; Phase 6: SIMD Operations, Fast Memory Pool, and Cache Optimization
;==============================================================================

.686
.model flat, stdcall
option casemap:none
.xmm ; Enable SSE instructions

include windows.inc
include user32.inc
include kernel32.inc

includelib user32.lib
includelib kernel32.lib

;==============================================================================
; PERFORMANCE CONSTANTS
;==============================================================================
POOL_BLOCK_SIZE         equ 4096
MAX_POOL_BLOCKS         equ 1024
CACHE_LINE_SIZE         equ 64

;==============================================================================
; PERFORMANCE STRUCTURES
;==============================================================================
MEM_POOL_BLOCK struct
    pData           dd ?
    used            dd ?
    next            dd ?
MEM_POOL_BLOCK ends

MEM_POOL struct
    firstBlock      dd ?
    currentBlock    dd ?
    totalAllocated  dd ?
    lock            dd ?
MEM_POOL ends

;==============================================================================
; PERFORMANCE STATE
;==============================================================================
.data?
globalPool              MEM_POOL <?>
cpuFeatures             dd ?

.code

;==============================================================================
; Phase 6.1: CPU Feature Detection
;==============================================================================
Perf_Init proc
    ; Check for SSE2 support
    mov eax, 1
    cpuid
    mov cpuFeatures, edx ; EDX bit 26 is SSE2
    
    ; Initialize global memory pool
    invoke MemPool_Init, addr globalPool
    
    ret
Perf_Init endp

;==============================================================================
; Phase 6.2: SIMD Accelerated Operations
;==============================================================================

; Fast SIMD Memcpy (SSE2)
; Assumes 16-byte alignment for best performance
Perf_MemcpySSE proc pDest:DWORD, pSrc:DWORD, count:DWORD
    mov edi, pDest
    mov esi, pSrc
    mov ecx, count
    
    ; Handle small copies or unaligned start
    .if ecx < 64
        rep movsb
        ret
    .endif
    
    ; Copy 64 bytes at a time using XMM registers
    shr ecx, 6 ; count / 64
    
copy_loop:
    movdqu xmm0, [esi]
    movdqu xmm1, [esi + 16]
    movdqu xmm2, [esi + 32]
    movdqu xmm3, [esi + 48]
    
    movdqu [edi], xmm0
    movdqu [edi + 16], xmm1
    movdqu [edi + 32], xmm2
    movdqu [edi + 48], xmm3
    
    add esi, 64
    add edi, 64
    dec ecx
    jnz copy_loop
    
    ; Handle remaining bytes
    mov ecx, count
    and ecx, 63
    rep movsb
    
    ret
Perf_MemcpySSE endp

; Fast SIMD Memset (SSE2)
Perf_MemsetSSE proc pDest:DWORD, val:BYTE, count:DWORD
    mov edi, pDest
    movzx eax, val
    mov ecx, count
    
    ; Broadcast byte to all 16 bytes of XMM0
    movd xmm0, eax
    punpcklbw xmm0, xmm0
    punpcklwd xmm0, xmm0
    pshufd xmm0, xmm0, 0
    
    .if ecx < 64
        mov al, val
        rep stosb
        ret
    .endif
    
    shr ecx, 6 ; count / 64
    
set_loop:
    movdqu [edi], xmm0
    movdqu [edi + 16], xmm0
    movdqu [edi + 32], xmm0
    movdqu [edi + 48], xmm0
    
    add edi, 64
    dec ecx
    jnz set_loop
    
    mov ecx, count
    and ecx, 63
    mov al, val
    rep stosb
    
    ret
Perf_MemsetSSE endp

;==============================================================================
; Phase 6.3: High-Performance Memory Pool
;==============================================================================
MemPool_Init proc pPool:DWORD
    mov edi, pPool
    assume edi:ptr MEM_POOL
    
    invoke VirtualAlloc, NULL, POOL_BLOCK_SIZE, MEM_COMMIT, PAGE_READWRITE
    mov [edi].firstBlock, eax
    mov [edi].currentBlock, eax
    mov [edi].totalAllocated, 0
    mov [edi].lock, 0
    
    mov esi, eax
    assume esi:ptr MEM_POOL_BLOCK
    invoke VirtualAlloc, NULL, POOL_BLOCK_SIZE - SIZEOF MEM_POOL_BLOCK, MEM_COMMIT, PAGE_READWRITE
    mov [esi].pData, eax
    mov [esi].used, 0
    mov [esi].next, 0
    
    ret
MemPool_Init endp

MemPool_Alloc proc pPool:DWORD, size:DWORD
    local blockSize:DWORD
    
    mov edi, pPool
    assume edi:ptr MEM_POOL
    
    ; Simple spinlock
lock_loop:
    lock bts [edi].lock, 0
    jc lock_loop
    
    mov esi, [edi].currentBlock
    assume esi:ptr MEM_POOL_BLOCK
    
    ; Align size to 16 bytes
    mov eax, size
    add eax, 15
    and eax, -16
    mov blockSize, eax
    
    mov eax, [esi].used
    add eax, blockSize
    .if eax > (POOL_BLOCK_SIZE - SIZEOF MEM_POOL_BLOCK)
        ; Need new block
        invoke VirtualAlloc, NULL, POOL_BLOCK_SIZE, MEM_COMMIT, PAGE_READWRITE
        mov ebx, eax
        mov [esi].next, ebx
        mov [edi].currentBlock, ebx
        
        mov esi, ebx
        invoke VirtualAlloc, NULL, POOL_BLOCK_SIZE - SIZEOF MEM_POOL_BLOCK, MEM_COMMIT, PAGE_READWRITE
        mov [esi].pData, eax
        mov [esi].used, 0
        mov [esi].next, 0
    .endif
    
    mov eax, [esi].pData
    add eax, [esi].used
    mov ebx, blockSize
    add [esi].used, ebx
    
    ; Unlock
    mov [edi].lock, 0
    
    ret
MemPool_Alloc endp

;==============================================================================
; Phase 6.4: Fast String Hashing (FNV-1a)
;==============================================================================
Perf_HashString proc pStr:DWORD
    mov esi, pStr
    mov eax, 0811C9DC5h ; FNV offset basis
    mov ecx, 01000193h  ; FNV prime
    
hash_loop:
    movzx edx, byte ptr [esi]
    test dl, dl
    jz hash_done
    
    xor eax, edx
    mul ecx
    
    inc esi
    jmp hash_loop
    
hash_done:
    ret
Perf_HashString endp

;==============================================================================
; Phase 6.5: Cache-Aligned Data Access
;==============================================================================
; Ensures a pointer is aligned to a cache line boundary
Perf_AlignToCache proc pPtr:DWORD
    mov eax, pPtr
    add eax, CACHE_LINE_SIZE - 1
    and eax, -CACHE_LINE_SIZE
    ret
Perf_AlignToCache endp

end
