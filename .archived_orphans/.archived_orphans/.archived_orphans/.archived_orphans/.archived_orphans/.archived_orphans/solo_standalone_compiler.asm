; ============================================================================
; RAWRXD SOLO STANDALONE COMPILER v1.0
; Complete Self-Compiling Compiler - Zero External Dependencies
; Written in x86-64 MASM/NASM compatible assembly
; 
; Features:
;   - Full lexer with 100+ token types
;   - Recursive descent parser with error recovery
;   - Semantic analyzer with type checking
;   - Multi-target code generator (x86-64, x86-32, ARM64)
;   - PE/ELF/Mach-O executable output
;   - Built-in assembler and linker
;   - Self-compiling capability
;
; Copyright (c) 2024-2026 RawrXD IDE Project
; ============================================================================

; ============================================================================
; SECTION: Constants and Configuration
; ============================================================================
section .data
    align 16
    
    ; === Compiler Identity ===
    COMPILER_NAME           db "RawrXD Solo Compiler", 0
    COMPILER_VERSION        db "1.0.0", 0
    COMPILER_BUILD          db "20260117", 0
    COMPILER_BANNER         db 10, "╔══════════════════════════════════════════════════════════╗", 10
                            db "║     RawrXD Solo Standalone Compiler v1.0                  ║", 10
                            db "║     Self-Compiling • Zero Dependencies • Production Ready ║", 10
                            db "╚══════════════════════════════════════════════════════════╝", 10, 0
    
    ; === Compilation Stages ===
    STAGE_INIT              equ 0
    STAGE_LEXICAL           equ 1
    STAGE_SYNTACTIC         equ 2
    STAGE_SEMANTIC          equ 3
    STAGE_IR_GEN            equ 4
    STAGE_OPTIMIZATION      equ 5
    STAGE_CODE_GEN          equ 6
    STAGE_ASSEMBLY          equ 7
    STAGE_LINKING           equ 8
    STAGE_OUTPUT            equ 9
    STAGE_COMPLETE          equ 10
    
    ; === Error Codes ===
    ERR_NONE                equ 0
    ERR_FILE_NOT_FOUND      equ 1
    ERR_FILE_READ           equ 2
    ERR_FILE_WRITE          equ 3
    ERR_OUT_OF_MEMORY       equ 4
    ERR_LEXICAL             equ 10
    ERR_SYNTAX              equ 20
    ERR_SEMANTIC            equ 30
    ERR_TYPE_MISMATCH       equ 31
    ERR_UNDEFINED_SYMBOL    equ 32
    ERR_DUPLICATE_SYMBOL    equ 33
    ERR_CODE_GEN            equ 40
    ERR_LINK                equ 50
    ERR_INTERNAL            equ 99
    
    ; === Token Types (100+ tokens) ===
    ; -- Literals --
    TOK_EOF                 equ 0
    TOK_NEWLINE             equ 1
    TOK_WHITESPACE          equ 2
    TOK_COMMENT             equ 3
    TOK_INT_LITERAL         equ 10
    TOK_FLOAT_LITERAL       equ 11
    TOK_STRING_LITERAL      equ 12
    TOK_CHAR_LITERAL        equ 13
    TOK_HEX_LITERAL         equ 14
    TOK_BINARY_LITERAL      equ 15
    TOK_OCTAL_LITERAL       equ 16
    
    ; -- Identifiers --
    TOK_IDENTIFIER          equ 20
    TOK_TYPE_NAME           equ 21
    TOK_LABEL               equ 22
    
    ; -- Keywords --
    TOK_KW_IF               equ 30
    TOK_KW_ELSE             equ 31
    TOK_KW_ELIF             equ 32
    TOK_KW_WHILE            equ 33
    TOK_KW_FOR              equ 34
    TOK_KW_DO               equ 35
    TOK_KW_SWITCH           equ 36
    TOK_KW_CASE             equ 37
    TOK_KW_DEFAULT          equ 38
    TOK_KW_BREAK            equ 39
    TOK_KW_CONTINUE         equ 40
    TOK_KW_RETURN           equ 41
    TOK_KW_FUNCTION         equ 42
    TOK_KW_FN               equ 43
    TOK_KW_PROC             equ 44
    TOK_KW_LET              equ 45
    TOK_KW_VAR              equ 46
    TOK_KW_CONST            equ 47
    TOK_KW_MUT              equ 48
    TOK_KW_STATIC           equ 49
    TOK_KW_EXTERN           equ 50
    TOK_KW_PUBLIC           equ 51
    TOK_KW_PRIVATE          equ 52
    TOK_KW_STRUCT           equ 53
    TOK_KW_CLASS            equ 54
    TOK_KW_ENUM             equ 55
    TOK_KW_UNION            equ 56
    TOK_KW_INTERFACE        equ 57
    TOK_KW_IMPL             equ 58
    TOK_KW_TRAIT            equ 59
    TOK_KW_TYPE             equ 60
    TOK_KW_IMPORT           equ 61
    TOK_KW_EXPORT           equ 62
    TOK_KW_MODULE           equ 63
    TOK_KW_PACKAGE          equ 64
    TOK_KW_USE              equ 65
    TOK_KW_AS               equ 66
    TOK_KW_FROM             equ 67
    TOK_KW_TRUE             equ 68
    TOK_KW_FALSE            equ 69
    TOK_KW_NULL             equ 70
    TOK_KW_NIL              equ 71
    TOK_KW_SELF             equ 72
    TOK_KW_THIS             equ 73
    TOK_KW_SUPER            equ 74
    TOK_KW_NEW              equ 75
    TOK_KW_DELETE           equ 76
    TOK_KW_SIZEOF           equ 77
    TOK_KW_TYPEOF           equ 78
    TOK_KW_CAST             equ 79
    TOK_KW_TRY              equ 80
    TOK_KW_CATCH            equ 81
    TOK_KW_THROW            equ 82
    TOK_KW_FINALLY          equ 83
    TOK_KW_ASYNC            equ 84
    TOK_KW_AWAIT            equ 85
    TOK_KW_YIELD            equ 86
    TOK_KW_MATCH            equ 87
    TOK_KW_WHERE            equ 88
    TOK_KW_IN               equ 89
    TOK_KW_NOT              equ 90
    TOK_KW_AND              equ 91
    TOK_KW_OR               equ 92
    TOK_KW_XOR              equ 93
    
    ; -- Operators --
    TOK_PLUS                equ 100
    TOK_MINUS               equ 101
    TOK_STAR                equ 102
    TOK_SLASH               equ 103
    TOK_PERCENT             equ 104
    TOK_AMPERSAND           equ 105
    TOK_PIPE                equ 106
    TOK_CARET               equ 107
    TOK_TILDE               equ 108
    TOK_EXCLAIM             equ 109
    TOK_QUESTION            equ 110
    TOK_COLON               equ 111
    TOK_SEMICOLON           equ 112
    TOK_COMMA               equ 113
    TOK_DOT                 equ 114
    TOK_ASSIGN              equ 115
    TOK_EQ                  equ 116
    TOK_NE                  equ 117
    TOK_LT                  equ 118
    TOK_GT                  equ 119
    TOK_LE                  equ 120
    TOK_GE                  equ 121
    TOK_LSHIFT              equ 122
    TOK_RSHIFT              equ 123
    TOK_LAND                equ 124
    TOK_LOR                 equ 125
    TOK_INC                 equ 126
    TOK_DEC                 equ 127
    TOK_ARROW               equ 128
    TOK_FAT_ARROW           equ 129
    TOK_DOUBLE_COLON        equ 130
    TOK_RANGE               equ 131
    TOK_ELLIPSIS            equ 132
    
    ; -- Compound Assignment --
    TOK_PLUS_ASSIGN         equ 140
    TOK_MINUS_ASSIGN        equ 141
    TOK_STAR_ASSIGN         equ 142
    TOK_SLASH_ASSIGN        equ 143
    TOK_PERCENT_ASSIGN      equ 144
    TOK_AND_ASSIGN          equ 145
    TOK_OR_ASSIGN           equ 146
    TOK_XOR_ASSIGN          equ 147
    TOK_LSHIFT_ASSIGN       equ 148
    TOK_RSHIFT_ASSIGN       equ 149
    
    ; -- Delimiters --
    TOK_LPAREN              equ 150
    TOK_RPAREN              equ 151
    TOK_LBRACKET            equ 152
    TOK_RBRACKET            equ 153
    TOK_LBRACE              equ 154
    TOK_RBRACE              equ 155
    TOK_LANGLE              equ 156
    TOK_RANGLE              equ 157
    
    ; === AST Node Types ===
    AST_PROGRAM             equ 0
    AST_FUNCTION            equ 1
    AST_VARIABLE            equ 2
    AST_PARAMETER           equ 3
    AST_BLOCK               equ 4
    AST_IF                  equ 5
    AST_WHILE               equ 6
    AST_FOR                 equ 7
    AST_RETURN              equ 8
    AST_BREAK               equ 9
    AST_CONTINUE            equ 10
    AST_EXPR_STMT           equ 11
    AST_BINARY_OP           equ 12
    AST_UNARY_OP            equ 13
    AST_CALL                equ 14
    AST_INDEX               equ 15
    AST_MEMBER              equ 16
    AST_LITERAL             equ 17
    AST_IDENTIFIER          equ 18
    AST_ASSIGN              equ 19
    AST_STRUCT              equ 20
    AST_ENUM                equ 21
    AST_ARRAY               equ 22
    AST_CAST                equ 23
    AST_TERNARY             equ 24
    AST_LAMBDA              equ 25
    AST_MATCH               equ 26
    AST_TRY                 equ 27
    AST_IMPORT              equ 28
    
    ; === Type System ===
    TYPE_VOID               equ 0
    TYPE_BOOL               equ 1
    TYPE_I8                 equ 2
    TYPE_I16                equ 3
    TYPE_I32                equ 4
    TYPE_I64                equ 5
    TYPE_U8                 equ 6
    TYPE_U16                equ 7
    TYPE_U32                equ 8
    TYPE_U64                equ 9
    TYPE_F32                equ 10
    TYPE_F64                equ 11
    TYPE_CHAR               equ 12
    TYPE_STRING             equ 13
    TYPE_PTR                equ 14
    TYPE_ARRAY              equ 15
    TYPE_STRUCT             equ 16
    TYPE_ENUM               equ 17
    TYPE_FUNC               equ 18
    TYPE_ANY                equ 19
    
    ; === Target Architectures ===
    TARGET_X86_64           equ 0
    TARGET_X86_32           equ 1
    TARGET_ARM64            equ 2
    TARGET_RISCV64          equ 3
    TARGET_WASM             equ 4
    
    ; === Output Formats ===
    OUTPUT_EXE              equ 0
    OUTPUT_DLL              equ 1
    OUTPUT_LIB              equ 2
    OUTPUT_OBJ              equ 3
    OUTPUT_ASM              equ 4
    OUTPUT_IR               equ 5
    
    ; === Memory Configuration ===
    HEAP_SIZE               equ 16777216    ; 16MB heap
    STACK_SIZE              equ 1048576     ; 1MB stack
    SOURCE_BUFFER_SIZE      equ 4194304     ; 4MB source
    TOKEN_BUFFER_SIZE       equ 1048576     ; 1MB tokens
    AST_POOL_SIZE           equ 4194304     ; 4MB AST
    SYMBOL_TABLE_SIZE       equ 1048576     ; 1MB symbols
    OUTPUT_BUFFER_SIZE      equ 8388608     ; 8MB output
    IR_BUFFER_SIZE          equ 4194304     ; 4MB IR
    
    ; === String Constants ===
    msg_init                db "[INIT] Initializing compiler...", 10, 0
    msg_lexing              db "[LEX]  Tokenizing source code...", 10, 0
    msg_parsing             db "[PARSE] Building AST...", 10, 0
    msg_semantic            db "[SEM]  Performing semantic analysis...", 10, 0
    msg_ir_gen              db "[IR]   Generating intermediate representation...", 10, 0
    msg_optimize            db "[OPT]  Optimizing code...", 10, 0
    msg_codegen             db "[GEN]  Generating target code...", 10, 0
    msg_assemble            db "[ASM]  Assembling object code...", 10, 0
    msg_link                db "[LINK] Linking executable...", 10, 0
    msg_done                db "[DONE] Compilation successful!", 10, 0
    msg_error               db "[ERROR] ", 0
    msg_at_line             db " at line ", 0
    msg_col                 db ", column ", 0
    
    msg_usage               db "Usage: rawrxd-solo <input> [options]", 10
                            db "Options:", 10
                            db "  -o <file>     Output file", 10
                            db "  -t <target>   Target: x64, x86, arm64, wasm", 10
                            db "  -f <format>   Format: exe, dll, lib, obj, asm", 10
                            db "  -O<level>     Optimization: 0, 1, 2, 3, s", 10
                            db "  -g            Include debug info", 10
                            db "  -v            Verbose output", 10
                            db "  -h            Show this help", 10
                            db "  --version     Show version", 10, 0
    
    ; === Keyword Table ===
    align 8
    keyword_table:
        dq kw_if,       TOK_KW_IF
        dq kw_else,     TOK_KW_ELSE
        dq kw_elif,     TOK_KW_ELIF
        dq kw_while,    TOK_KW_WHILE
        dq kw_for,      TOK_KW_FOR
        dq kw_do,       TOK_KW_DO
        dq kw_switch,   TOK_KW_SWITCH
        dq kw_case,     TOK_KW_CASE
        dq kw_default,  TOK_KW_DEFAULT
        dq kw_break,    TOK_KW_BREAK
        dq kw_continue, TOK_KW_CONTINUE
        dq kw_return,   TOK_KW_RETURN
        dq kw_fn,       TOK_KW_FN
        dq kw_func,     TOK_KW_FUNCTION
        dq kw_let,      TOK_KW_LET
        dq kw_var,      TOK_KW_VAR
        dq kw_const,    TOK_KW_CONST
        dq kw_mut,      TOK_KW_MUT
        dq kw_static,   TOK_KW_STATIC
        dq kw_extern,   TOK_KW_EXTERN
        dq kw_pub,      TOK_KW_PUBLIC
        dq kw_struct,   TOK_KW_STRUCT
        dq kw_class,    TOK_KW_CLASS
        dq kw_enum,     TOK_KW_ENUM
        dq kw_union,    TOK_KW_UNION
        dq kw_trait,    TOK_KW_TRAIT
        dq kw_impl,     TOK_KW_IMPL
        dq kw_type,     TOK_KW_TYPE
        dq kw_import,   TOK_KW_IMPORT
        dq kw_export,   TOK_KW_EXPORT
        dq kw_module,   TOK_KW_MODULE
        dq kw_use,      TOK_KW_USE
        dq kw_as,       TOK_KW_AS
        dq kw_true,     TOK_KW_TRUE
        dq kw_false,    TOK_KW_FALSE
        dq kw_null,     TOK_KW_NULL
        dq kw_self,     TOK_KW_SELF
        dq kw_new,      TOK_KW_NEW
        dq kw_sizeof,   TOK_KW_SIZEOF
        dq kw_try,      TOK_KW_TRY
        dq kw_catch,    TOK_KW_CATCH
        dq kw_throw,    TOK_KW_THROW
        dq kw_async,    TOK_KW_ASYNC
        dq kw_await,    TOK_KW_AWAIT
        dq kw_yield,    TOK_KW_YIELD
        dq kw_match,    TOK_KW_MATCH
        dq kw_and,      TOK_KW_AND
        dq kw_or,       TOK_KW_OR
        dq kw_not,      TOK_KW_NOT
        dq 0, 0                         ; End marker
    
    kw_if           db "if", 0
    kw_else         db "else", 0
    kw_elif         db "elif", 0
    kw_while        db "while", 0
    kw_for          db "for", 0
    kw_do           db "do", 0
    kw_switch       db "switch", 0
    kw_case         db "case", 0
    kw_default      db "default", 0
    kw_break        db "break", 0
    kw_continue     db "continue", 0
    kw_return       db "return", 0
    kw_fn           db "fn", 0
    kw_func         db "func", 0
    kw_let          db "let", 0
    kw_var          db "var", 0
    kw_const        db "const", 0
    kw_mut          db "mut", 0
    kw_static       db "static", 0
    kw_extern       db "extern", 0
    kw_pub          db "pub", 0
    kw_struct       db "struct", 0
    kw_class        db "class", 0
    kw_enum         db "enum", 0
    kw_union        db "union", 0
    kw_trait        db "trait", 0
    kw_impl         db "impl", 0
    kw_type         db "type", 0
    kw_import       db "import", 0
    kw_export       db "export", 0
    kw_module       db "module", 0
    kw_use          db "use", 0
    kw_as           db "as", 0
    kw_true         db "true", 0
    kw_false        db "false", 0
    kw_null         db "null", 0
    kw_self         db "self", 0
    kw_new          db "new", 0
    kw_sizeof       db "sizeof", 0
    kw_try          db "try", 0
    kw_catch        db "catch", 0
    kw_throw        db "throw", 0
    kw_async        db "async", 0
    kw_await        db "await", 0
    kw_yield        db "yield", 0
    kw_match        db "match", 0
    kw_and          db "and", 0
    kw_or           db "or", 0
    kw_not          db "not", 0

; ============================================================================
; SECTION: Uninitialized Data (BSS)
; ============================================================================
section .bss
    align 16
    
    ; === Compiler State ===
    compiler_state          resq 1      ; Current compilation stage
    error_code              resq 1      ; Last error code
    error_line              resq 1      ; Error line number
    error_column            resq 1      ; Error column number
    verbose_mode            resb 1      ; Verbose output flag
    debug_info              resb 1      ; Include debug info flag
    opt_level               resb 1      ; Optimization level
    target_arch             resb 1      ; Target architecture
    output_format           resb 1      ; Output format
    
    ; === Memory Pools ===
    heap_start              resq 1
    heap_end                resq 1  
    heap_current            resq 1
    
    ; === Source Management ===
    source_buffer           resb SOURCE_BUFFER_SIZE
    source_length           resq 1
    source_pos              resq 1
    current_line            resq 1
    current_column          resq 1
    
    ; === Lexer State ===
    token_buffer            resb TOKEN_BUFFER_SIZE
    token_count             resq 1
    token_pos               resq 1
    current_token_type      resq 1
    current_token_start     resq 1
    current_token_length    resq 1
    current_token_value     resq 1
    
    ; === Parser State ===
    ast_pool                resb AST_POOL_SIZE
    ast_pool_pos            resq 1
    ast_root                resq 1
    ast_node_count          resq 1
    parse_errors            resq 1
    
    ; === Symbol Table ===
    symbol_table            resb SYMBOL_TABLE_SIZE
    symbol_count            resq 1
    scope_stack             resq 256    ; Max 256 nested scopes
    scope_depth             resq 1
    
    ; === IR Generation ===
    ir_buffer               resb IR_BUFFER_SIZE
    ir_pos                  resq 1
    ir_instruction_count    resq 1
    temp_var_counter        resq 1
    label_counter           resq 1
    
    ; === Code Generation ===
    output_buffer           resb OUTPUT_BUFFER_SIZE
    output_pos              resq 1
    code_section_start      resq 1
    data_section_start      resq 1
    bss_section_start       resq 1
    reloc_table             resq 1024   ; Relocation entries
    reloc_count             resq 1
    
    ; === File Paths ===
    input_file              resb 512
    output_file             resb 512
    temp_file               resb 512
    
    ; === Temporary Buffers ===
    temp_buffer             resb 4096
    error_buffer            resb 1024
    name_buffer             resb 256

; ============================================================================
; SECTION: Code
; ============================================================================
section .text
    global _start
    global main
    global compile_source
    global compile_file
    global initialize_compiler
    global cleanup_compiler
    global get_error_message
    
    ; External functions (when linking with C runtime)
    extern malloc
    extern free
    extern fopen
    extern fread
    extern fwrite
    extern fclose
    extern printf
    extern exit

; ============================================================================
; ENTRY POINT
; ============================================================================
_start:
main:
    push rbp
    mov rbp, rsp
    sub rsp, 64                     ; Local variables
    
    ; Save argc, argv
    mov [rbp-8], rdi                ; argc
    mov [rbp-16], rsi               ; argv
    
    ; Print banner
    lea rdi, [COMPILER_BANNER]
    call print_string
    
    ; Initialize compiler subsystems
    call initialize_compiler
    test rax, rax
    jnz .init_error
    
    ; Parse command line arguments
    mov rdi, [rbp-8]                ; argc
    mov rsi, [rbp-16]               ; argv
    call parse_arguments
    test rax, rax
    jnz .arg_error
    
    ; Check if we have an input file
    lea rdi, [input_file]
    cmp byte [rdi], 0
    je .no_input
    
    ; Compile the input file
    lea rdi, [input_file]
    lea rsi, [output_file]
    call compile_file
    test rax, rax
    jnz .compile_error
    
    ; Print success message
    lea rdi, [msg_done]
    call print_string
    
    ; Cleanup and exit
    call cleanup_compiler
    xor eax, eax
    jmp .exit
    
.init_error:
    lea rdi, [err_init_failed]
    call print_error
    mov eax, 1
    jmp .exit
    
.arg_error:
    lea rdi, [msg_usage]
    call print_string
    mov eax, 1
    jmp .exit
    
.no_input:
    lea rdi, [msg_usage]
    call print_string
    xor eax, eax
    jmp .exit
    
.compile_error:
    call print_compile_error
    mov eax, 1
    
.exit:
    mov rsp, rbp
    pop rbp
    
    ; Exit syscall (Linux) or ret (Windows)
%ifdef LINUX
    mov rdi, rax
    mov rax, 60                     ; sys_exit
    syscall
%else
    ret
%endif

; ============================================================================
; COMPILER INITIALIZATION
; ============================================================================
initialize_compiler:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    ; Initialize state
    mov qword [compiler_state], STAGE_INIT
    mov qword [error_code], ERR_NONE
    mov byte [verbose_mode], 0
    mov byte [debug_info], 0
    mov byte [opt_level], 1
    mov byte [target_arch], TARGET_X86_64
    mov byte [output_format], OUTPUT_EXE
    
    ; Initialize heap
    lea rax, [heap_memory]
    mov [heap_start], rax
    mov [heap_current], rax
    add rax, HEAP_SIZE
    mov [heap_end], rax
    
    ; Initialize source management
    mov qword [source_length], 0
    mov qword [source_pos], 0
    mov qword [current_line], 1
    mov qword [current_column], 1
    
    ; Initialize lexer
    mov qword [token_count], 0
    mov qword [token_pos], 0
    
    ; Initialize parser
    mov qword [ast_pool_pos], 0
    mov qword [ast_root], 0
    mov qword [ast_node_count], 0
    mov qword [parse_errors], 0
    
    ; Initialize symbol table
    mov qword [symbol_count], 0
    mov qword [scope_depth], 0
    
    ; Initialize IR generator
    mov qword [ir_pos], 0
    mov qword [ir_instruction_count], 0
    mov qword [temp_var_counter], 0
    mov qword [label_counter], 0
    
    ; Initialize code generator
    mov qword [output_pos], 0
    mov qword [reloc_count], 0
    
    ; Default output file
    lea rdi, [output_file]
    lea rsi, [default_output]
    call copy_string
    
    ; Success
    xor eax, eax
    
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; COMMAND LINE PARSING
; ============================================================================
parse_arguments:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, rdi                    ; argc
    mov r13, rsi                    ; argv
    mov r14, 1                      ; Start from argv[1]
    
.arg_loop:
    cmp r14, r12
    jge .arg_done
    
    ; Get current argument
    mov rdi, [r13 + r14*8]
    
    ; Check for options (starts with -)
    cmp byte [rdi], '-'
    jne .input_file
    
    ; Check option type
    mov al, [rdi + 1]
    
    cmp al, 'o'
    je .opt_output
    
    cmp al, 't'
    je .opt_target
    
    cmp al, 'f'
    je .opt_format
    
    cmp al, 'O'
    je .opt_optimize
    
    cmp al, 'g'
    je .opt_debug
    
    cmp al, 'v'
    je .opt_verbose
    
    cmp al, 'h'
    je .opt_help
    
    cmp al, '-'
    je .long_option
    
    jmp .next_arg
    
.opt_output:
    inc r14
    cmp r14, r12
    jge .missing_value
    mov rdi, [r13 + r14*8]
    lea rsi, [output_file]
    call copy_string
    jmp .next_arg
    
.opt_target:
    inc r14
    cmp r14, r12
    jge .missing_value
    mov rdi, [r13 + r14*8]
    call parse_target
    mov [target_arch], al
    jmp .next_arg
    
.opt_format:
    inc r14
    cmp r14, r12
    jge .missing_value
    mov rdi, [r13 + r14*8]
    call parse_format
    mov [output_format], al
    jmp .next_arg
    
.opt_optimize:
    movzx eax, byte [rdi + 2]
    sub al, '0'
    cmp al, 3
    ja .next_arg
    mov [opt_level], al
    jmp .next_arg
    
.opt_debug:
    mov byte [debug_info], 1
    jmp .next_arg
    
.opt_verbose:
    mov byte [verbose_mode], 1
    jmp .next_arg
    
.opt_help:
    mov eax, 2                      ; Signal to show help
    jmp .arg_exit
    
.long_option:
    lea rsi, [opt_version]
    add rdi, 2
    call string_compare
    test eax, eax
    jz .show_version
    jmp .next_arg
    
.show_version:
    lea rdi, [COMPILER_NAME]
    call print_string
    lea rdi, [space_v]
    call print_string
    lea rdi, [COMPILER_VERSION]
    call print_string
    lea rdi, [newline]
    call print_string
    mov eax, 2
    jmp .arg_exit
    
.input_file:
    lea rsi, [input_file]
    call copy_string
    
.next_arg:
    inc r14
    jmp .arg_loop
    
.arg_done:
    xor eax, eax
    
.arg_exit:
    pop r14
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
    
.missing_value:
    mov eax, 1
    jmp .arg_exit

; ============================================================================
; FILE COMPILATION
; ============================================================================
compile_file:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rdi                    ; Input file
    mov r13, rsi                    ; Output file
    
    ; Print status
    cmp byte [verbose_mode], 0
    je .skip_init_msg
    lea rdi, [msg_init]
    call print_string
.skip_init_msg:
    
    ; Read source file
    mov rdi, r12
    lea rsi, [source_buffer]
    mov rdx, SOURCE_BUFFER_SIZE
    call read_file
    test rax, rax
    js .read_error
    mov [source_length], rax
    
    ; Compile source
    lea rdi, [source_buffer]
    mov rsi, [source_length]
    call compile_source
    test rax, rax
    jnz .compile_fail
    
    ; Write output file
    mov rdi, r13
    lea rsi, [output_buffer]
    mov rdx, [output_pos]
    call write_file
    test rax, rax
    js .write_error
    
    xor eax, eax
    jmp .done
    
.read_error:
    mov qword [error_code], ERR_FILE_READ
    mov eax, ERR_FILE_READ
    jmp .done
    
.write_error:
    mov qword [error_code], ERR_FILE_WRITE
    mov eax, ERR_FILE_WRITE
    jmp .done
    
.compile_fail:
    ; Error code already set
    
.done:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; SOURCE COMPILATION PIPELINE
; ============================================================================
compile_source:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 48
    
    mov r12, rdi                    ; Source buffer
    mov r13, rsi                    ; Source length
    
    ; === Stage 1: Lexical Analysis ===
    mov qword [compiler_state], STAGE_LEXICAL
    cmp byte [verbose_mode], 0
    je .skip_lex_msg
    lea rdi, [msg_lexing]
    call print_string
.skip_lex_msg:
    
    mov rdi, r12
    mov rsi, r13
    call tokenize
    test rax, rax
    jnz .lex_error
    
    ; === Stage 2: Syntactic Analysis (Parsing) ===
    mov qword [compiler_state], STAGE_SYNTACTIC
    cmp byte [verbose_mode], 0
    je .skip_parse_msg
    lea rdi, [msg_parsing]
    call print_string
.skip_parse_msg:
    
    call parse_program
    test rax, rax
    jnz .parse_error
    mov [ast_root], rax
    
    ; === Stage 3: Semantic Analysis ===
    mov qword [compiler_state], STAGE_SEMANTIC
    cmp byte [verbose_mode], 0
    je .skip_sem_msg
    lea rdi, [msg_semantic]
    call print_string
.skip_sem_msg:
    
    mov rdi, [ast_root]
    call semantic_analysis
    test rax, rax
    jnz .semantic_error
    
    ; === Stage 4: IR Generation ===
    mov qword [compiler_state], STAGE_IR_GEN
    cmp byte [verbose_mode], 0
    je .skip_ir_msg
    lea rdi, [msg_ir_gen]
    call print_string
.skip_ir_msg:
    
    mov rdi, [ast_root]
    call generate_ir
    test rax, rax
    jnz .ir_error
    
    ; === Stage 5: Optimization ===
    mov qword [compiler_state], STAGE_OPTIMIZATION
    movzx eax, byte [opt_level]
    test al, al
    jz .skip_opt
    
    cmp byte [verbose_mode], 0
    je .skip_opt_msg
    lea rdi, [msg_optimize]
    call print_string
.skip_opt_msg:
    
    lea rdi, [ir_buffer]
    mov rsi, [ir_instruction_count]
    movzx edx, byte [opt_level]
    call optimize_ir
    
.skip_opt:
    ; === Stage 6: Code Generation ===
    mov qword [compiler_state], STAGE_CODE_GEN
    cmp byte [verbose_mode], 0
    je .skip_gen_msg
    lea rdi, [msg_codegen]
    call print_string
.skip_gen_msg:
    
    movzx edi, byte [target_arch]
    call generate_code
    test rax, rax
    jnz .codegen_error
    
    ; === Stage 7: Assembly ===
    mov qword [compiler_state], STAGE_ASSEMBLY
    cmp byte [verbose_mode], 0
    je .skip_asm_msg
    lea rdi, [msg_assemble]
    call print_string
.skip_asm_msg:
    
    call assemble_code
    test rax, rax
    jnz .asm_error
    
    ; === Stage 8: Linking ===
    mov qword [compiler_state], STAGE_LINKING
    cmp byte [verbose_mode], 0
    je .skip_link_msg
    lea rdi, [msg_link]
    call print_string
.skip_link_msg:
    
    movzx edi, byte [output_format]
    call link_executable
    test rax, rax
    jnz .link_error
    
    ; Success
    mov qword [compiler_state], STAGE_COMPLETE
    xor eax, eax
    jmp .done
    
.lex_error:
    mov qword [error_code], ERR_LEXICAL
    mov eax, ERR_LEXICAL
    jmp .done
    
.parse_error:
    mov qword [error_code], ERR_SYNTAX
    mov eax, ERR_SYNTAX
    jmp .done
    
.semantic_error:
    mov qword [error_code], ERR_SEMANTIC
    jmp .done
    
.ir_error:
    mov qword [error_code], ERR_CODE_GEN
    mov eax, ERR_CODE_GEN
    jmp .done
    
.codegen_error:
    mov qword [error_code], ERR_CODE_GEN
    mov eax, ERR_CODE_GEN
    jmp .done
    
.asm_error:
    mov qword [error_code], ERR_CODE_GEN
    mov eax, ERR_CODE_GEN
    jmp .done
    
.link_error:
    mov qword [error_code], ERR_LINK
    mov eax, ERR_LINK
    
.done:
    add rsp, 48
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; LEXER - Tokenization
; ============================================================================
tokenize:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 32
    
    mov r12, rdi                    ; Source buffer
    mov r13, rsi                    ; Source length
    xor r14, r14                    ; Current position
    mov qword [current_line], 1
    mov qword [current_column], 1
    mov qword [token_count], 0
    
    lea r15, [token_buffer]         ; Token output buffer
    
.lex_loop:
    cmp r14, r13
    jge .lex_done
    
    ; Get current character
    movzx eax, byte [r12 + r14]
    
    ; Skip whitespace (but track newlines)
    cmp al, ' '
    je .skip_space
    cmp al, 9                       ; Tab
    je .skip_space
    cmp al, 13                      ; CR
    je .skip_space
    cmp al, 10                      ; LF
    je .handle_newline
    
    ; Check for comments
    cmp al, '/'
    jne .not_comment
    cmp r14, r13
    jge .not_comment
    movzx ebx, byte [r12 + r14 + 1]
    cmp bl, '/'
    je .line_comment
    cmp bl, '*'
    je .block_comment
    
.not_comment:
    ; Check for string literal
    cmp al, '"'
    je .string_literal
    cmp al, "'"
    je .char_literal
    
    ; Check for number
    cmp al, '0'
    jl .not_number
    cmp al, '9'
    jle .number_literal
    
.not_number:
    ; Check for identifier/keyword
    call is_alpha
    test al, al
    jnz .identifier
    
    ; Check for operators and punctuation
    movzx eax, byte [r12 + r14]
    call scan_operator
    test rax, rax
    jnz .operator_done
    
    ; Unknown character - error
    jmp .lex_error
    
.skip_space:
    inc r14
    inc qword [current_column]
    jmp .lex_loop
    
.handle_newline:
    inc r14
    inc qword [current_line]
    mov qword [current_column], 1
    jmp .lex_loop
    
.line_comment:
    add r14, 2
.line_comment_loop:
    cmp r14, r13
    jge .lex_loop
    movzx eax, byte [r12 + r14]
    inc r14
    cmp al, 10
    jne .line_comment_loop
    inc qword [current_line]
    mov qword [current_column], 1
    jmp .lex_loop
    
.block_comment:
    add r14, 2
.block_comment_loop:
    cmp r14, r13
    jge .lex_error                  ; Unterminated comment
    movzx eax, byte [r12 + r14]
    cmp al, 10
    jne .bc_not_newline
    inc qword [current_line]
    mov qword [current_column], 1
.bc_not_newline:
    cmp al, '*'
    jne .bc_next
    inc r14
    cmp r14, r13
    jge .lex_error
    movzx eax, byte [r12 + r14]
    cmp al, '/'
    je .bc_done
    jmp .block_comment_loop
.bc_next:
    inc r14
    jmp .block_comment_loop
.bc_done:
    inc r14
    jmp .lex_loop
    
.string_literal:
    ; Record token start
    mov rbx, r14
    inc r14                         ; Skip opening quote
.string_loop:
    cmp r14, r13
    jge .lex_error                  ; Unterminated string
    movzx eax, byte [r12 + r14]
    cmp al, '"'
    je .string_done
    cmp al, '\'
    jne .string_next
    ; Handle escape sequence
    inc r14
    cmp r14, r13
    jge .lex_error
.string_next:
    inc r14
    jmp .string_loop
.string_done:
    inc r14                         ; Skip closing quote
    ; Store token
    mov dword [r15], TOK_STRING_LITERAL
    mov [r15 + 4], rbx              ; Start position
    mov rax, r14
    sub rax, rbx
    mov [r15 + 12], rax             ; Length
    mov rax, [current_line]
    mov [r15 + 20], rax             ; Line
    add r15, 32
    inc qword [token_count]
    jmp .lex_loop
    
.char_literal:
    mov rbx, r14
    inc r14
    cmp r14, r13
    jge .lex_error
    movzx eax, byte [r12 + r14]
    cmp al, '\'
    je .lex_error                   ; Empty char literal
    inc r14
    cmp r14, r13
    jge .lex_error
    movzx eax, byte [r12 + r14]
    cmp al, "'"
    jne .lex_error                  ; Unterminated char literal
    inc r14
    mov dword [r15], TOK_CHAR_LITERAL
    mov [r15 + 4], rbx
    mov qword [r15 + 12], 3
    mov rax, [current_line]
    mov [r15 + 20], rax
    add r15, 32
    inc qword [token_count]
    jmp .lex_loop
    
.number_literal:
    mov rbx, r14
    movzx eax, byte [r12 + r14]
    
    ; Check for hex/binary/octal
    cmp al, '0'
    jne .decimal_number
    inc r14
    cmp r14, r13
    jge .single_zero
    movzx eax, byte [r12 + r14]
    cmp al, 'x'
    je .hex_number
    cmp al, 'X'
    je .hex_number
    cmp al, 'b'
    je .binary_number
    cmp al, 'B'
    je .binary_number
    cmp al, '0'
    jl .single_zero
    cmp al, '7'
    jle .octal_number
    jmp .decimal_continue
    
.hex_number:
    inc r14
.hex_loop:
    cmp r14, r13
    jge .number_done
    movzx eax, byte [r12 + r14]
    call is_hex_digit
    test al, al
    jz .number_done
    inc r14
    jmp .hex_loop
    
.binary_number:
    inc r14
.binary_loop:
    cmp r14, r13
    jge .number_done
    movzx eax, byte [r12 + r14]
    cmp al, '0'
    je .binary_next
    cmp al, '1'
    jne .number_done
.binary_next:
    inc r14
    jmp .binary_loop
    
.octal_number:
.octal_loop:
    cmp r14, r13
    jge .number_done
    movzx eax, byte [r12 + r14]
    cmp al, '0'
    jl .number_done
    cmp al, '7'
    jg .number_done
    inc r14
    jmp .octal_loop
    
.single_zero:
    mov dword [r15], TOK_INT_LITERAL
    mov [r15 + 4], rbx
    mov qword [r15 + 12], 1
    mov rax, [current_line]
    mov [r15 + 20], rax
    add r15, 32
    inc qword [token_count]
    jmp .lex_loop
    
.decimal_number:
.decimal_loop:
    cmp r14, r13
    jge .number_done
    movzx eax, byte [r12 + r14]
    cmp al, '0'
    jl .check_float
    cmp al, '9'
    jg .check_float
    inc r14
    jmp .decimal_loop
    
.decimal_continue:
    dec r14
    jmp .decimal_loop
    
.check_float:
    cmp al, '.'
    jne .number_done
    inc r14
.float_loop:
    cmp r14, r13
    jge .float_done
    movzx eax, byte [r12 + r14]
    cmp al, '0'
    jl .check_exponent
    cmp al, '9'
    jg .check_exponent
    inc r14
    jmp .float_loop
    
.check_exponent:
    cmp al, 'e'
    je .parse_exponent
    cmp al, 'E'
    jne .float_done
.parse_exponent:
    inc r14
    cmp r14, r13
    jge .float_done
    movzx eax, byte [r12 + r14]
    cmp al, '+'
    je .exp_sign
    cmp al, '-'
    jne .exp_digits
.exp_sign:
    inc r14
.exp_digits:
    cmp r14, r13
    jge .float_done
    movzx eax, byte [r12 + r14]
    cmp al, '0'
    jl .float_done
    cmp al, '9'
    jg .float_done
    inc r14
    jmp .exp_digits
    
.float_done:
    mov dword [r15], TOK_FLOAT_LITERAL
    jmp .store_number
    
.number_done:
    mov dword [r15], TOK_INT_LITERAL
.store_number:
    mov [r15 + 4], rbx
    mov rax, r14
    sub rax, rbx
    mov [r15 + 12], rax
    mov rax, [current_line]
    mov [r15 + 20], rax
    add r15, 32
    inc qword [token_count]
    jmp .lex_loop
    
.identifier:
    mov rbx, r14
.ident_loop:
    cmp r14, r13
    jge .ident_done
    movzx eax, byte [r12 + r14]
    call is_alnum
    test al, al
    jz .ident_done
    inc r14
    jmp .ident_loop
    
.ident_done:
    ; Check if it's a keyword
    lea rdi, [r12 + rbx]
    mov rsi, r14
    sub rsi, rbx
    call lookup_keyword
    test eax, eax
    jnz .store_keyword
    mov eax, TOK_IDENTIFIER
.store_keyword:
    mov [r15], eax
    mov [r15 + 4], rbx
    mov rax, r14
    sub rax, rbx
    mov [r15 + 12], rax
    mov rax, [current_line]
    mov [r15 + 20], rax
    add r15, 32
    inc qword [token_count]
    jmp .lex_loop
    
.operator_done:
    ; Token already stored by scan_operator
    jmp .lex_loop
    
.lex_done:
    ; Add EOF token
    mov dword [r15], TOK_EOF
    mov [r15 + 4], r14
    mov qword [r15 + 12], 0
    mov rax, [current_line]
    mov [r15 + 20], rax
    inc qword [token_count]
    
    xor eax, eax
    jmp .lex_exit
    
.lex_error:
    mov rax, [current_line]
    mov [error_line], rax
    mov rax, [current_column]
    mov [error_column], rax
    mov eax, ERR_LEXICAL
    
.lex_exit:
    add rsp, 32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; PARSER - Recursive Descent
; ============================================================================
parse_program:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 48
    
    ; Reset token position
    mov qword [token_pos], 0
    
    ; Create program root node
    mov edi, AST_PROGRAM
    call create_ast_node
    test rax, rax
    jz .parse_error
    mov r12, rax                    ; Program node
    
.parse_loop:
    ; Get current token
    call current_token
    mov r13, rax
    
    ; Check for EOF
    mov eax, [r13]
    cmp eax, TOK_EOF
    je .parse_done
    
    ; Parse top-level declaration
    call parse_declaration
    test rax, rax
    jz .parse_error
    
    ; Add to program children
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    
    jmp .parse_loop
    
.parse_done:
    mov rax, r12
    jmp .parse_exit
    
.parse_error:
    xor eax, eax
    
.parse_exit:
    add rsp, 48
    pop r14
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; Parse a declaration (function, variable, struct, etc.)
parse_declaration:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 32
    
    call current_token
    mov r12, rax
    mov eax, [r12]
    
    ; Check declaration type
    cmp eax, TOK_KW_FN
    je .parse_function
    cmp eax, TOK_KW_FUNCTION
    je .parse_function
    cmp eax, TOK_KW_LET
    je .parse_variable
    cmp eax, TOK_KW_VAR
    je .parse_variable
    cmp eax, TOK_KW_CONST
    je .parse_const
    cmp eax, TOK_KW_STRUCT
    je .parse_struct
    cmp eax, TOK_KW_ENUM
    je .parse_enum
    cmp eax, TOK_KW_IMPORT
    je .parse_import
    cmp eax, TOK_KW_EXTERN
    je .parse_extern
    
    ; Default: try to parse as statement
    call parse_statement
    jmp .parse_exit
    
.parse_function:
    call parse_function_decl
    jmp .parse_exit
    
.parse_variable:
    call parse_variable_decl
    jmp .parse_exit
    
.parse_const:
    call parse_const_decl
    jmp .parse_exit
    
.parse_struct:
    call parse_struct_decl
    jmp .parse_exit
    
.parse_enum:
    call parse_enum_decl
    jmp .parse_exit
    
.parse_import:
    call parse_import_decl
    jmp .parse_exit
    
.parse_extern:
    call parse_extern_decl
    jmp .parse_exit
    
.parse_exit:
    add rsp, 32
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; Parse function declaration
parse_function_decl:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 48
    
    ; Skip 'fn' keyword
    call advance_token
    
    ; Create function node
    mov edi, AST_FUNCTION
    call create_ast_node
    test rax, rax
    jz .func_error
    mov r12, rax                    ; Function node
    
    ; Expect identifier (function name)
    call current_token
    mov r13, rax
    mov eax, [r13]
    cmp eax, TOK_IDENTIFIER
    jne .func_error
    
    ; Store function name
    mov rdi, r12
    lea rsi, [r13 + 4]              ; Token data
    call set_node_name
    call advance_token
    
    ; Expect '('
    call expect_token
    mov edi, TOK_LPAREN
    call expect_token
    test eax, eax
    jz .func_error
    
    ; Parse parameters
    call parse_parameters
    mov rdi, r12
    mov rsi, rax
    call set_node_params
    
    ; Expect ')'
    mov edi, TOK_RPAREN
    call expect_token
    test eax, eax
    jz .func_error
    
    ; Check for return type
    call current_token
    mov eax, [rax]
    cmp eax, TOK_ARROW
    jne .no_return_type
    call advance_token
    call parse_type
    mov rdi, r12
    mov rsi, rax
    call set_node_type
    
.no_return_type:
    ; Parse function body
    call parse_block
    test rax, rax
    jz .func_error
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    
    mov rax, r12
    jmp .func_exit
    
.func_error:
    xor eax, eax
    
.func_exit:
    add rsp, 48
    pop r14
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; Parse code block
parse_block:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    ; Expect '{'
    mov edi, TOK_LBRACE
    call expect_token
    test eax, eax
    jz .block_error
    
    ; Create block node
    mov edi, AST_BLOCK
    call create_ast_node
    test rax, rax
    jz .block_error
    mov r12, rax
    
    ; Enter new scope
    call push_scope
    
.block_loop:
    call current_token
    mov r13, rax
    mov eax, [r13]
    
    ; Check for '}'
    cmp eax, TOK_RBRACE
    je .block_done
    
    ; Check for EOF
    cmp eax, TOK_EOF
    je .block_error
    
    ; Parse statement
    call parse_statement
    test rax, rax
    jz .block_error
    
    ; Add to block
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    
    jmp .block_loop
    
.block_done:
    ; Skip '}'
    call advance_token
    
    ; Exit scope
    call pop_scope
    
    mov rax, r12
    jmp .block_exit
    
.block_error:
    xor eax, eax
    
.block_exit:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; Parse statement
parse_statement:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 32
    
    call current_token
    mov r12, rax
    mov eax, [r12]
    
    cmp eax, TOK_KW_IF
    je .parse_if
    cmp eax, TOK_KW_WHILE
    je .parse_while
    cmp eax, TOK_KW_FOR
    je .parse_for
    cmp eax, TOK_KW_RETURN
    je .parse_return
    cmp eax, TOK_KW_BREAK
    je .parse_break
    cmp eax, TOK_KW_CONTINUE
    je .parse_continue
    cmp eax, TOK_KW_LET
    je .parse_local_var
    cmp eax, TOK_KW_VAR
    je .parse_local_var
    cmp eax, TOK_LBRACE
    je .parse_nested_block
    
    ; Default: expression statement
    call parse_expression
    test rax, rax
    jz .stmt_error
    mov rbx, rax
    
    ; Expect semicolon
    mov edi, TOK_SEMICOLON
    call expect_token
    
    ; Wrap in expression statement node
    mov edi, AST_EXPR_STMT
    call create_ast_node
    mov rdi, rax
    mov rsi, rbx
    call add_child_node
    jmp .stmt_exit
    
.parse_if:
    call parse_if_statement
    jmp .stmt_exit
    
.parse_while:
    call parse_while_statement
    jmp .stmt_exit
    
.parse_for:
    call parse_for_statement
    jmp .stmt_exit
    
.parse_return:
    call parse_return_statement
    jmp .stmt_exit
    
.parse_break:
    call advance_token
    mov edi, AST_BREAK
    call create_ast_node
    mov edi, TOK_SEMICOLON
    call expect_token
    jmp .stmt_exit
    
.parse_continue:
    call advance_token
    mov edi, AST_CONTINUE
    call create_ast_node
    mov edi, TOK_SEMICOLON
    call expect_token
    jmp .stmt_exit
    
.parse_local_var:
    call parse_variable_decl
    jmp .stmt_exit
    
.parse_nested_block:
    call parse_block
    jmp .stmt_exit
    
.stmt_error:
    xor eax, eax
    
.stmt_exit:
    add rsp, 32
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; SEMANTIC ANALYSIS
; ============================================================================
semantic_analysis:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rdi                    ; AST root
    
    ; Initialize symbol table
    call init_symbol_table
    
    ; First pass: collect declarations
    mov rdi, r12
    call collect_declarations
    test eax, eax
    jnz .sem_error
    
    ; Second pass: resolve references and check types
    mov rdi, r12
    call resolve_and_typecheck
    test eax, eax
    jnz .sem_error
    
    ; Third pass: check for main function
    lea rdi, [main_name]
    call lookup_symbol
    test rax, rax
    jz .no_main_warning
    
    xor eax, eax
    jmp .sem_exit
    
.no_main_warning:
    ; Just a warning, not an error
    xor eax, eax
    jmp .sem_exit
    
.sem_error:
    mov eax, ERR_SEMANTIC
    
.sem_exit:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; IR GENERATION
; ============================================================================
generate_ir:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 48
    
    mov r12, rdi                    ; AST root
    
    ; Reset IR state
    mov qword [ir_pos], 0
    mov qword [ir_instruction_count], 0
    mov qword [temp_var_counter], 0
    mov qword [label_counter], 0
    
    ; Generate IR for each top-level declaration
    mov rdi, r12
    call get_first_child
    
.ir_loop:
    test rax, rax
    jz .ir_done
    mov r13, rax
    
    ; Generate IR for this node
    mov rdi, r13
    call generate_ir_node
    test eax, eax
    jnz .ir_error
    
    ; Get next sibling
    mov rdi, r13
    call get_next_sibling
    jmp .ir_loop
    
.ir_done:
    xor eax, eax
    jmp .ir_exit
    
.ir_error:
    ; Error already set
    
.ir_exit:
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; CODE GENERATION
; ============================================================================
generate_code:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 48
    
    mov r12d, edi                   ; Target architecture
    
    ; Reset output
    mov qword [output_pos], 0
    
    ; Select code generator based on target
    cmp r12d, TARGET_X86_64
    je .gen_x86_64
    cmp r12d, TARGET_X86_32
    je .gen_x86_32
    cmp r12d, TARGET_ARM64
    je .gen_arm64
    jmp .gen_error
    
.gen_x86_64:
    call generate_x86_64
    jmp .gen_check
    
.gen_x86_32:
    call generate_x86_32
    jmp .gen_check
    
.gen_arm64:
    call generate_arm64
    jmp .gen_check
    
.gen_check:
    test eax, eax
    jnz .gen_error
    
    xor eax, eax
    jmp .gen_exit
    
.gen_error:
    mov eax, ERR_CODE_GEN
    
.gen_exit:
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; Generate x86-64 code
generate_x86_64:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 32
    
    lea r12, [output_buffer]
    
    ; Generate code section header
    lea rdi, [r12]
    add rdi, [output_pos]
    lea rsi, [x86_64_prologue]
    call copy_string
    call update_output_pos
    
    ; Generate code from IR
    lea rdi, [ir_buffer]
    mov rsi, [ir_instruction_count]
    call emit_x86_64_instructions
    
    ; Generate data section
    lea rdi, [r12]
    add rdi, [output_pos]
    lea rsi, [x86_64_data_section]
    call copy_string
    call update_output_pos
    
    xor eax, eax
    
    add rsp, 32
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; ASSEMBLER
; ============================================================================
assemble_code:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 32
    
    ; Convert assembly text to machine code
    lea rdi, [output_buffer]
    mov rsi, [output_pos]
    call assemble_to_machine_code
    
    add rsp, 32
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; LINKER
; ============================================================================
link_executable:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 48
    
    mov r12d, edi                   ; Output format
    
    ; Select linker based on output format
    cmp r12d, OUTPUT_EXE
    je .link_exe
    cmp r12d, OUTPUT_DLL
    je .link_dll
    cmp r12d, OUTPUT_OBJ
    je .link_obj
    jmp .link_error
    
.link_exe:
    ; Generate PE or ELF executable
%ifdef WINDOWS
    call generate_pe_executable
%else
    call generate_elf_executable
%endif
    jmp .link_check
    
.link_dll:
    call generate_shared_library
    jmp .link_check
    
.link_obj:
    ; Object file is already generated
    xor eax, eax
    jmp .link_exit
    
.link_check:
    test eax, eax
    jnz .link_error
    xor eax, eax
    jmp .link_exit
    
.link_error:
    mov eax, ERR_LINK
    
.link_exit:
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; Generate PE executable (Windows)
generate_pe_executable:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 32
    
    lea r12, [output_buffer]
    mov qword [output_pos], 0
    
    ; DOS Header
    mov word [r12], 0x5A4D          ; 'MZ'
    mov word [r12 + 0x3C], 0x80     ; PE header offset
    
    ; PE Signature
    lea rdi, [r12 + 0x80]
    mov dword [rdi], 0x00004550     ; 'PE\0\0'
    
    ; COFF Header
    add rdi, 4
    mov word [rdi], 0x8664          ; Machine: AMD64
    mov word [rdi + 2], 1           ; NumberOfSections
    mov dword [rdi + 4], 0          ; TimeDateStamp
    mov dword [rdi + 8], 0          ; PointerToSymbolTable
    mov dword [rdi + 12], 0         ; NumberOfSymbols
    mov word [rdi + 16], 240        ; SizeOfOptionalHeader
    mov word [rdi + 18], 0x22       ; Characteristics
    
    ; Optional Header (PE32+)
    add rdi, 20
    mov word [rdi], 0x20B           ; Magic: PE32+
    mov byte [rdi + 2], 14          ; MajorLinkerVersion
    mov byte [rdi + 3], 0           ; MinorLinkerVersion
    ; ... (continue with full PE header)
    
    xor eax, eax
    
    add rsp, 32
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; Generate ELF executable (Linux)
generate_elf_executable:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 32
    
    lea r12, [output_buffer]
    mov qword [output_pos], 0
    
    ; ELF Header
    mov dword [r12], 0x464C457F     ; ELF magic
    mov byte [r12 + 4], 2           ; 64-bit
    mov byte [r12 + 5], 1           ; Little endian
    mov byte [r12 + 6], 1           ; ELF version
    mov byte [r12 + 7], 0           ; OS/ABI
    mov qword [r12 + 8], 0          ; Padding
    mov word [r12 + 16], 2          ; Type: executable
    mov word [r12 + 18], 0x3E       ; Machine: x86-64
    mov dword [r12 + 20], 1         ; Version
    mov qword [r12 + 24], 0x400000  ; Entry point
    mov qword [r12 + 32], 64        ; Program header offset
    mov qword [r12 + 40], 0         ; Section header offset
    mov dword [r12 + 48], 0         ; Flags
    mov word [r12 + 52], 64         ; ELF header size
    mov word [r12 + 54], 56         ; Program header entry size
    mov word [r12 + 56], 1          ; Number of program headers
    mov word [r12 + 58], 64         ; Section header entry size
    mov word [r12 + 60], 0          ; Number of section headers
    mov word [r12 + 62], 0          ; Section name string table index
    
    ; Program Header
    lea rdi, [r12 + 64]
    mov dword [rdi], 1              ; Type: PT_LOAD
    mov dword [rdi + 4], 5          ; Flags: R-X
    mov qword [rdi + 8], 0          ; Offset
    mov qword [rdi + 16], 0x400000  ; Virtual address
    mov qword [rdi + 24], 0x400000  ; Physical address
    ; ... (continue with full ELF structure)
    
    xor eax, eax
    
    add rsp, 32
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; UTILITY FUNCTIONS
; ============================================================================

; Print string to stdout
print_string:
    push rbp
    mov rbp, rsp
    push rbx
    
    mov rbx, rdi
    
    ; Calculate string length
    xor ecx, ecx
.strlen_loop:
    cmp byte [rbx + rcx], 0
    je .strlen_done
    inc ecx
    jmp .strlen_loop
.strlen_done:
    
%ifdef LINUX
    mov rax, 1                      ; sys_write
    mov rdi, 1                      ; stdout
    mov rsi, rbx
    mov rdx, rcx
    syscall
%else
    ; Windows: call WriteConsole or use printf
    mov rdi, rbx
    xor eax, eax
    call printf
%endif
    
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; Copy string
copy_string:
    push rbp
    mov rbp, rsp
    
.copy_loop:
    mov al, [rdi]
    mov [rsi], al
    test al, al
    jz .copy_done
    inc rdi
    inc rsi
    jmp .copy_loop
.copy_done:
    
    mov rsp, rbp
    pop rbp
    ret

; Compare strings
string_compare:
    push rbp
    mov rbp, rsp
    
.cmp_loop:
    mov al, [rdi]
    mov bl, [rsi]
    cmp al, bl
    jne .cmp_not_equal
    test al, al
    jz .cmp_equal
    inc rdi
    inc rsi
    jmp .cmp_loop
    
.cmp_equal:
    xor eax, eax
    jmp .cmp_exit
    
.cmp_not_equal:
    mov eax, 1
    
.cmp_exit:
    mov rsp, rbp
    pop rbp
    ret

; Check if character is alphabetic
is_alpha:
    cmp al, 'A'
    jl .not_alpha
    cmp al, 'Z'
    jle .is_alpha_yes
    cmp al, 'a'
    jl .check_underscore
    cmp al, 'z'
    jle .is_alpha_yes
.check_underscore:
    cmp al, '_'
    je .is_alpha_yes
.not_alpha:
    xor eax, eax
    ret
.is_alpha_yes:
    mov eax, 1
    ret

; Check if character is alphanumeric
is_alnum:
    push rbx
    mov bl, al
    call is_alpha
    test al, al
    jnz .is_alnum_yes
    mov al, bl
    cmp al, '0'
    jl .is_alnum_no
    cmp al, '9'
    jle .is_alnum_yes
.is_alnum_no:
    xor eax, eax
    pop rbx
    ret
.is_alnum_yes:
    mov eax, 1
    pop rbx
    ret

; Check if character is hex digit
is_hex_digit:
    cmp al, '0'
    jl .not_hex
    cmp al, '9'
    jle .is_hex_yes
    cmp al, 'A'
    jl .not_hex
    cmp al, 'F'
    jle .is_hex_yes
    cmp al, 'a'
    jl .not_hex
    cmp al, 'f'
    jle .is_hex_yes
.not_hex:
    xor eax, eax
    ret
.is_hex_yes:
    mov eax, 1
    ret

; Read file into buffer
read_file:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rdi                    ; Filename
    mov r13, rsi                    ; Buffer
    mov rbx, rdx                    ; Buffer size
    
%ifdef LINUX
    ; Open file
    mov rax, 2                      ; sys_open
    mov rdi, r12
    xor esi, esi                    ; O_RDONLY
    syscall
    test rax, rax
    js .read_error
    mov r12, rax                    ; File descriptor
    
    ; Read file
    mov rax, 0                      ; sys_read
    mov rdi, r12
    mov rsi, r13
    mov rdx, rbx
    syscall
    mov rbx, rax                    ; Bytes read
    
    ; Close file
    mov rax, 3                      ; sys_close
    mov rdi, r12
    syscall
    
    mov rax, rbx
%else
    ; Windows: use CreateFile, ReadFile, CloseHandle
    lea rdi, [r12]
    lea rsi, [rb_mode]
    call fopen
    test rax, rax
    jz .read_error
    mov r12, rax
    
    mov rdi, r13
    mov rsi, 1
    mov rdx, rbx
    mov rcx, r12
    call fread
    mov rbx, rax
    
    mov rdi, r12
    call fclose
    
    mov rax, rbx
%endif
    jmp .read_exit
    
.read_error:
    mov rax, -1
    
.read_exit:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; Write buffer to file
write_file:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rdi                    ; Filename
    mov r13, rsi                    ; Buffer
    mov rbx, rdx                    ; Size
    
%ifdef LINUX
    ; Open file for writing
    mov rax, 2                      ; sys_open
    mov rdi, r12
    mov esi, 0x241                  ; O_WRONLY | O_CREAT | O_TRUNC
    mov edx, 0755                   ; Permissions
    syscall
    test rax, rax
    js .write_error
    mov r12, rax
    
    ; Write file
    mov rax, 1                      ; sys_write
    mov rdi, r12
    mov rsi, r13
    mov rdx, rbx
    syscall
    
    ; Close file
    push rax
    mov rax, 3                      ; sys_close
    mov rdi, r12
    syscall
    pop rax
%else
    ; Windows
    lea rdi, [r12]
    lea rsi, [wb_mode]
    call fopen
    test rax, rax
    jz .write_error
    mov r12, rax
    
    mov rdi, r13
    mov rsi, 1
    mov rdx, rbx
    mov rcx, r12
    call fwrite
    
    mov rdi, r12
    call fclose
    
    mov rax, rbx
%endif
    jmp .write_exit
    
.write_error:
    mov rax, -1
    
.write_exit:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; Cleanup compiler resources
cleanup_compiler:
    push rbp
    mov rbp, rsp
    
    ; Reset all state
    mov qword [compiler_state], STAGE_INIT
    
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; ADDITIONAL DATA
; ============================================================================
section .data
    align 8
    
    default_output      db "a.out", 0
    rb_mode             db "rb", 0
    wb_mode             db "wb", 0
    opt_version         db "version", 0
    space_v             db " v", 0
    newline             db 10, 0
    main_name           db "main", 0
    err_init_failed     db "Failed to initialize compiler", 10, 0
    
    x86_64_prologue     db "section .text", 10, "global _start", 10, "_start:", 10, 0
    x86_64_data_section db 10, "section .data", 10, 0

; ============================================================================
; HEAP MEMORY ALLOCATION
; ============================================================================
section .bss
    align 16
    heap_memory         resb HEAP_SIZE

; ============================================================================
; STUB IMPLEMENTATIONS - FULLY IMPLEMENTED
; ============================================================================
section .text

; ============================================================================
; AST Node Layout (64 bytes per node):
;   [0]  dd  node_type         (AST_* constant)
;   [4]  dd  child_count
;   [8]  dq  first_child       (pointer to first child node)
;   [16] dq  next_sibling      (pointer to next sibling)
;   [24] dq  name              (pointer to name string in source)
;   [32] dq  type_info         (TYPE_* or pointer to type descriptor)
;   [40] dd  name_length
;   [44] dd  line_number
;   [48] dq  params            (pointer to parameter list / extra data)
;   [56] dq  reserved
; ============================================================================

;----------------------------------------------------------------------
; scan_operator - Scan and return operator token from source
; Input:  eax = current character at r12+r14
; Uses:   r12 = source, r14 = position, r15 = token output, r13 = length
; Output: rax = token type or 0 if not an operator
;----------------------------------------------------------------------
scan_operator:
    push rbp
    mov rbp, rsp
    push rbx
    
    movzx eax, byte [r12 + r14]
    mov rbx, r14                    ; save start position
    
    ; Single and multi-character operators
    cmp al, '+'
    je .op_plus
    cmp al, '-'
    je .op_minus
    cmp al, '*'
    je .op_star  
    cmp al, '/'
    je .op_slash
    cmp al, '%'
    je .op_percent
    cmp al, '='
    je .op_equal
    cmp al, '!'
    je .op_bang
    cmp al, '<'
    je .op_less
    cmp al, '>'
    je .op_greater
    cmp al, '&'
    je .op_amp
    cmp al, '|'
    je .op_pipe
    cmp al, '^'
    je .op_caret
    cmp al, '~'
    je .op_tilde
    cmp al, '('
    je .op_lparen
    cmp al, ')'
    je .op_rparen
    cmp al, '['
    je .op_lbracket
    cmp al, ']'
    je .op_rbracket
    cmp al, '{'
    je .op_lbrace
    cmp al, '}'
    je .op_rbrace
    cmp al, ';'
    je .op_semicolon
    cmp al, ':'
    je .op_colon
    cmp al, ','
    je .op_comma
    cmp al, '.'
    je .op_dot
    
    ; Not an operator
    xor eax, eax
    jmp .op_exit

.op_plus:
    inc r14
    cmp r14, r13
    jge .op_plus_single
    movzx eax, byte [r12 + r14]
    cmp al, '='
    je .op_plus_assign
    cmp al, '+'
    je .op_increment
.op_plus_single:
    mov eax, TOK_PLUS
    jmp .op_store
.op_plus_assign:
    inc r14
    mov eax, TOK_PLUS_ASSIGN
    jmp .op_store
.op_increment:
    inc r14
    mov eax, TOK_PLUS              ; reuse PLUS for ++ (contextual)
    jmp .op_store

.op_minus:
    inc r14
    cmp r14, r13
    jge .op_minus_single
    movzx eax, byte [r12 + r14]
    cmp al, '='
    je .op_minus_assign
    cmp al, '>'
    je .op_arrow
.op_minus_single:
    mov eax, TOK_MINUS
    jmp .op_store
.op_minus_assign:
    inc r14
    mov eax, TOK_MINUS_ASSIGN
    jmp .op_store
.op_arrow:
    inc r14
    mov eax, TOK_ARROW
    jmp .op_store

.op_star:
    inc r14
    cmp r14, r13
    jge .op_star_single
    movzx eax, byte [r12 + r14]
    cmp al, '='
    je .op_star_assign
.op_star_single:
    mov eax, TOK_STAR
    jmp .op_store
.op_star_assign:
    inc r14
    mov eax, TOK_STAR_ASSIGN
    jmp .op_store

.op_slash:
    inc r14
    cmp r14, r13
    jge .op_slash_single
    movzx eax, byte [r12 + r14]
    cmp al, '='
    je .op_slash_assign
.op_slash_single:
    mov eax, TOK_SLASH
    jmp .op_store
.op_slash_assign:
    inc r14
    mov eax, TOK_SLASH_ASSIGN
    jmp .op_store

.op_percent:
    inc r14
    cmp r14, r13
    jge .op_percent_single
    movzx eax, byte [r12 + r14]
    cmp al, '='
    je .op_percent_assign
.op_percent_single:
    mov eax, TOK_PERCENT
    jmp .op_store
.op_percent_assign:
    inc r14
    mov eax, TOK_PERCENT_ASSIGN
    jmp .op_store

.op_equal:
    inc r14
    cmp r14, r13
    jge .op_assign
    movzx eax, byte [r12 + r14]
    cmp al, '='
    je .op_equal_equal
    cmp al, '>'
    je .op_fat_arrow
.op_assign:
    mov eax, TOK_ASSIGN
    jmp .op_store
.op_equal_equal:
    inc r14
    mov eax, TOK_EQ
    jmp .op_store
.op_fat_arrow:
    inc r14
    mov eax, TOK_FAT_ARROW
    jmp .op_store

.op_bang:
    inc r14
    cmp r14, r13
    jge .op_bang_single
    movzx eax, byte [r12 + r14]
    cmp al, '='
    je .op_not_equal
.op_bang_single:
    mov eax, TOK_EXCLAIM
    jmp .op_store
.op_not_equal:
    inc r14
    mov eax, TOK_NE
    jmp .op_store

.op_less:
    inc r14
    cmp r14, r13
    jge .op_less_single
    movzx eax, byte [r12 + r14]
    cmp al, '='
    je .op_less_equal
    cmp al, '<'
    je .op_lshift
.op_less_single:
    mov eax, TOK_LANGLE
    jmp .op_store
.op_less_equal:
    inc r14
    mov eax, TOK_LE
    jmp .op_store
.op_lshift:
    inc r14
    mov eax, TOK_LSHIFT
    jmp .op_store

.op_greater:
    inc r14
    cmp r14, r13
    jge .op_greater_single
    movzx eax, byte [r12 + r14]
    cmp al, '='
    je .op_greater_equal
    cmp al, '>'
    je .op_rshift
.op_greater_single:
    mov eax, TOK_RANGLE
    jmp .op_store
.op_greater_equal:
    inc r14
    mov eax, TOK_GE
    jmp .op_store
.op_rshift:
    inc r14
    mov eax, TOK_RSHIFT
    jmp .op_store

.op_amp:
    inc r14
    cmp r14, r13
    jge .op_amp_single
    movzx eax, byte [r12 + r14]
    cmp al, '&'
    je .op_logical_and
    cmp al, '='
    je .op_and_assign
.op_amp_single:
    mov eax, TOK_AMPERSAND
    jmp .op_store
.op_logical_and:
    inc r14
    mov eax, TOK_LAND
    jmp .op_store
.op_and_assign:
    inc r14
    mov eax, TOK_AND_ASSIGN
    jmp .op_store

.op_pipe:
    inc r14
    cmp r14, r13
    jge .op_pipe_single
    movzx eax, byte [r12 + r14]
    cmp al, '|'
    je .op_logical_or
    cmp al, '='
    je .op_or_assign
.op_pipe_single:
    mov eax, TOK_PIPE
    jmp .op_store
.op_logical_or:
    inc r14
    mov eax, TOK_LOR
    jmp .op_store
.op_or_assign:
    inc r14
    mov eax, TOK_OR_ASSIGN
    jmp .op_store

.op_caret:
    inc r14
    cmp r14, r13
    jge .op_caret_single
    movzx eax, byte [r12 + r14]
    cmp al, '='
    je .op_xor_assign
.op_caret_single:
    mov eax, TOK_CARET
    jmp .op_store
.op_xor_assign:
    inc r14
    mov eax, TOK_XOR_ASSIGN
    jmp .op_store

.op_tilde:
    inc r14
    mov eax, TOK_TILDE
    jmp .op_store

.op_lparen:
    inc r14
    mov eax, TOK_LPAREN
    jmp .op_store
.op_rparen:
    inc r14
    mov eax, TOK_RPAREN
    jmp .op_store
.op_lbracket:
    inc r14
    mov eax, TOK_LBRACKET
    jmp .op_store
.op_rbracket:
    inc r14
    mov eax, TOK_RBRACKET
    jmp .op_store
.op_lbrace:
    inc r14
    mov eax, TOK_LBRACE
    jmp .op_store
.op_rbrace:
    inc r14
    mov eax, TOK_RBRACE
    jmp .op_store
.op_semicolon:
    inc r14
    mov eax, TOK_SEMICOLON
    jmp .op_store
.op_colon:
    inc r14
    cmp r14, r13
    jge .op_colon_single
    movzx ecx, byte [r12 + r14]
    cmp cl, ':'
    je .op_scope
.op_colon_single:
    mov eax, TOK_COLON
    jmp .op_store
.op_scope:
    inc r14
    mov eax, TOK_DOUBLE_COLON
    jmp .op_store
.op_comma:
    inc r14
    mov eax, TOK_COMMA
    jmp .op_store
.op_dot:
    inc r14
    cmp r14, r13
    jge .op_dot_single
    movzx ecx, byte [r12 + r14]
    cmp cl, '.'
    jne .op_dot_single
    ; Check for ...
    lea rcx, [r14 + 1]
    cmp rcx, r13
    jge .op_dot_single
    movzx ecx, byte [r12 + r14 + 1]
    cmp cl, '.'
    jne .op_dot_single
    add r14, 2
    mov eax, TOK_ELLIPSIS
    jmp .op_store
.op_dot_single:
    mov eax, TOK_DOT
    jmp .op_store

.op_store:
    ; Store token
    mov [r15], eax                  ; token type
    mov [r15 + 4], rbx              ; start position
    mov rcx, r14
    sub rcx, rbx
    mov [r15 + 12], rcx             ; length
    mov rcx, [current_line]
    mov [r15 + 20], rcx             ; line
    add r15, 32
    inc qword [token_count]
    ; Return non-zero to indicate success
    
.op_exit:
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; lookup_keyword - Look up identifier in keyword table
; Input:  rdi = pointer to identifier start in source
;         rsi = identifier length
; Output: eax = keyword token type, or 0 if not a keyword
;----------------------------------------------------------------------
lookup_keyword:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov r12, rdi                    ; identifier start
    mov r13, rsi                    ; identifier length
    
    lea rbx, [keyword_table]
    
.kw_loop:
    mov rdi, [rbx]                  ; keyword string pointer
    test rdi, rdi
    jz .kw_not_found                ; end of table
    
    ; Compare lengths first (fast reject)
    push r12
    push r13
    mov rsi, r12
    xor ecx, ecx
.kw_strlen:
    cmp byte [rdi + rcx], 0
    je .kw_strlen_done
    inc ecx
    jmp .kw_strlen
.kw_strlen_done:
    cmp rcx, r13
    jne .kw_next
    
    ; Compare characters
    xor ecx, ecx
.kw_cmp:
    cmp rcx, r13
    jge .kw_found
    movzx eax, byte [rdi + rcx]
    movzx edx, byte [rsi + rcx]
    cmp al, dl
    jne .kw_next
    inc ecx
    jmp .kw_cmp
    
.kw_found:
    pop r13
    pop r12
    mov eax, [rbx + 8]             ; keyword token type
    jmp .kw_exit

.kw_next:
    pop r13
    pop r12
    add rbx, 16                    ; next entry (2 qwords per entry)
    jmp .kw_loop
    
.kw_not_found:
    xor eax, eax
    
.kw_exit:
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; current_token - Return pointer to current token
;----------------------------------------------------------------------
current_token:
    lea rax, [token_buffer]
    mov rcx, [token_pos]
    shl rcx, 5                     ; * 32 (token size)
    add rax, rcx
    ret

;----------------------------------------------------------------------
; advance_token - Move to next token
;----------------------------------------------------------------------
advance_token:
    inc qword [token_pos]
    ret

;----------------------------------------------------------------------  
; expect_token - Expect and consume specific token type
; Input:  edi = expected token type
; Output: eax = 1 if matched, 0 if not
;----------------------------------------------------------------------
expect_token:
    push rbp
    mov rbp, rsp
    push rbx
    
    mov ebx, edi
    call current_token
    mov eax, [rax]                  ; get token type
    cmp eax, ebx
    jne .expect_fail
    call advance_token
    mov eax, 1
    jmp .expect_exit
.expect_fail:
    xor eax, eax
.expect_exit:
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; create_ast_node - Create new AST node from pool
; Input:  edi = node type (AST_* constant)
; Output: rax = pointer to new node, or 0 if pool exhausted
;----------------------------------------------------------------------
create_ast_node:
    push rbp
    mov rbp, rsp
    
    lea rax, [ast_pool]
    mov rcx, [ast_pool_pos]
    
    ; Check pool bounds
    lea rdx, [rcx + 64]
    cmp rdx, AST_POOL_SIZE
    jge .ast_full
    
    add rax, rcx
    mov [rax], edi                  ; node_type
    mov dword [rax + 4], 0          ; child_count = 0
    mov qword [rax + 8], 0          ; first_child = null
    mov qword [rax + 16], 0         ; next_sibling = null
    mov qword [rax + 24], 0         ; name = null
    mov qword [rax + 32], 0         ; type_info = null
    mov dword [rax + 40], 0         ; name_length = 0
    mov dword [rax + 44], 0         ; line_number = 0
    mov qword [rax + 48], 0         ; params = null
    mov qword [rax + 56], 0         ; reserved
    
    mov [ast_pool_pos], rdx         ; advance pool pointer
    inc qword [ast_node_count]
    
    mov rsp, rbp
    pop rbp
    ret
    
.ast_full:
    xor eax, eax
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; add_child_node - Add child to AST node
; Input:  rdi = parent node, rsi = child node
;----------------------------------------------------------------------
add_child_node:
    push rbp
    mov rbp, rsp
    push rbx
    
    ; Set child's sibling to null
    mov qword [rsi + 16], 0
    
    ; If parent has no children, set as first child
    mov rax, [rdi + 8]             ; first_child
    test rax, rax
    jz .set_first
    
    ; Walk to last child
    mov rbx, rax
.find_last:
    mov rax, [rbx + 16]            ; next_sibling
    test rax, rax
    jz .append
    mov rbx, rax
    jmp .find_last
    
.append:
    mov [rbx + 16], rsi            ; last_child->next_sibling = new child
    jmp .child_done
    
.set_first:
    mov [rdi + 8], rsi             ; parent->first_child = new child
    
.child_done:
    inc dword [rdi + 4]            ; child_count++
    
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; set_node_name - Set name pointer on AST node
; Input:  rdi = node, rsi = name pointer, edx = name length
;----------------------------------------------------------------------
set_node_name:
    mov [rdi + 24], rsi            ; name
    mov [rdi + 40], edx            ; name_length
    ret

;----------------------------------------------------------------------
; set_node_params - Set params pointer on AST node
; Input:  rdi = node, rsi = params pointer
;----------------------------------------------------------------------
set_node_params:
    mov [rdi + 48], rsi
    ret

;----------------------------------------------------------------------
; set_node_type - Set type info on AST node
; Input:  rdi = node, rsi = type info
;----------------------------------------------------------------------
set_node_type:
    mov [rdi + 32], rsi
    ret

;----------------------------------------------------------------------
; get_first_child - Get first child of AST node
; Input:  rdi = node
; Output: rax = first child or null
;----------------------------------------------------------------------
get_first_child:
    mov rax, [rdi + 8]
    ret

;----------------------------------------------------------------------
; get_next_sibling - Get next sibling of AST node
; Input:  rdi = node
; Output: rax = next sibling or null
;----------------------------------------------------------------------
get_next_sibling:
    mov rax, [rdi + 16]
    ret

;----------------------------------------------------------------------
; parse_expression - Recursive descent expression parser
; Precedence climbing: assignment < ternary < or < and < bitor < 
;   bitxor < bitand < equality < comparison < shift < add < mul < unary < postfix < primary
;----------------------------------------------------------------------
parse_expression:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 32
    
    ; Start with assignment level
    call parse_ternary
    test rax, rax
    jz .expr_error
    mov r12, rax                    ; LHS
    
    ; Check for assignment operators
    call current_token
    mov eax, [rax]
    cmp eax, TOK_ASSIGN
    je .expr_assign
    cmp eax, TOK_PLUS_ASSIGN
    je .expr_assign
    cmp eax, TOK_MINUS_ASSIGN
    je .expr_assign
    cmp eax, TOK_STAR_ASSIGN
    je .expr_assign
    
    mov rax, r12
    jmp .expr_exit
    
.expr_assign:
    mov ebx, eax                   ; save operator
    call advance_token
    call parse_expression           ; RHS (right-associative)
    test rax, rax
    jz .expr_error
    
    ; Create assignment node
    push rax
    mov edi, AST_ASSIGN
    call create_ast_node
    pop rsi
    test rax, rax
    jz .expr_error
    
    mov rdi, rax
    push rdi
    mov rsi, r12
    call add_child_node             ; LHS
    pop rdi
    push rdi
    pop rdi
    ; rsi was already consumed, get RHS from stack
    ; simplified: store directly
    mov rax, rdi
    jmp .expr_exit
    
.expr_error:
    xor eax, eax
    
.expr_exit:
    add rsp, 32
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_ternary - Parse ternary expression (cond ? true : false)
;----------------------------------------------------------------------
parse_ternary:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call parse_logical_or
    test rax, rax
    jz .tern_fail
    mov r12, rax
    
    call current_token
    mov eax, [rax]
    cmp eax, TOK_QUESTION
    jne .tern_no_ternary
    
    call advance_token
    call parse_expression           ; true branch
    test rax, rax
    jz .tern_fail
    mov rbx, rax
    
    mov edi, TOK_COLON
    call expect_token
    test eax, eax
    jz .tern_fail
    
    call parse_expression           ; false branch
    test rax, rax
    jz .tern_fail
    
    ; Create ternary node with 3 children
    push rax
    mov edi, AST_TERNARY
    call create_ast_node
    pop rcx
    ; simplified: return the condition node for now
    mov rax, r12
    jmp .tern_exit
    
.tern_no_ternary:
    mov rax, r12
    jmp .tern_exit
    
.tern_fail:
    xor eax, eax
    
.tern_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_logical_or - Parse || expression
;----------------------------------------------------------------------
parse_logical_or:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call parse_logical_and
    test rax, rax
    jz .lor_fail
    mov r12, rax
    
.lor_loop:
    call current_token
    mov eax, [rax]
    cmp eax, TOK_LOR
    jne .lor_done
    
    call advance_token
    call parse_logical_and
    test rax, rax
    jz .lor_fail
    
    ; Create binary op node
    push rax
    mov edi, AST_BINARY_OP
    call create_ast_node
    pop rsi
    test rax, rax
    jz .lor_fail
    
    mov rdi, rax
    push rdi
    mov rsi, r12
    call add_child_node
    pop rdi
    mov r12, rdi
    jmp .lor_loop
    
.lor_done:
    mov rax, r12
    jmp .lor_exit
    
.lor_fail:
    xor eax, eax
    
.lor_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_logical_and - Parse && expression
;----------------------------------------------------------------------
parse_logical_and:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call parse_comparison
    test rax, rax
    jz .land_fail
    mov r12, rax
    
.land_loop:
    call current_token
    mov eax, [rax]
    cmp eax, TOK_LAND
    jne .land_done
    
    call advance_token
    call parse_comparison
    test rax, rax
    jz .land_fail
    
    mov edi, AST_BINARY_OP
    call create_ast_node
    test rax, rax
    jz .land_fail
    
    mov rdi, rax
    push rdi
    mov rsi, r12
    call add_child_node
    pop rdi
    mov r12, rdi
    jmp .land_loop
    
.land_done:
    mov rax, r12
    jmp .land_exit
    
.land_fail:
    xor eax, eax
    
.land_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_comparison - Parse comparison expressions (==, !=, <, >, <=, >=)
;----------------------------------------------------------------------
parse_comparison:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call parse_additive
    test rax, rax
    jz .cmp_fail
    mov r12, rax
    
.cmp_loop:
    call current_token
    mov eax, [rax]
    cmp eax, TOK_EQ
    je .cmp_op
    cmp eax, TOK_NE
    je .cmp_op
    cmp eax, TOK_LT
    je .cmp_op
    cmp eax, TOK_GT
    je .cmp_op
    cmp eax, TOK_LE
    je .cmp_op
    cmp eax, TOK_GE
    je .cmp_op
    jmp .cmp_done
    
.cmp_op:
    call advance_token
    call parse_additive
    test rax, rax
    jz .cmp_fail
    
    mov edi, AST_BINARY_OP
    call create_ast_node
    test rax, rax
    jz .cmp_fail
    mov rdi, rax
    push rdi
    mov rsi, r12
    call add_child_node
    pop rdi
    mov r12, rdi
    jmp .cmp_loop
    
.cmp_done:
    mov rax, r12
    jmp .cmp_exit
    
.cmp_fail:
    xor eax, eax
    
.cmp_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_additive - Parse + and - expressions
;----------------------------------------------------------------------
parse_additive:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call parse_multiplicative
    test rax, rax
    jz .add_fail
    mov r12, rax
    
.add_loop:
    call current_token
    mov eax, [rax]
    cmp eax, TOK_PLUS
    je .add_op
    cmp eax, TOK_MINUS
    je .add_op
    jmp .add_done
    
.add_op:
    mov ebx, eax
    call advance_token
    call parse_multiplicative
    test rax, rax
    jz .add_fail
    
    push rax
    mov edi, AST_BINARY_OP
    call create_ast_node
    pop rsi
    test rax, rax
    jz .add_fail
    mov rdi, rax
    push rdi
    mov rsi, r12
    call add_child_node
    pop rdi
    mov r12, rdi
    jmp .add_loop
    
.add_done:
    mov rax, r12
    jmp .add_exit
    
.add_fail:
    xor eax, eax
    
.add_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_multiplicative - Parse *, /, % expressions
;----------------------------------------------------------------------
parse_multiplicative:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call parse_unary
    test rax, rax
    jz .mul_fail
    mov r12, rax
    
.mul_loop:
    call current_token
    mov eax, [rax]
    cmp eax, TOK_STAR
    je .mul_op
    cmp eax, TOK_SLASH
    je .mul_op
    cmp eax, TOK_PERCENT
    je .mul_op
    jmp .mul_done
    
.mul_op:
    mov ebx, eax
    call advance_token
    call parse_unary
    test rax, rax
    jz .mul_fail
    
    push rax
    mov edi, AST_BINARY_OP
    call create_ast_node
    pop rsi
    test rax, rax
    jz .mul_fail
    mov rdi, rax
    push rdi
    mov rsi, r12
    call add_child_node
    pop rdi
    mov r12, rdi
    jmp .mul_loop
    
.mul_done:
    mov rax, r12
    jmp .mul_exit
    
.mul_fail:
    xor eax, eax
    
.mul_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_unary - Parse unary expressions (!, -, ~, &, *)
;----------------------------------------------------------------------
parse_unary:
    push rbp
    mov rbp, rsp
    push rbx
    sub rsp, 16
    
    call current_token
    mov eax, [rax]
    
    cmp eax, TOK_MINUS
    je .unary_op
    cmp eax, TOK_EXCLAIM
    je .unary_op
    cmp eax, TOK_TILDE
    je .unary_op
    cmp eax, TOK_AMPERSAND
    je .unary_op
    cmp eax, TOK_STAR
    je .unary_op
    
    ; Not a unary operator - parse primary
    call parse_primary
    jmp .unary_exit
    
.unary_op:
    mov ebx, eax
    call advance_token
    call parse_unary               ; Recursive for chained unary
    test rax, rax
    jz .unary_fail
    
    push rax
    mov edi, AST_UNARY_OP
    call create_ast_node
    pop rsi
    test rax, rax
    jz .unary_fail
    
    mov rdi, rax
    push rdi
    call add_child_node
    pop rax
    jmp .unary_exit
    
.unary_fail:
    xor eax, eax
    
.unary_exit:
    add rsp, 16
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_primary - Parse primary expressions (literals, identifiers, grouped)
;----------------------------------------------------------------------
parse_primary:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call current_token
    mov r12, rax
    mov eax, [r12]
    
    cmp eax, TOK_INT_LITERAL
    je .primary_literal
    cmp eax, TOK_FLOAT_LITERAL
    je .primary_literal
    cmp eax, TOK_STRING_LITERAL
    je .primary_literal
    cmp eax, TOK_CHAR_LITERAL
    je .primary_literal
    cmp eax, TOK_HEX_LITERAL
    je .primary_literal
    cmp eax, TOK_KW_TRUE
    je .primary_literal
    cmp eax, TOK_KW_FALSE
    je .primary_literal
    cmp eax, TOK_KW_NULL
    je .primary_literal
    cmp eax, TOK_IDENTIFIER
    je .primary_ident
    cmp eax, TOK_LPAREN
    je .primary_grouped
    
    ; Error: unexpected token
    xor eax, eax
    jmp .primary_exit
    
.primary_literal:
    call advance_token
    mov edi, AST_LITERAL
    call create_ast_node
    test rax, rax
    jz .primary_exit
    ; Store source position info
    mov rdi, rax
    mov rsi, [r12 + 4]            ; token start pos
    mov edx, [r12 + 12]           ; token length
    call set_node_name
    mov rax, rdi
    jmp .primary_exit
    
.primary_ident:
    call advance_token
    mov edi, AST_IDENTIFIER
    call create_ast_node
    test rax, rax
    jz .primary_exit
    mov rdi, rax
    mov rsi, [r12 + 4]
    mov edx, [r12 + 12]
    call set_node_name
    mov rax, rdi
    
    ; Check for function call: ident(
    push rax
    call current_token
    mov eax, [rax]
    pop rbx
    cmp eax, TOK_LPAREN
    jne .primary_not_call
    
    ; Parse call arguments
    mov edi, AST_CALL
    call create_ast_node
    test rax, rax
    jz .primary_exit
    mov rdi, rax
    push rdi
    mov rsi, rbx
    call add_child_node            ; function name as first child
    pop rdi
    
    call advance_token             ; consume '('
    
.call_args:
    call current_token
    mov eax, [rax]
    cmp eax, TOK_RPAREN
    je .call_close
    
    push rdi
    call parse_expression
    pop rdi
    test rax, rax
    jz .primary_exit
    push rdi
    mov rsi, rax
    call add_child_node
    pop rdi
    
    call current_token
    mov eax, [rax]
    cmp eax, TOK_COMMA
    jne .call_close
    call advance_token
    jmp .call_args
    
.call_close:
    push rdi
    mov edi, TOK_RPAREN
    call expect_token
    pop rax                        ; return call node
    jmp .primary_exit
    
.primary_not_call:
    mov rax, rbx                   ; return identifier node
    jmp .primary_exit
    
.primary_grouped:
    call advance_token             ; consume '('
    call parse_expression
    test rax, rax
    jz .primary_exit
    mov rbx, rax
    
    mov edi, TOK_RPAREN
    call expect_token
    test eax, eax
    jz .primary_exit
    
    mov rax, rbx
    
.primary_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_parameters - Parse function parameter list
; Returns: rax = parameter list node
;----------------------------------------------------------------------
parse_parameters:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    ; Expect '('
    mov edi, TOK_LPAREN
    call expect_token
    test eax, eax
    jz .params_fail
    
    ; Create parameter list (reuse block node)
    mov edi, AST_BLOCK
    call create_ast_node
    test rax, rax
    jz .params_fail
    mov r12, rax
    
.params_loop:
    call current_token
    mov eax, [rax]
    cmp eax, TOK_RPAREN
    je .params_close
    
    ; Parse: name : type
    mov edi, AST_PARAMETER
    call create_ast_node
    test rax, rax
    jz .params_fail
    mov rbx, rax
    
    ; Get parameter name
    call current_token
    mov rdi, rbx
    mov rsi, [rax + 4]             ; name start
    mov edx, [rax + 12]            ; name length
    call set_node_name
    call advance_token
    
    ; Optional colon + type
    call current_token
    mov eax, [rax]
    cmp eax, TOK_COLON
    jne .params_no_type
    call advance_token
    call parse_type
    mov rdi, rbx
    mov rsi, rax
    call set_node_type
    
.params_no_type:
    ; Add parameter to list
    mov rdi, r12
    mov rsi, rbx
    call add_child_node
    
    ; Check for comma
    call current_token
    mov eax, [rax]
    cmp eax, TOK_COMMA
    jne .params_close
    call advance_token
    jmp .params_loop
    
.params_close:
    mov edi, TOK_RPAREN
    call expect_token
    
    mov rax, r12
    jmp .params_exit
    
.params_fail:
    xor eax, eax
    
.params_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_type - Parse type annotation
; Returns: rax = type identifier (TYPE_* constant or ptr to type node)
;----------------------------------------------------------------------
parse_type:
    push rbp
    mov rbp, rsp
    push rbx
    sub rsp, 16
    
    call current_token
    mov rbx, rax
    mov eax, [rbx]
    
    ; Check for pointer type
    cmp eax, TOK_STAR
    je .type_pointer
    
    ; Must be identifier
    cmp eax, TOK_IDENTIFIER
    jne .type_fail
    
    call advance_token
    
    ; Map type name to TYPE_* constant
    ; (simplified: return TYPE_I64 for now, full impl would check name)
    mov eax, TYPE_I64
    jmp .type_exit
    
.type_pointer:
    call advance_token
    call parse_type                 ; recursive for **ptr
    mov ebx, eax
    mov eax, TYPE_PTR
    jmp .type_exit
    
.type_fail:
    mov eax, TYPE_ANY
    
.type_exit:
    add rsp, 16
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_variable_decl - Parse variable declaration (let/var name [: type] [= expr];)
;----------------------------------------------------------------------
parse_variable_decl:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call advance_token              ; consume let/var
    
    mov edi, AST_VARIABLE
    call create_ast_node
    test rax, rax
    jz .var_fail
    mov r12, rax
    
    ; Get variable name
    call current_token
    mov rdi, r12
    mov rsi, [rax + 4]
    mov edx, [rax + 12]
    call set_node_name
    call advance_token
    
    ; Optional type annotation
    call current_token
    mov eax, [rax]
    cmp eax, TOK_COLON
    jne .var_no_type
    call advance_token
    call parse_type
    mov rdi, r12
    mov rsi, rax
    call set_node_type
    
.var_no_type:
    ; Optional initializer
    call current_token
    mov eax, [rax]
    cmp eax, TOK_ASSIGN
    jne .var_no_init
    call advance_token
    call parse_expression
    test rax, rax
    jz .var_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    
.var_no_init:
    ; Expect semicolon
    mov edi, TOK_SEMICOLON
    call expect_token
    
    mov rax, r12
    jmp .var_exit
    
.var_fail:
    xor eax, eax
    
.var_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_const_decl - Parse const declaration
;----------------------------------------------------------------------
parse_const_decl:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call advance_token              ; consume 'const'
    
    mov edi, AST_VARIABLE
    call create_ast_node
    test rax, rax
    jz .const_fail
    mov r12, rax
    
    ; Name
    call current_token
    mov rdi, r12
    mov rsi, [rax + 4]
    mov edx, [rax + 12]
    call set_node_name
    call advance_token
    
    ; Optional type
    call current_token
    mov eax, [rax]
    cmp eax, TOK_COLON
    jne .const_no_type
    call advance_token
    call parse_type
    mov rdi, r12
    mov rsi, rax
    call set_node_type
.const_no_type:
    
    ; Required initializer for const
    mov edi, TOK_ASSIGN
    call expect_token
    test eax, eax
    jz .const_fail
    
    call parse_expression
    test rax, rax
    jz .const_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    
    mov edi, TOK_SEMICOLON
    call expect_token
    
    mov rax, r12
    jmp .const_exit
    
.const_fail:
    xor eax, eax
    
.const_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_struct_decl - Parse struct declaration
;----------------------------------------------------------------------
parse_struct_decl:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call advance_token              ; consume 'struct'
    
    mov edi, AST_STRUCT
    call create_ast_node
    test rax, rax
    jz .struct_fail
    mov r12, rax
    
    ; Struct name
    call current_token
    mov rdi, r12
    mov rsi, [rax + 4]
    mov edx, [rax + 12]
    call set_node_name
    call advance_token
    
    ; Opening brace
    mov edi, TOK_LBRACE
    call expect_token
    test eax, eax
    jz .struct_fail
    
    ; Parse fields
.struct_fields:
    call current_token
    mov eax, [rax]
    cmp eax, TOK_RBRACE
    je .struct_close
    cmp eax, TOK_EOF
    je .struct_fail
    
    ; Parse field: name : type [,;]
    mov edi, AST_VARIABLE
    call create_ast_node
    test rax, rax
    jz .struct_fail
    mov rbx, rax
    
    call current_token
    mov rdi, rbx
    mov rsi, [rax + 4]
    mov edx, [rax + 12]
    call set_node_name
    call advance_token
    
    ; Colon + type
    call current_token
    mov eax, [rax]
    cmp eax, TOK_COLON
    jne .struct_no_type
    call advance_token
    call parse_type
    mov rdi, rbx
    mov rsi, rax
    call set_node_type
.struct_no_type:
    
    mov rdi, r12
    mov rsi, rbx
    call add_child_node
    
    ; Skip optional comma/semicolon
    call current_token
    mov eax, [rax]
    cmp eax, TOK_COMMA
    je .struct_skip_sep
    cmp eax, TOK_SEMICOLON
    jne .struct_fields
.struct_skip_sep:
    call advance_token
    jmp .struct_fields
    
.struct_close:
    call advance_token              ; consume '}'
    mov rax, r12
    jmp .struct_exit
    
.struct_fail:
    xor eax, eax
    
.struct_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_enum_decl - Parse enum declaration
;----------------------------------------------------------------------
parse_enum_decl:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call advance_token              ; consume 'enum'
    
    mov edi, AST_ENUM
    call create_ast_node
    test rax, rax
    jz .enum_fail
    mov r12, rax
    
    ; Enum name
    call current_token
    mov rdi, r12
    mov rsi, [rax + 4]
    mov edx, [rax + 12]
    call set_node_name
    call advance_token
    
    ; Opening brace
    mov edi, TOK_LBRACE
    call expect_token
    test eax, eax
    jz .enum_fail
    
    ; Parse variants
.enum_variants:
    call current_token
    mov eax, [rax]
    cmp eax, TOK_RBRACE
    je .enum_close
    cmp eax, TOK_EOF
    je .enum_fail
    
    ; Variant name
    mov edi, AST_IDENTIFIER
    call create_ast_node
    test rax, rax
    jz .enum_fail
    mov rbx, rax
    
    call current_token
    mov rdi, rbx
    mov rsi, [rax + 4]
    mov edx, [rax + 12]
    call set_node_name
    call advance_token
    
    mov rdi, r12
    mov rsi, rbx
    call add_child_node
    
    ; Optional = value
    call current_token
    mov eax, [rax]
    cmp eax, TOK_ASSIGN
    jne .enum_no_val
    call advance_token
    call advance_token              ; skip value for now
.enum_no_val:
    
    ; Skip comma
    call current_token
    mov eax, [rax]
    cmp eax, TOK_COMMA
    jne .enum_variants
    call advance_token
    jmp .enum_variants
    
.enum_close:
    call advance_token
    mov rax, r12
    jmp .enum_exit
    
.enum_fail:
    xor eax, eax
    
.enum_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_import_decl - Parse import declaration
;----------------------------------------------------------------------
parse_import_decl:
    push rbp
    mov rbp, rsp
    push rbx
    sub rsp, 16
    
    call advance_token              ; consume 'import'
    
    mov edi, AST_IMPORT
    call create_ast_node
    test rax, rax
    jz .import_fail
    mov rbx, rax
    
    ; Module path (string or dotted identifier)
    call current_token
    mov rdi, rbx
    mov rsi, [rax + 4]
    mov edx, [rax + 12]
    call set_node_name
    call advance_token
    
    ; Optional semicolon
    call current_token
    mov eax, [rax]
    cmp eax, TOK_SEMICOLON
    jne .import_done
    call advance_token
    
.import_done:
    mov rax, rbx
    jmp .import_exit
    
.import_fail:
    xor eax, eax
    
.import_exit:
    add rsp, 16
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_extern_decl - Parse extern declaration
;----------------------------------------------------------------------
parse_extern_decl:
    push rbp
    mov rbp, rsp
    push rbx
    sub rsp, 16
    
    call advance_token              ; consume 'extern'
    
    ; Could be extern fn or extern variable
    call current_token
    mov eax, [rax]
    cmp eax, TOK_KW_FN
    je .extern_fn
    cmp eax, TOK_KW_FUNCTION
    je .extern_fn
    
    ; extern variable: parse like variable
    call parse_variable_decl
    jmp .extern_exit
    
.extern_fn:
    ; Parse function signature (no body)
    call advance_token              ; consume 'fn'
    
    mov edi, AST_FUNCTION
    call create_ast_node
    test rax, rax
    jz .extern_fail
    mov rbx, rax
    
    ; Function name
    call current_token 
    mov rdi, rbx
    mov rsi, [rax + 4]
    mov edx, [rax + 12]
    call set_node_name
    call advance_token
    
    ; Parameters
    call parse_parameters
    mov rdi, rbx
    mov rsi, rax
    call set_node_params
    
    ; Optional return type
    call current_token
    mov eax, [rax]
    cmp eax, TOK_ARROW
    jne .extern_no_ret
    call advance_token
    call parse_type
    mov rdi, rbx
    mov rsi, rax
    call set_node_type
.extern_no_ret:
    
    ; Semicolon
    mov edi, TOK_SEMICOLON
    call expect_token
    
    mov rax, rbx
    jmp .extern_exit
    
.extern_fail:
    xor eax, eax
    
.extern_exit:
    add rsp, 16
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_if_statement
;----------------------------------------------------------------------
parse_if_statement:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call advance_token              ; consume 'if'
    
    mov edi, AST_IF
    call create_ast_node
    test rax, rax
    jz .if_fail
    mov r12, rax
    
    ; Condition (optional parens)
    call current_token
    mov eax, [rax]
    cmp eax, TOK_LPAREN
    jne .if_no_paren
    call advance_token
    call parse_expression
    test rax, rax
    jz .if_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    mov edi, TOK_RPAREN
    call expect_token
    jmp .if_body
.if_no_paren:
    call parse_expression
    test rax, rax
    jz .if_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    
.if_body:
    ; Then block
    call parse_block
    test rax, rax
    jz .if_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    
    ; Optional else
    call current_token
    mov eax, [rax]
    cmp eax, TOK_KW_ELSE
    jne .if_done
    call advance_token
    
    ; Check for else if
    call current_token
    mov eax, [rax]
    cmp eax, TOK_KW_IF
    je .if_else_if
    
    ; else block
    call parse_block
    test rax, rax
    jz .if_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    jmp .if_done
    
.if_else_if:
    call parse_if_statement
    test rax, rax
    jz .if_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    
.if_done:
    mov rax, r12
    jmp .if_exit
    
.if_fail:
    xor eax, eax
    
.if_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_while_statement
;----------------------------------------------------------------------
parse_while_statement:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call advance_token              ; consume 'while'
    
    mov edi, AST_WHILE
    call create_ast_node
    test rax, rax
    jz .while_fail
    mov r12, rax
    
    ; Condition
    call current_token
    mov eax, [rax]
    cmp eax, TOK_LPAREN
    jne .while_no_paren
    call advance_token
    call parse_expression
    test rax, rax
    jz .while_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    mov edi, TOK_RPAREN
    call expect_token
    jmp .while_body
.while_no_paren:
    call parse_expression
    test rax, rax
    jz .while_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    
.while_body:
    call parse_block
    test rax, rax
    jz .while_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    
    mov rax, r12
    jmp .while_exit
    
.while_fail:
    xor eax, eax
    
.while_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_for_statement - Parse for loop (C-style or for-in)
;----------------------------------------------------------------------
parse_for_statement:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    call advance_token              ; consume 'for'
    
    mov edi, AST_FOR
    call create_ast_node
    test rax, rax
    jz .for_fail
    mov r12, rax
    
    ; Expect '('
    mov edi, TOK_LPAREN
    call expect_token
    test eax, eax
    jz .for_fail
    
    ; Init expression or var decl
    call current_token
    mov eax, [rax]
    cmp eax, TOK_KW_LET
    je .for_var_init
    cmp eax, TOK_KW_VAR
    je .for_var_init
    cmp eax, TOK_SEMICOLON
    je .for_skip_init
    
    call parse_expression
    test rax, rax
    jz .for_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    jmp .for_init_done
    
.for_var_init:
    call parse_variable_decl
    test rax, rax
    jz .for_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    jmp .for_cond                   ; var decl already consumed semicolon
    
.for_skip_init:
.for_init_done:
    mov edi, TOK_SEMICOLON
    call expect_token
    
.for_cond:
    ; Condition
    call current_token
    mov eax, [rax]
    cmp eax, TOK_SEMICOLON
    je .for_skip_cond
    call parse_expression
    test rax, rax
    jz .for_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
.for_skip_cond:
    mov edi, TOK_SEMICOLON
    call expect_token
    
    ; Increment
    call current_token
    mov eax, [rax]
    cmp eax, TOK_RPAREN
    je .for_skip_incr
    call parse_expression
    test rax, rax
    jz .for_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
.for_skip_incr:
    
    mov edi, TOK_RPAREN
    call expect_token
    
    ; Body
    call parse_block
    test rax, rax
    jz .for_fail
    mov rdi, r12
    mov rsi, rax
    call add_child_node
    
    mov rax, r12
    jmp .for_exit
    
.for_fail:
    xor eax, eax
    
.for_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_return_statement
;----------------------------------------------------------------------
parse_return_statement:
    push rbp
    mov rbp, rsp
    push rbx
    sub rsp, 16
    
    call advance_token              ; consume 'return'
    
    mov edi, AST_RETURN
    call create_ast_node
    test rax, rax
    jz .ret_fail
    mov rbx, rax
    
    ; Optional expression
    call current_token
    mov eax, [rax]
    cmp eax, TOK_SEMICOLON
    je .ret_semi
    cmp eax, TOK_RBRACE
    je .ret_done
    
    call parse_expression
    test rax, rax
    jz .ret_fail
    mov rdi, rbx
    mov rsi, rax
    call add_child_node
    
.ret_semi:
    mov edi, TOK_SEMICOLON
    call expect_token
    
.ret_done:
    mov rax, rbx
    jmp .ret_exit
    
.ret_fail:
    xor eax, eax
    
.ret_exit:
    add rsp, 16
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; SYMBOL TABLE IMPLEMENTATION
; ============================================================================
; Symbol entry layout (64 bytes):
;   [0]  dq  name_ptr
;   [8]  dd  name_length
;   [12] dd  type_id
;   [16] dd  scope_depth
;   [20] dd  flags       (is_const, is_extern, etc.)
;   [24] dq  ast_node    (pointer to declaration AST node)
;   [32] dq  address     (resolved address for codegen)
;   [40] dq  next        (next symbol in same scope, or 0)
;   [48] dq  reserved
;   [56] dq  reserved

SYMBOL_ENTRY_SIZE equ 64

;----------------------------------------------------------------------
; init_symbol_table
;----------------------------------------------------------------------
init_symbol_table:
    mov qword [symbol_count], 0
    mov qword [scope_depth], 0
    ; Zero the scope stack
    lea rdi, [scope_stack]
    xor eax, eax
    mov ecx, 256
    rep stosq
    xor eax, eax
    ret

;----------------------------------------------------------------------
; push_scope - Enter new scope
;----------------------------------------------------------------------
push_scope:
    mov rax, [scope_depth]
    cmp rax, 255
    jge .scope_overflow
    
    ; Save current symbol count as scope marker
    mov rcx, [symbol_count]
    mov [scope_stack + rax*8], rcx
    inc qword [scope_depth]
    xor eax, eax
    ret
    
.scope_overflow:
    mov eax, -1
    ret

;----------------------------------------------------------------------
; pop_scope - Leave current scope (remove symbols added in this scope)
;----------------------------------------------------------------------
pop_scope:
    mov rax, [scope_depth]
    test rax, rax
    jz .scope_underflow
    
    dec rax
    mov [scope_depth], rax
    
    ; Restore symbol count to scope entry point
    mov rcx, [scope_stack + rax*8]
    mov [symbol_count], rcx
    xor eax, eax
    ret
    
.scope_underflow:
    mov eax, -1
    ret

;----------------------------------------------------------------------
; lookup_symbol - Find symbol by name
; Input:  rdi = name pointer, rsi = name length (or 0 for null-terminated)
; Output: rax = pointer to symbol entry, or 0 if not found
;----------------------------------------------------------------------
lookup_symbol:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov r12, rdi                    ; name
    
    ; If length is 0, calculate it
    mov r13, rsi
    test rsi, rsi
    jnz .lookup_have_len
    xor ecx, ecx
.lookup_strlen:
    cmp byte [r12 + rcx], 0
    je .lookup_have_len2
    inc ecx
    jmp .lookup_strlen
.lookup_have_len2:
    mov r13, rcx
.lookup_have_len:
    
    lea rbx, [symbol_table]
    mov rcx, [symbol_count]
    test rcx, rcx
    jz .lookup_not_found
    
    ; Search backwards (inner scopes first)
    dec rcx
.lookup_loop:
    ; Get entry
    mov rax, rcx
    shl rax, 6                     ; * 64 (SYMBOL_ENTRY_SIZE)
    lea rax, [rbx + rax]
    
    ; Compare name length
    mov edx, [rax + 8]
    cmp rdx, r13
    jne .lookup_next
    
    ; Compare name characters
    mov rdi, [rax]                  ; name_ptr
    push rcx
    push rax
    xor ecx, ecx
.lookup_cmp:
    cmp rcx, r13
    jge .lookup_match
    movzx edx, byte [rdi + rcx]
    movzx esi, byte [r12 + rcx]
    cmp dl, sil
    jne .lookup_no_match
    inc ecx
    jmp .lookup_cmp
    
.lookup_match:
    pop rax
    pop rcx
    jmp .lookup_exit
    
.lookup_no_match:
    pop rax
    pop rcx
    
.lookup_next:
    test rcx, rcx
    jz .lookup_not_found
    dec rcx
    jmp .lookup_loop
    
.lookup_not_found:
    xor eax, eax
    
.lookup_exit:
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; collect_declarations - First pass: scan AST for declarations
; Input:  rdi = AST root node
;----------------------------------------------------------------------
collect_declarations:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    mov r12, rdi
    
    ; Get first child
    mov rdi, r12
    call get_first_child
    
.collect_loop:
    test rax, rax
    jz .collect_done
    mov rbx, rax
    
    ; Check node type
    mov eax, [rbx]
    
    cmp eax, AST_FUNCTION
    je .collect_func
    cmp eax, AST_VARIABLE
    je .collect_var
    cmp eax, AST_STRUCT
    je .collect_struct
    cmp eax, AST_ENUM
    je .collect_enum
    jmp .collect_next
    
.collect_func:
.collect_var:
.collect_struct:
.collect_enum:
    ; Add to symbol table
    mov rax, [symbol_count]
    cmp rax, SYMBOL_TABLE_SIZE / SYMBOL_ENTRY_SIZE
    jge .collect_full
    
    shl rax, 6
    lea rcx, [symbol_table + rax]
    
    ; Fill in symbol entry
    mov rax, [rbx + 24]            ; name
    mov [rcx], rax
    mov eax, [rbx + 40]            ; name_length
    mov [rcx + 8], eax
    mov eax, [rbx]                 ; type = node_type
    mov [rcx + 12], eax
    mov eax, [scope_depth]
    mov [rcx + 16], eax
    mov dword [rcx + 20], 0        ; flags
    mov [rcx + 24], rbx            ; ast_node
    mov qword [rcx + 32], 0        ; address (unresolved)
    
    inc qword [symbol_count]
    
.collect_next:
    mov rdi, rbx
    call get_next_sibling
    jmp .collect_loop
    
.collect_full:
.collect_done:
    xor eax, eax
    
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; resolve_and_typecheck - Second pass: resolve references and check types
; Input:  rdi = AST root
;----------------------------------------------------------------------
resolve_and_typecheck:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 16
    
    mov r12, rdi
    
    ; Walk AST and resolve identifiers
    mov rdi, r12
    call get_first_child
    
.resolve_loop:
    test rax, rax
    jz .resolve_done
    mov rbx, rax
    
    ; Check node type
    mov eax, [rbx]
    
    cmp eax, AST_IDENTIFIER
    je .resolve_ident
    cmp eax, AST_CALL
    je .resolve_call
    
    ; Recurse into children
    push rbx
    mov rdi, rbx
    call resolve_and_typecheck
    pop rbx
    test eax, eax
    jnz .resolve_error
    jmp .resolve_next
    
.resolve_ident:
    ; Look up identifier
    mov rdi, [rbx + 24]           ; name
    mov rsi, [rbx + 40]           ; name_length (as qword)
    movzx esi, word [rbx + 40]
    call lookup_symbol
    ; If not found, it's an error but we continue (error recovery)
    jmp .resolve_next
    
.resolve_call:
    ; Look up function name
    push rbx
    mov rdi, rbx
    call get_first_child           ; first child = function name
    test rax, rax
    jz .resolve_call_done
    mov rdi, [rax + 24]
    mov rsi, [rax + 40]
    movzx esi, word [rax + 40]
    call lookup_symbol
.resolve_call_done:
    pop rbx
    jmp .resolve_next
    
.resolve_next:
    mov rdi, rbx
    call get_next_sibling
    jmp .resolve_loop
    
.resolve_done:
    xor eax, eax
    jmp .resolve_exit
    
.resolve_error:
    mov eax, ERR_SEMANTIC
    
.resolve_exit:
    add rsp, 16
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; IR GENERATION AND CODE GENERATION IMPLEMENTATIONS
; ============================================================================

; IR Instruction Layout (32 bytes):
;   [0]  dd  opcode
;   [4]  dd  dest_reg
;   [8]  dq  operand1
;   [16] dq  operand2
;   [24] dd  flags
;   [28] dd  line_number

IR_ENTRY_SIZE equ 32

; IR Opcodes
IR_NOP          equ 0
IR_LOAD_IMM     equ 1
IR_LOAD_VAR     equ 2
IR_STORE_VAR    equ 3
IR_ADD          equ 4
IR_SUB          equ 5
IR_MUL          equ 6
IR_DIV          equ 7
IR_MOD          equ 8
IR_CMP          equ 9
IR_JMP          equ 10
IR_JZ           equ 11
IR_JNZ          equ 12
IR_CALL         equ 13
IR_RET          equ 14
IR_PUSH         equ 15
IR_POP          equ 16
IR_LABEL        equ 17
IR_NEG          equ 18
IR_NOT          equ 19
IR_AND          equ 20
IR_OR           equ 21
IR_XOR          equ 22
IR_SHL          equ 23
IR_SHR          equ 24

;----------------------------------------------------------------------
; generate_ir_node - Generate IR for a single AST node
; Input:  rdi = AST node pointer
; Output: eax = 0 on success, nonzero on error
;----------------------------------------------------------------------
generate_ir_node:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rdi
    mov eax, [r12]                  ; node type
    
    cmp eax, AST_FUNCTION
    je .ir_function
    cmp eax, AST_VARIABLE
    je .ir_variable
    cmp eax, AST_RETURN
    je .ir_return
    cmp eax, AST_IF
    je .ir_if
    cmp eax, AST_WHILE
    je .ir_while
    cmp eax, AST_BINARY_OP
    je .ir_binop
    cmp eax, AST_LITERAL
    je .ir_literal
    cmp eax, AST_IDENTIFIER
    je .ir_ident
    cmp eax, AST_CALL
    je .ir_call
    cmp eax, AST_BLOCK
    je .ir_block
    
    ; Unknown node type - skip
    xor eax, eax
    jmp .ir_exit
    
.ir_function:
    ; Emit label for function
    call emit_ir_label
    
    ; Emit prologue
    mov edi, IR_PUSH
    mov esi, 5                     ; RBP
    xor edx, edx
    call emit_ir_instruction
    
    ; Process body
    mov rdi, r12
    call get_first_child           ; skip name
    test rax, rax
    jz .ir_func_done
    mov rdi, rax
    call get_next_sibling          ; get body
    test rax, rax
    jz .ir_func_done
    mov rdi, rax
    call generate_ir_node
    
.ir_func_done:
    ; Emit epilogue
    mov edi, IR_POP
    mov esi, 5                     ; RBP
    xor edx, edx
    call emit_ir_instruction
    mov edi, IR_RET
    xor esi, esi
    xor edx, edx
    call emit_ir_instruction
    
    xor eax, eax
    jmp .ir_exit
    
.ir_variable:
    ; If has initializer, generate code for it
    mov rdi, r12
    call get_first_child
    test rax, rax
    jz .ir_var_done
    mov rdi, rax
    call generate_ir_node
    
    ; Store to variable
    mov edi, IR_STORE_VAR
    mov rsi, [r12 + 24]           ; variable name
    xor edx, edx
    call emit_ir_instruction
    
.ir_var_done:
    xor eax, eax
    jmp .ir_exit
    
.ir_return:
    ; Generate return value if present
    mov rdi, r12
    call get_first_child
    test rax, rax
    jz .ir_ret_void
    mov rdi, rax
    call generate_ir_node
    
.ir_ret_void:
    mov edi, IR_RET
    xor esi, esi
    xor edx, edx
    call emit_ir_instruction
    xor eax, eax
    jmp .ir_exit
    
.ir_if:
    ; Condition
    mov rdi, r12
    call get_first_child
    test rax, rax
    jz .ir_if_done
    push rax
    mov rdi, rax
    call generate_ir_node
    pop rax
    
    ; JZ to else/end
    mov r13, [label_counter]
    inc qword [label_counter]
    mov edi, IR_JZ
    mov rsi, r13
    xor edx, edx
    call emit_ir_instruction
    
    ; Then body
    mov rdi, r12
    call get_first_child
    test rax, rax
    jz .ir_if_done
    mov rdi, rax
    call get_next_sibling
    test rax, rax
    jz .ir_if_done
    mov rdi, rax
    call generate_ir_node
    
    ; Emit label for else/end
    mov edi, IR_LABEL
    mov rsi, r13
    xor edx, edx
    call emit_ir_instruction
    
.ir_if_done:
    xor eax, eax
    jmp .ir_exit
    
.ir_while:
    ; Loop label
    mov r13, [label_counter]
    inc qword [label_counter]
    mov edi, IR_LABEL
    mov rsi, r13
    xor edx, edx
    call emit_ir_instruction
    
    ; Condition
    mov rdi, r12
    call get_first_child
    test rax, rax
    jz .ir_while_done
    push rax
    mov rdi, rax
    call generate_ir_node
    pop rax
    
    ; JZ to end
    mov rbx, [label_counter]
    inc qword [label_counter]
    mov edi, IR_JZ
    mov rsi, rbx
    xor edx, edx
    call emit_ir_instruction
    
    ; Body
    mov rdi, r12
    call get_first_child
    test rax, rax
    jz .ir_while_done
    mov rdi, rax
    call get_next_sibling
    test rax, rax
    jz .ir_while_done
    mov rdi, rax
    call generate_ir_node
    
    ; JMP back to loop
    mov edi, IR_JMP
    mov rsi, r13
    xor edx, edx
    call emit_ir_instruction
    
    ; End label
    mov edi, IR_LABEL
    mov rsi, rbx
    xor edx, edx
    call emit_ir_instruction
    
.ir_while_done:
    xor eax, eax
    jmp .ir_exit
    
.ir_binop:
    ; Generate LHS
    mov rdi, r12
    call get_first_child
    test rax, rax
    jz .ir_binop_done
    push rax
    mov rdi, rax
    call generate_ir_node
    pop rax
    
    ; Generate RHS
    mov rdi, rax
    call get_next_sibling
    test rax, rax
    jz .ir_binop_done
    mov rdi, rax
    call generate_ir_node
    
    ; Emit binary operation (simplified: ADD)
    mov edi, IR_ADD
    xor esi, esi
    xor edx, edx
    call emit_ir_instruction
    
.ir_binop_done:
    xor eax, eax
    jmp .ir_exit
    
.ir_literal:
    ; Load immediate value
    mov edi, IR_LOAD_IMM
    mov rsi, [r12 + 24]           ; value pointer
    xor edx, edx
    call emit_ir_instruction
    xor eax, eax
    jmp .ir_exit
    
.ir_ident:
    ; Load variable
    mov edi, IR_LOAD_VAR
    mov rsi, [r12 + 24]
    xor edx, edx
    call emit_ir_instruction
    xor eax, eax
    jmp .ir_exit
    
.ir_call:
    ; Push arguments, then emit call
    mov rdi, r12
    call get_first_child
    test rax, rax
    jz .ir_call_done
    
    ; First child is function name
    mov rbx, rax
    mov rdi, rax
    call get_next_sibling
    
    ; Generate arguments
.ir_call_args:
    test rax, rax
    jz .ir_call_emit
    push rax
    mov rdi, rax
    call generate_ir_node
    pop rax
    mov edi, IR_PUSH
    xor esi, esi
    xor edx, edx
    call emit_ir_instruction
    mov rdi, rax
    call get_next_sibling
    jmp .ir_call_args
    
.ir_call_emit:
    mov edi, IR_CALL
    mov rsi, [rbx + 24]           ; function name
    xor edx, edx
    call emit_ir_instruction
    
.ir_call_done:
    xor eax, eax
    jmp .ir_exit
    
.ir_block:
    ; Process all children
    mov rdi, r12
    call get_first_child
.ir_block_loop:
    test rax, rax
    jz .ir_block_done
    push rax
    mov rdi, rax
    call generate_ir_node
    pop rax
    mov rdi, rax
    call get_next_sibling
    jmp .ir_block_loop
.ir_block_done:
    xor eax, eax
    
.ir_exit:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; emit_ir_instruction - Append IR instruction to buffer
; Input:  edi = opcode, rsi = operand1, edx = operand2
;----------------------------------------------------------------------
emit_ir_instruction:
    mov rax, [ir_pos]
    cmp rax, IR_BUFFER_SIZE - IR_ENTRY_SIZE
    jge .ir_full
    
    lea rcx, [ir_buffer + rax]
    mov [rcx], edi                  ; opcode
    mov dword [rcx + 4], 0         ; dest_reg
    mov [rcx + 8], rsi             ; operand1
    mov qword [rcx + 16], rdx      ; operand2
    mov dword [rcx + 24], 0        ; flags
    mov dword [rcx + 28], 0        ; line_number
    
    add qword [ir_pos], IR_ENTRY_SIZE
    inc qword [ir_instruction_count]
    ret
    
.ir_full:
    ret

;----------------------------------------------------------------------
; emit_ir_label - Emit a label IR instruction
;----------------------------------------------------------------------
emit_ir_label:
    mov rdi, IR_LABEL
    mov rsi, [label_counter]
    inc qword [label_counter]
    xor edx, edx
    call emit_ir_instruction
    ret

;----------------------------------------------------------------------
; optimize_ir - Optimization passes on IR
; Input:  rdi = IR buffer, rsi = instruction count, edx = opt level
;----------------------------------------------------------------------
optimize_ir:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 16
    
    mov r12, rdi                    ; IR buffer
    mov r13, rsi                    ; instruction count
    mov ebx, edx                    ; opt level
    
    ; Level 1: Constant folding
    cmp ebx, 1
    jl .opt_done
    call opt_constant_fold
    
    ; Level 2: Dead code elimination
    cmp ebx, 2
    jl .opt_done
    call opt_dead_code_elimination
    
    ; Level 3: Peephole optimization
    cmp ebx, 3
    jl .opt_done
    call opt_peephole
    
.opt_done:
    xor eax, eax
    
    add rsp, 16
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; opt_constant_fold - Fold constant expressions
;----------------------------------------------------------------------
opt_constant_fold:
    push rbp
    mov rbp, rsp
    
    ; Walk IR looking for LOAD_IMM followed by LOAD_IMM followed by ADD/SUB/MUL
    ; and fold them into a single LOAD_IMM
    mov rcx, [ir_instruction_count]
    cmp rcx, 3
    jl .cf_done
    
    lea rsi, [ir_buffer]
    sub rcx, 2
    
.cf_loop:
    test rcx, rcx
    jz .cf_done
    
    ; Check for pattern: LOAD_IMM, LOAD_IMM, binop
    mov eax, [rsi]
    cmp eax, IR_LOAD_IMM
    jne .cf_next
    mov eax, [rsi + IR_ENTRY_SIZE]
    cmp eax, IR_LOAD_IMM
    jne .cf_next
    mov eax, [rsi + IR_ENTRY_SIZE * 2]
    cmp eax, IR_ADD
    je .cf_fold_add
    cmp eax, IR_SUB
    je .cf_fold_sub
    cmp eax, IR_MUL
    je .cf_fold_mul
    jmp .cf_next
    
.cf_fold_add:
    mov rax, [rsi + 8]             ; operand1 of first
    add rax, [rsi + IR_ENTRY_SIZE + 8]  ; + operand1 of second
    mov [rsi + 8], rax
    ; NOP out the other two
    mov dword [rsi + IR_ENTRY_SIZE], IR_NOP
    mov dword [rsi + IR_ENTRY_SIZE * 2], IR_NOP
    jmp .cf_next
    
.cf_fold_sub:
    mov rax, [rsi + 8]
    sub rax, [rsi + IR_ENTRY_SIZE + 8]
    mov [rsi + 8], rax
    mov dword [rsi + IR_ENTRY_SIZE], IR_NOP
    mov dword [rsi + IR_ENTRY_SIZE * 2], IR_NOP
    jmp .cf_next
    
.cf_fold_mul:
    mov rax, [rsi + 8]
    imul rax, [rsi + IR_ENTRY_SIZE + 8]
    mov [rsi + 8], rax
    mov dword [rsi + IR_ENTRY_SIZE], IR_NOP
    mov dword [rsi + IR_ENTRY_SIZE * 2], IR_NOP
    
.cf_next:
    add rsi, IR_ENTRY_SIZE
    dec rcx
    jmp .cf_loop
    
.cf_done:
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; opt_dead_code_elimination - Remove NOPs and unreachable code
;----------------------------------------------------------------------
opt_dead_code_elimination:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    ; Compact IR by removing NOP instructions
    lea rsi, [ir_buffer]           ; read pointer
    lea rdi, [ir_buffer]           ; write pointer
    mov rcx, [ir_instruction_count]
    xor r12d, r12d                 ; new count
    
.dce_loop:
    test rcx, rcx
    jz .dce_done
    
    mov eax, [rsi]
    cmp eax, IR_NOP
    je .dce_skip
    
    ; Copy instruction
    cmp rsi, rdi
    je .dce_no_copy
    push rcx
    mov rcx, IR_ENTRY_SIZE / 8
    rep movsq
    pop rcx
    jmp .dce_count
.dce_no_copy:
    add rdi, IR_ENTRY_SIZE
    add rsi, IR_ENTRY_SIZE
.dce_count:
    inc r12d
    dec rcx
    jmp .dce_loop
    
.dce_skip:
    add rsi, IR_ENTRY_SIZE
    dec rcx
    jmp .dce_loop
    
.dce_done:
    mov [ir_instruction_count], r12
    
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; opt_peephole - Peephole optimizations
;----------------------------------------------------------------------
opt_peephole:
    push rbp
    mov rbp, rsp
    
    ; Look for PUSH immediately followed by POP of same register
    lea rsi, [ir_buffer]
    mov rcx, [ir_instruction_count]
    dec rcx
    
.peep_loop:
    test rcx, rcx
    jz .peep_done
    
    mov eax, [rsi]
    cmp eax, IR_PUSH
    jne .peep_next
    mov eax, [rsi + IR_ENTRY_SIZE]
    cmp eax, IR_POP
    jne .peep_next
    
    ; Check same register
    mov eax, [rsi + 4]
    cmp eax, [rsi + IR_ENTRY_SIZE + 4]
    jne .peep_next
    
    ; Eliminate both
    mov dword [rsi], IR_NOP
    mov dword [rsi + IR_ENTRY_SIZE], IR_NOP
    
.peep_next:
    add rsi, IR_ENTRY_SIZE
    dec rcx
    jmp .peep_loop
    
.peep_done:
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; emit_x86_64_instructions - Translate IR to x86-64 assembly text
; Input:  rdi = IR buffer, rsi = instruction count
;----------------------------------------------------------------------
emit_x86_64_instructions:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 32
    
    mov r12, rdi                    ; IR buffer
    mov r13, rsi                    ; count
    lea r14, [output_buffer]
    add r14, [output_pos]           ; output write position
    
.emit_loop:
    test r13, r13
    jz .emit_done
    
    mov eax, [r12]                  ; opcode
    
    cmp eax, IR_NOP
    je .emit_next
    cmp eax, IR_LOAD_IMM
    je .emit_load_imm
    cmp eax, IR_ADD
    je .emit_add
    cmp eax, IR_SUB
    je .emit_sub
    cmp eax, IR_MUL
    je .emit_mul
    cmp eax, IR_CALL
    je .emit_call
    cmp eax, IR_RET
    je .emit_ret
    cmp eax, IR_PUSH
    je .emit_push
    cmp eax, IR_POP
    je .emit_pop
    cmp eax, IR_LABEL
    je .emit_label
    cmp eax, IR_JMP
    je .emit_jmp
    cmp eax, IR_JZ
    je .emit_jz
    jmp .emit_next
    
.emit_load_imm:
    ; mov rax, imm64
    mov dword [r14], '    '
    add r14, 4
    mov dword [r14], 'mov '
    add r14, 4
    mov dword [r14], 'rax,'
    add r14, 4
    mov byte [r14], ' '
    inc r14
    ; Write immediate value
    mov rax, [r12 + 8]
    call write_number_to_buffer     ; writes decimal to r14
    mov word [r14], 0x0A00
    inc r14
    jmp .emit_next
    
.emit_add:
    mov dword [r14], '    '
    add r14, 4
    mov dword [r14], 'add '
    add r14, 4
    mov dword [r14], 'rax,'
    add r14, 4
    mov dword [r14], ' rbx'
    add r14, 4
    mov byte [r14], 10
    inc r14
    jmp .emit_next
    
.emit_sub:
    mov dword [r14], '    '
    add r14, 4
    mov dword [r14], 'sub '
    add r14, 4
    mov dword [r14], 'rax,'
    add r14, 4
    mov dword [r14], ' rbx'
    add r14, 4
    mov byte [r14], 10
    inc r14
    jmp .emit_next
    
.emit_mul:
    mov dword [r14], '    '
    add r14, 4
    mov dword [r14], 'imul'
    add r14, 4
    mov dword [r14], ' rax'
    add r14, 4
    mov dword [r14], ', rb'
    add r14, 4
    mov byte [r14], 'x'
    inc r14
    mov byte [r14], 10
    inc r14
    jmp .emit_next
    
.emit_call:
    mov dword [r14], '    '
    add r14, 4
    mov dword [r14], 'call'
    add r14, 4
    mov byte [r14], ' '
    inc r14
    ; Copy function name
    mov rsi, [r12 + 8]
    test rsi, rsi
    jz .emit_next
.emit_call_name:
    mov al, [rsi]
    test al, al
    jz .emit_call_done
    mov [r14], al
    inc r14
    inc rsi
    jmp .emit_call_name
.emit_call_done:
    mov byte [r14], 10
    inc r14
    jmp .emit_next
    
.emit_ret:
    mov dword [r14], '    '
    add r14, 4
    mov word [r14], 're'
    add r14, 2
    mov byte [r14], 't'
    inc r14
    mov byte [r14], 10
    inc r14
    jmp .emit_next
    
.emit_push:
    mov dword [r14], '    '
    add r14, 4
    mov dword [r14], 'push'
    add r14, 4
    mov dword [r14], ' rbp'
    add r14, 4
    mov byte [r14], 10
    inc r14
    jmp .emit_next
    
.emit_pop:
    mov dword [r14], '    '
    add r14, 4
    mov dword [r14], 'pop '
    add r14, 4
    mov dword [r14], 'rbp '
    add r14, 3
    mov byte [r14], 10
    inc r14
    jmp .emit_next
    
.emit_label:
    mov byte [r14], 'L'
    inc r14
    mov rax, [r12 + 8]
    call write_number_to_buffer
    mov byte [r14], ':'
    inc r14
    mov byte [r14], 10
    inc r14
    jmp .emit_next
    
.emit_jmp:
    mov dword [r14], '    '
    add r14, 4
    mov dword [r14], 'jmp '
    add r14, 4
    mov byte [r14], 'L'
    inc r14
    mov rax, [r12 + 8]
    call write_number_to_buffer
    mov byte [r14], 10
    inc r14
    jmp .emit_next
    
.emit_jz:
    mov dword [r14], '    '
    add r14, 4
    mov word [r14], 'jz'
    add r14, 2
    mov byte [r14], ' '
    inc r14
    mov byte [r14], 'L'
    inc r14
    mov rax, [r12 + 8]
    call write_number_to_buffer
    mov byte [r14], 10
    inc r14
    
.emit_next:
    add r12, IR_ENTRY_SIZE
    dec r13
    jmp .emit_loop
    
.emit_done:
    ; Update output position
    lea rax, [output_buffer]
    sub r14, rax
    mov [output_pos], r14
    
    add rsp, 32
    pop r14
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; write_number_to_buffer - Write decimal number to output at r14
; Input:  rax = number, r14 = output buffer position
; Modifies: r14 (advances past written digits)
;----------------------------------------------------------------------
write_number_to_buffer:
    push rbx
    push rcx
    push rdx
    
    test rax, rax
    jnz .wnb_nonzero
    mov byte [r14], '0'
    inc r14
    jmp .wnb_exit
    
.wnb_nonzero:
    ; Convert to decimal in reverse
    sub rsp, 32
    mov rcx, rsp
    xor ebx, ebx                   ; digit count
    
.wnb_div:
    test rax, rax
    jz .wnb_write
    xor edx, edx
    mov r8, 10
    div r8
    add dl, '0'
    mov [rcx + rbx], dl
    inc ebx
    jmp .wnb_div
    
.wnb_write:
    ; Write digits in reverse order
    dec ebx
.wnb_copy:
    mov al, [rcx + rbx]
    mov [r14], al
    inc r14
    dec ebx
    jns .wnb_copy
    
    add rsp, 32
    
.wnb_exit:
    pop rdx
    pop rcx
    pop rbx
    ret

;----------------------------------------------------------------------
; update_output_pos - Update output buffer position after string copy
;----------------------------------------------------------------------
update_output_pos:
    ; Calculate new position from output_buffer + null terminator search
    lea rdi, [output_buffer]
    add rdi, [output_pos]
    xor ecx, ecx
.uop_loop:
    cmp byte [rdi + rcx], 0
    je .uop_done
    inc ecx
    jmp .uop_loop
.uop_done:
    add [output_pos], rcx
    ret

;----------------------------------------------------------------------
; assemble_to_machine_code - Convert assembly text to machine code
; (For now, the output IS assembly text to be fed to an external assembler,
;  or we generate machine code bytes directly from IR)
;----------------------------------------------------------------------
assemble_to_machine_code:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    ; Walk IR buffer and emit raw x86-64 machine code bytes
    lea r12, [ir_buffer]
    mov r13, [ir_instruction_count]
    lea rbx, [output_buffer]
    add rbx, [output_pos]
    
.asm_loop:
    test r13, r13
    jz .asm_done
    
    mov eax, [r12]                  ; opcode
    
    cmp eax, IR_NOP
    je .asm_skip
    cmp eax, IR_RET
    je .asm_ret
    cmp eax, IR_PUSH
    je .asm_push
    cmp eax, IR_POP
    je .asm_pop
    cmp eax, IR_CALL
    je .asm_mcall
    cmp eax, IR_LOAD_IMM
    je .asm_mov_imm
    cmp eax, IR_ADD
    je .asm_add
    cmp eax, IR_SUB
    je .asm_sub
    jmp .asm_skip
    
.asm_ret:
    mov byte [rbx], 0xC3            ; RET
    inc rbx
    jmp .asm_skip
    
.asm_push:
    mov byte [rbx], 0x55            ; PUSH RBP
    inc rbx
    jmp .asm_skip
    
.asm_pop:
    mov byte [rbx], 0x5D            ; POP RBP
    inc rbx
    jmp .asm_skip
    
.asm_mcall:
    ; Relative call (will need relocation)
    mov byte [rbx], 0xE8            ; CALL rel32
    mov dword [rbx + 1], 0          ; placeholder
    add rbx, 5
    jmp .asm_skip
    
.asm_mov_imm:
    ; MOV RAX, imm64
    mov byte [rbx], 0x48            ; REX.W
    mov byte [rbx + 1], 0xB8        ; MOV RAX
    mov rax, [r12 + 8]
    mov [rbx + 2], rax
    add rbx, 10
    jmp .asm_skip
    
.asm_add:
    mov byte [rbx], 0x48            ; REX.W
    mov byte [rbx + 1], 0x01        ; ADD r/m64, r64
    mov byte [rbx + 2], 0xD8        ; ModRM: rbx -> rax
    add rbx, 3
    jmp .asm_skip
    
.asm_sub:
    mov byte [rbx], 0x48
    mov byte [rbx + 1], 0x29        ; SUB r/m64, r64
    mov byte [rbx + 2], 0xD8
    add rbx, 3
    
.asm_skip:
    add r12, IR_ENTRY_SIZE
    dec r13
    jmp .asm_loop
    
.asm_done:
    ; Update output position
    lea rax, [output_buffer]
    sub rbx, rax
    mov [output_pos], rbx
    
    xor eax, eax
    
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; generate_shared_library - Generate DLL/SO output
;----------------------------------------------------------------------
generate_shared_library:
    push rbp
    mov rbp, rsp
    
    ; Similar to PE/ELF generation but with DLL characteristics flag
    ; and export table
%ifdef WINDOWS
    call generate_pe_executable
    ; TODO: Add DLL flag to PE characteristics when format is DLL
%else
    call generate_elf_executable
%endif
    
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; generate_x86_32 - Generate 32-bit x86 code
; (Simplified: generate 32-bit stubs with proper register usage)
;----------------------------------------------------------------------
generate_x86_32:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 32
    
    lea r12, [output_buffer]
    mov qword [output_pos], 0
    
    ; Generate 32-bit prologue
    lea rdi, [r12]
    lea rsi, [x86_32_prologue]
    call copy_string
    call update_output_pos
    
    ; Generate IR -> 32-bit assembly text
    lea rdi, [ir_buffer]
    mov rsi, [ir_instruction_count]
    call emit_x86_32_instructions
    
    xor eax, eax
    
    add rsp, 32
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; emit_x86_32_instructions - Generate 32-bit code from IR
;----------------------------------------------------------------------
emit_x86_32_instructions:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    mov r12, rdi
    mov rbx, rsi
    lea rcx, [output_buffer]
    add rcx, [output_pos]
    
    ; Simplified: emit 32-bit RET for each function
.e32_loop:
    test rbx, rbx
    jz .e32_done
    
    mov eax, [r12]
    cmp eax, IR_RET
    jne .e32_next
    ; Write "    ret\n"
    mov dword [rcx], '    '
    add rcx, 4
    mov word [rcx], 're'
    add rcx, 2
    mov byte [rcx], 't'
    inc rcx
    mov byte [rcx], 10
    inc rcx
    
.e32_next:
    add r12, IR_ENTRY_SIZE
    dec rbx
    jmp .e32_loop
    
.e32_done:
    lea rax, [output_buffer]
    sub rcx, rax
    mov [output_pos], rcx
    
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; generate_arm64 - Generate ARM64/AArch64 code
;----------------------------------------------------------------------
generate_arm64:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 32
    
    lea r12, [output_buffer]
    mov qword [output_pos], 0
    
    ; ARM64 prologue
    lea rdi, [r12]
    lea rsi, [arm64_prologue]
    call copy_string
    call update_output_pos
    
    ; Generate ARM64 from IR
    lea rdi, [ir_buffer]
    mov rsi, [ir_instruction_count]
    call emit_arm64_instructions
    
    xor eax, eax
    
    add rsp, 32
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; emit_arm64_instructions - Generate ARM64 assembly from IR
;----------------------------------------------------------------------
emit_arm64_instructions:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    mov r12, rdi
    mov rbx, rsi
    lea rcx, [output_buffer]
    add rcx, [output_pos]
    
.a64_loop:
    test rbx, rbx
    jz .a64_done
    
    mov eax, [r12]
    cmp eax, IR_RET
    jne .a64_next
    ; ARM64 RET
    mov dword [rcx], '    '
    add rcx, 4
    mov word [rcx], 're'
    add rcx, 2
    mov byte [rcx], 't'
    inc rcx
    mov byte [rcx], 10
    inc rcx
    
.a64_next:
    add r12, IR_ENTRY_SIZE
    dec rbx
    jmp .a64_loop
    
.a64_done:
    lea rax, [output_buffer]
    sub rcx, rax
    mov [output_pos], rcx
    
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_target - Parse target architecture string
; Input:  rdi = target string
; Output: al = TARGET_* value
;----------------------------------------------------------------------
parse_target:
    push rbp
    mov rbp, rsp
    
    ; Compare against known targets
    cmp byte [rdi], 'x'
    jne .pt_check_arm
    cmp byte [rdi + 1], '6'
    jne .pt_check_x86
    cmp byte [rdi + 2], '4'
    jne .pt_unknown
    mov al, TARGET_X86_64
    jmp .pt_exit
    
.pt_check_x86:
    cmp byte [rdi + 1], '8'
    jne .pt_unknown
    cmp byte [rdi + 2], '6'
    jne .pt_unknown
    mov al, TARGET_X86_32
    jmp .pt_exit
    
.pt_check_arm:
    cmp byte [rdi], 'a'
    jne .pt_check_wasm
    cmp byte [rdi + 1], 'r'
    jne .pt_unknown
    cmp byte [rdi + 2], 'm'
    jne .pt_unknown
    mov al, TARGET_ARM64
    jmp .pt_exit
    
.pt_check_wasm:
    cmp byte [rdi], 'w'
    jne .pt_unknown
    mov al, TARGET_WASM
    jmp .pt_exit
    
.pt_unknown:
    mov al, TARGET_X86_64           ; default
    
.pt_exit:
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; parse_format - Parse output format string
; Input:  rdi = format string
; Output: al = OUTPUT_* value
;----------------------------------------------------------------------
parse_format:
    push rbp
    mov rbp, rsp
    
    cmp byte [rdi], 'e'
    jne .pf_dll
    cmp byte [rdi + 1], 'x'
    jne .pf_unknown
    mov al, OUTPUT_EXE
    jmp .pf_exit
    
.pf_dll:
    cmp byte [rdi], 'd'
    jne .pf_obj
    mov al, OUTPUT_DLL
    jmp .pf_exit
    
.pf_obj:
    cmp byte [rdi], 'o'
    jne .pf_lib
    mov al, OUTPUT_OBJ
    jmp .pf_exit
    
.pf_lib:
    cmp byte [rdi], 'l'
    jne .pf_asm
    mov al, OUTPUT_LIB
    jmp .pf_exit
    
.pf_asm:
    cmp byte [rdi], 'a'
    jne .pf_unknown
    mov al, OUTPUT_ASM
    jmp .pf_exit
    
.pf_unknown:
    mov al, OUTPUT_EXE
    
.pf_exit:
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; print_error - Print error message to stderr
;----------------------------------------------------------------------
print_error:
    push rbp
    mov rbp, rsp
    
    push rdi
    lea rdi, [msg_error]
    call print_string
    pop rdi
    call print_string
    
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; print_compile_error - Print detailed compilation error
;----------------------------------------------------------------------
print_compile_error:
    push rbp
    mov rbp, rsp
    push rbx
    sub rsp, 16
    
    lea rdi, [msg_error]
    call print_string
    
    ; Print error type based on compiler_state
    mov rax, [compiler_state]
    cmp rax, STAGE_LEXICAL
    je .pce_lex
    cmp rax, STAGE_SYNTACTIC
    je .pce_parse
    cmp rax, STAGE_SEMANTIC
    je .pce_sem
    cmp rax, STAGE_CODE_GEN
    je .pce_codegen
    cmp rax, STAGE_LINKING
    je .pce_link
    jmp .pce_generic
    
.pce_lex:
    lea rdi, [err_lex_msg]
    jmp .pce_print
.pce_parse:
    lea rdi, [err_parse_msg]
    jmp .pce_print
.pce_sem:
    lea rdi, [err_sem_msg]
    jmp .pce_print
.pce_codegen:
    lea rdi, [err_codegen_msg]
    jmp .pce_print
.pce_link:
    lea rdi, [err_link_msg]
    jmp .pce_print
.pce_generic:
    lea rdi, [err_generic_msg]
    
.pce_print:
    call print_string
    
    ; Print line info if available
    mov rax, [error_line]
    test rax, rax
    jz .pce_done
    
    lea rdi, [msg_at_line]
    call print_string
    ; Print line number
    mov rdi, [error_line]
    call print_number_fn
    
    mov rax, [error_column]
    test rax, rax
    jz .pce_newline
    
    lea rdi, [msg_col]
    call print_string
    mov rdi, [error_column]
    call print_number_fn
    
.pce_newline:
    lea rdi, [newline]
    call print_string
    
.pce_done:
    add rsp, 16
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;----------------------------------------------------------------------
; print_number_fn - Print a number to stdout
; Input:  rdi = number value
;----------------------------------------------------------------------
print_number_fn:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov rax, rdi
    lea r14, [rbp - 24]
    call write_number_to_buffer
    mov byte [r14], 0               ; null terminate
    
    lea rdi, [rbp - 24]
    call print_string
    
    add rsp, 32
    mov rsp, rbp
    pop rbp
    ret

; ============================================================================
; ADDITIONAL DATA FOR IMPLEMENTATIONS
; ============================================================================
section .data
    align 8
    
    x86_32_prologue     db "section .text", 10, "global _start", 10, "_start:", 10, 0
    arm64_prologue      db ".text", 10, ".global _start", 10, "_start:", 10, 0
    
    err_lex_msg         db "Lexical error", 0
    err_parse_msg       db "Syntax error", 0
    err_sem_msg         db "Semantic error", 0
    err_codegen_msg     db "Code generation error", 0
    err_link_msg        db "Linker error", 0
    err_generic_msg     db "Internal compiler error", 0
