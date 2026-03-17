;  ============================================================================
;  IDE_01_MASTER_SIMPLE.asm  –  Simplified master control (no timing macros)
;  ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

extern _enter_crit:PROC
extern _leave_crit:PROC
extern _log_json:PROC
extern _init_log_dir:PROC

; Convenience macro for inline strings
CSTR MACRO text:VARARG
    LOCAL string
    .data
    string db text,0
    .code
    EXITM <offset string>
ENDM

; ============================================================================
;  EXPORTS
; ============================================================================
PUBLIC IDEMaster_Initialize
PUBLIC IDEMaster_LoadModel
PUBLIC IDEMaster_HotSwapModel
PUBLIC IDEMaster_ExecuteAgenticTask
PUBLIC AgentPlan_Create
PUBLIC AgentPlan_Resolve
PUBLIC AgentLoop_SingleStep
PUBLIC AgentLoop_RunUntilDone

; ============================================================================
;  CONSTANTS
; ============================================================================
MAX_PLAN_NODES    equ 256
MAX_GOAL_LEN      equ 256
TICK_BUDGET_US    equ 10000
MAX_ITERATIONS    equ 1000

; ============================================================================
;  PLAN NODE STRUCTURE
; ============================================================================
PLAN_NODE STRUCT
    id          DWORD   ?
    parent_id   DWORD   ?
    state       DWORD   ?
    priority    DWORD   ?
    goal        BYTE MAX_GOAL_LEN dup(?)
    result      BYTE MAX_GOAL_LEN dup(?)
PLAN_NODE ENDS

; ============================================================================
;  DATA
; ============================================================================
.data
g_plan_table    PLAN_NODE MAX_PLAN_NODES dup(<>)
g_plan_count    DWORD 0
g_next_plan_id  DWORD 1
g_loop_active   DWORD 0
g_tick_count    DWORD 0

szInit          db "IDEMaster_Initialize", 0
szLoad          db "IDEMaster_LoadModel", 0
szSwap          db "IDEMaster_HotSwapModel", 0
szExec          db "IDEMaster_ExecuteAgenticTask", 0
szPlanCreate    db "AgentPlan_Create", 0
szPlanResolve   db "AgentPlan_Resolve", 0
szLoopStep      db "AgentLoop_SingleStep", 0
szLoopRun       db "AgentLoop_RunUntilDone", 0

; ============================================================================
;  CODE
; ============================================================================
.code

; ============================================================================
;  IDEMaster_Initialize
; ============================================================================
IDEMaster_Initialize PROC
    pushad
    call _enter_crit
    
    call _init_log_dir
    
    mov g_plan_count, 0
    mov g_next_plan_id, 1
    mov g_loop_active, 0
    mov g_tick_count, 0
    
    call _leave_crit
    
    push 0                      ; result
    push 10                     ; duration_us
    push offset szInit
    call _log_json
    add esp, 12
    
    popad
    xor eax, eax
    ret
IDEMaster_Initialize ENDP

; ============================================================================
;  IDEMaster_LoadModel
; ============================================================================
IDEMaster_LoadModel PROC pPath:DWORD
    pushad
    call _enter_crit
    
    mov eax, pPath
    test eax, eax
    jz load_fail
    
    mov eax, g_next_plan_id
    inc g_next_plan_id
    mov [esp+28], eax          ; Save to return value
    jmp load_done
    
load_fail:
    xor eax, eax
    mov [esp+28], eax
    
load_done:
    call _leave_crit
    
    push 0
    push 5
    push offset szLoad
    call _log_json
    add esp, 12
    
    popad
    ret
IDEMaster_LoadModel ENDP

; ============================================================================
;  IDEMaster_HotSwapModel
; ============================================================================
IDEMaster_HotSwapModel PROC hOld:DWORD, hNew:DWORD
    pushad
    call _enter_crit
    
    ; TODO: Real hot-swap
    
    call _leave_crit
    
    push 0
    push 5
    push offset szSwap
    call _log_json
    add esp, 12
    
    popad
    xor eax, eax
    ret
IDEMaster_HotSwapModel ENDP

; ============================================================================
;  IDEMaster_ExecuteAgenticTask
; ============================================================================
IDEMaster_ExecuteAgenticTask PROC pTask:DWORD
    pushad
    call _enter_crit
    
    ; TODO: Real execution
    
    call _leave_crit
    
    push 0
    push 10
    push offset szExec
    call _log_json
    add esp, 12
    
    popad
    xor eax, eax
    ret
IDEMaster_ExecuteAgenticTask ENDP

; ============================================================================
;  AgentPlan_Create
; ============================================================================
AgentPlan_Create PROC parentId:DWORD, pGoalJson:DWORD
    LOCAL newId:DWORD
    
    pushad
    call _enter_crit
    
    mov eax, g_plan_count
    cmp eax, MAX_PLAN_NODES
    jge plan_create_fail
    
    ; Find free slot
    xor ecx, ecx
    mov esi, offset g_plan_table
    
plan_find_slot:
    cmp ecx, MAX_PLAN_NODES
    jge plan_create_fail
    
    mov eax, (PLAN_NODE PTR [esi]).id
    test eax, eax
    jz plan_found_slot
    
    add esi, sizeof PLAN_NODE
    inc ecx
    jmp plan_find_slot
    
plan_found_slot:
    mov eax, g_next_plan_id
    inc g_next_plan_id
    mov newId, eax
    
    mov (PLAN_NODE PTR [esi]).id, eax
    mov eax, parentId
    mov (PLAN_NODE PTR [esi]).parent_id, eax
    mov (PLAN_NODE PTR [esi]).state, 0
    mov (PLAN_NODE PTR [esi]).priority, 100
    
    mov eax, pGoalJson
    test eax, eax
    jz plan_no_goal
    
    invoke lstrcpynA, addr (PLAN_NODE PTR [esi]).goal, pGoalJson, MAX_GOAL_LEN
    
plan_no_goal:
    inc g_plan_count
    jmp plan_create_done
    
plan_create_fail:
    xor eax, eax
    mov newId, eax
    
plan_create_done:
    call _leave_crit
    
    push 0
    push 20
    push offset szPlanCreate
    call _log_json
    add esp, 12
    
    popad
    mov eax, newId
    ret
AgentPlan_Create ENDP

; ============================================================================
;  AgentPlan_Resolve
; ============================================================================
AgentPlan_Resolve PROC planId:DWORD, pOutJson:DWORD, cbOut:DWORD
    pushad
    call _enter_crit
    
    mov eax, pOutJson
    test eax, eax
    jz plan_resolve_fail
    
    xor ecx, ecx
    mov esi, offset g_plan_table
    
plan_resolve_find:
    cmp ecx, MAX_PLAN_NODES
    jge plan_resolve_fail
    
    mov eax, (PLAN_NODE PTR [esi]).id
    cmp eax, planId
    je plan_resolve_found
    
    add esi, sizeof PLAN_NODE
    inc ecx
    jmp plan_resolve_find
    
plan_resolve_found:
    invoke wsprintfA, pOutJson, \
           CSTR('{"id":%d,"parent":%d,"state":%d}'), \
           (PLAN_NODE PTR [esi]).id, \
           (PLAN_NODE PTR [esi]).parent_id, \
           (PLAN_NODE PTR [esi]).state
    jmp plan_resolve_done
    
plan_resolve_fail:
    
plan_resolve_done:
    call _leave_crit
    
    push 0
    push 15
    push offset szPlanResolve
    call _log_json
    add esp, 12
    
    popad
    xor eax, eax
    ret
AgentPlan_Resolve ENDP

; ============================================================================
;  AgentLoop_SingleStep
; ============================================================================
AgentLoop_SingleStep PROC
    pushad
    
    inc g_tick_count
    
    invoke AgentPlan_Create, 0, CSTR("autonomy_tick")
    
    push 0
    push 500
    push offset szLoopStep
    call _log_json
    add esp, 12
    
    popad
    xor eax, eax
    ret
AgentLoop_SingleStep ENDP

; ============================================================================
;  AgentLoop_RunUntilDone
; ============================================================================
AgentLoop_RunUntilDone PROC pGoalJson:DWORD
    LOCAL iteration:DWORD
    
    pushad
    
    invoke AgentPlan_Create, 0, pGoalJson
    
    mov g_loop_active, 1
    mov iteration, 0
    
loop_iter:
    mov eax, iteration
    cmp eax, 10
    jge loop_done
    
    invoke AgentLoop_SingleStep
    
    inc iteration
    invoke Sleep, 10
    jmp loop_iter
    
loop_done:
    mov g_loop_active, 0
    
    push 0
    push 5000
    push offset szLoopRun
    call _log_json
    add esp, 12
    
    popad
    xor eax, eax
    ret
AgentLoop_RunUntilDone ENDP

END
