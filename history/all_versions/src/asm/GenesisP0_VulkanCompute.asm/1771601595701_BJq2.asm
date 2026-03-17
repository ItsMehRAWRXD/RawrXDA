; GenesisP0_VulkanCompute.asm
; Circular buffer bootstrap for Vulkan-adjacent compute path.

OPTION CASEMAP:NONE

includelib kernel32.lib

EXTERN LoadLibraryA:PROC
EXTERN FreeLibrary:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN UTC_LogEvent:PROC

MEM_COMMIT_RESERVE EQU 3000h
MEM_RELEASE        EQU 8000h
PAGE_READWRITE     EQU 04h
DEFAULT_RING_SIZE  EQU 04000000h

.data
align 8
g_vk_module        dq 0
g_ring_buffer      dq 0
g_ring_size        dq DEFAULT_RING_SIZE
g_ring_head        dq 0
g_ring_tail        dq 0
sz_vulkan_dll      db "vulkan-1.dll", 0
sz_evt_vk_init     db "[GenesisP0] VulkanCompute Init", 0

.code
PUBLIC Genesis_VulkanCompute_Init
PUBLIC Genesis_VulkanCompute_Dispatch
PUBLIC Genesis_VulkanCompute_Shutdown

; void* Genesis_VulkanCompute_Init(void* vkInstance, uint64_t optionalSize)
; RCX = optional vk instance (unused), RDX = optional ring size
Genesis_VulkanCompute_Init PROC
    sub rsp, 28h

    lea rcx, sz_evt_vk_init
    call UTC_LogEvent

    cmp rdx, 0
    jz init_size_ready
    mov qword ptr [g_ring_size], rdx

init_size_ready:
    mov rax, qword ptr [g_ring_buffer]
    test rax, rax
    jnz init_done

    lea rcx, sz_vulkan_dll
    call LoadLibraryA
    test rax, rax
    jz init_fail
    mov qword ptr [g_vk_module], rax

    xor rcx, rcx
    mov rdx, qword ptr [g_ring_size]
    mov r8d, MEM_COMMIT_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz init_fail_cleanup

    mov qword ptr [g_ring_buffer], rax
    mov qword ptr [g_ring_head], 0
    mov qword ptr [g_ring_tail], 0
    jmp init_done

init_fail_cleanup:
    mov rcx, qword ptr [g_vk_module]
    test rcx, rcx
    jz init_fail
    call FreeLibrary
    mov qword ptr [g_vk_module], 0

init_fail:
    xor rax, rax

init_done:
    add rsp, 28h
    ret
Genesis_VulkanCompute_Init ENDP

; uint64_t Genesis_VulkanCompute_Dispatch(const void* src, void* dst, uint64_t bytes)
Genesis_VulkanCompute_Dispatch PROC
    mov r10, rcx
    mov r11, rdx
    mov rax, r8

    test r10, r10
    jz dispatch_fail
    test r11, r11
    jz dispatch_fail
    test rax, rax
    jz dispatch_fail

    mov rdx, qword ptr [g_ring_buffer]
    test rdx, rdx
    jz dispatch_fail

    mov rcx, qword ptr [g_ring_size]
    cmp rax, rcx
    jbe dispatch_size_ok
    mov rax, rcx

dispatch_size_ok:
    xor r9, r9

copy_to_ring:
    cmp r9, rax
    jae copy_to_output
    mov cl, byte ptr [r10+r9]
    mov byte ptr [rdx+r9], cl
    inc r9
    jmp copy_to_ring

copy_to_output:
    xor r9, r9

copy_from_ring:
    cmp r9, rax
    jae dispatch_update
    mov cl, byte ptr [rdx+r9]
    mov byte ptr [r11+r9], cl
    inc r9
    jmp copy_from_ring

dispatch_update:
    mov rcx, qword ptr [g_ring_size]

    mov r8, qword ptr [g_ring_head]
    add r8, rax
    cmp r8, rcx
    jb head_ok
    sub r8, rcx
head_ok:
    mov qword ptr [g_ring_head], r8

    mov r8, qword ptr [g_ring_tail]
    add r8, rax
    cmp r8, rcx
    jb tail_ok
    sub r8, rcx
tail_ok:
    mov qword ptr [g_ring_tail], r8

    ret

dispatch_fail:
    xor rax, rax
    ret
Genesis_VulkanCompute_Dispatch ENDP

; int Genesis_VulkanCompute_Shutdown(void)
Genesis_VulkanCompute_Shutdown PROC
    sub rsp, 28h

    mov rcx, qword ptr [g_ring_buffer]
    test rcx, rcx
    jz shutdown_skip_buffer
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
    mov qword ptr [g_ring_buffer], 0

shutdown_skip_buffer:
    mov rcx, qword ptr [g_vk_module]
    test rcx, rcx
    jz shutdown_done
    call FreeLibrary
    mov qword ptr [g_vk_module], 0

shutdown_done:
    mov qword ptr [g_ring_head], 0
    mov qword ptr [g_ring_tail], 0
    mov eax, 1
    add rsp, 28h
    ret
Genesis_VulkanCompute_Shutdown ENDP

END
