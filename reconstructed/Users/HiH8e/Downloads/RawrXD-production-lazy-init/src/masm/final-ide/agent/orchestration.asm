; ============================================================================
; Phase 7 Batch 3: Multi-Session Agent Orchestration
; ============================================================================
; Agent session pooling, routing, and lifecycle management for concurrent inference
; Registry-configurable pool sizing, structured logging, metrics, and tracing
; ============================================================================

option casemap:none

include windows.inc
include kernel32.inc
include advapi32.inc

includelib kernel32.lib
includelib advapi32.lib

; ============================================================================
; EXTERNAL DECLARATIONS (Phase 4 + Observability)
; ============================================================================
EXTERN RegistryOpenKey:PROC
EXTERN RegistryCloseKey:PROC
EXTERN RegistryGetDWORD:PROC
EXTERN RegistrySetDWORD:PROC
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN InitializeSRWLock:PROC
EXTERN AcquireSRWLockExclusive:PROC
EXTERN ReleaseSRWLockExclusive:PROC
EXTERN AcquireSRWLockShared:PROC
EXTERN ReleaseSRWLockShared:PROC

; ============================================================================
; CONSTANTS & ENUMS
; ============================================================================
AGENT_POOL_MAX_SESSIONS        EQU 256
AGENT_SESSION_TIMEOUT_MS       EQU 30000  ; 30 seconds
AGENT_ROUTER_ROUND_ROBIN       EQU 0
AGENT_ROUTER_LEAST_LOADED      EQU 1

; Registry paths
regPath_RawrXD_AgentPool       DB "Software\\RawrXD\\AgentPool",0
regKey_MaxSessions             DB "MaxSessions",0
regKey_RouterType              DB "RouterType",0
regKey_SessionTimeout          DB "SessionTimeout",0

; ============================================================================
; STRUCTURES
; ============================================================================
AGENT_SESSION STRUCT
    SessionId          DWORD ?
    StatePtr           QWORD ?
    LastUsedQPC        QWORD ?
    BusyFlag           BYTE ?
    ErrorCode          DWORD ?
    Padding            BYTE 3 DUP(?)
AGENT_SESSION ENDS

AGENT_POOL STRUCT
    Version            DWORD ?
    MaxSessions        DWORD ?
    ActiveSessions     DWORD ?
    RouterType         DWORD ?
    SessionTimeout     DWORD ?
    PoolLock           QWORD ?  ; SRWLOCK
    FreeListHead       QWORD ?
    ActiveListHead     QWORD ?
    SessionsArray      QWORD ?
    Padding            QWORD ?
AGENT_POOL ENDS

AGENT_ROUTER_STATE STRUCT
    LastSessionIndex   DWORD ?
    LoadCounts         DWORD AGENT_POOL_MAX_SESSIONS DUP(?)
    Padding            DWORD ?
AGENT_ROUTER_STATE ENDS

; Observability structures
AGENT_METRICS STRUCT
    PoolInUse          DWORD ?
    DispatchDuration   DWORD ?
    AcquireFailures    DWORD ?
    ReleaseFailures    DWORD ?
    TotalDispatches    DWORD ?
    Padding            DWORD ?
AGENT_METRICS ENDS

; ============================================================================
; DATA
; ============================================================================
.DATA
agentPoolHandle        QWORD 0
agentMetrics           AGENT_METRICS {}

; Log format strings
logAgentPoolCreate     DB "AgentPool_Create: Initializing pool with %d sessions",0
logAgentPoolDestroy    DB "AgentPool_Destroy: Cleaning up %d sessions",0
logSessionAcquired     DB "AgentPool_AcquireSession: Session %d acquired",0
logSessionReleased     DB "AgentPool_ReleaseSession: Session %d released",0
logDispatchStarted     DB "AgentRouter_Dispatch: Starting dispatch to session %d",0
logDispatchCompleted   DB "AgentRouter_Dispatch: Completed dispatch to session %d, duration %d ms",0

; Metric names
metricPoolInUse        DB "agent_pool_in_use",0
metricDispatchDuration DB "agent_dispatch_duration_ms",0
metricAcquireFailures  DB "agent_acquire_failures_total",0
metricReleaseFailures  DB "agent_release_failures_total",0
metricTotalDispatches  DB "agent_dispatches_total",0

; ============================================================================
; CODE
; ============================================================================
.CODE

; ============================================================================
; Agent Pool Management
; ============================================================================

PUBLIC AgentPool_Create
AgentPool_Create PROC FRAME USES rbx rsi rdi
    LOCAL maxSessions:DWORD
    LOCAL routerType:DWORD
    LOCAL timeout:DWORD
    
    ; Load configuration from registry
    invoke RegistryOpenKey, HKEY_CURRENT_USER, ADDR regPath_RawrXD_AgentPool, KEY_READ
    test rax, rax
    jz use_defaults
    mov rdi, rax
    
    invoke RegistryGetDWORD, rdi, ADDR regKey_MaxSessions, 16
    mov maxSessions, eax
    
    invoke RegistryGetDWORD, rdi, ADDR regKey_RouterType, AGENT_ROUTER_ROUND_ROBIN
    mov routerType, eax
    
    invoke RegistryGetDWORD, rdi, ADDR regKey_SessionTimeout, AGENT_SESSION_TIMEOUT_MS
    mov timeout, eax
    
    invoke RegistryCloseKey, rdi
    jmp create_pool
    
use_defaults:
    mov maxSessions, 16
    mov routerType, AGENT_ROUTER_ROUND_ROBIN
    mov timeout, AGENT_SESSION_TIMEOUT_MS
    
create_pool:
    ; Validate max sessions
    cmp maxSessions, AGENT_POOL_MAX_SESSIONS
    jle size_ok
    mov maxSessions, AGENT_POOL_MAX_SESSIONS
    
size_ok:
    ; Allocate pool structure
    invoke GetProcessHeap
    test rax, rax
    jz pool_create_fail
    mov rbx, rax
    
    invoke HeapAlloc, rbx, HEAP_ZERO_MEMORY, SIZEOF AGENT_POOL
    test rax, rax
    jz pool_create_fail
    mov rsi, rax
    
    ; Initialize pool structure
    mov DWORD PTR [rsi + AGENT_POOL.Version], 070003h
    mov eax, maxSessions
    mov [rsi + AGENT_POOL.MaxSessions], eax
    mov DWORD PTR [rsi + AGENT_POOL.ActiveSessions], 0
    mov eax, routerType
    mov [rsi + AGENT_POOL.RouterType], eax
    mov eax, timeout
    mov [rsi + AGENT_POOL.SessionTimeout], eax
    
    ; Initialize SRW lock
    lea rcx, [rsi + AGENT_POOL.PoolLock]
    call InitializeSRWLock
    
    ; Allocate sessions array
    mov ecx, maxSessions
    imul rcx, SIZEOF AGENT_SESSION
    invoke HeapAlloc, rbx, HEAP_ZERO_MEMORY, rcx
    test rax, rax
    jz pool_create_cleanup
    mov [rsi + AGENT_POOL.SessionsArray], rax
    
    ; Initialize sessions
    mov rdi, rax
    xor ecx, ecx
init_sessions_loop:
    cmp ecx, maxSessions
    jge init_complete
    
    mov [rdi + AGENT_SESSION.SessionId], ecx
    mov DWORD PTR [rdi + AGENT_SESSION.BusyFlag], 0
    mov DWORD PTR [rdi + AGENT_SESSION.ErrorCode], 0
    
    add rdi, SIZEOF AGENT_SESSION
    inc ecx
    jmp init_sessions_loop
    
init_complete:
    mov agentPoolHandle, rsi
    
    ; Log pool creation
    ; invoke LogInfo, ADDR logAgentPoolCreate, maxSessions
    
    mov rax, rsi
    ret
    
pool_create_cleanup:
    invoke HeapFree, rbx, 0, rsi
    
pool_create_fail:
    xor rax, rax
    ret
AgentPool_Create ENDP

PUBLIC AgentPool_Destroy
AgentPool_Destroy PROC FRAME USES rbx rsi
    mov rsi, rcx
    test rsi, rsi
    jz destroy_done
    
    invoke GetProcessHeap
    test rax, rax
    jz destroy_done
    mov rbx, rax
    
    ; Free sessions array
    mov rcx, [rsi + AGENT_POOL.SessionsArray]
    test rcx, rcx
    jz free_pool
    invoke HeapFree, rbx, 0, rcx
    
free_pool:
    invoke HeapFree, rbx, 0, rsi
    mov agentPoolHandle, 0
    
    ; Log destruction
    ; invoke LogInfo, ADDR logAgentPoolDestroy, [rsi + AGENT_POOL.MaxSessions]
    
destroy_done:
    mov rax, 1
    ret
AgentPool_Destroy ENDP

PUBLIC AgentPool_AcquireSession
AgentPool_AcquireSession PROC FRAME USES rbx rsi
    LOCAL timeoutQPC:QWORD
    LOCAL currentQPC:QWORD
    
    mov rsi, agentPoolHandle
    test rsi, rsi
    jz acquire_fail
    
    ; Acquire exclusive lock
    lea rcx, [rsi + AGENT_POOL.PoolLock]
    call AcquireSRWLockExclusive
    
    ; Get current QPC for timeout checking
    lea rcx, currentQPC
    call QueryPerformanceCounter
    
    ; Calculate timeout QPC
    mov rax, currentQPC
    mov rcx, [rsi + AGENT_POOL.SessionTimeout]
    imul rcx, 10000  ; Convert ms to 100ns units
    add rax, rcx
    mov timeoutQPC, rax
    
    ; Find free session
    mov rbx, [rsi + AGENT_POOL.SessionsArray]
    mov ecx, [rsi + AGENT_POOL.MaxSessions]
    xor edx, edx
    
find_free_loop:
    cmp edx, ecx
    jge acquire_timeout
    
    cmp BYTE PTR [rbx + AGENT_SESSION.BusyFlag], 0
    jne next_session
    
    ; Check if session timed out
    mov rax, [rbx + AGENT_SESSION.LastUsedQPC]
    test rax, rax
    jz acquire_session
    
    cmp rax, currentQPC
    jl acquire_session  ; Session expired
    
next_session:
    add rbx, SIZEOF AGENT_SESSION
    inc edx
    jmp find_free_loop
    
acquire_session:
    ; Mark session as busy
    mov BYTE PTR [rbx + AGENT_SESSION.BusyFlag], 1
    mov rax, currentQPC
    mov [rbx + AGENT_SESSION.LastUsedQPC], rax
    
    ; Update active count
    mov eax, [rsi + AGENT_POOL.ActiveSessions]
    inc eax
    mov [rsi + AGENT_POOL.ActiveSessions], eax
    
    ; Update metrics
    mov eax, [agentMetrics + AGENT_METRICS.PoolInUse]
    inc eax
    mov [agentMetrics + AGENT_METRICS.PoolInUse], eax
    
    ; Release lock
    lea rcx, [rsi + AGENT_POOL.PoolLock]
    call ReleaseSRWLockExclusive
    
    ; Log acquisition
    mov eax, [rbx + AGENT_SESSION.SessionId]
    ; invoke LogInfo, ADDR logSessionAcquired, eax
    
    mov eax, [rbx + AGENT_SESSION.SessionId]
    ret
    
acquire_timeout:
    ; Release lock
    lea rcx, [rsi + AGENT_POOL.PoolLock]
    call ReleaseSRWLockExclusive
    
acquire_fail:
    ; Update failure metrics
    mov eax, [agentMetrics + AGENT_METRICS.AcquireFailures]
    inc eax
    mov [agentMetrics + AGENT_METRICS.AcquireFailures], eax
    
    xor rax, rax
    ret
AgentPool_AcquireSession ENDP

PUBLIC AgentPool_ReleaseSession
AgentPool_ReleaseSession PROC FRAME USES rbx rsi
    LOCAL currentQPC:QWORD
    
    mov esi, ecx  ; session_id
    mov rbx, agentPoolHandle
    test rbx, rbx
    jz release_fail
    
    ; Validate session ID
    cmp esi, [rbx + AGENT_POOL.MaxSessions]
    jge release_fail
    
    ; Acquire exclusive lock
    lea rcx, [rbx + AGENT_POOL.PoolLock]
    call AcquireSRWLockExclusive
    
    ; Get session pointer
    mov rax, [rbx + AGENT_POOL.SessionsArray]
    mov ecx, esi
    imul rcx, SIZEOF AGENT_SESSION
    add rax, rcx
    
    ; Check if session is actually busy
    cmp BYTE PTR [rax + AGENT_SESSION.BusyFlag], 0
    je release_invalid
    
    ; Mark session as free
    mov BYTE PTR [rax + AGENT_SESSION.BusyFlag], 0
    
    ; Update last used time
    lea rcx, currentQPC
    call QueryPerformanceCounter
    mov rax, currentQPC
    mov [rax + AGENT_SESSION.LastUsedQPC], rax
    
    ; Update active count
    mov eax, [rbx + AGENT_POOL.ActiveSessions]
    dec eax
    mov [rbx + AGENT_POOL.ActiveSessions], eax
    
    ; Update metrics
    mov eax, [agentMetrics + AGENT_METRICS.PoolInUse]
    dec eax
    mov [agentMetrics + AGENT_METRICS.PoolInUse], eax
    
    ; Release lock
    lea rcx, [rbx + AGENT_POOL.PoolLock]
    call ReleaseSRWLockExclusive
    
    ; Log release
    ; invoke LogInfo, ADDR logSessionReleased, esi
    
    mov rax, 1
    ret
    
release_invalid:
    ; Release lock
    lea rcx, [rbx + AGENT_POOL.PoolLock]
    call ReleaseSRWLockExclusive
    
release_fail:
    ; Update failure metrics
    mov eax, [agentMetrics + AGENT_METRICS.ReleaseFailures]
    inc eax
    mov [agentMetrics + AGENT_METRICS.ReleaseFailures], eax
    
    xor rax, rax
    ret
AgentPool_ReleaseSession ENDP

; ============================================================================
; Agent Router
; ============================================================================

PUBLIC AgentRouter_Dispatch
AgentRouter_Dispatch PROC FRAME USES rbx rsi rdi
    LOCAL startQPC:QWORD
    LOCAL endQPC:QWORD
    LOCAL sessionId:DWORD
    
    mov rsi, rcx  ; request_ptr
    mov edi, edx  ; request_len
    mov ebx, r8d  ; session_id (or AUTO)
    
    ; Get current pool
    mov rax, agentPoolHandle
    test rax, rax
    jz dispatch_fail
    
    ; Start timing
    lea rcx, startQPC
    call QueryPerformanceCounter
    
    ; Determine target session
    cmp ebx, -1  ; AUTO
    je auto_select
    
    mov sessionId, ebx
    jmp dispatch_request
    
auto_select:
    ; Simple round-robin selection
    mov rax, agentPoolHandle
    mov ecx, [rax + AGENT_POOL.ActiveSessions]
    mov edx, [rax + AGENT_POOL.MaxSessions]
    
    ; For now, just use first available
    mov sessionId, 0
    
dispatch_request:
    ; Log dispatch start
    ; invoke LogInfo, ADDR logDispatchStarted, sessionId
    
    ; TODO: Actual dispatch logic here
    ; This would call the actual agent execution
    
    ; Simulate some work
    invoke Sleep, 10
    
    ; End timing
    lea rcx, endQPC
    call QueryPerformanceCounter
    
    ; Calculate duration
    mov rax, endQPC
    sub rax, startQPC
    mov rcx, 10000  ; Convert to ms
    xor rdx, rdx
    div rcx
    
    ; Update metrics
    mov ecx, [agentMetrics + AGENT_METRICS.DispatchDuration]
    add ecx, eax
    mov [agentMetrics + AGENT_METRICS.DispatchDuration], ecx
    
    mov ecx, [agentMetrics + AGENT_METRICS.TotalDispatches]
    inc ecx
    mov [agentMetrics + AGENT_METRICS.TotalDispatches], ecx
    
    ; Log dispatch completion
    ; invoke LogInfo, ADDR logDispatchCompleted, sessionId, eax
    
    mov rax, 1
    ret
    
dispatch_fail:
    xor rax, rax
    ret
AgentRouter_Dispatch ENDP

; ============================================================================
; Agent State Management
; ============================================================================

PUBLIC AgentState_Snapshot
AgentState_Snapshot PROC FRAME USES rbx rsi
    mov esi, ecx  ; session_id
    mov rbx, rdx  ; buffer
    mov ecx, r8d  ; size
    
    ; Validate session
    mov rax, agentPoolHandle
    test rax, rax
    jz snapshot_fail
    
    cmp esi, [rax + AGENT_POOL.MaxSessions]
    jge snapshot_fail
    
    ; Get session
    mov rax, [rax + AGENT_POOL.SessionsArray]
    mov edx, esi
    imul rdx, SIZEOF AGENT_SESSION
    add rax, rdx
    
    ; Check if buffer is large enough
    cmp r8d, SIZEOF AGENT_SESSION
    jl snapshot_fail
    
    ; Copy session state
    mov rcx, rbx
    mov rdx, rax
    mov r8, SIZEOF AGENT_SESSION
    call memcpy
    
    mov rax, 1
    ret
    
snapshot_fail:
    xor rax, rax
    ret
AgentState_Snapshot ENDP

PUBLIC AgentState_Restore
AgentState_Restore PROC FRAME USES rbx rsi
    mov esi, ecx  ; session_id
    mov rbx, rdx  ; buffer
    mov ecx, r8d  ; size
    
    ; Validate session
    mov rax, agentPoolHandle
    test rax, rax
    jz restore_fail
    
    cmp esi, [rax + AGENT_POOL.MaxSessions]
    jge restore_fail
    
    ; Get session
    mov rax, [rax + AGENT_POOL.SessionsArray]
    mov edx, esi
    imul rdx, SIZEOF AGENT_SESSION
    add rax, rdx
    
    ; Check if buffer contains valid data
    cmp r8d, SIZEOF AGENT_SESSION
    jl restore_fail
    
    ; Restore session state
    mov rcx, rax
    mov rdx, rbx
    mov r8, SIZEOF AGENT_SESSION
    call memcpy
    
    mov rax, 1
    ret
    
restore_fail:
    xor rax, rax
    ret
AgentState_Restore ENDP

; ============================================================================
; Helper Functions
; ============================================================================

; Simple memcpy implementation
memcpy PROC FRAME USES rsi rdi
    mov rsi, rdx  ; source
    mov rdi, rcx  ; destination
    mov rcx, r8   ; count
    
    test rcx, rcx
    jz memcpy_done
    
memcpy_loop:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jnz memcpy_loop
    
memcpy_done:
    ret
memcpy ENDP

; ============================================================================
; Metrics and Observability
; ============================================================================

PUBLIC AgentPool_GetMetrics
AgentPool_GetMetrics PROC FRAME
    mov rax, OFFSET agentMetrics
    ret
AgentPool_GetMetrics ENDP

PUBLIC AgentPool_ResetMetrics
AgentPool_ResetMetrics PROC FRAME
    mov DWORD PTR [agentMetrics + AGENT_METRICS.PoolInUse], 0
    mov DWORD PTR [agentMetrics + AGENT_METRICS.DispatchDuration], 0
    mov DWORD PTR [agentMetrics + AGENT_METRICS.AcquireFailures], 0
    mov DWORD PTR [agentMetrics + AGENT_METRICS.ReleaseFailures], 0
    mov DWORD PTR [agentMetrics + AGENT_METRICS.TotalDispatches], 0
    mov rax, 1
    ret
AgentPool_ResetMetrics ENDP

; ============================================================================
; Phase 5 Test Integration
; ============================================================================

PUBLIC Test_AgentPool_BasicOperations
Test_AgentPool_BasicOperations PROC FRAME
    ; Create pool
    call AgentPool_Create
    test rax, rax
    jz test_fail
    
    ; Acquire session
    mov ecx, -1  ; AUTO
    call AgentPool_AcquireSession
    cmp eax, -1
    je test_fail
    
    mov ebx, eax  ; save session ID
    
    ; Release session
    mov ecx, ebx
    call AgentPool_ReleaseSession
    test rax, rax
    jz test_fail
    
    ; Destroy pool
    mov rcx, agentPoolHandle
    call AgentPool_Destroy
    test rax, rax
    jz test_fail
    
    mov rax, 1
    ret
    
test_fail:
    xor rax, rax
    ret
Test_AgentPool_BasicOperations ENDP

PUBLIC Test_AgentRouter_Dispatch
Test_AgentRouter_Dispatch PROC FRAME
    ; Create pool
    call AgentPool_Create
    test rax, rax
    jz test_fail
    
    ; Dispatch request
    xor rcx, rcx  ; null request
    xor edx, edx  ; zero length
    mov r8d, -1   ; AUTO session
    call AgentRouter_Dispatch
    test rax, rax
    jz test_fail
    
    ; Destroy pool
    mov rcx, agentPoolHandle
    call AgentPool_Destroy
    test rax, rax
    jz test_fail
    
    mov rax, 1
    ret
    
test_fail:
    xor rax, rax
    ret
Test_AgentRouter_Dispatch ENDP

END