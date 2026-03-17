;  ============================================================================
;  IDE_13_CACHE.asm  –  Agent memory with KV store and vector search
;  ============================================================================
;  Implements:
;    - AgentMemory_Store (key-value + vector embedding)
;    - AgentMemory_Recall (top-k vector search)
;    - KVCache_Init / Put / Get
;  ============================================================================

include ..\agent_core\IDE_INC.ASM

; ============================================================================
;  EXPORTS
; ============================================================================
PUBLIC AgentMemory_Store
PUBLIC AgentMemory_Recall
PUBLIC KVCache_Init
PUBLIC KVCache_Put
PUBLIC KVCache_Get

; ============================================================================
;  CONSTANTS
; ============================================================================
MAX_MEMORY_ENTRIES    equ 4096
MAX_KEY_LEN           equ 128
MAX_VALUE_LEN         equ 1024
VECTOR_DIM            equ 768          ; embedding dimension

; ============================================================================
;  MEMORY ENTRY STRUCTURE
; ============================================================================
MEMORY_ENTRY STRUCT
    key         BYTE MAX_KEY_LEN dup(?)
    value       BYTE MAX_VALUE_LEN dup(?)
    vector      REAL4 VECTOR_DIM dup(?)
    timestamp   QWORD ?
    access_count DWORD ?
    valid       DWORD ?
MEMORY_ENTRY ENDS

; ============================================================================
;  DATA
; ============================================================================
.data
g_memory_table  MEMORY_ENTRY MAX_MEMORY_ENTRIES dup(<>)
g_memory_count  DWORD 0
g_heap_handle   DWORD 0

szMemStore      db "AgentMemory_Store", 0
szMemRecall     db "AgentMemory_Recall", 0
szCacheInit     db "KVCache_Init", 0
szCachePut      db "KVCache_Put", 0
szCacheGet      db "KVCache_Get", 0

; ============================================================================
;  CODE
; ============================================================================
.code

; ============================================================================
;  KVCache_Init – Initialize cache subsystem
; ============================================================================
KVCache_Init PROC
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    ; Create private heap for cache
    invoke HeapCreate, 0, 100000h, 0
    .if eax == NULL
        mov hr, E_OUTOFMEMORY
        jmp done
    .endif
    
    mov g_heap_handle, eax
    mov g_memory_count, 0
    mov hr, 0
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur
    
    LOG offset szCacheInit, hr, dword ptr [dur]
    
    mov eax, hr
    ret
KVCache_Init ENDP

; ============================================================================
;  KVCache_Put – Store key-value pair
; ============================================================================
KVCache_Put PROC uses ebx esi edi, pKey:DWORD, pValue:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL pEntry:PTR MEMORY_ENTRY
    LOCAL hash:DWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    .if pKey == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    .if pValue == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; Simple hash: sum of characters mod table size
    mov esi, pKey
    xor eax, eax
    xor ecx, ecx
hash_loop:
    movzx edx, byte ptr [esi + ecx]
    .if dl == 0
        jmp hash_done
    .endif
    add eax, edx
    inc ecx
    jmp hash_loop
    
hash_done:
    xor edx, edx
    mov ecx, MAX_MEMORY_ENTRIES
    div ecx
    mov hash, edx
    
    ; Find entry by hash
    mov ecx, hash
    imul ecx, sizeof MEMORY_ENTRY
    lea esi, g_memory_table
    add esi, ecx
    mov pEntry, esi
    
    ; Copy key
    invoke lstrcpynA, addr (MEMORY_ENTRY PTR [esi]).key, pKey, MAX_KEY_LEN
    
    ; Copy value
    invoke lstrcpynA, addr (MEMORY_ENTRY PTR [esi]).value, pValue, MAX_VALUE_LEN
    
    ; Set timestamp
    lea eax, (MEMORY_ENTRY PTR [esi]).timestamp
    push eax
    call _perf_now
    add esp, 4
    
    mov (MEMORY_ENTRY PTR [esi]).access_count, 1
    mov (MEMORY_ENTRY PTR [esi]).valid, 1
    
    ; TODO: Generate vector embedding from value
    
    inc g_memory_count
    mov hr, 0
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur
    
    LOG offset szCachePut, hr, dword ptr [dur]
    
    mov eax, hr
    ret
KVCache_Put ENDP

; ============================================================================
;  KVCache_Get – Retrieve value by key
; ============================================================================
KVCache_Get PROC uses ebx esi edi, pKey:DWORD, pValue:DWORD, cbValue:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL hash:DWORD
    
    PERF_INIT offset start
    call _enter_crit

    .if pKey == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    .if pValue == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; Hash the key
    mov esi, pKey
    xor eax, eax
    xor ecx, ecx
hash_loop2:
    movzx edx, byte ptr [esi + ecx]
    .if dl == 0
        jmp hash_done2
    .endif
    add eax, edx
    inc ecx
    jmp hash_loop2
    
hash_done2:
    xor edx, edx
    mov ecx, MAX_MEMORY_ENTRIES
    div ecx
    mov hash, edx
    
    ; Find entry
    mov ecx, hash
    imul ecx, sizeof MEMORY_ENTRY
    lea esi, g_memory_table
    add esi, ecx
    
    ; Check if valid
    .if (MEMORY_ENTRY PTR [esi]).valid == 0
        mov hr, E_FAIL
        jmp done
    .endif
    
    ; Compare keys
    invoke lstrcmpA, addr (MEMORY_ENTRY PTR [esi]).key, pKey
    .if eax != 0
        mov hr, E_FAIL
        jmp done
    .endif
    
    ; Copy value
    invoke lstrcpynA, pValue, addr (MEMORY_ENTRY PTR [esi]).value, cbValue
    
    ; Update access count
    inc (MEMORY_ENTRY PTR [esi]).access_count
    
    mov hr, 0
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur
    
    LOG offset szCacheGet, hr, dword ptr [dur]
    
    mov eax, hr
    ret
KVCache_Get ENDP

; ============================================================================
;  AgentMemory_Store – Store with vector embedding
; ============================================================================
AgentMemory_Store PROC uses ebx esi edi, pKey:DWORD, pValue:DWORD, pVector:PTR REAL4
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    
    PERF_INIT offset start
    
    ; Use KVCache_Put for storage
    invoke KVCache_Put, pKey, pValue
    mov hr, eax
    
    ; TODO: If pVector provided, store embedding
    
    PERF_DELTA offset start, offset dur, offset dur
    LOG offset szMemStore, hr, dword ptr [dur]
    
    mov eax, hr
    ret
AgentMemory_Store ENDP

; ============================================================================
;  AgentMemory_Recall – Vector search top-k
;  Input:  pQuery = query string or vector
;          pResults = output buffer for results
;          maxResults = maximum results to return
;  Output: eax = number of results found
; ============================================================================
AgentMemory_Recall PROC uses ebx esi edi, pQuery:DWORD, pResults:DWORD, maxResults:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL found:DWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    .if pQuery == 0
        mov hr, E_INVALIDARG
        xor eax, eax
        mov found, eax
        jmp done
    .endif
    .if pResults == 0
        mov hr, E_INVALIDARG
        xor eax, eax
        mov found, eax
        jmp done
    .endif
    
    ; TODO: Real vector search
    ; For now, return simple string match
    
    xor ecx, ecx
    xor ebx, ebx            ; result count
    mov esi, offset g_memory_table
    
search_loop:
    .if ecx >= MAX_MEMORY_ENTRIES
        jmp search_done
    .endif
    
    .if (MEMORY_ENTRY PTR [esi]).valid == 1
        ; Simple substring match
        invoke lstrcmpA, addr (MEMORY_ENTRY PTR [esi]).key, pQuery
        .if eax == 0
            ; Found match
            inc ebx
            .if ebx >= maxResults
                jmp search_done
            .endif
        .endif
    .endif
    
    add esi, sizeof MEMORY_ENTRY
    inc ecx
    jmp search_loop
    
search_done:
    mov found, ebx
    mov hr, 0
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur
    
    LOG offset szMemRecall, hr, dword ptr [dur]
    
    mov eax, found
    ret
AgentMemory_Recall ENDP

END
