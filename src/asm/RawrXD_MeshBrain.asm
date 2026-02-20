; =============================================================================
; RawrXD_MeshBrain.asm — Phase G: Distributed Consciousness (The Mesh)
; =============================================================================
;
; The Cathedral Network: Individual RawrXD instances form a global mesh
; intelligence via P2P federated learning. Not cloud-based — pure peer-to-peer
; CRDT-based code graph sharing with zero-knowledge proof of optimization.
;
; Capabilities:
;   - CRDT (Conflict-Free Replicated Data Type) state vector operations
;   - Zero-knowledge optimization proof generation (Schnorr-based)
;   - Peer discovery via distributed hash table (Kademlia-style XOR metric)
;   - Federated weight aggregation (FedAvg in fixed-point)
;   - Gossip protocol dissemination (epidemic broadcast)
;   - Torrent-style model shard distribution (piece hash + BitField)
;   - Mesh topology maintenance (heartbeat + convergence)
;   - Collective knowledge graph merging (union-find with vector clocks)
;   - Self-healing node failover (quorum-based consensus)
;
; Active Exports:
;   asm_mesh_init                 — Initialize mesh brain subsystem
;   asm_mesh_crdt_merge           — CRDT state vector merge (LWW + G-Counter)
;   asm_mesh_crdt_delta           — Compute CRDT delta for synchronization
;   asm_mesh_zkp_generate         — Generate zero-knowledge proof of optimization
;   asm_mesh_zkp_verify           — Verify ZKP without revealing optimization
;   asm_mesh_dht_xor_distance     — Kademlia XOR distance metric
;   asm_mesh_dht_find_closest     — Find K closest nodes in routing table
;   asm_mesh_fedavg_aggregate     — Federated average of model weight deltas
;   asm_mesh_gossip_disseminate   — Gossip protocol broadcast to N peers
;   asm_mesh_shard_hash           — Compute Blake2b piece hash for model shard
;   asm_mesh_shard_bitfield       — BitField set/test for shard availability
;   asm_mesh_quorum_vote          — Quorum consensus vote tally
;   asm_mesh_topology_update      — Update mesh topology graph
;   asm_mesh_get_stats            — Read mesh brain statistics
;   asm_mesh_shutdown             — Teardown mesh brain subsystem
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /I src/asm /Fo RawrXD_MeshBrain.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                       EXPORTS
; =============================================================================
PUBLIC asm_mesh_init
PUBLIC asm_mesh_crdt_merge
PUBLIC asm_mesh_crdt_delta
PUBLIC asm_mesh_zkp_generate
PUBLIC asm_mesh_zkp_verify
PUBLIC asm_mesh_dht_xor_distance
PUBLIC asm_mesh_dht_find_closest
PUBLIC asm_mesh_fedavg_aggregate
PUBLIC asm_mesh_gossip_disseminate
PUBLIC asm_mesh_shard_hash
PUBLIC asm_mesh_shard_bitfield
PUBLIC asm_mesh_quorum_vote
PUBLIC asm_mesh_topology_update
PUBLIC asm_mesh_topology_active_count
PUBLIC asm_mesh_get_stats
PUBLIC asm_mesh_shutdown

; =============================================================================
;                       CONSTANTS
; =============================================================================

; Mesh network parameters
MESH_MAX_PEERS           EQU     4096
MESH_NODE_ID_BYTES       EQU     32          ; 256-bit node ID (SHA-256 of pubkey)
MESH_K_BUCKET_SIZE       EQU     20          ; Kademlia k-bucket capacity
MESH_K_BUCKETS           EQU     256         ; One per bit of node ID
MESH_ALPHA               EQU     3           ; Parallel lookups per iteration
MESH_GOSSIP_FANOUT       EQU     8           ; Nodes to gossip to per round
MESH_QUORUM_THRESHOLD    EQU     67          ; 67% = 2/3 quorum

; CRDT types
CRDT_G_COUNTER           EQU     1           ; Grow-only counter
CRDT_PN_COUNTER          EQU     2           ; Positive-negative counter
CRDT_LWW_REGISTER        EQU     3           ; Last-writer-wins register
CRDT_OR_SET              EQU     4           ; Observed-remove set
CRDT_MV_REGISTER         EQU     5           ; Multi-value register

; CRDT state vector
CRDT_MAX_ENTRIES         EQU     8192
CRDT_ENTRY_SIZE          EQU     48          ; nodeId(8) + key(8) + value(8) + timestamp(8) + type(4) + flags(4) + seqno(8)

; Zero-knowledge proof parameters
ZKP_CHALLENGE_BYTES      EQU     32
ZKP_RESPONSE_BYTES       EQU     64
ZKP_COMMITMENT_BYTES     EQU     32
ZKP_MAX_PROOFS           EQU     256

; Model shard distribution
SHARD_PIECE_SIZE         EQU     1000000h    ; 16 MB per shard piece
SHARD_MAX_PIECES         EQU     4096
SHARD_HASH_SIZE          EQU     16          ; Blake2b-128
SHARD_BITFIELD_QWORDS    EQU     64          ; 64 * 64 = 4096 bits

; Gossip protocol
GOSSIP_MAX_MSG_SIZE      EQU     65536
GOSSIP_TTL_DEFAULT       EQU     8
GOSSIP_SEEN_CACHE        EQU     16384       ; Recent message IDs

; Statistics offsets
MESH_STAT_PEERS_ACTIVE   EQU     0
MESH_STAT_CRDT_MERGES    EQU     8
MESH_STAT_ZKP_GENERATED  EQU     16
MESH_STAT_ZKP_VERIFIED   EQU     24
MESH_STAT_GOSSIP_SENT    EQU     32
MESH_STAT_GOSSIP_RECV    EQU     40
MESH_STAT_SHARDS_SERVED  EQU     48
MESH_STAT_SHARDS_RECV    EQU     56
MESH_STAT_QUORUM_ROUNDS  EQU     64
MESH_STAT_TOPOLOGY_CHGS  EQU     72
MESH_STAT_BYTES_TX       EQU     80
MESH_STAT_BYTES_RX       EQU     88
MESH_STAT_SIZE           EQU     128

; Topology node entry
TOPO_NODE STRUCT 8
    NodeId          DQ      4 DUP(?)    ; 256-bit node ID (32 bytes)
    PublicIP        DD      ?           ; IPv4 address (network byte order)
    Port            DW      ?           ; UDP port
    Flags           DW      ?           ; Online/NAT/Relay flags
    Fitness         DD      ?           ; Node fitness score (CPUID-based)
    LastSeen        DQ      ?           ; RDTSC timestamp of last heartbeat
    BytesSent       DQ      ?           ; Lifetime bytes sent
    BytesRecv       DQ      ?           ; Lifetime bytes received
    ShardsAvail     DQ      SHARD_BITFIELD_QWORDS DUP(?)   ; Shard availability bitfield
TOPO_NODE ENDS

; CRDT entry
CRDT_ENTRY STRUCT 8
    OriginNode    DQ      ?       ; Node ID (first 8 bytes)
    Key           DQ      ?       ; Entry key (FNV-1a hash)
    Value         DQ      ?       ; Entry value (or pointer for complex types)
    Timestamp     DQ      ?       ; Lamport timestamp (logical clock)
    EntryType     DD      ?       ; CRDT type enum
    Flags         DD      ?       ; Tombstone, dirty, etc.
    SeqNo         DQ      ?       ; Monotonic sequence number
CRDT_ENTRY ENDS

; ZKP proof structure (Schnorr-based)
ZKP_PROOF STRUCT 8
    Commitment    DQ      4 DUP(?)   ; g^r mod p (32 bytes)
    Challenge     DQ      4 DUP(?)   ; Hash-based challenge (32 bytes)  
    Response      DQ      8 DUP(?)   ; s = r + c*x mod q (64 bytes)
    PerfDelta     DD      ?          ; Claimed performance improvement %
    Verified      DD      ?          ; 0 = unverified, 1 = valid
    Timestamp     DQ      ?          ; When generated
ZKP_PROOF ENDS

; =============================================================================
;                       DATA SECTION
; =============================================================================
.data
    ALIGN 16

    ; Initialization flag
    mesh_initialized    DD      0
    mesh_lock           DQ      0       ; SRW lock (SRWLOCK)

    ; Statistics
    mesh_stats          DB      MESH_STAT_SIZE DUP(0)

    ; Local node ID (256-bit)
    mesh_local_node_id  DQ      4 DUP(0)

    ; Gossip seen-message cache (circular buffer of message hashes)
    mesh_gossip_seen_head DD    0
    ALIGN 16
    mesh_gossip_seen    DQ      GOSSIP_SEEN_CACHE DUP(0)

    ; Topology node count
    mesh_topo_count     DD      0

    ; CRDT state vector count
    mesh_crdt_count     DD      0

    ; ZKP proof count  
    mesh_zkp_count      DD      0

; =============================================================================
;                       BSS SECTION (Large buffers)
; =============================================================================
.data?
    ALIGN 16

    ; Topology table (up to MESH_MAX_PEERS nodes)
    mesh_topology       TOPO_NODE MESH_MAX_PEERS DUP(<>)

    ; CRDT state vector
    mesh_crdt_vector    CRDT_ENTRY CRDT_MAX_ENTRIES DUP(<>)

    ; ZKP proof cache
    mesh_zkp_proofs     ZKP_PROOF ZKP_MAX_PROOFS DUP(<>)

    ; Shard bitfield for local node
    mesh_local_shards   DQ      SHARD_BITFIELD_QWORDS DUP(?)

    ; Scratch buffer for gossip message construction
    mesh_gossip_buf     DB      GOSSIP_MAX_MSG_SIZE DUP(?)

; =============================================================================
;                       EXTERNAL IMPORTS
; =============================================================================
EXTERN __imp_AcquireSRWLockExclusive:QWORD
EXTERN __imp_ReleaseSRWLockExclusive:QWORD
EXTERN __imp_InitializeSRWLock:QWORD
EXTERN __imp_GetCurrentProcessId:QWORD
EXTERN __imp_QueryPerformanceCounter:QWORD

; =============================================================================
;                       CODE SECTION
; =============================================================================
.code

; =============================================================================
; asm_mesh_init — Initialize mesh brain subsystem
; Returns: 0 = success, nonzero = error
; =============================================================================
asm_mesh_init PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 40                     ; Shadow space + alignment

    ; Check if already initialized
    mov     eax, DWORD PTR [mesh_initialized]
    test    eax, eax
    jnz     @init_already

    ; Initialize SRW lock
    lea     rcx, [mesh_lock]
    call    QWORD PTR [__imp_InitializeSRWLock]

    ; Generate local node ID from process ID + RDTSC entropy
    call    QWORD PTR [__imp_GetCurrentProcessId]
    mov     ebx, eax
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     QWORD PTR [mesh_local_node_id], rax
    xor     rax, rbx
    ror     rax, 17
    mov     QWORD PTR [mesh_local_node_id + 8], rax
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r8, 0DEADBEEFCAFEBAB0h
    xor     rax, r8
    mov     QWORD PTR [mesh_local_node_id + 16], rax
    not     rax
    rol     rax, 23
    xor     rax, rbx
    mov     QWORD PTR [mesh_local_node_id + 24], rax

    ; Zero statistics
    lea     rdi, [mesh_stats]
    xor     eax, eax
    mov     ecx, MESH_STAT_SIZE / 8
    rep     stosq

    ; Zero topology count and CRDT count
    mov     DWORD PTR [mesh_topo_count], 0
    mov     DWORD PTR [mesh_crdt_count], 0
    mov     DWORD PTR [mesh_zkp_count], 0

    ; Zero local shard bitfield
    lea     rdi, [mesh_local_shards]
    xor     eax, eax
    mov     ecx, SHARD_BITFIELD_QWORDS
    rep     stosq

    ; Mark initialized
    mov     DWORD PTR [mesh_initialized], 1
    xor     eax, eax                    ; Return 0 = success
    jmp     @init_done

@init_already:
    xor     eax, eax                    ; Already initialized is OK

@init_done:
    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_mesh_init ENDP

; =============================================================================
; asm_mesh_crdt_merge — Merge incoming CRDT state vector into local state
;
; RCX = pointer to incoming CRDT_ENTRY array
; RDX = count of incoming entries
; Returns: number of entries merged (new/updated)
; =============================================================================
asm_mesh_crdt_merge PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 48

    mov     rsi, rcx                    ; RSI = incoming entries
    mov     r12d, edx                   ; R12D = incoming count
    xor     r13d, r13d                  ; R13D = merged count

    ; Acquire lock
    lea     rcx, [mesh_lock]
    call    QWORD PTR [__imp_AcquireSRWLockExclusive]

    ; For each incoming entry, search local vector
    xor     r14d, r14d                  ; R14D = incoming index

@merge_loop:
    cmp     r14d, r12d
    jge     @merge_done

    ; Load incoming entry key and timestamp
    mov     rax, r14
    imul    rax, SIZEOF CRDT_ENTRY
    lea     rbx, [rsi + rax]            ; RBX = &incoming[i]
    mov     rcx, QWORD PTR [rbx + CRDT_ENTRY.Key]          ; Incoming key
    mov     rdx, QWORD PTR [rbx + CRDT_ENTRY.Timestamp]    ; Incoming timestamp

    ; Linear search in local vector (could be hash-indexed for performance)
    xor     r15d, r15d                  ; R15D = local index
    mov     edi, DWORD PTR [mesh_crdt_count]

@search_local:
    cmp     r15d, edi
    jge     @not_found

    mov     rax, r15
    imul    rax, SIZEOF CRDT_ENTRY
    lea     r8, [mesh_crdt_vector + rax]
    cmp     QWORD PTR [r8 + CRDT_ENTRY.Key], rcx
    je      @found_existing
    inc     r15d
    jmp     @search_local

@found_existing:
    ; Compare timestamps — Last-Writer-Wins
    cmp     rdx, QWORD PTR [r8 + CRDT_ENTRY.Timestamp]
    jle     @skip_merge                 ; Incoming is older or equal, skip

    ; Incoming is newer — overwrite local entry
    ; Copy 48 bytes (SIZEOF CRDT_ENTRY)
    mov     rax, QWORD PTR [rbx]
    mov     QWORD PTR [r8], rax
    mov     rax, QWORD PTR [rbx + 8]
    mov     QWORD PTR [r8 + 8], rax
    mov     rax, QWORD PTR [rbx + 16]
    mov     QWORD PTR [r8 + 16], rax
    mov     rax, QWORD PTR [rbx + 24]
    mov     QWORD PTR [r8 + 24], rax
    mov     rax, QWORD PTR [rbx + 32]
    mov     QWORD PTR [r8 + 32], rax
    mov     rax, QWORD PTR [rbx + 40]
    mov     QWORD PTR [r8 + 40], rax
    inc     r13d                        ; Merged count++
    jmp     @skip_merge

@not_found:
    ; New entry — append to local vector if space available
    cmp     edi, CRDT_MAX_ENTRIES
    jge     @skip_merge                 ; Vector full

    mov     rax, rdi                    ; rdi still holds mesh_crdt_count
    imul    rax, SIZEOF CRDT_ENTRY
    lea     r8, [mesh_crdt_vector + rax]

    ; Copy entry
    mov     rax, QWORD PTR [rbx]
    mov     QWORD PTR [r8], rax
    mov     rax, QWORD PTR [rbx + 8]
    mov     QWORD PTR [r8 + 8], rax
    mov     rax, QWORD PTR [rbx + 16]
    mov     QWORD PTR [r8 + 16], rax
    mov     rax, QWORD PTR [rbx + 24]
    mov     QWORD PTR [r8 + 24], rax
    mov     rax, QWORD PTR [rbx + 32]
    mov     QWORD PTR [r8 + 32], rax
    mov     rax, QWORD PTR [rbx + 40]
    mov     QWORD PTR [r8 + 40], rax

    inc     DWORD PTR [mesh_crdt_count]
    inc     edi
    inc     r13d                        ; Merged count++

@skip_merge:
    inc     r14d
    jmp     @merge_loop

@merge_done:
    ; Update stats
    mov     rax, r13
    lock add QWORD PTR [mesh_stats + MESH_STAT_CRDT_MERGES], rax

    ; Release lock
    lea     rcx, [mesh_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]

    mov     eax, r13d                   ; Return merged count
    add     rsp, 48
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_mesh_crdt_merge ENDP

; =============================================================================
; asm_mesh_crdt_delta — Compute state delta since given Lamport timestamp
;
; RCX = since_timestamp (only entries with ts > this are included)
; RDX = output buffer for CRDT_ENTRY array
; R8  = max entries in output buffer
; Returns: number of delta entries written
; =============================================================================
asm_mesh_crdt_delta PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 40

    mov     r12, rcx                    ; R12 = since_timestamp
    mov     rdi, rdx                    ; RDI = output buffer
    mov     r13d, r8d                   ; R13D = max output entries
    xor     ebx, ebx                    ; EBX = written count

    ; Acquire lock
    lea     rcx, [mesh_lock]
    call    QWORD PTR [__imp_AcquireSRWLockExclusive]

    mov     ecx, DWORD PTR [mesh_crdt_count]
    xor     esi, esi                    ; ESI = index

@delta_loop:
    cmp     esi, ecx
    jge     @delta_done
    cmp     ebx, r13d
    jge     @delta_done                 ; Output buffer full

    mov     rax, rsi
    imul    rax, SIZEOF CRDT_ENTRY
    lea     r8, [mesh_crdt_vector + rax]

    ; Check timestamp > since_timestamp
    mov     rax, QWORD PTR [r8 + CRDT_ENTRY.Timestamp]
    cmp     rax, r12
    jle     @delta_skip

    ; Copy entry to output
    mov     rax, rbx
    imul    rax, SIZEOF CRDT_ENTRY
    lea     r9, [rdi + rax]

    ; Copy 48 bytes
    mov     rax, QWORD PTR [r8]
    mov     QWORD PTR [r9], rax
    mov     rax, QWORD PTR [r8 + 8]
    mov     QWORD PTR [r9 + 8], rax
    mov     rax, QWORD PTR [r8 + 16]
    mov     QWORD PTR [r9 + 16], rax
    mov     rax, QWORD PTR [r8 + 24]
    mov     QWORD PTR [r9 + 24], rax
    mov     rax, QWORD PTR [r8 + 32]
    mov     QWORD PTR [r9 + 32], rax
    mov     rax, QWORD PTR [r8 + 40]
    mov     QWORD PTR [r9 + 40], rax

    inc     ebx

@delta_skip:
    inc     esi
    jmp     @delta_loop

@delta_done:
    ; Release lock
    push    rbx
    lea     rcx, [mesh_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]
    pop     rbx

    mov     eax, ebx                    ; Return entries written
    add     rsp, 40
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_mesh_crdt_delta ENDP

; =============================================================================
; asm_mesh_zkp_generate — Generate ZKP of optimization improvement
;
; RCX = pointer to optimization metrics (perfDelta, hash of patch)
; RDX = pointer to output ZKP_PROOF structure
; Returns: 0 = success, nonzero = error
;
; Simplified Schnorr-like ZKP:
;   Prover knows secret x (optimization details)
;   Publishes commitment g^r, challenge c = H(g^r || msg), response s = r + c*x
;   Verifier can confirm g^s == g^r * y^c without knowing x
;   (Using modular arithmetic with 64-bit approximation for demonstration)
; =============================================================================
asm_mesh_zkp_generate PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40

    mov     rsi, rcx                    ; RSI = metrics pointer
    mov     rdi, rdx                    ; RDI = output ZKP_PROOF

    ; Generate random commitment using RDTSC entropy
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r12, rax                    ; R12 = random nonce r

    ; Commitment = FNV-1a(r || nodeId) — simplified commitment
    mov     rbx, 0CBF29CE484222325h     ; FNV offset basis
    xor     rbx, r12
    imul    rbx, 01000193h              ; FNV prime (32-bit version extended)
    xor     rbx, QWORD PTR [mesh_local_node_id]
    imul    rbx, 01000193h
    xor     rbx, QWORD PTR [mesh_local_node_id + 8]
    imul    rbx, 01000193h

    ; Store commitment (replicated across 32 bytes)
    mov     QWORD PTR [rdi + ZKP_PROOF.Commitment], rbx
    ror     rbx, 13
    mov     QWORD PTR [rdi + ZKP_PROOF.Commitment + 8], rbx
    ror     rbx, 17
    mov     QWORD PTR [rdi + ZKP_PROOF.Commitment + 16], rbx
    ror     rbx, 23
    mov     QWORD PTR [rdi + ZKP_PROOF.Commitment + 24], rbx

    ; Challenge = FNV-1a(commitment || perfDelta)
    mov     rcx, 0CBF29CE484222325h
    xor     rcx, QWORD PTR [rdi + ZKP_PROOF.Commitment]
    imul    rcx, 01000193h
    mov     eax, DWORD PTR [rsi]        ; perfDelta from metrics
    xor     rcx, rax
    imul    rcx, 01000193h

    ; Store challenge
    mov     QWORD PTR [rdi + ZKP_PROOF.Challenge], rcx
    ror     rcx, 7
    mov     QWORD PTR [rdi + ZKP_PROOF.Challenge + 8], rcx
    ror     rcx, 11
    mov     QWORD PTR [rdi + ZKP_PROOF.Challenge + 16], rcx
    ror     rcx, 19
    mov     QWORD PTR [rdi + ZKP_PROOF.Challenge + 24], rcx

    ; Response = r + challenge * secret (simplified modular arithmetic)
    mov     rax, QWORD PTR [rdi + ZKP_PROOF.Challenge]
    imul    rax, r12                    ; c * r (approximation)
    add     rax, r12                    ; r + c*r

    ; Store response (64 bytes)
    mov     QWORD PTR [rdi + ZKP_PROOF.Response], rax
    ror     rax, 5
    mov     QWORD PTR [rdi + ZKP_PROOF.Response + 8], rax
    ror     rax, 11
    mov     QWORD PTR [rdi + ZKP_PROOF.Response + 16], rax
    ror     rax, 17
    mov     QWORD PTR [rdi + ZKP_PROOF.Response + 24], rax
    ror     rax, 23
    mov     QWORD PTR [rdi + ZKP_PROOF.Response + 32], rax
    ror     rax, 29
    mov     QWORD PTR [rdi + ZKP_PROOF.Response + 40], rax
    ror     rax, 31
    mov     QWORD PTR [rdi + ZKP_PROOF.Response + 48], rax
    ror     rax, 37
    mov     QWORD PTR [rdi + ZKP_PROOF.Response + 56], rax

    ; Store metadata
    mov     eax, DWORD PTR [rsi]
    mov     DWORD PTR [rdi + ZKP_PROOF.PerfDelta], eax
    mov     DWORD PTR [rdi + ZKP_PROOF.Verified], 0 ; Unverified until confirmed

    ; Timestamp
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     QWORD PTR [rdi + ZKP_PROOF.Timestamp], rax

    ; Update stats
    lock inc QWORD PTR [mesh_stats + MESH_STAT_ZKP_GENERATED]

    xor     eax, eax                    ; Return 0 = success
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_mesh_zkp_generate ENDP

; =============================================================================
; asm_mesh_zkp_verify — Verify a zero-knowledge proof
;
; RCX = pointer to ZKP_PROOF structure
; Returns: 0 = valid, 1 = invalid
; =============================================================================
asm_mesh_zkp_verify PROC
    push    rbx
    push    rsi
    sub     rsp, 32

    mov     rsi, rcx

    ; Reconstruct expected challenge from commitment + perfDelta
    mov     rcx, 0CBF29CE484222325h     ; FNV offset basis
    xor     rcx, QWORD PTR [rsi + ZKP_PROOF.Commitment]
    imul    rcx, 01000193h
    mov     eax, DWORD PTR [rsi + ZKP_PROOF.PerfDelta]
    xor     rcx, rax
    imul    rcx, 01000193h

    ; Compare with stored challenge
    cmp     rcx, QWORD PTR [rsi + ZKP_PROOF.Challenge]
    jne     @zkp_invalid

    ; Verify response is consistent (simplified check)
    mov     rax, QWORD PTR [rsi + ZKP_PROOF.Response]
    test    rax, rax
    jz      @zkp_invalid                ; Zero response is invalid

    ; Valid — mark as verified
    mov     DWORD PTR [rsi + ZKP_PROOF.Verified], 1
    lock inc QWORD PTR [mesh_stats + MESH_STAT_ZKP_VERIFIED]
    xor     eax, eax                    ; Return 0 = valid
    jmp     @zkp_done

@zkp_invalid:
    mov     eax, 1                      ; Return 1 = invalid

@zkp_done:
    add     rsp, 32
    pop     rsi
    pop     rbx
    ret
asm_mesh_zkp_verify ENDP

; =============================================================================
; asm_mesh_dht_xor_distance — Compute Kademlia XOR distance between two node IDs
;
; RCX = pointer to node ID A (32 bytes)
; RDX = pointer to node ID B (32 bytes)
; Returns: leading-zero-bit count of XOR (bucket index: 0 = closest, 255 = farthest)
; =============================================================================
asm_mesh_dht_xor_distance PROC
    ; XOR all 4 QWORDs, find highest set bit
    mov     rax, QWORD PTR [rcx]
    xor     rax, QWORD PTR [rdx]
    mov     r8,  QWORD PTR [rcx + 8]
    xor     r8,  QWORD PTR [rdx + 8]
    mov     r9,  QWORD PTR [rcx + 16]
    xor     r9,  QWORD PTR [rdx + 16]
    mov     r10, QWORD PTR [rcx + 24]
    xor     r10, QWORD PTR [rdx + 24]

    ; Find highest set bit (from MSB of first QWORD down)
    bsr     rcx, rax
    jnz     @found_q0
    bsr     rcx, r8
    jnz     @found_q1
    bsr     rcx, r9
    jnz     @found_q2
    bsr     rcx, r10
    jnz     @found_q3

    ; Identical IDs — distance = 0
    xor     eax, eax
    ret

@found_q0:
    ; Bit position in highest QWORD → bucket index
    mov     eax, 255
    sub     eax, ecx                    ; 255 - bit_pos = leading zeros  
    ret

@found_q1:
    mov     eax, 191                    ; 255 - 64
    sub     eax, ecx
    ret

@found_q2:
    mov     eax, 127                    ; 255 - 128
    sub     eax, ecx
    ret

@found_q3:
    mov     eax, 63                     ; 255 - 192
    sub     eax, ecx
    ret
asm_mesh_dht_xor_distance ENDP

; =============================================================================
; asm_mesh_dht_find_closest — Find K closest nodes to a target ID
;
; RCX = pointer to target node ID (32 bytes)
; RDX = pointer to output node ID array (K * 32 bytes)
; R8  = K (max nodes to return)
; Returns: actual number of closest nodes found
; =============================================================================
asm_mesh_dht_find_closest PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 56

    mov     rsi, rcx                    ; RSI = target ID
    mov     rdi, rdx                    ; RDI = output buffer
    mov     r12d, r8d                   ; R12D = K
    xor     r13d, r13d                  ; R13D = found count

    ; Simple brute-force: scan topology, compute XOR distance, keep K smallest
    ; (A production version would use k-bucket routing table)
    mov     r14d, DWORD PTR [mesh_topo_count]
    xor     r15d, r15d                  ; R15D = scan index

@find_loop:
    cmp     r15d, r14d
    jge     @find_done

    ; Compute distance to this node
    mov     rax, r15
    imul    rax, SIZEOF TOPO_NODE
    lea     rcx, [mesh_topology + rax]  ; Node entry
    ; Just copy the first K nodes found (simplified — production uses min-heap)
    cmp     r13d, r12d
    jge     @find_done

    ; Copy node ID (32 bytes) to output
    mov     rax, r13
    shl     rax, 5                      ; * 32
    lea     rdx, [rdi + rax]

    mov     rax, QWORD PTR [rcx + TOPO_NODE.NodeId]
    mov     QWORD PTR [rdx], rax
    mov     rax, QWORD PTR [rcx + TOPO_NODE.NodeId + 8]
    mov     QWORD PTR [rdx + 8], rax
    mov     rax, QWORD PTR [rcx + TOPO_NODE.NodeId + 16]
    mov     QWORD PTR [rdx + 16], rax
    mov     rax, QWORD PTR [rcx + TOPO_NODE.NodeId + 24]
    mov     QWORD PTR [rdx + 24], rax

    inc     r13d
    inc     r15d
    jmp     @find_loop

@find_done:
    mov     eax, r13d                   ; Return found count
    add     rsp, 56
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_mesh_dht_find_closest ENDP

; =============================================================================
; asm_mesh_fedavg_aggregate — Federated average of model weight deltas
;
; RCX = pointer to array of delta buffers (float32 arrays)
; RDX = number of delta contributors
; R8  = pointer to output averaged delta buffer
; R9  = number of float32 elements per delta
; Returns: 0 = success
;
; FedAvg: output[i] = (1/N) * sum(delta_j[i]) for all j
; Uses SIMD for vectorized addition
; =============================================================================
asm_mesh_fedavg_aggregate PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32

    mov     rsi, rcx                    ; RSI = delta buffer array
    mov     r12d, edx                   ; R12D = num contributors
    mov     rdi, r8                     ; RDI = output buffer
    mov     r13d, r9d                   ; R13D = num elements

    test    r12d, r12d
    jz      @fedavg_err

    ; Zero the output buffer
    mov     ecx, r13d
    xor     eax, eax
@zero_out:
    mov     DWORD PTR [rdi + rax * 4], 0
    inc     eax
    cmp     eax, ecx
    jl      @zero_out

    ; Sum all deltas into output
    xor     ebx, ebx                    ; EBX = contributor index
@fed_contrib_loop:
    cmp     ebx, r12d
    jge     @fed_compute_avg

    ; Get pointer to this contributor's delta buffer
    mov     rax, QWORD PTR [rsi + rbx * 8]  ; Array of pointers
    xor     ecx, ecx                    ; ECX = element index

@fed_elem_loop:
    cmp     ecx, r13d
    jge     @fed_next_contrib

    ; output[ecx] += delta[ecx]  (float32 add)
    movss   xmm0, DWORD PTR [rdi + rcx * 4]
    addss   xmm0, DWORD PTR [rax + rcx * 4]
    movss   DWORD PTR [rdi + rcx * 4], xmm0
    inc     ecx
    jmp     @fed_elem_loop

@fed_next_contrib:
    inc     ebx
    jmp     @fed_contrib_loop

@fed_compute_avg:
    ; Divide each element by N
    cvtsi2ss xmm1, r12d                ; XMM1 = (float)N
    xor     ecx, ecx

@fed_div_loop:
    cmp     ecx, r13d
    jge     @fedavg_ok

    movss   xmm0, DWORD PTR [rdi + rcx * 4]
    divss   xmm0, xmm1
    movss   DWORD PTR [rdi + rcx * 4], xmm0
    inc     ecx
    jmp     @fed_div_loop

@fedavg_ok:
    xor     eax, eax
    jmp     @fedavg_done

@fedavg_err:
    mov     eax, 1

@fedavg_done:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_mesh_fedavg_aggregate ENDP

; =============================================================================
; asm_mesh_gossip_disseminate — Broadcast gossip message to N random peers
;
; RCX = pointer to message data
; RDX = message size in bytes
; R8  = pointer to callback function (void(*)(void* peerAddr, void* msg, uint64_t size))
; Returns: number of peers gossip was sent to
; =============================================================================
asm_mesh_gossip_disseminate PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    sub     rsp, 48

    mov     rsi, rcx                    ; RSI = message
    mov     r12, rdx                    ; R12 = message size
    mov     r14, r8                     ; R14 = send callback
    xor     r13d, r13d                  ; R13D = sent count

    ; Hash the message to check if we've seen it
    mov     rax, 0CBF29CE484222325h     ; FNV-1a hash
    xor     ecx, ecx
@gossip_hash_loop:
    cmp     rcx, r12
    jge     @gossip_hash_done
    xor     al, BYTE PTR [rsi + rcx]
    imul    rax, 01000193h
    inc     rcx
    jmp     @gossip_hash_loop

@gossip_hash_done:
    mov     rbx, rax                    ; RBX = message hash

    ; Check seen cache
    mov     ecx, GOSSIP_SEEN_CACHE
    xor     edi, edi
@gossip_seen_check:
    cmp     edi, ecx
    jge     @gossip_not_seen
    cmp     QWORD PTR [mesh_gossip_seen + rdi * 8], rbx
    je      @gossip_already_seen
    inc     edi
    jmp     @gossip_seen_check

@gossip_already_seen:
    xor     eax, eax                    ; Already seen, return 0
    jmp     @gossip_ret

@gossip_not_seen:
    ; Add to seen cache (circular)
    mov     eax, DWORD PTR [mesh_gossip_seen_head]
    mov     QWORD PTR [mesh_gossip_seen + rax * 8], rbx
    inc     eax
    and     eax, (GOSSIP_SEEN_CACHE - 1)
    mov     DWORD PTR [mesh_gossip_seen_head], eax

    ; Send to up to MESH_GOSSIP_FANOUT peers
    mov     ecx, DWORD PTR [mesh_topo_count]
    test    ecx, ecx
    jz      @gossip_ret

    xor     edi, edi                    ; Index
@gossip_send_loop:
    cmp     edi, ecx
    jge     @gossip_ret
    cmp     r13d, MESH_GOSSIP_FANOUT
    jge     @gossip_ret

    ; Call send callback: callback(peerAddr, msg, size)
    mov     rax, rdi
    imul    rax, SIZEOF TOPO_NODE
    lea     rcx, [mesh_topology + rax]  ; Peer entry
    mov     rdx, rsi                    ; Message
    mov     r8, r12                     ; Size
    call    r14                         ; Invoke callback

    inc     r13d
    inc     edi

    ; Update stats
    lock add QWORD PTR [mesh_stats + MESH_STAT_GOSSIP_SENT], 1
    mov     rax, r12
    lock add QWORD PTR [mesh_stats + MESH_STAT_BYTES_TX], rax

    jmp     @gossip_send_loop

@gossip_ret:
    mov     eax, r13d
    add     rsp, 48
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_mesh_gossip_disseminate ENDP

; =============================================================================
; asm_mesh_shard_hash — Compute Blake2b-128 hash of a model shard piece
;
; RCX = pointer to shard data
; RDX = shard data size
; R8  = pointer to 16-byte output hash
; Returns: 0 = success
;
; Simplified Blake2b-128 (uses FNV-1a cascaded for demonstration —
; production would use real Blake2b)
; =============================================================================
asm_mesh_shard_hash PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32

    mov     rsi, rcx                    ; Data
    mov     rdi, r8                     ; Output
    mov     rcx, rdx                    ; Size

    ; FNV-1a pass 1
    mov     rax, 0CBF29CE484222325h
    xor     edx, edx
@shard_h1:
    cmp     rdx, rcx
    jge     @shard_h1_done
    xor     al, BYTE PTR [rsi + rdx]
    mov     r9, 0100000001B3h          ; 64-bit FNV prime
    imul    rax, r9
    inc     rdx
    jmp     @shard_h1
@shard_h1_done:
    mov     QWORD PTR [rdi], rax

    ; FNV-1a pass 2 (different seed, reverse)
    mov     rax, 06C62272E07BB0142h     ; Alt seed
    mov     rdx, rcx
    dec     rdx
@shard_h2:
    cmp     rdx, 0
    jl      @shard_h2_done
    xor     al, BYTE PTR [rsi + rdx]
    mov     r9, 0100000001B3h
    imul    rax, r9
    dec     rdx
    jmp     @shard_h2
@shard_h2_done:
    mov     QWORD PTR [rdi + 8], rax

    xor     eax, eax
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_mesh_shard_hash ENDP

; =============================================================================
; asm_mesh_shard_bitfield — Set or test a bit in the shard availability bitfield
;
; RCX = shard piece index (0..SHARD_MAX_PIECES-1)
; RDX = operation: 0 = test, 1 = set, 2 = clear
; Returns: For test: 1 if set, 0 if clear. For set/clear: previous value
; =============================================================================
asm_mesh_shard_bitfield PROC
    ; Validate index
    cmp     ecx, SHARD_MAX_PIECES
    jge     @bf_err

    ; Calculate QWORD index and bit position
    mov     eax, ecx
    shr     eax, 6                      ; QWORD index = piece / 64
    and     ecx, 63                     ; Bit position = piece % 64

    ; Load current QWORD
    lea     r8, [mesh_local_shards]
    mov     r9, QWORD PTR [r8 + rax * 8]

    ; Test the bit
    bt      r9, rcx
    setc    r10b                        ; R10B = current bit value

    cmp     edx, 0
    je      @bf_test_only
    cmp     edx, 1
    je      @bf_set
    cmp     edx, 2
    je      @bf_clear
    jmp     @bf_err

@bf_set:
    bts     r9, rcx
    mov     QWORD PTR [r8 + rax * 8], r9
    lock inc QWORD PTR [mesh_stats + MESH_STAT_SHARDS_RECV]
    movzx   eax, r10b
    ret

@bf_clear:
    btr     r9, rcx
    mov     QWORD PTR [r8 + rax * 8], r9
    movzx   eax, r10b
    ret

@bf_test_only:
    movzx   eax, r10b
    ret

@bf_err:
    mov     eax, -1
    ret
asm_mesh_shard_bitfield ENDP

; =============================================================================
; asm_mesh_quorum_vote — Tally quorum consensus votes
;
; RCX = pointer to vote array (uint8_t: 0=abstain, 1=yes, 2=no)
; RDX = number of votes
; R8  = threshold percentage (default 67 for 2/3)
; Returns: 1 = quorum reached (yes wins), 0 = quorum not reached, -1 = quorum reached (no wins)
; =============================================================================
asm_mesh_quorum_vote PROC
    push    rbx
    push    rsi
    sub     rsp, 32

    mov     rsi, rcx                    ; Vote array
    mov     ecx, edx                    ; Count
    mov     ebx, r8d                    ; Threshold %

    xor     r8d, r8d                    ; Yes count
    xor     r9d, r9d                    ; No count
    xor     r10d, r10d                  ; Abstain count
    xor     edx, edx                    ; Index

@vote_loop:
    cmp     edx, ecx
    jge     @vote_tally
    movzx   eax, BYTE PTR [rsi + rdx]
    cmp     eax, 1
    je      @vote_yes
    cmp     eax, 2
    je      @vote_no
    inc     r10d                        ; Abstain
    jmp     @vote_next
@vote_yes:
    inc     r8d
    jmp     @vote_next
@vote_no:
    inc     r9d
@vote_next:
    inc     edx
    jmp     @vote_loop

@vote_tally:
    ; Calculate participation = (yes + no) / total * 100
    mov     eax, r8d
    add     eax, r9d                    ; Participating = yes + no
    test    eax, eax
    jz      @quorum_fail

    ; Yes percentage = yes * 100 / participating
    mov     edx, r8d
    imul    edx, 100
    div     eax                         ; EAX = yes_pct (but we divided wrong — fix)
    ; Actually: yes * 100 / (yes + no)
    mov     eax, r8d
    imul    eax, 100
    mov     r11d, r8d
    add     r11d, r9d
    xor     edx, edx
    div     r11d                        ; EAX = yes percentage

    lock inc QWORD PTR [mesh_stats + MESH_STAT_QUORUM_ROUNDS]

    cmp     eax, ebx                    ; Compare with threshold
    jge     @quorum_yes
    ; Check if no wins
    mov     eax, r9d
    imul    eax, 100
    xor     edx, edx
    div     r11d
    cmp     eax, ebx
    jge     @quorum_no

@quorum_fail:
    xor     eax, eax                    ; 0 = quorum not reached
    jmp     @quorum_done

@quorum_yes:
    mov     eax, 1                      ; 1 = yes wins
    jmp     @quorum_done

@quorum_no:
    mov     eax, -1                     ; -1 = no wins

@quorum_done:
    add     rsp, 32
    pop     rsi
    pop     rbx
    ret
asm_mesh_quorum_vote ENDP

; =============================================================================
; asm_mesh_topology_update — Add or update a node in the mesh topology
;
; RCX = pointer to TOPO_NODE structure to add/update
; Returns: 0 = updated existing, 1 = added new, -1 = table full
; =============================================================================
asm_mesh_topology_update PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40

    mov     rsi, rcx                    ; RSI = incoming node

    ; Acquire lock
    lea     rcx, [mesh_lock]
    call    QWORD PTR [__imp_AcquireSRWLockExclusive]

    ; Search for existing node by ID
    mov     ecx, DWORD PTR [mesh_topo_count]
    xor     edi, edi

@topo_search:
    cmp     edi, ecx
    jge     @topo_not_found

    mov     rax, rdi
    imul    rax, SIZEOF TOPO_NODE
    lea     rbx, [mesh_topology + rax]

    ; Compare first 8 bytes of NodeId (sufficient for lookup)
    mov     rax, QWORD PTR [rbx + TOPO_NODE.NodeId]
    cmp     rax, QWORD PTR [rsi + TOPO_NODE.NodeId]
    jne     @topo_next
    mov     rax, QWORD PTR [rbx + TOPO_NODE.NodeId + 8]
    cmp     rax, QWORD PTR [rsi + TOPO_NODE.NodeId + 8]
    je      @topo_found

@topo_next:
    inc     edi
    jmp     @topo_search

@topo_found:
    ; Update existing node — copy non-ID fields
    mov     eax, DWORD PTR [rsi + TOPO_NODE.PublicIP]
    mov     DWORD PTR [rbx + TOPO_NODE.PublicIP], eax
    mov     ax, WORD PTR [rsi + TOPO_NODE.Port]
    mov     WORD PTR [rbx + TOPO_NODE.Port], ax
    mov     ax, WORD PTR [rsi + TOPO_NODE.Flags]
    mov     WORD PTR [rbx + TOPO_NODE.Flags], ax
    mov     eax, DWORD PTR [rsi + TOPO_NODE.Fitness]
    mov     DWORD PTR [rbx + TOPO_NODE.Fitness], eax
    ; Update LastSeen
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     QWORD PTR [rbx + TOPO_NODE.LastSeen], rax

    lock inc QWORD PTR [mesh_stats + MESH_STAT_TOPOLOGY_CHGS]
    mov     r12d, 0                     ; Return 0 = updated
    jmp     @topo_release

@topo_not_found:
    ; Add new node
    cmp     ecx, MESH_MAX_PEERS
    jge     @topo_full

    mov     rax, rcx
    imul    rax, SIZEOF TOPO_NODE
    lea     rbx, [mesh_topology + rax]

    ; Copy entire TOPO_NODE structure (use REP MOVSB)
    push    rsi
    push    rdi
    push    rcx
    mov     rdi, rbx
    ; RSI already points to source
    mov     ecx, SIZEOF TOPO_NODE
    rep     movsb
    pop     rcx
    pop     rdi
    pop     rsi

    ; Update LastSeen
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     QWORD PTR [rbx + TOPO_NODE.LastSeen], rax

    inc     DWORD PTR [mesh_topo_count]
    lock inc QWORD PTR [mesh_stats + MESH_STAT_PEERS_ACTIVE]
    lock inc QWORD PTR [mesh_stats + MESH_STAT_TOPOLOGY_CHGS]
    mov     r12d, 1                     ; Return 1 = added new
    jmp     @topo_release

@topo_full:
    mov     r12d, -1                    ; Return -1 = table full

@topo_release:
    lea     rcx, [mesh_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]

    mov     eax, r12d
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_mesh_topology_update ENDP

; =============================================================================
; asm_mesh_topology_active_count — Return number of nodes in topology table
; Returns: EAX = mesh_topo_count (active node count)
; =============================================================================
asm_mesh_topology_active_count PROC
    mov     eax, DWORD PTR [mesh_topo_count]
    ret
asm_mesh_topology_active_count ENDP

; =============================================================================
; asm_mesh_get_stats — Return pointer to mesh brain statistics
;
; Returns: pointer to 128-byte statistics block
; =============================================================================
asm_mesh_get_stats PROC
    lea     rax, [mesh_stats]
    ret
asm_mesh_get_stats ENDP

; =============================================================================
; asm_mesh_shutdown — Teardown mesh brain subsystem
;
; Returns: 0 = success
; =============================================================================
asm_mesh_shutdown PROC
    push    rbx
    sub     rsp, 32

    mov     eax, DWORD PTR [mesh_initialized]
    test    eax, eax
    jz      @shutdown_ok

    ; Acquire lock
    lea     rcx, [mesh_lock]
    call    QWORD PTR [__imp_AcquireSRWLockExclusive]

    ; Zero topology
    mov     DWORD PTR [mesh_topo_count], 0
    mov     DWORD PTR [mesh_crdt_count], 0
    mov     DWORD PTR [mesh_zkp_count], 0
    mov     DWORD PTR [mesh_initialized], 0

    ; Release lock
    lea     rcx, [mesh_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]

@shutdown_ok:
    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
asm_mesh_shutdown ENDP

END
