; ============================================================================
; SovereignTelemetry_x64.asm
; Low-overhead atomic update for SovereignStatsBlockV2
; ============================================================================

.code

; SovereignTelemetry_Update(SovereignStatsBlockV2* block, float tps, float mspt, uint32_t draftAcc, uint32_t draftRej, float pressure)
; RCX: block
; XMM1: tps
; XMM2: mspt
; R8D: draftAcc
; R9D: draftRej
; [RSP+40]: pressure (on stack for 6th param in Win64 ABI)

SovereignTelemetry_Update PROC
    test rcx, rcx
    jz @exit

    ; Increment sequence ID (Relaxed order fine here as we're not using it for a lock)
    inc dword ptr [rcx + 12]

    ; Move floats to registers
    movss dword ptr [rcx + 16], xmm1 ; tokensPerSec
    movss dword ptr [rcx + 20], xmm2 ; msPerToken
    
    ; Update speculative stats using atomic addition (since these are cumulative)
    lock add dword ptr [rcx + 24], r8d ; draftAccepted
    lock add dword ptr [rcx + 28], r9d ; draftRejected

    ; Move memory pressure from stack (Shadow space is 32 bytes, parameters start at RSP+40)
    movss xmm4, dword ptr [rsp + 40]
    movss dword ptr [rcx + 32], xmm4 ; memoryPressure

@exit:
    ret
SovereignTelemetry_Update ENDP

; SovereignTelemetry_UpdateWeights(SovereignStatsBlockV2* block, float w0, float w1, float w2)
; RCX: block
; XMM1: w0
; XMM2: w1
; XMM3: w2
SovereignTelemetry_UpdateWeights PROC
    test rcx, rcx
    jz @exit
    movss dword ptr [rcx + 36], xmm1 ; weightRetain
    movss dword ptr [rcx + 40], xmm2 ; weightCompress
    movss dword ptr [rcx + 44], xmm3 ; weightTierDown
@exit:
    ret
SovereignTelemetry_UpdateWeights ENDP

END
