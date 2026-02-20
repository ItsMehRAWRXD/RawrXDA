; ============================================================================
; RawrXD Pipe Server - MASM Stub
; Generated: 2026-01-26 06:43:06
; Toolchain: PowerShell masm64.ps1/link64.ps1 compatible
; Exports (via DEF): StartPipeServer, StopPipeServer
; ============================================================================

option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

option win64:3

.code

StartPipeServer PROC
    xor eax, eax
    ret
StartPipeServer ENDP

StopPipeServer PROC
    xor eax, eax
    ret
StopPipeServer ENDP

END
