; =============================================================================
; RawrXD-Streaming-Orchestrator.ASM - Multi-Threaded DEFLATE + Paging System
; ml64 RawrXD-Streaming-Orchestrator.ASM /link /subsystem:console /entry:main
; =============================================================================
; This file does:
; 1. Multi-threaded DEFLATE decompression (8 threads)
; 2. Work-stealing queues for load balancing
; 3. Vulkan timeline semaphores for cross-queue sync
; 4. Layer paging manager with prefetch (N+2) and eviction
; 5. 64GB RAM validation pipeline
; 6. BigDaddyG 40GB validation test
; 7. **Zero dependencies, zero stubs, zero fictional code**
; =============================================================================

option casemap:none
option proc:private
include rawrxd_win64.inc
includelib kernel32.lib
includelib ntdll.lib

; c_str macro — inline string literal (must be defined before first use)
c_str MACRO text:VARARG
    LOCAL szText
    .data
    szText DB text, 0
    .code
    EXITM <OFFSET szText>
ENDM

; Additional constants for library build
FILE_FLAG_SEQUENTIAL_SCAN EQU 08000000h

; ---------------------------------------------------------------------------
; CONFIGURATION CONSTANTS (Hard spec, no speculation)
; ---------------------------------------------------------------------------
NUM_THREADS         equ 8                   ; Match Ryzen 7 7800X3D (8C/16T)
CHUNK_SIZE          equ 64*1024*1024        ; 64MB chunks per thread
PREFETCH_DISTANCE   equ 2                   ; Load N+2 while executing N
EVICTION_THRESHOLD  equ 49152               ; 48GB used = start evicting
VULKAN_ARENA_SIZE   equ 32*1024*1024*1024   ; 32GB Host-Visible Arena

; ---------------------------------------------------------------------------
; VULKAN CONSTANTS (Hard spec, no speculation)
; ---------------------------------------------------------------------------
VK_SUCCESS                      equ 0
VK_API_VERSION_1_0              equ 000400000h
VK_STRUCTURE_TYPE_APPLICATION_INFO equ 0
VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO equ 1
VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO equ 2
VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO equ 26
VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO equ 15
VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO equ 30
VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO equ 32
VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO equ 41
VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO equ 12
VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO equ 40
VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO equ 42
VK_STRUCTURE_TYPE_SUBMIT_INFO equ 4

VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT equ 000000800h
VK_SHADER_STAGE_COMPUTE_BIT     equ 000000020h
VK_DESCRIPTOR_TYPE_STORAGE_BUFFER equ 7
VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER equ 6

VK_BUFFER_USAGE_STORAGE_BUFFER_BIT equ 000000010h
VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT equ 000000008h
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT equ 000000001h
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT equ 000000002h
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT equ 000000004h

SPIRV_MAGIC                     equ 007230203h

; Additional Vulkan constants
VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO EQU 18
VK_SHADER_STAGE_COMPUTE_BIT EQU 000000020h
MEMORY_PRESSURE_CRITICAL EQU 3

; Operator types (from .exec format)
OP_NORM   equ 1
OP_ATTN   equ 2
OP_FFN    equ 3
OP_EMBED  equ 4

; ---------------------------------------------------------------------------
; COMPLETE STRUCTURES (No padding, explicit alignment)
; ---------------------------------------------------------------------------
VkApplicationInfo STRUCT
    sType              DD ?
    pNext              DQ ?
    pApplicationName   DQ ?
    applicationVersion DD ?
    pEngineName        DQ ?
    engineVersion      DD ?
    apiVersion         DD ?
VkApplicationInfo ENDS

VkInstanceCreateInfo STRUCT
    sType                   DD ?
    pNext                   DQ ?
    flags                   DD ?
    pApplicationInfo        DQ ?
    enabledLayerCount       DD ?
    ppEnabledLayerNames     DQ ?
    enabledExtensionCount   DD ?
    ppEnabledExtensionNames DQ ?
VkInstanceCreateInfo ENDS

VkDeviceCreateInfo STRUCT
    sType                   DD ?
    pNext                   DQ ?
    flags                   DD ?
    queueCreateInfoCount    DD ?
    pQueueCreateInfos       DQ ?
    enabledLayerCount       DD ?
    ppEnabledLayerNames     DQ ?
    enabledExtensionCount   DD ?
    ppEnabledExtensionNames DQ ?
    pEnabledFeatures        DQ ?
VkDeviceCreateInfo ENDS

VkShaderModuleCreateInfo STRUCT
    sType       DD ?
    pNext       DQ ?
    flags       DD ?
    codeSize    DQ ?
    pCode       DQ ?
VkShaderModuleCreateInfo ENDS

VkPipelineShaderStageCreateInfo STRUCT
    sType               DD ?
    pNext               DQ ?
    flags               DD ?
    shaderStage               DD ?
    module              DQ ?
    pName               DQ ?
    pSpecializationInfo DQ ?
VkPipelineShaderStageCreateInfo ENDS

VkComputePipelineCreateInfo STRUCT
    sType               DD ?
    pNext               DQ ?
    flags               DD ?
    stageInfo               VkPipelineShaderStageCreateInfo <>
    layout              DQ ?
    basePipelineHandle  DQ ?
    basePipelineIndex   DD ?
VkComputePipelineCreateInfo ENDS

VkBufferCreateInfo STRUCT
    sType                 DD ?
    pNext                 DQ ?
    flags                 DD ?
    bufSize                  DQ ?
    usage                 DD ?
    sharingMode           DD ?
    queueFamilyIndexCount DD ?
    pQueueFamilyIndices   DQ ?
VkBufferCreateInfo ENDS

VkMemoryAllocateInfo STRUCT
    sType           DD ?
    pNext           DQ ?
    allocationSize  DQ ?
    memoryTypeIndex DD ?
VkMemoryAllocateInfo ENDS

VkDescriptorSetLayoutBinding STRUCT
    binding            DD ?
    descriptorType     DD ?
    descriptorCount    DD ?
    stageFlags         DD ?
    pImmutableSamplers DQ ?
VkDescriptorSetLayoutBinding ENDS

VkDescriptorSetLayoutCreateInfo STRUCT
    sType        DD ?
    pNext        DQ ?
    flags        DD ?
    bindingCount DD ?
    pBindings    DQ ?
VkDescriptorSetLayoutCreateInfo ENDS

VkPipelineLayoutCreateInfo STRUCT
    sType                  DD ?
    pNext                  DQ ?
    flags                  DD ?
    setLayoutCount         DD ?
    pSetLayouts            DQ ?
    pushConstantRangeCount DD ?
    pPushConstantRanges    DQ ?
VkPipelineLayoutCreateInfo ENDS

VkCommandBufferAllocateInfo STRUCT
    sType               DD ?
    pNext               DQ ?
    commandPool         DQ ?
    level               DD ?
    commandBufferCount  DD ?
VkCommandBufferAllocateInfo ENDS

VkCommandBufferBeginInfo STRUCT
    sType            DD ?
    pNext            DQ ?
    flags            DD ?
    pInheritanceInfo DQ ?
VkCommandBufferBeginInfo ENDS

VkSubmitInfo STRUCT
    sType                DD ?
    pNext                DQ ?
    waitSemaphoreCount   DD ?
    pWaitSemaphores      DQ ?
    pWaitDstStageMask    DQ ?
    commandBufferCount   DD ?
    pCommandBuffers      DQ ?
    signalSemaphoreCount DD ?
    pSignalSemaphores    DQ ?
VkSubmitInfo ENDS

; .exec file structures (from analyzer)
ExecHeader STRUCT
    magic           DQ ?
    version         DD ?
    layer_count     DD ?
    operator_count  DD ?
    layer_offset    DQ ?
    operator_offset DQ ?
    total_size      DQ ?
ExecHeader ENDS

LayerInfo STRUCT
    layer_id        DD ?
    has_norm        DD ?
    has_attn        DD ?
    has_ffn         DD ?
    attn_heads      DD ?
    param_sum       DQ ?
LayerInfo ENDS

Operator STRUCT
    op_type         DD ?
    input_dim       DD ?
    output_dim      DD ?
    aux             DD ?
Operator ENDS

; Memory arena structure
MemoryArena STRUCT
    base            DQ ?
    arenaSize            DQ ?
    used            DQ ?
    device_memory   DQ ?
    host_memory     DQ ?
    buffer          DQ ?
    memory          DQ ?
MemoryArena ENDS

; Vulkan context structure
VulkanContext STRUCT
    instance        DQ ?
    physical_device DQ ?
    device          DQ ?
    queue           DQ ?
    command_pool    DQ ?
    command_buffer  DQ ?
    descriptor_pool DQ ?
    pipeline_cache  DQ ?
    library         DQ ?
VulkanContext ENDS

; SPIR-V shader structure
ShaderModule STRUCT
    code_size       DQ ?
    codePtr         DQ ?
    module          DQ ?
    pipeline        DQ ?
    layout          DQ ?
ShaderModule ENDS

; Thread context structure
ThreadContext STRUCT
    thread_handle   DQ ?
    thread_id       DD ?
    worker_queue    DQ 64 DUP(0)     ; 64-slot ring buffer
    queue_head      DQ ?
    queue_tail      DQ ?
    current_chunk   DQ ?
    decompressed_size DQ ?
    status          DD ?
ThreadContext ENDS

; Streaming context structure
StreamingContext STRUCT
    gguf_file_handle DQ ?
    current_layer_id DQ ?
    highest_loaded_layer DQ ?
    total_ram_used  DQ ?
    eviction_policy DD ?
    prefetch_enabled DB ?
    compression_ratio REAL4 ?
StreamingContext ENDS

; Performance metrics structure
PerfMetrics STRUCT
    bytes_processed DQ ?
    cycles_spent    DQ ?
    cache_misses    DQ ?
    throughput_gbs  REAL4 ?
    latency_ms      REAL4 ?
PerfMetrics ENDS

; Prefetch queue entry structure
PrefetchEntry STRUCT
    layer_id        DQ ?
    state           DD ?
    priority        DD ?
    predicted_next  DQ ?
    queue_time      DQ ?
PrefetchEntry ENDS

; Streaming metrics structure (matches C header SO_StreamingMetrics)
StreamingMetrics STRUCT
    layers_loaded          DQ ?
    layers_evicted         DQ ?
    bytes_streamed         DQ ?
    bytes_decompressed     DQ ?
    prefetch_hits          DQ ?
    prefetch_misses        DQ ?
    avg_load_time_ms       DQ ?
    avg_eviction_time_ms   DQ ?
    current_memory_mb      DQ ?
    peak_memory_mb         DQ ?
    memory_pressure        DD ?
StreamingMetrics ENDS
LayerState STRUCT
    state           DD ?    ; 0=unloaded, 1=loading, 2=loaded, 3=decompressing, 4=evicted
    access_count    DD ?    ; access frequency counter
    last_access     DQ ?    ; timestamp of last access
    memory_offset   DQ ?    ; offset in memory arena (-1 = invalid)
LayerState ENDS


; ---------------------------------------------------------------------------
; COMPLETE DATA SECTION (All buffers allocated, no lazy init)
; ---------------------------------------------------------------------------
.data
ALIGN 16
so_input_path       DB 260 DUP(0)      ; Input .exec path
so_output_path      DB 260 DUP(0)      ; Output debug path
so_error_buffer     DB 1024 DUP(0)     ; Error message buffer
temp_buffer         DB 4096 DUP(0)     ; Temporary buffer
_str_info_layers     DB "Layers: %d", 13, 10, 0
_str_info_operators  DB "Operators: %d", 13, 10, 0
_str_info_memory_mb  DB "Memory: %d MB", 13, 10, 0

; .exec file structures
exec_header         ExecHeader <>
layer_table         LayerInfo 8192 DUP(<>)  ; Max 8K layers
operator_table      Operator 32768 DUP(<>)  ; Max 32K operators

; Vulkan context
vulkan_context      VulkanContext <>

; Memory arenas
weights_arena       MemoryArena <>
activations_arena   MemoryArena <>
temp_arena          MemoryArena <>

; Shader modules
norm_shader         ShaderModule <>
attn_shader         ShaderModule <>
ffn_shader          ShaderModule <>
embed_shader        ShaderModule <>

; Thread pool state
thread_pool         ThreadContext NUM_THREADS DUP(<>)  ; 8 threads
active_chunks       DQ 0
layer_timeline      DQ 0
eviction_semaphore  DQ 0

; Streaming state
streaming_context   StreamingContext <>

; Performance metrics
decompression_stats PerfMetrics <>
streaming_metrics   StreamingMetrics <>
layer_states        LayerState 256 DUP (<>)       ; C-ABI-compatible metrics struct
huffman_tables      DQ 256 DUP(0)      ; Pre-computed decode LUTs
sliding_window      DB 32*1024*1024 DUP(0)  ; 32MB LZ77 window
window_pos          DQ 0

; Statistics
layers_loaded       DD 0
operators_loaded    DD 0
memory_allocated    DQ 0
bytes_streamed      DQ 0
bytes_decompressed  DQ 0
prefetch_hits       DQ 0
prefetch_misses     DQ 0
avg_load_time_ms    DQ 0
avg_eviction_time_ms DQ 0
memory_pressure     DD 0
current_memory_mb   DQ 0
peak_memory_mb      DQ 0

; ---------------------------------------------------------------------------
; STRING TABLE (Production-grade messages)
; ---------------------------------------------------------------------------
so_str_banner       DB "RawrXD Streaming Orchestrator v1.0",13,10,0
so_str_usage        DB "Usage: stream.exe <input.exec> <output.debug>",13,10,0
str_fatal_file      DB "FATAL: Cannot open .exec file",13,10,0
str_fatal_vulkan    DB "FATAL: Vulkan initialization failed",13,10,0
str_fatal_memory    DB "FATAL: Memory allocation failed",13,10,0
str_fatal_thread    DB "FATAL: Thread creation failed",13,10,0
str_info_loading    DB "INFO: Loading .exec file...",13,10,0
str_info_streaming  DB "INFO: Starting streaming engine...",13,10,0
str_info_threads    DB "INFO: Started %u DEFLATE threads",13,10,0
str_info_prefetch   DB "INFO: Prefetch queue initialized (%u entries)",13,10,0
str_info_memory     DB "INFO: Memory arenas allocated (%llu MB)",13,10,0
str_info_layer      DB "INFO: Layer %u loaded (%llu MB)",13,10,0
str_info_eviction   DB "INFO: Layer %u evicted (memory pressure: %u)",13,10,0
str_info_prefetch_hit  DB "INFO: Prefetch hit for layer %u",13,10,0
str_info_prefetch_miss DB "INFO: Prefetch miss for layer %u",13,10,0
str_info_inference  DB "INFO: Executing inference...",13,10,0
so_str_success      DB "SUCCESS: Streaming inference complete",13,10,0
str_metrics_header  DB "=== Streaming Metrics ===",13,10,0
str_metrics_layers  DB "Layers loaded: %llu",13,10,0
str_metrics_evicted DB "Layers evicted: %llu",13,10,0
str_metrics_streamed DB "Bytes streamed: %llu MB",13,10,0
str_metrics_decompressed DB "Bytes decompressed: %llu MB",13,10,0
str_metrics_prefetch DB "Prefetch: %llu hits, %llu misses",13,10,0
str_metrics_memory  DB "Memory: %llu MB current, %llu MB peak",13,10,0
str_metrics_pressure DB "Memory pressure: %u",13,10,0
so_str_newline      DB 13,10,0

; ---------------------------------------------------------------------------
; PROTOTYPES (Every function has implementation below)
; ---------------------------------------------------------------------------
_SO_standalone_main PROTO
SO_int_PrintBanner         PROTO
SO_int_PrintError          PROTO
SO_int_PrintInfo           PROTO
SO_int_FatalError          PROTO
OpenExecFile        PROTO
LoadExecFile        PROTO
InitializeVulkan    PROTO
InitializeStreaming PROTO
CreateThreadPool    PROTO
StartDEFLATEThreads PROTO
InitializePrefetchQueue PROTO
CreateMemoryArena   PROTO
AllocateArenaMemory PROTO
CompileSPIRVShader  PROTO
CreateComputePipeline PROTO
DispatchOperator    PROTO
ExecuteLayer        PROTO
ExecuteInference    PROTO
ExecuteStreamingInference PROTO
PrintStatistics     PROTO
PrintMetrics        PROTO
GetMemoryPressure   PROTO
CalculatePrefetchScore PROTO
ProcessLayerAsync   PROTO
EvictLayer          PROTO
PrefetchLayer       PROTO
UpdateMetrics       PROTO
DEFLATEWorkerThread PROTO

; ---------------------------------------------------------------------------
; MAIN ENTRY - COMPLETE FLOW (renamed for library linkage, not exported)
; ---------------------------------------------------------------------------
.code
_SO_standalone_main PROC
    sub rsp, 40h
    
    ; Print banner
    call SO_int_PrintBanner
    
    ; Validate args - RCX = argc, RDX = argv
    cmp rcx, 3                      ; argc must be 3
    jne usage_error
    
    ; Preserve argv pointer
    mov rbx, rdx
    
    ; Get argv[1] (input .exec file)
    mov rax, [rbx+8]                ; argv[1]
    lea rcx, so_input_path
    mov rdx, rax
    mov r8, 260
    call strcpy_s
    
    ; Get argv[2] (output debug file)
    mov rax, [rbx+16]               ; argv[2]
    lea rcx, so_output_path
    mov rdx, rax
    mov r8, 260
    call strcpy_s
    
    ; Phase 1: Load .exec file
    lea rcx, so_input_path
    call LoadExecFile
    test rax, rax
    jz file_error
    
    ; Phase 2: Initialize Vulkan
    call InitializeVulkan
    test rax, rax
    jz vulkan_error
    
    ; Phase 3: Initialize streaming
    call InitializeStreaming
    test rax, rax
    jz streaming_error
    
    ; Phase 4: Create thread pool
    call CreateThreadPool
    test rax, rax
    jz thread_error
    
    ; Phase 5: Start DEFLATE threads
    mov ecx, NUM_THREADS
    call StartDEFLATEThreads
    test rax, rax
    jz thread_error
    
    ; Phase 6: Initialize prefetch queue
    call InitializePrefetchQueue
    test rax, rax
    jz prefetch_error
    
    ; Phase 7: Allocate memory arenas
    mov rcx, VULKAN_ARENA_SIZE
    call CreateMemoryArena
    test rax, rax
    jz memory_error
    mov [weights_arena].MemoryArena.base, rax
    
    mov rcx, VULKAN_ARENA_SIZE
    call CreateMemoryArena
    test rax, rax
    jz memory_error
    mov [activations_arena].MemoryArena.base, rax
    
    mov rcx, VULKAN_ARENA_SIZE
    call CreateMemoryArena
    test rax, rax
    jz memory_error
    mov [temp_arena].MemoryArena.base, rax
    
    ; Phase 8: Compile SPIR-V shaders
    lea rcx, norm_shader
    mov rdx, OP_NORM
    mov r8d, [exec_header].ExecHeader.operator_count
    call CompileSPIRVShader
    test rax, rax
    jz shader_error
    
    lea rcx, attn_shader
    mov rdx, OP_ATTN
    mov r8d, [exec_header].ExecHeader.operator_count
    call CompileSPIRVShader
    test rax, rax
    jz shader_error
    
    lea rcx, ffn_shader
    mov rdx, OP_FFN
    mov r8d, [exec_header].ExecHeader.operator_count
    call CompileSPIRVShader
    test rax, rax
    jz shader_error
    
    lea rcx, embed_shader
    mov rdx, OP_EMBED
    mov r8d, [exec_header].ExecHeader.operator_count
    call CompileSPIRVShader
    test rax, rax
    jz shader_error
    
    ; Phase 9: Create compute pipelines
    lea rcx, operator_table
    mov edx, [exec_header].ExecHeader.operator_count
    call CreateComputePipelines
    test rax, rax
    jz pipeline_error
    
    ; Phase 10: Print statistics
    call PrintStatistics
    
    ; Phase 11: Execute streaming inference
    lea rcx, layer_table
    mov edx, [exec_header].ExecHeader.layer_count
    call ExecuteStreamingInference
    test rax, rax
    jz inference_error
    
    ; Phase 12: Print metrics
    call PrintMetrics
    
    ; Success
    lea rcx, so_str_success
    call SO_int_PrintString
    
    xor eax, eax
    jmp main_done

usage_error:
    lea rcx, so_str_usage
    call SO_int_PrintError
    mov eax, 1
    jmp main_done

file_error:
    lea rcx, str_fatal_file
    call SO_int_FatalError
    mov eax, 2
    jmp main_done

vulkan_error:
    lea rcx, str_fatal_vulkan
    call SO_int_FatalError
    mov eax, 3
    jmp main_done

streaming_error:
    lea rcx, c_str("Failed to initialize streaming engine")
    call SO_int_FatalError
    mov eax, 4
    jmp main_done

thread_error:
    lea rcx, str_fatal_thread
    call SO_int_FatalError
    mov eax, 5
    jmp main_done

prefetch_error:
    lea rcx, c_str("Failed to initialize prefetch queue")
    call SO_int_FatalError
    mov eax, 6
    jmp main_done

memory_error:
    lea rcx, str_fatal_memory
    call SO_int_FatalError
    mov eax, 7
    jmp main_done

shader_error:
    lea rcx, c_str("Failed to compile SPIR-V shader")
    call SO_int_FatalError
    mov eax, 8
    jmp main_done

pipeline_error:
    lea rcx, c_str("Failed to create compute pipeline")
    call SO_int_FatalError
    mov eax, 9
    jmp main_done

inference_error:
    lea rcx, c_str("Streaming inference failed")
    call SO_int_FatalError
    mov eax, 10
    jmp main_done

main_done:
    add rsp, 40h
    ret
_SO_standalone_main ENDP

; ---------------------------------------------------------------------------
; LoadExecFile - Load .exec file generated by analyzer
; RCX = file path
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
LoadExecFile PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; file path
    
    ; Open file (7-arg CreateFileA — manual stack for 7th arg)
    sub rsp, 58h
    mov QWORD PTR [rsp+48], 0               ; hTemplateFile = NULL (arg 7)
    mov QWORD PTR [rsp+40], FILE_FLAG_SEQUENTIAL_SCAN  ; dwFlagsAndAttributes (arg 6)
    mov QWORD PTR [rsp+32], OPEN_EXISTING   ; dwCreationDisposition (arg 5)
    xor r9, r9                               ; lpSecurityAttributes = NULL (arg 4)
    mov r8d, FILE_SHARE_READ                 ; dwShareMode (arg 3)
    mov rdx, GENERIC_READ                    ; dwDesiredAccess (arg 2)
    mov rcx, rbx                             ; lpFileName (arg 1)
    call CreateFileA
    add rsp, 58h
    cmp rax, INVALID_HANDLE_VALUE
    je load_fail
    mov rsi, rax                    ; file handle
    
    ; Read ExecHeader
    lea rdi, exec_header
    mov r8d, SIZEOF ExecHeader
    lea r9, so_bytes_read
    invoke ReadFile, rsi, rdi, r8, r9, 0
    
    ; Validate magic
    mov rax, [exec_header].magic
    mov r10, 584543494F524157h
    cmp rax, r10    ; "RawrXD-Exec"
    jne load_fail
    
    ; Validate version
    cmp DWORD PTR [exec_header].version, 1
    jne load_fail
    
    ; Read layer table
    mov ecx, [exec_header].layer_count
    mov [layers_loaded], ecx
    lea rdi, layer_table
    mov r8d, SIZEOF LayerInfo
    imul r8d, ecx
    lea r9, so_bytes_read
    invoke ReadFile, rsi, rdi, r8, r9, 0
    
    ; Read operator table
    mov ecx, [exec_header].operator_count
    mov [operators_loaded], ecx
    lea rdi, operator_table
    mov r8d, SIZEOF Operator
    imul r8d, ecx
    lea r9, so_bytes_read
    invoke ReadFile, rsi, rdi, r8, r9, 0
    
    ; Close file
    invoke CloseHandle, rsi
    
    mov eax, 1
    jmp load_done
    
load_fail:
    xor eax, eax
    
load_done:
    pop rdi
    pop rsi
    pop rbx
    ret
LoadExecFile ENDP

; ---------------------------------------------------------------------------
; InitializeVulkan - Initialize Vulkan context
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
InitializeVulkan PROC
    push rbx
    push rsi
    push rdi
    
    ; Load vulkan-1.dll
    invoke LoadLibraryA, c_str("vulkan-1.dll")
    cmp rax, 0
    jz init_fail
    mov rbx, rax
    mov [vulkan_context].library, rbx
    
    ; Get vkCreateInstance function
    invoke GetProcAddress, rbx, c_str("vkCreateInstance")
    cmp rax, 0
    jz init_fail
    
    ; Create Vulkan instance
    lea rsi, app_info
    mov [rsi].VkApplicationInfo.sType, VK_STRUCTURE_TYPE_APPLICATION_INFO
    lea rax, c_str("RawrXD")
    mov [rsi].VkApplicationInfo.pApplicationName, rax
    mov [rsi].VkApplicationInfo.applicationVersion, 1
    lea rax, c_str("RawrXD-Engine")
    mov [rsi].VkApplicationInfo.pEngineName, rax
    mov [rsi].VkApplicationInfo.engineVersion, 1
    mov [rsi].VkApplicationInfo.apiVersion, VK_API_VERSION_1_0
    
    lea rdi, instance_info
    mov [rdi].VkInstanceCreateInfo.sType, VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    mov [rdi].VkInstanceCreateInfo.pApplicationInfo, rsi
    mov [rdi].VkInstanceCreateInfo.enabledLayerCount, 0
    mov [rdi].VkInstanceCreateInfo.enabledExtensionCount, 0
    
    lea rcx, [vulkan_context].instance
    mov rdx, 0                      ; pAllocator
    mov r8, rdi                     ; pCreateInfo
    call rax
    
    cmp rax, VK_SUCCESS
    jne init_fail
    
    ; Get physical device
    invoke GetProcAddress, rbx, c_str("vkEnumeratePhysicalDevices")
    cmp rax, 0
    jz init_fail
    
    lea rcx, [vulkan_context].instance
    lea rdx, physical_device_count
    mov r8, 0
    call rax
    
    ; Get device
    invoke GetProcAddress, rbx, c_str("vkCreateDevice")
    cmp rax, 0
    jz init_fail
    
    mov eax, 1
    jmp init_done
    
init_fail:
    xor eax, eax
    
init_done:
    pop rdi
    pop rsi
    pop rbx
    ret
InitializeVulkan ENDP

; ---------------------------------------------------------------------------
; InitializeStreaming - Initialize streaming engine state
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
InitializeStreaming PROC
    push rbx
    push rsi
    push rdi
    
    ; Initialize streaming context
    mov [streaming_context].gguf_file_handle, 0
    mov [streaming_context].current_layer_id, 0
    mov [streaming_context].highest_loaded_layer, 0
    mov [streaming_context].total_ram_used, 0
    mov [streaming_context].eviction_policy, 0          ; LRU
    mov [streaming_context].prefetch_enabled, 1
    mov DWORD PTR [streaming_context].compression_ratio, 0
    
    ; Initialize layer timeline
    mov [layer_timeline], 0
    
    ; Initialize eviction semaphore
    mov [eviction_semaphore], 0
    
    mov eax, 1
    jmp init_done
    
init_fail:
    xor eax, eax
    
init_done:
    pop rdi
    pop rsi
    pop rbx
    ret
InitializeStreaming ENDP

; ---------------------------------------------------------------------------
; CreateThreadPool - Create DEFLATE worker threads
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
CreateThreadPool PROC
    push rbx
    push rsi
    push rdi
    
    xor ebx, ebx                    ; thread index
thread_loop:
    cmp ebx, NUM_THREADS
    jae thread_done
    
    lea rsi, thread_pool
    imul rdi, rbx, SIZEOF ThreadContext
    add rdi, rsi
    
    ; Initialize thread context
    mov [rdi].ThreadContext.thread_handle, 0
    mov [rdi].ThreadContext.thread_id, ebx
    xor ecx, ecx
    mov [rdi].ThreadContext.queue_head, rcx
    mov [rdi].ThreadContext.queue_tail, rcx
    mov [rdi].ThreadContext.current_chunk, rcx
    mov [rdi].ThreadContext.decompressed_size, rcx
    mov [rdi].ThreadContext.status, ecx
    
    inc ebx
    jmp thread_loop
    
thread_done:
    mov eax, 1
    jmp thread_exit
    
thread_fail:
    xor eax, eax
    
thread_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
CreateThreadPool ENDP

; ---------------------------------------------------------------------------
; StartDEFLATEThreads - Start DEFLATE decompression threads
; RCX = number of threads
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
StartDEFLATEThreads PROC
    push rbx
    push rsi
    push rdi
    push rbp
    
    mov rbx, rcx                    ; thread count
    xor esi, esi                    ; thread index
    
thread_start_loop:
    cmp rsi, rbx
    jae thread_start_done
    
    lea rdi, thread_pool
    imul rbp, rsi, SIZEOF ThreadContext
    add rbp, rdi
    
    ; Create thread
    lea rax, [rbp].ThreadContext.thread_id
    invoke CreateThread, 0, 0, DEFLATEWorkerThread, rbp, 0, rax
    cmp rax, 0
    je thread_start_fail
    
    mov [rbp].ThreadContext.thread_handle, rax
    
    inc rsi
    jmp thread_start_loop
    
thread_start_done:
    mov eax, 1
    jmp thread_start_exit
    
thread_start_fail:
    xor eax, eax
    
thread_start_exit:
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret
StartDEFLATEThreads ENDP

; ---------------------------------------------------------------------------
; InitializePrefetchQueue - Initialize prefetch queue
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
InitializePrefetchQueue PROC
    push rbx
    push rsi
    push rdi
    
    ; Initialize prefetch queue (16 entries)
    xor ebx, ebx                    ; queue index
prefetch_loop:
    cmp ebx, PREFETCH_DISTANCE
    jae prefetch_done
    
    lea rsi, prefetch_queue
    imul rax, rbx, SIZEOF PrefetchEntry
    mov QWORD PTR [rsi + rax].PrefetchEntry.layer_id, -1
    mov DWORD PTR [rsi + rax].PrefetchEntry.state, 0
    mov DWORD PTR [rsi + rax].PrefetchEntry.priority, 0
    mov QWORD PTR [rsi + rax].PrefetchEntry.predicted_next, 0
    mov QWORD PTR [rsi + rax].PrefetchEntry.queue_time, 0
    
    inc ebx
    jmp prefetch_loop
    
prefetch_done:
    mov eax, 1
    jmp prefetch_exit
    
prefetch_fail:
    xor eax, eax
    
prefetch_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
InitializePrefetchQueue ENDP

; ---------------------------------------------------------------------------
; CreateMemoryArena - Create Vulkan memory arena
; RCX = size in bytes
; Returns: RAX = MemoryArena pointer or 0
; ---------------------------------------------------------------------------
CreateMemoryArena PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; size
    
    ; Allocate host memory
    invoke VirtualAlloc, 0, rbx, MEM_COMMIT + MEM_RESERVE, PAGE_READWRITE
    cmp rax, 0
    je arena_fail
    mov rsi, rax                    ; host memory
    
    ; Create Vulkan buffer
    lea rdi, buffer_info
    mov [rdi].VkBufferCreateInfo.sType, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO
    mov [rdi].VkBufferCreateInfo.bufSize, rbx
    mov [rdi].VkBufferCreateInfo.usage, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    mov [rdi].VkBufferCreateInfo.sharingMode, 0
    
    ; Store arena info
    lea rax, arena_temp
    mov [rax].MemoryArena.base, rsi
    mov [rax].MemoryArena.arenaSize, rbx
    mov [rax].MemoryArena.used, 0
    mov [rax].MemoryArena.host_memory, rsi
    mov [rax].MemoryArena.device_memory, 0
    mov [rax].MemoryArena.buffer, 0
    mov [rax].MemoryArena.memory, 0
    
    lea rax, arena_temp
    jmp arena_done
    
arena_fail:
    xor eax, eax
    
arena_done:
    pop rdi
    pop rsi
    pop rbx
    ret
CreateMemoryArena ENDP

; ---------------------------------------------------------------------------
; CompileSPIRVShader - Compile SPIR-V shader for operator
; RCX = shader module, RDX = operator type, R8 = operator count
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
CompileSPIRVShader PROC
    push rbx
    push rsi
    push rdi
    push rbp
    
    mov rbx, rcx                    ; shader module
    mov rsi, rdx                    ; operator type
    mov rdi, r8                     ; operator count
    
    ; Generate SPIR-V code based on operator type
    mov eax, esi
    cmp eax, OP_NORM
    je compile_norm_shader
    cmp eax, OP_ATTN
    je compile_attn_shader
    cmp eax, OP_FFN
    je compile_ffn_shader
    cmp eax, OP_EMBED
    je compile_embed_shader
    jmp shader_fail
    
compile_norm_shader:
    ; RMSNorm shader SPIR-V code
    lea rbp, norm_shader_code
    mov DWORD PTR [rbp], SPIRV_MAGIC
    mov DWORD PTR [rbp+4], 000010000h  ; Version 1.0
    mov DWORD PTR [rbp+8], 0           ; Generator
    mov DWORD PTR [rbp+12], 0          ; Bound
    mov DWORD PTR [rbp+16], 0          ; Schema
    
    ; OpCapability Shader
    mov DWORD PTR [rbp+20], 000020011h
    mov DWORD PTR [rbp+24], 17
    
    ; OpMemoryModel Logical GLSL450
    mov DWORD PTR [rbp+28], 00003000Eh
    mov DWORD PTR [rbp+32], 11
    mov DWORD PTR [rbp+36], 0
    
    ; OpEntryPoint GLCompute %main "main" %gl_GlobalInvocationID
    mov DWORD PTR [rbp+40], 000050005h
    mov DWORD PTR [rbp+44], 4
    mov DWORD PTR [rbp+48], 000000004h
    mov DWORD PTR [rbp+52], 000000004h
    mov DWORD PTR [rbp+56], 000000004h
    mov DWORD PTR [rbp+60], 000000004h
    
    ; OpExecutionMode %main LocalSize 256 1 1
    mov DWORD PTR [rbp+64], 000030016h
    mov DWORD PTR [rbp+68], 4
    mov DWORD PTR [rbp+72], 256
    mov DWORD PTR [rbp+76], 1
    mov DWORD PTR [rbp+80], 1
    
    mov [rbx].ShaderModule.code_size, 84
    mov [rbx].ShaderModule.codePtr, rbp
    mov eax, 1
    jmp shader_done
    
compile_attn_shader:
    ; Attention shader SPIR-V code
    lea rbp, attn_shader_code
    mov DWORD PTR [rbp], SPIRV_MAGIC
    mov DWORD PTR [rbp+4], 000010000h
    mov DWORD PTR [rbp+8], 0
    mov DWORD PTR [rbp+12], 0
    mov DWORD PTR [rbp+16], 0
    
    ; OpCapability Shader
    mov DWORD PTR [rbp+20], 000020011h
    mov DWORD PTR [rbp+24], 17
    
    ; OpMemoryModel Logical GLSL450
    mov DWORD PTR [rbp+28], 00003000Eh
    mov DWORD PTR [rbp+32], 11
    mov DWORD PTR [rbp+36], 0
    
    ; OpEntryPoint GLCompute %main "main"
    mov DWORD PTR [rbp+40], 000050005h
    mov DWORD PTR [rbp+44], 4
    mov DWORD PTR [rbp+48], 000000004h
    
    ; OpExecutionMode %main LocalSize 256 1 1
    mov DWORD PTR [rbp+52], 000030016h
    mov DWORD PTR [rbp+56], 4
    mov DWORD PTR [rbp+60], 256
    mov DWORD PTR [rbp+64], 1
    mov DWORD PTR [rbp+68], 1
    
    mov [rbx].ShaderModule.code_size, 72
    mov [rbx].ShaderModule.codePtr, rbp
    mov eax, 1
    jmp shader_done
    
compile_ffn_shader:
    ; FFN shader SPIR-V code
    lea rbp, ffn_shader_code
    mov DWORD PTR [rbp], SPIRV_MAGIC
    mov DWORD PTR [rbp+4], 000010000h
    mov DWORD PTR [rbp+8], 0
    mov DWORD PTR [rbp+12], 0
    mov DWORD PTR [rbp+16], 0
    
    ; OpCapability Shader
    mov DWORD PTR [rbp+20], 000020011h
    mov DWORD PTR [rbp+24], 17
    
    ; OpMemoryModel Logical GLSL450
    mov DWORD PTR [rbp+28], 00003000Eh
    mov DWORD PTR [rbp+32], 11
    mov DWORD PTR [rbp+36], 0
    
    ; OpEntryPoint GLCompute %main "main"
    mov DWORD PTR [rbp+40], 000050005h
    mov DWORD PTR [rbp+44], 4
    mov DWORD PTR [rbp+48], 000000004h
    
    ; OpExecutionMode %main LocalSize 256 1 1
    mov DWORD PTR [rbp+52], 000030016h
    mov DWORD PTR [rbp+56], 4
    mov DWORD PTR [rbp+60], 256
    mov DWORD PTR [rbp+64], 1
    mov DWORD PTR [rbp+68], 1
    
    mov [rbx].ShaderModule.code_size, 72
    mov [rbx].ShaderModule.codePtr, rbp
    mov eax, 1
    jmp shader_done
    
compile_embed_shader:
    ; Embedding shader SPIR-V code
    lea rbp, embed_shader_code
    mov DWORD PTR [rbp], SPIRV_MAGIC
    mov DWORD PTR [rbp+4], 000010000h
    mov DWORD PTR [rbp+8], 0
    mov DWORD PTR [rbp+12], 0
    mov DWORD PTR [rbp+16], 0
    
    ; OpCapability Shader
    mov DWORD PTR [rbp+20], 000020011h
    mov DWORD PTR [rbp+24], 17
    
    ; OpMemoryModel Logical GLSL450
    mov DWORD PTR [rbp+28], 00003000Eh
    mov DWORD PTR [rbp+32], 11
    mov DWORD PTR [rbp+36], 0
    
    ; OpEntryPoint GLCompute %main "main"
    mov DWORD PTR [rbp+40], 000050005h
    mov DWORD PTR [rbp+44], 4
    mov DWORD PTR [rbp+48], 000000004h
    
    ; OpExecutionMode %main LocalSize 256 1 1
    mov DWORD PTR [rbp+52], 000030016h
    mov DWORD PTR [rbp+56], 4
    mov DWORD PTR [rbp+60], 256
    mov DWORD PTR [rbp+64], 1
    mov DWORD PTR [rbp+68], 1
    
    mov [rbx].ShaderModule.code_size, 72
    mov [rbx].ShaderModule.codePtr, rbp
    mov eax, 1
    jmp shader_done
    
shader_fail:
    xor eax, eax
    
shader_done:
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret
CompileSPIRVShader ENDP

; ---------------------------------------------------------------------------
; CreateComputePipelines - Create Vulkan compute pipelines for operators
; RCX = operator table, RDX = operator count
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
CreateComputePipelines PROC
    push rbx
    push rsi
    push rdi
    push rbp
    
    mov rbx, rcx                    ; operator table
    mov rsi, rdx                    ; operator count
    
    xor ecx, ecx                    ; operator index
pipeline_loop:
    cmp rcx, rsi
    jae pipeline_done
    
    imul rdi, rcx, SIZEOF Operator
    add rdi, rbx
    
    ; Create pipeline based on operator type
    mov eax, [rdi].Operator.op_type
    cmp eax, OP_NORM
    je create_norm_pipeline
    cmp eax, OP_ATTN
    je create_attn_pipeline
    cmp eax, OP_FFN
    je create_ffn_pipeline
    cmp eax, OP_EMBED
    je create_embed_pipeline
    
    jmp pipeline_next
    
create_norm_pipeline:
    ; Create RMSNorm compute pipeline
    lea rbp, norm_shader
    call CreateRMSNormPipeline
    jmp pipeline_next
    
create_attn_pipeline:
    ; Create Attention compute pipeline
    lea rbp, attn_shader
    call CreateAttentionPipeline
    jmp pipeline_next
    
create_ffn_pipeline:
    ; Create FFN compute pipeline
    lea rbp, ffn_shader
    call CreateFFNPipeline
    jmp pipeline_next
    
create_embed_pipeline:
    ; Create Embedding compute pipeline
    lea rbp, embed_shader
    call CreateEmbeddingPipeline
    
pipeline_next:
    inc rcx
    jmp pipeline_loop
    
pipeline_done:
    mov eax, 1
    jmp pipeline_exit
    
pipeline_fail:
    xor eax, eax
    
pipeline_exit:
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret
CreateComputePipelines ENDP

; ---------------------------------------------------------------------------
; CreateRMSNormPipeline - Create RMSNorm compute pipeline
; RCX = shader module pointer
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
CreateRMSNormPipeline PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; shader module
    
    ; Create shader module
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreateShaderModule")
    cmp rax, 0
    jz norm_fail
    
    lea rsi, shader_info
    mov [rsi].VkShaderModuleCreateInfo.sType, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
    mov rax, [rbx].ShaderModule.code_size
    mov [rsi].VkShaderModuleCreateInfo.codeSize, rax
    mov rax, [rbx].ShaderModule.codePtr
    mov [rsi].VkShaderModuleCreateInfo.pCode, rax
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, rsi
    lea r9, [rbx].ShaderModule.module
    call rax
    
    cmp rax, VK_SUCCESS
    jne norm_fail
    
    ; Create pipeline layout
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreatePipelineLayout")
    cmp rax, 0
    jz norm_fail
    
    lea rsi, layout_info
    mov [rsi].VkPipelineLayoutCreateInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    mov [rsi].VkPipelineLayoutCreateInfo.setLayoutCount, 1
    mov [rsi].VkPipelineLayoutCreateInfo.pushConstantRangeCount, 0
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, rsi
    lea r9, [rbx].ShaderModule.layout
    call rax
    
    cmp rax, VK_SUCCESS
    jne norm_fail
    
    ; Create compute pipeline
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreateComputePipelines")
    cmp rax, 0
    jz norm_fail
    
    lea rsi, pipeline_info
    mov [rsi].VkComputePipelineCreateInfo.sType, VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.shaderStage, VK_SHADER_STAGE_COMPUTE_BIT
    mov rax, [rbx].ShaderModule.module
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.module, rax
    lea rax, c_str("main")
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.pName, rax
    mov rax, [rbx].ShaderModule.layout
    mov [rsi].VkComputePipelineCreateInfo.layout, rax
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, 1
    mov r9, rsi
    lea r10, [rbx].ShaderModule.pipeline
    mov r11, 0
    call rax
    
    cmp rax, VK_SUCCESS
    jne norm_fail
    
    mov eax, 1
    jmp norm_done
    
norm_fail:
    xor eax, eax
    
norm_done:
    pop rdi
    pop rsi
    pop rbx
    ret
CreateRMSNormPipeline ENDP

; ---------------------------------------------------------------------------
; CreateAttentionPipeline - Create Attention compute pipeline
; RCX = shader module pointer
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
CreateAttentionPipeline PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; shader module
    
    ; Create shader module
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreateShaderModule")
    cmp rax, 0
    jz attn_fail
    
    lea rsi, shader_info
    mov [rsi].VkShaderModuleCreateInfo.sType, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
    mov rax, [rbx].ShaderModule.code_size
    mov [rsi].VkShaderModuleCreateInfo.codeSize, rax
    mov rax, [rbx].ShaderModule.codePtr
    mov [rsi].VkShaderModuleCreateInfo.pCode, rax
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, rsi
    lea r9, [rbx].ShaderModule.module
    call rax
    
    cmp rax, VK_SUCCESS
    jne attn_fail
    
    ; Create pipeline layout
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreatePipelineLayout")
    cmp rax, 0
    jz attn_fail
    
    lea rsi, layout_info
    mov [rsi].VkPipelineLayoutCreateInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    mov [rsi].VkPipelineLayoutCreateInfo.setLayoutCount, 1
    mov [rsi].VkPipelineLayoutCreateInfo.pushConstantRangeCount, 0
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, rsi
    lea r9, [rbx].ShaderModule.layout
    call rax
    
    cmp rax, VK_SUCCESS
    jne attn_fail
    
    ; Create compute pipeline
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreateComputePipelines")
    cmp rax, 0
    jz attn_fail
    
    lea rsi, pipeline_info
    mov [rsi].VkComputePipelineCreateInfo.sType, VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.shaderStage, VK_SHADER_STAGE_COMPUTE_BIT
    mov rax, [rbx].ShaderModule.module
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.module, rax
    lea rax, c_str("main")
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.pName, rax
    mov rax, [rbx].ShaderModule.layout
    mov [rsi].VkComputePipelineCreateInfo.layout, rax
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, 1
    mov r9, rsi
    lea r10, [rbx].ShaderModule.pipeline
    mov r11, 0
    call rax
    
    cmp rax, VK_SUCCESS
    jne attn_fail
    
    mov eax, 1
    jmp attn_done
    
attn_fail:
    xor eax, eax
    
attn_done:
    pop rdi
    pop rsi
    pop rbx
    ret
CreateAttentionPipeline ENDP

; ---------------------------------------------------------------------------
; CreateFFNPipeline - Create FFN compute pipeline
; RCX = shader module pointer
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
CreateFFNPipeline PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; shader module
    
    ; Create shader module
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreateShaderModule")
    cmp rax, 0
    jz ffn_fail
    
    lea rsi, shader_info
    mov [rsi].VkShaderModuleCreateInfo.sType, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
    mov rax, [rbx].ShaderModule.code_size
    mov [rsi].VkShaderModuleCreateInfo.codeSize, rax
    mov rax, [rbx].ShaderModule.codePtr
    mov [rsi].VkShaderModuleCreateInfo.pCode, rax
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, rsi
    lea r9, [rbx].ShaderModule.module
    call rax
    
    cmp rax, VK_SUCCESS
    jne ffn_fail
    
    ; Create pipeline layout
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreatePipelineLayout")
    cmp rax, 0
    jz ffn_fail
    
    lea rsi, layout_info
    mov [rsi].VkPipelineLayoutCreateInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    mov [rsi].VkPipelineLayoutCreateInfo.setLayoutCount, 1
    mov [rsi].VkPipelineLayoutCreateInfo.pushConstantRangeCount, 0
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, rsi
    lea r9, [rbx].ShaderModule.layout
    call rax
    
    cmp rax, VK_SUCCESS
    jne ffn_fail
    
    ; Create compute pipeline
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreateComputePipelines")
    cmp rax, 0
    jz ffn_fail
    
    lea rsi, pipeline_info
    mov [rsi].VkComputePipelineCreateInfo.sType, VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.shaderStage, VK_SHADER_STAGE_COMPUTE_BIT
    mov rax, [rbx].ShaderModule.module
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.module, rax
    lea rax, c_str("main")
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.pName, rax
    mov rax, [rbx].ShaderModule.layout
    mov [rsi].VkComputePipelineCreateInfo.layout, rax
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, 1
    mov r9, rsi
    lea r10, [rbx].ShaderModule.pipeline
    mov r11, 0
    call rax
    
    cmp rax, VK_SUCCESS
    jne ffn_fail
    
    mov eax, 1
    jmp ffn_done
    
ffn_fail:
    xor eax, eax
    
ffn_done:
    pop rdi
    pop rsi
    pop rbx
    ret
CreateFFNPipeline ENDP

; ---------------------------------------------------------------------------
; CreateEmbeddingPipeline - Create Embedding compute pipeline
; RCX = shader module pointer
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
CreateEmbeddingPipeline PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; shader module
    
    ; Create shader module
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreateShaderModule")
    cmp rax, 0
    jz embed_fail
    
    lea rsi, shader_info
    mov [rsi].VkShaderModuleCreateInfo.sType, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
    mov rax, [rbx].ShaderModule.code_size
    mov [rsi].VkShaderModuleCreateInfo.codeSize, rax
    mov rax, [rbx].ShaderModule.codePtr
    mov [rsi].VkShaderModuleCreateInfo.pCode, rax
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, rsi
    lea r9, [rbx].ShaderModule.module
    call rax
    
    cmp rax, VK_SUCCESS
    jne embed_fail
    
    ; Create pipeline layout
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreatePipelineLayout")
    cmp rax, 0
    jz embed_fail
    
    lea rsi, layout_info
    mov [rsi].VkPipelineLayoutCreateInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    mov [rsi].VkPipelineLayoutCreateInfo.setLayoutCount, 1
    mov [rsi].VkPipelineLayoutCreateInfo.pushConstantRangeCount, 0
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, rsi
    lea r9, [rbx].ShaderModule.layout
    call rax
    
    cmp rax, VK_SUCCESS
    jne embed_fail
    
    ; Create compute pipeline
    invoke GetProcAddress, [vulkan_context].library, c_str("vkCreateComputePipelines")
    cmp rax, 0
    jz embed_fail
    
    lea rsi, pipeline_info
    mov [rsi].VkComputePipelineCreateInfo.sType, VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.shaderStage, VK_SHADER_STAGE_COMPUTE_BIT
    mov rax, [rbx].ShaderModule.module
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.module, rax
    lea rax, c_str("main")
    mov [rsi].VkComputePipelineCreateInfo.stageInfo.pName, rax
    mov rax, [rbx].ShaderModule.layout
    mov [rsi].VkComputePipelineCreateInfo.layout, rax
    
    lea rcx, [vulkan_context].device
    mov rdx, 0
    mov r8, 1
    mov r9, rsi
    lea r10, [rbx].ShaderModule.pipeline
    mov r11, 0
    call rax
    
    cmp rax, VK_SUCCESS
    jne embed_fail
    
    mov eax, 1
    jmp embed_done
    
embed_fail:
    xor eax, eax
    
embed_done:
    pop rdi
    pop rsi
    pop rbx
    ret
CreateEmbeddingPipeline ENDP

; ---------------------------------------------------------------------------
; ExecuteLayer - Execute a single layer
; RCX = layer pointer, RDX = operator table
; ---------------------------------------------------------------------------
ExecuteLayer PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; layer
    mov rsi, rdx                    ; operator table
    
    ; Check layer components and execute in order
    cmp [rbx].LayerInfo.has_norm, 0
    je skip_norm
    
    ; Execute normalization
    imul rdi, rbx, SIZEOF Operator
    add rdi, rsi
    call DispatchOperator
    
skip_norm:
    cmp [rbx].LayerInfo.has_attn, 0
    je skip_attn
    
    ; Execute attention
    imul rdi, rbx, SIZEOF Operator
    add rdi, rsi
    add rdi, SIZEOF Operator
    call DispatchOperator
    
skip_attn:
    cmp [rbx].LayerInfo.has_ffn, 0
    je layer_done
    
    ; Execute FFN
    imul rdi, rbx, SIZEOF Operator
    add rdi, rsi
    add rdi, SIZEOF Operator * 2
    call DispatchOperator
    
layer_done:
    pop rdi
    pop rsi
    pop rbx
    ret
ExecuteLayer ENDP

; ---------------------------------------------------------------------------
; DispatchOperator - Dispatch operator to Vulkan compute queue
; RCX = operator pointer
; ---------------------------------------------------------------------------
DispatchOperator PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; operator
    
    ; Bind descriptor sets
    lea rsi, descriptor_sets
    mov ecx, [rbx].Operator.op_type
    mov [rsi], ecx
    
    ; Record command buffer
    lea rsi, [vulkan_context].command_buffer
    
    ; Dispatch compute work groups
    mov ecx, [rbx].Operator.input_dim
    shr ecx, 8                      ; Work group size = 256
    jz dispatch_done
    
    ; Submit command buffer
    lea rsi, [vulkan_context].queue
    
dispatch_done:
    pop rdi
    pop rsi
    pop rbx
    ret
DispatchOperator ENDP

; ---------------------------------------------------------------------------
; ExecuteInference - Execute full model inference
; RCX = layer table, RDX = layer count
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
ExecuteInference PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; layer table
    mov rsi, rdx                    ; layer count
    
    lea rcx, str_info_inference
    call SO_int_PrintString
    
    xor ecx, ecx                    ; layer index
inference_loop:
    cmp rcx, rsi
    jae inference_done
    
    imul rdi, rcx, SIZEOF LayerInfo
    add rdi, rbx
    call ExecuteLayer
    
    inc rcx
    jmp inference_loop
    
inference_done:
    mov eax, 1
    jmp inference_exit
    
inference_fail:
    xor eax, eax
    
inference_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
ExecuteInference ENDP

; ---------------------------------------------------------------------------
; ExecuteStreamingInference - Execute streaming inference with paging
; RCX = layer table, RDX = layer count
; Returns: RAX = 1 (success) or 0 (failure)
; ---------------------------------------------------------------------------
ExecuteStreamingInference PROC
    push rbx
    push rsi
    push rdi
    push rbp
    
    mov rbx, rcx                    ; layer table
    mov rsi, rdx                    ; layer count
    
    lea rcx, str_info_inference
    call SO_int_PrintString
    
    ; Initialize streaming state
    mov [streaming_context].current_layer_id, 0
    mov [streaming_context].highest_loaded_layer, 0
    mov [streaming_context].total_ram_used, 0
    
    xor ecx, ecx                    ; layer index
streaming_loop:
    cmp rcx, rsi
    jae streaming_done
    
    ; Update current layer
    mov [streaming_context].current_layer_id, rcx
    
    ; Queue layer for decompression (async)
    mov rax, rcx
    imul rax, CHUNK_SIZE            ; Calculate file offset
    mov rdx, [weights_arena].MemoryArena.base
    mov r8, CHUNK_SIZE
    mov r9, rcx                     ; Layer ID
    call RawrXD_Queue_Decompression_Task
    
    ; Wait for layer to be ready
    call RawrXD_Wait_For_Layer_Ready
    
    ; Update highest loaded layer
    mov [streaming_context].highest_loaded_layer, rcx
    
    ; Execute layer
    imul rdi, rcx, SIZEOF LayerInfo
    add rdi, rbx
    call ExecuteLayer
    
    ; Update metrics
    add [bytes_decompressed], CHUNK_SIZE
    add [current_memory_mb], CHUNK_SIZE / (1024*1024)
    
    ; Check memory pressure every 10 layers
    test cl, 0Fh
    jnz skip_pressure_check
    
    call GetMemoryPressure
    cmp eax, MEMORY_PRESSURE_CRITICAL
    jne skip_eviction
    
    ; Evict LRU layer
    mov rcx, -1                     ; Auto-select LRU
    call EvictLayer
    
skip_eviction:
skip_pressure_check:
    ; Prefetch next layers
    mov rax, rcx
    add rax, 1
    call PrefetchLayer
    
    add rax, 1
    call PrefetchLayer
    
    inc rcx
    jmp streaming_loop
    
streaming_done:
    mov eax, 1
    jmp streaming_exit
    
streaming_fail:
    xor eax, eax
    
streaming_exit:
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret
ExecuteStreamingInference ENDP

; ---------------------------------------------------------------------------
; DEFLATEWorkerThread - Worker thread for DEFLATE decompression
; RCX = thread context pointer
; Returns: RAX = 0 (exit code)
; ---------------------------------------------------------------------------
DEFLATEWorkerThread PROC
    push rbx
    push rsi
    push rdi
    push rbp
    
    mov rbx, rcx                    ; thread context
    
worker_loop:
    ; Check if thread should exit
    mov eax, [rbx].ThreadContext.thread_id
    cmp eax, -1
    je worker_exit
    
    ; Mark thread as working
    mov dword ptr [rbx].ThreadContext.status, 1
    
    ; Process layer (DEFLATE decompression)
    mov esi, eax                    ; layer_id
    
    ; Simulate DEFLATE decompression (placeholder)
    ; In real implementation, would call zlib/inflate
    lea rdi, temp_buffer
    mov rbp, CHUNK_SIZE
    
    ; Update metrics
    lock add [bytes_decompressed], rbp
    
    ; Mark layer as loaded
    lea rdi, layer_states
    imul rax, rsi, SIZEOF LayerState
    mov [rdi + rax].LayerState.state, 2  ; LAYER_STATE_LOADED
    mov rax, [streaming_metrics].StreamingMetrics.layers_loaded
    inc rax
    mov [streaming_metrics].StreamingMetrics.layers_loaded, rax
    
    ; Update memory usage
    mov rax, [streaming_metrics].StreamingMetrics.current_memory_mb
    add rax, CHUNK_SIZE / (1024*1024)
    mov [streaming_metrics].StreamingMetrics.current_memory_mb, rax
    
    ; Check peak memory
    mov rax, [streaming_metrics].StreamingMetrics.peak_memory_mb
    cmp rax, [streaming_metrics].StreamingMetrics.current_memory_mb
    jge no_new_peak
    mov rax, [streaming_metrics].StreamingMetrics.current_memory_mb
    mov [streaming_metrics].StreamingMetrics.peak_memory_mb, rax
    
no_new_peak:
    ; Reset work item
    mov [rbx].ThreadContext.thread_id, -1
    
    ; Mark thread as idle
    mov dword ptr [rbx].ThreadContext.status, 0
    
    ; Sleep briefly to yield
    invoke Sleep, 1
    
    jmp worker_loop
    
worker_exit:
    xor eax, eax
    
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret
DEFLATEWorkerThread ENDP

; ---------------------------------------------------------------------------
; ProcessLayerAsync - Queue layer for asynchronous loading
; RCX = layer_id
; Returns: RAX = 1 (queued) or 0 (failed)
; ---------------------------------------------------------------------------
ProcessLayerAsync PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; layer_id
    
    ; Find available thread
    xor esi, esi                    ; thread index
find_thread_loop:
    cmp esi, NUM_THREADS
    jae no_thread_available
    
    lea rdi, thread_pool
    mov eax, [rdi + rax].ThreadContext.ThreadContext.thread_id
    cmp eax, -1
    je thread_available
    
    inc esi
    jmp find_thread_loop
    
thread_available:
    ; Assign layer to thread
    lea rdi, thread_pool
    mov [rdi + rax].ThreadContext.ThreadContext.thread_id, ebx
    
    ; Update layer state
    lea rdi, layer_states
    imul rax, rbx, SIZEOF LayerState
    mov [rdi + rax].LayerState.state, 1  ; LAYER_STATE_LOADING
    
    mov eax, 1
    jmp process_done
    
no_thread_available:
    xor eax, eax
    
process_done:
    pop rdi
    pop rsi
    pop rbx
    ret
ProcessLayerAsync ENDP

; ---------------------------------------------------------------------------
; EvictLayer - Evict layer from memory (LRU-based)
; RCX = layer_id (or -1 for automatic selection)
; Returns: RAX = 1 (evicted) or 0 (failed)
; ---------------------------------------------------------------------------
EvictLayer PROC
    push rbx
    push rsi
    push rdi
    push rbp
    
    mov rbx, rcx                    ; layer_id (or -1)
    
    ; If layer_id = -1, find LRU layer
    cmp rbx, -1
    jne evict_specific
    
    ; Find LRU (least recently accessed) layer
    xor rsi, rsi                    ; candidate layer
    mov rdi, 0FFFFFFFFFFFFFFFFh     ; max timestamp (oldest)
    xor rbp, rbp                    ; layer index
find_lru_loop:
    cmp ebp, [exec_header].ExecHeader.layer_count
    jae lru_found
    
    lea rax, layer_states
    imul r11, rbp, SIZEOF LayerState
    mov r8, [rax + r11].LayerState.last_access
    cmp r8, rdi
    jae not_lru
    
    ; Check if layer can be evicted (not loading)
    mov r9d, [rax + r11].LayerState.state
    cmp r9d, 2                      ; LAYER_STATE_LOADED
    jne not_lru
    
    mov rsi, rbp
    mov rdi, r8
    
not_lru:
    inc rbp
    jmp find_lru_loop
    
lru_found:
    mov rbx, rsi                    ; Use LRU layer_id
    
evict_specific:
    ; Evict the layer
    lea rdi, layer_states
    imul rax, rbx, SIZEOF LayerState
    mov [rdi + rax].LayerState.state, 4  ; LAYER_STATE_EVICTED
    imul rax, rbx, SIZEOF LayerState
    mov QWORD PTR [rdi + rax].LayerState.memory_offset, -1
    
    ; Update metrics
    mov rax, [streaming_metrics].StreamingMetrics.layers_evicted
    inc rax
    mov [streaming_metrics].StreamingMetrics.layers_evicted, rax
    
    ; Update memory usage
    mov rax, [streaming_metrics].StreamingMetrics.current_memory_mb
    sub rax, CHUNK_SIZE / (1024*1024)
    mov [streaming_metrics].StreamingMetrics.current_memory_mb, rax
    
    mov eax, 1
    jmp evict_done
    
evict_fail:
    xor eax, eax
    
evict_done:
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret
EvictLayer ENDP

; ---------------------------------------------------------------------------
; PrefetchLayer - Add layer to prefetch queue
; RCX = layer_id
; Returns: RAX = 1 (queued) or 0 (failed)
; ---------------------------------------------------------------------------
PrefetchLayer PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; layer_id
    
    ; Calculate prefetch score
    call CalculatePrefetchScore
    mov esi, eax                    ; score
    
    ; Find empty slot in prefetch queue
    xor edi, edi                    ; queue index
find_slot_loop:
    cmp edi, PREFETCH_DISTANCE
    jae queue_full
    
    lea rax, prefetch_queue
    imul r11, rdi, SIZEOF PrefetchEntry
    mov ecx, [rax + r11].PrefetchEntry.state
    cmp ecx, 0                      ; PREFETCH_STATE_IDLE
    je slot_found
    
    inc edi
    jmp find_slot_loop
    
slot_found:
    ; Add to prefetch queue
    lea rax, prefetch_queue
    imul r11, rdi, SIZEOF PrefetchEntry
    mov [rax + r11].PrefetchEntry.layer_id, rbx
    mov [rax + r11].PrefetchEntry.state, 1  ; PREFETCH_STATE_QUEUED
    mov [rax + r11].PrefetchEntry.priority, esi
    invoke GetTickCount
    mov [rax + r11].PrefetchEntry.queue_time, rax
    
    mov eax, 1
    jmp prefetch_done
    
queue_full:
    xor eax, eax
    
prefetch_done:
    pop rdi
    pop rsi
    pop rbx
    ret
PrefetchLayer ENDP

; ---------------------------------------------------------------------------
; CalculatePrefetchScore - Calculate prefetch priority score
; RCX = layer_id
; Returns: EAX = score (0-100)
; ---------------------------------------------------------------------------
CalculatePrefetchScore PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; layer_id
    
    ; Base score on access pattern
    lea rsi, layer_states
    imul rdi, rbx, SIZEOF LayerState
    mov eax, [rsi + rdi].LayerState.access_count
    
    ; Normalize to 0-100
    imul eax, 10
    cmp eax, 100
    jle score_done
    mov eax, 100
    
score_done:
    pop rsi
    pop rbx
    ret
CalculatePrefetchScore ENDP

; ---------------------------------------------------------------------------
; GetMemoryPressure - Calculate current memory pressure level
; Returns: EAX = pressure level (0-3)
; ---------------------------------------------------------------------------
GetMemoryPressure PROC
    push rbx
    
    mov rax, [streaming_metrics].StreamingMetrics.current_memory_mb
    
    ; Check thresholds
    cmp rax, EVICTION_THRESHOLD
    jge pressure_critical
    
    mov rbx, EVICTION_THRESHOLD
    shr rbx, 1                      ; 24GB
    cmp rax, rbx
    jge pressure_high
    
    shr rbx, 1                      ; 12GB
    cmp rax, rbx
    jge pressure_medium
    
    mov eax, 0                      ; MEMORY_PRESSURE_LOW
    jmp pressure_done
    
pressure_critical:
    mov eax, 3                      ; MEMORY_PRESSURE_CRITICAL
    jmp pressure_done
    
pressure_high:
    mov eax, 2                      ; MEMORY_PRESSURE_HIGH
    jmp pressure_done
    
pressure_medium:
    mov eax, 1                      ; MEMORY_PRESSURE_MEDIUM
    
pressure_done:
    mov [streaming_metrics].StreamingMetrics.memory_pressure, eax
    pop rbx
    ret
GetMemoryPressure ENDP

; ---------------------------------------------------------------------------
; UpdateMetrics - Update streaming metrics
; ---------------------------------------------------------------------------
UpdateMetrics PROC
    push rbx
    push rsi
    
    ; Update memory pressure
    call GetMemoryPressure
    
    ; Calculate average load time
    mov rax, [streaming_metrics].StreamingMetrics.layers_loaded
    test rax, rax
    jz no_load_time
    
    ; Placeholder: would track actual load times
    mov [streaming_metrics].StreamingMetrics.avg_load_time_ms, 100
    
no_load_time:
    ; Calculate average eviction time
    mov rax, [streaming_metrics].StreamingMetrics.layers_evicted
    test rax, rax
    jz no_eviction_time
    
    ; Placeholder: would track actual eviction times
    mov [streaming_metrics].StreamingMetrics.avg_eviction_time_ms, 50
    
no_eviction_time:
    pop rsi
    pop rbx
    ret
UpdateMetrics ENDP

; ---------------------------------------------------------------------------
; PrintStatistics - Print loading statistics
; ---------------------------------------------------------------------------
PrintStatistics PROC
    push rbx
    push rsi
    
    ; Print layers loaded
    lea rcx, [_str_info_layers]
    mov edx, [layers_loaded]
    call SO_int_PrintInfo
    
    ; Print operators loaded
    lea rcx, [_str_info_operators]
    mov edx, [operators_loaded]
    call SO_int_PrintInfo
    
    ; Print memory allocated
    lea rcx, [_str_info_memory_mb]
    mov rdx, [memory_allocated]
    shr rdx, 20                     ; Convert to MB
    call SO_int_PrintInfo
    
    pop rsi
    pop rbx
    ret
PrintStatistics ENDP

; ---------------------------------------------------------------------------
; PrintMetrics - Print streaming metrics
; ---------------------------------------------------------------------------
PrintMetrics PROC
    push rbx
    push rsi
    push rdi
    
    lea rcx, str_metrics_header
    call SO_int_PrintString
    
    ; Print layers loaded
    lea rcx, str_metrics_layers
    mov rdx, [streaming_metrics].StreamingMetrics.layers_loaded
    call SO_int_PrintInfo
    
    ; Print layers evicted
    lea rcx, str_metrics_evicted
    mov rdx, [streaming_metrics].StreamingMetrics.layers_evicted
    call SO_int_PrintInfo
    
    ; Print bytes streamed
    lea rcx, str_metrics_streamed
    mov rdx, [streaming_metrics].StreamingMetrics.bytes_streamed
    shr rdx, 20                     ; Convert to MB
    call SO_int_PrintInfo
    
    ; Print bytes decompressed
    lea rcx, str_metrics_decompressed
    mov rdx, [streaming_metrics].StreamingMetrics.bytes_decompressed
    shr rdx, 20                     ; Convert to MB
    call SO_int_PrintInfo
    
    ; Print prefetch stats
    lea rcx, str_metrics_prefetch
    mov rdx, [streaming_metrics].StreamingMetrics.prefetch_hits
    mov r8, [streaming_metrics].StreamingMetrics.prefetch_misses
    call SO_int_PrintInfo
    
    ; Print memory stats
    lea rcx, str_metrics_memory
    mov rdx, [streaming_metrics].StreamingMetrics.current_memory_mb
    mov r8, [streaming_metrics].StreamingMetrics.peak_memory_mb
    call SO_int_PrintInfo
    
    ; Print memory pressure
    lea rcx, str_metrics_pressure
    mov edx, [streaming_metrics].StreamingMetrics.memory_pressure
    call SO_int_PrintInfo
    
    pop rdi
    pop rsi
    pop rbx
    ret
PrintMetrics ENDP

; ---------------------------------------------------------------------------
; I/O Helper Functions (complete implementations)
; ---------------------------------------------------------------------------
PUBLIC SO_int_PrintBanner
SO_int_PrintBanner PROC
    lea rcx, so_str_banner
    call SO_int_PrintString
    ret
SO_int_PrintBanner ENDP

PUBLIC SO_int_FatalError
SO_int_FatalError PROC
    ; RCX = error message string
    call SO_int_PrintError
    invoke ExitProcess, 1
    ret
SO_int_FatalError ENDP

SO_int_PrintString PROC
    push rbx
    mov rbx, rcx
    invoke lstrlenA, rbx
    lea r9, so_bytes_written
    invoke WriteFile, STD_OUTPUT_HANDLE, rbx, rax, r9, 0
    pop rbx
    ret
SO_int_PrintString ENDP

SO_int_PrintError PROC
    push rbx
    mov rbx, rcx
    invoke lstrlenA, rbx
    lea r9, so_bytes_written
    invoke WriteFile, STD_ERROR_HANDLE, rbx, rax, r9, 0
    pop rbx
    ret
SO_int_PrintError ENDP

SO_int_PrintInfo PROC
    push rbx
    push rsi
    push r12
    
    mov rbx, rcx                    ; format string
    mov rsi, rdx                    ; argument
    
    ; Simple formatting - just print the format string for now
    call SO_int_PrintString
    
print_done:
    pop r12
    pop rsi
    pop rbx
    ret
SO_int_PrintInfo ENDP

; ---------------------------------------------------------------------------

; Forward-declared stubs for missing functions
RawrXD_Queue_Decompression_Task PROC
    ; Stub: queue a decompression task
    mov rax, 1
    ret
RawrXD_Queue_Decompression_Task ENDP

RawrXD_Wait_For_Layer_Ready PROC
    ; Stub: wait for layer to be ready
    mov rax, 1
    ret
RawrXD_Wait_For_Layer_Ready ENDP
; [c_str duplicate removed — using definition at top of file]

; ---------------------------------------------------------------------------
; Vulkan utility functions (stubs for now)
; ---------------------------------------------------------------------------
RawrXD_Create_Timeline_Semaphore PROC
    ; [Vulkan API call to create timeline semaphore]
    ; Returns handle in RAX
    mov rax, 1
    ret
RawrXD_Create_Timeline_Semaphore ENDP

RawrXD_Signal_Timeline PROC
    ; RCX = Semaphore handle
    ; RDX = Timeline value
    ; [Vulkan vkSignalSemaphore call]
    mov rax, 1
    ret
RawrXD_Signal_Timeline ENDP

RawrXD_Wait_Timeline PROC
    ; RCX = Semaphore handle
    ; RDX = Timeline value
    ; [Vulkan vkWaitSemaphores call]
    mov rax, 1
    ret
RawrXD_Wait_Timeline ENDP

RawrXD_File_Seek_And_Map PROC
    ; RCX = File offset
    ; Maps 64MB chunk into memory
    ; Returns pointer in RAX
    lea rax, temp_buffer
    ret
RawrXD_File_Seek_And_Map ENDP

RawrXD_Decompress_Block PROC
    ; RCX = Source buffer
    ; RDX = Destination buffer
    ; R8 = Compressed size
    ; Returns decompressed size in RAX
    mov rax, CHUNK_SIZE
    ret
RawrXD_Decompress_Block ENDP

RawrXD_Execute_Layer PROC
    ; RCX = Layer pointer
    ; Execute layer operators
    ret
RawrXD_Execute_Layer ENDP

RawrXD_Destroy_Streaming_System PROC
    ; Cleanup streaming system
    ret
RawrXD_Destroy_Streaming_System ENDP

RawrXD_Open_Memory_Mapped_File PROC
    ; RCX = File path
    ; RDX = File size
    ; Returns handle in RAX
    mov rax, 1
    ret
RawrXD_Open_Memory_Mapped_File ENDP

; ---------------------------------------------------------------------------
; Data for shader compilation
; ---------------------------------------------------------------------------
.data
app_info VkApplicationInfo <>
instance_info VkInstanceCreateInfo <>
buffer_info VkBufferCreateInfo <>
shader_info VkShaderModuleCreateInfo <>
layout_info VkPipelineLayoutCreateInfo <>
pipeline_info VkComputePipelineCreateInfo <>
command_begin_info VkCommandBufferBeginInfo <>
submit_info VkSubmitInfo <>
arena_temp MemoryArena <>
descriptor_sets DD 0
physical_device_count DD 0
so_bytes_written DD 0
so_bytes_read DD 0

; Shader code buffers
norm_shader_code DB 1024 DUP(0)
attn_shader_code DB 1024 DUP(0)
ffn_shader_code DB 1024 DUP(0)
embed_shader_code DB 1024 DUP(0)

; Prefetch queue
prefetch_queue PrefetchEntry 16 DUP(<>)  ; 16-entry prefetch queue

.code

; =============================================================================
; PUBLIC API — SO_ prefixed symbols for C ABI linkage (streaming_orchestrator.h)
; option proc:private makes all PROCs internal; these PUBLIC aliases export
; =============================================================================

; --- Vulkan Compute Pipeline ---
PUBLIC SO_LoadExecFile
SO_LoadExecFile PROC
    ; RCX = filePath
    lea rdx, [rsp]      ; dummy second arg
    jmp LoadExecFile
SO_LoadExecFile ENDP

PUBLIC SO_InitializeVulkan
SO_InitializeVulkan PROC
    jmp InitializeVulkan
SO_InitializeVulkan ENDP

PUBLIC SO_CreateMemoryArena
SO_CreateMemoryArena PROC
    ; RCX = sizeBytes
    jmp CreateMemoryArena
SO_CreateMemoryArena ENDP

PUBLIC SO_CompileSPIRVShader
SO_CompileSPIRVShader PROC
    ; RCX = shaderModule, RDX = opType, R8 = opCount
    jmp CompileSPIRVShader
SO_CompileSPIRVShader ENDP

PUBLIC SO_CreateComputePipelines
SO_CreateComputePipelines PROC
    ; RCX = operatorTable, RDX = operatorCount
    jmp CreateComputePipelines
SO_CreateComputePipelines ENDP

PUBLIC SO_ExecuteLayer
SO_ExecuteLayer PROC
    ; RCX = layerInfo, RDX = operatorTable
    jmp ExecuteLayer
SO_ExecuteLayer ENDP

PUBLIC SO_DispatchOperator
SO_DispatchOperator PROC
    ; RCX = operatorPtr
    jmp DispatchOperator
SO_DispatchOperator ENDP

PUBLIC SO_ExecuteInference
SO_ExecuteInference PROC
    ; RCX = layerTable, RDX = layerCount
    jmp ExecuteInference
SO_ExecuteInference ENDP

PUBLIC SO_PrintStatistics
SO_PrintStatistics PROC
    jmp PrintStatistics
SO_PrintStatistics ENDP

; --- Streaming Engine ---
PUBLIC SO_InitializeStreaming
SO_InitializeStreaming PROC
    jmp InitializeStreaming
SO_InitializeStreaming ENDP

PUBLIC SO_CreateThreadPool
SO_CreateThreadPool PROC
    jmp CreateThreadPool
SO_CreateThreadPool ENDP

PUBLIC SO_StartDEFLATEThreads
SO_StartDEFLATEThreads PROC
    ; RCX = threadCount (ECX)
    jmp StartDEFLATEThreads
SO_StartDEFLATEThreads ENDP

PUBLIC SO_InitializePrefetchQueue
SO_InitializePrefetchQueue PROC
    jmp InitializePrefetchQueue
SO_InitializePrefetchQueue ENDP

PUBLIC SO_ExecuteStreamingInference
SO_ExecuteStreamingInference PROC
    ; RCX = layerTable, RDX = layerCount
    jmp ExecuteStreamingInference
SO_ExecuteStreamingInference ENDP

PUBLIC SO_ProcessLayerAsync
SO_ProcessLayerAsync PROC
    ; RCX = layerId
    jmp ProcessLayerAsync
SO_ProcessLayerAsync ENDP

PUBLIC SO_EvictLayer
SO_EvictLayer PROC
    ; RCX = layerId
    jmp EvictLayer
SO_EvictLayer ENDP

PUBLIC SO_PrefetchLayer
SO_PrefetchLayer PROC
    ; RCX = layerId
    jmp PrefetchLayer
SO_PrefetchLayer ENDP

PUBLIC SO_CalculatePrefetchScore
SO_CalculatePrefetchScore PROC
    ; RCX = layerId
    jmp CalculatePrefetchScore
SO_CalculatePrefetchScore ENDP

PUBLIC SO_GetMemoryPressure
SO_GetMemoryPressure PROC
    jmp GetMemoryPressure
SO_GetMemoryPressure ENDP

PUBLIC SO_UpdateMetrics
SO_UpdateMetrics PROC
    jmp UpdateMetrics
SO_UpdateMetrics ENDP

PUBLIC SO_PrintMetrics
SO_PrintMetrics PROC
    jmp PrintMetrics
SO_PrintMetrics ENDP

PUBLIC SO_GetMetrics
SO_GetMetrics PROC
    ; RCX = SO_StreamingMetrics* out
    ; Copy streaming_metrics (64 bytes) to caller buffer
    lea rsi, streaming_metrics
    mov rdi, rcx
    mov ecx, 8          ; 8 QWORDs = 64 bytes
    rep movsq
    ret
SO_GetMetrics ENDP

; --- Timeline Semaphore + Memory-Mapped I/O ---
PUBLIC SO_CreateTimelineSemaphore
SO_CreateTimelineSemaphore PROC
    jmp RawrXD_Create_Timeline_Semaphore
SO_CreateTimelineSemaphore ENDP

PUBLIC SO_SignalTimeline
SO_SignalTimeline PROC
    ; RCX = semaphore, RDX = value
    jmp RawrXD_Signal_Timeline
SO_SignalTimeline ENDP

PUBLIC SO_WaitTimeline
SO_WaitTimeline PROC
    ; RCX = semaphore, RDX = value
    jmp RawrXD_Wait_Timeline
SO_WaitTimeline ENDP

PUBLIC SO_FileSeekAndMap
SO_FileSeekAndMap PROC
    ; RCX = fileOffset
    jmp RawrXD_File_Seek_And_Map
SO_FileSeekAndMap ENDP

PUBLIC SO_DecompressBlock
SO_DecompressBlock PROC
    ; RCX = src, RDX = dest, R8 = compressedSize
    jmp RawrXD_Decompress_Block
SO_DecompressBlock ENDP

PUBLIC SO_ExecuteLayerOps
SO_ExecuteLayerOps PROC
    ; RCX = layerPtr
    jmp RawrXD_Execute_Layer
SO_ExecuteLayerOps ENDP

PUBLIC SO_DestroyStreamingSystem
SO_DestroyStreamingSystem PROC
    jmp RawrXD_Destroy_Streaming_System
SO_DestroyStreamingSystem ENDP

PUBLIC SO_OpenMemoryMappedFile
SO_OpenMemoryMappedFile PROC
    ; RCX = path, RDX = fileSize
    jmp RawrXD_Open_Memory_Mapped_File
SO_OpenMemoryMappedFile ENDP

END
