; ═══════════════════════════════════════════════════════════════════
; swarm_consensus.asm — RawrXD Lock-Free Multi-Node Consensus
; Phase 12: Sovereign Expansion — Distributed State Sync
;
; Architecture:
;   - CAS-based (CMPXCHG16B) state updates for node term/log index
; ═══════════════════════════════════════════════════════════════════

; ── Cross-module imports ─────────────────────────────────────────
EXTERN SwarmNet_SendShard:PROC
EXTERN g_hHeap:QWORD
EXTERN GetTickCount64:PROC

; ── Public exports ───────────────────────────────────────────────
PUBLIC Consensus_Init
PUBLIC Consensus_Propose
PUBLIC Consensus_Vote
PUBLIC Consensus_AppendEntries
PUBLIC Consensus_GetState
PUBLIC g_consensusTerm
PUBLIC g_consensusLeader
PUBLIC g_raftLogCount

; ── Data ─────────────────────────────────────────────────────────
.data
align 16
g_consensusTerm     dq 0                ; Current consensus term (epoch)
g_consensusLeader   dd -1               ; Current leader node ID (-1 = none)
g_nodeState         dd 0                ; 0=follower, 1=candidate, 2=leader
g_localState        dq 0, 0             ; 16-byte CAS target: [term, logIndex]
g_votesReceived     dd 0                ; Vote count for current proposal
g_totalNodes        dd 3                ; Total nodes in cluster (default 3)
g_myNodeId          dd 0                ; This node's ID
g_lastHeartbeat     dq 0                ; Tick count of last heartbeat from leader
g_votedFor          dd -1               ; Candidate voted in current term (-1 = none)
g_majorityNeeded    dd 2                ; Cached quorum = floor(total/2)+1
g_electionTimeoutMs dd 2000             ; Election timeout window
g_heartbeatTimeoutMs dd 1500            ; Leader heartbeat timeout window
g_commitIndex       dq 0                ; Highest committed log index

RAFT_LOG_CAP        equ 2048
align 16
g_raftLogTerm       dq RAFT_LOG_CAP dup(0)
g_raftLogHash       dq RAFT_LOG_CAP dup(0)
g_raftLogCount      dq 0
g_lastApplied       dq 0

.code

Consensus_Init PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    mov     g_consensusTerm, 0
    mov     g_consensusLeader, -1
    mov     g_nodeState, 0              ; Start as follower
    mov     g_votesReceived, 0
    mov     g_votedFor, -1
    mov     qword ptr g_localState, 0
    mov     qword ptr g_localState+8, 0
    mov     g_commitIndex, 0
    mov     g_raftLogCount, 0
    mov     g_lastApplied, 0

    ; Compute majorityNeeded = floor(totalNodes/2) + 1
    mov     eax, g_totalNodes
    shr     eax, 1
    inc     eax
    mov     g_majorityNeeded, eax

    ; Record initial heartbeat time
    call    GetTickCount64
    mov     g_lastHeartbeat, rax
    xor     eax, eax
    add     rsp, 28h
    ret
Consensus_Init ENDP

; ────────────────────────────────────────────────────────────────
; Consensus_Propose — Propose a new term (start election)
;   RCX = proposedTerm (new term number, must be > current)
;   RDX = proposerNodeId
;   Returns: EAX = 1 if proposal accepted (CAS succeeded), 0 if stale
;
;   Uses lock cmpxchg16b to atomically update [g_localState] from
;   (oldTerm, oldLogIdx) → (newTerm, 0). If the CAS fails, another
;   node already advanced the term — our proposal is stale.
; ────────────────────────────────────────────────────────────────
Consensus_Propose PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     r8, rcx                 ; proposedTerm
    mov     r9d, edx                ; proposerNodeId

    ; Only propose if new term > current
    cmp     r8, g_consensusTerm
    jbe     @propose_stale

    ; Split-brain guard: reject election if current leader heartbeat is still fresh
    cmp     g_consensusLeader, -1
    je      @propose_cas
    call    GetTickCount64
    sub     rax, g_lastHeartbeat
    cmp     eax, g_heartbeatTimeoutMs
    jb      @propose_stale

    ; Prepare CAS: expect old state, write new state
@propose_cas:
    lea     rdi, g_localState
    mov     rax, [rdi]              ; old term (low 64 bits)
    mov     rdx, [rdi+8]           ; old logIndex (high 64 bits)

    mov     rbx, r8                 ; new term (low 64 bits)
    mov     rcx, rdx
    inc     rcx                     ; new logIndex = oldLogIndex + 1

    lock cmpxchg16b oword ptr [rdi]
    jnz     @propose_stale          ; CAS failed — another node won

    ; CAS succeeded — update consensus state
    mov     g_consensusTerm, r8
    mov     g_nodeState, 1          ; Become candidate
    mov     g_votesReceived, 1      ; Vote for self
    mov     g_votedFor, r9d
    mov     g_consensusLeader, -1   ; no leader until quorum

    ; If single-node / already quorum, transition to leader immediately
    mov     eax, g_votesReceived
    cmp     eax, g_majorityNeeded
    jb      @propose_mark_hb
    mov     g_nodeState, 2          ; leader
    mov     g_consensusLeader, r9d
    mov     g_commitIndex, rcx

    ; Broadcast proposal to all peers via SwarmNet_SendShard
    ; (In production: send RequestVote RPC to all nodes)
    ; For now: just record timing
@propose_mark_hb:
    call    GetTickCount64
    mov     g_lastHeartbeat, rax

    mov     eax, 1                  ; Proposal accepted
    jmp     @propose_ret

@propose_stale:
    xor     eax, eax                ; Stale — rejected

@propose_ret:
    add     rsp, 28h
    pop     rdi
    pop     rbx
    ret
Consensus_Propose ENDP

; ────────────────────────────────────────────────────────────────
; Consensus_Vote — Cast a vote for a proposed term
;   RCX = candidateTerm (term being voted on)
;   EDX = candidateNodeId (who proposed)
;   Returns: EAX = 1 if vote granted, 0 if rejected
;
;   Raft rule: grant vote if candidateTerm >= our term, and we
;   haven't already voted in this term.
; ────────────────────────────────────────────────────────────────
Consensus_Vote PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Reject if candidate's term is behind ours
    cmp     rcx, g_consensusTerm
    jb      @vote_reject

    ; If candidate term is newer, step down and clear vote tracking
    ja      @vote_new_term

    ; Same term: enforce one-vote-per-term unless same candidate
    mov     eax, g_votedFor
    cmp     eax, -1
    je      @vote_record
    cmp     eax, edx
    jne     @vote_reject
    jmp     @vote_record

@vote_new_term:
    ; Accept: update our term and record vote
    mov     g_consensusTerm, rcx
    mov     g_consensusLeader, -1
    mov     g_nodeState, 0          ; Become follower
    mov     g_votesReceived, 0

@vote_record:
    mov     g_votedFor, edx         ; Record candidate for this term
    mov     g_consensusLeader, edx  ; Observed leader/candidate

    ; If this vote is for local candidate, accumulate quorum progress
    mov     eax, g_myNodeId
    cmp     edx, eax
    jne     @vote_mark_hb
    lock inc dword ptr g_votesReceived
    mov     eax, g_votesReceived
    cmp     eax, g_majorityNeeded
    jb      @vote_mark_hb
    mov     g_nodeState, 2          ; quorum reached -> leader
    mov     eax, g_myNodeId
    mov     g_consensusLeader, eax

    ; Update heartbeat (to prevent timeout)
@vote_mark_hb:
    push    rcx
    push    rdx
    call    GetTickCount64
    mov     g_lastHeartbeat, rax
    pop     rdx
    pop     rcx

    mov     eax, 1                  ; Vote granted
    jmp     @vote_ret

@vote_reject:
    xor     eax, eax                ; Vote denied

@vote_ret:
    add     rsp, 28h
    ret
Consensus_Vote ENDP

; ────────────────────────────────────────────────────────────────
; Consensus_AppendEntries — Append a replicated log record
;   RCX = leaderTerm
;   EDX = leaderNodeId
;   R8  = prevLogIndex
;   R9  = prevLogTerm
;   [rsp+28h] = entryTerm
;   [rsp+30h] = entryHash
;   Returns: EAX = 1 accepted, 0 rejected
;
; Behavior:
;   - Reject stale terms
;   - Verify prevLogIndex/prevLogTerm continuity
;   - Truncate conflicting tail and append new entry
;   - Advance commitIndex to appended index
; ────────────────────────────────────────────────────────────────
Consensus_AppendEntries PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Stack args after prolog:
    ;  entryTerm @ [rsp+60h]
    ;  entryHash @ [rsp+68h]
    mov     r10, qword ptr [rsp + 60h]      ; entryTerm
    mov     r11, qword ptr [rsp + 68h]      ; entryHash

    ; Reject stale leader term
    cmp     rcx, g_consensusTerm
    jb      @ca_reject

    ; Newer term => step down to follower and accept leader
    cmp     rcx, g_consensusTerm
    je      @ca_term_ok
    mov     g_consensusTerm, rcx
    mov     g_nodeState, 0                  ; follower
    mov     g_votedFor, -1
@ca_term_ok:

    ; Track active leader + heartbeat
    mov     g_consensusLeader, edx
    call    GetTickCount64
    mov     g_lastHeartbeat, rax

    ; Validate prevLogIndex range: 0..g_raftLogCount
    mov     rax, g_raftLogCount
    cmp     r8, rax
    ja      @ca_reject

    ; If prevLogIndex > 0, verify prev term matches
    test    r8, r8
    jz      @ca_append
    mov     rbx, r8
    dec     rbx                               ; zero-based slot
    cmp     rbx, RAFT_LOG_CAP
    jae     @ca_reject
    lea     rax, [g_raftLogTerm]              ; RIP-relative base
    mov     rsi, qword ptr [rax + rbx*8]       ; register-relative lookup
    cmp     rsi, r9
    jne     @ca_reject

@ca_append:
    ; New entry index = prevLogIndex + 1
    lea     rbx, [r8 + 1]

    ; Capacity check
    cmp     rbx, RAFT_LOG_CAP
    ja      @ca_reject

    ; Truncate conflicting tail to prevLogIndex
    mov     g_raftLogCount, r8

    ; Write new entry at slot (index-1)
    lea     rsi, [rbx - 1]
    lea     rax, [g_raftLogTerm]              ; RIP-relative base
    mov     qword ptr [rax + rsi*8], r10        ; register-relative store
    lea     rax, [g_raftLogHash]               ; RIP-relative base
    mov     qword ptr [rax + rsi*8], r11        ; register-relative store

    ; Commit appended entry
    mov     g_raftLogCount, rbx
    mov     g_commitIndex, rbx
    mov     g_lastApplied, rbx

    ; Mirror index into CAS-visible local state high qword
    mov     qword ptr g_localState+8, rbx

    mov     eax, 1
    jmp     @ca_ret

@ca_reject:
    xor     eax, eax

@ca_ret:
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
Consensus_AppendEntries ENDP

Consensus_GetState PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Heartbeat timeout: if leader is stale, clear leadership and force re-election.
    cmp     g_consensusLeader, -1
    je      @cgs_ret

    call    GetTickCount64
    sub     rax, g_lastHeartbeat
    cmp     eax, g_heartbeatTimeoutMs
    jb      @cgs_ret

    mov     g_consensusLeader, -1
    mov     g_nodeState, 0
    mov     g_votedFor, -1

@cgs_ret:
    mov     eax, g_consensusLeader
    add     rsp, 28h
    ret
Consensus_GetState ENDP

END
