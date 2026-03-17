/*==========================================================================
 * x64_encoder.c — Complete x64 instruction encoder
 *
 * Encodes instructions to machine code using Intel opcode maps.
 * Supports the most common x64 instructions needed by a real assembler.
 *
 * Encoding strategy:
 *   1. Determine REX prefix need (64-bit operands, extended regs)
 *   2. Emit legacy prefixes (66h for 16-bit, F2/F3 for REP)
 *   3. Emit opcode bytes (1-3 bytes)
 *   4. Construct ModR/M + SIB as needed
 *   5. Emit displacement (0, 1, or 4 bytes)
 *   6. Emit immediate (1, 2, 4, or 8 bytes)
 *=========================================================================*/
#include "x64_encoder.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ---- Helpers ---- */
static int reg_needs_rex(x64_reg_t r) { return r >= REG_R8 && r <= REG_R15; }
static uint8_t reg_lo3(x64_reg_t r) { return (uint8_t)(r & 7); }

static void emit8(x64_encoded_t *e, uint8_t v) {
    if (e->len < 15) e->bytes[e->len++] = v;
}
static void emit16(x64_encoded_t *e, uint16_t v) {
    emit8(e, (uint8_t)(v & 0xFF));
    emit8(e, (uint8_t)((v >> 8) & 0xFF));
}
static void emit32(x64_encoded_t *e, uint32_t v) {
    emit8(e, (uint8_t)(v & 0xFF));
    emit8(e, (uint8_t)((v >> 8) & 0xFF));
    emit8(e, (uint8_t)((v >> 16) & 0xFF));
    emit8(e, (uint8_t)((v >> 24) & 0xFF));
}
static void emit64(x64_encoded_t *e, uint64_t v) {
    emit32(e, (uint32_t)(v & 0xFFFFFFFF));
    emit32(e, (uint32_t)((v >> 32) & 0xFFFFFFFF));
}

/* ---- REX prefix builder ----
 * REX = 0100 W R X B
 *   W=1: 64-bit operand size
 *   R=1: extends ModR/M reg field
 *   X=1: extends SIB index field
 *   B=1: extends ModR/M rm or SIB base field
 */
static void emit_rex(x64_encoded_t *e, int w, int r_ext, int x_ext, int b_ext) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (r_ext) rex |= 0x04;
    if (x_ext) rex |= 0x02;
    if (b_ext) rex |= 0x01;
    emit8(e, rex);
}

/* ---- ModR/M byte builder ---- */
static void emit_modrm(x64_encoded_t *e, uint8_t mod, uint8_t reg, uint8_t rm) {
    emit8(e, (uint8_t)((mod << 6) | ((reg & 7) << 3) | (rm & 7)));
}

/* ---- SIB byte builder ---- */
static void emit_sib(x64_encoded_t *e, uint8_t scale, uint8_t index, uint8_t base) {
    uint8_t s = 0;
    switch (scale) {
        case 1: s = 0; break;
        case 2: s = 1; break;
        case 4: s = 2; break;
        case 8: s = 3; break;
    }
    emit8(e, (uint8_t)((s << 6) | ((index & 7) << 3) | (base & 7)));
}

/*--------------------------------------------------------------------------
 * Encode memory operand into ModR/M + SIB + displacement
 * reg_field = the /r or /digit value for the ModR/M reg bits
 * Returns REX.B and REX.X bits needed
 *-------------------------------------------------------------------------*/
static void encode_mem(x64_encoded_t *e, mem_operand_t *m, uint8_t reg_field,
                       int *rex_b, int *rex_x) {
    *rex_b = 0;
    *rex_x = 0;

    /* RIP-relative: mod=00, rm=101, no SIB */
    if (m->base == REG_NONE && m->index == REG_NONE) {
        emit_modrm(e, 0x00, reg_field, 0x05);
        emit32(e, (uint32_t)m->disp);
        return;
    }

    int need_sib = (m->index != REG_NONE) ||
                   (m->base == REG_RSP) || (m->base == REG_R12);

    if (m->base != REG_NONE && reg_needs_rex(m->base)) *rex_b = 1;
    if (m->index != REG_NONE && reg_needs_rex(m->index)) *rex_x = 1;

    uint8_t mod;
    if (!m->has_disp && m->disp == 0 &&
        m->base != REG_RBP && m->base != REG_R13) {
        mod = 0x00;
    } else if (m->disp >= -128 && m->disp <= 127) {
        mod = 0x01;
    } else {
        mod = 0x02;
    }

    /* Special: [disp32] with SIB (no base) */
    if (m->base == REG_NONE && m->index != REG_NONE) {
        emit_modrm(e, 0x00, reg_field, 0x04);
        emit_sib(e, m->scale, reg_lo3(m->index), 0x05);
        emit32(e, (uint32_t)m->disp);
        return;
    }

    if (need_sib) {
        emit_modrm(e, mod, reg_field, 0x04);
        uint8_t idx = (m->index != REG_NONE) ? reg_lo3(m->index) : 0x04; /* 0x04=no index */
        emit_sib(e, (m->index != REG_NONE) ? m->scale : 1,
                 idx, reg_lo3(m->base));
    } else {
        emit_modrm(e, mod, reg_field, reg_lo3(m->base));
    }

    if (mod == 0x01) emit8(e, (uint8_t)(m->disp & 0xFF));
    else if (mod == 0x02) emit32(e, (uint32_t)m->disp);
}

/*--------------------------------------------------------------------------
 * Main instruction encoder — covers the essential x64 instruction set
 *-------------------------------------------------------------------------*/
x64_encoded_t x64_encode(x64_mnemonic_t mnem, x64_operand_t *op1, x64_operand_t *op2) {
    x64_encoded_t enc;
    memset(&enc, 0, sizeof(enc));

    /* Shorthand checks */
    int has1 = op1 && op1->type != OP_NONE;
    int has2 = op2 && op2->type != OP_NONE;

    /* ============================================================
     * NOP / RET / single-byte instructions
     * ============================================================ */
    switch (mnem) {
    case MNEM_NOP:   emit8(&enc, 0x90); return enc;
    case MNEM_RET:
        if (has1 && op1->type == OP_IMM) {
            emit8(&enc, 0xC2);
            emit16(&enc, (uint16_t)op1->imm);
        } else {
            emit8(&enc, 0xC3);
        }
        return enc;
    case MNEM_CLC:     emit8(&enc, 0xF8); return enc;
    case MNEM_STC:     emit8(&enc, 0xF9); return enc;
    case MNEM_CLD:     emit8(&enc, 0xFC); return enc;
    case MNEM_STD:     emit8(&enc, 0xFD); return enc;
    case MNEM_SYSCALL: emit8(&enc, 0x0F); emit8(&enc, 0x05); return enc;
    case MNEM_CPUID:   emit8(&enc, 0x0F); emit8(&enc, 0xA2); return enc;
    case MNEM_RDTSC:   emit8(&enc, 0x0F); emit8(&enc, 0x31); return enc;
    case MNEM_CDQ:     emit8(&enc, 0x99); return enc;
    case MNEM_CQO:
        emit_rex(&enc, 1, 0, 0, 0);
        emit8(&enc, 0x99);
        return enc;
    case MNEM_INT:
        if (has1 && op1->type == OP_IMM) {
            if (op1->imm == 3) {
                emit8(&enc, 0xCC);
            } else {
                emit8(&enc, 0xCD);
                emit8(&enc, (uint8_t)op1->imm);
            }
        }
        return enc;
    default:
        break;
    }

    /* ============================================================
     * PUSH / POP — register or immediate
     * ============================================================ */
    if (mnem == MNEM_PUSH) {
        if (has1 && op1->type == OP_REG) {
            if (reg_needs_rex(op1->reg))
                emit_rex(&enc, 0, 0, 0, 1);
            emit8(&enc, (uint8_t)(0x50 + reg_lo3(op1->reg)));
        } else if (has1 && op1->type == OP_IMM) {
            if (op1->imm >= -128 && op1->imm <= 127) {
                emit8(&enc, 0x6A);
                emit8(&enc, (uint8_t)(op1->imm & 0xFF));
            } else {
                emit8(&enc, 0x68);
                emit32(&enc, (uint32_t)op1->imm);
            }
        }
        return enc;
    }
    if (mnem == MNEM_POP) {
        if (has1 && op1->type == OP_REG) {
            if (reg_needs_rex(op1->reg))
                emit_rex(&enc, 0, 0, 0, 1);
            emit8(&enc, (uint8_t)(0x58 + reg_lo3(op1->reg)));
        }
        return enc;
    }

    /* ============================================================
     * MOV — the swiss army knife
     * ============================================================ */
    if (mnem == MNEM_MOV) {
        /* MOV reg, imm */
        if (has1 && has2 && op1->type == OP_REG && op2->type == OP_IMM) {
            x64_reg_t r = op1->reg;
            int64_t imm = op2->imm;
            operand_size_t sz = op1->size;

            if (sz == SZ_BYTE) {
                if (reg_needs_rex(r)) emit_rex(&enc, 0, 0, 0, 1);
                emit8(&enc, (uint8_t)(0xB0 + reg_lo3(r)));
                emit8(&enc, (uint8_t)imm);
            } else if (sz == SZ_WORD) {
                emit8(&enc, 0x66);
                if (reg_needs_rex(r)) emit_rex(&enc, 0, 0, 0, 1);
                emit8(&enc, (uint8_t)(0xB8 + reg_lo3(r)));
                emit16(&enc, (uint16_t)imm);
            } else if (sz == SZ_DWORD) {
                if (reg_needs_rex(r)) emit_rex(&enc, 0, 0, 0, 1);
                emit8(&enc, (uint8_t)(0xB8 + reg_lo3(r)));
                emit32(&enc, (uint32_t)imm);
            } else { /* QWORD */
                /* If value fits in 32-bit sign-extended, use MOV r/m64, imm32 (C7 /0) */
                if (imm >= (int64_t)-2147483648LL && imm <= (int64_t)2147483647LL) {
                    emit_rex(&enc, 1, 0, 0, reg_needs_rex(r));
                    emit8(&enc, 0xC7);
                    emit_modrm(&enc, 0x03, 0, reg_lo3(r));
                    emit32(&enc, (uint32_t)imm);
                } else {
                    /* Full 64-bit MOV r64, imm64 */
                    emit_rex(&enc, 1, 0, 0, reg_needs_rex(r));
                    emit8(&enc, (uint8_t)(0xB8 + reg_lo3(r)));
                    emit64(&enc, (uint64_t)imm);
                }
            }
            return enc;
        }

        /* MOV reg, reg */
        if (has1 && has2 && op1->type == OP_REG && op2->type == OP_REG) {
            operand_size_t sz = op1->size;
            if (sz == SZ_WORD) emit8(&enc, 0x66);
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int r_ext = reg_needs_rex(op2->reg);
            int b_ext = reg_needs_rex(op1->reg);
            if (w || r_ext || b_ext || sz == SZ_BYTE)
                emit_rex(&enc, w, r_ext, 0, b_ext);
            emit8(&enc, (sz == SZ_BYTE) ? 0x88 : 0x89);
            emit_modrm(&enc, 0x03, reg_lo3(op2->reg), reg_lo3(op1->reg));
            return enc;
        }

        /* MOV reg, [mem] */
        if (has1 && has2 && op1->type == OP_REG && op2->type == OP_MEM) {
            operand_size_t sz = op1->size;
            if (sz == SZ_WORD) emit8(&enc, 0x66);
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int r_ext = reg_needs_rex(op1->reg);
            /* pre-calculate rex_b, rex_x */
            int rex_b = 0, rex_x = 0;
            if (op2->mem.base != REG_NONE && reg_needs_rex(op2->mem.base)) rex_b = 1;
            if (op2->mem.index != REG_NONE && reg_needs_rex(op2->mem.index)) rex_x = 1;
            if (w || r_ext || rex_x || rex_b || sz == SZ_BYTE)
                emit_rex(&enc, w, r_ext, rex_x, rex_b);
            emit8(&enc, (sz == SZ_BYTE) ? 0x8A : 0x8B);
            int dummy_b, dummy_x;
            encode_mem(&enc, &op2->mem, reg_lo3(op1->reg), &dummy_b, &dummy_x);
            return enc;
        }

        /* MOV [mem], reg */
        if (has1 && has2 && op1->type == OP_MEM && op2->type == OP_REG) {
            operand_size_t sz = op2->size;
            if (sz == SZ_WORD) emit8(&enc, 0x66);
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int r_ext = reg_needs_rex(op2->reg);
            int rex_b = 0, rex_x = 0;
            if (op1->mem.base != REG_NONE && reg_needs_rex(op1->mem.base)) rex_b = 1;
            if (op1->mem.index != REG_NONE && reg_needs_rex(op1->mem.index)) rex_x = 1;
            if (w || r_ext || rex_x || rex_b || sz == SZ_BYTE)
                emit_rex(&enc, w, r_ext, rex_x, rex_b);
            emit8(&enc, (sz == SZ_BYTE) ? 0x88 : 0x89);
            int dummy_b, dummy_x;
            encode_mem(&enc, &op1->mem, reg_lo3(op2->reg), &dummy_b, &dummy_x);
            return enc;
        }

        /* MOV [mem], imm */
        if (has1 && has2 && op1->type == OP_MEM && op2->type == OP_IMM) {
            operand_size_t sz = op1->size;
            if (sz == SZ_WORD) emit8(&enc, 0x66);
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int rex_b = 0, rex_x = 0;
            if (op1->mem.base != REG_NONE && reg_needs_rex(op1->mem.base)) rex_b = 1;
            if (op1->mem.index != REG_NONE && reg_needs_rex(op1->mem.index)) rex_x = 1;
            if (w || rex_x || rex_b)
                emit_rex(&enc, w, 0, rex_x, rex_b);
            emit8(&enc, (sz == SZ_BYTE) ? 0xC6 : 0xC7);
            int dummy_b, dummy_x;
            encode_mem(&enc, &op1->mem, 0, &dummy_b, &dummy_x);
            if (sz == SZ_BYTE) emit8(&enc, (uint8_t)op2->imm);
            else if (sz == SZ_WORD) emit16(&enc, (uint16_t)op2->imm);
            else emit32(&enc, (uint32_t)op2->imm);
            return enc;
        }

        /* MOV reg, label (RIP-relative) */
        if (has1 && has2 && op1->type == OP_REG && op2->type == OP_LABEL) {
            int w = (op1->size == SZ_QWORD) ? 1 : 0;
            int r_ext = reg_needs_rex(op1->reg);
            if (w || r_ext) emit_rex(&enc, w, r_ext, 0, 0);
            emit8(&enc, 0x8B);
            emit_modrm(&enc, 0x00, reg_lo3(op1->reg), 0x05); /* RIP-relative */
            enc.reloc_offset = (uint32_t)enc.len;
            enc.reloc_type = 4; /* IMAGE_REL_AMD64_REL32 */
            strncpy(enc.reloc_symbol, op2->label, 127);
            emit32(&enc, 0); /* placeholder */
            return enc;
        }

        return enc;
    }

    /* ============================================================
     * LEA reg, [mem]
     * ============================================================ */
    if (mnem == MNEM_LEA && has1 && has2 && op1->type == OP_REG && op2->type == OP_MEM) {
        operand_size_t sz = op1->size;
        if (sz == SZ_WORD) emit8(&enc, 0x66);
        int w = (sz == SZ_QWORD) ? 1 : 0;
        int r_ext = reg_needs_rex(op1->reg);
        int rex_b = 0, rex_x = 0;
        if (op2->mem.base != REG_NONE && reg_needs_rex(op2->mem.base)) rex_b = 1;
        if (op2->mem.index != REG_NONE && reg_needs_rex(op2->mem.index)) rex_x = 1;
        if (w || r_ext || rex_x || rex_b)
            emit_rex(&enc, w, r_ext, rex_x, rex_b);
        emit8(&enc, 0x8D);
        int dummy_b, dummy_x;
        encode_mem(&enc, &op2->mem, reg_lo3(op1->reg), &dummy_b, &dummy_x);
        return enc;
    }

    /* ============================================================
     * ALU group: ADD, SUB, AND, OR, XOR, CMP, TEST
     * Opcodes follow the standard ALU encoding pattern:
     *   ADD=0, OR=1, ADC=2, SBB=3, AND=4, SUB=5, XOR=6, CMP=7
     * ============================================================ */
    {
        int alu_group = -1;
        switch (mnem) {
            case MNEM_ADD: alu_group = 0; break;
            case MNEM_OR:  alu_group = 1; break;
            case MNEM_AND: alu_group = 4; break;
            case MNEM_SUB: alu_group = 5; break;
            case MNEM_XOR: alu_group = 6; break;
            case MNEM_CMP: alu_group = 7; break;
            default: break;
        }
        if (alu_group >= 0 && has1 && has2) {
            /* ALU reg, imm */
            if (op1->type == OP_REG && op2->type == OP_IMM) {
                operand_size_t sz = op1->size;
                if (sz == SZ_WORD) emit8(&enc, 0x66);
                int w = (sz == SZ_QWORD) ? 1 : 0;
                int b_ext = reg_needs_rex(op1->reg);
                if (w || b_ext) emit_rex(&enc, w, 0, 0, b_ext);

                /* Short form for AL/AX/EAX/RAX */
                if (op1->reg == REG_RAX && sz != SZ_BYTE &&
                    !(op2->imm >= -128 && op2->imm <= 127)) {
                    emit8(&enc, (uint8_t)(alu_group * 8 + 5));
                    if (sz == SZ_WORD) emit16(&enc, (uint16_t)op2->imm);
                    else emit32(&enc, (uint32_t)op2->imm);
                    return enc;
                }
                if (op1->reg == REG_RAX && sz == SZ_BYTE) {
                    emit8(&enc, (uint8_t)(alu_group * 8 + 4));
                    emit8(&enc, (uint8_t)op2->imm);
                    return enc;
                }

                /* imm8 sign-extended form: 83 /digit ib */
                if (sz != SZ_BYTE && op2->imm >= -128 && op2->imm <= 127) {
                    emit8(&enc, 0x83);
                    emit_modrm(&enc, 0x03, (uint8_t)alu_group, reg_lo3(op1->reg));
                    emit8(&enc, (uint8_t)(op2->imm & 0xFF));
                } else if (sz == SZ_BYTE) {
                    emit8(&enc, 0x80);
                    emit_modrm(&enc, 0x03, (uint8_t)alu_group, reg_lo3(op1->reg));
                    emit8(&enc, (uint8_t)op2->imm);
                } else {
                    emit8(&enc, 0x81);
                    emit_modrm(&enc, 0x03, (uint8_t)alu_group, reg_lo3(op1->reg));
                    if (sz == SZ_WORD) emit16(&enc, (uint16_t)op2->imm);
                    else emit32(&enc, (uint32_t)op2->imm);
                }
                return enc;
            }

            /* ALU reg, reg */
            if (op1->type == OP_REG && op2->type == OP_REG) {
                operand_size_t sz = op1->size;
                if (sz == SZ_WORD) emit8(&enc, 0x66);
                int w = (sz == SZ_QWORD) ? 1 : 0;
                int r_ext = reg_needs_rex(op2->reg);
                int b_ext = reg_needs_rex(op1->reg);
                if (w || r_ext || b_ext || sz == SZ_BYTE)
                    emit_rex(&enc, w, r_ext, 0, b_ext);
                uint8_t base_op = (sz == SZ_BYTE)
                    ? (uint8_t)(alu_group * 8 + 0)
                    : (uint8_t)(alu_group * 8 + 1);
                emit8(&enc, base_op);
                emit_modrm(&enc, 0x03, reg_lo3(op2->reg), reg_lo3(op1->reg));
                return enc;
            }

            /* ALU reg, [mem] */
            if (op1->type == OP_REG && op2->type == OP_MEM) {
                operand_size_t sz = op1->size;
                if (sz == SZ_WORD) emit8(&enc, 0x66);
                int w = (sz == SZ_QWORD) ? 1 : 0;
                int r_ext = reg_needs_rex(op1->reg);
                int rex_b = 0, rex_x = 0;
                if (op2->mem.base != REG_NONE && reg_needs_rex(op2->mem.base)) rex_b = 1;
                if (op2->mem.index != REG_NONE && reg_needs_rex(op2->mem.index)) rex_x = 1;
                if (w || r_ext || rex_x || rex_b || sz == SZ_BYTE)
                    emit_rex(&enc, w, r_ext, rex_x, rex_b);
                uint8_t base_op = (sz == SZ_BYTE)
                    ? (uint8_t)(alu_group * 8 + 2)
                    : (uint8_t)(alu_group * 8 + 3);
                emit8(&enc, base_op);
                int db, dx;
                encode_mem(&enc, &op2->mem, reg_lo3(op1->reg), &db, &dx);
                return enc;
            }

            /* ALU [mem], reg */
            if (op1->type == OP_MEM && op2->type == OP_REG) {
                operand_size_t sz = op2->size;
                if (sz == SZ_WORD) emit8(&enc, 0x66);
                int w = (sz == SZ_QWORD) ? 1 : 0;
                int r_ext = reg_needs_rex(op2->reg);
                int rex_b = 0, rex_x = 0;
                if (op1->mem.base != REG_NONE && reg_needs_rex(op1->mem.base)) rex_b = 1;
                if (op1->mem.index != REG_NONE && reg_needs_rex(op1->mem.index)) rex_x = 1;
                if (w || r_ext || rex_x || rex_b || sz == SZ_BYTE)
                    emit_rex(&enc, w, r_ext, rex_x, rex_b);
                uint8_t base_op = (sz == SZ_BYTE)
                    ? (uint8_t)(alu_group * 8 + 0)
                    : (uint8_t)(alu_group * 8 + 1);
                emit8(&enc, base_op);
                int db, dx;
                encode_mem(&enc, &op1->mem, reg_lo3(op2->reg), &db, &dx);
                return enc;
            }

            /* ALU [mem], imm */
            if (op1->type == OP_MEM && op2->type == OP_IMM) {
                operand_size_t sz = op1->size;
                if (sz == SZ_WORD) emit8(&enc, 0x66);
                int w = (sz == SZ_QWORD) ? 1 : 0;
                int rex_b = 0, rex_x = 0;
                if (op1->mem.base != REG_NONE && reg_needs_rex(op1->mem.base)) rex_b = 1;
                if (op1->mem.index != REG_NONE && reg_needs_rex(op1->mem.index)) rex_x = 1;
                if (w || rex_x || rex_b)
                    emit_rex(&enc, w, 0, rex_x, rex_b);
                if (sz != SZ_BYTE && op2->imm >= -128 && op2->imm <= 127) {
                    emit8(&enc, 0x83);
                } else if (sz == SZ_BYTE) {
                    emit8(&enc, 0x80);
                } else {
                    emit8(&enc, 0x81);
                }
                int db, dx;
                encode_mem(&enc, &op1->mem, (uint8_t)alu_group, &db, &dx);
                if (sz == SZ_BYTE) emit8(&enc, (uint8_t)op2->imm);
                else if (sz != SZ_BYTE && op2->imm >= -128 && op2->imm <= 127)
                    emit8(&enc, (uint8_t)(op2->imm & 0xFF));
                else if (sz == SZ_WORD) emit16(&enc, (uint16_t)op2->imm);
                else emit32(&enc, (uint32_t)op2->imm);
                return enc;
            }
        }
    }

    /* ============================================================
     * TEST — special encoding separate from ALU group
     * ============================================================ */
    if (mnem == MNEM_TEST && has1 && has2) {
        if (op1->type == OP_REG && op2->type == OP_REG) {
            operand_size_t sz = op1->size;
            if (sz == SZ_WORD) emit8(&enc, 0x66);
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int r_ext = reg_needs_rex(op2->reg);
            int b_ext = reg_needs_rex(op1->reg);
            if (w || r_ext || b_ext || sz == SZ_BYTE)
                emit_rex(&enc, w, r_ext, 0, b_ext);
            emit8(&enc, (sz == SZ_BYTE) ? 0x84 : 0x85);
            emit_modrm(&enc, 0x03, reg_lo3(op2->reg), reg_lo3(op1->reg));
            return enc;
        }
        if (op1->type == OP_REG && op2->type == OP_IMM) {
            operand_size_t sz = op1->size;
            if (sz == SZ_WORD) emit8(&enc, 0x66);
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int b_ext = reg_needs_rex(op1->reg);
            if (w || b_ext || sz == SZ_BYTE)
                emit_rex(&enc, w, 0, 0, b_ext);
            if (op1->reg == REG_RAX && sz == SZ_BYTE) {
                emit8(&enc, 0xA8);
            } else if (op1->reg == REG_RAX) {
                emit8(&enc, 0xA9);
            } else {
                emit8(&enc, (sz == SZ_BYTE) ? 0xF6 : 0xF7);
                emit_modrm(&enc, 0x03, 0, reg_lo3(op1->reg));
            }
            if (sz == SZ_BYTE) emit8(&enc, (uint8_t)op2->imm);
            else if (sz == SZ_WORD) emit16(&enc, (uint16_t)op2->imm);
            else emit32(&enc, (uint32_t)op2->imm);
            return enc;
        }
    }

    /* ============================================================
     * INC / DEC / NEG / NOT — unary r/m
     * ============================================================ */
    if ((mnem == MNEM_INC || mnem == MNEM_DEC || mnem == MNEM_NEG || mnem == MNEM_NOT)
         && has1) {
        uint8_t digit;
        switch (mnem) {
            case MNEM_INC: digit = 0; break;
            case MNEM_DEC: digit = 1; break;
            case MNEM_NOT: digit = 2; break;
            case MNEM_NEG: digit = 3; break;
            default: digit = 0; break;
        }

        if (op1->type == OP_REG) {
            operand_size_t sz = op1->size;
            if (sz == SZ_WORD) emit8(&enc, 0x66);
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int b_ext = reg_needs_rex(op1->reg);
            if (w || b_ext || sz == SZ_BYTE)
                emit_rex(&enc, w, 0, 0, b_ext);
            emit8(&enc, (sz == SZ_BYTE) ? 0xF6 : 0xF7);
            emit_modrm(&enc, 0x03, digit, reg_lo3(op1->reg));
            return enc;
        }
        if (op1->type == OP_MEM) {
            operand_size_t sz = op1->size;
            if (sz == SZ_WORD) emit8(&enc, 0x66);
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int rex_b = 0, rex_x = 0;
            if (op1->mem.base != REG_NONE && reg_needs_rex(op1->mem.base)) rex_b = 1;
            if (op1->mem.index != REG_NONE && reg_needs_rex(op1->mem.index)) rex_x = 1;
            if (w || rex_x || rex_b)
                emit_rex(&enc, w, 0, rex_x, rex_b);
            emit8(&enc, (sz == SZ_BYTE) ? 0xF6 : 0xF7);
            int db, dx;
            encode_mem(&enc, &op1->mem, digit, &db, &dx);
            return enc;
        }
    }

    /* ============================================================
     * IMUL — 2 and 3 operand forms
     * ============================================================ */
    if (mnem == MNEM_IMUL) {
        /* IMUL r/m (single operand — F7 /5) */
        if (has1 && !has2 && op1->type == OP_REG) {
            operand_size_t sz = op1->size;
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int b_ext = reg_needs_rex(op1->reg);
            if (w || b_ext) emit_rex(&enc, w, 0, 0, b_ext);
            emit8(&enc, 0xF7);
            emit_modrm(&enc, 0x03, 5, reg_lo3(op1->reg));
            return enc;
        }
        /* IMUL reg, reg/mem — 0F AF */
        if (has1 && has2 && op1->type == OP_REG && op2->type == OP_REG) {
            operand_size_t sz = op1->size;
            if (sz == SZ_WORD) emit8(&enc, 0x66);
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int r_ext = reg_needs_rex(op1->reg);
            int b_ext = reg_needs_rex(op2->reg);
            if (w || r_ext || b_ext) emit_rex(&enc, w, r_ext, 0, b_ext);
            emit8(&enc, 0x0F); emit8(&enc, 0xAF);
            emit_modrm(&enc, 0x03, reg_lo3(op1->reg), reg_lo3(op2->reg));
            return enc;
        }
        /* IMUL reg, imm — 6B or 69 */
        if (has1 && has2 && op1->type == OP_REG && op2->type == OP_IMM) {
            operand_size_t sz = op1->size;
            if (sz == SZ_WORD) emit8(&enc, 0x66);
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int r_ext = reg_needs_rex(op1->reg);
            int b_ext = reg_needs_rex(op1->reg);
            if (w || r_ext || b_ext) emit_rex(&enc, w, r_ext, 0, b_ext);
            if (op2->imm >= -128 && op2->imm <= 127) {
                emit8(&enc, 0x6B);
                emit_modrm(&enc, 0x03, reg_lo3(op1->reg), reg_lo3(op1->reg));
                emit8(&enc, (uint8_t)(op2->imm & 0xFF));
            } else {
                emit8(&enc, 0x69);
                emit_modrm(&enc, 0x03, reg_lo3(op1->reg), reg_lo3(op1->reg));
                emit32(&enc, (uint32_t)op2->imm);
            }
            return enc;
        }
    }

    /* MUL / DIV / IDIV — single operand F7 */
    if ((mnem == MNEM_MUL || mnem == MNEM_DIV || mnem == MNEM_IDIV) && has1) {
        uint8_t digit;
        switch (mnem) {
            case MNEM_MUL:  digit = 4; break;
            case MNEM_DIV:  digit = 6; break;
            case MNEM_IDIV: digit = 7; break;
            default: digit = 4; break;
        }
        if (op1->type == OP_REG) {
            operand_size_t sz = op1->size;
            int w = (sz == SZ_QWORD) ? 1 : 0;
            int b_ext = reg_needs_rex(op1->reg);
            if (w || b_ext || sz == SZ_BYTE)
                emit_rex(&enc, w, 0, 0, b_ext);
            emit8(&enc, (sz == SZ_BYTE) ? 0xF6 : 0xF7);
            emit_modrm(&enc, 0x03, digit, reg_lo3(op1->reg));
            return enc;
        }
    }

    /* ============================================================
     * Shift group: SHL, SHR, SAR, ROL, ROR
     * Opcode C1 (r/m, imm8) or D3 (r/m, CL)
     * ============================================================ */
    {
        int shift_digit = -1;
        switch (mnem) {
            case MNEM_ROL: shift_digit = 0; break;
            case MNEM_ROR: shift_digit = 1; break;
            case MNEM_SHL: shift_digit = 4; break;
            case MNEM_SHR: shift_digit = 5; break;
            case MNEM_SAR: shift_digit = 7; break;
            default: break;
        }
        if (shift_digit >= 0 && has1) {
            if (op1->type == OP_REG) {
                operand_size_t sz = op1->size;
                if (sz == SZ_WORD) emit8(&enc, 0x66);
                int w = (sz == SZ_QWORD) ? 1 : 0;
                int b_ext = reg_needs_rex(op1->reg);
                if (w || b_ext || sz == SZ_BYTE)
                    emit_rex(&enc, w, 0, 0, b_ext);

                if (has2 && op2->type == OP_IMM) {
                    if (op2->imm == 1) {
                        emit8(&enc, (sz == SZ_BYTE) ? 0xD0 : 0xD1);
                    } else {
                        emit8(&enc, (sz == SZ_BYTE) ? 0xC0 : 0xC1);
                    }
                    emit_modrm(&enc, 0x03, (uint8_t)shift_digit, reg_lo3(op1->reg));
                    if (op2->imm != 1) emit8(&enc, (uint8_t)op2->imm);
                } else if (has2 && op2->type == OP_REG && op2->reg == REG_RCX) {
                    /* shift by CL */
                    emit8(&enc, (sz == SZ_BYTE) ? 0xD2 : 0xD3);
                    emit_modrm(&enc, 0x03, (uint8_t)shift_digit, reg_lo3(op1->reg));
                }
                return enc;
            }
        }
    }

    /* ============================================================
     * XCHG reg, reg
     * ============================================================ */
    if (mnem == MNEM_XCHG && has1 && has2 && op1->type == OP_REG && op2->type == OP_REG) {
        operand_size_t sz = op1->size;
        if (sz == SZ_WORD) emit8(&enc, 0x66);
        int w = (sz == SZ_QWORD) ? 1 : 0;
        /* Short form xchg rax, r */
        if (op1->reg == REG_RAX && sz != SZ_BYTE) {
            if (w || reg_needs_rex(op2->reg))
                emit_rex(&enc, w, 0, 0, reg_needs_rex(op2->reg));
            emit8(&enc, (uint8_t)(0x90 + reg_lo3(op2->reg)));
        } else {
            int r_ext = reg_needs_rex(op1->reg);
            int b_ext = reg_needs_rex(op2->reg);
            if (w || r_ext || b_ext || sz == SZ_BYTE)
                emit_rex(&enc, w, r_ext, 0, b_ext);
            emit8(&enc, (sz == SZ_BYTE) ? 0x86 : 0x87);
            emit_modrm(&enc, 0x03, reg_lo3(op1->reg), reg_lo3(op2->reg));
        }
        return enc;
    }

    /* ============================================================
     * MOVZX / MOVSX — zero/sign extend
     * ============================================================ */
    if ((mnem == MNEM_MOVZX || mnem == MNEM_MOVSX) && has1 && has2 &&
         op1->type == OP_REG && op2->type == OP_REG) {
        operand_size_t dst_sz = op1->size;
        operand_size_t src_sz = op2->size;
        int w = (dst_sz == SZ_QWORD) ? 1 : 0;
        int r_ext = reg_needs_rex(op1->reg);
        int b_ext = reg_needs_rex(op2->reg);

        if (mnem == MNEM_MOVSX && src_sz == SZ_DWORD) {
            /* MOVSXD r64, r32 — opcode 63 */
            emit_rex(&enc, 1, r_ext, 0, b_ext);
            emit8(&enc, 0x63);
            emit_modrm(&enc, 0x03, reg_lo3(op1->reg), reg_lo3(op2->reg));
        } else {
            if (w || r_ext || b_ext) emit_rex(&enc, w, r_ext, 0, b_ext);
            emit8(&enc, 0x0F);
            if (mnem == MNEM_MOVZX) {
                emit8(&enc, (src_sz == SZ_BYTE) ? 0xB6 : 0xB7);
            } else {
                emit8(&enc, (src_sz == SZ_BYTE) ? 0xBE : 0xBF);
            }
            emit_modrm(&enc, 0x03, reg_lo3(op1->reg), reg_lo3(op2->reg));
        }
        return enc;
    }

    /* ============================================================
     * JMP / CALL — near relative or register indirect
     * ============================================================ */
    if (mnem == MNEM_JMP || mnem == MNEM_CALL) {
        /* JMP/CALL label */
        if (has1 && op1->type == OP_LABEL) {
            emit8(&enc, (mnem == MNEM_JMP) ? 0xE9 : 0xE8);
            enc.reloc_offset = (uint32_t)enc.len;
            enc.reloc_type = 4; /* REL32 */
            strncpy(enc.reloc_symbol, op1->label, 127);
            emit32(&enc, 0);
            return enc;
        }
        /* JMP/CALL imm (relative offset known) */
        if (has1 && op1->type == OP_IMM) {
            emit8(&enc, (mnem == MNEM_JMP) ? 0xE9 : 0xE8);
            emit32(&enc, (uint32_t)op1->imm);
            return enc;
        }
        /* JMP/CALL reg */
        if (has1 && op1->type == OP_REG) {
            int b_ext = reg_needs_rex(op1->reg);
            if (b_ext) emit_rex(&enc, 0, 0, 0, b_ext);
            emit8(&enc, 0xFF);
            emit_modrm(&enc, 0x03, (mnem == MNEM_JMP) ? 4 : 2, reg_lo3(op1->reg));
            return enc;
        }
        /* JMP/CALL [mem] */
        if (has1 && op1->type == OP_MEM) {
            int rex_b = 0, rex_x = 0;
            if (op1->mem.base != REG_NONE && reg_needs_rex(op1->mem.base)) rex_b = 1;
            if (op1->mem.index != REG_NONE && reg_needs_rex(op1->mem.index)) rex_x = 1;
            if (rex_b || rex_x) emit_rex(&enc, 0, 0, rex_x, rex_b);
            emit8(&enc, 0xFF);
            int db, dx;
            encode_mem(&enc, &op1->mem, (mnem == MNEM_JMP) ? 4 : 2, &db, &dx);
            return enc;
        }
    }

    /* ============================================================
     * Conditional jumps (Jcc)
     * Near form: 0F 8x rel32
     * Short form: 7x rel8
     * We always emit near form; the linker/user can optimize later
     * ============================================================ */
    {
        int cc = -1;
        switch (mnem) {
            case MNEM_JO:  cc = 0x0; break;
            case MNEM_JNO: cc = 0x1; break;
            case MNEM_JB:  cc = 0x2; break;
            case MNEM_JAE: cc = 0x3; break;
            case MNEM_JE: case MNEM_JZ:  cc = 0x4; break;
            case MNEM_JNE: case MNEM_JNZ: cc = 0x5; break;
            case MNEM_JBE: cc = 0x6; break;
            case MNEM_JA:  cc = 0x7; break;
            case MNEM_JS:  cc = 0x8; break;
            case MNEM_JNS: cc = 0x9; break;
            case MNEM_JL:  cc = 0xC; break;
            case MNEM_JGE: cc = 0xD; break;
            case MNEM_JLE: cc = 0xE; break;
            case MNEM_JG:  cc = 0xF; break;
            default: break;
        }
        if (cc >= 0 && has1) {
            if (op1->type == OP_LABEL) {
                emit8(&enc, 0x0F);
                emit8(&enc, (uint8_t)(0x80 + cc));
                enc.reloc_offset = (uint32_t)enc.len;
                enc.reloc_type = 4; /* REL32 */
                strncpy(enc.reloc_symbol, op1->label, 127);
                emit32(&enc, 0);
            } else if (op1->type == OP_IMM) {
                /* Known offset */
                if (op1->imm >= -128 && op1->imm <= 127) {
                    emit8(&enc, (uint8_t)(0x70 + cc));
                    emit8(&enc, (uint8_t)(op1->imm & 0xFF));
                } else {
                    emit8(&enc, 0x0F);
                    emit8(&enc, (uint8_t)(0x80 + cc));
                    emit32(&enc, (uint32_t)op1->imm);
                }
            }
            return enc;
        }
    }

    /* Unhandled instruction */
    return enc;
}

/* ============================================================
 * Mnemonic string parser
 * ============================================================ */
typedef struct { const char *name; x64_mnemonic_t mnem; } mnem_entry_t;
static const mnem_entry_t mnem_table[] = {
    {"mov", MNEM_MOV}, {"movzx", MNEM_MOVZX}, {"movsx", MNEM_MOVSX},
    {"movsxd", MNEM_MOVSX}, {"lea", MNEM_LEA}, {"xchg", MNEM_XCHG},
    {"push", MNEM_PUSH}, {"pop", MNEM_POP},
    {"add", MNEM_ADD}, {"sub", MNEM_SUB}, {"mul", MNEM_MUL},
    {"imul", MNEM_IMUL}, {"div", MNEM_DIV}, {"idiv", MNEM_IDIV},
    {"inc", MNEM_INC}, {"dec", MNEM_DEC}, {"neg", MNEM_NEG}, {"not", MNEM_NOT},
    {"and", MNEM_AND}, {"or", MNEM_OR}, {"xor", MNEM_XOR}, {"test", MNEM_TEST},
    {"shl", MNEM_SHL}, {"shr", MNEM_SHR}, {"sar", MNEM_SAR},
    {"sal", MNEM_SHL}, /* SAL = SHL */
    {"rol", MNEM_ROL}, {"ror", MNEM_ROR},
    {"cmp", MNEM_CMP},
    {"jmp", MNEM_JMP}, {"call", MNEM_CALL}, {"ret", MNEM_RET}, {"int", MNEM_INT},
    {"je", MNEM_JE}, {"jne", MNEM_JNE}, {"jz", MNEM_JZ}, {"jnz", MNEM_JNZ},
    {"ja", MNEM_JA}, {"jae", MNEM_JAE}, {"jb", MNEM_JB}, {"jbe", MNEM_JBE},
    {"jg", MNEM_JG}, {"jge", MNEM_JGE}, {"jl", MNEM_JL}, {"jle", MNEM_JLE},
    {"js", MNEM_JS}, {"jns", MNEM_JNS}, {"jo", MNEM_JO}, {"jno", MNEM_JNO},
    {"nop", MNEM_NOP}, {"clc", MNEM_CLC}, {"stc", MNEM_STC},
    {"cld", MNEM_CLD}, {"std", MNEM_STD},
    {"syscall", MNEM_SYSCALL}, {"cpuid", MNEM_CPUID}, {"rdtsc", MNEM_RDTSC},
    {"cdq", MNEM_CDQ}, {"cqo", MNEM_CQO},
    {NULL, MNEM_UNKNOWN}
};

x64_mnemonic_t x64_parse_mnemonic(const char *s) {
    char lower[32];
    int i;
    for (i = 0; s[i] && i < 31; i++) lower[i] = (char)tolower((unsigned char)s[i]);
    lower[i] = '\0';
    for (i = 0; mnem_table[i].name; i++) {
        if (strcmp(lower, mnem_table[i].name) == 0) return mnem_table[i].mnem;
    }
    return MNEM_UNKNOWN;
}

const char *x64_mnemonic_name(x64_mnemonic_t m) {
    for (int i = 0; mnem_table[i].name; i++) {
        if (mnem_table[i].mnem == m) return mnem_table[i].name;
    }
    return "???";
}

/* ============================================================
 * Register string parser
 * Returns register ID and deduces operand size
 * ============================================================ */
typedef struct { const char *name; x64_reg_t reg; operand_size_t size; } reg_entry_t;
static const reg_entry_t reg_table[] = {
    /* 64-bit */
    {"rax", REG_RAX, SZ_QWORD}, {"rcx", REG_RCX, SZ_QWORD},
    {"rdx", REG_RDX, SZ_QWORD}, {"rbx", REG_RBX, SZ_QWORD},
    {"rsp", REG_RSP, SZ_QWORD}, {"rbp", REG_RBP, SZ_QWORD},
    {"rsi", REG_RSI, SZ_QWORD}, {"rdi", REG_RDI, SZ_QWORD},
    {"r8",  REG_R8,  SZ_QWORD}, {"r9",  REG_R9,  SZ_QWORD},
    {"r10", REG_R10, SZ_QWORD}, {"r11", REG_R11, SZ_QWORD},
    {"r12", REG_R12, SZ_QWORD}, {"r13", REG_R13, SZ_QWORD},
    {"r14", REG_R14, SZ_QWORD}, {"r15", REG_R15, SZ_QWORD},
    /* 32-bit */
    {"eax", REG_RAX, SZ_DWORD}, {"ecx", REG_RCX, SZ_DWORD},
    {"edx", REG_RDX, SZ_DWORD}, {"ebx", REG_RBX, SZ_DWORD},
    {"esp", REG_RSP, SZ_DWORD}, {"ebp", REG_RBP, SZ_DWORD},
    {"esi", REG_RSI, SZ_DWORD}, {"edi", REG_RDI, SZ_DWORD},
    {"r8d",  REG_R8,  SZ_DWORD}, {"r9d",  REG_R9,  SZ_DWORD},
    {"r10d", REG_R10, SZ_DWORD}, {"r11d", REG_R11, SZ_DWORD},
    {"r12d", REG_R12, SZ_DWORD}, {"r13d", REG_R13, SZ_DWORD},
    {"r14d", REG_R14, SZ_DWORD}, {"r15d", REG_R15, SZ_DWORD},
    /* 16-bit */
    {"ax", REG_RAX, SZ_WORD}, {"cx", REG_RCX, SZ_WORD},
    {"dx", REG_RDX, SZ_WORD}, {"bx", REG_RBX, SZ_WORD},
    {"sp", REG_RSP, SZ_WORD}, {"bp", REG_RBP, SZ_WORD},
    {"si", REG_RSI, SZ_WORD}, {"di", REG_RDI, SZ_WORD},
    {"r8w",  REG_R8,  SZ_WORD}, {"r9w",  REG_R9,  SZ_WORD},
    {"r10w", REG_R10, SZ_WORD}, {"r11w", REG_R11, SZ_WORD},
    {"r12w", REG_R12, SZ_WORD}, {"r13w", REG_R13, SZ_WORD},
    {"r14w", REG_R14, SZ_WORD}, {"r15w", REG_R15, SZ_WORD},
    /* 8-bit */
    {"al", REG_RAX, SZ_BYTE}, {"cl", REG_RCX, SZ_BYTE},
    {"dl", REG_RDX, SZ_BYTE}, {"bl", REG_RBX, SZ_BYTE},
    {"spl", REG_RSP, SZ_BYTE}, {"bpl", REG_RBP, SZ_BYTE},
    {"sil", REG_RSI, SZ_BYTE}, {"dil", REG_RDI, SZ_BYTE},
    {"r8b",  REG_R8,  SZ_BYTE}, {"r9b",  REG_R9,  SZ_BYTE},
    {"r10b", REG_R10, SZ_BYTE}, {"r11b", REG_R11, SZ_BYTE},
    {"r12b", REG_R12, SZ_BYTE}, {"r13b", REG_R13, SZ_BYTE},
    {"r14b", REG_R14, SZ_BYTE}, {"r15b", REG_R15, SZ_BYTE},
    /* Legacy 8-bit (no REX) */
    {"ah", REG_RSP, SZ_BYTE}, {"ch", REG_RBP, SZ_BYTE},
    {"dh", REG_RSI, SZ_BYTE}, {"bh", REG_RDI, SZ_BYTE},
    {NULL, REG_NONE, SZ_BYTE}
};

x64_reg_t x64_parse_register(const char *s, operand_size_t *out_size) {
    char lower[16];
    int i;
    for (i = 0; s[i] && i < 15; i++) lower[i] = (char)tolower((unsigned char)s[i]);
    lower[i] = '\0';
    for (i = 0; reg_table[i].name; i++) {
        if (strcmp(lower, reg_table[i].name) == 0) {
            if (out_size) *out_size = reg_table[i].size;
            return reg_table[i].reg;
        }
    }
    if (out_size) *out_size = SZ_QWORD;
    return REG_NONE;
}
