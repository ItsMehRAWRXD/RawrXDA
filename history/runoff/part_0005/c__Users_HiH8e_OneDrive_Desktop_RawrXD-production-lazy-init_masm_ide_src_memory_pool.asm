; ============================================================================
; RawrXD Agentic IDE - Memory Pool Allocator (Pure MASM)
; Implements memory pooling for performance optimization in Phase 6
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib

; ============================================================================
; CONSTANTS
; ============================================================================

; Memory pool sizes
DEFAULT_POOL_SIZE       EQU 1024 * 1024     ; 1 MB default pool size
SMALL_BLOCK_SIZE        EQU 64              ; Small block size
MEDIUM_BLOCK_SIZE       EQU 256             ; Medium block size
LARGE_BLOCK_SIZE        EQU 1024            ; Large block size

; Memory pool types
POOL_TYPE_SMALL         EQU 1
POOL_TYPE_MEDIUM        EQU 2
POOL_TYPE_LARGE         EQU 3
POOL_TYPE_CUSTOM        EQU 4

; ============================================================================
; DATA STRUCTURES
; ============================================================================

MEMORY_BLOCK STRUCT
    blockSize       DWORD ?
    isFree          DWORD ?
    nextBlock       DWORD ?     ; Pointer to next block
    prevBlock       DWORD ?     ; Pointer to previous block
MEMORY_BLOCK ENDS

MEMORY_POOL STRUCT
    poolType        DWORD ?
    blockSize       DWORD ?
    totalSize       DWORD ?
    usedSize        DWORD ?
    freeSize        DWORD ?
    pMemory         DWORD ?     ; Pointer to allocated memory
    pFirstBlock     DWORD ?     ; Pointer to first block
    pFreeList       DWORD ?     ; Pointer to free block list
    blockCount      DWORD ?
    freeBlockCount  DWORD ?
MEMORY_POOL ENDS

; ============================================================================
; GLOBAL VARIABLES
; ============================================================================

.data
    g_smallPool         MEMORY_POOL <>
    g_mediumPool        MEMORY_POOL <>
    g_largePool         MEMORY_POOL <>
    g_initialized       DWORD FALSE

; ============================================================================
; FUNCTION PROTOTYPES
; ============================================================================

MemoryPool_Init             proto
MemoryPool_Create           proto :DWORD, :DWORD
MemoryPool_Destroy          proto :DWORD
MemoryPool_Alloc            proto :DWORD, :DWORD
MemoryPool_Free             proto :DWORD, :DWORD
MemoryPool_FindFreeBlock    proto :DWORD, :DWORD
MemoryPool_SplitBlock       proto :DWORD, :DWORD, :DWORD
MemoryPool_CoalesceBlocks   proto :DWORD, :DWORD
MemoryPool_GetPoolStats     proto :DWORD, :DWORD
MemoryPool_Cleanup          proto

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; ============================================================================
; MemoryPool_Init - Initialize memory pools
; ============================================================================
MemoryPool_Init proc
    ; Check if already initialized
    .if g_initialized == TRUE
        jmp @Exit
    .endif
    
    ; Create small block pool (64-byte blocks)
    invoke MemoryPool_Create, POOL_TYPE_SMALL, DEFAULT_POOL_SIZE
    mov g_smallPool, eax
    
    ; Create medium block pool (256-byte blocks)
    invoke MemoryPool_Create, POOL_TYPE_MEDIUM, DEFAULT_POOL_SIZE
    mov g_mediumPool, eax
    
    ; Create large block pool (1024-byte blocks)
    invoke MemoryPool_Create, POOL_TYPE_LARGE, DEFAULT_POOL_SIZE
    mov g_largePool, eax
    
    ; Set initialized flag
    mov g_initialized, TRUE
    
@Exit:
    ret
MemoryPool_Init endp

; ============================================================================
; MemoryPool_Create - Create a memory pool
; Parameters: poolType - type of pool, poolSize - size of pool in bytes
; Returns: Handle to memory pool in eax
; ============================================================================
MemoryPool_Create proc poolType:DWORD, poolSize:DWORD
    LOCAL hPool:DWORD
    LOCAL pPool:DWORD
    LOCAL blockSize:DWORD
    LOCAL blockCount:DWORD
    LOCAL pMemory:DWORD
    LOCAL pBlock:DWORD
    LOCAL i:DWORD
    
    ; Allocate memory for pool structure
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, sizeof MEMORY_POOL
    mov pPool, eax
    mov hPool, eax
    test eax, eax
    jz @Exit
    
    ; Set pool properties
    mov eax, poolType
    mov [pPool].MEMORY_POOL.poolType, eax
    
    mov eax, poolSize
    mov [pPool].MEMORY_POOL.totalSize, eax
    mov [pPool].MEMORY_POOL.freeSize, eax
    mov [pPool].MEMORY_POOL.usedSize, 0
    
    ; Determine block size based on pool type
    mov eax, poolType
    .if eax == POOL_TYPE_SMALL
        mov blockSize, SMALL_BLOCK_SIZE
    .elseif eax == POOL_TYPE_MEDIUM
        mov blockSize, MEDIUM_BLOCK_SIZE
    .elseif eax == POOL_TYPE_LARGE
        mov blockSize, LARGE_BLOCK_SIZE
    .else
        mov blockSize, SMALL_BLOCK_SIZE  ; Default to small
    .endif
    
    mov eax, blockSize
    mov [pPool].MEMORY_POOL.blockSize, eax
    
    ; Calculate number of blocks
    mov eax, poolSize
    mov ecx, blockSize
    xor edx, edx
    div ecx
    mov blockCount, eax
    mov [pPool].MEMORY_POOL.blockCount, eax
    mov [pPool].MEMORY_POOL.freeBlockCount, eax
    
    ; Allocate memory for the pool
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, poolSize
    mov pMemory, eax
    mov [pPool].MEMORY_POOL.pMemory, eax
    test eax, eax
    jz @Cleanup
    
    ; Initialize blocks
    mov i, 0
@BlockInitLoop:
    mov eax, i
    cmp eax, blockCount
    jge @InitComplete
    
    ; Calculate block address
    mov eax, pMemory
    mov ecx, i
    mov edx, blockSize
    mul edx
    add eax, ecx
    mov pBlock, eax
    
    ; Initialize block
    mov [pBlock].MEMORY_BLOCK.blockSize, blockSize
    mov [pBlock].MEMORY_BLOCK.isFree, TRUE
    mov [pBlock].MEMORY_BLOCK.nextBlock, 0
    mov [pBlock].MEMORY_BLOCK.prevBlock, 0
    
    ; Add to free list (simplified - in real implementation would be more complex)
    .if i == 0
        mov [pPool].MEMORY_POOL.pFirstBlock, pBlock
        mov [pPool].MEMORY_POOL.pFreeList, pBlock
    .endif
    
    inc i
    jmp @BlockInitLoop
    
@InitComplete:
    mov eax, hPool
    jmp @Exit
    
@Cleanup:
    ; Clean up on failure
    invoke GlobalFree, pPool
    xor eax, eax
    
@Exit:
    ret
MemoryPool_Create endp

; ============================================================================
; MemoryPool_Destroy - Destroy a memory pool
; Parameters: hPool - handle to memory pool
; ============================================================================
MemoryPool_Destroy proc hPool:DWORD
    LOCAL pPool:DWORD
    
    mov eax, hPool
    mov pPool, eax
    test eax, eax
    jz @Exit
    
    ; Free pool memory
    mov eax, [pPool].MEMORY_POOL.pMemory
    test eax, eax
    jz @FreeStruct
    
    invoke GlobalFree, eax
    mov [pPool].MEMORY_POOL.pMemory, 0
    
@FreeStruct:
    ; Free pool structure
    invoke GlobalFree, pPool
    
@Exit:
    ret
MemoryPool_Destroy endp

; ============================================================================
; MemoryPool_Alloc - Allocate memory from pool
; Parameters: hPool - handle to memory pool, size - size to allocate
; Returns: Pointer to allocated memory in eax
; ============================================================================
MemoryPool_Alloc proc hPool:DWORD, size:DWORD
    LOCAL pPool:DWORD
    LOCAL pBlock:DWORD
    LOCAL blockSize:DWORD
    
    mov eax, hPool
    mov pPool, eax
    test eax, eax
    jz @Exit
    
    ; Find appropriate block size
    mov eax, size
    mov blockSize, eax
    
    ; Find a free block
    invoke MemoryPool_FindFreeBlock, pPool, blockSize
    mov pBlock, eax
    test eax, eax
    jz @Exit
    
    ; Mark block as used
    mov [pBlock].MEMORY_BLOCK.isFree, FALSE
    
    ; Update pool statistics
    mov eax, [pPool].MEMORY_POOL.usedSize
    add eax, blockSize
    mov [pPool].MEMORY_POOL.usedSize, eax
    
    mov eax, [pPool].MEMORY_POOL.freeSize
    sub eax, blockSize
    mov [pPool].MEMORY_POOL.freeSize, eax
    
    dec [pPool].MEMORY_POOL.freeBlockCount
    
    ; Return pointer to block data (skip header)
    lea eax, [pBlock + sizeof MEMORY_BLOCK]
    jmp @Exit
    
@Exit:
    ret
MemoryPool_Alloc endp

; ============================================================================
; MemoryPool_Free - Free memory back to pool
; Parameters: hPool - handle to memory pool, pMemory - pointer to memory
; ============================================================================
MemoryPool_Free proc hPool:DWORD, pMemory:DWORD
    LOCAL pPool:DWORD
    LOCAL pBlock:DWORD
    LOCAL blockSize:DWORD
    
    mov eax, hPool
    mov pPool, eax
    test eax, eax
    jz @Exit
    
    mov eax, pMemory
    test eax, eax
    jz @Exit
    
    ; Calculate block pointer (subtract header size)
    sub eax, sizeof MEMORY_BLOCK
    mov pBlock, eax
    
    ; Validate block
    mov eax, [pBlock].MEMORY_BLOCK.isFree
    .if eax == FALSE
        ; Mark block as free
        mov [pBlock].MEMORY_BLOCK.isFree, TRUE
        
        ; Update pool statistics
        mov eax, [pBlock].MEMORY_BLOCK.blockSize
        mov blockSize, eax
        
        mov eax, [pPool].MEMORY_POOL.usedSize
        sub eax, blockSize
        mov [pPool].MEMORY_POOL.usedSize, eax
        
        mov eax, [pPool].MEMORY_POOL.freeSize
        add eax, blockSize
        mov [pPool].MEMORY_POOL.freeSize, eax
        
        inc [pPool].MEMORY_POOL.freeBlockCount
        
        ; Try to coalesce adjacent free blocks
        invoke MemoryPool_CoalesceBlocks, pPool, pBlock
    .endif
    
@Exit:
    ret
MemoryPool_Free endp

; ============================================================================
; MemoryPool_FindFreeBlock - Find a free block of appropriate size
; Parameters: pPool - pointer to memory pool, size - requested size
; Returns: Pointer to free block in eax
; ============================================================================
MemoryPool_FindFreeBlock proc pPool:DWORD, size:DWORD
    LOCAL pBlock:DWORD
    LOCAL pCurrent:DWORD
    
    mov eax, pPool
    test eax, eax
    jz @Exit
    
    ; Start with first block
    mov eax, [eax].MEMORY_POOL.pFirstBlock
    mov pCurrent, eax
    
@SearchLoop:
    test eax, eax
    jz @Exit
    
    ; Check if block is free
    mov eax, [pCurrent].MEMORY_BLOCK.isFree
    .if eax == TRUE
        ; Check if block size is sufficient
        mov eax, [pCurrent].MEMORY_BLOCK.blockSize
        mov ecx, size
        .if eax >= ecx
            mov eax, pCurrent
            jmp @Exit
        .endif
    .endif
    
    ; Move to next block
    mov eax, [pCurrent].MEMORY_BLOCK.nextBlock
    mov pCurrent, eax
    jmp @SearchLoop
    
@Exit:
    ret
MemoryPool_FindFreeBlock endp

; ============================================================================
; MemoryPool_SplitBlock - Split a block into smaller blocks
; Parameters: pPool - pointer to memory pool, pBlock - block to split, size - requested size
; ============================================================================
MemoryPool_SplitBlock proc pPool:DWORD, pBlock:DWORD, size:DWORD
    ; Simplified implementation - in a real system, this would split blocks
    ; when a large block is used for a smaller allocation
    
    ret
MemoryPool_SplitBlock endp

; ============================================================================
; MemoryPool_CoalesceBlocks - Coalesce adjacent free blocks
; Parameters: pPool - pointer to memory pool, pBlock - block to coalesce
; ============================================================================
MemoryPool_CoalesceBlocks proc pPool:DWORD, pBlock:DWORD
    ; Simplified implementation - in a real system, this would merge
    ; adjacent free blocks to reduce fragmentation
    
    ret
MemoryPool_CoalesceBlocks endp

; ============================================================================
; MemoryPool_GetPoolStats - Get memory pool statistics
; Parameters: hPool - handle to memory pool, pStats - pointer to stats buffer
; ============================================================================
MemoryPool_GetPoolStats proc hPool:DWORD, pStats:DWORD
    LOCAL pPool:DWORD
    
    mov eax, hPool
    mov pPool, eax
    test eax, eax
    jz @Exit
    
    mov eax, pStats
    test eax, eax
    jz @Exit
    
    ; Copy pool data to stats buffer
    invoke RtlMoveMemory, pStats, pPool, sizeof MEMORY_POOL
    
@Exit:
    ret
MemoryPool_GetPoolStats endp

; ============================================================================
; MemoryPool_Cleanup - Clean up all memory pools
; ============================================================================
MemoryPool_Cleanup proc
    ; Destroy small pool
    mov eax, g_smallPool
    test eax, eax
    jz @SkipSmall
    invoke MemoryPool_Destroy, eax
    mov g_smallPool, 0
    
@SkipSmall:
    ; Destroy medium pool
    mov eax, g_mediumPool
    test eax, eax
    jz @SkipMedium
    invoke MemoryPool_Destroy, eax
    mov g_mediumPool, 0
    
@SkipMedium:
    ; Destroy large pool
    mov eax, g_largePool
    test eax, eax
    jz @SkipLarge
    invoke MemoryPool_Destroy, eax
    mov g_largePool, 0
    
@SkipLarge:
    ; Clear initialized flag
    mov g_initialized, FALSE
    
    ret
MemoryPool_Cleanup endp

end