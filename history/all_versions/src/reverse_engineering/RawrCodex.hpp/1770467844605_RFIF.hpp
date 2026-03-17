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
#include <cstring>
#include <iomanip>

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

        // ================================================================
        // Parse Optional Header for Data Directory entries
        // ================================================================
        size_t optHeaderOffset = peOffset + 24;
        uint16_t optMagic = 0;
        if (optHeaderOffset + 2 <= m_fileSize) {
            optMagic = *reinterpret_cast<uint16_t*>(&m_binaryData[optHeaderOffset]);
        }
        bool isPE32Plus = (optMagic == 0x20B);  // PE32+ (64-bit)

        // Image base
        uint64_t imageBase = 0;
        if (isPE32Plus && optHeaderOffset + 30 <= m_fileSize) {
            imageBase = *reinterpret_cast<uint64_t*>(&m_binaryData[optHeaderOffset + 24]);
        } else if (optHeaderOffset + 32 <= m_fileSize) {
            imageBase = *reinterpret_cast<uint32_t*>(&m_binaryData[optHeaderOffset + 28]);
        }

        // Number of Data Directory entries
        uint32_t numDataDirs = 0;
        size_t dataDirBase = 0;
        if (isPE32Plus) {
            // PE32+: NumberOfRvaAndSizes at offset 108 into optional header
            if (optHeaderOffset + 112 <= m_fileSize) {
                numDataDirs = *reinterpret_cast<uint32_t*>(&m_binaryData[optHeaderOffset + 108]);
                dataDirBase = optHeaderOffset + 112;
            }
        } else {
            // PE32: NumberOfRvaAndSizes at offset 92 into optional header
            if (optHeaderOffset + 96 <= m_fileSize) {
                numDataDirs = *reinterpret_cast<uint32_t*>(&m_binaryData[optHeaderOffset + 92]);
                dataDirBase = optHeaderOffset + 96;
            }
        }
        if (numDataDirs > 16) numDataDirs = 16; // Safety cap

        // Read import and export directory RVA/Size
        uint32_t exportDirRVA = 0, exportDirSize = 0;
        uint32_t importDirRVA = 0, importDirSize = 0;
        if (numDataDirs > 0 && dataDirBase + 8 <= m_fileSize) {
            exportDirRVA  = *reinterpret_cast<uint32_t*>(&m_binaryData[dataDirBase + 0]);
            exportDirSize = *reinterpret_cast<uint32_t*>(&m_binaryData[dataDirBase + 4]);
        }
        if (numDataDirs > 1 && dataDirBase + 16 <= m_fileSize) {
            importDirRVA  = *reinterpret_cast<uint32_t*>(&m_binaryData[dataDirBase + 8]);
            importDirSize = *reinterpret_cast<uint32_t*>(&m_binaryData[dataDirBase + 12]);
        }

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

        // ================================================================
        // Parse Import Directory Table (IDT)
        // ================================================================
        if (importDirRVA != 0 && importDirSize != 0) {
            size_t importFileOffset = RvaToFileOffset(importDirRVA);
            if (importFileOffset != 0) {
                // Each IMAGE_IMPORT_DESCRIPTOR is 20 bytes
                // Terminated by an all-zero entry
                for (size_t idx = 0; idx < 1024; ++idx) { // Safety cap
                    size_t descOffset = importFileOffset + idx * 20;
                    if (descOffset + 20 > m_fileSize) break;

                    uint32_t iltRVA     = *reinterpret_cast<uint32_t*>(&m_binaryData[descOffset + 0]);
                    uint32_t nameRVA    = *reinterpret_cast<uint32_t*>(&m_binaryData[descOffset + 12]);
                    uint32_t iatRVA     = *reinterpret_cast<uint32_t*>(&m_binaryData[descOffset + 16]);

                    // Terminator: all zeros
                    if (iltRVA == 0 && nameRVA == 0 && iatRVA == 0) break;

                    // Read DLL name
                    std::string dllName;
                    size_t nameFileOff = RvaToFileOffset(nameRVA);
                    if (nameFileOff != 0 && nameFileOff < m_fileSize) {
                        for (size_t c = nameFileOff; c < m_fileSize && m_binaryData[c] != 0; ++c) {
                            dllName += static_cast<char>(m_binaryData[c]);
                            if (dllName.size() > 260) break; // MAX_PATH safety
                        }
                    }

                    // Walk the Import Lookup Table (ILT) or IAT
                    uint32_t thunkRVA = (iltRVA != 0) ? iltRVA : iatRVA;
                    size_t thunkFileOff = RvaToFileOffset(thunkRVA);
                    if (thunkFileOff == 0) continue;

                    size_t entrySize = isPE32Plus ? 8 : 4;
                    for (size_t t = 0; t < 8192; ++t) { // Safety cap per DLL
                        size_t entryOff = thunkFileOff + t * entrySize;
                        if (entryOff + entrySize > m_fileSize) break;

                        uint64_t thunkValue = 0;
                        if (isPE32Plus) {
                            thunkValue = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff]);
                        } else {
                            thunkValue = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff]);
                        }
                        if (thunkValue == 0) break; // End of thunk array

                        Import imp;
                        imp.moduleName = dllName;
                        imp.address    = imageBase + iatRVA + t * entrySize;

                        // Check ordinal flag (bit 63 for PE32+, bit 31 for PE32)
                        bool isOrdinal = false;
                        if (isPE32Plus) {
                            isOrdinal = (thunkValue & 0x8000000000000000ULL) != 0;
                        } else {
                            isOrdinal = (thunkValue & 0x80000000UL) != 0;
                        }

                        if (isOrdinal) {
                            imp.ordinal = static_cast<uint16_t>(thunkValue & 0xFFFF);
                            imp.functionName = "Ordinal_" + std::to_string(imp.ordinal);
                        } else {
                            // Hint/Name Table entry: 2-byte hint + null-terminated name
                            uint32_t hintNameRVA = static_cast<uint32_t>(thunkValue & 0x7FFFFFFF);
                            size_t hintNameOff = RvaToFileOffset(hintNameRVA);
                            if (hintNameOff != 0 && hintNameOff + 2 < m_fileSize) {
                                imp.ordinal = *reinterpret_cast<uint16_t*>(&m_binaryData[hintNameOff]);
                                size_t nameStart = hintNameOff + 2;
                                for (size_t c = nameStart; c < m_fileSize && m_binaryData[c] != 0; ++c) {
                                    imp.functionName += static_cast<char>(m_binaryData[c]);
                                    if (imp.functionName.size() > 512) break;
                                }
                            } else {
                                imp.functionName = "Unknown";
                                imp.ordinal = 0;
                            }
                        }

                        m_imports.push_back(imp);
                    }
                }
            }
        }

        // ================================================================
        // Parse Export Directory Table (EDT)
        // ================================================================
        if (exportDirRVA != 0 && exportDirSize != 0) {
            size_t exportFileOffset = RvaToFileOffset(exportDirRVA);
            if (exportFileOffset != 0 && exportFileOffset + 40 <= m_fileSize) {
                // IMAGE_EXPORT_DIRECTORY fields
                uint32_t numberOfFunctions    = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 20]);
                uint32_t numberOfNames        = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 24]);
                uint32_t addressTableRVA      = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 28]);
                uint32_t namePointerTableRVA  = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 32]);
                uint32_t ordinalTableRVA      = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 36]);
                uint32_t ordinalBase          = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 16]);

                // Safety caps
                if (numberOfFunctions > 65536) numberOfFunctions = 65536;
                if (numberOfNames > 65536) numberOfNames = 65536;

                size_t addrTableOff    = RvaToFileOffset(addressTableRVA);
                size_t nameTableOff    = RvaToFileOffset(namePointerTableRVA);
                size_t ordinalTableOff = RvaToFileOffset(ordinalTableRVA);

                // Build name→ordinal mapping from the name table
                std::unordered_map<uint16_t, std::string> ordinalToName;
                if (nameTableOff != 0 && ordinalTableOff != 0) {
                    for (uint32_t n = 0; n < numberOfNames; ++n) {
                        size_t nameRVAoff = nameTableOff + n * 4;
                        size_t ordOff     = ordinalTableOff + n * 2;
                        if (nameRVAoff + 4 > m_fileSize || ordOff + 2 > m_fileSize) break;

                        uint32_t funcNameRVA = *reinterpret_cast<uint32_t*>(&m_binaryData[nameRVAoff]);
                        uint16_t ordIndex    = *reinterpret_cast<uint16_t*>(&m_binaryData[ordOff]);

                        size_t funcNameOff = RvaToFileOffset(funcNameRVA);
                        if (funcNameOff != 0 && funcNameOff < m_fileSize) {
                            std::string funcName;
                            for (size_t c = funcNameOff; c < m_fileSize && m_binaryData[c] != 0; ++c) {
                                funcName += static_cast<char>(m_binaryData[c]);
                                if (funcName.size() > 512) break;
                            }
                            ordinalToName[ordIndex] = funcName;
                        }
                    }
                }

                // Walk the Export Address Table
                if (addrTableOff != 0) {
                    for (uint32_t f = 0; f < numberOfFunctions; ++f) {
                        size_t entryOff = addrTableOff + f * 4;
                        if (entryOff + 4 > m_fileSize) break;

                        uint32_t funcRVA = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff]);
                        if (funcRVA == 0) continue;

                        Export exp;
                        exp.ordinal = static_cast<uint16_t>(ordinalBase + f);
                        exp.address = imageBase + funcRVA;

                        // Check if this is a forwarder (RVA points within export dir)
                        if (funcRVA >= exportDirRVA && funcRVA < exportDirRVA + exportDirSize) {
                            exp.isForwarder = true;
                            size_t fwdOff = RvaToFileOffset(funcRVA);
                            if (fwdOff != 0 && fwdOff < m_fileSize) {
                                for (size_t c = fwdOff; c < m_fileSize && m_binaryData[c] != 0; ++c) {
                                    exp.forwarderName += static_cast<char>(m_binaryData[c]);
                                    if (exp.forwarderName.size() > 512) break;
                                }
                            }
                        } else {
                            exp.isForwarder = false;
                        }

                        // Look up name
                        auto it = ordinalToName.find(static_cast<uint16_t>(f));
                        if (it != ordinalToName.end()) {
                            exp.name = it->second;
                        } else {
                            exp.name = "Ordinal_" + std::to_string(exp.ordinal);
                        }

                        m_exports.push_back(exp);

                        // Also add to symbols table
                        Symbol sym;
                        sym.name = exp.name;
                        sym.address = exp.address;
                        sym.size = 0;
                        sym.section = "";
                        sym.type = "export";
                        sym.isMangled = false;
                        sym.demangledName = exp.name;
                        m_symbols.push_back(sym);
                    }
                }
            }
        }

        // Build symbols from imports
        for (const auto& imp : m_imports) {
            Symbol sym;
            sym.name = imp.functionName;
            sym.address = imp.address;
            sym.size = 0;
            sym.section = ".idata";
            sym.type = "import";
            sym.isMangled = false;
            sym.demangledName = imp.functionName;
            m_symbols.push_back(sym);
        }

        return true;
    }

    // ================================================================
    // RVA → File Offset Converter (uses section table)
    // ================================================================
    size_t RvaToFileOffset(uint32_t rva) const {
        for (const auto& section : m_sections) {
            uint32_t secVA = static_cast<uint32_t>(section.virtualAddress);
            uint32_t secVSize = static_cast<uint32_t>(section.virtualSize);
            uint32_t secRawOff = static_cast<uint32_t>(section.rawOffset);
            uint32_t secRawSize = static_cast<uint32_t>(section.rawSize);

            if (rva >= secVA && rva < secVA + std::max(secVSize, secRawSize)) {
                size_t offset = secRawOff + (rva - secVA);
                if (offset < m_fileSize) return offset;
            }
        }
        return 0; // Not found
    }

    bool ParseELF() {
        // Validate ELF header (already checked magic bytes in LoadBinary)
        if (m_fileSize < 64) return false;

        uint8_t elfClass = m_binaryData[4];   // 1 = 32-bit, 2 = 64-bit
        uint8_t elfData  = m_binaryData[5];   // 1 = LE, 2 = BE
        (void)elfData; // We assume LE for now (x86/x64)

        if (elfClass == 2) {
            // ELF64
            m_architecture = "x64";
            m_bitness = 64;

            if (m_fileSize < 64) return false;

            // ELF64 header fields
            uint64_t shoff     = *reinterpret_cast<uint64_t*>(&m_binaryData[40]); // Section header table offset
            uint16_t shentsize = *reinterpret_cast<uint16_t*>(&m_binaryData[58]); // Section header entry size
            uint16_t shnum     = *reinterpret_cast<uint16_t*>(&m_binaryData[60]); // Number of section headers
            uint16_t shstrndx  = *reinterpret_cast<uint16_t*>(&m_binaryData[62]); // Section name string table index

            if (shoff == 0 || shnum == 0) return true; // No sections, but valid ELF
            if (shoff + static_cast<uint64_t>(shnum) * shentsize > m_fileSize) return false;
            if (shentsize < 64) return false;

            // Read section name string table first
            std::vector<uint8_t> shstrtab;
            if (shstrndx < shnum) {
                size_t strSecOff = static_cast<size_t>(shoff + shstrndx * shentsize);
                if (strSecOff + 64 <= m_fileSize) {
                    uint64_t strOff  = *reinterpret_cast<uint64_t*>(&m_binaryData[strSecOff + 24]);
                    uint64_t strSize = *reinterpret_cast<uint64_t*>(&m_binaryData[strSecOff + 32]);
                    if (strOff + strSize <= m_fileSize) {
                        shstrtab.assign(m_binaryData.begin() + strOff,
                                       m_binaryData.begin() + strOff + strSize);
                    }
                }
            }

            // Parse all section headers
            for (uint16_t i = 0; i < shnum; ++i) {
                size_t entryOff = static_cast<size_t>(shoff + i * shentsize);
                if (entryOff + 64 > m_fileSize) break;

                uint32_t sh_name       = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 0]);
                uint32_t sh_type       = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 4]);
                uint64_t sh_flags      = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff + 8]);
                uint64_t sh_addr       = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff + 16]);
                uint64_t sh_offset     = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff + 24]);
                uint64_t sh_size       = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff + 32]);
                (void)sh_flags;

                Section section;
                // Read name from string table
                if (sh_name < shstrtab.size()) {
                    for (size_t c = sh_name; c < shstrtab.size() && shstrtab[c] != 0; ++c) {
                        section.name += static_cast<char>(shstrtab[c]);
                    }
                } else {
                    section.name = ".section_" + std::to_string(i);
                }

                section.virtualAddress = sh_addr;
                section.virtualSize    = sh_size;
                section.rawOffset      = sh_offset;
                section.rawSize        = sh_size;
                section.characteristics = sh_type;

                // Read section data (SHT_PROGBITS=1, SHT_SYMTAB=2, SHT_STRTAB=3, etc.)
                if (sh_type != 8 /* SHT_NOBITS */ && sh_offset + sh_size <= m_fileSize && sh_size > 0) {
                    section.data.assign(m_binaryData.begin() + sh_offset,
                                       m_binaryData.begin() + sh_offset + sh_size);
                }

                m_sections.push_back(section);

                // Parse symbol table (SHT_SYMTAB=2 or SHT_DYNSYM=11)
                if (sh_type == 2 || sh_type == 11) {
                    uint32_t sh_link    = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 40]);
                    uint64_t sh_entsize = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff + 56]);
                    if (sh_entsize == 0) sh_entsize = 24; // Standard Elf64_Sym size

                    // Get the linked string table
                    std::vector<uint8_t> symstrtab;
                    if (sh_link < shnum) {
                        size_t linkOff = static_cast<size_t>(shoff + sh_link * shentsize);
                        if (linkOff + 64 <= m_fileSize) {
                            uint64_t linkDataOff  = *reinterpret_cast<uint64_t*>(&m_binaryData[linkOff + 24]);
                            uint64_t linkDataSize = *reinterpret_cast<uint64_t*>(&m_binaryData[linkOff + 32]);
                            if (linkDataOff + linkDataSize <= m_fileSize) {
                                symstrtab.assign(m_binaryData.begin() + linkDataOff,
                                                m_binaryData.begin() + linkDataOff + linkDataSize);
                            }
                        }
                    }

                    // Walk symbol entries
                    uint64_t numSyms = (sh_entsize > 0) ? sh_size / sh_entsize : 0;
                    for (uint64_t s = 0; s < numSyms && s < 65536; ++s) {
                        size_t symOff = static_cast<size_t>(sh_offset + s * sh_entsize);
                        if (symOff + 24 > m_fileSize) break;

                        uint32_t st_name  = *reinterpret_cast<uint32_t*>(&m_binaryData[symOff + 0]);
                        uint8_t  st_info  = m_binaryData[symOff + 4];
                        uint64_t st_value = *reinterpret_cast<uint64_t*>(&m_binaryData[symOff + 8]);
                        uint64_t st_size  = *reinterpret_cast<uint64_t*>(&m_binaryData[symOff + 16]);

                        uint8_t st_type = st_info & 0x0F;
                        // STT_FUNC=2, STT_OBJECT=1

                        Symbol sym;
                        if (st_name < symstrtab.size()) {
                            for (size_t c = st_name; c < symstrtab.size() && symstrtab[c] != 0; ++c) {
                                sym.name += static_cast<char>(symstrtab[c]);
                            }
                        }
                        if (sym.name.empty()) continue;

                        sym.address = st_value;
                        sym.size    = st_size;
                        sym.type    = (st_type == 2) ? "function" : "data";
                        sym.isMangled = (sym.name.size() > 2 && sym.name[0] == '_' && sym.name[1] == 'Z');
                        sym.demangledName = sym.name; // Demangling would require __cxa_demangle

                        m_symbols.push_back(sym);
                    }
                }
            }
        } else if (elfClass == 1) {
            // ELF32
            m_architecture = "x86";
            m_bitness = 32;
            // Minimal ELF32 support — parse sections only
            if (m_fileSize < 52) return false;
            uint32_t shoff     = *reinterpret_cast<uint32_t*>(&m_binaryData[32]);
            uint16_t shentsize = *reinterpret_cast<uint16_t*>(&m_binaryData[46]);
            uint16_t shnum     = *reinterpret_cast<uint16_t*>(&m_binaryData[48]);
            if (shoff == 0 || shnum == 0) return true;
            if (shentsize < 40) return false;

            for (uint16_t i = 0; i < shnum; ++i) {
                size_t entryOff = shoff + i * shentsize;
                if (entryOff + 40 > m_fileSize) break;

                Section section;
                section.name           = ".elf32_sec_" + std::to_string(i);
                section.virtualAddress = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 12]);
                section.virtualSize    = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 20]);
                section.rawOffset      = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 16]);
                section.rawSize        = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 20]);
                section.characteristics = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 4]);

                uint32_t sh_type = section.characteristics;
                if (sh_type != 8 && section.rawOffset + section.rawSize <= m_fileSize && section.rawSize > 0) {
                    section.data.assign(m_binaryData.begin() + section.rawOffset,
                                       m_binaryData.begin() + section.rawOffset + section.rawSize);
                }
                m_sections.push_back(section);
            }
        } else {
            return false; // Unknown ELF class
        }

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
