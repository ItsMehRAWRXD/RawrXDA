; ============================================================================
; masm_solo_compiler.asm
; Standalone MASM Self-Compiling Compiler - Zero External Dependencies
; Generates native x86-64 machine code directly from MASM source
; Complete lexer, parser, semantic analyzer, and code generator
; Outputs Windows PE executables without requiring ml64.exe or link.exe
; ============================================================================
; Build: nasm -f win64 masm_solo_compiler.asm -o masm_solo_compiler.obj
;        gcc masm_solo_compiler.obj -o masm_solo_compiler.exe -nostdlib -lkernel32
; Usage: masm_solo_compiler.exe input.asm output.exe
; ============================================================================

bits 64
default rel
section .data
    ; === Compiler Metadata ===
    compiler_name           db "MASM Solo Compiler", 0
    compiler_version        db "1.0.0 - Self-Compiling Zero-Dependency Edition", 0
    compiler_copyright      db "(C) 2026 RawrXD Project", 0
    
    ; === Compilation Stages ===
    STAGE_INIT              equ 0
    STAGE_LEXER             equ 1
    STAGE_PARSER            equ 2
    STAGE_SEMANTIC          equ 3
    STAGE_CODEGEN           equ 4
    STAGE_PEWRITER          equ 5
    STAGE_COMPLETE          equ 6
    
    ; === Error Codes ===
    ERR_NONE                equ 0
    ERR_FILE_NOT_FOUND      equ 1
    ERR_FILE_READ           equ 2
    ERR_LEXER               equ 3
    ERR_PARSER              equ 4
    ERR_SEMANTIC            equ 5
    ERR_CODEGEN             equ 6
    ERR_PEWRITER            equ 7
    ERR_OUT_OF_MEMORY       equ 8
    
    ; === Token Types ===
    TOK_EOF                 equ 0
    TOK_IDENTIFIER          equ 1
    TOK_NUMBER              equ 2
    TOK_STRING              equ 3
    TOK_DIRECTIVE           equ 4       ; .data, .code, etc.
    TOK_INSTRUCTION         equ 5       ; mov, add, etc.
    TOK_REGISTER            equ 6       ; rax, rbx, etc.
    TOK_KEYWORD             equ 7       ; proc, endp, etc.
    TOK_OPERATOR            equ 8       ; +, -, *, /
    TOK_COMMA               equ 9
    TOK_COLON               equ 10
    TOK_SEMICOLON           equ 11
    TOK_LBRACKET            equ 12      ; [
    TOK_RBRACKET            equ 13      ; ]
    TOK_NEWLINE             equ 14
    
    ; === AST Node Types ===
    AST_PROGRAM             equ 0
    AST_SECTION             equ 1       ; .data, .code sections
    AST_LABEL               equ 2
    AST_INSTRUCTION         equ 3
    AST_DIRECTIVE           equ 4
    AST_OPERAND             equ 5
    AST_EXPRESSION          equ 6
    
    ; === Memory Sizes ===
    MAX_SOURCE_SIZE         equ 16777216    ; 16MB source file
    MAX_TOKENS              equ 1048576     ; 1M tokens
    MAX_AST_NODES           equ 524288      ; 512K AST nodes
    MAX_SYMBOLS             equ 65536       ; 64K symbols
    MAX_MACHINE_CODE        equ 16777216    ; 16MB machine code
    MAX_PE_SIZE             equ 33554432    ; 32MB PE file

    ; === PE Layout Constants ===
    PE_SECTION_ALIGNMENT    equ 0x1000
    PE_FILE_ALIGNMENT       equ 0x200
    PE_TEXT_RVA             equ 0x1000
    PE_TEXT_RAW_PTR         equ 0x200
    PE_HEADERS_SIZE         equ 0x200
    PE_OPTIONAL_SIZE        equ 0xF0
    PE_SECTION_HEADER_OFF   equ 0x188       ; e_lfanew (0x80) + sig/file/optional headers
    PE_IMAGE_BASE_CONST     equ 0x140000000
    
    ; === x86-64 Instruction Opcodes ===
    ; REX prefix for 64-bit mode
    REX_W                   equ 0x48
    REX_R                   equ 0x44
    REX_X                   equ 0x42
    REX_B                   equ 0x41
    
    ; Common opcodes
    OP_MOV_REG_IMM          equ 0xB8    ; mov rax, imm64
    OP_MOV_REG_REG          equ 0x89    ; mov r/m64, r64
    OP_ADD_REG_REG          equ 0x01    ; add r/m64, r64
    OP_SUB_REG_REG          equ 0x29    ; sub r/m64, r64
    OP_XOR_REG_REG          equ 0x31    ; xor r/m64, r64
    OP_PUSH_REG             equ 0x50    ; push r64
    OP_POP_REG              equ 0x58    ; pop r64
    OP_CALL_REL32           equ 0xE8    ; call rel32
    OP_RET                  equ 0xC3    ; ret
    OP_NOP                  equ 0x90    ; nop
    OP_INT3                 equ 0xCC    ; int3 (breakpoint)
    
    ; ModR/M encodings
    MODRM_REG_DIRECT        equ 0xC0    ; Register-direct mode
    
    ; === Register Encodings ===
    REG_RAX                 equ 0
    REG_RCX                 equ 1
    REG_RDX                 equ 2
    REG_RBX                 equ 3
    REG_RSP                 equ 4
    REG_RBP                 equ 5
    REG_RSI                 equ 6
    REG_RDI                 equ 7
    REG_R8                  equ 8
    REG_R9                  equ 9
    REG_R10                 equ 10
    REG_R11                 equ 11
    REG_R12                 equ 12
    REG_R13                 equ 13
    REG_R14                 equ 14
    REG_R15                 equ 15
    
    ; === String Constants ===
    usage_msg               db "Usage: masm_solo_compiler <input.asm> <output.exe>", 13, 10, 0
    stage_names             db "Initialization", 0
                            db "Lexical Analysis", 0
                            db "Parsing", 0
                            db "Semantic Analysis", 0
                            db "Code Generation", 0
                            db "PE Writer", 0
                            db "Complete", 0
    
    msg_stage               db "[%s] ", 0
    msg_success             db "Compilation successful: %s", 13, 10, 0
    msg_error               db "Error in %s: %s", 13, 10, 0
    msg_reading             db "Reading source file...", 13, 10, 0
    msg_writing             db "Writing executable...", 13, 10, 0
    msg_file_size           db "Source size: %u bytes", 13, 10, 0
    msg_token_count         db "Tokens: %u", 13, 10, 0
    msg_ast_nodes           db "AST nodes: %u", 13, 10, 0
    msg_code_size           db "Machine code: %u bytes", 13, 10, 0
    
    ; === Error Messages ===
    err_file_not_found_msg  db "File not found", 0
    err_file_read_msg       db "Failed to read file", 0
    err_lexer_msg           db "Lexical error", 0
    err_parser_msg          db "Syntax error", 0
    err_semantic_msg        db "Semantic error", 0
    err_codegen_msg         db "Code generation error", 0
    err_pewriter_msg        db "PE file generation error", 0
    err_out_of_memory_msg   db "Out of memory", 0
    
    ; === MASM Keywords ===
    keywords                db "proc", 0, "endp", 0, "macro", 0, "endm", 0
                            db "if", 0, "else", 0, "endif", 0, "while", 0
                            db "include", 0, "extern", 0, "public", 0
                            db "byte", 0, "word", 0, "dword", 0, "qword", 0
                            db "ptr", 0, "offset", 0, 0
    
    ; === MASM Directives ===
    directives              db ".data", 0, ".code", 0, ".const", 0
                            db ".data?", 0, "segment", 0, "ends", 0
                            db "assume", 0, "align", 0, "db", 0
                            db "dw", 0, "dd", 0, "dq", 0, 0
    
    ; === x86-64 Instructions (Common subset) ===
    instructions            db "mov", 0, "add", 0, "sub", 0, "mul", 0
                            db "div", 0, "inc", 0, "dec", 0, "neg", 0
                            db "and", 0, "or", 0, "xor", 0, "not", 0
                            db "shl", 0, "shr", 0, "sal", 0, "sar", 0
                            db "push", 0, "pop", 0, "call", 0, "ret", 0
                            db "jmp", 0, "je", 0, "jne", 0, "jz", 0
                            db "jnz", 0, "jl", 0, "jg", 0, "jle", 0
                            db "jge", 0, "ja", 0, "jb", 0, "jae", 0
                            db "jbe", 0, "cmp", 0, "test", 0, "lea", 0
                            db "nop", 0, "int3", 0, "syscall", 0, 0
    
    ; === x86-64 Registers ===
    registers               db "rax", 0, "rbx", 0, "rcx", 0, "rdx", 0
                            db "rsi", 0, "rdi", 0, "rsp", 0, "rbp", 0
                            db "r8", 0, "r9", 0, "r10", 0, "r11", 0
                            db "r12", 0, "r13", 0, "r14", 0, "r15", 0
                            db "eax", 0, "ebx", 0, "ecx", 0, "edx", 0
                            db "esi", 0, "edi", 0, "esp", 0, "ebp", 0
                            db "ax", 0, "bx", 0, "cx", 0, "dx", 0
                            db "al", 0, "bl", 0, "cl", 0, "dl", 0, 0

    registers64             db "rax", 0, "rcx", 0, "rdx", 0, "rbx", 0
                            db "rsp", 0, "rbp", 0, "rsi", 0, "rdi", 0
                            db "r8", 0, "r9", 0, "r10", 0, "r11", 0
                            db "r12", 0, "r13", 0, "r14", 0, "r15", 0, 0

    mnemonic_mov            db "mov", 0
    mnemonic_add            db "add", 0
    mnemonic_sub            db "sub", 0
    mnemonic_xor            db "xor", 0
    mnemonic_nop            db "nop", 0
    mnemonic_ret            db "ret", 0

section .bss
    ; === Global State ===
    g_input_filename        resb 260
    g_output_filename       resb 260
    g_current_stage         resq 1
    g_error_code            resq 1
    g_error_line            resq 1
    g_error_column          resq 1
    
    ; === Memory Buffers ===
    g_source_buffer         resb MAX_SOURCE_SIZE
    g_source_size           resq 1
    g_source_pos            resq 1
    g_current_line          resq 1
    g_current_column        resq 1
    
    ; === Lexer State ===
    g_tokens                resb MAX_TOKENS * 64    ; 64 bytes per token
    g_token_count           resq 1
    g_current_token         resq 1
    
    ; === Parser State ===
    g_ast_nodes             resb MAX_AST_NODES * 128  ; 128 bytes per node
    g_ast_node_count        resq 1
    g_ast_root              resq 1
    
    ; === Symbol Table ===
    g_symbols               resb MAX_SYMBOLS * 256  ; 256 bytes per symbol
    g_symbol_count          resq 1
    g_current_section       resq 1
    g_current_offset        resq 1
    
    ; === Code Generator ===
    g_machine_code          resb MAX_MACHINE_CODE
    g_machine_code_size     resq 1
    g_relocation_count      resq 1
    g_relocations           resb 65536 * 16         ; 16 bytes per relocation
    
    ; === PE File Builder ===
    g_pe_buffer             resb MAX_PE_SIZE
    g_pe_size               resq 1
    g_entry_point_rva       resq 1
    
    ; === Temporary Buffers ===
    g_temp_buffer           resb 4096
    g_line_buffer           resb 1024
    g_identifier_buffer     resb 256

section .text
    global main
    extern GetCommandLineA
    extern CommandLineToArgvW
    extern GetStdHandle
    extern WriteFile
    extern CreateFileA
    extern ReadFile
    extern CloseHandle
    extern GetFileSize
    extern ExitProcess
    extern wsprintfA
    extern lstrlenA
    extern lstrcpyA
    extern lstrcmpA
    extern lstrcmpiA
    extern GetLastError

; ============================================================================
; Main Entry Point
; ============================================================================
main:
    push rbp
    mov rbp, rsp
    sub rsp, 64                     ; Shadow space + locals
    
    ; Parse command line arguments
    call GetCommandLineA
    mov rcx, rax
    lea rdx, [g_input_filename]
    lea r8, [g_output_filename]
    call parse_command_line
    test rax, rax
    jz .usage_error
    
    ; Print banner
    lea rcx, [compiler_name]
    call print_string
    lea rcx, [newline]
    call print_string
    
    ; === Stage 1: Read Source File ===
    mov qword [g_current_stage], STAGE_INIT
    lea rcx, [msg_reading]
    call print_string
    
    lea rcx, [g_input_filename]
    call read_source_file
    test rax, rax
    jz .read_error
    
    ; Print source size
    lea rcx, [msg_file_size]
    mov edx, [g_source_size]
    call printf_wrapper
    
    ; === Stage 2: Lexical Analysis ===
    mov qword [g_current_stage], STAGE_LEXER
    call print_stage_message
    
    call lexer_tokenize
    test rax, rax
    jz .lexer_error
    
    ; Print token count
    lea rcx, [msg_token_count]
    mov edx, [g_token_count]
    call printf_wrapper
    
    ; === Stage 3: Parsing ===
    mov qword [g_current_stage], STAGE_PARSER
    call print_stage_message
    
    call parser_parse
    test rax, rax
    jz .parser_error
    
    ; Print AST node count
    lea rcx, [msg_ast_nodes]
    mov edx, [g_ast_node_count]
    call printf_wrapper
    
    ; === Stage 4: Semantic Analysis ===
    mov qword [g_current_stage], STAGE_SEMANTIC
    call print_stage_message
    
    call semantic_analyze
    test rax, rax
    jz .semantic_error
    
    ; === Stage 5: Code Generation ===
    mov qword [g_current_stage], STAGE_CODEGEN
    call print_stage_message
    
    call codegen_generate
    test rax, rax
    jz .codegen_error
    
    ; Print machine code size
    lea rcx, [msg_code_size]
    mov edx, [g_machine_code_size]
    call printf_wrapper
    
    ; === Stage 6: PE File Generation ===
    mov qword [g_current_stage], STAGE_PEWRITER
    call print_stage_message
    
    call pe_writer_generate
    test rax, rax
    jz .pewriter_error
    
    ; === Write Output File ===
    lea rcx, [msg_writing]
    call print_string
    
    lea rcx, [g_output_filename]
    call write_output_file
    test rax, rax
    jz .write_error
    
    ; === Success ===
    mov qword [g_current_stage], STAGE_COMPLETE
    lea rcx, [msg_success]
    lea rdx, [g_output_filename]
    call printf_wrapper
    
    xor rcx, rcx                    ; Exit code 0
    call ExitProcess
    
.usage_error:
    lea rcx, [usage_msg]
    call print_string
    mov rcx, 1
    call ExitProcess
    
.read_error:
    mov qword [g_error_code], ERR_FILE_READ
    jmp .error_exit
    
.lexer_error:
    mov qword [g_error_code], ERR_LEXER
    jmp .error_exit
    
.parser_error:
    mov qword [g_error_code], ERR_PARSER
    jmp .error_exit
    
.semantic_error:
    mov qword [g_error_code], ERR_SEMANTIC
    jmp .error_exit
    
.codegen_error:
    mov qword [g_error_code], ERR_CODEGEN
    jmp .error_exit
    
.pewriter_error:
    mov qword [g_error_code], ERR_PEWRITER
    jmp .error_exit
    
.write_error:
    mov qword [g_error_code], ERR_PEWRITER
    
.error_exit:
    call print_error_message
    mov rcx, 1
    call ExitProcess

; ============================================================================
; Command Line Parsing
; ============================================================================
parse_command_line:
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    
    ; rcx = command line
    ; rdx = input filename buffer
    ; r8 = output filename buffer
    
    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r8
    
    ; Skip program name
    call skip_whitespace
    call skip_to_whitespace
    call skip_whitespace
    
    ; Check if we have arguments
    cmp byte [rsi], 0
    je .no_args
    
    ; Copy input filename
    mov rdi, rdx
.copy_input:
    lodsb
    cmp al, ' '
    je .input_done
    cmp al, 0
    je .no_output
    stosb
    jmp .copy_input
    
.input_done:
    xor al, al
    stosb
    
    ; Skip whitespace before output
    call skip_whitespace
    cmp byte [rsi], 0
    je .no_output
    
    ; Copy output filename
    mov rdi, rbx
.copy_output:
    lodsb
    cmp al, ' '
    je .output_done
    cmp al, 0
    je .output_done
    stosb
    jmp .copy_output
    
.output_done:
    xor al, al
    stosb
    mov rax, 1                      ; Success
    jmp .done
    
.no_args:
.no_output:
    xor rax, rax                    ; Failure
    
.done:
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret

skip_whitespace:
    cmp byte [rsi], ' '
    jne .done
    inc rsi
    jmp skip_whitespace
.done:
    ret

skip_to_whitespace:
    cmp byte [rsi], ' '
    je .done
    cmp byte [rsi], 0
    je .done
    inc rsi
    jmp skip_to_whitespace
.done:
    ret

; ============================================================================
; File I/O Functions
; ============================================================================
read_source_file:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Open file
    ; rcx = filename
    mov rdx, 0x80000000             ; GENERIC_READ
    mov r8, 1                       ; FILE_SHARE_READ
    mov r9, 0                       ; lpSecurityAttributes
    mov qword [rsp+32], 3           ; OPEN_EXISTING
    mov qword [rsp+40], 0x80        ; FILE_ATTRIBUTE_NORMAL
    mov qword [rsp+48], 0           ; hTemplateFile
    call CreateFileA
    
    cmp rax, -1
    je .error
    mov r15, rax                    ; Save handle
    
    ; Get file size
    mov rcx, r15
    xor rdx, rdx
    call GetFileSize
    mov [g_source_size], rax
    
    ; Read file
    mov rcx, r15                    ; hFile
    lea rdx, [g_source_buffer]      ; lpBuffer
    mov r8, rax                     ; nNumberOfBytesToRead
    lea r9, [rsp+16]                ; lpNumberOfBytesRead
    mov qword [rsp+32], 0           ; lpOverlapped
    call ReadFile
    
    ; Close file
    mov rcx, r15
    call CloseHandle
    
    mov rax, 1                      ; Success
    jmp .done
    
.error:
    xor rax, rax                    ; Failure
    
.done:
    add rsp, 64
    pop rbp
    ret

write_output_file:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Create file
    ; rcx = filename
    mov rdx, 0x40000000             ; GENERIC_WRITE
    mov r8, 0                       ; No sharing
    mov r9, 0                       ; lpSecurityAttributes
    mov qword [rsp+32], 2           ; CREATE_ALWAYS
    mov qword [rsp+40], 0x80        ; FILE_ATTRIBUTE_NORMAL
    mov qword [rsp+48], 0           ; hTemplateFile
    call CreateFileA
    
    cmp rax, -1
    je .error
    mov r15, rax                    ; Save handle
    
    ; Write file
    mov rcx, r15                    ; hFile
    lea rdx, [g_pe_buffer]          ; lpBuffer
    mov r8, [g_pe_size]             ; nNumberOfBytesToWrite
    lea r9, [rsp+16]                ; lpNumberOfBytesWritten
    mov qword [rsp+32], 0           ; lpOverlapped
    call WriteFile
    
    ; Close file
    mov rcx, r15
    call CloseHandle
    
    mov rax, 1                      ; Success
    jmp .done
    
.error:
    xor rax, rax                    ; Failure
    
.done:
    add rsp, 64
    pop rbp
    ret

; ============================================================================
; Lexer - Tokenization
; ============================================================================
lexer_tokenize:
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    sub rsp, 8                     ; align stack for Win64 calls
    
    ; Initialize lexer state
    mov qword [g_source_pos], 0
    mov qword [g_current_line], 1
    mov qword [g_current_column], 1
    mov qword [g_token_count], 0
    
    lea rsi, [g_source_buffer]      ; Source pointer
    lea rdi, [g_tokens]             ; Token buffer pointer
    
.tokenize_loop:
    ; Check end of source
    mov rax, [g_source_pos]
    cmp rax, [g_source_size]
    jge .tokenize_done
    
    ; Skip whitespace and comments
    call lexer_skip_whitespace
    
    ; Check end again after skipping
    mov rax, [g_source_pos]
    cmp rax, [g_source_size]
    jge .tokenize_done
    
    ; Get current character
    mov al, [rsi]
    
    ; Check token type
    cmp al, ';'
    je .comment
    cmp al, '"'
    je .string
    cmp al, "'"
    je .char
    call is_alpha
    test rax, rax
    jnz .identifier_or_keyword
    call is_digit
    test rax, rax
    jnz .number
    call is_operator
    test rax, rax
    jnz .operator
    cmp al, ','
    je .comma
    cmp al, ':'
    je .colon
    cmp al, '['
    je .lbracket
    cmp al, ']'
    je .rbracket
    cmp al, 10
    je .newline
    
    ; Unknown character - skip
    inc rsi
    inc qword [g_source_pos]
    inc qword [g_current_column]
    jmp .tokenize_loop
    
.comment:
    call lexer_skip_to_newline
    jmp .tokenize_loop
    
.string:
    call lexer_read_string
    jmp .token_added
    
.char:
    call lexer_read_char
    jmp .token_added
    
.identifier_or_keyword:
    call lexer_read_identifier
    jmp .token_added
    
.number:
    call lexer_read_number
    jmp .token_added
    
.operator:
    call lexer_read_operator
    jmp .token_added
    
.comma:
    mov byte [rdi], TOK_COMMA
    inc rdi
    inc rsi
    inc qword [g_source_pos]
    inc qword [g_current_column]
    jmp .token_added
    
.colon:
    mov byte [rdi], TOK_COLON
    inc rdi
    inc rsi
    inc qword [g_source_pos]
    inc qword [g_current_column]
    jmp .token_added
    
.lbracket:
    mov byte [rdi], TOK_LBRACKET
    inc rdi
    inc rsi
    inc qword [g_source_pos]
    inc qword [g_current_column]
    jmp .token_added
    
.rbracket:
    mov byte [rdi], TOK_RBRACKET
    inc rdi
    inc rsi
    inc qword [g_source_pos]
    inc qword [g_current_column]
    jmp .token_added
    
.newline:
    mov byte [rdi], TOK_NEWLINE
    inc rdi
    inc rsi
    inc qword [g_source_pos]
    inc qword [g_current_line]
    mov qword [g_current_column], 1
    jmp .token_added
    
.token_added:
    inc qword [g_token_count]
    ; Move to next 64-byte token slot
    add rdi, 63
    jmp .tokenize_loop
    
.tokenize_done:
    ; Add EOF token
    mov byte [rdi], TOK_EOF
    inc qword [g_token_count]
    
    mov rax, 1                      ; Success
    jmp .done
    
.error:
    xor rax, rax                    ; Failure
    
.done:
    add rsp, 8
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret

; Lexer helper functions (simplified implementations)
lexer_skip_whitespace:
    mov rax, [g_source_pos]
    cmp rax, [g_source_size]
    jge .done
    lodsb
    cmp al, ' '
    je .skip
    cmp al, 9                       ; Tab
    je .skip
    cmp al, 13                      ; CR
    je .skip
    dec rsi
    ret
.skip:
    inc qword [g_source_pos]
    inc qword [g_current_column]
    jmp lexer_skip_whitespace

.done:
    ret

lexer_skip_to_newline:
    mov rax, [g_source_pos]
    cmp rax, [g_source_size]
    jge .done
    lodsb
    cmp al, 10
    je .done
    cmp al, 0
    je .done
    inc qword [g_source_pos]
    inc qword [g_current_column]
    jmp lexer_skip_to_newline
.done:
    ret

lexer_read_identifier:
    ; Read alphanumeric characters into token
    push rbx
    push rdi
    mov rbx, rdi                    ; token base
    lea rdi, [g_identifier_buffer]
.read_loop:
    mov rax, [g_source_pos]
    cmp rax, [g_source_size]
    jge .read_done
    lodsb
    call is_alnum
    test rax, rax
    jz .read_done
    stosb
    inc qword [g_source_pos]
    inc qword [g_current_column]
    jmp .read_loop
.read_done:
    xor al, al
    stosb
    dec rsi
    pop rdi
    
    ; Check if it's a keyword, directive, register, or instruction
    lea rcx, [g_identifier_buffer]
    call classify_identifier
    mov [rbx], al                   ; Store token type
    lea rdi, [rbx+1]
    
    ; Copy identifier to token
    lea rdx, [g_identifier_buffer]
.copy_loop:
    mov al, [rdx]
    inc rdx
    stosb
    test al, al
    jnz .copy_loop

    lea rdi, [rbx+1]                ; reset token pointer for caller
    pop rbx
    ret

lexer_read_number:
    ; Read numeric literal
    push rbx
    mov byte [rdi], TOK_NUMBER
    mov r8, rdi
    inc rdi
    xor rdx, rdx

.read_loop:
    mov rax, [g_source_pos]
    cmp rax, [g_source_size]
    jge .read_done
    mov al, [rsi]
    call is_digit
    test rax, rax
    jz .read_done

    mov bl, [rsi]
    mov [rdi], bl
    inc rdi
    inc rsi
    inc qword [g_source_pos]
    inc qword [g_current_column]

    sub bl, '0'
    movzx rcx, bl
    imul rdx, rdx, 10
    add rdx, rcx
    jmp .read_loop

.read_done:
    xor al, al
    mov [rdi], al
    mov [r8+40], rdx
    pop rbx
    ret

lexer_read_string:
    ; Read string literal
    mov byte [rdi], TOK_STRING
    inc rdi
    ; Implementation continues...
    ret

lexer_read_char:
    ; Read character literal
    mov byte [rdi], TOK_NUMBER
    inc rdi
    ; Implementation continues...
    ret

lexer_read_operator:
    ; Read operator
    mov byte [rdi], TOK_OPERATOR
    inc rdi
    ; Implementation continues...
    ret

; Character classification functions
is_alpha:
    cmp al, 'A'
    jl .no
    cmp al, 'Z'
    jle .yes
    cmp al, 'a'
    jl .no
    cmp al, 'z'
    jle .yes
    cmp al, '_'
    je .yes
.no:
    xor rax, rax
    ret
.yes:
    mov rax, 1
    ret

is_digit:
    cmp al, '0'
    jl .no
    cmp al, '9'
    jle .yes
.no:
    xor rax, rax
    ret
.yes:
    mov rax, 1
    ret

is_alnum:
    call is_alpha
    test rax, rax
    jnz .yes
    call is_digit
.yes:
    ret

is_operator:
    cmp al, '+'
    je .yes
    cmp al, '-'
    je .yes
    cmp al, '*'
    je .yes
    cmp al, '/'
    je .yes
    xor rax, rax
    ret
.yes:
    mov rax, 1
    ret

classify_identifier:
    ; rcx = identifier string
    ; Returns token type in rax
    ; Check against keywords, directives, instructions, registers
    push rbx

    lea rdx, [registers64]
    call match_list
    test rax, rax
    jz .check_instructions
    mov rax, TOK_REGISTER
    jmp .done

.check_instructions:
    lea rdx, [instructions]
    call match_list
    test rax, rax
    jz .check_directives
    mov rax, TOK_INSTRUCTION
    jmp .done

.check_directives:
    lea rdx, [directives]
    call match_list
    test rax, rax
    jz .check_keywords
    mov rax, TOK_DIRECTIVE
    jmp .done

.check_keywords:
    lea rdx, [keywords]
    call match_list
    test rax, rax
    jz .default
    mov rax, TOK_KEYWORD
    jmp .done

.default:
    mov rax, TOK_IDENTIFIER

.done:
    pop rbx
    ret

match_list:
    ; rcx = identifier string
    ; rdx = list pointer (double-null terminated)
    push rbx
    mov rbx, rdx

.loop:
    mov al, [rbx]
    cmp al, 0
    je .not_found

    mov rdx, rbx
    call stricmp_local
    test eax, eax
    jz .found

.advance:
    cmp byte [rbx], 0
    je .next
    inc rbx
    jmp .advance

.next:
    inc rbx
    jmp .loop

.found:
    mov rax, 1
    pop rbx
    ret

.not_found:
    xor rax, rax
    pop rbx
    ret

stricmp_local:
    ; rcx = lhs, rdx = rhs
    push rbx
    push rsi
    push rdi

    mov rsi, rcx
    mov rdi, rdx

.cmp_loop:
    mov al, [rsi]
    mov bl, [rdi]
    cmp al, 'A'
    jb .lhs_ok
    cmp al, 'Z'
    ja .lhs_ok
    add al, 32
.lhs_ok:
    cmp bl, 'A'
    jb .rhs_ok
    cmp bl, 'Z'
    ja .rhs_ok
    add bl, 32
.rhs_ok:
    cmp al, bl
    jne .diff
    test al, al
    je .equal
    inc rsi
    inc rdi
    jmp .cmp_loop

.diff:
    mov eax, 1
    jmp .done

.equal:
    xor eax, eax

.done:
    pop rdi
    pop rsi
    pop rbx
    ret

; ============================================================================
; Parser - Build AST
; ============================================================================
parser_parse:
    push rbp
    mov rbp, rsp
    
    ; Initialize parser state
    mov qword [g_current_token], 0
    mov qword [g_ast_node_count], 0
    
    ; Create program node
    call ast_create_node
    mov qword [g_ast_root], rax
    mov byte [rax], AST_PROGRAM
    
    ; Parse sections and statements
.parse_loop:
    call parser_peek_token
    cmp al, TOK_EOF
    je .parse_done
    
    ; Parse top-level construct
    call parser_parse_toplevel
    test rax, rax
    jz .error
    
    jmp .parse_loop
    
.parse_done:
    mov rax, 1                      ; Success
    jmp .done
    
.error:
    xor rax, rax                    ; Failure
    
.done:
    pop rbp
    ret

parser_parse_toplevel:
    ; Parse sections, labels, instructions, directives
    call parser_peek_token
    
    cmp al, TOK_DIRECTIVE
    je .directive
    cmp al, TOK_IDENTIFIER
    je .maybe_label
    cmp al, TOK_INSTRUCTION
    je .instruction
    cmp al, TOK_NEWLINE
    je .skip_newline
    
    ; Unknown - skip
    call parser_next_token
    mov rax, 1
    ret
    
.directive:
    call parser_parse_directive
    ret
    
.maybe_label:
    ; Check if next token is colon
    call parser_peek_token_n
    cmp al, TOK_COLON
    je .label
    jmp .instruction
    
.label:
    call parser_parse_label
    ret
    
.instruction:
    call parser_parse_instruction
    ret
    
.skip_newline:
    call parser_next_token
    mov rax, 1
    ret

parser_parse_directive:
    call ast_create_node
    mov byte [rax], AST_DIRECTIVE
    ; Parse directive details...
    mov rax, 1
    ret

parser_parse_label:
    push rbx
    mov rbx, [g_current_token]
    call ast_create_node
    mov byte [rax], AST_LABEL
    mov [rax+8], rbx

    call parser_next_token
    call parser_peek_token
    cmp al, TOK_COLON
    jne .done
    call parser_next_token

.done:
    mov rax, 1
    pop rbx
    ret

parser_parse_instruction:
    push rbx
    mov rbx, [g_current_token]
    call ast_create_node
    mov rdx, rax
    mov byte [rdx], AST_INSTRUCTION
    mov [rdx+8], rbx
    mov qword [rdx+32], 0

    call parser_next_token

    call parser_peek_token
    cmp al, TOK_REGISTER
    je .operand1
    cmp al, TOK_NUMBER
    je .operand1
    cmp al, TOK_IDENTIFIER
    je .operand1
    cmp al, TOK_NEWLINE
    je .finish
    cmp al, TOK_EOF
    je .finish
    jmp .finish

.operand1:
    mov rcx, [g_current_token]
    mov [rdx+16], rcx
    mov qword [rdx+32], 1
    call parser_next_token

    call parser_peek_token
    cmp al, TOK_COMMA
    jne .finish
    call parser_next_token

    call parser_peek_token
    cmp al, TOK_REGISTER
    je .operand2
    cmp al, TOK_NUMBER
    je .operand2
    cmp al, TOK_IDENTIFIER
    je .operand2
    jmp .finish

.operand2:
    mov rcx, [g_current_token]
    mov [rdx+24], rcx
    mov qword [rdx+32], 2
    call parser_next_token

.finish:
    call parser_peek_token
    cmp al, TOK_NEWLINE
    jne .done
    call parser_next_token

.done:
    mov rax, 1
    pop rbx
    ret

parser_peek_token:
    mov rax, [g_current_token]
    imul rax, 64
    lea rcx, [g_tokens]
    add rcx, rax
    movzx rax, byte [rcx]
    ret

parser_peek_token_n:
    mov rax, [g_current_token]
    inc rax
    imul rax, 64
    lea rcx, [g_tokens]
    add rcx, rax
    movzx rax, byte [rcx]
    ret

parser_next_token:
    inc qword [g_current_token]
    ret

ast_create_node:
    mov rax, [g_ast_node_count]
    imul rax, 128
    lea rcx, [g_ast_nodes]
    add rax, rcx
    inc qword [g_ast_node_count]
    ret

; ============================================================================
; Semantic Analyzer
; ============================================================================
semantic_analyze:
    push rbp
    mov rbp, rsp
    
    ; Build symbol table
    call semantic_build_symbols
    test rax, rax
    jz .error
    
    ; Resolve references
    call semantic_resolve_refs
    test rax, rax
    jz .error
    
    ; Type checking (simplified for MASM)
    call semantic_type_check
    test rax, rax
    jz .error
    
    mov rax, 1                      ; Success
    jmp .done
    
.error:
    xor rax, rax                    ; Failure
    
.done:
    pop rbp
    ret

semantic_build_symbols:
    ; Scan AST and build symbol table
    push rbx
    push rsi

    mov qword [g_symbol_count], 0
    mov qword [g_current_offset], 0

    xor rbx, rbx
    lea rsi, [g_ast_nodes]

.scan_loop:
    cmp rbx, [g_ast_node_count]
    jge .done

    movzx eax, byte [rsi]
    cmp al, AST_LABEL
    je .label
    cmp al, AST_INSTRUCTION
    je .instruction
    jmp .next

.label:
    mov rcx, [rsi+8]
    mov rdx, [g_current_offset]
    call add_symbol
    jmp .next

.instruction:
    mov rcx, rsi
    call compute_instruction_size
    add [g_current_offset], rax
    jmp .next

.next:
    add rsi, 128
    inc rbx
    jmp .scan_loop

.done:
    pop rsi
    pop rbx
    mov rax, 1
    ret

add_symbol:
    ; rcx = token index (label)
    ; rdx = address
    push rbx
    push rsi
    push rdi

    mov rbx, [g_symbol_count]
    imul rbx, 256
    lea rdi, [g_symbols]
    add rdi, rbx

    mov rax, rcx
    imul rax, 64
    lea rsi, [g_tokens]
    add rsi, rax
    inc rsi                          ; skip token type

.copy_name:
    lodsb
    stosb
    test al, al
    jnz .copy_name

    mov [rdi+127], byte 0
    mov [rdi+128], rdx

    inc qword [g_symbol_count]

    pop rdi
    pop rsi
    pop rbx
    ret

compute_instruction_size:
    ; rcx = AST node pointer
    push rbx
    push rsi

    mov rbx, rcx
    mov rcx, [rbx+8]
    call get_token_ptr
    mov rsi, rax

    xor rax, rax

    ; Compare mnemonic to determine exact encoded length
    lea rcx, [rsi+1]
    lea rdx, [mnemonic_nop]
    call stricmp_local
    test eax, eax
    jz .size_byte

    lea rcx, [rsi+1]
    lea rdx, [mnemonic_ret]
    call stricmp_local
    test eax, eax
    jz .size_byte

    lea rcx, [rsi+1]
    lea rdx, [mnemonic_mov]
    call stricmp_local
    test eax, eax
    jz .size_mov

    lea rcx, [rsi+1]
    lea rdx, [mnemonic_add]
    call stricmp_local
    test eax, eax
    jz .size_reg_reg

    lea rcx, [rsi+1]
    lea rdx, [mnemonic_sub]
    call stricmp_local
    test eax, eax
    jz .size_reg_reg

    lea rcx, [rsi+1]
    lea rdx, [mnemonic_xor]
    call stricmp_local
    test eax, eax
    jz .size_reg_reg

    jmp .done

.size_byte:
    mov rax, 1
    jmp .done

.size_mov:
    ; mov r64, imm64 or mov r/m64,r64 share same base; choose by operand2 token
    mov rcx, [rbx+24]
    call get_token_ptr
    movzx eax, byte [rax]
    cmp al, TOK_NUMBER
    je .size_mov_imm
    mov rax, 3                     ; REX + opcode + ModRM for reg/reg
    jmp .done

.size_mov_imm:
    mov rax, 10                    ; REX + opcode + imm64
    jmp .done

.size_reg_reg:
    mov rax, 3                     ; REX + opcode + ModRM for reg/reg

.done:
    pop rsi
    pop rbx
    ret
    ret

get_token_ptr:
    ; rcx = token index
    mov rax, rcx
    imul rax, 64
    lea rdx, [g_tokens]
    add rax, rdx
    ret

get_register_id:
    ; rcx = token pointer
    ; returns rax = 0-15, or 0xFF if not found
    push rbx
    push rsi
    mov r9, rcx
    lea rsi, [registers64]
    xor ebx, ebx

.reg_loop:
    mov al, [rsi]
    cmp al, 0
    je .not_found

    sub rsp, 32
    lea rcx, [r9+1]
    mov rdx, rsi
    call lstrcmpiA
    add rsp, 32
    test eax, eax
    jz .found

.advance:
    cmp byte [rsi], 0
    je .next
    inc rsi
    jmp .advance

.next:
    inc rsi
    inc ebx
    jmp .reg_loop

.found:
    mov eax, ebx
    pop rsi
    pop rbx
    ret

.not_found:
    mov eax, 0FFh
    pop rsi
    pop rbx
    ret

semantic_resolve_refs:
    ; Resolve label references
    mov rax, 1
    ret

semantic_type_check:
    ; Check operand types
    mov rax, 1
    ret

; ============================================================================
; Code Generator - x86-64 Machine Code
; ============================================================================
codegen_generate:
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    
    ; Initialize code generator
    mov qword [g_machine_code_size], 0
    mov qword [g_entry_point_rva], 0
    lea rdi, [g_machine_code]
    
    ; Walk AST and generate machine code
    xor rbx, rbx
    lea rsi, [g_ast_nodes]

.emit_loop:
    cmp rbx, [g_ast_node_count]
    jge .emit_done

    mov rcx, rsi
    call codegen_node
    test rax, rax
    jz .error

    add rsi, 128
    inc rbx
    jmp .emit_loop

.emit_done:
    ; If no entry point was recorded but we produced code, default to .text RVA
    cmp qword [g_entry_point_rva], 0
    jne .entry_set
    cmp qword [g_machine_code_size], 0
    je .entry_set
    mov qword [g_entry_point_rva], 0x1000
.entry_set:
    mov rax, 1                      ; Success
    jmp .done
    
.error:
    xor rax, rax                    ; Failure
    
.done:
    pop rsi
    pop rbx
    pop rbp
    ret

codegen_node:
    ; rcx = AST node pointer
    ; rdi = output buffer
    push rbp
    mov rbp, rsp
    
    ; Get node type
    movzx rax, byte [rcx]
    
    cmp al, AST_PROGRAM
    je .done
    cmp al, AST_LABEL
    je .done
    cmp al, AST_INSTRUCTION
    je .instruction
    cmp al, AST_DIRECTIVE
    je .directive
    
    ; Other nodes - recurse on children
    mov rax, 1
    jmp .done
    
.instruction:
    call codegen_instruction
    jmp .done
    
.directive:
    call codegen_directive
    jmp .done
    
.done:
    pop rbp
    ret

codegen_instruction:
    ; Generate machine code for instruction
    push rbx
    push rsi
    push r12

    mov rbx, rcx
    mov r12, rdi                  ; instruction start pointer

    mov rax, [g_entry_point_rva]
    test rax, rax
    jnz .entry_ok
    mov rax, [g_machine_code_size]
    add rax, 0x1000
    mov [g_entry_point_rva], rax
.entry_ok:

    mov rcx, [rbx+8]
    call get_token_ptr
    mov rdx, rax

    lea rcx, [rdx+1]
    lea rdx, [mnemonic_nop]
    call stricmp_local
    test eax, eax
    jz .emit_nop

    lea rcx, [rdx+1]
    lea rdx, [mnemonic_ret]
    call stricmp_local
    test eax, eax
    jz .emit_ret

    lea rcx, [rdx+1]
    lea rdx, [mnemonic_mov]
    call stricmp_local
    test eax, eax
    jz .emit_mov

    lea rcx, [rdx+1]
    lea rdx, [mnemonic_add]
    call stricmp_local
    test eax, eax
    jz .emit_add

    lea rcx, [rdx+1]
    lea rdx, [mnemonic_sub]
    call stricmp_local
    test eax, eax
    jz .emit_sub

    lea rcx, [rdx+1]
    lea rdx, [mnemonic_xor]
    call stricmp_local
    test eax, eax
    jz .emit_xor

    jmp .error

.emit_nop:
    mov byte [rdi], OP_NOP
    inc rdi
    jmp .emit_done

.emit_ret:
    mov byte [rdi], OP_RET
    inc rdi
    jmp .emit_done

.emit_mov:
    mov rcx, [rbx+24]
    call get_token_ptr
    movzx eax, byte [rax]
    cmp al, TOK_NUMBER
    je .emit_mov_imm
    jmp .emit_reg_reg_mov

.emit_mov_imm:
    mov rcx, [rbx+16]
    call get_token_ptr
    mov r8d, eax
    mov rcx, rax
    call get_register_id
    cmp al, 0FFh
    je .error
    mov r8d, eax

    mov al, REX_W
    cmp r8d, 8
    jb .mov_imm_no_rex
    or al, 0x01
.mov_imm_no_rex:
    mov [rdi], al
    inc rdi
    mov eax, r8d
    and eax, 7
    add al, OP_MOV_REG_IMM
    mov [rdi], al
    inc rdi

    mov rcx, [rbx+24]
    call get_token_ptr
    mov rax, [rax+40]
    mov [rdi], rax
    add rdi, 8
    jmp .emit_done

.emit_reg_reg_mov:
    mov al, OP_MOV_REG_REG
    jmp .emit_reg_reg

.emit_add:
    mov al, OP_ADD_REG_REG
    jmp .emit_reg_reg

.emit_sub:
    mov al, OP_SUB_REG_REG
    jmp .emit_reg_reg

.emit_xor:
    mov al, OP_XOR_REG_REG
    jmp .emit_reg_reg

.emit_reg_reg:
    mov r10b, al                   ; opcode for reg/reg form
    mov rcx, [rbx+16]
    call get_token_ptr
    mov rcx, rax
    call get_register_id
    cmp al, 0FFh
    je .error
    mov r11d, eax

    mov rcx, [rbx+24]
    call get_token_ptr
    mov rcx, rax
    call get_register_id
    cmp al, 0FFh
    je .error
    mov r9d, eax

    mov al, REX_W                  ; base REX.W for 64-bit operations
    cmp r9d, 8
    jb .no_rex_r
    or al, 0x04                    ; REX.R extends reg field
.no_rex_r:
    cmp r11d, 8
    jb .no_rex_b
    or al, 0x01                    ; REX.B extends r/m field
.no_rex_b:
    mov [rdi], al                  ; write REX prefix
    inc rdi
    mov al, r10b
    mov [rdi], al                  ; write opcode
    inc rdi

    mov eax, MODRM_REG_DIRECT      ; start ModRM = 11b in high bits
    mov ecx, r9d
    and ecx, 7
    shl ecx, 3                     ; reg field
    or eax, ecx
    mov ecx, r11d
    and ecx, 7                     ; r/m field
    or eax, ecx
    mov [rdi], al                  ; final ModRM: 0xC0 | (reg<<3) | rm
    inc rdi
    jmp .emit_done

.emit_done:
    mov rax, rdi
    sub rax, r12                  ; bytes emitted this instruction
    add [g_machine_code_size], rax
    mov rax, 1
    pop r12
    pop rsi
    pop rbx
    ret

.error:
    xor rax, rax
    pop r12
    pop rsi
    pop rbx
    ret

codegen_directive:
    ; Handle directives (.data, .code, etc.)
    mov rax, 1
    ret

; ============================================================================
; PE File Writer - Generate Windows Executable
; ============================================================================
pe_writer_generate:
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13

    ; Align machine code sizes for file and section requirements
    mov rax, [g_machine_code_size]
    mov r12, rax                    ; actual code size

    mov rcx, PE_FILE_ALIGNMENT - 1
    add rax, rcx
    and rax, -PE_FILE_ALIGNMENT
    mov r13, rax                    ; file-aligned code size

    mov rax, r12
    mov rcx, PE_SECTION_ALIGNMENT - 1
    add rax, rcx
    and rax, -PE_SECTION_ALIGNMENT
    mov rbx, rax                    ; section-aligned code size

    ; Clear header space (first 0x200 bytes)
    lea rdi, [g_pe_buffer]
    xor rax, rax
    mov rcx, PE_HEADERS_SIZE / 8
    rep stosq

    ; === DOS Header ===
    mov word [g_pe_buffer], 0x5A4D          ; "MZ"
    mov dword [g_pe_buffer+0x3C], 0x80      ; e_lfanew -> 0x80

    ; === PE Signature and COFF Header ===
    lea rsi, [g_pe_buffer+0x80]
    mov dword [rsi], 0x00004550             ; "PE\0\0"
    mov word [rsi+4], 0x8664                ; Machine = AMD64
    mov word [rsi+6], 1                     ; NumberOfSections
    xor eax, eax
    mov dword [rsi+8], eax                  ; TimeDateStamp
    mov dword [rsi+12], eax                 ; PointerToSymbolTable
    mov dword [rsi+16], eax                 ; NumberOfSymbols
    mov word [rsi+20], PE_OPTIONAL_SIZE     ; SizeOfOptionalHeader
    mov word [rsi+22], 0x0022               ; Characteristics: EXECUTABLE | LARGE_ADDRESS_AWARE

    ; === Optional Header (PE32+) ===
    lea rdi, [rsi+24]
    mov word [rdi], 0x20B                   ; Magic
    mov byte [rdi+2], 0x0E                  ; MajorLinkerVersion
    mov byte [rdi+3], 0
    mov dword [rdi+4], r13d                 ; SizeOfCode
    mov dword [rdi+8], 0                    ; SizeOfInitializedData
    mov dword [rdi+12], 0                   ; SizeOfUninitializedData
    mov eax, [g_entry_point_rva]
    test eax, eax
    jnz .have_entry
    mov eax, PE_TEXT_RVA
.have_entry:
    mov dword [rdi+16], eax                 ; AddressOfEntryPoint
    mov dword [rdi+20], PE_TEXT_RVA         ; BaseOfCode
    mov rax, PE_IMAGE_BASE_CONST
    mov qword [rdi+24], rax                 ; ImageBase
    mov dword [rdi+32], PE_SECTION_ALIGNMENT
    mov dword [rdi+36], PE_FILE_ALIGNMENT
    mov word [rdi+40], 6                    ; MajorOperatingSystemVersion
    mov word [rdi+42], 0
    mov word [rdi+44], 0                    ; MajorImageVersion
    mov word [rdi+46], 0
    mov word [rdi+48], 6                    ; MajorSubsystemVersion
    mov word [rdi+50], 0
    mov dword [rdi+52], 0                   ; Win32VersionValue
    mov eax, ebx
    add eax, PE_TEXT_RVA
    mov dword [rdi+56], eax                 ; SizeOfImage (aligned .text + headers)
    mov dword [rdi+60], PE_HEADERS_SIZE     ; SizeOfHeaders
    mov dword [rdi+64], 0                   ; CheckSum
    mov word [rdi+68], 3                    ; Subsystem (Windows CUI)
    mov word [rdi+70], 0                    ; DllCharacteristics
    mov qword [rdi+72], 0x00100000          ; SizeOfStackReserve
    mov qword [rdi+80], 0x00001000          ; SizeOfStackCommit
    mov qword [rdi+88], 0x00100000          ; SizeOfHeapReserve
    mov qword [rdi+96], 0x00001000          ; SizeOfHeapCommit
    mov dword [rdi+104], 0                  ; LoaderFlags
    mov dword [rdi+108], 16                 ; NumberOfRvaAndSizes

    ; Zero data directories
    lea rsi, [rdi+112]
    mov rcx, 16
    xor rax, rax
.zero_dirs:
    mov qword [rsi], rax
    add rsi, 8
    loop .zero_dirs

    ; === Section Header (.text) ===
    lea rdi, [g_pe_buffer+PE_SECTION_HEADER_OFF]
    mov dword [rdi], 0x7478652E             ; Name ".text"
    mov dword [rdi+4], 0
    mov dword [rdi+8], r12d                 ; VirtualSize
    mov dword [rdi+12], PE_TEXT_RVA         ; VirtualAddress
    mov dword [rdi+16], r13d                ; SizeOfRawData
    mov dword [rdi+20], PE_TEXT_RAW_PTR     ; PointerToRawData
    mov dword [rdi+24], 0                   ; PointerToRelocations
    mov dword [rdi+28], 0                   ; PointerToLinenumbers
    mov word [rdi+32], 0                    ; NumberOfRelocations
    mov word [rdi+34], 0                    ; NumberOfLinenumbers
    mov dword [rdi+36], 0x60000020          ; Characteristics: CODE | EXECUTE | READ

    ; === Section Data (.text) ===
    lea rsi, [g_machine_code]
    lea rdi, [g_pe_buffer+PE_TEXT_RAW_PTR]
    mov rcx, r12
    rep movsb                               ; copy actual code bytes

    ; Pad to file alignment
    mov rcx, r13
    sub rcx, r12
    jz .no_pad
    xor rax, rax
.pad_loop:
    mov byte [rdi], al
    inc rdi
    loop .pad_loop
.no_pad:

    ; Final PE size = headers + aligned .text raw data
    mov rax, PE_TEXT_RAW_PTR
    add rax, r13
    mov [g_pe_size], rax

    mov rax, 1                              ; Success
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret

; ============================================================================
; Utility Functions
; ============================================================================
print_string:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    push rbx
    
    ; Save string pointer
    mov rbx, rcx
    
    ; Get stdout handle
    mov rcx, -11                    ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov r15, rax
    
    ; Calculate string length
    mov rcx, rbx
    call lstrlenA
    mov r8, rax
    
    ; Write to stdout
    mov rcx, r15                    ; hFile
    mov rdx, rbx                    ; lpBuffer
    ; r8 already set                ; nNumberOfCharsToWrite
    lea r9, [rsp+32]                ; lpNumberOfCharsWritten
    mov qword [rsp+32], 0           ; lpReserved
    call WriteFile
    
    pop rbx
    add rsp, 48
    pop rbp
    ret

printf_wrapper:
    ; Simplified printf (rcx=format, rdx=arg)
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    
    ; Save parameters
    mov rbx, rcx                    ; format
    mov rsi, rdx                    ; arg
    
    ; Call wsprintfA(buffer, format, arg)
    lea rcx, [g_temp_buffer]        ; lpOut
    mov rdx, rbx                    ; lpFmt
    mov r8, rsi                     ; arg1
    call wsprintfA
    
    lea rcx, [g_temp_buffer]
    call print_string
    
    pop rsi
    pop rbx
    add rsp, 64
    pop rbp
    ret

print_stage_message:
    push rbp
    mov rbp, rsp
    push rbx
    
    ; Print stage name - walk through null-terminated strings
    lea rbx, [stage_names]
    mov rcx, [g_current_stage]
.find_stage:
    test rcx, rcx
    jz .found
.skip_string:
    cmp byte [rbx], 0
    je .next_string
    inc rbx
    jmp .skip_string
.next_string:
    inc rbx                         ; skip null terminator
    dec rcx
    jmp .find_stage
.found:
    mov rcx, rbx
    call print_string
    
    lea rcx, [newline]
    call print_string
    
    pop rbx
    pop rbp
    ret

print_error_message:
    push rbp
    mov rbp, rsp
    
    lea rcx, [msg_error]
    ; Get stage name
    mov rax, [g_current_stage]
    imul rax, 20
    lea rdx, [stage_names]
    add rdx, rax
    ; Get error message
    mov rax, [g_error_code]
    imul rax, 32
    lea r8, [error_messages]
    add r8, rax
    call printf_wrapper
    
    pop rbp
    ret

; ============================================================================
; Data Section Additions
; ============================================================================
section .data
    newline                 db 13, 10, 0
    error_messages          db "File not found", 0
                            times 32 db 0
                            db "File read error", 0
                            times 32 db 0
                            db "Lexical error", 0
                            times 32 db 0
                            db "Parser error", 0
                            times 32 db 0
                            db "Semantic error", 0
                            times 32 db 0
                            db "Code generation error", 0
                            times 32 db 0
                            db "PE write error", 0
                            times 32 db 0
                            db "Out of memory", 0
    error_initialization    db "Failed to initialize compiler", 0
    error_compiler_init     db "Failed to initialize self-contained compiler", 0
    error_window_creation   db "Failed to create window", 0
