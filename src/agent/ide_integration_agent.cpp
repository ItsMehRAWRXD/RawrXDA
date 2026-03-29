// ide_integration_agent.cpp — Autonomous IDE orchestration agent
#include "ide_integration_agent.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <array>

namespace fs = std::filesystem;

namespace rawrxd::agent {

IDEIntegrationAgent::IDEIntegrationAgent() : m_state(AgentState::Idle) {}

IDEIntegrationAgent::~IDEIntegrationAgent() {}

std::string IDEIntegrationAgent::getStateString() const {
    switch (m_state) {
        case AgentState::Idle:       return "Idle";
        case AgentState::Listening:  return "Listening";
        case AgentState::Processing: return "Processing";
        case AgentState::Executing:  return "Executing";
        case AgentState::Error:      return "Error";
        default: return "Unknown";
    }
}

void IDEIntegrationAgent::executeCommand(const std::string& command) {
    changeState(AgentState::Processing);

    if (m_onOutput) {
        m_onOutput("Processing: " + command);
    }

    ParsedCommand parsed = parseCommand(command);
    executeAction(parsed);

    changeState(AgentState::Idle);
}

void IDEIntegrationAgent::processVoiceCommand(const std::string& transcribedText) {
    // Voice commands would match against a more natural grammar
    // For example: "Install GitHub Copilot" -> action=install-extension, params={ext=github-copilot}
    executeCommand(transcribedText);
}

void IDEIntegrationAgent::executeTask(const AgentTask& task) {
    changeState(AgentState::Executing);

    if (m_onOutput) {
        m_onOutput("Starting task: " + task.description);
    }

    const AgentTask& mutableTask = const_cast<AgentTask&>(task);

    for (size_t i = 0; i < mutableTask.steps.size(); ++i) {
        mutableTask.currentStep = i;

        if (m_onOutput) {
            m_onOutput("Step " + std::to_string(i + 1) + ": " + mutableTask.steps[i]);
        }

        // Execute step
        executeCommand(mutableTask.steps[i]);
    }

    mutableTask.completed = true;
    changeState(AgentState::Idle);
}

bool IDEIntegrationAgent::autonomouslyInstallExtension(const std::string& extensionId) {
    if (m_onOutput) {
        m_onOutput("Installing extension: " + extensionId);
    }

    // Execute VS Code CLI to install the extension
    std::string cmd = "code --install-extension " + extensionId + " --force 2>&1";
    CommandResult result = autonomouslyRunCommand(cmd);
    if (!result.success || result.exitCode != 0) {
        if (m_onError) m_onError("Extension install failed: " + result.errorMessage);
        return false;
    }
    if (m_onOutput) m_onOutput("Extension installed: " + extensionId);
    return true;
}

bool IDEIntegrationAgent::autonomouslySwitchProvider(const std::string& provider) {
    if (m_onOutput) {
        m_onOutput("Switching AI provider to: " + provider);
    }

    // Notify the registered provider-switch callback if wired
    if (m_onProviderSwitch) {
        return m_onProviderSwitch(provider);
    }
    // Fallback: write to settings file for the IDE to pick up on next poll
    if (m_onError) m_onError("No provider switch handler wired — update settings manually");
    return false;
}

bool IDEIntegrationAgent::autonomouslyAnalyzeFile(const std::string& filePath) {
    if (m_onOutput) {
        m_onOutput("Analyzing file: " + filePath);
    }

    // Read file contents
    if (!fs::exists(filePath)) {
        if (m_onError) m_onError("File not found: " + filePath);
        return false;
    }
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        if (m_onError) m_onError("Cannot open file: " + filePath);
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    ifs.close();

    // Delegate to AI analysis callback if wired
    if (m_onAnalyzeRequest) {
        std::string analysis = m_onAnalyzeRequest(filePath, content);
        if (m_onOutput) m_onOutput("Analysis result:\n" + analysis);
        return true;
    }
    // Fallback: basic metrics
    size_t lines = std::count(content.begin(), content.end(), '\n') + 1;
    size_t bytes = content.size();
    std::ostringstream report;
    report << "File: " << filePath << "\n"
           << "Lines: " << lines << "\n"
           << "Size: " << bytes << " bytes";
    if (m_onOutput) m_onOutput(report.str());
    return true;
}

bool IDEIntegrationAgent::autonomouslyCreateFile(const std::string& fileName, 
                                                 const std::string& description) {
    if (m_onOutput) {
        m_onOutput("Creating file: " + fileName + " - " + description);
    }

    // Generate content via AI callback if available
    std::string content;
    if (m_onGenerateContent) {
        content = m_onGenerateContent(fileName, description);
    } else {
        // Minimal template based on extension
        fs::path p(fileName);
        std::string ext = p.extension().string();
        if (ext == ".cpp" || ext == ".c") {
            content = "// " + p.filename().string() + " — " + description + "\n\n";
            content += "#include <iostream>\n\nint main() {\n    return 0;\n}\n";
        } else if (ext == ".h" || ext == ".hpp") {
            std::string guard = p.stem().string();
            std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);
            guard += "_H";
            content = "#pragma once\n// " + description + "\n\n";
        } else if (ext == ".py") {
            content = "# " + p.filename().string() + " — " + description + "\n\n";
        } else {
            content = "// " + description + "\n";
        }
    }

    // Write to disk
    std::ofstream ofs(fileName);
    if (!ofs.is_open()) {
        if (m_onError) m_onError("Cannot create file: " + fileName);
        return false;
    }
    ofs << content;
    ofs.close();
    if (m_onOutput) m_onOutput("File created: " + fileName + " (" + std::to_string(content.size()) + " bytes)");
    return true;
}

CommandResult IDEIntegrationAgent::autonomouslyRunCommand(const std::string& command) {
    if (m_onOutput) {
        m_onOutput("Running: " + command);
    }

    CommandResult result;
    result.exitCode = -1;
    result.success = false;

    // Execute via _popen and capture output
    std::array<char, 4096> buffer;
    std::ostringstream captured;
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        result.errorMessage = "Failed to launch command";
        return result;
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        captured << buffer.data();
    }
    int rc = _pclose(pipe);
    result.exitCode = rc;
    result.output = captured.str();
    result.success = (rc == 0);
    if (rc != 0) {
        result.errorMessage = "Command exited with code " + std::to_string(rc);
    }
    return result;
}

bool IDEIntegrationAgent::autonomouslyRefactorCode(const std::string& filePath,
                                                   const std::string& refactoringType) {
    if (m_onOutput) {
        m_onOutput("Refactoring " + filePath + " - type: " + refactoringType);
    }

    // Read the file
    if (!fs::exists(filePath)) {
        if (m_onError) m_onError("File not found: " + filePath);
        return false;
    }
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        if (m_onError) m_onError("Cannot open file: " + filePath);
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    ifs.close();

    // Delegate to AI refactoring callback if wired
    if (m_onRefactorRequest) {
        std::string refactored = m_onRefactorRequest(filePath, content, refactoringType);
        if (refactored.empty()) {
            if (m_onError) m_onError("Refactoring returned empty result");
            return false;
        }
        std::ofstream ofs(filePath);
        ofs << refactored;
        ofs.close();
        if (m_onOutput) m_onOutput("Refactoring applied to: " + filePath);
        return true;
    }
    if (m_onError) m_onError("No refactoring handler wired");
    return false;
}

bool IDEIntegrationAgent::autonomouslyGenerateTests(const std::string& filePath) {
    if (m_onOutput) {
        m_onOutput("Generating tests for: " + filePath);
    }

    if (!fs::exists(filePath)) {
        if (m_onError) m_onError("File not found: " + filePath);
        return false;
    }
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        if (m_onError) m_onError("Cannot open file: " + filePath);
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    ifs.close();

    // Delegate to AI test generation callback
    if (m_onGenerateTests) {
        std::string testCode = m_onGenerateTests(filePath, content);
        if (testCode.empty()) {
            if (m_onError) m_onError("Test generation returned empty result");
            return false;
        }
        // Write test file adjacent to source
        fs::path testPath = fs::path(filePath).parent_path() / ("test_" + fs::path(filePath).stem().string() + fs::path(filePath).extension().string());
        std::ofstream ofs(testPath);
        ofs << testCode;
        ofs.close();
        if (m_onOutput) m_onOutput("Tests written to: " + testPath.string());
        return true;
    }
    if (m_onError) m_onError("No test generation handler wired");
    return false;
}

void IDEIntegrationAgent::changeState(AgentState newState) {
    if (m_state != newState) {
        AgentState oldState = m_state;
        m_state = newState;

        if (m_onStateChange) {
            m_onStateChange(oldState, newState);
        }
    }
}

IDEIntegrationAgent::ParsedCommand IDEIntegrationAgent::parseCommand(const std::string& command) {
    ParsedCommand result;

    // Simple command parser - in production would use NLP
    std::istringstream iss(command);
    std::string word;

    // Extract action (first meaningful word or compound)
    if (iss >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);

        if (word == "install") {
            result.action = "install-extension";
            if (iss >> word) {
                result.parameters["extension"] = word;
            }
        } else if (word == "switch") {
            result.action = "switch-provider";
            if (iss >> word && word == "to") {
                if (iss >> word) {
                    result.parameters["provider"] = word;
                }
            }
        } else if (word == "analyze") {
            result.action = "analyze-file";
            if (iss >> word) {
                result.parameters["file"] = word;
            }
        } else if (word == "create") {
            result.action = "create-file";
            if (iss >> word) {
                result.parameters["name"] = word;
            }
        } else if (word == "run") {
            result.action = "run-command";
            std::string rest;
            std::getline(iss, rest);
            result.parameters["command"] = rest;
        } else if (word == "refactor") {
            result.action = "refactor-code";
        } else if (word == "test") {
            result.action = "generate-tests";
        } else {
            result.action = "unknown";
        }
    }

    return result;
}

void IDEIntegrationAgent::executeAction(const ParsedCommand& cmd) {
    if (cmd.action == "install-extension") {
        auto it = cmd.parameters.find("extension");
        if (it != cmd.parameters.end()) {
            autonomouslyInstallExtension(it->second);
        }
    } else if (cmd.action == "switch-provider") {
        auto it = cmd.parameters.find("provider");
        if (it != cmd.parameters.end()) {
            autonomouslySwitchProvider(it->second);
        }
    } else if (cmd.action == "analyze-file") {
        auto it = cmd.parameters.find("file");
        if (it != cmd.parameters.end()) {
            autonomouslyAnalyzeFile(it->second);
        }
    } else if (cmd.action == "create-file") {
        auto nameIt = cmd.parameters.find("name");
        if (nameIt != cmd.parameters.end()) {
            autonomouslyCreateFile(nameIt->second, "");
        }
    } else if (cmd.action == "run-command") {
        auto it = cmd.parameters.find("command");
        if (it != cmd.parameters.end()) {
            autonomouslyRunCommand(it->second);
        }
    } else if (cmd.action == "refactor-code") {
        // Refactor action implementation
    } else if (cmd.action == "generate-tests") {
        // Test generation action implementation
    } else {
        if (m_onError) {
            m_onError("Unknown action: " + cmd.action);
        }
    }
}

}  // namespace rawrxd::agent
