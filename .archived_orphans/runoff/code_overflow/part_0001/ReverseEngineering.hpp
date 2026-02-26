#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace RawrXD {
namespace ReverseEngineering {

// Custom Disassembler - No external dependencies
class NativeDisassembler {
public:
    struct Instruction {
        uint64_t address;
        std::vector<uint8_t> bytes;
        std::string mnemonic;
        std::string operands;
        std::string comment;
        bool isJump;
        bool isCall;
        uint64_t jumpTarget;
    };

    struct Function {
        uint64_t startAddress;
        uint64_t endAddress;
        uint64_t entryPoint;    // Compatibility: same as startAddress
        size_t   size;         // Compatibility: endAddress - startAddress (set by analyzer)
        std::string name;
        std::vector<Instruction> instructions;
        std::vector<uint64_t> callers;
        std::vector<uint64_t> callees;
    };

    // Disassemble x64 code
    static std::vector<Instruction> DisassembleX64(const uint8_t* data, size_t size, uint64_t baseAddress = 0);
    
    // Analyze control flow
    static std::vector<Function> AnalyzeFunctions(const std::vector<Instruction>& instructions);
    
    // Pattern matching for common structures
    static std::vector<std::string> FindPatterns(const uint8_t* data, size_t size);
    
    // String extraction
    static std::vector<std::string> ExtractStrings(const uint8_t* data, size_t size);
    
    // Import/Export analysis
    static std::unordered_map<std::string, uint64_t> AnalyzeImports(const std::string& filePath);
    static std::unordered_map<std::string, uint64_t> AnalyzeExports(const std::string& filePath);

private:
    static void DecodeX64Instruction(const uint8_t* bytes, size_t& length, Instruction& inst);
};

// Custom Binary Analyzer (like dumpbin but better)
class BinaryAnalyzer {
public:
    struct Section {
        std::string name;
        uint64_t virtualAddress;
        uint32_t virtualSize;
        uint32_t rawSize;
        uint32_t characteristics;
        std::string permissions; // "RWX"
    };

    struct ImportFunction {
        std::string dllName;
        std::string functionName;
        uint64_t address;
        bool isOrdinal;
        uint16_t ordinal;
    };

    struct ExportFunction {
        std::string name;
        uint64_t address;
        uint16_t ordinal;
        bool isForwarder;
        std::string forwarder;
    };

    struct BinaryInfo {
        std::string architecture; // "x64", "x86", "ARM64"
        std::string subsystem;
        uint64_t entryPoint;
        uint64_t imageBase;
        uint32_t fileSize;
        std::vector<Section> sections;
        std::vector<ImportFunction> imports;
        std::vector<ExportFunction> exports;
        std::unordered_map<std::string, std::string> metadata;
    };

    static BinaryInfo AnalyzePE(const std::string& filePath);
    static std::string GenerateReport(const BinaryInfo& info);
    static std::vector<uint8_t> ExtractSection(const std::string& filePath, const std::string& sectionName);
};

// Reverse Engineering Codex/Knowledge Base
class RECodex {
public:
    struct Pattern {
        std::string name;
        std::vector<uint8_t> signature;
        std::vector<uint8_t> mask;
        std::string description;
        std::function<void(uint64_t)> analyzer;
    };

    struct KnownFunction {
        std::string name;
        std::string library;
        std::vector<std::string> parameters;
        std::string description;
        std::vector<Pattern> variants;
    };

    // Built-in pattern database
    static std::vector<Pattern> GetCommonPatterns();
    static std::vector<KnownFunction> GetWindowsAPIDatabase();
    static std::vector<Pattern> GetMalwarePatterns();
    static std::vector<Pattern> GetCompilerPatterns();
    
    // Pattern matching
    static std::vector<std::pair<uint64_t, std::string>> ScanForPatterns(
        const uint8_t* data, size_t size, const std::vector<Pattern>& patterns);
        
    // AI-powered analysis
    static std::string AnalyzeWithAI(const std::string& assembly, const std::string& context = "");
};

// Custom Compiler Interface
class NativeCompiler {
public:
    struct CompileOptions {
        std::string targetArch = "x64";
        std::string optimization = "O2";
        bool generateDebugInfo = false;
        bool enableIntrinsics = true;
        std::vector<std::string> includePaths;
        std::vector<std::string> libraryPaths;
        std::vector<std::string> defines;
    };

    struct CompileResult {
        bool success;
        std::vector<uint8_t> machineCode;
        std::string assembly;
        std::string errorLog;
        std::unordered_map<std::string, uint64_t> symbols;
    };

    // Compile C++ to machine code (JIT-style)
    static CompileResult CompileToNative(const std::string& sourceCode, const CompileOptions& options = {});
    
    // Inline assembly compilation
    static CompileResult CompileAssembly(const std::string& assembly, const CompileOptions& options = {});
    
    // Patch existing binary
    static bool PatchBinary(const std::string& filePath, uint64_t offset, const std::vector<uint8_t>& newCode);
    
    // Generate shellcode
    static std::vector<uint8_t> GenerateShellcode(const std::string& payload, const std::string& arch = "x64");
};

} // namespace ReverseEngineering
} // namespace RawrXD