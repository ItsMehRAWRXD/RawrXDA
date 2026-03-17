; ============================================================================
; RawrXD Agentic IDE - Autonomous Loop Engine Implementation
; Pure MASM - Full Plan-Execute-Verify-Reflect Cycle
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include ..\include\winapi_min.inc

 .data
include constants.inc
include structures.inc
include macros.inc

; ============================================================================
; External declarations
; ============================================================================

extern ModelInvoker_Invoke:proc
extern ActionExecutor_ExecutePlan:proc

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    g_pModelInvoker     dd 0
    g_pActionExecutor   dd 0
    g_dwMaxIterations   dd 10
    g_szProjectRoot     db MAX_PATH_SIZE dup(0)
    g_szModel           db "llama2", 0
    g_bCancelled        dd 0
    g_bPaused           dd 0
    
    ; History
    g_pHistory          dd 0
    g_dwHistoryCount    dd 0
    
    ; Callbacks
    g_pfnIteration      dd 0
    g_pfnStage          dd 0
    g_pfnCompletion     dd 0
    
    ; Stage names
    szStagePlanning     db "Planning", 0
    szStageExecution    db "Execution", 0
    szStageVerification db "Verification", 0
    szStageReflection   db "Reflection", 0
    szStageAdaptation   db "Adaptation", 0
    szStageCompleted    db "Completed", 0
    szStageFailed       db "Failed", 0

.data?
    g_hMutex            dd ?

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; LoopEngine_Init - Initialize autonomous loop engine
; Returns: Handle in eax
; ============================================================================
LoopEngine_Init proc
    ; Create mutex
    invoke CreateMutex, NULL, FALSE, NULL
    mov g_hMutex, eax
    
    ; Allocate history
    mov eax, MAX_ITERATION_COUNT
    mov ecx, sizeof LOOP_ITERATION
    imul eax, ecx
    MemAlloc eax
    mov g_pHistory, eax
    
    mov eax, 1  ; Success
    ret
LoopEngine_Init endp

; ============================================================================
; LoopEngine_RunAutonomousLoop - Run full autonomous loop
; Input: pszObjective
; Returns: History pointer in eax, count in ecx
; ============================================================================
LoopEngine_RunAutonomousLoop proc pszObjective:DWORD
    LOCAL i:DWORD
    LOCAL szCurrentObjective db 512 dup(0)
    LOCAL pPlan:DWORD
    LOCAL pResult:DWORD
    LOCAL pIteration:DWORD
    LOCAL bVerified:DWORD
    LOCAL szReflection db 1024 dup(0)
    
    invoke WaitForSingleObject, g_hMutex, INFINITE
    
    mov g_bCancelled, 0
    mov g_dwHistoryCount, 0
    
    szCopy addr szCurrentObjective, pszObjective
    
    ; Main loop
    mov i, 0
    @@MainLoop:
        .if i >= g_dwMaxIterations
            jmp @LoopComplete
        .endif
        
        .if g_bCancelled
            jmp @LoopComplete
        .endif
        
        ; Wait if paused
        @@PauseWait:
            .if !g_bPaused
                jmp @ResumePause
            .endif
            invoke Sleep, 100
            jmp @PauseWait
        
        @ResumePause:
        
        ; Allocate iteration structure
        MemAlloc sizeof LOOP_ITERATION
        mov pIteration, eax
        
        mov ecx, pIteration
        assume ecx:ptr LOOP_ITERATION
        mov LOOP_ITERATION ptr [ecx].dwIteration, i
        assume ecx:nothing
        
        ; ============================================
        ; Stage 1: PLANNING
        ; ============================================
        
        mov ecx, pIteration
        assume ecx:ptr LOOP_ITERATION
        mov LOOP_ITERATION ptr [ecx].dwStage, 0  ; PLANNING
        assume ecx:nothing
        
        ; Emit stage callback
        .if g_pfnStage != 0
            push offset szStagePlanning
            push 0
            call g_pfnStage
        .endif
        
        ; Generate plan
        invoke ModelInvoker_Invoke, addr szCurrentObjective
        mov pPlan, eax
        
        .if pPlan == 0
            inc i
            jmp @MainLoop
        .endif
        
        ; Copy plan to iteration
        mov ecx, pIteration
        invoke RtlCopyMemory, ecx + offset LOOP_ITERATION.planData, pPlan, sizeof EXECUTION_PLAN
        
        ; ============================================
        ; Stage 2: EXECUTION
        ; ============================================
        
        mov ecx, pIteration
        assume ecx:ptr LOOP_ITERATION
        mov LOOP_ITERATION ptr [ecx].dwStage, 1  ; EXECUTION
        assume ecx:nothing
        
        .if g_pfnStage != 0
            push offset szStageExecution
            push 1
            call g_pfnStage
        .endif
        
        ; Execute plan
        invoke ActionExecutor_ExecutePlan, pPlan
        mov pResult, eax
        
        mov ecx, pIteration
        invoke RtlCopyMemory, ecx + offset LOOP_ITERATION.resultData, pResult, sizeof PLAN_RESULT
        
        ; ============================================
        ; Stage 3: VERIFICATION
        ; ============================================
        
        mov ecx, pIteration
        assume ecx:ptr LOOP_ITERATION
        mov LOOP_ITERATION ptr [ecx].dwStage, 2  ; VERIFICATION
        assume ecx:nothing
        
        .if g_pfnStage != 0
            push offset szStageVerification
            push 2
            call g_pfnStage
        .endif
        
        ; Check if all actions succeeded
        call VerifyResults
        mov bVerified, eax
        
        ; ============================================
        ; Stage 4: REFLECTION
        ; ============================================
        
        mov ecx, pIteration
        assume ecx:ptr LOOP_ITERATION
        mov LOOP_ITERATION ptr [ecx].dwStage, 3  ; REFLECTION
        assume ecx:nothing
        
        .if g_pfnStage != 0
            push offset szStageReflection
            push 3
            call g_pfnStage
        .endif
        
        ; Analyze results
        call ReflectOnResults
        mov ecx, pIteration
        lea edx, [ecx + offset LOOP_ITERATION.szReflection]
        szCopy edx, eax
        
        ; ============================================
        ; Stage 5: ADAPTATION
        ; ============================================
        
        mov ecx, pIteration
        assume ecx:ptr LOOP_ITERATION
        mov LOOP_ITERATION ptr [ecx].dwStage, 4  ; ADAPTATION
        assume ecx:nothing
        
        .if g_pfnStage != 0
            push offset szStageAdaptation
            push 4
            call g_pfnStage
        .endif
        
        ; Plan next iteration
        call AdaptForNextIteration
        szCopy addr szCurrentObjective, eax
        
        ; Check for completion
        .if bVerified && [ecx].resultData.bSuccess
            mov LOOP_ITERATION ptr [ecx].dwStage, 5  ; COMPLETED
            mov LOOP_ITERATION ptr [ecx].bSuccess, 1
            
            .if g_pfnStage != 0
                push offset szStageCompleted
                push 5
                call g_pfnStage
            .endif
            
            ; Add to history
            call AddIterationToHistory
            
            ; Call completion callback
            .if g_pfnCompletion != 0
                push offset szObjectiveAchieved
                push 1
                call g_pfnCompletion
            .endif
            
            jmp @LoopComplete
        .endif
        
        ; Add to history
        call AddIterationToHistory
        
        ; Emit iteration callback
        .if g_pfnIteration != 0
            push pIteration
            call g_pfnIteration
        .endif
        
        inc i
        jmp @MainLoop
    
    @LoopComplete:
    
    .if g_bCancelled
        .if g_pfnCompletion != 0
            push offset szLoopCancelled
            push 0
            call g_pfnCompletion
        .endif
    .elseif g_dwHistoryCount >= g_dwMaxIterations
        .if g_pfnCompletion != 0
            push offset szMaxIterations
            push 0
            call g_pfnCompletion
        .endif
    .endif
    
    invoke ReleaseMutex, g_hMutex
    
    mov eax, g_pHistory
    mov ecx, g_dwHistoryCount
    ret
LoopEngine_RunAutonomousLoop endp

; ============================================================================
; VerifyResults - Verify execution results
; Returns: 1 if verified, 0 if not
; ============================================================================
VerifyResults proc
    LOCAL pResult:DWORD
    
    mov eax, pIteration
    assume eax:ptr LOOP_ITERATION
    
    ; Check if all actions were successful
    mov ecx, [eax + offset LOOP_ITERATION.resultData].dwFailureCount
    assume eax:nothing
    
    .if ecx == 0
        mov eax, 1
    .else
        xor eax, eax
    .endif
    
    ret
VerifyResults endp

; ============================================================================
; ReflectOnResults - Generate reflection on iteration results
; Returns: Pointer to reflection string in eax
; ============================================================================
ReflectOnResults proc
    LOCAL szReflection db 1024 dup(0)
    
    mov eax, pIteration
    assume eax:ptr LOOP_ITERATION
    
    ; Build reflection string
    invoke wsprintfA, addr szReflection, addr szReflectionFormat,
        LOOP_ITERATION ptr [eax].dwIteration,
        [eax + offset LOOP_ITERATION.resultData].dwSuccessCount,
        [eax + offset LOOP_ITERATION.resultData].dwFailureCount
    
    assume eax:nothing
    
    lea eax, szReflection
    ret
ReflectOnResults endp

; ============================================================================
; AdaptForNextIteration - Plan next iteration based on reflection
; Returns: Pointer to next objective in eax
; ============================================================================
AdaptForNextIteration proc
    LOCAL szNextObjective db 512 dup(0)
    
    mov eax, pIteration
    assume eax:ptr LOOP_ITERATION
    
    ; Simple adaptation: if failure, try to fix
    mov ecx, [eax + offset LOOP_ITERATION.resultData].dwFailureCount
    assume eax:nothing
    
    .if ecx > 0
        szCopy addr szNextObjective, offset szRefineApproach
    .else
        szCopy addr szNextObjective, offset szContinuePhase
    .endif
    
    lea eax, szNextObjective
    ret
AdaptForNextIteration endp

; ============================================================================
; AddIterationToHistory - Add iteration to history
; ============================================================================
AddIterationToHistory proc
    LOCAL dwOffset:DWORD
    LOCAL pDest:DWORD
    
    .if g_dwHistoryCount >= MAX_ITERATION_COUNT
        ret
    .endif
    
    ; Calculate destination
    mov eax, g_dwHistoryCount
    mov ecx, sizeof LOOP_ITERATION
    imul eax, ecx
    add eax, g_pHistory
    mov pDest, eax
    
    ; Copy iteration to history
    invoke RtlCopyMemory, pDest, pIteration, sizeof LOOP_ITERATION
    
    inc g_dwHistoryCount
    ret
AddIterationToHistory endp

; ============================================================================
; LoopEngine_SetMaxIterations - Set maximum iterations
; ============================================================================
LoopEngine_SetMaxIterations proc dwMax:DWORD
    mov eax, dwMax
    mov g_dwMaxIterations, eax
    ret
LoopEngine_SetMaxIterations endp

; ============================================================================
; LoopEngine_Cancel - Cancel loop execution
; ============================================================================
LoopEngine_Cancel proc
    mov g_bCancelled, 1
    ret
LoopEngine_Cancel endp

; ============================================================================
; LoopEngine_Pause - Pause loop execution
; ============================================================================
LoopEngine_Pause proc
    mov g_bPaused, 1
    ret
LoopEngine_Pause endp

; ============================================================================
; LoopEngine_Resume - Resume loop execution
; ============================================================================
LoopEngine_Resume proc
    mov g_bPaused, 0
    ret
LoopEngine_Resume endp

; ============================================================================
; Callback setters
; ============================================================================

LoopEngine_SetIterationCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnIteration, eax
    ret
LoopEngine_SetIterationCallback endp

LoopEngine_SetStageCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnStage, eax
    ret
LoopEngine_SetStageCallback endp

LoopEngine_SetCompletionCallback proc pfn:DWORD
    mov eax, pfn
    mov g_pfnCompletion, eax
    ret
LoopEngine_SetCompletionCallback endp

; ============================================================================
; Cleanup
; ============================================================================

LoopEngine_Cleanup proc
    .if g_pHistory != 0
        MemFree g_pHistory
        mov g_pHistory, 0
    .endif
    
    .if g_hMutex != 0
        invoke CloseHandle, g_hMutex
        mov g_hMutex, 0
    .endif
    
    ret
LoopEngine_Cleanup endp

; ============================================================================
; Data
; ============================================================================

.data
    szReflectionFormat  db "Iteration %d: %d success, %d failed", 0
    szRefineApproach    db "Refine approach and retry failed actions", 0
    szContinuePhase     db "Continue with next phase", 0
    szObjectiveAchieved db "Objective achieved", 0
    szLoopCancelled     db "Loop cancelled by user", 0
    szMaxIterations     db "Maximum iterations reached", 0

end
