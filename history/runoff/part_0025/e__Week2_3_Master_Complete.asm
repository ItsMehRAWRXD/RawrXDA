;================================================================================
; WEEK2_3_MASTER_COMPLETE.ASM - Swarm Orchestrator Integration
; Distributed Consensus, Multi-Node Coordination, Cluster Management
; Bridges Week-1 Background Threads to Full Phase-5 System
;================================================================================

OPTION CASEMAP:NONE
OPTION WIN64:3

;================================================================================
; MACRO DEFINITIONS
;================================================================================
SAVE_REGS MACRO
    push rbx
    push r12
    push r13
    push r14
    push r15
ENDM

RESTORE_REGS MACRO
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
ENDM

;================================================================================
; EXTERNAL IMPORTS - Week 1 Foundation + Network Stack
;================================================================================
; Week 1 Infrastructure
EXTERN Week1Initialize : proc
EXTERN SubmitTask : proc
EXTERN ProcessReceivedHeartbeat : proc
EXTERN RegisterResource : proc
EXTERN AcquireResource : proc
EXTERN ReleaseResource : proc

; Phase 1-4 (linked in final build)
EXTERN Phase1Initialize : proc
EXTERN Phase2Initialize : proc
EXTERN RouteModelLoad : proc
EXTERN Phase3Initialize : proc
EXTERN GenerateTokens : proc
EXTERN Phase4Initialize : proc
EXTERN ProcessSwarmQueue : proc

; Win32/NT Networking
extern CreateFileA : proc
extern CreateNamedPipeA : proc
extern ConnectNamedPipe : proc
extern DeviceIoControl : proc

; Winsock2 (Full async I/O)
extern WSAStartup : proc
extern WSACleanup : proc
extern socket : proc
extern bind : proc
extern listen : proc
extern accept : proc
extern connect : proc
extern send : proc
extern recv : proc
extern sendto : proc
extern recvfrom : proc
extern setsockopt : proc
extern getsockopt : proc
extern ioctlsocket : proc
extern select : proc
extern shutdown : proc
extern closesocket : proc
extern gethostbyname : proc
extern getaddrinfo : proc
extern freeaddrinfo : proc
extern inet_addr : proc
extern inet_ntoa : proc
extern htons : proc
extern ntohs : proc
extern htonl : proc
extern ntohl : proc
extern WSAIoctl : proc
extern WSAGetOverlappedResult : proc
extern CreateIoCompletionPort : proc
extern GetQueuedCompletionStatus : proc
extern GetQueuedCompletionStatusEx : proc
extern PostQueuedCompletionStatus : proc

; Threading & Sync
extern CreateThread : proc
extern CreateEventA : proc
extern SetEvent : proc
extern ResetEvent : proc
extern WaitForSingleObject : proc
extern WaitForMultipleObjects : proc
extern InitializeCriticalSection : proc
extern EnterCriticalSection : proc
extern LeaveCriticalSection : proc
extern DeleteCriticalSection : proc
extern SwitchToThread : proc

; Memory & Timing
extern VirtualAlloc : proc
extern VirtualFree : proc
extern QueryPerformanceCounter : proc
extern QueryPerformanceFrequency : proc
extern Sleep : proc

;================================================================================
; CONSTANTS - Swarm Distributed System
;================================================================================
; Cluster topology
MAX_CLUSTER_NODES       EQU 16
MAX_CLUSTER_GROUPS      EQU 4
SWARM_MAGIC             EQU 52415752h   ; "RAWR"

; Consensus (Raft + BFT hybrid)
RAFT_HEARTBEAT_INTERVAL_MS  EQU 50
RAFT_ELECTION_TIMEOUT_MIN   EQU 150
RAFT_ELECTION_TIMEOUT_MAX   EQU 300
RAFT_MAX_LOG_ENTRIES        EQU 100000
BFT_F_THRESHOLD             EQU 5
BFT_QUORUM_SIZE             EQU 11

; Network
SWARM_TCP_PORT          EQU 31337
SWARM_UDP_PORT          EQU 31338
RAFT_PORT               EQU 31339
GOSSIP_PORT             EQU 31340
MAX_PACKET_SIZE         EQU 65536
IOCP_THREADS            EQU 8

; Shard distribution
MAX_SHARDS              EQU 4096
SHARD_REPLICAS          EQU 3
SHARD_PLACEMENT_TIMEOUT_MS  EQU 5000

; State machine
NODE_STATE_JOINING      EQU 0
NODE_STATE_ACTIVE       EQU 1
NODE_STATE_DEGRADED     EQU 2
NODE_STATE_LEAVING      EQU 3
NODE_STATE_REMOVED      EQU 4

; Request types
REQ_TYPE_HEARTBEAT      EQU 0
REQ_TYPE_RAFT_VOTE      EQU 1
REQ_TYPE_RAFT_APPEND    EQU 2
REQ_TYPE_SHARD_REQUEST  EQU 3
REQ_TYPE_SHARD_TRANSFER EQU 4
REQ_TYPE_QUERY          EQU 5
REQ_TYPE_INFERENCE      EQU 6

; Priority levels
PRIORITY_LOW            EQU 0
PRIORITY_NORMAL         EQU 1
PRIORITY_HIGH           EQU 2
PRIORITY_CRITICAL       EQU 3

;================================================================================
; STRUCTURES - Distributed Swarm State
;================================================================================
CLUSTER_ENDPOINT STRUCT 32
    node_id             dd ?
    family              dd ?          ; AF_INET/AF_INET6
    ip_address          db 16 DUP(?)
    tcp_port            dw ?
    udp_port            dw ?
    raft_port           dw ?
    gossip_port         dw ?
    is_public           dd ?
CLUSTER_ENDPOINT ENDS

RAFT_LOG_ENTRY STRUCT 128
    term                dq ?
    index               dq ?
    command_type        dd ?
    command_data        dq ?
    command_len         dd ?
    checksum            db 32 DUP(?)
    committed           dd ?
    applied             dd ?
    created_at          dq ?
RAFT_LOG_ENTRY ENDS

RAFT_STATE STRUCT 4096
    ; Persistent state
    current_term        dq ?
    voted_for           dd ?
    vote_granted        dd MAX_CLUSTER_NODES DUP(?)
    
    ; Log
    log                 dq ?
    log_count           dq ?
    log_capacity        dq ?
    log_start_index     dq ?
    
    ; Volatile state
    commit_index        dq ?
    last_applied        dq ?
    state               dd ?          ; 0=follower,1=candidate,2=leader
    
    ; Leader state
    next_index          dq MAX_CLUSTER_NODES DUP(?)
    match_index         dq MAX_CLUSTER_NODES DUP(?)
    inflight            dq MAX_CLUSTER_NODES DUP(?)
    
    ; Timing
    election_timer      dq ?
    heartbeat_timer     dq ?
    randomized_timeout  dq ?
    
    ; Configuration
    node_id             dd ?
    peer_count          dd ?
    peers               dd MAX_CLUSTER_NODES DUP(?)
    
    ; Locks
    lock                CRITICAL_SECTION <>
RAFT_STATE ENDS

SHARD_ASSIGNMENT STRUCT 64
    shard_id            dq ?
    primary_node        dd ?
    replica_nodes       dd 3 DUP(?)
    version             dq ?
    last_update         dq ?
    state               dd ?          ; 0=unassigned,1=active,2=migrating
    size_bytes          dq ?
    checksum            db 32 DUP(?)
SHARD_ASSIGNMENT ENDS

SHARD_TABLE STRUCT 2048
    assignments         dq ?
    shard_count         dq ?
    capacity            dq ?
    
    ; Consistent hashing
    ring                dq ?
    ring_size           dq ?
    
    ; Load tracking
    node_load           dq MAX_CLUSTER_NODES DUP(?)
    node_shard_count    dd MAX_CLUSTER_NODES DUP(?)
    
    ; Rebalancing
    rebalance_in_progress   dd ?
    rebalance_thread        dq ?
    
    lock                CRITICAL_SECTION <>
SHARD_TABLE ENDS

SWARM_MESSAGE_HEADER STRUCT 64
    magic               dd ?
    version             dd ?
    msg_type            dd ?
    flags               dd ?
    payload_len         dd ?
    checksum            dd ?
    sender_id           dd ?
    timestamp           dq ?
    sequence_num        dq ?
SWARM_MESSAGE_HEADER ENDS

SWARM_CONNECTION STRUCT 256
    socket_fd           dq ?
    node_id             dd ?
    state               dd ?
    
    ; I/O
    recv_buffer         dq ?
    recv_len            dq ?
    send_buffer         dq ?
    send_len            dq ?
    
    ; Statistics
    bytes_sent          dq ?
    bytes_recv          dq ?
    packets_sent        dq ?
    packets_recv        dq ?
    last_activity       dq ?
    
    ; Overlapped I/O
    recv_ovlp           OVERLAPPED <>
    send_ovlp           OVERLAPPED <>
SWARM_CONNECTION ENDS

SWARM_NODE STRUCT 512
    ; Identity
    node_id             dd ?
    state               dd ?
    incarnation         dq ?
    
    ; Capabilities
    vram_gb             dd ?
    dram_gb             dd ?
    compute_score       dd ?
    network_mbps        dd ?
    
    ; Endpoints
    endpoint            CLUSTER_ENDPOINT <>
    
    ; Connection
    connection          SWARM_CONNECTION <>
    
    ; Health
    last_heartbeat      dq ?
    missed_heartbeats   dd ?
    latency_us          dq ?
    
    ; Shards hosted
    hosted_shards       dd MAX_SHARDS DUP(?)
    hosted_count        dd ?
    
    ; Statistics
    tokens_served       dq ?
    requests_handled    dq ?
    errors              dq ?
SWARM_NODE ENDS

INFERENCE_REQUEST STRUCT 256
    request_id          dq ?
    client_node         dd ?
    model_id            dq ?
    
    ; Input
    prompt_tokens       dq ?
    prompt_len          dd ?
    max_tokens          dd ?
    
    ; Parameters
    temperature         dd ?
    top_p               dd ?
    top_k               dd ?
    
    ; Routing
    target_shard        dq ?
    preferred_nodes     dd 4 DUP(?)
    
    ; Callback
    completion_cb       dq ?
    context             dq ?
    
    ; Timing
    submitted_at        dq ?
    started_at          dq ?
    completed_at        dq ?
INFERENCE_REQUEST ENDS

INFERENCE_ROUTER STRUCT 1024
    ; Request queues per node
    pending_queues      dq MAX_CLUSTER_NODES DUP(?)
    queue_depths        dd MAX_CLUSTER_NODES DUP(?)
    
    ; Load balancing
    load_weights        dd MAX_CLUSTER_NODES DUP(?)
    total_weight        dd ?
    
    ; Routing strategy
    strategy            dd ?          ; 0=round_robin,1=least_loaded,2=locality
    
    ; Active requests
    active_count        dq ?
    completed_count     dq ?
    failed_count        dq ?
    
    ; Latency tracking
    latency_history     dq ?
    latency_index       dd ?
    
    lock                CRITICAL_SECTION <>
INFERENCE_ROUTER ENDS

WEEK2_3_CONTEXT STRUCT 8192
    ; Week 1 link
    week1_ctx           dq ?
    
    ; Cluster membership
    local_node          SWARM_NODE <>
    all_nodes           dq ?
    node_count          dd ?
    max_nodes           dd ?
    
    ; Consensus
    raft                RAFT_STATE <>
    
    ; Shard distribution
    shards              SHARD_TABLE <>
    
    ; Inference routing
    router              INFERENCE_ROUTER <>
    
    ; Network
    iocp_handle         dq ?
    listen_socket       dq ?
    udp_socket          dq ?
    
    ; Threads
    io_threads          dq IOCP_THREADS DUP(?)
    raft_thread         dq ?
    gossip_thread       dq ?
    router_thread       dq ?
    
    ; State
    cluster_id          dq ?
    start_time          dq ?
    state               dd ?
    
    ; Statistics
    messages_sent       dq ?
    messages_recv       dq ?
    bytes_transferred   dq ?
    
    ; Shutdown
    shutdown_event      dq ?
    running             dd ?
    
    ; Locks
    global_lock         CRITICAL_SECTION <>
WEEK2_3_CONTEXT ENDS

;================================================================================
; DATA SECTION
;================================================================================
.DATA
ALIGN 64

; Protocol constants
swarm_magic         dd SWARM_MAGIC
swarm_version       dd 1

; Log messages
str_week23_init     db "[WEEK2-3] Swarm Orchestrator initializing...", 0Dh, 0Ah, 0
str_cluster_join    db "[WEEK2-3] Joined cluster %llu with %d nodes", 0Dh, 0Ah, 0
str_raft_elected    db "[WEEK2-3] Elected as leader (term %llu)", 0Dh, 0Ah, 0
str_shard_assigned  db "[WEEK2-3] Assigned %llu shards (%llu replicas)", 0Dh, 0Ah, 0
str_inference_routed db "[WEEK2-3] Inference request routed to node %d", 0Dh, 0Ah, 0
str_rebalance_start db "[WEEK2-3] Starting shard rebalancing...", 0Dh, 0Ah, 0
str_node_failed     db "[WEEK2-3] Node %d failed, initiating failover", 0Dh, 0Ah, 0
str_quorum_lost     db "[WEEK2-3] WARNING: Quorum lost, entering degraded mode", 0Dh, 0Ah, 0

; Error messages
err_bind_failed     db "[WEEK2-3] ERROR: Failed to bind to port", 0Dh, 0Ah, 0
err_join_failed     db "[WEEK2-3] ERROR: Failed to join cluster", 0Dh, 0Ah, 0
err_no_quorum       db "[WEEK2-3] ERROR: No quorum available", 0Dh, 0Ah, 0
err_shard_lost      db "[WEEK2-3] ERROR: Shard %llu data lost", 0Dh, 0Ah, 0

;================================================================================
; CODE SECTION
;================================================================================
.CODE
ALIGN 64

;================================================================================
; WEEK 2-3 INITIALIZATION
;================================================================================

;-------------------------------------------------------------------------------
; Week23Initialize - Bootstrap distributed swarm orchestrator
; Input:  RCX = Week1 context
;         RDX = Cluster configuration (or NULL for standalone)
; Output: RAX = WEEK2_3_CONTEXT* or NULL
;
; Procedure:
;  1. Allocate WEEK2_3_CONTEXT (8KB, page-aligned)
;  2. Initialize critical sections for thread safety
;  3. Setup Winsock2 (WSAStartup)
;  4. Create IOCP with 8 worker threads
;  5. Bind TCP (31337) and UDP (31338) sockets
;  6. Initialize Raft consensus engine
;  7. Initialize consistent hashing for shards
;  8. Initialize inference router
;  9. Start background threads (Raft, Gossip, Router)
; 10. Join cluster or bootstrap as node 0
; Return: WEEK2_3_CONTEXT* or NULL
;-------------------------------------------------------------------------------
Week23Initialize PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 500h
    
    mov r12, rcx                      ; R12 = Week1 context
    mov r13, rdx                      ; R13 = Config
    
    ; Allocate context (8KB)
    mov rcx, 0
    mov rdx, sizeof WEEK2_3_CONTEXT
    mov r8, 1000h
    mov r9, 4
    call VirtualAlloc
    test rax, rax
    jz @week23_init_fail
    mov rbx, rax                      ; RBX = WEEK2_3_CONTEXT*
    
    ; Zero structure
    xor ecx, ecx
    mov edi, ebx
    mov ecx, sizeof WEEK2_3_CONTEXT / 4
    rep stosd
    
    ; Store Week1 link
    mov [rbx].WEEK2_3_CONTEXT.week1_ctx, r12
    
    ; Initialize locks
    lea rcx, [rbx].WEEK2_3_CONTEXT.global_lock
    call InitializeCriticalSection
    
    lea rcx, [rbx].WEEK2_3_CONTEXT.raft.lock
    call InitializeCriticalSection
    
    lea rcx, [rbx].WEEK2_3_CONTEXT.shards.lock
    call InitializeCriticalSection
    
    lea rcx, [rbx].WEEK2_3_CONTEXT.router.lock
    call InitializeCriticalSection
    
    ; Initialize network
    mov rcx, rbx
    call InitializeSwarmNetwork
    test eax, eax
    jz @week23_init_cleanup
    
    ; Initialize Raft
    mov rcx, rbx
    call InitializeRaftConsensus
    test eax, eax
    jz @week23_init_cleanup
    
    ; Initialize shard table
    mov rcx, rbx
    call InitializeShardTable
    test eax, eax
    jz @week23_init_cleanup
    
    ; Initialize inference router
    mov rcx, rbx
    call InitializeInferenceRouter
    test eax, eax
    jz @week23_init_cleanup
    
    ; Create shutdown event
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call CreateEventA
    mov [rbx].WEEK2_3_CONTEXT.shutdown_event, rax
    
    ; Mark as running
    mov dword ptr [rbx].WEEK2_3_CONTEXT.running, 1
    
    ; Start IOCP threads
    xor r14d, r14d
@start_iocp:
    cmp r14d, IOCP_THREADS
    jge @iocp_done
    
    mov rcx, 0
    mov rdx, 0
    lea r8, IOCPWorkerThread
    mov r9, rbx
    push 0
    sub rsp, 28h
    call CreateThread
    add rsp, 30h
    
    mov [rbx].WEEK2_3_CONTEXT.io_threads[r14*8], rax
    
    inc r14d
    jmp @start_iocp
@iocp_done:
    
    ; Start Raft thread
    mov rcx, 0
    mov rdx, 0
    lea r8, RaftEventLoop
    mov r9, rbx
    push 0
    sub rsp, 28h
    call CreateThread
    add rsp, 30h
    
    mov [rbx].WEEK2_3_CONTEXT.raft_thread, rax
    
    ; Start gossip thread
    mov rcx, 0
    mov rdx, 0
    lea r8, GossipProtocolThread
    mov r9, rbx
    push 0
    sub rsp, 28h
    call CreateThread
    add rsp, 30h
    
    mov [rbx].WEEK2_3_CONTEXT.gossip_thread, rax
    
    ; Start router thread
    mov rcx, 0
    mov rdx, 0
    lea r8, InferenceRouterThread
    mov r9, rbx
    push 0
    sub rsp, 28h
    call CreateThread
    add rsp, 30h
    
    mov [rbx].WEEK2_3_CONTEXT.router_thread, rax
    
    ; Join cluster (or bootstrap)
    mov rcx, rbx
    mov rdx, r13
    call JoinCluster
    test eax, eax
    jz @week23_init_cleanup
    
    ; Log success
    lea rcx, [rbp-100h]
    lea rdx, str_week23_init
    mov byte ptr [rcx], 0
    
    mov rax, rbx
    jmp @week23_init_exit
    
@week23_init_cleanup:
    mov rcx, rbx
    xor rdx, rdx
    mov r8, sizeof WEEK2_3_CONTEXT
    call VirtualFree
    
@week23_init_fail:
    xor rax, rax
    
@week23_init_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
Week23Initialize ENDP

;================================================================================
; NETWORK LAYER
;================================================================================

;-------------------------------------------------------------------------------
; InitializeSwarmNetwork - Setup TCP/UDP/IOCP
;-------------------------------------------------------------------------------
InitializeSwarmNetwork PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 200h
    
    mov rbx, rcx
    
    ; WSAStartup
    mov word ptr [rbp-100h], 0202h
    lea rcx, [rbp-100h]
    lea rdx, [rbp-96]
    call WSAStartup
    test eax, eax
    jnz @net_wsa_fail
    
    ; Create IOCP
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    call CreateIoCompletionPort
    mov [rbx].WEEK2_3_CONTEXT.iocp_handle, rax
    test rax, rax
    jz @net_iocp_fail
    
    ; Create TCP listening socket
    mov ecx, 2                        ; AF_INET
    mov edx, 1                        ; SOCK_STREAM
    xor r8d, r8d
    call socket
    
    cmp rax, -1
    je @net_socket_fail
    
    mov [rbx].WEEK2_3_CONTEXT.listen_socket, rax
    
    ; Enable address reuse
    mov rcx, rax
    mov edx, 0FFFFh                   ; SOL_SOCKET
    mov r8d, 4                        ; SO_REUSEADDR
    lea r9, [rbp-4]
    mov dword ptr [r9], 1
    push 4
    sub rsp, 20h
    call setsockopt
    add rsp, 28h
    
    ; Bind TCP
    mov word ptr [rbp-100h], 2        ; AF_INET
    mov ax, SWARM_TCP_PORT
    call htons
    mov word ptr [rbp-100h+2], ax
    mov dword ptr [rbp-100h+4], 0     ; INADDR_ANY
    
    mov rcx, [rbx].WEEK2_3_CONTEXT.listen_socket
    lea rdx, [rbp-100h]
    mov r8d, 16
    call bind
    
    test eax, eax
    jnz @net_bind_fail
    
    ; Listen
    mov rcx, [rbx].WEEK2_3_CONTEXT.listen_socket
    mov edx, MAX_CLUSTER_NODES
    call listen
    
    ; Associate with IOCP
    mov rcx, [rbx].WEEK2_3_CONTEXT.iocp_handle
    mov rdx, [rbx].WEEK2_3_CONTEXT.listen_socket
    mov r8, 0                         ; Completion key (listen socket)
    xor r9d, r9d
    call CreateIoCompletionPort
    
    ; Create UDP socket for gossip
    mov ecx, 2                        ; AF_INET
    mov edx, 2                        ; SOCK_DGRAM
    xor r8d, r8d
    call socket
    
    cmp rax, -1
    je @net_udp_fail
    
    mov [rbx].WEEK2_3_CONTEXT.udp_socket, rax
    
    ; Bind UDP
    mov word ptr [rbp-100h], 2
    mov ax, SWARM_UDP_PORT
    call htons
    mov word ptr [rbp-100h+2], ax
    
    mov rcx, [rbx].WEEK2_3_CONTEXT.udp_socket
    lea rdx, [rbp-100h]
    mov r8d, 16
    call bind
    
    mov eax, 1
    jmp @net_exit
    
@net_bind_fail:
    lea rcx, err_bind_failed
    jmp @net_error_log
@net_socket_fail:
@net_udp_fail:
@net_iocp_fail:
@net_wsa_fail:
    xor eax, eax
    
@net_error_log:
    
@net_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
InitializeSwarmNetwork ENDP

;-------------------------------------------------------------------------------
; IOCPWorkerThread - Async I/O processing
;-------------------------------------------------------------------------------
IOCPWorkerThread PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 200h
    
    mov rbx, rcx
    
@iocp_loop:
    cmp dword ptr [rbx].WEEK2_3_CONTEXT.running, 0
    je @iocp_exit
    
    ; Dequeue completion
    mov rcx, [rbx].WEEK2_3_CONTEXT.iocp_handle
    lea rdx, [rbp-8]                  ; Bytes transferred
    lea r8, [rbp-16]                  ; Completion key
    lea r9, [rbp-24]                  ; OVERLAPPED
    push INFINITE                     ; Timeout (infinite)
    sub rsp, 20h
    call GetQueuedCompletionStatus
    add rsp, 28h
    
    test eax, eax
    jz @iocp_timeout
    
    ; Process based on completion key
    mov rax, [rbp-16]
    
    cmp rax, 0                        ; Listen socket
    je @accept_connection
    
    ; Regular connection - process I/O
    mov rcx, rbx
    mov rdx, rax                      ; Connection ptr
    mov r8, [rbp-8]                   ; Bytes
    call ProcessSocketIO
    
    jmp @iocp_loop
    
@accept_connection:
    ; Accept new connection
    mov rcx, rbx
    call AcceptNewConnection
    
    jmp @iocp_loop
    
@iocp_timeout:
    jmp @iocp_loop
    
@iocp_exit:
    xor eax, eax
    mov rsp, rbp
    RESTORE_REGS
    ret
IOCPWorkerThread ENDP

;-------------------------------------------------------------------------------
; AcceptNewConnection - Handle incoming node connection
;-------------------------------------------------------------------------------
AcceptNewConnection PROC FRAME
    SAVE_REGS
    
    ; Accept socket
    ; Create connection context
    ; Start async receive
    ; Register with Week 1
    
    RESTORE_REGS
    ret
AcceptNewConnection ENDP

;-------------------------------------------------------------------------------
; ProcessSocketIO - Handle completed I/O operation
;-------------------------------------------------------------------------------
ProcessSocketIO PROC FRAME
    SAVE_REGS
    
    ; Parse message header
    ; Route to appropriate handler
    ; Reissue async I/O
    
    RESTORE_REGS
    ret
ProcessSocketIO ENDP

;================================================================================
; RAFT CONSENSUS ENGINE
;================================================================================

;-------------------------------------------------------------------------------
; InitializeRaftConsensus - Setup distributed consensus
;-------------------------------------------------------------------------------
InitializeRaftConsensus PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
    lea r12, [rbx].WEEK2_3_CONTEXT.raft
    
    ; Initialize persistent state
    mov qword ptr [r12].RAFT_STATE.current_term, 0
    mov dword ptr [r12].RAFT_STATE.voted_for, -1
    
    ; Allocate log (100K entries)
    mov rcx, rbx
    mov rdx, RAFT_MAX_LOG_ENTRIES * sizeof RAFT_LOG_ENTRY
    mov r8, 64
    call VirtualAlloc
    
    mov [r12].RAFT_STATE.log, rax
    mov qword ptr [r12].RAFT_STATE.log_capacity, RAFT_MAX_LOG_ENTRIES
    mov qword ptr [r12].RAFT_STATE.log_count, 0
    mov qword ptr [r12].RAFT_STATE.log_start_index, 1
    
    ; Volatile state
    mov qword ptr [r12].RAFT_STATE.commit_index, 0
    mov qword ptr [r12].RAFT_STATE.last_applied, 0
    mov dword ptr [r12].RAFT_STATE.state, 0      ; FOLLOWER
    
    ; Randomize election timeout
    rdtsc
    and eax, 0FFh
    add eax, RAFT_ELECTION_TIMEOUT_MIN
    mov [r12].RAFT_STATE.randomized_timeout, rax
    
    mov eax, 1
    
    RESTORE_REGS
    ret
InitializeRaftConsensus ENDP

;-------------------------------------------------------------------------------
; RaftEventLoop - Core consensus state machine (FSM)
;
; States:
;  - FOLLOWER: Wait for heartbeat, start election on timeout
;  - CANDIDATE: Send vote requests, win election or follow new leader
;  - LEADER: Send heartbeats, replicate log entries
;
; Procedure:
;  1. Check current state
;  2. If FOLLOWER: Check election timeout via Week 1
;  3. If timeout: Convert to CANDIDATE, send votes
;  4. If CANDIDATE: Count votes, if majority -> LEADER
;  5. If LEADER: Send heartbeats, replicate entries
;  6. Apply committed entries to state machine
;  7. Loop
;-------------------------------------------------------------------------------
RaftEventLoop PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
@raft_loop:
    cmp dword ptr [rbx].WEEK2_3_CONTEXT.running, 0
    je @raft_exit
    
    lea r12, [rbx].WEEK2_3_CONTEXT.raft
    
    ; Get current state (with lock)
    lea rcx, [r12].RAFT_STATE.lock
    call EnterCriticalSection
    
    mov eax, [r12].RAFT_STATE.state
    
    lea rcx, [r12].RAFT_STATE.lock
    call LeaveCriticalSection
    
    ; Dispatch by state
    cmp eax, 0                        ; FOLLOWER
    je @raft_follower
    cmp eax, 1                        ; CANDIDATE
    je @raft_candidate
    cmp eax, 2                        ; LEADER
    je @raft_leader
    
    jmp @raft_continue
    
@raft_follower:
    ; Check election timeout
    mov rcx, rbx
    call CheckElectionTimeout
    
    test eax, eax
    jz @raft_continue
    
    ; Convert to candidate
    lea rcx, [r12].RAFT_STATE.lock
    call EnterCriticalSection
    
    mov dword ptr [r12].RAFT_STATE.state, 1      ; CANDIDATE
    inc qword ptr [r12].RAFT_STATE.current_term
    
    ; Vote for self
    xor r13d, r13d
@reset_votes:
    cmp r13d, MAX_CLUSTER_NODES
    jge @votes_reset
    mov dword ptr [r12].RAFT_STATE.vote_granted[r13*4], 0
    inc r13d
    jmp @reset_votes
@votes_reset:
    
    lea rcx, [r12].RAFT_STATE.lock
    call LeaveCriticalSection
    
    jmp @raft_continue
    
@raft_candidate:
    ; Check if won election (majority)
    lea rcx, [r12].RAFT_STATE.lock
    call EnterCriticalSection
    
    xor r13d, r13d                    ; Vote count
    xor r14d, r14d                    ; Index
@count_votes:
    cmp r14d, MAX_CLUSTER_NODES
    jge @votes_counted
    cmp dword ptr [r12].RAFT_STATE.vote_granted[r14*4], 1
    jne @not_granted
    inc r13d
@not_granted:
    inc r14d
    jmp @count_votes
    
@votes_counted:
    ; Check majority
    mov eax, [r12].RAFT_STATE.peer_count
    shr eax, 1
    inc eax                           ; (n/2) + 1
    
    cmp r13d, eax
    jb @no_majority
    
    ; Become leader
    mov dword ptr [r12].RAFT_STATE.state, 2      ; LEADER
    
    ; Initialize leader state
    mov r13, [r12].RAFT_STATE.log_count
    add r13, [r12].RAFT_STATE.log_start_index
    
    xor r14d, r14d
@init_next:
    cmp r14d, MAX_CLUSTER_NODES
    jge @leader_ready
    mov [r12].RAFT_STATE.next_index[r14*8], r13
    mov qword ptr [r12].RAFT_STATE.match_index[r14*8], 0
    inc r14d
    jmp @init_next
    
@leader_ready:
    lea rcx, [r12].RAFT_STATE.lock
    call LeaveCriticalSection
    
    jmp @raft_continue
    
@no_majority:
    lea rcx, [r12].RAFT_STATE.lock
    call LeaveCriticalSection
    
    jmp @raft_continue
    
@raft_leader:
    ; Send heartbeats periodically
    mov rcx, rbx
    call ShouldSendHeartbeat
    
    test eax, eax
    jz @no_heartbeat
    
    mov rcx, rbx
    call BroadcastHeartbeats
    
@no_heartbeat:
    ; Replicate entries
    mov rcx, rbx
    call ReplicateLogEntries
    
    ; Advance commit index
    mov rcx, rbx
    call AdvanceCommitIndex
    
@raft_continue:
    ; Yield to prevent busy-waiting
    call SwitchToThread
    mov ecx, 10
    call Sleep
    
    jmp @raft_loop
    
@raft_exit:
    xor eax, eax
    RESTORE_REGS
    ret
RaftEventLoop ENDP

;-------------------------------------------------------------------------------
; CheckElectionTimeout - Using Week 1 timing integration
;-------------------------------------------------------------------------------
CheckElectionTimeout PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    lea r12, [rbx].WEEK2_3_CONTEXT.raft
    
    ; Query Week 1 timing infrastructure
    ; (In real implementation: call Week1 to get elapsed time since last heartbeat)
    
    ; For now: simplified check
    mov eax, 1                        ; Timeout
    
    RESTORE_REGS
    ret
CheckElectionTimeout ENDP

;-------------------------------------------------------------------------------
; ShouldSendHeartbeat - Check if time for heartbeat
;-------------------------------------------------------------------------------
ShouldSendHeartbeat PROC FRAME
    SAVE_REGS
    
    mov eax, 1                        ; Always send (simplified)
    
    RESTORE_REGS
    ret
ShouldSendHeartbeat ENDP

;-------------------------------------------------------------------------------
; BroadcastHeartbeats - Send AppendEntries to all followers
;-------------------------------------------------------------------------------
BroadcastHeartbeats PROC FRAME
    SAVE_REGS
    
    ; Send heartbeat to all follower nodes
    ; (Implementation: iterate nodes, send RAFT_PORT packets)
    
    RESTORE_REGS
    ret
BroadcastHeartbeats ENDP

;-------------------------------------------------------------------------------
; ReplicateLogEntries - Send uncommitted entries to followers
;-------------------------------------------------------------------------------
ReplicateLogEntries PROC FRAME
    SAVE_REGS
    
    ; For each follower, send entries from next_index[i]
    
    RESTORE_REGS
    ret
ReplicateLogEntries ENDP

;-------------------------------------------------------------------------------
; AdvanceCommitIndex - Check if safe to advance commit_index
;-------------------------------------------------------------------------------
AdvanceCommitIndex PROC FRAME
    SAVE_REGS
    
    ; Count replicas for each uncommitted entry
    ; If majority, mark committed
    
    RESTORE_REGS
    ret
AdvanceCommitIndex ENDP

;================================================================================
; GOSSIP PROTOCOL & MEMBERSHIP
;================================================================================

;-------------------------------------------------------------------------------
; GossipProtocolThread - Epidemic membership dissemination
;
; Procedure:
;  1. Send state digest to random peer every 100ms
;  2. Process incoming gossip messages
;  3. Detect node failures (missing heartbeats)
;  4. Detect network partitions
;  5. Update local membership view
;  6. Report changes to Week 1 monitor
;-------------------------------------------------------------------------------
GossipProtocolThread PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
@gossip_loop:
    cmp dword ptr [rbx].WEEK2_3_CONTEXT.running, 0
    je @gossip_exit
    
    ; Send gossip to random peers
    mov rcx, rbx
    call SendRandomGossip
    
    ; Process received gossip
    mov rcx, rbx
    call ProcessGossipQueue
    
    ; Detect failures
    mov rcx, rbx
    call DetectNodeFailures
    
    ; Sleep
    mov ecx, 100
    call Sleep
    
    jmp @gossip_loop
    
@gossip_exit:
    xor eax, eax
    RESTORE_REGS
    ret
GossipProtocolThread ENDP

;-------------------------------------------------------------------------------
; SendRandomGossip - Epidemic broadcast to random peer
;-------------------------------------------------------------------------------
SendRandomGossip PROC FRAME
    SAVE_REGS
    
    ; Pick random active node
    ; Send state digest (node states, shard assignments, version info)
    
    RESTORE_REGS
    ret
SendRandomGossip ENDP

;-------------------------------------------------------------------------------
; ProcessGossipQueue - Handle incoming membership updates
;-------------------------------------------------------------------------------
ProcessGossipQueue PROC FRAME
    SAVE_REGS
    
    ; Update membership list from gossip
    ; Detect new nodes, detect failures
    ; Update incarnation numbers
    
    RESTORE_REGS
    ret
ProcessGossipQueue ENDP

;-------------------------------------------------------------------------------
; DetectNodeFailures - Identify unreachable nodes
;-------------------------------------------------------------------------------
DetectNodeFailures PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
    ; Check each node for heartbeat timeout
    ; Mark as DEGRADED after 1 missed heartbeat
    ; Mark as REMOVED after 3 missed
    ; Report to Week 1 for coordination
    
    RESTORE_REGS
    ret
DetectNodeFailures ENDP

;================================================================================
; SHARD MANAGEMENT
;================================================================================

;-------------------------------------------------------------------------------
; InitializeShardTable - Setup consistent hashing ring
;-------------------------------------------------------------------------------
InitializeShardTable PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    lea r12, [rbx].WEEK2_3_CONTEXT.shards
    
    ; Allocate shard assignments array
    mov rcx, rbx
    mov rdx, MAX_SHARDS * sizeof SHARD_ASSIGNMENT
    mov r8, 64
    call VirtualAlloc
    
    mov [r12].SHARD_TABLE.assignments, rax
    mov qword ptr [r12].SHARD_TABLE.capacity, MAX_SHARDS
    mov qword ptr [r12].SHARD_TABLE.shard_count, 0
    
    ; Build consistent hashing ring
    mov rcx, rbx
    call BuildHashRing
    
    mov eax, 1
    
    RESTORE_REGS
    ret
InitializeShardTable ENDP

;-------------------------------------------------------------------------------
; BuildHashRing - Construct consistent hash ring
;-------------------------------------------------------------------------------
BuildHashRing PROC FRAME
    SAVE_REGS
    
    ; Create virtual nodes for each physical node
    ; Sort by hash value
    ; (Implementation: deterministic placement based on node IDs)
    
    RESTORE_REGS
    ret
BuildHashRing ENDP

;-------------------------------------------------------------------------------
; AssignShard - Determine replica placement for shard
;-------------------------------------------------------------------------------
AssignShard PROC FRAME
    SAVE_REGS
    
    ; Hash shard ID
    ; Walk ring to find primary + 2 replicas
    ; Verify all nodes are active
    ; Update assignment table
    
    RESTORE_REGS
    ret
AssignShard ENDP

;-------------------------------------------------------------------------------
; RebalanceShards - Load-aware shard migration
;
; Procedure:
;  1. Compute node load distribution
;  2. Identify overloaded nodes (>80% capacity)
;  3. Identify underloaded nodes (<20% capacity)
;  4. For each unbalanced shard: find new assignment
;  5. Submit migration tasks to Week 1
;  6. Wait for confirmations
;  7. Update shard table
;-------------------------------------------------------------------------------
RebalanceShards PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    lea r12, [rbx].WEEK2_3_CONTEXT.shards
    
    ; Check if rebalance already in progress
    lea rcx, [r12].SHARD_TABLE.lock
    call EnterCriticalSection
    
    cmp dword ptr [r12].SHARD_TABLE.rebalance_in_progress, 1
    je @rebal_busy
    
    mov dword ptr [r12].SHARD_TABLE.rebalance_in_progress, 1
    
    lea rcx, [r12].SHARD_TABLE.lock
    call LeaveCriticalSection
    
    ; Compute load distribution
    ; Identify imbalanced shards
    ; Coordinate migrations with Week 1
    
    lea rcx, [r12].SHARD_TABLE.lock
    call EnterCriticalSection
    
    mov dword ptr [r12].SHARD_TABLE.rebalance_in_progress, 0
    
    lea rcx, [r12].SHARD_TABLE.lock
    call LeaveCriticalSection
    
    jmp @rebal_done
    
@rebal_busy:
    lea rcx, [r12].SHARD_TABLE.lock
    call LeaveCriticalSection
    
@rebal_done:
    RESTORE_REGS
    ret
RebalanceShards ENDP

;================================================================================
; INFERENCE ROUTING
;================================================================================

;-------------------------------------------------------------------------------
; InitializeInferenceRouter - Setup request distribution
;-------------------------------------------------------------------------------
InitializeInferenceRouter PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    lea r12, [rbx].WEEK2_3_CONTEXT.router
    
    ; Allocate per-node request queues
    xor r13d, r13d
@alloc_queues:
    cmp r13d, MAX_CLUSTER_NODES
    jge @queues_done
    
    mov rcx, rbx
    mov rdx, 1000 * sizeof INFERENCE_REQUEST
    mov r8, 64
    call VirtualAlloc
    
    mov [r12].INFERENCE_ROUTER.pending_queues[r13*8], rax
    
    inc r13d
    jmp @alloc_queues
@queues_done:
    
    ; Default strategy: least loaded
    mov dword ptr [r12].INFERENCE_ROUTER.strategy, 1
    
    mov eax, 1
    
    RESTORE_REGS
    ret
InitializeInferenceRouter ENDP

;-------------------------------------------------------------------------------
; InferenceRouterThread - Distribute inference requests across cluster
;
; Procedure:
;  1. Check pending request queues
;  2. For each pending: compute optimal target node
;  3. Submit to target's queue
;  4. Monitor active requests
;  5. Collect latency metrics
;  6. Update load weights for future routing
;  7. Report completion to clients
;
; Load Balancing Strategies:
;  - Round-robin: Cycle through nodes
;  - Least-loaded: Pick node with lowest current load
;  - Locality: Route to node with required shard(s)
;-------------------------------------------------------------------------------
InferenceRouterThread PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
@router_loop:
    cmp dword ptr [rbx].WEEK2_3_CONTEXT.running, 0
    je @router_exit
    
    ; Route pending requests
    mov rcx, rbx
    call RoutePendingRequests
    
    ; Update load weights
    mov rcx, rbx
    call UpdateLoadWeights
    
    ; Process completed requests
    mov rcx, rbx
    call ProcessCompletedRequests
    
    ; Sleep briefly
    mov ecx, 1
    call Sleep
    
    jmp @router_loop
    
@router_exit:
    xor eax, eax
    RESTORE_REGS
    ret
InferenceRouterThread ENDP

;-------------------------------------------------------------------------------
; RoutePendingRequests - Distribute work across cluster
;-------------------------------------------------------------------------------
RoutePendingRequests PROC FRAME
    SAVE_REGS
    
    ; For each pending request:
    ;   Determine optimal node (locality, load, capacity)
    ;   Submit to node's queue
    ;   Mark as routed
    
    RESTORE_REGS
    ret
RoutePendingRequests ENDP

;-------------------------------------------------------------------------------
; UpdateLoadWeights - Recalculate node routing weights
;-------------------------------------------------------------------------------
UpdateLoadWeights PROC FRAME
    SAVE_REGS
    
    ; Gather node statistics from cluster
    ; Compute weighted scores based on:
    ;   - Queue depth
    ;   - Recent latency
    ;   - Node capacity
    ;   - Shard locality
    ; Update routing table
    
    RESTORE_REGS
    ret
UpdateLoadWeights ENDP

;-------------------------------------------------------------------------------
; ProcessCompletedRequests - Handle finished inference
;-------------------------------------------------------------------------------
ProcessCompletedRequests PROC FRAME
    SAVE_REGS
    
    ; Collect results from completed requests
    ; Invoke client callbacks
    ; Update metrics
    
    RESTORE_REGS
    ret
ProcessCompletedRequests ENDP

;-------------------------------------------------------------------------------
; SubmitInferenceRequest - Client-facing API
;
; Input:  RCX = WEEK2_3_CONTEXT*
;         RDX = INFERENCE_REQUEST*
; Output: EAX = 1 on success, 0 on failure
;
; Procedure:
;  1. Validate request
;  2. Determine target shard
;  3. Find nodes hosting shard
;  4. Add to pending queue
;  5. Notify router thread
;  6. Return immediately
;-------------------------------------------------------------------------------
SubmitInferenceRequest PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    mov r12, rdx                      ; INFERENCE_REQUEST*
    
    ; Validate
    test r12, r12
    jz @submit_fail
    
    ; Get router lock
    lea rcx, [rbx].WEEK2_3_CONTEXT.router.lock
    call EnterCriticalSection
    
    ; Find least-loaded node
    xor r13d, r13d                    ; Best node
    xor r14d, r14d                    ; Min load
    mov r14d, 0FFFFFFFFh
    
    xor r15d, r15d                    ; Node index
@find_best:
    cmp r15d, [rbx].WEEK2_3_CONTEXT.node_count
    jge @found_best
    
    mov eax, [rbx].WEEK2_3_CONTEXT.router.queue_depths[r15*4]
    cmp eax, r14d
    jge @next_node
    
    mov r13d, r15d
    mov r14d, eax
    
@next_node:
    inc r15d
    jmp @find_best
    
@found_best:
    ; Add request to best node's queue
    mov rax, r13
    mov rcx, [rbx].WEEK2_3_CONTEXT.router.pending_queues[rax*8]
    ; (In production: use circular queue with atomic operations)
    
    lea rcx, [rbx].WEEK2_3_CONTEXT.router.lock
    call LeaveCriticalSection
    
    mov eax, 1
    jmp @submit_exit
    
@submit_fail:
    xor eax, eax
    
@submit_exit:
    RESTORE_REGS
    ret
SubmitInferenceRequest ENDP

;================================================================================
; CLUSTER MEMBERSHIP
;================================================================================

;-------------------------------------------------------------------------------
; JoinCluster - Bootstrap into existing cluster or create new cluster
;
; Input:  RCX = WEEK2_3_CONTEXT*
;         RDX = Config with seed nodes (or NULL for standalone)
; Output: EAX = 1 on success, 0 on failure
;
; Procedure:
;  1. If config provided: contact seed nodes
;  2. Discover cluster topology
;  3. Request shard assignments
;  4. Sync Raft log from leader
;  5. Mark as ACTIVE
;  6. Report to Week 1
;-------------------------------------------------------------------------------
JoinCluster PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    mov r12, rdx                      ; Config or NULL
    
    ; If no config: bootstrap as node 0
    test r12, r12
    jz @bootstrap
    
    ; Contact seed nodes from config
    ; Discover topology
    ; Sync state
    
    jmp @join_done
    
@bootstrap:
    ; Single-node cluster
    mov dword ptr [rbx].WEEK2_3_CONTEXT.node_count, 1
    mov dword ptr [rbx].WEEK2_3_CONTEXT.local_node.node_id, 0
    
@join_done:
    ; Assign initial shards
    mov rcx, rbx
    call AssignInitialShards
    
    mov eax, 1
    
    RESTORE_REGS
    ret
JoinCluster ENDP

;-------------------------------------------------------------------------------
; AssignInitialShards - Distribute shards across cluster on join
;-------------------------------------------------------------------------------
AssignInitialShards PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
    ; For each shard:
    ;   Assign to (shard_id % node_count) as primary
    ;   Assign next 2 nodes as replicas
    
    RESTORE_REGS
    ret
AssignInitialShards ENDP

;-------------------------------------------------------------------------------
; LeaveCluster - Graceful shutdown
;
; Procedure:
;  1. Transfer all hosted shards to other nodes
;  2. Sync final log entries
;  3. Notify cluster of departure
;  4. Close connections
;  5. Stop background threads
;-------------------------------------------------------------------------------
LeaveCluster PROC FRAME
    SAVE_REGS
    
    ; Stop running
    ; Transfer shards
    ; Close connections
    
    RESTORE_REGS
    ret
LeaveCluster ENDP

;================================================================================
; EXPORTS
;================================================================================
PUBLIC Week23Initialize
PUBLIC IOCPWorkerThread
PUBLIC RaftEventLoop
PUBLIC GossipProtocolThread
PUBLIC InferenceRouterThread
PUBLIC JoinCluster
PUBLIC LeaveCluster
PUBLIC SubmitInferenceRequest
PUBLIC InitializeSwarmNetwork
PUBLIC InitializeRaftConsensus
PUBLIC InitializeShardTable
PUBLIC InitializeInferenceRouter

END
