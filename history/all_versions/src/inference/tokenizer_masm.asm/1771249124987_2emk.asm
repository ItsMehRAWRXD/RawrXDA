; ================================================================
; tokenizer_masm.asm — Pure MASM UTF-8 BPE Tokenizer
; SSE4.2 PCMPESTRI for high-speed token boundary detection
; Assemble: ml64 /c tokenizer_masm.asm
; ================================================================

option casemap:none

.data
ALIGN 16

; Delimiter set for PCMPESTRI (whitespace + punctuation)
; Space, Tab, LF, CR, ., ,, ;, :, !, ?, (, ), [, ], {, }
delimiters db ' ', 9, 10, 13, '.', ',', ';', ':', '!', '?', '(', ')', '[', ']', '{', '}'
delimiter_count equ 16

; Special tokens
EOS_TOKEN equ 0FFFFFFFFh
BOS_TOKEN equ 1
UNK_TOKEN equ 0

.code
ALIGN 16

; ================================================================
; RawrXD_Tokenize_SSE42
; ================================================================
; High-performance UTF-8 tokenizer using SSE4.2 string instructions
;
; RCX = input string pointer (UTF-8)
; RDX = input length in bytes
; R8  = output token ID buffer (dword array)
; R9  = merge table base pointer
;       Layout: [4-byte hash] [4-byte token_id] per entry
;       Table size: 64K entries (512KB)
;
; Returns: RAX = number of tokens emitted
; ================================================================
RawrXD_Tokenize_SSE42 PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 64
    .allocstack 64
    .endprolog

    mov r12, rcx        ; input pointer
    mov r13, rdx        ; remaining length
    mov r14, r8         ; output token buffer
    mov r15, r9         ; merge table base
    xor ebx, ebx        ; token count = 0

    ; Emit BOS token
    mov dword ptr [r14], BOS_TOKEN
    add r14, 4
    inc ebx

    ; Load delimiter set into XMM1
    lea rax, delimiters
    movdqu xmm1, [rax]

    cmp r13, 0
    jle tokenize_done

tokenize_loop:
    cmp r13, 0
    jle tokenize_eos

    ; Skip leading whitespace
skip_ws:
    cmp r13, 0
    jle tokenize_eos
    movzx eax, byte ptr [r12]
    cmp al, ' '
    je advance_skip
    cmp al, 9
    je advance_skip
    cmp al, 10
    je advance_skip
    cmp al, 13
    je advance_skip
    jmp find_token_end

advance_skip:
    inc r12
    dec r13
    jmp skip_ws

find_token_end:
    ; Save token start
    mov [rbp-8], r12    ; token_start

    ; Use PCMPESTRI to find next delimiter
    cmp r13, 16
    jb short_scan

    ; Load 16 bytes of input
    movdqu xmm0, [r12]

    ; PCMPESTRI: find first byte in XMM0 that matches any byte in XMM1
    ; imm8 = 0000_00_00b = unsigned bytes, equal any, positive polarity, least significant
    mov eax, delimiter_count    ; EAX = length of delimiter set
    mov edx, 16                 ; EDX = length of input (max 16)
    cmp r13, 16
    jge use_16
    mov edx, r13d
use_16:
    pcmpestri xmm1, xmm0, 00000000b

    ; ECX = index of first match (0-15), or 16 if no match
    jc found_delimiter      ; CF=1 means match found
    jz end_of_input         ; ZF=1 means end of input string

    ; No delimiter found in 16 bytes — advance past all 16
    add r12, 16
    sub r13, 16
    jmp find_token_end

found_delimiter:
    ; ECX = offset of delimiter from current position
    cmp ecx, 0
    je skip_single_delim    ; delimiter at position 0 = skip it

    ; Token is [token_start .. r12 + ecx)
    mov rsi, [rbp-8]        ; token_start
    mov rdi, r12
    add rdi, rcx            ; token_end
    sub rdi, rsi            ; token_length

    ; Compute token hash and look up in merge table
    mov rcx, rsi            ; string ptr
    mov rdx, rdi            ; string len
    call compute_token_hash

    ; Look up hash in merge table
    ; RAX = hash, R15 = table base
    and eax, 0FFFFh         ; 64K entry table
    shl eax, 3              ; 8 bytes per entry
    add rax, r15
    mov eax, [rax + 4]      ; token_id

    ; Store token
    mov [r14], eax
    add r14, 4
    inc ebx

    ; Advance past token + delimiter
    mov rax, [rbp-8]
    add rax, rdi
    inc rax                 ; skip delimiter
    sub r13, rax
    add r13, r12
    mov r12, rax
    jmp tokenize_loop

skip_single_delim:
    inc r12
    dec r13
    jmp tokenize_loop

short_scan:
    ; Handle < 16 bytes remaining
    mov rsi, [rbp-8]        ; token_start = current pos
    xor ecx, ecx

short_loop:
    cmp ecx, r13d
    jge short_emit
    movzx eax, byte ptr [r12 + rcx]
    cmp al, ' '
    je short_emit
    cmp al, 9
    je short_emit
    cmp al, 10
    je short_emit
    cmp al, 13
    je short_emit
    inc ecx
    jmp short_loop

short_emit:
    cmp ecx, 0
    je short_skip

    ; Hash and emit token
    mov rdx, rcx            ; length
    mov rcx, r12            ; string
    push r12
    push r13
    call compute_token_hash
    pop r13
    pop r12

    and eax, 0FFFFh
    shl eax, 3
    add rax, r15
    mov eax, [rax + 4]

    mov [r14], eax
    add r14, 4
    inc ebx

    movzx ecx, byte ptr [r12]  ; restore ecx approximation
    add r12, rcx
    sub r13, rcx
    jmp tokenize_loop

short_skip:
    inc r12
    dec r13
    jmp tokenize_loop

end_of_input:
    ; Process remaining bytes
    mov rsi, [rbp-8]
    mov rdi, r12
    add rdi, r13
    sub rdi, rsi
    cmp rdi, 0
    jle tokenize_eos

    ; Hash remaining token
    mov rcx, rsi
    mov rdx, rdi
    call compute_token_hash
    and eax, 0FFFFh
    shl eax, 3
    add rax, r15
    mov eax, [rax + 4]
    mov [r14], eax
    add r14, 4
    inc ebx

tokenize_eos:
    ; Emit EOS token
    mov dword ptr [r14], EOS_TOKEN
    inc ebx

tokenize_done:
    mov eax, ebx            ; return token count

    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

RawrXD_Tokenize_SSE42 ENDP

; ================================================================
; compute_token_hash — FNV-1a hash for token strings
; ================================================================
; RCX = string pointer
; RDX = string length
; Returns: EAX = 32-bit hash
; ================================================================
compute_token_hash PROC
    mov rax, 2166136261     ; FNV offset basis (32-bit)
    mov r8, 16777619        ; FNV prime

hash_loop:
    cmp rdx, 0
    jle hash_done
    movzx r9d, byte ptr [rcx]
    xor eax, r9d
    imul rax, r8
    inc rcx
    dec rdx
    jmp hash_loop

hash_done:
    ret
compute_token_hash ENDP

; ================================================================
; Public exports
; ================================================================
PUBLIC RawrXD_Tokenize_SSE42
PUBLIC compute_token_hash

END
