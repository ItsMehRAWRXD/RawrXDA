#include "codex_ultimate.h"
#include "../reverse_engineering/RawrCodex.hpp"
#include "../reverse_engineering/RawrDumpBin.hpp"
#include "../reverse_engineering/RawrCompiler.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#include <iostream>
#include <filesystem>

// SCAFFOLD_256: Codex ultimate module


// SCAFFOLD_126: modules/codex_ultimate


using namespace RawrXD::ReverseEngineering;

std::unique_ptr<CodexUltimate> g_codex;

std::vector<DisassemblyResult> CodexUltimate::Disassemble(const std::string& binary_path, 
                                                         uint64_t start_addr, 
                                                         size_t length) {
    std::vector<DisassemblyResult> results;
    RawrCodex codex;
    
    if (!codex.LoadBinary(binary_path)) {
        return results;
    }

    // Auto-detect code section if start_addr is 0
    if (start_addr == 0) {
        auto sections = codex.GetSections();
        for(const auto& s : sections) {
             if (s.characteristics & 0x20000000) { // MEM_EXECUTE
                 start_addr = s.virtualAddress;
                 break;
             }
        }
    }

    // Disassemble using RawrCodex
    // Estimate instruction count (avg 4 bytes)
    size_t instr_count = length / 4;
    if (instr_count == 0) instr_count = 100;
    
    auto lines = codex.Disassemble(start_addr, instr_count);
    
    for (const auto& line : lines) {
        DisassemblyResult res;
        std::stringstream ss;
        ss << "0x" << std::hex << line.address;
        res.address = ss.str();
        
        std::stringstream bytes_ss;
        for (auto b : line.bytes) {
            bytes_ss << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
        }
        res.bytes = bytes_ss.str();
        
        res.instruction = line.mnemonic;
        res.operands = line.operands;
        res.comment = line.comment;
        
        results.push_back(res);
    }
    
    return results;
}

PEHeaderInfo CodexUltimate::DumpPE(const std::string& pe_file) {
    PEHeaderInfo info;
    
    // Load PE file
    HANDLE hFile = CreateFileA(pe_file.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return info;
    }
    
    HANDLE hMapping = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping) {
        CloseHandle(hFile);
        return info;
    }
    
    LPVOID pFile = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!pFile) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return info;
    }
    
    // Parse PE headers (simplified)
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)pFile;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        UnmapViewOfFile(pFile);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return info;
    }
    
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)pFile + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        UnmapViewOfFile(pFile);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return info;
    }
    
    // Extract header info
    info.machine = std::to_string(ntHeaders->FileHeader.Machine);
    info.timestamp = std::to_string(ntHeaders->FileHeader.TimeDateStamp);
    info.entry_point = "0x" + std::to_string(ntHeaders->OptionalHeader.AddressOfEntryPoint);
    
    // Extract sections
    PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        info.sections.push_back(std::string((char*)sectionHeader[i].Name));
    }
    
    UnmapViewOfFile(pFile);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    
    return info;
}

std::string CodexUltimate::DumpExports(const std::string& dll_path) {
    RawrDumpBin dumpbin;
    return dumpbin.DumpExports(dll_path);
}

std::string CodexUltimate::DumpImports(const std::string& exe_path) {
    RawrDumpBin dumpbin;
    return dumpbin.DumpImports(exe_path);
}

std::string CodexUltimate::DumpHeaders(const std::string& pe_file) {
    RawrDumpBin dumpbin;
    return dumpbin.DumpHeaders(pe_file);
}

bool CodexUltimate::CompileMASM64(const std::string& asm_file, const std::string& output_obj) {
    RawrCompiler compiler;
    CompilerOptions opts;
    opts.targetArch = "x64";
    compiler.SetOptions(opts);
    
    auto result = compiler.CompileSource(asm_file);
    if (result.success) {
        if (!result.objectFile.empty() && result.objectFile != output_obj) {
             try {
                if (std::filesystem::exists(output_obj)) std::filesystem::remove(output_obj);
                std::filesystem::rename(result.objectFile, output_obj);
             } catch (...) { return false; }
        }
        return true;
    }
    return false;
}

bool CodexUltimate::LinkObject(const std::vector<std::string>& obj_files, const std::string& output_exe) {
    RawrCompiler compiler;
    return compiler.Link(obj_files, output_exe, false);
}

std::string CodexUltimate::AnalyzeBinary(const std::string& binary_path) {
    RawrDumpBin dumpbin;
    return dumpbin.DumpAll(binary_path);
}

std::string CodexUltimate::FindVulnerabilities(const std::string& binary_path) {
    RawrDumpBin dumpbin;
    return dumpbin.DumpVulnerabilities(binary_path);
}

std::string CodexUltimate::GenerateExploit(const std::string& vulnerability) {
    std::string exploit = "# Exploit for: " + vulnerability + "\n\n";
    exploit += "# This is a template - real exploit would require:\n";
    exploit += "# 1. Precise vulnerability analysis\n";
    exploit += "# 2. Target environment details\n";
    exploit += "# 3. Payload generation\n";
    exploit += "# 4. Reliability testing\n\n";
    exploit += "print('Exploit generation is for educational purposes only')\n";
    return exploit;
}

std::string CodexUltimate::GetInstructionInfo(const std::string& instruction) {
    std::unordered_map<std::string, std::string> instruction_info = {
        {"mov", "Move data between registers/memory"},
        {"add", "Add two operands"},
        {"sub", "Subtract two operands"},
        {"jmp", "Unconditional jump"},
        {"call", "Call procedure"},
        {"ret", "Return from procedure"},
        {"push", "Push onto stack"},
        {"pop", "Pop from stack"},
        {"nop", "No operation"},
        {"int3", "Breakpoint interrupt"}
    };
    
    auto it = instruction_info.find(instruction);
    if (it != instruction_info.end()) {
        return it->second;
    }
    return "Unknown instruction";
}

std::string CodexUltimate::GetRegisterState(const std::string& context) {
    return "Register state analysis would require actual debugger context";
}
