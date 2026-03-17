option casemap:none
IFNDEF RAWR_API_INC_GUARD
RAWR_API_INC_GUARD EQU 1

; ---- calling convention: Win64 (RCX,RDX,R8,R9 + shadow) ----
; All exported procs MUST:
; - preserve RBX RBP RSI RDI R12-R15
; - preserve XMM6-XMM15 if touched
; - align RSP to 16 before any CALL
; - never assume args live in home space

; ---- Engine entrypoints ----
EXTERN RawrDumpBin_Create              : PROC  ; ()
EXTERN RawrDumpBin_Destroy             : PROC  ; (ctx*)
EXTERN RawrDumpBin_ParsePE             : PROC  ; (ctx*, mapped_base, size)
EXTERN RawrDumpBin_GetSections         : PROC  ; (ctx*, out_sections*, out_count*)
EXTERN RawrDumpBin_GetExports          : PROC  ; (ctx*, out_exports*, out_count*)
EXTERN RawrDumpBin_GetImports          : PROC  ; (ctx*, out_imports*, out_count*)
EXTERN RawrDumpBin_RVAToFileOffset     : PROC  ; (ctx*, rva, out_offset*)
EXTERN Codex_Create                    : PROC  ; (path_w, out_ctx*)
EXTERN Codex_Destroy                   : PROC  ; (ctx*)
EXTERN Codex_AnalyzeSecurity           : PROC  ; (ctx*, out_report*)
EXTERN Codex_CalculateEntropy          : PROC  ; (data_ptr, data_sz, out_entropy_f64*)
EXTERN Codex_GetEnhancedSections       : PROC  ; (ctx*, out_sections*, out_count*)
EXTERN NativeAgent_Initialize          : PROC  ; (cfg_ptr)
EXTERN NativeAgent_UpdateConfig        : PROC  ; (cfg_ptr)
EXTERN NativeAgent_ProcessTask         : PROC  ; (task_ptr, task_sz, out_result_ptr*, out_result_sz*)
EXTERN NativeAgent_GetSessionStatus    : PROC  ; (out_status*)
EXTERN CPUInference_Initialize         : PROC  ; (model_path_w, out_ctx*)
EXTERN CPUInference_Destroy            : PROC  ; (ctx*)
EXTERN CPUInference_RunForward         : PROC  ; (ctx*, in_tokens*, in_count, out_tokens*, out_count*)
EXTERN CPUOps_MatMulVec_F32            : PROC  ; (dst*, mat*, vec*, n)
EXTERN CPUOps_RMSNorm_F32              : PROC  ; (dst*, src*, weight*, n, epsilon)
EXTERN CPUOps_Softmax_F32              : PROC  ; (dst*, src*, n)
EXTERN CPUOps_RoPE_F32                 : PROC  ; (dst*, src*, n, theta)
EXTERN CPUOps_SiLU_F32                 : PROC  ; (dst*, src*, n)
EXTERN TaskDispatcher_Initialize       : PROC  ; (n_workers)
EXTERN TaskDispatcher_SubmitTask       : PROC  ; (task_type, payload_ptr, payload_sz, out_task_id*)
EXTERN TaskDispatcher_GetQueueDepth    : PROC  ; (out_depth*)
EXTERN TaskDispatcher_Shutdown         : PROC  ; ()

; ---- ABI canary ----
EXTERN abi_canary_call0                : PROC  ; (fn_ptr) -> failure_mask

ENDIF