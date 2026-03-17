; ============================================================================
; LSP_CLIENT.INC - Language Server Protocol Header
; ============================================================================

EXTERN lsp_client_init:PROC
EXTERN lsp_client_shutdown:PROC
EXTERN lsp_initialize_workspace:PROC
EXTERN lsp_did_open:PROC
EXTERN lsp_did_change:PROC
EXTERN lsp_did_save:PROC
EXTERN lsp_completion:PROC
EXTERN lsp_hover:PROC
EXTERN lsp_diagnostics:PROC
EXTERN lsp_document_symbols:PROC
EXTERN lsp_goto_definition:PROC
EXTERN lsp_references:PROC

LSP_COMPLETION_ITEM STRUCT
    label qword
    kind qword
    detail qword
    documentation qword
    start_line dword
    start_col dword
    end_line dword
    end_col dword
LSP_COMPLETION_ITEM ENDS

LSP_DIAGNOSTIC STRUCT
    message qword
    severity dword
    line dword
    column dword
    source qword
LSP_DIAGNOSTIC ENDS
