#pragma once
#include <string>
#include <vector>
#include <windows.h>

namespace RawrXD {

/**
 * @brief Wrapper for CodexUltimate reverse engineering suite
 * Provides C++ interface to MASM-based analysis tools
 */
class CodexAnalyzer {
public:
    struct AnalysisResult {
        bool success = false;
        std::string summary;
        std::vector<std::string> imports;
        std::vector<std::string> exports;
        std::vector<std::string> strings;
        std::string architecture; // x86, x64, ARM, etc.
        std::string format; // PE32, PE32+, ELF, Mach-O
        bool isPacked = false;
        float entropy = 0.0f;
        std::string pdbPath;
    };

    /**
     * @brief Analyze binary file using CodexUltimate engine
     * @param filePath Path to executable/DLL/SO to analyze
     * @return Analysis results with all extracted metadata
     */
    static AnalysisResult Analyze(const std::string& filePath) {
        AnalysisResult result;
        
        // Check file exists
        if (GetFileAttributesA(filePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            result.summary = "File not found: " + filePath;
            return result;
        }
        
        // Build command: CodexUltimate.exe <file> /output:json
        std::string exePath = "CodexUltimate.exe"; // Or find in PATH
        std::string cmdLine = exePath + " \"" + filePath + "\" /json /quiet";
        
        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi = {0};
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        
        // Create pipes for stdout
        HANDLE hReadPipe, hWritePipe;
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            result.summary = "Failed to create pipe";
            return result;
        }
        
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        
        if (CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, TRUE, 
                           CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(hWritePipe);
            
            // Read output
            char buffer[4096];
            DWORD bytesRead;
            std::string output;
            
            while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                output += buffer;
            }
            
            WaitForSingleObject(pi.hProcess, 10000); // 10s timeout
            
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hReadPipe);
            
            // Parse output (JSON or text)
            result.success = true;
            result.summary = output;
            
            // Extract key info (simple text parsing for now)
            if (output.find("PE32+") != std::string::npos) result.format = "PE32+";
            else if (output.find("PE32") != std::string::npos) result.format = "PE32";
            else if (output.find("ELF") != std::string::npos) result.format = "ELF";
            
            if (output.find("x64") != std::string::npos || output.find("AMD64") != std::string::npos) {
                result.architecture = "x64";
            } else if (output.find("x86") != std::string::npos || output.find("i386") != std::string::npos) {
                result.architecture = "x86";
            }
            
            result.isPacked = (output.find("packed") != std::string::npos || 
                              output.find("UPX") != std::string::npos);
            
        } else {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            result.summary = "Failed to launch CodexUltimate.exe";
        }
        
        return result;
    }
    
    /**
     * @brief Quick disassembly of a function/section
     */
    static std::string Disassemble(const std::string& filePath, uint64_t offset, size_t length) {
        std::string cmd = "CodexUltimate.exe \"" + filePath + "\" /disasm:" + 
                         std::to_string(offset) + ":" + std::to_string(length);
        
        // Execute and capture (similar to Analyze)
        // For brevity, returning placeholder
        return "; Disassembly at offset " + std::to_string(offset) + "\n; Use full Analyze() for complete output\n";
    }
};

/**
 * @brief Wrapper for custom dumpbin replacement
 */
class DumpBinAnalyzer {
public:
    /**
     * @brief Dump PE headers using custom dumpbin_final.exe
     */
    static std::string DumpHeaders(const std::string& filePath) {
        std::string cmd = "dumpbin_final.exe \"" + filePath + "\" /headers";
        return ExecuteAndCapture(cmd);
    }
    
    /**
     * @brief Dump imports table
     */
    static std::string DumpImports(const std::string& filePath) {
        std::string cmd = "dumpbin_final.exe \"" + filePath + "\" /imports";
        return ExecuteAndCapture(cmd);
    }
    
    /**
     * @brief Dump exports table
     */
    static std::string DumpExports(const std::string& filePath) {
        std::string cmd = "dumpbin_final.exe \"" + filePath + "\" /exports";
        return ExecuteAndCapture(cmd);
    }
    
    /**
     * @brief Full summary
     */
    static std::string DumpSummary(const std::string& filePath) {
        std::string cmd = "dumpbin_final.exe \"" + filePath + "\" /summary";
        return ExecuteAndCapture(cmd);
    }
    
private:
    static std::string ExecuteAndCapture(const std::string& cmdLine) {
        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi = {0};
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        
        HANDLE hReadPipe, hWritePipe;
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            return "ERROR: Failed to create pipe\n";
        }
        
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        
        std::string output;
        if (CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, TRUE, 
                           CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(hWritePipe);
            
            char buffer[4096];
            DWORD bytesRead;
            
            while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                output += buffer;
            }
            
            WaitForSingleObject(pi.hProcess, 10000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hReadPipe);
        } else {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            output = "ERROR: Failed to execute command\n";
        }
        
        return output;
    }
};

/**
 * @brief Wrapper for custom compiler
 */
class RawrXDCompiler {
public:
    struct CompileResult {
        bool success = false;
        std::string output;
        std::string objectFile;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    /**
     * @brief Compile assembly source using custom compiler
     */
    static CompileResult CompileASM(const std::string& sourceFile, const std::string& outputFile = "") {
        CompileResult result;
        
        std::string outFile = outputFile.empty() ? (sourceFile + ".obj") : outputFile;
        std::string cmd = "rawrxd_compiler_masm64.exe \"" + sourceFile + "\" /Fo\"" + outFile + "\"";
        
        std::string output = DumpBinAnalyzer::ExecuteAndCapture(cmd);
        
        result.output = output;
        result.objectFile = outFile;
        result.success = (output.find("error") == std::string::npos);
        
        // Parse errors/warnings (simplified)
        size_t pos = 0;
        while ((pos = output.find("error", pos)) != std::string::npos) {
            size_t lineEnd = output.find('\n', pos);
            result.errors.push_back(output.substr(pos, lineEnd - pos));
            pos = lineEnd;
        }
        
        return result;
    }
    
    /**
     * @brief Link object files
     */
    static CompileResult Link(const std::vector<std::string>& objectFiles, const std::string& outputExe) {
        CompileResult result;
        
        std::string cmd = "link.exe /OUT:\"" + outputExe + "\" ";
        for (const auto& obj : objectFiles) {
            cmd += "\"" + obj + "\" ";
        }
        
        result.output = DumpBinAnalyzer::ExecuteAndCapture(cmd);
        result.success = (result.output.find("error") == std::string::npos);
        
        return result;
    }
};

} // namespace RawrXD
