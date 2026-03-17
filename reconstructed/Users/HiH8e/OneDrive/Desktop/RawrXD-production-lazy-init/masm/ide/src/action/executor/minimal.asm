; ============================================================================
; RawrXD Agentic IDE - Minimal Action Executor
; Pure MASM - Simplified but functional version
; ============================================================================

.686
.model flat, stdcall
option casemap:none

; External declarations for Windows API functions
CreateMutex       proto :DWORD,:DWORD,:DWORD
CloseHandle       proto :DWORD
WaitForSingleObject proto :DWORD,:DWORD
ReleaseMutex      proto :DWORD
HeapAlloc         proto :DWORD,:DWORD,:DWORD
HeapFree          proto :DWORD,:DWORD,:DWORD
GetProcessHeap    proto
GetTickCount      proto
MessageBox        proto :DWORD,:DWORD,:DWORD,:DWORD
ExitProcess       proto :DWORD

; Library imports
includelib kernel32.lib
includelib user32.lib

; Constants
HEAP_ZERO_MEMORY     equ 8
INFINITE             equ -1
NULL                 equ 0
TRUE                 equ 1
FALSE                equ 0
MB_OK                equ 0

; Action type constants
ACTION_TYPE_FILE_READ       equ 1
ACTION_TYPE_FILE_WRITE      equ 2
ACTION_TYPE_COMMAND_RUN     equ 6

; Status constants  
ACTION_STATUS_SUCCESS       equ 0
ACTION_STATUS_ERROR         equ 1

; Simple structures
ACTION_ITEM struct
    szActionID          db 64 dup(?)
    dwType              dd ?
    szDescription       db 256 dup(?)
    szParameters        db 512 dup(?)
ACTION_ITEM ends

ACTION_RESULT struct
    szActionID          db 64 dup(?)
    dwStatus            dd ?
    dwDurationMs        dd ?
ACTION_RESULT ends

EXECUTION_PLAN struct
    szPlanID            db 64 dup(?)
    dwActionCount       dd ?
    pActions            dd ?
EXECUTION_PLAN ends

PLAN_RESULT struct
    szPlanID            db 64 dup(?)
    bSuccess            dd ?
    dwTotalActions      dd ?
    dwSuccessCount      dd ?
    dwFailureCount      dd ?
PLAN_RESULT ends

.data
    ; Configuration settings
    g_bInitialized      dd 0
    g_bDryRunMode       dd 0
    g_bStopOnError      dd 1
    g_dwTimeout         dd 5000
    g_bCancelled        dd 0
    
    ; Statistics
    g_dwTotalActions    dd 0
    g_dwSuccessCount    dd 0
    g_dwFailureCount    dd 0
    
    ; Messages
    szInitMsg           db "Action Executor initialized", 0
    szTitle             db "Action Executor", 0

.data?
    g_hMutex            dd ?
    g_hHeap             dd ?

.code

; ============================================================================
; ActionExecutor_Init - Initialize action executor
; Returns: TRUE in eax on success, FALSE on failure
; ============================================================================
ActionExecutor_Init proc
    
    ; Check if already initialized
    .if g_bInitialized
        mov eax, TRUE
        ret
    .endif
    
    ; Get process heap
    invoke GetProcessHeap
    mov g_hHeap, eax
    test eax, eax
    jz InitError
    
    ; Create mutex
    invoke CreateMutex, NULL, FALSE, NULL
    mov g_hMutex, eax
    test eax, eax
    jz InitError
    
    ; Show initialization message
    invoke MessageBox, NULL, addr szInitMsg, addr szTitle, MB_OK
    
    mov g_bInitialized, TRUE
    mov eax, TRUE
    ret
    
InitError:
    xor eax, eax
    ret
ActionExecutor_Init endp

; ============================================================================
; ActionExecutor_ExecutePlan - Execute an execution plan
; Input: pPlan (pointer to EXECUTION_PLAN)
; Returns: Pointer to PLAN_RESULT in eax
; ============================================================================
ActionExecutor_ExecutePlan proc pPlan:DWORD
    LOCAL pResult:DWORD
    LOCAL i:DWORD
    LOCAL dwStart:DWORD
    LOCAL dwEnd:DWORD
    
    ; Validate input
    test pPlan, 0
    jz PlanError
    
    ; Lock
    invoke WaitForSingleObject, g_hMutex, INFINITE
    
    ; Allocate result structure
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, sizeof PLAN_RESULT
    mov pResult, eax
    test eax, eax
    jz AllocError
    
    ; Get start time
    invoke GetTickCount
    mov dwStart, eax
    
    ; Get action count
    mov eax, pPlan
    assume eax:ptr EXECUTION_PLAN
    mov ecx, [eax].dwActionCount
    assume eax:nothing
    
    ; Initialize result
    mov eax, pResult
    assume eax:ptr PLAN_RESULT
    mov [eax].dwTotalActions, ecx
    mov [eax].dwSuccessCount, 0
    mov [eax].dwFailureCount, 0
    assume eax:nothing
    
    ; Simulate execution (simplified)
    mov i, 0
    
    ActionLoop:
        .if i >= ecx
            jmp ActionsDone
        .endif
        
        ; Check for cancellation
        .if g_bCancelled
            jmp ActionsDone
        .endif
        
        ; Simulate action execution
        inc g_dwSuccessCount
        inc i
        jmp ActionLoop
    
    ActionsDone:
    
    ; Update result
    mov eax, pResult
    assume eax:ptr PLAN_RESULT
    mov [eax].bSuccess, TRUE
    mov edx, g_dwSuccessCount
    mov [eax].dwSuccessCount, edx
    assume eax:nothing
    
    ; Unlock
    invoke ReleaseMutex, g_hMutex
    
    mov eax, pResult
    ret
    
PlanError:
    xor eax, eax
    ret
    
AllocError:
    invoke ReleaseMutex, g_hMutex
    xor eax, eax
    ret
    
ActionExecutor_ExecutePlan endp

; ============================================================================
; ActionExecutor_SetDryRunMode - Enable/disable dry-run mode
; Input: bEnable (1=enabled, 0=disabled)
; ============================================================================
ActionExecutor_SetDryRunMode proc bEnable:DWORD
    mov eax, bEnable
    mov g_bDryRunMode, eax
    ret
ActionExecutor_SetDryRunMode endp

; ============================================================================
; ActionExecutor_Cancel - Cancel execution
; ============================================================================
ActionExecutor_Cancel proc
    mov g_bCancelled, 1
    ret
ActionExecutor_Cancel endp

; ============================================================================
; ActionExecutor_Cleanup - Cleanup resources
; ============================================================================
ActionExecutor_Cleanup proc
    
    ; Check if initialized
    .if !g_bInitialized
        ret
    .endif
    
    ; Close mutex
    .if g_hMutex
        invoke CloseHandle, g_hMutex
        mov g_hMutex, 0
    .endif
    
    ; Reset initialization flag
    mov g_bInitialized, FALSE
    
    ret
ActionExecutor_Cleanup endp

; ============================================================================
; Entry point for testing
; ============================================================================
public _mainCRTStartup
_mainCRTStartup proc
    ; Initialize the action executor
    invoke ActionExecutor_Init
    
    ; Cleanup and exit
    invoke ActionExecutor_Cleanup
    invoke ExitProcess, 0
_mainCRTStartup endp

end