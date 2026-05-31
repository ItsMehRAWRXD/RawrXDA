; ============================================================================
; GPU_DirectDispatch.asm — GPU Direct Dispatch from MASM
; ============================================================================
; Directly records Vulkan compute dispatches from assembly.
; Bypasses C++ overhead for inference hot paths.
;
; Exports:
;   GPUDD_Init              — Initialize GPU dispatch context
;   GPUDD_CreateBuffer      — Create GPU buffer (RCX=size, RDX=usage)
;   GPUDD_UploadData        — Upload data to GPU buffer
;   GPUDD_DispatchCompute   — Dispatch compute shader (RCX=groupsX, RDX=groupsY, R8=groupsZ)
;   GPUDD_ReadbackData      — Read GPU buffer back to CPU
;   GPUDD_DestroyBuffer       — Destroy GPU buffer
;   GPUDD_Shutdown          — Cleanup
;
; Architecture:
;   1. Get Vulkan device/queue from global context
;   2. Create command pool + command buffer
;   3. Record compute dispatch
;   4. Submit to queue
;   5. Synchronize with fence
; ============================================================================

PUBLIC GPUDD_Init
PUBLIC GPUDD_CreateBuffer
PUBLIC GPUDD_UploadData
PUBLIC GPUDD_DispatchCompute
PUBLIC GPUDD_ReadbackData
PUBLIC GPUDD_DestroyBuffer
PUBLIC GPUDD_Shutdown

; ---------------------------------------------------------------------------
; External C++ bridge (from vulkan_context.cpp)
; ---------------------------------------------------------------------------
EXTRN ?device@VulkanContext@GPU@RawrXD@@QEAAPEAUVkDevice_T@@XZ:PROC
EXTRN ?computeQueue@VulkanContext@GPU@RawrXD@@QEAAPEAUVkQueue_T@@XZ:PROC
EXTRN ?queueFamilyIndex@VulkanContext@GPU@RawrXD@@QEAAIXZ:PROC
EXTRN ?findMemoryType@VulkanContext@GPU@RawrXD@@QEAAIIJ@Z:PROC

; ---------------------------------------------------------------------------
; Win32 APIs
; ---------------------------------------------------------------------------
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN CreateEventW:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC

; ---------------------------------------------------------------------------
; Vulkan function pointers (loaded at init)
; ---------------------------------------------------------------------------
.data
ALIGN 8
; Function pointers
pfn_vkCreateCommandPool     dq 0
pfn_vkDestroyCommandPool    dq 0
pfn_vkAllocateCommandBuffers dq 0
pfn_vkFreeCommandBuffers      dq 0
pfn_vkBeginCommandBuffer      dq 0
pfn_vkEndCommandBuffer        dq 0
pfn_vkCmdBindPipeline         dq 0
pfn_vkCmdDispatch             dq 0
pfn_vkCmdCopyBuffer           dq 0
pfn_vkCmdPipelineBarrier      dq 0
pfn_vkCreateFence             dq 0
pfn_vkDestroyFence            dq 0
pfn_vkQueueSubmit             dq 0
pfn_vkWaitForFences           dq 0
pfn_vkResetFences             dq 0
pfn_vkCreateBuffer            dq 0
pfn_vkDestroyBuffer           dq 0
pfn_vkAllocateMemory          dq 0
pfn_vkFreeMemory              dq 0
pfn_vkBindBufferMemory        dq 0
pfn_vkMapMemory               dq 0
pfn_vkUnmapMemory             dq 0
pfn_vkFlushMappedMemoryRanges dq 0
pfn_vkInvalidateMappedMemoryRanges dq 0
pfn_vkGetBufferMemoryRequirements dq 0

; State
g_initialized               db 0
g_vkDevice                  dq 0
g_vkQueue                   dq 0
g_queueFamilyIndex          dd 0
g_commandPool               dq 0
g_fence                     dq 0
g_stagingBuffer             dq 0
g_stagingMemory             dq 0
g_stagingSize               dq 67108864     ; 64MB staging buffer

; Buffer registry (simple array)
MAX_GPU_BUFFERS             EQU 256
ALIGN 8
g_bufferHandles             dq MAX_GPU_BUFFERS DUP(0)
g_bufferMemories             dq MAX_GPU_BUFFERS DUP(0)
g_bufferSizes               dq MAX_GPU_BUFFERS DUP(0)
g_bufferCount               dd 0

; Constants
VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO      EQU 00000027h
VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO  EQU 00000028h
VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO     EQU 00000029h
VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO            EQU 0000000Ch
VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO          EQU 0000000Dh
VK_STRUCTURE_TYPE_FENCE_CREATE_INFO             EQU 00000017h
VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE             EQU 0000001Bh
VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER         EQU 0000002Ch

VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT   EQU 00000002h
VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT       EQU 00000001h
VK_BUFFER_USAGE_STORAGE_BUFFER_BIT                EQU 00000008h
VK_BUFFER_USAGE_TRANSFER_SRC_BIT                  EQU 00000001h
VK_BUFFER_USAGE_TRANSFER_DST_BIT                  EQU 00000002h
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT               EQU 00000001h
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT             EQU 00000002h
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT              EQU 00000004h

VK_SHARING_MODE_EXCLUSIVE                         EQU 0
VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT            EQU 00000800h
VK_PIPELINE_STAGE_TRANSFER_BIT                    EQU 00001000h
VK_ACCESS_SHADER_WRITE_BIT                      EQU 00020000h
VK_ACCESS_SHADER_READ_BIT                       EQU 00000020h
VK_ACCESS_TRANSFER_WRITE_BIT                    EQU 00001000h
VK_ACCESS_TRANSFER_READ_BIT                     EQU 00000800h

VK_SUCCESS                                        EQU 0
VK_NULL_HANDLE                                    EQU 0
VK_WHOLE_SIZE                                     EQU 0FFFFFFFFFFFFFFFFh

; ---------------------------------------------------------------------------
; .code
; ---------------------------------------------------------------------------
.code

; ============================================================================
; GPUDD_Init — Initialize GPU dispatch context
; Returns RAX = 1 on success
; ============================================================================
GPUDD_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 128
    .allocstack 128
    .endprolog

    cmp     byte ptr [g_initialized], 1
    je      _gpudd_init_ok

    ; Get Vulkan device from context
    call    ?device@VulkanContext@GPU@RawrXD@@QEAAPEAUVkDevice_T@@XZ
    mov     [g_vkDevice], rax
    test    rax, rax
    jz      _gpudd_init_fail

    ; Get compute queue
    call    ?computeQueue@VulkanContext@GPU@RawrXD@@QEAAPEAUVkQueue_T@@XZ
    mov     [g_vkQueue], rax

    ; Get queue family index
    call    ?queueFamilyIndex@VulkanContext@GPU@RawrXD@@QEAAIXZ
    mov     [g_queueFamilyIndex], eax

    ; Create command pool
    lea     rcx, [rsp+32]           ; VkCommandPoolCreateInfo
    mov     dword ptr [rcx], VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO
    mov     dword ptr [rcx+4], 0    ; pNext
    mov     dword ptr [rcx+8], VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    mov     eax, [g_queueFamilyIndex]
    mov     dword ptr [rcx+12], eax ; queueFamilyIndex

    mov     rcx, [g_vkDevice]
    lea     rdx, [rsp+32]
    xor     r8, r8
    lea     r9, [g_commandPool]
    call    qword ptr [pfn_vkCreateCommandPool]
    test    eax, eax
    jnz     _gpudd_init_fail

    ; Create fence
    lea     rcx, [rsp+32]           ; VkFenceCreateInfo
    mov     dword ptr [rcx], VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
    mov     dword ptr [rcx+4], 0    ; pNext
    mov     dword ptr [rcx+8], 0    ; flags

    mov     rcx, [g_vkDevice]
    lea     rdx, [rsp+32]
    xor     r8, r8
    lea     r9, [g_fence]
    call    qword ptr [pfn_vkCreateFence]
    test    eax, eax
    jnz     _gpudd_init_fail

    ; Create staging buffer (64MB host-visible)
    mov     rcx, g_stagingSize
    mov     edx, VK_BUFFER_USAGE_TRANSFER_SRC_BIT OR VK_BUFFER_USAGE_TRANSFER_DST_BIT
    call    GPUDD_CreateBuffer
    test    rax, rax
    jz      _gpudd_init_fail
    mov     [g_stagingBuffer], rax

    mov     byte ptr [g_initialized], 1

_gpudd_init_ok:
    mov     rax, 1
    jmp     _gpudd_init_done

_gpudd_init_fail:
    xor     rax, rax

_gpudd_init_done:
    add     rsp, 128
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GPUDD_Init ENDP

; ============================================================================
; GPUDD_CreateBuffer — RCX=size, RDX=usage flags
; Returns RAX = buffer handle, RDX = memory handle
; ============================================================================
GPUDD_CreateBuffer PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     rbx, rcx                    ; RBX = size
    mov     rsi, rdx                    ; RSI = usage

    ; Find free slot
    xor     ecx, ecx
_find_slot:
    cmp     ecx, MAX_GPU_BUFFERS
    jae     _create_fail
    mov     rax, [g_bufferHandles + rcx * 8]
    test    rax, rax
    jz      _slot_found
    inc     ecx
    jmp     _find_slot
_slot_found:
    mov     edi, ecx                    ; EDI = slot index

    ; Create buffer
    lea     rcx, [rsp+32]           ; VkBufferCreateInfo
    mov     dword ptr [rcx], VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO
    mov     qword ptr [rcx+4], 0    ; pNext
    mov     [rcx+12], rbx           ; size
    mov     [rcx+20], rsi           ; usage
    mov     dword ptr [rcx+28], VK_SHARING_MODE_EXCLUSIVE
    mov     dword ptr [rcx+32], 0   ; queueFamilyIndexCount
    mov     qword ptr [rcx+36], 0   ; pQueueFamilyIndices

    mov     rcx, [g_vkDevice]
    lea     rdx, [rsp+32]
    xor     r8, r8
    lea     r9, [rsp+64]            ; buffer handle out
    call    qword ptr [pfn_vkCreateBuffer]
    test    eax, eax
    jnz     _create_fail

    mov     rax, [rsp+64]
    mov     [g_bufferHandles + rdi * 8], rax

    ; Get memory requirements
    mov     rcx, [g_vkDevice]
    mov     rdx, [rsp+64]
    lea     r8, [rsp+32]            ; VkMemoryRequirements
    call    qword ptr [pfn_vkGetBufferMemoryRequirements]

    ; Allocate memory
    mov     eax, [rsp+32+16]        ; memoryTypeBits
    mov     edx, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    call    ?findMemoryType@VulkanContext@GPU@RawrXD@@QEAAIIJ@Z

    lea     rcx, [rsp+64]           ; VkMemoryAllocateInfo
    mov     dword ptr [rcx], VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO
    mov     qword ptr [rcx+4], 0    ; pNext
    mov     rax, [rsp+32]           ; size from requirements
    mov     [rcx+12], rax
    mov     dword ptr [rcx+20], eax ; memoryTypeIndex

    mov     rcx, [g_vkDevice]
    lea     rdx, [rsp+64]
    xor     r8, r8
    lea     r9, [rsp+96]            ; memory handle out
    call    qword ptr [pfn_vkAllocateMemory]
    test    eax, eax
    jnz     _create_fail

    mov     rax, [rsp+96]
    mov     [g_bufferMemories + rdi * 8], rax

    ; Bind memory
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_bufferHandles + rdi * 8]
    mov     r8, [rsp+96]
    xor     r9d, r9d
    call    qword ptr [pfn_vkBindBufferMemory]

    mov     [g_bufferSizes + rdi * 8], rbx
    inc     dword ptr [g_bufferCount]

    mov     rax, [g_bufferHandles + rdi * 8]
    mov     rdx, [g_bufferMemories + rdi * 8]
    jmp     _create_done

_create_fail:
    xor     rax, rax
    xor     rdx, rdx

_create_done:
    add     rsp, 128
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GPUDD_CreateBuffer ENDP

; ============================================================================
; GPUDD_UploadData — RCX=buffer handle, RDX=CPU data pointer, R8=size
; Returns RAX = 1 on success
; ============================================================================
GPUDD_UploadData PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     rbx, rcx                    ; RBX = buffer
    mov     rsi, rdx                    ; RSI = data
    mov     rdi, r8                     ; RDI = size

    ; Map staging buffer
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_stagingMemory]
    xor     r8, r8
    mov     r9, VK_WHOLE_SIZE
    lea     rax, [rsp+32]           ; ppData
    mov     qword ptr [rsp+64], rax
    call    qword ptr [pfn_vkMapMemory]
    test    eax, eax
    jnz     _upload_fail

    ; Copy data to staging
    mov     rcx, [rsp+32]           ; mapped pointer
    mov     rdx, rsi
    mov     r8, rdi
    rep     movsb

    ; Unmap
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_stagingMemory]
    call    qword ptr [pfn_vkUnmapMemory]

    ; Record copy command
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_commandPool]
    call    _allocateCommandBuffer
    test    rax, rax
    jz      _upload_fail
    mov     r12, rax                ; R12 = command buffer

    ; Begin command buffer
    lea     rcx, [rsp+32]           ; VkCommandBufferBeginInfo
    mov     dword ptr [rcx], VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    mov     qword ptr [rcx+4], 0    ; pNext
    mov     dword ptr [rcx+12], VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    mov     rcx, r12
    lea     rdx, [rsp+32]
    call    qword ptr [pfn_vkBeginCommandBuffer]

    ; Copy staging → device buffer
    lea     rcx, [rsp+32]           ; VkBufferCopy
    mov     qword ptr [rcx], 0      ; srcOffset
    mov     qword ptr [rcx+8], 0    ; dstOffset
    mov     [rcx+16], rdi           ; size
    mov     rcx, r12
    mov     rdx, [g_stagingBuffer]
    mov     r8, rbx
    mov     r9d, 1
    lea     rax, [rsp+32]
    mov     qword ptr [rsp+64], rax
    call    qword ptr [pfn_vkCmdCopyBuffer]

    ; Pipeline barrier
    lea     rcx, [rsp+32]           ; VkBufferMemoryBarrier
    mov     dword ptr [rcx], VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER
    mov     qword ptr [rcx+4], 0    ; pNext
    mov     qword ptr [rcx+12], VK_ACCESS_TRANSFER_WRITE_BIT
    mov     qword ptr [rcx+16], VK_ACCESS_SHADER_READ_BIT
    xor     r9d, r9d
    mov     dword ptr [rcx+20], r9d           ; srcQueueFamilyIndex
    mov     dword ptr [rcx+24], r9d           ; dstQueueFamilyIndex
    mov     qword ptr [rcx+28], rbx           ; buffer
    mov     qword ptr [rcx+36], 0   ; offset
    mov     qword ptr [rcx+44], VK_WHOLE_SIZE ; size

    mov     rcx, r12
    mov     edx, VK_PIPELINE_STAGE_TRANSFER_BIT
    mov     r8d, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    xor     r9d, r9d
    mov     qword ptr [rsp+64], 0
    mov     qword ptr [rsp+72], 0
    lea     rax, [rsp+32]
    mov     qword ptr [rsp+80], rax
    mov     qword ptr [rsp+88], 1
    call    qword ptr [pfn_vkCmdPipelineBarrier]

    ; End command buffer
    mov     rcx, r12
    call    qword ptr [pfn_vkEndCommandBuffer]

    ; Submit
    call    _submitAndWait

    ; Free command buffer
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_commandPool]
    mov     r8d, 1
    lea     r9, [r12]
    call    qword ptr [pfn_vkFreeCommandBuffers]

    mov     rax, 1
    jmp     _upload_done

_upload_fail:
    xor     rax, rax

_upload_done:
    add     rsp, 128
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GPUDD_UploadData ENDP

; ============================================================================
; GPUDD_DispatchCompute — RCX=groupsX, RDX=groupsY, R8=groupsZ
; Returns RAX = 1 on success
; ============================================================================
GPUDD_DispatchCompute PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     ebx, ecx                    ; RBX = groupsX
    mov     esi, edx                    ; RSI = groupsY
    mov     edi, r8d                    ; RDI = groupsZ

    ; Allocate command buffer
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_commandPool]
    call    _allocateCommandBuffer
    test    rax, rax
    jz      _dispatch_fail
    mov     r12, rax

    ; Begin
    lea     rcx, [rsp+32]
    mov     dword ptr [rcx], VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    mov     qword ptr [rcx+4], 0
    mov     dword ptr [rcx+12], VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    mov     rcx, r12
    lea     rdx, [rsp+32]
    call    qword ptr [pfn_vkBeginCommandBuffer]

    ; Dispatch
    mov     rcx, r12
    mov     edx, ebx
    mov     r8d, esi
    mov     r9d, edi
    call    qword ptr [pfn_vkCmdDispatch]

    ; End
    mov     rcx, r12
    call    qword ptr [pfn_vkEndCommandBuffer]

    ; Submit and wait
    call    _submitAndWait

    ; Free
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_commandPool]
    mov     r8d, 1
    lea     r9, [r12]
    call    qword ptr [pfn_vkFreeCommandBuffers]

    mov     rax, 1
    jmp     _dispatch_done

_dispatch_fail:
    xor     rax, rax

_dispatch_done:
    add     rsp, 128
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GPUDD_DispatchCompute ENDP

; ============================================================================
; GPUDD_ReadbackData — RCX=buffer handle, RDX=CPU dest pointer, R8=size
; Returns RAX = 1 on success
; ============================================================================
GPUDD_ReadbackData PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     rbx, rcx                    ; RBX = buffer
    mov     rsi, rdx                    ; RSI = dest
    mov     rdi, r8                     ; RDI = size

    ; Record copy device → staging
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_commandPool]
    call    _allocateCommandBuffer
    test    rax, rax
    jz      _readback_fail
    mov     r12, rax

    ; Begin
    lea     rcx, [rsp+32]
    mov     dword ptr [rcx], VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    mov     qword ptr [rcx+4], 0
    mov     dword ptr [rcx+12], VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    mov     rcx, r12
    lea     rdx, [rsp+32]
    call    qword ptr [pfn_vkBeginCommandBuffer]

    ; Copy buffer → staging
    lea     rcx, [rsp+32]
    mov     qword ptr [rcx], 0
    mov     qword ptr [rcx+8], 0
    mov     [rcx+16], rdi
    mov     rcx, r12
    mov     rdx, rbx
    mov     r8, [g_stagingBuffer]
    mov     r9d, 1
    lea     rax, [rsp+32]
    mov     qword ptr [rsp+64], rax
    call    qword ptr [pfn_vkCmdCopyBuffer]

    ; End
    mov     rcx, r12
    call    qword ptr [pfn_vkEndCommandBuffer]

    ; Submit
    call    _submitAndWait

    ; Free command buffer
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_commandPool]
    mov     r8d, 1
    lea     r9, [r12]
    call    qword ptr [pfn_vkFreeCommandBuffers]

    ; Map staging and copy to dest
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_stagingMemory]
    xor     r8, r8
    mov     r9, VK_WHOLE_SIZE
    lea     rax, [rsp+32]
    mov     qword ptr [rsp+64], rax
    call    qword ptr [pfn_vkMapMemory]
    test    eax, eax
    jnz     _readback_fail

    mov     rcx, rsi
    mov     rdx, [rsp+32]
    mov     r8, rdi
    rep     movsb

    mov     rcx, [g_vkDevice]
    mov     rdx, [g_stagingMemory]
    call    qword ptr [pfn_vkUnmapMemory]

    mov     rax, 1
    jmp     _readback_done

_readback_fail:
    xor     rax, rax

_readback_done:
    add     rsp, 128
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GPUDD_ReadbackData ENDP

; ============================================================================
; GPUDD_DestroyBuffer — RCX=buffer handle
; ============================================================================
GPUDD_DestroyBuffer PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx

    ; Find slot
    xor     ecx, ecx
_find_destroy:
    cmp     ecx, MAX_GPU_BUFFERS
    jae     _destroy_done
    cmp     [g_bufferHandles + rcx * 8], rbx
    je      _destroy_found
    inc     ecx
    jmp     _find_destroy
_destroy_found:
    mov     ebx, ecx

    ; Destroy buffer
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_bufferHandles + rbx * 8]
    xor     r8, r8
    call    qword ptr [pfn_vkDestroyBuffer]

    ; Free memory
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_bufferMemories + rbx * 8]
    xor     r8, r8
    call    qword ptr [pfn_vkFreeMemory]

    mov     qword ptr [g_bufferHandles + rbx * 8], 0
    mov     qword ptr [g_bufferMemories + rbx * 8], 0
    mov     qword ptr [g_bufferSizes + rbx * 8], 0
    dec     dword ptr [g_bufferCount]

_destroy_done:
    add     rsp, 40
    pop     rbx
    ret
GPUDD_DestroyBuffer ENDP

; ============================================================================
; GPUDD_Shutdown — Cleanup all resources
; ============================================================================
GPUDD_Shutdown PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Destroy all buffers
    xor     ecx, ecx
_destroy_loop:
    cmp     ecx, MAX_GPU_BUFFERS
    jae     _destroy_all_done
    mov     rax, [g_bufferHandles + rcx * 8]
    test    rax, rax
    jz      _destroy_next
    mov     rcx, rax
    call    GPUDD_DestroyBuffer
_destroy_next:
    inc     ecx
    jmp     _destroy_loop
_destroy_all_done:

    ; Destroy staging buffer
    mov     rcx, [g_stagingBuffer]
    test    rcx, rcx
    jz      _no_staging
    call    GPUDD_DestroyBuffer
    mov     qword ptr [g_stagingBuffer], 0
_no_staging:

    ; Destroy fence
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_fence]
    xor     r8, r8
    call    qword ptr [pfn_vkDestroyFence]
    mov     qword ptr [g_fence], 0

    ; Destroy command pool
    mov     rcx, [g_vkDevice]
    mov     rdx, [g_commandPool]
    xor     r8, r8
    call    qword ptr [pfn_vkDestroyCommandPool]
    mov     qword ptr [g_commandPool], 0

    mov     byte ptr [g_initialized], 0
    mov     qword ptr [g_vkDevice], 0
    mov     qword ptr [g_vkQueue], 0

    add     rsp, 40
    pop     rbx
    ret
GPUDD_Shutdown ENDP

; ============================================================================
; Internal: _allocateCommandBuffer
; RCX=device, RDX=commandPool
; Returns RAX = command buffer
; ============================================================================
_allocateCommandBuffer PROC
    push    rbx
    sub     rsp, 64

    lea     r8, [rsp+32]            ; VkCommandBufferAllocateInfo
    mov     dword ptr [r8], VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
    mov     qword ptr [r8+4], 0     ; pNext
    mov     [r8+12], rdx            ; commandPool
    mov     dword ptr [r8+20], 0    ; level = PRIMARY
    mov     dword ptr [r8+24], 1    ; commandBufferCount

    lea     r9, [rsp+48]            ; pCommandBuffers
    call    qword ptr [pfn_vkAllocateCommandBuffers]
    test    eax, eax
    jnz     _alloc_fail
    mov     rax, [rsp+48]
    jmp     _alloc_done
_alloc_fail:
    xor     rax, rax
_alloc_done:
    add     rsp, 64
    pop     rbx
    ret
_allocateCommandBuffer ENDP

; ============================================================================
; Internal: _submitAndWait
; Submits current command buffer and waits for fence
; ============================================================================
_submitAndWait PROC
    push    rbx
    sub     rsp, 64

    ; Reset fence
    mov     rcx, [g_vkDevice]
    mov     edx, 1
    lea     r8, [g_fence]
    call    qword ptr [pfn_vkResetFences]

    ; Submit
    lea     rcx, [rsp+32]           ; VkSubmitInfo
    mov     dword ptr [rcx], 00000004h  ; VK_STRUCTURE_TYPE_SUBMIT_INFO
    mov     qword ptr [rcx+4], 0    ; pNext
    mov     dword ptr [rcx+12], 0   ; waitSemaphoreCount
    mov     qword ptr [rcx+16], 0   ; pWaitSemaphores
    mov     qword ptr [rcx+24], 0   ; pWaitDstStageMask
    mov     dword ptr [rcx+32], 1   ; commandBufferCount
    mov     rax, [rsp+48]           ; command buffer (caller must set)
    mov     [rcx+40], rax
    mov     dword ptr [rcx+48], 0   ; signalSemaphoreCount
    mov     qword ptr [rcx+56], 0   ; pSignalSemaphores

    mov     rcx, [g_vkQueue]
    lea     rdx, [rsp+32]
    xor     r8, r8
    mov     r9, [g_fence]
    call    qword ptr [pfn_vkQueueSubmit]

    ; Wait for fence
    mov     rcx, [g_vkDevice]
    mov     edx, 1
    lea     r8, [g_fence]
    mov     r9d, 0FFFFFFFFFFFFFFFFh  ; timeout = infinite
    call    qword ptr [pfn_vkWaitForFences]

    add     rsp, 64
    pop     rbx
    ret
_submitAndWait ENDP

END
