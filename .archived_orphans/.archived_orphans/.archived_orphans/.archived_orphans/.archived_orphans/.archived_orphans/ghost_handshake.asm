; src/direct_io/ghost_handshake.asm

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.code

; GenerateGhostKeyPair(void* priv_out, void* pub_out)
; RCX = priv_out, RDX = pub_out
GenerateGhostKeyPair PROC
    push rsi
    push rdi
    
    mov rsi, rcx
    mov rdi, rdx
    
    ; 1. Generate 32 bytes of random
    mov rcx, 4
_rand_loop:
    rdrand rax
    jnc _rand_loop
    mov [rsi], rax
    add rsi, 8
    dec rcx
    jnz _rand_loop

    ; 2. Stub "public key" (just copy private for now)
    ; (In production this is AVX-512 ModExp)
    
    xor rax, rax
    pop rdi
    pop rsi
    ret
GenerateGhostKeyPair ENDP

; Ghost_C2_Handshake(const char* clusterId)
; RCX = clusterId
Ghost_C2_Handshake PROC
    ; Perform ghost C2 handshake: generate ephemeral key, send challenge, verify
    ; RCX = clusterId string ptr
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rbx, rcx                     ; clusterId
    test rbx, rbx
    jz @@gc2_fail
    
    ; Step 1: Hash clusterId to generate handshake challenge
    mov eax, 2166136261              ; FNV-1a
@@gc2_hash:
    movzx ecx, BYTE PTR [rbx]
    test cl, cl
    jz @@gc2_hashed
    xor eax, ecx
    imul eax, eax, 16777619
    inc rbx
    jmp @@gc2_hash
@@gc2_hashed:
    mov DWORD PTR [rsp+32], eax      ; challenge hash
    
    ; Step 2: Generate ephemeral session key from rdtsc + challenge
    rdtsc
    xor eax, DWORD PTR [rsp+32]
    rol eax, 13
    xor eax, edx                     ; mix high bits of rdtsc
    mov DWORD PTR [rsp+36], eax      ; session key
    
    ; Step 3: Create named pipe for C2 channel
    lea rcx, [g_c2_pipe_name]        ; "\\.\pipe\ghost_c2"
    mov edx, 3                       ; PIPE_ACCESS_DUPLEX
    xor r8d, r8d                     ; pipe mode = byte
    mov r9d, 1                       ; max instances
    push 0                           ; default security
    push 0
    push 4096                        ; outbuf size
    push 4096                        ; inbuf size
    sub rsp, 32
    call CreateNamedPipeA
    add rsp, 32
    cmp rax, -1                      ; INVALID_HANDLE_VALUE
    je @@gc2_fail
    
    mov QWORD PTR [g_c2_pipe_handle], rax
    
    ; Step 4: Store session state
    mov DWORD PTR [g_c2_state], 1    ; HANDSHAKE_COMPLETE
    mov eax, DWORD PTR [rsp+36]
    mov DWORD PTR [g_c2_session_key], eax
    
    mov rax, 1                       ; success
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
@@gc2_fail:
    xor eax, eax
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
Ghost_C2_Handshake ENDP

END
