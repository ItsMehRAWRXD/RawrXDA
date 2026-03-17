; ═══════════════════════════════════════════════════════════════════════════════
; rawrxd_compiler_protos.inc - Compiler Toolchain Cross-Module Declarations
; ═══════════════════════════════════════════════════════════════════════════════
; Resolves undefined CALL targets in the self-hosted compiler pipeline:
;   solo_standalone_compiler.asm, masm_solo_compiler.asm,
;   RawrXD_Compiler_Engine_Complete.asm, language_terraformer.asm
;
; These are C-calling-convention functions across compiler .obj files.
;
; Usage:  INCLUDE rawrxd_compiler_protos.inc
; ═══════════════════════════════════════════════════════════════════════════════

IFNDEF RAWRXD_COMPILER_PROTOS_INC
RAWRXD_COMPILER_PROTOS_INC EQU 1

; ─── Lexer / Tokenizer ────────────────────────────────────────────────────────
EXTERNDEF lexer_tokenize:PROC
EXTERNDEF lexer_read_char:PROC
EXTERNDEF lexer_read_identifier:PROC
EXTERNDEF lexer_read_number:PROC
EXTERNDEF lexer_read_operator:PROC
EXTERNDEF lexer_read_string:PROC
EXTERNDEF lexer_skip_to_newline:PROC
EXTERNDEF lexer_skip_whitespace:PROC
EXTERNDEF tokenize:PROC
EXTERNDEF skip_whitespace:PROC
EXTERNDEF skip_to_whitespace:PROC
EXTERNDEF skip_to_quote:PROC
EXTERNDEF scan_operator:PROC
EXTERNDEF is_alnum:PROC
EXTERNDEF is_alpha:PROC
EXTERNDEF is_digit:PROC
EXTERNDEF is_hex_digit:PROC
EXTERNDEF is_operator:PROC

; ─── Parser ──────────────────────────────────────────────────────────────────
EXTERNDEF parser_parse:PROC
EXTERNDEF parser_next_token:PROC
EXTERNDEF parser_peek_token:PROC
EXTERNDEF parser_peek_token_nnt:PROC
EXTERNDEF parser_parse_directive:PROC
EXTERNDEF parser_parse_instruction:PROC
EXTERNDEF parser_parse_label:PROC
EXTERNDEF parser_parse_toplevel:PROC
EXTERNDEF advance_token:PROC
EXTERNDEF current_token:PROC
EXTERNDEF expect_token:PROC
EXTERNDEF parse_args:PROC
EXTERNDEF parse_arguments:PROC
EXTERNDEF parse_block:PROC
EXTERNDEF parse_cli_mode:PROC
EXTERNDEF parse_command_line:PROC
EXTERNDEF parse_const_decl:PROC
EXTERNDEF parse_declaration:PROC
EXTERNDEF parse_enum_decl:PROC
EXTERNDEF parse_expression:PROC
EXTERNDEF parse_extern_decl:PROC
EXTERNDEF parse_for_statement:PROC
EXTERNDEF parse_format:PROC
EXTERNDEF parse_function_decl:PROC
EXTERNDEF parse_if_statement:PROC
EXTERNDEF parse_import_decl:PROC
EXTERNDEF parse_next_token:PROC
EXTERNDEF parse_parameters:PROC
EXTERNDEF parse_program:PROC
EXTERNDEF parse_return_statement:PROC
EXTERNDEF parse_statement:PROC
EXTERNDEF parse_struct_decl:PROC
EXTERNDEF parse_target:PROC
EXTERNDEF parse_type:PROC
EXTERNDEF parse_variable_decl:PROC
EXTERNDEF parse_while_statement:PROC

; ─── AST (Abstract Syntax Tree) ──────────────────────────────────────────────
EXTERNDEF ast_create_node:PROC
EXTERNDEF create_ast_node:PROC
EXTERNDEF add_child_node:PROC
EXTERNDEF get_first_child:PROC
EXTERNDEF get_next_sibling:PROC
EXTERNDEF set_node_name:PROC
EXTERNDEF set_node_params:PROC
EXTERNDEF set_node_type:PROC

; ─── Semantic Analysis ───────────────────────────────────────────────────────
EXTERNDEF semantic_analysis:PROC
EXTERNDEF semantic_analyze:PROC
EXTERNDEF semantic_build_symbols:PROC
EXTERNDEF semantic_resolve_refs:PROC
EXTERNDEF semantic_type_check:PROC
EXTERNDEF resolve_and_typecheck:PROC
EXTERNDEF collect_declarations:PROC

; ─── Symbol Table ─────────────────────────────────────────────────────────────
EXTERNDEF init_symbol_table:PROC
EXTERNDEF lookup_keyword:PROC
EXTERNDEF lookup_symbol:PROC
EXTERNDEF push_scope:PROC
EXTERNDEF pop_scope:PROC

; ─── Code Generation ─────────────────────────────────────────────────────────
EXTERNDEF codegen_directive:PROC
EXTERNDEF codegen_generate:PROC
EXTERNDEF codegen_node:PROC
EXTERNDEF generate_code:PROC
EXTERNDEF generate_ir:PROC
EXTERNDEF generate_ir_node:PROC
EXTERNDEF optimize_ir:PROC
EXTERNDEF emit_x86_64_instructions:PROC
EXTERNDEF generate_arm64:PROC
EXTERNDEF generate_x86_32:PROC
EXTERNDEF generate_x86_64:PROC
EXTERNDEF update_output_pos:PROC

; ─── Output Generation ───────────────────────────────────────────────────────
EXTERNDEF generate_pe_executable:PROC
EXTERNDEF generate_elf_executable:PROC
EXTERNDEF generate_shared_library:PROC
EXTERNDEF pe_writer_generate:PROC
EXTERNDEF link_executable:PROC
EXTERNDEF write_output_file:PROC

; ─── Compiler Frontend ───────────────────────────────────────────────────────
EXTERNDEF initialize_compiler:PROC
EXTERNDEF cleanup_compiler:PROC
EXTERNDEF compile_file:PROC
EXTERNDEF compile_source:PROC
EXTERNDEF assemble_code:PROC
EXTERNDEF assemble_to_machine_code:PROC
EXTERNDEF read_source_file:PROC

; ─── PE Analysis (compiler-side) ─────────────────────────────────────────────
EXTERNDEF analyze_imports:PROC
EXTERNDEF analyze_pe_headers:PROC
EXTERNDEF analyze_sections:PROC
EXTERNDEF extract_strings:PROC

; ─── Utility ─────────────────────────────────────────────────────────────────
EXTERNDEF copy_string:PROC
EXTERNDEF string_compare:PROC
EXTERNDEF print_compile_error:PROC
EXTERNDEF print_error:PROC
EXTERNDEF print_error_message:PROC
EXTERNDEF print_stage_message:PROC
EXTERNDEF print_string:PROC
EXTERNDEF printf_wrapper:PROC
EXTERNDEF read_file:PROC
EXTERNDEF write_file:PROC

; ─── Identifier Classification ────────────────────────────────────────────────
EXTERNDEF classify_identifieruffer:PROC

ENDIF ; RAWRXD_COMPILER_PROTOS_INC
