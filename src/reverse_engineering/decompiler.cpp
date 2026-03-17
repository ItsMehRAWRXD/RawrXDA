// ============================================================================
// RawrXD Decompiler - Pattern-Based Decompilation Engine Implementation
// Converts disassembled x86/x64 instructions to C/C++ pseudocode
// Pure Win32, No External Dependencies
// ============================================================================

#include "decompiler.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <sstream>

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// Constructor
// ============================================================================

Decompiler::Decompiler()
    : m_language("c")
    , m_indentString("    ")
    , m_showAddresses(true)
    , m_showComments(true)
    , m_verbose(false)
    , m_varCounter(0)
    , m_argCounter(0)
    , m_labelCounter(0)
{
    InitializePatterns();
}

void Decompiler::InitializePatterns()
{
    // Common idiom patterns
    
    // XOR reg, reg -> reg = 0
    InstructionPattern zeroReg;
    zeroReg.name = "zero_register";
    zeroReg.description = "Zero register via XOR";
    zeroReg.sequence = { InstructionType::Xor };
    zeroReg.codeTemplate = "$dst = 0;";
    m_patterns.push_back(zeroReg);
    
    // TEST reg, reg; JZ -> if (reg == 0)
    InstructionPattern testZero;
    testZero.name = "test_zero";
    testZero.description = "Test if register is zero";
    testZero.sequence = { InstructionType::Test, InstructionType::Jcc };
    testZero.codeTemplate = "if ($reg == 0)";
    m_patterns.push_back(testZero);
    
    // CMP reg, imm; Jcc -> if (reg <op> imm)
    InstructionPattern cmpBranch;
    cmpBranch.name = "compare_branch";
    cmpBranch.description = "Compare and branch";
    cmpBranch.sequence = { InstructionType::Cmp, InstructionType::Jcc };
    cmpBranch.codeTemplate = "if ($left $op $right)";
    m_patterns.push_back(cmpBranch);
    
    // LEA reg, [reg+reg*scale+disp] -> array indexing
    InstructionPattern arrayIndex;
    arrayIndex.name = "array_index";
    arrayIndex.description = "Array element access";
    arrayIndex.sequence = { InstructionType::Lea };
    arrayIndex.codeTemplate = "$dst = &$base[$index];";
    m_patterns.push_back(arrayIndex);
    
    // PUSH rbp; MOV rbp, rsp -> function prologue
    InstructionPattern prologue;
    prologue.name = "function_prologue";
    prologue.description = "Standard function prologue";
    prologue.sequence = { InstructionType::Push, InstructionType::Mov };
    prologue.codeTemplate = "// function prologue";
    m_patterns.push_back(prologue);
}

// ============================================================================
// Configuration
// ============================================================================

void Decompiler::SetOutputLanguage(const std::string& lang)
{
    if (lang == "c" || lang == "cpp" || lang == "pseudo") {
        m_language = lang;
    } else {
        m_language = "c";
    }
}

// ============================================================================
// Core Decompilation
// ============================================================================

std::string Decompiler::DecompileFunction(const std::vector<Instruction>& instructions)
{
    DecompiledFunction func = DecompileFunctionEx(instructions);
    return FormatDecompiledFunction(func);
}

DecompiledFunction Decompiler::DecompileFunctionEx(const std::vector<Instruction>& instructions)
{
    DecompiledFunction func;
    m_varCounter = 0;
    m_argCounter = 0;
    m_labelCounter = 0;
    m_registerValues.clear();
    
    if (instructions.empty()) {
        func.name = "unknown_function";
        func.returnType = "void";
        return func;
    }
    
    func.startAddress = instructions.front().address;
    func.endAddress = instructions.back().address;
    
    // Generate function name from address
    char nameBuf[32];
    snprintf(nameBuf, sizeof(nameBuf), "sub_%llX", (unsigned long long)func.startAddress);
    func.name = nameBuf;
    
    // Analyze function structure
    AnalyzePrologue(instructions, func);
    AnalyzeStackFrame(instructions, func);
    InferParameters(instructions, func);
    InferLocals(instructions, func);
    
    // Generate function signature
    std::ostringstream sig;
    sig << func.returnType << " " << func.name << "(";
    for (size_t i = 0; i < func.parameters.size(); ++i) {
        if (i > 0) sig << ", ";
        sig << GetTypeString(func.parameters[i].type) << " " << func.parameters[i].name;
    }
    sig << ")";
    func.signature = sig.str();
    
    // Track state for control flow
    bool inPrologue = true;
    bool sawCmp = false;
    Instruction lastCmp;
    
    // Decompile instructions
    for (size_t i = 0; i < instructions.size(); ++i) {
        const Instruction& inst = instructions[i];
        DecompiledStatement stmt;
        stmt.address = inst.address;
        
        // Skip prologue instructions
        if (inPrologue) {
            if (inst.type == InstructionType::Push || 
                (inst.type == InstructionType::Mov && inst.operandsStr.find("rbp") != std::string::npos) ||
                (inst.type == InstructionType::Sub && inst.operandsStr.find("rsp") != std::string::npos)) {
                continue;
            }
            inPrologue = false;
        }
        
        // Skip epilogue
        if (inst.type == InstructionType::Leave || 
            (inst.type == InstructionType::Pop && inst.operandsStr.find("rbp") != std::string::npos) ||
            (inst.type == InstructionType::Add && inst.operandsStr.find("rsp") != std::string::npos)) {
            continue;
        }
        
        // Handle comparison
        if (inst.type == InstructionType::Cmp || inst.type == InstructionType::Test) {
            sawCmp = true;
            lastCmp = inst;
            continue; // Will be handled with the following branch
        }
        
        // Handle branches
        if (inst.isBranch && inst.isConditional && sawCmp) {
            sawCmp = false;
            std::string condition = GenerateCondition(lastCmp, inst.type);
            stmt.code = "if (" + condition + ") {";
            stmt.comment = "branch to " + std::to_string(inst.branchTarget);
            func.body.push_back(stmt);
            
            DecompiledStatement gotoStmt;
            gotoStmt.address = inst.address;
            gotoStmt.indentLevel = 1;
            char labelBuf[32];
            snprintf(labelBuf, sizeof(labelBuf), "goto label_%llX;", (unsigned long long)inst.branchTarget);
            gotoStmt.code = labelBuf;
            func.body.push_back(gotoStmt);
            
            DecompiledStatement closeStmt;
            closeStmt.code = "}";
            func.body.push_back(closeStmt);
            continue;
        }
        
        sawCmp = false;
        
        // Generate code based on instruction type
        switch (inst.type) {
            case InstructionType::Mov:
            case InstructionType::Movzx:
            case InstructionType::Movsx:
            case InstructionType::Movsxd:
                stmt.code = GenerateAssignment(inst);
                break;
                
            case InstructionType::Lea:
                stmt.code = GenerateAssignment(inst);
                break;
                
            case InstructionType::Add:
            case InstructionType::Sub:
            case InstructionType::Imul:
            case InstructionType::And:
            case InstructionType::Or:
            case InstructionType::Xor:
            case InstructionType::Shl:
            case InstructionType::Shr:
            case InstructionType::Sar:
                stmt.code = GenerateExpression(inst);
                break;
                
            case InstructionType::Inc:
                stmt.code = OperandToExpression(inst.operands[0]) + "++;";
                break;
                
            case InstructionType::Dec:
                stmt.code = OperandToExpression(inst.operands[0]) + "--;";
                break;
                
            case InstructionType::Not:
                stmt.code = OperandToExpression(inst.operands[0]) + " = ~" + 
                            OperandToExpression(inst.operands[0]) + ";";
                break;
                
            case InstructionType::Neg:
                stmt.code = OperandToExpression(inst.operands[0]) + " = -" + 
                            OperandToExpression(inst.operands[0]) + ";";
                break;
                
            case InstructionType::Call:
                stmt.code = GenerateCall(inst, instructions);
                break;
                
            case InstructionType::Ret:
                if (m_registerValues.count(RegisterId::RAX)) {
                    stmt.code = "return " + m_registerValues[RegisterId::RAX] + ";";
                } else {
                    stmt.code = "return;";
                }
                break;
                
            case InstructionType::Jmp:
                if (!inst.isConditional) {
                    char labelBuf[32];
                    snprintf(labelBuf, sizeof(labelBuf), "goto label_%llX;", 
                             (unsigned long long)inst.branchTarget);
                    stmt.code = labelBuf;
                }
                break;
                
            case InstructionType::Push:
                stmt.code = "// push " + OperandToExpression(inst.operands[0]);
                break;
                
            case InstructionType::Pop:
                stmt.code = "// pop " + OperandToExpression(inst.operands[0]);
                break;
                
            case InstructionType::Nop:
            case InstructionType::Int3:
                continue; // Skip
                
            default:
                stmt.code = "// " + inst.mnemonic + " " + inst.operandsStr;
                break;
        }
        
        if (!stmt.code.empty()) {
            func.body.push_back(stmt);
        }
    }
    
    // Infer return type from return statements
    for (const auto& stmt : func.body) {
        if (stmt.code.find("return ") != std::string::npos && 
            stmt.code != "return;") {
            func.returnType = "int64_t"; // Default to 64-bit return
            break;
        }
    }
    if (func.returnType.empty()) {
        func.returnType = "void";
    }
    
    return func;
}

std::string Decompiler::DecompileBasicBlock(const BasicBlock& block)
{
    std::ostringstream oss;
    
    char labelBuf[32];
    snprintf(labelBuf, sizeof(labelBuf), "label_%llX:", (unsigned long long)block.startAddress);
    oss << labelBuf << "\n";
    
    for (const auto& inst : block.instructions) {
        std::string code;
        
        switch (inst.type) {
            case InstructionType::Mov:
            case InstructionType::Lea:
                code = GenerateAssignment(inst);
                break;
            case InstructionType::Call:
                code = GenerateCall(inst, block.instructions);
                break;
            case InstructionType::Ret:
                code = "return;";
                break;
            default:
                code = GenerateExpression(inst);
                break;
        }
        
        if (!code.empty()) {
            oss << m_indentString << code << "\n";
        }
    }
    
    return oss.str();
}

// ============================================================================
// Pattern Recognition
// ============================================================================

std::string Decompiler::RecognizePattern(const std::vector<Instruction>& instructions)
{
    if (instructions.empty()) return "";
    
    std::ostringstream result;
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        const Instruction& inst = instructions[i];
        
        // XOR reg, reg -> zero pattern
        if (inst.type == InstructionType::Xor && 
            inst.operandCount >= 2 &&
            inst.operands[0].type == OperandType::Register &&
            inst.operands[1].type == OperandType::Register &&
            inst.operands[0].reg == inst.operands[1].reg) {
            result << "Pattern: Zero register via XOR\n";
            result << "  " << OperandToExpression(inst.operands[0]) << " = 0;\n";
            continue;
        }
        
        // TEST reg, reg + JZ/JNZ -> null check
        if (inst.type == InstructionType::Test && i + 1 < instructions.size()) {
            const Instruction& next = instructions[i + 1];
            if (next.type == InstructionType::Jcc) {
                std::string reg = OperandToExpression(inst.operands[0]);
                result << "Pattern: Null/zero check\n";
                result << "  if (" << reg << " == 0)\n";
                continue;
            }
        }
        
        // CMP + Jcc -> comparison
        if (inst.type == InstructionType::Cmp && i + 1 < instructions.size()) {
            const Instruction& next = instructions[i + 1];
            if (next.type == InstructionType::Jcc) {
                result << "Pattern: Comparison branch\n";
                result << "  " << GenerateCondition(inst, next.type) << "\n";
                ++i; // Skip the Jcc
                continue;
            }
        }
        
        // LEA with scale -> array access
        if (inst.type == InstructionType::Lea && 
            inst.operandCount >= 2 &&
            inst.operands[1].type == OperandType::Memory &&
            inst.operands[1].scale > 1) {
            result << "Pattern: Array indexing\n";
            std::string dst = OperandToExpression(inst.operands[0]);
            std::string base = RegisterToVariable(inst.operands[1].base);
            std::string index = RegisterToVariable(inst.operands[1].index);
            result << "  " << dst << " = &" << base << "[" << index << "];\n";
            continue;
        }
        
        // IMUL with 3 operands -> multiplication
        if (inst.type == InstructionType::Imul && inst.operandCount >= 3) {
            result << "Pattern: Three-operand multiplication\n";
            result << "  " << OperandToExpression(inst.operands[0]) << " = "
                   << OperandToExpression(inst.operands[1]) << " * "
                   << OperandToExpression(inst.operands[2]) << ";\n";
            continue;
        }
        
        // MOVZX/MOVSX -> type conversion
        if (inst.type == InstructionType::Movzx || inst.type == InstructionType::Movsx) {
            result << "Pattern: Type extension\n";
            std::string castType = (inst.type == InstructionType::Movsx) ? "(int64_t)" : "(uint64_t)";
            result << "  " << OperandToExpression(inst.operands[0]) << " = "
                   << castType << OperandToExpression(inst.operands[1]) << ";\n";
            continue;
        }
    }
    
    return result.str();
}

std::string Decompiler::RecognizeIdiom(const Instruction& inst1, const Instruction& inst2)
{
    // PUSH + CALL -> function argument
    if (inst1.type == InstructionType::Push && inst2.type == InstructionType::Call) {
        return "Function call with pushed argument";
    }
    
    // XOR + MOV -> initialize then assign
    if (inst1.type == InstructionType::Xor && inst2.type == InstructionType::Mov) {
        if (inst1.operands[0].reg == inst1.operands[1].reg) {
            return "Clear register then assign";
        }
    }
    
    // TEST + JZ/JNZ -> conditional
    if (inst1.type == InstructionType::Test && inst2.type == InstructionType::Jcc) {
        return "Conditional branch";
    }
    
    // LEA + MOV -> computed address then store
    if (inst1.type == InstructionType::Lea && inst2.type == InstructionType::Mov) {
        return "Address calculation and store";
    }
    
    return "";
}

// ============================================================================
// Type Inference
// ============================================================================

std::string Decompiler::InferVariableTypes(const std::vector<Instruction>& instructions)
{
    std::ostringstream result;
    std::unordered_map<std::string, VariableType> varTypes;
    
    for (const auto& inst : instructions) {
        for (uint8_t i = 0; i < inst.operandCount && i < 4; ++i) {
            const Operand& op = inst.operands[i];
            
            if (op.type == OperandType::Memory) {
                std::string varName = MemoryToVariable(op);
                VariableType inferred = InferTypeFromUsage(inst, i);
                
                auto it = varTypes.find(varName);
                if (it == varTypes.end()) {
                    varTypes[varName] = inferred;
                } else if (inferred != VariableType::Unknown) {
                    // Refine type if we have better info
                    if (it->second == VariableType::Unknown) {
                        it->second = inferred;
                    }
                }
            }
        }
    }
    
    result << "Inferred variable types:\n";
    for (const auto& kv : varTypes) {
        result << "  " << kv.first << ": " << GetTypeString(kv.second) << "\n";
    }
    
    return result.str();
}

VariableType Decompiler::InferTypeFromUsage(const Instruction& inst, int operandIndex)
{
    // Infer from instruction type
    switch (inst.type) {
        case InstructionType::Call:
            return VariableType::Pointer; // Likely function pointer or data pointer
            
        case InstructionType::Movzx:
            return VariableType::UInt32;
            
        case InstructionType::Movsx:
        case InstructionType::Movsxd:
            return VariableType::Int32;
            
        case InstructionType::Div:
        case InstructionType::Idiv:
        case InstructionType::Mul:
        case InstructionType::Imul:
            return VariableType::Int64;
            
        case InstructionType::Test:
            // TEST often used for flags/boolean checks
            return VariableType::Bool;
            
        default:
            break;
    }
    
    // Infer from operand size
    if (operandIndex < inst.operandCount) {
        uint8_t size = inst.operands[operandIndex].size;
        return InferTypeFromSize(size, true);
    }
    
    return VariableType::Unknown;
}

VariableType Decompiler::InferTypeFromSize(uint8_t size, bool isSigned)
{
    if (isSigned) {
        switch (size) {
            case 1: return VariableType::Int8;
            case 2: return VariableType::Int16;
            case 4: return VariableType::Int32;
            case 8: return VariableType::Int64;
            default: return VariableType::Unknown;
        }
    } else {
        switch (size) {
            case 1: return VariableType::UInt8;
            case 2: return VariableType::UInt16;
            case 4: return VariableType::UInt32;
            case 8: return VariableType::UInt64;
            default: return VariableType::Unknown;
        }
    }
}

// ============================================================================
// Expression Generation
// ============================================================================

std::string Decompiler::GenerateExpression(const Instruction& inst)
{
    if (inst.operandCount == 0) return "";
    
    std::string dst = OperandToExpression(inst.operands[0]);
    std::string op;
    
    switch (inst.type) {
        case InstructionType::Add: op = "+"; break;
        case InstructionType::Sub: op = "-"; break;
        case InstructionType::Imul:
        case InstructionType::Mul: op = "*"; break;
        case InstructionType::Idiv:
        case InstructionType::Div: op = "/"; break;
        case InstructionType::And: op = "&"; break;
        case InstructionType::Or: op = "|"; break;
        case InstructionType::Xor: op = "^"; break;
        case InstructionType::Shl:
        case InstructionType::Sal: op = "<<"; break;
        case InstructionType::Shr: op = ">>"; break;
        case InstructionType::Sar: op = ">>"; break; // Arithmetic shift (same in C for signed)
        default:
            return "// " + inst.mnemonic + " " + inst.operandsStr;
    }
    
    // XOR reg, reg is a common idiom for zeroing
    if (inst.type == InstructionType::Xor && inst.operandCount >= 2 &&
        inst.operands[0].type == OperandType::Register &&
        inst.operands[1].type == OperandType::Register &&
        inst.operands[0].reg == inst.operands[1].reg) {
        // Update tracking
        m_registerValues[inst.operands[0].reg] = "0";
        return dst + " = 0;";
    }
    
    if (inst.operandCount >= 2) {
        std::string src = OperandToExpression(inst.operands[1]);
        std::string result = dst + " " + op + "= " + src + ";";
        return result;
    }
    
    return dst + " " + op + "= ???;";
}

std::string Decompiler::GenerateAssignment(const Instruction& inst)
{
    if (inst.operandCount < 2) return "";
    
    std::string dst = OperandToExpression(inst.operands[0]);
    std::string src = OperandToExpression(inst.operands[1]);
    
    // Track register values
    if (inst.operands[0].type == OperandType::Register) {
        m_registerValues[inst.operands[0].reg] = src;
    }
    
    // Type cast for MOVZX/MOVSX
    if (inst.type == InstructionType::Movzx) {
        std::string castType = GetTypeString(InferTypeFromSize(inst.operands[0].size, false));
        return dst + " = (" + castType + ")(unsigned)" + src + ";";
    }
    if (inst.type == InstructionType::Movsx || inst.type == InstructionType::Movsxd) {
        std::string castType = GetTypeString(InferTypeFromSize(inst.operands[0].size, true));
        return dst + " = (" + castType + ")(signed)" + src + ";";
    }
    
    // LEA generates address calculation
    if (inst.type == InstructionType::Lea) {
        if (inst.operands[1].type == OperandType::Memory) {
            return dst + " = " + src + "; // effective address";
        }
    }
    
    return dst + " = " + src + ";";
}

std::string Decompiler::GenerateCall(const Instruction& inst, const std::vector<Instruction>& context)
{
    std::string target;
    
    // Get call target
    if (inst.branchTarget != 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "sub_%llX", (unsigned long long)inst.branchTarget);
        target = buf;
    } else if (inst.operandCount > 0) {
        target = OperandToExpression(inst.operands[0]);
    } else {
        target = "unknown_func";
    }
    
    // Try to get arguments from context (Windows x64 calling convention)
    std::vector<std::string> args;
    
    // Look for RCX, RDX, R8, R9 setup before call
    for (const auto& prev : context) {
        if (prev.address >= inst.address) break;
        
        if (prev.type == InstructionType::Mov && prev.operandCount >= 2 &&
            prev.operands[0].type == OperandType::Register) {
            RegisterId reg = prev.operands[0].reg;
            if (reg == RegisterId::RCX || reg == RegisterId::RDX ||
                reg == RegisterId::R8 || reg == RegisterId::R9) {
                // Found argument setup
            }
        }
    }
    
    // Check for stored result
    std::string result_assignment;
    bool hasResult = false;
    
    // Look at instruction after call for result usage
    // (would need context after call, simplify for now)
    
    if (hasResult) {
        return "result = " + target + "();";
    }
    
    return target + "();";
}

std::string Decompiler::GenerateCondition(const Instruction& cmpInst, InstructionType jccType)
{
    std::string left, right;
    
    if (cmpInst.operandCount >= 2) {
        left = OperandToExpression(cmpInst.operands[0]);
        right = OperandToExpression(cmpInst.operands[1]);
    } else if (cmpInst.operandCount >= 1) {
        left = OperandToExpression(cmpInst.operands[0]);
        right = "0";
    } else {
        return "/* unknown condition */";
    }
    
    // TEST instruction sets ZF based on AND result
    if (cmpInst.type == InstructionType::Test) {
        if (cmpInst.operandCount >= 2 &&
            cmpInst.operands[0].type == OperandType::Register &&
            cmpInst.operands[1].type == OperandType::Register &&
            cmpInst.operands[0].reg == cmpInst.operands[1].reg) {
            // TEST reg, reg -> check if zero
            return JccToCondition(jccType) == "==" ? 
                   left + " == 0" : left + " != 0";
        }
        return "(" + left + " & " + right + ") " + JccToCondition(jccType) + " 0";
    }
    
    return left + " " + JccToCondition(jccType) + " " + right;
}

// ============================================================================
// Control Flow Reconstruction
// ============================================================================

std::vector<DecompiledStatement> Decompiler::ReconstructControlFlow(
    const std::vector<BasicBlock>& blocks)
{
    std::vector<DecompiledStatement> result;
    
    for (size_t i = 0; i < blocks.size(); ++i) {
        const BasicBlock& bb = blocks[i];
        
        // Add label
        DecompiledStatement label;
        char labelBuf[32];
        snprintf(labelBuf, sizeof(labelBuf), "label_%llX:", (unsigned long long)bb.startAddress);
        label.code = labelBuf;
        label.address = bb.startAddress;
        result.push_back(label);
        
        // Check for loop
        if (bb.isLoopHeader) {
            auto loopStatements = ReconstructLoop(blocks, i);
            result.insert(result.end(), loopStatements.begin(), loopStatements.end());
            continue;
        }
        
        // Check for if/else
        if (!bb.instructions.empty() && bb.instructions.back().isConditional) {
            auto ifStatements = ReconstructIfElse(blocks, i);
            result.insert(result.end(), ifStatements.begin(), ifStatements.end());
            continue;
        }
        
        // Regular block
        for (const auto& inst : bb.instructions) {
            DecompiledStatement stmt;
            stmt.address = inst.address;
            stmt.indentLevel = 1;
            stmt.code = GenerateExpression(inst);
            if (!stmt.code.empty()) {
                result.push_back(stmt);
            }
        }
    }
    
    return result;
}

std::vector<DecompiledStatement> Decompiler::ReconstructLoop(
    const std::vector<BasicBlock>& blocks, size_t loopHeaderIdx)
{
    std::vector<DecompiledStatement> result;
    
    if (loopHeaderIdx >= blocks.size()) return result;
    
    const BasicBlock& header = blocks[loopHeaderIdx];
    
    // Simple while loop reconstruction
    DecompiledStatement whileStart;
    whileStart.code = "while (/* condition */) {";
    whileStart.address = header.startAddress;
    result.push_back(whileStart);
    
    // Loop body
    for (const auto& inst : header.instructions) {
        DecompiledStatement stmt;
        stmt.address = inst.address;
        stmt.indentLevel = 1;
        stmt.code = GenerateExpression(inst);
        if (!stmt.code.empty()) {
            result.push_back(stmt);
        }
    }
    
    DecompiledStatement whileEnd;
    whileEnd.code = "}";
    result.push_back(whileEnd);
    
    return result;
}

std::vector<DecompiledStatement> Decompiler::ReconstructIfElse(
    const std::vector<BasicBlock>& blocks, size_t branchIdx)
{
    std::vector<DecompiledStatement> result;
    
    if (branchIdx >= blocks.size()) return result;
    
    const BasicBlock& bb = blocks[branchIdx];
    
    // Generate if statement
    DecompiledStatement ifStart;
    ifStart.code = "if (/* condition */) {";
    ifStart.address = bb.startAddress;
    result.push_back(ifStart);
    
    // Then branch (simplify - just add comment)
    DecompiledStatement thenStmt;
    thenStmt.code = "// then branch";
    thenStmt.indentLevel = 1;
    result.push_back(thenStmt);
    
    DecompiledStatement ifEnd;
    ifEnd.code = "}";
    result.push_back(ifEnd);
    
    // Check for else branch
    if (bb.successors.size() > 1) {
        DecompiledStatement elseStart;
        elseStart.code = "else {";
        result.push_back(elseStart);
        
        DecompiledStatement elseStmt;
        elseStmt.code = "// else branch";
        elseStmt.indentLevel = 1;
        result.push_back(elseStmt);
        
        DecompiledStatement elseEnd;
        elseEnd.code = "}";
        result.push_back(elseEnd);
    }
    
    return result;
}

// ============================================================================
// Output Formatting
// ============================================================================

std::string Decompiler::FormatDecompiledFunction(const DecompiledFunction& func)
{
    std::ostringstream oss;
    
    // Function header
    oss << "// Function at 0x" << std::hex << func.startAddress << std::dec << "\n";
    oss << "// Size: " << (func.endAddress - func.startAddress) << " bytes\n";
    oss << "\n";
    
    // Local variable declarations
    if (!func.locals.empty()) {
        oss << "// Local variables:\n";
        for (const auto& var : func.locals) {
            oss << "// " << GetTypeString(var.type) << " " << var.name;
            if (var.stackOffset != 0) {
                oss << " @ [rbp" << (var.stackOffset >= 0 ? "+" : "") << var.stackOffset << "]";
            }
            oss << "\n";
        }
        oss << "\n";
    }
    
    // Function signature
    oss << func.signature << "\n{\n";
    
    // Body
    oss << FormatStatements(func.body);
    
    oss << "}\n";
    
    return oss.str();
}

std::string Decompiler::FormatStatements(const std::vector<DecompiledStatement>& statements)
{
    std::ostringstream oss;
    
    for (const auto& stmt : statements) {
        // Indentation
        for (int i = 0; i < stmt.indentLevel + 1; ++i) {
            oss << m_indentString;
        }
        
        // Code
        oss << stmt.code;
        
        // Address comment
        if (m_showAddresses && stmt.address != 0) {
            oss << " // 0x" << std::hex << stmt.address << std::dec;
        }
        
        // Additional comment
        if (m_showComments && !stmt.comment.empty()) {
            oss << " // " << stmt.comment;
        }
        
        oss << "\n";
    }
    
    return oss.str();
}

std::string Decompiler::GetTypeString(VariableType type)
{
    if (m_language == "cpp") {
        switch (type) {
            case VariableType::Int8: return "int8_t";
            case VariableType::Int16: return "int16_t";
            case VariableType::Int32: return "int32_t";
            case VariableType::Int64: return "int64_t";
            case VariableType::UInt8: return "uint8_t";
            case VariableType::UInt16: return "uint16_t";
            case VariableType::UInt32: return "uint32_t";
            case VariableType::UInt64: return "uint64_t";
            case VariableType::Float32: return "float";
            case VariableType::Float64: return "double";
            case VariableType::Pointer: return "void*";
            case VariableType::String: return "char*";
            case VariableType::Struct: return "struct_t";
            case VariableType::Array: return "array_t";
            case VariableType::Bool: return "bool";
            case VariableType::Void: return "void";
            default: return "auto";
        }
    } else {
        // C style
        switch (type) {
            case VariableType::Int8: return "char";
            case VariableType::Int16: return "short";
            case VariableType::Int32: return "int";
            case VariableType::Int64: return "long long";
            case VariableType::UInt8: return "unsigned char";
            case VariableType::UInt16: return "unsigned short";
            case VariableType::UInt32: return "unsigned int";
            case VariableType::UInt64: return "unsigned long long";
            case VariableType::Float32: return "float";
            case VariableType::Float64: return "double";
            case VariableType::Pointer: return "void*";
            case VariableType::String: return "char*";
            case VariableType::Struct: return "struct_t";
            case VariableType::Array: return "array_t";
            case VariableType::Bool: return "int";
            case VariableType::Void: return "void";
            default: return "int";
        }
    }
}

// ============================================================================
// Internal Helpers
// ============================================================================

void Decompiler::AnalyzePrologue(const std::vector<Instruction>& instructions,
                                  DecompiledFunction& func)
{
    if (instructions.empty()) return;
    
    // Look for standard prologue: push rbp; mov rbp, rsp; sub rsp, X
    size_t i = 0;
    
    // Check for push rbp
    if (i < instructions.size() && instructions[i].type == InstructionType::Push) {
        ++i;
    }
    
    // Check for mov rbp, rsp
    if (i < instructions.size() && instructions[i].type == InstructionType::Mov) {
        ++i;
    }
    
    // Check for sub rsp, X (stack allocation)
    if (i < instructions.size() && instructions[i].type == InstructionType::Sub) {
        if (instructions[i].operandCount >= 2 &&
            instructions[i].operands[1].type == OperandType::Immediate) {
            // Found stack frame size
            // func.stackFrameSize would be set from operand
        }
        ++i;
    }
}

void Decompiler::AnalyzeEpilogue(const std::vector<Instruction>& instructions,
                                  DecompiledFunction& func)
{
    // Look backwards for epilogue: leave; ret or add rsp, X; pop rbp; ret
    // This affects return type inference
}

void Decompiler::AnalyzeStackFrame(const std::vector<Instruction>& instructions,
                                    DecompiledFunction& func)
{
    // Scan for stack accesses to identify locals and parameters
    std::unordered_map<int64_t, uint8_t> stackAccesses; // offset -> size
    
    for (const auto& inst : instructions) {
        for (uint8_t i = 0; i < inst.operandCount && i < 4; ++i) {
            const Operand& op = inst.operands[i];
            if (op.type == OperandType::Memory && 
                (op.base == RegisterId::RBP || op.base == RegisterId::RSP)) {
                int64_t offset = op.displacement;
                if (stackAccesses.find(offset) == stackAccesses.end() ||
                    stackAccesses[offset] < op.size) {
                    stackAccesses[offset] = op.size;
                }
            }
        }
    }
    
    // Classify: negative offsets = locals, positive offsets = parameters/return
    for (const auto& kv : stackAccesses) {
        Variable var;
        var.stackOffset = kv.first;
        var.size = kv.second;
        var.type = InferTypeFromSize(kv.second, true);
        
        if (kv.first < 0) {
            var.isLocal = true;
            var.isParameter = false;
            char nameBuf[16];
            snprintf(nameBuf, sizeof(nameBuf), "var_%X", (unsigned)(-kv.first));
            var.name = nameBuf;
            func.locals.push_back(var);
        } else if (kv.first >= 16) { // Skip return address area
            var.isLocal = false;
            var.isParameter = true;
            char nameBuf[16];
            snprintf(nameBuf, sizeof(nameBuf), "arg_%X", (unsigned)kv.first);
            var.name = nameBuf;
            func.parameters.push_back(var);
        }
    }
}

void Decompiler::InferParameters(const std::vector<Instruction>& instructions,
                                  DecompiledFunction& func)
{
    // Look for accesses to RCX, RDX, R8, R9 (Windows x64) at function start
    bool usesRCX = false, usesRDX = false, usesR8 = false, usesR9 = false;
    
    for (size_t i = 0; i < instructions.size() && i < 20; ++i) {
        const Instruction& inst = instructions[i];
        for (uint8_t j = 0; j < inst.operandCount && j < 4; ++j) {
            if (inst.operands[j].type == OperandType::Register) {
                RegisterId reg = inst.operands[j].reg;
                if (reg == RegisterId::RCX) usesRCX = true;
                if (reg == RegisterId::RDX) usesRDX = true;
                if (reg == RegisterId::R8) usesR8 = true;
                if (reg == RegisterId::R9) usesR9 = true;
            }
        }
    }
    
    // Add inferred register parameters
    if (usesRCX && func.parameters.empty()) {
        Variable arg;
        arg.name = "arg1";
        arg.type = VariableType::Int64;
        arg.reg = RegisterId::RCX;
        arg.isParameter = true;
        func.parameters.push_back(arg);
    }
    if (usesRDX) {
        Variable arg;
        arg.name = "arg2";
        arg.type = VariableType::Int64;
        arg.reg = RegisterId::RDX;
        arg.isParameter = true;
        func.parameters.push_back(arg);
    }
}

void Decompiler::InferLocals(const std::vector<Instruction>& instructions,
                              DecompiledFunction& func)
{
    // Already handled in AnalyzeStackFrame
}

std::string Decompiler::OperandToExpression(const Operand& op, uint8_t size)
{
    if (size == 0) size = op.size;
    char buf[128];
    
    switch (op.type) {
        case OperandType::Register:
            return RegisterToVariable(op.reg);
            
        case OperandType::Immediate:
            return ImmediateToString(op.displacement, size);
            
        case OperandType::Memory:
            return MemoryToVariable(op);
            
        case OperandType::RelOffset:
            snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)op.displacement);
            return buf;
            
        default:
            return "???";
    }
}

std::string Decompiler::RegisterToVariable(RegisterId reg)
{
    // Check if we have a tracked value
    auto it = m_registerValues.find(reg);
    if (it != m_registerValues.end()) {
        return it->second;
    }
    
    // Return register name
    uint8_t code = static_cast<uint8_t>(reg) & 0x0F;
    static const char* names[] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };
    
    if (code < 16) return names[code];
    return "reg?";
}

std::string Decompiler::MemoryToVariable(const Operand& op)
{
    std::ostringstream oss;
    
    // Check for common patterns
    if (op.base == RegisterId::RBP) {
        if (op.displacement < 0) {
            // Local variable
            char buf[16];
            snprintf(buf, sizeof(buf), "var_%X", (unsigned)(-op.displacement));
            return buf;
        } else if (op.displacement > 0) {
            // Stack parameter
            char buf[16];
            snprintf(buf, sizeof(buf), "arg_%X", (unsigned)op.displacement);
            return buf;
        }
    }
    
    // Generate pointer dereference
    oss << "*(";
    
    bool needsPlus = false;
    if (op.isRipRelative) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)op.displacement);
        return buf; // Global variable
    }
    
    if (op.base != RegisterId::NONE) {
        oss << RegisterToVariable(op.base);
        needsPlus = true;
    }
    
    if (op.index != RegisterId::NONE) {
        if (needsPlus) oss << " + ";
        oss << RegisterToVariable(op.index);
        if (op.scale > 1) {
            oss << "*" << (int)op.scale;
        }
        needsPlus = true;
    }
    
    if (op.displacement != 0) {
        if (op.displacement > 0) {
            if (needsPlus) oss << " + ";
            oss << "0x" << std::hex << op.displacement << std::dec;
        } else {
            oss << " - 0x" << std::hex << (-op.displacement) << std::dec;
        }
    }
    
    oss << ")";
    return oss.str();
}

std::string Decompiler::ImmediateToString(int64_t value, uint8_t size)
{
    char buf[32];
    
    // Small values as decimal
    if (value >= -100 && value <= 100) {
        snprintf(buf, sizeof(buf), "%lld", (long long)value);
        return buf;
    }
    
    // Hex for larger values
    if (size <= 2) {
        snprintf(buf, sizeof(buf), "0x%04llX", (unsigned long long)(value & 0xFFFF));
    } else if (size <= 4) {
        snprintf(buf, sizeof(buf), "0x%08llX", (unsigned long long)(value & 0xFFFFFFFF));
    } else {
        snprintf(buf, sizeof(buf), "0x%016llX", (unsigned long long)value);
    }
    
    return buf;
}

std::string Decompiler::InvertCondition(const std::string& condition)
{
    if (condition == "==") return "!=";
    if (condition == "!=") return "==";
    if (condition == "<") return ">=";
    if (condition == ">=") return "<";
    if (condition == ">") return "<=";
    if (condition == "<=") return ">";
    return "!(" + condition + ")";
}

std::string Decompiler::JccToCondition(InstructionType jccType)
{
    // Map Jcc type to comparison operator
    // Note: These need to be from the perspective of the original CMP instruction
    switch (jccType) {
        case InstructionType::Jcc:
            return "?"; // Generic
        default:
            break;
    }
    
    // For specific conditions, we'd need the full mnemonic
    // Default to equality check
    return "==";
}

} // namespace ReverseEngineering
} // namespace RawrXD
