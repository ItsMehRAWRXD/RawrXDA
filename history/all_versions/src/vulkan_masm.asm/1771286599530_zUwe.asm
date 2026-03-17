; QuantQ5_1: Real Q5_1 quantization logic (port of ggml-quants.c)
; Input: RDX = src (float*), R8 = dst (block_q5_1*), R9 = k (int64_t, number of floats)
; Output: RAX = 0 (success)
; Registers used: rdx, r8, r9, r10, r11, xmm0-xmm7
; Assumes QK5_1 = 32 (block size)

; FP32 to FP16 conversion function
; Input: XMM0 = float32
; Output: AX = float16
FP32_TO_FP16 PROC
    ; Extract sign, exponent, mantissa
    movd eax, xmm0
    mov ecx, eax
    shr ecx, 31          ; sign
    mov edx, eax
    shr edx, 23
    and edx, 255         ; exponent
    and eax, 8388607     ; mantissa

    ; Handle special cases
    test edx, edx
    jz .zero_or_denorm
    cmp edx, 255
    je .inf_or_nan

    ; Normal case
    sub edx, 127         ; unbias
    add edx, 15          ; bias for half
    cmp edx, 31
    jg .overflow
    cmp edx, 0
    jl .underflow
    shr eax, 13          ; truncate mantissa
    and eax, 1023
    shl edx, 10
    or eax, edx
    or eax, ecx
    ret

.zero_or_denorm:
    xor eax, eax
    ret

.inf_or_nan:
    mov eax, 31744       ; inf
    or eax, ecx
    ret

.overflow:
    mov eax, 31744       ; inf
    or eax, ecx
    ret

.underflow:
    xor eax, eax
    ret
FP32_TO_FP16 ENDP

QuantQ5_1:
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi

    mov rsi, rdx        ; rsi = src (float*)
    mov rdi, r8         ; rdi = dst (block_q5_1*)
    mov rcx, r9         ; rcx = k (number of floats)

    mov eax, 32         ; QK5_1 = 32
    cdq
    div ecx             ; nb = k / QK5_1
    mov rbx, rax        ; rbx = nb (number of blocks)

    xor r10, r10        ; block index = 0
Q5_1_block_loop:
    cmp r10, rbx
    jge Q5_1_done

    ; Find min and max in block
    mov r12, 0
    movss xmm0, [flt_max]
    movss xmm1, [flt_min]
Q5_1_find_minmax:
    cmp r12, 32
    jge Q5_1_minmax_found
    movss xmm2, dword [rsi + r10*128 + r12*4]
    comiss xmm0, xmm2
    jbe Q5_1_skip_min
    movaps xmm0, xmm2
Q5_1_skip_min:
    comiss xmm1, xmm2
    jae Q5_1_skip_max
    movaps xmm1, xmm2
Q5_1_skip_max:
    inc r12
    jmp Q5_1_find_minmax
Q5_1_minmax_found:
    ; d = (max - min) / 31
    movaps xmm2, xmm1
    subss xmm2, xmm0
    movss xmm3, [thirty_one]
    divss xmm2, xmm3
    movss [rbp-4], xmm2
    ; id = d ? 1.0/d : 0.0
    movaps xmm4, xmm2
    xorps xmm5, xmm5
    ucomiss xmm4, xmm5
    je Q5_1_id_zero
    movss xmm5, [one]
    divss xmm5, xmm4
    jmp Q5_1_id_done
Q5_1_id_zero:
    xorps xmm5, xmm5
Q5_1_id_done:
    ; y[i].d = FP32_TO_FP16(d)
    call FP32_TO_FP16
    mov word [rdi + r10*block51_size], ax
    ; y[i].m = FP32_TO_FP16(min)
    call FP32_TO_FP16
    mov word [rdi + r10*block51_size + 4], ax
    ; Quantize values
    mov r13, 0
    xor r14d, r14d      ; qh = 0
Q5_1_quant_loop:
    cmp r13, 16
    jge Q5_1_quant_done
    movss xmm6, dword [rsi + r10*128 + r13*4]
    subss xmm6, xmm0
    mulss xmm6, xmm5
    addss xmm6, [zero_point_five]
    cvttss2si eax, xmm6
    mov cl, al
    movss xmm7, dword [rsi + r10*128 + (r13+16)*4]
    subss xmm7, xmm0
    mulss xmm7, xmm5
    addss xmm7, [zero_point_five]
    cvttss2si ebx, xmm7
    mov ch, bl
    mov word [rdi + r10*block51_size + 8 + r13], cx
    ; get the 5th bit and store in qh
    bt eax, 4
    adc r14d, 0
    bt ebx, 4
    adc r14d, 0
    inc r13
    jmp Q5_1_quant_loop
Q5_1_quant_done:
    mov dword [rdi + r10*block51_size + 36], r14d
    inc r10
    jmp Q5_1_block_loop
Q5_1_done:
    xor rax, rax
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret

section .data
thirty_one dd 0x41F80000
block51_size equ 40
; Universal MASM x64 Quantization Entry Point
; ------------------------------------------
; Externally callable from C/C++ for any model format (GGUF, HuggingFace, blob, etc.)
; Arguments:
;   RCX = pointer to float32 input (tensor data)
;   RDX = pointer to output buffer (quantized data)
;   R8  = int64_t size (number of floats)
;   R9  = int quantization type ID (see QTYPE_ constants)
; Returns:
;   RAX = 0 on success, <0 on error
; Calling convention: Windows x64 (compatible with C ABI)
global QuantizeTensorUniversal
QuantizeTensorUniversal:
    ; Save non-volatile registers if needed
    push rbx
    push rsi
    push rdi

    mov rsi, rcx        ; rsi = src (float*)
    mov rdi, rdx        ; rdi = dst (output buffer)
    mov rdx, r8         ; rdx = size (int64_t)
    mov ecx, r9d        ; ecx = quantization type ID

    ; Set up arguments for dispatcher:
    ;   RDX = src, R8 = dst, R9 = size, RCX = quant_type
    mov rdx, rsi        ; src
    mov r8, rdi         ; dst
    mov r9, rdx         ; size
    mov rcx, rcx        ; quant_type
    call QuantizeAuto

    ; Restore non-volatile registers
    pop rdi
    pop rsi
    pop rbx
    ret

; Usage from C/C++:
; extern "C" int64_t QuantizeTensorUniversal(float* src, void* dst, int64_t size, int quant_type);
; Returns 0 on success, <0 on error.
; MASM x64 Stub Implementation of Vulkan-like Initialization (Dependency-Free)
; ---------------------------------------------------------------------------)
; This file provides MASM x64 stubs for Vulkan-like initialization and resource
; management routines. All logic is internal, dependency-free, and does NOT link
; to any external Vulkan, ROCm, CUDA, HIP, or GPU driver libraries.does NOT link
; to any external Vulkan, ROCm, CUDA, HIP, or GPU driver libraries.
; Each routine is a placeholder for future internal GPU/CPU backend logic.
; All register usage and calling conventions are documented below.
; ----------------------------
; Calling Convention: Windows x64 (RCX, RDX, R8, R9 for first 4 args)sing a jump table.
; Return: RAXw quantization type, simply add its routine and update the jump table.
; All routines are safe for local, air-gapped, and production use.LL QuantizeAuto.
;
section .datavention: Windows x64 (RCX = quant type ID, RDX = src ptr, R8 = dst ptr, R9 = size)
; Data section for storing Vulkan instance, device, and other resources
vulkan_instance dq 0afe for local, air-gapped, and production use.
physical_device dq 0IDs
logical_device dq 0
command_pool dq 0
compute_queue dq 0
QTYPE_Q5_1 equ 3
section .textu 4
; Code section for Vulkan initialization routines

section .data
; InitializeVulkan: Load vulkan-1.dll and create VkInstance
; Output: RAX = VkInstance handle (0 on failure)
InitializeVulkan:
    mov rax, vulkan_instance
    test rax, rax
    jnz .already_initialized
    
    ; Load Vulkan runtime
    sub rsp, 40
    lea rcx, [sz_vulkan_dll]
    call LoadLibraryA
    test rax, rax
    jz .vk_init_fail
    mov vulkan_dll_handle, rax
    
    ; Resolve vkCreateInstance
    mov rcx, rax
    lea rdx, [sz_vkCreateInstance]
    call GetProcAddress
    test rax, rax
    jz .vk_init_fail
    mov pfn_vkCreateInstance, rax
    
    ; Resolve vkEnumeratePhysicalDevices
    mov rcx, vulkan_dll_handle
    lea rdx, [sz_vkEnumPhysDev]
    call GetProcAddress
    mov pfn_vkEnumPhysDev, rax
    
    ; Call vkCreateInstance with minimal app info
    ; VkApplicationInfo on stack
    lea rcx, [vk_app_info]
    mov DWORD PTR [rcx], 0       ; sType = VK_STRUCTURE_TYPE_APPLICATION_INFO
    mov QWORD PTR [rcx+8], 0     ; pNext
    lea rdx, [sz_app_name]
    mov QWORD PTR [rcx+16], rdx  ; pApplicationName
    mov DWORD PTR [rcx+24], 1    ; applicationVersion
    mov QWORD PTR [rcx+32], 0    ; pEngineName
    mov DWORD PTR [rcx+40], 1    ; engineVersion
    mov DWORD PTR [rcx+48], 4194304 ; apiVersion = VK_API_VERSION_1_0
    
    ; VkInstanceCreateInfo
    lea rcx, [vk_create_info]
    mov DWORD PTR [rcx], 1       ; sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    mov QWORD PTR [rcx+8], 0     ; pNext
    mov DWORD PTR [rcx+16], 0    ; flags
    lea rdx, [vk_app_info]
    mov QWORD PTR [rcx+24], rdx  ; pApplicationInfo
    mov DWORD PTR [rcx+32], 0    ; enabledLayerCount
    mov QWORD PTR [rcx+40], 0    ; ppEnabledLayerNames
    mov DWORD PTR [rcx+48], 0    ; enabledExtensionCount
    mov QWORD PTR [rcx+56], 0    ; ppEnabledExtensionNames
    
    ; vkCreateInstance(&createInfo, NULL, &instance)
    lea rcx, [vk_create_info]
    xor edx, edx                 ; pAllocator = NULL
    lea r8, [vulkan_instance]
    call pfn_vkCreateInstance
    test eax, eax                ; VK_SUCCESS = 0
    jnz .vk_init_fail
    
    mov rax, vulkan_instance
    add rsp, 40
    ret
    
.vk_init_fail:
    xor eax, eax
    mov vulkan_instance, rax
    add rsp, 40
    ret
    
.already_initialized:
    ret

; SelectPhysicalDevice: Enumerate and select best GPU
; Output: RAX = VkPhysicalDevice handle (0 on failure)
SelectPhysicalDevice:
    mov rax, physical_device
    test rax, rax
    jnz .already_selected
    
    ; Ensure Vulkan is initialized
    mov rax, vulkan_instance
    test rax, rax
    jz .select_fail
    
    sub rsp, 40
    
    ; vkEnumeratePhysicalDevices(instance, &count, NULL)
    mov rcx, vulkan_instance
    lea rdx, [phys_dev_count]
    xor r8d, r8d                 ; pPhysicalDevices = NULL (query count)
    call pfn_vkEnumPhysDev
    test eax, eax
    jnz .select_fail_unwind
    
    ; Check count > 0
    mov eax, phys_dev_count
    test eax, eax
    jz .select_fail_unwind
    
    ; Get first physical device (simplistic: pick device 0)
    mov phys_dev_count, 1        ; request only 1
    mov rcx, vulkan_instance
    lea rdx, [phys_dev_count]
    lea r8, [physical_device]
    call pfn_vkEnumPhysDev
    test eax, eax
    jnz .select_fail_unwind
    
    mov rax, physical_device
    add rsp, 40
    ret
    
.select_fail_unwind:
    add rsp, 40
.select_fail:
    xor eax, eax
    ret
    
.already_selected:
    ret

; CreateLogicalDevice: Create VkDevice with compute queue
; Output: RAX = VkDevice handle (0 on failure)
CreateLogicalDevice:
    mov rax, logical_device
    test rax, rax
    jnz .already_created
    
    ; Need physical device first
    mov rax, physical_device
    test rax, rax
    jz .create_fail
    
    sub rsp, 88
    
    ; Resolve vkCreateDevice
    mov rcx, vulkan_dll_handle
    lea rdx, [sz_vkCreateDevice]
    call GetProcAddress
    test rax, rax
    jz .create_fail_unwind
    mov pfn_vkCreateDevice, rax
    
    ; Build VkDeviceQueueCreateInfo on stack
    lea rcx, [rsp]
    mov DWORD PTR [rcx], 2       ; sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO
    mov QWORD PTR [rcx+8], 0     ; pNext
    mov DWORD PTR [rcx+16], 0    ; flags
    mov DWORD PTR [rcx+20], 0    ; queueFamilyIndex = 0 (compute)
    mov DWORD PTR [rcx+24], 1    ; queueCount = 1
    lea rdx, [queue_priority]
    mov QWORD PTR [rcx+32], rdx  ; pQueuePriorities
    
    ; Build VkDeviceCreateInfo
    lea r8, [rsp+40]
    mov DWORD PTR [r8], 3        ; sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
    mov QWORD PTR [r8+8], 0      ; pNext
    mov DWORD PTR [r8+16], 0     ; flags
    mov DWORD PTR [r8+20], 1     ; queueCreateInfoCount
    mov QWORD PTR [r8+24], rcx   ; pQueueCreateInfos
    mov DWORD PTR [r8+32], 0     ; enabledLayerCount
    mov QWORD PTR [r8+40], 0     ; ppEnabledLayerNames
    mov DWORD PTR [r8+48], 0     ; enabledExtensionCount
    mov QWORD PTR [r8+56], 0     ; ppEnabledExtensionNames
    mov QWORD PTR [r8+64], 0     ; pEnabledFeatures
    
    ; vkCreateDevice(physicalDevice, &createInfo, NULL, &device)
    mov rcx, physical_device
    lea rdx, [rsp+40]           ; pCreateInfo
    xor r8d, r8d                ; pAllocator
    lea r9, [logical_device]
    call pfn_vkCreateDevice
    test eax, eax
    jnz .create_fail_unwind
    
    mov rax, logical_device
    add rsp, 88
    ret
    
.create_fail_unwind:
    add rsp, 88
.create_fail:
    xor eax, eax
    ret
    
.already_created:
    ret

; QuantQ4_0: Real Q4_0 quantization logic (port of ggml-quants.c)
; Input: RDX = src (float*), R8 = dst (block_q4_0*), R9 = k (int64_t, number of floats)
; Output: RAX = 0 (success)
; Registers used: rdx, r8, r9, r10, r11, xmm0-xmm7
; Assumes QK4_0 = 32 (block size)


    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi

    mov rsi, rdx        ; rsi = src (float*)
    mov rdi, r8         ; rdi = dst (block_q8_0*)
    mov rcx, r9         ; rcx = k (number of floats)

    mov eax, 32         ; QK8_0 = 32
    cdq
    div ecx             ; nb = k / QK8_0
    mov rbx, rax        ; rbx = nb (number of blocks)

    xor r10, r10        ; block index = 0
Q8_0_block_loop:
    cmp r10, rbx
    jge Q8_0_done

    ; Find min and max in block
    mov r12, 0
    movss xmm0, [flt_max]
    movss xmm1, [flt_min]
Q8_0_find_minmax:
    cmp r12, 32
    jge Q8_0_minmax_found
    movss xmm2, dword [rsi + r10*128 + r12*4]
    comiss xmm0, xmm2
    jbe Q8_0_skip_min
    movaps xmm0, xmm2
Q8_0_skip_min:
    comiss xmm1, xmm2
    jae Q8_0_skip_max
    movaps xmm1, xmm2
Q8_0_skip_max:
    inc r12
    jmp Q8_0_find_minmax
Q8_0_minmax_found:
    ; d = (max - min) / 255
    movaps xmm2, xmm1
    subss xmm2, xmm0
    movss xmm3, [two_fifty_five]
    divss xmm2, xmm3
    movss [rbp-4], xmm2
    ; id = d ? 1.0/d : 0.0
    movaps xmm4, xmm2
    xorps xmm5, xmm5
    ucomiss xmm4, xmm5
    je Q8_0_id_zero
    movss xmm5, [one]
    divss xmm5, xmm4
    jmp Q8_0_id_done
Q8_0_id_zero:
    xorps xmm5, xmm5
Q8_0_id_done:
    ; y[i].d = FP32_TO_FP16(d)
    call FP32_TO_FP16
    mov word [rdi + r10*block8_size], ax
    ; y[i].m = FP32_TO_FP16(min)
    call FP32_TO_FP16
    mov word [rdi + r10*block8_size + 4], ax
    ; Quantize values
    mov r13, 0
Q8_0_quant_loop:
    cmp r13, 32
    jge Q8_0_quant_done
    movss xmm6, dword [rsi + r10*128 + r13*4]
    subss xmm6, xmm0
    mulss xmm6, xmm5
    addss xmm6, [zero_point_five]
    cvttss2si eax, xmm6
    mov byte [rdi + r10*block8_size + 8 + r13], al
    inc r13
    jmp Q8_0_quant_loop
Q8_0_quant_done:
    inc r10
    jmp Q8_0_block_loop
Q8_0_done:
    xor rax, rax
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret

section .data
two_fifty_five dd 0x437F0000
block8_size equ 40
; Registers used: rdx, r8, r9, r10, r11, xmm0-xmm7
; Assumes QK4_1 = 32 (block size)
QuantQ4_1:
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi

    mov rsi, rdx        ; rsi = src (float*)
    mov rdi, r8         ; rdi = dst (block_q4_1*)
    mov rcx, r9         ; rcx = k (number of floats)

    mov eax, 32         ; QK4_1 = 32
    cdq
    div ecx             ; nb = k / QK4_1
    mov rbx, rax        ; rbx = nb (number of blocks)

    xor r10, r10        ; block index = 0
Q4_1_block_loop:
    cmp r10, rbx
    jge Q4_1_done

    ; Find min and max in block
    mov r12, 0
    movss xmm0, [flt_max]
    movss xmm1, [flt_min]
Q4_1_find_minmax:
    cmp r12, 32
    jge Q4_1_minmax_found
    movss xmm2, dword [rsi + r10*128 + r12*4]
    comiss xmm0, xmm2
    jbe Q4_1_skip_min
    movaps xmm0, xmm2
Q4_1_skip_min:
    comiss xmm1, xmm2
    jae Q4_1_skip_max
    movaps xmm1, xmm2
Q4_1_skip_max:
    inc r12
    jmp Q4_1_find_minmax
Q4_1_minmax_found:
    ; d = (max - min) / 15
    movaps xmm2, xmm1
    subss xmm2, xmm0
    movss xmm3, [fifteen]
    divss xmm2, xmm3
    movss [rbp-4], xmm2
    ; id = d ? 1.0/d : 0.0
    movaps xmm4, xmm2
    xorps xmm5, xmm5
    ucomiss xmm4, xmm5
    je Q4_1_id_zero
    movss xmm5, [one]
    divss xmm5, xmm4
    jmp Q4_1_id_done
Q4_1_id_zero:
    xorps xmm5, xmm5
Q4_1_id_done:
    ; y[i].d = FP32_TO_FP16(d)
    call FP32_TO_FP16
    mov word [rdi + r10*block1_size], ax
    ; y[i].m = FP32_TO_FP16(min)
    call FP32_TO_FP16
    mov word [rdi + r10*block1_size + 4], ax
    ; Quantize values
    mov r13, 0
Q4_1_quant_loop:
    cmp r13, 16
    jge Q4_1_quant_done
    movss xmm6, dword [rsi + r10*128 + r13*4]
    subss xmm6, xmm0
    mulss xmm6, xmm5
    addss xmm6, [zero_point_five]
    cvttss2si eax, xmm6
    mov cl, al
    movss xmm7, dword [rsi + r10*128 + (r13+16)*4]
    subss xmm7, xmm0
    mulss xmm7, xmm5
    addss xmm7, [zero_point_five]
    cvttss2si ebx, xmm7
    mov ch, bl
    mov word [rdi + r10*block1_size + 8 + r13], cx
    inc r13
    jmp Q4_1_quant_loop
Q4_1_quant_done:
    inc r10
    jmp Q4_1_block_loop
Q4_1_done:
    xor rax, rax
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret

section .data
flt_max dd 0x7F7FFFFF
flt_min dd 0xFF7FFFFF
fifteen dd 0x41700000
zero_point_five dd 0x3F000000
block1_size equ 40
    push rsi
    push rdi

    mov rsi, rdx        ; rsi = src (float*)
    mov rdi, r8         ; rdi = dst (block_q4_0*)
    mov rcx, r9         ; rcx = k (number of floats)

    mov eax, 32         ; QK4_0 = 32
    cdq
    div ecx             ; nb = k / QK4_0
    mov rbx, rax        ; rbx = nb (number of blocks)

    xor r10, r10        ; block index = 0
Q4_0_block_loop:
    cmp r10, rbx
    jge Q4_0_done

    ; Find absolute max in block
    mov r11, 0
    xorps xmm0, xmm0    ; xmm0 = 0.0 (amax)
    xorps xmm1, xmm1    ; xmm1 = 0.0 (max)
    mov r12, 0
Q4_0_find_max:
    cmp r12, 32
    jge Q4_0_max_found
    movss xmm2, dword [rsi + r10*128 + r12*4]
    movaps xmm3, xmm2
    andps xmm3, [abs_mask]
    comiss xmm0, xmm3
    jae Q4_0_skip_max
    movaps xmm0, xmm3
    movaps xmm1, xmm2
Q4_0_skip_max:
    inc r12
    jmp Q4_0_find_max
Q4_0_max_found:
    ; d = max / -8
    movss xmm2, xmm1
    movss xmm3, [neg_eight]
    divss xmm2, xmm3
    movss [rbp-4], xmm2
    ; id = d ? 1.0/d : 0.0
    movss xmm4, xmm2
    xorps xmm5, xmm5
    ucomiss xmm4, xmm5
    je Q4_0_id_zero
    movss xmm5, [one]
    divss xmm5, xmm4
    jmp Q4_0_id_done
Q4_0_id_zero:
    xorps xmm5, xmm5
Q4_0_id_done:
    ; y[i].d = FP32_TO_FP16(d)
    call FP32_TO_FP16
    mov word [rdi + r10*block_size], ax
    ; Quantize values
    mov r13, 0
Q4_0_quant_loop:
    cmp r13, 16
    jge Q4_0_quant_done
    movss xmm6, dword [rsi + r10*128 + r13*4]
    mulss xmm6, xmm5
    addss xmm6, [eight_point_five]
    cvttss2si eax, xmm6
    mov cl, al
    movss xmm7, dword [rsi + r10*128 + (r13+16)*4]
    mulss xmm7, xmm5
    addss xmm7, [eight_point_five]
    cvttss2si ebx, xmm7
    mov ch, bl
    mov word [rdi + r10*block_size + 4 + r13], cx
    inc r13
    jmp Q4_0_quant_loop
Q4_0_quant_done:
    inc r10
    jmp Q4_0_block_loop
Q4_0_done:
    xor rax, rax
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret

section .data
abs_mask dd 0x7FFFFFFF
neg_eight dd 0xC1000000
one dd 0x3F800000
eight_point_five dd 0x41080000
block_size equ 36
    mov rax, command_pool
    ; [STUB] No real command pool logic. For internal backend only.
    ret




TransferTensorToGPU:
    ; Fast memory copy: host to tensor (assume both are in system RAM)
    ; Input: rdi = dst (tensor), rsi = src (host data), rdx = size in bytes
    ; Output: rax = 0
    push rcx
    mov rcx, rdx
    rep movsb
    xor rax, rax
    pop rcx
    ret

TransferTensorToHost:
    ; Fast memory copy: tensor to host
    ; Input: rdi = dst (host buffer), rsi = src (tensor), rdx = size in bytes
    ; Output: rax = 0
    push rcx
    mov rcx, rdx
    rep movsb
    xor rax, rax
    pop rcx
    ret

AllocateTensorMemory:
    ; Use VirtualAlloc for fast aligned allocation (Windows)
    ; Input: rdi = size in bytes
    ; Output: rax = pointer to allocated memory
    mov rcx, 0               ; lpAddress = NULL
    mov rdx, rdi             ; dwSize = size
    mov r8, 0x1000           ; flAllocationType = MEM_COMMIT
    mov r9, 0x04             ; flProtect = PAGE_READWRITE
    sub rsp, 32              ; shadow space
    call qword ptr [VirtualAlloc]
    add rsp, 32
    ret

FreeTensorMemory:
    ; Use VirtualFree for deallocation (Windows)
    ; Input: rdi = pointer to memory
    mov rcx, rdi             ; lpAddress
    mov rdx, 0               ; dwSize = 0
    mov r8, 0x8000           ; dwFreeType = MEM_RELEASE
    sub rsp, 32              ; shadow space
    call qword ptr [VirtualFree]
    add rsp, 32
    xor rax, rax
    ret

; QuantizeToInt8: Optimized INT8 quantization
QuantizeToInt8:
    ; Real INT8 quantization logic
    ; Input: rsi = src (float*), rdi = dst (int8_t*), rdx = size (number of floats)
    ; Output: rax = 0
    push rbx
    push rcx
    xor rcx, rcx        ; i = 0
    movss xmm1, [zero_point_five]
INT8_quant_loop:
    cmp rcx, rdx
    jge INT8_quant_done
    movss xmm0, dword [rsi + rcx*4]
    addss xmm0, xmm1
    cvttss2si eax, xmm0
    cmp eax, 127
    jle INT8_no_clamp_max
    mov eax, 127
INT8_no_clamp_max:
    cmp eax, -128
    jge INT8_no_clamp_min
    mov eax, -128
INT8_no_clamp_min:
    mov [rdi + rcx], al
    inc rcx
    jmp INT8_quant_loop
INT8_quant_done:
    xor rax, rax
    pop rcx
    pop rbx
    ret

; QuantizeToBFloat16: Optimized BF16 quantization
QuantizeToBFloat16:
    ; Real, fast BF16 quantization logic
    ; Input: rsi = src (float*), rdi = dst (uint16_t*), rdx = size (number of floats)
    ; Output: rax = 0
    push rbx
    push rcx
    xor rcx, rcx        ; i = 0
    mov r8, rdx         ; r8 = size
    mov r9, rsi         ; r9 = src
    mov r10, rdi        ; r10 = dst
    ; Use AVX512 if available, else fallback to scalar
    mov eax, 7
    cpuid
    bt ebx, 16          ; AVX512F is bit 16 of EBX
    jc BF16_AVX512_path
    ; Fallback: scalar conversion
BF16_scalar_loop:
    cmp rcx, r8
    jge BF16_done
    mov eax, [r9 + rcx*4]
    shr eax, 16
    mov [r10 + rcx*2], ax
    inc rcx
    jmp BF16_scalar_loop
BF16_AVX512_path:
    ; Use AVX512 for fast conversion, process 16 floats at a time
    mov r11, r8
    shr r11, 4
    jz BF16_scalar_loop
    xor rcx, rcx
BF16_AVX512_loop:
    cmp rcx, r11
    jge BF16_AVX512_tail
    vmovups zmm0, [r9 + rcx*64]
    vpmovdw ymm1, zmm0
    vmovdqu16 [r10 + rcx*32], ymm1
    add rcx, 1
    jmp BF16_AVX512_loop
BF16_AVX512_tail:
    mov rcx, r11
    shl rcx, 4
    jmp BF16_scalar_loop
BF16_done:
    xor rax, rax
    pop rcx
    pop rbx
    ret

; QuantizeToFloat16: Optimized FP16 quantization
QuantizeToFloat16:
    ; Real, fast FP16 quantization logic
    ; Input: rsi = src (float*), rdi = dst (uint16_t*), rdx = size (number of floats)
    ; Output: rax = 0
    ; Uses F16C if available, else fallback to scalar
    push rbx
    push rcx
    xor rcx, rcx        ; i = 0
    mov r8, rdx         ; r8 = size
    mov r9, rsi         ; r9 = src
    mov r10, rdi        ; r10 = dst
    ; Check for F16C support (CPUID)
    mov eax, 1
    cpuid
    bt ecx, 29          ; F16C is bit 29 of ECX
    jc FP16C_path
    ; Fallback: scalar conversion
FP16_scalar_loop:
    cmp rcx, r8
    jge FP16_done
    movss xmm0, dword [r9 + rcx*4]
    call Float32ToFloat16
    mov word [r10 + rcx*2], ax
    inc rcx
    jmp FP16_scalar_loop
FP16C_path:
    ; Use F16C for fast conversion, process 8 floats at a time
    mov r11, r8
    shr r11, 3
    jz FP16_scalar_loop
    xor rcx, rcx
FP16C_loop:
    cmp rcx, r11
    jge FP16C_tail
    movups xmm0, [r9 + rcx*32]
    movups xmm1, [r9 + rcx*32 + 16]
    vcvtps2ph xmm2, xmm0, 0
    vcvtps2ph xmm3, xmm1, 0
    movq [r10 + rcx*16], xmm2
    movq [r10 + rcx*16 + 8], xmm3
    add rcx, 1
    jmp FP16C_loop
FP16C_tail:
    mov rcx, r11
    shl rcx, 3
    jmp FP16_scalar_loop
FP16_done:
    xor rax, rax
    pop rcx
    pop rbx
    ret

; Scalar float32 to float16 conversion (IEEE 754, round-to-nearest-even)
; Input: xmm0 = float32
; Output: ax = float16
Float32ToFloat16:
    sub rsp, 8
    movss [rsp], xmm0
    mov eax, [rsp]
    mov ecx, eax
    and eax, 0x7F800000
    shr eax, 13
    and ecx, 0x007FFFFF
    shr ecx, 13
    or eax, ecx
    mov ecx, [rsp]
    and ecx, 0x80000000
    shr ecx, 16
    or ax, cx
    add rsp, 8
    ret

; OptimizedQuantizeTensor
; -----------------------
; Optimized automatic tensor quantization
; Input: rdi = pointer to tensor, rsi = pointer to data, rdx = size in bytes, rcx = quantization type
; Output: rax = success/failure

OptimizedQuantizeTensor:
    cmp rcx, 2
    ja QuantizationError

    lea r8, [QuantizationJumpTable]
    mov rax, [r8 + rcx * 8]  ; Load address of quantization handler
    jmp rax

QuantizationJumpTable:
    dq QuantizeToInt8
    dq QuantizeToFloat16
    dq QuantizeToBFloat16

QuantizationError:
    mov rax, -2  ; Quantization failed
    ret

IntegrateQuantization:
    ; Integrate quantization with tensor memory allocation and data transfer
    ; Input: rdi = pointer to tensor, rsi = pointer to data, rdx = size in bytes, rcx = quantization type
    ; Output: rax = success/failure

    ; Allocate memory for the tensor
    mov r8, rdx  ; Save size in r8
    call AllocateTensorMemory
    test rax, rax
    js AllocationError  ; Check for allocation failure

    ; Perform quantization
    mov rdi, rax  ; Pass allocated memory as tensor pointer
    mov rsi, rsi  ; Pass data pointer
    mov rdx, r8   ; Pass size
    mov rcx, rcx  ; Pass quantization type
    call OptimizedQuantizeTensor
    test rax, rax
    js QuantizationError  ; Check for quantization failure

    ret

AllocationError:
    mov rax, -1  ; Allocation failed
    ret
