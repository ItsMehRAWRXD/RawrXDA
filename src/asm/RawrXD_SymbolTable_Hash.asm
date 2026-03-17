; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_SymbolTable_Hash.asm - FNV-1a Hash Table for Ultra-Fast Symbol Lookup
; ═══════════════════════════════════════════════════════════════════════════════
; Part of RawrXD Win32IDE v14.7.x - Zero Bloat
; MASM64 optimized hash map for symbols
; ═══════════════════════════════════════════════════════════════════════════════

include RawrXD_Defs.inc

FNV_32_PRIME EQU 01000193h
FNV_32_OFFSET EQU 811C9DC5h

SymbolEntry STRUCT
    Hash        DWORD ?
    Address     QWORD ?
    NamePtr     QWORD ?
    Next        QWORD ?         ; For collision handling (Chained Hash Map)
SymbolEntry ENDS

.data
g_SymbolTable QWORD ?            ; Array of SymbolEntry Pointers
g_TableSize   QWORD 65536        ; 2^16 buckets for fast zero-overhead mask

.code

; ------------------------------------------------------------------------------
; RawrXD_FNV1a_Hash
; RCX = Pointer to String
; Returns: RAX = 32-bit Hash
; ------------------------------------------------------------------------------
RawrXD_FNV1a_Hash PROC
    push rbx
    xor rax, rax
    mov eax, FNV_32_OFFSET
    mov r8d, FNV_32_PRIME
    
    HashLoop:
        movzx rbx, byte ptr [rcx]
        test rbx, rbx
        jz Done
        xor rax, rbx
        mul r8
        inc rcx
        jmp HashLoop
    Done:
    pop rbx
    ret
RawrXD_FNV1a_Hash ENDS

; ------------------------------------------------------------------------------
; RawrXD_Symbol_Insert
; RCX = Name String
; RDX = VA
; Returns: RAX = Pointer to Entry or 0 if failed
; ------------------------------------------------------------------------------
RawrXD_Symbol_Insert PROC
    push rbx
    push rsi
    push rdi
    
    sub rsp, 40                 ; Shadow space + Alignment

    mov rsi, rcx                ; RSI = Name
    mov rdi, rdx                ; RDI = VA

    ; 1. Calculate Hash
    call RawrXD_FNV1a_Hash
    mov rbx, rax                ; RBX = Hash
    
    ; 2. Allocate SymbolEntry
    call GetProcessHeap
    mov rcx, rax
    mov rdx, HEAP_ZERO_MEMORY
    mov r8, sizeof SymbolEntry
    call HeapAlloc
    test rax, rax
    jz Failed

    ; 3. Fill entry
    mov [rax].SymbolEntry.Hash, ebx
    mov [rax].SymbolEntry.Address, rdi
    mov [rax].SymbolEntry.NamePtr, rsi

    ; 4. Insert into bucket (Simple Chaining)
    mov rdx, g_TableSize
    dec rdx
    and rbx, rdx                ; RBX = Bucket index (Hash & (Size-1))
    
    ; Logic to insert at g_SymbolTable[rbx] goes here...
    ; (Skeleton for performance focus)

Failed:
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
RawrXD_Symbol_Insert ENDS

END
