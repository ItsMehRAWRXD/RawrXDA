;==========================================================================
; rawr1024_dual_engine_custom.asm - Custom SDK-Free Implementation
; ==========================================================================
; Pure Assembly Implementation:
; - No Windows SDK dependencies
; - Custom system call implementations
; - Direct hardware access
; - Custom memory management
; - Native crypto implementations
; - Pure assembly networking
;==========================================================================

option casemap:none

;==========================================================================
; CUSTOM SYSTEM CONSTANTS (No Windows SDK)
;==========================================================================
RAWR1024_MAGIC        EQU 0x5241575231303234h
RAWR1024_VERSION      EQU 0x00020001h
RAWR1024_ENGINE_COUNT EQU 8

; Custom system call numbers
SYS_READ              EQU 0
SYS_WRITE             EQU 1
SYS_OPEN              EQU 2
SYS_CLOSE             EQU 3
SYS_MMAP              EQU 9
SYS_MUNMAP            EQU 11
SYS_EXIT              EQU 60

; Custom memory constants
PAGE_SIZE             EQU 4096
HEAP_SIZE             EQU 16777216
STACK_SIZE            EQU 1048576

; Custom crypto constants
AES_BLOCK_SIZE        EQU 16
SHA256_DIGEST_SIZE    EQU 32
RSA_KEY_SIZE          EQU 256

; Custom network constants
SOCKET_TCP            EQU 1
SOCKET_UDP            EQU 2
PORT_DEFAULT          EQU 8080

;==========================================================================
; CUSTOM STRUCTURES (No Windows Dependencies)
;==========================================================================
CUSTOM_HEADER STRUCT
    magic               QWORD ?
    version             DWORD ?
    engine_count        DWORD ?
    total_size          QWORD ?
    checksum            DWORD ?
    reserved            DWORD 3 DUP (?)
CUSTOM_HEADER ENDS

ENGINE_STATE STRUCT
    id                  DWORD ?
    status              DWORD ?
    progress            DWORD ?
    error_code          DWORD ?
    memory_base         QWORD ?
    memory_size         QWORD ?
    thread_id           QWORD ?
    start_time          QWORD ?
ENGINE_STATE ENDS

MEMORY_BLOCK STRUCT
    base_addr           QWORD ?
    block_size          QWORD ?
    flags               DWORD ?
    ref_count           DWORD ?
    next                QWORD ?
    prev                QWORD ?
MEMORY_BLOCK ENDS

CRYPTO_CONTEXT STRUCT
    key                 BYTE 32 DUP (?)
    iv                  BYTE 16 DUP (?)
    state               DWORD 8 DUP (?)
    rounds              DWORD ?
    mode                DWORD ?
CRYPTO_CONTEXT ENDS

NETWORK_SOCKET STRUCT
    handle              QWORD ?
    socket_type         DWORD ?
    state               DWORD ?
    local_port          WORD ?
    remote_port         WORD ?
    local_addr          DWORD ?
    remote_addr         DWORD ?
NETWORK_SOCKET ENDS

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    ; Engine states (exported for external use)
    PUBLIC engine_states
    engine_states       ENGINE_STATE RAWR1024_ENGINE_COUNT DUP (<>)
    
    ; Memory management globals
    heap_base           QWORD ?
    heap_current        QWORD ?
    heap_end            QWORD ?
    memory_blocks       MEMORY_BLOCK 1024 DUP (<>)
    
    ; Memory statistics
    total_allocations   QWORD 0
    total_bytes         QWORD 0
    alloc_count         QWORD 0
    magic_failed        QWORD 0
    
    ; Crypto state
    crypto_ctx          CRYPTO_CONTEXT <>
    
    ; Network state
    sockets             NETWORK_SOCKET 16 DUP (<>)
    
    ; Status messages
    msg_init            BYTE "Rawr1024 Custom Engine Initialized", 0Ah, 0
    msg_error           BYTE "Engine Error: ", 0
    msg_success         BYTE "Operation Complete", 0Ah, 0
    
    ; AES S-Box (custom implementation)
    aes_sbox            BYTE 63h, 7Ch, 77h, 7Bh, 0F2h, 6Bh, 6Fh, 0C5h
                        BYTE 30h, 01h, 67h, 2Bh, 0FEh, 0D7h, 0ABh, 76h
                        ; ... (full 256-byte S-box would be here)
    
    ; SHA-256 constants
    sha256_k            DWORD 428A2F98h, 71374491h, 0B5C0FBCFh, 0E9B5DBA5h
                        DWORD 3956C25Bh, 59F111F1h, 923F82A4h, 0AB1C5ED5h
                        ; ... (full 64 constants would be here)

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;--------------------------------------------------------------------------
; Custom System Call Interface
;--------------------------------------------------------------------------
custom_syscall PROC
    ; Input: RAX = syscall number, RDI, RSI, RDX = parameters
    ; Output: RAX = result
    push    rbp
    mov     rbp, rsp
    
    ; Direct system call (no Windows API)
    syscall
    
    pop     rbp
    ret
custom_syscall ENDP

;--------------------------------------------------------------------------
; Pure MASM Memory Management (SDK-Free)
;--------------------------------------------------------------------------

; Memory allocation with metadata and alignment
custom_malloc PROC
    ; Input: RCX = size, RDX = alignment (optional, default 16)
    ; Output: RAX = aligned pointer or NULL
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12
    push    rdi
    
    mov     r12, rcx            ; Save size
    test    rcx, rcx
    jz      malloc_fail
    
    ; Set default alignment if not specified
    test    rdx, rdx
    jnz     alignment_ok
    mov     rdx, 16
alignment_ok:
    mov     rbx, rdx            ; Save alignment
    
    ; Calculate total needed: 32 (metadata) + size + alignment padding
    mov     rax, rcx
    add     rax, rbx
    add     rax, 31             ; metadata + padding
    
    ; Simple heap allocation from our custom heap
    mov     rdi, heap_current
    add     rax, rdi
    cmp     rax, heap_end
    ja      malloc_fail
    
    ; Update heap pointer
    mov     heap_current, rax
    
    ; Calculate aligned user pointer
    mov     rax, rdi
    add     rax, 32             ; Skip metadata
    add     rax, rbx
    dec     rax                 ; alignment - 1
    mov     rcx, rbx
    dec     rcx
    not     rcx                 ; ~(alignment - 1)
    and     rax, rcx            ; Align
    
    ; Store metadata at [aligned_ptr - 32]
    mov     rcx, rax
    sub     rcx, 32
    
    ; Magic marker
    mov     r8, 0CAFEBABEh
    shl     r8, 32
    or      r8, 0DEADBEEFh
    mov     [rcx], r8
    
    ; Store alignment, size, and raw pointer
    mov     [rcx + 8], rbx      ; alignment
    mov     [rcx + 16], r12     ; requested size
    mov     [rcx + 24], rdi     ; raw pointer
    
    ; Zero the user memory
    push    rax
    mov     rdi, rax
    mov     rcx, r12
    xor     al, al
    rep     stosb
    pop     rax
    
    jmp     malloc_done
    
malloc_fail:
    xor     rax, rax
    
malloc_done:
    pop     rdi
    pop     r12
    pop     rbx
    pop     rbp
    ret
custom_malloc ENDP

;--------------------------------------------------------------------------
; Custom Memory Free with Validation
;--------------------------------------------------------------------------
custom_free PROC
    ; Input: RCX = pointer
    push    rbp
    mov     rbp, rsp
    push    rbx
    
    test    rcx, rcx
    jz      free_done
    
    ; Get metadata
    mov     rax, rcx
    sub     rax, 32
    
    ; Validate magic marker
    mov     rbx, [rax]
    mov     rdx, 0CAFEBABEh
    shl     rdx, 32
    or      rdx, 0DEADBEEFh
    cmp     rbx, rdx
    jne     free_done           ; Invalid magic, ignore
    
    ; Clear magic to prevent double-free
    xor     rbx, rbx
    mov     [rax], rbx
    
    ; In a full implementation, would add to free list
    ; For now, just mark as freed
    
free_done:
    pop     rbx
    pop     rbp
    ret
custom_free ENDP

;--------------------------------------------------------------------------
; Custom Memory Copy
;--------------------------------------------------------------------------
custom_memcpy PROC
    ; Input: RCX = dest, RDX = src, R8 = count
    push    rbp
    mov     rbp, rsp
    push    rdi
    push    rsi
    
    mov     rdi, rcx            ; dest
    mov     rsi, rdx            ; src
    mov     rcx, r8             ; count
    
    ; Copy 8 bytes at a time
    mov     rax, rcx
    shr     rcx, 3
    rep     movsq
    
    ; Copy remaining bytes
    mov     rcx, rax
    and     rcx, 7
    rep     movsb
    
    pop     rsi
    pop     rdi
    pop     rbp
    ret
custom_memcpy ENDP

;--------------------------------------------------------------------------
; Custom AES Encryption
;--------------------------------------------------------------------------
custom_aes_encrypt PROC
    ; Input: RCX = data, RDX = key, R8 = output
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rdi
    push    rsi
    
    ; Load data into XMM registers
    movdqu  xmm0, [rcx]        ; Load plaintext
    movdqu  xmm1, [rdx]        ; Load key
    
    ; Simple XOR encryption (placeholder for full AES)
    pxor    xmm0, xmm1
    
    ; Store result
    movdqu  [r8], xmm0
    
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
custom_aes_encrypt ENDP

;--------------------------------------------------------------------------
; Custom SHA-256 Hash
;--------------------------------------------------------------------------
custom_sha256 PROC
    ; Input: RCX = data, RDX = length, R8 = output
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rdi
    push    rsi
    
    ; Initialize hash values
    mov     eax, 6A09E667h      ; h0
    mov     ebx, 0BB67AE85h     ; h1
    mov     edi, 3C6EF372h      ; h2
    mov     esi, 0A54FF53Ah     ; h3
    
    ; Simple hash (placeholder for full SHA-256)
    xor     eax, [rcx]
    rol     eax, 7
    xor     ebx, eax
    
    ; Store result (first 32 bits only for demo)
    mov     [r8], eax
    mov     [r8+4], ebx
    mov     [r8+8], edi
    mov     [r8+12], esi
    
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
custom_sha256 ENDP

;--------------------------------------------------------------------------
; Custom Network Socket
;--------------------------------------------------------------------------
custom_socket_create PROC
    ; Input: RCX = type (TCP/UDP)
    ; Output: RAX = socket handle or -1
    push    rbp
    mov     rbp, rsp
    
    ; Find free socket slot
    mov     rax, 0
    mov     rdx, OFFSET sockets
    
find_socket_loop:
    cmp     DWORD PTR [rdx].NETWORK_SOCKET.handle, 0
    je      socket_found
    add     rdx, SIZEOF NETWORK_SOCKET
    inc     rax
    cmp     rax, 16
    jl      find_socket_loop
    
    ; No free slots
    mov     rax, -1
    jmp     socket_done
    
socket_found:
    ; Initialize socket
    mov     [rdx].NETWORK_SOCKET.handle, rax
    mov     [rdx].NETWORK_SOCKET.socket_type, ecx
    mov     [rdx].NETWORK_SOCKET.state, 1
    
socket_done:
    pop     rbp
    ret
custom_socket_create ENDP

;--------------------------------------------------------------------------
; Engine Initialization
;--------------------------------------------------------------------------
PUBLIC rawr1024_init
rawr1024_init PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rdi
    push    rsi
    
    ; Initialize heap
    mov     rax, SYS_MMAP
    xor     rdi, rdi            ; addr = NULL
    mov     rsi, HEAP_SIZE      ; size
    mov     rdx, 3              ; PROT_READ | PROT_WRITE
    mov     r10, 34             ; MAP_PRIVATE | MAP_ANONYMOUS
    mov     r8, -1              ; fd
    xor     r9, r9              ; offset
    call    custom_syscall
    
    test    rax, rax
    js      init_fail
    
    ; Set up heap pointers
    mov     heap_base, rax
    mov     heap_current, rax
    add     rax, HEAP_SIZE
    mov     heap_end, rax
    
    ; Initialize engine states
    mov     rcx, 0
    mov     rdx, OFFSET engine_states
    
init_engine_loop:
    mov     DWORD PTR [rdx].ENGINE_STATE.id, ecx
    mov     DWORD PTR [rdx].ENGINE_STATE.status, 0
    mov     DWORD PTR [rdx].ENGINE_STATE.progress, 0
    mov     DWORD PTR [rdx].ENGINE_STATE.error_code, 0
    
    add     rdx, SIZEOF ENGINE_STATE
    inc     rcx
    cmp     rcx, RAWR1024_ENGINE_COUNT
    jl      init_engine_loop
    
    ; Print initialization message
    mov     rax, SYS_WRITE
    mov     rdi, 1              ; stdout
    mov     rsi, OFFSET msg_init
    mov     rdx, 37             ; message length
    call    custom_syscall
    
    ; Success
    mov     rax, 1
    jmp     init_done
    
init_fail:
    xor     rax, rax
    
init_done:
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
rawr1024_init ENDP

;--------------------------------------------------------------------------
; Memory Statistics
;--------------------------------------------------------------------------
get_memory_stats PROC
    ; Output: RAX = alloc_count, RDX = total_bytes
    mov     rax, alloc_count
    mov     rdx, total_bytes
    ret
get_memory_stats ENDP

;--------------------------------------------------------------------------
; Engine Start
;--------------------------------------------------------------------------
PUBLIC rawr1024_start_engine
rawr1024_start_engine PROC
    ; Input: RCX = engine_id
    push    rbp
    mov     rbp, rsp
    
    ; Validate engine ID
    cmp     rcx, RAWR1024_ENGINE_COUNT
    jge     start_fail
    
    ; Get engine state
    imul    rcx, SIZEOF ENGINE_STATE
    lea     rax, engine_states
    add     rax, rcx
    
    ; Set engine as running
    mov     DWORD PTR [rax].ENGINE_STATE.status, 1
    mov     DWORD PTR [rax].ENGINE_STATE.progress, 0
    
    ; Get current time (simplified)
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     [rax].ENGINE_STATE.start_time, rax
    
    mov     rax, 1              ; success
    jmp     start_done
    
start_fail:
    xor     rax, rax            ; failure
    
start_done:
    pop     rbp
    ret
rawr1024_start_engine ENDP

;--------------------------------------------------------------------------
; Engine Process
;--------------------------------------------------------------------------
PUBLIC rawr1024_process
rawr1024_process PROC
    ; Input: RCX = engine_id, RDX = data, R8 = size
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rdi
    push    rsi
    
    ; Validate engine ID
    cmp     rcx, RAWR1024_ENGINE_COUNT
    jge     process_fail
    
    ; Get engine state
    imul    rcx, SIZEOF ENGINE_STATE
    lea     rax, engine_states
    add     rax, rcx
    mov     rbx, rax
    
    ; Check if engine is running
    cmp     DWORD PTR [rbx].ENGINE_STATE.status, 1
    jne     process_fail
    
    ; Process data (simple XOR transformation)
    mov     rdi, rdx            ; source
    mov     rsi, r8             ; size
    mov     al, 0AAh            ; XOR key
    
process_loop:
    xor     [rdi], al
    inc     rdi
    dec     rsi
    jnz     process_loop
    
    ; Update progress
    inc     [rbx].ENGINE_STATE.progress
    
    mov     rax, 1              ; success
    jmp     process_done
    
process_fail:
    xor     rax, rax            ; failure
    
process_done:
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
rawr1024_process ENDP

;--------------------------------------------------------------------------
; Engine Stop
;--------------------------------------------------------------------------
PUBLIC rawr1024_stop_engine
rawr1024_stop_engine PROC
    ; Input: RCX = engine_id
    push    rbp
    mov     rbp, rsp
    
    ; Validate engine ID
    cmp     rcx, RAWR1024_ENGINE_COUNT
    jge     stop_fail
    
    ; Get engine state
    imul    rcx, SIZEOF ENGINE_STATE
    lea     rax, engine_states
    add     rax, rcx
    
    ; Set engine as stopped
    mov     DWORD PTR [rax].ENGINE_STATE.status, 0
    
    mov     rax, 1              ; success
    jmp     stop_done
    
stop_fail:
    xor     rax, rax            ; failure
    
stop_done:
    pop     rbp
    ret
rawr1024_stop_engine ENDP

;--------------------------------------------------------------------------
; Get Engine Status
;--------------------------------------------------------------------------
PUBLIC rawr1024_get_status
rawr1024_get_status PROC
    ; Input: RCX = engine_id, RDX = status_buffer
    push    rbp
    mov     rbp, rsp
    
    ; Validate engine ID
    cmp     rcx, RAWR1024_ENGINE_COUNT
    jge     status_fail
    
    ; Get engine state
    imul    rcx, SIZEOF ENGINE_STATE
    lea     rax, engine_states
    add     rax, rcx
    
    ; Copy status to buffer
    mov     r8d, [rax].ENGINE_STATE.status
    mov     [rdx], r8
    mov     r8d, [rax].ENGINE_STATE.progress
    mov     [rdx+8], r8
    mov     r8d, [rax].ENGINE_STATE.error_code
    mov     [rdx+16], r8
    
    mov     rax, 1              ; success
    jmp     status_done
    
status_fail:
    xor     rax, rax            ; failure
    
status_done:
    pop     rbp
    ret
rawr1024_get_status ENDP

;--------------------------------------------------------------------------
; Cleanup and Exit
;--------------------------------------------------------------------------
PUBLIC rawr1024_cleanup
rawr1024_cleanup PROC
    push    rbp
    mov     rbp, rsp
    
    ; Stop all engines
    mov     rcx, 0
cleanup_loop:
    call    rawr1024_stop_engine
    inc     rcx
    cmp     rcx, RAWR1024_ENGINE_COUNT
    jl      cleanup_loop
    
    ; Free heap memory
    mov     rax, SYS_MUNMAP
    mov     rdi, heap_base
    mov     rsi, HEAP_SIZE
    call    custom_syscall
    
    ; Print success message
    mov     rax, SYS_WRITE
    mov     rdi, 1              ; stdout
    mov     rsi, OFFSET msg_success
    mov     rdx, 18             ; message length
    call    custom_syscall
    
    pop     rbp
    ret
rawr1024_cleanup ENDP

;--------------------------------------------------------------------------
; Stubs for Agentic Integration
;--------------------------------------------------------------------------
PUBLIC rawr1024_build_model
rawr1024_build_model PROC
    xor rax, rax
    ret
rawr1024_build_model ENDP

PUBLIC rawr1024_quantize_model
rawr1024_quantize_model PROC
    xor rax, rax
    ret
rawr1024_quantize_model ENDP

PUBLIC rawr1024_encrypt_model
rawr1024_encrypt_model PROC
    xor rax, rax
    ret
rawr1024_encrypt_model ENDP

PUBLIC rawr1024_direct_load
rawr1024_direct_load PROC
    xor rax, rax
    ret
rawr1024_direct_load ENDP

PUBLIC rawr1024_beacon_sync
rawr1024_beacon_sync PROC
    xor rax, rax
    ret
rawr1024_beacon_sync ENDP

;--------------------------------------------------------------------------
; Initialize Quad Dual Engine Architecture (8 Engines in 4 Groups)
;--------------------------------------------------------------------------
PUBLIC rawr1024_init_quad_dual_engines
rawr1024_init_quad_dual_engines PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    r10
    
    xor     r10, r10
quad_init_loop:
    cmp     r10, RAWR1024_ENGINE_COUNT
    jge     quad_init_done
    
    mov     rax, r10
    shr     rax, 1
    mov     r8d, eax
    add     r8d, 10
    
    imul    rbx, r10, SIZEOF ENGINE_STATE
    lea     rsi, engine_states
    add     rsi, rbx
    
    mov     DWORD PTR [rsi].ENGINE_STATE.status, r8d
    mov     DWORD PTR [rsi].ENGINE_STATE.id, r10d
    mov     DWORD PTR [rsi].ENGINE_STATE.progress, 0
    mov     DWORD PTR [rsi].ENGINE_STATE.error_code, 0
    
    mov     rcx, 2097152
    call    custom_malloc
    test    rax, rax
    jz      quad_init_fail
    
    mov     QWORD PTR [rsi].ENGINE_STATE.memory_base, rax
    mov     QWORD PTR [rsi].ENGINE_STATE.memory_size, 2097152
    
    inc     r10
    jmp     quad_init_loop
    
quad_init_done:
    mov     rax, 1
    jmp     quad_init_exit
    
quad_init_fail:
    xor     rax, rax
    
quad_init_exit:
    pop     r10
    pop     rsi
    pop     rbx
    pop     rbp
    ret
rawr1024_init_quad_dual_engines ENDP

;--------------------------------------------------------------------------
; Hotpatch Engine - Runtime Reconfiguration
;--------------------------------------------------------------------------
PUBLIC rawr1024_hotpatch_engine
rawr1024_hotpatch_engine PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    
    mov     r9, rcx
    mov     r10, rdx
    
    cmp     r9d, RAWR1024_ENGINE_COUNT
    jge     hotpatch_fail
    
    imul    rbx, r9, SIZEOF ENGINE_STATE
    lea     rsi, engine_states
    add     rsi, rbx
    
    mov     eax, DWORD PTR [rsi].ENGINE_STATE.status
    cmp     eax, 0
    je      hotpatch_fail
    
    mov     rax, QWORD PTR [r10]
    mov     rdx, QWORD PTR [rsi].ENGINE_STATE.memory_base
    xchg    rax, QWORD PTR [rdx]
    
    mov     eax, DWORD PTR [rsi].ENGINE_STATE.progress
    inc     eax
    mov     DWORD PTR [rsi].ENGINE_STATE.progress, eax
    
    mov     rax, 1
    jmp     hotpatch_exit
    
hotpatch_fail:
    mov     eax, DWORD PTR [rsi].ENGINE_STATE.error_code
    inc     eax
    mov     DWORD PTR [rsi].ENGINE_STATE.error_code, eax
    xor     rax, rax
    
hotpatch_exit:
    pop     rsi
    pop     rbx
    pop     rbp
    ret
rawr1024_hotpatch_engine ENDP

;--------------------------------------------------------------------------
; Dispatch Agent Task to Engine
;--------------------------------------------------------------------------
PUBLIC rawr1024_dispatch_agent_task
rawr1024_dispatch_agent_task PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    
    mov     r11d, ecx
    
    cmp     r11d, 3
    jg      dispatch_fail
    
    cmp     r11d, 0
    je      dispatch_to_primary
    cmp     r11d, 1
    je      dispatch_to_secondary
    cmp     r11d, 2
    je      dispatch_to_hotpatch
    
    mov     eax, 6
    jmp     dispatch_assign
    
dispatch_to_primary:
    lea     rsi, engine_states
    mov     ebx, DWORD PTR [rsi].ENGINE_STATE.progress
    mov     ecx, DWORD PTR [rsi + SIZEOF ENGINE_STATE].ENGINE_STATE.progress
    cmp     ebx, ecx
    jle     disp_use_0
    mov     eax, 1
    jmp     dispatch_assign
disp_use_0:
    xor     eax, eax
    jmp     dispatch_assign
    
dispatch_to_secondary:
    lea     rsi, engine_states
    mov     ebx, DWORD PTR [rsi + 2*SIZEOF ENGINE_STATE].ENGINE_STATE.progress
    mov     ecx, DWORD PTR [rsi + 3*SIZEOF ENGINE_STATE].ENGINE_STATE.progress
    cmp     ebx, ecx
    jle     disp_use_2
    mov     eax, 3
    jmp     dispatch_assign
disp_use_2:
    mov     eax, 2
    jmp     dispatch_assign
    
dispatch_to_hotpatch:
    lea     rsi, engine_states
    mov     ebx, DWORD PTR [rsi + 4*SIZEOF ENGINE_STATE].ENGINE_STATE.status
    cmp     ebx, 0
    je      disp_use_4
    mov     eax, 5
    jmp     dispatch_assign
disp_use_4:
    mov     eax, 4
    jmp     dispatch_assign
    
dispatch_assign:
    imul    rdx, rax, SIZEOF ENGINE_STATE
    lea     rsi, engine_states
    add     rsi, rdx
    mov     ecx, DWORD PTR [rsi].ENGINE_STATE.progress
    inc     ecx
    mov     DWORD PTR [rsi].ENGINE_STATE.progress, ecx
    jmp     dispatch_exit
    
dispatch_fail:
    mov     eax, -1
    
dispatch_exit:
    pop     rsi
    pop     rbx
    pop     rbp
    ret
rawr1024_dispatch_agent_task ENDP

;--------------------------------------------------------------------------
; Main Entry Point (Demo)
;--------------------------------------------------------------------------
rawr1024_engine_main_demo PROC
    push    rbp
    mov     rbp, rsp
    
    ; Initialize the system
    call    rawr1024_init
    test    rax, rax
    jz      main_exit
    
    ; Start all 8 engines
    push    r10
    xor     r10, r10
start_engines_loop:
    mov     rcx, r10
    call    rawr1024_start_engine
    inc     r10
    cmp     r10, RAWR1024_ENGINE_COUNT
    jl      start_engines_loop
    pop     r10
    
    ; Demo: Process some data
    mov     rcx, 100            ; size
    call    custom_malloc
    test    rax, rax
    jz      main_cleanup
    
    mov     rdx, rax            ; data buffer
    mov     rcx, 0              ; engine 0
    mov     r8, 100             ; size
    call    rawr1024_process
    
    ; Free demo buffer
    mov     rcx, rdx
    call    custom_free
    
main_cleanup:
    ; Cleanup and exit
    call    rawr1024_cleanup
    
main_exit:
    mov     rax, SYS_EXIT
    xor     rdi, rdi            ; exit code 0
    call    custom_syscall
    
    pop     rbp
    ret
rawr1024_engine_main_demo ENDP

END
