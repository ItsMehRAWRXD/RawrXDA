/**
 * \file decompiler.cpp
 * \brief Implementation of decompiler
 * \author RawrXD Team
 * \date 2026-01-22
 */

#include "decompiler.h"
#include <QDebug>
#include <QRegularExpression>
#include <algorithm>

using namespace RawrXD::ReverseEngineering;

Decompiler::Decompiler(const Disassembler& disasm)
    : m_disasm(disasm) {
}

DecompiledFunction Decompiler::decompile(const FunctionInfo& funcInfo) {
    DecompiledFunction decFunc;
    decFunc.name = funcInfo.name;
    decFunc.address = funcInfo.address;
    
    // Build control flow graph
    decFunc.basicBlocks = buildCFG(funcInfo);
    
    // Extract variables
    decFunc.variables = extractVariables(funcInfo);
    
    // Infer signature
    decFunc.signature = inferSignature(funcInfo);
    
    // Calculate complexity
    decFunc.complexity = calculateComplexity(decFunc.basicBlocks);
    decFunc.hasLoops = detectLoop(decFunc.basicBlocks);
    
    // Generate pseudo-code for each block
    QString pseudoCode;
    pseudoCode += decFunc.signature + " {\n";
    
    for (int i = 0; i < decFunc.basicBlocks.size(); ++i) {
        auto& block = decFunc.basicBlocks[i];
        
        // Add block label
        pseudoCode += QString("  // Block at 0x%1\n").arg(block.address, 8, 16, QChar('0'));
        
        // Generate pseudo-code for instructions
        for (const auto& instr : block.instructions) {
            QString instrCode = instructionToPseudoCode(instr);
            if (!instrCode.isEmpty()) {
                pseudoCode += "  " + instrCode + "\n";
            }
        }
        
        pseudoCode += "\n";
    }
    
    pseudoCode += "}\n";
    decFunc.pseudoCode = pseudoCode;
    
    m_functions.append(decFunc);
    
    qDebug() << "Decompiled" << funcInfo.name << "with complexity" << decFunc.complexity;
    return decFunc;
}

DecompiledFunction Decompiler::decompileAt(uint64_t address, const QString& name) {
    FunctionInfo funcInfo = m_disasm.functionAt(address);
    if (funcInfo.address == 0) {
        funcInfo.name = name.isEmpty() ? QString("sub_%1").arg(address, 8, 16, QChar('0')) : name;
    }
    return decompile(funcInfo);
}

QVector<ControlFlowBlock> Decompiler::buildCFG(const FunctionInfo& funcInfo) {
    QVector<ControlFlowBlock> blocks;
    
    if (funcInfo.instructions.isEmpty()) {
        return blocks;
    }
    
    ControlFlowBlock currentBlock;
    currentBlock.address = funcInfo.address;
    
    for (const auto& instr : funcInfo.instructions) {
        currentBlock.instructions.append(instr);
        
        // End block on branches/returns/calls
        if (instr.isBranch || instr.isReturn) {
            currentBlock.size = instr.address + instr.bytes.size() - currentBlock.address;
            blocks.append(currentBlock);
            currentBlock = ControlFlowBlock();
            currentBlock.address = instr.address + instr.bytes.size();
        }
    }
    
    // Add last block if not empty
    if (!currentBlock.instructions.isEmpty()) {
        currentBlock.size = funcInfo.instructions.last().address + 
                           funcInfo.instructions.last().bytes.size() - currentBlock.address;
        blocks.append(currentBlock);
    }
    
    qDebug() << "Built CFG with" << blocks.size() << "basic blocks";
    return blocks;
}

QString Decompiler::instructionToPseudoCode(const Instruction& instr) {
    // Convert assembly instruction to pseudo-code
    
    if (instr.mnemonic == "mov" || instr.mnemonic == "movq" || instr.mnemonic == "movl") {
        QStringList ops = instr.operands.split(",");
        if (ops.size() == 2) {
            return QString("%1 = %2;").arg(ops[1].trimmed(), ops[0].trimmed());
        }
    }
    
    if (instr.mnemonic == "add" || instr.mnemonic == "addq" || instr.mnemonic == "addl") {
        QStringList ops = instr.operands.split(",");
        if (ops.size() == 2) {
            return QString("%1 += %2;").arg(ops[1].trimmed(), ops[0].trimmed());
        }
    }
    
    if (instr.mnemonic == "sub" || instr.mnemonic == "subq" || instr.mnemonic == "subl") {
        QStringList ops = instr.operands.split(",");
        if (ops.size() == 2) {
            return QString("%1 -= %2;").arg(ops[1].trimmed(), ops[0].trimmed());
        }
    }
    
    if (instr.mnemonic == "cmp" || instr.mnemonic == "cmpq" || instr.mnemonic == "cmpl") {
        QStringList ops = instr.operands.split(",");
        if (ops.size() == 2) {
            return QString("if (%1 == %2) { ... }").arg(ops[1].trimmed(), ops[0].trimmed());
        }
    }
    
    if (instr.mnemonic == "jmp") {
        return QString("goto 0x%1;").arg(instr.target, 8, 16, QChar('0'));
    }
    
    if (instr.mnemonic == "je" || instr.mnemonic == "jz") {
        return QString("if (equal) goto 0x%1;").arg(instr.target, 8, 16, QChar('0'));
    }
    
    if (instr.mnemonic == "jne" || instr.mnemonic == "jnz") {
        return QString("if (!equal) goto 0x%1;").arg(instr.target, 8, 16, QChar('0'));
    }
    
    if (instr.mnemonic == "call") {
        return QString("// call 0x%1").arg(instr.target, 8, 16, QChar('0'));
    }
    
    if (instr.mnemonic == "ret" || instr.mnemonic == "retn") {
        return "return;";
    }
    
    if (instr.mnemonic == "push") {
        return QString("push(%1);").arg(instr.operands);
    }
    
    if (instr.mnemonic == "pop") {
        return QString("pop(%1);").arg(instr.operands);
    }
    
    // Generic pseudo-code for other instructions
    return QString("// %1 %2").arg(instr.mnemonic, instr.operands);
}

QString Decompiler::simplifyAssembly(const Instruction& instr) {
    // Remove unnecessary details from assembly
    return instr.disassembly;
}

QVector<Variable> Decompiler::extractVariables(const FunctionInfo& funcInfo) {
    QVector<Variable> variables;
    QMap<int, Variable> stackVars;
    
    // Scan for stack accesses
    for (const auto& instr : funcInfo.instructions) {
        // Look for patterns like [rsp+offset]
        QRegularExpression stackPattern("\\[(rsp|rbp|esp|ebp)(\\+|\\-)([0-9a-fA-Fx]+)\\]");
        QRegularExpressionMatch match = stackPattern.match(instr.operands);
        
        if (match.hasMatch()) {
            bool ok;
            int offset = match.captured(3).toInt(&ok, 0);
            if (match.captured(2) == "-") {
                offset = -offset;
            }
            
            if (!stackVars.contains(offset)) {
                Variable var;
                var.name = QString("var_%1").arg(std::abs(offset));
                var.stackOffset = offset;
                var.type = "int";  // Default type
                stackVars[offset] = var;
            }
        }
    }
    
    for (auto it = stackVars.begin(); it != stackVars.end(); ++it) {
        variables.append(it.value());
    }
    
    return variables;
}

QString Decompiler::inferSignature(const FunctionInfo& funcInfo) {
    // Infer function signature from calling convention and assembly
    
    QString paramList;
    int paramCount = 0;
    
    // Common calling conventions
    // x64: rcx, rdx, r8, r9 (Windows) or rdi, rsi, rdx, rcx, r8, r9 (Unix)
    // x86: Arguments on stack
    
    // Look for common parameter register usage
    bool hasRCX = false, hasRDX = false, hasR8 = false, hasR9 = false;
    
    for (const auto& instr : funcInfo.instructions) {
        if (instr.operands.contains("rcx")) hasRCX = true;
        if (instr.operands.contains("rdx")) hasRDX = true;
        if (instr.operands.contains("r8")) hasR8 = true;
        if (instr.operands.contains("r9")) hasR9 = true;
    }
    
    if (hasRCX) { paramCount++; }
    if (hasRDX) { paramCount++; }
    if (hasR8) { paramCount++; }
    if (hasR9) { paramCount++; }
    
    // Build parameter list
    for (int i = 0; i < paramCount && i < 4; ++i) {
        if (i > 0) paramList += ", ";
        paramList += QString("void* arg%1").arg(i);
    }
    
    return QString("void %1(%2)").arg(funcInfo.name, paramList);
}

bool Decompiler::detectLoop(const QVector<ControlFlowBlock>& blocks) {
    // Simple loop detection: check if any block jumps backward
    for (const auto& block : blocks) {
        for (const auto& target : block.successors) {
            if (target < block.address) {
                return true;  // Backward jump = loop
            }
        }
    }
    return false;
}

int Decompiler::calculateComplexity(const QVector<ControlFlowBlock>& blocks) {
    // Cyclomatic complexity = edges - nodes + 2
    int edges = 0;
    for (const auto& block : blocks) {
        edges += block.successors.size();
    }
    return std::max(1, edges - blocks.size() + 2);
}

uint64_t Decompiler::resolveOperandValue(const QString& operand) {
    // Try to parse immediate value
    if (operand.startsWith("0x") || operand.startsWith("0X")) {
        bool ok;
        return operand.toULongLong(&ok, 16);
    }
    return 0;
}

bool Decompiler::isStackVariable(int offset, bool& isParameter) {
    // Determine if offset is parameter or local variable
    // Parameters typically have positive offsets (above rbp)
    // Locals typically have negative offsets (below rbp)
    isParameter = (offset > 0);
    return true;
}
