; ==============================================================================
; CUDA GPU Backend — MASM64 Dynamic Loader via nvcuda.dll
; Implements: Init, Device Detection, Memory Management, Kernel Dispatch
; Architecture: LoadLibraryW + GetProcAddress pattern (no static link to CUDA SDK)
; ==============================================================================

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


EXTERN LoadLibraryW : PROC
EXTERN GetProcAddress : PROC
EXTERN FreeLibrary : PROC

.data

; nvcuda.dll wide string
szNvCuda            DW 'n','v','c','u','d','a','.','d','l','l',0

; Function name strings for GetProcAddress
szCuInit            DB "cuInit", 0
szCuDeviceGetCount  DB "cuDeviceGetCount", 0
szCuDeviceGet       DB "cuDeviceGet", 0
szCuDeviceGetName   DB "cuDeviceGetName", 0
szCuDeviceGetAttr   DB "cuDeviceGetAttribute", 0
szCuCtxCreate       DB "cuCtxCreate_v2", 0
szCuCtxDestroy      DB "cuCtxDestroy_v2", 0
szCuMemAlloc        DB "cuMemAlloc_v2", 0
szCuMemFree         DB "cuMemFree_v2", 0
szCuMemcpyHtoD      DB "cuMemcpyHtoD_v2", 0
szCuMemcpyDtoH      DB "cuMemcpyDtoH_v2", 0
szCuLaunchKernel    DB "cuLaunchKernel", 0
szCuCtxSync         DB "cuCtxSynchronize", 0

ALIGN 8
; Function pointers (populated by cuda_init via GetProcAddress)
pfnCuInit           QWORD 0
pfnCuDeviceGetCount QWORD 0
pfnCuDeviceGet      QWORD 0
pfnCuDeviceGetName  QWORD 0
pfnCuDeviceGetAttr  QWORD 0
pfnCuCtxCreate      QWORD 0
pfnCuCtxDestroy     QWORD 0
pfnCuMemAlloc       QWORD 0
pfnCuMemFree        QWORD 0
pfnCuMemcpyHtoD     QWORD 0
pfnCuMemcpyDtoH     QWORD 0
pfnCuLaunchKernel   QWORD 0
pfnCuCtxSync        QWORD 0

; State
hNvCudaLib          QWORD 0
hCudaCtx            QWORD 0
cuda_device_id      DWORD 0
cuda_initialized    DWORD 0
cuda_device_count   DWORD 0

; Device info
cuda_device_name    BYTE 256 DUP(0)
cuda_compute_major  DWORD 0
cuda_compute_minor  DWORD 0
cuda_sm_count       DWORD 0

; CUDA attributes
CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR EQU 75
CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR EQU 76
CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT     EQU 16

.data?
ALIGN 8
cuda_alloc_tracker  QWORD 64 DUP(?)    ; Track allocations for cleanup

.code

; ==============================================================================
; cuda_init — Load nvcuda.dll, resolve CUDA Driver API, create context
; Returns: EAX = 0 success, -1 CUDA not available
; ==============================================================================
cuda_init PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 48
    .allocstack 48
    .endprolog

    ; Already initialized?
    cmp dword ptr [cuda_initialized], 1
    je ci_ok

    ; Load nvcuda.dll dynamically
    lea rcx, [szNvCuda]
    call LoadLibraryW
    test rax, rax
    jz ci_fail
    mov [hNvCudaLib], rax
    mov rbx, rax

    ; Resolve cuInit
    mov rcx, rbx
    lea rdx, [szCuInit]
    call GetProcAddress
    test rax, rax
    jz ci_cleanup_dll
    mov [pfnCuInit], rax

    ; Resolve cuDeviceGetCount
    mov rcx, rbx
    lea rdx, [szCuDeviceGetCount]
    call GetProcAddress
    test rax, rax
    jz ci_cleanup_dll
    mov [pfnCuDeviceGetCount], rax

    ; Resolve cuDeviceGet
    mov rcx, rbx
    lea rdx, [szCuDeviceGet]
    call GetProcAddress
    test rax, rax
    jz ci_cleanup_dll
    mov [pfnCuDeviceGet], rax

    ; Resolve cuDeviceGetName
    mov rcx, rbx
    lea rdx, [szCuDeviceGetName]
    call GetProcAddress
    mov [pfnCuDeviceGetName], rax

    ; Resolve cuDeviceGetAttribute
    mov rcx, rbx
    lea rdx, [szCuDeviceGetAttr]
    call GetProcAddress
    mov [pfnCuDeviceGetAttr], rax

    ; Resolve cuCtxCreate_v2
    mov rcx, rbx
    lea rdx, [szCuCtxCreate]
    call GetProcAddress
    test rax, rax
    jz ci_cleanup_dll
    mov [pfnCuCtxCreate], rax

    ; Resolve cuCtxDestroy_v2
    mov rcx, rbx
    lea rdx, [szCuCtxDestroy]
    call GetProcAddress
    mov [pfnCuCtxDestroy], rax

    ; Resolve cuMemAlloc_v2
    mov rcx, rbx
    lea rdx, [szCuMemAlloc]
    call GetProcAddress
    test rax, rax
    jz ci_cleanup_dll
    mov [pfnCuMemAlloc], rax

    ; Resolve cuMemFree_v2
    mov rcx, rbx
    lea rdx, [szCuMemFree]
    call GetProcAddress
    mov [pfnCuMemFree], rax

    ; Resolve cuMemcpyHtoD_v2
    mov rcx, rbx
    lea rdx, [szCuMemcpyHtoD]
    call GetProcAddress
    mov [pfnCuMemcpyHtoD], rax

    ; Resolve cuMemcpyDtoH_v2
    mov rcx, rbx
    lea rdx, [szCuMemcpyDtoH]
    call GetProcAddress
    mov [pfnCuMemcpyDtoH], rax

    ; Resolve cuLaunchKernel
    mov rcx, rbx
    lea rdx, [szCuLaunchKernel]
    call GetProcAddress
    mov [pfnCuLaunchKernel], rax

    ; Resolve cuCtxSynchronize
    mov rcx, rbx
    lea rdx, [szCuCtxSync]
    call GetProcAddress
    mov [pfnCuCtxSync], rax

    ; cuInit(0)
    xor ecx, ecx
    call [pfnCuInit]
    test eax, eax
    jnz ci_cleanup_dll

    ; cuDeviceGetCount(&count)
    lea rcx, [cuda_device_count]
    call [pfnCuDeviceGetCount]
    test eax, eax
    jnz ci_cleanup_dll

    cmp dword ptr [cuda_device_count], 0
    je ci_cleanup_dll

    ; cuDeviceGet(&device, 0)
    lea rcx, [cuda_device_id]
    xor edx, edx
    call [pfnCuDeviceGet]
    test eax, eax
    jnz ci_cleanup_dll

    ; cuCtxCreate(&ctx, 0, device)
    lea rcx, [hCudaCtx]
    xor edx, edx
    mov r8d, [cuda_device_id]
    call [pfnCuCtxCreate]
    test eax, eax
    jnz ci_cleanup_dll

    ; Zero allocation tracker
    lea rcx, [cuda_alloc_tracker]
    xor eax, eax
    mov edx, 64
ci_zero_track:
    mov [rcx], rax
    add rcx, 8
    dec edx
    jnz ci_zero_track

    mov dword ptr [cuda_initialized], 1

ci_ok:
    xor eax, eax
    jmp ci_ret

ci_cleanup_dll:
    mov rcx, [hNvCudaLib]
    test rcx, rcx
    jz ci_fail
    call FreeLibrary
    mov qword ptr [hNvCudaLib], 0

ci_fail:
    mov eax, -1

ci_ret:
    add rsp, 48
    pop r12
    pop rbx
    pop rbp
    ret
cuda_init ENDP

; ==============================================================================
; cuda_detect_device — Query device name, compute capability, SM count
; Returns: EAX = 0 success, -1 not initialized
; ==============================================================================
cuda_detect_device PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 32
    .allocstack 32
    .endprolog

    cmp dword ptr [cuda_initialized], 0
    je cd_fail

    ; cuDeviceGetName(name, 256, device)
    lea rcx, [cuda_device_name]
    mov edx, 256
    mov r8d, [cuda_device_id]
    call [pfnCuDeviceGetName]

    ; cuDeviceGetAttribute(&major, MAJOR, device)
    cmp qword ptr [pfnCuDeviceGetAttr], 0
    je cd_skip_attr
    lea rcx, [cuda_compute_major]
    mov edx, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR
    mov r8d, [cuda_device_id]
    call [pfnCuDeviceGetAttr]

    ; cuDeviceGetAttribute(&minor, MINOR, device)
    lea rcx, [cuda_compute_minor]
    mov edx, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR
    mov r8d, [cuda_device_id]
    call [pfnCuDeviceGetAttr]

    ; cuDeviceGetAttribute(&sm_count, SM_COUNT, device)
    lea rcx, [cuda_sm_count]
    mov edx, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT
    mov r8d, [cuda_device_id]
    call [pfnCuDeviceGetAttr]

cd_skip_attr:
    xor eax, eax
    jmp cd_ret

cd_fail:
    mov eax, -1

cd_ret:
    add rsp, 32
    pop rbp
    ret
cuda_detect_device ENDP

; ==============================================================================
; cuda_memory_alloc — Allocate GPU memory via cuMemAlloc
; RCX = size in bytes
; Returns: RAX = device pointer, or 0 on failure
; ==============================================================================
cuda_memory_alloc PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog

    cmp dword ptr [cuda_initialized], 0
    je cma_fail

    mov rbx, rcx                    ; size

    ; cuMemAlloc(&dptr, size) — dptr is a local
    mov qword ptr [rbp-16], 0
    lea rcx, [rbp-16]
    mov rdx, rbx
    call [pfnCuMemAlloc]
    test eax, eax
    jnz cma_fail

    mov rax, [rbp-16]              ; device pointer

    ; Track in allocation table
    push rax
    lea rcx, [cuda_alloc_tracker]
    mov edx, 64
cma_track:
    cmp qword ptr [rcx], 0
    je cma_store_it
    add rcx, 8
    dec edx
    jnz cma_track
    jmp cma_tracked                 ; Table full, skip tracking

cma_store_it:
    pop rax
    push rax
    mov [rcx], rax

cma_tracked:
    pop rax
    jmp cma_ret

cma_fail:
    xor eax, eax

cma_ret:
    add rsp, 48
    pop rbx
    pop rbp
    ret
cuda_memory_alloc ENDP

; ==============================================================================
; cuda_execute_kernel — Launch a CUDA kernel (via cuLaunchKernel)
; RCX = CUfunction handle
; RDX = param array (void**)
; R8  = grid size (total threads)
; R9  = block size (threads per block)
; Returns: EAX = 0 success
; ==============================================================================
cuda_execute_kernel PROC FRAME
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
    sub rsp, 112
    .allocstack 112
    .endprolog

    cmp dword ptr [cuda_initialized], 0
    je cek_fail

    cmp qword ptr [pfnCuLaunchKernel], 0
    je cek_fail

    mov rbx, rcx                    ; CUfunction
    mov r12, rdx                    ; params
    mov r13, r8                     ; grid total
    mov r14, r9                     ; block size

    ; Calculate grid dimensions: gridX = ceil(total / blockSize)
    mov rax, r13
    xor edx, edx
    div r14
    test rdx, rdx
    jz cek_no_round
    inc rax
cek_no_round:
    mov r13, rax                    ; gridX

    ; cuLaunchKernel(func, gridX, 1, 1, blockX, 1, 1, 0, NULL, params, NULL)
    ; Win64: first 4 in regs, rest on stack
    mov rcx, rbx                    ; func
    mov edx, r13d                   ; gridDimX
    mov r8d, 1                      ; gridDimY
    mov r9d, 1                      ; gridDimZ
    mov dword ptr [rsp+32], r14d    ; blockDimX
    mov dword ptr [rsp+40], 1       ; blockDimY
    mov dword ptr [rsp+48], 1       ; blockDimZ
    mov dword ptr [rsp+56], 0       ; sharedMemBytes
    mov qword ptr [rsp+64], 0       ; hStream = NULL
    mov [rsp+72], r12               ; kernelParams
    mov qword ptr [rsp+80], 0       ; extra = NULL
    call [pfnCuLaunchKernel]
    test eax, eax
    jnz cek_fail

    ; Synchronize
    cmp qword ptr [pfnCuCtxSync], 0
    je cek_ok
    call [pfnCuCtxSync]

cek_ok:
    xor eax, eax
    jmp cek_ret

cek_fail:
    mov eax, -1

cek_ret:
    add rsp, 112
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
cuda_execute_kernel ENDP

; ==============================================================================
; cuda_shutdown — Cleanup all resources
; ==============================================================================
PUBLIC cuda_shutdown
cuda_shutdown PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 32
    .allocstack 32
    .endprolog

    cmp dword ptr [cuda_initialized], 0
    je cs_ret

    ; Free all tracked GPU allocations
    cmp qword ptr [pfnCuMemFree], 0
    je cs_skip_free
    lea rbx, [cuda_alloc_tracker]
    mov r12d, 64
cs_free_loop:
    mov rcx, [rbx]
    test rcx, rcx
    jz cs_free_next
    call [pfnCuMemFree]
    mov qword ptr [rbx], 0
cs_free_next:
    add rbx, 8
    dec r12d
    jnz cs_free_loop

cs_skip_free:
    ; Destroy CUDA context
    cmp qword ptr [pfnCuCtxDestroy], 0
    je cs_unload
    mov rcx, [hCudaCtx]
    test rcx, rcx
    jz cs_unload
    call [pfnCuCtxDestroy]
    mov qword ptr [hCudaCtx], 0

cs_unload:
    ; Unload nvcuda.dll
    mov rcx, [hNvCudaLib]
    test rcx, rcx
    jz cs_done
    call FreeLibrary
    mov qword ptr [hNvCudaLib], 0

cs_done:
    mov dword ptr [cuda_initialized], 0

cs_ret:
    add rsp, 32
    pop r12
    pop rbx
    pop rbp
    ret
cuda_shutdown ENDP

END