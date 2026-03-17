// =============================================================================
// encoder_codegen_integration.cpp
// Integration of x64 instruction encoder with compiler code generation
// Bridges AST instructions to encoded bytes via InstructionEncoder
// =============================================================================

#include "encoder_integration.hpp"
#include <vector>
#include <memory>
#include <stdexcept>

namespace rawrxd {
namespace compiler {

using namespace encoder;

// =============================================================================
// CODEGEN INSTRUCTION DISPATCH
// =============================================================================

/// Represents an instruction in the AST (Intermediate Representation)
struct ASTInstruction {
    enum class Type : uint32_t {
        MOV_REG_IMM64 = 1,  // MOV r64, imm64
        MOV_REG_REG   = 2,  // MOV r64, r64
        PUSH_REG      = 3,  // PUSH r64
        POP_REG       = 4,  // POP r64
        ADD_REG_REG   = 5,  // ADD r64, r64
        SUB_REG_IMM   = 6,  // SUB r64, imm32
        CALL_REL32    = 7,  // CALL rel32
        JMP_REL32     = 8,  // JMP rel32
        LEA_REG_DISP  = 9,  // LEA r64, [base+disp]
        NOP           = 0,  // No operation (placeholder)
    };

    Type type = Type::NOP;
    Register dest_reg = Register::RAX;
    Register src_reg = Register::RAX;
    uint64_t immediate = 0;
    int32_t displacement = 0;
    uint32_t operand_flags = 0;
    uint32_t line_number = 0;  // For error reporting
    
    // Source location for debugging
    std::string source_line;
    
    ASTInstruction() = default;
    explicit ASTInstruction(Type t) : type(t) {}
};

// =============================================================================
// CODE GENERATOR WITH ENCODER INTEGRATION
// =============================================================================

class CodeGenerator {
public:
    /// Initialize code generator with output buffer
    CodeGenerator(uint8_t* code_buffer, uint32_t buffer_size)
        : m_code_buffer(code_buffer),
          m_buffer_size(buffer_size),
          m_code_offset(0) {
        
        InstructionEncoder::init_context(
            m_encode_ctx,
            code_buffer,
            buffer_size
        );
    }

    /// Emit a single instruction from the AST
    /// \return Instruction length in bytes, or 0 on error
    uint8_t emit_instruction(const ASTInstruction& inst) {
        InstructionEncoder enc;
        uint8_t len = 0;

        try {
            switch (inst.type) {
                case ASTInstruction::Type::MOV_REG_IMM64:
                    len = emit_mov_imm(enc, inst);
                    break;

                case ASTInstruction::Type::MOV_REG_REG:
                    len = emit_mov_reg(enc, inst);
                    break;

                case ASTInstruction::Type::PUSH_REG:
                    len = emit_push(enc, inst);
                    break;

                case ASTInstruction::Type::POP_REG:
                    len = emit_pop(enc, inst);
                    break;

                case ASTInstruction::Type::ADD_REG_REG:
                    len = emit_add(enc, inst);
                    break;

                case ASTInstruction::Type::SUB_REG_IMM:
                    len = emit_sub(enc, inst);
                    break;

                case ASTInstruction::Type::CALL_REL32:
                    len = emit_call(enc, inst);
                    break;

                case ASTInstruction::Type::JMP_REL32:
                    len = emit_jmp(enc, inst);
                    break;

                case ASTInstruction::Type::LEA_REG_DISP:
                    len = emit_lea(enc, inst);
                    break;

                case ASTInstruction::Type::NOP:
                    len = 0;  // No operation
                    break;

                default:
                    throw std::runtime_error("Unknown instruction type");
            }

            // Emit encoded bytes to buffer
            if (len > 0) {
                uint32_t bytes_written = enc.emit_to_buffer(m_encode_ctx);
                if (bytes_written == 0) {
                    throw std::runtime_error("Code buffer overflow");
                }
                m_code_offset += bytes_written;
                m_instructions.push_back(inst);
                m_encoded_lengths.push_back(len);
            }

            return len;

        } catch (const std::exception& e) {
            // Enhanced error reporting
            throw std::runtime_error(
                "Code generation error at line " + 
                std::to_string(inst.line_number) + 
                ": " + e.what()
            );
        }
    }

    /// Get current code offset
    uint32_t get_code_offset() const { return m_code_offset; }

    /// Get generated code
    const uint8_t* get_code() const { return m_code_buffer; }

    /// Get instruction count
    size_t get_instruction_count() const { return m_instructions.size(); }

    /// Get total code size
    uint32_t get_code_size() const { return m_code_offset; }

    /// Verify buffer capacity
    bool has_space(uint32_t bytes_needed) const {
        return (m_code_offset + bytes_needed) <= m_buffer_size;
    }

private:
    uint8_t* m_code_buffer;
    uint32_t m_buffer_size;
    uint32_t m_code_offset;
    EncodeCtx m_encode_ctx;
    std::vector<ASTInstruction> m_instructions;
    std::vector<uint8_t> m_encoded_lengths;

    // =======================================================================
    // INSTRUCTION EMISSION METHODS
    // =======================================================================

    uint8_t emit_mov_imm(InstructionEncoder& enc, const ASTInstruction& inst) {
        if (inst.immediate == 0 && !fits_imm32(inst.immediate)) {
            // For smaller immediates, could use MOV r64, sign-extended-imm32
            // For now, use full 64-bit form
        }
        return enc.encode_mov_imm(inst.dest_reg, inst.immediate);
    }

    uint8_t emit_mov_reg(InstructionEncoder& enc, const ASTInstruction& inst) {
        return enc.encode_mov_reg(inst.dest_reg, inst.src_reg);
    }

    uint8_t emit_push(InstructionEncoder& enc, const ASTInstruction& inst) {
        return enc.encode_push(inst.dest_reg);
    }

    uint8_t emit_pop(InstructionEncoder& enc, const ASTInstruction& inst) {
        return enc.encode_pop(inst.dest_reg);
    }

    uint8_t emit_add(InstructionEncoder& enc, const ASTInstruction& inst) {
        return enc.encode_add(inst.dest_reg, inst.src_reg);
    }

    uint8_t emit_sub(InstructionEncoder& enc, const ASTInstruction& inst) {
        // Use 8-bit immediate if value fits
        bool use_8bit = fits_imm8(inst.immediate);
        return enc.encode_sub(
            inst.dest_reg,
            static_cast<uint32_t>(inst.immediate),
            use_8bit
        );
    }

    uint8_t emit_call(InstructionEncoder& enc, const ASTInstruction& inst) {
        // immediate is the relative offset
        return enc.encode_call(static_cast<int32_t>(inst.immediate));
    }

    uint8_t emit_jmp(InstructionEncoder& enc, const ASTInstruction& inst) {
        return enc.encode_jmp(static_cast<int32_t>(inst.immediate));
    }

    uint8_t emit_lea(InstructionEncoder& enc, const ASTInstruction& inst) {
        return enc.encode_lea(
            inst.dest_reg,
            inst.src_reg,
            inst.displacement
        );
    }
};

// =============================================================================
// ASSEMBLER FACADE - High-level assembly API
// =============================================================================

class Assembler {
public:
    explicit Assembler(uint32_t code_buffer_size = 65536)
        : m_code_buffer(std::make_unique<uint8_t[]>(code_buffer_size)),
          m_codegen(m_code_buffer.get(), code_buffer_size) {
    }

    /// MOV r64, imm64
    Assembler& mov(Register dest, uint64_t imm) {
        ASTInstruction inst(ASTInstruction::Type::MOV_REG_IMM64);
        inst.dest_reg = dest;
        inst.immediate = imm;
        m_codegen.emit_instruction(inst);
        return *this;
    }

    /// MOV r64, r64
    Assembler& mov(Register dest, Register src) {
        ASTInstruction inst(ASTInstruction::Type::MOV_REG_REG);
        inst.dest_reg = dest;
        inst.src_reg = src;
        m_codegen.emit_instruction(inst);
        return *this;
    }

    /// PUSH r64
    Assembler& push(Register reg) {
        ASTInstruction inst(ASTInstruction::Type::PUSH_REG);
        inst.dest_reg = reg;
        m_codegen.emit_instruction(inst);
        return *this;
    }

    /// POP r64
    Assembler& pop(Register reg) {
        ASTInstruction inst(ASTInstruction::Type::POP_REG);
        inst.dest_reg = reg;
        m_codegen.emit_instruction(inst);
        return *this;
    }

    /// ADD r64, r64
    Assembler& add(Register dest, Register src) {
        ASTInstruction inst(ASTInstruction::Type::ADD_REG_REG);
        inst.dest_reg = dest;
        inst.src_reg = src;
        m_codegen.emit_instruction(inst);
        return *this;
    }

    /// SUB r64, imm
    Assembler& sub(Register dest, uint32_t imm) {
        ASTInstruction inst(ASTInstruction::Type::SUB_REG_IMM);
        inst.dest_reg = dest;
        inst.immediate = imm;
        m_codegen.emit_instruction(inst);
        return *this;
    }

    /// CALL rel32
    Assembler& call(int32_t rel32) {
        ASTInstruction inst(ASTInstruction::Type::CALL_REL32);
        inst.immediate = rel32;
        m_codegen.emit_instruction(inst);
        return *this;
    }

    /// JMP rel32
    Assembler& jmp(int32_t rel32) {
        ASTInstruction inst(ASTInstruction::Type::JMP_REL32);
        inst.immediate = rel32;
        m_codegen.emit_instruction(inst);
        return *this;
    }

    /// LEA r64, [base+disp]
    Assembler& lea(Register dest, Register base, int32_t disp = 0) {
        ASTInstruction inst(ASTInstruction::Type::LEA_REG_DISP);
        inst.dest_reg = dest;
        inst.src_reg = base;
        inst.displacement = disp;
        m_codegen.emit_instruction(inst);
        return *this;
    }

    /// Get generated code
    const uint8_t* get_code() const { return m_codegen.get_code(); }

    /// Get code size
    uint32_t get_size() const { return m_codegen.get_code_size(); }

    /// Get instruction count
    size_t get_instruction_count() const {
        return m_codegen.get_instruction_count();
    }

private:
    std::unique_ptr<uint8_t[]> m_code_buffer;
    CodeGenerator m_codegen;
};

// =============================================================================
// EXAMPLE USAGE / TEST
// =============================================================================

/// Create a simple function prologue/epilogue
void example_function_prologue() {
    Assembler asm_;

    // Function prologue for x64 ABI
    asm_.push(Register::RBP);           // PUSH RBP
    asm_.mov(Register::RBP, Register::RSP);  // MOV RBP, RSP
    asm_.sub(Register::RSP, 32);        // SUB RSP, 32 (allocate stack)

    // Function body (placeholder)
    asm_.mov(Register::RAX, 0x0000000000000001ull);  // MOV RAX, 1 (return value)

    // Function epilogue
    asm_.mov(Register::RSP, Register::RBP);  // MOV RSP, RBP
    asm_.pop(Register::RBP);            // POP RBP
    asm_.ret();                          // RET (not encoded, placeholder)

    // Output
    printf("Generated code size: %u bytes\n", asm_.get_size());
    printf("Instructions: %zu\n", asm_.get_instruction_count());

    // Dump bytes
    const uint8_t* code = asm_.get_code();
    printf("Bytes: ");
    for (uint32_t i = 0; i < asm_.get_size(); i++) {
        printf("%02X ", code[i]);
    }
    printf("\n");
}

} // namespace compiler
} // namespace rawrxd

// =============================================================================
// C++ MAIN ENTRY POINT
// =============================================================================

#include <cstdio>

int main() {
    printf("=== RawrXD x64 Encoder Integration Test ===\n\n");

    try {
        // Create assembler instance
        rawrxd::compiler::Assembler asm_;

        // Generate some code
        asm_.mov(rawrxd::encoder::Register::RAX, 0x1234567890ABCDEFull);
        asm_.mov(rawrxd::encoder::Register::RCX, rawrxd::encoder::Register::RDX);
        asm_.push(rawrxd::encoder::Register::R15);
        asm_.add(rawrxd::encoder::Register::R8, rawrxd::encoder::Register::R9);
        asm_.sub(rawrxd::encoder::Register::R10, 100);
        asm_.call(0x1000);
        asm_.jmp(-4);

        // Display results
        printf("Total instructions: %zu\n", asm_.get_instruction_count());
        printf("Total code size: %u bytes\n", asm_.get_size());
        printf("\nGenerated bytes:\n");

        const uint8_t* code = asm_.get_code();
        for (uint32_t i = 0; i < asm_.get_size(); i++) {
            printf("%02X ", code[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        printf("\n\n");

        printf("✓ Integration test PASSED\n");
        return 0;

    } catch (const std::exception& e) {
        printf("✗ Integration test FAILED: %s\n", e.what());
        return 1;
    }
}
