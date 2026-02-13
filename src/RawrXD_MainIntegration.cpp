// RawrXD_MainIntegration.cpp - THE GLUE CODE
#include "RawrXD_AutonomousCore.hpp"
#include "RawrXD_TerminalIntegration.hpp"
#include "RawrXD_MultiFileEditor.hpp"

// Build system integration - captures and parses compiler diagnostics
class BuildIntegration {
    std::string m_lastOutput;
    std::vector<std::string> m_errors;
    std::string m_buildDir;
public:
    BuildIntegration() {
        char buf[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, buf);
        m_buildDir = buf;
    }
    
    bool RunBuild(const std::string& target = "") {
        m_errors.clear();
        m_lastOutput.clear();
        
        // Determine build command based on available build system
        std::string cmd;
        if (GetFileAttributesA("CMakeLists.txt") != INVALID_FILE_ATTRIBUTES) {
            cmd = "cmake --build build --config Release";
            if (!target.empty()) cmd += " --target " + target;
        } else if (GetFileAttributesA("Makefile") != INVALID_FILE_ATTRIBUTES) {
            cmd = "make";
            if (!target.empty()) cmd += " " + target;
        } else if (GetFileAttributesA("build.bat") != INVALID_FILE_ATTRIBUTES) {
            cmd = "build.bat";
        } else {
            m_errors.push_back("No build system detected (CMakeLists.txt/Makefile/build.bat)");
            return false;
        }
        
        cmd += " 2>&1";
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) {
            m_errors.push_back("Failed to execute build command: " + cmd);
            return false;
        }
        
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line(buffer);
            m_lastOutput += line;
            
            // Parse MSVC error format: file(line): error Cxxxx: message
            if (line.find(": error ") != std::string::npos ||
                line.find(": fatal error ") != std::string::npos) {
                m_errors.push_back(line);
            }
            // Parse GCC/Clang error format: file:line:col: error: message
            else if (line.find(": error:") != std::string::npos) {
                m_errors.push_back(line);
            }
            // Parse linker errors
            else if (line.find("LNK2001") != std::string::npos ||
                     line.find("LNK2019") != std::string::npos ||
                     line.find("undefined reference") != std::string::npos) {
                m_errors.push_back(line);
            }
        }
        
        int exitCode = _pclose(pipe);
        return exitCode == 0;
    }
    
    std::string GetLastErrors() {
        if (m_errors.empty()) return "";
        std::string result;
        for (const auto& err : m_errors) {
            result += err;
            if (result.back() != '\n') result += '\n';
        }
        return result;
    }
    
    std::string GetFullOutput() const { return m_lastOutput; }
    size_t GetErrorCount() const { return m_errors.size(); }
};

class RawrXDAgenticIDE {
    LLMClient llm_;
    ToolExecutionEngine tools_;
    FileSystemTools fs_;
    TerminalIntegration terminal_;
    BuildIntegration build_;
    
    AutonomousOrchestrator orchestrator_;
    MultiFileEditEngine editor_;
    
public:
    RawrXDAgenticIDE() 
        : orchestrator_(llm_, tools_, fs_, terminal_, build_) {}
    
    void Initialize(HWND main_window) {
        editor_.Initialize(main_window);
        
        char current_dir[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, current_dir);
        terminal_.StartTerminalSession(current_dir);
        
        // Wire callbacks (simplified for example)
        /*
        orchestrator_.SetEditCallback([this](auto edits) {
            editor_.LoadEdits(edits);
            editor_.ShowDiffViewer(edits[0]);
        });
        
        orchestrator_.SetTerminalCallback([this](auto cmd) {
            return terminal_.ExecuteBuildCommand(cmd);
        });
        */
    }
    
    // THE ENTRY POINT - "Implement user authentication"
    void ExecuteAgenticTask(const std::string& natural_language) {
        // This is what Cursor does internally
        auto result = orchestrator_.ExecuteAutonomousTask(natural_language);
        
        if (result.find("COMPLETED") == 0) {
            MessageBoxA(nullptr, "Task completed successfully!", "RawrXD Agent", MB_OK);
        } else {
            MessageBoxA(nullptr, result.c_str(), "RawrXD Agent - Failed", MB_ICONERROR);
        }
    }
};

int main() {
    // Basic entry for testing
    RawrXDAgenticIDE ide;
    ide.Initialize(nullptr);
    // ide.ExecuteAgenticTask("Refactor the codebase to use smart pointers.");
    return 0;
}
