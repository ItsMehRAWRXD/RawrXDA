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
    
    // Run dumpbin /headers
    std::string cmd = "dumpbin /headers \"" + activeFile + "\"";
    AppendTerminalText("Command: " + cmd + "\r\n\r\n");
    
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
        AppendTerminalText("Make sure Visual Studio Build Tools are installed\r\n");
    }
    
    CloseHandle(hReadPipe);
    
    AppendTerminalText("\r\n═══════════════════════════════════════════════\r\n");
}

void Win32IDE::AppendTerminalText(const std::string& text) {
    if (!m_hwndTerminal) return;
    
    int len = GetWindowTextLengthA(m_hwndTerminal);
    SendMessageA(m_hwndTerminal, EM_SETSEL, len, len);
    SendMessageA(m_hwndTerminal, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}
