// codegen_integration.cpp - Integration patch for masm_solo_compiler
// This bridges your existing AST to the new x64_codegen_engine
//
// Usage: Include this file in your main compiler source and call GenerateCode()
// after AST validation completes.

#include "x64_codegen_engine.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace RawrXD {
namespace CodeGen {

// Forward declare your AST node type (adjust to match your actual implementation)
struct ASTNode {
    NodeType type;
    std::string value;
    std::vector<ASTNode*> children;
    int64_t immediate_value;
};

// Helper to convert string to lowercase
static std::string toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Main code generation entry point
// Replace your failing codegen with this function
bool GenerateCode(ASTNode* root, const std::string& output_path) {
    CodeGenerator codegen;
    std::vector<Instruction> instruction_buffer;
    
    // Register name mapping
    auto parse_register = [](const std::string& name) -> Register {
        static const std::unordered_map<std::string, Register> reg_map = {
            {"rax", Register::RAX}, {"rcx", Register::RCX}, {"rdx", Register::RDX},
            {"rbx", Register::RBX}, {"rsp", Register::RSP}, {"rbp", Register::RBP},
            {"rsi", Register::RSI}, {"rdi", Register::RDI},
            {"r8", Register::R8}, {"r9", Register::R9}, {"r10", Register::R10},
            {"r11", Register::R11}, {"r12", Register::R12}, {"r13", Register::R13},
            {"r14", Register::R14}, {"r15", Register::R15},
            // 32-bit registers (for zero-extending moves)
            {"eax", Register::EAX}, {"ecx", Register::ECX}, {"edx", Register::EDX},
            {"ebx", Register::EBX}, {"esp", Register::ESP}, {"ebp", Register::EBP},
            {"esi", Register::ESI}, {"edi", Register::EDI},
            {"r8d", Register::R8D}, {"r9d", Register::R9D}, {"r10d", Register::R10D},
            {"r11d", Register::R11D}, {"r12d", Register::R12D}, {"r13d", Register::R13D},
            {"r14d", Register::R14D}, {"r15d", Register::R15D}
        };
        
        std::string lower = toLower(name);
        auto it = reg_map.find(lower);
        return (it != reg_map.end()) ? it->second : Register::NONE;
    };
    
    // Recursive AST walker
    std::function<void(ASTNode*)> walk = [&](ASTNode* node) {
        if (!node) return;
        
        if (node->type == NodeType::INSTRUCTION) {
            Instruction inst;
            inst.mnemonic = toLower(node->value);
            
            for (auto& child : node->children) {
                Operand op;
                
                if (child->type == NodeType::REGISTER) {
                    op.type = OperandType::REG;
                    op.reg = parse_register(child->value);
                    if (op.reg == Register::NONE) {
                        std::cerr << "[CODEGEN] Unknown register: " << child->value << std::endl;
                    }
                } else if (child->type == NodeType::IMMEDIATE) {
                    op.type = OperandType::IMM;
                    // Handle different immediate formats
                    const std::string& val = child->value;
                    if (val.length() > 2 && (val[1] == 'x' || val[1] == 'X')) {
                        // Hex: 0x...
                        op.immediate = std::stoll(val, nullptr, 16);
                    } else if (val.length() > 1 && (val.back() == 'h' || val.back() == 'H')) {
                        // Hex suffix: ...h
                        op.immediate = std::stoll(val.substr(0, val.length() - 1), nullptr, 16);
                    } else {
                        // Decimal
                        op.immediate = std::stoll(val);
                    }
                } else if (child->type == NodeType::LABEL_REF) {
                    op.type = OperandType::LABEL_REF;
                    op.label_name = child->value;
                } else if (child->type == NodeType::MEMORY) {
                    op.type = OperandType::MEM;
                    // Memory operand parsing would go here
                    // For now, handle simple [reg] or [reg+disp]
                    if (!child->children.empty()) {
                        auto& mem_child = child->children[0];
                        if (mem_child->type == NodeType::REGISTER) {
                            op.base = parse_register(mem_child->value);
                        }
                    }
                }
                
                inst.operands.push_back(op);
            }
            
            instruction_buffer.push_back(inst);
            
        } else if (node->type == NodeType::LABEL_DEF) {
            codegen.visit_label(node->value);
        }
        
        // Recurse into children
        for (auto& child : node->children) {
            walk(child);
        }
    };
    
    // Walk the AST
    walk(root);
    
    // Generate PE binary
    auto pe_data = codegen.compile(instruction_buffer);
    
    // Write to output file
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        std::cerr << "[CODEGEN] Failed to open output file: " << output_path << std::endl;
        return false;
    }
    
    out.write(reinterpret_cast<const char*>(pe_data.data()), pe_data.size());
    
    if (!out.good()) {
        std::cerr << "[CODEGEN] Failed to write output file" << std::endl;
        return false;
    }
    
    std::cout << "[CODEGEN] Generated " << pe_data.size() << " bytes -> " << output_path << std::endl;
    return true;
}

// Diagnostic function - add to your codegen error handler for verbose output
void PrintInstructionDebug(const Instruction& inst) {
    std::cout << "[CODEGEN] Mnemonic: " << inst.mnemonic 
              << ", Operands: " << inst.operands.size();
    
    for (size_t i = 0; i < inst.operands.size(); ++i) {
        const auto& op = inst.operands[i];
        std::cout << "\n  Op" << i << ": ";
        switch (op.type) {
            case OperandType::REG:
                std::cout << "REG(" << static_cast<int>(op.reg) << ")";
                break;
            case OperandType::IMM:
                std::cout << "IMM(0x" << std::hex << op.immediate << std::dec << ")";
                break;
            case OperandType::MEM:
                std::cout << "MEM";
                break;
            case OperandType::LABEL_REF:
                std::cout << "LABEL(" << op.label_name << ")";
                break;
        }
    }
    std::cout << std::endl;
}

// Standalone test function - compile a minimal program
bool TestCodegen(const std::string& output_path) {
    X64Encoder encoder;
    
    // Emit: 
    //   mov rax, 0x1234567890ABCDEF
    //   xor rcx, rcx
    //   ret
    encoder.encode_mov_reg_imm64(Register::RAX, 0x1234567890ABCDEFULL);
    encoder.encode_xor_reg_reg(Register::RCX, Register::RCX);
    encoder.encode_ret();
    
    auto pe = encoder.generate_pe();
    
    std::ofstream out(output_path, std::ios::binary);
    if (!out) return false;
    
    out.write(reinterpret_cast<const char*>(pe.data()), pe.size());
    std::cout << "[TEST] Generated " << pe.size() << " bytes" << std::endl;
    
    return out.good();
}

// Alternative: Simple text-based instruction input
// This can be used to test the encoder directly from assembly text
std::vector<Instruction> ParseSimpleAsm(const std::string& source) {
    std::vector<Instruction> instructions;
    std::istringstream iss(source);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Skip empty lines and comments
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        if (line[start] == ';') continue;
        
        // Skip directives
        if (line[start] == '.') continue;
        if (line.find("PROC") != std::string::npos) continue;
        if (line.find("ENDP") != std::string::npos) continue;
        if (line.find("END") == 0) continue;
        
        // Parse label
        size_t colon = line.find(':');
        if (colon != std::string::npos && colon > start) {
            // This is a label - skip for now
            line = line.substr(colon + 1);
            start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
        }
        
        // Parse mnemonic
        size_t end = line.find_first_of(" \t,", start);
        if (end == std::string::npos) end = line.length();
        
        std::string mnemonic = toLower(line.substr(start, end - start));
        if (mnemonic.empty()) continue;
        
        Instruction inst;
        inst.mnemonic = mnemonic;
        
        // Parse operands (comma-separated)
        size_t pos = end;
        while (pos < line.length()) {
            pos = line.find_first_not_of(" \t,", pos);
            if (pos == std::string::npos) break;
            if (line[pos] == ';') break; // Comment
            
            size_t op_end = line.find_first_of(" \t,;", pos);
            if (op_end == std::string::npos) op_end = line.length();
            
            std::string op_str = line.substr(pos, op_end - pos);
            
            Operand op;
            // Check if it's a register
            static const std::unordered_map<std::string, Register> reg_map = {
                {"rax", Register::RAX}, {"rcx", Register::RCX}, {"rdx", Register::RDX},
                {"rbx", Register::RBX}, {"rsp", Register::RSP}, {"rbp", Register::RBP},
                {"rsi", Register::RSI}, {"rdi", Register::RDI},
                {"r8", Register::R8}, {"r9", Register::R9}, {"r10", Register::R10},
                {"r11", Register::R11}, {"r12", Register::R12}, {"r13", Register::R13},
                {"r14", Register::R14}, {"r15", Register::R15}
            };
            
            std::string lower_op = toLower(op_str);
            auto it = reg_map.find(lower_op);
            if (it != reg_map.end()) {
                op.type = OperandType::REG;
                op.reg = it->second;
            } else if (op_str.length() > 2 && 
                       (op_str.substr(0, 2) == "0x" || op_str.substr(0, 2) == "0X")) {
                op.type = OperandType::IMM;
                op.immediate = std::stoll(op_str, nullptr, 16);
            } else if (std::isdigit(op_str[0]) || op_str[0] == '-') {
                op.type = OperandType::IMM;
                op.immediate = std::stoll(op_str);
            } else {
                op.type = OperandType::LABEL_REF;
                op.label_name = op_str;
            }
            
            inst.operands.push_back(op);
            pos = op_end;
        }
        
        instructions.push_back(inst);
    }
    
    return instructions;
}

} // namespace CodeGen
} // namespace RawrXD

#endif // CODEGEN_INTEGRATION_CPP
