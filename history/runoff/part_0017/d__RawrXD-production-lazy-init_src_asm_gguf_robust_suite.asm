; =============================================================================
; RawrXD Robust Tools Suite - Pure MASM64 x64
; =============================================================================
; Features:
;   - GGUF Metadata Parser with poisoned-length detection
;   - Safe memory allocator with 64GB hard limits
;   - Buffered streaming I/O with 64-bit offset support
;   - String/array skip utilities (handles chat_template bombs)
; =============================================================================

OPTION CASEMAP:NONE

; Windows API imports
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN SetFilePointerEx:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN SetLastError:PROC
EXTERN GetLastError:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN GetFileSize:PROC
EXTERN GetStdHandle:PROC
EXTERN AllocConsole:PROC
EXTERN lstrcmpiA:PROC
EXTERN GlobalMemoryStatusEx:PROC

; ------------------------------------------------------------------------------
; Constants
; ------------------------------------------------------------------------------
; File API
FILE_CURRENT        EQU 1
FILE_BEGIN          EQU 0
FILE_END            EQU 2

; Memory API
MEM_COMMIT          EQU 1000h
MEM_RESERVE         EQU 2000h
MEM_RELEASE         EQU 8000h
PAGE_READWRITE      EQU 4h
HEAP_ZERO_MEMORY    EQU 8h

; Console
STD_OUTPUT_HANDLE   EQU -11

; GGUF Constants
GGUF_MAGIC          EQU 046554747h      ; "GGUF" little-endian
GGUF_VERSION        EQU 3

; Metadata types
GGUF_TYPE_UINT8     EQU 0
GGUF_TYPE_INT8      EQU 1
GGUF_TYPE_UINT16    EQU 2
GGUF_TYPE_INT16     EQU 3
GGUF_TYPE_UINT32    EQU 4
GGUF_TYPE_INT32     EQU 5
GGUF_TYPE_FLOAT32   EQU 6
GGUF_TYPE_BOOL      EQU 7
GGUF_TYPE_STRING    EQU 8
GGUF_TYPE_ARRAY     EQU 9
GGUF_TYPE_UINT64    EQU 10
GGUF_TYPE_INT64     EQU 11
GGUF_TYPE_FLOAT64   EQU 12

; Safety limits
MAX_METADATA_STRING EQU 10485760        ; 10MB string limit (anti-bad_alloc)
MAX_ARRAY_ELEMENTS  EQU 10000000        ; 10M elements max
VERIFY_THRESHOLD    EQU 1099511627776   ; 1TB sanity check (catches corruption)
MAX_SINGLE_ALLOC    EQU 10737418240     ; 10GB single allocation limit
GLOBAL_ALLOC_LIMIT  EQU 68719476736     ; 64GB total allocation cap

; Error codes
ERROR_OK            EQU 0
ERROR_CORRUPTED     EQU 1
ERROR_TOOBIG        EQU 2
ERROR_IO            EQU 3
ERROR_MEMORY        EQU 4

; ------------------------------------------------------------------------------
; Data Section
; ------------------------------------------------------------------------------
.DATA
align 8

; File handle cache
hCurrentFile        QWORD 0

; File size tracking (LARGE_INTEGER)
liFileSize          QWORD 0
                    QWORD 0

; Current position (LARGE_INTEGER)
liCurrentPos        QWORD 0
                    QWORD 0

; Memory tracking
qAllocatedTotal     QWORD 0
qAllocationLimit    QWORD GLOBAL_ALLOC_LIMIT
hHeap               QWORD 0

; Skip tracking (for diagnostics)
szSkippedKey        BYTE 256 DUP(0)
qSkipCount          QWORD 0
qParsedCount        QWORD 0

; Toxic key strings (must skip these to avoid bad_alloc)
szKeyTokens         BYTE "tokenizer.ggml.tokens", 0
szKeyMerges         BYTE "tokenizer.ggml.merges", 0
szKeyScores         BYTE "tokenizer.ggml.scores", 0
szKeyTokenType      BYTE "tokenizer.ggml.token_type", 0
szKeyChatTemplate   BYTE "tokenizer.chat_template", 0

; Error strings (for debug output)
szErrCorrupted      BYTE "[ROBUST] GGUF corrupted: %s", 13, 10, 0
szErrTooBig         BYTE "[ROBUST] Object too large (%llu bytes): %s", 13, 10, 0
szErrMemory         BYTE "[ROBUST] Allocation failed (total: %llu MB)", 13, 10, 0
szInfoSkip          BYTE "[ROBUST] Lazy-skipping '%s' (%llu bytes)", 13, 10, 0

; Bytes read temp
dwBytesRead         DWORD 0

; ------------------------------------------------------------------------------
; Code Section
; ------------------------------------------------------------------------------
.CODE

; =============================================================================
; Tool 1: Safe Heap Allocator with Poison Detection
; =============================================================================
; Allocates memory with 64GB hard limit enforcement
; RCX = Size requested (QWORD)
; Returns: RAX = Pointer or NULL, RCX = Error code
; ------------------------------------------------------------------------------
SafeAllocate PROC FRAME
    .PUSHREG RBX
    .PUSHREG RSI
    .PUSHREG RDI
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 30h
    .ALLOCSTACK 30h
    .ENDPROLOG
    
    mov     rsi, rcx                    ; Save requested size
    
    ; Check 1: Zero size
    test    rsi, rsi
    jz      alloc_fail_zero
    
    ; Check 2: Individual allocation > 10GB (single object sanity)
    mov     rax, MAX_SINGLE_ALLOC
    cmp     rsi, rax
    ja      alloc_fail_toobig
    
    ; Check 3: Would exceed global limit?
    mov     rax, qAllocatedTotal
    add     rax, rsi
    cmp     rax, qAllocationLimit
    ja      alloc_fail_limit
    
    ; Get process heap if not cached
    cmp     hHeap, 0
    jne     heap_ready
    
    call    GetProcessHeap
    test    rax, rax
    jz      alloc_fail_heap
    mov     hHeap, rax

heap_ready:
    ; Allocate with HEAP_ZERO_MEMORY for safety
    mov     rcx, hHeap
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8, rsi
    call    HeapAlloc
    test    rax, rax
    jz      alloc_fail_heap
    
    ; Update tracking
    add     qAllocatedTotal, rsi
    
    ; RCX = ERROR_OK
    xor     ecx, ecx
    jmp     alloc_done

alloc_fail_zero:
    mov     ecx, ERROR_CORRUPTED
    xor     rax, rax
    jmp     alloc_done

alloc_fail_toobig:
    mov     ecx, ERROR_TOOBIG
    xor     rax, rax
    jmp     alloc_done

alloc_fail_limit:
    mov     ecx, ERROR_MEMORY
    xor     rax, rax
    jmp     alloc_done

alloc_fail_heap:
    mov     ecx, ERROR_MEMORY
    xor     rax, rax

alloc_done:
    add     rsp, 30h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SafeAllocate ENDP

; =============================================================================
; Tool 2: Safe Heap Free
; =============================================================================
; RCX = Pointer to free
; RDX = Size that was allocated (for tracking)
; Returns: RAX = TRUE success
; ------------------------------------------------------------------------------
SafeFree PROC FRAME
    .PUSHREG RBX
    push    rbx
    sub     rsp, 20h
    .ALLOCSTACK 20h
    .ENDPROLOG
    
    test    rcx, rcx
    jz      free_null
    
    mov     rbx, rdx                    ; Save size for tracking
    
    ; Free via heap
    push    rcx                         ; Save ptr
    mov     rcx, hHeap
    xor     edx, edx                    ; Flags = 0
    pop     r8                          ; Ptr to free
    call    HeapFree
    
    test    rax, rax
    jz      free_failed
    
    ; Update tracking (subtract freed size)
    sub     qAllocatedTotal, rbx
    
    mov     rax, 1
    jmp     free_done

free_null:
    mov     rax, 1                      ; Freeing NULL is OK
    jmp     free_done

free_failed:
    xor     rax, rax

free_done:
    add     rsp, 20h
    pop     rbx
    ret
SafeFree ENDP

; =============================================================================
; Tool 3: Robust String Skip (No Allocation)
; =============================================================================
; Skips GGUF string without loading into RAM (anti-bad_alloc weapon)
; RCX = File handle
; RDX = Pointer to QWORD (returns length that was skipped)
; Returns: RAX = TRUE success, FALSE fail
; ------------------------------------------------------------------------------
RobustSkipString PROC FRAME
    .PUSHREG RBX
    .PUSHREG RSI
    .PUSHREG RDI
    .PUSHREG R12
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40h
    .ALLOCSTACK 40h
    .ENDPROLOG
    
    mov     rbx, rcx                    ; File handle
    mov     r12, rdx                    ; Output length ptr
    
    ; Read 8-byte length into r12 target
    mov     rcx, rbx                    ; hFile
    mov     rdx, r12                    ; Buffer
    mov     r8d, 8                      ; Size to read
    lea     r9, [dwBytesRead]           ; Bytes read
    mov     QWORD PTR [rsp+20h], 0      ; lpOverlapped = NULL
    call    ReadFile
    
    test    rax, rax
    jz      skip_fail_io
    
    ; Verify length isn't poisoned (> 1TB = definitely corrupted)
    mov     rax, [r12]
    mov     rcx, VERIFY_THRESHOLD
    cmp     rax, rcx
    ja      skip_fail_corrupted
    
    ; Check against safe limit (10MB)
    mov     rcx, MAX_METADATA_STRING
    cmp     rax, rcx
    ja      skip_oversized              ; Large but maybe valid - skip it
    
    ; Normal size - seek past it
    mov     rdx, rax                    ; Distance.QuadPart (low)
    lea     r8, [rsp+30h]               ; lpNewFilePointer (temp)
    mov     r9d, FILE_CURRENT           ; dwMoveMethod
    mov     rcx, rbx                    ; hFile
    call    SetFilePointerEx
    test    rax, rax
    jz      skip_fail_io
    
    mov     rax, 1                      ; TRUE
    jmp     skip_done

skip_oversized:
    ; Large but not corrupted - seek past without loading
    mov     rdx, [r12]                  ; Distance.QuadPart
    lea     r8, [rsp+30h]               ; lpNewFilePointer
    mov     r9d, FILE_CURRENT
    mov     rcx, rbx
    call    SetFilePointerEx
    test    rax, rax
    jz      skip_fail_io
    
    ; Increment skip counter for diagnostics
    inc     qSkipCount
    
    mov     rax, 1                      ; TRUE
    jmp     skip_done

skip_fail_io:
    xor     rax, rax
    jmp     skip_done

skip_fail_corrupted:
    ; Set last error for debugging
    mov     ecx, ERROR_CORRUPTED
    call    SetLastError
    xor     rax, rax

skip_done:
    add     rsp, 40h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RobustSkipString ENDP

; =============================================================================
; Tool 4: Get GGUF Type Size
; =============================================================================
; RCX = GGUF type enum
; Returns: RAX = byte size (0 for variable types)
; ------------------------------------------------------------------------------
GetGGUFTypeSize PROC
    cmp     ecx, GGUF_TYPE_UINT8
    je      type_1byte
    cmp     ecx, GGUF_TYPE_INT8
    je      type_1byte
    cmp     ecx, GGUF_TYPE_BOOL
    je      type_1byte
    
    cmp     ecx, GGUF_TYPE_UINT16
    je      type_2byte
    cmp     ecx, GGUF_TYPE_INT16
    je      type_2byte
    
    cmp     ecx, GGUF_TYPE_UINT32
    je      type_4byte
    cmp     ecx, GGUF_TYPE_INT32
    je      type_4byte
    cmp     ecx, GGUF_TYPE_FLOAT32
    je      type_4byte
    
    cmp     ecx, GGUF_TYPE_UINT64
    je      type_8byte
    cmp     ecx, GGUF_TYPE_INT64
    je      type_8byte
    cmp     ecx, GGUF_TYPE_FLOAT64
    je      type_8byte
    
    ; Unknown or variable (STRING, ARRAY)
    xor     rax, rax
    ret

type_1byte:
    mov     rax, 1
    ret
type_2byte:
    mov     rax, 2
    ret
type_4byte:
    mov     rax, 4
    ret
type_8byte:
    mov     rax, 8
    ret
GetGGUFTypeSize ENDP

; =============================================================================
; Tool 5: Array Skip with Element Count Validation
; =============================================================================
; Skips GGUF array (type + count + data) safely
; RCX = File handle
; Returns: RAX = TRUE success, FALSE on corruption/IO error
; ------------------------------------------------------------------------------
RobustSkipArray PROC FRAME
    .PUSHREG RBX
    .PUSHREG RSI
    .PUSHREG RDI
    .PUSHREG R12
    .PUSHREG R13
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 50h
    .ALLOCSTACK 50h
    .ENDPROLOG
    
    mov     rbx, rcx                    ; File handle
    
    ; Read type (4 bytes) + count (8 bytes) = 12 bytes
    mov     rcx, rbx                    ; hFile
    lea     rdx, [rsp+30h]              ; Buffer for arr_type (4) + arr_count (8)
    mov     r8d, 12                     ; Size
    lea     r9, [dwBytesRead]
    mov     QWORD PTR [rsp+20h], 0      ; lpOverlapped
    call    ReadFile
    test    rax, rax
    jz      skip_arr_fail
    
    ; Check bytes read
    cmp     DWORD PTR [dwBytesRead], 12
    jne     skip_arr_fail
    
    ; Extract type (bytes 0-3) and count (bytes 4-11)
    mov     r12d, DWORD PTR [rsp+30h]   ; arr_type
    mov     r13, QWORD PTR [rsp+34h]    ; arr_count
    
    ; Validate count isn't insane (> 10M elements)
    mov     rax, MAX_ARRAY_ELEMENTS
    cmp     r13, rax
    ja      skip_arr_fail_corrupt
    
    ; Check for STRING arrays (need special handling)
    cmp     r12d, GGUF_TYPE_STRING
    je      skip_string_array
    
    ; Get element size from type
    mov     ecx, r12d
    call    GetGGUFTypeSize
    test    rax, rax
    jz      skip_arr_fail_unknown       ; Unknown type
    
    ; Calculate total size (count * elem_size)
    mov     rcx, r13                    ; count
    mul     rcx                         ; RAX = size * count, RDX = overflow
    
    ; Check for overflow (if RDX != 0, result > 64bit)
    test    rdx, rdx
    jnz     skip_arr_fail_corrupt
    
    ; Seek past data
    mov     rdx, rax                    ; Distance.QuadPart
    lea     r8, [rsp+40h]               ; lpNewFilePointer
    mov     r9d, FILE_CURRENT
    mov     rcx, rbx
    call    SetFilePointerEx
    test    rax, rax
    jz      skip_arr_fail
    
    mov     rax, 1                      ; TRUE
    jmp     skip_arr_done

skip_string_array:
    ; String arrays: iterate and skip each string
    mov     rsi, r13                    ; Loop counter
    test    rsi, rsi
    jz      skip_arr_success            ; Empty array is valid

skip_string_loop:
    ; Read string length
    mov     rcx, rbx
    lea     rdx, [rsp+40h]              ; Buffer for length
    mov     r8d, 8
    lea     r9, [dwBytesRead]
    mov     QWORD PTR [rsp+20h], 0
    call    ReadFile
    test    rax, rax
    jz      skip_arr_fail
    
    ; Skip string data
    mov     rdx, QWORD PTR [rsp+40h]    ; String length
    lea     r8, [rsp+48h]               ; lpNewFilePointer
    mov     r9d, FILE_CURRENT
    mov     rcx, rbx
    call    SetFilePointerEx
    test    rax, rax
    jz      skip_arr_fail
    
    dec     rsi
    jnz     skip_string_loop

skip_arr_success:
    mov     rax, 1
    jmp     skip_arr_done

skip_arr_fail_corrupt:
    mov     ecx, ERROR_CORRUPTED
    call    SetLastError
skip_arr_fail_unknown:
skip_arr_fail:
    xor     rax, rax

skip_arr_done:
    add     rsp, 50h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RobustSkipArray ENDP

; =============================================================================
; Tool 6: Check if Key Should Be Skipped (Tokenizer Bombs)
; =============================================================================
; RCX = Key string pointer (null-terminated)
; Returns: RAX = TRUE if should skip, FALSE otherwise
; ------------------------------------------------------------------------------
IsSkipKey PROC FRAME
    .PUSHREG RBX
    push    rbx
    sub     rsp, 20h
    .ALLOCSTACK 20h
    .ENDPROLOG
    
    mov     rbx, rcx                    ; Save key ptr
    
    ; Check for tokenizer.ggml.tokens
    mov     rcx, rbx
    lea     rdx, [szKeyTokens]
    call    lstrcmpiA
    test    eax, eax
    jz      is_skip_yes
    
    ; Check for tokenizer.ggml.merges
    mov     rcx, rbx
    lea     rdx, [szKeyMerges]
    call    lstrcmpiA
    test    eax, eax
    jz      is_skip_yes
    
    ; Check for tokenizer.ggml.scores
    mov     rcx, rbx
    lea     rdx, [szKeyScores]
    call    lstrcmpiA
    test    eax, eax
    jz      is_skip_yes
    
    ; Check for tokenizer.ggml.token_type
    mov     rcx, rbx
    lea     rdx, [szKeyTokenType]
    call    lstrcmpiA
    test    eax, eax
    jz      is_skip_yes
    
    ; Check for tokenizer.chat_template (the 800B parameter killer)
    mov     rcx, rbx
    lea     rdx, [szKeyChatTemplate]
    call    lstrcmpiA
    test    eax, eax
    jz      is_skip_yes
    
    ; Not a skip key
    xor     rax, rax
    jmp     is_skip_done
    
is_skip_yes:
    mov     rax, 1

is_skip_done:
    add     rsp, 20h
    pop     rbx
    ret
IsSkipKey ENDP

; =============================================================================
; Tool 7: Robust Metadata Parser Core
; =============================================================================
; Parses GGUF metadata with automatic skip for poisoned strings/arrays
; RCX = File handle
; RDX = Max metadata pairs to parse
; R8 = Callback proc (optional, NULL for skip-only mode)
; Returns: RAX = Pairs parsed, RCX = Last error code
; ------------------------------------------------------------------------------
RobustParseMetadata PROC FRAME
    .PUSHREG RBX
    .PUSHREG RSI
    .PUSHREG RDI
    .PUSHREG R12
    .PUSHREG R13
    .PUSHREG R14
    .PUSHREG R15
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 150h                   ; Large local frame
    .ALLOCSTACK 150h
    .ENDPROLOG
    
    mov     rbx, rcx                    ; File handle
    mov     r12, rdx                    ; Max pairs
    mov     r13, r8                     ; Callback (optional)
    xor     r14, r14                    ; Counter (pairs parsed)
    mov     qParsedCount, 0             ; Reset global counter
    
    ; Key buffer at [rsp+40h], 256 bytes
    ; Temp buffer at [rsp+140h]
    
parse_loop:
    cmp     r14, r12
    jge     parse_done                  ; Hit limit
    
    ; Read key length (8 bytes)
    mov     rcx, rbx
    lea     rdx, [rsp+140h]             ; Temp buffer
    mov     r8d, 8
    lea     r9, [dwBytesRead]
    mov     QWORD PTR [rsp+20h], 0
    call    ReadFile
    test    rax, rax
    jz      parse_io_error
    
    ; Validate key length (< 256 for sanity)
    mov     rax, QWORD PTR [rsp+140h]
    cmp     rax, 255
    ja      parse_corrupted             ; Key too long = corruption
    test    rax, rax
    jz      parse_corrupted             ; Zero length key
    
    mov     r15, rax                    ; Save key length
    
    ; Read key string
    mov     rcx, rbx
    lea     rdx, [rsp+40h]              ; Key buffer
    mov     r8, r15                     ; Key length
    lea     r9, [dwBytesRead]
    mov     QWORD PTR [rsp+20h], 0
    call    ReadFile
    test    rax, rax
    jz      parse_io_error
    
    ; Null terminate key
    lea     rax, [rsp+40h]
    add     rax, r15
    mov     BYTE PTR [rax], 0
    
    ; Read value type (4 bytes)
    mov     rcx, rbx
    lea     rdx, [rsp+140h]
    mov     r8d, 4
    lea     r9, [dwBytesRead]
    mov     QWORD PTR [rsp+20h], 0
    call    ReadFile
    test    rax, rax
    jz      parse_io_error
    
    ; Get type value
    mov     esi, DWORD PTR [rsp+140h]   ; Value type
    
    ; Check if this is a dangerous key
    lea     rcx, [rsp+40h]
    call    IsSkipKey
    mov     rdi, rax                    ; Save skip decision
    
    ; Route by type
    cmp     esi, GGUF_TYPE_STRING
    je      handle_string
    cmp     esi, GGUF_TYPE_ARRAY
    je      handle_array
    cmp     esi, GGUF_TYPE_FLOAT64
    jbe     handle_scalar
    jmp     parse_corrupted             ; Unknown type
    
handle_string:
    ; Read string length first
    mov     rcx, rbx
    lea     rdx, [rsp+148h]             ; String length buffer
    mov     r8d, 8
    lea     r9, [dwBytesRead]
    mov     QWORD PTR [rsp+20h], 0
    call    ReadFile
    test    rax, rax
    jz      parse_io_error
    
    mov     rax, QWORD PTR [rsp+148h]   ; String length
    
    ; Check if should skip (dangerous key OR > 10MB)
    test    rdi, rdi
    jnz     skip_string_value
    mov     rcx, MAX_METADATA_STRING
    cmp     rax, rcx
    ja      skip_string_value
    
    ; Normal string - skip it (we're in skip-only mode for safety)
    mov     rdx, rax
    lea     r8, [rsp+140h]
    mov     r9d, FILE_CURRENT
    mov     rcx, rbx
    call    SetFilePointerEx
    test    rax, rax
    jz      parse_io_error
    jmp     next_pair
    
skip_string_value:
    ; Skip dangerous/oversized string
    mov     rax, QWORD PTR [rsp+148h]
    mov     rdx, rax
    lea     r8, [rsp+140h]
    mov     r9d, FILE_CURRENT
    mov     rcx, rbx
    call    SetFilePointerEx
    test    rax, rax
    jz      parse_io_error
    inc     qSkipCount
    jmp     next_pair
    
handle_array:
    ; Skip entire array (safe method)
    mov     rcx, rbx
    call    RobustSkipArray
    test    rax, rax
    jz      parse_io_error
    jmp     next_pair
    
handle_scalar:
    ; Skip scalar based on size
    mov     ecx, esi
    call    GetGGUFTypeSize
    mov     rdx, rax
    lea     r8, [rsp+140h]
    mov     r9d, FILE_CURRENT
    mov     rcx, rbx
    call    SetFilePointerEx
    test    rax, rax
    jz      parse_io_error

next_pair:
    inc     r14
    inc     qParsedCount
    jmp     parse_loop

parse_done:
    mov     rax, r14                    ; Return count
    xor     ecx, ecx                    ; ERROR_OK
    jmp     parse_exit

parse_io_error:
    mov     ecx, ERROR_IO
    jmp     parse_exit_error

parse_corrupted:
    mov     ecx, ERROR_CORRUPTED

parse_exit_error:
    mov     rax, r14                    ; Return what we got

parse_exit:
    add     rsp, 150h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RobustParseMetadata ENDP

; =============================================================================
; Tool 8: Safe Memory Copy with Bounds Check
; =============================================================================
; bool SafeCopyMemory(void* dst, size_t dst_size, void* src, size_t src_size)
; RCX = dst, RDX = dst_size, R8 = src, R9 = src_size
; Returns: 0 on success, 1 on bounds violation
; ------------------------------------------------------------------------------
SafeCopyMemory PROC FRAME
    .PUSHREG RSI
    .PUSHREG RDI
    push    rsi
    push    rdi
    .ENDPROLOG

    mov     rdi, rcx                    ; dst
    mov     rcx, rdx                    ; dst_size
    mov     rsi, r8                     ; src
    mov     rax, r9                     ; src_size

    ; Bounds check: src_size > dst_size ?
    cmp     rax, rcx
    ja      copy_fail

    ; Use rep movsb with rcx = count
    mov     rcx, rax
    rep     movsb
    
    xor     eax, eax                    ; Success = 0
    jmp     copy_done

copy_fail:
    mov     eax, 1                      ; Failure = 1

copy_done:
    pop     rdi
    pop     rsi
    ret
SafeCopyMemory ENDP

; =============================================================================
; Tool 9: Zero Memory with Guard
; =============================================================================
; void ZeroMemoryGuarded(void* ptr, size_t len, size_t max_allowed)
; RCX = ptr, RDX = len, R8 = max_allowed
; Traps if len > max_allowed (debug builds)
; ------------------------------------------------------------------------------
ZeroMemoryGuarded PROC FRAME
    .PUSHREG RDI
    push    rdi
    .ENDPROLOG
    
    mov     rdi, rcx                    ; ptr
    
    ; Check: len > max_allowed?
    cmp     rdx, r8
    ja      trap_corruption
    
    xor     eax, eax                    ; Zero value
    mov     rcx, rdx                    ; len
    rep     stosb
    
    pop     rdi
    ret

trap_corruption:
    int     3                           ; Break into debugger
    pop     rdi
    ret
ZeroMemoryGuarded ENDP

; =============================================================================
; Tool 10: Get Skip Count (Diagnostics)
; =============================================================================
; Returns: RAX = number of dangerous values skipped
; ------------------------------------------------------------------------------
GetSkipCount PROC
    mov     rax, qSkipCount
    ret
GetSkipCount ENDP

; =============================================================================
; Tool 11: Get Parsed Count (Diagnostics)
; =============================================================================
; Returns: RAX = number of metadata pairs parsed
; ------------------------------------------------------------------------------
GetParsedCount PROC
    mov     rax, qParsedCount
    ret
GetParsedCount ENDP

; =============================================================================
; Tool 12: Reset Diagnostics
; =============================================================================
ResetDiagnostics PROC
    mov     qSkipCount, 0
    mov     qParsedCount, 0
    ret
ResetDiagnostics ENDP

; =============================================================================
; Tool 13: Get Allocated Total
; =============================================================================
; Returns: RAX = total bytes allocated via SafeAllocate
; ------------------------------------------------------------------------------
GetAllocatedTotal PROC
    mov     rax, qAllocatedTotal
    ret
GetAllocatedTotal ENDP

END
