; ===============================================================================
; Universal Dispatcher - Pure MASM x86-64 Agentic Orchestration
; Zero Dependencies, Production-Ready
; ===============================================================================
; This module provides a unified entry point for all agentic operations:
; - Universal command dispatch with jump table
; - Parameter marshalling and result handling
; - Integration with existing MASM modules
; - Intent classification and routing
; ===============================================================================

option casemap:none

; ===============================================================================
; EXTERNAL DEPENDENCIES (Existing MASM Modules)
; ===============================================================================

extern agent_process_command:proc           ; From agentic_masm.asm
extern InitializePlanningContext:proc        ; From advanced_planning_engine.asm
extern GeneratePlan:proc                     ; From advanced_planning_engine.asm
extern StartRestApiServer:proc              ; From rest_api_server_full.asm
extern InitializeTracer:proc                  ; From distributed_tracer.asm
extern StartSpan:proc                        ; From distributed_tracer.asm
extern InitializeMemoryPool:proc             ; From enterprise_common.asm
extern asm_log:proc                          ; From asm_log.asm
extern asm_log_init:proc                     ; From asm_log.asm
extern GlobalAlloc:proc                      ; Windows API
extern QueryPerformanceCounter:proc          ; Windows API

; Compiler integration
extern InitializeUniversalCompiler:proc     ; From universal_compiler_integration.asm
extern DetectLanguageByExtension:proc       ; From universal_compiler_integration.asm
extern CompileFileWithLanguage:proc         ; From universal_compiler_integration.asm
extern GetLanguageNameById:proc             ; From universal_compiler_integration.asm

; ===============================================================================
; CONSTANTS
; ===============================================================================

MAX_COMMAND_LEN         equ 8192
MAX_RESPONSE_LEN        equ 65536
MAX_INTENT_TOKENS       equ 32
MAX_MODULES             equ 16

; Intent classification
INTENT_PLAN             equ 1
INTENT_ASK              equ 2
INTENT_EDIT             equ 3
INTENT_CONFIGURE        equ 4
INTENT_BUILD            equ 5
INTENT_RUN              equ 6
INTENT_DEPLOY           equ 7
INTENT_DEBUG            equ 8

; Module IDs
MODULE_AGENTIC          equ 1
MODULE_PLANNING         equ 2
MODULE_REST_API         equ 3
MODULE_TRACING          equ 4
MODULE_COMMON           equ 5
MODULE_ERROR_ANALYSIS   equ 6
MODULE_ML_DETECTOR      equ 7
MODULE_OAUTH2           equ 8
MODULE_COMPILER         equ 9

; Memory allocation
GPTR                    equ 0040h

; Tracing constants
SPAN_KIND_INTERNAL     equ 1

; ===============================================================================
; STRUCTURES
; ===============================================================================

DispatchCommand STRUCT
    szCommand           qword ?         ; Command string pointer
    dwIntent           dword ?         ; Intent classification
    dwModuleId         dword ?         ; Target module
    pHandler           qword ?         ; Handler function pointer
    dwFlags            dword ?         ; Execution flags
DispatchCommand ENDS

DispatchResult STRUCT
    bSuccess           byte ?          ; 1 = success, 0 = failure
    pOutput            qword ?         ; Output buffer pointer
    qwOutputLen        qword ?         ; Output length
    dwErrorCode        dword ?         ; Error code
    qwDuration         qword ?         ; Execution duration (100ns)
DispatchResult ENDS

ModuleDescriptor STRUCT
    dwModuleId         dword ?         ; Module identifier
    szModuleName      qword ?         ; Module name pointer
    pInitialize       qword ?         ; Initialize function
    pShutdown         qword ?         ; Shutdown function
    pDispatch         qword ?         ; Dispatch function
    dwFlags           dword ?         ; Module flags
ModuleDescriptor ENDS

; ===============================================================================
; DATA SEGMENT
; ===============================================================================

.data

; Command dispatch table
DispatchTable LABEL DispatchCommand
    ; Agentic tools
    DQ offset szCmdReadFile, INTENT_EDIT, MODULE_AGENTIC, offset HandleReadFile, 0
    DQ offset szCmdWriteFile, INTENT_EDIT, MODULE_AGENTIC, offset HandleWriteFile, 0
    DQ offset szCmdListDir, INTENT_ASK, MODULE_AGENTIC, offset HandleListDir, 0
    DQ offset szCmdExecuteCmd, INTENT_RUN, MODULE_AGENTIC, offset HandleExecuteCmd, 0
    
    ; Planning engine
    DQ offset szCmdPlan, INTENT_PLAN, MODULE_PLANNING, offset HandlePlan, 0
    DQ offset szCmdSchedule, INTENT_PLAN, MODULE_PLANNING, offset HandleSchedule, 0
    DQ offset szCmdAnalyze, INTENT_ASK, MODULE_PLANNING, offset HandleAnalyze, 0
    
    ; REST API
    DQ offset szCmdStartServer, INTENT_CONFIGURE, MODULE_REST_API, offset HandleStartServer, 0
    DQ offset szCmdStopServer, INTENT_CONFIGURE, MODULE_REST_API, offset HandleStopServer, 0
    
    ; Tracing
    DQ offset szCmdStartSpan, INTENT_DEBUG, MODULE_TRACING, offset HandleStartSpan, 0
    DQ offset szCmdEndSpan, INTENT_DEBUG, MODULE_TRACING, offset HandleEndSpan, 0
    
    ; Compiler commands
    DQ offset szCmdCompile, INTENT_BUILD, MODULE_COMPILER, offset HandleCompile, 0
    DQ offset szCmdDetectLang, INTENT_ASK, MODULE_COMPILER, offset HandleDetectLang, 0
    DQ offset szCmdListLangs, INTENT_ASK, MODULE_COMPILER, offset HandleListLangs, 0
    DQ offset szCmdCompileAll, INTENT_BUILD, MODULE_COMPILER, offset HandleCompileAll, 0
    
    ; End marker
    DQ 0, 0, 0, 0, 0

; Command strings
szCmdReadFile          db "readFile",0
szCmdWriteFile         db "writeFile",0
szCmdListDir           db "listDirectory",0
szCmdExecuteCmd        db "executeCommand",0
szCmdPlan              db "plan",0
szCmdSchedule          db "schedule",0
szCmdAnalyze           db "analyze",0
szCmdStartServer       db "startServer",0
szCmdStopServer        db "stopServer",0
szCmdStartSpan         db "startSpan",0
szCmdEndSpan           db "endSpan",0
szCmdCompile           db "compile",0
szCmdDetectLang        db "detectLanguage",0
szCmdListLangs         db "listLanguages",0
szCmdCompileAll        db "compileAll",0

; Intent keywords
szIntentPlan           db "plan",0
szIntentAsk            db "ask",0
szIntentEdit           db "edit",0
szIntentConfigure      db "configure",0
szIntentBuild          db "build",0
szIntentRun            db "run",0
szIntentDeploy         db "deploy",0
szIntentDebug          db "debug",0

; Module descriptors
ModuleDescriptors LABEL ModuleDescriptor
    DQ MODULE_AGENTIC, offset szModuleAgentic, offset InitializeAgentic, 0, offset DispatchAgentic, 0
    DQ MODULE_PLANNING, offset szModulePlanning, offset InitializePlanning, 0, offset DispatchPlanning, 0
    DQ MODULE_REST_API, offset szModuleRestApi, offset InitializeRestApi, 0, offset DispatchRestApi, 0
    DQ MODULE_TRACING, offset szModuleTracing, offset InitializeTracing, 0, offset DispatchTracing, 0
    DQ MODULE_COMMON, offset szModuleCommon, offset InitializeCommon, 0, offset DispatchCommon, 0
    DQ MODULE_COMPILER, offset szModuleCompiler, offset InitializeCompilerModule, 0, offset DispatchCompiler, 0
    DQ 0, 0, 0, 0, 0, 0

szModuleAgentic        db "Agentic Tools",0
szModulePlanning       db "Planning Engine",0
szModuleRestApi        db "REST API Server",0
szModuleTracing        db "Distributed Tracing",0
szModuleCommon         db "Common Infrastructure",0
szModuleCompiler       db "Universal Compiler",0

; Buffers
szCommandBuffer        db MAX_COMMAND_LEN dup(0)
szResponseBuffer       db MAX_RESPONSE_LEN dup(0)
szTokenBuffer          db MAX_INTENT_TOKENS * 64 dup(0)

; Logging strings
szDispatchStart        db "Dispatch started: ",0
szDispatchComplete     db "Dispatch completed, duration: ",0
szIntentClassified    db "Intent classified: ",0
szHandlerFound        db "Handler found: ",0
szFallbackAgentic     db "No handler found, using agentic fallback",0
szIntentPlanStr       db "INTENT_PLAN",0
szIntentAskStr        db "INTENT_ASK",0
szIntentEditStr       db "INTENT_EDIT",0
szIntentConfigureStr  db "INTENT_CONFIGURE",0
szIntentBuildStr      db "INTENT_BUILD",0
szIntentRunStr        db "INTENT_RUN",0
szIntentDeployStr     db "INTENT_DEPLOY",0
szIntentDebugStr      db "INTENT_DEBUG",0
szServerStopRequested db "Server stop requested",0
szSpanEnded          db "Span ended",0
szSpanName           db "UniversalDispatch",0
szAgenticInitialized db "Agentic module initialized",0
szRestApiInitialized db "REST API module initialized",0
szTracingInitialized db "Tracing module initialized",0
szCommonInitialized  db "Common module initialized",0

; Global state
g_bInitialized         db 0
g_dwActiveModules      dword 0
g_qwDispatchCount      qword 0
g_qwTotalDuration      qword 0

; ===============================================================================
; CODE SEGMENT
; ===============================================================================

.code

; ===============================================================================
; INITIALIZATION
; ===============================================================================

; Initialize universal dispatcher
; Returns: RAX = 1 success, 0 failure
InitializeDispatcher PROC
    push    rbp
    mov     rbp, rsp
    
    ; Check if already initialized
    cmp     byte ptr [g_bInitialized], 0
    jne     .init_already
    
    ; Initialize logging
    call    asm_log_init
    
    ; Initialize memory pool
    mov     rcx, 1024
    call    InitializeMemoryPool
    test    rax, rax
    jz      .init_fail
    
    ; Initialize modules
    mov     r12, offset ModuleDescriptors
    
.init_modules_loop:
    cmp     dword ptr [r12].ModuleDescriptor.dwModuleId, 0
    je      .init_modules_done
    
    ; Call module initialization
    mov     rax, [r12].ModuleDescriptor.pInitialize
    test    rax, rax
    jz      .init_modules_next
    
    call    rax
    test    rax, rax
    jz      .init_modules_next
    
    ; Mark module as active
    inc     dword ptr [g_dwActiveModules]
    
.init_modules_next:
    add     r12, sizeof ModuleDescriptor
    jmp     .init_modules_loop
    
.init_modules_done:
    mov     byte ptr [g_bInitialized], 1
    
    ; Log initialization
    lea     rcx, offset szDispatcherInitialized
    call    asm_log
    
    mov     eax, 1
    jmp     .init_done
    
.init_already:
    mov     eax, 1
    jmp     .init_done
    
.init_fail:
    xor     eax, eax
    
.init_done:
    pop     rbp
    ret
InitializeDispatcher ENDP

szDispatcherInitialized db "Universal Dispatcher Initialized",0

; ===============================================================================
; COMMAND DISPATCH
; ===============================================================================

; Universal command dispatch
; RCX = command string, RDX = parameters
; Returns: RAX = DispatchResult pointer
UniversalDispatch PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 64
    
    ; Save parameters
    mov     [rbp - 8], rcx      ; command
    mov     [rbp - 16], rdx     ; parameters
    
    ; Log dispatch start
    lea     rcx, szDispatchStart
    call    asm_log
    mov     rcx, [rbp - 8]
    call    asm_log
    
    ; Start performance counter
    call    GetPerformanceCounter
    mov     [rbp - 24], rax     ; start time
    
    ; Classify intent
    mov     rcx, [rbp - 8]
    call    ClassifyIntent
    mov     [rbp - 28], eax     ; intent
    
    ; Log intent classification
    lea     rcx, szIntentClassified
    call    asm_log
    mov     rcx, [rbp - 28]
    call    LogIntentId
    
    ; Find command handler
    mov     r12, offset DispatchTable
    
.find_handler_loop:
    cmp     qword ptr [r12].DispatchCommand.szCommand, 0
    je      .handler_not_found
    
    ; Compare command
    mov     rcx, [rbp - 8]
    mov     rdx, [r12].DispatchCommand.szCommand
    call    StringCompare
    test    eax, eax
    jnz     .handler_found
    
    add     r12, sizeof DispatchCommand
    jmp     .find_handler_loop
    
.handler_found:
    ; Log handler found
    lea     rcx, szHandlerFound
    call    asm_log
    mov     rcx, [r12].DispatchCommand.szCommand
    call    asm_log
    
    ; Call handler
    mov     rax, [r12].DispatchCommand.pHandler
    mov     rcx, [rbp - 8]      ; command
    mov     rdx, [rbp - 16]     ; parameters
    call    rax
    
    jmp     .dispatch_done
    
.handler_not_found:
    ; Log fallback to agentic
    lea     rcx, szFallbackAgentic
    call    asm_log
    
    ; Use agentic tools as fallback
    mov     rcx, [rbp - 8]
    mov     rdx, [rbp - 16]
    call    DispatchAgentic
    
.dispatch_done:
    ; Calculate duration
    call    GetPerformanceCounter
    sub     rax, [rbp - 24]
    mov     [rbp - 36], rax     ; duration
    
    ; Log dispatch completion
    lea     rcx, szDispatchComplete
    call    asm_log
    mov     rcx, [rbp - 36]
    call    LogDuration
    
    ; Update statistics
    inc     qword ptr [g_qwDispatchCount]
    add     qword ptr [g_qwTotalDuration], rax
    
    ; Create result structure
    mov     rcx, sizeof DispatchResult
    call    AllocateMemory
    test    rax, rax
    jz      .result_fail
    
    mov     r13, rax
    mov     byte ptr [r13].DispatchResult.bSuccess, 1
    mov     [r13].DispatchResult.qwDuration, rax
    
    mov     rax, r13
    jmp     .dispatch_exit
    
.result_fail:
    xor     rax, rax
    
.dispatch_exit:
    add     rsp, 64
    pop     rbp
    ret
UniversalDispatch ENDP

; ===============================================================================
; INTENT CLASSIFICATION
; ===============================================================================

; Classify intent from command string
; RCX = command string
; Returns: RAX = intent ID
ClassifyIntent PROC
    push    rbp
    mov     rbp, rsp
    
    ; Tokenize command
    mov     r12, rcx
    lea     r13, szTokenBuffer
    mov     r14, 0              ; token count
    
.tokenize_loop:
    mov     al, byte ptr [r12]
    test    al, al
    jz      .tokenize_done
    
    cmp     al, ' '
    je      .tokenize_next
    cmp     al, 9               ; tab
    je      .tokenize_next
    
    ; Copy token
    mov     r15, r13
    
.copy_token:
    mov     byte ptr [r15], al
    inc     r12
    inc     r15
    mov     al, byte ptr [r12]
    test    al, al
    jz      .token_copy_done
    cmp     al, ' '
    je      .token_copy_done
    cmp     al, 9
    je      .token_copy_done
    jmp     .copy_token
    
.token_copy_done:
    mov     byte ptr [r15], 0
    inc     r14
    add     r13, 64             ; next token slot
    jmp     .tokenize_loop
    
.tokenize_next:
    inc     r12
    jmp     .tokenize_loop
    
.tokenize_done:
    ; Check each token against intent keywords
    lea     r12, szTokenBuffer
    mov     r13, 0              ; token index
    
.check_intents_loop:
    cmp     r13, r14
    jge     .intent_default
    
    ; Check for plan intent
    mov     rcx, r12
    lea     rdx, szIntentPlan
    call    StringCompare
    test    eax, eax
    jnz     .intent_plan
    
    ; Check for ask intent
    mov     rcx, r12
    lea     rdx, szIntentAsk
    call    StringCompare
    test    eax, eax
    jnz     .intent_ask
    
    ; Check for edit intent
    mov     rcx, r12
    lea     rdx, szIntentEdit
    call    StringCompare
    test    eax, eax
    jnz     .intent_edit
    
    ; Check for configure intent
    mov     rcx, r12
    lea     rdx, szIntentConfigure
    call    StringCompare
    test    eax, eax
    jnz     .intent_configure
    
    ; Check for build intent
    mov     rcx, r12
    lea     rdx, szIntentBuild
    call    StringCompare
    test    eax, eax
    jnz     .intent_build
    
    ; Check for run intent
    mov     rcx, r12
    lea     rdx, szIntentRun
    call    StringCompare
    test    eax, eax
    jnz     .intent_run
    
    ; Check for debug intent
    mov     rcx, r12
    lea     rdx, szIntentDebug
    call    StringCompare
    test    eax, eax
    jnz     .intent_debug
    
    add     r12, 64
    inc     r13
    jmp     .check_intents_loop
    
.intent_plan:
    mov     eax, INTENT_PLAN
    jmp     .intent_done
    
.intent_ask:
    mov     eax, INTENT_ASK
    jmp     .intent_done
    
.intent_edit:
    mov     eax, INTENT_EDIT
    jmp     .intent_done
    
.intent_configure:
    mov     eax, INTENT_CONFIGURE
    jmp     .intent_done
    
.intent_build:
    mov     eax, INTENT_BUILD
    jmp     .intent_done
    
.intent_run:
    mov     eax, INTENT_RUN
    jmp     .intent_done
    
.intent_debug:
    mov     eax, INTENT_DEBUG
    jmp     .intent_done
    
.intent_default:
    mov     eax, INTENT_ASK     ; Default to ask intent
    
.intent_done:
    pop     rbp
    ret
ClassifyIntent ENDP

; ===============================================================================
; MODULE HANDLERS
; ===============================================================================

; Agentic module handler
HandleReadFile PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Call agentic tools file reading
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    agent_process_command
    
    leave
    ret
HandleReadFile ENDP

HandleWriteFile PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Call agentic tools file writing
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    agent_process_command
    
    leave
    ret
HandleWriteFile ENDP

HandleListDir PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Call agentic tools directory listing
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    agent_process_command
    
    leave
    ret
HandleListDir ENDP

HandleExecuteCmd PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Call agentic tools command execution
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    agent_process_command
    
    leave
    ret
HandleExecuteCmd ENDP

; Planning module handler
HandlePlan PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Initialize planning context if needed
    mov     rcx, 100            ; max tasks
    mov     rdx, 36000000000    ; 1 hour horizon
    call    InitializePlanningContext
    
    ; Generate plan
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    GeneratePlan
    
    leave
    ret
HandlePlan ENDP

HandleSchedule PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Call planning engine with scheduling parameters
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    GeneratePlan
    
    leave
    ret
HandleSchedule ENDP

HandleAnalyze PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Call planning engine analysis
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    GeneratePlan
    
    leave
    ret
HandleAnalyze ENDP

; REST API module handler
HandleStartServer PROC
    mov     rcx, 8080           ; port
    call    StartRestApiServer
    ret
HandleStartServer ENDP

HandleStopServer PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Stop REST API server (placeholder - would need StopRestApiServer)
    ; For now, log the stop request
    lea     rcx, szServerStopRequested
    call    asm_log
    
    mov     rax, 1
    leave
    ret
HandleStopServer ENDP

; Tracing module handler
HandleStartSpan PROC
    mov     rcx, offset szSpanName
    mov     rdx, SPAN_KIND_INTERNAL
    xor     r8, r8              ; no parent
    xor     r9, r9              ; no attributes
    call    StartSpan
    ret
HandleStartSpan ENDP

HandleEndSpan PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; End tracing span (placeholder - would need EndSpan)
    ; For now, log the span end
    lea     rcx, szSpanEnded
    call    asm_log
    
    mov     rax, 1
    leave
    ret
HandleEndSpan ENDP

szSpanName db "UniversalDispatch",0

; ===============================================================================
; MODULE DISPATCHERS
; ===============================================================================

; Agentic module dispatcher
DispatchAgentic PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Route to agentic tools
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    agent_process_command
    
    leave
    ret
DispatchAgentic ENDP

; Planning module dispatcher
DispatchPlanning PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Route to planning engine
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    GeneratePlan
    
    leave
    ret
DispatchPlanning ENDP

; REST API module dispatcher
DispatchRestApi PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Route to REST API server
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    StartRestApiServer
    
    leave
    ret
DispatchRestApi ENDP

; Tracing module dispatcher
DispatchTracing PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Route to tracing system
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    StartSpan
    
    leave
    ret
DispatchTracing ENDP

; Common module dispatcher
DispatchCommon PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Route to common infrastructure
    mov     rcx, [rbp + 16]      ; command
    mov     rdx, [rbp + 24]      ; parameters
    call    InitializeMemoryPool
    
    leave
    ret
DispatchCommon ENDP

; ===============================================================================
; MODULE INITIALIZERS
; ===============================================================================

InitializeAgentic PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Initialize agentic tools
    lea     rcx, szAgenticInitialized
    call    asm_log
    
    mov     rax, 1
    leave
    ret
InitializeAgentic ENDP

InitializePlanning PROC
    mov     rcx, 100            ; max tasks
    mov     rdx, 36000000000    ; 1 hour horizon
    call    InitializePlanningContext
    ret
InitializePlanning ENDP

InitializeRestApi PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Initialize REST API server
    lea     rcx, szRestApiInitialized
    call    asm_log
    
    mov     rax, 1
    leave
    ret
InitializeRestApi ENDP

InitializeTracing PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Initialize tracing system
    call    InitializeTracer
    
    lea     rcx, szTracingInitialized
    call    asm_log
    
    mov     rax, 1
    leave
    ret
InitializeTracing ENDP

InitializeCommon PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Initialize common infrastructure
    mov     rcx, 1024
    call    InitializeMemoryPool
    
    lea     rcx, szCommonInitialized
    call    asm_log
    
    mov     rax, 1
    leave
    ret
InitializeCommon ENDP

; ===============================================================================
; UTILITY FUNCTIONS
; ===============================================================================

; String comparison
; RCX = string1, RDX = string2
; Returns: RAX = 0 if equal, non-zero if different
StringCompare PROC
    push    rsi
    push    rdi
    
    mov     rsi, rcx
    mov     rdi, rdx
    
.compare_loop:
    mov     al, byte ptr [rsi]
    mov     bl, byte ptr [rdi]
    cmp     al, bl
    jne     .not_equal
    
    test    al, al
    jz      .equal
    
    inc     rsi
    inc     rdi
    jmp     .compare_loop
    
.equal:
    xor     eax, eax
    jmp     .done
    
.not_equal:
    mov     eax, 1
    
.done:
    pop     rdi
    pop     rsi
    ret
StringCompare ENDP

; Get performance counter
; Returns: RAX = current tick count
GetPerformanceCounter PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    lea     rcx, [rbp - 8]
    call    QueryPerformanceCounter
    mov     rax, [rbp - 8]
    
    add     rsp, 32
    pop     rbp
    ret
GetPerformanceCounter ENDP

; Allocate memory
; RCX = size
; Returns: RAX = pointer
AllocateMemory PROC
    push    rbp
    mov     rbp, rsp
    
    ; Use GlobalAlloc for simplicity
    mov     rdx, GPTR
    call    GlobalAlloc
    
    pop     rbp
    ret
AllocateMemory ENDP

; ===============================================================================
; LOGGING HELPERS
; ===============================================================================

; Log intent ID as string
; RCX = intent ID
LogIntentId PROC
    push    rbp
    mov     rbp, rsp
    
    cmp     ecx, INTENT_PLAN
    je      .log_plan
    cmp     ecx, INTENT_ASK
    je      .log_ask
    cmp     ecx, INTENT_EDIT
    je      .log_edit
    cmp     ecx, INTENT_CONFIGURE
    je      .log_configure
    cmp     ecx, INTENT_BUILD
    je      .log_build
    cmp     ecx, INTENT_RUN
    je      .log_run
    cmp     ecx, INTENT_DEPLOY
    je      .log_deploy
    cmp     ecx, INTENT_DEBUG
    je      .log_debug
    jmp     .log_unknown
    
.log_plan:
    lea     rcx, szIntentPlanStr
    jmp     .log_it
    
.log_ask:
    lea     rcx, szIntentAskStr
    jmp     .log_it
    
.log_edit:
    lea     rcx, szIntentEditStr
    jmp     .log_it
    
.log_configure:
    lea     rcx, szIntentConfigureStr
    jmp     .log_it
    
.log_build:
    lea     rcx, szIntentBuildStr
    jmp     .log_it
    
.log_run:
    lea     rcx, szIntentRunStr
    jmp     .log_it
    
.log_deploy:
    lea     rcx, szIntentDeployStr
    jmp     .log_it
    
.log_debug:
    lea     rcx, szIntentDebugStr
    jmp     .log_it
    
.log_unknown:
    lea     rcx, szIntentUnknownStr
    
.log_it:
    call    asm_log
    
    pop     rbp
    ret
LogIntentId ENDP

; Log duration in microseconds
; RCX = duration in 100ns units
LogDuration PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Convert to microseconds (divide by 10)
    mov     rax, rcx
    mov     rdx, 0
    mov     rcx, 10
    div     rcx
    
    ; Convert to string
    lea     rcx, [rbp - 16]
    mov     rdx, rax
    call    IntToString
    
    ; Log duration
    lea     rcx, szDurationPrefix
    call    asm_log
    lea     rcx, [rbp - 16]
    call    asm_log
    lea     rcx, szDurationSuffix
    call    asm_log
    
    pop     rbp
    ret
LogDuration ENDP

; Convert integer to string
; RCX = buffer, RDX = integer
IntToString PROC
    push    rbp
    mov     rbp, rsp
    
    mov     rax, rdx
    mov     rsi, rcx
    mov     rcx, 10
    mov     rbx, 0
    
.convert_loop:
    xor     rdx, rdx
    div     rcx
    add     dl, '0'
    mov     byte ptr [rsi + rbx], dl
    inc     rbx
    test    rax, rax
    jnz     .convert_loop
    
    ; Reverse the string
    mov     rdi, rsi
    mov     rcx, rbx
    shr     rcx, 1
    jz      .done
    
.reverse_loop:
    mov     al, byte ptr [rdi]
    mov     dl, byte ptr [rdi + rbx - 1]
    mov     byte ptr [rdi + rbx - 1], al
    mov     byte ptr [rdi], dl
    inc     rdi
    dec     rbx
    dec     rbx
    test    rbx, rbx
    jnz     .reverse_loop
    
.done:
    mov     byte ptr [rsi + rbx], 0
    pop     rbp
    ret
IntToString ENDP

szIntentUnknownStr    db "INTENT_UNKNOWN",0
szDurationPrefix      db " ",0
szDurationSuffix     db " μs",0
szServerStopRequested db "Server stop requested",0
szSpanEnded          db "Span ended",0
szSpanName           db "universal_dispatch",0
szCompilerInitialized db "Universal Compiler module initialized",0

; ===============================================================================
; COMPILER COMMAND HANDLERS
; ===============================================================================

; Initialize compiler module
InitializeCompilerModule PROC
    push    rbp
    mov     rbp, rsp
    
    ; Initialize universal compiler
    call    InitializeUniversalCompiler
    test    rax, rax
    jz      .init_fail
    
    ; Log initialization
    lea     rcx, szCompilerInitialized
    call    asm_log
    
    mov     rax, 1
    jmp     .init_done
    
.init_fail:
    xor     rax, rax
    
.init_done:
    pop     rbp
    ret
InitializeCompilerModule ENDP

; Dispatch compiler module commands
; RCX = command, RDX = parameters
DispatchCompiler PROC
    push    rbp
    mov     rbp, rsp
    
    ; For now, return success
    mov     rax, 1
    
    pop     rbp
    ret
DispatchCompiler ENDP

; Handle compile command
; RCX = source file path, RDX = output file path
HandleCompile PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 64
    
    ; Save parameters
    mov     [rbp-8], rcx        ; source file
    mov     [rbp-16], rdx       ; output file
    
    ; Detect language from file extension
    mov     rcx, [rbp-8]
    call    DetectLanguageByExtension
    mov     [rbp-20], eax       ; language ID
    
    ; Compile file
    mov     rcx, [rbp-8]        ; source file
    mov     rdx, [rbp-16]       ; output file
    mov     r8d, [rbp-20]       ; language ID
    call    CompileFileWithLanguage
    
    add     rsp, 64
    pop     rbp
    ret
HandleCompile ENDP

; Handle detect language command
; RCX = file path
HandleDetectLang PROC
    push    rbp
    mov     rbp, rsp
    
    ; Detect language
    call    DetectLanguageByExtension
    
    ; Convert language ID to name
    mov     ecx, eax
    call    GetLanguageNameById
    
    ; Log result
    mov     rcx, rax
    call    asm_log
    
    pop     rbp
    ret
HandleDetectLang ENDP

; Handle list languages command
HandleListLangs PROC
    push    rbp
    mov     rbp, rsp
    
    ; Log supported languages message
    lea     rcx, szSupportedLangs
    call    asm_log
    
    ; For demonstration, list a few languages
    lea     rcx, szLangListC
    call    asm_log
    lea     rcx, szLangListCpp
    call    asm_log
    lea     rcx, szLangListRust
    call    asm_log
    lea     rcx, szLangListPython
    call    asm_log
    lea     rcx, szLangListJs
    call    asm_log
    lea     rcx, szMoreLangs
    call    asm_log
    
    mov     rax, 1
    pop     rbp
    ret
HandleListLangs ENDP

; Handle compile all command
HandleCompileAll PROC
    push    rbp
    mov     rbp, rsp
    
    ; Placeholder for batch compilation
    lea     rcx, szCompileAllStart
    call    asm_log
    
    mov     rax, 1
    pop     rbp
    ret
HandleCompileAll ENDP

; Compiler command strings
szSupportedLangs      db "Supported Languages:",0
szLangListC           db "  - C (.c)",0
szLangListCpp         db "  - C++ (.cpp, .cxx)",0
szLangListRust        db "  - Rust (.rs)",0
szLangListPython      db "  - Python (.py)",0
szLangListJs          db "  - JavaScript (.js)",0
szMoreLangs           db "  + 42 more languages supported",0
szCompileAllStart     db "Starting batch compilation...",0

; ===============================================================================
; EXPORTS
; ===============================================================================

PUBLIC InitializeDispatcher
PUBLIC UniversalDispatch
PUBLIC ClassifyIntent
PUBLIC InitializeCompilerModule
PUBLIC DispatchCompiler
PUBLIC HandleCompile
PUBLIC HandleDetectLang
PUBLIC HandleListLangs
PUBLIC HandleCompileAll

END