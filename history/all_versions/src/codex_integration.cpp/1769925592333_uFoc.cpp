// Codex Reverse Engineering Integration
// Always accessible reverse engineering system
// Generated: 2026-01-25 06:34:12

#include "codex_integration.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <windows.h>

namespace RawrXD {

CodexIntegration::CodexIntegration() {}
CodexIntegration::~CodexIntegration() {}

AnalysisResult CodexIntegration::analyzeBinary(const std::string& filePath) {
    AnalysisResult result;
    result.originalPath = filePath;
    result.fileType = "Unknown";
    result.entropy = 0;

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return result;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (file.read((char*)buffer.data(), size)) {
        // Basic Type Detection (PE / ELF)
        if (size > 2 && buffer[0] == 'M' && buffer[1] == 'Z') {
            result.fileType = "PE Executable (Windows)";
            // Parse PE header for Arch
        } else if (size > 4 && buffer[0] == 0x7F && buffer[1] == 'E' && buffer[2] == 'L' && buffer[3] == 'F') {
            result.fileType = "ELF Executable (Linux/Unix)";
        }

        result.hash = calculateHash(buffer);
        result.entropy = calculateEntropy(buffer);
        result.strings = extractStrings(buffer);
        
        // Basic Export Parsing (for PE)
        if (result.fileType.find("PE") != std::string::npos) {
             result.exportedFunctions = parseExports(buffer);
        }
    }

    return result;
}

std::string CodexIntegration::reconstructCode(const AnalysisResult& analysis) {
    std::stringstream code;
    code << "// Reconstructed Code for " << analysis.originalPath << "\n";
    code << "// Type: " << analysis.fileType << "\n";
    code << "// Hash: " << analysis.hash << "\n";
    code << "// Entropy: " << analysis.entropy << "\n\n";

    if (!analysis.exportedFunctions.empty()) {
        code << "// Exported Functions:\n";
        for (const auto& func : analysis.exportedFunctions) {
            code << "void " << func << "() { /* ... */ }\n";
        }
    }
    
    code << "\n// Recovered Strings (Heuristic Constants):\n";
    int i = 0;
    for (const auto& str : analysis.strings) {
        if (i++ > 20) break; // Limit
        code << "const char* str_" << i << " = \"" << str << "\";\n";
    }

    return code.str();
}

std::string CodexIntegration::calculateHash(const std::vector<uint8_t>& data) {
    // Simple FNV-1a hash for demo (Production would use SHA256)
    uint64_t hash = 14695981039346656037ULL;
    for (uint8_t byte : data) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

double CodexIntegration::calculateEntropy(const std::vector<uint8_t>& data) {
    if (data.empty()) return 0.0;
    
    std::vector<int> counts(256, 0);
    for (uint8_t byte : data) counts[byte]++;

    double entropy = 0.0;
    for (int count : counts) {
        if (count > 0) {
            double p = (double)count / data.size();
            entropy -= p * std::log2(p);
        }
    }
    return entropy;
}

std::vector<std::string> CodexIntegration::extractStrings(const std::vector<uint8_t>& data, size_t minLen) {
    std::vector<std::string> strings;
    std::string current;
    for (uint8_t byte : data) {
        if (byte >= 32 && byte <= 126) {
            current += (char)byte;
        } else {
            if (current.length() >= minLen) {
                strings.push_back(current);
            }
            current.clear();
        }
    }
    if (current.length() >= minLen) strings.push_back(current);
    
    // Sort logic removed for speed
    return strings;
}

std::vector<std::string> CodexIntegration::parseExports(const std::vector<uint8_t>& data) {
    std::vector<std::string> exports;
    // Minimal PE Export directory parsing logic
    // 1. Find PE Offset at 0x3C
    if (data.size() < 0x3C + 4) return exports;
    
    uint32_t peOffset = *(uint32_t*)&data[0x3C];
    if (peOffset + 24 + 96 + 8 > data.size()) return exports; // Safety check
    
    // 2. Read Export Table RVA from Optional Header (offset 120 from PE start for x64, 104 for x86)
    // Assuming x64 for simplicity or checking Magic
    uint16_t magic = *(uint16_t*)&data[peOffset + 24];
    uint32_t exportDirRVA = 0;
    
    if (magic == 0x20b) { // PE32+ (64-bit)
         if (peOffset + 24 + 112 + 4 > data.size()) return exports;
         exportDirRVA = *(uint32_t*)&data[peOffset + 24 + 112]; 
         // Note: offset calculation 24 (FileHeader) + 112 (DataDirectories start at offset 112 in OptionalHeader64?)
         // OptionalHeader64 size is variable, but DataDirectory[0] is Export.
    } else {
         exportDirRVA = *(uint32_t*)&data[peOffset + 24 + 96];
    }
    
    if (exportDirRVA == 0) return exports;

    // RVA to Offset conversion is complex without full section parsing.
    // Simplifying: Scanning for string patterns that look like mangled names near the RVA if mapped?
    // In a real reverse engineer tool we parse sections.
    // For "No Stub" requirement, let's implement the section walk.
    
    // Number of sections
    uint16_t numSections = *(uint16_t*)&data[peOffset + 6];
    uint32_t optHeaderSize = *(uint16_t*)&data[peOffset + 20];
    uint32_t sectionTableOffset = peOffset + 24 + optHeaderSize;
    
    uint32_t fileOffset = 0;
    
    for (int i=0; i<numSections; i++) {
        uint32_t entry = sectionTableOffset + (i * 40);
        if (entry + 40 > data.size()) break;
        
        uint32_t vAddr = *(uint32_t*)&data[entry + 12];
        uint32_t vSize = *(uint32_t*)&data[entry + 8];
        uint32_t rawOffset = *(uint32_t*)&data[entry + 20];
        
        if (exportDirRVA >= vAddr && exportDirRVA < vAddr + vSize) {
            fileOffset = rawOffset + (exportDirRVA - vAddr);
            break;
        }
    }
    
    if (fileOffset == 0 || fileOffset + 40 > data.size()) return exports;
    
    // IMAGE_EXPORT_DIRECTORY
    // 0x20: AddressOfNames RVA
    uint32_t namesRVA = *(uint32_t*)&data[fileOffset + 32];
    uint32_t numNames = *(uint32_t*)&data[fileOffset + 24];
    
    // Find section for namesRVA
    uint32_t namesOffset = 0;
    for (int i=0; i<numSections; i++) {
        uint32_t entry = sectionTableOffset + (i * 40);
        uint32_t vAddr = *(uint32_t*)&data[entry + 12];
        uint32_t vSize = *(uint32_t*)&data[entry + 8];
        uint32_t rawOffset = *(uint32_t*)&data[entry + 20];
        if (namesRVA >= vAddr && namesRVA < vAddr + vSize) {
            namesOffset = rawOffset + (namesRVA - vAddr);
            break;
        }
    }

    if (namesOffset != 0 && namesOffset + numNames * 4 <= data.size()) {
        for (uint32_t i=0; i<numNames; i++) {
            uint32_t nameRVA = *(uint32_t*)&data[namesOffset + i*4];
            // Convert Name RVA
             for (int j=0; j<numSections; j++) {
                uint32_t entry = sectionTableOffset + (j * 40);
                uint32_t vAddr = *(uint32_t*)&data[entry + 12];
                uint32_t vSize = *(uint32_t*)&data[entry + 8];
                uint32_t rawOffset = *(uint32_t*)&data[entry + 20];
                if (nameRVA >= vAddr && nameRVA < vAddr + vSize) {
                    uint32_t nameOffset = rawOffset + (nameRVA - vAddr);
                    if (nameOffset < data.size()) {
                        const char* str = (const char*)&data[nameOffset];
                        size_t maxLen = data.size() - nameOffset;
                        size_t len = strnlen(str, maxLen);
                        exports.push_back(std::string(str, len));
                    }
                    break;
                }
             }
        }
    }

    return exports;
}

} // namespace RawrXD

// Codex reverse engineering is always available
// Script: d:\lazy init ide\Build-CodexReverse.ps1
// Output: d:\lazy init ide\CodexReverse.asm

// TODO: Integrate Codex reverse engineering capabilities
// - Binary analysis
// - Code reconstruction
// - Algorithm extraction
// - Optimization patterns

