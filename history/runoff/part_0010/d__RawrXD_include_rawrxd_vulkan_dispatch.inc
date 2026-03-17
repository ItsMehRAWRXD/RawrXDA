; ═══════════════════════════════════════════════════════════════════════════════
; rawrxd_vulkan_dispatch.inc - Vulkan API Dispatch Table (x64)
; ═══════════════════════════════════════════════════════════════════════════════
; Resolves all undefined Vulkan call targets via global function pointer table.
; Vulkan entry points are loaded dynamically via vkGetInstanceProcAddr /
; vkGetDeviceProcAddr — these are the dispatch pointers.
;
; Usage:  INCLUDE rawrxd_vulkan_dispatch.inc
; ═══════════════════════════════════════════════════════════════════════════════

IFNDEF RAWRXD_VULKAN_DISPATCH_INC
RAWRXD_VULKAN_DISPATCH_INC EQU 1

; ─── Vulkan Loader Entry Points ───────────────────────────────────────────────
EXTERNDEF vkGetInstanceProcAddr:PROC
EXTERNDEF vkGetDeviceProcAddr:PROC

; ─── Global Dispatch Table (populated at runtime via GetProcAddress) ──────────
; These are QWORD function pointers in the .data segment of vk_instance.asm
; or the initializing module. EXTERNDEF allows cross-module reference.

EXTERNDEF g_vkCreateInstance:QWORD
EXTERNDEF g_vkEnumeratePhysicalDevices:QWORD
EXTERNDEF g_vkGetPhysicalDeviceProperties:QWORD
EXTERNDEF g_vkCreateDevice:QWORD
EXTERNDEF g_vkGetDeviceQueue:QWORD
EXTERNDEF g_vkCreateCommandPool:QWORD
EXTERNDEF g_vkCmdBindPipeline:QWORD
EXTERNDEF g_vkCmdBindDescriptorSets:QWORD
EXTERNDEF g_vkCmdDispatch:QWORD
EXTERNDEF g_vkCmdPipelineBarrier:QWORD
EXTERNDEF g_vkQueueSubmit:QWORD
EXTERNDEF g_vkQueueBindSparse:QWORD

; ─── Direct Vulkan API calls (from vulkan-1.dll) ─────────────────────────────
EXTERNDEF vkCmdPushConstants:PROC
EXTERNDEF vkGetPhysicalDeviceMemoryProperties:PROC
EXTERNDEF vkDestroyInstance:PROC
EXTERNDEF vkDestroyDevice:PROC
EXTERNDEF vkDestroyCommandPool:PROC
EXTERNDEF vkAllocateCommandBuffers:PROC
EXTERNDEF vkBeginCommandBuffer:PROC
EXTERNDEF vkEndCommandBuffer:PROC
EXTERNDEF vkQueueWaitIdle:PROC
EXTERNDEF vkDeviceWaitIdle:PROC
EXTERNDEF vkCreateBuffer:PROC
EXTERNDEF vkDestroyBuffer:PROC
EXTERNDEF vkAllocateMemory:PROC
EXTERNDEF vkFreeMemory:PROC
EXTERNDEF vkBindBufferMemory:PROC
EXTERNDEF vkMapMemory:PROC
EXTERNDEF vkUnmapMemory:PROC
EXTERNDEF vkGetBufferMemoryRequirements:PROC
EXTERNDEF vkCreateShaderModule:PROC
EXTERNDEF vkDestroyShaderModule:PROC
EXTERNDEF vkCreateComputePipelines:PROC
EXTERNDEF vkDestroyPipeline:PROC
EXTERNDEF vkCreatePipelineLayout:PROC
EXTERNDEF vkDestroyPipelineLayout:PROC
EXTERNDEF vkCreateDescriptorSetLayout:PROC
EXTERNDEF vkDestroyDescriptorSetLayout:PROC
EXTERNDEF vkCreateDescriptorPool:PROC
EXTERNDEF vkDestroyDescriptorPool:PROC
EXTERNDEF vkAllocateDescriptorSets:PROC
EXTERNDEF vkUpdateDescriptorSets:PROC
EXTERNDEF vkResetDescriptorPool:PROC
EXTERNDEF vkCreateFence:PROC
EXTERNDEF vkDestroyFence:PROC
EXTERNDEF vkWaitForFences:PROC
EXTERNDEF vkResetFences:PROC
EXTERNDEF vkCreateSemaphore:PROC
EXTERNDEF vkDestroySemaphore:PROC
EXTERNDEF vkFreeCommandBuffers:PROC
EXTERNDEF vkResetCommandBuffer:PROC

; ─── Vulkan Constants ─────────────────────────────────────────────────────────
VK_SUCCESS                      EQU 0
VK_NOT_READY                    EQU 1
VK_TIMEOUT                      EQU 2
VK_ERROR_OUT_OF_HOST_MEMORY     EQU -1
VK_ERROR_OUT_OF_DEVICE_MEMORY   EQU -2
VK_ERROR_INITIALIZATION_FAILED  EQU -3
VK_ERROR_DEVICE_LOST            EQU -4

VK_BUFFER_USAGE_STORAGE_BUFFER_BIT EQU 00000020h
VK_BUFFER_USAGE_TRANSFER_SRC_BIT  EQU 00000001h
VK_BUFFER_USAGE_TRANSFER_DST_BIT  EQU 00000002h

VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT    EQU 00000002h
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT   EQU 00000004h
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT    EQU 00000001h

VK_DESCRIPTOR_TYPE_STORAGE_BUFFER      EQU 7
VK_SHADER_STAGE_COMPUTE_BIT            EQU 00000020h

VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT   EQU 00000800h
VK_ACCESS_SHADER_READ_BIT              EQU 00000020h
VK_ACCESS_SHADER_WRITE_BIT             EQU 00000040h

ENDIF ; RAWRXD_VULKAN_DISPATCH_INC
