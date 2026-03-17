#pragma once
// ============================================================================
// RawrXD Decompiler - Pattern-Based Decompilation Engine
// Converts disassembled x86/x64 instructions to C/C++ pseudocode
// Pure Win32, No External Dependencies
// ============================================================================

#ifndef RAWRXD_DECOMPILER_H
#define RAWRXD_DECOMPILER_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include "disassembler.h"

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// Decompilation Structures
// ============================================================================

enum class VariableType {
    Unknown,
    Int8, Int16, Int32, Int64,
    UInt8, UInt16, UInt32, UInt64,
    Float32, Float64,
    Pointer,
    String,
    Struct,
    Array,
    Bool,
    Void
};

struct Variable {
    std::string     name;           // Generated name (e.g., "var_8", "arg_0")
    VariableType    type;           // Inferred type
    int64_t         stackOffset;    // Offset from RBP (negative = local, positive = arg)
    RegisterId      reg;            // Register if not stack-based
    bool            isParameter;    // True if function parameter
    bool            isLocal;        // True if local variable
    uint32_t        size;           // Size in bytes
    std::string     comment;        // Type inference reasoning
};

struct DecompiledStatement {
    std::string     code;           // C/C++ code
    uint64_t        address;        // Original instruction address
    std::string     comment;        // Additional info
    int             indentLevel;    // Indentation level
    
    DecompiledStatement() : address(0), indentLevel(0) {}
    DecompiledStatement(const std::string& c, uint64_t addr = 0, int indent = 0)
        : code(c), address(addr), indentLevel(indent) {}
};

struct DecompiledFunction {
    std::string                     name;           // Function name
    std::string                     returnType;     // Inferred return type
    std::vector<Variable>           parameters;     // Function parameters
    std::vector<Variable>           locals;         // Local variables
    std::vector<DecompiledStatement> body;          // Function body statements
    std::string                     signature;      // Full signature
    uint64_t                        startAddress;
    uint64_t                        endAddress;
};

// ============================================================================
// Pattern Definitions
// ============================================================================

struct InstructionPattern {
    std::string     name;           // Pattern name
    std::string     description;    // Human-readable description
    std::vector<InstructionType> sequence;  // Expected instruction sequence
    std::string     codeTemplate;   // C code template
};

// ============================================================================
// Decompiler Class
// ============================================================================

class Decompiler {
public:
    Decompiler();
    ~Decompiler() = default;

    // ========================================================================
    // Configuration
    // ========================================================================
    void SetOutputLanguage(const std::string& lang);  // "c", "cpp", "pseudo"
    std::string GetOutputLanguage() const { return m_language; }
    
    void SetIndentString(const std::string& indent) { m_indentString = indent; }
    void SetShowAddresses(bool show) { m_showAddresses = show; }
    void SetShowComments(bool show) { m_showComments = show; }
    void SetVerbose(bool verbose) { m_verbose = verbose; }

    // ========================================================================
    // Core Decompilation
    // ========================================================================
    std::string DecompileFunction(const std::vector<Instruction>& instructions);
    DecompiledFunction DecompileFunctionEx(const std::vector<Instruction>& instructions);
    std::string DecompileBasicBlock(const BasicBlock& block);
    
    // ========================================================================
    // Pattern Recognition
    // ========================================================================
    std::string RecognizePattern(const std::vector<Instruction>& instructions);
    std::string RecognizeIdiom(const Instruction& inst1, const Instruction& inst2);
    
    // ========================================================================
    // Type Inference
    // ========================================================================
    std::string InferVariableTypes(const std::vector<Instruction>& instructions);
    VariableType InferTypeFromUsage(const Instruction& inst, int operandIndex);
    VariableType InferTypeFromSize(uint8_t size, bool isSigned = true);
    
    // ========================================================================
    // Expression Generation
    // ========================================================================
    std::string GenerateExpression(const Instruction& inst);
    std::string GenerateAssignment(const Instruction& inst);
    std::string GenerateCall(const Instruction& inst, const std::vector<Instruction>& context);
    std::string GenerateCondition(const Instruction& cmpInst, InstructionType jccType);
    
    // ========================================================================
    // Control Flow Reconstruction
    // ========================================================================
    std::vector<DecompiledStatement> ReconstructControlFlow(
        const std::vector<BasicBlock>& blocks);
    std::vector<DecompiledStatement> ReconstructLoop(
        const std::vector<BasicBlock>& blocks, size_t loopHeaderIdx);
    std::vector<DecompiledStatement> ReconstructIfElse(
        const std::vector<BasicBlock>& blocks, size_t branchIdx);
    
    // ========================================================================
    // Output Formatting
    // ========================================================================
    std::string FormatDecompiledFunction(const DecompiledFunction& func);
    std::string FormatStatements(const std::vector<DecompiledStatement>& statements);
    std::string GetTypeString(VariableType type);

private:
    // Internal helpers
    void AnalyzePrologue(const std::vector<Instruction>& instructions,
                         DecompiledFunction& func);
    void AnalyzeEpilogue(const std::vector<Instruction>& instructions,
                         DecompiledFunction& func);
    void AnalyzeStackFrame(const std::vector<Instruction>& instructions,
                           DecompiledFunction& func);
    void InferParameters(const std::vector<Instruction>& instructions,
                         DecompiledFunction& func);
    void InferLocals(const std::vector<Instruction>& instructions,
                     DecompiledFunction& func);
    
    std::string OperandToExpression(const Operand& op, uint8_t size = 0);
    std::string RegisterToVariable(RegisterId reg);
    std::string MemoryToVariable(const Operand& op);
    std::string ImmediateToString(int64_t value, uint8_t size);
    
    std::string InvertCondition(const std::string& condition);
    std::string JccToCondition(InstructionType jccType);
    
    void InitializePatterns();
    
    // Configuration
    std::string m_language;         // Output language
    std::string m_indentString;     // Indentation string
    bool        m_showAddresses;    // Show original addresses
    bool        m_showComments;     // Show comments
    bool        m_verbose;          // Verbose output
    
    // Pattern database
    std::vector<InstructionPattern> m_patterns;
    
    // Register tracking
    std::unordered_map<RegisterId, std::string> m_registerValues;
    
    // Variable naming counters
    uint32_t m_varCounter;
    uint32_t m_argCounter;
    uint32_t m_labelCounter;
};

} // namespace ReverseEngineering
} // namespace RawrXD

#endif // RAWRXD_DECOMPILER_H
