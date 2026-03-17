// Codex Integration - Binary Analysis and Code Reconstruction
// Provides reverse engineering capabilities for the IDE

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace CodexIntegration {

// Detected code pattern
struct CodePattern {
    std::string name;
    std::string category;     // "algorithm", "data_structure", "design_pattern"
    uint64_t address = 0;
    size_t size = 0;
    float confidence = 0.0f;
    std::string pseudocode;
};

// Binary analysis result
struct AnalysisResult {
    bool success = false;
    std::string errorMessage;
    std::string architecture;       // x86, x86_64, ARM64
    size_t textSectionSize = 0;
    size_t dataSectionSize = 0;
    int functionCount = 0;
    int stringCount = 0;
    std::vector<CodePattern> patterns;
    std::vector<std::string> imports;
    std::vector<std::string> exports;
    std::vector<std::string> strings;
};

// Analyze a PE binary for code patterns and structure
AnalysisResult analyzeBinary(const std::string& filePath) {
    AnalysisResult result;
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        result.errorMessage = "Cannot open file: " + filePath;
        return result;
    }
    
    // Read first 2 bytes for format detection
    uint16_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 2);
    
    if (magic == 0x5A4D) { // 'MZ' - PE format
        // Read PE header offset
        file.seekg(0x3C, std::ios::beg);
        uint32_t peOffset = 0;
        file.read(reinterpret_cast<char*>(&peOffset), 4);
        
        // Read PE signature
        file.seekg(peOffset, std::ios::beg);
        uint32_t peSig = 0;
        file.read(reinterpret_cast<char*>(&peSig), 4);
        
        if (peSig == 0x00004550) { // "PE\0\0"
            // Read COFF header
            uint16_t machine = 0;
            file.read(reinterpret_cast<char*>(&machine), 2);
            
            if (machine == 0x8664) result.architecture = "x86_64";
            else if (machine == 0x014C) result.architecture = "x86";
            else if (machine == 0xAA64) result.architecture = "ARM64";
            else result.architecture = "unknown (0x" + std::to_string(machine) + ")";
            
            uint16_t numSections = 0;
            file.read(reinterpret_cast<char*>(&numSections), 2);
            
            // Skip rest of COFF header + optional header to get to section table
            file.seekg(12, std::ios::cur); // TimeDateStamp, PointerToSymbolTable, NumberOfSymbols
            uint16_t optHeaderSize = 0;
            file.read(reinterpret_cast<char*>(&optHeaderSize), 2);
            file.seekg(2 + optHeaderSize, std::ios::cur); // Characteristics + optional header
            
            // Parse section headers
            for (int i = 0; i < numSections; ++i) {
                char sectionName[9] = {};
                file.read(sectionName, 8);
                
                uint32_t virtualSize = 0, rawSize = 0;
                file.read(reinterpret_cast<char*>(&virtualSize), 4);
                file.seekg(4, std::ios::cur); // VirtualAddress
                file.read(reinterpret_cast<char*>(&rawSize), 4);
                file.seekg(16, std::ios::cur); // Rest of section header
                
                if (std::string(sectionName) == ".text") {
                    result.textSectionSize = rawSize;
                } else if (std::string(sectionName) == ".data" || 
                           std::string(sectionName) == ".rdata") {
                    result.dataSectionSize += rawSize;
                }
            }
            
            result.success = true;
        }
    } else if (magic == 0x457F) { // ELF
        result.architecture = "ELF";
        result.success = true;
    } else {
        result.errorMessage = "Unknown binary format";
    }
    
    // Extract printable strings (minimum 4 chars)
    file.seekg(0, std::ios::beg);
    std::string currentString;
    char c;
    while (file.get(c)) {
        if (c >= 32 && c < 127) {
            currentString += c;
        } else {
            if (currentString.size() >= 4) {
                result.strings.push_back(currentString);
                result.stringCount++;
            }
            currentString.clear();
        }
        // Cap string extraction at 10000 to avoid memory issues
        if (result.stringCount >= 10000) break;
    }
    
    file.close();
    
    std::cout << "[Codex] Analyzed: " << filePath << " (" << result.architecture 
              << ", .text=" << result.textSectionSize 
              << ", strings=" << result.stringCount << ")" << std::endl;
    
    return result;
}

// Extract function signatures from a DLL's export table
std::vector<std::string> extractExports(const std::string& dllPath) {
    std::vector<std::string> exports;
    
#ifdef _WIN32
    HMODULE hMod = LoadLibraryExA(dllPath.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!hMod) return exports;
    
    // Parse PE export directory from loaded module
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hMod;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hMod + dosHeader->e_lfanew);
    
    DWORD exportDirRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (exportDirRVA == 0) {
        FreeLibrary(hMod);
        return exports;
    }
    
    PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hMod + exportDirRVA);
    DWORD* nameRVAs = (DWORD*)((BYTE*)hMod + exportDir->AddressOfNames);
    
    for (DWORD i = 0; i < exportDir->NumberOfNames; ++i) {
        const char* name = (const char*)((BYTE*)hMod + nameRVAs[i]);
        exports.push_back(name);
    }
    
    FreeLibrary(hMod);
#endif
    
    return exports;
}

} // namespace CodexIntegration

