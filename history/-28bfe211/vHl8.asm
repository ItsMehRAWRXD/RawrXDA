;=========================================================================
; system_init_stubs.asm - Stub implementations for orchestrator EXTERNs
;=========================================================================

option casemap:none

include windows.inc

EXTERN console_log:PROC

.data
    szStubInit BYTE "[stubs] initializing system init stubs", 0
    szRunDiag  BYTE "[stubs] run_diagnostics invoked", 0
    szGitAvail BYTE "[stubs] git_is_available invoked", 0
    szSemantic BYTE "[stubs] semantic_init invoked", 0
    szSession  BYTE "[stubs] session_init invoked", 0
    szAgentExt BYTE "[stubs] agentic_extensions_init invoked", 0
    szAIRoute  BYTE "[stubs] ai_routing_init invoked", 0
    szAutoCore BYTE "[stubs] autonomous_core_init invoked", 0
    szIDEFeat  BYTE "[stubs] ide_features_init invoked", 0
    szCloudAPI BYTE "[stubs] cloud_api_init invoked", 0
    szPerfInit BYTE "[stubs] performance_engine_init invoked", 0
    szGGMLInit BYTE "[stubs] ggml_core_init invoked", 0
    szLspInit  BYTE "[stubs] lsp_init invoked", 0

.code

PUBLIC run_diagnostics
run_diagnostics PROC
    lea rcx, szRunDiag
    call console_log
    mov rax, 1
    ret
run_diagnostics ENDP

PUBLIC git_is_available
git_is_available PROC
    lea rcx, szGitAvail
    call console_log
    mov rax, 1
    ret
git_is_available ENDP

PUBLIC semantic_init
semantic_init PROC
    lea rcx, szSemantic
    call console_log
    mov rax, 1
    ret
semantic_init ENDP

PUBLIC session_init
session_init PROC
    lea rcx, szSession
    call console_log
    mov rax, 1
    ret
session_init ENDP

PUBLIC agentic_extensions_init
agentic_extensions_init PROC
    lea rcx, szAgentExt
    call console_log
    mov rax, 1
    ret
agentic_extensions_init ENDP

PUBLIC ai_routing_init
ai_routing_init PROC
    lea rcx, szAIRoute
    call console_log
    mov rax, 1
    ret
ai_routing_init ENDP

PUBLIC autonomous_core_init
autonomous_core_init PROC
    lea rcx, szAutoCore
    call console_log
    mov rax, 1
    ret
autonomous_core_init ENDP

PUBLIC ide_features_init
ide_features_init PROC
    lea rcx, szIDEFeat
    call console_log
    mov rax, 1
    ret
ide_features_init ENDP

PUBLIC cloud_api_init
cloud_api_init PROC
    lea rcx, szCloudAPI
    call console_log
    mov rax, 1
    ret
cloud_api_init ENDP

PUBLIC performance_engine_init
performance_engine_init PROC
    lea rcx, szPerfInit
    call console_log
    mov rax, 1
    ret
performance_engine_init ENDP

PUBLIC ggml_core_init
ggml_core_init PROC
    lea rcx, szGGMLInit
    call console_log
    mov rax, 1
    ret
ggml_core_init ENDP

PUBLIC lsp_init
lsp_init PROC
    lea rcx, szLspInit
    call console_log
    mov rax, 1
    ret
lsp_init ENDP

END