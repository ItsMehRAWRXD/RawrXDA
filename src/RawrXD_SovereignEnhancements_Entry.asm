; RawrXD_SovereignEnhancements_Entry.asm
; Standalone sovereign entrypoint shim for DLL export surface.

OPTION CASEMAP:NONE

RAWRXD_FEATURE_TIERED_MEMORY    EQU 1
RAWRXD_FEATURE_DYNAMIC_QUANT    EQU 1
RAWRXD_FEATURE_SPECULATIVE      EQU 1
RAWRXD_FEATURE_KV_COMPRESS      EQU 1
RAWRXD_FEATURE_CONT_BATCH       EQU 1
RAWRXD_FEATURE_MOE_ROUTING      EQU 1
RAWRXD_FEATURE_THERMAL_THROTTLE EQU 1

RAWRXD_TARGET_800B              EQU 800
RAWRXD_TARGET_TPS_120B          EQU 50
RAWRXD_TARGET_TPS_800B          EQU 10

EXTERN TieredOrchestrator_Initialize:PROC
EXTERN DynamicQuant_Initialize:PROC
EXTERN SpeculativeDecoding_Initialize:PROC
EXTERN KVCacheCompression_Initialize:PROC
EXTERN ContinuousBatching_Initialize:PROC
EXTERN MoE_Initialize:PROC
EXTERN ThermalAwareThrottling_Initialize:PROC
EXTERN ThermalAwareThrottling_GetCurrentTarget:PROC
EXTERN MoE_RouteTokens:PROC
EXTERN SpeculativeDecoding_GenerateDrafts:PROC
EXTERN SpeculativeDecoding_VerifyDrafts:PROC
EXTERN KVCacheCompression_UpdateScores:PROC
EXTERN TieredOrchestrator_MigratePage:PROC

.CODE

SovereignEnhancements_InitializeAll PROC
    ; rcx=model_size_billions, rdx=available_vram_bytes, r8=available_ram_bytes, r9=feature_flags
    push rbx
    push rsi

    mov rsi, r9

    test rsi, RAWRXD_FEATURE_TIERED_MEMORY
    jz skip_tiered
    mov r9, 0
    call TieredOrchestrator_Initialize
skip_tiered:

    test rsi, RAWRXD_FEATURE_DYNAMIC_QUANT
    jz skip_quant
    xor rcx, rcx
    mov edx, 4
    mov r8d, 90
    call DynamicQuant_Initialize
skip_quant:

    test rsi, RAWRXD_FEATURE_SPECULATIVE
    jz skip_spec
    mov ecx, 4096
    mov edx, 32000
    mov r8d, 4
    call SpeculativeDecoding_Initialize
skip_spec:

    test rsi, RAWRXD_FEATURE_KV_COMPRESS
    jz skip_kv
    mov ecx, 32
    mov edx, 32
    mov r8d, 128
    mov r9d, 131072
    call KVCacheCompression_Initialize
skip_kv:

    test rsi, RAWRXD_FEATURE_CONT_BATCH
    jz skip_batch
    mov rcx, rdx
    mov rdx, 256
    call ContinuousBatching_Initialize
skip_batch:

    test rsi, RAWRXD_FEATURE_MOE_ROUTING
    jz skip_moe
    cmp rcx, RAWRXD_TARGET_800B
    jb skip_moe
    mov ecx, 64
    mov edx, 4096
    mov r8d, 7168
    mov r9d, 2
    call MoE_Initialize
skip_moe:

    test rsi, RAWRXD_FEATURE_THERMAL_THROTTLE
    jz done
    xor ecx, ecx
    mov edx, RAWRXD_TARGET_TPS_120B
    cmp rcx, RAWRXD_TARGET_800B
    jb thermal_ok
    mov edx, RAWRXD_TARGET_TPS_800B
thermal_ok:
    call ThermalAwareThrottling_Initialize

done:
    xor eax, eax
    pop rsi
    pop rbx
    ret
SovereignEnhancements_InitializeAll ENDP

SovereignEnhancements_InferenceStep PROC
    ; rcx=input_tokens, rdx=batch_ctx, r8=output_buf
    call ThermalAwareThrottling_GetCurrentTarget
    call MoE_RouteTokens
    call SpeculativeDecoding_GenerateDrafts
    call SpeculativeDecoding_VerifyDrafts
    call KVCacheCompression_UpdateScores

    xor rcx, rcx
    mov edx, 1
    xor r8d, r8d
    call TieredOrchestrator_MigratePage

    xor eax, eax
    ret
SovereignEnhancements_InferenceStep ENDP

END
