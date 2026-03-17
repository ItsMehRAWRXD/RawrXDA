.CODE

; ============================================================================
; RawrXD_SwarmCoordinator.asm
; Multi-Agent Task Coordination with Voting & Consensus
; x64 MASM — N-agent swarm for parallel reasoning + consensus voting
; ============================================================================

; ─────────────────────────────────────────────────────────────
; CONSTANTS
; ─────────────────────────────────────────────────────────────

MAX_AGENTS              EQU 8
AGENT_BUFFER_SIZE       EQU 4096
SWARM_TIMEOUT_MS        EQU 5000
CONSENSUS_THRESHOLD     EQU 0.67            ; 2/3 majority

; Agent message types
MSG_INITIALIZE          EQU 1
MSG_WORK                EQU 2
MSG_REPORT_RESULT       EQU 3
MSG_SHUTDOWN            EQU 4

; Voting strategies
VOTE_MAJORITY           EQU 1               ; >50%
VOTE_CONSENSUS          EQU 2               ; >67%
VOTE_WEIGHTED_AVERAGE   EQU 3               ; Quality-weighted

; ─────────────────────────────────────────────────────────────
; DATA STRUCTURES
; ─────────────────────────────────────────────────────────────

ALIGN 64

; Agent state table
g_AgentTable STRUCT
    state           DWORD 0             ; IDLE, WORKING, WAITING, COMPLETE
    task_id         QWORD 0
    output_ptr      QWORD 0
    output_size     DWORD 0
    quality_score   REAL8 0.0
    timestamp_ms    QWORD 0
ENDS

g_Agents                g_AgentTable MAX_AGENTS DUP(<>)
g_AgentCount            DWORD 0

; Shared coordination state
ALIGN 64
g_SwarmLock             QWORD 0             ; Interlocked spinlock
g_ActiveAgentMask       QWORD 0             ; Bitmask of active agents
g_ConsensusBuffer       BYTE 4096 DUP(0)
g_VotingScores          REAL8 MAX_AGENTS DUP(0.0)
g_ConsensusOutput       QWORD 0

; Task decomposition
g_TaskQueue             QWORD 16 DUP(0)
g_TaskQueueSize         DWORD 0
g_TaskQueueIdx          DWORD 0

; ─────────────────────────────────────────────────────────────
; Initialize Swarm
; ─────────────────────────────────────────────────────────────

RawrXD_SwarmCoordinator_Initialize PROC FRAME
    ; Input: rcx = agent count (1-8)
    ; Output: rax = success
    
    push rbx
    .PUSHREG rbx
    .ENDPROLOG
    
    mov ebx, ecx                        ; rbx = agent count
    cmp ebx, MAX_AGENTS
    jle .count_ok
    mov ebx, MAX_AGENTS
    
.count_ok:
    mov [g_AgentCount], ebx
    
    ; Initialize each agent state
    lea rax, [g_Agents]
    xor ecx, ecx
    
.init_agent_loop:
    cmp ecx, ebx
    jge .init_done
    
    ; Calculate offset within struct array
    mov edx, sizeof_g_AgentTable
    imul edx, ecx
    
    ; Set initial state to IDLE
    mov dword [rax + rdx + 0], 0  ; state = IDLE
    mov qword [rax + rdx + 8], 0  ; task_id = 0
    mov qword [rax + rdx + 16], ecx ; agent index
    
    inc ecx
    jmp .init_agent_loop
    
.init_done:
    xor eax, eax                        ; Success
    
    pop rbx
    ret
RawrXD_SwarmCoordinator_Initialize ENDP

; ─────────────────────────────────────────────────────────────
; Dispatch Task to Swarm
; ─────────────────────────────────────────────────────────────

RawrXD_SwarmCoordinator_DispatchTask PROC FRAME
    ; Input: rcx = task (prompt/goal)
    ;        rdx = output buffer array (N buffers)
    ; Output: rax = consensus output
    
    push rbx
    push rsi
    .PUSHREG rbx
    .PUSHREG rsi
    .ENDPROLOG
    
    mov rsi, rcx                        ; rsi = task
    
    ; Dispatch to all active agents
    xor ebx, ebx
    
.dispatch_agent_loop:
    cmp ebx, [g_AgentCount]
    jge .dispatch_done
    
    ; Calculate agent struct offset
    mov eax, sizeof_g_AgentTable
    imul eax, ebx
    lea rax, [g_Agents + rax]
    
    ; Set agent task
    mov qword [rax + 8], rsi            ; task_id = prompt
    mov dword [rax + 0], 1              ; state = WORKING
    
    ; In production: spin up thread/fiber for agent[ebx]
    ; For now: sequential simulation
    mov rcx, rbx
    call Execute_Agent_Task
    
    inc ebx
    jmp .dispatch_agent_loop
    
.dispatch_done:
    ; Wait for all agents to complete OR timeout
    call Wait_For_Agent_Completion
    
    ; Execute consensus voting
    call Execute_Consensus_Voting
    
    xor eax, eax                        ; Success
    
    pop rsi
    pop rbx
    ret
RawrXD_SwarmCoordinator_DispatchTask ENDP

; ─────────────────────────────────────────────────────────────
; Execute Agent Task (per-agent reasoning)
; ─────────────────────────────────────────────────────────────

Execute_Agent_Task PROC
    ; rcx = agent_id
    ; Executes reasoning chain for this agent
    
    ; Step 1: Call agentic inference
    mov rdx, rcx
    shl rdx, 12                         ; rdx = output buffer for agent
    mov r8d, 5                          ; reasoning depth = 5
    call RawrXD_AgenticInference_ChainOfThought
    
    ; rax = bytes written, xmm0 = quality score
    
    ; Step 2: Store result
    mov edx, sizeof_g_AgentTable
    imul edx, ecx
    lea rdx, [g_Agents + rdx]
    
    mov [rdx + 24], eax                 ; output_size = bytes written
    movsd xmmword [rdx + 32], xmm0      ; quality_score = xmm0
    mov dword [rdx + 0], 3              ; state = COMPLETE
    
    ret
Execute_Agent_Task ENDP

; ─────────────────────────────────────────────────────────────
; Wait for Agent Completion
; ─────────────────────────────────────────────────────────────

Wait_For_Agent_Completion PROC
    ; Wait for all agents with timeout
    
    lea rax, [g_Agents]
    mov ecx, [g_AgentCount]
    xor edx, edx                        ; timeout counter
    
.wait_loop:
    mov r8d, 0                          ; completed_count
    xor esi, esi
    
.check_agents:
    cmp esi, ecx
    jge .check_done
    
    mov r9d, sizeof_g_AgentTable
    imul r9d, esi
    
    cmp dword [rax + r9 + 0], 3        ; Check if COMPLETE
    jne .check_skip
    
    inc r8d
    
.check_skip:
    inc esi
    jmp .check_agents
    
.check_done:
    cmp r8d, ecx                        ; All complete?
    je .all_complete
    
    inc edx
    cmp edx, 100                        ; Timeout (simplified)
    jge .timeout
    
    pause
    jmp .wait_loop
    
.all_complete:
    ret
    
.timeout:
    ; Force shutdown incomplete agents
    ret
Wait_For_Agent_Completion ENDP

; ─────────────────────────────────────────────────────────────
; Execute Consensus Voting
; ─────────────────────────────────────────────────────────────

Execute_Consensus_Voting PROC FRAME
    ; Gather all agent outputs and vote on best
    ; Strategy: weighted average by quality score
    
    push rbx
    push rsi
    push rdi
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .ENDPROLOG
    
    lea rsi, [g_Agents]
    mov ecx, [g_AgentCount]
    
    ; Normalize quality scores (sum = 1.0)
    xor edx, edx
    movsd xmm7, [zero]
    
    ; Sum all scores
    xor edi, edi
.sum_scores:
    cmp edi, ecx
    jge .sum_done
    
    mov eax, sizeof_g_AgentTable
    imul eax, edi
    
    movsd xmm0, [rsi + rax + 32]        ; quality_score
    addsd xmm7, xmm0
    
    inc edi
    jmp .sum_scores
    
.sum_done:
    ; Normalize each score
    xor edi, edi
.normalize:
    cmp edi, ecx
    jge .normalize_done
    
    mov eax, sizeof_g_AgentTable
    imul eax, edi
    
    movsd xmm0, [rsi + rax + 32]        ; quality_score
    divsd xmm0, xmm7                    ; score / sum
    movsd [g_VotingScores + rdi * 8], xmm0
    
    inc edi
    jmp .normalize
    
.normalize_done:
    ; Find best agent (highest normalized score)
    xor ebx, ebx                        ; best_agent = 0
    movsd xmm0, [g_VotingScores]
    
    xor edi, edi
.find_best:
    cmp edi, ecx
    jge .find_best_done
    
    movsd xmm1, [g_VotingScores + rdi * 8]
    comisd xmm1, xmm0
    jbe .find_best_skip
    
    movsd xmm0, xmm1
    mov ebx, edi
    
.find_best_skip:
    inc edi
    jmp .find_best
    
.find_best_done:
    ; Copy best agent output to consensus buffer
    mov eax, sizeof_g_AgentTable
    imul eax, ebx
    
    mov rsi, [rsi + rax + 16]           ; output_ptr
    mov ecx, [rsi + rax + 24]           ; output_size
    
    lea rdi, [g_ConsensusBuffer]
    xor edx, edx
    
.copy_consensus:
    cmp edx, ecx
    jge .copy_done
    
    mov r8b, [rsi + rdx]
    mov [rdi + rdx], r8b
    inc edx
    jmp .copy_consensus
    
.copy_done:
    mov [g_ConsensusOutput], rdi
    
    pop rdi
    pop rsi
    pop rbx
    ret
Execute_Consensus_Voting ENDP

; ─────────────────────────────────────────────────────────────
; Get Consensus Output
; ─────────────────────────────────────────────────────────────

RawrXD_SwarmCoordinator_GetConsensus PROC
    ; Returns consensus output from voting
    mov rax, [g_ConsensusOutput]
    ret
RawrXD_SwarmCoordinator_GetConsensus ENDP

; ─────────────────────────────────────────────────────────────
; Get Agent Stats (for telemetry)
; ─────────────────────────────────────────────────────────────

RawrXD_SwarmCoordinator_GetStats PROC
    ; Input: rcx = stats buffer
    ; Output: writes agent metrics to buffer
    
    lea rax, [g_Agents]
    mov edx, [g_AgentCount]
    
    xor esi, esi
.stats_loop:
    cmp esi, edx
    jge .stats_done
    
    mov r8d, sizeof_g_AgentTable
    imul r8d, esi
    
    movsd xmm0, [rax + r8 + 32]         ; quality_score
    mov r9d, [rax + r8 + 24]            ; output_size
    
    ; Write to stats buffer
    ; Format: agent_id | bytes | quality
    mov qword [rcx], rsi                ; agent_id
    mov qword [rcx + 8], r9             ; bytes
    movsd [rcx + 16], xmm0              ; quality
    
    add rcx, 32
    inc esi
    jmp .stats_loop
    
.stats_done:
    ret
RawrXD_SwarmCoordinator_GetStats ENDP

; ─────────────────────────────────────────────────────────────
; DATA
; ─────────────────────────────────────────────────────────────

.DATA

sizeof_g_AgentTable     EQU 48

zero                    REAL8 0.0

END
