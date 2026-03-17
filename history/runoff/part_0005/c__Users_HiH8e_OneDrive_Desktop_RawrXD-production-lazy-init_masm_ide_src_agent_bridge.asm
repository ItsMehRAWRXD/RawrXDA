; ============================================================================
; RawrXD Agentic IDE - IDEAgentBridge (Orchestrator) Implementation
; Pure MASM - Full Agentic Orchestration
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

.data
include constants.inc
include structures.inc
include macros.inc; ============================================================================
; External declarations
; ============================================================================

extern ModelInvoker_Init:proc
extern ModelInvoker_Invoke:proc
extern ModelInvoker_SetEndpoint:proc
extern ActionExecutor_Init:proc
extern ActionExecutor_ExecutePlan:proc
extern ActionExecutor_SetProjectRoot:proc

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    g_pModelInvoker     dd 0
    g_pActionExecutor   dd 0
    g_szProjectRoot     db MAX_PATH_SIZE dup(0)
    g_bDryRunMode       dd 0
    g_bRequireApproval  dd 1
    g_bExecutionInProgress dd 0
    
    ; Execution history
    g_pHistory          dd 0
    g_dwHistoryCount    dd 0
    g_dwMaxHistory      dd 100
    
    ; Callbacks
    g_pfnThinkingStarted dd 0
    g_pfnGeneratedPlan  dd 0
    g_pfnApprovalNeeded dd 0
    g_pfnExecutionStarted dd 0
    g_pfnExecutionProgress dd 0
    g_pfnProgressUpdated dd 0
    g_pfnCompleted      dd 0
    g_pfnError          dd 0
    g_pfnInputRequested dd 0

.data?
    g_hMutex            dd ?
    g_CurrentPlan       EXECUTION_PLAN <>
    g_qwExecutionStart   dq ?

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; IDEAgentBridge_Init - Initialize the agent bridge
; Returns: Handle in eax
; ============================================================================
IDEAgentBridge_Init proc
    LOCAL hInvoker:DWORD
    LOCAL hExecutor:DWORD
    
    ; Create mutex
    invoke CreateMutex, NULL, FALSE, NULL
    mov g_hMutex, eax
    
    ; Initialize ModelInvoker
    invoke ModelInvoker_Init
    mov g_pModelInvoker, eax
    mov hInvoker, eax
    
    ; Initialize ActionExecutor
    invoke ActionExecutor_Init
    mov g_pActionExecutor, eax
    mov hExecutor, eax
    
    ; Allocate history buffer
    mov eax, g_dwMaxHistory
    mov ecx, sizeof LOOP_ITERATION
    imul eax, ecx
    MemAlloc eax
    mov g_pHistory, eax
    
    mov eax, 1  ; Success
    ret
IDEAgentBridge_Init endp

; ============================================================================
; IDEAgentBridge_ExecuteWish - Main entry point for agentic execution
; Input: pszWish, bRequireApproval
; ============================================================================
IDEAgentBridge_ExecuteWish proc pszWish:DWORD, bRequireApproval:DWORD
    LOCAL hPlan:DWORD
    LOCAL hThread:DWORD
    
    ; Lock
    invoke WaitForSingleObject, g_hMutex, INFINITE
    
    ; Check if already executing
    .if g_bExecutionInProgress
        invoke ReleaseMutex, g_hMutex
        ret
    .endif
    
    mov g_bExecutionInProgress, 1
    mov eax, bRequireApproval
    mov g_bRequireApproval, eax
    
    ; Emit thinking started
    .if g_pfnThinkingStarted != 0
        push pszWish
        call g_pfnThinkingStarted
    .endif
    
    ; Invoke model in thread
    invoke CreateThread, NULL, 0, offset WishWorkerThread, pszWish, 0, NULL
    mov hThread, eax
    
    .if hThread != 0
        invoke CloseHandle, hThread
    .endif
    
    invoke ReleaseMutex, g_hMutex
    
    ret
IDEAgentBridge_ExecuteWish endp

; ============================================================================
; WishWorkerThread - Worker thread for wish processing
; ============================================================================
WishWorkerThread proc pszWish:DWORD
    LOCAL pPlan:DWORD
    
    ; Invoke model
    invoke ModelInvoker_Invoke, pszWish
    mov pPlan, eax
    
    .if pPlan != 0
        ; Store current plan
        invoke RtlCopyMemory, addr g_CurrentPlan, pPlan, sizeof EXECUTION_PLAN
        
        ; Emit plan generated
        .if g_pfnGeneratedPlan != 0
            push pPlan
            call g_pfnGeneratedPlan
        .endif
        
        ; Check if approval needed
        .if g_bRequireApproval
            .if g_pfnApprovalNeeded != 0
                push pPlan
                call g_pfnApprovalNeeded
            .endif
        .else
            ; Auto-approve and execute
            call IDEAgentBridge_ApprovePlanInternal
        .endif
    .else
        ; Error during plan generation
        .if g_pfnError != 0
            push offset szPlanGenError
            push 0  ; Not recoverable
            call g_pfnError
        .endif
    .endif
    
    xor eax, eax
    ret
WishWorkerThread endp

; ============================================================================
; IDEAgentBridge_ApprovePlan - User approves plan
; ============================================================================
IDEAgentBridge_ApprovePlan proc
    invoke WaitForSingleObject, g_hMutex, INFINITE
    
    call IDEAgentBridge_ApprovePlanInternal
    
    invoke ReleaseMutex, g_hMutex
    ret
IDEAgentBridge_ApprovePlan endp

; ============================================================================
; IDEAgentBridge_ApprovePlanInternal - Internal approval
; ============================================================================
IDEAgentBridge_ApprovePlanInternal proc
    LOCAL hThread:DWORD
    LOCAL qwStart:QWORD
    
    ; Get start time
    invoke GetTickCount
    mov qword ptr g_qwExecutionStart, eax
    
    ; Emit execution started
    mov eax, g_CurrentPlan.dwActionCount
    .if g_pfnExecutionStarted != 0
        push eax
        call g_pfnExecutionStarted
    .endif
    
    ; Execute plan in worker thread
    invoke CreateThread, NULL, 0, offset ExecutionWorkerThread, 
        offset g_CurrentPlan, 0, NULL
    mov hThread, eax
    
    .if hThread != 0
        invoke CloseHandle, hThread
    .endif
    
    ret
IDEAgentBridge_ApprovePlanInternal endp

; ============================================================================
; ExecutionWorkerThread - Worker thread for plan execution
; ============================================================================
ExecutionWorkerThread proc pPlan:DWORD
    LOCAL pResult:DWORD
    LOCAL qwEnd:QWORD
    LOCAL dwDuration:DWORD
    
    ; Execute plan
    invoke ActionExecutor_ExecutePlan, pPlan
    mov pResult, eax
    
    ; Get end time
    invoke GetTickCount
    mov qword ptr qwEnd, eax
    
    ; Calculate duration
    mov eax, qword ptr qwEnd
    sub eax, qword ptr g_qwExecutionStart
    mov dwDuration, eax
    
    ; Emit completion
    .if g_pfnCompleted != 0
        push dwDuration
        push offset g_CurrentPlan.szPlanID
        call g_pfnCompleted
    .endif
    
    ; Add to history
    call AddToExecutionHistory
    
    ; Set execution complete
    mov g_bExecutionInProgress, 0
    
    xor eax, eax
    ret
ExecutionWorkerThread endp

; ============================================================================
; IDEAgentBridge_RejectPlan - User rejects plan
; ============================================================================
IDEAgentBridge_RejectPlan proc
    invoke WaitForSingleObject, g_hMutex, INFINITE
    
    ; Clear current plan
    MemZero addr g_CurrentPlan, sizeof EXECUTION_PLAN
    
    ; Emit error
    .if g_pfnError != 0
        push offset szPlanRejected
        push 0
        call g_pfnError
    .endif
    
    invoke ReleaseMutex, g_hMutex
    ret
IDEAgentBridge_RejectPlan endp

; ============================================================================
; IDEAgentBridge_CancelExecution - Cancel execution
; ============================================================================
IDEAgentBridge_CancelExecution proc
    ; Would call ActionExecutor_Cancel here
    mov g_bExecutionInProgress, 0
    ret
IDEAgentBridge_CancelExecution endp

; ============================================================================
; IDEAgentBridge_GetExecutionHistory - Get execution history
; Input: dwLimit
; Returns: Pointer to history array in eax, count in ecx
; ============================================================================
IDEAgentBridge_GetExecutionHistory proc dwLimit:DWORD
    mov eax, g_pHistory
    mov ecx, g_dwHistoryCount
    
    .if dwLimit != 0 && dwLimit < ecx
        mov ecx, dwLimit
    .endif
    
    ret
IDEAgentBridge_GetExecutionHistory endp

; ============================================================================
; IDEAgentBridge_ClearExecutionHistory - Clear history
; ============================================================================
IDEAgentBridge_ClearExecutionHistory proc
    MemZero g_pHistory, g_dwMaxHistory * sizeof LOOP_ITERATION
    mov g_dwHistoryCount, 0
    ret
IDEAgentBridge_ClearExecutionHistory endp

; ============================================================================
; AddToExecutionHistory - Add entry to execution history
; ============================================================================
AddToExecutionHistory proc
    LOCAL pEntry:DWORD
    LOCAL dwOffset:DWORD
    
    .if g_dwHistoryCount >= g_dwMaxHistory
        ret
    .endif
    
    ; Calculate offset
    mov eax, g_dwHistoryCount
    mov ecx, sizeof LOOP_ITERATION
    imul eax, ecx
    add eax, g_pHistory
    mov pEntry, eax
    
    ; Copy current plan to history
    invoke RtlCopyMemory, pEntry, addr g_CurrentPlan, sizeof EXECUTION_PLAN
    
    inc g_dwHistoryCount
    ret
AddToExecutionHistory endp

; ============================================================================
; IDEAgentBridge_SetProjectRoot - Set project root
; Input: pszRoot
; ============================================================================
IDEAgentBridge_SetProjectRoot proc pszRoot:DWORD
    szCopy addr g_szProjectRoot, pszRoot
    invoke ActionExecutor_SetProjectRoot, pszRoot
    ret
IDEAgentBridge_SetProjectRoot endp

; ============================================================================
; IDEAgentBridge_SetDryRunMode - Set dry-run mode
; Input: bEnable
; ============================================================================
IDEAgentBridge_SetDryRunMode proc bEnable:DWORD
    mov eax, bEnable
    mov g_bDryRunMode, eax
    ret
IDEAgentBridge_SetDryRunMode endp

; ============================================================================
; IDEAgentBridge_SetModel - Set LLM model
; Input: pszModel
; ============================================================================
IDEAgentBridge_SetModel proc pszModel:DWORD
    ; Would call ModelInvoker_SetModel here
    ret
IDEAgentBridge_SetModel endp

; ============================================================================
; Callback setters
; ============================================================================

IDEAgentBridge_SetThinkingStartedCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnThinkingStarted, eax
    ret
IDEAgentBridge_SetThinkingStartedCallback endp

IDEAgentBridge_SetGeneratedPlanCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnGeneratedPlan, eax
    ret
IDEAgentBridge_SetGeneratedPlanCallback endp

IDEAgentBridge_SetApprovalNeededCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnApprovalNeeded, eax
    ret
IDEAgentBridge_SetApprovalNeededCallback endp

IDEAgentBridge_SetExecutionStartedCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnExecutionStarted, eax
    ret
IDEAgentBridge_SetExecutionStartedCallback endp

IDEAgentBridge_SetExecutionProgressCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnExecutionProgress, eax
    ret
IDEAgentBridge_SetExecutionProgressCallback endp

IDEAgentBridge_SetProgressUpdatedCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnProgressUpdated, eax
    ret
IDEAgentBridge_SetProgressUpdatedCallback endp

IDEAgentBridge_SetCompletedCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnCompleted, eax
    ret
IDEAgentBridge_SetCompletedCallback endp

IDEAgentBridge_SetErrorCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnError, eax
    ret
IDEAgentBridge_SetErrorCallback endp

IDEAgentBridge_SetUserInputRequestedCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnInputRequested, eax
    ret
IDEAgentBridge_SetUserInputRequestedCallback endp

; ============================================================================
; Cleanup
; ============================================================================

IDEAgentBridge_Cleanup proc
    .if g_pHistory != 0
        MemFree g_pHistory
        mov g_pHistory, 0
    .endif
    
    .if g_hMutex != 0
        invoke CloseHandle, g_hMutex
        mov g_hMutex, 0
    .endif
    
    ret
IDEAgentBridge_Cleanup endp

; ============================================================================
; Data
; ============================================================================

.data
    szPlanGenError      db "Failed to generate plan", 0
    szPlanRejected      db "Plan rejected by user", 0

end
