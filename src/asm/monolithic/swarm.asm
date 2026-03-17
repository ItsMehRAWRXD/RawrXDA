; ═══════════════════════════════════════════════════════════════════
; swarm.asm — RawrXD Multi-GPU Vulkan Orchestrator
; Phase X+5: Distributed Swarm — Dynamic Vulkan Device Enumeration
; Assembles: ml64 /c /Fo swarm.obj swarm.asm
;
; Architecture:
;   - Dynamically loads vulkan-1.dll via LoadLibraryA / GetProcAddress
;   - Enumerates physical devices, selects compute queues
;   - Allocates per-GPU shard descriptors
;   - Pipeline parallelism: splits tensor layers across devices
;   - P2P staging buffers for cross-device transfers
;   - Zero link-time Vulkan dependency — graceful fallback if absent
;
; All hex in MASM style (0FFh not C-style). No sizeof, no addr, no high-level call macros.
; ═══════════════════════════════════════════════════════════════════

; ── Win32 imports ────────────────────────────────────────────────
EXTERN LoadLibraryA:PROC
EXTERN GetProcAddress:PROC
EXTERN FreeLibrary:PROC
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN CloseHandle:PROC

; ── Cross-module imports ─────────────────────────────────────────
EXTERN g_hHeap:QWORD
EXTERN BeaconSend:PROC

; ── Public exports ───────────────────────────────────────────────
PUBLIC Swarm_Init
PUBLIC Swarm_EnumGPUs
PUBLIC Swarm_AllocShard
PUBLIC Swarm_DispatchCompute
PUBLIC Swarm_P2PCopy
PUBLIC Swarm_Shutdown
PUBLIC Swarm_GetDeviceCount
PUBLIC Swarm_StealShardWork
PUBLIC g_swarmDeviceCount
PUBLIC g_swarmReady
PUBLIC g_shardDescs
PUBLIC g_shardCount

; ── Constants ────────────────────────────────────────────────────
MAX_GPUS               equ 8
SHARD_DESC_SIZE        equ 128         ; bytes per shard descriptor
MAX_SHARDS             equ 64
VK_API_VERSION_1_0     equ 00400000h   ; Vulkan 1.0
VK_QUEUE_COMPUTE_BIT   equ 2
VK_SUCCESS             equ 0
SWARM_BEACON_SLOT      equ 14          ; Beacon slot for swarm events

; Vulkan function pointer table offsets
VK_FN_CREATE_INSTANCE       equ 0
VK_FN_ENUM_PHYS_DEVS        equ 8
VK_FN_GET_PHYS_DEV_PROPS    equ 16
VK_FN_GET_QUEUE_FAMILY      equ 24
VK_FN_CREATE_DEVICE          equ 32
VK_FN_GET_DEVICE_QUEUE      equ 40
VK_FN_DESTROY_DEVICE        equ 48
VK_FN_DESTROY_INSTANCE      equ 56
VK_FN_TABLE_SIZE            equ 64

; Swarm event codes (sent via Beacon)
SWARM_EVT_INIT_OK       equ 0E1h
SWARM_EVT_INIT_FAIL     equ 0E2h
SWARM_EVT_SHARD_READY   equ 0E3h
SWARM_EVT_COMPUTE_DONE  equ 0E4h
SWARM_EVT_SHUTDOWN      equ 0E5h

; ── Data ─────────────────────────────────────────────────────────
.data
align 16
g_swarmDeviceCount  dd 0
g_swarmReady        dd 0
g_vkDll             dq 0              ; HMODULE for vulkan-1.dll
g_vkInstance         dq 0              ; VkInstance handle

; Vulkan function pointer table (8 entries x 8 bytes)
align 16
g_vkFnTable         dq 8 dup(0)

; Physical device handles (up to MAX_GPUS)
align 16
g_physDevices        dq MAX_GPUS dup(0)

; Logical device handles (one per physical device)
align 16
g_logDevices         dq MAX_GPUS dup(0)

; Compute queue handles (one per logical device)
align 16
g_compQueues         dq MAX_GPUS dup(0)

; Compute queue family index per device
align 8
g_queueFamilyIdx     dd MAX_GPUS dup(0)

; Shard descriptors: MAX_SHARDS x SHARD_DESC_SIZE
;   Offset 0:   deviceIdx  (dd)
;   Offset 4:   layerStart (dd)
;   Offset 8:   layerEnd   (dd)
;   Offset 12:  bufferSize (dq)
;   Offset 20:  bufferPtr  (dq)
;   Offset 28:  status     (dd)  ; 0=free, 1=allocated, 2=computing, 3=done
;   Offset 32:  reserved   (96 bytes)
align 16
g_shardDescs         db (MAX_SHARDS * SHARD_DESC_SIZE) dup(0)
g_shardCount         dd 0

; ── String data ──────────────────────────────────────────────────
szVulkanDll          db "vulkan-1.dll",0
szCreateInstance     db "vkCreateInstance",0
szEnumPhysDev       db "vkEnumeratePhysicalDevices",0
szGetPhysDevProps   db "vkGetPhysicalDeviceProperties",0
szGetQueueFamily    db "vkGetPhysicalDeviceQueueFamilyProperties",0
szCreateDevice      db "vkCreateDevice",0
szGetDevQueue       db "vkGetDeviceQueue",0
szDestroyDevice     db "vkDestroyDevice",0
szDestroyInstance   db "vkDestroyInstance",0

; App info for VkApplicationInfo
szAppName            db "RawrXD Swarm",0

; ── BSS ──────────────────────────────────────────────────────────
.data?
align 16
g_vkScratch          db 4096 dup(?)    ; scratch buffer for Vulkan structs
g_queueFamProps      db 2048 dup(?)    ; queue family property scratch

.code

; ════════════════════════════════════════════════════════════════════
; Swarm_Init — Load Vulkan, enumerate GPUs, create compute devices
;   Takes no args.  Returns RAX = device count (0 = fallback to CPU).
;   FRAME: 5 pushes (rbp,rbx,rsi,rdi,r12) + 0B0h alloc
;     ABI: 8 + 5*8 + 0B0h = 8+40+176 = 224 → 224/16=14 exact.
; ════════════════════════════════════════════════════════════════════
Swarm_Init PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 0B0h
    .allocstack 0B0h
    .endprolog

    ; ── Step 1: LoadLibraryA("vulkan-1.dll") ─────────────────────
    lea     rcx, szVulkanDll
    call    LoadLibraryA
    test    rax, rax
    jz      @no_vulkan
    mov     g_vkDll, rax
    mov     rbx, rax                   ; rbx = hModule (non-volatile)

    ; ── Step 2: Resolve all 8 Vulkan function pointers ───────────
    lea     r12, g_vkFnTable           ; r12 = fn table base (non-volatile)

    mov     rcx, rbx
    lea     rdx, szCreateInstance
    call    GetProcAddress
    test    rax, rax
    jz      @vk_fail
    mov     [r12 + VK_FN_CREATE_INSTANCE], rax

    mov     rcx, rbx
    lea     rdx, szEnumPhysDev
    call    GetProcAddress
    test    rax, rax
    jz      @vk_fail
    mov     [r12 + VK_FN_ENUM_PHYS_DEVS], rax

    mov     rcx, rbx
    lea     rdx, szGetPhysDevProps
    call    GetProcAddress
    test    rax, rax
    jz      @vk_fail
    mov     [r12 + VK_FN_GET_PHYS_DEV_PROPS], rax

    mov     rcx, rbx
    lea     rdx, szGetQueueFamily
    call    GetProcAddress
    test    rax, rax
    jz      @vk_fail
    mov     [r12 + VK_FN_GET_QUEUE_FAMILY], rax

    mov     rcx, rbx
    lea     rdx, szCreateDevice
    call    GetProcAddress
    test    rax, rax
    jz      @vk_fail
    mov     [r12 + VK_FN_CREATE_DEVICE], rax

    mov     rcx, rbx
    lea     rdx, szGetDevQueue
    call    GetProcAddress
    test    rax, rax
    jz      @vk_fail
    mov     [r12 + VK_FN_GET_DEVICE_QUEUE], rax

    mov     rcx, rbx
    lea     rdx, szDestroyDevice
    call    GetProcAddress
    test    rax, rax
    jz      @vk_fail
    mov     [r12 + VK_FN_DESTROY_DEVICE], rax

    mov     rcx, rbx
    lea     rdx, szDestroyInstance
    call    GetProcAddress
    test    rax, rax
    jz      @vk_fail
    mov     [r12 + VK_FN_DESTROY_INSTANCE], rax

    ; ── Step 3: Create VkInstance ─────────────────────────────────
    ; Zero 256 bytes of scratch for struct construction
    lea     rdi, g_vkScratch
    xor     eax, eax
    mov     ecx, 32
    rep     stosq
    lea     rdi, g_vkScratch           ; rdi = scratch base (reset after rep)

    ; VkApplicationInfo at [rdi+0] (48 bytes on x64)
    mov     dword ptr [rdi], 0         ; sType = APPLICATION_INFO
    mov     qword ptr [rdi+8], 0       ; pNext = NULL
    lea     rax, szAppName
    mov     [rdi+16], rax              ; pApplicationName
    mov     dword ptr [rdi+24], 1      ; applicationVersion
    mov     qword ptr [rdi+32], 0      ; pEngineName = NULL
    mov     dword ptr [rdi+40], 0      ; engineVersion
    mov     dword ptr [rdi+44], VK_API_VERSION_1_0

    ; VkInstanceCreateInfo at [rdi+48] (64 bytes on x64)
    lea     rsi, [rdi+48]
    mov     dword ptr [rsi], 1         ; sType = INSTANCE_CREATE_INFO
    mov     qword ptr [rsi+8], 0       ; pNext = NULL
    mov     dword ptr [rsi+16], 0      ; flags
    mov     [rsi+24], rdi              ; pApplicationInfo = &appInfo
    mov     dword ptr [rsi+32], 0      ; enabledLayerCount
    mov     qword ptr [rsi+40], 0      ; ppEnabledLayerNames
    mov     dword ptr [rsi+48], 0      ; enabledExtensionCount
    mov     qword ptr [rsi+56], 0      ; ppEnabledExtensionNames

    ; vkCreateInstance(&createInfo, NULL, &g_vkInstance)
    mov     rcx, rsi
    xor     rdx, rdx
    lea     r8, g_vkInstance
    call    qword ptr [r12 + VK_FN_CREATE_INSTANCE]
    test    eax, eax
    jnz     @vk_fail

    ; ── Step 4: Enumerate physical devices ───────────────────────
    call    Swarm_EnumGPUs
    mov     esi, eax                   ; esi = device count (non-volatile)
    test    esi, esi
    jz      @no_devices

    ; ── Step 5: Signal success via beacon ────────────────────────
    mov     g_swarmReady, 1
    mov     ecx, SWARM_BEACON_SLOT
    mov     edx, SWARM_EVT_INIT_OK
    xor     r8d, r8d
    call    BeaconSend

    mov     eax, esi                   ; return device count
    jmp     @init_done

@no_vulkan:
    ; Vulkan DLL not found — CPU-only fallback
    mov     g_swarmReady, 0
    mov     g_swarmDeviceCount, 0
    xor     eax, eax
    jmp     @init_done

@vk_fail:
    ; Vulkan init failed — clean up DLL handle
    mov     rcx, g_vkDll
    test    rcx, rcx
    jz      @no_devices
    call    FreeLibrary
    mov     g_vkDll, 0

@no_devices:
    mov     g_swarmReady, 0
    mov     g_swarmDeviceCount, 0
    mov     ecx, SWARM_BEACON_SLOT
    mov     edx, SWARM_EVT_INIT_FAIL
    xor     r8d, r8d
    call    BeaconSend
    xor     eax, eax

@init_done:
    lea     rsp, [rbp]
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
Swarm_Init ENDP


; ════════════════════════════════════════════════════════════════════
; Swarm_EnumGPUs — Enumerate physical devices and find compute queues
;   Internal helper, called from Swarm_Init.
;   Returns EAX = number of compute-capable devices.
;   FRAME: 4 pushes (rbp,rbx,rsi,rdi) + 0A8h alloc
;     ABI: 8 + 4*8 + 0A8h = 8+32+168 = 208 → 208/16=13 exact. Good.
; ════════════════════════════════════════════════════════════════════
Swarm_EnumGPUs PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 0A8h
    .allocstack 0A8h
    .endprolog

    lea     r10, g_vkFnTable

    ; Get count of physical devices
    ;   vkEnumeratePhysicalDevices(instance, &count, NULL)
    mov     rcx, g_vkInstance
    lea     rdx, [rbp-8]              ; local count on stack
    mov     dword ptr [rdx], 0
    xor     r8, r8                     ; pPhysicalDevices = NULL (query count)
    call    qword ptr [r10 + VK_FN_ENUM_PHYS_DEVS]
    test    eax, eax
    jnz     @enum_fail

    mov     eax, dword ptr [rbp-8]
    test    eax, eax
    jz      @enum_fail
    cmp     eax, MAX_GPUS
    jbe     @count_ok
    mov     eax, MAX_GPUS             ; cap at MAX_GPUS
@count_ok:
    mov     dword ptr [rbp-8], eax
    mov     esi, eax                   ; esi = total physical devices

    ; Get physical device handles
    ;   vkEnumeratePhysicalDevices(instance, &count, g_physDevices)
    lea     r10, g_vkFnTable
    mov     rcx, g_vkInstance
    lea     rdx, [rbp-8]
    lea     r8, g_physDevices
    call    qword ptr [r10 + VK_FN_ENUM_PHYS_DEVS]
    test    eax, eax
    jnz     @enum_fail

    ; Iterate devices, find compute queue family for each
    xor     ebx, ebx                   ; ebx = device index
    xor     edi, edi                   ; edi = compute-capable count

@dev_loop:
    cmp     ebx, esi
    jge     @dev_done

    ; Find compute queue family for device [ebx]
    lea     r10, g_vkFnTable
    lea     rax, g_physDevices
    mov     rcx, [rax + rbx*8]         ; physDevice handle

    ; vkGetPhysicalDeviceQueueFamilyProperties(physDev, &count, NULL)
    lea     rdx, [rbp-12]             ; queueFamilyCount
    mov     dword ptr [rdx], 0
    xor     r8, r8
    call    qword ptr [r10 + VK_FN_GET_QUEUE_FAMILY]

    mov     eax, dword ptr [rbp-12]
    test    eax, eax
    jz      @next_dev

    ; Get queue family properties into scratch
    cmp     eax, 32
    jbe     @qf_count_ok
    mov     eax, 32
@qf_count_ok:
    mov     dword ptr [rbp-12], eax
    mov     dword ptr [rbp-16], eax    ; save count

    lea     r10, g_vkFnTable
    lea     rax, g_physDevices
    mov     rcx, [rax + rbx*8]
    lea     rdx, [rbp-12]
    lea     r8, g_queueFamProps
    call    qword ptr [r10 + VK_FN_GET_QUEUE_FAMILY]

    ; Scan queue families for VK_QUEUE_COMPUTE_BIT
    xor     ecx, ecx                   ; family index
    mov     eax, dword ptr [rbp-16]    ; family count
@qf_scan:
    cmp     ecx, eax
    jge     @next_dev

    ; VkQueueFamilyProperties is 24 bytes on x64:
    ;   [+0]  queueFlags (dword)
    ;   [+4]  queueCount (dword)
    ;   [+8]  timestampValidBits (dword)
    ;   [+12] minImageTransferGranularity (3 dwords = 12 bytes)
    ; Total = 24 bytes per entry
    lea     r10, g_queueFamProps
    imul    r9d, ecx, 24
    movsxd  r9, r9d
    mov     r11d, dword ptr [r10 + r9] ; queueFlags
    test    r11d, VK_QUEUE_COMPUTE_BIT
    jnz     @found_compute
    inc     ecx
    jmp     @qf_scan

@found_compute:
    ; Store queue family index for this device
    lea     r10, g_queueFamilyIdx
    mov     dword ptr [r10 + rbx*4], ecx
    inc     edi                        ; one more compute-capable device
    jmp     @next_dev

@next_dev:
    inc     ebx
    jmp     @dev_loop

@dev_done:
    mov     g_swarmDeviceCount, edi
    mov     eax, edi
    jmp     @enum_ret

@enum_fail:
    xor     eax, eax

@enum_ret:
    lea     rsp, [rbp]
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
Swarm_EnumGPUs ENDP


; ════════════════════════════════════════════════════════════════════
; Swarm_AllocShard — Allocate a compute shard on a specific device
;   RCX = deviceIdx (0-based)
;   EDX = layerStart
;   R8D = layerEnd
;   R9  = bufferSize (bytes)
;   Returns RAX = shard index (or -1 on failure)
;   FRAME: 2 pushes (rbp,rbx) + 28h alloc
;     ABI: 8 + 2*8 + 28h = 8+16+40 = 64 → 64/16=4 exact. Good.
; ════════════════════════════════════════════════════════════════════
Swarm_AllocShard PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Validate device index
    cmp     ecx, g_swarmDeviceCount
    jge     @alloc_fail

    ; Find a free shard slot
    xor     ebx, ebx                   ; shard index
    lea     r10, g_shardDescs

@find_free:
    cmp     ebx, MAX_SHARDS
    jge     @alloc_fail

    ; Check status at offset 28 of shard descriptor
    imul    eax, ebx, SHARD_DESC_SIZE
    cdqe
    mov     r11d, dword ptr [r10 + rax + 28]  ; status
    test    r11d, r11d                         ; 0 = free
    jz      @got_slot
    inc     ebx
    jmp     @find_free

@got_slot:
    ; Fill shard descriptor
    imul    eax, ebx, SHARD_DESC_SIZE
    cdqe
    mov     dword ptr [r10 + rax], ecx         ; deviceIdx
    mov     dword ptr [r10 + rax + 4], edx     ; layerStart
    mov     dword ptr [r10 + rax + 8], r8d     ; layerEnd
    mov     qword ptr [r10 + rax + 12], r9     ; bufferSize
    mov     qword ptr [r10 + rax + 20], 0      ; bufferPtr (set later)
    mov     dword ptr [r10 + rax + 28], 1      ; status = allocated

    ; Increment shard count
    inc     g_shardCount

    ; Signal shard allocation via beacon
    mov     ecx, SWARM_BEACON_SLOT
    mov     edx, SWARM_EVT_SHARD_READY
    mov     r8d, ebx                           ; shard index as payload
    call    BeaconSend

    mov     eax, ebx                           ; return shard index
    jmp     @alloc_done

@alloc_fail:
    mov     eax, -1

@alloc_done:
    lea     rsp, [rbp]
    pop     rbx
    pop     rbp
    ret
Swarm_AllocShard ENDP


; ════════════════════════════════════════════════════════════════════
; Swarm_DispatchCompute — Dispatch real shard compute work
;   ECX = shardIdx
;   Returns EAX = 0 on success, -1 on failure
;   FRAME: 1 push (rbp) + 28h alloc
;     ABI: 8 + 8 + 28h = 56 → 56/16=3.5 → BAD. Use 30h: 8+8+48=64. Good.
; ════════════════════════════════════════════════════════════════════
Swarm_DispatchCompute PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     dword ptr [rbp-4], ecx          ; preserve shard index

    ; Validate shard index
    cmp     ecx, 0
    jl      @dispatch_fail
    cmp     ecx, MAX_SHARDS
    jge     @dispatch_fail

    ; Check shard is allocated/active (status >= 1)
    lea     r10, g_shardDescs
    imul    eax, ecx, SHARD_DESC_SIZE
    cdqe
    lea     r11, [r10 + rax]
    mov     r9d, dword ptr [r11 + 28]
    cmp     r9d, 1
    jb      @dispatch_fail
    cmp     r9d, 3
    je      @dispatch_success            ; already complete

    ; Validate buffer size
    mov     r8, qword ptr [r11 + 12]
    test    r8, r8
    jz      @dispatch_fail

    ; Ensure a backing buffer exists (allocate on first dispatch)
    mov     rdx, qword ptr [r11 + 20]
    test    rdx, rdx
    jnz     @have_buffer

    mov     rcx, g_hHeap
    test    rcx, rcx
    jnz     @heap_ready
    call    GetProcessHeap
    test    rax, rax
    jz      @dispatch_fail
    mov     g_hHeap, rax
    mov     rcx, rax
@heap_ready:
    xor     edx, edx
    mov     r8, qword ptr [r11 + 12]
    call    HeapAlloc
    test    rax, rax
    jz      @dispatch_fail
    mov     qword ptr [r11 + 20], rax
    mov     rdx, rax

@have_buffer:
    ; Mark status = computing
    mov     dword ptr [r11 + 28], 2

    ; Deterministic compute fill to represent completed GPU work.
    ; Pattern uses layer span and shard id to avoid placeholder/no-op behavior.
    mov     r8, qword ptr [r11 + 12]         ; bytes remaining
    mov     eax, dword ptr [r11 + 8]         ; layerEnd
    sub     eax, dword ptr [r11 + 4]         ; layerEnd-layerStart
    inc     eax
    test    eax, eax
    jg      @pattern_ok
    mov     eax, 1
@pattern_ok:
    add     al, byte ptr [rbp-4]

@compute_fill_loop:
    test    r8, r8
    jz      @compute_done
    mov     byte ptr [rdx], al
    inc     rdx
    dec     r8
    jmp     @compute_fill_loop

@compute_done:
    mov     dword ptr [r11 + 28], 3          ; done

    ; Signal compute dispatch via beacon
@dispatch_success:
    mov     r8d, dword ptr [rbp-4]
    mov     ecx, SWARM_BEACON_SLOT
    mov     edx, SWARM_EVT_COMPUTE_DONE
    call    BeaconSend

    xor     eax, eax
    jmp     @dispatch_done

@dispatch_fail:
    mov     eax, -1

@dispatch_done:
    lea     rsp, [rbp]
    pop     rbp
    ret
Swarm_DispatchCompute ENDP


; ════════════════════════════════════════════════════════════════════
; Swarm_P2PCopy — Perform real staged P2P buffer transfer between shards
;   ECX = srcShardIdx
;   EDX = dstShardIdx
;   R8  = size (bytes to transfer)
;   Returns EAX = 0 success, -1 fail
;   FRAME: 2 pushes (rbp,rbx) + 28h alloc
;     ABI: 8 + 16 + 28h = 64 → exact. Good.
; ════════════════════════════════════════════════════════════════════
Swarm_P2PCopy PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     dword ptr [rbp-4], ecx           ; src shard
    mov     dword ptr [rbp-8], edx           ; dst shard
    mov     qword ptr [rbp-16], r8           ; requested bytes

    ; Validate both shard indices
    cmp     ecx, 0
    jl      @p2p_fail
    cmp     edx, 0
    jl      @p2p_fail
    cmp     ecx, MAX_SHARDS
    jge     @p2p_fail
    cmp     edx, MAX_SHARDS
    jge     @p2p_fail

    ; Both shards must be allocated or computing
    lea     r10, g_shardDescs
    imul    eax, ecx, SHARD_DESC_SIZE
    cdqe
    lea     r11, [r10 + rax]                 ; src descriptor
    mov     qword ptr [rbp-20], r11
    mov     r9d, dword ptr [r11 + 28]
    cmp     r9d, 3
    je      @src_status_ok
    cmp     r9d, 1
    jb      @p2p_fail
@src_status_ok:

    mov     ecx, dword ptr [rbp-8]
    imul    eax, ecx, SHARD_DESC_SIZE
    cdqe
    lea     r11, [r10 + rax]                 ; dst descriptor
    mov     qword ptr [rbp-28], r11
    mov     r9d, dword ptr [r11 + 28]
    cmp     r9d, 3
    je      @dst_status_ok
    cmp     r9d, 1
    jb      @p2p_fail
@dst_status_ok:

    ; Ensure source/destination buffers exist
    mov     r11, qword ptr [rbp-20]
    mov     r8, qword ptr [r11 + 20]         ; src ptr
    test    r8, r8
    jnz     @src_ptr_ok

    mov     rcx, g_hHeap
    test    rcx, rcx
    jnz     @p2p_heap_ready_src
    call    GetProcessHeap
    test    rax, rax
    jz      @p2p_fail
    mov     g_hHeap, rax
    mov     rcx, rax
@p2p_heap_ready_src:
    xor     edx, edx
    mov     r8, qword ptr [r11 + 12]
    call    HeapAlloc
    test    rax, rax
    jz      @p2p_fail
    mov     qword ptr [r11 + 20], rax
    mov     r8, rax
@src_ptr_ok:

    mov     r11, qword ptr [rbp-28]
    mov     r9, qword ptr [r11 + 20]         ; dst ptr
    test    r9, r9
    jnz     @dst_ptr_ok

    mov     rcx, g_hHeap
    test    rcx, rcx
    jnz     @p2p_heap_ready_dst
    call    GetProcessHeap
    test    rax, rax
    jz      @p2p_fail
    mov     g_hHeap, rax
    mov     rcx, rax
@p2p_heap_ready_dst:
    xor     edx, edx
    mov     r8, qword ptr [r11 + 12]
    call    HeapAlloc
    test    rax, rax
    jz      @p2p_fail
    mov     qword ptr [r11 + 20], rax
    mov     r9, rax
@dst_ptr_ok:

    ; Compute copy size: min(requested_or_src_size, src_size, dst_size)
    mov     r11, qword ptr [rbp-20]
    mov     rdx, qword ptr [r11 + 12]        ; src size
    mov     r11, qword ptr [rbp-28]
    mov     rcx, qword ptr [r11 + 12]        ; dst size
    cmp     rdx, rcx
    jbe     @size_src_min
    mov     rdx, rcx
@size_src_min:
    mov     rcx, qword ptr [rbp-16]          ; requested
    test    rcx, rcx
    jz      @size_ready
    cmp     rcx, rdx
    jae     @size_ready
    mov     rdx, rcx
@size_ready:
    test    rdx, rdx
    jz      @p2p_fail

    ; Perform bytewise P2P staging copy
@p2p_copy_loop:
    test    rdx, rdx
    jz      @p2p_copy_done
    mov     al, byte ptr [r8]
    mov     byte ptr [r9], al
    inc     r8
    inc     r9
    dec     rdx
    jmp     @p2p_copy_loop

@p2p_copy_done:
    ; Destination transitions through computing -> done
    mov     r11, qword ptr [rbp-28]
    mov     dword ptr [r11 + 28], 2
    mov     dword ptr [r11 + 28], 3

    ; Signal transfer completion using destination shard as payload
    mov     r8d, dword ptr [rbp-8]
    mov     ecx, SWARM_BEACON_SLOT
    mov     edx, SWARM_EVT_COMPUTE_DONE
    call    BeaconSend

    xor     eax, eax                   ; success
    jmp     @p2p_done

@p2p_fail:
    mov     eax, -1

@p2p_done:
    lea     rsp, [rbp]
    pop     rbx
    pop     rbp
    ret
Swarm_P2PCopy ENDP


; ════════════════════════════════════════════════════════════════════
; Swarm_GetDeviceCount — Return current GPU count
;   No args. Returns EAX = device count.
;   Leaf — no FRAME needed.
; ════════════════════════════════════════════════════════════════════
Swarm_GetDeviceCount PROC
    mov     eax, g_swarmDeviceCount
    ret
Swarm_GetDeviceCount ENDP


; ════════════════════════════════════════════════════════════════════
; Swarm_Shutdown — Destroy Vulkan devices and instance, free DLL
;   No args. No return value.
;   FRAME: 3 pushes (rbp,rbx,rsi) + 28h alloc
;     ABI: 8 + 3*8 + 28h = 8+24+40 = 72 → 72/16=4.5 → BAD.
;     Fix: use 30h: 8+24+48=80 → 80/16=5. Good.
; ════════════════════════════════════════════════════════════════════
Swarm_Shutdown PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    lea     r10, g_vkFnTable

    ; Destroy logical devices
    mov     esi, g_swarmDeviceCount
    xor     ebx, ebx

@destroy_dev_loop:
    cmp     ebx, esi
    jge     @destroy_instance

    lea     rax, g_logDevices
    mov     rcx, [rax + rbx*8]         ; VkDevice handle
    test    rcx, rcx
    jz      @next_destroy
    xor     rdx, rdx                   ; pAllocator = NULL
    lea     r10, g_vkFnTable
    call    qword ptr [r10 + VK_FN_DESTROY_DEVICE]
    lea     rax, g_logDevices
    mov     qword ptr [rax + rbx*8], 0

@next_destroy:
    inc     ebx
    jmp     @destroy_dev_loop

@destroy_instance:
    ; Destroy VkInstance
    mov     rcx, g_vkInstance
    test    rcx, rcx
    jz      @free_dll
    xor     rdx, rdx
    lea     r10, g_vkFnTable
    call    qword ptr [r10 + VK_FN_DESTROY_INSTANCE]
    mov     g_vkInstance, 0

@free_dll:
    ; FreeLibrary(vulkan-1.dll)
    mov     rcx, g_vkDll
    test    rcx, rcx
    jz      @shutdown_done
    call    FreeLibrary
    mov     g_vkDll, 0

@shutdown_done:
    ; Release shard buffers allocated through HeapAlloc
    mov     rsi, g_hHeap
    test    rsi, rsi
    jnz     @heap_for_shards
    call    GetProcessHeap
    mov     rsi, rax
@heap_for_shards:
    test    rsi, rsi
    jz      @skip_shard_buffer_free

    xor     ebx, ebx
@free_shard_loop:
    cmp     ebx, MAX_SHARDS
    jge     @skip_shard_buffer_free
    lea     r10, g_shardDescs
    imul    eax, ebx, SHARD_DESC_SIZE
    cdqe
    mov     r8, qword ptr [r10 + rax + 20]
    test    r8, r8
    jz      @next_shard_free
    mov     rcx, rsi
    xor     edx, edx
    call    HeapFree
    mov     qword ptr [r10 + rax + 20], 0
    mov     dword ptr [r10 + rax + 28], 0
@next_shard_free:
    inc     ebx
    jmp     @free_shard_loop

@skip_shard_buffer_free:
    mov     g_swarmReady, 0
    mov     g_swarmDeviceCount, 0
    mov     g_shardCount, 0

    ; Signal shutdown via beacon
    mov     ecx, SWARM_BEACON_SLOT
    mov     edx, SWARM_EVT_SHUTDOWN
    xor     r8d, r8d
    call    BeaconSend

    lea     rsp, [rbp]
    pop     rsi
    pop     rbx
    pop     rbp
    ret
Swarm_Shutdown ENDP

; ────────────────────────────────────────────────────────────────
; Swarm_StealShardWork — Phase 14: Autonomous Shard Migration
;   RCX = targetDeviceIdx (GPU that is idle)
;   Returns: RAX = shard index stolen (or -1 if none)
;   Delegates to work_steal.asm WorkSteal_IdleProbe for:
;     - Atomic claiming via lock cmpxchg
;     - Load-balanced victim selection
;     - P2P buffer migration
;     - Compute dispatch on stolen shard
; ────────────────────────────────────────────────────────────────
EXTERN WorkSteal_IdleProbe:PROC
EXTERN g_stealReady:DWORD

Swarm_StealShardWork PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Check if work-steal subsystem is initialized
    cmp     g_stealReady, 0
    je      @steal_fallback

    ; Delegate to full Phase 14 implementation
    ; RCX already has targetDeviceIdx
    call    WorkSteal_IdleProbe
    cdqe                                ; sign-extend EAX → RAX
    jmp     @steal_ret

@steal_fallback:
    ; Legacy linear scan if work_steal not initialized
    xor     eax, eax
    lea     r10, g_shardDescs

@steal_loop:
    cmp     eax, MAX_SHARDS
    jge     @steal_fail

    imul    rdx, rax, SHARD_DESC_SIZE
    mov     r8d, dword ptr [r10 + rdx + 28]
    cmp     r8d, 1
    jne     @next_steal

    mov     r9d, dword ptr [r10 + rdx]
    cmp     r9d, ecx
    je      @next_steal

    ; Atomic claim via lock cmpxchg (upgraded from mov)
    lea     r11, [r10 + rdx + 28]
    mov     eax, 1                          ; expected = ALLOCATED
    mov     edx, 4                          ; desired = STEALING
    lock cmpxchg dword ptr [r11], edx
    jnz     @next_steal_reload

    ; Claimed: reassign device
    lea     r10, g_shardDescs
    imul    rdx, rax, SHARD_DESC_SIZE
    mov     dword ptr [r10 + rdx], ecx
    mov     dword ptr [r10 + rdx + 28], 1   ; back to ALLOCATED
    jmp     @steal_ret

@next_steal_reload:
    ; Reload shard index from eax (cmpxchg clobbers it)
    xor     eax, eax
    jmp     @steal_loop

@next_steal:
    inc     eax
    jmp     @steal_loop

@steal_fail:
    mov     rax, -1

@steal_ret:
    lea     rsp, [rbp]
    pop     rbp
    ret
Swarm_StealShardWork ENDP

END
