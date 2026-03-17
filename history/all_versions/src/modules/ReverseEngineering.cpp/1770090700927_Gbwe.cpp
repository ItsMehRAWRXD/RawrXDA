#include "ReverseEngineering.hpp"
#include <windows.h>
#include <winnt.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace RawrXD {
namespace ReverseEngineering {

// ===============================================================
// NATIVE DISASSEMBLER IMPLEMENTATION
// ===============================================================

std::vector<NativeDisassembler::Instruction> NativeDisassembler::DisassembleX64(
    const uint8_t* data, size_t size, uint64_t baseAddress) {
    
    std::vector<Instruction> instructions;
    size_t offset = 0;
    
    while (offset < size) {
        Instruction inst;
        inst.address = baseAddress + offset;
        
        size_t instLength = 0;
        inst.mnemonic = DecodeX64Instruction(data + offset, instLength, inst.address);
        
        if (instLength == 0) {
            instLength = 1; // Skip invalid byte
            inst.mnemonic = "db";
            inst.operands = "0x" + std::to_string(data[offset]);
        }
        
        inst.bytes.assign(data + offset, data + offset + instLength);
        
        // Basic jump/call detection
        if (inst.mnemonic.find("jmp") == 0 || inst.mnemonic.find("j") == 0) {
            inst.isJump = true;
            // Parse jump target (simplified)
        }
        if (inst.mnemonic.find("call") == 0) {
            inst.isCall = true;
        }
        
        instructions.push_back(inst);
        offset += instLength;
    }
    
    return instructions;
}

std::string NativeDisassembler::DecodeX64Instruction(const uint8_t* bytes, size_t& length, uint64_t address) {
    // Simplified x64 decoder - in production would use full decoder
    if (bytes[0] == 0x48 && bytes[1] == 0x89) {
        length = 3;
        return "mov";
    }
    if (bytes[0] == 0xE8) {
        length = 5;
        return "call";
    }
    if (bytes[0] == 0xE9) {
        length = 5;
        return "jmp";
    }
    if (bytes[0] == 0x90) {
        length = 1;
        return "nop";
    }
    if (bytes[0] == 0xC3) {
        length = 1;
        return "ret";
    }
    if (bytes[0] == 0xCC) {
        length = 1;
        return "int3";
    }
    
    // Default fallback
    length = 1;
    std::stringstream ss;
    ss << "db 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[0];
    return ss.str();
}

std::vector<std::string> NativeDisassembler::FindPatterns(const uint8_t* data, size_t size) {
    std::vector<std::string> patterns;
    
    // Common function prologue patterns
    for (size_t i = 0; i < size - 3; ++i) {
        // Standard x64 prologue: push rbp; mov rbp, rsp
        if (data[i] == 0x55 && data[i+1] == 0x48 && data[i+2] == 0x89 && data[i+3] == 0xE5) {
            patterns.push_back("Function prologue at 0x" + std::to_string(i));
        }
        
        // API call pattern
        if (data[i] == 0xFF && data[i+1] == 0x15) {
            patterns.push_back("API call at 0x" + std::to_string(i));
        }
    }
    
    return patterns;
}

std::vector<std::string> NativeDisassembler::ExtractStrings(const uint8_t* data, size_t size) {
    std::vector<std::string> strings;
    std::string current;
    
    for (size_t i = 0; i < size; ++i) {
        if (data[i] >= 32 && data[i] <= 126) { // Printable ASCII
            current += (char)data[i];
        } else {
            if (current.length() >= 4) { // Minimum string length
                strings.push_back(current);
            }
            current.clear();
        }
    }
    
    return strings;
}

std::vector<NativeDisassembler::Function> NativeDisassembler::AnalyzeFunctions(const std::vector<Instruction>& instructions) {
    std::vector<Function> functions;
    if (instructions.empty()) return functions;

    Function currentFunc;
    currentFunc.startAddress = instructions.front().address;
    currentFunc.name = "sub_" + std::to_string(currentFunc.startAddress);
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        currentFunc.instructions.push_back(instructions[i]);
        // Simple heuristic for function end: ret instruction
        if (instructions[i].mnemonic.find("ret") != std::string::npos) {
            currentFunc.endAddress = instructions[i].address;
            functions.push_back(currentFunc);
            
            // Start next function
            if (i + 1 < instructions.size()) {
                currentFunc = Function();
                currentFunc.startAddress = instructions[i + 1].address;
                currentFunc.name = "sub_" + std::to_string(currentFunc.startAddress);
            }
        }
    }
    
    // Add last function if not ended with ret
    if (!currentFunc.instructions.empty() && !functions.empty() && functions.back().endAddress != currentFunc.instructions.back().address) {
       // Only add if we haven't just added it
       // Actually simpler: if currentFunc has instructions and isn't in 'functions' yet
       // But my loop adds to 'functions' on 'ret'. 
       // If loop finishes without 'ret', we have a dangling function.
       currentFunc.endAddress = currentFunc.instructions.back().address;
       functions.push_back(currentFunc);
    } else if (functions.empty() && !currentFunc.instructions.empty()) {
        currentFunc.endAddress = currentFunc.instructions.back().address;
        functions.push_back(currentFunc);
    }
    
    return functions;
}

std::unordered_map<std::string, uint64_t> NativeDisassembler::AnalyzeImports(const std::string& filePath) {
    std::unordered_map<std::string, uint64_t> importsMap;
    auto info = BinaryAnalyzer::AnalyzePE(filePath);
    for (const auto& imp : info.imports) {
        importsMap[imp.dllName + "::" + imp.functionName] = imp.address;
    }
    return importsMap;
}

std::unordered_map<std::string, uint64_t> NativeDisassembler::AnalyzeExports(const std::string& filePath) {
    std::unordered_map<std::string, uint64_t> exportsMap;
    auto info = BinaryAnalyzer::AnalyzePE(filePath);
    for (const auto& exp : info.exports) {
        exportsMap[exp.name] = exp.address;
    }
    return exportsMap;
}

// ===============================================================
// BINARY ANALYZER IMPLEMENTATION  
// ===============================================================

BinaryAnalyzer::BinaryInfo BinaryAnalyzer::AnalyzePE(const std::string& filePath) {
    BinaryInfo info;
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return info;
    
    // Read DOS header
    IMAGE_DOS_HEADER dosHeader;
    file.read(reinterpret_cast<char*>(&dosHeader), sizeof(dosHeader));
    
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        return info; // Not a PE file
    }
    
    // Seek to PE header
    file.seekg(dosHeader.e_lfanew);
    
    // Read PE signature
    uint32_t peSignature;
    file.read(reinterpret_cast<char*>(&peSignature), sizeof(peSignature));
    
    if (peSignature != IMAGE_NT_SIGNATURE) {
        return info; // Invalid PE
    }
    
    // Read file header
    IMAGE_FILE_HEADER fileHeader;
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    
    // Determine architecture
    switch (fileHeader.Machine) {
        case IMAGE_FILE_MACHINE_AMD64:
            info.architecture = "x64";
            break;
        case IMAGE_FILE_MACHINE_I386:
            info.architecture = "x86";
            break;
        case IMAGE_FILE_MACHINE_ARM64:
            info.architecture = "ARM64";
            break;
        default:
            info.architecture = "Unknown";
    }
    
    // Read optional header (x64 version)
    IMAGE_OPTIONAL_HEADER64 optHeader;
    file.read(reinterpret_cast<char*>(&optHeader), sizeof(optHeader));
    
    info.entryPoint = optHeader.AddressOfEntryPoint;
    info.imageBase = optHeader.ImageBase;
    
    // Read sections
    for (int i = 0; i < fileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER sectionHeader;
        file.read(reinterpret_cast<char*>(&sectionHeader), sizeof(sectionHeader));
        
        Section section;
        section.name = std::string(reinterpret_cast<char*>(sectionHeader.Name), 8);
        section.virtualAddress = sectionHeader.VirtualAddress;
        section.virtualSize = sectionHeader.Misc.VirtualSize;
        section.rawSize = sectionHeader.SizeOfRawData;
        section.characteristics = sectionHeader.Characteristics;
        
        // Build permissions string
        section.permissions = "";
        if (sectionHeader.Characteristics & IMAGE_SCN_MEM_READ) section.permissions += "R";
        if (sectionHeader.Characteristics & IMAGE_SCN_MEM_WRITE) section.permissions += "W";
        if (sectionHeader.Characteristics & IMAGE_SCN_MEM_EXECUTE) section.permissions += "X";
        
        info.sections.push_back(section);
    }
    
    return info;
}

std::string BinaryAnalyzer::GenerateReport(const BinaryInfo& info) {
    std::stringstream report;
    
    report << "=== BINARY ANALYSIS REPORT ===\n";
    report << "Architecture: " << info.architecture << "\n";
    report << "Entry Point: 0x" << std::hex << info.entryPoint << "\n";
    report << "Image Base: 0x" << std::hex << info.imageBase << "\n";
    report << "\nSections:\n";
    
    for (const auto& section : info.sections) {
        report << "  " << section.name << "\n";
        report << "    Virtual Address: 0x" << std::hex << section.virtualAddress << "\n";
        report << "    Virtual Size: 0x" << std::hex << section.virtualSize << "\n";
        report << "    Permissions: " << section.permissions << "\n";
    }
    
    if (!info.imports.empty()) {
        report << "\nImports:\n";
        for (const auto& imp : info.imports) {
            report << "  " << imp.dllName << "!" << imp.functionName << "\n";
        }
    }
    
    return report.str();
}

std::vector<uint8_t> BinaryAnalyzer::ExtractSection(const std::string& filePath, const std::string& sectionName) {
    std::vector<uint8_t> data;
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return data;

    IMAGE_DOS_HEADER dosHeader;
    file.read(reinterpret_cast<char*>(&dosHeader), sizeof(dosHeader));
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) return data;

    file.seekg(dosHeader.e_lfanew);
    uint32_t peSignature;
    file.read(reinterpret_cast<char*>(&peSignature), sizeof(peSignature));
    if (peSignature != IMAGE_NT_SIGNATURE) return data;

    IMAGE_FILE_HEADER fileHeader;
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));

    // Skip Optional Header
    file.seekg(fileHeader.SizeOfOptionalHeader, std::ios::cur);

    for (int i = 0; i < fileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER sectionHeader;
        file.read(reinterpret_cast<char*>(&sectionHeader), sizeof(sectionHeader));
        
        char nameBuffer[9] = {0};
        memcpy(nameBuffer, sectionHeader.Name, 8);
        std::string currentName = nameBuffer;
        
        if (currentName == sectionName) {
            data.resize(sectionHeader.SizeOfRawData);
            file.seekg(sectionHeader.PointerToRawData);
            file.read(reinterpret_cast<char*>(data.data()), sectionHeader.SizeOfRawData);
            break;
        }
    }
    return data;
}

// ===============================================================
// RE CODEX IMPLEMENTATION
// ===============================================================

std::vector<RECodex::Pattern> RECodex::GetCommonPatterns() {
    std::vector<Pattern> patterns;
    
    // Windows API call pattern
    Pattern apiCall;
    apiCall.name = "Windows API Call";
    apiCall.signature = {0xFF, 0x15}; // call [rip+offset]
    apiCall.mask = {0xFF, 0xFF};
    apiCall.description = "Indirect call to Windows API function";
    patterns.push_back(apiCall);
    
    // Function prologue
    Pattern prologue;
    prologue.name = "Function Prologue";
    return "AI analysis would be performed here using the agent's inference capabilities.";
}

std::vector<RECodex::Pattern> RECodex::GetMalwarePatterns() {
    std::vector<Pattern> patterns;
    // Example pattern: Shellcode stub
    Pattern shellcode;
    shellcode.name = "Generic Shellcode Loop";
    shellcode.signature = {0xEB, 0xFE}; // jmp $
    shellcode.mask = {0xFF, 0xFF};
    shellcode.description = "Infinite loop often used in shellcode placeholders";
    patterns.push_back(shellcode);
    return patterns;
}

std::vector<RECodex::Pattern> RECodex::GetCompilerPatterns() {
    std::vector<Pattern> patterns;
    // Example: MSVC Prologue
    Pattern msvc;
    msvc.name = "MSVC Prologue";
    msvc.signature = {0x55, 0x8B, 0xEC}; // push ebp; mov ebp, esp
    msvc.mask = {0xFF, 0xFF, 0xFF};
    msvc.description = "Standard MSVC 32-bit Prologue";
    patterns.push_back(msvc);
    return patterns;
}

std::vector<std::pair<uint64_t, std::string>> RECodex::ScanForPatterns(
    const uint8_t* data, size_t size, const std::vector<Pattern>& patterns) {
    std::vector<std::pair<uint64_t, std::string>> results;
    
    for (const auto& pattern : patterns) {
        if (pattern.signature.empty() || size < pattern.signature.size()) continue;
        
        for (size_t i = 0; i <= size - pattern.signature.size(); ++i) {
            bool match = true;
            for (size_t j = 0; j < pattern.signature.size(); ++j) {
                if (pattern.mask.size() > j && pattern.mask[j] == 0x00) continue; // Wildcard
                if (data[i + j] != pattern.signature[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                results.push_back({(uint64_t)i, pattern.name});
            }
        }
    }
    return results;
}

// ===============================================================
    
    // Exception handler setup
    Pattern exception;
    exception.name = "SEH Setup";
    exception.signature = {0x64, 0x48, 0x89, 0x25}; // mov fs:[offset], rsp
    exception.mask = {0xFF, 0xFF, 0xFF, 0xFF};
    exception.description = "Structured exception handler setup";
    patterns.push_back(exception);
    
    return patterns;
}

std::vector<RECodex::KnownFunction> RECodex::GetWindowsAPIDatabase() {
    std::vector<KnownFunction> functions;
    
    KnownFunction createFile;
    createFile.name = "CreateFileA";
    createFile.library = "kernel32.dll";
    createFile.parameters = {"LPCSTR", "DWORD", "DWORD", "LPSECURITY_ATTRIBUTES", "DWORD", "DWORD", "HANDLE"};
    createFile.description = "Creates or opens a file or I/O device";
    functions.push_back(createFile);
    
    KnownFunction writeFile;
    writeFile.name = "WriteFile";
    writeFile.library = "kernel32.dll";
    writeFile.parameters = {"HANDLE", "LPCVOID", "DWORD", "LPDWORD", "LPOVERLAPPED"};
    writeFile.description = "Writes data to a file";
    functions.push_back(writeFile);
    return file.good();
}

std::vector<uint8_t> NativeCompiler::GenerateShellcode(const std::string& payload, const std::string& arch) {
    std::vector<uint8_t> shellcode;
    // Placeholder implementation for shellcode generation
    // Real implementation would compile 'payload' assembly or C code into position-independent code
    
    // NOP sled
    for(int i=0; i<16; i++) shellcode.push_back(0x90);
    
    // Add dummy payload (int3 breakdown)
    shellcode.push_back(0xCC);
    
    return shellcode;
}

} // namespace ReverseEngineering
std::string RECodex::AnalyzeWithAI(const std::string& assembly, const std::string& context) {
    // This would integrate with the RawrXD agent for AI-powered analysis
    return "AI analysis would be performed here using the agent's inference capabilities.";
}

// ===============================================================
// NATIVE COMPILER IMPLEMENTATION
// ===============================================================

NativeCompiler::CompileResult NativeCompiler::CompileToNative(
    const std::string& sourceCode, const CompileOptions& options) {
    
    CompileResult result;
    result.success = false;
    
    // In a full implementation, this would:
    // 1. Parse the C++ source
    // 2. Generate intermediate representation
    // 3. Optimize the IR
    // 4. Generate machine code
    
    // For now, return a simple example
    if (sourceCode.find("int main") != std::string::npos) {
        // Simple main function - generate basic prologue/epilogue
        result.machineCode = {
            0x55,                           // push rbp
            0x48, 0x89, 0xE5,               // mov rbp, rsp
            0x48, 0x83, 0xEC, 0x10,         // sub rsp, 10h
            0xB8, 0x00, 0x00, 0x00, 0x00,   // mov eax, 0
            0x48, 0x83, 0xC4, 0x10,         // add rsp, 10h
            0x5D,                           // pop rbp
            0xC3                            // ret
        };
        result.assembly = "push rbp\nmov rbp, rsp\nsub rsp, 10h\nmov eax, 0\nadd rsp, 10h\npop rbp\nret";
        result.success = true;
    }
    
    return result;
}

NativeCompiler::CompileResult NativeCompiler::CompileAssembly(
    const std::string& assembly, const CompileOptions& options) {
    
    CompileResult result;
    result.success = false;
    
    // Simple assembler for basic instructions
    std::istringstream lines(assembly);
    std::string line;
    
    while (std::getline(lines, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line == "nop") {
            result.machineCode.push_back(0x90);
        } else if (line == "ret") {
            result.machineCode.push_back(0xC3);
        } else if (line == "int3") {
            result.machineCode.push_back(0xCC);
        } else if (line == "push rbp") {
            result.machineCode.push_back(0x55);
        } else if (line == "pop rbp") {
            result.machineCode.push_back(0x5D);
        }
        // Add more instruction mappings as needed
    }
    
    result.success = !result.machineCode.empty();
    result.assembly = assembly;
    
    return result;
}

bool NativeCompiler::PatchBinary(const std::string& filePath, uint64_t offset, const std::vector<uint8_t>& newCode) {
    std::fstream file(filePath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) return false;
    
    file.seekp(offset);
    file.write(reinterpret_cast<const char*>(newCode.data()), newCode.size());
    
    return file.good();
}

} // namespace ReverseEngineering
} // namespace RawrXD