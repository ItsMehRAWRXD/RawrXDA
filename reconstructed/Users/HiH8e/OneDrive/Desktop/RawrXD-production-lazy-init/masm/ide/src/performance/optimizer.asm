; ============================================================================
; RawrXD Agentic IDE - Performance Optimizer (Pure MASM)
; Main performance optimization controller for Phase 6
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\gdi32.inc
include \masm32\include\psapi.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\gdi32.lib
includelib \masm32\lib\psapi.lib

; ============================================================================
; CONSTANTS
; ============================================================================

; Optimization types
OPT_TYPE_MEMORY         EQU 1
OPT_TYPE_STARTUP        EQU 2
OPT_TYPE_FILE_ENUM      EQU 3
OPT_TYPE_TREEVIEW       EQU 4
OPT_TYPE_RESOURCE       EQU 5

; Metric types (kept in sync with performance_monitor.asm)
METRIC_MEMORY_USAGE     EQU 1
METRIC_CPU_USAGE        EQU 2
METRIC_STARTUP_TIME     EQU 3
METRIC_FILE_ENUM_TIME   EQU 4
METRIC_TREEVIEW_RENDER  EQU 5

; Configuration flags
FLAG_LAZY_INIT          EQU 0001h
FLAG_ASYNC_FILE_OPS     EQU 0002h
FLAG_VIRTUAL_RENDERING  EQU 0004h
FLAG_AUTO_CLEANUP       EQU 0008h

; ============================================================================
; DATA STRUCTURES
; ============================================================================

OPTIMIZATION_CONFIG STRUCT
    flags               DWORD ?
    memoryLimitMB       DWORD ?
    cpuLimitPercent     DWORD ?
    lazyInitEnabled     DWORD ?     ; Lazy initialization flag
    asyncFileOpsEnabled DWORD ?     ; Async file operations flag
    virtualRendering    DWORD ?     ; Virtual rendering flag
    autoCleanupEnabled  DWORD ?     ; Automatic cleanup flag
OPTIMIZATION_CONFIG ENDS

PERFORMANCE_OPTIMIZER STRUCT
    config              OPTIMIZATION_CONFIG <>
    isInitialized       DWORD ?
    hPerfMonitor       DWORD ?     ; Handle to performance monitor
    hMemoryPool        DWORD ?     ; Handle to memory pool
    hFileEnum          DWORD ?     ; Handle to file enumeration
    optimizationFlags  DWORD ?     ; Active optimizations
    startupTick        DWORD ?
PERFORMANCE_OPTIMIZER ENDS

; ============================================================================
; GLOBAL VARIABLES
; ============================================================================

.data
    g_perfOptimizer     PERFORMANCE_OPTIMIZER <>
    szOptimizerClass    db "PerformanceOptimizer", 0

; ============================================================================
; FUNCTION PROTOTYPES
; ============================================================================

PerformanceOptimizer_Init           proto
PerformanceOptimizer_Cleanup        proto
PerformanceOptimizer_OptimizeMemory proto
PerformanceOptimizer_OptimizeStartup proto
PerformanceOptimizer_OptimizeFileEnumeration proto
PerformanceOptimizer_OptimizeTreeView proto
PerformanceOptimizer_CleanupResources proto
PerformanceOptimizer_StartMonitoring proto
PerformanceOptimizer_StopMonitoring proto
PerformanceOptimizer_GetMemoryInfo proto :DWORD
PerformanceOptimizer_SetMemoryLimit proto :DWORD
PerformanceOptimizer_SetLazyInit proto :DWORD
PerformanceOptimizer_SetAsyncFileOps proto :DWORD
PerformanceOptimizer_SetVirtualRendering proto :DWORD
PerformanceOptimizer_SetAutoCleanup proto :DWORD
PerformanceOptimizer_ApplyOptimizations proto

; Include external module prototypes
extern PerformanceMonitor_Init:proc
extern PerformanceMonitor_Start:proc
extern PerformanceMonitor_Stop:proc
extern PerformanceMonitor_RecordMetric:proc
extern PerformanceMonitor_GetMemoryUsage:proc
extern PerformanceMonitor_SetMemoryLimit:proc
extern PerformanceMonitor_Cleanup:proc

extern MemoryPool_Init:proc
extern MemoryPool_Create:proc
extern MemoryPool_Destroy:proc
extern MemoryPool_Alloc:proc
extern MemoryPool_Free:proc
extern MemoryPool_Cleanup:proc

extern FileEnumeration_Init:proc
extern FileEnumeration_Cleanup:proc

extrn RtlZeroMemory:proc
extrn EmptyWorkingSet:proc

; ============================================================================
; EXTERNAL REFERENCES
; ============================================================================

extrn hInstance:DWORD
extrn hMainWindow:DWORD

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; ============================================================================
; PerformanceOptimizer_Init - Initialize performance optimizer
; ============================================================================
PerformanceOptimizer_Init proc
    ; Check if already initialized
    .if g_perfOptimizer.isInitialized == TRUE
        jmp @Exit
    .endif
    
    ; Initialize configuration with defaults
    mov g_perfOptimizer.config.flags, FLAG_LAZY_INIT or FLAG_ASYNC_FILE_OPS or FLAG_VIRTUAL_RENDERING or FLAG_AUTO_CLEANUP
    mov g_perfOptimizer.config.memoryLimitMB, 50
    mov g_perfOptimizer.config.cpuLimitPercent, 80
    mov g_perfOptimizer.config.lazyInitEnabled, TRUE
    mov g_perfOptimizer.config.asyncFileOpsEnabled, TRUE
    mov g_perfOptimizer.config.virtualRendering, TRUE
    mov g_perfOptimizer.config.autoCleanupEnabled, TRUE
    
    ; Initialize performance monitor
    call PerformanceMonitor_Init
    
    ; Initialize memory pool
    call MemoryPool_Init
    
    ; Initialize file enumeration
    call FileEnumeration_Init

    ; Capture process start tick for startup metrics
    invoke GetTickCount
    mov g_perfOptimizer.startupTick, eax
    
    ; Set initialized flag
    mov g_perfOptimizer.isInitialized, TRUE
    
@Exit:
    ret
PerformanceOptimizer_Init endp

; ============================================================================
; PerformanceOptimizer_Cleanup - Clean up performance optimizer
; ============================================================================
PerformanceOptimizer_Cleanup proc
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Clean up file enumeration
    call FileEnumeration_Cleanup
    
    ; Clean up memory pool
    call MemoryPool_Cleanup
    
    ; Clean up performance monitor
    call PerformanceMonitor_Cleanup
    
    ; Clear optimizer structure
    invoke RtlZeroMemory, addr g_perfOptimizer, sizeof PERFORMANCE_OPTIMIZER
    
@Exit:
    ret
PerformanceOptimizer_Cleanup endp

; ============================================================================
; PerformanceOptimizer_OptimizeMemory - Optimize memory usage
; ============================================================================
PerformanceOptimizer_OptimizeMemory proc
    LOCAL memoryUsage:DWORD
    LOCAL memoryLimit:DWORD
    
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Get current memory usage
    call PerformanceMonitor_GetMemoryUsage
    mov memoryUsage, eax
    
    ; Get memory limit
    mov eax, g_perfOptimizer.config.memoryLimitMB
    mov ecx, 1024 * 1024  ; Convert MB to bytes
    mul ecx
    mov memoryLimit, eax
    
    ; Check if we're exceeding memory limit
    mov eax, memoryUsage
    .if eax > memoryLimit
        ; Trigger memory optimization
        call PerformanceOptimizer_CleanupResources
        invoke GetCurrentProcess
        invoke SetProcessWorkingSetSize, eax, -1, -1
    .endif

    ; Record metric
    invoke PerformanceMonitor_RecordMetric, METRIC_MEMORY_USAGE, memoryUsage, 0  ; 0 = bytes
    
@Exit:
    ret
PerformanceOptimizer_OptimizeMemory endp

; ============================================================================
; PerformanceOptimizer_OptimizeStartup - Optimize startup time
; ============================================================================
PerformanceOptimizer_OptimizeStartup proc
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Check if lazy initialization is enabled (configuration already honored at component level)
    mov eax, g_perfOptimizer.config.lazyInitEnabled
    .if eax == TRUE
        ; nothing additional here; the calling code defers expensive subsystems
    .endif

    ; Record startup time metric based on captured start tick
    invoke GetTickCount
    sub eax, g_perfOptimizer.startupTick
    invoke PerformanceMonitor_RecordMetric, METRIC_STARTUP_TIME, eax, 1  ; 1 = milliseconds
    
@Exit:
    ret
PerformanceOptimizer_OptimizeStartup endp

; ============================================================================
; PerformanceOptimizer_OptimizeFileEnumeration - Optimize file enumeration
; ============================================================================
PerformanceOptimizer_OptimizeFileEnumeration proc
    LOCAL startTick:DWORD
    LOCAL endTick:DWORD
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Check if async file operations are enabled (caller uses async APIs)
    mov eax, g_perfOptimizer.config.asyncFileOpsEnabled
    .if eax == FALSE
        ret
    .endif

    invoke GetTickCount
    mov startTick, eax

    ; No direct work here; enumeration runs in background threads

    invoke GetTickCount
    mov endTick, eax
    sub endTick, startTick
    invoke PerformanceMonitor_RecordMetric, METRIC_FILE_ENUM_TIME, endTick, 1  ; 1 = ms
    
@Exit:
    ret
PerformanceOptimizer_OptimizeFileEnumeration endp

; ============================================================================
; PerformanceOptimizer_OptimizeTreeView - Optimize TreeView rendering
; ============================================================================
PerformanceOptimizer_OptimizeTreeView proc
    LOCAL startTick:DWORD
    LOCAL endTick:DWORD
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Check if virtual rendering is enabled (implemented in UI layer)
    mov eax, g_perfOptimizer.config.virtualRendering
    .if eax == FALSE
        ret
    .endif

    invoke GetTickCount
    mov startTick, eax

    ; UI layer is responsible for virtualization; this hook exists for metrics

    invoke GetTickCount
    mov endTick, eax
    sub endTick, startTick
    invoke PerformanceMonitor_RecordMetric, METRIC_TREEVIEW_RENDER, endTick, 1  ; ms
    
@Exit:
    ret
PerformanceOptimizer_OptimizeTreeView endp

; ============================================================================
; PerformanceOptimizer_CleanupResources - Clean up unused resources
; ============================================================================
PerformanceOptimizer_CleanupResources proc
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Check if automatic cleanup is enabled
    mov eax, g_perfOptimizer.config.autoCleanupEnabled
    .if eax == TRUE
        ; Trim working set to release unused pages
        invoke GetCurrentProcess
        invoke SetProcessWorkingSetSize, eax, -1, -1

        ; Release pooled memory to reduce footprint
        call MemoryPool_Cleanup
    .endif
    
@Exit:
    ret
PerformanceOptimizer_CleanupResources endp

; ============================================================================
; PerformanceOptimizer_StartMonitoring - Start performance monitoring
; ============================================================================
PerformanceOptimizer_StartMonitoring proc
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Start performance monitor
    call PerformanceMonitor_Start
    
@Exit:
    ret
PerformanceOptimizer_StartMonitoring endp

; ============================================================================
; PerformanceOptimizer_StopMonitoring - Stop performance monitoring
; ============================================================================
PerformanceOptimizer_StopMonitoring proc
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Stop performance monitor
    call PerformanceMonitor_Stop
    
@Exit:
    ret
PerformanceOptimizer_StopMonitoring endp

; ============================================================================
; PerformanceOptimizer_GetMemoryInfo - Get current memory information
; Parameters: pMemoryInfo - pointer to memory info structure (first DWORD = bytes in use)
; ============================================================================
PerformanceOptimizer_GetMemoryInfo proc pMemoryInfo:DWORD
    LOCAL memoryUsage:DWORD
    
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        xor eax, eax
        jmp @Exit
    .endif
    
    mov eax, pMemoryInfo
    test eax, eax
    jz @Exit
    
    ; Get current memory usage
    call PerformanceMonitor_GetMemoryUsage
    mov memoryUsage, eax
    
    ; Fill memory info structure (first DWORD = current usage in bytes)
    mov ecx, memoryUsage
    mov [eax], ecx
    
    mov eax, 1  ; Success
    
@Exit:
    ret
PerformanceOptimizer_GetMemoryInfo endp

; ============================================================================
; PerformanceOptimizer_SetMemoryLimit - Set memory usage limit
; Parameters: limitMB - memory limit in MB
; ============================================================================
PerformanceOptimizer_SetMemoryLimit proc limitMB:DWORD
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Update configuration
    mov eax, limitMB
    mov g_perfOptimizer.config.memoryLimitMB, eax
    
    ; Update performance monitor
    invoke PerformanceMonitor_SetMemoryLimit, limitMB
    
@Exit:
    ret
PerformanceOptimizer_SetMemoryLimit endp

; ============================================================================
; PerformanceOptimizer_SetLazyInit - Enable/disable lazy initialization
; Parameters: enabled - TRUE to enable, FALSE to disable
; ============================================================================
PerformanceOptimizer_SetLazyInit proc enabled:DWORD
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Update configuration
    mov eax, enabled
    mov g_perfOptimizer.config.lazyInitEnabled, eax
    
    ; Update flags
    .if eax == TRUE
        or g_perfOptimizer.config.flags, FLAG_LAZY_INIT
    .else
        and g_perfOptimizer.config.flags, NOT FLAG_LAZY_INIT
    .endif
    
@Exit:
    ret
PerformanceOptimizer_SetLazyInit endp

; ============================================================================
; PerformanceOptimizer_SetAsyncFileOps - Enable/disable async file operations
; Parameters: enabled - TRUE to enable, FALSE to disable
; ============================================================================
PerformanceOptimizer_SetAsyncFileOps proc enabled:DWORD
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Update configuration
    mov eax, enabled
    mov g_perfOptimizer.config.asyncFileOpsEnabled, eax
    
    ; Update flags
    .if eax == TRUE
        or g_perfOptimizer.config.flags, FLAG_ASYNC_FILE_OPS
    .else
        and g_perfOptimizer.config.flags, NOT FLAG_ASYNC_FILE_OPS
    .endif
    
@Exit:
    ret
PerformanceOptimizer_SetAsyncFileOps endp

; ============================================================================
; PerformanceOptimizer_SetVirtualRendering - Enable/disable virtual rendering
; Parameters: enabled - TRUE to enable, FALSE to disable
; ============================================================================
PerformanceOptimizer_SetVirtualRendering proc enabled:DWORD
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Update configuration
    mov eax, enabled
    mov g_perfOptimizer.config.virtualRendering, eax
    
    ; Update flags
    .if eax == TRUE
        or g_perfOptimizer.config.flags, FLAG_VIRTUAL_RENDERING
    .else
        and g_perfOptimizer.config.flags, NOT FLAG_VIRTUAL_RENDERING
    .endif
    
@Exit:
    ret
PerformanceOptimizer_SetVirtualRendering endp

; ============================================================================
; PerformanceOptimizer_SetAutoCleanup - Enable/disable automatic cleanup
; Parameters: enabled - TRUE to enable, FALSE to disable
; ============================================================================
PerformanceOptimizer_SetAutoCleanup proc enabled:DWORD
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Update configuration
    mov eax, enabled
    mov g_perfOptimizer.config.autoCleanupEnabled, eax
    
    ; Update flags
    .if eax == TRUE
        or g_perfOptimizer.config.flags, FLAG_AUTO_CLEANUP
    .else
        and g_perfOptimizer.config.flags, NOT FLAG_AUTO_CLEANUP
    .endif
    
@Exit:
    ret
PerformanceOptimizer_SetAutoCleanup endp

; ============================================================================
; PerformanceOptimizer_ApplyOptimizations - Apply all optimizations
; ============================================================================
PerformanceOptimizer_ApplyOptimizations proc
    ; Check if initialized
    .if g_perfOptimizer.isInitialized == FALSE
        jmp @Exit
    .endif
    
    ; Apply memory optimization
    call PerformanceOptimizer_OptimizeMemory
    
    ; Apply startup optimization
    call PerformanceOptimizer_OptimizeStartup
    
    ; Apply file enumeration optimization
    call PerformanceOptimizer_OptimizeFileEnumeration
    
    ; Apply TreeView optimization
    call PerformanceOptimizer_OptimizeTreeView
    
    ; Clean up resources
    call PerformanceOptimizer_CleanupResources
    
@Exit:
    ret
PerformanceOptimizer_ApplyOptimizations endp

end