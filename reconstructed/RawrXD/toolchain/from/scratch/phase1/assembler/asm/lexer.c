/*==========================================================================
 * asm_lexer.c — Intel-syntax assembly tokenizer
 *
 * Scans assembly source character-by-character, producing tokens.
 * Handles:
 *   - Line comments (;)
 *   - Hex (0x/0h suffix), decimal, octal (0o), binary (0b)
 *   - String literals (single and double quoted)
 *   - Register names (auto-classified)
 *   - Size hints (BYTE, WORD, DWORD, QWORD)
 *   - Directives (db, dw, dd, dq, section, segment, extern, global, etc.)
 *   - Labels (identifier followed by colon)
 *=========================================================================*/
#include "asm_lexer.h"
#include "x64_encoder.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- Token list management ---- */
static asm_token_list_t *token_list_new(void) {
    asm_token_list_t *l = (asm_token_list_t *)calloc(1, sizeof(asm_token_list_t));
    l->capacity = 256;
    l->tokens = (asm_token_t *)calloc((size_t)l->capacity, sizeof(asm_token_t));
    return l;
}

static void token_list_add(asm_token_list_t *l, asm_token_t *tok) {
    if (l->count >= l->capacity) {
        l->capacity *= 2;
        l->tokens = (asm_token_t *)realloc(l->tokens, (size_t)l->capacity * sizeof(asm_token_t));
    }
    l->tokens[l->count++] = *tok;
}

void asm_token_list_free(asm_token_list_t *list) {
    if (!list) return;
    free(list->tokens);
    free(list);
}

/* ---- Classification helpers ---- */
static int is_ident_start(char c) { return isalpha((unsigned char)c) || c == '_' || c == '.'; }
static int is_ident_char(char c) { return isalnum((unsigned char)c) || c == '_' || c == '.' || c == '$'; }

/* Check if identifier is a register name */
static int is_register(const char *s) {
    operand_size_t sz;
    return x64_parse_register(s, &sz) != REG_NONE;
}

/* Check if identifier is a size hint */
static size_hint_t check_size_hint(const char *s) {
    char lower[32];
    int i;
    for (i = 0; s[i] && i < 31; i++) lower[i] = (char)tolower((unsigned char)s[i]);
    lower[i] = '\0';
    if (strcmp(lower, "byte") == 0)  return HINT_BYTE;
    if (strcmp(lower, "word") == 0)  return HINT_WORD;
    if (strcmp(lower, "dword") == 0) return HINT_DWORD;
    if (strcmp(lower, "qword") == 0) return HINT_QWORD;
    if (strcmp(lower, "tbyte") == 0) return HINT_TBYTE;
    if (strcmp(lower, "oword") == 0) return HINT_OWORD;
    if (strcmp(lower, "yword") == 0) return HINT_YWORD;
    return HINT_NONE;
}

/* Check if identifier is a data directive */
static int check_directive(const char *s) {
    char lower[32];
    int i;
    for (i = 0; s[i] && i < 31; i++) lower[i] = (char)tolower((unsigned char)s[i]);
    lower[i] = '\0';
    if (strcmp(lower, "db") == 0 || strcmp(lower, "dw") == 0 ||
        strcmp(lower, "dd") == 0 || strcmp(lower, "dq") == 0 ||
        strcmp(lower, "dt") == 0 || strcmp(lower, "resb") == 0 ||
        strcmp(lower, "resw") == 0 || strcmp(lower, "resd") == 0 ||
        strcmp(lower, "resq") == 0 || strcmp(lower, "align") == 0 ||
        strcmp(lower, "times") == 0 || strcmp(lower, "equ") == 0 ||
        strcmp(lower, "incbin") == 0 || strcmp(lower, "bits") == 0 ||
        strcmp(lower, "org") == 0 || strcmp(lower, "proc") == 0 ||
        strcmp(lower, "endp") == 0 || strcmp(lower, "end") == 0)
        return 1;
    return 0;
}

/* Check section/segment keywords */
static int check_section(const char *s) {
    char lower[32];
    int i;
    for (i = 0; s[i] && i < 31; i++) lower[i] = (char)tolower((unsigned char)s[i]);
    lower[i] = '\0';
    return strcmp(lower, "section") == 0 || strcmp(lower, "segment") == 0 ||
           strcmp(lower, ".code") == 0 || strcmp(lower, ".data") == 0 ||
           strcmp(lower, ".data?") == 0 || strcmp(lower, ".const") == 0;
}

static int check_extern(const char *s) {
    char lower[32];
    int i;
    for (i = 0; s[i] && i < 31; i++) lower[i] = (char)tolower((unsigned char)s[i]);
    lower[i] = '\0';
    return strcmp(lower, "extern") == 0 || strcmp(lower, "extrn") == 0 ||
           strcmp(lower, "externdef") == 0;
}

static int check_global(const char *s) {
    char lower[32];
    int i;
    for (i = 0; s[i] && i < 31; i++) lower[i] = (char)tolower((unsigned char)s[i]);
    lower[i] = '\0';
    return strcmp(lower, "global") == 0 || strcmp(lower, "public") == 0;
}

static int check_ptr(const char *s) {
    char lower[32];
    int i;
    for (i = 0; s[i] && i < 31; i++) lower[i] = (char)tolower((unsigned char)s[i]);
    lower[i] = '\0';
    return strcmp(lower, "ptr") == 0;
}

/* ---- Parse a number ---- */
static int64_t parse_number(const char *s, int len) {
    char buf[256];
    if (len > 255) len = 255;
    memcpy(buf, s, (size_t)len);
    buf[len] = '\0';

    /* MASM-style hex suffix: 0ABCDh */
    if (len > 1 && (buf[len-1] == 'h' || buf[len-1] == 'H')) {
        buf[len-1] = '\0';
        return (int64_t)strtoull(buf, NULL, 16);
    }
    /* MASM-style binary suffix: 10101b */
    if (len > 1 && (buf[len-1] == 'b' || buf[len-1] == 'B') &&
        buf[0] >= '0' && buf[0] <= '1') {
        buf[len-1] = '\0';
        return (int64_t)strtoull(buf, NULL, 2);
    }
    /* C-style prefixes */
    if (len > 2 && buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X'))
        return (int64_t)strtoull(buf + 2, NULL, 16);
    if (len > 2 && buf[0] == '0' && (buf[1] == 'b' || buf[1] == 'B'))
        return (int64_t)strtoull(buf + 2, NULL, 2);
    if (len > 2 && buf[0] == '0' && (buf[1] == 'o' || buf[1] == 'O'))
        return (int64_t)strtoull(buf + 2, NULL, 8);

    return (int64_t)strtoll(buf, NULL, 10);
}

/* ============================================================
 * Main lexer — tokenize entire source string
 * ============================================================ */
asm_token_list_t *asm_lex_string(const char *source) {
    asm_token_list_t *list = token_list_new();
    const char *p = source;
    int line = 1, col = 1;

    while (*p) {
        /* Skip whitespace (not newline) */
        if (*p == ' ' || *p == '\t' || *p == '\r') {
            if (*p == '\t') col = ((col - 1) / 4 + 1) * 4 + 1;
            else col++;
            p++;
            continue;
        }

        /* Newline */
        if (*p == '\n') {
            asm_token_t tok = {0};
            tok.type = TOK_NEWLINE;
            tok.line = line;
            tok.col = col;
            strcpy(tok.text, "\\n");
            token_list_add(list, &tok);
            line++;
            col = 1;
            p++;
            continue;
        }

        /* Comment — skip to end of line */
        if (*p == ';') {
            while (*p && *p != '\n') p++;
            continue;
        }
        /* C-style line comment */
        if (*p == '/' && *(p+1) == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }

        /* String literal */
        if (*p == '"' || *p == '\'') {
            char delim = *p;
            asm_token_t tok = {0};
            tok.type = TOK_STRING;
            tok.line = line;
            tok.col = col;
            p++; col++;
            int i = 0;
            while (*p && *p != delim && *p != '\n' && i < 254) {
                if (*p == '\\' && *(p+1)) {
                    p++; col++;
                    switch (*p) {
                        case 'n': tok.text[i++] = '\n'; break;
                        case 'r': tok.text[i++] = '\r'; break;
                        case 't': tok.text[i++] = '\t'; break;
                        case '0': tok.text[i++] = '\0'; break;
                        case '\\': tok.text[i++] = '\\'; break;
                        default: tok.text[i++] = *p; break;
                    }
                } else {
                    tok.text[i++] = *p;
                }
                p++; col++;
            }
            tok.text[i] = '\0';
            if (*p == delim) { p++; col++; }
            token_list_add(list, &tok);
            continue;
        }

        /* Number: starts with digit, or hex like 0ABCDh */
        if (isdigit((unsigned char)*p)) {
            asm_token_t tok = {0};
            tok.type = TOK_NUMBER;
            tok.line = line;
            tok.col = col;
            const char *start = p;
            /* Consume digits + hex chars + suffix */
            while (isalnum((unsigned char)*p) || *p == '_') { p++; col++; }
            int numlen = (int)(p - start);
            if (numlen > 255) numlen = 255;
            memcpy(tok.text, start, (size_t)numlen);
            tok.text[numlen] = '\0';
            tok.num_val = parse_number(start, numlen);
            token_list_add(list, &tok);
            continue;
        }

        /* Identifier / keyword / register / directive */
        if (is_ident_start(*p)) {
            asm_token_t tok = {0};
            tok.line = line;
            tok.col = col;
            const char *start = p;
            while (is_ident_char(*p)) { p++; col++; }
            int idlen = (int)(p - start);
            if (idlen > 255) idlen = 255;
            memcpy(tok.text, start, (size_t)idlen);
            tok.text[idlen] = '\0';

            /* Classify */
            size_hint_t hint = check_size_hint(tok.text);
            if (hint != HINT_NONE) {
                tok.type = TOK_SIZE_HINT;
                tok.hint_val = hint;
            } else if (check_ptr(tok.text)) {
                tok.type = TOK_PTR;
            } else if (check_section(tok.text)) {
                tok.type = TOK_SECTION;
            } else if (check_extern(tok.text)) {
                tok.type = TOK_EXTERN;
            } else if (check_global(tok.text)) {
                tok.type = TOK_GLOBAL;
            } else if (is_register(tok.text)) {
                tok.type = TOK_REGISTER;
            } else if (check_directive(tok.text)) {
                tok.type = TOK_DIRECTIVE;
            } else {
                tok.type = TOK_IDENT;
            }
            token_list_add(list, &tok);
            continue;
        }

        /* Single-character tokens */
        {
            asm_token_t tok = {0};
            tok.line = line;
            tok.col = col;
            tok.text[0] = *p;
            tok.text[1] = '\0';
            switch (*p) {
                case ',': tok.type = TOK_COMMA; break;
                case ':': tok.type = TOK_COLON; break;
                case '[': tok.type = TOK_LBRACKET; break;
                case ']': tok.type = TOK_RBRACKET; break;
                case '+': tok.type = TOK_PLUS; break;
                case '-': tok.type = TOK_MINUS; break;
                case '*': tok.type = TOK_STAR; break;
                case '$': tok.type = TOK_DOLLAR; break;
                case '#': tok.type = TOK_HASH; break;
                default:
                    tok.type = TOK_ERROR;
                    break;
            }
            token_list_add(list, &tok);
            p++; col++;
        }
    }

    /* EOF token */
    {
        asm_token_t tok = {0};
        tok.type = TOK_EOF;
        tok.line = line;
        tok.col = col;
        strcpy(tok.text, "<eof>");
        token_list_add(list, &tok);
    }

    return list;
}

/* ---- Lex from file ---- */
asm_token_list_t *asm_lex_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)(sz + 1));
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);

    asm_token_list_t *result = asm_lex_string(buf);
    free(buf);
    return result;
}

/* ---- Debug dump ---- */
static const char *tok_type_name(asm_token_type_t t) {
    switch (t) {
        case TOK_EOF:       return "EOF";
        case TOK_NEWLINE:   return "NEWLINE";
        case TOK_IDENT:     return "IDENT";
        case TOK_NUMBER:    return "NUMBER";
        case TOK_STRING:    return "STRING";
        case TOK_REGISTER:  return "REGISTER";
        case TOK_COMMA:     return "COMMA";
        case TOK_COLON:     return "COLON";
        case TOK_LBRACKET:  return "LBRACKET";
        case TOK_RBRACKET:  return "RBRACKET";
        case TOK_PLUS:      return "PLUS";
        case TOK_MINUS:     return "MINUS";
        case TOK_STAR:      return "STAR";
        case TOK_DOT:       return "DOT";
        case TOK_DOLLAR:    return "DOLLAR";
        case TOK_HASH:      return "HASH";
        case TOK_DIRECTIVE: return "DIRECTIVE";
        case TOK_SIZE_HINT: return "SIZE_HINT";
        case TOK_PTR:       return "PTR";
        case TOK_SECTION:   return "SECTION";
        case TOK_EXTERN:    return "EXTERN";
        case TOK_GLOBAL:    return "GLOBAL";
        case TOK_ERROR:     return "ERROR";
    }
    return "?";
}

void asm_token_dump(const asm_token_t *tok) {
    printf("  [%d:%d] %-10s \"%s\"", tok->line, tok->col,
           tok_type_name(tok->type), tok->text);
    if (tok->type == TOK_NUMBER) printf(" = 0x%llx", (unsigned long long)tok->num_val);
    if (tok->type == TOK_SIZE_HINT) printf(" = %d bytes", tok->hint_val);
    printf("\n");
}
