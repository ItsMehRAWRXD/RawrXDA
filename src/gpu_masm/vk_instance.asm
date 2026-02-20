; Vulkan instance management (MASM x64)
option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; Windows API externs
extern LoadLibraryA:proc
extern GetProcAddress:proc
extern FreeLibrary:proc

; Vulkan function pointers
.data
    g_vkLib dq 0
    g_vkCreateInstance dq 0
    g_vkDestroyInstance dq 0
    g_vkEnumeratePhysicalDevices dq 0
    g_vkGetPhysicalDeviceProperties dq 0
    g_vkCreateDevice dq 0
    g_vkDestroyDevice dq 0
    g_vkGetDeviceProcAddr dq 0
    g_vkInstance dq 0
    g_vkDevice dq 0

.code
vk_init PROC
    sub rsp, 28h
    
    ; Load Vulkan library
    lea rcx, szVulkanDll
    call LoadLibraryA
    test rax, rax
    jz @@load_failed
    mov g_vkLib, rax
    
    ; Get vkCreateInstance
    mov rcx, rax
    lea rdx, szCreateInstance
    call GetProcAddress
    test rax, rax
    jz @@proc_failed
    mov g_vkCreateInstance, rax
    
    ; Get vkDestroyInstance
    mov rcx, g_vkLib
    lea rdx, szDestroyInstance
    call GetProcAddress
    test rax, rax
    jz @@proc_failed
    mov g_vkDestroyInstance, rax
    
    ; Get vkEnumeratePhysicalDevices
    mov rcx, g_vkLib
    lea rdx, szEnumeratePhysicalDevices
    call GetProcAddress
    test rax, rax
    jz @@proc_failed
    mov g_vkEnumeratePhysicalDevices, rax
    
    ; Create instance
    lea rcx, instanceCreateInfo
    xor rdx, rdx  ; allocator
    lea r8, g_vkInstance
    call g_vkCreateInstance
    test eax, eax
    jnz @@create_failed
    
    ; Success
    xor rax, rax
    jmp @@done
    
@@load_failed:
    mov rax, 1
    jmp @@done
    
@@proc_failed:
    call FreeLibrary
    mov rax, 2
    jmp @@done
    
@@create_failed:
    call FreeLibrary
    mov rax, 3
    
@@done:
    add rsp, 28h
    ret
vk_init ENDP

vk_shutdown PROC
    sub rsp, 28h
    
    ; Destroy instance
    mov rcx, g_vkInstance
    test rcx, rcx
    jz @@no_instance
    xor rdx, rdx
    call g_vkDestroyInstance
    
@@no_instance:
    ; Free library
    mov rcx, g_vkLib
    test rcx, rcx
    jz @@no_lib
    call FreeLibrary
    
@@no_lib:
    xor rax, rax
    add rsp, 28h
    ret
vk_shutdown ENDP

; Data
.data
szVulkanDll db "vulkan-1.dll", 0
szCreateInstance db "vkCreateInstance", 0
szDestroyInstance db "vkDestroyInstance", 0
szEnumeratePhysicalDevices db "vkEnumeratePhysicalDevices", 0

; Minimal instance create info
instanceCreateInfo:
    dd 100001000h  ; sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    dq 0           ; pNext
    dd 0           ; flags
    dq 0           ; pApplicationInfo
    dd 0           ; enabledLayerCount
    dq 0           ; ppEnabledLayerNames
    dd 0           ; enabledExtensionCount
    dq 0           ; ppEnabledExtensionNames

END
