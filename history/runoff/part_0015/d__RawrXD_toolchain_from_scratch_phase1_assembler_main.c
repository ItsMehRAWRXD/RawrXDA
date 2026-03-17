/*==========================================================================
 * Phase 1 Assembler — main.c
 *
 * Complete x64 assembler: .asm → .obj
 * Pipeline: Lex → Parse → Encode → Write COFF
 *
 * Usage:
 *   rawrxd_asm input.asm [-o output.obj] [-v] [-dump-tokens] [-dump-stmts]
 *
 * Features:
 *   - Intel syntax (MASM-compatible)
 *   - Full x64 register set (RAX-R15, all sizes)
 *   - Section support (.text, .data, .rdata, .bss, custom)
 *   - Label resolution with forward/backward references
 *   - EXTERN/GLOBAL symbol declarations
 *   - Data directives (db, dw, dd, dq)
 *   - ALIGN, EQU, RESB/RESW/RESD/RESQ directives
 *   - Generates standard COFF .obj (linkable with phase2_linker)
 *=========================================================================*/
#include "asm_lexer.h"
#include "asm_parser.h"
#include "x64_encoder.h"
#include "coff_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Label tracking for resolution ---- */
typedef struct {
    char     name[128];
    int      section_idx;   /* COFF section index */
    uint32_t offset;         /* byte offset within section */
    int      is_global;
} label_info_t;

typedef struct {
    label_info_t *labels;
    int count;
    int capacity;
} label_table_t;

static label_table_t *label_table_new(void) {
    label_table_t *t = (label_table_t *)calloc(1, sizeof(label_table_t));
    t->capacity = 256;
    t->labels = (label_info_t *)calloc(256, sizeof(label_info_t));
    return t;
}

static void label_table_free(label_table_t *t) {
    if (!t) return;
    free(t->labels);
    free(t);
}

static void label_table_add(label_table_t *t, const char *name, int sec_idx, uint32_t offset) {
    if (t->count >= t->capacity) {
        t->capacity *= 2;
        t->labels = (label_info_t *)realloc(t->labels,
                     (size_t)t->capacity * sizeof(label_info_t));
    }
    label_info_t *l = &t->labels[t->count++];
    memset(l, 0, sizeof(*l));
    strncpy(l->name, name, 127);
    l->section_idx = sec_idx;
    l->offset = offset;
    l->is_global = 0;
}

static label_info_t *label_table_find(label_table_t *t, const char *name) {
    for (int i = 0; i < t->count; i++) {
        if (strcmp(t->labels[i].name, name) == 0) return &t->labels[i];
    }
    return NULL;
}

/* ---- EQU constant tracking ---- */
typedef struct { char name[128]; int64_t value; } equ_entry_t;
typedef struct {
    equ_entry_t *entries;
    int count, capacity;
} equ_table_t;

static equ_table_t *equ_table_new(void) {
    equ_table_t *t = (equ_table_t *)calloc(1, sizeof(equ_table_t));
    t->capacity = 64;
    t->entries = (equ_entry_t *)calloc(64, sizeof(equ_entry_t));
    return t;
}
static void equ_table_free(equ_table_t *t) { if (t) { free(t->entries); free(t); } }
static void equ_table_add(equ_table_t *t, const char *name, int64_t value) {
    if (t->count >= t->capacity) {
        t->capacity *= 2;
        t->entries = (equ_entry_t *)realloc(t->entries,
                      (size_t)t->capacity * sizeof(equ_entry_t));
    }
    strncpy(t->entries[t->count].name, name, 127);
    t->entries[t->count].value = value;
    t->count++;
}

/* ---- Extern tracking ---- */
typedef struct {
    char **names;
    int count, capacity;
} extern_table_t;

static extern_table_t *extern_table_new(void) {
    extern_table_t *t = (extern_table_t *)calloc(1, sizeof(extern_table_t));
    t->capacity = 64;
    t->names = (char **)calloc(64, sizeof(char *));
    return t;
}
static void extern_table_free(extern_table_t *t) {
    if (!t) return;
    for (int i = 0; i < t->count; i++) free(t->names[i]);
    free(t->names);
    free(t);
}
static void extern_table_add(extern_table_t *t, const char *name) {
    if (t->count >= t->capacity) {
        t->capacity *= 2;
        t->names = (char **)realloc(t->names, (size_t)t->capacity * sizeof(char *));
    }
    t->names[t->count++] = strdup(name);
}
static int extern_table_has(extern_table_t *t, const char *name) {
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->names[i], name) == 0) return 1;
    return 0;
}

/* ============================================================
 * Assemble: process parsed statements into COFF object
 * ============================================================ */
static int assemble(asm_stmt_list_t *stmts, coff_obj_builder_t *obj, int verbose) {
    label_table_t *labels = label_table_new();
    equ_table_t *equs = equ_table_new();
    extern_table_t *externs = extern_table_new();

    int current_section = -1;
    int errors = 0;

    /* ---- Pass 1: Collect labels, sections, externs, globals ---- */
    /* We also emit code/data in this pass since we do single-pass
     * with relocations for forward references */

    for (int i = 0; i < stmts->count; i++) {
        asm_stmt_t *s = &stmts->stmts[i];

        switch (s->type) {
        case STMT_SECTION: {
            int idx = coff_obj_find_section(obj, s->section_name);
            if (idx < 0) {
                idx = coff_obj_add_section(obj, s->section_name, s->section_flags);
            }
            current_section = idx;
            if (verbose) printf("[SECTION] '%s' (idx=%d)\n", s->section_name, idx);
            break;
        }

        case STMT_EXTERN:
            extern_table_add(externs, s->symbol);
            /* Add as undefined external symbol */
            coff_obj_add_symbol(obj, s->symbol, 0, 0, SYM_TYPE_NULL, SYM_CLASS_EXTERNAL);
            if (verbose) printf("[EXTERN] '%s'\n", s->symbol);
            break;

        case STMT_GLOBAL: {
            /* Mark label as global (will be applied when label is defined) */
            label_info_t *lbl = label_table_find(labels, s->symbol);
            if (lbl) lbl->is_global = 1;
            else {
                /* Pre-register so we know it's global when we see the label */
                label_table_add(labels, s->symbol, -1, 0);
                label_info_t *newlbl = label_table_find(labels, s->symbol);
                if (newlbl) newlbl->is_global = 1;
            }
            if (verbose) printf("[GLOBAL] '%s'\n", s->symbol);
            break;
        }

        case STMT_LABEL: {
            if (current_section < 0) {
                /* Auto-create .text section */
                current_section = coff_obj_add_section(obj, ".text",
                    SCN_CNT_CODE | SCN_MEM_EXECUTE | SCN_MEM_READ | SCN_ALIGN_16);
            }
            uint32_t offset = coff_section_size(obj, current_section);
            label_info_t *existing = label_table_find(labels, s->label);
            if (existing && existing->section_idx >= 0) {
                fprintf(stderr, "WARNING: line %d: duplicate label '%s'\n", s->line, s->label);
            } else if (existing) {
                existing->section_idx = current_section;
                existing->offset = offset;
            } else {
                label_table_add(labels, s->label, current_section, offset);
            }

            /* Add symbol to COFF */
            int sec_num = current_section + 1;
            label_info_t *lbl = label_table_find(labels, s->label);
            uint8_t sclass = (lbl && lbl->is_global) ? SYM_CLASS_EXTERNAL : SYM_CLASS_STATIC;
            uint16_t stype = SYM_TYPE_NULL;

            /* Peek ahead: if next statement is an instruction, mark as function */
            if (i + 1 < stmts->count && stmts->stmts[i + 1].type == STMT_INSTRUCTION)
                stype = SYM_TYPE_FUNCTION;

            coff_obj_add_symbol(obj, s->label, (int32_t)offset,
                               (int16_t)sec_num, stype, sclass);
            if (verbose) printf("[LABEL] '%s' at section %d offset 0x%X\n",
                               s->label, current_section, offset);
            break;
        }

        case STMT_EQU:
            equ_table_add(equs, s->equ_name, s->equ_value);
            /* Add as absolute symbol */
            coff_obj_add_symbol(obj, s->equ_name, (int32_t)s->equ_value,
                               -1, SYM_TYPE_NULL, SYM_CLASS_STATIC);
            if (verbose) printf("[EQU] '%s' = 0x%llx\n", s->equ_name, (unsigned long long)s->equ_value);
            break;

        case STMT_ALIGN:
            if (current_section >= 0) {
                coff_section_align(obj, current_section, (uint32_t)s->align_value);
            }
            break;

        case STMT_RESV:
            if (current_section >= 0) {
                uint32_t total = (uint32_t)(s->resv_size * s->align_value);
                uint8_t *zeros = (uint8_t *)calloc(1, total);
                coff_section_append(obj, current_section, zeros, total);
                free(zeros);
            }
            break;

        case STMT_DATA: {
            if (current_section < 0) {
                current_section = coff_obj_add_section(obj, ".data",
                    SCN_CNT_INITIALIZED | SCN_MEM_READ | SCN_MEM_WRITE);
            }
            for (int d = 0; d < s->data_count; d++) {
                data_item_t *item = &s->data_items[d];
                if (item->is_string) {
                    /* String: emit bytes directly */
                    coff_section_append(obj, current_section,
                        (const uint8_t *)item->str_val, (uint32_t)item->str_len);
                } else {
                    /* Numeric value */
                    uint8_t buf[8] = {0};
                    int64_t v = item->num_val;
                    switch (s->data_size) {
                        case 1: buf[0] = (uint8_t)v; break;
                        case 2: buf[0]=(uint8_t)v; buf[1]=(uint8_t)(v>>8); break;
                        case 4:
                            buf[0]=(uint8_t)v; buf[1]=(uint8_t)(v>>8);
                            buf[2]=(uint8_t)(v>>16); buf[3]=(uint8_t)(v>>24);
                            break;
                        case 8:
                            for (int b = 0; b < 8; b++) buf[b]=(uint8_t)(v>>(b*8));
                            break;
                    }
                    coff_section_append(obj, current_section,
                        buf, (uint32_t)s->data_size);
                }
            }
            break;
        }

        case STMT_INSTRUCTION: {
            if (current_section < 0) {
                current_section = coff_obj_add_section(obj, ".text",
                    SCN_CNT_CODE | SCN_MEM_EXECUTE | SCN_MEM_READ | SCN_ALIGN_16);
            }

            /* Encode instruction */
            x64_operand_t *op1 = (s->num_operands >= 1) ? &s->operands[0] : NULL;
            x64_operand_t *op2 = (s->num_operands >= 2) ? &s->operands[1] : NULL;

            x64_encoded_t enc = x64_encode(s->mnemonic, op1, op2);

            if (enc.len == 0) {
                fprintf(stderr, "WARNING: line %d: failed to encode '%s'\n",
                        s->line, x64_mnemonic_name(s->mnemonic));
                errors++;
            } else {
                uint32_t offset = coff_section_size(obj, current_section);

                /* If instruction has a relocation, add it to the section */
                if (enc.reloc_type != 0 && enc.reloc_symbol[0]) {
                    coff_section_add_reloc(obj, current_section,
                        offset + enc.reloc_offset,
                        enc.reloc_type, enc.reloc_symbol);
                }

                /* Check operands for label references that need relocations */
                for (int opn = 0; opn < s->num_operands; opn++) {
                    if (s->operands[opn].type == OP_LABEL && enc.reloc_type == 0) {
                        /* This label reference wasn't handled by the encoder's
                         * relocation system — it may be a data reference or
                         * memory operand with a label */
                    }
                }

                coff_section_append(obj, current_section, enc.bytes, (uint32_t)enc.len);

                if (verbose) {
                    printf("[ENCODE] line %d: %s →", s->line,
                           x64_mnemonic_name(s->mnemonic));
                    for (int b = 0; b < enc.len; b++) printf(" %02X", enc.bytes[b]);
                    if (enc.reloc_type) printf(" [reloc: %s @+%u]",
                        enc.reloc_symbol, enc.reloc_offset);
                    printf("\n");
                }
            }
            break;
        }

        default:
            break;
        }
    }

    label_table_free(labels);
    equ_table_free(equs);
    extern_table_free(externs);
    return errors;
}

/* ============================================================
 * Main entry point
 * ============================================================ */
int main(int argc, char **argv) {
    const char *input = NULL;
    const char *output = NULL;
    int verbose = 0;
    int dump_tokens = 0;
    int dump_stmts = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-dump-tokens") == 0) {
            dump_tokens = 1;
        } else if (strcmp(argv[i], "-dump-stmts") == 0) {
            dump_stmts = 1;
        } else if (argv[i][0] != '-') {
            input = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
        }
    }

    if (!input) {
        fprintf(stderr, "RawrXD Assembler — Phase 1 (x64 .asm → .obj)\n");
        fprintf(stderr, "Usage: rawrxd_asm <input.asm> [-o output.obj] [-v] [-dump-tokens] [-dump-stmts]\n");
        return 1;
    }

    /* Default output name */
    char default_output[512];
    if (!output) {
        strncpy(default_output, input, 500);
        char *dot = strrchr(default_output, '.');
        if (dot) strcpy(dot, ".obj");
        else strcat(default_output, ".obj");
        output = default_output;
    }

    printf("RawrXD Assembler v1.0 — Phase 1\n");
    printf("Input:  %s\n", input);
    printf("Output: %s\n", output);

    /* ---- Stage 1: Lex ---- */
    asm_token_list_t *tokens = asm_lex_file(input);
    if (!tokens) {
        fprintf(stderr, "ERROR: cannot open '%s'\n", input);
        return 1;
    }
    printf("Lexed %d tokens\n", tokens->count);

    if (dump_tokens) {
        printf("=== Tokens ===\n");
        for (int i = 0; i < tokens->count; i++)
            asm_token_dump(&tokens->tokens[i]);
    }

    /* ---- Stage 2: Parse ---- */
    asm_stmt_list_t *stmts = asm_parse(tokens);
    printf("Parsed %d statements\n", stmts->count);

    if (dump_stmts) {
        asm_stmt_list_dump(stmts);
    }

    /* ---- Stage 3: Assemble (encode + build COFF) ---- */
    coff_obj_builder_t *obj = coff_obj_new();
    int errors = assemble(stmts, obj, verbose);

    if (errors > 0) {
        fprintf(stderr, "%d encoding error(s)\n", errors);
    }

    /* ---- Stage 4: Write COFF .obj ---- */
    printf("Writing COFF: %d sections, %d symbols\n",
           obj->section_count, obj->symbol_count);

    if (coff_obj_write(obj, output) == 0) {
        printf("SUCCESS: wrote '%s'\n", output);
    } else {
        fprintf(stderr, "ERROR: failed to write '%s'\n", output);
        coff_obj_free(obj);
        asm_stmt_list_free(stmts);
        asm_token_list_free(tokens);
        return 1;
    }

    /* ---- Cleanup ---- */
    coff_obj_free(obj);
    asm_stmt_list_free(stmts);
    asm_token_list_free(tokens);
    return (errors > 0) ? 1 : 0;
}
