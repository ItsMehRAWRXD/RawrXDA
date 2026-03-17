// ============================================================================
// cli_state.h — CLIState Structure Definition (Shared Header)
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Extracted from cli_shell.cpp so feature_handlers.cpp and other shared
// components can properly access editor buffer content via cliStatePtr.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_CLI_STATE_H
#define RAWRXD_CLI_STATE_H

#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <utility>

// Forward declarations — full definitions in their respective headers
class AgenticEngine;
class SubAgentManager;
class VoiceChat;

struct CLIState {
    std::string currentFile;
    std::string editorBuffer;
    std::vector<std::string> clipboard;
    std::deque<std::string> undoStack;
    std::deque<std::string> redoStack;

    // Agentic state
    std::string agentGoal;
    std::vector<std::string> agentMemory;
    bool agentLoopRunning = false;

    // Autonomy state
    bool autonomyEnabled = false;
    int maxActionsPerMinute = 60;

    // Debugger state
    std::vector<std::pair<std::string, int>> breakpoints; // file, line
    bool debuggingActive = false;
    int debugCurrentLine = 1;
    int debugStepCount = 0;

    // Terminal state
    std::vector<std::string> terminalPanes;

    // Agentic engine + SubAgent manager (set from main or externally)
    AgenticEngine* agenticEngine = nullptr;
    SubAgentManager* subAgentMgr = nullptr;

    // Voice state (Phase 33 parity with Win32IDE)
    std::unique_ptr<VoiceChat> voiceChat;
    bool voiceInitialized = false;
    bool voiceTTSEnabled = false;
};

#endif // RAWRXD_CLI_STATE_H
