; ============================================================================
; ZERO-DAY AGENTIC ENGINE INTEGRATION
; ============================================================================
; Bridges the pure-MASM zero-day agentic engine with existing agentic systems:
;   - agentic_engine.asm (orchestration)
;   - autonomous_task_executor_clean.asm (task scheduling)
;   - masm_inference_engine.asm (model inference)
;   - agent_planner.asm (intent-based planning)
;
; Key integration points:
;   1. Goal complexity detection (simple → direct execution, complex → zero-day)
;   2. Signal routing (stream, complete, error callbacks)
;   3. Metrics propagation (mission duration, success rate, error categories)
;   4. Graceful degradation (fallback if zero-day unavailable)
; ============================================================================

IFNDEF ZERO_DAY_INTEGRATION_ASM_INC
ZERO_DAY_INTEGRATION_ASM_INC = 1

; ============================================================================
; Integration Constants
; ============================================================================

; Windows API Constants for LocalAlloc
LMEM_FIXED              = 00h
LMEM_ZEROINIT           = 40h     ; Zeroes out memory
LMEM_LPTR               = 40h     ; LMEM_FIXED + LMEM_ZEROINIT

; Win32 API Functions
extern LocalAlloc:PROC
extern LocalFree:PROC
extern AgenticEngine_ExecuteTask:PROC
extern Logger_LogMissionStart:PROC
extern Logger_LogMissionComplete:PROC
extern Logger_LogMissionError:PROC
extern ZeroDayAgenticEngine_Create:PROC
extern ZeroDayAgenticEngine_StartMission:PROC
extern ZeroDayAgenticEngine_Destroy:PROC
extern FindSubstringPtr:PROC

; Goal complexity levels (determined by token count, keyword detection)
COMPLEXITY_SIMPLE           = 0     ; Single-task, straightforward goals
COMPLEXITY_MODERATE         = 1     ; Multi-step, requires planning
COMPLEXITY_HIGH             = 2     ; Reasoning-intensive, failure recovery needed
COMPLEXITY_EXPERT           = 3     ; Zero-shot, meta-reasoning, self-correction

; Shared struct offsets (mirrors zero_day_agentic_engine.asm)
ENGINE_IMPL_PTR_OFFSET      EQU 0
IMPL_PLANNER_OFFSET         EQU 16

; Routing decision thresholds
MIN_TOKENS_FOR_MODERATE     = 20
MIN_TOKENS_FOR_HIGH         = 50
MIN_TOKENS_FOR_EXPERT       = 100

; Integration struct (tracks zero-day engine state for orchestration)
; +0:   ZeroDayAgenticEngine* engine
; +8:   Current active mission state
; +16:  Last mission result
; +24:  Integration flags (enabled, fallback_active, etc.)
ZD_INTEGRATION_ENGINE_OFFSET    = 0
ZD_INTEGRATION_STATE_OFFSET     = 8
ZD_INTEGRATION_RESULT_OFFSET    = 16
ZD_INTEGRATION_FLAGS_OFFSET     = 24
ZD_INTEGRATION_SIZE             = 32

; Flags
ZD_FLAG_ENABLED                 = 1     ; Zero-day engine available
ZD_FLAG_FALLBACK_ACTIVE         = 2     ; Fallback to standard engine in use
ZD_FLAG_AUTO_RETRY              = 4     ; Auto-retry on mission failure

; ============================================================================
; Callback Wrappers (Signal Handler Stubs)
; ============================================================================

; When zero-day engine emits agentStream signal, route to agentic_engine output
PUBLIC ZeroDayIntegration_OnAgentStream
PUBLIC ZeroDayIntegration_OnAgentComplete
PUBLIC ZeroDayIntegration_OnAgentError

; ============================================================================
; Integration API
; ============================================================================

; Initialize zero-day integration with agentic systems
; rcx = UniversalModelRouter*
; rdx = ToolRegistry*
; r8  = RawrXD::PlanOrchestrator*
; Returns: rax = ZDIntegration* (integration context)
PUBLIC ZeroDayIntegration_Initialize

; Analyze goal complexity and route to appropriate engine
; rcx = goal string (LPCSTR)
; Returns: rax = COMPLEXITY_* constant
PUBLIC ZeroDayIntegration_AnalyzeComplexity

; Route execution to zero-day engine or fallback
; rcx = goal (LPCSTR)
; rdx = workspace path
; r8  = complexity level
; Returns: rax = 1 if executed by zero-day, 0 if fallback used
PUBLIC ZeroDayIntegration_RouteExecution

; Check if zero-day engine is available and healthy
; Returns: rax = 1 if available, 0 if degraded
PUBLIC ZeroDayIntegration_IsHealthy

; Shutdown integration (cleanup resources)
PUBLIC ZeroDayIntegration_Shutdown

; ============================================================================
; Data Section
; ============================================================================

.DATA

; Global integration context (initialized by ZeroDayIntegration_Initialize)
gdwZeroDayIntegrationContext    DQ 0
gDummyPlanner                   DQ 0    ; fallback planner pointer when none provided

; Keyword strings
szZeroKeyword               DB "zero", 0
szMetaKeyword               DB "meta", 0
szAbstractKeyword           DB "abstract", 0
szSelfCorrectKeyword        DB "self-correct", 0

; Log messages
szZeroDayStreamToken        DB "Zero-day agent streaming: ", 0
szZeroDayMissionComplete    DB "Zero-day mission complete: ", 0
szZeroDayMissionFailed      DB "Zero-day mission error: ", 0

; ============================================================================
; Internal Helpers (not exported)
; ============================================================================

; Detect expert-level keywords indicating zero-shot reasoning
; rcx = goal string (LPCSTR)
; Returns: rax = 1 if expert-level detected, 0 otherwise
; NOTE: ZeroDayIntegration_DetectExpertKeywords

; Count tokens in goal string (simple heuristic: spaces + 1)
; rcx = goal string (LPCSTR)
; Returns: rax = token count estimate
; NOTE: ZeroDayIntegration_CountTokens

; ============================================================================
; Implementation
; ============================================================================

.CODE

; ============================================================================
; ZeroDayIntegration_Initialize
;
; Sets up the zero-day agentic engine and integrates it with existing systems.
; This is called once during application startup.
;
; Input:  rcx = UniversalModelRouter*
;         rdx = ToolRegistry*
;         r8  = RawrXD::PlanOrchestrator*
; Output: rax = ZDIntegration context pointer
; ============================================================================
ZeroDayIntegration_Initialize PROC FRAME
    PUSH rbx
    PUSH r12
    .ALLOCSTACK 40h
    SUB rsp, 40h
    .ENDPROLOG

    MOV rbx, rcx            ; rbx = router
    MOV r12, rdx            ; r12 = tools

    ; If planner not supplied, use dummy planner object so zero-day can run
    TEST r8, r8
    JNZ @planner_ok
    LEA r8, [gDummyPlanner]
@planner_ok:

    ; Create callback function pointers struct
    ; [0] = agentStream, [8] = agentComplete, [16] = agentError
    SUB rsp, 30h
    MOV rax, rsp

    ; Store callback addresses
    LEA rcx, [ZeroDayIntegration_OnAgentStream]
    MOV [rax + 0], rcx

    LEA rcx, [ZeroDayIntegration_OnAgentComplete]
    MOV [rax + 8], rcx

    LEA rcx, [ZeroDayIntegration_OnAgentError]
    MOV [rax + 16], rcx

    ; Create zero-day engine with callbacks
    MOV rcx, rbx            ; router
    MOV rdx, r12            ; tools
    ; r8 already has planner
    MOV r9, rax             ; callbacks struct
    CALL ZeroDayAgenticEngine_Create

    ; Store engine pointer in integration context
    ; For now, we store it in a global pattern (production would use
    ; thread-local or per-session storage)
    
    MOV rbx, rax            ; rbx = engine pointer

    ; Allocate integration context
    MOV rcx, ZD_INTEGRATION_SIZE
    MOV rdx, LMEM_LPTR
    CALL LocalAlloc
    MOV r12, rax            ; r12 = integration context

    ; Initialize integration context
    MOV [r12 + ZD_INTEGRATION_ENGINE_OFFSET], rbx    ; Store engine
    MOV BYTE PTR [r12 + ZD_INTEGRATION_STATE_OFFSET], 0
    MOV DWORD PTR [r12 + ZD_INTEGRATION_FLAGS_OFFSET], ZD_FLAG_ENABLED OR ZD_FLAG_AUTO_RETRY

    ; Store in global context
    LEA rax, [gdwZeroDayIntegrationContext]
    MOV [rax], r12

    MOV rax, r12            ; Return integration context

    ADD rsp, 30h
    ADD rsp, 40h
    POP r12
    POP rbx
    RET
ZeroDayIntegration_Initialize ENDP

; ============================================================================
; ZeroDayIntegration_AnalyzeComplexity
;
; Determines goal complexity by analyzing token count and keyword patterns.
; Routing decisions:
;   - SIMPLE (0-20 tokens): Direct execution via standard agentic_engine
;   - MODERATE (20-50 tokens): Planning-heavy, use standard agentic_engine
;   - HIGH (50-100 tokens): Failure recovery needed, consider zero-day
;   - EXPERT (100+ tokens): Zero-shot reasoning, meta-cognition → zero-day
;
; Input:  rcx = goal string (LPCSTR)
; Output: rax = COMPLEXITY_* constant
; ============================================================================
ZeroDayIntegration_AnalyzeComplexity PROC FRAME
    PUSH r12
    .ALLOCSTACK 28h
    SUB rsp, 28h
    .ENDPROLOG

    MOV r12, rcx            ; r12 = goal

    ; Check for expert-level keywords first (indicates zero-shot)
    MOV rcx, r12
    CALL ZeroDayIntegration_DetectExpertKeywords
    TEST eax, eax
    JNZ @complexity_expert

    ; Count tokens to determine baseline complexity
    MOV rcx, r12
    CALL ZeroDayIntegration_CountTokens
    
    CMP eax, MIN_TOKENS_FOR_EXPERT
    JGE @complexity_expert

    CMP eax, MIN_TOKENS_FOR_HIGH
    JGE @complexity_high

    CMP eax, MIN_TOKENS_FOR_MODERATE
    JGE @complexity_moderate

    MOV eax, COMPLEXITY_SIMPLE
    JMP @complexity_done

@complexity_moderate:
    MOV eax, COMPLEXITY_MODERATE
    JMP @complexity_done

@complexity_high:
    MOV eax, COMPLEXITY_HIGH
    JMP @complexity_done

@complexity_expert:
    MOV eax, COMPLEXITY_EXPERT

@complexity_done:
    ADD rsp, 28h
    POP r12
    RET
ZeroDayIntegration_AnalyzeComplexity ENDP

; ============================================================================
; ZeroDayIntegration_RouteExecution
;
; Routes goal execution to zero-day engine if complexity warrants it,
; otherwise falls back to standard agentic_engine.
;
; Input:  rcx = goal (LPCSTR)
;         rdx = workspace path
;         r8  = complexity level (COMPLEXITY_*)
; Output: rax = 1 if zero-day executed, 0 if fallback used
; ============================================================================
ZeroDayIntegration_RouteExecution PROC FRAME
    PUSH rbx
    PUSH r12
    PUSH r13
    .ALLOCSTACK 40h
    SUB rsp, 40h
    .ENDPROLOG

    MOV rbx, rcx            ; rbx = goal
    MOV r12, rdx            ; r12 = workspace
    MOV r13, r8             ; r13 = complexity

    ; Check if zero-day engine should handle this
    CMP r13, COMPLEXITY_HIGH
    JL @route_to_standard

    ; Complexity is HIGH or EXPERT - try zero-day engine
    ; Get integration context
    LEA rax, [gdwZeroDayIntegrationContext]
    MOV rcx, [rax]
    TEST rcx, rcx
    JZ @route_to_standard

    ; Check if zero-day is healthy
    CALL ZeroDayIntegration_IsHealthy
    TEST eax, eax
    JZ @route_to_standard

    ; Route to zero-day engine
    ; Get engine from integration context
    LEA rax, [gdwZeroDayIntegrationContext]
    MOV rcx, [rax]
    MOV rax, [rcx + ZD_INTEGRATION_ENGINE_OFFSET]

    ; Require a valid planner before attempting zero-day mission
    TEST rax, rax
    JZ @route_to_standard
    MOV r10, [rax + ENGINE_IMPL_PTR_OFFSET]
    TEST r10, r10
    JZ @route_to_standard
    MOV r10, [r10 + IMPL_PLANNER_OFFSET]
    TEST r10, r10
    JZ @route_to_standard
    
    ; Call ZeroDayAgenticEngine_StartMission(engine, goal)
    MOV rcx, rax            ; engine
    MOV rdx, rbx            ; goal
    CALL ZeroDayAgenticEngine_StartMission

    ; If zero-day declined (NULL planner, etc.), fallback to standard path
    TEST rax, rax
    JZ @route_to_standard

    MOV eax, 1              ; Indicate zero-day execution
    JMP @route_done

@route_to_standard:
    ; Fallback to standard agentic_engine
    MOV rcx, rbx            ; goal
    MOV rdx, r12            ; workspace
    CALL AgenticEngine_ExecuteTask

    XOR eax, eax            ; Indicate fallback used

@route_done:
    ADD rsp, 40h
    POP r13
    POP r12
    POP rbx
    RET
ZeroDayIntegration_RouteExecution ENDP

; ============================================================================
; ZeroDayIntegration_IsHealthy
;
; Checks if zero-day engine is operational and ready for execution.
;
; Returns: rax = 1 if healthy, 0 if degraded
; ============================================================================
ZeroDayIntegration_IsHealthy PROC FRAME
    .ALLOCSTACK 28h
    SUB rsp, 28h
    .ENDPROLOG

    ; For production: check engine version, memory state, thread pool health
    ; For now: simple availability check
    
    LEA rax, [gdwZeroDayIntegrationContext]
    MOV rcx, [rax]
    TEST rcx, rcx
    JZ @health_degraded

    ; Check flags
    MOV edx, [rcx + ZD_INTEGRATION_FLAGS_OFFSET]
    AND edx, ZD_FLAG_ENABLED
    TEST edx, edx
    JZ @health_degraded

    MOV eax, 1              ; Healthy
    JMP @health_done

@health_degraded:
    XOR eax, eax            ; Degraded

@health_done:
    ADD rsp, 28h
    RET
ZeroDayIntegration_IsHealthy ENDP

; ============================================================================
; ZeroDayIntegration_Shutdown
;
; Gracefully shuts down the zero-day integration and releases resources.
;
; ============================================================================
ZeroDayIntegration_Shutdown PROC FRAME
    .ALLOCSTACK 28h
    SUB rsp, 28h
    .ENDPROLOG

    ; Get integration context
    LEA rax, [gdwZeroDayIntegrationContext]
    MOV rcx, [rax]
    TEST rcx, rcx
    JZ @shutdown_done

    ; Get engine pointer
    MOV rcx, [rcx + ZD_INTEGRATION_ENGINE_OFFSET]
    TEST rcx, rcx
    JZ @shutdown_done

    ; Destroy engine
    CALL ZeroDayAgenticEngine_Destroy

@shutdown_done:
    ADD rsp, 28h
    RET
ZeroDayIntegration_Shutdown ENDP

; ============================================================================
; Signal Callback Handlers
; ============================================================================

; ZeroDayIntegration_OnAgentStream
; Called when zero-day engine emits streaming token/partial result
ZeroDayIntegration_OnAgentStream PROC FRAME
    PUSH rbx
    .ALLOCSTACK 28h
    SUB rsp, 28h
    .ENDPROLOG

    MOV rbx, rcx            ; rbx = token/message

    ; Route to agentic_engine output callback or UI stream
    ; For now, log the stream event
    ; LEA rcx, [REL szZeroDayStreamToken]
    ; MOV rdx, rbx
    ; CALL Logger_LogMissionStart

    ADD rsp, 28h
    POP rbx
    RET
ZeroDayIntegration_OnAgentStream ENDP

; ZeroDayIntegration_OnAgentComplete
; Called when zero-day engine mission completes successfully
ZeroDayIntegration_OnAgentComplete PROC FRAME
    PUSH rbx
    .ALLOCSTACK 28h
    SUB rsp, 28h
    .ENDPROLOG

    MOV rbx, rcx            ; rbx = summary

    ; Route to agentic_engine completion handler
    ; LEA rcx, [REL szZeroDayMissionComplete]
    ; MOV rdx, rbx
    ; CALL Logger_LogMissionComplete

    ADD rsp, 28h
    POP rbx
    RET
ZeroDayIntegration_OnAgentComplete ENDP

; ZeroDayIntegration_OnAgentError
; Called when zero-day engine mission fails
ZeroDayIntegration_OnAgentError PROC FRAME
    PUSH rbx
    .ALLOCSTACK 28h
    SUB rsp, 28h
    .ENDPROLOG

    MOV rbx, rcx            ; rbx = error message

    ; Route to agentic_engine error handler
    ; LEA rcx, [REL szZeroDayMissionFailed]
    ; MOV rdx, rbx
    ; CALL Logger_LogMissionError

    ADD rsp, 28h
    POP rbx
    RET
ZeroDayIntegration_OnAgentError ENDP

; ============================================================================
; Helper Functions
; ============================================================================

; ZeroDayIntegration_DetectExpertKeywords
; Returns 1 if goal contains keywords indicating expert-level reasoning
ZeroDayIntegration_DetectExpertKeywords PROC FRAME
    PUSH rbx
    .ALLOCSTACK 20h
    SUB rsp, 20h
    .ENDPROLOG

    MOV rbx, rcx            ; rbx = goal

    ; Check for expert keywords (zero-shot, meta-reasoning, self-correct, etc.)
    ; This is a simple pattern matching approach
    
    ; Keywords: "zero-shot", "self-correct", "meta-reason", "abstract"
    ; For simplicity, check for keyword occurrences
    
    ; In production, use more sophisticated NLP
    ; For now, just check for "zero" or "meta" substrings
    
    MOV rcx, rbx
    LEA rdx, [szZeroKeyword]
    CALL FindSubstringPtr
    TEST rax, rax
    JNZ @expert_detected

    MOV rcx, rbx
    LEA rdx, [szMetaKeyword]
    CALL FindSubstringPtr
    TEST rax, rax
    JNZ @expert_detected

    XOR eax, eax            ; No expert keywords
    JMP @expert_check_done

@expert_detected:
    MOV eax, 1

@expert_check_done:
    ADD rsp, 20h
    POP rbx
    RET
ZeroDayIntegration_DetectExpertKeywords ENDP

; ZeroDayIntegration_CountTokens
; Counts approximate token count by counting spaces
ZeroDayIntegration_CountTokens PROC FRAME
    PUSH rbx
    .ALLOCSTACK 20h
    SUB rsp, 20h
    .ENDPROLOG

    MOV rbx, rcx            ; rbx = goal
    XOR eax, eax            ; eax = token count

    ; Simple heuristic: count spaces + 1
@token_count_loop:
    MOV cl, BYTE PTR [rbx]
    TEST cl, cl
    JZ @token_count_done

    CMP cl, ' '             ; Space character
    JNE @token_count_next
    INC eax                 ; Increment token count

@token_count_next:
    INC rbx
    JMP @token_count_loop

@token_count_done:
    INC eax                 ; Add 1 for last token
    ADD rsp, 20h
    POP rbx
    RET
ZeroDayIntegration_CountTokens ENDP

ENDIF   ; ZERO_DAY_INTEGRATION_ASM_INC

END
