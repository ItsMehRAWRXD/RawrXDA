#pragma once

#include "RawrCodex.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// RAWR DUMPBIN - Advanced Binary Dumping & Analysis Tool
// Custom implementation similar to Microsoft's dumpbin but with AI features
// ============================================================================

class RawrDumpBin {
public:
    RawrDumpBin() : m_verbose(false), m_showHex(true) {}

    void SetVerbose(bool verbose) { m_verbose = verbose; }
    void SetShowHex(bool show) { m_showHex = show; }

    // ========================================================================
    // Main Dump Functions
    // ========================================================================

    std::string DumpHeaders(const std::string& filePath) {
        RawrCodex codex;
        if (!codex.LoadBinary(filePath)) {
            return "Error: Failed to load binary\n";
        }

        std::ostringstream oss;
        oss << "=".repeat(80) << "\n";
        oss << "RAWR DUMPBIN - Binary Header Analysis\n";
        oss << "File: " << filePath << "\n";
        oss << "=".repeat(80) << "\n\n";

        oss << "Architecture: " << (std::filesystem::file_size(filePath) > 0 ? "PE/COFF" : "Unknown") << "\n";
        oss << "Machine Type: x64\n";
        oss << "Sections:\n";

        auto sections = codex.GetSections();
        for (const auto& section : sections) {
            oss << "  " << std::left << std::setw(12) << section.name;
            oss << " VA: 0x" << std::hex << std::setw(8) << std::setfill('0') 
                << section.virtualAddress;
            oss << " Size: 0x" << std::hex << section.virtualSize;
            oss << " Characteristics: 0x" << std::hex << section.characteristics << "\n";
        }

        return oss.str();
    }

    std::string DumpImports(const std::string& filePath) {
        RawrCodex codex;
        if (!codex.LoadBinary(filePath)) {
            return "Error: Failed to load binary\n";
        }

        std::ostringstream oss;
        oss << "=".repeat(80) << "\n";
        oss << "IMPORT TABLE\n";
        oss << "=".repeat(80) << "\n\n";

        auto imports = codex.GetImports();
        std::string currentModule;

        for (const auto& imp : imports) {
            if (imp.moduleName != currentModule) {
                currentModule = imp.moduleName;
                oss << "\n" << currentModule << ":\n";
            }
            oss << "  0x" << std::hex << std::setw(16) << std::setfill('0') 
                << imp.address << " ";
            oss << imp.functionName;
            if (imp.ordinal != 0) {
                oss << " (Ordinal: " << std::dec << imp.ordinal << ")";
            }
            oss << "\n";
        }

        return oss.str();
    }

    std::string DumpExports(const std::string& filePath) {
        RawrCodex codex;
        if (!codex.LoadBinary(filePath)) {
            return "Error: Failed to load binary\n";
        }

        std::ostringstream oss;
        oss << "=".repeat(80) << "\n";
        oss << "EXPORT TABLE\n";
        oss << "=".repeat(80) << "\n\n";

        auto exports = codex.GetExports();
        for (const auto& exp : exports) {
            oss << "  0x" << std::hex << std::setw(16) << std::setfill('0') 
                << exp.address << " ";
            oss << std::left << std::setw(40) << exp.name;
            oss << " Ordinal: " << std::dec << exp.ordinal;
            if (exp.isForwarder) {
                oss << " -> " << exp.forwarderName;
            }
            oss << "\n";
        }

        return oss.str();
    }

    std::string DumpDisassembly(const std::string& filePath, uint64_t startAddr, size_t count) {
        RawrCodex codex;
        if (!codex.LoadBinary(filePath)) {
            return "Error: Failed to load binary\n";
        }

        std::ostringstream oss;
        oss << "=".repeat(80) << "\n";
        oss << "DISASSEMBLY at 0x" << std::hex << startAddr << "\n";
        oss << "=".repeat(80) << "\n\n";

        auto disasm = codex.Disassemble(startAddr, count);
        for (const auto& line : disasm) {
            oss << "0x" << std::hex << std::setw(16) << std::setfill('0') 
                << line.address << "  ";
            
            if (m_showHex) {
                for (size_t i = 0; i < 8; ++i) {
                    if (i < line.bytes.size()) {
                        oss << std::hex << std::setw(2) << std::setfill('0') 
                            << static_cast<int>(line.bytes[i]) << " ";
                    } else {
                        oss << "   ";
                    }
                }
                oss << " ";
            }
            
            oss << std::left << std::setw(8) << line.mnemonic;
            oss << line.operands;
            
            if (!line.comment.empty()) {
                oss << "  ; " << line.comment;
            }
            
            oss << "\n";
        }

        return oss.str();
    }

    std::string DumpStrings(const std::string& filePath, size_t minLength = 4) {
        RawrCodex codex;
        if (!codex.LoadBinary(filePath)) {
            return "Error: Failed to load binary\n";
        }

        std::ostringstream oss;
        oss << "=".repeat(80) << "\n";
        oss << "STRING TABLE (min length: " << minLength << ")\n";
        oss << "=".repeat(80) << "\n\n";

        auto strings = codex.ExtractStrings(minLength);
        for (size_t i = 0; i < strings.size(); ++i) {
            oss << std::dec << std::setw(6) << i << ": " << strings[i] << "\n";
        }
        
        oss << "\nTotal: " << strings.size() << " strings\n";

        return oss.str();
    }

    std::string DumpVulnerabilities(const std::string& filePath) {
        RawrCodex codex;
        if (!codex.LoadBinary(filePath)) {
            return "Error: Failed to load binary\n";
        }

        std::ostringstream oss;
        oss << "=".repeat(80) << "\n";
        oss << "SECURITY ANALYSIS\n";
        oss << "=".repeat(80) << "\n\n";

        auto vulns = codex.DetectVulnerabilities();
        
        if (vulns.empty()) {
            oss << "✓ No obvious vulnerabilities detected\n";
        } else {
            for (const auto& vuln : vulns) {
                oss << "[" << vuln.severity << "] " << vuln.type << "\n";
                oss << "  Address: 0x" << std::hex << vuln.address << "\n";
                oss << "  Description: " << vuln.description << "\n\n";
            }
        }

        return oss.str();
    }

    std::string DumpAll(const std::string& filePath) {
        std::ostringstream oss;
        
        oss << DumpHeaders(filePath) << "\n\n";
        oss << DumpImports(filePath) << "\n\n";
        oss << DumpExports(filePath) << "\n\n";
        oss << DumpStrings(filePath) << "\n\n";
        oss << DumpVulnerabilities(filePath) << "\n";
        
        return oss.str();
    }

    // ========================================================================
    // Comparison Functions
    // ========================================================================

    std::string CompareBinaries(const std::string& file1, const std::string& file2) {
        RawrCodex codex1, codex2;
        
        if (!codex1.LoadBinary(file1) || !codex2.LoadBinary(file2)) {
            return "Error: Failed to load binaries\n";
        }

        std::ostringstream oss;
        oss << "=".repeat(80) << "\n";
        oss << "BINARY COMPARISON\n";
        oss << "=".repeat(80) << "\n\n";
        oss << "File 1: " << file1 << "\n";
        oss << "File 2: " << file2 << "\n\n";

        // Compare sections
        auto sections1 = codex1.GetSections();
        auto sections2 = codex2.GetSections();
        
        oss << "Section Comparison:\n";
        oss << "  File 1: " << sections1.size() << " sections\n";
        oss << "  File 2: " << sections2.size() << " sections\n\n";

        // Compare imports
        auto imports1 = codex1.GetImports();
        auto imports2 = codex2.GetImports();
        
        oss << "Import Comparison:\n";
        oss << "  File 1: " << imports1.size() << " imports\n";
        oss << "  File 2: " << imports2.size() << " imports\n\n";

        // Find differences
        std::vector<std::string> onlyInFile1, onlyInFile2;
        
        for (const auto& imp1 : imports1) {
            bool found = false;
            for (const auto& imp2 : imports2) {
                if (imp1.functionName == imp2.functionName && 
                    imp1.moduleName == imp2.moduleName) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                onlyInFile1.push_back(imp1.moduleName + "!" + imp1.functionName);
            }
        }
        
        for (const auto& imp2 : imports2) {
            bool found = false;
            for (const auto& imp1 : imports1) {
                if (imp1.functionName == imp2.functionName && 
                    imp1.moduleName == imp2.moduleName) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                onlyInFile2.push_back(imp2.moduleName + "!" + imp2.functionName);
            }
        }

        if (!onlyInFile1.empty()) {
            oss << "Only in File 1:\n";
            for (const auto& imp : onlyInFile1) {
                oss << "  - " << imp << "\n";
            }
        }
        
        if (!onlyInFile2.empty()) {
            oss << "Only in File 2:\n";
            for (const auto& imp : onlyInFile2) {
                oss << "  + " << imp << "\n";
            }
        }

        return oss.str();
    }

private:
    bool m_verbose;
    bool m_showHex;
};

// Helper for string repeat (C++20 doesn't have this built-in)
namespace {
    struct RepeatHelper {
        size_t count;
        RepeatHelper(size_t n) : count(n) {}
    };
    
    RepeatHelper repeat(size_t n) { return RepeatHelper(n); }
    
    std::ostream& operator<<(std::ostream& os, RepeatHelper rh) {
        for (size_t i = 0; i < rh.count; ++i) os << "=";
        return os;
    }
}

} // namespace ReverseEngineering
} // namespace RawrXD
