; GPU memory manager stubs (MASM x64)
option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


PUBLIC gpu_malloc
PUBLIC gpu_free
PUBLIC gpu_memcpy_host_to_device
PUBLIC gpu_memcpy_device_to_host

.code
gpu_malloc PROC
    ; Allocate GPU-accessible memory (host-side for unified memory model)
    ; RCX = size in bytes
    ; Returns: RAX = pointer to allocated memory, NULL on failure
    push rbx
    sub rsp, 32
    
    mov rbx, rcx                     ; save requested size
    test rcx, rcx
    jz @@gm_fail
    
    ; Align size up to 4KB page boundary
    add rcx, 0FFFh
    and rcx, 0FFFFFFFFFFFFF000h
    
    ; Allocate with VirtualAlloc for page-aligned memory
    xor ecx, ecx                     ; lpAddress = NULL
    mov rdx, rbx                     ; size
    add rdx, 0FFFh
    and rdx, 0FFFFFFFFFFFFF000h
    mov r8d, 3000h                   ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 04h                     ; PAGE_READWRITE
    call VirtualAlloc
    ; RAX = pointer or NULL
    
    add rsp, 32
    pop rbx
    ret
    
@@gm_fail:
    xor rax, rax
    add rsp, 32
    pop rbx
    ret
gpu_malloc ENDP

gpu_free PROC
    ; Free GPU-accessible memory
    ; RCX = pointer to free
    ; Returns: RAX = 0 on success
    sub rsp, 32
    
    test rcx, rcx
    jz @@gf_done
    
    ; VirtualFree(ptr, 0, MEM_RELEASE)
    xor edx, edx                     ; dwSize = 0 (entire region)
    mov r8d, 8000h                   ; MEM_RELEASE
    call VirtualFree
    
@@gf_done:
    xor rax, rax
    add rsp, 32
    ret
gpu_free ENDP

gpu_memcpy_host_to_device PROC
    ; rcx: dst, rdx: src, r8: size
    push rsi
    push rdi
    mov rdi, rcx
    mov rsi, rdx
    mov rcx, r8
    rep movsb
    pop rdi
    pop rsi
    xor rax, rax
    ret
gpu_memcpy_host_to_device ENDP

gpu_memcpy_device_to_host PROC
    ; rcx: dst, rdx: src, r8: size
    push rsi
    push rdi
    mov rdi, rcx
    mov rsi, rdx
    mov rcx, r8
    rep movsb
    pop rdi
    pop rsi
    xor rax, rax
    ret
gpu_memcpy_device_to_host ENDP

END
