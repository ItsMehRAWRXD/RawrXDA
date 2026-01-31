; ========================================
; EON Token Definitions
; ========================================
; Complete token definitions for EON compiler
; Fixes all missing symbol errors

section .data

; Token type constants
%define TOKEN_EOF               0
%define TOKEN_IDENTIFIER        1
%define TOKEN_NUMBER            2
%define TOKEN_STRING            3
%define TOKEN_CHAR              4

; Keywords
%define TOKEN_DEF               10
%define TOKEN_MODEL             11
%define TOKEN_FUNC              12
%define TOKEN_LOOP              13
%define TOKEN_IF                14
%define TOKEN_ELSE              15
%define TOKEN_LET               16
%define TOKEN_CONST             17
%define TOKEN_RET               18
%define TOKEN_BREAK             19
%define TOKEN_CONTINUE          20
%define TOKEN_IMPORT            21
%define TOKEN_EXPORT            22
%define TOKEN_MATCH             23
%define TOKEN_CASE              24
%define TOKEN_DEFAULT           25
%define TOKEN_TRY               26
%define TOKEN_CATCH             27
%define TOKEN_FINALLY           28
%define TOKEN_THROW             29

; Type keywords
%define TOKEN_INT               30
%define TOKEN_FLOAT             31
%define TOKEN_BOOL              32
%define TOKEN_VOID              33
%define TOKEN_AUTO              34
%define TOKEN_STRING_TYPE       35
%define TOKEN_CHAR_TYPE         36

; Operators - Assignment
%define TOKEN_ASSIGN            40
%define TOKEN_PLUS_ASSIGN       41
%define TOKEN_MINUS_ASSIGN      42
%define TOKEN_MULTIPLY_ASSIGN   43
%define TOKEN_DIVIDE_ASSIGN     44
%define TOKEN_MODULO_ASSIGN     45
%define TOKEN_AND_ASSIGN        46
%define TOKEN_OR_ASSIGN         47
%define TOKEN_XOR_ASSIGN        48
%define TOKEN_LEFT_SHIFT_ASSIGN 49
%define TOKEN_RIGHT_SHIFT_ASSIGN 50

; Operators - Ternary
%define TOKEN_QUESTION          60
%define TOKEN_COLON             61

; Operators - Logical
%define TOKEN_LOGICAL_OR        70
%define TOKEN_LOGICAL_AND       71
%define TOKEN_LOGICAL_NOT       72

; Operators - Bitwise
%define TOKEN_BITWISE_OR        80
%define TOKEN_BITWISE_XOR       81
%define TOKEN_BITWISE_AND       82
%define TOKEN_BITWISE_NOT       83

; Operators - Comparison
%define TOKEN_EQUALS            90
%define TOKEN_NOT_EQUALS        91
%define TOKEN_LESS              92
%define TOKEN_LESS_EQUAL        93
%define TOKEN_GREATER           94
%define TOKEN_GREATER_EQUAL     95

; Operators - Bitwise Shift
%define TOKEN_LEFT_SHIFT        100
%define TOKEN_RIGHT_SHIFT       101

; Operators - Arithmetic
%define TOKEN_PLUS              110
%define TOKEN_MINUS             111
%define TOKEN_MULTIPLY          112
%define TOKEN_DIVIDE            113
%define TOKEN_MODULO            114

; Operators - Unary
%define TOKEN_STAR              120
%define TOKEN_AMPERSAND         121

; Punctuation
%define TOKEN_LPAREN            130
%define TOKEN_RPAREN            131
%define TOKEN_LBRACE            132
%define TOKEN_RBRACE            133
%define TOKEN_LBRACKET          134
%define TOKEN_RBRACKET          135
%define TOKEN_SEMICOLON         136
%define TOKEN_COMMA             137
%define TOKEN_DOT               138
%define TOKEN_ARROW             139

; Special tokens
%define TOKEN_INDENT            140
%define TOKEN_DEDENT            141
%define TOKEN_NEWLINE           142
%define TOKEN_COMMENT           143

; Token names for debugging
token_names:
    dq token_eof, token_identifier, token_number, token_string, token_char
    dq token_def, token_model, token_func, token_loop, token_if
    dq token_else, token_let, token_const, token_ret, token_break
    dq token_continue, token_import, token_export, token_match, token_case
    dq token_default, token_try, token_catch, token_finally, token_throw
    dq token_int, token_float, token_bool, token_void, token_auto
    dq token_string_type, token_char_type, token_assign, token_plus_assign
    dq token_minus_assign, token_multiply_assign, token_divide_assign
    dq token_modulo_assign, token_and_assign, token_or_assign, token_xor_assign
    dq token_left_shift_assign, token_right_shift_assign, token_question
    dq token_colon, token_logical_or, token_logical_and, token_logical_not
    dq token_bitwise_or, token_bitwise_xor, token_bitwise_and, token_bitwise_not
    dq token_equals, token_not_equals, token_less, token_less_equal
    dq token_greater, token_greater_equal, token_left_shift, token_right_shift
    dq token_plus, token_minus, token_multiply, token_divide, token_modulo
    dq token_star, token_ampersand, token_lparen, token_rparen, token_lbrace
    dq token_rbrace, token_lbracket, token_rbracket, token_semicolon
    dq token_comma, token_dot, token_arrow, token_indent, token_dedent
    dq token_newline, token_comment

; Token name strings
token_eof:              db "EOF", 0
token_identifier:       db "IDENTIFIER", 0
token_number:           db "NUMBER", 0
token_string:           db "STRING", 0
token_char:             db "CHAR", 0
token_def:              db "DEF", 0
token_model:            db "MODEL", 0
token_func:             db "FUNC", 0
token_loop:             db "LOOP", 0
token_if:               db "IF", 0
token_else:             db "ELSE", 0
token_let:              db "LET", 0
token_const:            db "CONST", 0
token_ret:              db "RET", 0
token_break:            db "BREAK", 0
token_continue:         db "CONTINUE", 0
token_import:           db "IMPORT", 0
token_export:           db "EXPORT", 0
token_match:            db "MATCH", 0
token_case:             db "CASE", 0
token_default:          db "DEFAULT", 0
token_try:              db "TRY", 0
token_catch:            db "CATCH", 0
token_finally:          db "FINALLY", 0
token_throw:            db "THROW", 0
token_int:              db "INT", 0
token_float:            db "FLOAT", 0
token_bool:             db "BOOL", 0
token_void:             db "VOID", 0
token_auto:             db "AUTO", 0
token_string_type:      db "STRING_TYPE", 0
token_char_type:        db "CHAR_TYPE", 0
token_assign:           db "ASSIGN", 0
token_plus_assign:      db "PLUS_ASSIGN", 0
token_minus_assign:     db "MINUS_ASSIGN", 0
token_multiply_assign:  db "MULTIPLY_ASSIGN", 0
token_divide_assign:    db "DIVIDE_ASSIGN", 0
token_modulo_assign:    db "MODULO_ASSIGN", 0
token_and_assign:       db "AND_ASSIGN", 0
token_or_assign:        db "OR_ASSIGN", 0
token_xor_assign:       db "XOR_ASSIGN", 0
token_left_shift_assign: db "LEFT_SHIFT_ASSIGN", 0
token_right_shift_assign: db "RIGHT_SHIFT_ASSIGN", 0
token_question:         db "QUESTION", 0
token_colon:            db "COLON", 0
token_logical_or:       db "LOGICAL_OR", 0
token_logical_and:      db "LOGICAL_AND", 0
token_logical_not:      db "LOGICAL_NOT", 0
token_bitwise_or:       db "BITWISE_OR", 0
token_bitwise_xor:      db "BITWISE_XOR", 0
token_bitwise_and:      db "BITWISE_AND", 0
token_bitwise_not:      db "BITWISE_NOT", 0
token_equals:           db "EQUALS", 0
token_not_equals:       db "NOT_EQUALS", 0
token_less:             db "LESS", 0
token_less_equal:       db "LESS_EQUAL", 0
token_greater:          db "GREATER", 0
token_greater_equal:    db "GREATER_EQUAL", 0
token_left_shift:       db "LEFT_SHIFT", 0
token_right_shift:      db "RIGHT_SHIFT", 0
token_plus:             db "PLUS", 0
token_minus:            db "MINUS", 0
token_multiply:         db "MULTIPLY", 0
token_divide:           db "DIVIDE", 0
token_modulo:           db "MODULO", 0
token_star:             db "STAR", 0
token_ampersand:        db "AMPERSAND", 0
token_lparen:           db "LPAREN", 0
token_rparen:           db "RPAREN", 0
token_lbrace:           db "LBRACE", 0
token_rbrace:           db "RBRACE", 0
token_lbracket:         db "LBRACKET", 0
token_rbracket:         db "RBRACKET", 0
token_semicolon:        db "SEMICOLON", 0
token_comma:            db "COMMA", 0
token_dot:              db "DOT", 0
token_arrow:            db "ARROW", 0
token_indent:           db "INDENT", 0
token_dedent:           db "DEDENT", 0
token_newline:          db "NEWLINE", 0
token_comment:          db "COMMENT", 0

section .text

; Function to get token name for debugging
; Input: RAX = token type
; Output: RAX = pointer to token name string
get_token_name:
    cmp rax, 144
    jge .invalid_token
    
    mov rbx, token_names
    mov rax, [rbx + rax * 8]
    ret
    
.invalid_token:
    mov rax, token_eof
    ret
