#include "../ReverseEngineeringSuite.hpp"
#include "Win32IDE.h"
#include <sstream>

// Reverse Engineering Command Implementations

void Win32IDE::analyzeBinaryWithCodex() {
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Executable Files\0*.exe;*.dll;*.sys\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select Binary to Analyze";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        appendToOutput("Analyzing binary with CodexUltimate: " + std::string(filename), "ReverseEng", OutputSeverity::Info);
        
        std::thread([this, filename]() {
            auto result = RawrXD::CodexAnalyzer::Analyze(filename);
            
            std::stringstream ss;
            ss << "\n=== CODEX ANALYSIS RESULTS ===\n";
            ss << "File: " << filename << "\n";
            ss << "Format: " << result.format << "\n";
            ss << "Architecture: " << result.architecture << "\n";
            ss << "Packed: " << (result.isPacked ? "Yes" : "No") << "\n";
            
            if (!result.pdbPath.empty()) {
                ss << "PDB Path: " << result.pdbPath << "\n";
            }
            
            if (result.entropy > 0) {
                ss << "Entropy: " << result.entropy << " (higher = more packed/encrypted)\n";
            }
            
            if (!result.imports.empty()) {
                ss << "\nImports (" << result.imports.size() << "):\n";
                for (size_t i = 0; i < std::min(result.imports.size(), size_t(20)); i++) {
                    ss << "  " << result.imports[i] << "\n";
                }
                if (result.imports.size() > 20) {
                    ss << "  ... (" << (result.imports.size() - 20) << " more)\n";
                }
            }
            
            if (!result.exports.empty()) {
                ss << "\nExports (" << result.exports.size() << "):\n";
                for (size_t i = 0; i < std::min(result.exports.size(), size_t(20)); i++) {
                    ss << "  " << result.exports[i] << "\n";
                }
                if (result.exports.size() > 20) {
                    ss << "  ... (" << (result.exports.size() - 20) << " more)\n";
                }
            }
            
            ss << "\nFull Output:\n" << result.summary << "\n";
            ss << "==============================\n";
            
            appendToOutput(ss.str(), "ReverseEng", result.success ? OutputSeverity::Info : OutputSeverity::Error);
        }).detach();
    }
}

void Win32IDE::dumpBinaryHeaders() {
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "PE Files\0*.exe;*.dll;*.sys\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select Binary";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        appendToOutput("Dumping PE headers: " + std::string(filename), "DumpBin", OutputSeverity::Info);
        
        std::thread([this, filename]() {
            std::string output = RawrXD::DumpBinAnalyzer::DumpHeaders(filename);
            appendToOutput("\n=== PE HEADERS ===\n" + output + "\n==================\n", "DumpBin", OutputSeverity::Info);
        }).detach();
    }
}

void Win32IDE::dumpBinaryImports() {
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "PE Files\0*.exe;*.dll;*.sys\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select Binary";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        appendToOutput("Dumping imports: " + std::string(filename), "DumpBin", OutputSeverity::Info);
        
        std::thread([this, filename]() {
            std::string output = RawrXD::DumpBinAnalyzer::DumpImports(filename);
            appendToOutput("\n=== IMPORTS ===\n" + output + "\n===============\n", "DumpBin", OutputSeverity::Info);
        }).detach();
    }
}

void Win32IDE::dumpBinaryExports() {
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "PE Files\0*.exe;*.dll;*.sys\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select Binary";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        appendToOutput("Dumping exports: " + std::string(filename), "DumpBin", OutputSeverity::Info);
        
        std::thread([this, filename]() {
            std::string output = RawrXD::DumpBinAnalyzer::DumpExports(filename);
            appendToOutput("\n=== EXPORTS ===\n" + output + "\n===============\n", "DumpBin", OutputSeverity::Info);
        }).detach();
    }
}

void Win32IDE::disassembleBinary() {
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Executable Files\0*.exe;*.dll;*.sys\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select Binary to Disassemble";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        appendToOutput("Disassembling: " + std::string(filename), "Disasm", OutputSeverity::Info);
        
        std::thread([this, filename]() {
            std::string output = RawrXD::CodexAnalyzer::Disassemble(filename, 0, 1024);
            appendToOutput("\n=== DISASSEMBLY ===\n" + output + "\n===================\n", "Disasm", OutputSeverity::Info);
        }).detach();
    }
}

void Win32IDE::compileAssemblyFile() {
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Assembly Files\0*.asm\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select Assembly Source";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        appendToOutput("Compiling: " + std::string(filename), "Compiler", OutputSeverity::Info);
        
        std::thread([this, filename]() {
            auto result = RawrXD::RawrXDCompiler::CompileASM(filename);
            
            std::stringstream ss;
            ss << "\n=== COMPILATION RESULTS ===\n";
            ss << result.output << "\n";
            
            if (result.success) {
                ss << "Success! Object file: " << result.objectFile << "\n";
            } else {
                ss << "Compilation failed.\n";
                for (const auto& err : result.errors) {
                    ss << "ERROR: " << err << "\n";
                }
                for (const auto& warn : result.warnings) {
                    ss << "WARNING: " << warn << "\n";
                }
            }
            ss << "===========================\n";
            
            appendToOutput(ss.str(), "Compiler", result.success ? OutputSeverity::Info : OutputSeverity::Error);
        }).detach();
    }
}
