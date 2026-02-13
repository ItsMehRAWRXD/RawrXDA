; model-llm-harvester.asm
; A 100% native x64 Windows executable (Zero-CRT) that generates
; LLM dead-weights based on a key-hash for reproducible output.

EXTERN ExitProcess:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC

.data
    ; Global handles and buffers
    stdout_handle   dq 0
    output_buffer   db 0, 0, 0, 0 ; 4-byte buffer for a single weight
    
    ; Placeholder for hardcoded or parsed input key
    input_key_text  db "A secure and reproducible key.", 0
    key_length      equ $ - input_key_text - 1
    
    ; The initial seed value (will be overwritten by the key hash)
    seed_dq         dq 0

.code

; -----------------------------------------------------------------
; PRINT Macro: Writes a buffer to standard output.
; Optimized: Redundant push/pop rbp removed for minimal size.
; -----------------------------------------------------------------
PRINT MACRO text, text_size
    ; Set up the stack frame for the WriteFile call (Windows x64 ABI)
    sub rsp, 32                 ; Allocate shadow space (mandatory)

    mov rcx, [stdout_handle]    ; 1st arg: hFile
    lea rdx, [text]             ; 2nd arg: lpBuffer
    mov r8, text_size           ; 3rd arg: nNumberOfBytesToWrite
    mov r9, 0                   ; 4th arg: lpNumberOfBytesWritten (NULL)
    
    call WriteFile
    
    add rsp, 32
ENDM

; -----------------------------------------------------------------
; PRNG: XOR-Shift 32-bit pseudo-random number generator
; State is held and updated in R13D. Returns next value in EAX.
; -----------------------------------------------------------------
PRNG PROC
    mov eax, r13d       ; x = seed
    
    ; x ^= x << 13
    shl eax, 13
    xor r13d, eax       
    
    ; x ^= x >> 17
    mov eax, r13d       
    shr eax, 17
    xor r13d, eax       
    
    ; x ^= x << 5
    mov eax, r13d       
    shl eax, 5
    xor r13d, eax       
    
    mov eax, r13d       ; EAX now holds the new seed/random number
    ret
PRNG ENDP


main PROC
    ; --- 1. Zero-CRT Setup ---
    ; GetStdHandle(STD_OUTPUT_HANDLE = -11)
    sub rsp, 32
    mov ecx, -11
    call GetStdHandle
    mov [stdout_handle], rax
    add rsp, 32

    ; --- 2. Key Hashing (Simulated) ---
    ; Calculate the key-hash (ECX contains the final 32-bit hash)
    ; Loop over the input_key_text to perform a simple XOR hash (example)
    mov ecx, 0x1A2B3C4D ; Initial hash value
    mov rsi, offset input_key_text
    mov rdi, key_length
    
hash_loop:
    cmp rdi, 0
    je end_hash_loop
    
    xor cl, [rsi]       ; Hash operation
    inc rsi
    dec rdi
    jmp hash_loop
    
end_hash_loop:
    ; ECX now holds the final 32-bit key-hash.

    ; ----------------------------------------------------
    ; FINAL POLISH 1: Seed Reproducibility
    ; Copy the key-hash (ECX) into the PRNG seed register (R13D).
    ; This ensures deterministic and reproducible weight generation.
    ; ----------------------------------------------------
    mov r13d, ecx        ; key-hash -> seed (R13D)

    ; --- 3. Dead-Weight Generation Loop ---
    ; Generate a total of 256 weights (4 bytes each = 1024 bytes)
    mov rdi, 256         ; Total number of 4-byte weights to generate

weight_loop:
    ; Generate 32-bit PRNG value (in EAX)
    call PRNG
    
    ; Simulate LLM weight logic: Divide the random integer by 7.
    ; The result (EAX) is written as a 4-byte (float/int) weight.
    mov edx, 0           ; Clear EDX for 64-bit dividend in EDX:EAX
    mov ebx, 7           ; Divisor
    idiv ebx             ; EAX = EAX / 7
    
    ; Write the 4-byte weight (in EAX) to the output stream
    mov [output_buffer], eax
    PRINT output_buffer, 4
    
    dec rdi
    jnz weight_loop

    ; --- 4. ExitProcess ---
    sub rsp, 32
    mov ecx, 0           ; Exit code 0 (success)
    call ExitProcess
main ENDP

END
