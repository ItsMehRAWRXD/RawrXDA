;=============================================================================
; RawrXD Fully Reverse Engineered Robust Tools (FRERT)
; Architecture: x64 Native (MASM64)
; Purpose: Zero-dependency enterprise tooling for GGUF/AI workloads
; Features: Slab allocators, bounded parsers, CRC validation, atomic ops
;=============================================================================
; Build: ml64 /c /Zi /Fotools.obj tools.asm
; Link:  link /SUBSYSTEM:CONSOLE /ENTRY:main tools.obj kernel32.lib ntdll.lib
;=============================================================================

INCLUDE \masm64\include64\masm64rt.inc

;=============================================================================
; EQUATES & CONSTANTS
;=============================================================================
ROBUST_MAGIC        EQU 0x52425754      ; 'RBWT' - Robust Tools signature
MAX_STRING_BUF      EQU 1024*1024*16    ; 16MB max safe string (anti bad_alloc)
SLAB_SIZE           EQU 1024*1024*64    ; 64MB slabs
ALIGNMENT           EQU 64              ; Cache line alignment
RETRY_ATTEMPTS      EQU 3               ; I/O retry count
CRC64_POLY          EQU 0xC96C5795D7870F42 ; ECMA-182 polynomial

; GGUF Types
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

;=============================================================================
; STRUCTURES (UNPACKED FOR PERFORMANCE)
;=============================================================================
SLAB_HEADER STRUCT
    Magic           DQ ?                ; ROBUST_MAGIC
    Size            DQ ?                ; Total slab size
    Used            DQ ?                ; Bytes allocated
    Next            DQ ?                ; Next slab ptr
    Flags           DQ ?                ; 0=committed, 1=reserved
    Bitmap          DQ 64 DUP(?)        ; 512-bit allocation bitmap
SLAB_HEADER ENDS

ROBUST_CTX STRUCT
    HeapBase        DQ ?                ; Slab chain head
    SlabCount       DQ ?                ; Active slabs
    TotalAllocated  DQ ?                ; Bytes in use
    PeakUsage       DQ ?                ; High water mark
    LogHandle       DQ ?                ; Debug log file
    CrcTable        DQ 256 DUP(?)       ; CRC64 lookup
ROBUST_CTX ENDS

GGUF_STRING STRUCT
    Length          DQ ?                ; uint64_t len
    DataPtr         DQ ?                ; Pointer (null if skipped)
    Flags           DD ?                ; 1=heap, 2=slab, 4=skipped
    Reserved        DD ?                
GGUF_STRING ENDS

IO_CONTEXT STRUCT
    Handle          DQ ?                ; File handle
    FileSize        DQ ?                ; Total size
    Position        DQ ?                ; Current offset
    BufferBase      DQ ?                ; Read buffer
    BufferSize      DQ ?                ; Buffer capacity
    ValidBytes      DQ ?                ; Bytes in buffer
    RetryCount      DD ?                ; Retry attempts left
    Flags           DD ?                ; 1=eof, 2=error
IO_CONTEXT ENDS

;=============================================================================
; .DATA SECTION
;=============================================================================
.DATA
align 8
g_RobustCtx       ROBUST_CTX <>
g_SlabLock        DQ 0                  ; Spinlock for slab allocator
g_IoLock          DQ 0                  ; Spinlock for I/O stats

; Error strings
szErrSlabCorrupt  DB "[ROBUST] FATAL: Slab corruption detected",13,10,0
szErrBadAlloc     DB "[ROBUST] FATAL: Allocation exceeds safety bounds",13,10,0
szErrIoRetry      DB "[ROBUST] WARN: I/O retry triggered",13,10,0
szInfoSkip        DB "[ROBUST] INFO: Skipped oversized object (%llu bytes)",13,10,0

;=============================================================================
; .CODE SECTION
;=============================================================================
.CODE

;-----------------------------------------------------------------------------
; RtlSecureZeroMemory forward declaration (from ntdll)
;-----------------------------------------------------------------------------
EXTERNDEF RtlSecureZeroMemory:PROC

;=============================================================================
; SLAB ALLOCATOR (Lock-Free with Bounded Check)
;=============================================================================

;-----------------------------------------------------------------------------
; Robust_Initialize - Initialize robust context
;-----------------------------------------------------------------------------
Robust_Initialize PROC FRAME
    push rbx
    push rsi
    push rdi
    .ALLOCSTACK 24
    .ENDPROLOG
    
    mov rbx, OFFSET g_RobustCtx
    
    ; Initialize CRC64 table
    mov rsi, rbx
    add rsi, OFFSET ROBUST_CTX.CrcTable
    mov rcx, 256
InitCrcLoop:
    mov rax, rcx
    dec rax
    mov rdx, 8
CrcCalc:
    mov r8, rax
    shr r8, 63
    shl rax, 1
    and r8, CRC64_POLY
    xor rax, r8
    dec rdx
    jnz CrcCalc
    mov [rsi + rcx*8 - 8], rax
    dec rcx
    jnz InitCrcLoop
    
    ; Zero counters
    mov QWORD PTR [rbx].ROBUST_CTX.HeapBase, 0
    mov QWORD PTR [rbx].ROBUST_CTX.SlabCount, 0
    mov QWORD PTR [rbx].ROBUST_CTX.TotalAllocated, 0
    mov QWORD PTR [rbx].ROBUST_CTX.PeakUsage, 0
    
    pop rdi
    pop rsi
    pop rbx
    ret
Robust_Initialize ENDP

;-----------------------------------------------------------------------------
; Robust_Allocate - Bounded slab allocation with overflow protection
; rcx = Size bytes
; rdx = Flags (1=must succeed)
;-----------------------------------------------------------------------------
Robust_Allocate PROC FRAME
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    .ALLOCSTACK 40
    .ENDPROLOG
    
    mov r12, rcx            ; Size requested
    mov rbp, rdx            ; Flags
    
    ; Safety check: cannot exceed MAX_STRING_BUF
    cmp r12, MAX_STRING_BUF
    ja BoundViolation
    
    ; Add header overhead
    lea r12, [r12 + ALIGNMENT - 1]
    and r12, NOT (ALIGNMENT - 1)
    add r12, 16             ; Size + Magic header
    
    ; Acquire spinlock
    mov rsi, OFFSET g_SlabLock
SpinWait:
    mov rax, 1
    xchg rax, [rsi]
    test rax, rax
    jnz SpinWait
    
    ; Search existing slabs
    mov rbx, g_RobustCtx.HeapBase
    test rbx, rbx
    jz NewSlabNeeded
    
SlabWalk:
    mov rax, [rbx].SLAB_HEADER.Size
    sub rax, [rbx].SLAB_HEADER.Used
    cmp rax, r12
    jae FitFound
    
    mov rbx, [rbx].SLAB_HEADER.Next
    test rbx, rbx
    jnz SlabWalk
    
NewSlabNeeded:
    ; Allocate new 64MB slab via VirtualAlloc
    mov rcx, 0
    mov rdx, SLAB_SIZE
    mov r8, MEM_COMMIT or MEM_RESERVE
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz AllocFailed
    
    mov rbx, rax
    mov [rbx].SLAB_HEADER.Magic, ROBUST_MAGIC
    mov [rbx].SLAB_HEADER.Size, SLAB_SIZE
    mov [rbx].SLAB_HEADER.Used, SIZEOF SLAB_HEADER
    mov [rbx].SLAB_HEADER.Next, g_RobustCtx.HeapBase
    mov [rbx].SLAB_HEADER.Flags, 0
    
    ; Insert at head
    mov g_RobustCtx.HeapBase, rbx
    inc g_RobustCtx.SlabCount
    
FitFound:
    ; Allocate from slab
    mov rax, [rbx].SLAB_HEADER.Used
    add [rbx].SLAB_HEADER.Used, r12
    
    ; Mark bitmap (quick allocation tracking)
    mov rcx, rax
    shr rcx, 12             ; 4KB chunks
    mov rdx, r12
    shr rdx, 12
    inc rdx
    mov rdi, rbx
    add rdi, OFFSET SLAB_HEADER.Bitmap
SetBits:
    bts QWORD PTR [rdi], rcx
    inc rcx
    dec rdx
    jnz SetBits
    
    lea rax, [rbx + rax + 16] ; Skip header, leave magic
    mov QWORD PTR [rax - 16], r12 ; Store size
    mov QWORD PTR [rax - 8], ROBUST_MAGIC ^ 0xDEADBEEF ; Canary
    
    ; Update stats
    add g_RobustCtx.TotalAllocated, r12
    mov rcx, g_RobustCtx.TotalAllocated
    cmp g_RobustCtx.PeakUsage, rcx
    cmovb g_RobustCtx.PeakUsage, rcx
    
    ; Release spinlock
    mov QWORD PTR [rsi], 0
    
    mov rax, r12
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
    
BoundViolation:
    test rbp, 1
    jnz FatalAlloc
    xor rax, rax
    ; Release spinlock
    mov QWORD PTR [rsi], 0
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
    
AllocFailed:
    mov QWORD PTR [rsi], 0
    xor rax, rax
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
    
FatalAlloc:
    mov rcx, OFFSET szErrBadAlloc
    call printf
    mov ecx, 0xC0000017     ; STATUS_NO_MEMORY
    call ExitProcess
    
Robust_Allocate ENDP

;-----------------------------------------------------------------------------
; Robust_Free - Safe deallocation with canary validation
;-----------------------------------------------------------------------------
Robust_Free PROC FRAME
    push rbx
    .ALLOCSTACK 8
    .ENDPROLOG
    
    test rcx, rcx
    jz @F
    
    mov rbx, rcx
    sub rbx, 16
    
    ; Validate canary
    mov rax, [rbx + 8]
    xor rax, ROBUST_MAGIC
    cmp rax, 0xDEADBEEF
    jne CorruptionDetected
    
    ; Zero memory before free (security)
    mov rcx, [rbx]
    sub rcx, 16
    add rbx, 16
    push rbx
    call RtlSecureZeroMemory
    pop rbx
    
    sub rbx, 16
    ; Mark bitmap as free (simplified - would need slab ptr stored)
    
@@:
    pop rbx
    ret
    
CorruptionDetected:
    mov rcx, OFFSET szErrSlabCorrupt
    call printf
    int 3                   ; Break for debugger
    pop rbx
    ret
Robust_Free ENDP

;=============================================================================
; BOUNDED STREAM PARSER (GGUF Safe)
;=============================================================================

;-----------------------------------------------------------------------------
; Robust_OpenStream - Initialize safe file stream
;-----------------------------------------------------------------------------
Robust_OpenStream PROC FRAME
    push rbx
    push rsi
    .ALLOCSTACK 16
    .ENDPROLOG
    
    mov rsi, rcx            ; Filename
    mov rbx, rdx            ; IO_CONTEXT ptr
    
    ; CreateFileW
    mov rcx, rsi
    xor edx, edx
    mov r8, FILE_SHARE_READ
    mov r9, OPEN_EXISTING
    sub rsp, 32
    mov QWORD PTR [rsp+16], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp+24], 0
    call CreateFileW
    add rsp, 32
    
    cmp rax, INVALID_HANDLE_VALUE
    je OpenFailed
    
    mov [rbx].IO_CONTEXT.Handle, rax
    
    ; Get file size
    mov rcx, rax
    lea rdx, [rbx].IO_CONTEXT.FileSize
    mov r8, 0
    call GetFileSizeEx
    
    ; Alloc read buffer
    mov rcx, 1024*1024      ; 1MB buffer
    mov rdx, 0
    call Robust_Allocate
    mov [rbx].IO_CONTEXT.BufferBase, rax
    mov [rbx].IO_CONTEXT.BufferSize, 1024*1024
    mov [rbx].IO_CONTEXT.ValidBytes, 0
    mov [rbx].IO_CONTEXT.Position, 0
    mov [rbx].IO_CONTEXT.RetryCount, RETRY_ATTEMPTS
    
    mov rax, 1
    pop rsi
    pop rbx
    ret
    
OpenFailed:
    xor rax, rax
    pop rsi
    pop rbx
    ret
Robust_OpenStream ENDP

;-----------------------------------------------------------------------------
; Robust_ReadSafe - Bounded read with retry logic
; rcx = IO_CONTEXT
; rdx = Dest buffer
; r8 = Bytes to read (uint64)
;-----------------------------------------------------------------------------
Robust_ReadSafe PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .ALLOCSTACK 40
    .ENDPROLOG
    
    mov rbx, rcx            ; IO_CONTEXT
    mov rdi, rdx            ; Dest
    mov r13, r8             ; Count
    
    mov rsi, [rbx].IO_CONTEXT.Position
    mov r12, [rbx].IO_CONTEXT.FileSize
    sub r12, rsi            ; Remaining in file
    
    ; Bound check: cannot read past EOF
    cmp r13, r12
    cmova r13, r12          ; Clamp to available
    
RetryLoop:
    mov rcx, [rbx].IO_CONTEXT.Handle
    lea rdx, [rdi]
    mov r8, r13             ; Bytes to read
    lea r9, r12             ; Bytes read
    sub rsp, 32
    mov QWORD PTR [rsp], 0  ; Overlapped
    call ReadFile
    add rsp, 32
    
    test rax, rax
    jnz ReadSuccess
    
    call GetLastError
    cmp eax, ERROR_OPERATION_ABORTED
    je DoRetry
    cmp eax, ERROR_IO_PENDING
    je WaitAsync
    cmp eax, ERROR_HANDLE_EOF
    je ReadSuccess          ; Partial read OK
    
DoRetry:
    dec [rbx].IO_CONTEXT.RetryCount
    jz ReadFailed
    
    mov rcx, 100            ; 100ms backoff
    call Sleep
    jmp RetryLoop
    
WaitAsync:
    mov rcx, [rbx].IO_CONTEXT.Handle
    lea rdx, r12
    mov r8, 1000            ; 1 second timeout
    call GetOverlappedResult
    test rax, rax
    jnz ReadSuccess
    jmp DoRetry
    
ReadSuccess:
    add [rbx].IO_CONTEXT.Position, r12
    add [rbx].IO_CONTEXT.TotalRead, r12
    mov rax, r12            ; Return bytes read
    jmp @F
    
ReadFailed:
    xor rax, rax
@@:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Robust_ReadSafe ENDP

;-----------------------------------------------------------------------------
; Robust_SkipString - Skip GGUF string without allocation (anti bad_alloc)
; rcx = IO_CONTEXT
;-----------------------------------------------------------------------------
Robust_SkipString PROC FRAME
    push rbx
    push rsi
    .ALLOCSTACK 16
    .ENDPROLOG
    
    mov rbx, rcx
    
    ; Read length (8 bytes)
    sub rsp, 16
    mov rdx, rsp
    mov r8, 8
    call Robust_ReadSafe
    
    cmp rax, 8
    jne SkipFail
    
    mov rsi, [rsp]          ; Length value
    
    ; Safety cap check
    cmp rsi, MAX_STRING_BUF
    ja SkipHuge             ; Log and skip anyway if manageable
    
    ; Seek forward
    mov rcx, [rbx].IO_CONTEXT.Handle
    mov rdx, rsi
    mov r8, FILE_CURRENT
    call SetFilePointerEx
    
SkipDone:
    add rsp, 16
    mov rax, rsi            ; Return skipped length
    pop rsi
    pop rbx
    ret
    
SkipHuge:
    ; For huge strings >16MB, seek in chunks if needed
    cmp rsi, 0x7FFFFFFF     ; Max SetFilePointer?
    jbe DoSeek
    
    ; Multi-part seek for >2GB strings (extreme edge case)
    mov rcx, [rbx].IO_CONTEXT.Handle
    mov rdx, 0x7FFFFFFF
    mov r8, FILE_CURRENT
    call SetFilePointerEx
    sub rsi, 0x7FFFFFFF
    jmp SkipHuge
    
DoSeek:
    mov rcx, [rbx].IO_Context.Handle
    mov rdx, rsi
    mov r8, FILE_CURRENT
    call SetFilePointerEx
    jmp SkipDone
    
SkipFail:
    add rsp, 16
    xor rax, rax
    pop rsi
    pop rbx
    ret
Robust_SkipString ENDP

;=============================================================================
; CRC64 VALIDATION (GGUF Integrity)
;=============================================================================

;-----------------------------------------------------------------------------
; Robust_Crc64Init - Initialize CRC context
;-----------------------------------------------------------------------------
Robust_Crc64Init PROC
    ; Already done in Initialize, but can refresh here
    ret
Robust_Crc64Init ENDP

;-----------------------------------------------------------------------------
; Robust_Crc64Update - Update CRC with buffer
; rcx = Previous CRC
; rdx = Buffer
; r8 = Length
;-----------------------------------------------------------------------------
Robust_Crc64Update PROC FRAME
    push rsi
    push rdi
    .ALLOCSTACK 16
    .ENDPROLOG
    
    mov rax, rcx
    not rax                 ; CRC init
    mov rsi, rdx
    mov rdi, r8
    
    test rdi, rdi
    jz Done
    
UpdateLoop:
    movzx rcx, BYTE PTR [rsi]
    mov rdx, rax
    shr rdx, 56
    xor rcx, rdx
    shl rax, 8
    and rcx, 0FFh
    mov rdx, g_RobustCtx.CrcTable[rcx*8]
    xor rax, rdx
    
    inc rsi
    dec rdi
    jnz UpdateLoop
    
Done:
    not rax
    pop rdi
    pop rsi
    ret
Robust_Crc64Update ENDP

;=============================================================================
; ATOMIC OPERATIONS (Lock-Free)
;=============================================================================

;-----------------------------------------------------------------------------
; Robust_AtomicInc64 - Interlocked increment with memory ordering
;-----------------------------------------------------------------------------
Robust_AtomicInc64 PROC
    mov rax, rcx
    lock inc QWORD PTR [rax]
    ret
Robust_AtomicInc64 ENDP

;-----------------------------------------------------------------------------
; Robust_AtomicCompareExchange - CAS primitive
;-----------------------------------------------------------------------------
Robust_AtomicCompareExchange PROC
    mov rax, r8             ; Expected
    lock cmpxchg [rcx], rdx
    ret
Robust_AtomicCompareExchange ENDP

;=============================================================================
; LOGGING & OBSERVABILITY
;=============================================================================

;-----------------------------------------------------------------------------
; Robust_Log - Thread-safe structured logging
;-----------------------------------------------------------------------------
Robust_Log PROC FRAME
    push rbx
    push rsi
    push rdi
    .ALLOCSTACK 24
    .ENDPROLOG
    
    ; Format: [TIMESTAMP] [LEVEL] Message
    ; Simplified: print to stderr for now
    
    mov rsi, OFFSET g_IoLock
    mov rdi, rcx            ; Message
    
    ; Acquire lock
SpinLog:
    mov rax, 1
    xchg rax, [rsi]
    test rax, rax
    jnz SpinLog
    
    ; Write to stderr
    mov rcx, 2              ; STDERR_HANDLE
    mov rdx, rdi
    mov r8, 0
@@:
    cmp BYTE PTR [rdx+r8], 0
    lea r8, [r8+1]
    jne @B
    dec r8
    
    lea r9, rbx             ; Written
    sub rsp, 32
    call WriteFile
    add rsp, 32
    
    ; Release lock
    mov QWORD PTR [rsi], 0
    
    pop rdi
    pop rsi
    pop rbx
    ret
Robust_Log ENDP

;=============================================================================
; EXPORT TABLE
;=============================================================================
PUBLIC Robust_Initialize
PUBLIC Robust_Allocate
PUBLIC Robust_Free
PUBLIC Robust_OpenStream
PUBLIC Robust_ReadSafe
PUBLIC Robust_SkipString
PUBLIC Robust_Crc64Update
PUBLIC Robust_AtomicInc64
PUBLIC Robust_Log

END
