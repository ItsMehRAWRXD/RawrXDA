#include "AdvancedREBridge.h"
#include <windows.h>
#include <cstdio>
#include <memory>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace RawrXD::RE {

SovereignREBridge& SovereignREBridge::Instance() {
    static SovereignREBridge instance;
    return instance;
}

std::string SovereignREBridge::ExecutePipelineCommand(const std::string& command) {
    std::string result;
    std::shared_ptr<FILE> pipe(_popen(command.c_str(), "r"), _pclose);
    if (!pipe) return "Command execution failed (pipe error).";
    
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
    return result;
}

BinaryAnalysisResult SovereignREBridge::DisassembleRange(const std::string& binaryPath, uint64_t rvaStart, size_t length) {
    BinaryAnalysisResult res{};
    
    // Construct dumpbin command - range format is usually hex
    std::stringstream ss;
    ss << "dumpbin /DISASM /RANGE:0x" << std::hex << rvaStart 
       << ",0x" << std::hex << (rvaStart + length) << " " << binaryPath;
    
    std::string output = ExecutePipelineCommand(ss.str());
    
    if (output.empty() || output.find("fatal error") != std::string::npos) {
        res.success = false;
        res.errorMessage = "Dumpbin tool failed or file inaccessible.";
        return res;
    }

    res.disassembly = output;
    res.success = true;
    res.metadata["tool"] = "dumpbin";
    res.metadata["rva"] = rvaStart;
    return res;
}

bool SovereignREBridge::ValidateMasmSyntax(const std::string& asmSource, std::string& compilerOutput) {
    std::string tempPath = "D:/rawrxd/temp_verify_unit.asm";
    std::string objPath = "D:/rawrxd/temp_verify_unit.obj";
    
    std::ofstream out(tempPath);
    out << ".code\n" << asmSource << "\nend\n";
    out.close();

    // MASM path from environment config
    std::string ml64 = "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\ml64.exe";
    std::string cmd = "\"" + ml64 + "\" /c /Fo \"" + objPath + "\" \"" + tempPath + "\" 2>&1";
    
    compilerOutput = ExecutePipelineCommand(cmd);
    
    bool success = fs::exists(objPath);
    
    // Clean up
    if (fs::exists(tempPath)) fs::remove(tempPath);
    if (fs::exists(objPath)) fs::remove(objPath);
    
    return success;
}

nlohmann::json SovereignREBridge::RunHeuristicScan(uintptr_t targetBase, size_t range) {
    nlohmann::json result;
    result["scanner"] = "Sovereign_V2_Heuristics";
    result["target_address"] = targetBase;
    
    // Real heuristic scan: analyze memory segment for anomalies
    if (targetBase == 0 || range == 0) {
        result["status"] = "Invalid parameters: base address or range is zero";
        result["entropy_score"] = 0.0;
        result["anomalies"] = nlohmann::json::array();
        return result;
    }
    
    // Calculate Shannon entropy of the memory range
    const uint8_t* data = reinterpret_cast<const uint8_t*>(targetBase);
    size_t freq[256] = {};
    size_t validBytes = 0;
    
    for (size_t i = 0; i < range; ++i) {
        try {
            uint8_t byte = data[i];
            freq[byte]++;
            validBytes++;
        } catch (...) {
            // Memory access violation - stop scanning
            break;
        }
    }
    
    if (validBytes == 0) {
        result["status"] = "Memory access denied - could not read target range";
        result["entropy_score"] = 0.0;
        result["anomalies"] = nlohmann::json::array();
        return result;
    }
    
    double entropy = 0.0;
    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) {
            double p = static_cast<double>(freq[i]) / validBytes;
            entropy -= p * std::log2(p);
        }
    }
    
    // Normalize entropy to 0-1 range (max entropy for bytes is 8)
    double normalizedEntropy = entropy / 8.0;
    
    // Detect anomalies: high entropy = encrypted/packed, low entropy = nulls/code
    nlohmann::json anomalies = nlohmann::json::array();
    
    if (normalizedEntropy > 0.95) {
        anomalies.push_back({{"type", "high_entropy"}, {"description", "Possible encrypted or packed data"}, {"severity", "medium"}});
    } else if (normalizedEntropy < 0.1) {
        anomalies.push_back({{"type", "low_entropy"}, {"description", "Large block of uniform data (nulls or padding)"}, {"severity", "low"}});
    }
    
    // Check for common shellcode signatures
    if (validBytes >= 4) {
        for (size_t i = 0; i < validBytes - 4; ++i) {
            // Check for NOP sled (0x90)
            if (data[i] == 0x90 && data[i+1] == 0x90 && data[i+2] == 0x90) {
                anomalies.push_back({{"type", "nop_sled"}, {"offset", i}, {"description", "NOP sled detected - possible shellcode"}, {"severity", "high"}});
                break;
            }
        }
    }
    
    result["status"] = "Heuristic scan complete. Analyzed " + std::to_string(validBytes) + " bytes.";
    result["entropy_score"] = normalizedEntropy;
    result["anomalies"] = anomalies;
    result["bytes_scanned"] = validBytes;
    
    return result;
}

std::string SovereignREBridge::DecompileBlock(uintptr_t baseAddress, size_t size) {
    // Advanced Batch 4: Decompilation Bridge
    // This provides a starting point for translating x64 MASM back to high-level logic.
    // In production, this would call an LLM-assisted decompiler or a tool like RetDec/Ghidra headless.
    
    std::string result = "// Decompiled segment starting at 0x" + std::to_string(baseAddress) + "\n";
    result += "// Identifying control flow and potential variable assignments...\n";
    
    // Heuristic: identify common C/C++ function prologue/epilogue sequences
    const uint8_t* p = reinterpret_cast<const uint8_t*>(baseAddress);
    if (size >= 8 && p[0] == 0x48 && p[1] == 0x89 && p[2] == 0x5C && p[3] == 0x24) {
        result += "void identified_function() {\n";
        result += "    // [!] MASM optimized sequence found.\n";
        result += "    // Sequence matches: mov [rsp+...], rbx\n";
    }

    result += "    // ... Analysis in progress by Sovereign Agentic Loop.\n";
    result += "}\n";
    
    return result;
}

std::vector<uintptr_t> SovereignREBridge::DiscoverFunctions(const uint8_t* buffer, size_t size) {
    std::vector<uintptr_t> functionStarts;
    
    // Pattern match for: mov rax, rsp (found in many prologues)
    // or sub rsp, XX, or push rbp; mov rbp, rsp
    for (size_t i = 0; i < size - 5; ++i) {
        // Look for common 48 83 EC (sub rsp, ...)
        if (buffer[i] == 0x48 && buffer[i+1] == 0x83 && buffer[i+2] == 0xEC) {
            functionStarts.push_back(reinterpret_cast<uintptr_t>(buffer + i));
            // Skip a few bytes to avoid double counting same function
            i += 10; 
        }
    }
    
    return functionStarts;
}

} // namespace RawrXD::RE
