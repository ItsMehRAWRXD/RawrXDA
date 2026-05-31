; rawr_aperture_bypass.asm
; x64 MASM implementation of DDR5-to-GPU direct aperture bypass
; Targets AMD 7800 XT GART (Graphics Aperture Remapping Table)
; Uses 2MB huge pages for TLB efficiency
; NOTE: No FRAME directives ? ml64 unwind info has 40-byte prolog limit

.code

; ============================================================================
; BANDWIDTH-AWARE ADAPTIVE STREAMING (100 TPS path)
; Dynamically adjusts stride based on DDR5/PCIe bandwidth ratio
; ============================================================================

; extern "C" void RawrBandwidthAwareStream(void* src, void* dst, size_t size, uint64_t ddr5_bw, uint64_t pcie_bw);
RawrBandwidthAwareStream PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    ; rcx = src, rdx = dst, r8 = size
    ; r9 = ddr5_bw, [rbp+48] = pcie_bw
    mov rsi, rcx            ; src
    mov rdi, rdx            ; dst
    mov rbx, r8             ; size
    mov r12, r9             ; ddr5_bw
    mov r13, [rbp+48]       ; pcie_bw
    
    ; Compute adaptive stride based on bandwidth ratio
    ; If DDR5 >> PCIe, use smaller stride (more aggressive NT stores)
    ; If DDR5 ~ PCIe, use larger stride (conserve bandwidth)
    
    test r13, r13
    jz use_default_stride

use_default_stride:
    mov ecx, 64             ; safe default stride for unknown ratio
    test r13, r13
    jz stride_computed
    
    ; ratio = ddr5_bw / pcie_bw (fixed point 8.8)
    mov rax, r12
    shl rax, 8
    xor edx, edx
    div r13                 ; rax = ratio * 256
    
    ; stride = 64 + (ratio - 1) * 32, clamped to [64, 256]
    ; ratio < 256 (1.0): stride = 64 (most aggressive)
    ; ratio > 512 (2.0): stride = 256 (conservative)
    cmp eax, 256
    jb stride_aggressive
    cmp eax, 512
    ja stride_conservative
    
    ; Linear interpolation for 1.0 < ratio < 2.0
    sub eax, 256            ; (ratio - 1.0) * 256
    shr eax, 3              ; / 8 -> 0-32 range
    shl eax, 5              ; * 32
    add eax, 64             ; 64 + penalty
    mov ecx, eax
    jmp stride_computed
    
stride_aggressive:
    mov ecx, 64             ; max aggression
    jmp stride_computed
    
stride_conservative:
    mov ecx, 256            ; min aggression
    
stride_computed:
    ; ecx = stride (bytes between NT stores)
    mov rax, rsi            ; current src
    mov rdx, rdi            ; current dst
    
adaptive_stream_loop:
    cmp rbx, 64
    jb adaptive_stream_tail
    
    ; Load 64 bytes non-temporal
    db 066h, 0Fh, 038h, 02Ah, 000h          ; vmovntdqa xmm0, [rax]
    db 066h, 0Fh, 038h, 02Ah, 048h, 010h    ; vmovntdqa xmm1, [rax+16]
    db 066h, 0Fh, 038h, 02Ah, 050h, 020h    ; vmovntdqa xmm2, [rax+32]
    db 066h, 0Fh, 038h, 02Ah, 058h, 030h    ; vmovntdqa xmm3, [rax+48]
    
    ; Store 64 bytes non-temporal
    db 066h, 0Fh, 0E7h, 002h               ; movntdq [rdx], xmm0
    db 066h, 0Fh, 0E7h, 04Ah, 010h        ; movntdq [rdx+16], xmm1
    db 066h, 0Fh, 0E7h, 052h, 020h        ; movntdq [rdx+32], xmm2
    db 066h, 0Fh, 0E7h, 05Ah, 030h        ; movntdq [rdx+48], xmm3
    
    add rax, rcx            ; advance by stride
    add rdx, rcx
    sub rbx, rcx
    jmp adaptive_stream_loop
    
adaptive_stream_tail:
    ; Handle remaining bytes (< 64)
    test rbx, rbx
    jz adaptive_stream_done
    
    ; Fallback to regular copy for tail.
    ; rax = current src cursor, rdx = current dst cursor.
    mov rcx, rax
    
tail_copy_loop:
    test rbx, rbx
    jz adaptive_stream_done
    mov al, [rcx]
    mov [rdx], al
    inc rcx
    inc rdx
    dec rbx
    jnz tail_copy_loop
    
adaptive_stream_done:
    db 0Fh, 0AEh, 0F8h      ; sfence
    
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RawrBandwidthAwareStream ENDP

; ============================================================================
; PROACTIVE TENSOR EVICTION (8x MoE swarm)
; Evicts cold tensors before they cause OOM
; ============================================================================

; extern "C" uint32_t RawrProactiveEvictSwarm(void** tensor_ptrs, uint64_t* last_access, 
;                                             uint32_t* access_count, size_t count,
;                                             uint64_t current_time, uint64_t threshold_us,
;                                             uint32_t min_access_count);
; Returns number of evicted tensors
RawrProactiveEvictSwarm PROC
    push rbp
    mov rbp, rsp
    sub rsp, 56
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    ; rcx = tensor_ptrs
    ; rdx = last_access
    ; r8 = access_count
    ; r9 = count
    ; [rbp+48] = current_time
    ; [rbp+56] = threshold_us
    ; [rbp+64] = min_access_count
    
    mov rsi, rcx            ; tensor_ptrs
    mov rdi, rdx            ; last_access
    mov r12, r8             ; access_count
    mov r13, r9             ; count
    mov r14, [rbp+48]       ; current_time
    mov r15, [rbp+56]       ; threshold_us
    mov rbx, [rbp+64]       ; min_access_count
    
    xor eax, eax            ; evicted count
    
evict_swarm_loop:
    test r13, r13
    jz evict_swarm_done
    
    ; Check access count first (cheap check)
    mov ecx, [r12]
    cmp ecx, ebx            ; access_count < min_access_count?
    jb evict_this_tensor
    
    ; Check time since last access
    mov rcx, [rdi]          ; last_access[i]
    test rcx, rcx
    jz skip_evict_swarm     ; null = already evicted
    
    mov rdx, r14
    sub rdx, rcx            ; age = current - last_access
    cmp rdx, r15            ; age > threshold?
    ja evict_this_tensor
    
    jmp skip_evict_swarm
    
evict_this_tensor:
    ; Flush and mark as evicted
    mov rcx, [rsi]
    test rcx, rcx
    jz skip_evict_swarm
    
    push rax
    push rdx
    push r8
    mov rdx, 4096
    call RawrFlushCacheLines
    pop r8
    pop rdx
    pop rax
    
    mov qword ptr [rsi], 0
    mov qword ptr [rdi], 0
    mov dword ptr [r12], 0
    inc eax
    
skip_evict_swarm:
    add rsi, 8
    add rdi, 8
    add r12, 4
    dec r13
    jmp evict_swarm_loop
    
evict_swarm_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
RawrProactiveEvictSwarm ENDP

; ============================================================================
; EXPERT DEDUPLICATION CACHE (swarm shared expert detection)
; Returns bitmask of which experts are already in aperture
; ============================================================================

; extern "C" uint8_t RawrExpertDedupMask(void** expert_ptrs, uint64_t* expert_hashes,
;                                         uint64_t* aperture_hashes, size_t num_experts,
;                                         size_t aperture_count);
; Returns bitmask where bit N=1 means expert N is deduplicated
RawrExpertDedupMask PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    ; rcx = expert_ptrs
    ; rdx = expert_hashes
    ; r8 = aperture_hashes
    ; r9 = num_experts (max 8)
    ; [rbp+48] = aperture_count
    
    mov rsi, rcx            ; expert_ptrs
    mov rdi, rdx            ; expert_hashes
    mov r12, r8             ; aperture_hashes
    mov r13, r9             ; num_experts
    mov rbx, [rbp+48]       ; aperture_count
    
    xor eax, eax            ; result bitmask
    xor ecx, ecx            ; expert index
    
dedup_outer_loop:
    cmp ecx, r13d
    jae dedup_done
    
    ; Get expert hash
    mov r8, [rdi + rcx*8]
    test r8, r8
    jz dedup_next_expert
    
    ; Search aperture hashes for match
    xor edx, edx            ; aperture index
    
dedup_inner_loop:
    cmp rdx, rbx
    jae dedup_next_expert
    
    cmp r8, [r12 + rdx*8]
    jne dedup_next_aperture
    
    ; Match found! Set bit in result
    bts eax, ecx
    jmp dedup_next_expert
    
dedup_next_aperture:
    inc edx
    jmp dedup_inner_loop
    
dedup_next_expert:
    inc ecx
    jmp dedup_outer_loop
    
dedup_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
RawrExpertDedupMask ENDP

; ============================================================================
; SWARM COORDINATOR: PARALLEL AGENT SLOT ASSIGNMENT
; Assigns agent slots to minimize expert cache thrashing
; ============================================================================

; extern "C" void RawrSwarmAssignSlots(uint8_t* agent_expert_ids, size_t num_agents,
;                                      uint8_t* slot_assignments, uint8_t* slot_expert_cache,
;                                      size_t num_slots);
; Assigns agents to slots to maximize expert cache reuse
RawrSwarmAssignSlots PROC
    push rbp
    mov rbp, rsp
    sub rsp, 56
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    
    ; rcx = agent_expert_ids (array of expert IDs each agent needs)
    ; rdx = num_agents
    ; r8 = slot_assignments (output: slot for each agent)
    ; r9 = slot_expert_cache (current expert in each slot)
    ; [rbp+48] = num_slots
    
    mov rsi, rcx            ; agent_expert_ids
    mov rdi, r8             ; slot_assignments
    mov r12, r9             ; slot_expert_cache
    mov r13, [rbp+48]       ; num_slots
    mov r14, rdx            ; num_agents
    
    xor ebx, ebx            ; agent index
    
swarm_assign_loop:
    cmp ebx, r14d
    jae swarm_assign_done
    
    ; Get expert ID for this agent
    movzx eax, byte ptr [rsi + rbx]
    
    ; Search for slot with matching expert (cache hit)
    xor ecx, ecx            ; slot index
    
swarm_slot_search:
    cmp ecx, r13d
    jae swarm_slot_miss
    
    cmp al, [r12 + rcx]
    je swarm_slot_hit
    
    inc ecx
    jmp swarm_slot_search
    
swarm_slot_hit:
    ; Cache hit: assign agent to this slot
    mov byte ptr [rdi + rbx], cl
    jmp swarm_next_agent
    
swarm_slot_miss:
    ; Cache miss: find empty slot or evict LRU
    xor ecx, ecx
    
swarm_find_empty:
    cmp ecx, r13d
    jae swarm_evict_lru
    
    cmp byte ptr [r12 + rcx], 0FFh    ; 0FFh = empty slot marker
    je swarm_use_empty
    
    inc ecx
    jmp swarm_find_empty
    
swarm_use_empty:
    mov byte ptr [rdi + rbx], cl
    mov [r12 + rcx], al     ; update cache
    jmp swarm_next_agent
    
swarm_evict_lru:
    ; Evict slot 0 (simple LRU - could be improved)
    xor ecx, ecx
    mov byte ptr [rdi + rbx], 0
    mov [r12], al
    
swarm_next_agent:
    inc ebx
    jmp swarm_assign_loop
    
swarm_assign_done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
RawrSwarmAssignSlots ENDP

; ============================================================================
; LARGE PAGE ALLOCATION (2MB pages for TLB efficiency)
; ============================================================================

; extern "C" void* RawrAllocateHugePages(size_t size);
RawrAllocateHugePages PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40

    ; rcx = size (bytes)
    mov rax, 200000h        ; 2MB
    dec rax
    add rcx, rax
    not rax
    and rcx, rax            ; aligned size

    xor r9, r9              ; lpAddress = NULL
    mov r8, rcx             ; dwSize
    mov rdx, 20003000h      ; MEM_LARGE_PAGES | MEM_COMMIT | MEM_RESERVE
    mov ecx, 4              ; PAGE_READWRITE

    sub rsp, 32             ; shadow space
    call VirtualAlloc
    add rsp, 32

    mov rsp, rbp
    pop rbp
    ret
RawrAllocateHugePages ENDP

; ============================================================================
; MEMORY PINNING
; ============================================================================

; extern "C" bool RawrPinMemory(void* ptr, size_t size);
RawrPinMemory PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40

    ; rcx = ptr, rdx = size
    mov r8, rcx
    and r8, -4096           ; align down to 4KB

    mov r9, rcx
    sub r9, r8              ; offset from aligned start
    add rdx, r9
    add rdx, 4095
    and rdx, -4096          ; round up size

    mov rcx, r8             ; aligned ptr
    ; rdx = aligned size

    sub rsp, 32
    call VirtualLock
    add rsp, 32

    mov rsp, rbp
    pop rbp
    ret
RawrPinMemory ENDP

; ============================================================================
; MEMORY UNPINNING
; ============================================================================

; extern "C" bool RawrUnpinMemory(void* ptr, size_t size);
RawrUnpinMemory PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40

    mov r8, rcx
    and r8, -4096

    mov r9, rcx
    sub r9, r8
    add rdx, r9
    add rdx, 4095
    and rdx, -4096

    mov rcx, r8

    sub rsp, 32
    call VirtualUnlock
    add rsp, 32

    mov rsp, rbp
    pop rbp
    ret
RawrUnpinMemory ENDP

; ============================================================================
; PREFETCH HINTS
; ============================================================================

; extern "C" void RawrPrefetchMemory(void* ptr, size_t size);
RawrPrefetchMemory PROC
    push rbp
    mov rbp, rsp

    mov rax, rcx            ; start
    add rdx, rcx            ; end

prefetch_loop:
    cmp rax, rdx
    jae prefetch_done

    db 0Fh, 18h, 48h, 00h   ; prefetchnta [rax]

    add rax, 64
    jmp prefetch_loop

prefetch_done:
    pop rbp
    ret
RawrPrefetchMemory ENDP

; ============================================================================
; AGGRESSIVE NON-TEMPORAL STREAM COPY
; ============================================================================

; extern "C" void RawrAggressiveStream(void* src, void* dst, size_t size);
; RCX=src, RDX=dst, R8=size
; Uses AVX2 NT stores to avoid cache pollution when staging large expert blocks.
RawrAggressiveStream PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi

    mov rsi, rcx            ; src
    mov rdi, rdx            ; dst
    mov rbx, r8             ; size

    ; Main loop: 64-byte chunks
stream64_loop:
    cmp rbx, 64
    jb stream_tail

    vmovdqu ymm0, ymmword ptr [rsi]
    vmovdqu ymm1, ymmword ptr [rsi+32]
    vmovntdq ymmword ptr [rdi], ymm0
    vmovntdq ymmword ptr [rdi+32], ymm1

    add rsi, 64
    add rdi, 64
    sub rbx, 64
    jmp stream64_loop

stream_tail:
    test rbx, rbx
    jz stream_done

tail_loop:
    mov al, byte ptr [rsi]
    mov byte ptr [rdi], al
    inc rsi
    inc rdi
    dec rbx
    jnz tail_loop

stream_done:
    sfence                  ; guarantee NT writes visible to device reads
    vzeroupper

    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RawrAggressiveStream ENDP

; ============================================================================
; THREAD AFFINITY
; ============================================================================

; extern "C" bool RawrSetThreadAffinityToNUMA0();
RawrSetThreadAffinityToNUMA0 PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40

    call GetCurrentThread

    mov rcx, rax
    mov rdx, 0FFFFFFFFh     ; first 32 cores

    sub rsp, 32
    call SetThreadAffinityMask
    add rsp, 32

    mov rsp, rbp
    pop rbp
    ret
RawrSetThreadAffinityToNUMA0 ENDP

; ============================================================================
; FLUSH CACHE LINES
; ============================================================================

; extern "C" void RawrFlushCacheLines(void* ptr, size_t size);
RawrFlushCacheLines PROC
    push rbp
    mov rbp, rsp

    mov rax, rcx            ; start
    add rdx, rcx            ; end

flush_loop:
    cmp rax, rdx
    jae flush_done

    db 0Fh, 0AEh, 0F8h      ; clflush [rax]

    add rax, 64
    jmp flush_loop

flush_done:
    db 0Fh, 0AEh, 0F0h      ; mfence

    pop rbp
    ret
RawrFlushCacheLines ENDP

; ============================================================================
; MEMORY BARRIER
; ============================================================================

; extern "C" void RawrMemoryBarrier();
RawrMemoryBarrier PROC
    db 0Fh, 0AEh, 0F0h      ; mfence
    ret
RawrMemoryBarrier ENDP

; ============================================================================
; GET PHYSICAL ADDRESS (stub)
; ============================================================================

; extern "C" uint64_t RawrGetPhysicalAddress(void* ptr);
RawrGetPhysicalAddress PROC
    mov rax, rcx            ; return virtual address (driver translates)
    ret
RawrGetPhysicalAddress ENDP

; ============================================================================
; LARGE PAGE AVAILABILITY CHECK
; ============================================================================

; extern "C" bool RawrLargePagesAvailable();
RawrLargePagesAvailable PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40

    sub rsp, 32
    lea rcx, [rsp+8]
    xor edx, edx
    call OpenProcessToken
    add rsp, 32

    test eax, eax
    jz lp_unavailable

    mov eax, 1
    jmp lp_done

lp_unavailable:
    xor eax, eax

lp_done:
    mov rsp, rbp
    pop rbp
    ret
RawrLargePagesAvailable ENDP

; ============================================================================
; OVERFLOW TIER CHECK
; ============================================================================

; extern "C" uint32_t RawrCheckOverflowTier(float utilization);
RawrCheckOverflowTier PROC
    push rbp
    mov rbp, rsp

    ; ecx = float bits
    mov eax, ecx

    cmp eax, 3F733333h      ; 0.95
    jae tier3
    cmp eax, 3F59999Ah      ; 0.85
    jae tier2
    cmp eax, 3F333333h      ; 0.70 (was 0.75 for 192GB, lowered for 64GB)
    jae tier1

    xor eax, eax            ; 0 = normal
    jmp tier_done

tier1:
    mov eax, 1
    jmp tier_done

tier2:
    mov eax, 2
    jmp tier_done

tier3:
    mov eax, 3

tier_done:
    pop rbp
    ret
RawrCheckOverflowTier ENDP

; ============================================================================
; AGGRESSIVE OVERFLOW TIER CHECK (for 64GB systems - lower thresholds)
; ============================================================================

; extern "C" uint32_t RawrCheckOverflowTierAggressive(float utilization);
; Uses lower thresholds: 60%/75%/90% for tighter memory management
RawrCheckOverflowTierAggressive PROC
    push rbp
    mov rbp, rsp

    ; ecx = float bits
    mov eax, ecx

    cmp eax, 3F666666h      ; 0.90 (was 0.95)
    jae agg_tier3
    cmp eax, 3F400000h      ; 0.75 (was 0.85)
    jae agg_tier2
    cmp eax, 3F19999Ah      ; 0.60 (was 0.70)
    jae agg_tier1

    xor eax, eax            ; 0 = normal
    jmp agg_tier_done

agg_tier1:
    mov eax, 1
    jmp agg_tier_done

agg_tier2:
    mov eax, 2
    jmp agg_tier_done

agg_tier3:
    mov eax, 3

agg_tier_done:
    pop rbp
    ret
RawrCheckOverflowTierAggressive ENDP

; ============================================================================
; APERTURE BYPASS ACTIVATION
; ============================================================================

; extern "C" bool RawrActivateApertureBypass(void* ddr5_base, size_t size, uint32_t flags);
RawrActivateApertureBypass PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64

    ; save params
    mov [rsp+40], rcx       ; base
    mov [rsp+48], rdx       ; size
    mov [rsp+56], r8        ; flags

    ; align to 2MB
    mov rax, 1FFFFFh
    not rax
    and rcx, rax

    mov r9, [rsp+40]
    sub r9, rcx
    add rdx, r9
    add rdx, 1FFFFFh
    and rdx, rax

    sub rsp, 32
    call VirtualLock
    add rsp, 32

    test eax, eax
    jz bypass_fail

    ; prefetch if flag set
    mov r8, [rsp+56]
    test r8, 1
    jz bypass_skip_prefetch

    mov rcx, [rsp+40]
    mov rdx, [rsp+48]
    call RawrPrefetchMemory

bypass_skip_prefetch:
    call RawrMemoryBarrier
    mov eax, 1
    jmp bypass_done

bypass_fail:
    xor eax, eax

bypass_done:
    mov rsp, rbp
    pop rbp
    ret
RawrActivateApertureBypass ENDP

; ============================================================================
; APERTURE BYPASS DEACTIVATION
; ============================================================================

; extern "C" bool RawrDeactivateApertureBypass(void* ddr5_base, size_t size);
RawrDeactivateApertureBypass PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48

    mov rax, 1FFFFFh
    not rax
    and rcx, rax

    mov r9, [rsp+40]
    sub r9, rcx
    add rdx, r9
    add rdx, 1FFFFFh
    and rdx, rax

    sub rsp, 32
    call VirtualUnlock
    add rsp, 32

    mov rsp, rbp
    pop rbp
    ret
RawrDeactivateApertureBypass ENDP

; ============================================================================
; STREAMING PREFETCH
; ============================================================================

; extern "C" void RawrStreamingPrefetch(void* ptr, size_t size, uint32_t tier);
RawrStreamingPrefetch PROC
    push rbp
    mov rbp, rsp
    push rbx

    ; rcx=ptr, rdx=size, r8=tier
    mov eax, 256
    cmp r8d, 0
    je use_stride
    mov eax, 128
    cmp r8d, 1
    je use_stride
    mov eax, 64
    cmp r8d, 2
    je use_stride
    mov eax, 32

use_stride:
    mov ebx, eax
    mov rax, rcx
    add rdx, rcx

stream_loop:
    cmp rax, rdx
    jae stream_done

    db 0Fh, 18h, 48h, 00h   ; prefetchnta [rax]
    cmp r8d, 2
    jb skip_l2
    db 0Fh, 18h, 10h, 00h   ; prefetcht2 [rax]
skip_l2:
    add rax, rbx
    jmp stream_loop

stream_done:
    db 0Fh, 0AEh, 0F0h      ; mfence

    pop rbx
    pop rbp
    ret
RawrStreamingPrefetch ENDP

; ============================================================================
; EXPERT WEIGHTS PRELOAD
; ============================================================================

; extern "C" void RawrPreloadExpertWeights(void** expert_ptrs, size_t num_experts, size_t expert_size);
RawrPreloadExpertWeights PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi

    mov rsi, rcx            ; ptr array
    mov rdi, rdx            ; count
    mov rbx, r8             ; size

expert_loop:
    test rdi, rdi
    jz expert_done

    mov rcx, [rsi]
    mov rdx, rbx
    call RawrPrefetchMemory

    add rsi, 8
    dec rdi
    jmp expert_loop

expert_done:
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RawrPreloadExpertWeights ENDP

; ============================================================================
; DDR5 BANDWIDTH ESTIMATE
; ============================================================================

; extern "C" uint64_t RawrEstimateDDR5Bandwidth();
RawrEstimateDDR5Bandwidth PROC
    mov rax, 75000          ; 75 GB/s conservative
    ret
RawrEstimateDDR5Bandwidth ENDP

; ============================================================================
; PCIe BANDWIDTH ESTIMATE
; ============================================================================

; extern "C" uint64_t RawrEstimatePCIeBandwidth();
RawrEstimatePCIeBandwidth PROC
    mov rax, 31500          ; 31.5 GB/s PCIe 4.0 x16
    ret
RawrEstimatePCIeBandwidth ENDP

; ============================================================================
; APERTURE UTILIZATION
; ============================================================================

; extern "C" float RawrCalculateApertureUtilization(size_t used, size_t total);
RawrCalculateApertureUtilization PROC
    push rbp
    mov rbp, rsp

    test rdx, rdx
    jz util_zero

    cvtsi2ss xmm0, rcx
    cvtsi2ss xmm1, rdx
    divss xmm0, xmm1
    movd eax, xmm0
    jmp util_done

util_zero:
    xor eax, eax

util_done:
    pop rbp
    ret
RawrCalculateApertureUtilization ENDP

; ============================================================================
; LOOKAHEAD PREFETCH
; ============================================================================

; extern "C" void RawrLookaheadPrefetch(void** upcoming_tensors, size_t count, size_t tensor_size);
RawrLookaheadPrefetch PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi

    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r8

lookahead_loop:
    test rdi, rdi
    jz lookahead_done

    mov rcx, [rsi]
    mov rdx, rbx
    call RawrPrefetchMemory

    add rsi, 8
    dec rdi
    jmp lookahead_loop

lookahead_done:
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RawrLookaheadPrefetch ENDP

; ============================================================================
; AGGRESSIVE OVERFLOW FUNCTIONS (Stubs for now - full implementation later)
; ============================================================================

; extern "C" uint32_t RawrPredictOverflowTier(float current_util, float growth_rate);
; extern "C" uint32_t RawrPredictOverflowTier(float current_util, float growth_rate);
; 10-token lookahead formula: pressure = current_util + growth_rate * 10.0
; Aggressive thresholds: 0.60 / 0.75 / 0.90
RawrPredictOverflowTier PROC
    push rbp
    mov rbp, rsp

    ; ecx = current_util  (float bits)
    ; edx = growth_rate   (float bits)

    ; predicted = current_util + growth_rate * 10.0f  (where are we in 10 tokens?)
    movd xmm0, edx
    mulss xmm0, dword ptr [ten_tokens_f]    ; growth_rate * 10
    movd xmm1, ecx
    addss xmm0, xmm1                        ; predicted pressure
    movd ecx, xmm0

    cmp ecx, 3F666666h      ; 0.90f
    jae pred_tier3
    cmp ecx, 3F400000h      ; 0.75f
    jae pred_tier2
    cmp ecx, 3F19999Ah      ; 0.60f
    jae pred_tier1

    xor eax, eax
    jmp pred_done

pred_tier1:
    mov eax, 1
    jmp pred_done

pred_tier2:
    mov eax, 2
    jmp pred_done

pred_tier3:
    mov eax, 3

pred_done:
    pop rbp
    ret

ten_tokens_f dd 41200000h   ; 10.0f
RawrPredictOverflowTier ENDP

; extern "C" uint32_t RawrComputeAdaptiveThrottle(float util, uint64_t ddr5_bw, uint64_t pcie_bw);
RawrComputeAdaptiveThrottle PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    push rbx
    push rsi
    push rdi
    
    ; ecx = util (float bits)
    ; rdx = ddr5_bw (bytes/sec)
    ; r8 = pcie_bw (bytes/sec)
    
    ; Compute adaptive throttle based on bandwidth ratio and utilization
    ; Formula: throttle = min(100, (util * 100) + bandwidth_penalty)
    ; bandwidth_penalty = 0 if ddr5_bw > pcie_bw, else 20
    
    mov rsi, rdx        ; ddr5_bw
    mov rdi, r8         ; pcie_bw
    
    ; Check if DDR5 bandwidth is sufficient
    cmp rsi, rdi
    ja bw_ok
    
    ; DDR5 slower than PCIe - add penalty
    mov ebx, 20
    jmp calc_util
    
bw_ok:
    xor ebx, ebx        ; no penalty
    
calc_util:
    ; Convert utilization float to integer percentage
    ; util is in ecx as float bits
    movd xmm0, ecx
    mulss xmm0, dword ptr [hundred_float]  ; util * 100
    cvttss2si eax, xmm0
    
    ; Add bandwidth penalty
    add eax, ebx
    
    ; Clamp to 0-100
    cmp eax, 100
    cmovg eax, dword ptr [hundred_val]
    test eax, eax
    cmovs eax, dword ptr [zero_val]
    
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
    
hundred_float dd 42C80000h  ; 100.0f
hundred_val dd 100
zero_val dd 0
RawrComputeAdaptiveThrottle ENDP

; extern "C" void RawrProactiveEvict(void** tensor_ptrs, uint64_t* last_access, size_t count, uint64_t current_time, uint64_t threshold_ns);
RawrProactiveEvict PROC
    push rbp
    mov rbp, rsp
    sub rsp, 56
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    ; rcx = tensor_ptrs
    ; rdx = last_access
    ; r8 = count
    ; r9 = current_time
    ; threshold_ns is at [rbp+48] (after pushes)
    
    mov rsi, rcx        ; tensor_ptrs
    mov rdi, rdx        ; last_access
    mov rbx, r8         ; count
    mov r12, r9         ; current_time
    mov r13, [rbp+48]   ; threshold_ns
    
    ; Iterate through tensors and evict cold ones
    xor eax, eax        ; evicted count
    
evict_loop:
    test rbx, rbx
    jz evict_done
    
    ; Check if this tensor is cold
    mov rcx, [rdi]      ; last_access[i]
    sub rcx, r12        ; current_time - last_access
    
    ; If (current_time - last_access) > threshold, evict
    cmp rcx, r13
    jb skip_evict
    
    ; Evict: flush cache and unpin
    mov rcx, [rsi]      ; tensor_ptrs[i]
    test rcx, rcx
    jz skip_evict
    
    ; Flush cache lines (64 bytes per line, assume 4KB minimum)
    push rax
    push rdx
    push r8
    mov rdx, 4096       ; minimum flush size
    call RawrFlushCacheLines
    pop r8
    pop rdx
    pop rax
    
    ; Mark as evicted (set pointer to null)
    mov qword ptr [rsi], 0
    inc eax             ; evicted count++
    
skip_evict:
    add rsi, 8          ; next tensor ptr
    add rdi, 8          ; next last_access
    dec rbx
    jmp evict_loop
    
evict_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
RawrProactiveEvict ENDP

; extern "C" void* RawrTierAwareAlloc(size_t size, uint32_t tier, uint32_t flags);
RawrTierAwareAlloc PROC
    push rbp
    mov rbp, rsp
    sub rsp, 56
    push rbx
    push rsi
    
    ; rcx = size
    ; edx = tier
    ; r8d = flags
    
    mov rbx, rdx        ; tier
    mov rsi, rcx        ; size
    mov r10d, r8d       ; preserve flags
    
    ; Align size based on tier
    ; Tier 0/1: 4KB alignment
    ; Tier 2/3: 2MB alignment (large pages)
    cmp ebx, 2
    jb use_4kb_align
    
    ; 2MB alignment for tier 2/3
    mov rax, 1FFFFFh
    not rax
    add rsi, 1FFFFFh
    and rsi, rax        ; aligned to 2MB
    
    ; Try large pages for tier 2/3
    cmp ebx, 2
    jb skip_large_pages
    
    ; Attempt large page allocation
    mov rcx, rsi
    call RawrAllocateHugePages
    test rax, rax
    jnz tier_alloc_done
    
    ; Fallback to regular allocation
    
skip_large_pages:
use_4kb_align:
    ; 4KB alignment
    mov rax, 0FFFh
    not rax
    add rsi, 0FFFh
    and rsi, rax        ; aligned to 4KB
    
    ; Regular allocation
    xor r9, r9              ; lpAddress = NULL
    mov rcx, rsi            ; dwSize
    mov rdx, 3000h          ; MEM_COMMIT | MEM_RESERVE
    
    ; Set page protection based on flags
    ; FLAG_READ_ONLY (0x04): PAGE_READONLY
    ; FLAG_NON_COHERENT (0x02): PAGE_READWRITE | PAGE_NOCACHE
    mov r8d, 4              ; PAGE_READWRITE (default)
    test r10d, 4            ; FLAG_READ_ONLY
    jz check_nocache
    mov r8d, 2              ; PAGE_READONLY
    jmp do_alloc
    
check_nocache:
    ; Check for non-coherent flag (bypass cache)
    test r10d, 2            ; FLAG_NON_COHERENT
    jz do_alloc
    mov r8d, 104h           ; PAGE_READWRITE | PAGE_NOCACHE
    
do_alloc:
    ; VirtualAlloc(lpAddress=NULL, dwSize=rsi, flAllocationType=0x3000, flProtect=r8d)
    mov r9d, r8d            ; flProtect
    mov r8d, 3000h          ; flAllocationType
    mov rdx, rsi            ; dwSize
    xor rcx, rcx            ; lpAddress = NULL

    sub rsp, 32
    call VirtualAlloc
    add rsp, 32
    
tier_alloc_done:
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
RawrTierAwareAlloc ENDP

; extern "C" void RawrEmergencyCompressPath(void* src, void* dst, size_t size, uint32_t compression_level);
RawrEmergencyCompressPath PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    push rbx
    push rsi
    push rdi
    
    ; rcx = src
    ; rdx = dst
    ; r8 = size
    ; r9d = compression_level (0=none, 1=fast, 2=balanced, 3=max)
    
    mov rsi, rcx        ; src
    mov rdi, rdx        ; dst
    mov rbx, r8         ; size
    
    ; For now, use simple RLE-like compression for zeros
    ; Real implementation would use LZ4 or ZSTD
    
    cmp r9d, 0
    je no_compress
    
    ; Simple zero-run compression
    xor eax, eax        ; output offset
    xor edx, edx        ; input offset
    
compress_loop:
    cmp rdx, rbx
    jae compress_done
    
    ; Check for zero run
    movzx ecx, byte ptr [rsi + rdx]
    test ecx, ecx
    jnz copy_byte
    
    ; Count zero run
    push rax
    mov rax, rdx        ; start of run
    
count_zeros:
    inc rdx
    cmp rdx, rbx
    jae emit_zero_run
    cmp byte ptr [rsi + rdx], 0
    je count_zeros
    
emit_zero_run:
    ; Emit: 0xFF, count (up to 255)
    mov rcx, rdx
    sub rcx, rax        ; run length
    pop rax
    
    cmp ecx, 3
    jb copy_zeros_literal
    
    ; Compressed: 0xFF marker + count
    mov byte ptr [rdi + rax], 0FFh
    inc rax
    mov byte ptr [rdi + rax], cl
    inc rax
    jmp compress_loop
    
copy_zeros_literal:
    ; Not worth compressing, copy literally
    push rdx
    mov rdx, rax        ; restore start
    
copy_zero_lit:
    mov byte ptr [rdi + rax], 0
    inc rax
    inc rdx
    dec ecx
    jnz copy_zero_lit
    pop rdx
    jmp compress_loop
    
copy_byte:
    ; Non-zero byte, copy literally
    mov cl, byte ptr [rsi + rdx]
    mov byte ptr [rdi + rax], cl
    inc rax
    inc rdx
    jmp compress_loop
    
compress_done:
    ; Return compressed size in rax
    jmp emergency_done
    
no_compress:
    ; No compression, just memcpy
    mov rcx, rbx
    rep movsb
    mov rax, rbx        ; return original size
    
emergency_done:
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
RawrEmergencyCompressPath ENDP

; extern "C" void RawrRecordOverflowStats(uint32_t tier, uint64_t bytes_evicted);
RawrRecordOverflowStats PROC
    push rbp
    mov rbp, rsp
    
    ; ecx = tier
    ; rdx = bytes_evicted
    
    ; Select counter by tier
    lea rax, [overflow_stats_tier0_count]
    cmp ecx, 1
    jne stats_tier2
    lea rax, [overflow_stats_tier1_count]
    jmp stats_selected
stats_tier2:
    cmp ecx, 2
    jne stats_tier3
    lea rax, [overflow_stats_tier2_count]
    jmp stats_selected
stats_tier3:
    cmp ecx, 3
    jne stats_selected
    lea rax, [overflow_stats_tier3_count]
stats_selected:
    
    ; Increment tier count
    lock inc dword ptr [rax]
    
    ; Add to total bytes evicted
    lock add qword ptr [overflow_stats_total_bytes], rdx
    lock inc qword ptr [overflow_stats_total_evictions]
    
    mov rsp, rbp
    pop rbp
    ret
RawrRecordOverflowStats ENDP

; Stats storage (BSS section)
.data
overflow_stats_tier0_count dd 0
overflow_stats_tier1_count dd 0
overflow_stats_tier2_count dd 0
overflow_stats_tier3_count dd 0
overflow_stats_total_bytes dq 0
overflow_stats_total_evictions dq 0

; Aggressive overflow tracking
overflow_stats_predictive_promotions dd 0
overflow_stats_emergency_compressions dd 0
overflow_stats_proactive_evictions dd 0
overflow_stats_bandwidth_throttles dd 0
overflow_current_ddr5_bw dq 0
overflow_current_pcie_bw dq 0
overflow_last_util dd 0.0
overflow_growth_rate dd 0.0

; extern "C" void RawrGetOverflowStats(uint32_t* tier_counts_out, uint64_t* total_evictions_out, uint64_t* total_bytes_out);
RawrGetOverflowStats PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = tier_counts_out
    ; rdx = total_evictions_out
    ; r8 = total_bytes_out
    
    ; Copy tier counts atomically
    mov eax, dword ptr [overflow_stats_tier0_count]
    mov dword ptr [rcx], eax
    mov eax, dword ptr [overflow_stats_tier1_count]
    mov dword ptr [rcx+4], eax
    mov eax, dword ptr [overflow_stats_tier2_count]
    mov dword ptr [rcx+8], eax
    mov eax, dword ptr [overflow_stats_tier3_count]
    mov dword ptr [rcx+12], eax
    
    ; Copy total evictions
    mov rax, qword ptr [overflow_stats_total_evictions]
    mov qword ptr [rdx], rax
    
    ; Copy total bytes
    mov rax, qword ptr [overflow_stats_total_bytes]
    mov qword ptr [r8], rax
    
    mov rsp, rbp
    pop rbp
    ret
RawrGetOverflowStats ENDP

; ============================================================================
; AGGRESSIVE OVERFLOW: PREDICTIVE TIER PROMOTION
; ============================================================================

; extern "C" uint32_t RawrPredictiveTierPromotion(float current_util, float growth_rate, uint64_t ddr5_bw, uint64_t pcie_bw);
; Returns the tier we SHOULD be at based on predicted utilization
RawrPredictiveTierPromotion PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
    ; ecx = current_util (float bits)
    ; edx = growth_rate (float bits)
    ; r8 = ddr5_bw
    ; r9 = pcie_bw
    
    ; Store for stats
    mov dword ptr [overflow_last_util], ecx
    mov dword ptr [overflow_growth_rate], edx
    mov qword ptr [overflow_current_ddr5_bw], r8
    mov qword ptr [overflow_current_pcie_bw], r9
    
    ; Predict future utilization: predicted = current + (growth_rate * lookahead_factor)
    ; lookahead_factor = 2.0 for aggressive prediction
    movd xmm0, ecx        ; current_util
    movd xmm1, edx        ; growth_rate
    mulss xmm1, dword ptr [lookahead_factor]  ; growth * 2.0
    addss xmm0, xmm1      ; predicted_util
    movd ecx, xmm0        ; predicted bits
    
    ; Check predicted tier with aggressive thresholds
    cmp ecx, 3F666666h    ; 0.90
    jae pred_promo_tier3
    cmp ecx, 3F400000h    ; 0.75
    jae pred_promo_tier2
    cmp ecx, 3F19999Ah    ; 0.60
    jae pred_promo_tier1
    
    ; Check bandwidth ratio - if DDR5 < PCIe, promote anyway
    cmp r8, r9
    jb force_promotion
    
    xor eax, eax          ; 0 = normal
    jmp pred_promo_done
    
pred_promo_tier1:
    mov eax, 1
    jmp pred_promo_done
    
pred_promo_tier2:
    mov eax, 2
    jmp pred_promo_done
    
pred_promo_tier3:
    mov eax, 3
    jmp pred_promo_done
    
force_promotion:
    ; DDR5 bandwidth insufficient - force tier 1 minimum
    lock inc dword ptr [overflow_stats_bandwidth_throttles]
    mov eax, 1
    
pred_promo_done:
    ; Track predictive promotions
    lock inc dword ptr [overflow_stats_predictive_promotions]
    mov rsp, rbp
    pop rbp
    ret
    
lookahead_factor dd 40000000h  ; 2.0f
RawrPredictiveTierPromotion ENDP

; ============================================================================
; AGGRESSIVE OVERFLOW: BANDWIDTH-AWARE THROTTLE
; ============================================================================

; extern "C" uint32_t RawrBandwidthAwareThrottle(float util, uint64_t ddr5_bw, uint64_t pcie_bw, uint32_t current_tier);
; Returns adaptive throttle value (0-100) based on bandwidth ratio
RawrBandwidthAwareThrottle PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    push rbx
    push rsi
    
    ; ecx = util (float bits)
    ; rdx = ddr5_bw
    ; r8 = pcie_bw
    ; r9d = current_tier
    
    mov rsi, rdx         ; ddr5_bw
    mov rbx, r8          ; pcie_bw
    
    ; Compute bandwidth ratio: ddr5_bw / pcie_bw
    ; If ratio < 1.0, PCIe is bottleneck -> increase throttle
    ; If ratio > 2.0, DDR5 is abundant -> decrease throttle
    
    ; Avoid division by zero
    test rbx, rbx
    jz bw_max_throttle
    
    ; Compute ratio as fixed-point (8.8 format)
    ; ratio = (ddr5_bw * 256) / pcie_bw
    mov rax, rsi
    shl rax, 8           ; * 256
    xor edx, edx
    div rbx              ; rax = ratio * 256
    
    ; ratio < 256 (1.0) -> PCIe bottleneck
    cmp eax, 256
    jb pcie_bottleneck
    
    ; ratio > 512 (2.0) -> DDR5 abundant
    cmp eax, 512
    ja ddr5_abundant
    
    ; Normal case: ratio between 1.0 and 2.0
    ; Base throttle from utilization
    movd xmm0, ecx
    mulss xmm0, dword ptr [hundred_float]
    cvttss2si eax, xmm0
    
    ; Adjust based on tier
    cmp r9d, 2
    jb bw_adjust_done
    add eax, 10          ; +10% for tier 2+
    cmp r9d, 3
    jb bw_adjust_done
    add eax, 15          ; +15% for tier 3
    jmp bw_adjust_done
    
pcie_bottleneck:
    ; PCIe is bottleneck - aggressive throttle
    movd xmm0, ecx
    mulss xmm0, dword ptr [hundred_float]
    cvttss2si eax, xmm0
    add eax, 25          ; +25% penalty for PCIe bottleneck
    lock inc dword ptr [overflow_stats_bandwidth_throttles]
    jmp bw_clamp
    
ddr5_abundant:
    ; DDR5 is abundant - reduce throttle
    movd xmm0, ecx
    mulss xmm0, dword ptr [hundred_float]
    cvttss2si eax, xmm0
    sub eax, 10          ; -10% bonus for DDR5 abundance
    jmp bw_clamp
    
bw_max_throttle:
    mov eax, 100
    jmp bw_adjust_done
    
bw_clamp:
    ; Clamp to 0-100
    cmp eax, 100
    cmovg eax, dword ptr [hundred_val]
    test eax, eax
    cmovs eax, dword ptr [zero_val]
    
bw_adjust_done:
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
RawrBandwidthAwareThrottle ENDP

; ============================================================================
; AGGRESSIVE OVERFLOW: EMERGENCY TENSOR COMPRESSION
; ============================================================================

; extern "C" size_t RawrEmergencyTensorCompress(void* src, void* dst, size_t size, uint32_t tier);
; Returns compressed size, or 0 if compression not beneficial
RawrEmergencyTensorCompress PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    ; rcx = src
    ; rdx = dst
    ; r8 = size
    ; r9d = tier
    
    mov rsi, rcx        ; src
    mov rdi, rdx        ; dst
    mov rbx, r8         ; size
    mov r12d, r9d       ; tier
    
    ; Only compress for tier 2+ (critical memory pressure)
    cmp r12d, 2
    jb no_compress_benefit
    
    ; Check if compression is beneficial (>10% savings expected)
    ; For tensors, we use simple zero-run + sparse encoding
    
    ; First pass: count zeros
    xor r13d, r13d      ; zero count
    xor ecx, ecx       ; offset
    
count_zeros_loop:
    cmp rcx, rbx
    jae count_zeros_done
    cmp byte ptr [rsi + rcx], 0
    jne skip_zero
    inc r13d
skip_zero:
    inc ecx
    jmp count_zeros_loop
    
count_zeros_done:
    ; If >30% zeros, compression is beneficial
    mov rax, rbx
    imul rax, 30        ; 30%
    xor edx, edx
    mov rcx, 100
    div rcx             ; rax = size * 0.30
    cmp r13d, eax
    jb no_compress_benefit
    
    ; Compress using zero-run encoding
    xor eax, eax        ; output offset
    xor edx, edx        ; input offset
    
compress_tensor_loop:
    cmp rdx, rbx
    jae compress_tensor_done
    
    ; Check for zero run
    movzx ecx, byte ptr [rsi + rdx]
    test ecx, ecx
    jnz copy_tensor_byte
    
    ; Count zero run length
    push rax
    mov rax, rdx
    
count_tensor_zeros:
    inc rdx
    cmp rdx, rbx
    jae emit_tensor_zero_run
    cmp byte ptr [rsi + rdx], 0
    je count_tensor_zeros
    
emit_tensor_zero_run:
    mov rcx, rdx
    sub rcx, rax        ; run length
    pop rax
    
    ; Emit: 0xFF marker + 16-bit count (for runs > 3)
    cmp ecx, 3
    jb copy_tensor_zeros_lit
    
    ; Compressed format: 0xFF, count_lo, count_hi
    mov byte ptr [rdi + rax], 0FFh
    inc rax
    mov byte ptr [rdi + rax], cl
    inc rax
    mov byte ptr [rdi + rax], ch
    inc rax
    jmp compress_tensor_loop
    
copy_tensor_zeros_lit:
    ; Not worth compressing
    push rdx
    mov rdx, rax
    
copy_tensor_zero_lit:
    mov byte ptr [rdi + rax], 0
    inc rax
    inc rdx
    dec ecx
    jnz copy_tensor_zero_lit
    pop rdx
    jmp compress_tensor_loop
    
copy_tensor_byte:
    ; Non-zero byte
    mov cl, byte ptr [rsi + rdx]
    cmp cl, 0FFh
    jne emit_literal_byte
    
    ; Escape 0xFF bytes
    mov byte ptr [rdi + rax], 0FEh  ; escape marker
    inc rax
    mov byte ptr [rdi + rax], 0FFh
    inc rax
    inc rdx
    jmp compress_tensor_loop
    
emit_literal_byte:
    mov byte ptr [rdi + rax], cl
    inc rax
    inc rdx
    jmp compress_tensor_loop
    
compress_tensor_done:
    ; Check compression ratio
    cmp eax, ebx
    jae no_compress_benefit
    
    ; Success - track stat
    lock inc dword ptr [overflow_stats_emergency_compressions]
    jmp emergency_compress_done
    
no_compress_benefit:
    ; Compression not beneficial, return 0
    xor eax, eax
    
emergency_compress_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
RawrEmergencyTensorCompress ENDP

; ============================================================================
; AGGRESSIVE OVERFLOW: PROACTIVE COLD TENSOR EVICTION
; ============================================================================

; extern "C" uint32_t RawrProactiveColdEviction(void** tensor_ptrs, uint64_t* last_access, 
;                                               size_t count, uint64_t current_time, 
;                                               uint64_t threshold_ns, uint32_t tier);
; Returns count of evicted tensors
RawrProactiveColdEviction PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    
    ; rcx = tensor_ptrs
    ; rdx = last_access
    ; r8 = count
    ; r9 = current_time
    ; [rbp+48] = threshold_ns
    ; [rbp+56] = tier
    
    mov rsi, rcx        ; tensor_ptrs
    mov rdi, rdx        ; last_access
    mov rbx, r8         ; count
    mov r12, r9        ; current_time
    mov r13, [rbp+48]   ; threshold_ns
    mov r14d, [rbp+56]  ; tier
    
    ; Adjust threshold based on tier
    ; Tier 2: threshold * 0.5 (more aggressive)
    ; Tier 3: threshold * 0.25 (very aggressive)
    cmp r14d, 2
    jb evict_loop_start
    shr r13, 1          ; / 2 for tier 2
    cmp r14d, 3
    jb evict_loop_start
    shr r13, 1          ; / 4 for tier 3
    
evict_loop_start:
    xor eax, eax        ; evicted count
    
evict_cold_loop:
    test rbx, rbx
    jz evict_cold_done
    
    ; Check if tensor is cold
    mov rcx, [rdi]      ; last_access[i]
    test rcx, rcx
    jz skip_cold_evict  ; null = already evicted
    
    ; age = current_time - last_access
    mov rdx, r12
    sub rdx, rcx        ; rdx = age
    
    ; If age > threshold, evict
    cmp rdx, r13
    jb skip_cold_evict
    
    ; Evict: flush cache and mark as null
    mov rcx, [rsi]      ; tensor ptr
    test rcx, rcx
    jz skip_cold_evict
    
    ; Flush cache lines (assume 4KB minimum)
    push rax
    push rdx
    push r8
    mov rdx, 4096
    call RawrFlushCacheLines
    pop r8
    pop rdx
    pop rax
    
    ; Mark as evicted
    mov qword ptr [rsi], 0
    mov qword ptr [rdi], 0
    inc eax
    
    ; Track stat
    lock inc dword ptr [overflow_stats_proactive_evictions]
    
skip_cold_evict:
    add rsi, 8
    add rdi, 8
    dec rbx
    jmp evict_cold_loop
    
evict_cold_done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
RawrProactiveColdEviction ENDP

; ============================================================================
; AGGRESSIVE OVERFLOW: LOOKAHEAD PREFETCH (Depth 4-8)
; ============================================================================

; extern "C" void RawrLookaheadPrefetchDeep(void** upcoming_tensors, size_t count, 
;                                            size_t tensor_size, uint32_t depth);
; Prefetches multiple lookahead levels with tier-aware stride
RawrLookaheadPrefetchDeep PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    push rbx
    push rsi
    push rdi
    push r12
    
    ; rcx = upcoming_tensors
    ; rdx = count
    ; r8 = tensor_size
    ; r9d = depth (1-8)
    
    mov rsi, rcx        ; tensor array
    mov rdi, rdx        ; count
    mov rbx, r8         ; tensor_size
    mov r12d, r9d       ; depth
    
    ; Clamp depth to 8
    cmp r12d, 8
    cmova r12d, dword ptr [max_depth]
    
    ; Prefetch each tensor with decreasing priority
    xor ecx, ecx        ; tensor index
    
deep_prefetch_loop:
    cmp ecx, edi
    jae deep_prefetch_done
    
    ; Calculate prefetch priority based on position
    ; Earlier tensors = higher priority (PREFETCHNTA)
    ; Later tensors = lower priority (PREFETCHT2)
    
    mov rax, rcx
    imul rax, rbx       ; offset = index * tensor_size
    
    ; Get tensor pointer
    mov rdx, [rsi + rcx*8]
    test rdx, rdx
    jz skip_deep_tensor
    
    ; Determine prefetch instruction based on depth position
    cmp ecx, r12d
    jb use_nta_prefetch
    
    ; Use PREFETCHT2 for deeper lookahead
    db 0Fh, 18h, 10h, 00h  ; prefetcht2 [rdx]
    jmp do_deep_prefetch
    
use_nta_prefetch:
    db 0Fh, 18h, 48h, 00h  ; prefetchnta [rdx]
    
do_deep_prefetch:
    ; Prefetch multiple cache lines within tensor
    push rcx
    mov rcx, rdx
    mov r10, rdx
    add r10, rbx
    
line_prefetch_loop:
    cmp rcx, r10
    jae line_prefetch_done
    
    db 0Fh, 18h, 48h, 00h  ; prefetchnta [rcx]
    add rcx, 64         ; next cache line
    jmp line_prefetch_loop
    
line_prefetch_done:
    pop rcx
    
skip_deep_tensor:
    inc ecx
    jmp deep_prefetch_loop
    
deep_prefetch_done:
    ; Memory barrier
    db 0Fh, 0AEh, 0F0h  ; mfence
    
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
    
max_depth dd 8
RawrLookaheadPrefetchDeep ENDP

; ============================================================================
; AGGRESSIVE OVERFLOW: GET EXTENDED STATS
; ============================================================================

; extern "C" void RawrGetExtendedOverflowStats(uint32_t* tier_counts, uint64_t* total_evictions,
;                                              uint64_t* total_bytes, uint32_t* predictive_promos,
;                                              uint32_t* emergency_comps, uint32_t* proactive_evicts,
;                                              uint32_t* bw_throttles);
RawrGetExtendedOverflowStats PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = tier_counts
    ; rdx = total_evictions
    ; r8 = total_bytes
    ; r9 = predictive_promos
    ; [rbp+48] = emergency_comps
    ; [rbp+56] = proactive_evicts
    ; [rbp+64] = bw_throttles
    
    ; Copy basic stats
    mov eax, dword ptr [overflow_stats_tier0_count]
    mov dword ptr [rcx], eax
    mov eax, dword ptr [overflow_stats_tier1_count]
    mov dword ptr [rcx+4], eax
    mov eax, dword ptr [overflow_stats_tier2_count]
    mov dword ptr [rcx+8], eax
    mov eax, dword ptr [overflow_stats_tier3_count]
    mov dword ptr [rcx+12], eax
    
    mov rax, qword ptr [overflow_stats_total_evictions]
    mov qword ptr [rdx], rax
    
    mov rax, qword ptr [overflow_stats_total_bytes]
    mov qword ptr [r8], rax
    
    ; Copy extended stats
    mov eax, dword ptr [overflow_stats_predictive_promotions]
    mov dword ptr [r9], eax
    
    mov rax, [rbp+48]
    mov eax, dword ptr [overflow_stats_emergency_compressions]
    mov dword ptr [rax], eax
    
    mov rax, [rbp+56]
    mov eax, dword ptr [overflow_stats_proactive_evictions]
    mov dword ptr [rax], eax
    
    mov rax, [rbp+64]
    mov eax, dword ptr [overflow_stats_bandwidth_throttles]
    mov dword ptr [rax], eax
    
    mov rsp, rbp
    pop rbp
    ret
RawrGetExtendedOverflowStats ENDP

; ============================================================================
; EXPORTS
; ============================================================================

PUBLIC RawrAllocateHugePages
PUBLIC RawrPinMemory
PUBLIC RawrUnpinMemory
PUBLIC RawrPrefetchMemory
PUBLIC RawrSetThreadAffinityToNUMA0
PUBLIC RawrFlushCacheLines
PUBLIC RawrMemoryBarrier
PUBLIC RawrGetPhysicalAddress
PUBLIC RawrLargePagesAvailable
PUBLIC RawrCheckOverflowTier
PUBLIC RawrCheckOverflowTierAggressive
PUBLIC RawrActivateApertureBypass
PUBLIC RawrPredictiveTierPromotion
PUBLIC RawrBandwidthAwareThrottle
PUBLIC RawrEmergencyTensorCompress
PUBLIC RawrProactiveColdEviction
PUBLIC RawrLookaheadPrefetchDeep
PUBLIC RawrGetExtendedOverflowStats
PUBLIC RawrDeactivateApertureBypass
PUBLIC RawrStreamingPrefetch
PUBLIC RawrPreloadExpertWeights
PUBLIC RawrEstimateDDR5Bandwidth
PUBLIC RawrEstimatePCIeBandwidth
PUBLIC RawrCalculateApertureUtilization
PUBLIC RawrLookaheadPrefetch
PUBLIC RawrPredictOverflowTier
PUBLIC RawrComputeAdaptiveThrottle
PUBLIC RawrProactiveEvict
PUBLIC RawrTierAwareAlloc
PUBLIC RawrEmergencyCompressPath
PUBLIC RawrRecordOverflowStats
PUBLIC RawrGetOverflowStats
PUBLIC RawrAggressiveStream

; ============================================================================
; PREDICTIVE OVERFLOW DETECTION
; ============================================================================

; extern "C" uint32_t RawrPredictOverflowTime(float current_util, float growth_rate, uint64_t total_memory);
RawrPredictOverflowTime PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
    ; ecx = current_util (float bits)
    ; edx = growth_rate (float bits, bytes/ms)
    ; r8 = total_memory (bytes)
    
    ; Predict time until critical (95% utilization)
    ; time_ms = (0.95 - current_util) * total_memory / growth_rate
    
    ; Load 0.95 constant
    movss xmm0, dword ptr [critical_threshold]
    movd xmm1, ecx        ; current_util
    subss xmm0, xmm1      ; 0.95 - current_util
    
    ; If current_util >= 0.95, return 0 (already critical)
    comiss xmm1, xmm0
    jbe not_critical_yet
    xor eax, eax
    jmp predict_done
    
not_critical_yet:
    ; Convert to bytes needed
    cvtsi2ss xmm2, r8     ; total_memory
    mulss xmm0, xmm2      ; bytes until critical
    
    ; Divide by growth rate
    movd xmm3, edx        ; growth_rate
    divss xmm0, xmm3      ; time in ms
    
    ; Convert to integer
    cvttss2si eax, xmm0
    
predict_done:
    mov rsp, rbp
    pop rbp
    ret
    
critical_threshold dd 3F733333h  ; 0.95f
RawrPredictOverflowTime ENDP

; ============================================================================
; BANDWIDTH-AWARE THROTTLE ADJUSTMENT
; ============================================================================

; extern "C" uint32_t RawrAdjustThrottleForBandwidth(uint32_t base_throttle, uint64_t ddr5_bw, uint64_t pcie_bw, uint64_t gpu_compute_bw);
RawrAdjustThrottleForBandwidth PROC
    push rbp
    mov rbp, rsp
    
    ; ecx = base_throttle (0-100)
    ; rdx = ddr5_bw
    ; r8 = pcie_bw
    ; r9 = gpu_compute_bw
    
    ; Compute bandwidth bottleneck
    ; bottleneck = min(ddr5_bw, pcie_bw, gpu_compute_bw)
    ; If bottleneck is PCIe, increase throttle
    
    mov rax, rdx        ; ddr5_bw
    cmp rax, r8
    cmova rax, r8        ; min(ddr5, pcie)
    cmp rax, r9
    cmova rax, r9        ; min(all three)
    
    ; If PCIe is bottleneck (pcie_bw < ddr5_bw), add 10% throttle
    cmp r8, rdx
    jae no_pcie_penalty
    add ecx, 10
    
no_pcie_penalty:
    ; If GPU compute is bottleneck, add 5% throttle
    cmp r9, rdx
    jae no_gpu_penalty
    add ecx, 5
    
no_gpu_penalty:
    ; Clamp to 0-100
    cmp ecx, 100
    cmovg ecx, dword ptr [hundred_val_bw]
    
    mov eax, ecx
    
    pop rbp
    ret
    
hundred_val_bw dd 100
RawrAdjustThrottleForBandwidth ENDP

; ============================================================================
; TIER TRANSITION HISTOGRAM
; ============================================================================

; extern "C" void RawrRecordTierTransition(uint32_t from_tier, uint32_t to_tier);
RawrRecordTierTransition PROC
    push rbp
    mov rbp, rsp
    
    ; ecx = from_tier
    ; edx = to_tier
    
    ; Record transition in histogram (4x4 matrix)
    ; transition_histogram[from_tier * 4 + to_tier]++
    
    shl ecx, 2           ; from_tier * 4
    add ecx, edx         ; + to_tier
    lea rax, [tier_transition_histogram]
    add rax, rcx
    lock inc dword ptr [rax]
    
    pop rbp
    ret
RawrRecordTierTransition ENDP

.data
tier_transition_histogram dd 16 dup(0)  ; 4x4 matrix

; ============================================================================
; AGGRESSIVE PREFETCH WITH BANDWIDTH ESTIMATION
; ============================================================================

; extern "C" void RawrAggressivePrefetch(void* ptr, size_t size, uint32_t tier, uint64_t available_bw);
RawrAggressivePrefetch PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    
    ; rcx = ptr
    ; rdx = size
    ; r8d = tier
    ; r9 = available_bw (bytes/sec)
    
    mov rsi, rcx        ; ptr
    mov rbx, rdx        ; size
    
    ; Compute stride based on available bandwidth
    ; Higher bandwidth = more aggressive prefetch (smaller stride)
    ; stride = max(32, 256 - (available_bw / 1GB) * 16)
    
    mov rax, r9
    mov rcx, 1024*1024*1024  ; 1GB
    xor edx, edx
    div rcx              ; available_bw / 1GB
    shl eax, 4           ; * 16
    mov ecx, 256
    sub ecx, eax         ; 256 - penalty
    cmp ecx, 32
    cmovl ecx, dword ptr [min_stride]  ; min 32
    
    ; Prefetch loop with computed stride
    mov rax, rsi        ; current ptr
    add rbx, rsi        ; end ptr
    
aggressive_prefetch_loop:
    cmp rax, rbx
    jae aggressive_prefetch_done
    
    ; Prefetch to L1 (NTA)
    db 0Fh, 18h, 48h, 00h   ; prefetchnta [rax]
    
    ; For tier 2+, also prefetch to L2
    cmp r8d, 2
    jb skip_l2_prefetch
    db 0Fh, 18h, 10h, 00h   ; prefetcht2 [rax]
    
skip_l2_prefetch:
    add rax, rcx        ; stride
    jmp aggressive_prefetch_loop
    
aggressive_prefetch_done:
    db 0Fh, 0AEh, 0F0h      ; mfence
    
    pop rsi
    pop rbx
    pop rbp
    ret
    
min_stride dd 32
RawrAggressivePrefetch ENDP

PUBLIC RawrPredictOverflowTime
PUBLIC RawrAdjustThrottleForBandwidth
PUBLIC RawrRecordTierTransition
PUBLIC RawrAggressivePrefetch
PUBLIC RawrProactiveEvict
PUBLIC RawrTierAwareAlloc
PUBLIC RawrEmergencyCompressPath
PUBLIC RawrRecordOverflowStats
PUBLIC RawrGetOverflowStats
PUBLIC RAWR_Aggressive_Stream
PUBLIC RAWR_DoubleBuffer_Swap
PUBLIC RAWR_ExpertCache_Probe
PUBLIC RAWR_PCIe_FlushBarrier
PUBLIC RAWR_SwarmSlot_Prefetch
PUBLIC RawrBandwidthAwareStream
PUBLIC RawrProactiveEvictSwarm
PUBLIC RawrExpertDedupMask
PUBLIC RawrSwarmAssignSlots

EXTRN VirtualAlloc:PROC
EXTRN VirtualLock:PROC
EXTRN VirtualUnlock:PROC
EXTRN GetCurrentThread:PROC
EXTRN SetThreadAffinityMask:PROC
EXTRN OpenProcessToken:PROC

; ============================================================================
; NON-TEMPORAL STREAMING (bypasses L3 cache entirely)
; Prevents "Cache Poisoning" of the MoE router logic
; RCX: Source (DDR5 aperture base)
; RDX: Destination (GPU aperture window)
; R8:  Count (bytes, must be multiple of 64)
; ============================================================================
; ============================================================================
; AGGRESSIVE NT STREAM - AVX2 YMM 256-bit firehose
; Loads 2x32B via VMOVNTDQA ymm (bypasses L3 on Zen4/Raptor Lake)
; Prefetches 2 blocks (128B) ahead to hide DDR5 row-activation latency
; RCX: Source (DDR5), RDX: Destination (GPU aperture), R8: bytes (mult of 64)
; ============================================================================
RAWR_Aggressive_Stream PROC
    push rbp
    mov rbp, rsp
    align 16

    ; Zero-byte fast return
    test r8, r8
    jz stream_nt_done

    ; NT YMM path requires 32-byte alignment for vmovntdqa/vmovntdq.
    ; If either pointer is unaligned, fall back to safe scalar copy.
    test rcx, 1Fh
    jnz stream_scalar_loop
    test rdx, 1Fh
    jnz stream_scalar_loop

stream_nt_loop:
    cmp r8, 64
    jb stream_tail_loop

    test r8, r8
    jz stream_nt_done

    ; Software prefetch 2 iterations ahead (128 bytes) - hides ~80ns DDR5 latency
    db 0Fh, 18h, 81h, 80h, 00h, 00h, 00h   ; prefetchnta [rcx+128]
    db 0Fh, 18h, 81h, 0C0h, 00h, 00h, 00h  ; prefetchnta [rcx+192]

    ; Load 64 bytes non-temporal via AVX2 YMM (2x 32-byte, fully bypasses L3)
    db 0C4h, 0E2h, 07Dh, 02Ah, 001h        ; vmovntdqa ymm0, [rcx]
    db 0C4h, 0E2h, 07Dh, 02Ah, 049h, 020h  ; vmovntdqa ymm1, [rcx+32]

    ; Write 64 bytes non-temporal to GPU aperture (no RFO, direct to PCIe write buffer)
    db 0C5h, 0FDh, 0E7h, 002h              ; vmovntdq [rdx], ymm0
    db 0C5h, 0FDh, 0E7h, 04Ah, 020h        ; vmovntdq [rdx+32], ymm1

    add rcx, 64
    add rdx, 64
    sub r8, 64
    jmp stream_nt_loop

stream_scalar_loop:
    test r8, r8
    jz stream_nt_done

    mov al, byte ptr [rcx]
    mov byte ptr [rdx], al
    inc rcx
    inc rdx
    dec r8
    jmp stream_scalar_loop

stream_tail_loop:
    test r8, r8
    jz stream_nt_done

    mov al, byte ptr [rcx]
    mov byte ptr [rdx], al
    inc rcx
    inc rdx
    dec r8
    jmp stream_tail_loop

stream_nt_done:
    vzeroupper                              ; avoid AVX-SSE transition penalty
    db 0Fh, 0AEh, 0F8h                     ; sfence: commit NT buffers -> PCIe window
    pop rbp
    ret
RAWR_Aggressive_Stream ENDP

; ============================================================================
; DOUBLE-BUFFER APERTURE SWAP
; Atomically swaps active/shadow buffer pointers so the GPU never stalls
; RCX: pointer to active_ptr (qword)
; RDX: new shadow buffer address
; Returns old active_ptr in RAX
; ============================================================================
RAWR_DoubleBuffer_Swap PROC
    ; xchg with memory operand is atomic on x86/x64 (implicit lock)
    mov rax, rdx
    xchg qword ptr [rcx], rax
    ; sfence ensures NT stores to shadow buffer are committed before caller
    ; proceeds with the new active pointer
    db 0Fh, 0AEh, 0F8h      ; sfence
    ret
RAWR_DoubleBuffer_Swap ENDP

; ============================================================================
; EXPERT CACHE PROBE
; Checks whether an expert weight block is resident (accessed recently)
; RCX: expert_id (0-7 for 8x MoE)
; RDX: pointer to expert_last_access table (8 x uint64 timestamps)
; R8:  current_time_us
; R9:  warm_threshold_us  (e.g. 200000 = 200ms)
; Returns 1 in EAX if warm, 0 if cold
; ============================================================================
RAWR_ExpertCache_Probe PROC
    ; last_access = expert_last_access[expert_id]
    mov rax, [rdx + rcx*8]
    test rax, rax
    jz probe_cold

    ; age = current_time - last_access
    mov r10, r8
    sub r10, rax
    cmp r10, r9
    ja probe_cold

    mov eax, 1              ; warm
    ret
probe_cold:
    xor eax, eax            ; cold
    ret
RAWR_ExpertCache_Probe ENDP

; ============================================================================
; PCIe FLUSH BARRIER
; clflushopt + sfence: forces DDR5 dirty lines into the PCIe window < 500ns
; RCX: ptr
; RDX: size (bytes)
; ============================================================================
RAWR_PCIe_FlushBarrier PROC
    push rbp
    mov rbp, rsp
    mov rax, rcx
    add rdx, rcx            ; end
flush_opt_loop:
    cmp rax, rdx
    jae flush_opt_done
    db 066h, 0Fh, 0AEh, 038h   ; clflushopt [rax]
    add rax, 64
    jmp flush_opt_loop
flush_opt_done:
    db 0Fh, 0AEh, 0F8h          ; sfence
    pop rbp
    ret
RAWR_PCIe_FlushBarrier ENDP

; ============================================================================
; SWARM SLOT PREFETCH
; Prefetches expert weights for a specific swarm agent slot
; so parallel cursor agents don't fight over the same cache lines
; RCX: expert_ptr
; RDX: expert_size
; R8D: agent_slot (0-7) - used to select prefetch hint level
;      slots 0-1: PREFETCHNTA (highest priority, streaming)
;      slots 2-3: PREFETCHT0  (L1)
;      slots 4-7: PREFETCHT2  (L2, background)
; ============================================================================
RAWR_SwarmSlot_Prefetch PROC
    push rbp
    mov rbp, rsp
    push rbx
    ; select prefetch hint based on slot priority
    xor ebx, ebx            ; hint selector: 0=NTA, 1=T0, 2=T2
    cmp r8d, 2
    jb slot_hint_selected
    mov ebx, 1
    cmp r8d, 4
    jb slot_hint_selected
    mov ebx, 2
slot_hint_selected:
    mov rax, rcx            ; current
    add rdx, rcx            ; end
swarm_pf_loop:
    cmp rax, rdx
    jae swarm_pf_done
    ; branch on hint level
    test ebx, ebx
    jnz swarm_try_t0
    db 0Fh, 18h, 00h        ; prefetchnta [rax]
    jmp swarm_pf_advance
swarm_try_t0:
    cmp ebx, 1
    jne swarm_use_t2
    db 0Fh, 18h, 08h        ; prefetcht0 [rax]
    jmp swarm_pf_advance
swarm_use_t2:
    db 0Fh, 18h, 10h        ; prefetcht2 [rax]
swarm_pf_advance:
    add rax, 64
    jmp swarm_pf_loop
swarm_pf_done:
    db 0Fh, 0AEh, 0F0h      ; mfence
    pop rbx
    pop rbp
    ret
RAWR_SwarmSlot_Prefetch ENDP

; ============================================================================
; CLFLUSHOPT RANGE
; Zero-copy barrier primitive: clflushopt + mfence for aperture visibility
; RCX: ptr
; RDX: size (bytes)
; ============================================================================
; extern "C" void RawrClflushoptRange(void* ptr, size_t size);
RawrClflushoptRange PROC
    push rbp
    mov rbp, rsp

    mov rax, rcx            ; start
    add rdx, rcx            ; end

clflushopt_loop:
    cmp rax, rdx
    jae clflushopt_done

    db 066h, 0Fh, 0AEh, 038h   ; clflushopt [rax]
    add rax, 64
    jmp clflushopt_loop

clflushopt_done:
    db 0Fh, 0AEh, 0F0h         ; mfence
    pop rbp
    ret
RawrClflushoptRange ENDP

; ============================================================================
; COMPUTE PRESSURE
; Implements AggressiveOverflowManager::CalculatePressure
; pressure = vram_used_frac + growth_rate * 10.0  (10-token lookahead)
; ECX: vram_used_frac (float bits)
; EDX: growth_rate    (float bits)
; Returns pressure float bits in EAX
; ============================================================================
; extern "C" float RawrComputePressure(float vram_used_frac, float growth_rate);
RawrComputePressure PROC
    movd xmm0, edx
    mulss xmm0, dword ptr [cp_ten_f]    ; growth_rate * 10
    movd xmm1, ecx
    addss xmm0, xmm1                    ; + vram_used_frac
    movd eax, xmm0
    ret
cp_ten_f dd 41200000h                   ; 10.0f
RawrComputePressure ENDP

; ============================================================================
; SET PREFETCH DEPTH
; Maps pressure float to prefetch depth for double-buffered aperture pinning
;   pressure > 0.95 -> 8  (TIER_EMERGENCY: max PCIe parallelism, enable compression)
;   pressure > 0.80 -> 4  (TIER_STRIDE)
;   else            -> 2  (TIER_HYBRID / TIER_STEADY)
; ECX: pressure (float bits)
; Returns depth in EAX
; ============================================================================
; extern "C" uint32_t RawrSetPrefetchDepth(float pressure);
RawrSetPrefetchDepth PROC
    cmp ecx, 3F733333h      ; 0.95f
    jae depth_8
    cmp ecx, 3F4CCCCDh      ; 0.80f
    jae depth_4
    mov eax, 2
    ret
depth_4:
    mov eax, 4
    ret
depth_8:
    mov eax, 8
    ret
RawrSetPrefetchDepth ENDP

PUBLIC RawrComputePressure
PUBLIC RawrSetPrefetchDepth
PUBLIC RawrClflushoptRange

END
