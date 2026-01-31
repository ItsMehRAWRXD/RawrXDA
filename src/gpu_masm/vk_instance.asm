; Minimal Vulkan instance bootstrap (MASM x64)
option casemap:none

PUBLIC vk_init
PUBLIC vk_shutdown

.code
vk_init PROC
    ; TODO: Load vulkan-1.dll via LoadLibraryA, resolve vkCreateInstance
    xor rax, rax
    ret
vk_init ENDP

vk_shutdown PROC
    ; TODO: Destroy instance and free resources
    xor rax, rax
    ret
vk_shutdown ENDP

END
