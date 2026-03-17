; ==============================================================================
; RawrXD Robust Memory Tools
; Zero-allocation string/memory primitives for GGUF parsing
; Pure MASM x64 - No CRT dependencies
; ==============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:3

; Windows API imports
EXTERN SetFilePointerEx:PROC
EXTERN ReadFile:PROC
EXTERN SetLastError:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN GetFileSize:PROC

; Constants
FILE_CURRENT        EQU 1
FILE_BEGIN          EQU 0
FILE_END            EQU 2
MEM_COMMIT          EQU 1000h
MEM_RESERVE         EQU 2000h
MEM_RELEASE         EQU 8000h
PAGE_READWRITE      EQU 4h
INVALID_SET_FILE_POINTER EQU 0FFFFFFFFh

; GGUF Constants
GGUF_MAX_STRING_LENGTH  EQU 01000000h  ; 16MB hard limit
GGUF_MAGIC              EQU 46554747h  ; "GGUF" little-endian

.DATA
    align 16
    ErrorInvalidData    DWORD 0000000Dh
    ErrorInvalidParam   DWORD 00000057h

.CODE

; ------------------------------------------------------------------------------
; StrSafe_SkipChunk: Seeks past oversized data without heap allocation
; RCX = hFile (HANDLE)
; RDX = Length to skip (uint64)
; Returns: RAX = 1 (success), 0 (failure), LastError preserved
; Clobbers: R8-R11
; ------------------------------------------------------------------------------
StrSafe_SkipChunk PROC FRAME
    .PUSHREG RBX
    .PUSHREG RDI
    .PUSHREG RSI
    sub rsp, 40h
    .ALLOCSTACK 40h
    .ENDPROLOG
    
    mov rbx, rcx                    ; Save hFile
    mov rdi, rdx                    ; Save length to skip
    
    ; Setup LARGE_INTEGER for SetFilePointerEx
    mov QWORD PTR [rsp+20h], rdi    ; liDistance.QuadPart = length
    
    ; Call SetFilePointerEx(hFile, liDistance, NULL, FILE_CURRENT)
    lea r8, [rsp+20h]               ; R8 = &liDistance
    xor r9, r9                      ; R9 = NULL (lpNewFilePointer)
    mov DWORD PTR [rsp+30h], FILE_CURRENT ; 5th arg on stack
    mov rcx, rbx                    ; RCX = hFile
    mov rdx, r8                     ; RDX = &liDistance
    call SetFilePointerEx
    
    test rax, rax
    jz skip_failed
    
    ; Success
    mov rax, 1
    jmp skip_done
    
skip_failed:
    xor rax, rax
    
skip_done:
    add rsp, 40h
    pop rsi
    pop rdi
    pop rbx
    ret
StrSafe_SkipChunk ENDP

; ------------------------------------------------------------------------------
; MemSafe_PeekU64: Reads uint64_t without buffer allocation
; RCX = hFile
; RDX = pointer to uint64_t result
; Returns: RAX = bytes read (8) or 0
; ------------------------------------------------------------------------------
MemSafe_PeekU64 PROC FRAME
    .PUSHREG RBX
    .PUSHREG RDI
    sub rsp, 40h
    .ALLOCSTACK 40h
    .ENDPROLOG
    
    mov rbx, rcx                    ; Save hFile
    mov rdi, rdx                    ; Save result pointer
    
    ; ReadFile(hFile, &qwordBuf, 8, &dwRead, NULL)
    lea r8, [rsp+28h]               ; R8 = &dwRead (local)
    lea rdx, [rsp+20h]              ; RDX = &qwordBuf (local)
    mov r9d, 8                      ; R9 = nNumberOfBytesToRead
    xor eax, eax
    mov QWORD PTR [rsp+30h], rax    ; 5th arg = NULL (lpOverlapped)
    mov rcx, rbx                    ; RCX = hFile
    
    ; Shuffle args for ReadFile convention
    push 0                          ; lpOverlapped = NULL
    push r8                         ; lpNumberOfBytesRead
    mov r8d, 8                      ; nNumberOfBytesToRead
    call ReadFile
    add rsp, 10h
    
    test rax, rax
    jz peek_failed
    
    ; Check if we read exactly 8 bytes
    mov eax, DWORD PTR [rsp+28h]    ; dwRead
    cmp eax, 8
    jne peek_failed
    
    ; Copy result to output pointer
    mov rax, QWORD PTR [rsp+20h]    ; qwordBuf
    mov [rdi], rax
    
    mov rax, 8                      ; Return 8
    jmp peek_done
    
peek_failed:
    xor rax, rax
    
peek_done:
    add rsp, 40h
    pop rdi
    pop rbx
    ret
MemSafe_PeekU64 ENDP

; ------------------------------------------------------------------------------
; GGUF_SkipStringValue: Robust skip for corrupted string lengths
; RCX = hFile
; Returns: RAX = 1 success, 0 fail
; Side effect: File pointer advanced past string payload
; ------------------------------------------------------------------------------
GGUF_SkipStringValue PROC FRAME
    .PUSHREG RBX
    sub rsp, 30h
    .ALLOCSTACK 30h
    .ENDPROLOG
    
    mov rbx, rcx                    ; Save hFile
    
    ; Read string length (uint64_t)
    lea rdx, [rsp+20h]              ; &strLen (local)
    mov rcx, rbx
    call MemSafe_PeekU64
    
    test rax, rax
    jz skip_string_failed
    
    ; Sanity check: Max 16MB strings
    mov rax, QWORD PTR [rsp+20h]    ; strLen
    cmp rax, GGUF_MAX_STRING_LENGTH
    ja corrupted_length
    
    ; Skip payload using StrSafe_SkipChunk
    mov rdx, rax                    ; Length
    mov rcx, rbx                    ; hFile
    call StrSafe_SkipChunk
    
    test rax, rax
    jz skip_string_failed
    
    mov rax, 1
    jmp skip_string_done
    
corrupted_length:
    ; Set ERROR_INVALID_DATA
    mov ecx, ErrorInvalidData
    call SetLastError
    
skip_string_failed:
    xor rax, rax
    
skip_string_done:
    add rsp, 30h
    pop rbx
    ret
GGUF_SkipStringValue ENDP

; ------------------------------------------------------------------------------
; GGUF_SkipArrayValue: Skip GGUF arrays (type + count + data)
; RCX = hFile
; RDX = ElementType (byte, actually stored in DL)
; Returns: RAX = 1 success, 0 fail
; ------------------------------------------------------------------------------
GGUF_SkipArrayValue PROC FRAME
    .PUSHREG RBX
    .PUSHREG R12
    .PUSHREG R13
    sub rsp, 40h
    .ALLOCSTACK 40h
    .ENDPROLOG
    
    mov rbx, rcx                    ; hFile
    movzx r12d, dl                  ; elemType (extended to 32-bit)
    
    ; Read element count (uint64_t)
    lea rdx, [rsp+20h]              ; &elemCount
    mov rcx, rbx
    call MemSafe_PeekU64
    
    test rax, rax
    jz skip_array_failed
    
    mov r13, QWORD PTR [rsp+20h]    ; elemCount
    
    ; Calculate element size from type
    xor eax, eax
    mov al, 1                       ; Default 1 byte
    
    cmp r12d, 2                     ; UINT16/INT16
    jne @F
    mov al, 2
    jmp calc_total
@@:
    cmp r12d, 4                     ; UINT32/INT32/FLOAT32
    jl check_size8
    cmp r12d, 6
    jg check_size8
    mov al, 4
    jmp calc_total
    
check_size8:
    cmp r12d, 7                     ; BOOL
    je calc_total                   ; (size=1 already)
    cmp r12d, 10                    ; UINT64/INT64/FLOAT64
    jl string_or_array
    cmp r12d, 12
    jg unknown_type
    mov al, 8
    jmp calc_total
    
string_or_array:
    cmp r12d, 8                     ; STRING array
    je string_array_loop
    cmp r12d, 9                     ; Nested array (unsupported)
    je nested_array
    jmp unknown_type
    
calc_total:
    ; RAX = elemSize, R13 = elemCount
    ; Check for overflow: elemCount * elemSize
    movzx rcx, al
    mov rax, r13
    mul rcx                         ; RDX:RAX = result
    test rdx, rdx
    jnz overflow_detected
    
    ; Skip total bytes
    mov rdx, rax
    mov rcx, rbx
    call StrSafe_SkipChunk
    
    test rax, rax
    jz skip_array_failed
    jmp skip_array_success
    
string_array_loop:
    ; Skip each string individually
    test r13, r13
    jz skip_array_success
    
@@:
    mov rcx, rbx
    call GGUF_SkipStringValue
    test rax, rax
    jz skip_array_failed
    
    dec r13
    jnz @B
    jmp skip_array_success
    
nested_array:
    ; Recursive arrays - mark as risky, skip count elements
    test r13, r13
    jz skip_array_success
    
@@:
    mov rcx, rbx
    xor edx, edx                    ; Unknown inner type
    call GGUF_SkipArrayValue
    test rax, rax
    jz skip_array_failed
    
    dec r13
    jnz @B
    jmp skip_array_success
    
unknown_type:
    mov ecx, ErrorInvalidParam
    call SetLastError
    jmp skip_array_failed
    
overflow_detected:
    ; Multiplication overflow = corruption
    xor rax, rax
    jmp skip_array_done
    
skip_array_failed:
    xor rax, rax
    jmp skip_array_done
    
skip_array_success:
    mov rax, 1
    
skip_array_done:
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
GGUF_SkipArrayValue ENDP

; ------------------------------------------------------------------------------
; GGUF_StreamInit: Initialize buffered reader
; RCX = hFile (must be opened FILE_FLAG_SEQUENTIAL_SCAN)
; RDX = pointer to GGUF_STREAM_CTX (uninitialized)
; Returns: RAX = 1 success, 0 fail
; ------------------------------------------------------------------------------
GGUF_STREAM_BUFFERSIZE EQU 65536  ; 64KB ring buffer

; GGUF_STREAM_CTX structure layout
GGUF_STREAM_CTX STRUCT
    hFile_      QWORD ?
    pBuffer_    QWORD ?
    cbBuffer_   QWORD ?
    cbValid_    QWORD ?
    cbConsumed_ QWORD ?
    fileOffset_ QWORD ?
    fEOF_       BYTE ?
    fError_     BYTE ?
    padding_    WORD ?
GGUF_STREAM_CTX ENDS

GGUF_StreamInit PROC FRAME
    .PUSHREG RBX
    .PUSHREG RDI
    sub rsp, 28h
    .ALLOCSTACK 28h
    .ENDPROLOG
    
    mov rbx, rcx                    ; hFile
    mov rdi, rdx                    ; pCtx
    
    ; Initialize structure
    mov [rdi].GGUF_STREAM_CTX.hFile_, rbx
    mov QWORD PTR [rdi].GGUF_STREAM_CTX.cbValid_, 0
    mov QWORD PTR [rdi].GGUF_STREAM_CTX.cbConsumed_, 0
    mov BYTE PTR [rdi].GGUF_STREAM_CTX.fEOF_, 0
    mov BYTE PTR [rdi].GGUF_STREAM_CTX.fError_, 0
    
    ; Allocate ring buffer (64KB + 4KB guard)
    xor ecx, ecx                    ; lpAddress = NULL
    mov edx, GGUF_STREAM_BUFFERSIZE + 4096
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    
    test rax, rax
    jz init_failed
    
    mov [rdi].GGUF_STREAM_CTX.pBuffer_, rax
    mov QWORD PTR [rdi].GGUF_STREAM_CTX.cbBuffer_, GGUF_STREAM_BUFFERSIZE
    
    ; Success
    mov rax, 1
    jmp init_done
    
init_failed:
    xor rax, rax
    
init_done:
    add rsp, 28h
    pop rdi
    pop rbx
    ret
GGUF_StreamInit ENDP

; ------------------------------------------------------------------------------
; GGUF_StreamFree: Cleanup buffered reader
; RCX = pCtx
; ------------------------------------------------------------------------------
GGUF_StreamFree PROC FRAME
    .PUSHREG RBX
    sub rsp, 20h
    .ALLOCSTACK 20h
    .ENDPROLOG
    
    mov rbx, rcx
    
    ; Free buffer if allocated
    mov rcx, [rbx].GGUF_STREAM_CTX.pBuffer_
    test rcx, rcx
    jz free_done
    
    xor edx, edx                    ; dwSize = 0
    mov r8d, MEM_RELEASE
    call VirtualFree
    
    mov QWORD PTR [rbx].GGUF_STREAM_CTX.pBuffer_, 0
    
free_done:
    add rsp, 20h
    pop rbx
    ret
GGUF_StreamFree ENDP

END
