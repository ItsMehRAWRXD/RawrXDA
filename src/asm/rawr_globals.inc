; =============================================================================
; rawr_globals.inc — EXTERNDEF Declarations for Centralized Global State
; =============================================================================
; Include this file in any .asm module that needs to read/write shared globals.
; The definitions live in rawr_globals.asm (single translation unit).
;
; Usage:
;   INCLUDE rawr_globals.inc
;
; All symbols are resolved at link time via rawr_globals.obj.
; =============================================================================

IFNDEF RAWR_GLOBALS_INC
RAWR_GLOBALS_INC EQU 1

; ─── CPU Feature Flags ───────────────────────────────────────────────────────
EXTERNDEF g_HasAVX2:DWORD
EXTERNDEF g_HasFMA3:DWORD
EXTERNDEF g_HasAVX512F:DWORD

; ─── Inference Performance Counters ──────────────────────────────────────────
EXTERNDEF g_GemmCalls:QWORD
EXTERNDEF g_GemvCalls:QWORD
EXTERNDEF g_GemmFlops:QWORD

; ─── Inference Dispatch Pointers ─────────────────────────────────────────────
EXTERNDEF g_GemmDispatch:QWORD
EXTERNDEF g_GemvDispatch:QWORD

; ─── Flash Attention AVX-512 ─────────────────────────────────────────────────
EXTERNDEF g_NegInf:DWORD                ; 16x -inf (ZMMWORD), use as ZMMWORD PTR
EXTERNDEF g_AVX512Ready:DWORD
EXTERNDEF g_FlashAttnCalls:QWORD
EXTERNDEF g_FlashAttnTiles:QWORD

; ─── Enterprise License Flags ────────────────────────────────────────────────
EXTERNDEF g_800B_Unlocked:DWORD
EXTERNDEF g_EnterpriseFeatures:QWORD

; ─── Telemetry Counters (cache-line padded) ──────────────────────────────────
EXTERNDEF g_MetricTableStart:QWORD
EXTERNDEF g_Counter_Inference:QWORD
EXTERNDEF g_Counter_ScsiFails:QWORD
EXTERNDEF g_Counter_AgentLoop:QWORD
EXTERNDEF g_Counter_BytePatches:QWORD
EXTERNDEF g_Counter_MemPatches:QWORD
EXTERNDEF g_Counter_ServerPatches:QWORD
EXTERNDEF g_Counter_FlushOps:QWORD
EXTERNDEF g_Counter_Errors:QWORD
EXTERNDEF g_MetricTableEnd:QWORD

; ─── Shell Integration Counters (cache-line padded) ──────────────────────────
EXTERNDEF g_ShellInteg_MetricStart:QWORD
EXTERNDEF g_SI_Counter_CmdsExecuted:QWORD
EXTERNDEF g_SI_Counter_CmdsDetected:QWORD
EXTERNDEF g_SI_Counter_BytesRead:QWORD
EXTERNDEF g_SI_Counter_BytesWritten:QWORD
EXTERNDEF g_SI_Counter_OSC633Injected:QWORD
EXTERNDEF g_SI_Counter_PipeReadErrors:QWORD
EXTERNDEF g_SI_Counter_PipeWriteErrors:QWORD
EXTERNDEF g_SI_Counter_ProcessRestarts:QWORD
EXTERNDEF g_ShellInteg_MetricEnd:QWORD

; ─── Agentic Shell Bridge Counters (cache-line padded) ───────────────────────
EXTERNDEF g_AS_MetricStart:QWORD
EXTERNDEF g_AS_Counter_CtxCaptured:QWORD
EXTERNDEF g_AS_Counter_AIRequests:QWORD
EXTERNDEF g_AS_Counter_AISuccess:QWORD
EXTERNDEF g_AS_Counter_AIFailures:QWORD
EXTERNDEF g_AS_Counter_BytesSentToAI:QWORD
EXTERNDEF g_AS_Counter_BytesFromAI:QWORD
EXTERNDEF g_AS_Counter_CtxDropped:QWORD
EXTERNDEF g_AS_Counter_LocalStubHits:QWORD
EXTERNDEF g_AS_MetricEnd:QWORD

ENDIF ; RAWR_GLOBALS_INC
