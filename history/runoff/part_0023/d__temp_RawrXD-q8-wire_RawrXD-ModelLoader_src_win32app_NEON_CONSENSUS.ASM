; ==============================================================================
; NEON_CONSENSUS.ASM - Distributed Consensus & Predictive Sync
; "The swarm persists, the beat aligns across space and time"
; Pure MASM64 - Zero Dependencies - Byzantine Fault Tolerance for Visuals
; ==============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:3

; ==============================================================================
; CONSENSUS CONSTANTS
; ==============================================================================

; Raft Consensus States
RAFT_STATE_FOLLOWER         EQU 0
RAFT_STATE_CANDIDATE        EQU 1
RAFT_STATE_LEADER           EQU 2
RAFT_STATE_OBSERVER         EQU 3         ; Non-voting member

; Election Timing (randomized to prevent split votes)
ELECTION_TIMEOUT_MIN_MS     EQU 150
ELECTION_TIMEOUT_MAX_MS     EQU 300
HEARTBEAT_INTERVAL_MS       EQU 50        ; Leader -> Followers
MAX_ELECTION_ROUNDS         EQU 10

; Log Replication
LOG_ENTRY_PRESET            EQU 1
LOG_ENTRY_BEAT              EQU 2
LOG_ENTRY_OVERRIDE          EQU 3
LOG_ENTRY_CONFIG_CHANGE     EQU 4         ; Add/remove peer
LOG_MAX_ENTRIES             EQU 10000

; Byzantine Fault Tolerance
BFT_TOLERANCE_F             EQU 2         ; Tolerate F faulty nodes
BFT_MIN_NODES               EQU 3*2+1     ; 3F+1 minimum
BFT_QUORUM                  EQU 2*2+1     ; 2F+1 quorum

; Predictive Sync
PREDICT_BUFFER_SIZE         EQU 256       ; Circular buffer
SYNC_HISTORY_SIZE           EQU 64        ; Beat timestamps
LATENCY_WINDOW_MS           EQU 1000      ; Rolling average window
JITTER_COMPENSATION_MAX     EQU 50        ; Max ms to smooth

; Clock Synchronization
NTP_POLL_INTERVAL_MS        EQU 60000     ; Sync clock every minute
MAX_CLOCK_DRIFT_PPM         EQU 500       ; 0.05% max drift
CLOCK_SLEW_RATE             EQU 0.001     ; Gradual adjustment

; ==============================================================================
; RAFT CONSENSUS STRUCTURES
; ==============================================================================

; Piece Tree Node - The heart of Monaco's O(log n) text buffer
PIECE_NODE STRUCT 8
    ; Tree links
    left        QWORD ?      ; Left child
    right       QWORD ?      ; Right child
    parent      QWORD ?      ; Parent node
    color       DWORD ?      ; 0=red, 1=black (RB tree)
    padding     DWORD ?      ; Alignment
    
    ; Piece data (points to original buffer, not copied)
    bufferIndex QWORD ?      ; Index into buffer set
    start       QWORD ?      ; Start offset in original buffer
    length      QWORD ?      ; Length of this piece
    
    ; Cached metrics for O(1) line/offset calc
    lineCount   QWORD ?      ; Newlines in this subtree
    lengthTotal QWORD ?      ; Total chars in subtree
    
    ; LF cache - speeds up line→offset translation
    lfCache     QWORD 32 DUP(?) ; Line break positions (relative)
PIECE_NODE ENDS

; Text Buffer - Collection of immutable string pieces
TEXT_BUFFER STRUCT 8
    ; Piece tree root
    root        QWORD ?      ; PIECE_NODE root
    
    ; Buffer set (original buffers never modified, only appended)
    buffers     QWORD ?      ; Array of BUFFER_DESC ptrs
    bufferCount QWORD ?
    bufferCap   QWORD ?
    
    ; Line cache - speeds up line access
    lineCache   QWORD ?      ; LINE_CACHE_ENTRY array
    lineCacheSize QWORD ?
    
    ; EOL normalization
    eol         DWORD ?      ; 0=LF, 1=CRLF, 2=CR
    containsCRLF DWORD ?
    
    ; Atomic change tracking
    versionId   QWORD ?      ; Incremented on each edit
    
    ; Hot path: direct access to small buffers
    scratch     BYTE 4096 DUP(?) ; Scratch for small edits
TEXT_BUFFER ENDS

BUFFER_DESC STRUCT 8
    data        QWORD ?      ; Raw UTF-16LE data (immutable)
    length      QWORD ?      ; Char count
    isOriginal  DWORD ?      ; 1=from file, 0=from edit
    refCount    DWORD ?      ; GC tracking
BUFFER_DESC ENDS

; View Model - Decorates buffer with view state
VIEW_MODEL STRUCT 8
    ; Source buffer
    buffer      QWORD ?      ; TEXT_BUFFER ptr
    
    ; Viewport state
    viewLines   QWORD ?      ; Array of VIEW_LINE
    firstLine   QWORD ?      ; 1-based line number at top
    lineCount   QWORD ?      ; Total lines in viewport
    
    ; Scroll state
    scrollTop   QWORD ?      ; Vertical scroll pixels
    scrollLeft  QWORD ?      ; Horizontal scroll pixels
    
    ; Configuration
    tabSize     DWORD ?
    wordWrap    DWORD ?
    lineNumbers DWORD ?      ; 0=off, 1=on, 2=relative
    
    ; Rendering cache
    dirtyStart  QWORD ?      ; First dirty line
    dirtyEnd    QWORD ?      ; Last dirty line
    
    ; Minimap data
    minimapLines QWORD ?     ; Downsampled color data
VIEW_MODEL ENDS

VIEW_LINE STRUCT 8
    lineNumber  QWORD ?      ; 1-based
    content     QWORD ?      ; Rendered text (cached)
    width       QWORD ?      ; Pixel width
    isWrapped   BYTE ?       ; Continuation of prev line
    padding     BYTE 7 DUP(?)
VIEW_LINE ENDS

; ==============================================================================
; TOKENIZER - Incremental TextMate-style highlighting
; ==============================================================================

TOKENIZER_STATE STRUCT 8
    ruleStack   QWORD ?      ; Current parse stack (compressed)
    modeId      DWORD ?      ; Language mode
    version     QWORD ?      ; For invalidation
    
    ; Incremental parsing
    validUntil  QWORD ?      ; Line number of last valid parse
    dirtyFrom   QWORD ?      ; First line needing reparse
    
    ; Lookahead cache
    lineStates  QWORD ?      ; Per-line state snapshots
TOKENIZER_STATE ENDS

TOKEN STRUCT 4
    offset      WORD ?       ; Start offset in line
    length      WORD ?       ; Token length
    type        DWORD ?      ; Token type (color index)
TOKEN ENDS

; ==============================================================================
; EDIT OPERATIONS - Atomic changes for undo/redo
; ==============================================================================

EDIT_OPERATION STRUCT 8
    ; Range deleted
    oldStartLine    QWORD ?
    oldStartCol     QWORD ?
    oldEndLine      QWORD ?
    oldEndCol       QWORD ?
    oldText         QWORD ?      ; Deleted text (for undo)
    
    ; Range inserted
    newStartLine    QWORD ?
    newStartCol     QWORD ?
    newEndLine      QWORD ?
    newEndCol       QWORD ?
    newText         QWORD ?      ; Inserted text
    
    ; Metadata
    timestamp       QWORD ?
    beforeVersion   QWORD ?
    afterVersion    QWORD ?
EDIT_OPERATION ENDS

; ==============================================================================
; RAFT CONSENSUS STRUCTURES CONTINUED
; ==============================================================================

RAFT_STATE STRUCT 512
    ; Persistent state (saved to disk)
    current_term                QWORD ?         ; Monotonic term counter
    voted_for                   QWORD ?         ; Who we voted for this term
    log                         QWORD ?         ; Array of LOG_ENTRY
    log_count                   DWORD ?
    log_capacity                DWORD ?
    
    ; Volatile state
    state                       BYTE ?          ; RAFT_STATE_*
    last_heartbeat              QWORD ?         ; QPC timestamp
    
    ; Leader election
    election_timer              DWORD ?         ; Random timeout
    votes_received              DWORD ?         ; Bitmask of votes
    votes_needed                DWORD ?
    
    ; Leader only
    next_index                  QWORD ?         ; For each peer: next log to send
    match_index                 QWORD ?         ; For each peer: highest known match
    
    ; Commit state
    commit_index                QWORD ?
    last_applied                QWORD ?
    
    ; Cluster membership
    peers                       QWORD ?         ; Array of RAFT_PEER
    peer_count                  DWORD ?
    quorum_size                 DWORD ?
    
    ; Network
    rpc_socket                  QWORD ?
    rpc_port                    WORD ?
    
    ; Callbacks
    on_commit                   QWORD ?         ; Function ptr
    on_leader_elected           QWORD ?
    on_leader_deposed           QWORD ?
RAFT_STATE ENDS

RAFT_PEER STRUCT 128
    peer_id                     QWORD ?
    address                     SOCKADDR_IN <>
    is_voting                   BYTE ?          ; Can vote in elections
    is_connected                BYTE ?
    last_contact                QWORD ?
    
    ; Leader tracking
    next_log_index              QWORD ?         ; Leader's view
    match_log_index             QWORD ?
    inflight_count              DWORD ?         ; Unacknowledged entries
    
    ; Latency tracking
    latency_samples             DWORD 16 DUP(?) ; Rolling window
    latency_sample_index        BYTE ?
    avg_latency_ms              DWORD ?
    min_latency_ms              DWORD ?
    max_latency_ms              DWORD ?
RAFT_PEER ENDS

LOG_ENTRY STRUCT 64
    term                        QWORD ?         ; Term when entry created
    index                       QWORD ?         ; Position in log
    timestamp                   QWORD ?         ; Logical timestamp
    
    ; Command
    command_type                BYTE ?
    UNION
        preset_cmd              PRESET_COMMAND <>
        beat_cmd                BEAT_COMMAND <>
        override_cmd            OVERRIDE_COMMAND <>
        config_cmd              CONFIG_COMMAND <>
    ENDS
    
    ; Consensus
    votes_received              DWORD ?         ; Bitmask (for BFT)
    is_committed                BYTE ?
    client_id                   QWORD ?         ; Who requested
LOG_ENTRY ENDS

PRESET_COMMAND STRUCT
    preset_id                   DWORD ?
    transition_type             BYTE ?
    duration_ms                 WORD ?
    start_beat_phase            REAL4 ?
    context_features            REAL4 16 DUP(?) ; For AI curation
PRESET_COMMAND ENDS

BEAT_COMMAND STRUCT
    bpm                         REAL4 ?
    confidence                  REAL4 ?
    phase_offset                REAL4 ?
    predicted_next              QWORD ?         ; Timestamp
BEAT_COMMAND ENDS

OVERRIDE_COMMAND STRUCT
    peer_id                     QWORD ?         ; Who overrode
    override_flags              DWORD ?
    parameter_values            REAL4 8 DUP(?)  ; Zoom, rot, etc
    duration_ms                 WORD ?          ; How long override lasts
OVERRIDE_COMMAND ENDS

CONFIG_COMMAND STRUCT
    change_type                 BYTE ?          ; ADD_PEER, REMOVE_PEER
    peer_id                     QWORD ?
    peer_address                SOCKADDR_IN <>
CONFIG_COMMAND ENDS

; ==============================================================================
; PREDICTIVE SYNC STRUCTURES
; ==============================================================================

PREDICTIVE_SYNC STRUCT 1024
    ; Clock synchronization
    local_clock_offset          QWORD ?         ; Offset from master
    local_clock_drift           REAL4 ?         ; PPM drift rate
    last_ntp_sync               QWORD ?
    
    ; Latency tracking
    latency_history             DWORD SYNC_HISTORY_SIZE DUP(?)
    history_head                BYTE ?
    history_tail                BYTE ?
    avg_latency_ms              DWORD ?
    jitter_ms                   DWORD ?         ; Standard deviation
    
    ; Beat prediction
    beat_predictor              QWORD ?         ; BEAT_PREDICTOR_ADVANCED ptr
    
    ; Synchronization state
    sync_mode                   BYTE ?          ; MASTER, SLAVE, P2P
    master_peer_id              QWORD ?
    sync_quality                REAL4 ?         ; 0-1 confidence
    
    ; Buffer management
    receive_buffer              QWORD ?         ; SYNC_BUFFER ptr
    playout_buffer              QWORD ?         ; SYNC_BUFFER ptr
    
    ; Time compensation
    time_stretch_ratio          REAL4 ?         ; Speed up/slow down audio
    max_time_stretch            REAL4 ?         ; 0.05 = 5% max
    
    ; Visual sync (separate from audio)
    visual_delay_ms             DWORD ?         ; Delay to sync with audio
    visual_frame_buffer         QWORD ?         ; Queued frames
PREDICTIVE_SYNC ENDS

SYNC_BUFFER STRUCT 512
    ; Circular buffer of incoming events
    entries                     QWORD ?         ; Array of SYNC_ENTRY
    head                        WORD ?
    tail                        WORD ?
    count                       WORD ?
    capacity                    WORD ?
    
    ; Playout timing
    target_delay_ms             DWORD ?         ; Based on jitter
    current_delay_ms            DWORD ?
    min_delay_ms                DWORD ?
    max_delay_ms                DWORD ?
SYNC_BUFFER ENDS

SYNC_ENTRY STRUCT 64
    event_time_local            QWORD ?         ; When we received it
    event_time_remote           QWORD ?         ; Sender's timestamp
    adjusted_time               QWORD ?         ; Compensated
    
    ; Event data
    event_type                  BYTE ?
    event_data                  BYTE 55 DUP(?)  ; Union of event types
    
    ; Prediction
    is_predicted                BYTE ?          ; Generated locally
    confidence                  REAL4 ?
    original_source             QWORD ?         ; Who sent (0 = local)
SYNC_ENTRY ENDS

BEAT_PREDICTOR_ADVANCED STRUCT 256
    ; History for pattern detection
    beat_times                  QWORD SYNC_HISTORY_SIZE DUP(?)
    beat_intervals              REAL4 SYNC_HISTORY_SIZE DUP(?)
    beat_intensities            REAL4 SYNC_HISTORY_SIZE DUP(?)
    
    ; Pattern state
    pattern_detected            BYTE ?          ; 0=irregular, 1=regular, 2=complex
    pattern_period              REAL4 ?         ; Base beat period
    pattern_variance            REAL4 ?         ; How regular
    
    ; Multi-scale analysis
    tempo_octaves               REAL4 4 DUP(?)  ; 0.5x, 1x, 2x, 4x tempo
    best_octave                 BYTE ?
    
    ; Prediction
    predicted_beats             QWORD 8 DUP(?)  ; Next 8 beat times
    prediction_confidence       REAL4 ?
    
    ; Error tracking
    prediction_errors           REAL4 16 DUP(?) ; Last 16 predictions vs actual
    error_drift                 REAL4 ?         ; Systematic bias
BEAT_PREDICTOR_ADVANCED ENDS

; ==============================================================================
; BYZANTINE FAULT TOLERANCE EXTENSIONS
; ==============================================================================

BFT_STATE STRUCT 256
    ; PBFT-style prepared certificates
    prepared_certificates       QWORD ?         ; Array of PREPARED_CERT
    cert_count                  DWORD ?
    
    ; View changes
    view_number                 QWORD ?
    view_changes                QWORD ?         ; Array of VIEW_CHANGE_MSG
    view_change_count           DWORD ?
    
    ; Checkpointing
    last_checkpoint             QWORD ?
    checkpoint_proof            QWORD ?
    
    ; Request deduplication
    processed_requests          QWORD ?         ; Array of timestamps
    request_head                WORD ?
BFT_STATE ENDS

PREPARED_CERT STRUCT
    view_number                 QWORD ?
    sequence_number             QWORD ?
    digest                      BYTE 32 DUP(?)  ; SHA256 of entry
    prepare_count               DWORD ?
    prepared                    BYTE ?
    committed                   BYTE ?
    commit_count                DWORD ?
PREPARED_CERT ENDS

; ==============================================================================
; CODE SECTION
; ==============================================================================
.CODE
ALIGN 64

; ==============================================================================
; RAFT CONSENSUS - Leader Election & Log Replication
; ==============================================================================

; -----------------------------------------------------------------------------
; RaftInit - Initialize consensus state
; RCX = state ptr
; Returns: RAX = RAFT_STATE ptr
; -----------------------------------------------------------------------------
RaftInit PROC FRAME
    push rbx
    push r12
    
    mov rbx, rcx
    
    ; Load persistent state from disk
    mov rcx, rbx
    call RaftLoadPersistentState
    
    ; Start as follower
    mov byte ptr [rbx].RAFT_STATE.state, RAFT_STATE_FOLLOWER
    
    ; Randomize election timeout
    call RandomInt
    mov edx, ELECTION_TIMEOUT_MAX_MS - ELECTION_TIMEOUT_MIN_MS
    and eax, edx
    add eax, ELECTION_TIMEOUT_MIN_MS
    mov [rbx].RAFT_STATE.election_timer, eax
    
    ; Initialize peer arrays
    mov rcx, [rbx].RAFT_STATE.peer_count
    imul rcx, 8
    call AlignedAlloc
    mov [rbx].RAFT_STATE.next_index, rax
    
    mov rcx, [rbx].RAFT_STATE.peer_count
    imul rcx, 8
    call AlignedAlloc
    mov [rbx].RAFT_STATE.match_index, rax
    
    ; Initialize log
    mov rcx, LOG_MAX_ENTRIES * SIZEOF LOG_ENTRY
    call AlignedAllocZeroed
    mov [rbx].RAFT_STATE.log, rax
    mov [rbx].RAFT_STATE.log_capacity, LOG_MAX_ENTRIES
    
    mov rax, rbx
    
    pop r12
    pop rbx
    ret
RaftInit ENDP

; -----------------------------------------------------------------------------
; RaftStartElection - Convert to candidate, request votes
; RCX = RAFT_STATE ptr
; -----------------------------------------------------------------------------
RaftStartElection PROC FRAME
    push rbx
    push r12
    push r13
    
    mov rbx, rcx
    
    ; Increment term
    inc qword ptr [rbx].RAFT_STATE.current_term
    
    ; Convert to candidate
    mov byte ptr [rbx].RAFT_STATE.state, RAFT_STATE_CANDIDATE
    
    ; Vote for self
    mov rax, [g_myPeerID]
    mov [rbx].RAFT_STATE.voted_for, rax
    mov [rbx].RAFT_STATE.votes_received, 1
    
    ; Reset election timer with randomization
    call RandomInt
    mov edx, ELECTION_TIMEOUT_MAX_MS - ELECTION_TIMEOUT_MIN_MS
    and eax, edx
    add eax, ELECTION_TIMEOUT_MIN_MS
    mov [rbx].RAFT_STATE.election_timer, eax
    
    call QueryPerformanceCounter
    mov [rbx].RAFT_STATE.last_heartbeat, rax
    
    ; Send RequestVote RPC to all peers
    xor r12d, r12d                  ; Peer index
@vote_loop:
    cmp r12d, [rbx].RAFT_STATE.peer_count
    jae @done
    
    ; Get peer info
    mov rax, [rbx].RAFT_STATE.peers
    imul r13, r12, SIZEOF RAFT_PEER
    add r13, rax
    
    ; Build RequestVote message and send
    mov rcx, rbx
    mov rdx, r13
    call RaftSendVoteRequest
    
    inc r12d
    jmp @vote_loop
    
@done:
    pop r13
    pop r12
    pop rbx
    ret
RaftStartElection ENDP

; -----------------------------------------------------------------------------
; RaftBecomeLeader - Won election, assume leadership
; RCX = RAFT_STATE ptr
; -----------------------------------------------------------------------------
RaftBecomeLeader PROC FRAME
    push rbx
    push r12
    
    mov rbx, rcx
    
    ; Update state
    mov byte ptr [rbx].RAFT_STATE.state, RAFT_STATE_LEADER
    
    ; Initialize leader state
    xor ecx, ecx
@init_peer_loop:
    cmp ecx, [rbx].RAFT_STATE.peer_count
    jae @peers_done
    
    mov rax, [rbx].RAFT_STATE.next_index
    mov edx, [rbx].RAFT_STATE.log_count
    mov [rax + rcx*8], rdx
    
    mov rax, [rbx].RAFT_STATE.match_index
    mov qword ptr [rax + rcx*8], 0
    
    inc ecx
    jmp @init_peer_loop
    
@peers_done:
    ; Send immediate heartbeat to establish authority
    mov rcx, rbx
    call RaftSendHeartbeats
    
    ; Notify application
    cmp [rbx].RAFT_STATE.on_leader_elected, 0
    je @no_callback
    
    call [rbx].RAFT_STATE.on_leader_elected
    
@no_callback:
    pop r12
    pop rbx
    ret
RaftBecomeLeader ENDP

; -----------------------------------------------------------------------------
; RaftReplicateLog - Leader sends entries to followers
; RCX = RAFT_STATE ptr, RDX = peer index
; -----------------------------------------------------------------------------
RaftReplicateLog PROC FRAME
    push rbx
    push r12
    push r13
    
    mov rbx, rcx                    ; RAFT_STATE
    mov r12d, edx                   ; Peer index
    
    ; Get next index for this peer
    mov rax, [rbx].RAFT_STATE.next_index
    mov r13, [rax + r12*8]          ; Next log index to send
    
    cmp r13, [rbx].RAFT_STATE.log_count
    jae @all_sent
    
    ; Get entries from log
    mov rax, [rbx].RAFT_STATE.log
    imul r13, SIZEOF LOG_ENTRY
    add rax, r13
    
    ; Send to peer
    mov rcx, rbx
    mov rdx, r12d
    mov r8, rax                     ; Log entry
    call RaftSendToPeer
    
    ; Update next_index on success would be handled in reply
    
@all_sent:
    pop r13
    pop r12
    pop rbx
    ret
RaftReplicateLog ENDP

; ==============================================================================
; PREDICTIVE SYNC - Time & Latency Compensation
; ==============================================================================

; -----------------------------------------------------------------------------
; PredictiveSyncInit - Initialize predictive synchronization
; Returns: RAX = PREDICTIVE_SYNC ptr
; -----------------------------------------------------------------------------
PredictiveSyncInit PROC FRAME
    push rbx
    push r12
    
    mov ecx, SIZEOF PREDICTIVE_SYNC
    call AlignedAllocZeroed
    mov rbx, rax
    
    ; Default parameters
    movss xmm0, [flt_0_05]
    movss [rbx].PREDICTIVE_SYNC.max_time_stretch, xmm0
    mov [rbx].PREDICTIVE_SYNC.target_delay_ms, 100
    
    ; Initialize beat predictor
    mov ecx, SIZEOF BEAT_PREDICTOR_ADVANCED
    call AlignedAllocZeroed
    mov [rbx].PREDICTIVE_SYNC.beat_predictor, rax
    
    ; Allocate receive buffer
    mov ecx, SIZEOF SYNC_BUFFER
    call AlignedAllocZeroed
    mov r12, rax
    
    ; Initialize receive buffer
    mov ecx, PREDICT_BUFFER_SIZE * SIZEOF SYNC_ENTRY
    call AlignedAllocZeroed
    mov [r12].SYNC_BUFFER.entries, rax
    mov [r12].SYNC_BUFFER.capacity, PREDICT_BUFFER_SIZE
    
    mov [rbx].PREDICTIVE_SYNC.receive_buffer, r12
    
    ; Allocate playout buffer
    mov ecx, SIZEOF SYNC_BUFFER
    call AlignedAllocZeroed
    mov r12, rax
    
    ; Initialize playout buffer
    mov ecx, PREDICT_BUFFER_SIZE * SIZEOF SYNC_ENTRY
    call AlignedAllocZeroed
    mov [r12].SYNC_BUFFER.entries, rax
    mov [r12].SYNC_BUFFER.capacity, PREDICT_BUFFER_SIZE
    
    mov [rbx].PREDICTIVE_SYNC.playout_buffer, r12
    
    mov rax, rbx
    
    pop r12
    pop rbx
    ret
PredictiveSyncInit ENDP

; -----------------------------------------------------------------------------
; GeneratePrediction - Create synthetic event when real one is late
; RCX = PREDICTIVE_SYNC, RDX = expected event (SYNC_ENTRY ptr)
; -----------------------------------------------------------------------------
GeneratePrediction PROC FRAME
    push rbx
    push r12
    push r13
    
    mov rbx, rcx                    ; PREDICTIVE_SYNC
    mov r12, rdx                    ; Expected event
    
    ; Allocate new entry
    mov ecx, SIZEOF SYNC_ENTRY
    call AlignedAlloc
    mov r13, rax                    ; New entry
    
    ; Copy event structure
    mov rsi, r12
    mov rdi, r13
    mov ecx, SIZEOF SYNC_ENTRY
    call memcpy
    
    ; Mark as predicted
    mov byte ptr [r13].SYNC_ENTRY.is_predicted, 1
    movss xmm0, [flt_0_7]
    movss [r13].SYNC_ENTRY.confidence, xmm0
    
    ; Generate based on type
    movzx eax, byte ptr [r13].SYNC_ENTRY.event_type
    
    cmp al, 2                       ; EVENT_BEAT
    je @predict_beat
    cmp al, 1                       ; EVENT_PRESET_CHANGE
    je @predict_preset
    jmp @insert
    
@predict_beat:
    ; Use beat predictor to generate next beat
    mov rcx, [rbx].PREDICTIVE_SYNC.beat_predictor
    call BeatPredictorGetNextPredicted
    jmp @insert
    
@predict_preset:
    ; Use AI curation to predict likely next preset
    cmp [g_pCurationState], 0
    je @insert
    
    mov rcx, [g_pCurationState]
    call CurationSelectNext
    
@insert:
    ; Insert into playout buffer
    mov rcx, [rbx].PREDICTIVE_SYNC.playout_buffer
    mov rdx, r13
    call SyncBufferInsertSorted
    
    pop r13
    pop r12
    pop rbx
    ret
GeneratePrediction ENDP

; -----------------------------------------------------------------------------
; BeatPredictorAnalyze - Multi-scale tempo detection
; RCX = BEAT_PREDICTOR_ADVANCED ptr
; -----------------------------------------------------------------------------
BeatPredictorAnalyze PROC FRAME
    push rbx
    push r12
    push r13
    
    mov rbx, rcx                    ; BEAT_PREDICTOR_ADVANCED
    
    ; Calculate intervals from history
    mov ecx, [rbx].BEAT_PREDICTOR_ADVANCED.beat_times
    cmp ecx, 3                      ; Need at least 4 beats
    jb @not_enough_data
    
    ; Compute intervals
    xor edx, edx                    ; Interval index
@interval_loop:
    mov eax, ecx
    dec eax
    and eax, SYNC_HISTORY_SIZE - 1
    mov r8, [rbx].BEAT_PREDICTOR_ADVANCED.beat_times[rax*8]
    
    mov eax, ecx
    sub eax, 2
    and eax, SYNC_HISTORY_SIZE - 1
    mov r9, [rbx].BEAT_PREDICTOR_ADVANCED.beat_times[rax*8]
    
    sub r8, r9                      ; Interval in ticks
    cvtsi2ss xmm0, r8d
    divss xmm0, [flt_qpc_freq]      ; Convert to seconds
    
    movss [rbx].BEAT_PREDICTOR_ADVANCED.beat_intervals[rdx*4], xmm0
    
    inc edx
    dec ecx
    cmp edx, 16                     ; Analyze last 16 intervals
    jb @interval_loop
    
    ; Calculate confidence based on variance
    movss xmm0, [flt_1_0]
    movss [rbx].BEAT_PREDICTOR_ADVANCED.prediction_confidence, xmm0
    
    ; Generate future predictions
    call QueryPerformanceCounter
    mov r8, rax                     ; Last beat time
    
    movss xmm3, [rbx].BEAT_PREDICTOR_ADVANCED.pattern_period
    mov ecx, 8                      ; Predict 8 beats ahead
@predict_loop:
    cvtsi2ss xmm0, ecx
    mulss xmm0, xmm3                ; n * period
    
    cvtsi2ss xmm1, r8d
    addss xmm0, xmm1                ; last_beat + n*period
    
    cvtss2si eax, xmm0
    mov [rbx].BEAT_PREDICTOR_ADVANCED.predicted_beats[rcx*8 - 8], rax
    
    dec ecx
    jnz @predict_loop
    
@not_enough_data:
    pop r13
    pop r12
    pop rbx
    ret
BeatPredictorAnalyze ENDP

; -----------------------------------------------------------------------------
; SyncBufferInsertSorted - Insert event maintaining time order
; RCX = SYNC_BUFFER ptr, RDX = SYNC_ENTRY to insert
; -----------------------------------------------------------------------------
SyncBufferInsertSorted PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rcx                    ; SYNC_BUFFER
    mov r12, rdx                    ; SYNC_ENTRY to insert
    
    ; Check if buffer is full
    mov ax, [rbx].SYNC_BUFFER.count
    cmp ax, [rbx].SYNC_BUFFER.capacity
    jae @buffer_full
    
    ; Find insertion point
    xor r13d, r13d                  ; Insert position
    xor r14d, r14d                  ; Scan index
    
@scan_loop:
    cmp r14d, [rbx].SYNC_BUFFER.count
    jae @found_position
    
    ; Get entry at scan position
    mov rax, [rbx].SYNC_BUFFER.entries
    imul r8, r14, SIZEOF SYNC_ENTRY
    add r8, rax
    
    ; Compare timestamps
    mov r8, [r8].SYNC_ENTRY.adjusted_time
    mov r9, [r12].SYNC_ENTRY.adjusted_time
    
    cmp r9, r8
    jb @found_position
    
    mov r13d, r14d
    inc r13d
    inc r14d
    jmp @scan_loop
    
@found_position:
    ; Shift entries if needed
    mov eax, [rbx].SYNC_BUFFER.count
    cmp eax, r13d
    je @direct_insert
    
    ; Shift from count-1 down to insert position
    mov ecx, eax
    dec ecx
@shift_loop:
    cmp ecx, r13d
    jl @shift_done
    
    mov rax, [rbx].SYNC_BUFFER.entries
    imul r8, rcx, SIZEOF SYNC_ENTRY
    imul r9, rcx, SIZEOF SYNC_ENTRY
    add r9, SIZEOF SYNC_ENTRY
    add r8, rax
    add r9, rax
    
    mov r10d, SIZEOF SYNC_ENTRY
    call memmove
    
    dec ecx
    jmp @shift_loop
    
@shift_done:
@direct_insert:
    ; Insert at position
    mov rax, [rbx].SYNC_BUFFER.entries
    imul r8, r13, SIZEOF SYNC_ENTRY
    add r8, rax
    
    mov rsi, r12
    mov rdi, r8
    mov ecx, SIZEOF SYNC_ENTRY
    rep movsb
    
    inc word ptr [rbx].SYNC_BUFFER.count
    
@buffer_full:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
SyncBufferInsertSorted ENDP

; ==============================================================================
; BYZANTINE FAULT TOLERANCE - Handle malicious/faulty nodes
; ==============================================================================

; -----------------------------------------------------------------------------
; BFTVerifyEntry - Check entry has sufficient prepare certificates
; RCX = BFT_STATE ptr, RDX = LOG_ENTRY ptr
; Returns: EAX = 1 if verified, 0 otherwise
; -----------------------------------------------------------------------------
BFTVerifyEntry PROC FRAME
    push rbx
    push r12
    
    mov rbx, rcx                    ; BFT_STATE
    mov r12, rdx                    ; LOG_ENTRY
    
    ; Get certificate for this entry
    mov rax, [rbx].BFT_STATE.prepared_certificates
    mov ecx, [r12].LOG_ENTRY.index
    imul ecx, SIZEOF PREPARED_CERT
    add rax, rcx
    
    ; Check if we have 2F+1 prepares
    cmp [rax].PREPARED_CERT.prepare_count, BFT_QUORUM
    jb @not_prepared
    
    cmp [rax].PREPARED_CERT.prepared, 1
    jne @not_prepared
    
    mov byte ptr [r12].LOG_ENTRY.is_committed, 1
    mov eax, 1
    jmp @done
    
@not_prepared:
    xor eax, eax
    
@done:
    pop r12
    pop rbx
    ret
BFTVerifyEntry ENDP

; -----------------------------------------------------------------------------
; BFTHandleViewChange - New leader election with BFT
; RCX = BFT_STATE ptr
; Returns: EAX = 1 if successful, 0 if failed
; -----------------------------------------------------------------------------
BFTHandleViewChange PROC FRAME
    push rbx
    
    mov rbx, rcx                    ; BFT_STATE
    
    ; Increment view number
    inc qword ptr [rbx].BFT_STATE.view_number
    
    ; Reset prepare certificates
    mov rax, [rbx].BFT_STATE.prepared_certificates
    mov ecx, LOG_MAX_ENTRIES * SIZEOF PREPARED_CERT
    xor edx, edx
    
@clear_loop:
    cmp edx, ecx
    jae @clear_done
    
    mov [rax + rdx], 0
    add edx, 8
    jmp @clear_loop
    
@clear_done:
    mov eax, 1
    
    pop rbx
    ret
BFTHandleViewChange ENDP

; ==============================================================================
; STUB IMPLEMENTATIONS - To be completed as needed
; ==============================================================================

RaftLoadPersistentState PROC
    ret
RaftLoadPersistentState ENDP

RaftSendVoteRequest PROC
    ret
RaftSendVoteRequest ENDP

RaftSendHeartbeats PROC
    ret
RaftSendHeartbeats ENDP

RaftSendToPeer PROC
    ret
RaftSendToPeer ENDP

RandomInt PROC
    mov eax, 12345                  ; Placeholder
    ret
RandomInt ENDP

AlignedAlloc PROC
    push rbx
    mov rbx, rcx
    add rbx, 15
    and rbx, -16
    call VirtualAlloc
    pop rbx
    ret
AlignedAlloc ENDP

AlignedAllocZeroed PROC
    call AlignedAlloc
    test rax, rax
    jz @done
    mov rcx, rax
    xor r8, r8
    mov r9d, 0
@zero_loop:
    cmp r8, [rsp - 8]
    jae @done
    mov qword ptr [rcx + r8], 0
    add r8, 8
    jmp @zero_loop
@done:
    ret
AlignedAllocZeroed ENDP

memcpy PROC
    ; rdi = dest, rsi = src, ecx = count
    push rdi
    push rsi
    xor eax, eax
@copy_loop:
    cmp eax, ecx
    jae @done
    mov bl, [rsi + rax]
    mov [rdi + rax], bl
    inc eax
    jmp @copy_loop
@done:
    pop rsi
    pop rdi
    ret
memcpy ENDP

memmove PROC
    ; Same as memcpy for now
    call memcpy
    ret
memmove ENDP

CurationSelectNext PROC
    xor eax, eax
    ret
CurationSelectNext ENDP

BeatPredictorGetNextPredicted PROC
    ret
BeatPredictorGetNextPredicted ENDP

; ==============================================================================
; EXPORTS
; ==============================================================================
PUBLIC RaftInit
PUBLIC RaftStartElection
PUBLIC RaftBecomeLeader
PUBLIC RaftReplicateLog
PUBLIC PredictiveSyncInit
PUBLIC GeneratePrediction
PUBLIC BeatPredictorAnalyze
PUBLIC SyncBufferInsertSorted
PUBLIC BFTVerifyEntry
PUBLIC BFTHandleViewChange

; ==============================================================================
; DATA SECTION
; ==============================================================================
.DATA
ALIGN 64

; Global state (referenced by code)
g_myPeerID                  QWORD 0
g_pCurationState            QWORD 0

; Raft message types
RAFT_MSG_REQUEST_VOTE       EQU 1
RAFT_MSG_VOTE_GRANTED       EQU 2
RAFT_MSG_VOTE_DENIED        EQU 3
RAFT_MSG_APPEND_ENTRIES     EQU 4
RAFT_MSG_APPEND_REPLY       EQU 5
RAFT_MSG_INSTALL_SNAPSHOT   EQU 6

; Event types
EVENT_PRESET_CHANGE         EQU 1
EVENT_BEAT                  EQU 2
EVENT_OVERRIDE              EQU 3
EVENT_CONFIG_CHANGE         EQU 4

; Float constants
flt_0_0                     REAL4 0.0
flt_0_05                    REAL4 0.05
flt_0_5                     REAL4 0.5
flt_0_7                     REAL4 0.7
flt_1_0                     REAL4 1.0
flt_max_slew                REAL4 10000000.0    ; 1ms in QPC
flt_qpc_freq                REAL4 10000000.0    ; QPC frequency
flt_abs_mask                DWORD 7FFFFFFFh

; ==============================================================================
END
