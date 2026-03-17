/**
 * \file decompiler.h
 * \brief Decompiler for generating pseudo-C/C++ code from assembly
 * \author RawrXD Team
 * \date 2026-01-22
 */

#pragma once

#include "disassembler.h"
#include <QString>
#include <QVector>
#include <QMap>

namespace RawrXD {
namespace ReverseEngineering {

/**
 * \struct Variable
 * \brief Variable information
 */
struct Variable {
    QString name;               ///< Variable name
    QString type;               ///< Type (int, pointer, struct, etc.)
    uint64_t address;           ///< Memory/register address
    int stackOffset;            ///< Stack offset (if local)
    bool isParameter;           ///< True if function parameter
    bool isReturn;              ///< True if return value
};

/**
 * \struct ControlFlowBlock
 * \brief Basic block in control flow graph
 */
struct ControlFlowBlock {
    uint64_t address;           ///< Block start address
    uint64_t size;              ///< Block size
    QVector<Instruction> instructions;
    QString pseudoCode;         ///< Generated pseudo-code
    QVector<uint64_t> successors;  ///< Successor block addresses
    QVector<uint64_t> predecessors; ///< Predecessor block addresses
};

/**
 * \struct DecompiledFunction
 * \brief Decompiled function with control flow and pseudo-code
 */
struct DecompiledFunction {
    QString name;               ///< Function name
    uint64_t address;           ///< Entry point RVA
    QString signature;          ///< Function signature (inferred)
    QString pseudoCode;         ///< Full pseudo-code
    QVector<Variable> variables; ///< Local variables
    QVector<Variable> parameters; ///< Parameters
    QVector<ControlFlowBlock> basicBlocks;  ///< Control flow blocks
    bool hasLoops;              ///< True if function contains loops
    int complexity;             ///< Cyclomatic complexity
};

/**
 * \class Decompiler
 * \brief Decompiles binary functions to pseudo-code
 */
class Decompiler {
public:
    /**
     * \brief Construct decompiler from disassembler
     * \param disasm Disassembler instance
     */
    explicit Decompiler(const Disassembler& disasm);
    
    /**
     * \brief Decompile a function
     * \param funcInfo Function to decompile
     * \return Decompiled function with pseudo-code
     */
    DecompiledFunction decompile(const FunctionInfo& funcInfo);
    
    /**
     * \brief Decompile function at address
     * \param address Function entry point RVA
     * \param name Optional function name
     * \return Decompiled function
     */
    DecompiledFunction decompileAt(uint64_t address, const QString& name = "");
    
    /**
     * \brief Get all decompiled functions
     * \return Vector of decompiled functions
     */
    const QVector<DecompiledFunction>& functions() const { return m_functions; }
    
    /**
     * \brief Build control flow graph for function
     * \param funcInfo Function to analyze
     * \return Vector of basic blocks
     */
    QVector<ControlFlowBlock> buildCFG(const FunctionInfo& funcInfo);
    
    /**
     * \brief Extract variables from function
     * \param funcInfo Function to analyze
     * \return Vector of detected variables
     */
    QVector<Variable> extractVariables(const FunctionInfo& funcInfo);
    
    /**
     * \brief Infer function signature from assembly
     * \param funcInfo Function to analyze
     * \return Inferred signature string
     */
    QString inferSignature(const FunctionInfo& funcInfo);

private:
    const Disassembler& m_disasm;
    QVector<DecompiledFunction> m_functions;
    
    // Helper methods
    QString instructionToPseudoCode(const Instruction& instr);
    QString simplifyAssembly(const Instruction& instr);
    bool detectLoop(const QVector<ControlFlowBlock>& blocks);
    int calculateComplexity(const QVector<ControlFlowBlock>& blocks);
    uint64_t resolveOperandValue(const QString& operand);
    bool isStackVariable(int offset, bool& isParameter);
};

} // namespace ReverseEngineering
} // namespace RawrXD
