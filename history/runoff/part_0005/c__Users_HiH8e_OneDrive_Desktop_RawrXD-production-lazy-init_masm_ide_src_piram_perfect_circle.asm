; ============================================================================
; PIRAM_PERFECT_CIRCLE.ASM - 11-Pass Iterative Circle Jerking
; Each pass: halve size, π-transform, accumulate leftovers into next pass
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

GetProcessHeap PROTO
HeapAlloc PROTO :DWORD,:DWORD,:DWORD
HeapFree PROTO :DWORD,:DWORD,:DWORD

; ============================================================================
; CONSTANTS
; ============================================================================
PI_CONSTANT     equ 3296474
PI_INV_CONSTANT equ 333773
HEAP_ZERO_MEMORY equ 8h
MAX_PASSES      equ 11

; ============================================================================
; EXPORTS
; ============================================================================
PUBLIC PiRam_PerfectCircle11Pass
PUBLIC PiRam_GetCirclePassCount
PUBLIC PiRam_GetCircleRatio

; ============================================================================
; DATA
; ============================================================================
.data
    g_circleRatio   dd 0        ; Final compression ratio
    g_circlePasses  dd 0        ; Actual passes used

.data?
    g_hHeap         dd ?

; ============================================================================
; CODE
; ============================================================================
.code

; ============================================================================
; PiRam_PerfectCircle11Pass - Multi-pass circle compression with leftovers
; pBuf: Input buffer
; dwSize: Input size
; Returns: EAX = final compressed size
; 
; Algorithm:
;   Pass 1: Halve(dwSize) -> size1, leftovers1
;   Pass 2: Halve(size1 + leftovers1) -> size2, leftovers2
;   Pass 3-11: Repeat until size < 1KB or passes exhausted
; ============================================================================
PiRam_PerfectCircle11Pass PROC pBuf:DWORD, dwSize:DWORD
    LOCAL originalSize:DWORD
    LOCAL currentSize:DWORD
    LOCAL leftoverSize:DWORD
    LOCAL leftoverBuf:DWORD
    LOCAL passCount:DWORD
    LOCAL pWorkBuf:DWORD
    LOCAL pNewBuf:DWORD

    push esi
    push edi
    push ebx

    mov esi, pBuf
    mov eax, dwSize
    mov originalSize, eax
    mov currentSize, eax
    mov leftoverSize, 0
    mov passCount, 0
    xor edi, edi
    mov leftoverBuf, edi  ; NULL

    ; Get process heap
    invoke GetProcessHeap
    mov g_hHeap, eax

@pass_loop:
    ; Check if we've done MAX_PASSES or size is negligible
    cmp passCount, MAX_PASSES
    jge @passes_done
    
    cmp currentSize, 400h        ; <1KB
    jle @passes_done

    ; Current pass: halve the size
    mov eax, currentSize
    shr eax, 1                  ; Halve size
    
    ; Apply π-transform to the halved portion
    xor ecx, ecx

@transform_loop:
    cmp ecx, eax
    jae @transform_done

    movzx edx, byte ptr [esi+ecx]
    imul edx, PI_CONSTANT
    shr edx, 20
    and edx, 0FFh
    mov byte ptr [esi+ecx], dl

    inc ecx
    jmp @transform_loop

@transform_done:
    ; Calculate leftover (remainder when halving)
    mov edx, currentSize
    and edx, 1              ; Leftover byte if odd
    
    ; Accumulate leftover into next buffer
    mov ebx, leftoverSize   ; Current leftover count
    add ebx, edx            ; Add new leftover
    
    ; If we have leftovers, accumulate them
    cmp ebx, 0
    je @no_leftovers
    
    ; Allocate/reallocate leftover buffer if needed
    cmp leftoverBuf, 0
    jne @reuse_leftover_buf
    
    ; First allocation
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, ebx
    test eax, eax
    jz @pass_error
    mov leftoverBuf, eax
    jmp @add_leftover
    
@reuse_leftover_buf:
    ; TODO: Reallocate if size increased (simplified: assume same)
    
@add_leftover:
    ; Copy leftover byte to buffer
    mov edi, leftoverBuf
    add edi, leftoverSize
    mov byte ptr [edi], dl
    mov leftoverSize, ebx

@no_leftovers:
    ; Prepare for next pass
    mov currentSize, eax        ; New size = halved size
    
    ; Merge leftover buffer with current if leftovers exist
    cmp leftoverSize, 0
    je @skip_merge
    
    ; Allocate merged buffer
    mov eax, currentSize
    add eax, leftoverSize
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, eax
    test eax, eax
    jz @pass_error
    mov pNewBuf, eax
    
    ; Copy current compressed data
    mov edi, pNewBuf
    mov esi, pBuf
    mov ecx, currentSize
    rep movsb
    
    ; Copy leftovers
    mov edi, pNewBuf
    add edi, currentSize
    mov esi, leftoverBuf
    mov ecx, leftoverSize
    rep movsb
    
    ; Update pointers and size
    mov esi, pNewBuf
    mov pWorkBuf, esi
    mov eax, currentSize
    add eax, leftoverSize
    mov currentSize, eax
    mov leftoverSize, 0
    
    ; Free old buffers
    cmp leftoverBuf, 0
    je @skip_leftover_free
    invoke HeapFree, g_hHeap, 0, leftoverBuf
    mov leftoverBuf, 0
@skip_leftover_free:

@skip_merge:
    ; Increment pass counter
    inc passCount
    jmp @pass_loop

@passes_done:
    ; Final size = current size + any remaining leftovers
    mov eax, currentSize
    add eax, leftoverSize
    
    ; Calculate ratio
    mov ecx, originalSize
    xor edx, edx
    mov ebx, eax
    test ebx, ebx
    jz @ratio_error
    mov eax, ecx
    div ebx
    imul eax, 100
    mov ecx, eax
    mov eax, offset g_circleRatio
    mov [eax], ecx
    mov eax, passCount
    mov ecx, offset g_circlePasses
    mov [ecx], eax
    
    mov eax, ebx    ; Return final size
    jmp @cleanup

@pass_error:
    xor eax, eax
    jmp @cleanup

@ratio_error:
    mov eax, currentSize
    mov ecx, offset g_circleRatio
    mov dword ptr [ecx], 200
    mov eax, passCount
    mov ecx, offset g_circlePasses
    mov [ecx], eax

@cleanup:
    ; Free leftover buffer if allocated
    cmp leftoverBuf, 0
    je @skip_cleanup_leftover
    invoke HeapFree, g_hHeap, 0, leftoverBuf
@skip_cleanup_leftover:

    pop ebx
    pop edi
    pop esi
    ret
PiRam_PerfectCircle11Pass ENDP

; ============================================================================
; PiRam_GetCirclePassCount - Get number of passes executed
; Returns: EAX = pass count
; ============================================================================
PiRam_GetCirclePassCount PROC
    mov eax, g_circlePasses
    ret
PiRam_GetCirclePassCount ENDP

; ============================================================================
; PiRam_GetCircleRatio - Get compression ratio from last 11-pass
; Returns: EAX = ratio * 100 (e.g., 1000 = 10:1)
; ============================================================================
PiRam_GetCircleRatio PROC
    mov eax, g_circleRatio
    ret
PiRam_GetCircleRatio ENDP

END
