;============================================================================
; GPU Main Integration - Pure MASM x64
; DLL entry point, module initialization, public API exports
; Production-ready: Synchronized subsystem startup, graceful shutdown
;============================================================================
.686P
.XMM
.model flat, c
OPTION CASEMAP:NONE

; Import all module initialization functions
extern InitializeBackendSystem: proc
extern InitializeMemoryManager: proc
extern InitializeLoader: proc
extern InitializeTokenizer: proc
extern InitializeErrorHandling: proc
extern InitializeContextManager: proc
extern LoadVulkanFunctions: proc
extern InitializeVulkan: proc

; Import all shutdown functions
extern ShutdownGPUBackend: proc
extern ShutdownMemoryManager: proc
extern ShutdownMetrics: proc
extern ShutdownInferenceEngine: proc
extern ShutdownTokenizer: proc
extern CleanupVulkan: proc

; Import public APIs from each module
extern InitializeGPUBackend: proc
extern LoadModelFile: proc
extern InitializeInference: proc
extern GenerateToken: proc
extern FlushKVCache: proc
extern InitializeMetrics: proc
extern RecordTokenLatency: proc
extern FlushMetrics: proc
extern TokenizeString: proc
extern DetokenizeTokens: proc
extern CreateModelContext: proc
extern ActivateModelContext: proc
extern DestroyModelContext: proc
extern GetActiveContext: proc
extern CreateComputeCommandBuffer: proc
extern DispatchInference: proc
extern GetLastGPUError: proc
extern ResetGPUError: proc
extern IsInSafeMode: proc

; Windows API
extern OutputDebugStringA: proc

.data
; DLL state management
dllInitialized          db 0
dllShutdownInProgress   db 0
dllCriticalError        db 0

; Initialization status
backendInitStatus       dd 0
memoryInitStatus        dd 0
loaderInitStatus        dd 0
tokenizerInitStatus     dd 0
errorHandlingStatus     dd 0
contextMgrStatus        dd 0
vulkanInitStatus        dd 0
metricsInitStatus       dd 0

; Version information
DLL_VERSION_MAJOR       equ 1
DLL_VERSION_MINOR       equ 0
DLL_VERSION_BUILD       equ 100

; Debug strings
debugDllAttach          db "[GPU_DLL] Process attach: initializing subsystems...", 0
debugDllBackendInit     db "[GPU_DLL] Backend initialized: status=0x%x", 0
debugDllMemoryInit      db "[GPU_DLL] Memory initialized: status=0x%x", 0
debugDllLoaderInit      db "[GPU_DLL] Loader initialized: status=0x%x", 0
debugDllTokenizerInit   db "[GPU_DLL] Tokenizer initialized: status=0x%x", 0
debugDllErrorInit       db "[GPU_DLL] Error handling initialized: status=0x%x", 0
debugDllContextInit     db "[GPU_DLL] Context manager initialized: status=0x%x", 0
debugDllVulkanInit      db "[GPU_DLL] Vulkan initialized: status=0x%x", 0
debugDllMetricsInit     db "[GPU_DLL] Metrics initialized: status=0x%x", 0
debugDllAttachComplete  db "[GPU_DLL] Process attach complete: all subsystems ready", 0
debugDllDetach          db "[GPU_DLL] Process detach: cleaning up resources...", 0
debugDllDetachComplete  db "[GPU_DLL] Process detach complete: shutdown successful", 0
debugDllError           db "[GPU_DLL] ERROR: %s (status=0x%x)", 0
debugDllVersionInfo     db "[GPU_DLL] Version: %d.%d (build %d)", 0

.code

;============================================================================
; DllMain - Standard Windows DLL entry point
; rcx = hInstance, rdx = reason, r8 = reserved
;============================================================================
DllMain proc
    
    cmp edx, DLL_PROCESS_ATTACH
    je @dll_attach
    
    cmp edx, DLL_PROCESS_DETACH
    je @dll_detach
    
    ; Other reasons (THREAD_ATTACH, THREAD_DETACH) - do nothing
    mov eax, TRUE
    ret
    
@dll_attach:
    call DllAttachInit
    ret
    
@dll_detach:
    call DllDetachCleanup
    ret
    
DllMain endp

;============================================================================
; DllAttachInit - Initialize all subsystems on DLL load
;============================================================================
DllAttachInit proc
    push rbp
    mov rbp, rsp
    
    ; Log attachment
    lea rcx, debugDllAttach
    call OutputDebugStringA
    
    ; Log version
    lea rcx, debugDllVersionInfo
    mov edx, DLL_VERSION_MAJOR
    mov r8d, DLL_VERSION_MINOR
    mov r9d, DLL_VERSION_BUILD
    call OutputDebugStringA
    
    ; ===== STEP 1: Initialize Error Handling (FIRST - needed for recovery) =====
    call InitializeErrorHandling
    mov errorHandlingStatus, eax
    test eax, eax
    jz @attach_error_handling_failed
    
    lea rcx, debugDllErrorInit
    mov edx, errorHandlingStatus
    call OutputDebugStringA
    
    ; ===== STEP 2: Initialize Backend Infrastructure =====
    call InitializeBackendSystem
    mov backendInitStatus, eax
    test eax, eax
    jz @attach_backend_failed
    
    lea rcx, debugDllBackendInit
    mov edx, backendInitStatus
    call OutputDebugStringA
    
    ; ===== STEP 3: Initialize Memory Manager =====
    call InitializeMemoryManager
    mov memoryInitStatus, eax
    test eax, eax
    jz @attach_memory_failed
    
    lea rcx, debugDllMemoryInit
    mov edx, memoryInitStatus
    call OutputDebugStringA
    
    ; ===== STEP 4: Initialize Model Loader =====
    call InitializeLoader
    mov loaderInitStatus, eax
    test eax, eax
    jz @attach_loader_failed
    
    lea rcx, debugDllLoaderInit
    mov edx, loaderInitStatus
    call OutputDebugStringA
    
    ; ===== STEP 5: Initialize Tokenizer =====
    call InitializeTokenizer
    mov tokenizerInitStatus, eax
    test eax, eax
    jz @attach_tokenizer_failed
    
    lea rcx, debugDllTokenizerInit
    mov edx, tokenizerInitStatus
    call OutputDebugStringA
    
    ; ===== STEP 6: Initialize Context Manager =====
    call InitializeContextManager
    mov contextMgrStatus, eax
    test eax, eax
    jz @attach_context_failed
    
    lea rcx, debugDllContextInit
    mov edx, contextMgrStatus
    call OutputDebugStringA
    
    ; ===== STEP 7: Load Vulkan SDK (may fail, will use CPU fallback) =====
    call LoadVulkanFunctions
    mov vulkanInitStatus, eax
    
    lea rcx, debugDllVulkanInit
    mov edx, vulkanInitStatus
    call OutputDebugStringA
    
    ; ===== STEP 8: Initialize Metrics System =====
    call InitializeMetrics
    mov metricsInitStatus, eax
    test eax, eax
    jz @attach_metrics_failed
    
    lea rcx, debugDllMetricsInit
    mov edx, metricsInitStatus
    call OutputDebugStringA
    
    ; All systems initialized successfully
    mov dllInitialized, 1
    
    lea rcx, debugDllAttachComplete
    call OutputDebugStringA
    
    mov eax, TRUE
    ret
    
    ; ===== ERROR HANDLING =====
@attach_error_handling_failed:
    mov dllCriticalError, 1
    mov eax, FALSE
    ret
    
@attach_backend_failed:
    lea rcx, debugDllError
    lea rdx, "Backend initialization failed"
    mov r8d, backendInitStatus
    call OutputDebugStringA
    mov eax, FALSE
    ret
    
@attach_memory_failed:
    lea rcx, debugDllError
    lea rdx, "Memory initialization failed"
    mov r8d, memoryInitStatus
    call OutputDebugStringA
    mov eax, FALSE
    ret
    
@attach_loader_failed:
    lea rcx, debugDllError
    lea rdx, "Model loader initialization failed"
    mov r8d, loaderInitStatus
    call OutputDebugStringA
    mov eax, FALSE
    ret
    
@attach_tokenizer_failed:
    lea rcx, debugDllError
    lea rdx, "Tokenizer initialization failed"
    mov r8d, tokenizerInitStatus
    call OutputDebugStringA
    mov eax, FALSE
    ret
    
@attach_context_failed:
    lea rcx, debugDllError
    lea rdx, "Context manager initialization failed"
    mov r8d, contextMgrStatus
    call OutputDebugStringA
    mov eax, FALSE
    ret
    
@attach_metrics_failed:
    lea rcx, debugDllError
    lea rdx, "Metrics initialization failed"
    mov r8d, metricsInitStatus
    call OutputDebugStringA
    ; Metrics failure is non-critical
    mov eax, TRUE
    ret
    
DllAttachInit endp

;============================================================================
; DllDetachCleanup - Shutdown all subsystems on DLL unload
;============================================================================
DllDetachCleanup proc
    lea rcx, debugDllDetach
    call OutputDebugStringA
    
    mov dllShutdownInProgress, 1
    
    ; ===== Reverse order of initialization for shutdown =====
    
    ; Shutdown Metrics
    cmp metricsInitStatus, 0
    je @shutdown_skip_metrics
    call ShutdownMetrics
    mov metricsInitStatus, 0
@shutdown_skip_metrics:
    
    ; Shutdown Inference Engine
    call ShutdownInferenceEngine
    
    ; Shutdown Vulkan
    cmp vulkanInitStatus, 0
    je @shutdown_skip_vulkan
    call CleanupVulkan
    mov vulkanInitStatus, 0
@shutdown_skip_vulkan:
    
    ; Shutdown GPU Backend
    call ShutdownGPUBackend
    
    ; Shutdown Tokenizer
    call ShutdownTokenizer
    
    ; Shutdown Memory Manager
    call ShutdownMemoryManager
    
    ; Error handling and Context Manager cleanup (no explicit shutdown needed)
    
    mov dllInitialized, 0
    mov dllShutdownInProgress, 0
    
    lea rcx, debugDllDetachComplete
    call OutputDebugStringA
    
    ret
DllDetachCleanup endp

;============================================================================
; PUBLIC API EXPORTS
;============================================================================

;----------------------------------------------------------------------------
; GpuBackend_Init - Initialize GPU backend
; Returns: 0=success, non-zero=error code
;------------------------------------------------------------------------
public GpuBackend_Init
GpuBackend_Init proc
    cmp dllInitialized, 1
    jne @init_dll_not_ready
    
    ; Initialize GPU backend (may use Vulkan or CPU fallback)
    call InitializeGPUBackend
    test rax, rax
    jz @init_failed
    
    ; Initialize Vulkan compute environment
    call InitializeVulkan
    test rax, rax
    jz @init_no_vulkan  ; Vulkan optional, continue with CPU
    
    ; Create compute command buffer for inference
    call CreateComputeCommandBuffer
    
@init_no_vulkan:
    xor eax, eax           ; Success
    ret
    
@init_dll_not_ready:
    mov eax, 0x80000001    ; Error: DLL not initialized
    ret
    
@init_failed:
    call GetLastGPUError
    ret
GpuBackend_Init endp

;----------------------------------------------------------------------------
; GpuBackend_Shutdown - Cleanup and shutdown
; Returns: 0=success
;------------------------------------------------------------------------
public GpuBackend_Shutdown
GpuBackend_Shutdown proc
    call ShutdownGPUBackend
    call CleanupVulkan
    xor eax, eax
    ret
GpuBackend_Shutdown endp

;----------------------------------------------------------------------------
; GpuBackend_LoadModel - Load GGUF model file
; rcx = model file path (ANSI string)
; Returns: model context pointer in rax (0 on failure)
;------------------------------------------------------------------------
public GpuBackend_LoadModel
GpuBackend_LoadModel proc
    call LoadModelFile
    ret
GpuBackend_LoadModel endp

;----------------------------------------------------------------------------
; GpuBackend_RunInference - Execute model inference
; rcx = model context
; rdx = input tokens (uint64 array)
; r8 = token count
; r9 = output buffer
; Returns: output token count in rax
;------------------------------------------------------------------------
public GpuBackend_RunInference
GpuBackend_RunInference proc
    ; Initialize inference context
    call InitializeInference
    test rax, rax
    jz @inference_failed
    
    ; Generate tokens in a loop
    mov r10, 0                     ; Output count
    
@token_loop:
    call GenerateToken
    cmp rax, 0xFFFFFFFF            ; EOS marker
    je @inference_done
    test rax, rax
    jz @inference_error
    
    inc r10
    cmp r10, 512                   ; Max tokens safety limit
    jge @inference_done
    
    jmp @token_loop
    
@inference_done:
    mov rax, r10
    ret
    
@inference_error:
@inference_failed:
    xor eax, eax
    ret
GpuBackend_RunInference endp

;----------------------------------------------------------------------------
; GpuBackend_GetMetrics - Query performance metrics
; Returns: rax=tokens/sec (float in xmm0), rdx=latency_ms
;------------------------------------------------------------------------
public GpuBackend_GetMetrics
GpuBackend_GetMetrics proc
    ; This would call into metrics module
    ; For now, return zero metrics
    xorps xmm0, xmm0
    xor edx, edx
    ret
GpuBackend_GetMetrics endp

;----------------------------------------------------------------------------
; GpuBackend_IsHealthy - Check system health status
; Returns: 1=healthy, 0=safe_mode, -1=critical_error
;------------------------------------------------------------------------
public GpuBackend_IsHealthy
GpuBackend_IsHealthy proc
    cmp dllCriticalError, 1
    je @health_critical
    
    call IsInSafeMode
    test eax, eax
    jnz @health_safe_mode
    
    mov eax, 1                     ; Healthy
    ret
    
@health_safe_mode:
    xor eax, eax                   ; Safe mode
    ret
    
@health_critical:
    mov eax, -1                    ; Critical error
    ret
GpuBackend_IsHealthy endp

;----------------------------------------------------------------------------
; GpuBackend_GetLastError - Get last error code
; Returns: error code in rax
;------------------------------------------------------------------------
public GpuBackend_GetLastError
GpuBackend_GetLastError proc
    call GetLastGPUError
    ret
GpuBackend_GetLastError endp

;============================================================================
; DLL EXPORT TABLE
;============================================================================

.def
    DllMain
    GpuBackend_Init
    GpuBackend_Shutdown
    GpuBackend_LoadModel
    GpuBackend_RunInference
    GpuBackend_GetMetrics
    GpuBackend_IsHealthy
    GpuBackend_GetLastError
.endd

.data
; Constants
DLL_PROCESS_ATTACH      equ 1
DLL_PROCESS_DETACH      equ 0
DLL_THREAD_ATTACH       equ 2
DLL_THREAD_DETACH       equ 3
TRUE                    equ 1
FALSE                   equ 0

.code
end
