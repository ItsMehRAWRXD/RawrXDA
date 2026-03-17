#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>

namespace RawrXD {
namespace MASM {

enum class Register : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8 = 8,  R9 = 9,  R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    NONE = 0xFF
};

struct Operand {
    enum Type { None, Register, Immediate, Memory } type = None;
    RawrXD::MASM::Register reg = RawrXD::MASM::Register::NONE;
    int64_t imm = 0;
    int32_t displacement = 0;
    bool is_rip_relative = false;
};

struct Instruction {
    std::string mnemonic;
    Operand op1, op2, op3;
    size_t line = 0;
};

class InstructionEncoder {
public:
    std::vector<uint8_t> encode(const Instruction& inst) {
        buffer_.clear();

        if (inst.mnemonic == "nop") return encodeNOP(inst);
        if (inst.mnemonic == "int3") return encodeINT3(inst);
        if (inst.mnemonic == "mov") return encodeMOV(inst);
        if (inst.mnemonic == "push") return encodePUSH(inst);
        if (inst.mnemonic == "pop") return encodePOP(inst);
        if (inst.mnemonic == "ret") return encodeRET(inst);
        if (inst.mnemonic == "int") return encodeINT(inst);
        if (inst.mnemonic == "syscall") return encodeSYSCALL(inst);
        if (inst.mnemonic == "add") return encodeADD(inst);
        if (inst.mnemonic == "sub") return encodeSUB(inst);
        if (inst.mnemonic == "xor") return encodeXOR(inst);
        if (inst.mnemonic == "jmp") return encodeJMP(inst);
        if (inst.mnemonic == "call") return encodeCALL(inst);

        throw std::runtime_error("Unimplemented mnemonic: " + inst.mnemonic);
    }

private:
    std::vector<uint8_t> buffer_;

    static uint8_t getRegBits(RawrXD::MASM::Register r) { return static_cast<uint8_t>(r) & 0x7; }
    static bool needsRex(RawrXD::MASM::Register r) { return static_cast<uint8_t>(r) >= 8; }
    static bool isExtended(RawrXD::MASM::Register r) { return static_cast<uint8_t>(r) >= 8; }

    void emitREX(bool w, bool r, bool x, bool b) {
        uint8_t rex = 0x40;
        if (w) rex |= 0x08;
        if (r) rex |= 0x04;
        if (x) rex |= 0x02;
        if (b) rex |= 0x01;
        buffer_.push_back(rex);
    }

    void emitModRM(uint8_t mod, uint8_t regop, uint8_t rm) {
        buffer_.push_back((mod << 6) | ((regop & 7) << 3) | (rm & 7));
    }

    void emitImmediate(int64_t value, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            buffer_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }

    std::vector<uint8_t> encodeNOP(const Instruction&) {
        buffer_.push_back(0x90);
        return buffer_;
    }

    std::vector<uint8_t> encodeINT3(const Instruction&) {
        buffer_.push_back(0xCC);
        return buffer_;
    }

    std::vector<uint8_t> encodeMOV(const Instruction& inst) {
        if (inst.op1.type == Operand::Register && inst.op2.type == Operand::Immediate) {
            if (inst.op2.imm > INT32_MAX || inst.op2.imm < INT32_MIN) {
                // MOV r64, imm64
                emitREX(true, false, false, isExtended(inst.op1.reg));
                buffer_.push_back(0xB8 + getRegBits(inst.op1.reg));
                emitImmediate(inst.op2.imm, 8);
            } else {
                // MOV r/m64, imm32 (sign-extended)
                emitREX(true, false, false, isExtended(inst.op1.reg));
                buffer_.push_back(0xC7);
                emitModRM(3, 0, getRegBits(inst.op1.reg));
                emitImmediate(inst.op2.imm, 4);
            }
            return buffer_;
        }

        if (inst.op1.type == Operand::Register && inst.op2.type == Operand::Register) {
            bool r_ext = isExtended(inst.op2.reg);
            bool b_ext = isExtended(inst.op1.reg);
            emitREX(true, r_ext, false, b_ext);
            buffer_.push_back(0x89);  // MOV r/m64, r64
            emitModRM(3, getRegBits(inst.op2.reg), getRegBits(inst.op1.reg));
            return buffer_;
        }

        throw std::runtime_error("MOV: Unsupported operand combination");
    }

    std::vector<uint8_t> encodePUSH(const Instruction& inst) {
        if (inst.op1.type == Operand::Register) {
            if (needsRex(inst.op1.reg)) {
                emitREX(false, false, false, isExtended(inst.op1.reg));
            }
            buffer_.push_back(0x50 + getRegBits(inst.op1.reg));
            return buffer_;
        }
        if (inst.op1.type == Operand::Immediate) {
            buffer_.push_back(0x68);  // PUSH imm32
            emitImmediate(inst.op1.imm, 4);
            return buffer_;
        }
        throw std::runtime_error("PUSH: Unsupported operand");
    }

    std::vector<uint8_t> encodePOP(const Instruction& inst) {
        if (inst.op1.type == Operand::Register) {
            if (needsRex(inst.op1.reg)) {
                emitREX(false, false, false, isExtended(inst.op1.reg));
            }
            buffer_.push_back(0x58 + getRegBits(inst.op1.reg));
            return buffer_;
        }
        throw std::runtime_error("POP: Unsupported operand");
    }

    std::vector<uint8_t> encodeRET(const Instruction&) {
        buffer_.push_back(0xC3);
        return buffer_;
    }

    std::vector<uint8_t> encodeINT(const Instruction& inst) {
        if (inst.op1.type != Operand::Immediate) {
            throw std::runtime_error("INT requires immediate operand");
        }
        if (inst.op1.imm == 3) {
            buffer_.push_back(0xCC);
        } else {
            buffer_.push_back(0xCD);
            buffer_.push_back(static_cast<uint8_t>(inst.op1.imm));
        }
        return buffer_;
    }

    std::vector<uint8_t> encodeSYSCALL(const Instruction&) {
        buffer_.push_back(0x0F);
        buffer_.push_back(0x05);
        return buffer_;
    }

    std::vector<uint8_t> encodeADD(const Instruction& inst) {
        if (inst.op1.type == Operand::Register && inst.op2.type == Operand::Immediate) {
            emitREX(true, false, false, isExtended(inst.op1.reg));
            if (inst.op2.imm >= -128 && inst.op2.imm <= 127) {
                buffer_.push_back(0x83);
                emitModRM(3, 0, getRegBits(inst.op1.reg));
                buffer_.push_back(static_cast<uint8_t>(inst.op2.imm));
            } else {
                buffer_.push_back(0x81);
                emitModRM(3, 0, getRegBits(inst.op1.reg));
                emitImmediate(inst.op2.imm, 4);
            }
            return buffer_;
        }
        if (inst.op1.type == Operand::Register && inst.op2.type == Operand::Register) {
            emitREX(true, isExtended(inst.op2.reg), false, isExtended(inst.op1.reg));
            buffer_.push_back(0x01);  // ADD r/m64, r64
            emitModRM(3, getRegBits(inst.op2.reg), getRegBits(inst.op1.reg));
            return buffer_;
        }
        throw std::runtime_error("ADD: Unsupported operands");
    }

    std::vector<uint8_t> encodeSUB(const Instruction& inst) {
        if (inst.op1.type == Operand::Register && inst.op2.type == Operand::Immediate) {
            emitREX(true, false, false, isExtended(inst.op1.reg));
            if (inst.op2.imm >= -128 && inst.op2.imm <= 127) {
                buffer_.push_back(0x83);
                emitModRM(3, 5, getRegBits(inst.op1.reg));
                buffer_.push_back(static_cast<uint8_t>(inst.op2.imm));
            } else {
                buffer_.push_back(0x81);
                emitModRM(3, 5, getRegBits(inst.op1.reg));
                emitImmediate(inst.op2.imm, 4);
            }
            return buffer_;
        }
        if (inst.op1.type == Operand::Register && inst.op2.type == Operand::Register) {
            emitREX(true, isExtended(inst.op2.reg), false, isExtended(inst.op1.reg));
            buffer_.push_back(0x29);  // SUB r/m64, r64
            emitModRM(3, getRegBits(inst.op2.reg), getRegBits(inst.op1.reg));
            return buffer_;
        }
        throw std::runtime_error("SUB: Unsupported operands");
    }

    std::vector<uint8_t> encodeXOR(const Instruction& inst) {
        if (inst.op1.type == Operand::Register && inst.op2.type == Operand::Register) {
            emitREX(true, isExtended(inst.op2.reg), false, isExtended(inst.op1.reg));
            buffer_.push_back(0x31);  // XOR r/m64, r64
            emitModRM(3, getRegBits(inst.op2.reg), getRegBits(inst.op1.reg));
            return buffer_;
        }
        throw std::runtime_error("XOR: Unsupported operands");
    }

    std::vector<uint8_t> encodeJMP(const Instruction& inst) {
        if (inst.op1.type == Operand::Immediate) {
            // Short jump (rel8) or near jump (rel32)
            if (inst.op1.imm >= -128 && inst.op1.imm <= 127) {
                buffer_.push_back(0xEB);
                buffer_.push_back(static_cast<uint8_t>(inst.op1.imm));
            } else {
                buffer_.push_back(0xE9);
                emitImmediate(inst.op1.imm, 4);
            }
            return buffer_;
        }
        throw std::runtime_error("JMP: Unsupported operand");
    }

    std::vector<uint8_t> encodeCALL(const Instruction& inst) {
        if (inst.op1.type == Operand::Immediate) {
            buffer_.push_back(0xE8);
            emitImmediate(inst.op1.imm, 4);
            return buffer_;
        }
        throw std::runtime_error("CALL: Unsupported operand");
    }
};

} // namespace MASM
} // namespace RawrXD
