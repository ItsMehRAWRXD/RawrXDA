; ==============================================================================
; Vulkan GPU Backend — MASM64 Dynamic Loader via vulkan-1.dll
; Implements: Init, Device Detection, Memory Management, Compute Dispatch
; Architecture: LoadLibraryW + GetProcAddress — no static link to Vulkan SDK
; Vulkan is a lower-level API than CUDA/HIP so we manage instance, device,
; queue, command pools, descriptor sets, and pipelines explicitly.
; ==============================================================================

OPTION CASEMAP:NONE

EXTERN LoadLibraryW : PROC
EXTERN GetProcAddress : PROC
EXTERN FreeLibrary : PROC

; Vulkan constants
VK_SUCCESS                  EQU 0
VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO   EQU 1
VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO     EQU 3
VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO EQU 2
VK_STRUCTURE_TYPE_SUBMIT_INFO            EQU 4
VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO     EQU 12
VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO   EQU 5
VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO EQU 39
VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO EQU 40
VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO    EQU 42

VK_BUFFER_USAGE_STORAGE_BUFFER_BIT      EQU 20h
VK_BUFFER_USAGE_TRANSFER_SRC_BIT        EQU 01h
VK_BUFFER_USAGE_TRANSFER_DST_BIT        EQU 02h
VK_SHARING_MODE_EXCLUSIVE               EQU 0
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT     EQU 02h
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT    EQU 04h
VK_QUEUE_COMPUTE_BIT                    EQU 02h
VK_COMMAND_BUFFER_LEVEL_PRIMARY         EQU 0
VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT EQU 01h
VK_PIPELINE_BIND_POINT_COMPUTE         EQU 1

.data

ALIGN 8

; vulkan-1.dll wide string
szVulkanDll         DW 'v','u','l','k','a','n','-','1','.','d','l','l',0

; Vulkan API function name strings
szVkGetInstProcAddr         DB "vkGetInstanceProcAddr", 0
szVkCreateInstance          DB "vkCreateInstance", 0
szVkDestroyInstance         DB "vkDestroyInstance", 0
szVkEnumPhysDevices         DB "vkEnumeratePhysicalDevices", 0
szVkGetPhysDevProps         DB "vkGetPhysicalDeviceProperties", 0
szVkGetPhysDevMemProps      DB "vkGetPhysicalDeviceMemoryProperties", 0
szVkGetPhysDevQueueFamProps DB "vkGetPhysicalDeviceQueueFamilyProperties", 0
szVkCreateDevice            DB "vkCreateDevice", 0
szVkDestroyDevice           DB "vkDestroyDevice", 0
szVkGetDeviceQueue          DB "vkGetDeviceQueue", 0
szVkCreateBuffer            DB "vkCreateBuffer", 0
szVkDestroyBuffer           DB "vkDestroyBuffer", 0
szVkGetBufMemReqs           DB "vkGetBufferMemoryRequirements", 0
szVkAllocateMemory          DB "vkAllocateMemory", 0
szVkFreeMemory              DB "vkFreeMemory", 0
szVkBindBufMem              DB "vkBindBufferMemory", 0
szVkMapMemory               DB "vkMapMemory", 0
szVkUnmapMemory             DB "vkUnmapMemory", 0
szVkCreateCmdPool           DB "vkCreateCommandPool", 0
szVkDestroyCmdPool          DB "vkDestroyCommandPool", 0
szVkAllocCmdBufs            DB "vkAllocateCommandBuffers", 0
szVkBeginCmdBuf             DB "vkBeginCommandBuffer", 0
szVkEndCmdBuf               DB "vkEndCommandBuffer", 0
szVkCmdDispatch             DB "vkCmdDispatch", 0
szVkCmdBindPipeline         DB "vkCmdBindPipeline", 0
szVkCmdBindDescSets         DB "vkCmdBindDescriptorSets", 0
szVkQueueSubmit             DB "vkQueueSubmit", 0
szVkQueueWaitIdle           DB "vkQueueWaitIdle", 0
szVkDeviceWaitIdle          DB "vkDeviceWaitIdle", 0

ALIGN 8
; Function pointers — resolved at runtime
pfnVkGetInstProcAddr        QWORD 0
pfnVkCreateInstance         QWORD 0
pfnVkDestroyInstance        QWORD 0
pfnVkEnumPhysDevices        QWORD 0
pfnVkGetPhysDevProps        QWORD 0
pfnVkGetPhysDevMemProps     QWORD 0
pfnVkGetPhysDevQueueFamProps QWORD 0
pfnVkCreateDevice           QWORD 0
pfnVkDestroyDevice          QWORD 0
pfnVkGetDeviceQueue         QWORD 0
pfnVkCreateBuffer           QWORD 0
pfnVkDestroyBuffer          QWORD 0
pfnVkGetBufMemReqs          QWORD 0
pfnVkAllocateMemory         QWORD 0
pfnVkFreeMemory             QWORD 0
pfnVkBindBufMem             QWORD 0
pfnVkMapMemory              QWORD 0
pfnVkUnmapMemory            QWORD 0
pfnVkCreateCmdPool          QWORD 0
pfnVkDestroyCmdPool         QWORD 0
pfnVkAllocCmdBufs           QWORD 0
pfnVkBeginCmdBuf            QWORD 0
pfnVkEndCmdBuf              QWORD 0
pfnVkCmdDispatch            QWORD 0
pfnVkCmdBindPipeline        QWORD 0
pfnVkCmdBindDescSets        QWORD 0
pfnVkQueueSubmit            QWORD 0
pfnVkQueueWaitIdle          QWORD 0
pfnVkDeviceWaitIdle         QWORD 0

; State
hVulkanLib              QWORD 0
vk_instance             QWORD 0
vk_physical_device      QWORD 0
vk_device               QWORD 0
vk_compute_queue        QWORD 0
vk_command_pool         QWORD 0
vk_command_buffer       QWORD 0
vk_compute_queue_family DWORD 0
vk_initialized          DWORD 0
vk_device_count         DWORD 0

; Device properties (VkPhysicalDeviceProperties: 824 bytes, pad to 832)
ALIGN 8
vk_dev_props            BYTE 832 DUP(0)
; Offsets: +0 apiVersion, +4 driverVersion, +8 vendorID, +12 deviceID,
;          +16 deviceType, +20 deviceName[256]

; Memory properties (VkPhysicalDeviceMemoryProperties: ~520 bytes)
ALIGN 8
vk_mem_props            BYTE 520 DUP(0)

; Queue family properties (up to 16 families * 24 bytes)
ALIGN 8
vk_queue_fam_props      BYTE 384 DUP(0)
vk_queue_fam_count      DWORD 0

.data?
ALIGN 8
; Tracked allocations: parallel arrays of VkBuffer / VkDeviceMemory
vk_alloc_buffers        QWORD 64 DUP(?)
vk_alloc_memory         QWORD 64 DUP(?)
vk_alloc_count          DWORD ?

.code

; ==============================================================================
; vulkan_init — Load vulkan-1.dll, create instance, pick physical device,
;               find compute queue family, create logical device + queue,
;               create command pool and primary command buffer.
; Returns: EAX = 0 success, -1 Vulkan not available
; ==============================================================================
PUBLIC vulkan_init
vulkan_init PROC FRAME
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
    sub rsp, 256
    .allocstack 256
    .endprolog

    cmp dword ptr [vk_initialized], 1
    je vi_ok

    ; ── Load vulkan-1.dll ──
    lea rcx, [szVulkanDll]
    call LoadLibraryW
    test rax, rax
    jz vi_fail
    mov [hVulkanLib], rax
    mov rbx, rax

    ; Resolve vkGetInstanceProcAddr (bootstrap — the only one via GetProcAddress)
    mov rcx, rbx
    lea rdx, [szVkGetInstProcAddr]
    call GetProcAddress
    test rax, rax
    jz vi_cleanup_dll
    mov [pfnVkGetInstProcAddr], rax

    ; Resolve vkCreateInstance (pre-instance: pass NULL as instance)
    xor ecx, ecx
    lea rdx, [szVkCreateInstance]
    call [pfnVkGetInstProcAddr]
    test rax, rax
    jz vi_cleanup_dll
    mov [pfnVkCreateInstance], rax

    ; ── Create Vulkan Instance ──
    ; Build VkInstanceCreateInfo on stack (zero-init 64 bytes)
    lea r12, [rbp-256]
    xor eax, eax
    mov ecx, 64
vi_zero_ci:
    mov byte ptr [r12+rcx-1], 0
    dec ecx
    jnz vi_zero_ci
    ; sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    mov dword ptr [r12], VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO

    mov rcx, r12                ; pCreateInfo
    xor edx, edx               ; pAllocator = NULL
    lea r8, [vk_instance]
    call [pfnVkCreateInstance]
    cmp eax, VK_SUCCESS
    jne vi_cleanup_dll

    ; ── Resolve all instance-level functions ──
    ; Helper pattern: vkGetInstanceProcAddr(instance, name) → store
    mov rcx, [vk_instance]
    lea rdx, [szVkDestroyInstance]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkDestroyInstance], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkEnumPhysDevices]
    call [pfnVkGetInstProcAddr]
    test rax, rax
    jz vi_cleanup_instance
    mov [pfnVkEnumPhysDevices], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkGetPhysDevProps]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkGetPhysDevProps], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkGetPhysDevMemProps]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkGetPhysDevMemProps], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkGetPhysDevQueueFamProps]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkGetPhysDevQueueFamProps], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkCreateDevice]
    call [pfnVkGetInstProcAddr]
    test rax, rax
    jz vi_cleanup_instance
    mov [pfnVkCreateDevice], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkDestroyDevice]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkDestroyDevice], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkGetDeviceQueue]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkGetDeviceQueue], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkCreateBuffer]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkCreateBuffer], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkDestroyBuffer]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkDestroyBuffer], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkGetBufMemReqs]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkGetBufMemReqs], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkAllocateMemory]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkAllocateMemory], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkFreeMemory]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkFreeMemory], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkBindBufMem]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkBindBufMem], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkMapMemory]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkMapMemory], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkUnmapMemory]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkUnmapMemory], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkCreateCmdPool]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkCreateCmdPool], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkDestroyCmdPool]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkDestroyCmdPool], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkAllocCmdBufs]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkAllocCmdBufs], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkBeginCmdBuf]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkBeginCmdBuf], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkEndCmdBuf]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkEndCmdBuf], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkCmdDispatch]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkCmdDispatch], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkCmdBindPipeline]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkCmdBindPipeline], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkCmdBindDescSets]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkCmdBindDescSets], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkQueueSubmit]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkQueueSubmit], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkQueueWaitIdle]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkQueueWaitIdle], rax

    mov rcx, [vk_instance]
    lea rdx, [szVkDeviceWaitIdle]
    call [pfnVkGetInstProcAddr]
    mov [pfnVkDeviceWaitIdle], rax

    ; ── Enumerate Physical Devices ──
    mov rcx, [vk_instance]
    lea rdx, [vk_device_count]
    xor r8d, r8d                ; NULL = just get count
    call [pfnVkEnumPhysDevices]
    cmp eax, VK_SUCCESS
    jne vi_cleanup_instance

    cmp dword ptr [vk_device_count], 0
    je vi_cleanup_instance

    ; Get first physical device
    mov dword ptr [rbp-64], 1
    mov rcx, [vk_instance]
    lea rdx, [rbp-64]
    lea r8, [vk_physical_device]
    call [pfnVkEnumPhysDevices]
    cmp eax, VK_SUCCESS
    jne vi_cleanup_instance

    ; ── Find Compute Queue Family ──
    cmp qword ptr [pfnVkGetPhysDevQueueFamProps], 0
    je vi_use_queue_0

    mov rcx, [vk_physical_device]
    lea rdx, [vk_queue_fam_count]
    xor r8d, r8d
    call [pfnVkGetPhysDevQueueFamProps]

    mov rcx, [vk_physical_device]
    lea rdx, [vk_queue_fam_count]
    lea r8, [vk_queue_fam_props]
    call [pfnVkGetPhysDevQueueFamProps]

    ; Search for compute-capable family
    xor ebx, ebx
    mov r13d, [vk_queue_fam_count]
    lea r14, [vk_queue_fam_props]

vi_queue_search:
    cmp ebx, r13d
    jge vi_use_queue_0
    ; VkQueueFamilyProperties: queueFlags(4), queueCount(4), ...
    mov eax, [r14]
    test eax, VK_QUEUE_COMPUTE_BIT
    jnz vi_found_queue
    add r14, 24
    inc ebx
    jmp vi_queue_search

vi_found_queue:
    mov [vk_compute_queue_family], ebx
    jmp vi_create_device

vi_use_queue_0:
    mov dword ptr [vk_compute_queue_family], 0

vi_create_device:
    ; ── Create Logical Device ──
    lea r12, [rbp-192]

    ; Queue priority = 1.0f
    mov dword ptr [rbp-48], 3F800000h

    ; VkDeviceQueueCreateInfo (40 bytes)
    mov dword ptr [r12], VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO
    mov qword ptr [r12+8], 0
    mov dword ptr [r12+16], 0
    mov eax, [vk_compute_queue_family]
    mov dword ptr [r12+20], eax
    mov dword ptr [r12+24], 1
    lea rax, [rbp-48]
    mov qword ptr [r12+32], rax

    ; VkDeviceCreateInfo (72 bytes)
    lea r13, [r12+48]
    mov dword ptr [r13], VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
    mov qword ptr [r13+8], 0
    mov dword ptr [r13+16], 0
    mov dword ptr [r13+20], 1
    mov qword ptr [r13+24], r12
    mov dword ptr [r13+32], 0
    mov qword ptr [r13+40], 0
    mov dword ptr [r13+48], 0
    mov qword ptr [r13+56], 0
    mov qword ptr [r13+64], 0

    mov rcx, [vk_physical_device]
    mov rdx, r13
    xor r8d, r8d
    lea r9, [vk_device]
    call [pfnVkCreateDevice]
    cmp eax, VK_SUCCESS
    jne vi_cleanup_instance

    ; ── Get Compute Queue ──
    cmp qword ptr [pfnVkGetDeviceQueue], 0
    je vi_skip_queue
    mov rcx, [vk_device]
    mov edx, [vk_compute_queue_family]
    xor r8d, r8d
    lea r9, [vk_compute_queue]
    call [pfnVkGetDeviceQueue]
vi_skip_queue:

    ; ── Create Command Pool ──
    cmp qword ptr [pfnVkCreateCmdPool], 0
    je vi_skip_cmdpool

    lea r12, [rbp-128]
    mov dword ptr [r12], VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO
    mov qword ptr [r12+8], 0
    mov dword ptr [r12+16], 02h     ; RESET_COMMAND_BUFFER_BIT
    mov eax, [vk_compute_queue_family]
    mov dword ptr [r12+20], eax

    mov rcx, [vk_device]
    mov rdx, r12
    xor r8d, r8d
    lea r9, [vk_command_pool]
    call [pfnVkCreateCmdPool]
    cmp eax, VK_SUCCESS
    jne vi_cleanup_device

    ; ── Allocate Primary Command Buffer ──
    cmp qword ptr [pfnVkAllocCmdBufs], 0
    je vi_skip_cmdpool

    lea r12, [rbp-128]
    mov dword ptr [r12], VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
    mov qword ptr [r12+8], 0
    mov rax, [vk_command_pool]
    mov qword ptr [r12+16], rax
    mov dword ptr [r12+24], VK_COMMAND_BUFFER_LEVEL_PRIMARY
    mov dword ptr [r12+28], 1

    mov rcx, [vk_device]
    mov rdx, r12
    lea r8, [vk_command_buffer]
    call [pfnVkAllocCmdBufs]

vi_skip_cmdpool:
    ; Zero allocation tracker
    mov dword ptr [vk_alloc_count], 0
    mov dword ptr [vk_initialized], 1

vi_ok:
    xor eax, eax
    jmp vi_ret

vi_cleanup_device:
    cmp qword ptr [pfnVkDestroyDevice], 0
    je vi_cleanup_instance
    mov rcx, [vk_device]
    xor edx, edx
    call [pfnVkDestroyDevice]
    mov qword ptr [vk_device], 0

vi_cleanup_instance:
    cmp qword ptr [pfnVkDestroyInstance], 0
    je vi_cleanup_dll
    mov rcx, [vk_instance]
    xor edx, edx
    call [pfnVkDestroyInstance]
    mov qword ptr [vk_instance], 0

vi_cleanup_dll:
    mov rcx, [hVulkanLib]
    test rcx, rcx
    jz vi_fail
    call FreeLibrary
    mov qword ptr [hVulkanLib], 0

vi_fail:
    mov eax, -1

vi_ret:
    add rsp, 256
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
vulkan_init ENDP

; ==============================================================================
; vulkan_detect_device — Query physical device properties + memory properties
; Returns: EAX = 0 success, -1 not initialized
; ==============================================================================
PUBLIC vulkan_detect_device
vulkan_detect_device PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 32
    .allocstack 32
    .endprolog

    cmp dword ptr [vk_initialized], 0
    je vd_fail

    cmp qword ptr [pfnVkGetPhysDevProps], 0
    je vd_skip_props
    mov rcx, [vk_physical_device]
    lea rdx, [vk_dev_props]
    call [pfnVkGetPhysDevProps]
vd_skip_props:

    cmp qword ptr [pfnVkGetPhysDevMemProps], 0
    je vd_skip_mem
    mov rcx, [vk_physical_device]
    lea rdx, [vk_mem_props]
    call [pfnVkGetPhysDevMemProps]
vd_skip_mem:

    xor eax, eax
    jmp vd_ret
vd_fail:
    mov eax, -1
vd_ret:
    add rsp, 32
    pop rbp
    ret
vulkan_detect_device ENDP

; ==============================================================================
; vulkan_memory_alloc — Create VkBuffer + allocate + bind device memory
; RCX = size in bytes
; Returns: EAX = allocation index (0-63), or -1 on failure
; ==============================================================================
PUBLIC vulkan_memory_alloc
vulkan_memory_alloc PROC FRAME
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
    sub rsp, 160
    .allocstack 160
    .endprolog

    cmp dword ptr [vk_initialized], 0
    je vma_fail
    cmp qword ptr [pfnVkCreateBuffer], 0
    je vma_fail

    mov rbx, rcx                ; save size

    ; ── Create VkBuffer ──
    ; VkBufferCreateInfo (56 bytes)
    lea r12, [rbp-160]
    mov dword ptr [r12], VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO
    mov qword ptr [r12+8], 0
    mov dword ptr [r12+16], 0
    mov qword ptr [r12+24], rbx
    mov dword ptr [r12+32], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT or VK_BUFFER_USAGE_TRANSFER_SRC_BIT or VK_BUFFER_USAGE_TRANSFER_DST_BIT
    mov dword ptr [r12+36], VK_SHARING_MODE_EXCLUSIVE
    mov dword ptr [r12+40], 0
    mov qword ptr [r12+48], 0

    mov rcx, [vk_device]
    mov rdx, r12
    xor r8d, r8d
    lea r9, [rbp-80]            ; &buffer (output)
    call [pfnVkCreateBuffer]
    cmp eax, VK_SUCCESS
    jne vma_fail

    mov r13, [rbp-80]           ; VkBuffer handle

    ; ── Get memory requirements ──
    cmp qword ptr [pfnVkGetBufMemReqs], 0
    je vma_cleanup_buf

    ; VkMemoryRequirements: size(8), alignment(8), memoryTypeBits(4)
    mov rcx, [vk_device]
    mov rdx, r13
    lea r8, [rbp-112]
    call [pfnVkGetBufMemReqs]

    ; ── Find HOST_VISIBLE | HOST_COHERENT memory type ──
    mov eax, [rbp-96]           ; memoryTypeBits (at offset +16 in MemReqs? no: size=8,align=8,bits=4 → offset 16)
    mov r14d, eax
    lea r12, [vk_mem_props]
    mov ecx, [r12]              ; memoryTypeCount
    xor ebx, ebx

vma_find_type:
    cmp ebx, ecx
    jge vma_cleanup_buf
    bt r14d, ebx
    jnc vma_next_type
    ; memoryTypes[i].propertyFlags at offset 4 + i*8
    mov eax, ebx
    shl eax, 3
    add eax, 4
    mov edx, [r12 + rax]
    test edx, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT or VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    jnz vma_found_type
vma_next_type:
    inc ebx
    jmp vma_find_type

vma_found_type:
    cmp qword ptr [pfnVkAllocateMemory], 0
    je vma_cleanup_buf

    ; VkMemoryAllocateInfo (32 bytes)
    lea r12, [rbp-160]
    mov dword ptr [r12], VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO
    mov qword ptr [r12+8], 0
    mov rax, [rbp-112]          ; allocationSize from memReqs
    mov qword ptr [r12+16], rax
    mov dword ptr [r12+24], ebx

    mov rcx, [vk_device]
    mov rdx, r12
    xor r8d, r8d
    lea r9, [rbp-88]
    call [pfnVkAllocateMemory]
    cmp eax, VK_SUCCESS
    jne vma_cleanup_buf

    ; ── Bind buffer to memory ──
    cmp qword ptr [pfnVkBindBufMem], 0
    je vma_cleanup_mem

    mov rcx, [vk_device]
    mov rdx, r13
    mov r8, [rbp-88]
    xor r9d, r9d                ; offset 0
    call [pfnVkBindBufMem]
    cmp eax, VK_SUCCESS
    jne vma_cleanup_mem

    ; ── Track allocation ──
    mov eax, [vk_alloc_count]
    cmp eax, 64
    jge vma_cleanup_mem
    lea rcx, [vk_alloc_buffers]
    mov [rcx + rax*8], r13
    lea rcx, [vk_alloc_memory]
    mov r12, [rbp-88]
    mov [rcx + rax*8], r12
    inc dword ptr [vk_alloc_count]
    ; eax = index
    jmp vma_ret

vma_cleanup_mem:
    cmp qword ptr [pfnVkFreeMemory], 0
    je vma_cleanup_buf
    mov rcx, [vk_device]
    mov rdx, [rbp-88]
    xor r8d, r8d
    call [pfnVkFreeMemory]

vma_cleanup_buf:
    cmp qword ptr [pfnVkDestroyBuffer], 0
    je vma_fail
    mov rcx, [vk_device]
    mov rdx, r13
    xor r8d, r8d
    call [pfnVkDestroyBuffer]

vma_fail:
    mov eax, -1

vma_ret:
    add rsp, 160
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
vulkan_memory_alloc ENDP

; ==============================================================================
; vulkan_execute_command — Record and submit a compute dispatch
; RCX = VkPipeline, RDX = VkPipelineLayout, R8 = VkDescriptorSet
; R9  = dispatch group count X (Y=1, Z=1)
; Returns: EAX = 0 success, -1 failure
; ==============================================================================
PUBLIC vulkan_execute_command
vulkan_execute_command PROC FRAME
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
    sub rsp, 128
    .allocstack 128
    .endprolog

    cmp dword ptr [vk_initialized], 0
    je vec_fail

    mov rbx, rcx                ; pipeline
    mov r12, rdx                ; pipeline layout
    mov r13, r8                 ; descriptor set
    mov r14d, r9d               ; groupCountX
    mov r15, [vk_command_buffer]
    test r15, r15
    jz vec_fail

    ; ── Begin Command Buffer ──
    cmp qword ptr [pfnVkBeginCmdBuf], 0
    je vec_fail

    lea rax, [rbp-128]
    mov dword ptr [rax], VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    mov qword ptr [rax+8], 0
    mov dword ptr [rax+16], VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    mov qword ptr [rax+24], 0

    mov rcx, r15
    mov rdx, rax
    call [pfnVkBeginCmdBuf]
    cmp eax, VK_SUCCESS
    jne vec_fail

    ; ── Bind Pipeline ──
    cmp qword ptr [pfnVkCmdBindPipeline], 0
    je vec_dispatch
    mov rcx, r15
    mov edx, VK_PIPELINE_BIND_POINT_COMPUTE
    mov r8, rbx
    call [pfnVkCmdBindPipeline]

    ; ── Bind Descriptor Sets ──
    cmp qword ptr [pfnVkCmdBindDescSets], 0
    je vec_dispatch
    mov [rbp-64], r13           ; descriptor set handle on stack
    mov rcx, r15
    mov edx, VK_PIPELINE_BIND_POINT_COMPUTE
    mov r8, r12
    xor r9d, r9d                ; firstSet = 0
    mov dword ptr [rsp+32], 1   ; descriptorSetCount
    lea rax, [rbp-64]
    mov [rsp+40], rax           ; pDescriptorSets
    mov dword ptr [rsp+48], 0   ; dynamicOffsetCount
    mov qword ptr [rsp+56], 0   ; pDynamicOffsets
    call [pfnVkCmdBindDescSets]

vec_dispatch:
    cmp qword ptr [pfnVkCmdDispatch], 0
    je vec_end_cmd
    mov rcx, r15
    mov edx, r14d               ; groupCountX
    mov r8d, 1                  ; Y
    mov r9d, 1                  ; Z
    call [pfnVkCmdDispatch]

vec_end_cmd:
    cmp qword ptr [pfnVkEndCmdBuf], 0
    je vec_fail
    mov rcx, r15
    call [pfnVkEndCmdBuf]
    cmp eax, VK_SUCCESS
    jne vec_fail

    ; ── Submit to Queue ──
    cmp qword ptr [pfnVkQueueSubmit], 0
    je vec_fail
    cmp qword ptr [vk_compute_queue], 0
    je vec_fail

    ; VkSubmitInfo (72 bytes)
    lea rax, [rbp-128]
    mov dword ptr [rax], VK_STRUCTURE_TYPE_SUBMIT_INFO
    mov qword ptr [rax+8], 0
    mov dword ptr [rax+16], 0       ; waitSemaphoreCount
    mov qword ptr [rax+24], 0
    mov qword ptr [rax+32], 0       ; pWaitDstStageMask
    mov dword ptr [rax+40], 1       ; commandBufferCount
    lea rcx, [vk_command_buffer]
    mov qword ptr [rax+48], rcx     ; pCommandBuffers
    mov dword ptr [rax+56], 0       ; signalSemaphoreCount
    mov qword ptr [rax+64], 0

    ; vkQueueSubmit(queue, submitCount, pSubmits, fence)
    mov rcx, [vk_compute_queue]
    mov edx, 1
    mov r8, rax
    xor r9d, r9d                ; fence = NULL
    call [pfnVkQueueSubmit]
    cmp eax, VK_SUCCESS
    jne vec_fail

    ; Wait for completion
    cmp qword ptr [pfnVkQueueWaitIdle], 0
    je vec_ok
    mov rcx, [vk_compute_queue]
    call [pfnVkQueueWaitIdle]

vec_ok:
    xor eax, eax
    jmp vec_ret

vec_fail:
    mov eax, -1

vec_ret:
    add rsp, 128
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
vulkan_execute_command ENDP

; ==============================================================================
; vulkan_shutdown — Full teardown: free allocs, destroy pool, device, instance
; ==============================================================================
PUBLIC vulkan_shutdown
vulkan_shutdown PROC FRAME
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

    cmp dword ptr [vk_initialized], 0
    je vs_ret

    ; Wait for device idle
    cmp qword ptr [pfnVkDeviceWaitIdle], 0
    je vs_free_allocs
    mov rcx, [vk_device]
    test rcx, rcx
    jz vs_free_allocs
    call [pfnVkDeviceWaitIdle]

vs_free_allocs:
    mov r12d, [vk_alloc_count]
    test r12d, r12d
    jz vs_destroy_cmdpool
    xor ebx, ebx

vs_free_loop:
    cmp ebx, r12d
    jge vs_destroy_cmdpool

    ; Destroy buffer
    cmp qword ptr [pfnVkDestroyBuffer], 0
    je vs_free_mem
    lea rax, [vk_alloc_buffers]
    mov rdx, [rax + rbx*8]
    test rdx, rdx
    jz vs_free_mem
    mov rcx, [vk_device]
    xor r8d, r8d
    call [pfnVkDestroyBuffer]

vs_free_mem:
    cmp qword ptr [pfnVkFreeMemory], 0
    je vs_free_next
    lea rax, [vk_alloc_memory]
    mov rdx, [rax + rbx*8]
    test rdx, rdx
    jz vs_free_next
    mov rcx, [vk_device]
    xor r8d, r8d
    call [pfnVkFreeMemory]

vs_free_next:
    inc ebx
    jmp vs_free_loop

vs_destroy_cmdpool:
    cmp qword ptr [pfnVkDestroyCmdPool], 0
    je vs_destroy_device
    mov rcx, [vk_device]
    test rcx, rcx
    jz vs_destroy_device
    mov rdx, [vk_command_pool]
    test rdx, rdx
    jz vs_destroy_device
    xor r8d, r8d
    call [pfnVkDestroyCmdPool]
    mov qword ptr [vk_command_pool], 0
    mov qword ptr [vk_command_buffer], 0

vs_destroy_device:
    cmp qword ptr [pfnVkDestroyDevice], 0
    je vs_destroy_instance
    mov rcx, [vk_device]
    test rcx, rcx
    jz vs_destroy_instance
    xor edx, edx
    call [pfnVkDestroyDevice]
    mov qword ptr [vk_device], 0

vs_destroy_instance:
    cmp qword ptr [pfnVkDestroyInstance], 0
    je vs_unload
    mov rcx, [vk_instance]
    test rcx, rcx
    jz vs_unload
    xor edx, edx
    call [pfnVkDestroyInstance]
    mov qword ptr [vk_instance], 0

vs_unload:
    mov rcx, [hVulkanLib]
    test rcx, rcx
    jz vs_done
    call FreeLibrary
    mov qword ptr [hVulkanLib], 0

vs_done:
    mov dword ptr [vk_initialized], 0

vs_ret:
    add rsp, 32
    pop r12
    pop rbx
    pop rbp
    ret
vulkan_shutdown ENDP

END