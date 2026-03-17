;==========================================================================
; rawrxd_stubs.asm - Missing symbols for RawrXD IDE
;==========================================================================

.code

; String utilities
PUBLIC StringLength
StringLength PROC
    xor eax, eax
    mov rdx, rcx
len_loop:
    cmp byte ptr [rdx], 0
    je len_done
    inc eax
    inc rdx
    jmp len_loop
len_done:
    ret
StringLength ENDP

; IDE component stubs
PUBLIC ide_init_all_components
ide_init_all_components PROC
    mov eax, 1
    ret
ide_init_all_components ENDP

PUBLIC ide_init_file_tree
ide_init_file_tree PROC
    mov eax, 1
    ret
ide_init_file_tree ENDP

PUBLIC ide_editor_open_file
ide_editor_open_file PROC
    mov eax, 1
    ret
ide_editor_open_file ENDP

PUBLIC ide_tabs_create_tab
ide_tabs_create_tab PROC
    mov eax, 1
    ret
ide_tabs_create_tab ENDP

PUBLIC ide_minimap_init
ide_minimap_init PROC
    mov eax, 1
    ret
ide_minimap_init ENDP

PUBLIC ide_palette_init
ide_palette_init PROC
    mov eax, 1
    ret
ide_palette_init ENDP

PUBLIC ide_panes_init
ide_panes_init PROC
    mov eax, 1
    ret
ide_panes_init ENDP

; Unified hotpatch stubs
PUBLIC masm_unified_init
masm_unified_init PROC
    mov eax, 1
    ret
masm_unified_init ENDP

PUBLIC masm_unified_create
masm_unified_create PROC
    mov rax, rcx
    ret
masm_unified_create ENDP

PUBLIC masm_unified_apply
masm_unified_apply PROC
    xor eax, eax
    ret
masm_unified_apply ENDP

PUBLIC masm_unified_apply_memory_patch
masm_unified_apply_memory_patch PROC
    xor eax, eax
    ret
masm_unified_apply_memory_patch ENDP

PUBLIC masm_unified_apply_byte_patch
masm_unified_apply_byte_patch PROC
    xor eax, eax
    ret
masm_unified_apply_byte_patch ENDP

PUBLIC masm_unified_add_server_hotpatch
masm_unified_add_server_hotpatch PROC
    xor eax, eax
    ret
masm_unified_add_server_hotpatch ENDP

PUBLIC masm_unified_manager_create
masm_unified_manager_create PROC
    mov rax, rcx
    ret
masm_unified_manager_create ENDP

PUBLIC masm_unified_process_events
masm_unified_process_events PROC
    xor eax, eax
    ret
masm_unified_process_events ENDP

PUBLIC masm_unified_get_stats
masm_unified_get_stats PROC
    xor rax, rax
    ret
masm_unified_get_stats ENDP

PUBLIC masm_unified_destroy
masm_unified_destroy PROC
    xor eax, eax
    ret
masm_unified_destroy ENDP

; Agentic puppeteer stubs
PUBLIC masm_puppeteer_correct_response
masm_puppeteer_correct_response PROC
    xor rax, rax
    ret
masm_puppeteer_correct_response ENDP

PUBLIC masm_puppeteer_get_stats
masm_puppeteer_get_stats PROC
    xor rax, rax
    ret
masm_puppeteer_get_stats ENDP

; JSON helper function stubs
PUBLIC _append_bool
_append_bool PROC
    xor eax, eax
    ret
_append_bool ENDP

PUBLIC _find_json_key
_find_json_key PROC
    xor rax, rax
    ret
_find_json_key ENDP

PUBLIC _parse_json_bool
_parse_json_bool PROC
    xor eax, eax
    ret
_parse_json_bool ENDP

PUBLIC _parse_json_int
_parse_json_int PROC
    xor eax, eax
    ret
_parse_json_int ENDP

PUBLIC _read_json_from_file
_read_json_from_file PROC
    xor eax, eax
    ret
_read_json_from_file ENDP

PUBLIC _write_json_to_file
_write_json_to_file PROC
    xor eax, eax
    ret
_write_json_to_file ENDP

END
