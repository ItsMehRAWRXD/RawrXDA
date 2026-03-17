/*=============================================================================
 * RawrXD PE32+ Backend — pe_emitter.c
 * Monolithic implementation — no demos, no stubs
 *
 * Build:
 *   cl.exe /O2 /W4 /c pe_emitter.c
 *   (Link into your tool with: link pe_emitter.obj your_main.obj)
 *
 * Or as static lib:
 *   lib /OUT:pe_emitter.lib pe_emitter.obj
 *===========================================================================*/

#include "pe_emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * DOS Stub
 *---------------------------------------------------------------------------*/
const uint8_t PE_DOS_STUB[PE_DOS_STUB_SIZE] = {
    0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,
    0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x54, 0x68,
    0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72,
    0x61, 0x6D, 0x20, 0x63, 0x61, 0x6E, 0x6E, 0x6F,
    0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E,
    0x20, 0x69, 0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20,
    0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A,
    0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/

static uint32_t align_up(uint32_t v, uint32_t a) {
    return (v + a - 1) & ~(a - 1);
}

static void set_error(Emitter *e, const char *msg) {
    e->error = -1;
    strncpy(e->error_msg, msg, sizeof(e->error_msg) - 1);
    e->error_msg[sizeof(e->error_msg) - 1] = '\0';
}

/* REX prefix builder for r/m64 operations */
static uint8_t rex_w(uint8_t reg, uint8_t rm) {
    uint8_t r = 0x48;
    if (reg >= 8) r |= 0x04;  /* REX.R */
    if (rm >= 8)  r |= 0x01;  /* REX.B */
    return r;
}

static uint8_t rex_wrm(uint8_t reg, uint8_t rm) {
    return rex_w(reg, rm);
}

static uint8_t modrm(uint8_t mod, uint8_t reg, uint8_t rm) {
    return (uint8_t)((mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

/* Emit ModR/M + optional SIB + displacement for [base+disp] */
static void emit_modrm_mem(Emitter *e, uint8_t reg, uint8_t base, int32_t disp) {
    uint8_t b = base & 7;
    uint8_t r = reg & 7;

    if (b == 4) {
        /* RSP/R12 needs SIB byte */
        if (disp == 0 && b != 5) {
            em_byte(e, modrm(0, r, 4));
            em_byte(e, 0x24); /* SIB: index=none, base=RSP */
        } else if (disp >= -128 && disp <= 127) {
            em_byte(e, modrm(1, r, 4));
            em_byte(e, 0x24);
            em_byte(e, (uint8_t)(int8_t)disp);
        } else {
            em_byte(e, modrm(2, r, 4));
            em_byte(e, 0x24);
            em_u32(e, (uint32_t)disp);
        }
    } else if (b == 5 && disp == 0) {
        /* RBP/R13 with disp=0 needs [rbp+0] encoding */
        em_byte(e, modrm(1, r, b));
        em_byte(e, 0);
    } else if (disp == 0) {
        em_byte(e, modrm(0, r, b));
    } else if (disp >= -128 && disp <= 127) {
        em_byte(e, modrm(1, r, b));
        em_byte(e, (uint8_t)(int8_t)disp);
    } else {
        em_byte(e, modrm(2, r, b));
        em_u32(e, (uint32_t)disp);
    }
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

void em_init(Emitter *e) {
    memset(e, 0, sizeof(*e));
    e->image_base          = 0x0000000140000000ULL;
    e->section_alignment   = 0x1000;
    e->file_alignment      = 0x200;
    e->subsystem           = PE_SUBSYSTEM_WINDOWS_CUI;
    e->dll_characteristics = PE_DLLCHAR_DYNAMIC_BASE | PE_DLLCHAR_NX_COMPAT
                           | PE_DLLCHAR_HIGH_ENTROPY_VA | PE_DLLCHAR_TERMINAL_SERVER;
    e->characteristics     = PE_CHAR_EXECUTABLE_IMAGE | PE_CHAR_LARGE_ADDRESS_AWARE;
    e->stack_reserve       = 0x100000;
    e->stack_commit        = 0x1000;
    e->heap_reserve        = 0x100000;
    e->heap_commit         = 0x1000;
    e->entry_point_label   = 0xFFFFFFFF; /* Not set */
}

void em_init_dll(Emitter *e, const char *module_name) {
    em_init(e);
    e->characteristics |= PE_CHAR_DLL;
    if (module_name) {
        strncpy(e->export_module_name, module_name, 63);
    }
}

void em_reset(Emitter *e) {
    uint64_t ib = e->image_base;
    uint32_t sa = e->section_alignment;
    uint32_t fa = e->file_alignment;
    uint16_t ss = e->subsystem;
    uint16_t dc = e->dll_characteristics;
    uint16_t ch = e->characteristics;
    uint64_t sr = e->stack_reserve;
    uint64_t sc = e->stack_commit;
    uint64_t hr = e->heap_reserve;
    uint64_t hc = e->heap_commit;

    memset(e, 0, sizeof(*e));

    e->image_base          = ib;
    e->section_alignment   = sa;
    e->file_alignment      = fa;
    e->subsystem           = ss;
    e->dll_characteristics = dc;
    e->characteristics     = ch;
    e->stack_reserve       = sr;
    e->stack_commit        = sc;
    e->heap_reserve        = hr;
    e->heap_commit         = hc;
    e->entry_point_label   = 0xFFFFFFFF;
}

/*===========================================================================
 * Configuration
 *===========================================================================*/

void em_set_image_base(Emitter *e, uint64_t base)      { e->image_base = base; }
void em_set_subsystem(Emitter *e, uint16_t sub)         { e->subsystem = sub; }
void em_set_alignment(Emitter *e, uint32_t sa, uint32_t fa) {
    e->section_alignment = sa;
    e->file_alignment = fa;
}
void em_set_stack(Emitter *e, uint64_t r, uint64_t c)   { e->stack_reserve = r; e->stack_commit = c; }
void em_set_heap(Emitter *e, uint64_t r, uint64_t c)    { e->heap_reserve = r; e->heap_commit = c; }
void em_set_dll_characteristics(Emitter *e, uint16_t f) { e->dll_characteristics = f; }

void em_set_entry_label(Emitter *e, const char *name) {
    int idx = em_find_label(e, name);
    if (idx >= 0) {
        e->entry_point_label = (uint32_t)idx;
    }
    /* If not yet defined, pe_layout will search by name at resolve time */
}

/*===========================================================================
 * Raw Emission
 *===========================================================================*/

void em_byte(Emitter *e, uint8_t b) {
    if (e->code_len < EM_MAX_CODE_SIZE) {
        e->code[e->code_len++] = b;
    } else {
        set_error(e, "Code buffer overflow");
    }
}

void em_bytes(Emitter *e, const uint8_t *buf, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) em_byte(e, buf[i]);
}

void em_u16(Emitter *e, uint16_t v) {
    em_byte(e, (uint8_t)(v));
    em_byte(e, (uint8_t)(v >> 8));
}

void em_u32(Emitter *e, uint32_t v) {
    em_byte(e, (uint8_t)(v));
    em_byte(e, (uint8_t)(v >> 8));
    em_byte(e, (uint8_t)(v >> 16));
    em_byte(e, (uint8_t)(v >> 24));
}

void em_u64(Emitter *e, uint64_t v) {
    em_u32(e, (uint32_t)v);
    em_u32(e, (uint32_t)(v >> 32));
}

void em_align(Emitter *e, uint32_t alignment) {
    while (e->code_len & (alignment - 1)) em_nop(e);
}

uint32_t em_pos(const Emitter *e) {
    return e->code_len;
}

/*===========================================================================
 * Data Sections
 *===========================================================================*/

uint32_t em_rdata_bytes(Emitter *e, const void *buf, uint32_t len) {
    uint32_t off = e->rdata_len;
    if (e->rdata_len + len > EM_MAX_DATA_SIZE) {
        set_error(e, ".rdata overflow");
        return off;
    }
    memcpy(e->rdata + e->rdata_len, buf, len);
    e->rdata_len += len;
    return off;
}

uint32_t em_rdata_string(Emitter *e, const char *str) {
    uint32_t off = em_rdata_bytes(e, str, (uint32_t)strlen(str) + 1);
    if (e->rdata_len & 1) e->rdata_len++; /* Align to 2 */
    return off;
}

uint32_t em_rdata_wstring(Emitter *e, const uint16_t *wstr, uint32_t charcount) {
    return em_rdata_bytes(e, wstr, charcount * 2);
}

uint32_t em_rdata_u32(Emitter *e, uint32_t v) {
    while (e->rdata_len & 3) e->rdata_len++; /* Align 4 */
    return em_rdata_bytes(e, &v, 4);
}

uint32_t em_rdata_u64(Emitter *e, uint64_t v) {
    while (e->rdata_len & 7) e->rdata_len++; /* Align 8 */
    return em_rdata_bytes(e, &v, 8);
}

uint32_t em_rwdata_bytes(Emitter *e, const void *buf, uint32_t len) {
    uint32_t off = e->rwdata_len;
    if (e->rwdata_len + len > EM_MAX_DATA_SIZE) {
        set_error(e, ".data overflow");
        return off;
    }
    memcpy(e->rwdata + e->rwdata_len, buf, len);
    e->rwdata_len += len;
    return off;
}

uint32_t em_rwdata_zero(Emitter *e, uint32_t len) {
    uint32_t off = e->rwdata_len;
    if (e->rwdata_len + len > EM_MAX_DATA_SIZE) {
        set_error(e, ".data overflow");
        return off;
    }
    memset(e->rwdata + e->rwdata_len, 0, len);
    e->rwdata_len += len;
    return off;
}

uint32_t em_bss_reserve(Emitter *e, uint32_t len) {
    uint32_t off = e->bss_size;
    e->bss_size += len;
    return off;
}

/*===========================================================================
 * Labels
 *===========================================================================*/

void em_label(Emitter *e, const char *name) {
    /* Check for existing forward-reference placeholder */
    for (uint32_t i = 0; i < e->label_count; i++) {
        if (strcmp(e->labels[i].name, name) == 0 && e->labels[i].offset == 0xFFFFFFFF) {
            e->labels[i].offset = e->code_len;
            return;
        }
    }
    if (e->label_count >= EM_MAX_LABELS) {
        set_error(e, "Label limit exceeded");
        return;
    }
    strncpy(e->labels[e->label_count].name, name, 63);
    e->labels[e->label_count].name[63] = '\0';
    e->labels[e->label_count].offset = e->code_len;
    e->labels[e->label_count].exported = 0;
    e->label_count++;
}

void em_label_export(Emitter *e, const char *name) {
    em_label(e, name);
    if (e->label_count > 0) {
        e->labels[e->label_count - 1].exported = 1;
    }
    if (e->export_count < EM_MAX_EXPORTS) {
        strncpy(e->exports[e->export_count].name, name, 63);
        e->exports[e->export_count].code_offset = e->code_len;
        e->export_count++;
    }
}

int em_find_label(const Emitter *e, const char *name) {
    for (uint32_t i = 0; i < e->label_count; i++) {
        if (strcmp(e->labels[i].name, name) == 0) return (int)i;
    }
    return -1;
}

uint32_t em_label_offset(const Emitter *e, int idx) {
    if (idx < 0 || (uint32_t)idx >= e->label_count) return 0;
    return e->labels[idx].offset;
}

/*===========================================================================
 * Imports
 *===========================================================================*/

uint32_t em_import(Emitter *e, const char *dll, const char *func) {
    int dll_idx = -1;
    for (uint32_t i = 0; i < e->import_dll_count; i++) {
        if (_stricmp(e->imports[i].dll_name, dll) == 0) {
            dll_idx = (int)i;
            break;
        }
    }
    if (dll_idx < 0) {
        if (e->import_dll_count >= EM_MAX_IMPORT_DLLS) {
            set_error(e, "Import DLL limit exceeded");
            return 0;
        }
        dll_idx = (int)e->import_dll_count++;
        strncpy(e->imports[dll_idx].dll_name, dll, 63);
        e->imports[dll_idx].func_count = 0;
    }

    EmImportDll *d = &e->imports[dll_idx];
    for (uint32_t i = 0; i < d->func_count; i++) {
        if (strcmp(d->func_names[i], func) == 0)
            return (uint32_t)((dll_idx << 16) | i);
    }

    if (d->func_count >= EM_MAX_IMPORTS) {
        set_error(e, "Import function limit exceeded");
        return 0;
    }
    uint32_t fi = d->func_count++;
    strncpy(d->func_names[fi], func, 63);
    return (uint32_t)((dll_idx << 16) | fi);
}

/*===========================================================================
 * Fixup-Emitting Instructions
 *===========================================================================*/

static void add_fixup(Emitter *e, EmFixupKind kind, uint32_t patch_off, uint32_t target, int32_t addend) {
    if (e->fixup_count >= EM_MAX_FIXUPS) {
        set_error(e, "Fixup limit exceeded");
        return;
    }
    EmFixup *f = &e->fixups[e->fixup_count++];
    f->kind = kind;
    f->patch_offset = patch_off;
    f->target_index = target;
    f->addend = addend;
}

void em_call_import(Emitter *e, uint32_t import_id) {
    /* FF 15 disp32 — CALL QWORD PTR [RIP+disp32] */
    em_byte(e, 0xFF);
    em_byte(e, 0x15);
    uint32_t patch = e->code_len;
    em_u32(e, 0);
    add_fixup(e, EM_FIXUP_RIP_REL32_IAT, patch, import_id, 0);
}

void em_jmp_import(Emitter *e, uint32_t import_id) {
    /* FF 25 disp32 — JMP QWORD PTR [RIP+disp32] */
    em_byte(e, 0xFF);
    em_byte(e, 0x25);
    uint32_t patch = e->code_len;
    em_u32(e, 0);
    add_fixup(e, EM_FIXUP_RIP_REL32_IAT, patch, import_id, 0);
}

void em_lea_reg_rip_rdata(Emitter *e, uint8_t reg, uint32_t rdata_offset) {
    /* 48 8D <modrm> disp32 — LEA reg, [RIP+disp32] */
    if (reg >= 8) {
        em_byte(e, 0x4C); /* REX.WR */
    } else {
        em_byte(e, 0x48); /* REX.W */
    }
    em_byte(e, 0x8D);
    em_byte(e, modrm(0, reg & 7, 5)); /* mod=00, rm=101 = RIP-relative */
    uint32_t patch = e->code_len;
    em_u32(e, 0);
    add_fixup(e, EM_FIXUP_RIP_REL32_DATA, patch, rdata_offset, 0);
}

void em_lea_rN_rip_rdata(Emitter *e, uint8_t reg_n, uint32_t rdata_offset) {
    em_lea_reg_rip_rdata(e, reg_n, rdata_offset);
}

void em_call_label(Emitter *e, const char *label_name) {
    /* E8 rel32 */
    em_byte(e, 0xE8);
    uint32_t patch = e->code_len;
    em_u32(e, 0);
    if (e->fixup_count < EM_MAX_FIXUPS) {
        EmFixup *f = &e->fixups[e->fixup_count++];
        f->kind = EM_FIXUP_LABEL_REL32;
        f->patch_offset = patch;
        f->addend = 0;
        int idx = em_find_label(e, label_name);
        if (idx >= 0) {
            f->target_index = (uint32_t)idx;
        } else {
            /* Forward reference — add pending label placeholder */
            if (e->label_count < EM_MAX_LABELS) {
                strncpy(e->labels[e->label_count].name, label_name, 63);
                e->labels[e->label_count].name[63] = '\0';
                e->labels[e->label_count].offset = 0xFFFFFFFF; /* unresolved */
                f->target_index = e->label_count;
                e->label_count++;
            }
        }
    }
}

void em_jmp_label(Emitter *e, const char *label_name) {
    /* E9 rel32 */
    em_byte(e, 0xE9);
    uint32_t patch = e->code_len;
    em_u32(e, 0);
    if (e->fixup_count < EM_MAX_FIXUPS) {
        EmFixup *f = &e->fixups[e->fixup_count++];
        f->kind = EM_FIXUP_LABEL_REL32;
        f->patch_offset = patch;
        f->addend = 0;
        int idx = em_find_label(e, label_name);
        if (idx >= 0) {
            f->target_index = (uint32_t)idx;
        } else {
            if (e->label_count < EM_MAX_LABELS) {
                strncpy(e->labels[e->label_count].name, label_name, 63);
                e->labels[e->label_count].name[63] = '\0';
                e->labels[e->label_count].offset = 0xFFFFFFFF;
                f->target_index = e->label_count;
                e->label_count++;
            }
        }
    }
}

void em_jcc_label(Emitter *e, uint8_t cc, const char *label_name) {
    /* 0F 8x rel32 — near conditional jump */
    em_byte(e, 0x0F);
    em_byte(e, cc);
    uint32_t patch = e->code_len;
    em_u32(e, 0);
    if (e->fixup_count < EM_MAX_FIXUPS) {
        EmFixup *f = &e->fixups[e->fixup_count++];
        f->kind = EM_FIXUP_LABEL_REL32;
        f->patch_offset = patch;
        f->addend = 0;
        int idx = em_find_label(e, label_name);
        if (idx >= 0) {
            f->target_index = (uint32_t)idx;
        } else {
            if (e->label_count < EM_MAX_LABELS) {
                strncpy(e->labels[e->label_count].name, label_name, 63);
                e->labels[e->label_count].name[63] = '\0';
                e->labels[e->label_count].offset = 0xFFFFFFFF;
                f->target_index = e->label_count;
                e->label_count++;
            }
        }
    }
}

void em_mov_reg_abs_rdata(Emitter *e, uint8_t reg, uint32_t rdata_offset) {
    /* MOV reg, imm64 with absolute fixup */
    if (reg >= 8) {
        em_byte(e, 0x49);
    } else {
        em_byte(e, 0x48);
    }
    em_byte(e, (uint8_t)(0xB8 + (reg & 7)));
    uint32_t patch = e->code_len;
    em_u64(e, 0);
    add_fixup(e, EM_FIXUP_ABS64_DATA, patch, rdata_offset, 0);
    /* Register as needing base relocation */
    if (e->reloc_count < EM_MAX_RELOCS) {
        e->reloc_rvas[e->reloc_count++] = patch;
    }
}

/*===========================================================================
 * x64 Instruction Emitters
 *===========================================================================*/

/* ── Prologue / Epilogue ─────────────────────────────────────────────────── */

void em_prologue(Emitter *e, uint32_t local_bytes) {
    uint32_t total = (local_bytes + 0x28 + 15) & ~15u; /* 16-align + 32 shadow */
    em_push_r64(e, REG_RBP);
    em_mov_r64_r64(e, REG_RBP, REG_RSP);
    em_sub_r64_imm32(e, REG_RSP, (int32_t)total);
}

void em_epilogue(Emitter *e) {
    /* mov rsp, rbp */
    em_byte(e, 0x48); em_byte(e, 0x89); em_byte(e, modrm(3, REG_RBP, REG_RSP));
    /* pop rbp */
    em_pop_r64(e, REG_RBP);
    /* ret */
    em_ret(e);
}

void em_epilogue_noframe(Emitter *e, uint32_t local_bytes) {
    uint32_t total = (local_bytes + 0x28 + 15) & ~15u;
    em_add_r64_imm32(e, REG_RSP, (int32_t)total);
    em_ret(e);
}

/* ── Register-Register ALU ────────────────────────────────────────────────── */

static void emit_alu_r64_r64(Emitter *e, uint8_t opcode, uint8_t dst, uint8_t src) {
    em_byte(e, rex_wrm(src, dst));
    em_byte(e, opcode);
    em_byte(e, modrm(3, src & 7, dst & 7));
}

void em_mov_r64_r64(Emitter *e, uint8_t dst, uint8_t src)  { emit_alu_r64_r64(e, 0x89, dst, src); }
void em_add_r64_r64(Emitter *e, uint8_t dst, uint8_t src)  { emit_alu_r64_r64(e, 0x01, dst, src); }
void em_sub_r64_r64(Emitter *e, uint8_t dst, uint8_t src)  { emit_alu_r64_r64(e, 0x29, dst, src); }
void em_cmp_r64_r64(Emitter *e, uint8_t dst, uint8_t src)  { emit_alu_r64_r64(e, 0x39, dst, src); }
void em_test_r64_r64(Emitter *e, uint8_t dst, uint8_t src) { emit_alu_r64_r64(e, 0x85, dst, src); }
void em_and_r64_r64(Emitter *e, uint8_t dst, uint8_t src)  { emit_alu_r64_r64(e, 0x21, dst, src); }
void em_or_r64_r64(Emitter *e, uint8_t dst, uint8_t src)   { emit_alu_r64_r64(e, 0x09, dst, src); }

void em_xor_r32_r32(Emitter *e, uint8_t dst, uint8_t src) {
    /* No REX.W — operates on 32-bit regs, zeros upper 32 */
    uint8_t prefix = 0;
    if (dst >= 8 || src >= 8) {
        prefix = 0x40;
        if (src >= 8) prefix |= 0x04;
        if (dst >= 8) prefix |= 0x01;
        em_byte(e, prefix);
    }
    em_byte(e, 0x31);
    em_byte(e, modrm(3, src & 7, dst & 7));
}

void em_imul_r64_r64(Emitter *e, uint8_t dst, uint8_t src) {
    em_byte(e, rex_wrm(dst, src));
    em_byte(e, 0x0F);
    em_byte(e, 0xAF);
    em_byte(e, modrm(3, dst & 7, src & 7));
}

/* ── Register-Immediate ──────────────────────────────────────────────────── */

void em_mov_r64_imm64(Emitter *e, uint8_t reg, uint64_t imm) {
    if (reg >= 8) {
        em_byte(e, 0x49);
    } else {
        em_byte(e, 0x48);
    }
    em_byte(e, (uint8_t)(0xB8 + (reg & 7)));
    em_u64(e, imm);
}

void em_mov_r32_imm32(Emitter *e, uint8_t reg, uint32_t imm) {
    if (reg >= 8) {
        em_byte(e, 0x41);
    }
    em_byte(e, (uint8_t)(0xB8 + (reg & 7)));
    em_u32(e, imm);
}

static void emit_alu_r64_imm32(Emitter *e, uint8_t subop, uint8_t reg, int32_t imm) {
    em_byte(e, reg >= 8 ? 0x49 : 0x48);
    if (imm >= -128 && imm <= 127) {
        em_byte(e, 0x83);
        em_byte(e, modrm(3, subop, reg & 7));
        em_byte(e, (uint8_t)(int8_t)imm);
    } else {
        em_byte(e, 0x81);
        em_byte(e, modrm(3, subop, reg & 7));
        em_u32(e, (uint32_t)imm);
    }
}

void em_add_r64_imm32(Emitter *e, uint8_t reg, int32_t imm) { emit_alu_r64_imm32(e, 0, reg, imm); }
void em_sub_r64_imm32(Emitter *e, uint8_t reg, int32_t imm) { emit_alu_r64_imm32(e, 5, reg, imm); }
void em_cmp_r64_imm32(Emitter *e, uint8_t reg, int32_t imm) { emit_alu_r64_imm32(e, 7, reg, imm); }
void em_and_r64_imm32(Emitter *e, uint8_t reg, int32_t imm) { emit_alu_r64_imm32(e, 4, reg, imm); }
void em_or_r64_imm32(Emitter *e, uint8_t reg, int32_t imm)  { emit_alu_r64_imm32(e, 1, reg, imm); }

static void emit_shift_r64_imm8(Emitter *e, uint8_t subop, uint8_t reg, uint8_t count) {
    em_byte(e, reg >= 8 ? 0x49 : 0x48);
    em_byte(e, 0xC1);
    em_byte(e, modrm(3, subop, reg & 7));
    em_byte(e, count);
}

void em_shl_r64_imm8(Emitter *e, uint8_t reg, uint8_t count) { emit_shift_r64_imm8(e, 4, reg, count); }
void em_shr_r64_imm8(Emitter *e, uint8_t reg, uint8_t count) { emit_shift_r64_imm8(e, 5, reg, count); }
void em_sar_r64_imm8(Emitter *e, uint8_t reg, uint8_t count) { emit_shift_r64_imm8(e, 7, reg, count); }

/* ── Memory Operations ───────────────────────────────────────────────────── */

void em_mov_r64_m64(Emitter *e, uint8_t reg, uint8_t base, int32_t disp) {
    em_byte(e, rex_wrm(reg, base));
    em_byte(e, 0x8B);
    emit_modrm_mem(e, reg, base, disp);
}

void em_mov_m64_r64(Emitter *e, uint8_t base, int32_t disp, uint8_t reg) {
    em_byte(e, rex_wrm(reg, base));
    em_byte(e, 0x89);
    emit_modrm_mem(e, reg, base, disp);
}

void em_mov_r32_m32(Emitter *e, uint8_t reg, uint8_t base, int32_t disp) {
    uint8_t prefix = 0;
    if (reg >= 8 || base >= 8) {
        prefix = 0x40;
        if (reg >= 8) prefix |= 0x04;
        if (base >= 8) prefix |= 0x01;
        em_byte(e, prefix);
    }
    em_byte(e, 0x8B);
    emit_modrm_mem(e, reg, base, disp);
}

void em_mov_m32_r32(Emitter *e, uint8_t base, int32_t disp, uint8_t reg) {
    uint8_t prefix = 0;
    if (reg >= 8 || base >= 8) {
        prefix = 0x40;
        if (reg >= 8) prefix |= 0x04;
        if (base >= 8) prefix |= 0x01;
        em_byte(e, prefix);
    }
    em_byte(e, 0x89);
    emit_modrm_mem(e, reg, base, disp);
}

void em_mov_r8_m8(Emitter *e, uint8_t reg, uint8_t base, int32_t disp) {
    if (reg >= 4 || base >= 8) {
        uint8_t prefix = 0x40;
        if (reg >= 8) prefix |= 0x04;
        if (base >= 8) prefix |= 0x01;
        em_byte(e, prefix);
    }
    em_byte(e, 0x8A);
    emit_modrm_mem(e, reg, base, disp);
}

void em_mov_m8_r8(Emitter *e, uint8_t base, int32_t disp, uint8_t reg) {
    if (reg >= 4 || base >= 8) {
        uint8_t prefix = 0x40;
        if (reg >= 8) prefix |= 0x04;
        if (base >= 8) prefix |= 0x01;
        em_byte(e, prefix);
    }
    em_byte(e, 0x88);
    emit_modrm_mem(e, reg, base, disp);
}

void em_lea_r64_m(Emitter *e, uint8_t reg, uint8_t base, int32_t disp) {
    em_byte(e, rex_wrm(reg, base));
    em_byte(e, 0x8D);
    emit_modrm_mem(e, reg, base, disp);
}

/* ── Stack ────────────────────────────────────────────────────────────────── */

void em_push_r64(Emitter *e, uint8_t reg) {
    if (reg >= 8) em_byte(e, 0x41);
    em_byte(e, (uint8_t)(0x50 + (reg & 7)));
}

void em_pop_r64(Emitter *e, uint8_t reg) {
    if (reg >= 8) em_byte(e, 0x41);
    em_byte(e, (uint8_t)(0x58 + (reg & 7)));
}

void em_push_imm32(Emitter *e, int32_t imm) {
    if (imm >= -128 && imm <= 127) {
        em_byte(e, 0x6A);
        em_byte(e, (uint8_t)(int8_t)imm);
    } else {
        em_byte(e, 0x68);
        em_u32(e, (uint32_t)imm);
    }
}

/* ── Control Flow ─────────────────────────────────────────────────────────── */

void em_ret(Emitter *e)  { em_byte(e, 0xC3); }
void em_nop(Emitter *e)  { em_byte(e, 0x90); }
void em_int3(Emitter *e) { em_byte(e, 0xCC); }

void em_call_r64(Emitter *e, uint8_t reg) {
    if (reg >= 8) em_byte(e, 0x41);
    em_byte(e, 0xFF);
    em_byte(e, modrm(3, 2, reg & 7));
}

void em_jmp_r64(Emitter *e, uint8_t reg) {
    if (reg >= 8) em_byte(e, 0x41);
    em_byte(e, 0xFF);
    em_byte(e, modrm(3, 4, reg & 7));
}

void em_syscall(Emitter *e) {
    em_byte(e, 0x0F);
    em_byte(e, 0x05);
}

void em_nop_sled(Emitter *e, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) em_nop(e);
}

/*===========================================================================
 * PE Layout + Write
 *===========================================================================*/

int pe_layout(Emitter *e) {
    if (e->error) return -1;

    const uint32_t SA = e->section_alignment;
    const uint32_t FA = e->file_alignment;

    /* Count sections */
    uint32_t num_sections = 1; /* .text always */
    int has_rdata = (e->rdata_len > 0);
    int has_rwdata = (e->rwdata_len > 0);
    int has_bss = (e->bss_size > 0);
    int has_idata = (e->import_dll_count > 0);
    int has_reloc = (e->reloc_count > 0);
    int has_edata = (e->export_count > 0);

    if (has_rdata) num_sections++;
    if (has_rwdata) num_sections++;
    if (has_bss) num_sections++;
    if (has_idata) num_sections++;
    if (has_reloc) num_sections++;
    if (has_edata) num_sections++;

    /* Headers */
    uint32_t headers_raw = (uint32_t)(sizeof(PE_DOS_HEADER) + PE_DOS_STUB_SIZE
                          + 4 + sizeof(PE_COFF_HEADER)
                          + sizeof(PE_OPTIONAL_HEADER_64)
                          + PE_NUM_DATA_DIRECTORIES * sizeof(PE_DATA_DIRECTORY)
                          + num_sections * sizeof(PE_SECTION_HEADER));
    e->headers_size = align_up(headers_raw, FA);

    /* Section RVAs */
    uint32_t cur_rva = SA;

    /* .text */
    e->text_rva = cur_rva;
    cur_rva += align_up(e->code_len > 0 ? e->code_len : 1, SA);

    /* .rdata */
    if (has_rdata) {
        e->rdata_rva = cur_rva;
        cur_rva += align_up(e->rdata_len, SA);
    }

    /* .data */
    if (has_rwdata) {
        e->rwdata_rva = cur_rva;
        cur_rva += align_up(e->rwdata_len, SA);
    }

    /* .bss */
    if (has_bss) {
        e->bss_rva = cur_rva;
        cur_rva += align_up(e->bss_size, SA);
    }

    /* .idata layout */
    uint32_t idata_virt_size = 0;
    uint32_t idt_size = 0, ilt_offset = 0, ilt_size = 0;
    uint32_t iat_offset = 0, iat_size = 0;
    uint32_t hn_offset = 0, hn_size = 0;
    uint32_t names_offset = 0, names_size = 0;

    if (has_idata) {
        e->idata_rva = cur_rva;

        idt_size = (e->import_dll_count + 1) * (uint32_t)sizeof(PE_IMPORT_DESCRIPTOR);
        ilt_offset = idt_size;
        ilt_size = 0;
        for (uint32_t i = 0; i < e->import_dll_count; i++)
            ilt_size += (e->imports[i].func_count + 1) * 8;

        iat_offset = ilt_offset + ilt_size;
        iat_size = ilt_size;

        hn_offset = iat_offset + iat_size;
        hn_size = 0;
        for (uint32_t i = 0; i < e->import_dll_count; i++) {
            for (uint32_t j = 0; j < e->imports[i].func_count; j++) {
                hn_size += 2 + (uint32_t)strlen(e->imports[i].func_names[j]) + 1;
                if (hn_size & 1) hn_size++;
            }
        }

        names_offset = hn_offset + hn_size;
        names_size = 0;
        for (uint32_t i = 0; i < e->import_dll_count; i++)
            names_size += (uint32_t)strlen(e->imports[i].dll_name) + 1;

        idata_virt_size = names_offset + names_size;
        cur_rva += align_up(idata_virt_size > 0 ? idata_virt_size : 1, SA);
    }

    /* .edata (export directory) */
    if (has_edata) {
        e->edata_rva = cur_rva;
        cur_rva += SA;
    }

    /* .reloc */
    if (has_reloc) {
        e->reloc_rva = cur_rva;
        cur_rva += SA;
    }

    e->image_size = cur_rva;

    /* Compute IAT RVAs */
    if (has_idata) {
        uint32_t cur_iat = 0;
        for (uint32_t i = 0; i < e->import_dll_count; i++) {
            for (uint32_t j = 0; j < e->imports[i].func_count; j++) {
                e->imports[i].iat_rva[j] = e->idata_rva + iat_offset + cur_iat;
                cur_iat += 8;
            }
            cur_iat += 8; /* null terminator */
        }
    }

    /* Resolve all fixups */
    for (uint32_t i = 0; i < e->fixup_count; i++) {
        EmFixup *f = &e->fixups[i];
        uint32_t rip_addr = e->text_rva + f->patch_offset + 4;
        int32_t disp;

        switch (f->kind) {
        case EM_FIXUP_RIP_REL32_IAT: {
            uint32_t dll_idx = (f->target_index >> 16) & 0xFFFF;
            uint32_t func_idx = f->target_index & 0xFFFF;
            uint32_t target = e->imports[dll_idx].iat_rva[func_idx];
            disp = (int32_t)(target - rip_addr) + f->addend;
            memcpy(e->code + f->patch_offset, &disp, 4);
            break;
        }
        case EM_FIXUP_RIP_REL32_DATA: {
            uint32_t target = e->rdata_rva + f->target_index;
            disp = (int32_t)(target - rip_addr) + f->addend;
            memcpy(e->code + f->patch_offset, &disp, 4);
            break;
        }
        case EM_FIXUP_RIP_REL32: {
            disp = (int32_t)(f->target_index - rip_addr) + f->addend;
            memcpy(e->code + f->patch_offset, &disp, 4);
            break;
        }
        case EM_FIXUP_LABEL_REL32: {
            if (f->target_index < e->label_count &&
                e->labels[f->target_index].offset != 0xFFFFFFFF) {
                uint32_t target = e->text_rva + e->labels[f->target_index].offset;
                disp = (int32_t)(target - rip_addr) + f->addend;
                memcpy(e->code + f->patch_offset, &disp, 4);
            } else {
                set_error(e, "Unresolved label reference");
                return -1;
            }
            break;
        }
        case EM_FIXUP_ABS64_DATA: {
            uint64_t abs_addr = e->image_base + e->rdata_rva + f->target_index;
            memcpy(e->code + f->patch_offset, &abs_addr, 8);
            break;
        }
        case EM_FIXUP_ABS64_CODE: {
            uint64_t abs_addr = e->image_base + e->text_rva + f->target_index;
            memcpy(e->code + f->patch_offset, &abs_addr, 8);
            break;
        }
        }
    }

    return 0;
}

int pe_write(Emitter *e, const char *filename) {
    if (pe_layout(e) < 0) return -1;

    FILE *fp = fopen(filename, "wb");
    if (!fp) { set_error(e, "Cannot open output file"); return -1; }

    const uint32_t SA = e->section_alignment;
    const uint32_t FA = e->file_alignment;
    int has_rdata  = (e->rdata_len > 0);
    int has_rwdata = (e->rwdata_len > 0);
    int has_bss    = (e->bss_size > 0);
    int has_idata  = (e->import_dll_count > 0);
    int has_reloc  = (e->reloc_count > 0);
    int has_edata  = (e->export_count > 0);

    uint32_t num_sections = 1;
    if (has_rdata) num_sections++;
    if (has_rwdata) num_sections++;
    if (has_bss) num_sections++;
    if (has_idata) num_sections++;
    if (has_reloc) num_sections++;
    if (has_edata) num_sections++;

    /* ── DOS Header ──────────────────────────────────────────────────────── */
    PE_DOS_HEADER dos = {0};
    dos.e_magic = 0x5A4D;
    dos.e_cblp = 0x0090;
    dos.e_cp = 0x0003;
    dos.e_cparhdr = 0x0004;
    dos.e_maxalloc = 0xFFFF;
    dos.e_sp = 0x00B8;
    dos.e_lfarlc = 0x0040;
    dos.e_lfanew = (uint32_t)(sizeof(PE_DOS_HEADER) + PE_DOS_STUB_SIZE);
    fwrite(&dos, sizeof(dos), 1, fp);
    fwrite(PE_DOS_STUB, PE_DOS_STUB_SIZE, 1, fp);

    /* ── PE Signature ────────────────────────────────────────────────────── */
    uint32_t pe_sig = 0x00004550;
    fwrite(&pe_sig, 4, 1, fp);

    /* ── COFF Header ─────────────────────────────────────────────────────── */
    uint32_t opt_hdr_size = (uint32_t)(sizeof(PE_OPTIONAL_HEADER_64)
                          + PE_NUM_DATA_DIRECTORIES * sizeof(PE_DATA_DIRECTORY));
    PE_COFF_HEADER coff = {0};
    coff.Machine = 0x8664;
    coff.NumberOfSections = (uint16_t)num_sections;
    coff.TimeDateStamp = 0x66100000;
    coff.SizeOfOptionalHeader = (uint16_t)opt_hdr_size;
    coff.Characteristics = e->characteristics;
    fwrite(&coff, sizeof(coff), 1, fp);

    /* ── Optional Header ─────────────────────────────────────────────────── */
    uint32_t entry_rva = e->text_rva;
    if (e->entry_point_label < e->label_count) {
        entry_rva = e->text_rva + e->labels[e->entry_point_label].offset;
    }

    uint32_t text_raw    = align_up(e->code_len > 0 ? e->code_len : 1, FA);
    uint32_t rdata_raw   = has_rdata ? align_up(e->rdata_len, FA) : 0;
    uint32_t rwdata_raw  = has_rwdata ? align_up(e->rwdata_len, FA) : 0;

    /* Recompute idata sizes for the write pass */
    uint32_t idt_size = 0, ilt_offset = 0, ilt_size = 0;
    uint32_t iat_offset = 0, iat_size = 0;
    uint32_t hn_offset = 0, hn_size = 0;
    uint32_t names_offset = 0, names_size = 0;
    uint32_t idata_virt_size = 0;
    uint32_t idata_raw  = 0;

    if (has_idata) {
        idt_size = (e->import_dll_count + 1) * (uint32_t)sizeof(PE_IMPORT_DESCRIPTOR);
        ilt_offset = idt_size;
        for (uint32_t i = 0; i < e->import_dll_count; i++)
            ilt_size += (e->imports[i].func_count + 1) * 8;
        iat_offset = ilt_offset + ilt_size;
        iat_size = ilt_size;
        hn_offset = iat_offset + iat_size;
        for (uint32_t i = 0; i < e->import_dll_count; i++) {
            for (uint32_t j = 0; j < e->imports[i].func_count; j++) {
                hn_size += 2 + (uint32_t)strlen(e->imports[i].func_names[j]) + 1;
                if (hn_size & 1) hn_size++;
            }
        }
        names_offset = hn_offset + hn_size;
        for (uint32_t i = 0; i < e->import_dll_count; i++)
            names_size += (uint32_t)strlen(e->imports[i].dll_name) + 1;
        idata_virt_size = names_offset + names_size;
        idata_raw = align_up(idata_virt_size > 0 ? idata_virt_size : 1, FA);
    }

    PE_OPTIONAL_HEADER_64 opt = {0};
    opt.Magic = 0x020B;
    opt.MajorLinkerVersion = 14;
    opt.MinorLinkerVersion = 50;
    opt.SizeOfCode = text_raw;
    opt.SizeOfInitializedData = rdata_raw + rwdata_raw + idata_raw;
    opt.AddressOfEntryPoint = entry_rva;
    opt.BaseOfCode = e->text_rva;
    opt.ImageBase = e->image_base;
    opt.SectionAlignment = SA;
    opt.FileAlignment = FA;
    opt.MajorOperatingSystemVersion = 6;
    opt.MinorOperatingSystemVersion = 0;
    opt.MajorSubsystemVersion = 6;
    opt.MinorSubsystemVersion = 0;
    opt.SizeOfImage = e->image_size;
    opt.SizeOfHeaders = e->headers_size;
    opt.Subsystem = e->subsystem;
    opt.DllCharacteristics = e->dll_characteristics;
    opt.SizeOfStackReserve = e->stack_reserve;
    opt.SizeOfStackCommit = e->stack_commit;
    opt.SizeOfHeapReserve = e->heap_reserve;
    opt.SizeOfHeapCommit = e->heap_commit;
    opt.NumberOfRvaAndSizes = PE_NUM_DATA_DIRECTORIES;
    fwrite(&opt, sizeof(opt), 1, fp);

    /* ── Data Directories ────────────────────────────────────────────────── */
    PE_DATA_DIRECTORY dirs[PE_NUM_DATA_DIRECTORIES] = {0};
    if (has_idata) {
        dirs[PE_DIR_IMPORT].VirtualAddress = e->idata_rva;
        dirs[PE_DIR_IMPORT].Size = idt_size;
        dirs[PE_DIR_IAT].VirtualAddress = e->idata_rva + iat_offset;
        dirs[PE_DIR_IAT].Size = iat_size;
    }
    if (has_reloc) {
        uint32_t reloc_block_size = 8 + e->reloc_count * 2;
        reloc_block_size = align_up(reloc_block_size, 4);
        dirs[PE_DIR_BASERELOC].VirtualAddress = e->reloc_rva;
        dirs[PE_DIR_BASERELOC].Size = reloc_block_size;
    }
    if (e->export_count > 0) {
        dirs[PE_DIR_EXPORT].VirtualAddress = e->edata_rva;
        /* Size computed below during edata layout */
        uint32_t edata_est = 40; /* export dir table */
        edata_est += e->export_count * 4; /* AddressOfFunctions */
        edata_est += e->export_count * 4; /* AddressOfNames */
        edata_est += e->export_count * 2; /* AddressOfNameOrdinals */
        edata_est += (uint32_t)strlen(e->export_module_name) + 1;
        for (uint32_t i = 0; i < e->export_count; i++)
            edata_est += (uint32_t)strlen(e->exports[i].name) + 1;
        dirs[PE_DIR_EXPORT].Size = edata_est;
    }
    fwrite(dirs, sizeof(dirs), 1, fp);

    /* ── Section Headers ─────────────────────────────────────────────────── */
    uint32_t file_off = e->headers_size;
    PE_SECTION_HEADER sec;

    /* .text */
    memset(&sec, 0, sizeof(sec));
    memcpy(sec.Name, ".text", 5);
    sec.VirtualSize = e->code_len;
    sec.VirtualAddress = e->text_rva;
    sec.SizeOfRawData = text_raw;
    sec.PointerToRawData = file_off;
    sec.Characteristics = PE_SCN_CNT_CODE | PE_SCN_MEM_EXECUTE | PE_SCN_MEM_READ;
    fwrite(&sec, sizeof(sec), 1, fp);
    file_off += text_raw;

    /* .rdata */
    if (has_rdata) {
        memset(&sec, 0, sizeof(sec));
        memcpy(sec.Name, ".rdata", 6);
        sec.VirtualSize = e->rdata_len;
        sec.VirtualAddress = e->rdata_rva;
        sec.SizeOfRawData = rdata_raw;
        sec.PointerToRawData = file_off;
        sec.Characteristics = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ;
        fwrite(&sec, sizeof(sec), 1, fp);
        file_off += rdata_raw;
    }

    /* .data */
    if (has_rwdata) {
        memset(&sec, 0, sizeof(sec));
        memcpy(sec.Name, ".data", 5);
        sec.VirtualSize = e->rwdata_len;
        sec.VirtualAddress = e->rwdata_rva;
        sec.SizeOfRawData = rwdata_raw;
        sec.PointerToRawData = file_off;
        sec.Characteristics = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ | PE_SCN_MEM_WRITE;
        fwrite(&sec, sizeof(sec), 1, fp);
        file_off += rwdata_raw;
    }

    /* .bss */
    if (has_bss) {
        memset(&sec, 0, sizeof(sec));
        memcpy(sec.Name, ".bss", 4);
        sec.VirtualSize = e->bss_size;
        sec.VirtualAddress = e->bss_rva;
        sec.SizeOfRawData = 0;
        sec.PointerToRawData = 0;
        sec.Characteristics = PE_SCN_CNT_UNINITIALIZED_DATA | PE_SCN_MEM_READ | PE_SCN_MEM_WRITE;
        fwrite(&sec, sizeof(sec), 1, fp);
    }

    /* .idata */
    if (has_idata) {
        memset(&sec, 0, sizeof(sec));
        memcpy(sec.Name, ".idata", 6);
        sec.VirtualSize = idata_virt_size;
        sec.VirtualAddress = e->idata_rva;
        sec.SizeOfRawData = idata_raw;
        sec.PointerToRawData = file_off;
        sec.Characteristics = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ | PE_SCN_MEM_WRITE;
        fwrite(&sec, sizeof(sec), 1, fp);
        file_off += idata_raw;
    }

    /* .edata (export directory) */
    uint32_t edata_raw = 0;
    if (has_edata) {
        /* Compute edata virtual size */
        uint32_t edata_virt = 40; /* IMAGE_EXPORT_DIRECTORY: 40 bytes */
        edata_virt += e->export_count * 4; /* AddressOfFunctions (EAT) */
        edata_virt += e->export_count * 4; /* AddressOfNames (ENT) */
        edata_virt += e->export_count * 2; /* AddressOfNameOrdinals */
        edata_virt += (uint32_t)strlen(e->export_module_name) + 1; /* module name */
        for (uint32_t i = 0; i < e->export_count; i++)
            edata_virt += (uint32_t)strlen(e->exports[i].name) + 1; /* export names */

        edata_raw = align_up(edata_virt, FA);
        memset(&sec, 0, sizeof(sec));
        memcpy(sec.Name, ".edata", 6);
        sec.VirtualSize = edata_virt;
        sec.VirtualAddress = e->edata_rva;
        sec.SizeOfRawData = edata_raw;
        sec.PointerToRawData = file_off;
        sec.Characteristics = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ;
        fwrite(&sec, sizeof(sec), 1, fp);
        file_off += edata_raw;
    }

    /* .reloc */
    if (has_reloc) {
        memset(&sec, 0, sizeof(sec));
        memcpy(sec.Name, ".reloc", 6);
        sec.VirtualSize = SA;
        sec.VirtualAddress = e->reloc_rva;
        sec.SizeOfRawData = FA;
        sec.PointerToRawData = file_off;
        sec.Characteristics = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ;
        fwrite(&sec, sizeof(sec), 1, fp);
        file_off += FA;
    }

    /* ── Pad headers ─────────────────────────────────────────────────────── */
    {
        long pos = ftell(fp);
        uint32_t pad = e->headers_size - (uint32_t)pos;
        for (uint32_t i = 0; i < pad; i++) { uint8_t z = 0; fwrite(&z, 1, 1, fp); }
    }

    /* ── .text ───────────────────────────────────────────────────────────── */
    fwrite(e->code, e->code_len, 1, fp);
    { uint32_t pad = text_raw - e->code_len; for (uint32_t i = 0; i < pad; i++) { uint8_t z = 0; fwrite(&z, 1, 1, fp); } }

    /* ── .rdata ──────────────────────────────────────────────────────────── */
    if (has_rdata) {
        fwrite(e->rdata, e->rdata_len, 1, fp);
        uint32_t pad = rdata_raw - e->rdata_len;
        for (uint32_t i = 0; i < pad; i++) { uint8_t z = 0; fwrite(&z, 1, 1, fp); }
    }

    /* ── .data ───────────────────────────────────────────────────────────── */
    if (has_rwdata) {
        fwrite(e->rwdata, e->rwdata_len, 1, fp);
        uint32_t pad = rwdata_raw - e->rwdata_len;
        for (uint32_t i = 0; i < pad; i++) { uint8_t z = 0; fwrite(&z, 1, 1, fp); }
    }

    /* ── .idata ──────────────────────────────────────────────────────────── */
    if (has_idata) {
        uint8_t *idata_buf = (uint8_t *)calloc(idata_raw, 1);
        if (!idata_buf) { fclose(fp); set_error(e, "Alloc failed"); return -1; }

        uint32_t cur_ilt = ilt_offset;
        uint32_t cur_iat = iat_offset;
        uint32_t cur_hn = hn_offset;
        uint32_t cur_name = names_offset;

        for (uint32_t d = 0; d < e->import_dll_count; d++) {
            PE_IMPORT_DESCRIPTOR *ide = (PE_IMPORT_DESCRIPTOR *)(idata_buf + d * sizeof(PE_IMPORT_DESCRIPTOR));
            ide->OriginalFirstThunk = e->idata_rva + cur_ilt;
            ide->FirstThunk         = e->idata_rva + cur_iat;
            ide->Name               = e->idata_rva + cur_name;

            uint32_t nlen = (uint32_t)strlen(e->imports[d].dll_name) + 1;
            memcpy(idata_buf + cur_name, e->imports[d].dll_name, nlen);
            cur_name += nlen;

            for (uint32_t fi = 0; fi < e->imports[d].func_count; fi++) {
                uint64_t hn_rva = e->idata_rva + cur_hn;
                memcpy(idata_buf + cur_ilt, &hn_rva, 8); cur_ilt += 8;
                memcpy(idata_buf + cur_iat, &hn_rva, 8); cur_iat += 8;

                uint16_t hint = 0;
                memcpy(idata_buf + cur_hn, &hint, 2); cur_hn += 2;
                uint32_t fnlen = (uint32_t)strlen(e->imports[d].func_names[fi]) + 1;
                memcpy(idata_buf + cur_hn, e->imports[d].func_names[fi], fnlen);
                cur_hn += fnlen;
                if (cur_hn & 1) cur_hn++;
            }
            cur_ilt += 8; /* null terminator */
            cur_iat += 8;
        }

        fwrite(idata_buf, idata_raw, 1, fp);
        free(idata_buf);
    }

    /* ── .edata (export directory data) ──────────────────────────────────── */
    if (has_edata) {
        uint8_t *edata_buf = (uint8_t *)calloc(edata_raw, 1);
        if (!edata_buf) { fclose(fp); set_error(e, "Alloc failed"); return -1; }

        /* IMAGE_EXPORT_DIRECTORY (40 bytes) */
        uint32_t eat_off = 40;                                    /* EAT starts after dir */
        uint32_t ent_off = eat_off + e->export_count * 4;         /* ENT after EAT */
        uint32_t ord_off = ent_off + e->export_count * 4;         /* Ordinals after ENT */
        uint32_t str_off = ord_off + e->export_count * 2;         /* Strings after ordinals */

        /* Module name first, then function names */
        uint32_t modname_off = str_off;
        uint32_t modname_len = (uint32_t)strlen(e->export_module_name) + 1;
        memcpy(edata_buf + modname_off, e->export_module_name, modname_len);
        uint32_t funcname_off = modname_off + modname_len;

        /* Export directory fields (flat layout, no struct needed) */
        uint32_t zero32 = 0;
        uint32_t time_stamp = 0x66100000;
        memcpy(edata_buf + 0,  &zero32, 4);                      /* Characteristics */
        memcpy(edata_buf + 4,  &time_stamp, 4);                  /* TimeDateStamp */
        memcpy(edata_buf + 8,  &zero32, 4);                      /* MajorVersion + MinorVersion */
        uint32_t name_rva = e->edata_rva + modname_off;
        memcpy(edata_buf + 12, &name_rva, 4);                    /* Name RVA */
        uint32_t ordinal_base = 1;
        memcpy(edata_buf + 16, &ordinal_base, 4);                /* OrdinalBase */
        memcpy(edata_buf + 20, &e->export_count, 4);             /* NumberOfFunctions */
        memcpy(edata_buf + 24, &e->export_count, 4);             /* NumberOfNames */
        uint32_t eat_rva = e->edata_rva + eat_off;
        memcpy(edata_buf + 28, &eat_rva, 4);                     /* AddressOfFunctions */
        uint32_t ent_rva = e->edata_rva + ent_off;
        memcpy(edata_buf + 32, &ent_rva, 4);                     /* AddressOfNames */
        uint32_t ord_rva = e->edata_rva + ord_off;
        memcpy(edata_buf + 36, &ord_rva, 4);                     /* AddressOfNameOrdinals */

        /* Fill EAT, ENT, ordinals, and name strings */
        uint32_t cur_str = funcname_off;
        for (uint32_t i = 0; i < e->export_count; i++) {
            /* EAT: RVA of function code */
            uint32_t func_rva = e->text_rva + e->exports[i].code_offset;
            memcpy(edata_buf + eat_off + i * 4, &func_rva, 4);

            /* ENT: RVA of function name string */
            uint32_t fname_rva = e->edata_rva + cur_str;
            memcpy(edata_buf + ent_off + i * 4, &fname_rva, 4);

            /* Ordinal table */
            uint16_t ord = (uint16_t)i;
            memcpy(edata_buf + ord_off + i * 2, &ord, 2);

            /* Name string */
            uint32_t flen = (uint32_t)strlen(e->exports[i].name) + 1;
            memcpy(edata_buf + cur_str, e->exports[i].name, flen);
            cur_str += flen;
        }

        fwrite(edata_buf, edata_raw, 1, fp);
        free(edata_buf);
    }

    /* ── .reloc ──────────────────────────────────────────────────────────── */
    if (has_reloc) {
        uint8_t *reloc_buf = (uint8_t *)calloc(FA, 1);
        if (reloc_buf) {
            uint32_t page_rva = e->text_rva;
            uint32_t block_size = 8 + e->reloc_count * 2;
            block_size = align_up(block_size, 4);

            PE_BASE_RELOCATION *blk = (PE_BASE_RELOCATION *)reloc_buf;
            blk->VirtualAddress = page_rva;
            blk->SizeOfBlock = block_size;

            uint16_t *entries = (uint16_t *)(reloc_buf + 8);
            for (uint32_t i = 0; i < e->reloc_count; i++) {
                uint32_t off = e->reloc_rvas[i];
                entries[i] = (uint16_t)((PE_REL_BASED_DIR64 << 12) | (off & 0xFFF));
            }

            fwrite(reloc_buf, FA, 1, fp);
            free(reloc_buf);
        }
    }

    fclose(fp);
    return 0;
}

uint32_t pe_write_mem(Emitter *e, uint8_t *buf, uint32_t buf_size) {
    const char *tmp = "_pe_tmp.exe";
    if (pe_write(e, tmp) < 0) return 0;

    FILE *fp = fopen(tmp, "rb");
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    uint32_t sz = (uint32_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz > buf_size) { fclose(fp); remove(tmp); return 0; }
    fread(buf, sz, 1, fp);
    fclose(fp);
    remove(tmp);
    return sz;
}

const char *em_error(const Emitter *e) {
    return e->error ? e->error_msg : NULL;
}
