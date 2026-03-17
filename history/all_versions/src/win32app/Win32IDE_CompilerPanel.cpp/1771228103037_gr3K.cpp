// Win32IDE_CompilerPanel.cpp - LIVE COMPILER OUTPUT DISPLAY
// Shows real-time compiler/dumpbin output instead of stubs

#include "Win32IDE.h"
#include "../compiler/compiler_cpp_real.cpp"
#include "../compiler/compiler_asm_real.cpp"
#include <richedit.h>

using namespace RawrXD::Compiler;

// Display live compiler output inpanel
void Win32IDE::ShowCompilerOutput() {
    if (!m_hwndTerminal) return;
    
    // Clear terminal
    SetWindowTextA(m_hwndTerminal, "");
    
    // Get active file
    std::string activeFile = getCurrentFilePath();
    if (activeFile.empty()) {
        AppendTerminalText("❌ No file open\r\n");
        return;
    }
    
    AppendTerminalText("═══════════════════════════════════════════════\r\n");
    AppendTerminalText("  FORTRESS COMPILER - LIVE OUTPUT\r\n");
    AppendTerminalText("═══════════════════════════════════════════════\r\n\r\n");
    
    // Check extension
    bool is_asm = (activeFile.rfind(".asm") == activeFile.length() - 4);
    bool is_cpp = (activeFile.rfind(".cpp") == activeFile.length() - 4) ||
                  (activeFile.rfind(".c") == activeFile.length() - 2);
    
    if (is_cpp) {
        AppendTerminalText("🔨 C++ Compilation Starting...\r\n");
        AppendTerminalText("File: " + activeFile + "\r\n\r\n");
        
        CppCompilerImpl compiler;
        CompileOptions opts;
        opts.standard = "c++20";
        opts.optimization = OptimizationLevel::O2;
        opts.debug_info = true;
        opts.include_paths = {"d:\\rawrxd\\include", "d:\\rawrxd\\src"};
        
        CompileResult result = compiler.compile_file(activeFile, opts);
        
        AppendTerminalText("Command: " + result.command_line + "\r\n\r\n");
        AppendTerminalText(result.output);
        
        if (result.success) {
            AppendTerminalText("\r\n✅ SUCCESS - Object file: " + result.output_file + "\r\n");
            AppendTerminalText("⏱️  Duration: " + std::to_string(result.duration_ms) + "ms\r\n");
        } else {
            AppendTerminalText("\r\n❌ FAILED\r\n");
            AppendTerminalText("Error: " + result.error_message + "\r\n");
        }
        
        // Show diagnostics
        if (!result.diagnostics.empty()) {
            AppendTerminalText("\r\n📋 Diagnostics (" + std::to_string(result.diagnostics.size()) + "):\r\n");
            for (const auto& diag : result.diagnostics) {
                std::string severity;
                switch (diag.severity) {
                    case DiagnosticSeverity::Error: severity = "❌ ERROR"; break;
                    case DiagnosticSeverity::Warning: severity = "⚠️  WARN"; break;
                    case DiagnosticSeverity::Information: severity = "ℹ️  INFO"; break;
                    case DiagnosticSeverity::Hint: severity = "💡 HINT"; break;
                }
                
                char buf[512];
                sprintf_s(buf, "%s %s(%d,%d): %s %s\r\n",
                    severity.c_str(),
                    diag.file.c_str(),
                    diag.line,
                    diag.column,
                    diag.code.c_str(),
                    diag.message.c_str());
                AppendTerminalText(buf);
            }
        }
        
    } else if (is_asm) {
        AppendTerminalText("⚙️  Assembly Compilation Starting...\r\n");
        AppendTerminalText("File: " + activeFile + "\r\n\r\n");
        
        AsmCompilerImpl compiler;
        AsmCompileOptions opts;
        opts.generate_listing = true;
        opts.generate_debug_info = true;
        opts.warning_level = 3;
        opts.include_paths = {"d:\\rawrxd\\include", "d:\\rawrxd\\src\\asm"};
        
        AsmCompileResult result = compiler.compile_asm(activeFile, opts);
        
        AppendTerminalText("Command: " + result.command_line + "\r\n\r\n");
        AppendTerminalText(result.output);
        
        if (result.success) {
            AppendTerminalText("\r\n✅ SUCCESS\r\n");
            AppendTerminalText("Object: " + result.output_file + "\r\n");
            if (!result.listing_file.empty()) {
                AppendTerminalText("Listing: " + result.listing_file + "\r\n");
            }
            AppendTerminalText("⏱️  Duration: " + std::to_string(result.duration_ms) + "ms\r\n");
        } else {
            AppendTerminalText("\r\n❌ FAILED\r\n");
            AppendTerminalText("Error: " + result.error_message + "\r\n");
        }
        
    } else {
        AppendTerminalText("❌ Unknown file type: " + activeFile + "\r\n");
        AppendTerminalText("Supported: .cpp, .c, .asm\r\n");
    }
    
    AppendTerminalText("\r\n═══════════════════════════════════════════════\r\n");
}

// Display dumpbin output for active file
void Win32IDE::ShowDumpbinOutput() {
    if (!m_hwndTerminal) return;
    
    std::string activeFile = getCurrentFilePath();
    if (activeFile.empty()) {
        AppendTerminalText("❌ No file open\r\n");
        return;
    }
    
    // Check if PE/obj file
    bool is_binary = (activeFile.rfind(".exe") == activeFile.length() - 4) ||
                     (activeFile.rfind(".dll") == activeFile.length() - 4) ||
                     (activeFile.rfind(".obj") == activeFile.length() - 4);
    
    if (!is_binary) {
        AppendTerminalText("❌ Not a binary file\r\n");
        AppendTerminalText("Dumpbin requires: .exe, .dll, or .obj\r\n");
        return;
    }
    
    SetWindowTextA(m_hwndTerminal, "");
    AppendTerminalText("═══════════════════════════════════════════════\r\n");
    AppendTerminalText("  DUMPBIN - PE/COFF ANALYZER\r\n");
    AppendTerminalText("═══════════════════════════════════════════════\r\n\r\n");
    AppendTerminalText("File: " + activeFile + "\r\n\r\n");
    
    // Use INTERNAL PE parser - NO EXTERNAL DUMPBIN REQUIRED
    AppendTerminalText("Using Internal PE/COFF Parser (No SDK Required)\r\n\r\n");
    
    // Parse PE headers internally
    std::string pe_info = parse_pe_headers_internal(activeFile);
    AppendTerminalText(pe_info);
    
    AppendTerminalText("\r\n═══════════════════════════════════════════════\r\n");
    return;
    
    // Execute and capture output
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    
    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    
    if (CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, TRUE,
                       0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hWritePipe);
        
        char buffer[4096];
        DWORD bytesRead;
        
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            AppendTerminalText(buffer);
        }
        
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        AppendTerminalText("❌ Failed to execute dumpbin\r\n");
        if (rawrxd_dumpbin && rawrxd_dumpbin[0])
            AppendTerminalText("Check RAWRXD_DUMPBIN path or use custom toolchain.\r\n");
        else
            AppendTerminalText("Set RAWRXD_DUMPBIN to your custom dumpbin or install Build Tools.\r\n");
    }
    
    CloseHandle(hReadPipe);
    
    AppendTerminalText("\r\n═══════════════════════════════════════════════\r\n");
}

// INTERNAL PE PARSER - NO DUMPBIN.EXE REQUIRED
std::string parse_pe_headers_internal(const std::string& filepath) {
    std::stringstream output;
    
    HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        output << "❌ Failed to open file\\r\\n";
        return output.str();
    }
    
    DWORD fileSize = GetFileSize(hFile, NULL);
    BYTE* fileData = new BYTE[fileSize];
    DWORD bytesRead;
    ReadFile(hFile, fileData, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);
    
    if (bytesRead < sizeof(IMAGE_DOS_HEADER)) {
        delete[] fileData;
        output << "❌ File too small to be valid PE\\r\\n";
        return output.str();
    }
    
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)fileData;
    
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        delete[] fileData;
        output << "❌ Not a valid DOS/PE file (bad MZ signature)\\r\\n";
        return output.str();
    }
    
    if (dosHeader->e_lfanew >= fileSize) {
        delete[] fileData;
        output << "❌ Invalid PE header offset\\r\\n";
        return output.str();
    }
    
    IMAGE_NT_HEADERS64* ntHeaders = (IMAGE_NT_HEADERS64*)(fileData + dosHeader->e_lfanew);
    
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        delete[] fileData;
        output << "❌ Invalid PE signature\\r\\n";
        return output.str();
    }
    
    output << "✅ Valid PE/COFF File\\r\\n\\r\\n\";\n    output << \"FILE HEADER\\r\\n\";\n    output << \"────────────────────────────────────────────\\r\\n\";\n    output << \"Machine:              \";\n    \n    switch (ntHeaders->FileHeader.Machine) {\n        case IMAGE_FILE_MACHINE_AMD64: output << \"x64 (AMD64)\\r\\n\"; break;\n        case IMAGE_FILE_MACHINE_I386: output << \"x86 (I386)\\r\\n\"; break;\n        case IMAGE_FILE_MACHINE_ARM64: output << \"ARM64\\r\\n\"; break;\n        default: output << \"Unknown (\" << std::hex << ntHeaders->FileHeader.Machine << std::dec << \")\\r\\n\"; break;\n    }\n    \n    output << \"Sections:             \" << ntHeaders->FileHeader.NumberOfSections << \"\\r\\n\";\n    output << \"Timestamp:            \" << ntHeaders->FileHeader.TimeDateStamp << \"\\r\\n\";\n    output << \"Characteristics:      0x\" << std::hex << ntHeaders->FileHeader.Characteristics << std::dec << \"\\r\\n\\r\\n\";\n    \n    output << \"OPTIONAL HEADER\\r\\n\";\n    output << \"────────────────────────────────────────────\\r\\n\";\n    output << \"Magic:                0x\" << std::hex << ntHeaders->OptionalHeader.Magic << std::dec;\n    output << (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC ? \" (PE32+)\" : \" (PE32)\") << \"\\r\\n\";\n    output << \"Entry Point:          0x\" << std::hex << ntHeaders->OptionalHeader.AddressOfEntryPoint << std::dec << \"\\r\\n\";\n    output << \"Image Base:           0x\" << std::hex << ntHeaders->OptionalHeader.ImageBase << std::dec << \"\\r\\n\";\n    output << \"Subsystem:            \";\n    \n    switch (ntHeaders->OptionalHeader.Subsystem) {\n        case IMAGE_SUBSYSTEM_WINDOWS_GUI: output << \"Windows GUI\\r\\n\"; break;\n        case IMAGE_SUBSYSTEM_WINDOWS_CUI: output << \"Console\\r\\n\"; break;\n        default: output << \"Other\\r\\n\"; break;\n    }\n    \n    output << \"\\r\\nSECTIONS\\r\\n\";\n    output << \"────────────────────────────────────────────\\r\\n\";\n    \n    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(ntHeaders);\n    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {\n        output << \"Section \" << (i + 1) << \": \";\n        output << std::string((char*)section[i].Name, 8) << \"\\r\\n\";\n        output << \"  Virtual Size:       0x\" << std::hex << section[i].Misc.VirtualSize << std::dec << \"\\r\\n\";\n        output << \"  Virtual Address:    0x\" << std::hex << section[i].VirtualAddress << std::dec << \"\\r\\n\";\n        output << \"  Raw Data Size:      0x\" << std::hex << section[i].SizeOfRawData << std::dec << \"\\r\\n\";\n        output << \"  Characteristics:    0x\" << std::hex << section[i].Characteristics << std::dec << \"\\r\\n\\r\\n\";\n    }\n    \n    delete[] fileData;\n    return output.str();\n}\n\nvoid Win32IDE::AppendTerminalText(const std::string& text) {\n    if (!m_hwndTerminal) return;\n    \n    int len = GetWindowTextLengthA(m_hwndTerminal);\n    SendMessageA(m_hwndTerminal, EM_SETSEL, len, len);\n    SendMessageA(m_hwndTerminal, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());\n}\n\nstd::string Win32IDE::getCurrentFilePath() const {\n    // Return current file path (stub - implement based on your editor state)\n    if (m_currentFilePath.empty()) {\n        return \"\";"}
    }
    return m_currentFilePath;
}
