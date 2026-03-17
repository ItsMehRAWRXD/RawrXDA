/**
 * \file disassembler.h
 * \brief Disassembler using Capstone engine
 * \author RawrXD Team
 * \date 2026-01-22
 */

#pragma once

#include "binary_loader.h"
#include <QString>
#include <QVector>
#include <QMap>
#include <capstone/capstone.h>

namespace RawrXD {
namespace ReverseEngineering {

/**
 * \struct Instruction
 * \brief A single disassembled instruction
 */
struct Instruction {
    uint64_t address;           ///< Address/RVA
    QByteArray bytes;           ///< Raw bytes
    QString mnemonic;           ///< Instruction mnemonic (e.g., "mov")
    QString operands;           ///< Operands (e.g., "rax, rbx")
    QString disassembly;        ///< Full disassembly (mnemonic + operands)
    QString comment;            ///< Optional user comment
    bool isBranch;              ///< True if branch instruction
    bool isCall;                ///< True if call instruction
    bool isReturn;              ///< True if return instruction
    uint64_t target;            ///< Branch/call target (if branch/call)
};

/**
 * \struct FunctionInfo
 * \brief Information about a disassembled function
 */
struct FunctionInfo {
    QString name;               ///< Function name
    uint64_t address;           ///< Start address
    uint64_t size;              ///< Function size in bytes
    QVector<Instruction> instructions;  ///< All instructions in function
    bool isRecognized;          ///< True if recognized/exported function
};

/**
 * \class Disassembler
 * \brief Disassembles binary code using Capstone
 */
class Disassembler {
public:
    /**
     * \brief Construct disassembler from binary loader
     * \param loader Loaded binary
     */
    explicit Disassembler(const BinaryLoader& loader);
    
    /**
     * \brief Destructor - cleanup Capstone handle
     */
    ~Disassembler();
    
    /**
     * \brief Disassemble code at RVA
     * \param rva Relative virtual address
     * \param size Number of bytes to disassemble
     * \param funcName Optional function name
     * \return Function with disassembled instructions
     */
    FunctionInfo disassemble(uint64_t rva, uint64_t size, const QString& funcName = "");
    
    /**
     * \brief Get all disassembled functions
     * \return Vector of function info
     */
    const QVector<FunctionInfo>& functions() const { return m_functions; }
    
    /**
     * \brief Find function by address
     * \param address Address/RVA
     * \return Function info, or empty if not found
     */
    FunctionInfo functionAt(uint64_t address) const;
    
    /**
     * \brief Find function by name
     * \param name Function name
     * \return Function info, or empty if not found
     */
    FunctionInfo functionNamed(const QString& name) const;
    
    /**
     * \brief Get instruction at address
     * \param address Address/RVA
     * \return Instruction at address
     */
    Instruction instructionAt(uint64_t address) const;
    
    /**
     * \brief Analyze function boundaries from code section
     * \param sectionName Code section name (e.g., ".text")
     * \return Number of functions found
     */
    int analyzeCodeSection(const QString& sectionName = ".text");
    
    /**
     * \brief Get cross-references to address
     * \param target Target address
     * \return Vector of instructions that reference target
     */
    QVector<Instruction> crossReferences(uint64_t target) const;

private:
    const BinaryLoader& m_loader;
    csh m_handle;  // Capstone handle
    QVector<FunctionInfo> m_functions;
    QMap<uint64_t, FunctionInfo> m_functionsByAddress;
    QMap<QString, FunctionInfo> m_functionsByName;
    QVector<Instruction> m_allInstructions;
    
    bool initCapstone();
    void detectFunctionBoundaries(uint64_t codeStart, uint64_t codeSize);
    bool isFunctionPrologue(const Instruction& instr);
    bool isFunctionEpilogue(const Instruction& instr);
    uint64_t resolveJumpTarget(const Instruction& instr);
};

} // namespace ReverseEngineering
} // namespace RawrXD
