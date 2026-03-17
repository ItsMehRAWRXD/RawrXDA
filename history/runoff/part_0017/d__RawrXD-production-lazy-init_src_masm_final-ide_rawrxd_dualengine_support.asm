; ==========================================================================
; rawrxd_dualengine_support.asm - Dual-engine support stubs (Pure MASM)
; ==========================================================================
; Provides small support entrypoints referenced by the dual-engine modules.
; These match the simple behaviors in final-ide/main_masm.asm but avoid
; pulling in an additional executable entrypoint (main/_start) into libs.
; ==========================================================================

option casemap:none

.data
szHotPatchPath DB "hotpatch_engine",0

.code

PUBLIC RawrXD_GetSystemLoad
RawrXD_GetSystemLoad PROC
    ; Returns system load percentage (0-100)
    mov eax, 50
    ret
RawrXD_GetSystemLoad ENDP

PUBLIC RawrXD_HotPatchEnginePath
RawrXD_HotPatchEnginePath PROC
    lea rax, szHotPatchPath
    ret
RawrXD_HotPatchEnginePath ENDP

PUBLIC RawrXD_ProcessStreamingChunk
RawrXD_ProcessStreamingChunk PROC
    ; rcx = chunk data, rdx = chunk size
    mov eax, 1
    ret
RawrXD_ProcessStreamingChunk ENDP

PUBLIC RawrXD_PerformanceLogMetrics
RawrXD_PerformanceLogMetrics PROC
    ; rcx = metric name, rdx = metric value
    mov eax, 1
    ret
RawrXD_PerformanceLogMetrics ENDP

END
