/// ide_integration_agent.h — Autonomous agent for IDE orchestration (C++20/Win32)
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace rawrxd::agent {

// Command execution result
struct CommandResult {
    bool success;
    std::string output;
    int exitCode;
    std::string errorMessage;
};

// Agent state machine states
enum class AgentState {
    Idle,
    Listening,
    Processing,
    Executing,
    Error
};

// Agentic goal/task
struct AgentTask {
    std::string id;
    std::string description;
    std::vector<std::string> steps;
    int currentStep;
    bool completed;
};

/**
 * @class IDEIntegrationAgent
 * @brief Autonomous IDE agent that:
 * - Monitors IDE state and file changes
 * - Accepts voice/text commands
 * - Executes complex IDE operations autonomously
 * - Integrates with chat panel for user interaction
 * - Controls VSIX installation and extension management
 * - Manages AI provider switching
 */
class IDEIntegrationAgent {
public:
    IDEIntegrationAgent();
    ~IDEIntegrationAgent();

    // ========================================================================
    // Autonomous Execution Methods
    // ========================================================================

    /**
     * Execute high-level command autonomously
     * Examples:
     *  "Install GitHub Copilot and open chat"
     *  "Analyze current file for errors"
     *  "Create new PowerShell script with AI help"
     *  "Run and debug terminal command"
     */
    void executeCommand(const std::string& command);

    /**
     * Process voice input (text transcribed from speech)
     */
    void processVoiceCommand(const std::string& transcribedText);

    /**
     * Execute multi-step task
     */
    void executeTask(const AgentTask& task);

    // ========================================================================
    // State Management
    // ========================================================================

    AgentState getState() const { return m_state; }
    std::string getStateString() const;

    /**
     * Set idle timeout - agent returns to idle after this period
     */
    void setIdleTimeout(int milliseconds) { m_idleTimeout = milliseconds; }

    // ========================================================================
    // Specific Operations (Called Autonomously)
    // ========================================================================

    /**
     * Autonomously install an extension
     */
    bool autonomouslyInstallExtension(const std::string& extensionId);

    /**
     * Autonomously switch AI provider
     */
    bool autonomouslySwitchProvider(const std::string& provider);

    /**
     * Autonomously open file and analyze it
     */
    bool autonomouslyAnalyzeFile(const std::string& filePath);

    /**
     * Autonomously create new file based on description
     */
    bool autonomouslyCreateFile(const std::string& fileName, 
                               const std::string& description);

    /**
     * Autonomously run terminal command and capture output
     */
    CommandResult autonomouslyRunCommand(const std::string& command);

    /**
     * Autonomously refactor code in file
     */
    bool autonomouslyRefactorCode(const std::string& filePath,
                                 const std::string& refactoringType);

    /**
     * Autonomously generate tests for file
     */
    bool autonomouslyGenerateTests(const std::string& filePath);

    // ========================================================================
    // Callback Registration
    // ========================================================================

    using StateChangeCallback = std::function<void(AgentState oldState, AgentState newState)>;
    using OutputCallback = std::function<void(const std::string& message)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    void setOnStateChange(StateChangeCallback cb) { m_onStateChange = cb; }
    void setOnOutput(OutputCallback cb) { m_onOutput = cb; }
    void setOnError(ErrorCallback cb) { m_onError = cb; }

private:
    AgentState m_state = AgentState::Idle;
    int m_idleTimeout = 30000;  // 30 seconds

    // Callbacks
    StateChangeCallback m_onStateChange;
    OutputCallback m_onOutput;
    ErrorCallback m_onError;

    // Internal methods
    void changeState(AgentState newState);
    void parseAndExecuteCommand(const std::string& command);

    // Command parsing and execution
    struct ParsedCommand {
        std::string action;
        std::map<std::string, std::string> parameters;
    };

    ParsedCommand parseCommand(const std::string& command);
    void executeAction(const ParsedCommand& cmd);
};

}  // namespace rawrxd::agent
