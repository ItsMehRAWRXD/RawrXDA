; RawrXD_SovereignEnhancements_Master.inc
; Master integration header for +8 Sovereign Enhancements
; Enables unlimited model loading (120B-800B) on consumer hardware
; Version: v1.3.0-sovereign

; ============================================================================
; ENHANCEMENT INDEX
; ============================================================================
; 1. TieredMemoryOrchestrator  - RAM/VRAM/Disk/Remote tiering (2MB pages)
; 2. DynamicQuantization       - Q2_K to Q8_0 runtime hot-patching
; 3. SpeculativeDecoding       - Medusa 4-head tree attention
; 4. KVCacheCompression        - Heavy Hitters + tiered compression
; 5. ContinuousBatching        - PagedAttention request scheduling
; 6. MoEExpertRouting          - 64-expert sparse activation
; 7. AsyncStreamingLoader      - 128MB chunk async I/O with mmap
; 8. ThermalAwareThrottling    - NVML-based dynamic TPS adjustment
; ============================================================================

RAWRXD_FEATURE_TIERED_MEMORY    EQU 1
RAWRXD_FEATURE_DYNAMIC_QUANT    EQU 1
RAWRXD_FEATURE_SPECULATIVE      EQU 1
RAWRXD_FEATURE_KV_COMPRESS      EQU 1
RAWRXD_FEATURE_CONT_BATCH       EQU 1
RAWRXD_FEATURE_MOE_ROUTING      EQU 1
RAWRXD_FEATURE_ASYNC_LOADER     EQU 1
RAWRXD_FEATURE_THERMAL_THROTTLE EQU 1

RAWRXD_TARGET_120B              EQU 120
RAWRXD_TARGET_800B              EQU 800

RAWRXD_VRAM_TIER_PRIORITY       EQU 0
RAWRXD_RAM_TIER_PRIORITY        EQU 1
RAWRXD_DISK_TIER_PRIORITY       EQU 2
RAWRXD_REMOTE_TIER_PRIORITY     EQU 3

RAWRXD_QUANT_Q2_K               EQU 3
RAWRXD_QUANT_Q3_K               EQU 4
RAWRXD_QUANT_Q4_0               EQU 5
RAWRXD_QUANT_Q4_K               EQU 6
RAWRXD_QUANT_Q5_K               EQU 7
RAWRXD_QUANT_Q6_K               EQU 8
RAWRXD_QUANT_Q8_0               EQU 9

RAWRXD_TARGET_TPS_120B          EQU 50
RAWRXD_TARGET_TPS_800B          EQU 10
RAWRXD_TARGET_TTFT_MS           EQU 500

INCLUDE RawrXD_Enh1_TieredMemoryOrchestrator.asm
INCLUDE RawrXD_Enh2_DynamicQuantizationHotpatcher.asm
INCLUDE RawrXD_Enh3_SpeculativeDecodingEngine.asm
INCLUDE RawrXD_Enh4_KVCacheCompression.asm
INCLUDE RawrXD_Enh5_ContinuousBatching.asm
INCLUDE RawrXD_Enh6_MoEExpertRouting.asm
INCLUDE RawrXD_Enh7_AsyncStreamingLoader.asm
INCLUDE RawrXD_Enh8_ThermalAwareThrottling.asm

.CODE
ALIGN 64
SovereignEnhancements_InitializeAll PROC FRAME
    ; rcx = model_size_billions, rdx = vram_bytes, r8 = ram_bytes, r9 = feature_flags
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    .endprolog

    mov     r12, rcx
    mov     r13, rdx
    mov     r14, r8
    mov     r15, r9

    test    r15, RAWRXD_FEATURE_TIERED_MEMORY
    jz      .skip_tiered
    mov     rcx, r12 ; total model size
    mov     rdx, r13
    mov     r8, r14
    xor     r9, r9
    call    TieredOrchestrator_Initialize

.skip_tiered:
    test    r15, RAWRXD_FEATURE_DYNAMIC_QUANT
    jz      .skip_quant
    mov     ecx, RAWRXD_QUANT_Q4_0
    xor     edx, edx
    call    DynamicQuant_Initialize

.skip_quant:
    test    r15, RAWRXD_FEATURE_SPECULATIVE
    jz      .skip_speculative
    mov     ecx, 4096
    mov     edx, 32000
    mov     r8d, 4
    call    SpeculativeDecoding_Initialize

.skip_speculative:
    test    r15, RAWRXD_FEATURE_KV_COMPRESS
    jz      .skip_kv_compress
    mov     ecx, 32
    mov     edx, 32
    mov     r8d, 128
    mov     r9d, 131072
    call    KVCacheCompression_Initialize

.skip_kv_compress:
    test    r15, RAWRXD_FEATURE_CONT_BATCH
    jz      .skip_batching
    mov     rcx, r13
    mov     rdx, 256
    call    ContinuousBatching_Initialize

.skip_batching:
    cmp     r12, RAWRXD_TARGET_800B
    jb      .skip_moe
    test    r15, RAWRXD_FEATURE_MOE_ROUTING
    jz      .skip_moe
    mov     ecx, 64
    mov     edx, 4096
    mov     r8d, 7168
    mov     r9d, 2
    call    MoE_Initialize

.skip_moe:
    test    r15, RAWRXD_FEATURE_THERMAL_THROTTLE
    jz      .skip_thermal
    xor     ecx, ecx
    mov     edx, RAWRXD_TARGET_TPS_120B
    cmp     r12, RAWRXD_TARGET_800B
    jb      .thermal_ok
    mov     edx, RAWRXD_TARGET_TPS_800B
.thermal_ok:
    call    ThermalAwareThrottling_Initialize

.skip_thermal:
    xor     rax, rax
    vzeroupper
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignEnhancements_InitializeAll ENDP

ALIGN 64
SovereignEnhancements_InferenceStep PROC FRAME
    ; rcx = input_tokens, rdx = batch_context, r8 = output_buffer
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    call    ThermalAwareThrottling_GetCurrentTarget
    call    MoE_RouteTokens
    call    SpeculativeDecoding_GenerateDrafts
    call    SpeculativeDecoding_VerifyDrafts
    call    KVCacheCompression_UpdateScores
    call    TieredOrchestrator_MigratePage

    xor     rax, rax
    vzeroupper
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignEnhancements_InferenceStep ENDP

END