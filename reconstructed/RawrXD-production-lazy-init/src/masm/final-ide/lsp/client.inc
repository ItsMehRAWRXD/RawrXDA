; ============================================================================
; LSP_CLIENT.INC
; ============================================================================
IFNDEF @masm_lsp_client_init
EXTERN lsp_client_init:PROC
ENDIF
IFNDEF @masm_lsp_client_shutdown
EXTERN lsp_client_shutdown:PROC
ENDIF
IFNDEF @masm_lsp_initialize_workspace
EXTERN lsp_initialize_workspace:PROC
ENDIF
IFNDEF @masm_lsp_did_open
EXTERN lsp_did_open:PROC
ENDIF
IFNDEF @masm_lsp_did_change
EXTERN lsp_did_change:PROC
ENDIF
IFNDEF @masm_lsp_did_save
EXTERN lsp_did_save:PROC
ENDIF
IFNDEF @masm_lsp_completion
EXTERN lsp_completion:PROC
ENDIF
IFNDEF @masm_lsp_hover
EXTERN lsp_hover:PROC
ENDIF
IFNDEF @masm_lsp_diagnostics
EXTERN lsp_diagnostics:PROC
ENDIF
IFNDEF @masm_lsp_document_symbols
EXTERN lsp_document_symbols:PROC
ENDIF
IFNDEF @masm_lsp_goto_definition
EXTERN lsp_goto_definition:PROC
ENDIF
IFNDEF @masm_lsp_references
EXTERN lsp_references:PROC
ENDIF
