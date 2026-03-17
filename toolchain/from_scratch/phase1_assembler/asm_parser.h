/*==========================================================================
 * asm_parser.h — Assembly parser: tokens → instruction list
 *
 * Parses tokenized Intel-syntax x64 assembly into structured instruction
 * and data directives. Handles:
 *   - Section declarations (.text, .data, .bss, etc.)
 *   - Labels (both trailing colon and line-start)
 *   - Instructions with 0-2 operands
 *   - Data directives (db, dw, dd, dq with values/strings)
 *   - EXTERN / GLOBAL declarations
 *   - EQU constant definitions
 *   - ALIGN directives
 *   - Memory operands: [base + index*scale + disp]
 *   - Size hints: BYTE/WORD/DWORD/QWORD PTR
 *=========================================================================*/
#ifndef ASM_PARSER_H
#define ASM_PARSER_H

#include "asm_lexer.h"
#include "x64_encoder.h"

/* ---- Statement types ---- */
typedef enum {
    STMT_NONE = 0,
    STMT_INSTRUCTION,   /* mnemonic op1, op2 */
    STMT_INVOKE,        /* invoke target, args... */
    STMT_LABEL,         /* label: */
    STMT_PROC,          /* name PROC */
    STMT_ENDP,          /* name ENDP */
    STMT_UNWIND,        /* .pushreg/.allocstack/.setframe/... */
    STMT_DATA,          /* db/dw/dd/dq values */
    STMT_SECTION,       /* section .text */
    STMT_EXTERN,        /* extern symbol */
    STMT_GLOBAL,        /* global symbol */
    STMT_EQU,           /* name equ value */
    STMT_ALIGN,         /* align N */
    STMT_RESV,          /* resb/resw/resd/resq N */
    STMT_TIMES          /* times N instruction/data */
} stmt_type_t;

/* ---- Data item (for db/dw/dd/dq) ---- */
typedef struct {
    int     is_string;
    char    str_val[256];
    int     str_len;
    int64_t num_val;
} data_item_t;

/* ---- Parsed statement ---- */
typedef struct {
    stmt_type_t     type;
    int             line;

    /* STMT_INSTRUCTION */
    x64_mnemonic_t  mnemonic;
    x64_operand_t   operands[2];
    int             num_operands;

    /* STMT_INVOKE */
    char            invoke_target[128];
    x64_operand_t   invoke_args[8];
    int             invoke_arg_count;

    /* STMT_LABEL */
    char            label[128];

    /* STMT_PROC / STMT_ENDP */
    char            proc_name[128];

    /* STMT_UNWIND */
    char            unwind_directive[32];
    x64_operand_t   unwind_operands[2];
    int             unwind_operand_count;

    /* STMT_SECTION */
    char            section_name[64];
    uint32_t        section_flags;  /* SCN flags */

    /* STMT_DATA (db/dw/dd/dq) */
    int             data_size;      /* 1, 2, 4, 8 */
    data_item_t     data_items[256];
    int             data_count;

    /* STMT_EXTERN / STMT_GLOBAL */
    char            symbol[128];

    /* STMT_EQU */
    char            equ_name[128];
    int64_t         equ_value;

    /* STMT_ALIGN / STMT_RESV */
    int64_t         align_value;
    int             resv_size;      /* 1=resb, 2=resw, 4=resd, 8=resq */
} asm_stmt_t;

/* ---- Statement list ---- */
typedef struct {
    asm_stmt_t *stmts;
    int         count;
    int         capacity;
} asm_stmt_list_t;

/* ---- API ---- */
asm_stmt_list_t *asm_parse(asm_token_list_t *tokens);
void asm_stmt_list_free(asm_stmt_list_t *list);
void asm_stmt_dump(const asm_stmt_t *stmt);
void asm_stmt_list_dump(const asm_stmt_list_t *list);

#endif
