#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// RAWR CODEX - Advanced Binary Analysis & Reverse Engineering Suite
// ============================================================================

struct Symbol {
    std::string name;
    uint64_t address;
    uint64_t size;
    std::string section;
    std::string type; // "function", "data", "import", "export"
    bool isMangled;
    std::string demangledName;
};

struct Section {
    std::string name;
    uint64_t virtualAddress;
    uint64_t virtualSize;
    uint64_t rawSize;
    uint64_t rawOffset;
    uint32_t characteristics;
    std::vector<uint8_t> data;
};

struct Import {
    std::string moduleName;
    std::string functionName;
    uint64_t address;
    uint16_t ordinal;
};

struct Export {
    std::string name;
    uint64_t address;
    uint16_t ordinal;
    bool isForwarder;
    std::string forwarderName;
};

struct DisassemblyLine {
    uint64_t address;
    std::vector<uint8_t> bytes;
    std::string mnemonic;
    std::string operands;
    std::string comment;
};

class RawrCodex {
public:
    RawrCodex() : m_architecture("x64"), m_bitness(64) {}

    // ========================================================================
    // Core Analysis Functions
    // ========================================================================

    bool LoadBinary(const std::string& path) {
        m_filePath = path;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return false;

        m_fileSize = file.tellg();
        m_binaryData.resize(m_fileSize);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(m_binaryData.data()), m_fileSize);
        file.close();

        // Detect format
        if (m_fileSize < 2) return false;
        
        if (m_binaryData[0] == 0x4D && m_binaryData[1] == 0x5A) {
            // PE/COFF format
            return ParsePE();
        } else if (m_fileSize > 4 && 
                   m_binaryData[0] == 0x7F && m_binaryData[1] == 0x45 && 
                   m_binaryData[2] == 0x4C && m_binaryData[3] == 0x46) {
            // ELF format
            return ParseELF();
        }
        
        // Unknown format - treat as raw binary
        m_architecture = "unknown";
        return true;
    }

    std::vector<Symbol> GetSymbols() const { return m_symbols; }
    std::vector<Section> GetSections() const { return m_sections; }
    std::vector<Import> GetImports() const { return m_imports; }
    std::vector<Export> GetExports() const { return m_exports; }

    // ========================================================================
    // Advanced Disassembly (x64 Intel)
    // ========================================================================

    std::vector<DisassemblyLine> Disassemble(uint64_t startAddr, size_t count = 100) {
        std::vector<DisassemblyLine> result;
        
        // Find section containing this address
        const Section* section = FindSectionByVA(startAddr);
        if (!section) return result;

        uint64_t offset = startAddr - section->virtualAddress;
        if (offset >= section->data.size()) return result;

        const uint8_t* ptr = section->data.data() + offset;
        const uint8_t* end = section->data.data() + section->data.size();
        uint64_t currentAddr = startAddr;

        for (size_t i = 0; i < count && ptr < end; ++i) {
            DisassemblyLine line;
            line.address = currentAddr;

            // Simple x64 instruction decoding (simplified)
            size_t instrLen = DecodeInstruction(ptr, end - ptr, line);
            if (instrLen == 0) break;

            result.push_back(line);
            ptr += instrLen;
            currentAddr += instrLen;
        }

        return result;
    }

    // ========================================================================
    // String Extraction
    // ========================================================================

    std::vector<std::string> ExtractStrings(size_t minLength = 4) {
        std::vector<std::string> strings;
        std::string current;

        for (size_t i = 0; i < m_binaryData.size(); ++i) {
            uint8_t byte = m_binaryData[i];
            
            if (byte >= 0x20 && byte <= 0x7E) {
                current += static_cast<char>(byte);
            } else if (byte == 0 && current.length() >= minLength) {
                strings.push_back(current);
                current.clear();
            } else if (byte == 0) {
                current.clear();
            }
        }

        return strings;
    }

    // ========================================================================
    // Control Flow Analysis
    // ========================================================================

    struct BasicBlock {
        uint64_t startAddress;
        uint64_t endAddress;
        std::vector<uint64_t> successors;
        std::vector<uint64_t> predecessors;
        bool isReturn;
        bool isCall;
    };

    std::vector<BasicBlock> AnalyzeControlFlow(uint64_t functionStart) {
        std::vector<BasicBlock> blocks;
        // Simplified CFG construction
        // Real implementation would follow jumps and calls
        return blocks;
    }

    // ========================================================================
    // Pattern Matching
    // ========================================================================

    struct PatternMatch {
        uint64_t address;
        std::vector<uint8_t> bytes;
        std::string description;
    };

    std::vector<PatternMatch> FindPattern(const std::vector<uint8_t>& pattern, 
                                          const std::vector<uint8_t>& mask) {
        std::vector<PatternMatch> matches;
        
        for (size_t i = 0; i <= m_binaryData.size() - pattern.size(); ++i) {
            bool found = true;
            for (size_t j = 0; j < pattern.size(); ++j) {
                if (mask[j] == 0xFF && m_binaryData[i + j] != pattern[j]) {
                    found = false;
                    break;
                }
            }
            
            if (found) {
                PatternMatch match;
                match.address = i;
                match.bytes.assign(m_binaryData.begin() + i, 
                                  m_binaryData.begin() + i + pattern.size());
                matches.push_back(match);
            }
        }
        
        return matches;
    }

    // ========================================================================
    // Export to Various Formats
    // ========================================================================

    std::string ExportToIDA() {
        std::ostringstream oss;
        oss << "; IDA Script for " << m_filePath << "\n";
        oss << "; Generated by RawrCodex\n\n";
        
        for (const auto& sym : m_symbols) {
            if (sym.type == "function") {
                oss << "MakeFunction(" << std::hex << sym.address << ");\n";
                oss << "MakeName(" << std::hex << sym.address 
                    << ", \"" << sym.name << "\");\n";
            }
        }
        
        return oss.str();
    }

    std::string ExportToGhidra() {
        std::ostringstream oss;
        oss << "# Ghidra Script for " << m_filePath << "\n";
        oss << "# Generated by RawrCodex\n\n";
        
        for (const auto& sym : m_symbols) {
            oss << "createFunction(toAddr(0x" << std::hex << sym.address 
                << "), \"" << sym.name << "\");\n";
        }
        
        return oss.str();
    }

    // ========================================================================
    // Vulnerability Detection
    // ========================================================================

    struct Vulnerability {
        std::string type;
        uint64_t address;
        std::string description;
        std::string severity; // "critical", "high", "medium", "low"
    };

    std::vector<Vulnerability> DetectVulnerabilities() {
        std::vector<Vulnerability> vulns;
        
        // Check for dangerous functions
        std::vector<std::string> dangerousFuncs = {
            "strcpy", "strcat", "gets", "sprintf", "vsprintf",
            "scanf", "sscanf", "system", "popen", "exec"
        };
        
        for (const auto& imp : m_imports) {
            for (const auto& danger : dangerousFuncs) {
                if (imp.functionName.find(danger) != std::string::npos) {
                    Vulnerability vuln;
                    vuln.type = "dangerous_function";
                    vuln.address = imp.address;
                    vuln.description = "Use of " + imp.functionName + " can lead to buffer overflow";
                    vuln.severity = "high";
                    vulns.push_back(vuln);
                }
            }
        }
        
        // Check for stack canaries (absence is a vuln)
        bool hasStackProtection = false;
        for (const auto& imp : m_imports) {
            if (imp.functionName.find("__security") != std::string::npos ||
                imp.functionName.find("stack_chk") != std::string::npos) {
                hasStackProtection = true;
                break;
            }
        }
        
        if (!hasStackProtection) {
            Vulnerability vuln;
            vuln.type = "no_stack_protection";
            vuln.address = 0;
            vuln.description = "Binary compiled without stack canaries";
            vuln.severity = "medium";
            vulns.push_back(vuln);
        }
        
        return vulns;
    }

    // ========================================================================
    // Code Structure Analysis
    // ========================================================================

    std::string AnalyzeCodeStructure() {
        std::ostringstream oss;
        oss << "=== Code Structure Analysis ===\n\n";
        
        oss << "Binary: " << m_filePath << "\n";
        oss << "Architecture: " << m_architecture << " (" << m_bitness << "-bit)\n";
        oss << "File Size: " << m_fileSize << " bytes\n\n";
        
        oss << "Sections (" << m_sections.size() << "):\n";
        for (const auto& section : m_sections) {
            oss << "  " << section.name 
                << " - VA: 0x" << std::hex << section.virtualAddress
                << ", Size: " << std::dec << section.virtualSize << " bytes\n";
        }
        oss << "\n";
        
        oss << "Symbols (" << m_symbols.size() << "):\n";
        for (size_t i = 0; i < std::min(m_symbols.size(), size_t(20)); ++i) {
            oss << "  " << m_symbols[i].name 
                << " @ 0x" << std::hex << m_symbols[i].address
                << " [" << m_symbols[i].type << "]\n";
        }
        if (m_symbols.size() > 20) {
            oss << "  ... and " << (m_symbols.size() - 20) << " more\n";
        }
        oss << "\n";
        
        oss << "Imports (" << m_imports.size() << "):\n";
        std::unordered_map<std::string, int> moduleCount;
        for (const auto& imp : m_imports) {
            moduleCount[imp.moduleName]++;
        }
        for (const auto& [module, count] : moduleCount) {
            oss << "  " << module << ": " << count << " functions\n";
        }
        oss << "\n";
        
        oss << "Exports (" << m_exports.size() << "):\n";
        for (size_t i = 0; i < std::min(m_exports.size(), size_t(10)); ++i) {
            oss << "  " << m_exports[i].name 
                << " @ 0x" << std::hex << m_exports[i].address << "\n";
        }
        if (m_exports.size() > 10) {
            oss << "  ... and " << (m_exports.size() - 10) << " more\n";
        }
        
        return oss.str();
    }

private:
    std::string m_filePath;
    size_t m_fileSize;
    std::vector<uint8_t> m_binaryData;
    std::string m_architecture;
    int m_bitness;
    
    std::vector<Symbol> m_symbols;
    std::vector<Section> m_sections;
    std::vector<Import> m_imports;
    std::vector<Export> m_exports;

    // ========================================================================
    // PE Parser
    // ========================================================================

    bool ParsePE() {
        if (m_fileSize < 64) return false;
        
        // Read DOS header
        uint32_t peOffset = *reinterpret_cast<uint32_t*>(&m_binaryData[0x3C]);
        if (peOffset + 4 > m_fileSize) return false;
        
        // Verify PE signature
        if (m_binaryData[peOffset] != 'P' || m_binaryData[peOffset + 1] != 'E') 
            return false;
        
        // Read COFF header
        uint16_t machine = *reinterpret_cast<uint16_t*>(&m_binaryData[peOffset + 4]);
        uint16_t numSections = *reinterpret_cast<uint16_t*>(&m_binaryData[peOffset + 6]);
        uint16_t sizeOfOptHeader = *reinterpret_cast<uint16_t*>(&m_binaryData[peOffset + 20]);
        
        m_architecture = (machine == 0x8664) ? "x64" : "x86";
        m_bitness = (machine == 0x8664) ? 64 : 32;
        
        // Parse sections
        size_t sectionTableOffset = peOffset + 24 + sizeOfOptHeader;
        for (int i = 0; i < numSections; ++i) {
            size_t sectionOffset = sectionTableOffset + i * 40;
            if (sectionOffset + 40 > m_fileSize) break;
            
            Section section;
            char name[9] = {0};
            memcpy(name, &m_binaryData[sectionOffset], 8);
            section.name = name;
            
            section.virtualSize = *reinterpret_cast<uint32_t*>(&m_binaryData[sectionOffset + 8]);
            section.virtualAddress = *reinterpret_cast<uint32_t*>(&m_binaryData[sectionOffset + 12]);
            section.rawSize = *reinterpret_cast<uint32_t*>(&m_binaryData[sectionOffset + 16]);
            section.rawOffset = *reinterpret_cast<uint32_t*>(&m_binaryData[sectionOffset + 20]);
            section.characteristics = *reinterpret_cast<uint32_t*>(&m_binaryData[sectionOffset + 36]);
            
            // Read section data
            if (section.rawOffset + section.rawSize <= m_fileSize) {
                section.data.assign(m_binaryData.begin() + section.rawOffset,
                                   m_binaryData.begin() + section.rawOffset + section.rawSize);
            }
            
            m_sections.push_back(section);
        }
        
        // Parse imports/exports would go here
        // Simplified for now
        
        return true;
    }

    bool ParseELF() {
        // Simplified ELF parser
        m_architecture = "x64";
        m_bitness = 64;
        return true;
    }

    const Section* FindSectionByVA(uint64_t va) const {
        for (const auto& section : m_sections) {
            if (va >= section.virtualAddress && 
                va < section.virtualAddress + section.virtualSize) {
                return &section;
            }
        }
        return nullptr;
    }

    // ========================================================================
    // Simplified x64 Instruction Decoder
    // ========================================================================

    size_t DecodeInstruction(const uint8_t* ptr, size_t maxLen, DisassemblyLine& out) {
        if (maxLen < 1) return 0;
        
        uint8_t opcode = ptr[0];
        size_t len = 1;
        
        // Simplified decoder - real decoder would handle prefixes, ModR/M, SIB, etc.
        
        // Common single-byte instructions
        if (opcode == 0x90) {
            out.mnemonic = "nop";
            out.bytes.push_back(opcode);
            return 1;
        } else if (opcode == 0xC3) {
            out.mnemonic = "ret";
            out.bytes.push_back(opcode);
            return 1;
        } else if (opcode == 0xCC) {
            out.mnemonic = "int3";
            out.bytes.push_back(opcode);
            return 1;
        } else if (opcode >= 0x50 && opcode <= 0x57) {
            out.mnemonic = "push";
            out.operands = "r" + std::to_string(opcode - 0x50);
            out.bytes.push_back(opcode);
            return 1;
        } else if (opcode >= 0x58 && opcode <= 0x5F) {
            out.mnemonic = "pop";
            out.operands = "r" + std::to_string(opcode - 0x58);
            out.bytes.push_back(opcode);
            return 1;
        } else if (opcode == 0xE8 && maxLen >= 5) {
            // call rel32
            out.mnemonic = "call";
            int32_t offset = *reinterpret_cast<const int32_t*>(ptr + 1);
            out.operands = "0x" + std::to_string(offset);
            for (size_t i = 0; i < 5; ++i) out.bytes.push_back(ptr[i]);
            return 5;
        } else if (opcode == 0xE9 && maxLen >= 5) {
            // jmp rel32
            out.mnemonic = "jmp";
            int32_t offset = *reinterpret_cast<const int32_t*>(ptr + 1);
            out.operands = "0x" + std::to_string(offset);
            for (size_t i = 0; i < 5; ++i) out.bytes.push_back(ptr[i]);
            return 5;
        }
        
        // Default: unknown instruction
        out.mnemonic = "db";
        out.operands = "0x" + std::to_string(opcode);
        out.bytes.push_back(opcode);
        return 1;
    }
};

} // namespace ReverseEngineering
} // namespace RawrXD
