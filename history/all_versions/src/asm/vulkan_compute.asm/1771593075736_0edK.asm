; vulkan_compute.asm - Pure MASM64 Vulkan Compute Backend
; Reverse engineered from C++ to eliminate CRT/std dependencies
; Assemble: ml64 /c /Fo vulkan_compute.obj vulkan_compute.asm

 includelib kernel32.lib
 includelib vulkan-1.lib

 EXTERN vkCreateInstance:PROC
 EXTERN vkDestroyInstance:PROC
 EXTERN vkEnumeratePhysicalDevices:PROC
 EXTERN vkGetPhysicalDeviceQueueFamilyProperties:PROC
 EXTERN vkCreateDevice:PROC
 EXTERN vkDestroyDevice:PROC
 EXTERN vkGetDeviceQueue:PROC
 EXTERN vkCreateCommandPool:PROC
 EXTERN vkDestroyCommandPool:PROC
 EXTERN vkAllocateCommandBuffers:PROC
 EXTERN vkFreeCommandBuffers:PROC
 EXTERN vkDeviceWaitIdle:PROC

 ; VulkanCompute struct offsets (matching C++ class layout)
 VKC_INITIALIZED      equ 0   ; bool (1 byte)
 VKC_INSTANCE         equ 8   ; VkInstance (8 bytes)
 VKC_PHYSICAL_DEVICE  equ 16  ; VkPhysicalDevice
 VKC_DEVICE           equ 24  ; VkDevice
 VKC_COMPUTE_QUEUE    equ 32  ; VkQueue
 VKC_COMMAND_POOL     equ 40  ; VkCommandPool
 VKC_ASYNC_POOL       equ 48  ; VkCommandPool
 VKC_QUEUE_FAMILY_IDX equ 56  ; uint32

 .const
 VK_STRUCTURE_TYPE_APPLICATION_INFO      equ 0
 VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO  equ 1
 VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO equ 2
 VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO    equ 3
 VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO equ 39
 VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO equ 40
 VK_COMMAND_BUFFER_LEVEL_PRIMARY         equ 0
 VK_QUEUE_COMPUTE_BIT                    equ 00000002h
 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT equ 00000002h
 VK_COMMAND_POOL_CREATE_TRANSIENT_BIT    equ 00000001h
 VK_SUCCESS                              equ 0

 .data
 align 8
 g_vulkan_initialized dq 0
 app_name             db "RawrXD",0
 engine_name          db "RawrXD-Vulkan",0

 .code
 align 16

 ; Export public symbols
 PUBLIC VulkanCompute_Initialize
 PUBLIC VulkanCompute_Shutdown
 PUBLIC VulkanCompute_SelectPhysicalDevice
 PUBLIC VulkanCompute_CreateLogicalDevice
 PUBLIC VulkanCompute_InitializeCommandBufferPool
 PUBLIC VulkanCompute_AcquireCommandBuffer

 ; bool VulkanCompute_Initialize(void* this_ptr)
 VulkanCompute_Initialize PROC FRAME
     push rbx
     .pushreg rbx
     push rdi
     .pushreg rdi
     push rsi
     .pushreg rsi
     sub rsp, 0xA0
     .allocstack 0xA0
     .endprolog

     mov rbx, rcx           ; RBX = this pointer
     
     ; Check if already initialized
     cmp byte ptr [rbx+VKC_INITIALIZED], 0
     jne init_done_true

     ; Zero struct memory
     xor eax, eax
     mov rdi, rbx
     mov rcx, 8
     rep stosq

     ; Setup ApplicationInfo on stack
     lea rdi, [rsp+0x40]    ; appInfo
     mov dword ptr [rdi], VK_STRUCTURE_TYPE_APPLICATION_INFO
     mov qword ptr [rdi+8], 0
     lea rax, app_name
     mov qword ptr [rdi+16], rax
     mov dword ptr [rdi+24], 00010000h
     lea rax, engine_name
     mov qword ptr [rdi+32], rax
     mov dword ptr [rdi+40], 00010000h
     mov dword ptr [rdi+44], 00402000h  ; VK_API_VERSION_1_2

     ; Setup InstanceCreateInfo
     lea rsi, [rsp+0x80]
     mov dword ptr [rsi], VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
     mov qword ptr [rsi+8], 0
     mov dword ptr [rsi+16], 0
     mov qword ptr [rsi+24], rdi
     mov dword ptr [rsi+32], 0
     mov qword ptr [rsi+40], 0
     mov dword ptr [rsi+48], 0
     mov qword ptr [rsi+56], 0

     ; Call vkCreateInstance
     lea r8, [rbx+VKC_INSTANCE]
     mov rdx, 0
     mov rcx, rsi
     call vkCreateInstance
     
     cmp eax, VK_SUCCESS
     jne init_failed

     ; Select physical device
     mov rcx, rbx
     call VulkanCompute_SelectPhysicalDevice
     test al, al
     jz init_failed_cleanup

     ; Create logical device
     mov rcx, rbx
     call VulkanCompute_CreateLogicalDevice
     test al, al
     jz init_failed_cleanup

     ; Initialize command pools
     mov rcx, rbx
     call VulkanCompute_InitializeCommandBufferPool
     test al, al
     jz init_failed_cleanup

     ; Mark initialized
     mov byte ptr [rbx+VKC_INITIALIZED], 1
     mov rax, 1
     jmp init_done

 init_done_true:
     mov rax, 1
     jmp init_done

 init_failed_cleanup:
     mov rcx, rbx
     call VulkanCompute_Shutdown
     
 init_failed:
     xor rax, rax

 init_done:
     add rsp, 0xA0
     pop rsi
     pop rdi
     pop rbx
     ret
 VulkanCompute_Initialize ENDP

 ; bool SelectPhysicalDevice(void* this)
 VulkanCompute_SelectPhysicalDevice PROC FRAME
     push rbx
     .pushreg rbx
     push rdi
     .pushreg rdi
     sub rsp, 0x30
     .allocstack 0x30
     .endprolog

     mov rbx, rcx

     ; Get device count
     lea rdx, [rsp+0x20]
     mov rcx, [rbx+VKC_INSTANCE]
     call vkEnumeratePhysicalDevices
     
     cmp dword ptr [rsp+0x20], 0
     je no_devices

     ; Allocate space for device handles
     sub rsp, 0x80
     
     ; Enumerate devices
     lea r8, [rsp]
     lea rdx, [rsp+0x80]
     mov rcx, [rbx+VKC_INSTANCE]
     call vkEnumeratePhysicalDevices

     ; Take first device
     mov rdi, [rsp]
     mov [rbx+VKC_PHYSICAL_DEVICE], rdi
     mov dword ptr [rbx+VKC_QUEUE_FAMILY_IDX], 0
     
     mov rax, 1
     jmp cleanup_devices

 no_devices:
     xor rax, rax

 cleanup_devices:
     add rsp, 0x80
     add rsp, 0x30
     pop rdi
     pop rbx
     ret
 VulkanCompute_SelectPhysicalDevice ENDP

 ; bool CreateLogicalDevice(void* this)
 VulkanCompute_CreateLogicalDevice PROC FRAME
     push rbx
     .pushreg rbx
     sub rsp, 0x60
     .allocstack 0x60
     .endprolog

     mov rbx, rcx

     ; DeviceQueueCreateInfo
     lea rdi, [rsp+0x30]
     mov dword ptr [rdi], VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO
     mov qword ptr [rdi+8], 0
     mov dword ptr [rdi+16], 0
     mov eax, [rbx+VKC_QUEUE_FAMILY_IDX]
     mov dword ptr [rdi+20], eax
     mov dword ptr [rdi+24], 1
     mov dword ptr [rdi+32], 03F800000h  ; queuePriority = 1.0f

     ; DeviceCreateInfo
     lea rsi, [rsp]
     mov dword ptr [rsi], VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
     mov qword ptr [rsi+8], 0
     mov dword ptr [rsi+16], 0
     mov dword ptr [rsi+20], 1
     mov qword ptr [rsi+24], rdi
     mov dword ptr [rsi+32], 0
     mov dword ptr [rsi+40], 0
     mov dword ptr [rsi+48], 0
     mov qword ptr [rsi+56], 0

     ; Create device
     lea r8, [rbx+VKC_DEVICE]
     mov rdx, 0
     mov rcx, [rbx+VKC_PHYSICAL_DEVICE]
     call vkCreateDevice

     cmp eax, VK_SUCCESS
     jne create_failed

     ; Get device queue
     lea r9, [rbx+VKC_COMPUTE_QUEUE]
     mov r8d, 0
     mov edx, [rbx+VKC_QUEUE_FAMILY_IDX]
     mov rcx, [rbx+VKC_DEVICE]
     call vkGetDeviceQueue

     mov rax, 1
     jmp done

 create_failed:
     xor rax, rax

 done:
     add rsp, 0x60
     pop rbx
     ret
 VulkanCompute_CreateLogicalDevice ENDP

 ; bool InitializeCommandBufferPool(void* this)
 VulkanCompute_InitializeCommandBufferPool PROC FRAME
     push rbx
     .pushreg rbx
     sub rsp, 0x40
     .allocstack 0x40
     .endprolog

     mov rbx, rcx

     ; CommandPoolCreateInfo
     lea rdi, [rsp]
     mov dword ptr [rdi], VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO
     mov qword ptr [rdi+8], 0
     mov eax, [rbx+VKC_QUEUE_FAMILY_IDX]
     mov dword ptr [rdi+16], eax
     mov dword ptr [rdi+20], VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT

     ; Create main pool
     lea r8, [rbx+VKC_COMMAND_POOL]
     mov rdx, 0
     mov rcx, [rbx+VKC_DEVICE]
     call vkCreateCommandPool

     cmp eax, VK_SUCCESS
     jne pool_failed

     ; Create async pool
     mov dword ptr [rdi+20], VK_COMMAND_POOL_CREATE_TRANSIENT_BIT or VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
     lea r8, [rbx+VKC_ASYNC_POOL]
     mov rdx, 0
     mov rcx, [rbx+VKC_DEVICE]
     call vkCreateCommandPool

     cmp eax, VK_SUCCESS
     jne pool_failed_cleanup

     mov rax, 1
     jmp done

 pool_failed_cleanup:
     mov rdx, [rbx+VKC_COMMAND_POOL]
     mov rcx, [rbx+VKC_DEVICE]
     call vkDestroyCommandPool

 pool_failed:
     xor rax, rax

 done:
     add rsp, 0x40
     pop rbx
     ret
 VulkanCompute_InitializeCommandBufferPool ENDP

 ; void Shutdown(void* this)
 VulkanCompute_Shutdown PROC FRAME
     push rbx
     .pushreg rbx
     sub rsp, 0x20
     .allocstack 0x20
     .endprolog

     mov rbx, rcx

     cmp byte ptr [rbx+VKC_INITIALIZED], 0
     je shutdown_done

     ; Wait for device idle
     mov rcx, [rbx+VKC_DEVICE]
     test rcx, rcx
     jz skip_device_wait
     call vkDeviceWaitIdle

 skip_device_wait:
     ; Destroy command pools
     mov rdx, [rbx+VKC_COMMAND_POOL]
     test rdx, rdx
     jz skip_main_pool
     mov rcx, [rbx+VKC_DEVICE]
     call vkDestroyCommandPool
 skip_main_pool:

     mov rdx, [rbx+VKC_ASYNC_POOL]
     test rdx, rdx
     jz skip_async_pool
     mov rcx, [rbx+VKC_DEVICE]
     call vkDestroyCommandPool
 skip_async_pool:

     ; Destroy device
     mov rcx, [rbx+VKC_DEVICE]
     test rcx, rcx
     jz skip_device
     xor edx, edx
     call vkDestroyDevice
 skip_device:

     ; Destroy instance
     mov rcx, [rbx+VKC_INSTANCE]
     test rcx, rcx
     jz skip_instance
     xor edx, edx
     call vkDestroyInstance
 skip_instance:

     mov byte ptr [rbx+VKC_INITIALIZED], 0

 shutdown_done:
     add rsp, 0x20
     pop rbx
     ret
 VulkanCompute_Shutdown ENDP

 ; VkCommandBuffer AcquireCommandBuffer(void* this)
 VulkanCompute_AcquireCommandBuffer PROC FRAME
     push rbx
     .pushreg rbx
     sub rsp, 0x40
     .allocstack 0x40
     .endprolog

     mov rbx, rcx

     ; AllocateInfo
     lea rdi, [rsp]
     mov dword ptr [rdi], VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
     mov qword ptr [rdi+8], 0
     mov rax, [rbx+VKC_COMMAND_POOL]
     mov qword ptr [rdi+16], rax
     mov dword ptr [rdi+24], VK_COMMAND_BUFFER_LEVEL_PRIMARY
     mov dword ptr [rdi+28], 1

     lea r8, [rsp+0x30]
     mov rdx, rdi
     mov rcx, [rbx+VKC_DEVICE]
     call vkAllocateCommandBuffers

     mov rax, [rsp+0x30]
     
     add rsp, 0x40
     pop rbx
     ret
 VulkanCompute_AcquireCommandBuffer ENDP

 END
