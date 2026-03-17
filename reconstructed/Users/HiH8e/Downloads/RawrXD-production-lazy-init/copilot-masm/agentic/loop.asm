;======================================================================
; agentic_loop.asm - Agentic Recursion & Planning Engine
;======================================================================
INCLUDE windows.inc
INCLUDE bcrypt.inc

.CONST
MAX_PLAN_STEPS      EQU 10
AGENT_STATE_PLANNING EQU 1
AGENT_STATE_EXECUTING EQU 2
AGENT_STATE_REVIEWING EQU 3

.DATA?
g_agentState        DWORD ?
g_currentPlan       QWORD ?
g_planStepIndex     DWORD ?
g_toolCallCount     DWORD ?
g_maxToolCalls      DWORD 50
g_lastResult        QWORD ?
; Error and format strings
ERR_TASK_NULL       DB '{"error": "Task is null"}',0
ERR_GEN_PLAN        DB '{"error": "Failed to generate plan"}',0
ERR_MAX_TOOL_CALLS  DB '{"error": "Max tool calls exceeded"}',0

AGENT_PROMPT_FMT    DB "You are an expert software development assistant. Create a detailed execution plan for this task:\n\nTask: %s\n\nRequirements:\n1. Break down into logical steps\n2. Each step must be tool-callable\n3. Include file paths and code locations\n4. Specify verification criteria\n\nReturn JSON array of steps:\n[\n  {\n    \"step\": 1,\n    \"action\": \"read_file|write_file|execute|compile|test\",\n    \"target\": \"file_path\",\n    \"description\": \"...\",\n    \"verification\": \"...\"\n  }\n]",0

CORRECTION_FMT      DB "Step %d failed. Error: %s\n\nAnalyze the error and provide a corrected plan step.",0

REVIEW_FMT          DB "Review the completed task execution:\n\nPlan: %s\n\nResult: %s\n\nVerify:\n1. All requirements met\n2. No errors or issues\n3. Code quality acceptable\n4. Tests pass (if applicable)\n\nProvide final verification status.",0
PLANNING_STR        DB "planning",0
REVIEW_STR          DB "review",0

.CODE

AgenticLoop_Init PROC
    mov g_agentState, AGENT_STATE_PLANNING
    mov g_planStepIndex, 0
    mov g_toolCallCount, 0
    xor rax, rax
    mov g_currentPlan, rax
    ret
AgenticLoop_Init ENDP

AgenticLoop_ExecuteTask PROC pTask:QWORD
    LOCAL pContext:QWORD
    LOCAL pPlan:QWORD
    
    ; Validate task
    .if pTask == 0
        lea rax, ERR_TASK_NULL
        ret
    .endif
    
    ; 1. PLANNING PHASE: Generate execution plan
    invoke AgenticLoop_GeneratePlan, pTask
    mov pPlan, rax
    
    .if rax == 0
        lea rax, ERR_GEN_PLAN
        ret
    .endif
    
    mov g_currentPlan, pPlan
    
    ; 2. EXECUTING PHASE: Execute each step
    mov ecx, MAX_PLAN_STEPS
    .repeat
        ; Get current step
        invoke Plan_GetStep, pPlan, g_planStepIndex
        .if rax == 0
            .break
        .endif
        
        ; Execute step
        invoke AgenticLoop_ExecuteStep, rax
        mov g_lastResult, rax
        
        ; Check if step completed successfully
        invoke JsonExtractBool, rax, "success"
        .if rax == 0
            ; Step failed, enter review/correction loop
            invoke AgenticLoop_HandleStepFailure, g_planStepIndex, g_lastResult
        .endif
        
        inc g_planStepIndex
        
        ; Check tool call limit (prevent infinite loops)
        inc g_toolCallCount
        .if g_toolCallCount > g_maxToolCalls
            lea rax, ERR_MAX_TOOL_CALLS
            ret
        .endif
        
    .untilcxz
    
    ; 3. REVIEWING PHASE: Review final result
    invoke AgenticLoop_ReviewResult, pPlan, g_lastResult
    
    ret
AgenticLoop_ExecuteTask ENDP

AgenticLoop_GeneratePlan PROC pTask:QWORD
    LOCAL pPrompt:QWORD
    LOCAL pContext:QWORD
    
    ; Build planning prompt
    invoke GetProcessHeap
    mov rcx, rax
    mov rdx, HEAP_ZERO_MEMORY
    mov r8d, 32768            ; 32 KB prompt
    call HeapAlloc
    mov pPrompt, rax
    
        lea rcx, pPrompt
        lea rdx, AGENT_PROMPT_FMT
        mov r8, pTask
        call wsprintfA
    
    ; Call model to generate plan
    mov rcx, pPrompt
    lea rdx, PLANNING_STR
    mov r8d, 1000
    call ModelRouter_CallModel
    
    ; Parse JSON plan
    invoke JsonParseArray, rax
    ret
AgenticLoop_GeneratePlan ENDP

AgenticLoop_ExecuteStep PROC pStep:QWORD
    LOCAL action:QWORD
    LOCAL target:QWORD
    LOCAL pResult:QWORD
    
    ; Extract action and target
    invoke JsonExtractString, pStep, "action", ADDR actionBuffer
    invoke JsonExtractString, pStep, "target", ADDR targetBuffer
    
    ; Dispatch to appropriate tool
    invoke lstrcmpA, ADDR actionBuffer, "read_file"
    .if eax == 0
        invoke Tool_FileRead, ADDR targetBuffer
        mov pResult, rax
    .endif
    
    invoke lstrcmpA, ADDR actionBuffer, "write_file"
    .if eax == 0
        invoke Tool_FileWrite, ADDR payloadBuffer
        mov pResult, rax
    .endif
    
    invoke lstrcmpA, ADDR actionBuffer, "execute"
    .if eax == 0
        invoke Tool_ExecuteCommand, ADDR commandBuffer
        mov pResult, rax
    .endif
    
    invoke lstrcmpA, ADDR actionBuffer, "compile"
    .if eax == 0
        invoke Tool_CompileProject, ADDR compileParams
        mov pResult, rax
    .endif
    
    ; Verify step result
    invoke AgenticLoop_VerifyStep, pStep, pResult
    
    ret
AgenticLoop_ExecuteStep ENDP

AgenticLoop_HandleStepFailure PROC stepIndex:DWORD, pFailureResult:QWORD
    LOCAL pCorrectionPrompt:QWORD
    
    ; Build correction prompt
    invoke GetProcessHeap
    mov rcx, rax
    mov rdx, HEAP_ZERO_MEMORY
    mov r8d, 16384
    call HeapAlloc
    mov pCorrectionPrompt, rax
    
    lea rcx, pCorrectionPrompt
    lea rdx, CORRECTION_FMT
    mov edx, stepIndex
    mov r8, pFailureResult
    call wsprintfA
    
    ; Retry with corrected plan
    invoke AgenticLoop_ExecuteTask, pCorrectionPrompt
    
    ret
AgenticLoop_HandleStepFailure ENDP

AgenticLoop_ReviewResult PROC pPlan:QWORD, pFinalResult:QWORD
    LOCAL pReviewPrompt:QWORD
    
    ; Build review prompt
    invoke GetProcessHeap
    mov rcx, rax
    mov rdx, HEAP_ZERO_MEMORY
    mov r8d, 8192
    call HeapAlloc
    mov pReviewPrompt, rax
    
    lea rcx, pReviewPrompt
    lea rdx, REVIEW_FMT
    mov r8, pPlan
    mov r9, pFinalResult
    call wsprintfA
    
    ; Call model for review
    mov rcx, pReviewPrompt
    lea rdx, REVIEW_STR
    mov r8d, 500
    call ModelRouter_CallModel
    
    ret
AgenticLoop_ReviewResult ENDP

END