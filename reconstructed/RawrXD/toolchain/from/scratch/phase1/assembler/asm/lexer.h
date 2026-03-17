/*==========================================================================
 * asm_lexer.h — Assembly source tokenizer
 *
 * Tokenizes Intel-syntax x64 assembly into a stream of tokens.
 * Supports: identifiers, numbers (decimal/hex/octal/binary), strings,
 * registers, mnemonics, directives, labels, punctuation.
 *
 * Line-oriented: each call to asm_lex_line() tokenizes one line.
 *=========================================================================*/
#ifndef ASM_LEXER_H
#define ASM_LEXER_H

#include <stdint.h>
#include <stdio.h>

/* ---- Token types ---- */
typedef enum {
    TOK_EOF = 0,
    TOK_NEWLINE,
    TOK_IDENT,        /* symbol name, mnemonic, directive */
    TOK_NUMBER,        /* numeric literal */
    TOK_STRING,        /* "..." or '...' */
    TOK_REGISTER,      /* register name (rax, ecx, etc.) */
    TOK_COMMA,         /* , */
    TOK_COLON,         /* : (label suffix) */
    TOK_LBRACKET,      /* [ */
    TOK_RBRACKET,      /* ] */
    TOK_PLUS,          /* + */
    TOK_MINUS,         /* - */
    TOK_STAR,          /* * */
    TOK_DOT,           /* . (for directives: .text, .data) */
    TOK_DOLLAR,        /* $ (current address) */
    TOK_HASH,          /* # (comment in some syntaxes) */
    TOK_DIRECTIVE,     /* assembler directive (db, dw, dd, dq, etc.) */
    TOK_SIZE_HINT,     /* BYTE, WORD, DWORD, QWORD */
    TOK_PTR,           /* PTR keyword */
    TOK_SECTION,       /* section/segment directive */
    TOK_EXTERN,        /* extern/extrn declaration */
    TOK_GLOBAL,        /* global/public declaration */
    TOK_ERROR
} asm_token_type_t;

/* ---- Size hint values ---- */
typedef enum {
    HINT_NONE = 0,
    HINT_BYTE = 1,
    HINT_WORD = 2,
    HINT_DWORD = 4,
    HINT_QWORD = 8,
    HINT_TBYTE = 10,
    HINT_OWORD = 16,
    HINT_YWORD = 32
} size_hint_t;

/* ---- Token ---- */
typedef struct {
    asm_token_type_t type;
    char             text[256];   /* raw token text */
    int64_t          num_val;     /* if TOK_NUMBER */
    size_hint_t      hint_val;    /* if TOK_SIZE_HINT */
    int              line;        /* source line number */
    int              col;         /* source column */
} asm_token_t;

/* ---- Token list (dynamic array) ---- */
typedef struct {
    asm_token_t *tokens;
    int          count;
    int          capacity;
} asm_token_list_t;

/* ---- API ---- */
asm_token_list_t *asm_lex_file(const char *filename);
asm_token_list_t *asm_lex_string(const char *source);
void asm_token_list_free(asm_token_list_t *list);
void asm_token_dump(const asm_token_t *tok);

#endif
