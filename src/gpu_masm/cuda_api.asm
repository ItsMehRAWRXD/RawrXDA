; CUDA interop (MASM x64)
option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; CUDA API externs
extern cuInit:proc
extern cuDeviceGetCount:proc
extern cuCtxCreate:proc
extern cuCtxDestroy:proc
extern cuMemAlloc:proc
extern cuMemFree:proc
extern cuMemcpyHtoD:proc
extern cuMemcpyDtoH:proc

PUBLIC cuda_init
PUBLIC cuda_shutdown
PUBLIC cuda_get_device_count
PUBLIC cuda_alloc
PUBLIC cuda_free
PUBLIC cuda_memcpy_host_to_device
PUBLIC cuda_memcpy_device_to_host

.data
    g_cudaInitialized dd 0
    g_cudaDeviceCount dd 0
    g_cudaContext dq 0

.code
cuda_init PROC
    ; Initialize CUDA driver API
    sub rsp, 28h
    
    ; Check if already initialized
    cmp g_cudaInitialized, 1
    je @@already_init
    
    ; Initialize CUDA
    xor ecx, ecx  ; CU_CTX_SCHED_AUTO
    call cuInit
    test eax, eax
    jnz @@init_failed
    
    ; Get device count
    lea rcx, g_cudaDeviceCount
    call cuDeviceGetCount
    test eax, eax
    jnz @@init_failed
    
    ; Check if we have devices
    cmp g_cudaDeviceCount, 0
    je @@no_devices
    
    ; Create context on device 0
    xor ecx, ecx  ; device 0
    xor edx, edx  ; flags
    lea r8, g_cudaContext
    call cuCtxCreate
    test eax, eax
    jnz @@init_failed
    
    ; Success
    mov g_cudaInitialized, 1
    xor rax, rax
    jmp @@done
    
@@already_init:
    xor rax, rax
    jmp @@done
    
@@no_devices:
    mov rax, 1  ; No devices
    jmp @@done
    
@@init_failed:
    mov rax, 2  ; Init failed
    
@@done:
    add rsp, 28h
    ret
cuda_init ENDP

cuda_shutdown PROC
    ; Shutdown CUDA
    sub rsp, 28h
    
    cmp g_cudaInitialized, 0
    je @@not_init
    
    ; Destroy context
    mov rcx, g_cudaContext
    test rcx, rcx
    jz @@no_ctx
    call cuCtxDestroy
    
@@no_ctx:
    mov g_cudaInitialized, 0
    
@@not_init:
    xor rax, rax
    add rsp, 28h
    ret
cuda_shutdown ENDP

cuda_get_device_count PROC
    mov eax, g_cudaDeviceCount
    ret
cuda_get_device_count ENDP

cuda_alloc PROC
    ; RCX = size, RDX = device ptr out
    sub rsp, 28h
    
    cmp g_cudaInitialized, 0
    je @@not_init
    
    call cuMemAlloc
    jmp @@done
    
@@not_init:
    mov rax, 1
    
@@done:
    add rsp, 28h
    ret
cuda_alloc ENDP

cuda_free PROC
    ; RCX = device ptr
    sub rsp, 28h
    
    cmp g_cudaInitialized, 0
    je @@not_init
    
    call cuMemFree
    jmp @@done
    
@@not_init:
    mov rax, 1
    
@@done:
    add rsp, 28h
    ret
cuda_free ENDP

cuda_memcpy_host_to_device PROC
    ; RCX = dst device, RDX = src host, R8 = size
    sub rsp, 28h
    
    cmp g_cudaInitialized, 0
    je @@not_init
    
    call cuMemcpyHtoD
    jmp @@done
    
@@not_init:
    mov rax, 1
    
@@done:
    add rsp, 28h
    ret
cuda_memcpy_host_to_device ENDP

cuda_memcpy_device_to_host PROC
    ; RCX = dst host, RDX = src device, R8 = size
    sub rsp, 28h
    
    cmp g_cudaInitialized, 0
    je @@not_init
    
    call cuMemcpyDtoH
    jmp @@done
    
@@not_init:
    mov rax, 1
    
@@done:
    add rsp, 28h
    ret
cuda_memcpy_device_to_host ENDP

END
