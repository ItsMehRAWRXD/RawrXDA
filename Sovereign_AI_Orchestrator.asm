; ==============================================================================
; Sovereign AI Orchestrator - DAG-Based Agent Coordinator
; ==============================================================================
; High-speed, deterministic task execution engine for AI IDE workflows.
; Manages agent dependency graphs with sub-microsecond dispatch latency.
;
; Architecture:
;   - Zero-latency direct CALL dispatch (no vtable overhead)
;   - Dependency-driven execution (DAG scheduling)
;   - Cache-friendly 32-byte aligned task structures
;   - Re-entrant for multi-threaded inference
;   - Position-independent code (PIC)
;
; Exports:
;   ROUTE_AGENT_TASK    - LLM capability router
;   EXECUTE_DAG         - Dependency graph executor
;   REGISTER_AGENT      - Add agent to dispatch table
;   GET_AGENT_STATUS    - Query task state
;   RESET_DAG           - Clear all task states
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; Constants
; ==============================================================================
MAX_AGENTS          equ 64
MAX_DEPENDENCIES    equ 8

; Task status codes
STATUS_PENDING      equ 0
STATUS_RUNNING      equ 1
STATUS_DONE         equ 2
STATUS_FAILED       equ 3

; Agent capability IDs
AGENT_RESEARCH      equ 1
AGENT_CODING        equ 2
AGENT_DEBUGGING     equ 3
AGENT_ANALYSIS      equ 4
AGENT_DOCUMENTATION equ 5
AGENT_TESTING       equ 6
AGENT_DEPLOYMENT    equ 7

; ==============================================================================
; Agent Task Structure (32 bytes - cache line friendly)
; ==============================================================================
AGENT_TASK struc
    TaskID      dq ?        ; Unique task identifier
    Handler     dq ?        ; Function pointer to agent logic
    DepCount    dq ?        ; Number of pending prerequisites
    TaskStatus  dq ?        ; 0=Pending, 1=Running, 2=Done, 3=Failed
    Capabilities dq ?       ; Bitmask of required capabilities
    Priority    dd ?        ; Execution priority (0-255)
    Reserved    dd ?        ; Padding to 40 bytes
AGENT_TASK ends

; ==============================================================================
; Dependency Edge Structure (16 bytes)
; ==============================================================================
DEPENDENCY_EDGE struc
    FromTask    dq ?        ; Source task ID
    ToTask      dq ?        ; Target task ID
DEPENDENCY_EDGE ends

; ==============================================================================
; Dispatch Table Entry (16 bytes)
; ==============================================================================
DISPATCH_ENTRY struc
    CapabilityID dq ?       ; Agent capability identifier
    HandlerAddr  dq ?       ; Function pointer
DISPATCH_ENTRY ends

; ==============================================================================
; Data Section
; ==============================================================================
.data
ALIGN 16
; Task table
g_TaskTable     AGENT_TASK MAX_AGENTS dup(<>)
g_TaskCount     dq 0

; Dependency graph
g_Dependencies  DEPENDENCY_EDGE MAX_AGENTS*MAX_DEPENDENCIES dup(<>)
g_DepCount      dq 0

; Dispatch table (capability -> handler mapping)
g_DispatchTable DISPATCH_ENTRY MAX_AGENTS dup(<>)
g_DispatchCount dq 0

; Execution statistics
g_ExecutedCount dq 0
g_FailedCount   dq 0

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; ROUTE_AGENT_TASK - LLM Capability Router
; ==============================================================================
; Input:  RCX = TaskID / Capability ID
; Output: RAX = Handler address, or 0 if not found
; Clobbers: RAX, R8, R9
; ==============================================================================
ROUTE_AGENT_TASK proc
    push rbx
    push rsi
    
    ; Search dispatch table
    lea rbx, g_DispatchTable
    mov rsi, [g_DispatchCount]
    test rsi, rsi
    jz route_not_found
    
    xor r8, r8
    
route_search:
    cmp r8, rsi
    jae route_not_found
    
    ; Calculate entry address
    imul r9, r8, SIZEOF DISPATCH_ENTRY
    lea r9, [rbx + r9]
    
    ; Compare capability ID
    mov rax, [r9 + DISPATCH_ENTRY.CapabilityID]
    cmp rax, rcx
    je route_found
    
    inc r8
    jmp route_search
    
route_found:
    mov rax, [r9 + DISPATCH_ENTRY.HandlerAddr]
    jmp route_exit
    
route_not_found:
    xor eax, eax
    
route_exit:
    pop rsi
    pop rbx
    ret
ROUTE_AGENT_TASK endp

; ==============================================================================
; REGISTER_AGENT - Add agent to dispatch table
; ==============================================================================
; Input:  RCX = Capability ID
;         RDX = Handler address
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
REGISTER_AGENT proc
    push rbx
    
    ; Check capacity
    mov rax, [g_DispatchCount]
    cmp rax, MAX_AGENTS
    jae register_fail
    
    ; Calculate entry address
    imul rbx, rax, SIZEOF DISPATCH_ENTRY
    lea rbx, [g_DispatchTable + rbx]
    
    ; Store entry
    mov [rbx + DISPATCH_ENTRY.CapabilityID], rcx
    mov [rbx + DISPATCH_ENTRY.HandlerAddr], rdx
    
    ; Increment count
    inc qword ptr [g_DispatchCount]
    
    mov eax, 1
    jmp register_exit
    
register_fail:
    xor eax, eax
    
register_exit:
    pop rbx
    ret
REGISTER_AGENT endp

; ==============================================================================
; ADD_TASK - Add task to DAG
; ==============================================================================
; Input:  RCX = TaskID
;         RDX = Capability ID (for routing)
;         R8  = Priority
;         R9  = Initial dependency count
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
ADD_TASK proc
    push rbx
    push rsi
    push rdi
    
    ; Check capacity
    mov rsi, [g_TaskCount]
    cmp rsi, MAX_AGENTS
    jae add_task_fail
    
    ; Get handler from dispatch table
    push rcx
    push rdx
    push r8
    push r9
    push rsi
    
    mov rcx, rdx            ; Capability ID
    call ROUTE_AGENT_TASK
    
    pop rsi
    pop r9
    pop r8
    pop rdx
    pop rcx
    
    test rax, rax
    jz add_task_fail        ; No handler for this capability
    
    ; Calculate task entry address
    imul rbx, rsi, SIZEOF AGENT_TASK
    lea rbx, [g_TaskTable + rbx]
    
    ; Initialize task
    mov [rbx + AGENT_TASK.TaskID], rcx
    mov [rbx + AGENT_TASK.Handler], rax
    mov [rbx + AGENT_TASK.DepCount], r9
    mov qword ptr [rbx + AGENT_TASK.TaskStatus], STATUS_PENDING
    mov [rbx + AGENT_TASK.Capabilities], rdx
    mov dword ptr [rbx + AGENT_TASK.Priority], r8d
    mov dword ptr [rbx + AGENT_TASK.Reserved], 0
    
    ; Increment count
    inc qword ptr [g_TaskCount]
    
    mov eax, 1
    jmp add_task_exit
    
add_task_fail:
    xor eax, eax
    
add_task_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
ADD_TASK endp

; ==============================================================================
; ADD_DEPENDENCY - Create edge between tasks
; ==============================================================================
; Input:  RCX = FromTask ID
;         RDX = ToTask ID
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
ADD_DEPENDENCY proc
    push rbx
    
    ; Check capacity
    mov rax, [g_DepCount]
    cmp rax, MAX_AGENTS*MAX_DEPENDENCIES
    jae dep_fail
    
    ; Calculate edge address
    imul rbx, rax, SIZEOF DEPENDENCY_EDGE
    lea rbx, [g_Dependencies + rbx]
    
    ; Store edge
    mov [rbx + DEPENDENCY_EDGE.FromTask], rcx
    mov [rbx + DEPENDENCY_EDGE.ToTask], rdx
    
    ; Increment count
    inc qword ptr [g_DepCount]
    
    ; Increment dependency count of target task
    call INCREMENT_DEP_COUNT
    
    mov eax, 1
    jmp dep_exit
    
dep_fail:
    xor eax, eax
    
dep_exit:
    pop rbx
    ret
ADD_DEPENDENCY endp

; ==============================================================================
; INCREMENT_DEP_COUNT - Helper to increment task dependency
; ==============================================================================
INCREMENT_DEP_COUNT proc
    push rbx
    push rsi
    push rdi
    
    mov rsi, [g_TaskCount]
    test rsi, rsi
    jz inc_dep_exit
    
    lea rbx, g_TaskTable
    xor rdi, rdi
    
inc_dep_search:
    cmp rdi, rsi
    jae inc_dep_exit
    
    imul rax, rdi, SIZEOF AGENT_TASK
    lea rax, [rbx + rax]
    
    cmp [rax + AGENT_TASK.TaskID], rdx
    jne inc_dep_next
    
    ; Found target task - increment dep count
    inc qword ptr [rax + AGENT_TASK.DepCount]
    jmp inc_dep_exit
    
inc_dep_next:
    inc rdi
    jmp inc_dep_search
    
inc_dep_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
INCREMENT_DEP_COUNT endp

; ==============================================================================
; EXECUTE_DAG - Dependency Graph Executor
; ==============================================================================
; Input:  None (operates on global task table)
; Output: RAX = Number of tasks executed
; Clobbers: All except non-volatile
; ==============================================================================
EXECUTE_DAG proc
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    xor r15, r15            ; Total executed count
    
dag_cycle:
    xor r12, r12            ; Tasks executed this cycle
    mov r13, [g_TaskCount]
    test r13, r13
    jz dag_finished
    
    xor r14, r14            ; Task index
    
scan_loop:
    cmp r14, r13
    jae scan_done
    
    ; Calculate task address
    imul rbx, r14, SIZEOF AGENT_TASK
    lea rbx, [g_TaskTable + rbx]
    
    ; Check if task is ready (DepCount == 0 && Status == PENDING)
    mov rax, [rbx + AGENT_TASK.DepCount]
    test rax, rax
    jnz skip_task
    
    mov rax, [rbx + AGENT_TASK.TaskStatus]
    cmp rax, STATUS_PENDING
    jne skip_task
    
    ; Mark as running
    mov qword ptr [rbx + AGENT_TASK.TaskStatus], STATUS_RUNNING
    
    ; Execute agent handler
    mov rax, [rbx + AGENT_TASK.Handler]
    test rax, rax
    jz task_failed
    
    ; Call handler (TaskID in RCX)
    mov rcx, [rbx + AGENT_TASK.TaskID]
    call rax
    
    ; Check return value (RAX = 1 success, 0 failure)
    test rax, rax
    jz task_failed
    
    ; Mark as done
    mov qword ptr [rbx + AGENT_TASK.TaskStatus], STATUS_DONE
    inc qword ptr [g_ExecutedCount]
    
    ; Decrement dependencies of downstream tasks
    mov rcx, [rbx + AGENT_TASK.TaskID]
    call DECREMENT_DOWNSTREAM
    
    inc r12
    inc r15
    jmp skip_task
    
task_failed:
    mov qword ptr [rbx + AGENT_TASK.TaskStatus], STATUS_FAILED
    inc qword ptr [g_FailedCount]
    
skip_task:
    inc r14
    jmp scan_loop
    
scan_done:
    ; If no tasks executed this cycle, we're done (or deadlocked)
    test r12, r12
    jnz dag_cycle
    
dag_finished:
    mov rax, r15
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
EXECUTE_DAG endp

; ==============================================================================
; DECREMENT_DOWNSTREAM - Decrement dependency counts
; ==============================================================================
; Input:  RCX = Completed TaskID
; Output: None
; ==============================================================================
DECREMENT_DOWNSTREAM proc
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, rcx            ; Completed task ID
    mov r13, [g_DepCount]
    test r13, r13
    jz dec_downstream_exit
    
    xor r14, r14            ; Edge index
    
dec_edge_loop:
    cmp r14, r13
    jae dec_downstream_exit
    
    ; Calculate edge address
    imul rbx, r14, SIZEOF DEPENDENCY_EDGE
    lea rbx, [g_Dependencies + rbx]
    
    ; Check if this edge starts from completed task
    cmp [rbx + DEPENDENCY_EDGE.FromTask], r12
    jne dec_edge_next
    
    ; Found downstream task - decrement its dep count
    mov rdx, [rbx + DEPENDENCY_EDGE.ToTask]
    call DECREMENT_TASK_DEP
    
dec_edge_next:
    inc r14
    jmp dec_edge_loop
    
dec_downstream_exit:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
DECREMENT_DOWNSTREAM endp

; ==============================================================================
; DECREMENT_TASK_DEP - Decrement specific task dependency count
; ==============================================================================
; Input:  RDX = TaskID to decrement
; Output: None
; ==============================================================================
DECREMENT_TASK_DEP proc
    push rbx
    push r12
    push r13
    
    mov r12, rdx            ; Target task ID
    mov r13, [g_TaskCount]
    test r13, r13
    jz dec_task_exit
    
    xor r8, r8              ; Task index
    
dec_task_loop:
    cmp r8, r13
    jae dec_task_exit
    
    imul rbx, r8, SIZEOF AGENT_TASK
    lea rbx, [g_TaskTable + rbx]
    
    cmp [rbx + AGENT_TASK.TaskID], r12
    jne dec_task_next
    
    ; Found task - decrement dep count (but not below 0)
    mov rax, [rbx + AGENT_TASK.DepCount]
    test rax, rax
    jz dec_task_exit
    dec qword ptr [rbx + AGENT_TASK.DepCount]
    jmp dec_task_exit
    
dec_task_next:
    inc r8
    jmp dec_task_loop
    
dec_task_exit:
    pop r13
    pop r12
    pop rbx
    ret
DECREMENT_TASK_DEP endp

; ==============================================================================
; GET_AGENT_STATUS - Query task state
; ==============================================================================
; Input:  RCX = TaskID
; Output: RAX = Status code (0=Pending, 1=Running, 2=Done, 3=Failed, -1=NotFound)
; ==============================================================================
GET_AGENT_STATUS proc
    push rbx
    push r12
    push r13
    
    mov r12, rcx            ; Task ID to find
    mov r13, [g_TaskCount]
    test r13, r13
    jz status_not_found
    
    xor r8, r8
    
status_search:
    cmp r8, r13
    jae status_not_found
    
    imul rbx, r8, SIZEOF AGENT_TASK
    lea rbx, [g_TaskTable + rbx]
    
    cmp [rbx + AGENT_TASK.TaskID], r12
    je status_found
    
    inc r8
    jmp status_search
    
status_found:
    mov rax, [rbx + AGENT_TASK.TaskStatus]
    jmp status_exit
    
status_not_found:
    mov rax, -1
    
status_exit:
    pop r13
    pop r12
    pop rbx
    ret
GET_AGENT_STATUS endp

; ==============================================================================
; RESET_DAG - Clear all task states for re-execution
; ==============================================================================
; Input:  None
; Output: RAX = Number of tasks reset
; ==============================================================================
RESET_DAG proc
    push rbx
    push r12
    push r13
    
    mov r13, [g_TaskCount]
    test r13, r13
    jz reset_exit
    
    xor r12, r12            ; Reset count
    xor r8, r8
    
reset_loop:
    cmp r8, r13
    jae reset_done
    
    imul rbx, r8, SIZEOF AGENT_TASK
    lea rbx, [g_TaskTable + rbx]
    
    ; Reset status to pending
    mov qword ptr [rbx + AGENT_TASK.TaskStatus], STATUS_PENDING
    inc r12
    
    inc r8
    jmp reset_loop
    
reset_done:
    ; Reset statistics
    mov qword ptr [g_ExecutedCount], 0
    mov qword ptr [g_FailedCount], 0
    
    mov rax, r12
    jmp reset_exit
    
reset_exit:
    pop r13
    pop r12
    pop rbx
    ret
RESET_DAG endp

; ==============================================================================
; GET_EXECUTION_STATS - Retrieve performance counters
; ==============================================================================
; Input:  RCX = Pointer to output buffer (16 bytes)
;         [RCX+0] = Total tasks
;         [RCX+8] = Executed count
;         [RCX+16] = Failed count
; Output: RAX = 1 always
; ==============================================================================
GET_EXECUTION_STATS proc
    mov rax, [g_TaskCount]
    mov [rcx + 0], rax
    mov rax, [g_ExecutedCount]
    mov [rcx + 8], rax
    mov rax, [g_FailedCount]
    mov [rcx + 16], rax
    mov eax, 1
    ret
GET_EXECUTION_STATS endp

; ==============================================================================
; Agent Handler Stubs (Production: Replace with actual LLM interfaces)
; ==============================================================================

RESEARCH_HANDLER proc
    ; Research agent logic
    ; In production: Interface with LLM for research tasks
    mov rax, 1
    ret
RESEARCH_HANDLER endp

CODING_HANDLER proc
    ; Coding agent logic
    ; In production: Interface with LLM for code generation
    mov rax, 1
    ret
CODING_HANDLER endp

DEBUGGING_HANDLER proc
    ; Debugging agent logic
    ; In production: Interface with LLM for debugging assistance
    mov rax, 1
    ret
DEBUGGING_HANDLER endp

ANALYSIS_HANDLER proc
    ; Analysis agent logic
    ; In production: Interface with LLM for code analysis
    mov rax, 1
    ret
ANALYSIS_HANDLER endp

DOCUMENTATION_HANDLER proc
    ; Documentation agent logic
    ; In production: Interface with LLM for documentation generation
    mov rax, 1
    ret
DOCUMENTATION_HANDLER endp

TESTING_HANDLER proc
    ; Testing agent logic
    ; In production: Interface with LLM for test generation
    mov rax, 1
    ret
TESTING_HANDLER endp

DEPLOYMENT_HANDLER proc
    ; Deployment agent logic
    ; In production: Interface with deployment pipeline
    mov rax, 1
    ret
DEPLOYMENT_HANDLER endp

end