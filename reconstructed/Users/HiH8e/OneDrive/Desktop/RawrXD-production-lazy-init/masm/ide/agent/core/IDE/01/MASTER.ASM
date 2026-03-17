;  ============================================================================
;  IDE_01_MASTER.asm  –  Master control and autonomous agent loop
;  ============================================================================
;  Implements:
;    - AgentPlan_Create / Resolve (CRDT plan nodes)
;    - AgentLoop_SingleStep / RunUntilDone (autonomous execution)
;    - IDEMaster_Initialize / LoadModel / HotSwapModel / ExecuteAgenticTask
;  ============================================================================

include ..\agent_core\IDE_INC.ASM

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
TICK_BUDGET_US    equ 10000    ; 10ms per tick
MAX_ITERATIONS    equ 1000

; ============================================================================
;  PLAN NODE STRUCTURE
; ============================================================================
PLAN_NODE STRUCT
    id          DWORD   ?
    parent_id   DWORD   ?
    state       DWORD   ?      ; 0=open, 1=in-progress, 2=done, 3=failed
    priority    DWORD   ?
    goal        BYTE MAX_GOAL_LEN dup(?)
    result      BYTE MAX_GOAL_LEN dup(?)
    timestamp   QWORD   ?
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
szAutoTick      db "autonomous_tick", 0
szJsonTemplate  db '{"id":%d,"parent":%d,"state":%d}', 0

; ============================================================================
;  CODE
; ============================================================================
.code

; ============================================================================
;  IDEMaster_Initialize – Initialize IDE master controller
; ============================================================================
IDEMaster_Initialize PROC
    LOCAL start:DWORD, dur:DWORD, hr:SDWORD
    
    ; Start timing
    invoke GetTickCount
    mov start, eax
    
    call _enter_crit
    
    ; Ensure log directory exists
    call _init_log_dir
    
    ; Initialize plan table
    mov g_plan_count, 0
    mov g_next_plan_id, 1
    mov g_loop_active, 0
    mov g_tick_count, 0
    
    mov hr, 0              ; S_OK
    
    call _leave_crit
    
    ; End timing
    invoke GetTickCount
    sub eax, start
    mov dur, eax
    
    LOG offset szInit, hr, dur
    
    mov eax, hr
    ret
IDEMaster_Initialize ENDP

; ============================================================================
;  IDEMaster_LoadModel – Load an AI model
;  Input:  pPath = path to model file (wide string)
;  Output: eax = model handle (or 0 on failure)
; ============================================================================
IDEMaster_LoadModel PROC uses ebx pPath:DWORD
    LOCAL start:DWORD, dur:DWORD, hr:SDWORD
    LOCAL hModel:DWORD
    
    invoke GetTickCount
    mov start, eax
    call _enter_crit
    
    .if pPath == 0
        mov hr, E_INVALIDARG
        xor eax, eax
        mov hModel, eax
        jmp done
    .endif
    
    ; TODO: Real model loading logic
    ; For now, return a dummy handle
    mov eax, g_next_plan_id
    inc g_next_plan_id
    mov hModel, eax
    mov hr, 0
    
done:
    call _leave_crit
    invoke GetTickCount
    sub eax, start
    mov dur, eax
    
    LOG offset szLoad, hr, dur
    
    mov eax, hModel
    ret
IDEMaster_LoadModel ENDP

; ============================================================================
;  IDEMaster_HotSwapModel – Hot-swap running model
; ============================================================================
IDEMaster_HotSwapModel PROC uses ebx hOld:DWORD, hNew:DWORD
    LOCAL start:DWORD, dur:DWORD, hr:SDWORD
    
    invoke GetTickCount
    mov start, eax
    call _enter_crit
    
    .if hOld == 0 || hNew == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; TODO: Real hot-swap logic
    mov hr, 0
    
done:
    call _leave_crit
    invoke GetTickCount
    sub eax, start
    mov dur, eax
    
    LOG offset szSwap, hr, dur
    
    mov eax, hr
    ret
IDEMaster_HotSwapModel ENDP

; ============================================================================
;  IDEMaster_ExecuteAgenticTask – Execute an agentic task
; ============================================================================
IDEMaster_ExecuteAgenticTask PROC uses ebx pTask:DWORD
    LOCAL start:DWORD, dur:DWORD, hr:SDWORD
    
    invoke GetTickCount
    mov start, eax
    call _enter_crit
    
    .if pTask == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; TODO: Parse task JSON and execute
    mov hr, 0
    
done:
    call _leave_crit
    invoke GetTickCount
    sub eax, start
    mov dur, eax
    
    LOG offset szExec, hr, dur
    
    mov eax, hr
    ret
IDEMaster_ExecuteAgenticTask ENDP

; ============================================================================
;  AgentPlan_Create – Create a new plan node
;  Input:  parentId = parent plan ID (0 for root)
;          pGoalJson = JSON describing the goal
;  Output: eax = new plan ID (or 0 on failure)
; ============================================================================
AgentPlan_Create PROC uses ebx esi edi, parentId:DWORD, pGoalJson:DWORD
    LOCAL start:DWORD, dur:DWORD, hr:SDWORD
    LOCAL newId:DWORD, pNode:PTR PLAN_NODE
    
    invoke GetTickCount
    mov start, eax
    call _enter_crit
    
    ; Check if we have room
    mov eax, g_plan_count
    .if eax >= MAX_PLAN_NODES
        mov hr, E_OUTOFMEMORY
        xor eax, eax
        mov newId, eax
        jmp done
    .endif
    
    ; Find first free slot
    xor ecx, ecx
    mov esi, offset g_plan_table
    
find_slot:
    .if ecx >= MAX_PLAN_NODES
        mov hr, E_OUTOFMEMORY
        xor eax, eax
        mov newId, eax
        jmp done
    .endif
    
    ; Check if this slot is free (state == 0 and id == 0)
    mov eax, (PLAN_NODE PTR [esi]).id
    .if eax == 0
        ; Found free slot
        mov pNode, esi
        jmp init_node
    .endif
    
    add esi, sizeof PLAN_NODE
    inc ecx
    jmp find_slot
    
init_node:
    ; Initialize the node
    mov eax, g_next_plan_id
    inc g_next_plan_id
    mov newId, eax
    
    mov edi, pNode
    mov (PLAN_NODE PTR [edi]).id, eax
    mov eax, parentId
    mov (PLAN_NODE PTR [edi]).parent_id, eax
    mov (PLAN_NODE PTR [edi]).state, 0        ; open
    mov (PLAN_NODE PTR [edi]).priority, 100   ; default priority
    
    ; Copy goal
    .if pGoalJson != 0
        push pGoalJson
        lea eax, (PLAN_NODE PTR [edi]).goal
        push eax
        call lstrcpynA
        add esp, 8
    .endif
    
    ; Set timestamp
    lea eax, (PLAN_NODE PTR [edi]).timestamp
    push eax
    call _perf_now
    add esp, 4
    
    inc g_plan_count
    mov hr, 0
    
done:
    call _leave_crit
    invoke GetTickCount
    sub eax, start
    mov dur, eax
    
    LOG offset szPlanCreate, hr, dur
    
    mov eax, newId
    ret
AgentPlan_Create ENDP

; ============================================================================
;  AgentPlan_Resolve – Serialize plan to JSON
;  Input:  planId = plan ID
;          pOutJson = output buffer
;          cbOut = buffer size
;  Output: eax = HRESULT
; ============================================================================
AgentPlan_Resolve PROC uses ebx esi edi, planId:DWORD, pOutJson:DWORD, cbOut:DWORD
    LOCAL start:DWORD, dur:DWORD, hr:SDWORD
    LOCAL pNode:PTR PLAN_NODE
    
    invoke GetTickCount
    mov start, eax
    call _enter_crit
    
    .if pOutJson == 0 || cbOut < 64
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; Find the plan node
    xor ecx, ecx
    mov esi, offset g_plan_table
    
find_node:
    .if ecx >= MAX_PLAN_NODES
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    mov eax, (PLAN_NODE PTR [esi]).id
    .if eax == planId
        mov pNode, esi
        jmp serialize
    .endif
    
    add esi, sizeof PLAN_NODE
    inc ecx
    jmp find_node
    
serialize:
    mov esi, pNode
    
    ; Build JSON: {"id":123,"state":1,"goal":"..."}
    invoke wsprintfA, pOutJson, offset szJsonTemplate, \
           (PLAN_NODE PTR [esi]).id, \
           (PLAN_NODE PTR [esi]).parent_id, \
           (PLAN_NODE PTR [esi]).state
    
    mov hr, 0
    
done:
    call _leave_crit
    invoke GetTickCount
    sub eax, start
    mov dur, eax
    
    LOG offset szPlanResolve, hr, dur
    
    mov eax, hr
    ret
AgentPlan_Resolve ENDP

; ============================================================================
;  AgentLoop_SingleStep – Execute one autonomy tick
;  Output: eax = HRESULT (S_OK or E_TIMEOUT if over budget)
; ============================================================================
AgentLoop_SingleStep PROC uses ebx esi edi
    LOCAL start:DWORD, dur:DWORD, hr:SDWORD
    LOCAL planId:DWORD
    
    invoke GetTickCount
    mov start, eax
    
    inc g_tick_count
    
    ; Create a root plan for this tick
    invoke AgentPlan_Create, 0, offset szAutoTick
    mov planId, eax
    
    .if eax == 0
        mov hr, E_FAIL
        jmp done
    .endif
    
    ; TODO: Real agent logic:
    ; 1. Analyze current state
    ; 2. Select next tool to invoke
    ; 3. Execute tool
    ; 4. Update memory
    ; 5. Reflect on progress
    ; 6. Decide if done or replan
    
    mov hr, 0
    
done:
    invoke GetTickCount
    sub eax, start
    mov dur, eax
    
    ; Check if we exceeded tick budget
    .if dur > TICK_BUDGET_US
        mov hr, E_TIMEOUT
    .endif
    
    LOG offset szLoopStep, hr, dur
    
    mov eax, hr
    ret
AgentLoop_SingleStep ENDP

; ============================================================================
; ============================================================================
;  AgentLoop_RunUntilDone – Run autonomy loop until completion
;  Input:  pGoalJson = high-level goal
;  Output: eax = HRESULT
; ============================================================================
AgentLoop_RunUntilDone PROC uses ebx esi edi, pGoalJson:DWORD
    LOCAL start:DWORD, dur:DWORD, hr:SDWORD
    LOCAL iteration:DWORD
    LOCAL rootPlanId:DWORD
    
    invoke GetTickCount
    mov start, eax
    
    ; Create root plan
    invoke AgentPlan_Create, 0, pGoalJson
    mov rootPlanId, eax
    
    .if eax == 0
        mov hr, E_FAIL
        jmp done
    .endif
    
    mov g_loop_active, 1
    mov iteration, 0
    
loop_iteration:
    ; Check iteration limit
    mov eax, iteration
    .if eax >= MAX_ITERATIONS
        mov hr, E_TIMEOUT
        jmp done
    .endif
    
    ; Execute one step
    invoke AgentLoop_SingleStep
    .if eax < 0
        mov hr, eax
        jmp done
    .endif
    
    ; Check if root plan is done
    ; TODO: Real completion check
    ; For now, just do a few iterations
    inc iteration
    
    mov eax, iteration
    .if eax < 10
        invoke Sleep, 10
        jmp loop_iteration
    .endif
    
    mov hr, 0
    
done:
    mov g_loop_active, 0
    
    invoke GetTickCount
    sub eax, start
    mov dur, eax
    LOG offset szLoopRun, hr, dur
    
    mov eax, hr
    ret
AgentLoop_RunUntilDone ENDP
END
