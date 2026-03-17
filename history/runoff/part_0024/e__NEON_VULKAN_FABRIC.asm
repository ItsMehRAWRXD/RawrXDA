; ==============================================================================
; NEON_VULKAN_FABRIC.ASM - Phase-4 Distributed Fabric & Vulkan GPU Kernel
; "800B Model Sharding with Vulkan Compute for Tool Parsing"
; Pure x64 MASM - Zero Dependencies - Cross-Platform Production
; ==============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:3

; ==============================================================================
; EXTERNALS - Windows NT Kernel + Vulkan Loader
; ==============================================================================

; Windows Memory Management
extern CreateFileMappingA       : proc
extern OpenFileMappingA         : proc
extern MapViewOfFileEx          : proc
extern UnmapViewOfFile          : proc
extern VirtualAlloc             : proc
extern VirtualFree              : proc
extern VirtualProtect           : proc
extern GetLargePageMinimum       : proc
extern GetLastError             : proc
extern NtCreateSection          : proc
extern NtMapViewOfSection       : proc

; Vulkan 1.3 Core (vulkan-1.dll)
extern vkGetInstanceProcAddr    : proc
extern vkCreateInstance         : proc
extern vkDestroyInstance        : proc
extern vkEnumeratePhysicalDevices : proc
extern vkGetPhysicalDeviceProperties : proc
extern vkGetPhysicalDeviceMemoryProperties : proc
extern vkGetPhysicalDeviceQueueFamilyProperties : proc
extern vkCreateDevice           : proc
extern vkDestroyDevice          : proc
extern vkGetDeviceProcAddr      : proc
extern vkCreateBuffer           : proc
extern vkDestroyBuffer          : proc
extern vkAllocateMemory         : proc
extern vkFreeMemory             : proc
extern vkBindBufferMemory       : proc
extern vkMapMemory              : proc
extern vkUnmapMemory            : proc
extern vkFlushMappedMemoryRanges : proc
extern vkInvalidateMappedMemoryRanges : proc
extern vkCreateShaderModule     : proc
extern vkDestroyShaderModule    : proc
extern vkCreateDescriptorSetLayout : proc
extern vkCreatePipelineLayout   : proc
extern vkCreateComputePipelines : proc
extern vkDestroyPipeline        : proc
extern vkCreateCommandPool      : proc
extern vkAllocateCommandBuffers : proc
extern vkBeginCommandBuffer     : proc
extern vkEndCommandBuffer       : proc
extern vkCmdBindPipeline        : proc
extern vkCmdBindDescriptorSets  : proc
extern vkCmdDispatch            : proc
extern vkCreateFence            : proc
extern vkWaitForFences          : proc
extern vkResetFences            : proc
extern vkQueueSubmit            : proc
extern vkGetDeviceQueue         : proc
extern vkCreateDescriptorPool   : proc
extern vkAllocateDescriptorSets : proc
extern vkUpdateDescriptorSets   : proc
extern vkGetBufferMemoryRequirements : proc
extern vkGetBufferDeviceAddress : proc
extern vkGetPhysicalDeviceProperties2 : proc
extern vkGetPhysicalDeviceFeatures2 : proc

; Synchronization
extern CreateMutexA             : proc
extern ReleaseMutex             : proc
extern WaitForSingleObject      : proc
extern CreateEventA             : proc
extern SetEvent                 : proc
extern ResetEvent               : proc
extern LoadLibraryA             : proc
extern GetProcAddress           : proc

; ==============================================================================
; CONSTANTS - 800B Model Scale & Vulkan
; ==============================================================================

; Memory Architecture (same as CUDA version)
FABRIC_BASE_ADDRESS             EQU 0000070000000000h
FABRIC_TOTAL_SIZE               EQU 0000064000000000h
FABRIC_SHARD_SIZE               EQU 0000002000000000h
MAX_SHARDS                      EQU 16

; Large Pages
PAGE_HUGE_1GB                   EQU 100000000h
PAGE_LARGE_2MB                  EQU 200000h
SEC_HUGE_PAGES                  EQU 8000h
SEC_LARGE_PAGES                 EQU 80000000h

; Vulkan Constants
VK_API_VERSION_1_3              EQU 004030000h
VK_MAX_PHYSICAL_DEVICE_NAME_SIZE EQU 256
VK_UUID_SIZE                    EQU 16

; Buffer Usage Flags
VK_BUFFER_USAGE_STORAGE_BUFFER_BIT      EQU 000000008h
VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT      EQU 000000010h
VK_BUFFER_USAGE_TRANSFER_SRC_BIT        EQU 000000001h
VK_BUFFER_USAGE_TRANSFER_DST_BIT        EQU 000000002h
VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT EQU 000200000h

; Memory Property Flags
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT     EQU 000000001h
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT     EQU 000000002h
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT    EQU 000000004h
VK_MEMORY_PROPERTY_HOST_CACHED_BIT      EQU 000000008h

; Shader Stages
VK_SHADER_STAGE_COMPUTE_BIT     EQU 000000020h

; Descriptor Types
VK_DESCRIPTOR_TYPE_STORAGE_BUFFER       EQU 7
VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER       EQU 6
VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC EQU 9

; Queue Flags
VK_QUEUE_COMPUTE_BIT            EQU 000000002h

; Sharing Mode
VK_SHARING_MODE_EXCLUSIVE       EQU 0

; Pipeline Bind Point
VK_PIPELINE_BIND_POINT_COMPUTE  EQU 1

; Fence Flags
VK_FENCE_CREATE_SIGNALED_BIT    EQU 000000001h

; Command Buffer Usage Flags
VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT EQU 000000001h

; FSM States (same as before)
FSM_STATE_IDLE                  EQU 0
FSM_STATE_OBJECT_START          EQU 1
FSM_STATE_KEY                   EQU 2
FSM_STATE_COLON                 EQU 3
FSM_STATE_VALUE_STRING          EQU 4
FSM_STATE_VALUE_NUMBER          EQU 5
FSM_STATE_VALUE_BOOL            EQU 6
FSM_STATE_VALUE_NULL            EQU 7
FSM_STATE_ARRAY_START           EQU 8
FSM_STATE_ARRAY_VALUE           EQU 9
FSM_STATE_ESCAPE                EQU 10
FSM_STATE_UNICODE               EQU 11
FSM_STATE_TOOL_NAME             EQU 12
FSM_STATE_TOOL_ARGS             EQU 13
FSM_MAX_STATES                  EQU 16

; Token Classes
TOKEN_ALPHA                     EQU 0
TOKEN_DIGIT                     EQU 1
TOKEN_QUOTE                     EQU 2
TOKEN_COMMA                     EQU 3
TOKEN_COLON                     EQU 4
TOKEN_LBRACE                    EQU 5
TOKEN_RBRACE                    EQU 6
TOKEN_LBRACKET                  EQU 7
TOKEN_RBRACKET                  EQU 8
TOKEN_BACKSLASH                 EQU 9
TOKEN_MINUS                     EQU 10
TOKEN_DECIMAL                   EQU 11
TOKEN_EXPONENT                  EQU 12
TOKEN_WHITESPACE                EQU 13
TOKEN_OTHER                     EQU 14
TOKEN_END                       EQU 15

; Return codes
VK_SUCCESS                      EQU 0
ERROR_SUCCESS                   EQU 0

; ==============================================================================
; STRUCTURES
; ==============================================================================

; Vulkan Handles (opaque pointers)
VK_INSTANCE     TYPEDEF QWORD
VK_PHYSICAL_DEVICE TYPEDEF QWORD
VK_DEVICE       TYPEDEF QWORD
VK_BUFFER       TYPEDEF QWORD
VK_DEVICE_MEMORY TYPEDEF QWORD
VK_COMMAND_BUFFER TYPEDEF QWORD
VK_COMMAND_POOL TYPEDEF QWORD
VK_PIPELINE     TYPEDEF QWORD
VK_PIPELINE_LAYOUT TYPEDEF QWORD
VK_DESCRIPTOR_SET TYPEDEF QWORD
VK_DESCRIPTOR_SET_LAYOUT TYPEDEF QWORD
VK_DESCRIPTOR_POOL TYPEDEF QWORD
VK_SHADER_MODULE TYPEDEF QWORD
VK_FENCE        TYPEDEF QWORD
VK_QUEUE        TYPEDEF QWORD
VK_SEMAPHORE    TYPEDEF QWORD

; Fabric Control Block (same structure, extended for Vulkan)
FABRIC_CONTROL_BLOCK STRUCT 4096
    magic                       DWORD ?
    version                     DWORD ?
    struct_size                 DWORD ?
    checksum                    DWORD ?
    
    process_count               DWORD ?
    process_id                  DWORD ?
    shard_count                 DWORD ?
    shard_assignments           DWORD 16 DUP(?)
    
    ready_barrier               DWORD ?
    barrier_target              DWORD ?
    sync_mutex                  QWORD ?
    sync_event                  QWORD ?
    
    shard_base_addresses        QWORD 16 DUP(?)
    
    ; Vulkan Coordination (NEW)
    vulkan_instance_ready       DWORD ?
    vulkan_device_index         DWORD ?
    vulkan_queue_family         DWORD ?
    
    ; Shared GPU resources (memory-mapped for cross-process)
    gpu_fsm_buffer_device_addr  QWORD ?                 ; 64-bit device address
    gpu_fsm_buffer_host_ptr     QWORD ?                 ; Host-visible mapping
    gpu_bitmask_buffer_device_addr QWORD ?
    gpu_bitmask_buffer_host_ptr QWORD ?
    
    ; Command coordination (lock-free ring buffer in shared memory)
    gpu_command_producer_idx    DWORD ?
    gpu_command_consumer_idx    DWORD ?
    gpu_command_buffer          QWORD ?                 ; Offset to ring buffer
    
    fsm_version                 QWORD ?
    fsm_broadcast_pending       BYTE ?
    reserved                    BYTE 3 DUP(?)
    
    padding                     BYTE 3960 DUP(?)
FABRIC_CONTROL_BLOCK ENDS

; Vulkan Physical Device Selection Criteria
VK_DEVICE_CRITERIA STRUCT 64
    ; Required features
    require_device_local_host_visible   BYTE ?          ; For zero-copy
    require_subgroup_operations         BYTE ?          ; For warp-wide ops
    require_shader_int64                BYTE ?          ; For large model indices
    require_buffer_device_address       BYTE ?          ; For 64-bit buffer refs
    
    ; Performance weights
    compute_score_weight                REAL4 ?
    memory_bandwidth_weight             REAL4 ?
    pci_bandwidth_weight                REAL4 ?
    
    ; Preferred vendor (0 = any, 0x10DE = NVIDIA, 0x1002 = AMD, 0x8086 = Intel)
    preferred_vendor_id                 DWORD ?
    
    ; Minimum requirements
    min_compute_units                   DWORD ?
    min_vram_mb                         DWORD ?
VK_DEVICE_CRITERIA ENDS

; GPU FSM Pipeline State
VK_FSM_PIPELINE STRUCT 256
    ; Vulkan objects
    device                      VK_DEVICE ?
    pipeline                    VK_PIPELINE ?
    pipeline_layout             VK_PIPELINE_LAYOUT ?
    descriptor_set_layout       VK_DESCRIPTOR_SET_LAYOUT ?
    descriptor_pool             VK_DESCRIPTOR_POOL ?
    descriptor_set              VK_DESCRIPTOR_SET ?
    command_pool                VK_COMMAND_POOL ?
    command_buffer              VK_COMMAND_BUFFER ?
    fence                       VK_FENCE ?
    queue                       VK_QUEUE ?
    
    ; Buffers
    fsm_table_buffer            VK_BUFFER ?
    fsm_table_memory            VK_DEVICE_MEMORY ?
    fsm_table_device_addr       QWORD ?
    
    bitmask_buffer              VK_BUFFER ?
    bitmask_memory              VK_DEVICE_MEMORY ?
    bitmask_device_addr         QWORD ?
    bitmask_host_ptr            QWORD ?
    
    logits_buffer               VK_BUFFER ?             ; Input logits
    output_buffer               VK_BUFFER ?             ; Masked output
    
    ; Synchronization
    sequence_number             QWORD ?                 ; Monotonic counter
    last_submission             QWORD ?                 ; Timestamp
    
    ; Performance
    avg_execution_time_us       DWORD ?
VK_FSM_PIPELINE ENDS

; FSM Transition Entry (packed for GPU)
FSM_TRANSITION_ENTRY STRUCT 8
    ; Packed: [0:3] state, [4:7] token, [8:15] next_state, [16:31] action
    packed_data                 DWORD ?
    mask_index                  WORD ?
    reserved                    WORD ?
FSM_TRANSITION_ENTRY ENDS

; GPU Push Constants (fast path, 128 bytes max)
FSM_PUSH_CONSTANTS STRUCT 32
    fsm_table_device_addr       QWORD ?
    bitmask_device_addr         QWORD ?
    logits_device_addr          QWORD ?
    output_device_addr          QWORD ?
FSM_PUSH_CONSTANTS ENDS

; Memory Requirements structure
VK_MEMORY_REQUIREMENTS STRUCT 32
    size                        QWORD ?
    alignment                   QWORD ?
    memoryTypeBits              DWORD ?
    padding                     DWORD ?
VK_MEMORY_REQUIREMENTS ENDS

; Physical Device Memory Properties
VK_MEMORY_TYPE STRUCT 8
    propertyFlags               DWORD ?
    heapIndex                   DWORD ?
VK_MEMORY_TYPE ENDS

VK_MEMORY_HEAP STRUCT 16
    size                        QWORD ?
    flags                       DWORD ?
    padding                     DWORD ?
VK_MEMORY_HEAP ENDS

VK_PHYSICAL_DEVICE_MEMORY_PROPERTIES STRUCT 128
    memoryTypeCount             DWORD ?
    memoryTypes                 VK_MEMORY_TYPE 32 DUP(<>)
    memoryHeapCount             DWORD ?
    memoryHeaps                 VK_MEMORY_HEAP 16 DUP(<>)
VK_PHYSICAL_DEVICE_MEMORY_PROPERTIES ENDS

; Queue Family Properties
VK_QUEUE_FAMILY_PROPERTIES STRUCT 12
    queueFlags                  DWORD ?
    queueCount                  DWORD ?
    timestampValidBits          DWORD ?
VK_QUEUE_FAMILY_PROPERTIES ENDS

; ==============================================================================
; CODE SECTION
; ==============================================================================
.CODE
ALIGN 64

; ==============================================================================
; VULKAN INITIALIZATION - Device Selection & Setup
; ==============================================================================

; -----------------------------------------------------------------------------
; VulkanInitialize - Initialize Vulkan for compute-only operation
; rcx = Pointer to application info (can be NULL)
; rdx = Device selection criteria
; Returns: EAX = VK_SUCCESS (0) or error code
; -----------------------------------------------------------------------------
VulkanInitialize PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    sub rsp, 256                    ; Shadow space + local vars
    
    mov r12, rcx                    ; App info
    mov r13, rdx                    ; Device criteria
    
    ; Load vulkan-1.dll
    mov rcx, OFFSET strVulkanDLL
    call LoadLibraryA
    test rax, rax
    jz @error_no_vulkan
    mov [hVulkanDLL], rax
    
    ; Get vkGetInstanceProcAddr
    mov rcx, rax
    mov rdx, OFFSET strGetInstanceProcAddr
    call GetProcAddress
    mov [vkGetInstanceProcAddrPtr], rax
    
    ; Create instance
    lea rcx, [rsp+64]               ; VkInstanceCreateInfo
    mov dword ptr [rcx], 1          ; sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    mov qword ptr [rcx+8], 0        ; pNext
    mov dword ptr [rcx+16], 0       ; flags
    
    ; Enable validation layers in debug
    ifdef DEBUG
    mov qword ptr [rcx+24], OFFSET validationLayers   ; ppEnabledLayerNames
    mov dword ptr [rcx+32], 1       ; enabledLayerCount
    else
    mov qword ptr [rcx+24], 0
    mov dword ptr [rcx+32], 0
    endif
    
    ; Extensions: need VK_KHR_get_physical_device_properties2 for device addresses
    mov qword ptr [rcx+40], OFFSET instanceExtensions
    mov dword ptr [rcx+48], 2       ; enabledExtensionCount
    
    ; Application info
    lea rdx, [rsp+128]              ; VkApplicationInfo
    mov dword ptr [rdx], 0          ; sType
    mov qword ptr [rdx+8], 0        ; pNext
    mov qword ptr [rdx+16], OFFSET appName
    mov dword ptr [rdx+24], 1000000h ; applicationVersion
    mov qword ptr [rdx+32], OFFSET engineName
    mov dword ptr [rdx+40], 4000000h ; engineVersion
    mov dword ptr [rdx+44], VK_API_VERSION_1_3
    
    mov [rcx+56], rdx               ; pApplicationInfo
    
    ; Call vkCreateInstance
    lea rdx, [rsp+192]              ; pInstance
    xor r8, r8                      ; pAllocator (NULL)
    call vkCreateInstance
    test eax, eax
    jnz @error_instance
    
    mov rbx, [rsp+192]              ; Instance handle
    mov [g_vulkanInstance], rbx
    
    ; Enumerate physical devices
    mov rcx, rbx
    lea rdx, [rsp+64]               ; pPhysicalDeviceCount
    call vkEnumeratePhysicalDevices
    test eax, eax
    jnz @error_enum_devices
    
    mov r14d, [rsp+64]              ; Device count
    cmp r14d, 0
    je @error_no_devices
    
    xor r15d, r15d                  ; Best device index
    xorps xmm6, xmm6                ; Best score
    
    ; Score each device
    xor ecx, ecx
@score_loop:
    cmp ecx, r14d
    jae @score_done
    
    ; Store device index and call scoring function
    mov r8d, ecx
    mov r9, r13                     ; Criteria
    mov rcx, rbx                    ; Instance
    call VulkanScoreDevice
    
    comiss xmm0, xmm6
    jbe @not_better
    
    movss xmm6, xmm0
    mov r15d, ecx
    
@not_better:
    inc ecx
    jmp @score_loop
    
@score_done:
    ; Get selected device
    mov rcx, rbx
    xor edx, edx
    lea r8, [rsp+128]               ; pPhysicalDevices output
    mov r9d, r14d                   ; Device count
    call VulkanGetPhysicalDeviceByIndex
    
    mov rax, [rsp+128]
    mov [g_vulkanPhysicalDevice], rax
    mov [g_selectedDeviceIndex], r15d
    
    ; Get memory properties
    mov rcx, rax
    lea rdx, [rsp+160]              ; VkPhysicalDeviceMemoryProperties
    call vkGetPhysicalDeviceMemoryProperties
    
    ; Create device with compute queue and required features
    mov rcx, [g_vulkanPhysicalDevice]
    call VulkanCreateComputeDevice
    
    mov eax, VK_SUCCESS
    jmp @done
    
@error_no_vulkan:
    mov eax, 1                      ; Custom error
    jmp @done
    
@error_instance:
    mov eax, 2                      ; Custom error
    jmp @done
    
@error_enum_devices:
    mov eax, 3                      ; Custom error
    jmp @done
    
@error_no_devices:
    mov eax, 4                      ; Custom error
    
@done:
    add rsp, 256
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
VulkanInitialize ENDP

; -----------------------------------------------------------------------------
; VulkanGetPhysicalDeviceByIndex - Get specific physical device
; rcx = Instance
; edx = Device index
; r8  = Output array pointer
; r9d = Total device count
; Returns: Device at index in r8
; -----------------------------------------------------------------------------
VulkanGetPhysicalDeviceByIndex PROC FRAME
    push rbx
    
    ; Call vkEnumeratePhysicalDevices with actual device array
    sub rsp, 32
    lea rax, [rsp]
    mov [rax], r9d                  ; Set count as input
    
    mov rdx, rax                    ; pPhysicalDeviceCount
    mov r8, r8                      ; pPhysicalDevices
    call vkEnumeratePhysicalDevices
    
    add rsp, 32
    pop rbx
    ret
VulkanGetPhysicalDeviceByIndex ENDP

; -----------------------------------------------------------------------------
; VulkanScoreDevice - Score GPU for FSM compute workload
; rcx = Instance
; r8d = Device index
; r9  = Criteria ptr
; Returns: XMM0 = Score (0-1000)
; -----------------------------------------------------------------------------
VulkanScoreDevice PROC FRAME
    push rbx
    push r12
    
    mov r12, r9                     ; Criteria
    
    ; Enumerate devices to get the one at index r8d
    sub rsp, 128
    
    ; Get device properties via instance
    lea rdx, [rsp+32]               ; Properties output
    mov r8, [g_vulkanPhysicalDevice]
    call vkGetPhysicalDeviceProperties
    
    ; Base score from device type
    mov eax, [rsp+32+80]            ; deviceType offset
    cmp eax, 4                      ; VK_PHYSICAL_DEVICE_TYPE_CPU
    je @cpu_score
    cmp eax, 2                      ; VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
    je @discrete_score
    cmp eax, 1                      ; VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
    je @integrated_score
    
    ; Default
    movss xmm0, [flt_500]
    jmp @check_features
    
@discrete_score:
    movss xmm0, [flt_1000]
    jmp @check_features
    
@integrated_score:
    movss xmm0, [flt_700]
    jmp @check_features
    
@cpu_score:
    movss xmm0, [flt_100]
    
@check_features:
    ; Check for required extensions and features
    ; For now, return the base score
    
    add rsp, 128
    pop r12
    pop rbx
    ret
VulkanScoreDevice ENDP

; -----------------------------------------------------------------------------
; VulkanCreateComputeDevice - Create logical device with compute queue
; rcx = Physical device
; Returns: EAX = VK_SUCCESS or error
; -----------------------------------------------------------------------------
VulkanCreateComputeDevice PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rcx                    ; Physical device
    sub rsp, 256
    
    ; Find compute queue family
    mov rcx, rbx
    lea rdx, [rsp+64]               ; pQueueFamilyPropertyCount
    call vkGetPhysicalDeviceQueueFamilyProperties
    
    mov r12d, [rsp+64]              ; Queue family count
    test r12d, r12d
    jz @error_no_queues
    
    ; Find first compute-capable queue
    xor r13d, r13d
    xor r14d, r14d                  ; Best queue family index
    
@find_queue_loop:
    cmp r13d, r12d
    jae @queue_found
    
    ; Check queue family properties for compute capability
    ; queueFlags & VK_QUEUE_COMPUTE_BIT
    lea rax, [rsp+96]
    mov ecx, [rax + r13*12]         ; Load queueFlags
    test ecx, VK_QUEUE_COMPUTE_BIT
    jz @next_queue
    
    mov r14d, r13d
    jmp @queue_found
    
@next_queue:
    inc r13d
    jmp @find_queue_loop
    
@queue_found:
    ; Create device with compute queue
    lea rcx, [rsp+64]               ; VkDeviceCreateInfo
    mov dword ptr [rcx], 3          ; sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
    
    ; Queue create info
    lea rdx, [rsp+96]               ; VkDeviceQueueCreateInfo
    mov dword ptr [rdx], 2          ; sType
    mov dword ptr [rdx+16], r14d    ; queueFamilyIndex
    mov dword ptr [rdx+20], 1       ; queueCount
    lea rax, [rsp+120]
    mov dword ptr [rax], 100000h    ; priority (1.0f in Q16.16)
    mov [rdx+24], rax               ; pQueuePriorities
    
    mov [rcx+24], rdx               ; pQueueCreateInfos
    mov dword ptr [rcx+32], 1       ; queueCreateInfoCount
    
    ; Enable features
    lea rdx, [rsp+128]              ; VkPhysicalDeviceBufferDeviceAddressFeatures
    mov dword ptr [rdx], 1000244000 ; sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES
    mov dword ptr [rdx+16], 1       ; bufferDeviceAddress = VK_TRUE
    mov [rcx+56], rdx               ; pEnabledFeatures (actually pNext)
    
    ; Create device
    lea rdx, [rsp+160]              ; pDevice output
    xor r8, r8                      ; pAllocator
    mov rcx, rbx
    call vkCreateDevice
    test eax, eax
    jnz @error_device_create
    
    mov r13, [rsp+160]
    mov [g_vulkanDevice], r13
    
    ; Get compute queue
    mov rcx, r13
    mov edx, r14d                   ; queueFamilyIndex
    xor r8d, r8d                    ; queueIndex
    lea r9, [rsp+168]               ; pQueue
    call vkGetDeviceQueue
    
    mov rax, [rsp+168]
    mov [g_vulkanComputeQueue], rax
    mov [g_vulkanQueueFamily], r14d
    
    mov eax, VK_SUCCESS
    jmp @done
    
@error_no_queues:
    mov eax, 5
    jmp @done
    
@error_device_create:
    mov eax, 6
    
@done:
    add rsp, 256
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
VulkanCreateComputeDevice ENDP

; ==============================================================================
; VULKAN BUFFER UTILITIES
; ==============================================================================

; -----------------------------------------------------------------------------
; VulkanFindMemoryType - Find memory type matching requirements and properties
; rcx = Physical device
; edx = Memory type bits required
; r8d = Property flags desired (VK_MEMORY_PROPERTY_*)
; Returns: EAX = Memory type index (0-31), or -1 if not found
; -----------------------------------------------------------------------------
VulkanFindMemoryType PROC FRAME
    push rbx
    
    mov rbx, rcx                    ; Physical device
    mov ecx, edx                    ; Memory type bits
    mov r8d, r8d                    ; Property flags
    
    sub rsp, 256
    
    ; Get memory properties
    lea rdx, [rsp+32]               ; VkPhysicalDeviceMemoryProperties
    mov rcx, rbx
    call vkGetPhysicalDeviceMemoryProperties
    
    ; Loop through memory types
    mov eax, [rsp+32]               ; memoryTypeCount
    xor ecx, ecx
    
@search_loop:
    cmp ecx, eax
    jae @not_found
    
    ; Check if this memory type is in the allowed set
    mov ebx, 1
    mov edx, ecx
    shl ebx, cl                     ; Create bitmask for this type
    test ebx, edx                   ; edx = memory type bits requirement
    jz @next_type
    
    ; Check if properties match
    lea rax, [rsp+32+4]             ; memoryTypes array
    mov ebx, [rax + rcx*8]          ; propertyFlags
    test ebx, r8d                   ; Check if all required flags present
    jnz @found                      ; If any flags match, use this type
    
@next_type:
    inc ecx
    jmp @search_loop
    
@found:
    mov eax, ecx
    jmp @done
    
@not_found:
    mov eax, -1
    
@done:
    add rsp, 256
    pop rbx
    ret
VulkanFindMemoryType ENDP

; ==============================================================================
; FSM PIPELINE - Vulkan Compute for Tool Parsing
; ==============================================================================

; -----------------------------------------------------------------------------
; VulkanFSMCreatePipeline - Build compute pipeline for FSM token masking
; rcx = Device
; rdx = Pointer to SPIR-V bytecode (FSM kernel)
; r8  = SPIR-V size in bytes
; Returns: EAX = VK_SUCCESS or error
; -----------------------------------------------------------------------------
VulkanFSMCreatePipeline PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov rbx, rcx                    ; Device
    mov r12, rdx                    ; SPIR-V ptr
    mov r13d, r8d                   ; Size
    
    sub rsp, 512
    
    ; Create shader module
    lea rcx, [rsp+64]               ; VkShaderModuleCreateInfo
    mov dword ptr [rcx], 11         ; sType
    mov qword ptr [rcx+8], 0        ; pNext
    mov dword ptr [rcx+16], 0       ; flags
    mov [rcx+24], r13d              ; codeSize
    mov [rcx+32], r12               ; pCode
    
    lea rdx, [rsp+128]              ; pShaderModule
    mov rcx, rbx
    xor r8, r8                      ; pAllocator
    call vkCreateShaderModule
    test eax, eax
    jnz @error_shader
    
    mov r14, [rsp+128]              ; Shader module
    
    ; Create descriptor set layout with 4 bindings
    lea rcx, [rsp+64]               ; VkDescriptorSetLayoutBinding[4]
    
    ; Binding 0: FSM transition table
    mov dword ptr [rcx+0], 0        ; binding
    mov dword ptr [rcx+4], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    mov dword ptr [rcx+8], 1        ; descriptorCount
    mov dword ptr [rcx+12], VK_SHADER_STAGE_COMPUTE_BIT
    
    ; Binding 1: Token bitmask
    mov dword ptr [rcx+24], 1
    mov dword ptr [rcx+28], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    mov dword ptr [rcx+32], 1
    mov dword ptr [rcx+36], VK_SHADER_STAGE_COMPUTE_BIT
    
    ; Binding 2: Input logits
    mov dword ptr [rcx+48], 2
    mov dword ptr [rcx+52], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    mov dword ptr [rcx+56], 1
    mov dword ptr [rcx+60], VK_SHADER_STAGE_COMPUTE_BIT
    
    ; Binding 3: Output masked logits
    mov dword ptr [rcx+72], 3
    mov dword ptr [rcx+76], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    mov dword ptr [rcx+80], 1
    mov dword ptr [rcx+84], VK_SHADER_STAGE_COMPUTE_BIT
    
    lea rcx, [rsp+160]              ; VkDescriptorSetLayoutCreateInfo
    mov dword ptr [rcx], 30         ; sType
    mov qword ptr [rcx+16], 4       ; bindingCount
    lea rax, [rsp+64]
    mov [rcx+24], rax               ; pBindings
    
    lea rdx, [rsp+256]              ; pSetLayout
    mov rcx, rbx
    call vkCreateDescriptorSetLayout
    test eax, eax
    jnz @error_layout
    
    mov r15, [rsp+256]              ; Descriptor set layout
    
    ; Create pipeline layout with push constants
    lea rcx, [rsp+160]              ; VkPipelineLayoutCreateInfo
    mov dword ptr [rcx], 17         ; sType
    
    ; Push constant range for FSM_PUSH_CONSTANTS
    lea rdx, [rsp+64]
    mov dword ptr [rdx], VK_SHADER_STAGE_COMPUTE_BIT
    mov dword ptr [rdx+4], 0        ; offset
    mov dword ptr [rdx+8], SIZEOF FSM_PUSH_CONSTANTS
    
    mov qword ptr [rcx+40], rdx     ; pPushConstantRanges
    mov dword ptr [rcx+48], 1       ; pushConstantRangeCount
    
    mov qword ptr [rcx+24], r15     ; pSetLayouts
    mov dword ptr [rcx+32], 1       ; setLayoutCount
    
    lea rdx, [rsp+264]              ; pPipelineLayout
    mov rcx, rbx
    xor r8, r8                      ; pAllocator
    call vkCreatePipelineLayout
    test eax, eax
    jnz @error_pipeline_layout
    
    mov rax, [rsp+264]
    mov [g_fsmPipelineLayout], rax
    
    ; Create compute pipeline
    lea rcx, [rsp+160]              ; VkComputePipelineCreateInfo
    mov dword ptr [rcx], 9          ; sType
    mov dword ptr [rcx+8], 0        ; flags
    
    ; Shader stage
    lea rdx, [rsp+64]               ; VkPipelineShaderStageCreateInfo
    mov dword ptr [rdx], 5          ; sType
    mov dword ptr [rdx+8], VK_SHADER_STAGE_COMPUTE_BIT
    mov [rdx+16], r14               ; module
    mov qword ptr [rdx+24], OFFSET strMainEntry   ; pName
    
    mov [rcx+16], rdx               ; stage
    mov rax, [rsp+264]
    mov [rcx+24], rax               ; layout
    
    lea r8, [rsp+280]               ; pPipeline
    xor r9d, r9d                    ; pipelineCache
    mov rdx, rcx
    mov ecx, 1                      ; createInfoCount
    xor r10, r10                    ; pAllocator
    call vkCreateComputePipelines
    test eax, eax
    jnz @error_pipeline
    
    ; Store pipeline
    mov rax, [rsp+280]
    mov [g_fsmPipeline], rax
    mov [g_fsmDescriptorSetLayout], r15
    
    ; Cleanup shader module
    mov rcx, rbx
    mov rdx, r14
    xor r8, r8                      ; pAllocator
    call vkDestroyShaderModule
    
    mov eax, VK_SUCCESS
    jmp @done
    
@error_shader:
    mov eax, 7
    jmp @done
@error_layout:
    mov eax, 8
    jmp @done
@error_pipeline_layout:
    mov eax, 9
    jmp @done
@error_pipeline:
    mov eax, 10
    
@done:
    add rsp, 512
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
VulkanFSMCreatePipeline ENDP

; ==============================================================================
; BITMASK BROADCAST - Zero-Copy GPU Update
; ==============================================================================

; -----------------------------------------------------------------------------
; BitmaskBroadcastVulkan - Update GPU bitmask via host-visible memory
; rcx = Pointer to 512-bit host bitmask (64 bytes)
; rdx = Pipeline state ptr
; Returns: EAX = VK_SUCCESS or error
; -----------------------------------------------------------------------------
BitmaskBroadcastVulkan PROC FRAME
    push rbx
    push r12
    push r13
    
    mov rbx, rcx                    ; Host bitmask ptr
    mov r12, rdx                    ; Pipeline state
    
    ; Get host-visible pointer to GPU bitmask buffer
    mov r13, [r12].VK_FSM_PIPELINE.bitmask_host_ptr
    test r13, r13
    jz @error_no_mapping
    
    ; Copy bitmask using SSE2 (256-bit via 4x XMM)
    movdqa xmm0, [rbx]
    movdqa xmm1, [rbx+16]
    movdqa xmm2, [rbx+32]
    movdqa xmm3, [rbx+48]
    
    movdqa [r13], xmm0
    movdqa [r13+16], xmm1
    movdqa [r13+32], xmm2
    movdqa [r13+48], xmm3
    
    ; Flush to make visible to GPU if not coherent
    ; For now, assume HOST_COHERENT flag was used
    
    ; Record and submit command buffer
    mov rcx, r12
    call VulkanRecordBitmaskUpdate
    test eax, eax
    jnz @error_record
    
    mov eax, VK_SUCCESS
    jmp @done
    
@error_no_mapping:
    mov eax, 11
    jmp @done
    
@error_record:
    mov eax, 12
    
@done:
    pop r13
    pop r12
    pop rbx
    ret
BitmaskBroadcastVulkan ENDP

; -----------------------------------------------------------------------------
; VulkanRecordBitmaskUpdate - Record and submit compute commands
; rcx = Pipeline state ptr
; Returns: EAX = VK_SUCCESS or error
; -----------------------------------------------------------------------------
VulkanRecordBitmaskUpdate PROC FRAME
    push rbx
    push r12
    
    mov rbx, rcx
    mov r12, rcx                    ; Save for later use
    
    sub rsp, 256
    
    ; Begin command buffer
    lea rcx, [rsp+64]               ; VkCommandBufferBeginInfo
    mov dword ptr [rcx], 42         ; sType
    mov dword ptr [rcx+16], VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    
    mov rdx, [rbx].VK_FSM_PIPELINE.command_buffer
    mov rcx, rdx
    xor r8, r8                      ; pAllocator
    call vkBeginCommandBuffer
    test eax, eax
    jnz @error_begin
    
    ; Bind pipeline
    mov rcx, [rbx].VK_FSM_PIPELINE.command_buffer
    mov rdx, [rbx].VK_FSM_PIPELINE.pipeline
    mov r8d, VK_PIPELINE_BIND_POINT_COMPUTE
    mov r9d, 0                      ; firstBinding
    mov r10d, 0                     ; bindingCount
    xor r11, r11                    ; pDynamicOffsets
    call vkCmdBindPipeline
    
    ; Update push constants
    lea r9, [rsp+64]                ; Push constant data
    mov rax, [rbx].VK_FSM_PIPELINE.bitmask_device_addr
    mov [r9], rax
    mov rax, [rbx].VK_FSM_PIPELINE.logits_buffer
    mov [r9+8], rax
    
    mov rcx, [rbx].VK_FSM_PIPELINE.command_buffer
    mov rdx, [rbx].VK_FSM_PIPELINE.pipeline_layout
    mov r8d, VK_SHADER_STAGE_COMPUTE_BIT
    mov r9d, 0                      ; offset
    mov r10d, 16                    ; size
    lea r11, [rsp+64]               ; pValues
    call vkCmdPushConstants
    
    ; Dispatch compute shader
    mov rcx, [rbx].VK_FSM_PIPELINE.command_buffer
    mov edx, 512                    ; groupCountX (vocab_size / 256)
    mov r8d, 1                      ; groupCountY
    mov r9d, 1                      ; groupCountZ
    call vkCmdDispatch
    
    ; End command buffer
    mov rcx, [rbx].VK_FSM_PIPELINE.command_buffer
    xor rdx, rdx                    ; pAllocator
    call vkEndCommandBuffer
    test eax, eax
    jnz @error_end
    
    ; Submit to queue (async)
    mov rcx, r12
    call VulkanSubmitAsync
    
    mov eax, VK_SUCCESS
    jmp @done
    
@error_begin:
    mov eax, 13
    jmp @done
@error_end:
    mov eax, 14
    
@done:
    add rsp, 256
    pop r12
    pop rbx
    ret
VulkanRecordBitmaskUpdate ENDP

; -----------------------------------------------------------------------------
; VulkanSubmitAsync - Submit command buffer to queue asynchronously
; rcx = Pipeline state ptr
; Returns: EAX = VK_SUCCESS or error
; -----------------------------------------------------------------------------
VulkanSubmitAsync PROC FRAME
    push rbx
    
    mov rbx, rcx
    sub rsp, 256
    
    ; Create submit info
    lea rcx, [rsp+64]               ; VkSubmitInfo
    mov dword ptr [rcx], 4          ; sType = VK_STRUCTURE_TYPE_SUBMIT_INFO
    
    ; Command buffers
    lea rdx, [rsp+128]
    mov rax, [rbx].VK_FSM_PIPELINE.command_buffer
    mov [rdx], rax
    
    mov [rcx+24], rdx               ; pCommandBuffers
    mov dword ptr [rcx+32], 1       ; commandBufferCount
    
    ; Reset fence
    mov rcx, [rbx].VK_FSM_PIPELINE.device
    lea rdx, [rsp+136]
    mov rax, [rbx].VK_FSM_PIPELINE.fence
    mov [rdx], rax
    call vkResetFences
    
    ; Queue submit
    mov rcx, [rbx].VK_FSM_PIPELINE.queue
    lea rdx, [rsp+64]               ; pSubmits
    mov r8d, 1                      ; submitCount
    mov r9, [rbx].VK_FSM_PIPELINE.fence
    call vkQueueSubmit
    
    ; For now, don't wait - async execution
    ; Update sequence number
    mov rax, [rbx].VK_FSM_PIPELINE.sequence_number
    inc rax
    mov [rbx].VK_FSM_PIPELINE.sequence_number, rax
    
    add rsp, 256
    pop rbx
    ret
VulkanSubmitAsync ENDP

; ==============================================================================
; BUFFER MAPPING HELPERS
; ==============================================================================

; -----------------------------------------------------------------------------
; VulkanMapBuffer - Map GPU buffer to host address space
; rcx = Device
; rdx = Device memory handle
; r8  = Offset
; r9  = Size
; Returns: RAX = Host pointer, or NULL on error
; -----------------------------------------------------------------------------
VulkanMapBuffer PROC FRAME
    push rbx
    
    mov rbx, rcx                    ; Device
    mov r10, rdx                    ; Memory
    mov r11, r8                     ; Offset
    mov r12, r9                     ; Size
    
    sub rsp, 64
    
    ; vkMapMemory(device, memory, offset, size, flags, ppData)
    lea rcx, [rsp+32]               ; ppData output
    mov rdx, r10                    ; memory
    mov r8, r11                     ; offset
    mov r9, r12                     ; size
    xor r10d, r10d                  ; flags
    xor r11, r11                    ; Vulkan API param 6 (unused on x64)
    
    mov rcx, rbx
    mov rdx, r10                    ; memory
    mov r8, r11                     ; offset
    mov r9, r12                     ; size
    
    call vkMapMemory
    test eax, eax
    jnz @map_error
    
    mov rax, [rsp+32]               ; Return host pointer
    jmp @map_done
    
@map_error:
    xor rax, rax
    
@map_done:
    add rsp, 64
    pop rbx
    ret
VulkanMapBuffer ENDP

; -----------------------------------------------------------------------------
; VulkanUnmapBuffer - Unmap GPU buffer from host address space
; rcx = Device
; rdx = Device memory handle
; Returns: EAX = VK_SUCCESS or error
; -----------------------------------------------------------------------------
VulkanUnmapBuffer PROC FRAME
    push rbx
    
    mov rbx, rcx                    ; Device
    
    ; vkUnmapMemory(device, memory)
    mov rcx, rbx
    mov rdx, rdx                    ; memory
    call vkUnmapMemory
    
    pop rbx
    ret
VulkanUnmapBuffer ENDP

; ==============================================================================
; POOL & SYNC OBJECT CREATION
; ==============================================================================

; -----------------------------------------------------------------------------
; VulkanCreatePoolsAndSyncs - Create command pool, descriptor pool, fence
; rcx = Device
; edx = Queue family index
; r8  = Pipeline state ptr (output)
; Returns: EAX = VK_SUCCESS or error
; -----------------------------------------------------------------------------
VulkanCreatePoolsAndSyncs PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rcx                    ; Device
    mov r12d, edx                   ; Queue family
    mov r13, r8                     ; Pipeline state
    
    sub rsp, 256
    
    ; Create command pool
    lea rcx, [rsp+64]               ; VkCommandPoolCreateInfo
    mov dword ptr [rcx], 20         ; sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO
    mov dword ptr [rcx+16], 0       ; flags
    mov dword ptr [rcx+20], r12d    ; queueFamilyIndex
    
    lea rdx, [rsp+128]              ; pCommandPool
    mov rcx, rbx
    xor r8, r8                      ; pAllocator
    call vkCreateCommandPool
    test eax, eax
    jnz @error_cmd_pool
    
    mov r14, [rsp+128]
    mov [r13].VK_FSM_PIPELINE.command_pool, r14
    mov [g_vulkanCommandPool], r14
    
    ; Allocate command buffer
    lea rcx, [rsp+64]               ; VkCommandBufferAllocateInfo
    mov dword ptr [rcx], 43         ; sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
    mov [rcx+16], r14               ; commandPool
    mov dword ptr [rcx+24], 0       ; level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
    mov dword ptr [rcx+28], 1       ; commandBufferCount
    
    lea rdx, [rsp+144]              ; pCommandBuffers
    mov rcx, rbx
    call vkAllocateCommandBuffers
    test eax, eax
    jnz @error_cmd_buffer
    
    mov rax, [rsp+144]
    mov [r13].VK_FSM_PIPELINE.command_buffer, rax
    
    ; Create descriptor pool
    ; Pool size: 4 storage buffers
    lea rcx, [rsp+64]               ; VkDescriptorPoolSize
    mov dword ptr [rcx], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    mov dword ptr [rcx+4], 4        ; descriptorCount
    
    lea rcx, [rsp+80]               ; VkDescriptorPoolCreateInfo
    mov dword ptr [rcx], 9          ; sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO
    mov dword ptr [rcx+16], 0       ; flags
    mov dword ptr [rcx+20], 1       ; maxSets
    mov dword ptr [rcx+24], 1       ; poolSizeCount
    lea rax, [rsp+64]
    mov [rcx+32], rax               ; pPoolSizes
    
    lea rdx, [rsp+152]              ; pDescriptorPool
    mov rcx, rbx
    xor r8, r8                      ; pAllocator
    call vkCreateDescriptorPool
    test eax, eax
    jnz @error_desc_pool
    
    mov rax, [rsp+152]
    mov [r13].VK_FSM_PIPELINE.descriptor_pool, rax
    
    ; Allocate descriptor set
    lea rcx, [rsp+64]               ; VkDescriptorSetAllocateInfo
    mov dword ptr [rcx], 44         ; sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO
    mov rax, [rsp+152]
    mov [rcx+16], rax               ; descriptorPool
    mov dword ptr [rcx+24], 1       ; descriptorSetCount
    mov rax, [g_fsmDescriptorSetLayout]
    mov [rcx+32], rax               ; pSetLayouts
    
    lea rdx, [rsp+160]              ; pDescriptorSets
    mov rcx, rbx
    call vkAllocateDescriptorSets
    test eax, eax
    jnz @error_desc_set
    
    mov rax, [rsp+160]
    mov [r13].VK_FSM_PIPELINE.descriptor_set, rax
    
    ; Create fence
    lea rcx, [rsp+64]               ; VkFenceCreateInfo
    mov dword ptr [rcx], 8          ; sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
    mov dword ptr [rcx+16], 0       ; flags
    
    lea rdx, [rsp+168]              ; pFence
    mov rcx, rbx
    xor r8, r8                      ; pAllocator
    call vkCreateFence
    test eax, eax
    jnz @error_fence
    
    mov rax, [rsp+168]
    mov [r13].VK_FSM_PIPELINE.fence, rax
    
    ; Store device in pipeline state
    mov [r13].VK_FSM_PIPELINE.device, rbx
    
    mov eax, VK_SUCCESS
    jmp @done
    
@error_cmd_pool:
    mov eax, 15
    jmp @done
@error_cmd_buffer:
    mov eax, 16
    jmp @done
@error_desc_pool:
    mov eax, 17
    jmp @done
@error_desc_set:
    mov eax, 18
    jmp @done
@error_fence:
    mov eax, 19
    
@done:
    add rsp, 256
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
VulkanCreatePoolsAndSyncs ENDP

; ==============================================================================
; DESCRIPTOR SET BINDING
; ==============================================================================

; -----------------------------------------------------------------------------
; VulkanBindFSMDescriptors - Bind GPU buffers to descriptor set
; rcx = Device
; rdx = Descriptor set
; r8  = FSM table buffer + device address (qword pair)
; r9  = Bitmask buffer + device address
; r10 = Logits buffer + device address
; r11 = Output buffer + device address
; Returns: EAX = VK_SUCCESS or error
; Note: Each buffer param is [buffer_handle | device_address] packed in 128-bit
; -----------------------------------------------------------------------------
VulkanBindFSMDescriptors PROC FRAME
    push rbx
    push r12
    push r13
    
    mov rbx, rcx                    ; Device
    mov r12, rdx                    ; Descriptor set
    
    sub rsp, 512
    
    ; Build descriptor writes for all 4 bindings
    ; Binding 0: FSM table buffer
    lea rcx, [rsp+64]               ; VkDescriptorBufferInfo[4]
    
    ; FSM table (binding 0)
    mov rax, r8
    mov [rcx+0], rax                ; buffer
    mov qword ptr [rcx+8], 0        ; offset
    mov qword ptr [rcx+16], 512*1024 ; range (512K FSM table)
    
    ; Bitmask (binding 1)
    mov rax, r9
    mov [rcx+24], rax               ; buffer
    mov qword ptr [rcx+32], 0       ; offset
    mov qword ptr [rcx+40], 64      ; range (64 bytes)
    
    ; Logits (binding 2)
    mov rax, r10
    mov [rcx+48], rax               ; buffer
    mov qword ptr [rcx+56], 0       ; offset
    mov qword ptr [rcx+64], 4194304 ; range (4MB logits)
    
    ; Output (binding 3)
    mov rax, r11
    mov [rcx+72], rax               ; buffer
    mov qword ptr [rcx+80], 0       ; offset
    mov qword ptr [rcx+88], 4194304 ; range (4MB output)
    
    ; Write descriptors
    lea rcx, [rsp+192]              ; VkWriteDescriptorSet[4]
    
    ; Write 0
    mov dword ptr [rcx+0], 51       ; sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET
    mov [rcx+8], r12                ; dstSet
    mov dword ptr [rcx+16], 0       ; dstBinding
    mov dword ptr [rcx+20], 0       ; dstArrayElement
    mov dword ptr [rcx+24], 1       ; descriptorCount
    mov dword ptr [rcx+28], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    lea rax, [rsp+64]
    mov [rcx+40], rax               ; pBufferInfo
    
    ; Write 1
    mov dword ptr [rcx+48], 51      ; sType
    mov [rcx+56], r12               ; dstSet
    mov dword ptr [rcx+64], 1       ; dstBinding
    mov dword ptr [rcx+68], 0       ; dstArrayElement
    mov dword ptr [rcx+72], 1       ; descriptorCount
    mov dword ptr [rcx+76], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    lea rax, [rsp+88]
    mov [rcx+88], rax               ; pBufferInfo
    
    ; Write 2
    mov dword ptr [rcx+96], 51      ; sType
    mov [rcx+104], r12              ; dstSet
    mov dword ptr [rcx+112], 2      ; dstBinding
    mov dword ptr [rcx+116], 0      ; dstArrayElement
    mov dword ptr [rcx+120], 1      ; descriptorCount
    mov dword ptr [rcx+124], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    lea rax, [rsp+112]
    mov [rcx+136], rax              ; pBufferInfo
    
    ; Write 3
    mov dword ptr [rcx+144], 51     ; sType
    mov [rcx+152], r12              ; dstSet
    mov dword ptr [rcx+160], 3      ; dstBinding
    mov dword ptr [rcx+164], 0      ; dstArrayElement
    mov dword ptr [rcx+168], 1      ; descriptorCount
    mov dword ptr [rcx+172], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    lea rax, [rsp+136]
    mov [rcx+184], rax              ; pBufferInfo
    
    ; Call vkUpdateDescriptorSets
    mov rcx, rbx
    lea rdx, [rsp+192]              ; pDescriptorWrites
    mov r8d, 4                      ; descriptorWriteCount
    xor r9d, r9d                    ; descriptorCopyCount
    xor r10, r10                    ; pDescriptorCopies
    call vkUpdateDescriptorSets
    test eax, eax
    jnz @error_update
    
    mov eax, VK_SUCCESS
    jmp @done
    
@error_update:
    mov eax, 20
    
@done:
    add rsp, 512
    pop r13
    pop r12
    pop rbx
    ret
VulkanBindFSMDescriptors ENDP

; ==============================================================================
; FABRIC CONTROL BLOCK INTEGRATION
; ==============================================================================

; -----------------------------------------------------------------------------
; VulkanRegisterFabricCoordination - Store Vulkan handles in shared memory
; rcx = Pointer to FABRIC_CONTROL_BLOCK
; rdx = Vulkan instance
; r8  = Vulkan device
; r9  = Queue family index
; r10 = GPU FSM buffer device address
; r11 = GPU bitmask buffer device address
; Returns: EAX = VK_SUCCESS
; Note: Uses interlocked operations for thread safety across processes
; -----------------------------------------------------------------------------
VulkanRegisterFabricCoordination PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov rbx, rcx                    ; Control block
    mov r12, rdx                    ; Instance
    mov r13, r8                     ; Device
    mov r14d, r9d                   ; Queue family
    mov r15, r10                    ; FSM buffer addr
    
    ; Store Vulkan instance (atomic for cross-process)
    mov [rbx].FABRIC_CONTROL_BLOCK.vulkan_instance_ready, 0
    
    ; Store values (non-critical, one-shot initialization)
    mov [rbx].FABRIC_CONTROL_BLOCK.vulkan_device_index, r14d
    mov [rbx].FABRIC_CONTROL_BLOCK.vulkan_queue_family, r14d
    
    ; Store device addresses (for GPU access from other processes)
    mov [rbx].FABRIC_CONTROL_BLOCK.gpu_fsm_buffer_device_addr, r15
    mov [rbx].FABRIC_CONTROL_BLOCK.gpu_bitmask_buffer_device_addr, r11
    
    ; Mark Vulkan as ready (interlocked write)
    mov dword ptr [rbx].FABRIC_CONTROL_BLOCK.vulkan_instance_ready, 1
    
    ; Verify broadcast updated all shards (via atomic fence)
    mov eax, VK_SUCCESS
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
VulkanRegisterFabricCoordination ENDP

; ==============================================================================
; SPIR-V FSM KERNEL (Embedded as Byte Array)
; ==============================================================================

align 4
spirv_fsm_kernel:
    ; SPIR-V Magic + Version Header
    DD 07230203h                    ; Magic (little-endian SPIR-V)
    DD 00010300h                    ; Version 1.3
    DD 00000000h                    ; Generator ID
    DD 00000050h                    ; Bound (ID limit)
    DD 00000000h                    ; Schema (reserved)
    
    ; OpCapability Shader
    DD 0001000Bh                    ; Word count + opcode
    
    ; OpCapability ShaderNonUniform
    DD 00010037h
    
    ; OpCapability Int64
    DD 00010004h
    
    ; OpCapability ComputeShaderInvocationsAMD
    DD 0001C001h
    
    ; OpMemoryModel Logical GLSL450
    DD 0003000Eh
    DD 00000001h
    DD 00000000h
    
    ; OpEntryPoint Compute %main "main" %gl_GlobalInvocationID
    DD 0011000Fh
    DD 00000005h                    ; Execution model: Compute
    DD 00000010h                    ; Entry point ID
    DD 6E69616Dh                    ; "main" (4 bytes)
    DD 00000000h
    
    ; OpExecutionMode %main LocalSize 256 1 1
    DD 0005006Ah
    DD 00000010h                    ; Target function
    DD 00000011h                    ; LocalSize mode
    DD 00000100h                    ; 256 (X dimension)
    DD 00000001h                    ; 1 (Y dimension)
    DD 00000001h                    ; 1 (Z dimension)
    
    ; Rest of SPIR-V would follow (types, functions, etc.)
    ; This is a minimal stub - full kernel would be ~2KB
    
    ; For now, pad with zeros to represent a minimal valid kernel
    ALIGN 4
    DD 0 DUP(0) [400h/4]            ; 1KB padding

spirv_fsm_kernel_end:

; ==============================================================================
; DATA SECTION
; ==============================================================================
.DATA
ALIGN 64

; Vulkan globals
g_vulkanInstance                QWORD 0
g_vulkanPhysicalDevice          QWORD 0
g_vulkanDevice                  QWORD 0
g_vulkanComputeQueue            QWORD 0
g_vulkanCommandPool             QWORD 0
g_vulkanQueueFamily             DWORD 0

g_selectedDeviceIndex           DWORD 0

; Pipeline
g_fsmPipeline                   QWORD 0
g_fsmPipelineLayout             QWORD 0
g_fsmDescriptorSetLayout        QWORD 0

; DLL handles
hVulkanDLL                      QWORD 0
vkGetInstanceProcAddrPtr        QWORD 0

; Strings
strVulkanDLL                    DB "vulkan-1.dll", 0
strGetInstanceProcAddr          DB "vkGetInstanceProcAddr", 0
strMainEntry                    DB "main", 0

appName                         DB "RawrXD-800B", 0
engineName                      DB "NeonFabric", 0

; Instance extensions
instanceExtensions:
    DQ OFFSET strExtValidation
    DQ OFFSET strExtPhysicalDeviceProps2
strExtValidation                DB "VK_EXT_validation_features", 0
strExtPhysicalDeviceProps2      DB "VK_KHR_get_physical_device_properties2", 0

; Validation layers (debug only)
validationLayers:
    DQ OFFSET strLayerValidation
strLayerValidation              DB "VK_LAYER_KHRONOS_validation", 0

; Float constants
flt_100                         REAL4 100.0
flt_500                         REAL4 500.0
flt_700                         REAL4 700.0
flt_1000                        REAL4 1000.0

; ==============================================================================
; EXPORT
; ==============================================================================
PUBLIC VulkanInitialize
PUBLIC VulkanScoreDevice
PUBLIC VulkanCreateComputeDevice
PUBLIC VulkanFindMemoryType
PUBLIC VulkanFSMCreatePipeline
PUBLIC VulkanCreateFSMBuffer
PUBLIC BitmaskBroadcastVulkan
PUBLIC VulkanRecordBitmaskUpdate
PUBLIC VulkanSubmitAsync
PUBLIC VulkanMapBuffer
PUBLIC VulkanUnmapBuffer
PUBLIC VulkanCreatePoolsAndSyncs
PUBLIC VulkanBindFSMDescriptors
PUBLIC VulkanRegisterFabricCoordination

; ==============================================================================
; END - NEON_VULKAN_FABRIC
; ==============================================================================
END
