#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace RawrXD::RE {

    struct BinaryAnalysisResult {
        bool success;
        std::string disassembly;
        std::string verificationLog;
        nlohmann::json metadata;
        std::string errorMessage;
    };

    struct REDisasmResult {
        bool success = false;
        std::string assembly;
        std::string error;
    };

    class SovereignREBridge {
    public:
        static SovereignREBridge& Instance();

        // Performs disassembly on a specific range of a binary file.
        BinaryAnalysisResult DisassembleRange(const std::string& binaryPath, uint64_t rvaStart, size_t length);

        // Validates a block of x64 assembly by attempting to assemble it with ml64.
        bool ValidateMasmSyntax(const std::string& asmSource, std::string& compilerOutput);

        // Runs a heuristic scan on a memory block to identify potential logic flaws.
        nlohmann::json RunHeuristicScan(uintptr_t targetBase, size_t range);

        // Batch 4: Advanced Reverse Engineering & Decompilation Bridge
        // Provides higher-level decompilation of identified blocks into pseudocode.
        std::string DecompileBlock(uintptr_t baseAddress, size_t size);
        
        // Identifies function boundaries from a raw byte stream using pattern matching.
        std::vector<uintptr_t> DiscoverFunctions(const uint8_t* buffer, size_t size);

    private:
        SovereignREBridge() = default;
        std::string ExecutePipelineCommand(const std::string& command);
    };

    // AdvancedREBridge — Alias used by ToolRegistry for RE operations.
    class AdvancedREBridge {
    public:
        static AdvancedREBridge& Instance() {
            static AdvancedREBridge s;
            return s;
        }

        REDisasmResult Disassemble(const std::string& filePath, uint64_t rva, size_t size) {
            (void)filePath; (void)rva; (void)size;
            REDisasmResult r;
            r.success = false;
            r.error = "AdvancedREBridge: not connected to disassembler backend";
            return r;
        }

        bool VerifyAssembly(const std::string& asmCode, std::string& error) {
            (void)asmCode;
            error = "AdvancedREBridge: not connected to assembler backend";
            return false;
        }

        nlohmann::json AnalyzeLogic(const std::string& filePath, uint64_t rva) {
            (void)filePath; (void)rva;
            return nlohmann::json{{"status", "not_connected"}};
        }

    private:
        AdvancedREBridge() = default;
    };

} // namespace RawrXD::RE
