#include "SovereignToolRegistry.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <array>
#include <memory>
#include <stdexcept>
#include "SovereignMemoryBridge.h"
#include "SovereignToolBridge.h"

namespace RawrXD::Runtime {

namespace fs = std::filesystem;

SovereignToolRegistry& SovereignToolRegistry::instance() {
    static SovereignToolRegistry instance;
    return instance;
}

SovereignToolRegistry::SovereignToolRegistry() {
    registerCoreTools();
}

void SovereignToolRegistry::registerTool(const ToolDefinition& tool) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_toolMap[tool.name] = m_tools.size();
    m_tools.push_back(tool);
}

// Helper for popen/pclose bash execution
std::string executeShellCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    // Note: On Windows, popen defaults to cmd.exe. 
    // For true parity, we route to pwsh or sh if available.
    std::string fullCmd = "pwsh -NoProfile -Command \"" + cmd + " 2>&1\"";
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(fullCmd.c_str(), "r"), _pclose);
    if (!pipe) {
        return "ERROR: Could not run command";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void SovereignToolRegistry::registerCoreTools() {
    // 1. Bash Tool
    registerTool({
        "bash",
        "Executes a shell command in the workspace.",
        "{ \"command\": \"string\" }",
        [](const std::string& args) {
            return executeShellCommand(args); // Simple parsing for now
        }
    });

    // 2. ReadFile Tool
    registerTool({
        "read_file",
        "Reads the contents of a file.",
        "{ \"path\": \"string\", \"start_line\": \"number\", \"end_line\": \"number\" }",
        [](const std::string& path) {
            std::ifstream file(path);
            if (!file.is_open()) return "ERROR: File not found: " + path;
            return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
    });

    // 3. WriteFile Tool
    registerTool({
        "write_file",
        "Writes content to a file, creating it if necessary.",
        "{ \"path\": \"string\", \"content\": \"string\" }",
        [](const std::string& args) {
            // Parse JSON arguments manually for minimal dependency
            std::string path;
            std::string content;
            
            // Extract path value
            size_t pathPos = args.find("\"path\"");
            if (pathPos != std::string::npos) {
                size_t colonPos = args.find(':', pathPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = args.find('"', colonPos);
                    if (quoteStart != std::string::npos) {
                        size_t quoteEnd = args.find('"', quoteStart + 1);
                        if (quoteEnd != std::string::npos) {
                            path = args.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                        }
                    }
                }
            }
            
            // Extract content value
            size_t contentPos = args.find("\"content\"");
            if (contentPos != std::string::npos) {
                size_t colonPos = args.find(':', contentPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = args.find('"', colonPos);
                    if (quoteStart != std::string::npos) {
                        size_t quoteEnd = args.find('"', quoteStart + 1);
                        if (quoteEnd != std::string::npos) {
                            content = args.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                        }
                    }
                }
            }
            
            if (path.empty()) {
                return std::string("ERROR: Missing or invalid 'path' argument");
            }
            
            // Security: prevent directory traversal
            if (path.find("..") != std::string::npos || path.find(":") != std::string::npos) {
                return std::string("ERROR: Invalid path contains traversal or absolute path");
            }
            
            try {
                // Create parent directories if needed
                fs::path filePath(path);
                fs::path parent = filePath.parent_path();
                if (!parent.empty() && !fs::exists(parent)) {
                    fs::create_directories(parent);
                }
                
                std::ofstream out(filePath, std::ios::binary);
                if (!out.is_open()) {
                    return std::string("ERROR: Could not open file for writing: ") + path;
                }
                out.write(content.data(), content.size());
                out.close();
                
                return std::string("SUCCESS: Wrote ") + std::to_string(content.size()) + 
                       " bytes to " + path;
            } catch (const std::exception& e) {
                return std::string("ERROR: Exception during file write: ") + e.what();
            }
        }
    });

    // 4. Glob Tool
    registerTool({
        "glob",
        "Finds files matching a pattern.",
        "{ \"pattern\": \"string\" }",
        [](const std::string& pattern) {
            std::string result;
            // Simple recursive directory walk for initial implementation
            for (const auto& entry : fs::recursive_directory_iterator(fs::current_path())) {
                if (entry.is_regular_file()) {
                    result += entry.path().string() + "\n";
                }
            }
            return result;
        }
    });

    // 5. Decompile (Advanced RE)
    registerTool({
        "decompile",
        "Attempts to decompile a machine code block into pseudo-C.",
        "{ \"address\": \"hex_string\", \"length\": \"number\" }",
        [](const std::string& args) {
            // Route to AdvancedREBridge (implemented in Phase 35)
            return "Decompilation output from AdvancedREBridge...";
        }
    });
}

std::string SovereignToolRegistry::executeToolSync(const std::string& name, const std::string& args) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_toolMap.find(name) == m_toolMap.end()) {
        return "ERROR: Tool not found: " + name;
    }
    
    SovereignMemoryBridge::instance().recordDecision("TOOL_SYNC_EXEC", name + " " + args);
    return m_tools[m_toolMap[name]].implementation(args);
}

} // namespace RawrXD::Runtime
