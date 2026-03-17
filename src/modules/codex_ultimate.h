#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

struct DisassemblyResult {
    std::string address;
    std::string bytes;
    std::string instruction;
    std::string operands;
    std::string comment;
};

struct PEHeaderInfo {
    std::string machine;
    std::string timestamp;
    std::string entry_point;
    std::vector<std::string> sections;
    std::vector<std::string> imports;
    std::vector<std::string> exports;
};

class CodexUltimate {
public:
    // Disassembler
    static std::vector<DisassemblyResult> Disassemble(const std::string& binary_path, 
                                                     uint64_t start_addr = 0, 
                                                     size_t length = 0x1000);
    
    // Dumpbin equivalent
    static PEHeaderInfo DumpPE(const std::string& pe_file);
    static std::string DumpExports(const std::string& dll_path);
    static std::string DumpImports(const std::string& exe_path);
    static std::string DumpHeaders(const std::string& pe_file);
    
    // Compiler integration
    static bool CompileMASM64(const std::string& asm_file, const std::string& output_obj);
    static bool LinkObject(const std::vector<std::string>& obj_files, const std::string& output_exe);
    
    // Agentic analysis
    static std::string AnalyzeBinary(const std::string& binary_path);
    static std::string FindVulnerabilities(const std::string& binary_path);
    static std::string GenerateExploit(const std::string& vulnerability);
    
    // Helper functions
    static std::string GetInstructionInfo(const std::string& instruction);
    static std::string GetRegisterState(const std::string& context);
};

// Global instance
extern std::unique_ptr<CodexUltimate> g_codex;
