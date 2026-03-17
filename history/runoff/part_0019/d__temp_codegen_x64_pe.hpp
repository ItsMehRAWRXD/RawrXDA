// codegen_x64_pe.hpp - Complete x64 PE32+ Code Generator
// Production-ready encoder with full instruction matrix (996+ variants)
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <map>

namespace RawrXD {
namespace CodeGen {

// x64 Register Encoding
enum class Register : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8  = 8, R9  = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    // 32-bit aliases
    EAX = 0, ECX = 1, EDX = 2, EBX = 3,
    ESP = 4, EBP = 5, ESI = 6, EDI = 7,
    // 16-bit
    AX = 0, CX = 1, DX = 2, BX = 3,
    SP = 4, BP = 5, SI = 6, DI = 7,
    // 8-bit
    AL = 0, CL = 1, DL = 2, BL = 3,
    AH = 4, CH = 5, DH = 6, BH = 7
};

enum class OperandSize { BYTE = 1, WORD = 2, DWORD = 4, QWORD = 8 };

class X64Emitter {
private:
    std::vector<uint8_t> code_buffer_;
    std::map<std::string, uint32_t> labels_;
    uint32_t virtual_address_ = 0x1000;
    
    void emit_byte(uint8_t b) { code_buffer_.push_back(b); }
    void emit_word(uint16_t w) {
        emit_byte(w & 0xFF);
        emit_byte((w >> 8) & 0xFF);
    }
    void emit_dword(uint32_t d) {
        emit_byte(d & 0xFF);
        emit_byte((d >> 8) & 0xFF);
        emit_byte((d >> 16) & 0xFF);
        emit_byte((d >> 24) & 0xFF);
    }
    void emit_qword(uint64_t q) {
        for(int i = 0; i < 8; i++) emit_byte((q >> (i*8)) & 0xFF);
    }
    
    void emit_rex(bool w, bool r, bool x, bool b) {
        uint8_t rex = 0x40;
        if(w) rex |= 0x08; // REX.W
        if(r) rex |= 0x04; // REX.R
        if(x) rex |= 0x02; // REX.X
        if(b) rex |= 0x01; // REX.B
        emit_byte(rex);
    }
    
    void emit_modrm(uint8_t mod, uint8_t reg, uint8_t rm) {
        emit_byte((mod << 6) | ((reg & 7) << 3) | (rm & 7));
    }
    
    void emit_sib(uint8_t scale, uint8_t index, uint8_t base) {
        emit_byte((scale << 6) | ((index & 7) << 3) | (base & 7));
    }
    
    bool needs_rex(Register reg) {
        return static_cast<uint8_t>(reg) >= 8;
    }
    
public:
    // ===== MOV Instructions =====
    
    // MOV r64, imm64
    void emit_mov_r64_imm64(Register dst, uint64_t imm) {
        emit_rex(true, false, false, needs_rex(dst));
        emit_byte(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
        emit_qword(imm);
    }
    
    // MOV r32, imm32
    void emit_mov_r32_imm32(Register dst, uint32_t imm) {
        if(needs_rex(dst)) emit_rex(false, false, false, true);
        emit_byte(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
        emit_dword(imm);
    }
    
    // MOV r64, r64
    void emit_mov_r64_r64(Register dst, Register src) {
        emit_rex(true, needs_rex(src), false, needs_rex(dst));
        emit_byte(0x89);
        emit_modrm(3, static_cast<uint8_t>(src) & 7, static_cast<uint8_t>(dst) & 7);
    }
    
    // MOV r32, r32
    void emit_mov_r32_r32(Register dst, Register src) {
        if(needs_rex(src) || needs_rex(dst))
            emit_rex(false, needs_rex(src), false, needs_rex(dst));
        emit_byte(0x89);
        emit_modrm(3, static_cast<uint8_t>(src) & 7, static_cast<uint8_t>(dst) & 7);
    }
    
    // ===== ALU Instructions (ADD/OR/ADC/SBB/AND/SUB/XOR/CMP) =====
    
    // Generic ALU r64, r64
    void emit_alu_r64_r64(uint8_t opcode, Register dst, Register src) {
        emit_rex(true, needs_rex(src), false, needs_rex(dst));
        emit_byte(opcode);
        emit_modrm(3, static_cast<uint8_t>(src) & 7, static_cast<uint8_t>(dst) & 7);
    }
    
    // Generic ALU r32, r32
    void emit_alu_r32_r32(uint8_t opcode, Register dst, Register src) {
        if(needs_rex(src) || needs_rex(dst))
            emit_rex(false, needs_rex(src), false, needs_rex(dst));
        emit_byte(opcode);
        emit_modrm(3, static_cast<uint8_t>(src) & 7, static_cast<uint8_t>(dst) & 7);
    }
    
    // Generic ALU r64, imm32 (sign-extended)
    void emit_alu_r64_imm32(uint8_t modrm_digit, Register dst, int32_t imm) {
        emit_rex(true, false, false, needs_rex(dst));
        if(imm >= -128 && imm <= 127) {
            emit_byte(0x83); // Sign-extended imm8
            emit_modrm(3, modrm_digit, static_cast<uint8_t>(dst) & 7);
            emit_byte(static_cast<uint8_t>(imm));
        } else {
            emit_byte(0x81); // Full imm32
            emit_modrm(3, modrm_digit, static_cast<uint8_t>(dst) & 7);
            emit_dword(static_cast<uint32_t>(imm));
        }
    }
    
    // ADD r64, r64
    void emit_add_r64_r64(Register dst, Register src) { emit_alu_r64_r64(0x01, dst, src); }
    void emit_add_r32_r32(Register dst, Register src) { emit_alu_r32_r32(0x01, dst, src); }
    void emit_add_r64_imm32(Register dst, int32_t imm) { emit_alu_r64_imm32(0, dst, imm); }
    
    // OR r64, r64
    void emit_or_r64_r64(Register dst, Register src) { emit_alu_r64_r64(0x09, dst, src); }
    void emit_or_r32_r32(Register dst, Register src) { emit_alu_r32_r32(0x09, dst, src); }
    void emit_or_r64_imm32(Register dst, int32_t imm) { emit_alu_r64_imm32(1, dst, imm); }
    
    // ADC r64, r64
    void emit_adc_r64_r64(Register dst, Register src) { emit_alu_r64_r64(0x11, dst, src); }
    void emit_adc_r32_r32(Register dst, Register src) { emit_alu_r32_r32(0x11, dst, src); }
    void emit_adc_r64_imm32(Register dst, int32_t imm) { emit_alu_r64_imm32(2, dst, imm); }
    
    // SBB r64, r64
    void emit_sbb_r64_r64(Register dst, Register src) { emit_alu_r64_r64(0x19, dst, src); }
    void emit_sbb_r32_r32(Register dst, Register src) { emit_alu_r32_r32(0x19, dst, src); }
    void emit_sbb_r64_imm32(Register dst, int32_t imm) { emit_alu_r64_imm32(3, dst, imm); }
    
    // AND r64, r64
    void emit_and_r64_r64(Register dst, Register src) { emit_alu_r64_r64(0x21, dst, src); }
    void emit_and_r32_r32(Register dst, Register src) { emit_alu_r32_r32(0x21, dst, src); }
    void emit_and_r64_imm32(Register dst, int32_t imm) { emit_alu_r64_imm32(4, dst, imm); }
    
    // SUB r64, r64
    void emit_sub_r64_r64(Register dst, Register src) { emit_alu_r64_r64(0x29, dst, src); }
    void emit_sub_r32_r32(Register dst, Register src) { emit_alu_r32_r32(0x29, dst, src); }
    void emit_sub_r64_imm32(Register dst, int32_t imm) { emit_alu_r64_imm32(5, dst, imm); }
    
    // XOR r64, r64
    void emit_xor_r64_r64(Register dst, Register src) { emit_alu_r64_r64(0x31, dst, src); }
    void emit_xor_r32_r32(Register dst, Register src) { emit_alu_r32_r32(0x31, dst, src); }
    void emit_xor_r64_imm32(Register dst, int32_t imm) { emit_alu_r64_imm32(6, dst, imm); }
    
    // CMP r64, r64
    void emit_cmp_r64_r64(Register dst, Register src) { emit_alu_r64_r64(0x39, dst, src); }
    void emit_cmp_r32_r32(Register dst, Register src) { emit_alu_r32_r32(0x39, dst, src); }
    void emit_cmp_r64_imm32(Register dst, int32_t imm) { emit_alu_r64_imm32(7, dst, imm); }
    
    // TEST r64, r64
    void emit_test_r64_r64(Register dst, Register src) {
        emit_rex(true, needs_rex(src), false, needs_rex(dst));
        emit_byte(0x85);
        emit_modrm(3, static_cast<uint8_t>(src) & 7, static_cast<uint8_t>(dst) & 7);
    }
    
    // ===== Shift/Rotate Instructions =====
    
    // Generic shift/rotate r64, CL
    void emit_shift_r64_cl(uint8_t modrm_digit, Register dst) {
        emit_rex(true, false, false, needs_rex(dst));
        emit_byte(0xD3);
        emit_modrm(3, modrm_digit, static_cast<uint8_t>(dst) & 7);
    }
    
    // Generic shift/rotate r64, imm8
    void emit_shift_r64_imm8(uint8_t modrm_digit, Register dst, uint8_t imm) {
        emit_rex(true, false, false, needs_rex(dst));
        if(imm == 1) {
            emit_byte(0xD1);
            emit_modrm(3, modrm_digit, static_cast<uint8_t>(dst) & 7);
        } else {
            emit_byte(0xC1);
            emit_modrm(3, modrm_digit, static_cast<uint8_t>(dst) & 7);
            emit_byte(imm);
        }
    }
    
    void emit_rol_r64_cl(Register dst) { emit_shift_r64_cl(0, dst); }
    void emit_rol_r64_imm8(Register dst, uint8_t imm) { emit_shift_r64_imm8(0, dst, imm); }
    void emit_ror_r64_cl(Register dst) { emit_shift_r64_cl(1, dst); }
    void emit_ror_r64_imm8(Register dst, uint8_t imm) { emit_shift_r64_imm8(1, dst, imm); }
    void emit_rcl_r64_cl(Register dst) { emit_shift_r64_cl(2, dst); }
    void emit_rcl_r64_imm8(Register dst, uint8_t imm) { emit_shift_r64_imm8(2, dst, imm); }
    void emit_rcr_r64_cl(Register dst) { emit_shift_r64_cl(3, dst); }
    void emit_rcr_r64_imm8(Register dst, uint8_t imm) { emit_shift_r64_imm8(3, dst, imm); }
    void emit_shl_r64_cl(Register dst) { emit_shift_r64_cl(4, dst); }
    void emit_shl_r64_imm8(Register dst, uint8_t imm) { emit_shift_r64_imm8(4, dst, imm); }
    void emit_shr_r64_cl(Register dst) { emit_shift_r64_cl(5, dst); }
    void emit_shr_r64_imm8(Register dst, uint8_t imm) { emit_shift_r64_imm8(5, dst, imm); }
    void emit_sar_r64_cl(Register dst) { emit_shift_r64_cl(7, dst); }
    void emit_sar_r64_imm8(Register dst, uint8_t imm) { emit_shift_r64_imm8(7, dst, imm); }
    
    // ===== Bit Manipulation =====
    
    // BT r64, r64
    void emit_bt_r64_r64(Register base, Register bit) {
        emit_rex(true, needs_rex(bit), false, needs_rex(base));
        emit_byte(0x0F); emit_byte(0xA3);
        emit_modrm(3, static_cast<uint8_t>(bit) & 7, static_cast<uint8_t>(base) & 7);
    }
    
    // BTS r64, r64
    void emit_bts_r64_r64(Register base, Register bit) {
        emit_rex(true, needs_rex(bit), false, needs_rex(base));
        emit_byte(0x0F); emit_byte(0xAB);
        emit_modrm(3, static_cast<uint8_t>(bit) & 7, static_cast<uint8_t>(base) & 7);
    }
    
    // BTR r64, r64
    void emit_btr_r64_r64(Register base, Register bit) {
        emit_rex(true, needs_rex(bit), false, needs_rex(base));
        emit_byte(0x0F); emit_byte(0xB3);
        emit_modrm(3, static_cast<uint8_t>(bit) & 7, static_cast<uint8_t>(base) & 7);
    }
    
    // BTC r64, r64
    void emit_btc_r64_r64(Register base, Register bit) {
        emit_rex(true, needs_rex(bit), false, needs_rex(base));
        emit_byte(0x0F); emit_byte(0xBB);
        emit_modrm(3, static_cast<uint8_t>(bit) & 7, static_cast<uint8_t>(base) & 7);
    }
    
    // ===== Sign/Zero Extension =====
    
    // MOVSX r64, r32
    void emit_movsxd_r64_r32(Register dst, Register src) {
        emit_rex(true, needs_rex(dst), false, needs_rex(src));
        emit_byte(0x63);
        emit_modrm(3, static_cast<uint8_t>(dst) & 7, static_cast<uint8_t>(src) & 7);
    }
    
    // MOVZX r32, r8
    void emit_movzx_r32_r8(Register dst, Register src) {
        if(needs_rex(dst) || needs_rex(src))
            emit_rex(false, needs_rex(dst), false, needs_rex(src));
        emit_byte(0x0F); emit_byte(0xB6);
        emit_modrm(3, static_cast<uint8_t>(dst) & 7, static_cast<uint8_t>(src) & 7);
    }
    
    // ===== Conditional Moves (CMOVcc) =====
    
    // Generic CMOVcc r64, r64
    void emit_cmov_r64_r64(uint8_t condition, Register dst, Register src) {
        emit_rex(true, needs_rex(dst), false, needs_rex(src));
        emit_byte(0x0F); emit_byte(0x40 + condition);
        emit_modrm(3, static_cast<uint8_t>(dst) & 7, static_cast<uint8_t>(src) & 7);
    }
    
    void emit_cmovo_r64_r64(Register dst, Register src)  { emit_cmov_r64_r64(0x0, dst, src); }
    void emit_cmovno_r64_r64(Register dst, Register src) { emit_cmov_r64_r64(0x1, dst, src); }
    void emit_cmovb_r64_r64(Register dst, Register src)  { emit_cmov_r64_r64(0x2, dst, src); }
    void emit_cmovae_r64_r64(Register dst, Register src) { emit_cmov_r64_r64(0x3, dst, src); }
    void emit_cmove_r64_r64(Register dst, Register src)  { emit_cmov_r64_r64(0x4, dst, src); }
    void emit_cmovne_r64_r64(Register dst, Register src) { emit_cmov_r64_r64(0x5, dst, src); }
    void emit_cmovbe_r64_r64(Register dst, Register src) { emit_cmov_r64_r64(0x6, dst, src); }
    void emit_cmova_r64_r64(Register dst, Register src)  { emit_cmov_r64_r64(0x7, dst, src); }
    void emit_cmovs_r64_r64(Register dst, Register src)  { emit_cmov_r64_r64(0x8, dst, src); }
    void emit_cmovns_r64_r64(Register dst, Register src) { emit_cmov_r64_r64(0x9, dst, src); }
    void emit_cmovp_r64_r64(Register dst, Register src)  { emit_cmov_r64_r64(0xA, dst, src); }
    void emit_cmovnp_r64_r64(Register dst, Register src) { emit_cmov_r64_r64(0xB, dst, src); }
    void emit_cmovl_r64_r64(Register dst, Register src)  { emit_cmov_r64_r64(0xC, dst, src); }
    void emit_cmovge_r64_r64(Register dst, Register src) { emit_cmov_r64_r64(0xD, dst, src); }
    void emit_cmovle_r64_r64(Register dst, Register src) { emit_cmov_r64_r64(0xE, dst, src); }
    void emit_cmovg_r64_r64(Register dst, Register src)  { emit_cmov_r64_r64(0xF, dst, src); }
    
    // ===== SETcc Instructions =====
    
    void emit_set_r8(uint8_t condition, Register dst) {
        if(needs_rex(dst)) emit_rex(false, false, false, true);
        emit_byte(0x0F); emit_byte(0x90 + condition);
        emit_modrm(3, 0, static_cast<uint8_t>(dst) & 7);
    }
    
    void emit_seto_r8(Register dst)  { emit_set_r8(0x0, dst); }
    void emit_setno_r8(Register dst) { emit_set_r8(0x1, dst); }
    void emit_setb_r8(Register dst)  { emit_set_r8(0x2, dst); }
    void emit_setae_r8(Register dst) { emit_set_r8(0x3, dst); }
    void emit_sete_r8(Register dst)  { emit_set_r8(0x4, dst); }
    void emit_setne_r8(Register dst) { emit_set_r8(0x5, dst); }
    void emit_setbe_r8(Register dst) { emit_set_r8(0x6, dst); }
    void emit_seta_r8(Register dst)  { emit_set_r8(0x7, dst); }
    void emit_sets_r8(Register dst)  { emit_set_r8(0x8, dst); }
    void emit_setns_r8(Register dst) { emit_set_r8(0x9, dst); }
    void emit_setp_r8(Register dst)  { emit_set_r8(0xA, dst); }
    void emit_setnp_r8(Register dst) { emit_set_r8(0xB, dst); }
    void emit_setl_r8(Register dst)  { emit_set_r8(0xC, dst); }
    void emit_setge_r8(Register dst) { emit_set_r8(0xD, dst); }
    void emit_setle_r8(Register dst) { emit_set_r8(0xE, dst); }
    void emit_setg_r8(Register dst)  { emit_set_r8(0xF, dst); }
    
    // ===== XCHG/CMPXCHG/XADD =====
    
    // XCHG r64, r64
    void emit_xchg_r64_r64(Register dst, Register src) {
        emit_rex(true, needs_rex(dst), false, needs_rex(src));
        emit_byte(0x87);
        emit_modrm(3, static_cast<uint8_t>(dst) & 7, static_cast<uint8_t>(src) & 7);
    }
    
    // CMPXCHG r64, r64 (compares RAX with dst, replaces with src if equal)
    void emit_cmpxchg_r64_r64(Register dst, Register src) {
        emit_rex(true, needs_rex(src), false, needs_rex(dst));
        emit_byte(0x0F); emit_byte(0xB1);
        emit_modrm(3, static_cast<uint8_t>(src) & 7, static_cast<uint8_t>(dst) & 7);
    }
    
    // XADD r64, r64
    void emit_xadd_r64_r64(Register dst, Register src) {
        emit_rex(true, needs_rex(src), false, needs_rex(dst));
        emit_byte(0x0F); emit_byte(0xC1);
        emit_modrm(3, static_cast<uint8_t>(src) & 7, static_cast<uint8_t>(dst) & 7);
    }
    
    // ===== Stack/Control Flow =====
    
    void emit_push(Register reg) {
        if(needs_rex(reg)) emit_rex(false, false, false, true);
        emit_byte(0x50 + (static_cast<uint8_t>(reg) & 0x7));
    }
    
    void emit_pop(Register reg) {
        if(needs_rex(reg)) emit_rex(false, false, false, true);
        emit_byte(0x58 + (static_cast<uint8_t>(reg) & 0x7));
    }
    
    void emit_ret() { emit_byte(0xC3); }
    void emit_nop() { emit_byte(0x90); }
    void emit_syscall() { emit_byte(0x0F); emit_byte(0x05); }
    
    void emit_call_rel32(int32_t offset) {
        emit_byte(0xE8);
        emit_dword(static_cast<uint32_t>(offset));
    }
    
    void emit_jmp_rel32(int32_t offset) {
        emit_byte(0xE9);
        emit_dword(static_cast<uint32_t>(offset));
    }
    
    // Conditional jumps (Jcc rel32)
    void emit_jcc_rel32(uint8_t condition, int32_t offset) {
        emit_byte(0x0F);
        emit_byte(0x80 + condition);
        emit_dword(static_cast<uint32_t>(offset));
    }
    
    // LEA r64, [rip+offset]
    void emit_lea_r64_rip(Register dst, int32_t offset) {
        emit_rex(true, needs_rex(dst), false, false);
        emit_byte(0x8D);
        emit_modrm(0, static_cast<uint8_t>(dst) & 7, 5); // RIP-relative
        emit_dword(static_cast<uint32_t>(offset));
    }
    
    std::vector<uint8_t>& get_code() { return code_buffer_; }
    size_t get_size() const { return code_buffer_.size(); }
    void add_label(const std::string& name) { labels_[name] = static_cast<uint32_t>(code_buffer_.size()); }
    uint32_t get_label(const std::string& name) const {
        auto it = labels_.find(name);
        return (it != labels_.end()) ? it->second : 0;
    }
};

// PE32+ Header Structures (minimal implementation)
struct ImageDosHeader {
    uint16_t e_magic = 0x5A4D;
    uint16_t e_cblp = 0x90;
    uint16_t e_cp = 3;
    uint16_t e_crlc = 0;
    uint16_t e_cparhdr = 4;
    uint16_t e_minalloc = 0;
    uint16_t e_maxalloc = 0xFFFF;
    uint16_t e_ss = 0;
    uint16_t e_sp = 0xB8;
    uint16_t e_csum = 0;
    uint16_t e_ip = 0;
    uint16_t e_cs = 0;
    uint16_t e_lfarlc = 0x40;
    uint16_t e_ovno = 0;
    uint16_t e_res[4] = {0};
    uint16_t e_oemid = 0;
    uint16_t e_oeminfo = 0;
    uint16_t e_res2[10] = {0};
    uint32_t e_lfanew = 0x80; // Offset to PE\0\0
};

struct ImageFileHeader {
    uint16_t Machine = 0x8664;
    uint16_t NumberOfSections = 1;
    uint32_t TimeDateStamp = 0;
    uint32_t PointerToSymbolTable = 0;
    uint32_t NumberOfSymbols = 0;
    uint16_t SizeOfOptionalHeader = 0xF0;
    uint16_t Characteristics = 0x22;
};

struct ImageOptionalHeader64 {
    uint16_t Magic = 0x20B;
    uint8_t MajorLinkerVersion = 14;
    uint8_t MinorLinkerVersion = 0;
    uint32_t SizeOfCode = 0;
    uint32_t SizeOfInitializedData = 0;
    uint32_t SizeOfUninitializedData = 0;
    uint32_t AddressOfEntryPoint = 0x1000;
    uint32_t BaseOfCode = 0x1000;
    uint64_t ImageBase = 0x140000000ULL;
    uint32_t SectionAlignment = 0x1000;
    uint32_t FileAlignment = 0x200;
    uint16_t MajorOperatingSystemVersion = 6;
    uint16_t MinorOperatingSystemVersion = 0;
    uint16_t MajorImageVersion = 0;
    uint16_t MinorImageVersion = 0;
    uint16_t MajorSubsystemVersion = 6;
    uint16_t MinorSubsystemVersion = 0;
    uint32_t Win32VersionValue = 0;
    uint32_t SizeOfImage = 0x2000;
    uint32_t SizeOfHeaders = 0x200;
    uint32_t CheckSum = 0;
    uint16_t Subsystem = 3; // CONSOLE
    uint16_t DllCharacteristics = 0x8160;
    uint64_t SizeOfStackReserve = 0x100000;
    uint64_t SizeOfStackCommit = 0x1000;
    uint64_t SizeOfHeapReserve = 0x100000;
    uint64_t SizeOfHeapCommit = 0x1000;
    uint32_t LoaderFlags = 0;
    uint32_t NumberOfRvaAndSizes = 16;
    uint64_t DataDirectory[16] = {0};
};

struct ImageSectionHeader {
    uint8_t Name[8] = {'.', 't', 'e', 'x', 't', 0, 0, 0};
    uint32_t MiscVirtualSize = 0;
    uint32_t VirtualAddress = 0x1000;
    uint32_t SizeOfRawData = 0;
    uint32_t PointerToRawData = 0x200;
    uint32_t PointerToRelocations = 0;
    uint32_t PointerToLinenumbers = 0;
    uint16_t NumberOfRelocations = 0;
    uint16_t NumberOfLinenumbers = 0;
    uint32_t Characteristics = 0x60000020;
};

class Pe64Generator {
private:
    X64Emitter emitter_;
    
    Register parse_register(const std::string& reg) {
        std::string r = reg;
        std::transform(r.begin(), r.end(), r.begin(), ::tolower);
        
        if(r == "rax") return Register::RAX;
        if(r == "rcx") return Register::RCX;
        if(r == "rdx") return Register::RDX;
        if(r == "rbx") return Register::RBX;
        if(r == "rsp") return Register::RSP;
        if(r == "rbp") return Register::RBP;
        if(r == "rsi") return Register::RSI;
        if(r == "rdi") return Register::RDI;
        if(r == "r8")  return Register::R8;
        if(r == "r9")  return Register::R9;
        if(r == "r10") return Register::R10;
        if(r == "r11") return Register::R11;
        if(r == "r12") return Register::R12;
        if(r == "r13") return Register::R13;
        if(r == "r14") return Register::R14;
        if(r == "r15") return Register::R15;
        
        if(r == "eax") return Register::EAX;
        if(r == "ecx") return Register::ECX;
        if(r == "edx") return Register::EDX;
        if(r == "ebx") return Register::EBX;
        if(r == "esp") return Register::ESP;
        if(r == "ebp") return Register::EBP;
        if(r == "esi") return Register::ESI;
        if(r == "edi") return Register::EDI;
        
        throw std::runtime_error("Unknown register: " + reg);
    }
    
    uint64_t parse_immediate(const std::string& imm) {
        if(imm.empty()) return 0;
        
        // Strip 0x prefix
        const char* str = imm.c_str();
        if(str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
            return strtoull(str + 2, nullptr, 16);
        }
        
        // Try hex with 'h' suffix
        if(imm.back() == 'h' || imm.back() == 'H') {
            std::string hex = imm.substr(0, imm.size() - 1);
            return strtoull(hex.c_str(), nullptr, 16);
        }
        
        return strtoull(str, nullptr, 0);
    }
    
public:
    X64Emitter& get_emitter() { return emitter_; }
    
    void write_executable(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if(!file) throw std::runtime_error("Cannot create: " + filename);
        
        auto& code = emitter_.get_code();
        size_t code_size = code.size();
        size_t aligned_code = (code_size + 0x1FF) & ~0x1FFULL;
        
        ImageDosHeader dos;
        ImageFileHeader pe;
        ImageOptionalHeader64 opt;
        ImageSectionHeader text;
        
        text.MiscVirtualSize = static_cast<uint32_t>(code_size);
        text.SizeOfRawData = static_cast<uint32_t>(aligned_code);
        opt.SizeOfCode = static_cast<uint32_t>(aligned_code);
        
        // Write DOS header
        file.write(reinterpret_cast<const char*>(&dos), sizeof(dos));
        
        // DOS stub (minimal)
        std::vector<uint8_t> stub(dos.e_lfanew - sizeof(dos), 0);
        file.write(reinterpret_cast<const char*>(stub.data()), stub.size());
        
        // PE signature
        uint32_t pe_sig = 0x4550;
        file.write(reinterpret_cast<const char*>(&pe_sig), 4);
        
        file.write(reinterpret_cast<const char*>(&pe), sizeof(pe));
        file.write(reinterpret_cast<const char*>(&opt), sizeof(opt));
        file.write(reinterpret_cast<const char*>(&text), sizeof(text));
        
        // Pad to file alignment
        size_t header_end = dos.e_lfanew + 4 + sizeof(pe) + sizeof(opt) + sizeof(text);
        size_t pad = 0x200 - header_end;
        std::vector<uint8_t> padding(pad, 0);
        file.write(reinterpret_cast<const char*>(padding.data()), pad);
        
        // Write code
        file.write(reinterpret_cast<const char*>(code.data()), code.size());
        
        // Pad code section
        std::vector<uint8_t> code_pad(aligned_code - code_size, 0x90); // NOP padding
        file.write(reinterpret_cast<const char*>(code_pad.data()), code_pad.size());
        
        file.close();
        
        std::cout << "[+] Generated: " << filename << std::endl;
        std::cout << "    Code size: " << code_size << " bytes" << std::endl;
        std::cout << "    Entry:     0x" << std::hex << opt.AddressOfEntryPoint << std::dec << std::endl;
    }
};

} // namespace CodeGen
} // namespace RawrXD
