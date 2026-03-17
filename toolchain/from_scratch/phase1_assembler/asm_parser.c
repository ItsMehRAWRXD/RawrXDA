/*==========================================================================
 * asm_parser.c — Full Intel-syntax x64 assembly parser
 *
 * Converts token stream into structured statement list.
 * Parses one statement per line, handling:
 *   - Label definitions (ident ':')
 *   - Instructions with register, immediate, memory, label operands
 *   - Memory operands: [base], [base+disp], [base+index*scale+disp]
 *   - Size prefixes: BYTE/WORD/DWORD/QWORD PTR [...]
 *   - Data directives: db/dw/dd/dq with comma-separated values/strings
 *   - Section/extern/global/equ/align/resX directives
 *=========================================================================*/
#include "asm_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ---- Statement list management ---- */
static asm_stmt_list_t *stmt_list_new(void) {
    asm_stmt_list_t *l = (asm_stmt_list_t *)calloc(1, sizeof(asm_stmt_list_t));
    l->capacity = 128;
    l->stmts = (asm_stmt_t *)calloc((size_t)l->capacity, sizeof(asm_stmt_t));
    return l;
}

static void stmt_list_add(asm_stmt_list_t *l, asm_stmt_t *s) {
    if (l->count >= l->capacity) {
        l->capacity *= 2;
        l->stmts = (asm_stmt_t *)realloc(l->stmts, (size_t)l->capacity * sizeof(asm_stmt_t));
    }
    l->stmts[l->count++] = *s;
}

void asm_stmt_list_free(asm_stmt_list_t *list) {
    if (!list) return;
    free(list->stmts);
    free(list);
}

/* ---- Parser state ---- */
typedef struct {
    asm_token_list_t *tokens;
    int pos;
} parser_t;

static asm_token_t *peek(parser_t *p) {
    if (p->pos < p->tokens->count) return &p->tokens->tokens[p->pos];
    return NULL;
}

static asm_token_t *advance(parser_t *p) {
    if (p->pos < p->tokens->count) return &p->tokens->tokens[p->pos++];
    return NULL;
}

static int check(parser_t *p, asm_token_type_t t) {
    asm_token_t *tok = peek(p);
    return tok && tok->type == t;
}

static int match(parser_t *p, asm_token_type_t t) {
    if (check(p, t)) { advance(p); return 1; }
    return 0;
}

static void skip_to_eol(parser_t *p) {
    while (p->pos < p->tokens->count) {
        asm_token_t *tok = peek(p);
        if (!tok || tok->type == TOK_NEWLINE || tok->type == TOK_EOF) break;
        advance(p);
    }
    if (check(p, TOK_NEWLINE)) advance(p);
}

/* ---- Parse memory operand: [base + index*scale + disp] ---- */
static int parse_memory_operand(parser_t *p, x64_operand_t *op, operand_size_t size_hint) {
    /* We expect '[' has already been consumed */
    op->type = OP_MEM;
    op->size = size_hint ? size_hint : SZ_QWORD;
    op->mem.base = REG_NONE;
    op->mem.index = REG_NONE;
    op->mem.scale = 1;
    op->mem.disp = 0;
    op->mem.has_disp = 0;

    int sign = 1;
    int have_something = 0;

    while (p->pos < p->tokens->count) {
        asm_token_t *tok = peek(p);
        if (!tok || tok->type == TOK_RBRACKET || tok->type == TOK_NEWLINE || tok->type == TOK_EOF)
            break;

        if (tok->type == TOK_PLUS) { sign = 1; advance(p); continue; }
        if (tok->type == TOK_MINUS) { sign = -1; advance(p); continue; }

        if (tok->type == TOK_REGISTER) {
            operand_size_t rsz;
            x64_reg_t reg = x64_parse_register(tok->text, &rsz);
            advance(p);
            have_something = 1;

            /* Check for *scale after register */
            if (check(p, TOK_STAR)) {
                advance(p);
                asm_token_t *sc = peek(p);
                if (sc && sc->type == TOK_NUMBER) {
                    op->mem.index = reg;
                    op->mem.scale = (uint8_t)sc->num_val;
                    advance(p);
                }
            } else {
                /* Check if next is a number with * before it (index*scale) */
                if (op->mem.base == REG_NONE) {
                    op->mem.base = reg;
                } else {
                    op->mem.index = reg;
                    op->mem.scale = 1;
                }
            }
            continue;
        }

        /* RIP-relative */
        if (tok->type == TOK_IDENT) {
            char lower[128];
            int i;
            for (i = 0; tok->text[i] && i < 127; i++)
                lower[i] = (char)tolower((unsigned char)tok->text[i]);
            lower[i] = '\0';

            if (strcmp(lower, "rip") == 0) {
                advance(p);
                op->type = OP_RIP_REL;
                /* Consume + disp or + label */
                if (match(p, TOK_PLUS) || match(p, TOK_MINUS)) {
                    int s2 = (p->tokens->tokens[p->pos-1].type == TOK_MINUS) ? -1 : 1;
                    asm_token_t *next = peek(p);
                    if (next && next->type == TOK_NUMBER) {
                        op->mem.disp = (int32_t)(s2 * next->num_val);
                        op->mem.has_disp = 1;
                        advance(p);
                    } else if (next && next->type == TOK_IDENT) {
                        /* Label-based RIP-relative */
                        op->type = OP_LABEL;
                        strncpy(op->label, next->text, 127);
                        advance(p);
                    }
                }
                have_something = 1;
                continue;
            }

            /* Otherwise it's a symbol reference in memory (label-based) */
            op->type = OP_LABEL;
            strncpy(op->label, tok->text, 127);
            advance(p);
            have_something = 1;
            continue;
        }

        if (tok->type == TOK_NUMBER) {
            op->mem.disp += sign * (int32_t)tok->num_val;
            op->mem.has_disp = 1;
            advance(p);
            have_something = 1;
            continue;
        }

        /* Skip things we don't understand inside brackets */
        advance(p);
    }

    match(p, TOK_RBRACKET);
    (void)have_something;
    return 1;
}

/* ---- Parse one operand ---- */
static int parse_operand(parser_t *p, x64_operand_t *op) {
    memset(op, 0, sizeof(*op));

    asm_token_t *tok = peek(p);
    if (!tok || tok->type == TOK_NEWLINE || tok->type == TOK_EOF || tok->type == TOK_COMMA)
        return 0;

    /* Size hint: BYTE/WORD/DWORD/QWORD [PTR] */
    operand_size_t size_hint = SZ_QWORD;
    if (tok->type == TOK_SIZE_HINT) {
        switch (tok->hint_val) {
            case HINT_BYTE:  size_hint = SZ_BYTE; break;
            case HINT_WORD:  size_hint = SZ_WORD; break;
            case HINT_DWORD: size_hint = SZ_DWORD; break;
            case HINT_QWORD: size_hint = SZ_QWORD; break;
            default: size_hint = SZ_QWORD; break;
        }
        advance(p);
        match(p, TOK_PTR); /* optional PTR */
        tok = peek(p);
    }

    if (!tok) return 0;

    /* Register */
    if (tok->type == TOK_REGISTER) {
        operand_size_t rsz;
        op->reg = x64_parse_register(tok->text, &rsz);
        op->type = OP_REG;
        op->size = rsz;
        advance(p);
        return 1;
    }

    /* Memory: [...] */
    if (tok->type == TOK_LBRACKET) {
        advance(p);
        return parse_memory_operand(p, op, size_hint);
    }

    /* Immediate number */
    if (tok->type == TOK_NUMBER) {
        op->type = OP_IMM;
        op->imm = tok->num_val;
        op->size = size_hint;
        advance(p);
        return 1;
    }

    /* Negative immediate */
    if (tok->type == TOK_MINUS) {
        advance(p);
        tok = peek(p);
        if (tok && tok->type == TOK_NUMBER) {
            op->type = OP_IMM;
            op->imm = -tok->num_val;
            op->size = size_hint;
            advance(p);
            return 1;
        }
        return 0;
    }

    /* Label/symbol reference */
    if (tok->type == TOK_IDENT) {
        op->type = OP_LABEL;
        op->size = size_hint;
        strncpy(op->label, tok->text, 127);
        advance(p);
        return 1;
    }

    /* $ (current address) */
    if (tok->type == TOK_DOLLAR) {
        op->type = OP_LABEL;
        op->size = size_hint;
        strcpy(op->label, "$");
        advance(p);
        return 1;
    }

    return 0;
}

/* ---- Parse data directive values ---- */
static void parse_data_values(parser_t *p, asm_stmt_t *stmt) {
    stmt->data_count = 0;
    while (p->pos < p->tokens->count) {
        asm_token_t *tok = peek(p);
        if (!tok || tok->type == TOK_NEWLINE || tok->type == TOK_EOF) break;

        data_item_t item = {0};

        if (tok->type == TOK_STRING) {
            item.is_string = 1;
            strncpy(item.str_val, tok->text, 255);
            item.str_len = (int)strlen(tok->text);
            advance(p);
        } else if (tok->type == TOK_NUMBER) {
            item.num_val = tok->num_val;
            advance(p);
        } else if (tok->type == TOK_MINUS) {
            advance(p);
            tok = peek(p);
            if (tok && tok->type == TOK_NUMBER) {
                item.num_val = -tok->num_val;
                advance(p);
            }
        } else if (tok->type == TOK_IDENT || tok->type == TOK_DOLLAR) {
            /* Symbol reference in data */
            item.num_val = 0;
            strncpy(item.str_val, tok->text, 255);
            advance(p);
        } else {
            advance(p);
            continue;
        }

        if (stmt->data_count < 256)
            stmt->data_items[stmt->data_count++] = item;

        /* Comma separator */
        if (!match(p, TOK_COMMA)) break;
    }
}

/* ---- Determine data directive size ---- */
static int get_data_size(const char *directive) {
    char lower[16];
    int i;
    for (i = 0; directive[i] && i < 15; i++)
        lower[i] = (char)tolower((unsigned char)directive[i]);
    lower[i] = '\0';
    if (strcmp(lower, "db") == 0) return 1;
    if (strcmp(lower, "dw") == 0) return 2;
    if (strcmp(lower, "dd") == 0) return 4;
    if (strcmp(lower, "dq") == 0) return 8;
    if (strcmp(lower, "dt") == 0) return 10;
    return 1;
}

static int get_resv_size(const char *directive) {
    char lower[16];
    int i;
    for (i = 0; directive[i] && i < 15; i++)
        lower[i] = (char)tolower((unsigned char)directive[i]);
    lower[i] = '\0';
    if (strcmp(lower, "resb") == 0) return 1;
    if (strcmp(lower, "resw") == 0) return 2;
    if (strcmp(lower, "resd") == 0) return 4;
    if (strcmp(lower, "resq") == 0) return 8;
    return 1;
}

static void to_lower_copy(char *dst, size_t dst_size, const char *src) {
    size_t i = 0;
    if (!dst || dst_size == 0) return;
    while (src && src[i] && i + 1 < dst_size) {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

static int is_unwind_directive(const char *ident) {
    char lower[32];
    to_lower_copy(lower, sizeof(lower), ident);
    return strcmp(lower, ".pushreg") == 0 ||
           strcmp(lower, ".allocstack") == 0 ||
           strcmp(lower, ".setframe") == 0 ||
           strcmp(lower, ".savereg") == 0 ||
           strcmp(lower, ".savexmm128") == 0 ||
           strcmp(lower, ".pushframe") == 0 ||
           strcmp(lower, ".endprolog") == 0;
}

/* ---- Section flags from name ---- */
static uint32_t section_flags_from_name(const char *name) {
    char lower[64];
    int i;
    for (i = 0; name[i] && i < 63; i++)
        lower[i] = (char)tolower((unsigned char)name[i]);
    lower[i] = '\0';

    if (strstr(lower, ".text") || strstr(lower, ".code"))
        return 0x60000020; /* CNT_CODE | MEM_EXECUTE | MEM_READ */
    if (strstr(lower, ".rdata") || strstr(lower, ".const"))
        return 0x40000040; /* CNT_INITIALIZED_DATA | MEM_READ */
    if (strstr(lower, ".data"))
        return 0xC0000040; /* CNT_INITIALIZED_DATA | MEM_READ | MEM_WRITE */
    if (strstr(lower, ".bss"))
        return 0xC0000080; /* CNT_UNINITIALIZED_DATA | MEM_READ | MEM_WRITE */
    if (strstr(lower, ".pdata"))
        return 0x40000040;
    if (strstr(lower, ".xdata"))
        return 0x40000040;
    return 0xC0000040; /* default: initialized, RW */
}

/* ============================================================
 * Main parser
 * ============================================================ */
asm_stmt_list_t *asm_parse(asm_token_list_t *tokens) {
    asm_stmt_list_t *list = stmt_list_new();
    parser_t parser = { tokens, 0 };
    parser_t *p = &parser;

    while (p->pos < p->tokens->count) {
        asm_token_t *tok = peek(p);
        if (!tok || tok->type == TOK_EOF) break;

        /* Skip empty lines */
        if (tok->type == TOK_NEWLINE) { advance(p); continue; }

        int line = tok->line;

        /* ---- Section declaration ---- */
        if (tok->type == TOK_SECTION) {
            asm_stmt_t stmt = {0};
            stmt.type = STMT_SECTION;
            stmt.line = line;
            advance(p);

            /* Section name can be: .text, .data, identifier, etc. */
            tok = peek(p);
            if (tok && (tok->type == TOK_IDENT || tok->type == TOK_SECTION)) {
                strncpy(stmt.section_name, tok->text, 63);
                advance(p);
            } else if (tok && tok->type == TOK_DOT) {
                advance(p);
                tok = peek(p);
                if (tok && tok->type == TOK_IDENT) {
                    snprintf(stmt.section_name, 63, ".%s", tok->text);
                    advance(p);
                }
            }
            /* If section keyword already has the name (e.g., ".code") */
            if (stmt.section_name[0] == '\0') {
                asm_token_t *prev = &p->tokens->tokens[p->pos - 1];
                strncpy(stmt.section_name, prev->text, 63);
            }
            stmt.section_flags = section_flags_from_name(stmt.section_name);
            stmt_list_add(list, &stmt);
            skip_to_eol(p);
            continue;
        }

        /* ---- EXTERN declaration ---- */
        if (tok->type == TOK_EXTERN) {
            advance(p);
            while (p->pos < p->tokens->count) {
                tok = peek(p);
                if (!tok || tok->type == TOK_NEWLINE || tok->type == TOK_EOF) break;
                if (tok->type == TOK_IDENT) {
                    asm_stmt_t stmt = {0};
                    stmt.type = STMT_EXTERN;
                    stmt.line = line;
                    strncpy(stmt.symbol, tok->text, 127);
                    stmt_list_add(list, &stmt);
                    advance(p);
                } else if (tok->type == TOK_COMMA) {
                    advance(p);
                } else {
                    advance(p); /* skip size hints like :PROC */
                }
            }
            skip_to_eol(p);
            continue;
        }

        /* ---- GLOBAL declaration ---- */
        if (tok->type == TOK_GLOBAL) {
            advance(p);
            while (p->pos < p->tokens->count) {
                tok = peek(p);
                if (!tok || tok->type == TOK_NEWLINE || tok->type == TOK_EOF) break;
                if (tok->type == TOK_IDENT) {
                    asm_stmt_t stmt = {0};
                    stmt.type = STMT_GLOBAL;
                    stmt.line = line;
                    strncpy(stmt.symbol, tok->text, 127);
                    stmt_list_add(list, &stmt);
                    advance(p);
                } else if (tok->type == TOK_COMMA) {
                    advance(p);
                } else {
                    advance(p);
                }
            }
            skip_to_eol(p);
            continue;
        }

        /* ---- Label: ident followed by colon ---- */
        if (tok->type == TOK_IDENT) {
            char ident_lower[32];
            to_lower_copy(ident_lower, sizeof(ident_lower), tok->text);

            /* INVOKE target, arg1, arg2 ... */
            if (strcmp(ident_lower, "invoke") == 0) {
                asm_stmt_t stmt = {0};
                stmt.type = STMT_INVOKE;
                stmt.line = line;
                advance(p); /* invoke keyword */

                tok = peek(p);
                if (tok && tok->type == TOK_IDENT) {
                    strncpy(stmt.invoke_target, tok->text, 127);
                    advance(p);
                }

                while (stmt.invoke_arg_count < 8) {
                    if (!match(p, TOK_COMMA)) break;
                    if (!parse_operand(p, &stmt.invoke_args[stmt.invoke_arg_count])) break;
                    stmt.invoke_arg_count++;
                }

                stmt_list_add(list, &stmt);
                skip_to_eol(p);
                continue;
            }

            /* MASM unwind directives */
            if (is_unwind_directive(tok->text)) {
                asm_stmt_t stmt = {0};
                stmt.type = STMT_UNWIND;
                stmt.line = line;
                strncpy(stmt.unwind_directive, tok->text, sizeof(stmt.unwind_directive) - 1);
                advance(p); /* unwind directive */

                if (parse_operand(p, &stmt.unwind_operands[0])) {
                    stmt.unwind_operand_count = 1;
                    if (match(p, TOK_COMMA) && parse_operand(p, &stmt.unwind_operands[1])) {
                        stmt.unwind_operand_count = 2;
                    }
                }

                stmt_list_add(list, &stmt);
                skip_to_eol(p);
                continue;
            }

            /* Look ahead for colon */
            if (p->pos + 1 < p->tokens->count &&
                p->tokens->tokens[p->pos + 1].type == TOK_COLON) {
                asm_stmt_t stmt = {0};
                stmt.type = STMT_LABEL;
                stmt.line = line;
                strncpy(stmt.label, tok->text, 127);
                stmt_list_add(list, &stmt);
                advance(p); /* ident */
                advance(p); /* colon */
                continue; /* next statement on same line, if any */
            }

            /* Check for EQU: name equ value */
            if (p->pos + 1 < p->tokens->count &&
                p->tokens->tokens[p->pos + 1].type == TOK_DIRECTIVE) {
                char lower[16];
                const char *dt = p->tokens->tokens[p->pos + 1].text;
                to_lower_copy(lower, sizeof(lower), dt);

                /* name PROC */
                if (strcmp(lower, "proc") == 0) {
                    asm_stmt_t stmt = {0};
                    stmt.type = STMT_PROC;
                    stmt.line = line;
                    strncpy(stmt.proc_name, tok->text, sizeof(stmt.proc_name) - 1);
                    stmt_list_add(list, &stmt);
                    skip_to_eol(p);
                    continue;
                }

                /* name ENDP */
                if (strcmp(lower, "endp") == 0) {
                    asm_stmt_t stmt = {0};
                    stmt.type = STMT_ENDP;
                    stmt.line = line;
                    strncpy(stmt.proc_name, tok->text, sizeof(stmt.proc_name) - 1);
                    stmt_list_add(list, &stmt);
                    skip_to_eol(p);
                    continue;
                }

                if (strcmp(lower, "equ") == 0) {
                    asm_stmt_t stmt = {0};
                    stmt.type = STMT_EQU;
                    stmt.line = line;
                    strncpy(stmt.equ_name, tok->text, 127);
                    advance(p); /* name */
                    advance(p); /* equ */
                    tok = peek(p);
                    if (tok && tok->type == TOK_NUMBER) {
                        stmt.equ_value = tok->num_val;
                        advance(p);
                    }
                    stmt_list_add(list, &stmt);
                    skip_to_eol(p);
                    continue;
                }
            }
        }

        /* ---- Data directive: db/dw/dd/dq ---- */
        if (tok->type == TOK_DIRECTIVE) {
            char lower[16];
            int di;
            for (di = 0; tok->text[di] && di < 15; di++)
                lower[di] = (char)tolower((unsigned char)tok->text[di]);
            lower[di] = '\0';

            if (strcmp(lower, "db") == 0 || strcmp(lower, "dw") == 0 ||
                strcmp(lower, "dd") == 0 || strcmp(lower, "dq") == 0 ||
                strcmp(lower, "dt") == 0) {
                asm_stmt_t stmt = {0};
                stmt.type = STMT_DATA;
                stmt.line = line;
                stmt.data_size = get_data_size(tok->text);
                advance(p);
                parse_data_values(p, &stmt);
                stmt_list_add(list, &stmt);
                skip_to_eol(p);
                continue;
            }

            /* ALIGN */
            if (strcmp(lower, "align") == 0) {
                asm_stmt_t stmt = {0};
                stmt.type = STMT_ALIGN;
                stmt.line = line;
                advance(p);
                tok = peek(p);
                if (tok && tok->type == TOK_NUMBER) {
                    stmt.align_value = tok->num_val;
                    advance(p);
                }
                stmt_list_add(list, &stmt);
                skip_to_eol(p);
                continue;
            }

            /* RESB/RESW/RESD/RESQ */
            if (strcmp(lower, "resb") == 0 || strcmp(lower, "resw") == 0 ||
                strcmp(lower, "resd") == 0 || strcmp(lower, "resq") == 0) {
                asm_stmt_t stmt = {0};
                stmt.type = STMT_RESV;
                stmt.line = line;
                stmt.resv_size = get_resv_size(tok->text);
                advance(p);
                tok = peek(p);
                if (tok && tok->type == TOK_NUMBER) {
                    stmt.align_value = tok->num_val; /* count */
                    advance(p);
                }
                stmt_list_add(list, &stmt);
                skip_to_eol(p);
                continue;
            }

            /* directive-first PROC/ENDP forms */
            if (strcmp(lower, "proc") == 0 || strcmp(lower, "endp") == 0) {
                asm_stmt_t stmt = {0};
                stmt.type = (strcmp(lower, "proc") == 0) ? STMT_PROC : STMT_ENDP;
                stmt.line = line;
                advance(p); /* proc/endp */
                tok = peek(p);
                if (tok && tok->type == TOK_IDENT) {
                    strncpy(stmt.proc_name, tok->text, sizeof(stmt.proc_name) - 1);
                }
                stmt_list_add(list, &stmt);
                skip_to_eol(p);
                continue;
            }

            if (strcmp(lower, "end") == 0) {
                skip_to_eol(p);
                continue;
            }

            /* Unknown directive — skip */
            skip_to_eol(p);
            continue;
        }

        /* ---- Instruction or label-preceded data ---- */
        if (tok->type == TOK_IDENT || tok->type == TOK_REGISTER) {
            /* Try as mnemonic */
            x64_mnemonic_t mnem = x64_parse_mnemonic(tok->text);
            if (mnem != MNEM_UNKNOWN) {
                asm_stmt_t stmt = {0};
                stmt.type = STMT_INSTRUCTION;
                stmt.line = line;
                stmt.mnemonic = mnem;
                stmt.num_operands = 0;
                advance(p);

                /* Parse operands */
                if (parse_operand(p, &stmt.operands[0])) {
                    stmt.num_operands = 1;
                    if (match(p, TOK_COMMA)) {
                        if (parse_operand(p, &stmt.operands[1])) {
                            stmt.num_operands = 2;
                        }
                    }
                }

                stmt_list_add(list, &stmt);
                skip_to_eol(p);
                continue;
            }

            /* Could be a label without colon (NASM-style, at line start) */
            /* or label followed by data directive */
            {
                asm_stmt_t label_stmt = {0};
                label_stmt.type = STMT_LABEL;
                label_stmt.line = line;
                strncpy(label_stmt.label, tok->text, 127);
                advance(p);

                tok = peek(p);
                if (tok && tok->type == TOK_DIRECTIVE) {
                    /* label db/dw/dd/dq ... */
                    stmt_list_add(list, &label_stmt);
                    continue; /* re-parse the directive */
                }

                /* Just a standalone label if followed by newline */
                if (!tok || tok->type == TOK_NEWLINE || tok->type == TOK_EOF) {
                    stmt_list_add(list, &label_stmt);
                    continue;
                }

                /* Unknown — skip line */
                skip_to_eol(p);
                continue;
            }
        }

        /* Skip unknown tokens */
        advance(p);
    }

    return list;
}

/* ---- Debug dump ---- */
static const char *stmt_type_name(stmt_type_t t) {
    switch (t) {
        case STMT_NONE:        return "NONE";
        case STMT_INSTRUCTION: return "INSTR";
        case STMT_INVOKE:      return "INVOKE";
        case STMT_LABEL:       return "LABEL";
        case STMT_PROC:        return "PROC";
        case STMT_ENDP:        return "ENDP";
        case STMT_UNWIND:      return "UNWIND";
        case STMT_DATA:        return "DATA";
        case STMT_SECTION:     return "SECTION";
        case STMT_EXTERN:      return "EXTERN";
        case STMT_GLOBAL:      return "GLOBAL";
        case STMT_EQU:         return "EQU";
        case STMT_ALIGN:       return "ALIGN";
        case STMT_RESV:        return "RESV";
        case STMT_TIMES:       return "TIMES";
    }
    return "?";
}

static const char *op_type_name(operand_type_t t) {
    switch (t) {
        case OP_NONE:    return "none";
        case OP_REG:     return "reg";
        case OP_IMM:     return "imm";
        case OP_MEM:     return "mem";
        case OP_RIP_REL: return "rip";
        case OP_LABEL:   return "label";
    }
    return "?";
}

void asm_stmt_dump(const asm_stmt_t *stmt) {
    printf("  [line %3d] %-8s", stmt->line, stmt_type_name(stmt->type));
    switch (stmt->type) {
        case STMT_INSTRUCTION:
            printf(" %s", x64_mnemonic_name(stmt->mnemonic));
            for (int i = 0; i < stmt->num_operands; i++) {
                const x64_operand_t *op = &stmt->operands[i];
                printf(" %s", (i > 0) ? ", " : " ");
                printf("(%s", op_type_name(op->type));
                if (op->type == OP_REG) printf(" r%d sz%d", op->reg, op->size);
                else if (op->type == OP_IMM) printf(" 0x%llx", (unsigned long long)op->imm);
                else if (op->type == OP_LABEL) printf(" '%s'", op->label);
                else if (op->type == OP_MEM) printf(" base=%d idx=%d*%d+%d",
                    op->mem.base, op->mem.index, op->mem.scale, op->mem.disp);
                printf(")");
            }
            break;
        case STMT_INVOKE:
            printf(" target='%s' argc=%d", stmt->invoke_target, stmt->invoke_arg_count);
            for (int i = 0; i < stmt->invoke_arg_count; i++) {
                const x64_operand_t *op = &stmt->invoke_args[i];
                printf(" arg%d=(%s", i, op_type_name(op->type));
                if (op->type == OP_REG) printf(" r%d sz%d", op->reg, op->size);
                else if (op->type == OP_IMM) printf(" 0x%llx", (unsigned long long)op->imm);
                else if (op->type == OP_LABEL) printf(" '%s'", op->label);
                else if (op->type == OP_MEM) printf(" base=%d idx=%d*%d+%d",
                    op->mem.base, op->mem.index, op->mem.scale, op->mem.disp);
                printf(")");
            }
            break;
        case STMT_LABEL:
            printf(" '%s'", stmt->label);
            break;
        case STMT_PROC:
        case STMT_ENDP:
            printf(" '%s'", stmt->proc_name);
            break;
        case STMT_UNWIND:
            printf(" %s opcount=%d", stmt->unwind_directive, stmt->unwind_operand_count);
            for (int i = 0; i < stmt->unwind_operand_count; i++) {
                const x64_operand_t *op = &stmt->unwind_operands[i];
                printf(" op%d=(%s", i, op_type_name(op->type));
                if (op->type == OP_REG) printf(" r%d sz%d", op->reg, op->size);
                else if (op->type == OP_IMM) printf(" 0x%llx", (unsigned long long)op->imm);
                else if (op->type == OP_LABEL) printf(" '%s'", op->label);
                else if (op->type == OP_MEM) printf(" base=%d idx=%d*%d+%d",
                    op->mem.base, op->mem.index, op->mem.scale, op->mem.disp);
                printf(")");
            }
            break;
        case STMT_SECTION:
            printf(" '%s' flags=0x%08X", stmt->section_name, stmt->section_flags);
            break;
        case STMT_EXTERN:
        case STMT_GLOBAL:
            printf(" '%s'", stmt->symbol);
            break;
        case STMT_EQU:
            printf(" '%s' = 0x%llx", stmt->equ_name, (unsigned long long)stmt->equ_value);
            break;
        case STMT_DATA:
            printf(" size=%d count=%d", stmt->data_size, stmt->data_count);
            break;
        case STMT_ALIGN:
            printf(" %lld", (long long)stmt->align_value);
            break;
        case STMT_RESV:
            printf(" %d * %lld", stmt->resv_size, (long long)stmt->align_value);
            break;
        default:
            break;
    }
    printf("\n");
}

void asm_stmt_list_dump(const asm_stmt_list_t *list) {
    printf("=== Parsed %d statements ===\n", list->count);
    for (int i = 0; i < list->count; i++) {
        asm_stmt_dump(&list->stmts[i]);
    }
}
