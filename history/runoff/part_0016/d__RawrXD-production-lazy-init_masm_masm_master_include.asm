; ============================================================================
; MASM x64 MASTER ASSEMBLY INCLUDE FILE
; ============================================================================
; This file serves as the central integration point for all MASM x64
; assembly implementations in the RawrXD system, ensuring that all modules
; are properly linked and accessible throughout the codebase.
;
; Module Organization:
;   1. Zero-Day Agentic Engine (zero_day_agentic_engine.asm)
;   2. Zero-Day Integration Layer (zero_day_integration.asm)
;   3. Existing Agentic Systems (agentic_engine.asm, etc.)
;   4. Core MASM Utilities and Helpers
;
; ============================================================================

IFNDEF MASM_MASTER_INCLUDE_ASM
MASM_MASTER_INCLUDE_ASM = 1

; ============================================================================
; EXTERNAL DECLARATIONS - PUBLIC API EXPORTS
; ============================================================================
; All functions declared here are available throughout the compilation unit.
; Format: extern FunctionName:PROC
;
; Organization by module for clarity and maintainability.

; =============================================================================
; ZERO-DAY AGENTIC ENGINE PUBLIC API
; =============================================================================
; File: zero_day_agentic_engine.asm
; Purpose: Enterprise-grade autonomous agent execution with streaming,
;          metrics instrumentation, and comprehensive error handling.
; Status: Production-ready with full documentation

; Engine lifecycle functions
extern ZeroDayAgenticEngine_Create:PROC          ; Create engine instance
extern ZeroDayAgenticEngine_Destroy:PROC         ; Destroy engine (RAII)

; Mission execution
extern ZeroDayAgenticEngine_StartMission:PROC    ; Start mission (async)
extern ZeroDayAgenticEngine_AbortMission:PROC    ; Abort mission (graceful)

; Mission state query
extern ZeroDayAgenticEngine_GetMissionState:PROC ; Get state (IDLE/RUNNING/...)
extern ZeroDayAgenticEngine_GetMissionId:PROC    ; Get mission ID string

; Private/Helper (accessible but internal use preferred)
extern ZeroDayAgenticEngine_ExecuteMission:PROC  ; Core execution logic
extern ZeroDayAgenticEngine_EmitSignal:PROC      ; Callback routing
extern ZeroDayAgenticEngine_LogStructured:PROC   ; Structured logging
extern ZeroDayAgenticEngine_ValidateInstance:PROC ; Input validation
extern ZeroDayAgenticEngine_GenerateMissionId:PROC ; ID generation

; =============================================================================
; ZERO-DAY INTEGRATION LAYER PUBLIC API
; =============================================================================
; File: zero_day_integration.asm
; Purpose: Bridges zero-day engine with existing agentic systems,
;          includes complexity detection and routing.
; Status: Integration-ready

extern ZeroDayIntegration_Initialize:PROC       ; Setup zero-day
extern ZeroDayIntegration_AnalyzeComplexity:PROC ; Complexity detection
extern ZeroDayIntegration_RouteExecution:PROC   ; Route to appropriate engine
extern ZeroDayIntegration_IsHealthy:PROC        ; Health check
extern ZeroDayIntegration_Shutdown:PROC         ; Cleanup

; Callback wrappers for signal routing
extern ZeroDayIntegration_OnAgentStream:PROC    ; Stream signal handler
extern ZeroDayIntegration_OnAgentComplete:PROC  ; Complete signal handler
extern ZeroDayIntegration_OnAgentError:PROC     ; Error signal handler

; =============================================================================
; EXISTING AGENTIC SYSTEM EXPORTS (PLACEHOLDERS FOR COMPATIBILITY)
; =============================================================================
; These functions bridge to existing C++ implementations and are available
; for compatibility. They should be implemented or linked from existing code.

extern AgenticEngine_ExecuteTask:PROC            ; Standard task execution
extern AgenticEngine_CreateContext:PROC          ; Create agentic context
extern AgenticEngine_DestroyContext:PROC         ; Destroy context (RAII)
extern AgenticEngine_GetState:PROC               ; Get engine state
extern AgenticEngine_SetCallback:PROC            ; Register callbacks

; Model routing
extern UniversalModelRouter_GetModelState:PROC   ; Query model state
extern UniversalModelRouter_SelectModel:PROC     ; Choose appropriate model

; Tool management
extern ToolRegistry_InvokeToolSet:PROC          ; Invoke tools
extern ToolRegistry_QueryAvailableTools:PROC    ; List tools

; Planning and orchestration
extern PlanOrchestrator_PlanAndExecute:PROC     ; Plan and execute mission

; Logging and metrics
extern Logger_LogMissionStart:PROC              ; Log start
extern Logger_LogMissionComplete:PROC           ; Log completion
extern Logger_LogMissionError:PROC              ; Log error
extern Logger_LogStructured:PROC                ; Structured logging

extern Metrics_RecordHistogramMission:PROC      ; Record timing
extern Metrics_IncrementMissionCounter:PROC     ; Count operations
extern Metrics_RecordLatency:PROC               ; Record latency

; =============================================================================
; CORE MASM UTILITIES
; =============================================================================
; Standard Win32 and system utilities available for all modules

; Memory management
extern LocalAlloc:PROC
extern LocalFree:PROC
extern HeapAlloc:PROC
extern HeapFree:PROC

; Threading
extern CreateThread:PROC
extern GetCurrentThreadId:PROC
extern GetCurrentThread:PROC
extern SetThreadPriority:PROC
extern GetThreadPriority:PROC
extern WaitForSingleObject:PROC
extern WaitForMultipleObjects:PROC
extern ExitThread:PROC

; Synchronization primitives
extern CreateEventA:PROC
extern CreateMutexA:PROC
extern CreateCriticalSection:PROC
extern DeleteCriticalSection:PROC
extern EnterCriticalSection:PROC
extern LeaveCriticalSection:PROC
extern SetEvent:PROC
extern ResetEvent:PROC
extern ReleaseMutex:PROC
extern CloseHandle:PROC

; Timing
extern GetSystemTimeAsFileTime:PROC
extern GetTickCount:PROC
extern GetTickCount64:PROC
extern Sleep:PROC
extern QueryPerformanceCounter:PROC
extern QueryPerformanceFrequency:PROC

; String operations
extern lstrcpyA:PROC
extern lstrcpynA:PROC
extern lstrlenA:PROC
extern lstrcmpA:PROC
extern lstrcmpiA:PROC

; =============================================================================
; DETECTION AND FAILURE HANDLING
; =============================================================================
; Special purpose functions for agent self-correction and error detection

extern masm_detect_failure:PROC                 ; Detect execution failure
extern masm_puppeteer_correct_response:PROC     ; Correct responses

; =============================================================================
; CONFIGURATION MANAGEMENT (PHASE 3 - PRODUCTION READINESS)
; =============================================================================
; File: config_manager.asm
; Purpose: External configuration management for production deployment
; Status: Production-ready with environment detection and feature toggles

; Configuration lifecycle
extern Config_Initialize:PROC                    ; Initialize config system (thread-safe)
extern Config_LoadFromFile:PROC                  ; Load from JSON file
extern Config_LoadFromEnvironment:PROC           ; Load from env vars (overrides file)
extern Config_Cleanup:PROC                       ; Clean up (clear sensitive data)

; Configuration accessors
extern Config_GetString:PROC                     ; Get string config value
extern Config_GetInteger:PROC                    ; Get integer config value
extern Config_GetBoolean:PROC                    ; Get boolean config value
extern Config_GetSettings:PROC                   ; Get read-only config pointer

; Feature flag system
extern Config_IsFeatureEnabled:PROC              ; Check if feature flag enabled
extern Config_EnableFeature:PROC                 ; Enable feature at runtime
extern Config_DisableFeature:PROC                ; Disable feature at runtime

; Environment detection
extern Config_GetEnvironment:PROC                ; Get current environment name
extern Config_IsProduction:PROC                  ; Check if production mode

; =============================================================================
; CONSTANT DEFINITIONS - PUBLIC API SEMANTICS
; =============================================================================
; These constants define the behavior and interface of the zero-day engine

; Mission states (used throughout the system)
MISSION_STATE_IDLE          = 0
MISSION_STATE_RUNNING       = 1
MISSION_STATE_ABORTED       = 2
MISSION_STATE_COMPLETE      = 3
MISSION_STATE_ERROR         = 4

; Signal types for callback routing
SIGNAL_TYPE_STREAM          = 1     ; Progress/streaming data
SIGNAL_TYPE_COMPLETE        = 2     ; Mission completed
SIGNAL_TYPE_ERROR           = 3     ; Error occurred

; Logging levels
LOG_LEVEL_DEBUG             = 0     ; Detailed diagnostics
LOG_LEVEL_INFO              = 1     ; General info
LOG_LEVEL_WARN              = 2     ; Warnings
LOG_LEVEL_ERROR             = 3     ; Errors only

; Goal complexity levels (for routing)
COMPLEXITY_SIMPLE           = 0     ; Direct execution
COMPLEXITY_MODERATE         = 1     ; Standard planning
COMPLEXITY_HIGH             = 2     ; Failure recovery
COMPLEXITY_EXPERT           = 3     ; Zero-day reasoning

; Integration flags
ZD_FLAG_ENABLED             = 1     ; Zero-day available
ZD_FLAG_FALLBACK_ACTIVE     = 2     ; Fallback mode
ZD_FLAG_AUTO_RETRY          = 4     ; Auto-retry enabled

; Feature flags (Phase 3 - Configuration Management)
FEATURE_GPU_ACCELERATION    = 00000001h  ; Bit 0: GPU-accelerated inference
FEATURE_DISTRIBUTED_TRACE   = 00000002h  ; Bit 1: OpenTelemetry tracing
FEATURE_ADVANCED_METRICS    = 00000004h  ; Bit 2: Prometheus metrics
FEATURE_HOTPATCH_DYNAMIC    = 00000008h  ; Bit 3: Runtime hotpatching
FEATURE_QUANTUM_OPS         = 00000010h  ; Bit 4: Quantum algorithms
FEATURE_EXPERIMENTAL_MODELS = 00000020h  ; Bit 5: Experimental model support
FEATURE_DEBUG_MODE          = 00000040h  ; Bit 6: Debug logging/features
FEATURE_AUTO_UPDATE         = 00000080h  ; Bit 7: Automatic updates
FEATURE_AGENTIC_ORCHESTRATION = 00000100h  ; Bit 8: Agentic orchestration

; =============================================================================
; PHASE 4: ERROR HANDLING ENHANCEMENT EXPORTS
; =============================================================================
; File: error_handler.asm
; Purpose: Centralized error handling with structured logging and recovery
; Status: Production-ready

extern ErrorHandler_Initialize:PROC      ; Initialize error handling system
extern ErrorHandler_Capture:PROC         ; Capture error with context
extern ErrorHandler_GetStats:PROC        ; Get error statistics
extern ErrorHandler_Reset:PROC           ; Reset error stats and stack
extern ErrorHandler_Cleanup:PROC         ; Cleanup error handling resources

; Error severity levels
ERROR_SEVERITY_INFO         = 0
ERROR_SEVERITY_WARNING      = 1
ERROR_SEVERITY_ERROR        = 2
ERROR_SEVERITY_CRITICAL     = 3
ERROR_SEVERITY_FATAL        = 4

; Error categories
ERROR_CATEGORY_MEMORY       = 0
ERROR_CATEGORY_IO           = 1
ERROR_CATEGORY_NETWORK      = 2
ERROR_CATEGORY_VALIDATION   = 3
ERROR_CATEGORY_LOGIC        = 4
ERROR_CATEGORY_SYSTEM       = 5

; =============================================================================
; PHASE 4: RESOURCE GUARDS EXPORTS (RAII-Style Resource Management)
; =============================================================================
; File: resource_guards.asm
; Purpose: Automatic resource cleanup to prevent leaks
; Status: Production-ready

extern Guard_CreateFile:PROC             ; Create file handle guard
extern Guard_CreateMemory:PROC           ; Create memory allocation guard
extern Guard_CreateMutex:PROC            ; Create mutex lock guard
extern Guard_CreateRegistry:PROC         ; Create registry key guard
extern Guard_CreateSocket:PROC           ; Create socket guard
extern Guard_Destroy:PROC                ; Destroy guard and cleanup resource
extern Guard_Release:PROC                ; Release ownership without cleanup

; =============================================================================
; MACRO DEFINITIONS - HELPFUL FOR MODULE IMPLEMENTATION
; =============================================================================
; These macros standardize common patterns across all modules

; Macro: DECLARE_PROC
; Purpose: Standard procedure declaration with frame prologue/epilogue
; Usage: In .CODE section before your procedure implementation
DECLARE_PROC MACRO procname, preserve_list, stack_alloc_size
    procname PROC FRAME
    IFNB <preserve_list>
        PUSH_REG preserve_list
    ENDIF
    IFNB <stack_alloc_size>
        .ALLOCSTACK stack_alloc_size
        SUB rsp, stack_alloc_size
    ENDIF
    .ENDPROLOG
ENDM

; Macro: RETURN_PROC
; Purpose: Standard procedure epilogue and return
; Usage: Before RET in your procedure
RETURN_PROC MACRO preserve_list, stack_alloc_size
    IFNB <stack_alloc_size>
        ADD rsp, stack_alloc_size
    ENDIF
    IFNB <preserve_list>
        POP preserve_list
    ENDIF
    RET
ENDM

; =============================================================================
; BUILD INTEGRATION NOTES
; =============================================================================
;
; This file should be included in all MASM compilation units that need access
; to the zero-day agentic engine or other system modules.
;
; Linking order (for linker):
;   1. zero_day_agentic_engine.asm (core engine)
;   2. zero_day_integration.asm (integration layer)
;   3. Existing MASM/C++ modules
;
; Compiler flags required:
;   - ML64.exe for MASM x64 assembly
;   - /W3 /D_DEBUG /DWIN32 (or appropriate for release build)
;
; Include syntax in other .ASM files:
;   INCLUDE masm/masm_master_include.asm
;
; This ensures all external declarations are available when your module
; calls functions from other modules.
;
; =============================================================================
; CONFIGURATION OPTIONS
; =============================================================================
; These can be modified at compile-time to customize behavior

; Set to 1 to enable verbose logging in production
DEFAULT_VERBOSE_LOGGING     = 0

; Set to 1 to enable automatic retry on mission failure
DEFAULT_AUTO_RETRY_ENABLED  = 1

; Default timeout for mission execution (in milliseconds)
DEFAULT_MISSION_TIMEOUT_MS  = 300000   ; 5 minutes

; Thread stack size for worker threads (in bytes)
DEFAULT_THREAD_STACK_SIZE   = 65536    ; 64 KB

; =============================================================================
; VERSION INFORMATION
; =============================================================================
; Format: MAJOR.MINOR.PATCH
; Updated whenever breaking changes are made

MASM_MASTER_INCLUDE_VERSION = 100h    ; v1.0.0

; =============================================================================
; INCLUDE GUARD END
; =============================================================================

ENDIF ; MASM_MASTER_INCLUDE_ASM

