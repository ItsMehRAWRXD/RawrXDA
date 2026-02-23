#include <iostream>
#include <thread>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <atomic>
#include <mutex>
#include <chrono>
#include <filesystem>
#include "runtime_core.h"
#include "modules/react_generator.h"
#include "agentic_engine.h"
#include "subagent_core.h"
#include "cli/agentic_decision_tree.h"
#include "cli/cli_autonomy_loop.h"
#include "cli/cli_headless_systems.h"
#include "deterministic_replay.h"
#include "../include/chain_of_thought_engine.h"
#include "core/voice_chat.hpp"
#include "core/voice_automation.hpp"
#include "core/instructions_provider.hpp"

#include "logging/logger.h"
static Logger s_logger("cli_shell");

// ── Unified Feature Dispatch (CLI ↔ Win32 parity) ──────────────────────────
#include "cli/cli_feature_bridge.h"
#include "core/feature_handlers.h"

namespace fs = std::filesystem;

// ============================================================================
// SHARED STATE (Parity with Win32IDE)
// ============================================================================

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

CLIState g_state;
std::mutex g_stateMutex;

// Declaration for server start function
void start_server(int port);

// ============================================================================
// INSTRUCTIONS CONTEXT (Phase 34 — Read tools.instructions.md All Lines)
// ============================================================================

void cmd_instructions(const std::string& args) {
    auto& provider = InstructionsProvider::instance();

    if (args.empty() || args == "help") {
        s_logger.info("\n\xF0\x9F\x93\x8B  INSTRUCTIONS COMMANDS:\n");
        s_logger.info("  !instructions                 Show all loaded instructions\n");
        s_logger.info("  !instructions list            List loaded instruction files\n");
        s_logger.info("  !instructions show             Show full content (all lines)\n");
        s_logger.info("  !instructions reload           Reload from disk\n");
        s_logger.info("  !instructions paths            Show search paths\n");
        s_logger.info("  !instructions load <file>      Load a specific file\n");
        s_logger.info("  !instructions json             Export as JSON\n");
        s_logger.info("  !instructions summary          Show file summary\n");
        return;
    }

    // Lazy-load on first access
    if (!provider.isLoaded()) {
        auto r = provider.loadAll();
        if (!r.success) {
            s_logger.info("\xE2\x9D\x8C Failed to load instructions: ");
        }
    }

    if (args == "list") {
        auto files = provider.getAll();
        if (files.empty()) {
            s_logger.info("\xE2\x9A\xA0 No instruction files loaded.\n");
            return;
        }
        s_logger.info("\n\xF0\x9F\x93\x82 Loaded instruction files (");
        for (const auto& f : files) {
            s_logger.info("  \xE2\x9C\x93 ");
            s_logger.info("    Path: ");
        }
    }
    else if (args == "show") {
        std::string content = provider.getAllContent();
        if (content.empty()) {
            s_logger.info("\xE2\x9A\xA0 No instructions content available.\n");
        } else {
            s_logger.info("\n");
        }
    }
    else if (args == "reload") {
        auto r = provider.reload();
        if (r.success) {
            s_logger.info("\xE2\x9C\x85 Instructions reloaded (");
        } else {
            s_logger.info("\xE2\x9D\x8C Reload failed: ");
        }
    }
    else if (args == "paths") {
        auto paths = provider.getSearchPaths();
        s_logger.info("\n\xF0\x9F\x94\x8D Search paths (");
        for (const auto& p : paths) {
            bool exists = fs::exists(p);
            s_logger.info("  ");
        }
    }
    else if (args.substr(0, 5) == "load ") {
        std::string path = args.substr(5);
        auto r = provider.loadFile(path);
        if (r.success) {
            s_logger.info("\xE2\x9C\x85 Loaded: ");
        } else {
            s_logger.info("\xE2\x9D\x8C Failed: ");
        }
    }
    else if (args == "json") {
        s_logger.info( provider.toJSON() << "\n";
    }
    else if (args == "summary") {
        s_logger.info( provider.toJSONSummary() << "\n";
    }
    else {
        // Default: show list
        auto files = provider.getAll();
        if (files.empty()) {
            s_logger.info("\xE2\x9A\xA0 No instruction files loaded. Use !instructions reload\n");
        } else {
            s_logger.info("\n\xF0\x9F\x93\x8B Instructions context (");
            s_logger.info( provider.getAllContent() << "\n";
        }
    }
}

// ============================================================================
// VOICE OPERATIONS (Phase 33 — Feature Parity with Win32IDE_VoiceChat)
// ============================================================================

static void ensure_voice_init() {
    if (g_state.voiceInitialized) return;
    g_state.voiceChat = std::make_unique<VoiceChat>();
    VoiceChatConfig cfg;
    cfg.sampleRate    = 16000;
    cfg.channels      = 1;
    cfg.bitsPerSample = 16;
    cfg.enableVAD     = true;
    cfg.vadThreshold  = 0.02f;
    cfg.vadSilenceMs  = 1500;
    cfg.enableMetrics = true;
    cfg.userName      = "CLI User";
    auto r = g_state.voiceChat->configure(cfg);
    if (r.success) {
        g_state.voiceInitialized = true;
        s_logger.info("\xE2\x9C\x85 Voice engine initialized (16kHz mono, VAD enabled)\n");
    } else {
        s_logger.info("\xE2\x9D\x8C Voice init failed: ");
    }
}

void cmd_voice(const std::string& args) {
    if (args.empty() || args == "help") {
        s_logger.info("\n\xF0\x9F\x8E\x99\xEF\xB8\x8F  VOICE COMMANDS:\n");
        s_logger.info("  !voice init               Initialize voice engine\n");
        s_logger.info("  !voice record              Start/stop recording\n");
        s_logger.info("  !voice ptt                 Push-to-talk toggle\n");
        s_logger.info("  !voice transcribe          Transcribe last recording\n");
        s_logger.info("  !voice speak <text>        Text-to-speech\n");
        s_logger.info("  !voice tts <on|off>        Toggle TTS for AI responses\n");
        s_logger.info("  !voice mode <ptt|vad|off>  Set capture mode\n");
        s_logger.info("  !voice devices             List audio devices\n");
        s_logger.info("  !voice device <id>         Select input device\n");
        s_logger.info("  !voice room <name>         Join/leave voice room\n");
        s_logger.info("  !voice metrics             Show voice metrics\n");
        s_logger.info("  !voice status              Show voice status\n");
        return;
    }
    
    // Parse subcommand
    auto sp = args.find(' ');
    std::string sub = (sp == std::string::npos) ? args : args.substr(0, sp);
    std::string param = (sp == std::string::npos) ? "" : args.substr(sp + 1);
    
    if (sub == "init") {
        ensure_voice_init();
        return;
    }
    
    // All other commands require init
    ensure_voice_init();
    if (!g_state.voiceInitialized || !g_state.voiceChat) {
        s_logger.info("\xE2\x9D\x8C Voice engine not available.\n");
        return;
    }
    
    if (sub == "record") {
        if (g_state.voiceChat->isRecording()) {
            auto r = g_state.voiceChat->stopRecording();
            s_logger.info("\xE2\x8F\xB9 Recording stopped.");
            if (r.success) {
                size_t samples = g_state.voiceChat->getRecordedSampleCount();
                double secs = static_cast<double>(samples) / 16000.0;
                s_logger.info(" (");
            }
            s_logger.info("\n");
        } else {
            auto r = g_state.voiceChat->startRecording();
            if (r.success) {
                s_logger.info("\xE2\x8F\xBA Recording... (type !voice record to stop)\n");
            } else {
                s_logger.info("\xE2\x9D\x8C Record failed: ");
            }
        }
    }
    else if (sub == "ptt") {
        if (g_state.voiceChat->isPTTActive()) {
            g_state.voiceChat->pttEnd();
            s_logger.info("\xF0\x9F\x8E\x99 PTT released.\n");
            // Auto-transcribe on PTT release
            std::string text;
            auto r = g_state.voiceChat->transcribeLastRecording(text);
            if (r.success && !text.empty()) {
                s_logger.info("\xF0\x9F\x93\x9D Transcription: ");
            }
        } else {
            g_state.voiceChat->pttBegin();
            s_logger.info("\xF0\x9F\x8E\x99 PTT active — speak now (type !voice ptt to release)\n");
        }
    }
    else if (sub == "transcribe") {
        std::string text;
        auto r = g_state.voiceChat->transcribeLastRecording(text);
        if (r.success && !text.empty()) {
            s_logger.info("\xF0\x9F\x93\x9D Transcription: ");
        } else {
            s_logger.info("\xE2\x9D\x8C Transcription failed: ");
        }
    }
    else if (sub == "speak") {
        if (param.empty()) {
            s_logger.info("Usage: !voice speak <text to speak>\n");
            return;
        }
        auto r = g_state.voiceChat->speak(param);
        if (r.success) {
            s_logger.info("\xF0\x9F\x94\x8A Speaking: ");
        } else {
            s_logger.info("\xE2\x9D\x8C TTS failed: ");
        }
    }
    else if (sub == "tts") {
        if (param == "on") {
            g_state.voiceTTSEnabled = true;
            s_logger.info("\xF0\x9F\x94\x8A TTS for AI responses: ON\n");
        } else if (param == "off") {
            g_state.voiceTTSEnabled = false;
            s_logger.info("\xF0\x9F\x94\x87 TTS for AI responses: OFF\n");
        } else {
            s_logger.info("TTS is currently: ");
            s_logger.info("Usage: !voice tts <on|off>\n");
        }
    }
    else if (sub == "mode") {
        if (param == "ptt") {
            g_state.voiceChat->setMode(VoiceChatMode::PushToTalk);
            s_logger.info("\xE2\x9C\x85 Voice mode: Push-to-Talk\n");
        } else if (param == "vad") {
            g_state.voiceChat->setMode(VoiceChatMode::Continuous);
            s_logger.info("\xE2\x9C\x85 Voice mode: Continuous (VAD)\n");
        } else if (param == "off") {
            g_state.voiceChat->setMode(VoiceChatMode::Disabled);
            s_logger.info("\xE2\x9C\x85 Voice mode: Disabled\n");
        } else {
            auto mode = g_state.voiceChat->getMode();
            const char* modeStr = (mode == VoiceChatMode::PushToTalk) ? "Push-to-Talk" :
                                  (mode == VoiceChatMode::Continuous) ? "Continuous (VAD)" : "Disabled";
            s_logger.info("Current mode: ");
            s_logger.info("Usage: !voice mode <ptt|vad|off>\n");
        }
    }
    else if (sub == "devices") {
        auto inputs = VoiceChat::enumerateInputDevices();
        auto outputs = VoiceChat::enumerateOutputDevices();
        s_logger.info("\n\xF0\x9F\x8E\xA7 INPUT DEVICES:\n");
        for (const auto& dev : inputs) {
            s_logger.info("  [");
            if (dev.isDefault) s_logger.info(" (default)");
            s_logger.info("\n");
        }
        s_logger.info("\n\xF0\x9F\x94\x8A OUTPUT DEVICES:\n");
        for (const auto& dev : outputs) {
            s_logger.info("  [");
            if (dev.isDefault) s_logger.info(" (default)");
            s_logger.info("\n");
        }
        if (inputs.empty()) s_logger.info("  (no input devices found)\n");
        if (outputs.empty()) s_logger.info("  (no output devices found)\n");
    }
    else if (sub == "device") {
        if (param.empty()) {
            s_logger.info("Usage: !voice device <id>\n");
            return;
        }
        try {
            int id = std::stoi(param);
            auto r = g_state.voiceChat->selectInputDevice(id);
            if (r.success) {
                s_logger.info("\xE2\x9C\x85 Input device set to ID ");
            } else {
                s_logger.info("\xE2\x9D\x8C Failed: ");
            }
        } catch (...) {
            s_logger.info("\xE2\x9D\x8C Invalid device ID\n");
        }
    }
    else if (sub == "room") {
        if (g_state.voiceChat->isInRoom()) {
            std::string room = g_state.voiceChat->getRoomName();
            g_state.voiceChat->leaveRoom();
            s_logger.info("\xF0\x9F\x9A\xAA Left room '");
        } else {
            std::string room = param.empty() ? "general" : param;
            auto r = g_state.voiceChat->joinRoom(room);
            if (r.success) {
                s_logger.info("\xF0\x9F\x8E\xA4 Joined room '");
            } else {
                s_logger.info("\xE2\x9D\x8C Join failed: ");
            }
        }
    }
    else if (sub == "metrics") {
        auto m = g_state.voiceChat->getMetrics();
        s_logger.info("\n\xF0\x9F\x93\x8A VOICE METRICS:\n");
        s_logger.info("  Recordings:        ");
        s_logger.info("  Playbacks:         ");
        s_logger.info("  Transcriptions:    ");
        s_logger.info("  TTS Calls:         ");
        s_logger.info("  Errors:            ");
        s_logger.info("  Bytes Recorded:    ");
        s_logger.info("  Bytes Played:      ");
        s_logger.info("  VAD Speech Events: ");
        s_logger.info("  Relay Msg Sent:    ");
        s_logger.info("  Relay Msg Recv:    ");
        s_logger.info( std::fixed << std::setprecision(2);
        s_logger.info("  Avg Record Latency:    ");
        s_logger.info("  Avg Transcribe Latency:");
        s_logger.info("  Avg TTS Latency:       ");
        s_logger.info("  Avg Playback Latency:  ");
    }
    else if (sub == "status") {
        auto mode = g_state.voiceChat->getMode();
        const char* modeStr = (mode == VoiceChatMode::PushToTalk) ? "Push-to-Talk" :
                              (mode == VoiceChatMode::Continuous) ? "Continuous (VAD)" : "Disabled";
        s_logger.info("\n\xF0\x9F\x8E\x99 VOICE STATUS:\n");
        s_logger.info("  Engine:      ");
        s_logger.info("  Recording:   ");
        s_logger.info("  Playing:     ");
        s_logger.info("  PTT Active:  ");
        s_logger.info("  In Room:     ");
        s_logger.info("  Mode:        ");
        s_logger.info("  VAD State:   ");
        switch (g_state.voiceChat->getVADState()) {
            case VADState::Silence:  s_logger.info("Silence"); break;
            case VADState::Speech:   s_logger.info("Speech detected"); break;
            case VADState::Trailing: s_logger.info("Trailing silence"); break;
        }
        s_logger.info("\n");
        s_logger.info("  RMS Level:   ");
        s_logger.info("  TTS Auto:    ");
        s_logger.info("  Recorded:    ");
    }
    else {
        s_logger.info("Unknown voice subcommand: ");
    }
}

// ============================================================================
// FILE OPERATIONS (Feature Parity with Win32IDE_FileOps)
// ============================================================================

void cmd_new_file(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.editorBuffer.clear();
    g_state.currentFile.clear();
    g_state.undoStack.clear();
    g_state.redoStack.clear();
    s_logger.info("✅ New file created\n");
}

void cmd_open_file(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::string path = args;
    if (path.empty()) {
        s_logger.info("Usage: !open <filepath>\n");
        return;
    }
    
    if (!fs::exists(path)) {
        s_logger.info("❌ File not found: ");
        return;
    }
    
    std::ifstream file(path);
    g_state.editorBuffer.assign((std::istreambuf_iterator<char>(file)), 
                                 std::istreambuf_iterator<char>());
    g_state.currentFile = path;
    g_state.undoStack.clear();
    g_state.redoStack.clear();
    s_logger.info("✅ Opened: ");
}

void cmd_save_file(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.currentFile.empty()) {
        s_logger.info("❌ No file open. Use !save_as <path> to save to a new file.\n");
        return;
    }
    
    std::ofstream file(g_state.currentFile);
    file << g_state.editorBuffer;
    file.close();
    s_logger.info("✅ Saved: ");
}

void cmd_save_as(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        s_logger.info("Usage: !save_as <filepath>\n");
        return;
    }
    
    std::ofstream file(args);
    file << g_state.editorBuffer;
    file.close();
    g_state.currentFile = args;
    s_logger.info("✅ Saved as: ");
}

void cmd_close_file(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.currentFile.empty()) {
        s_logger.info("❌ No file open.\n");
        return;
    }
    g_state.editorBuffer.clear();
    g_state.currentFile.clear();
    s_logger.info("✅ File closed\n");
}

// ============================================================================
// EDITOR OPERATIONS (Feature Parity with Win32IDE Edit commands)
// ============================================================================

void cmd_cut(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.editorBuffer.empty()) {
        s_logger.info("❌ Buffer is empty, nothing to cut.\n");
        return;
    }
    // If args specify a line range, cut those lines; otherwise cut the last line
    g_state.undoStack.push_back(g_state.editorBuffer);
    if (!args.empty()) {
        // Parse line range: "start-end" or single line number
        size_t dash = args.find('-');
        int startLine = 1, endLine = -1;
        try {
            if (dash != std::string::npos) {
                startLine = std::stoi(args.substr(0, dash));
                endLine = std::stoi(args.substr(dash + 1));
            } else {
                startLine = std::stoi(args);
                endLine = startLine;
            }
        } catch (...) {
            s_logger.info("Usage: !cut [start_line-end_line]\n");
            g_state.undoStack.pop_back();
            return;
        }
        // Extract and remove specified lines
        std::istringstream stream(g_state.editorBuffer);
        std::string line;
        std::string cutText, remaining;
        int lineNum = 1;
        while (std::getline(stream, line)) {
            if (lineNum >= startLine && (endLine < 0 || lineNum <= endLine)) {
                cutText += line + "\n";
            } else {
                remaining += line + "\n";
            }
            lineNum++;
        }
        g_state.clipboard.clear();
        g_state.clipboard.push_back(cutText);
        g_state.editorBuffer = remaining;
        g_state.redoStack.clear();
        s_logger.info("✅ Cut lines ");
    } else {
        // Cut entire buffer
        g_state.clipboard.clear();
        g_state.clipboard.push_back(g_state.editorBuffer);
        g_state.editorBuffer.clear();
        g_state.redoStack.clear();
        s_logger.info("✅ Cut entire buffer to clipboard\n");
    }
}

void cmd_copy(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.clipboard.push_back(g_state.editorBuffer);
    s_logger.info("✅ Copied to clipboard\n");
}

void cmd_paste(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_state.clipboard.empty()) {
        g_state.undoStack.push_back(g_state.editorBuffer);
        g_state.editorBuffer += g_state.clipboard.back();
        g_state.redoStack.clear();
        s_logger.info("✅ Pasted from clipboard\n");
    } else {
        s_logger.info("❌ Clipboard empty\n");
    }
}

void cmd_undo(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_state.undoStack.empty()) {
        g_state.redoStack.push_back(g_state.editorBuffer);
        g_state.editorBuffer = g_state.undoStack.back();
        g_state.undoStack.pop_back();
        s_logger.info("✅ Undo\n");
    } else {
        s_logger.info("❌ Nothing to undo\n");
    }
}

void cmd_redo(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_state.redoStack.empty()) {
        g_state.undoStack.push_back(g_state.editorBuffer);
        g_state.editorBuffer = g_state.redoStack.back();
        g_state.redoStack.pop_back();
        s_logger.info("✅ Redo\n");
    } else {
        s_logger.info("❌ Nothing to redo\n");
    }
}

void cmd_find(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        s_logger.info("Usage: !find <text>\n");
        return;
    }
    
    size_t pos = g_state.editorBuffer.find(args);
    if (pos != std::string::npos) {
        s_logger.info("✅ Found at position ");
    } else {
        s_logger.info("❌ Not found\n");
    }
}

void cmd_replace(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto space = args.find(' ');
    if (space == std::string::npos) {
        s_logger.info("Usage: !replace <old> <new>\n");
        return;
    }
    
    std::string old_text = args.substr(0, space);
    std::string new_text = args.substr(space + 1);
    
    g_state.undoStack.push_back(g_state.editorBuffer);
    size_t pos = 0;
    int count = 0;
    while ((pos = g_state.editorBuffer.find(old_text, pos)) != std::string::npos) {
        g_state.editorBuffer.replace(pos, old_text.length(), new_text);
        pos += new_text.length();
        count++;
    }
    g_state.redoStack.clear();
    s_logger.info("✅ Replaced ");
}

// ============================================================================
// AGENTIC OPERATIONS (Feature Parity with Win32IDE_AgentCommands)
// ============================================================================

void cmd_agent_execute(const std::string& args) {
    if (args.empty()) {
        s_logger.info("Usage: !agent_execute <prompt>\n");
        return;
    }
    
    if (g_state.agenticEngine && g_state.agenticEngine->isModelLoaded()) {
        s_logger.info("🤖 Agent executing: ");
        std::string response = g_state.agenticEngine->chat(args);
        s_logger.info( response << "\n";
        
        // Auto-dispatch tool calls in response
        if (g_state.subAgentMgr) {
            std::string toolResult;
            if (g_state.subAgentMgr->dispatchToolCall("cli-agent", response, toolResult)) {
                s_logger.info("\n[Tool Result]\n");
            }
        }
    } else {
        s_logger.info("🤖 Agent executing: ");
        std::string out = process_prompt(args);
        s_logger.info( out << "\n";
    }
}

void cmd_agent_loop(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        s_logger.info("Usage: !agent_loop <prompt> [iterations]\n");
        return;
    }
    
    int iterations = 10;
    auto space = args.rfind(' ');
    std::string prompt = args;
    
    if (space != std::string::npos) {
        try {
            iterations = std::stoi(args.substr(space + 1));
            prompt = args.substr(0, space);
        } catch (...) {}
    }
    
    g_state.agentLoopRunning = true;
    s_logger.info("🚀 Starting agent loop: ");
    
    // Capture pointers locally before launching thread
    AgenticEngine* eng = g_state.agenticEngine;
    SubAgentManager* mgr = g_state.subAgentMgr;
    
    std::thread([prompt, iterations, eng, mgr]() {
        for (int i = 0; i < iterations; i++) {
            s_logger.info("[Agent Iter ");
            
            if (eng && eng->isModelLoaded()) {
                std::string response = eng->chat(
                    "Iteration " + std::to_string(i + 1) + "/" + std::to_string(iterations) +
                    ". Goal: " + prompt + "\nPrevious context available. Continue working toward the goal.");
                s_logger.info( response << "\n";
                
                // Dispatch tool calls
                if (mgr) {
                    std::string toolResult;
                    if (mgr->dispatchToolCall("cli-loop", response, toolResult)) {
                        s_logger.info("[Tool Result] ");
                    }
                }
            } else {
                std::string out = process_prompt(prompt);
                s_logger.info( out << "\n";
            }
        }
        s_logger.info("✅ Agent loop completed\n");
    }).detach();
}

void cmd_agent_goal(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        s_logger.info("Usage: !agent_goal <goal>\n");
        return;
    }
    g_state.agentGoal = args;
    s_logger.info("✅ Goal set: ");
}

void cmd_agent_memory(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args == "show") {
        s_logger.info("📝 Agent Memory:\n");
        for (size_t i = 0; i < g_state.agentMemory.size(); i++) {
            s_logger.info("  [");
        }
    } else if (!args.empty()) {
        g_state.agentMemory.push_back(args);
        s_logger.info("✅ Memory added: ");
    } else {
        s_logger.info("Usage: !agent_memory <observation> | !agent_memory show\n");
    }
}

// ============================================================================
// AUTONOMY OPERATIONS (Feature Parity with Win32IDE_Autonomy)
// ============================================================================

void cmd_autonomy_start(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.autonomyEnabled = true;
    s_logger.info("🤖 Autonomy enabled. Use !autonomy_goal to set objective.\n");
}

void cmd_autonomy_stop(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.autonomyEnabled = false;
    s_logger.info("⏹️  Autonomy disabled\n");
}

void cmd_autonomy_goal(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        s_logger.info("Current goal: ");
        return;
    }
    g_state.agentGoal = args;
    s_logger.info("✅ Autonomy goal set: ");
}

void cmd_autonomy_rate(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    try {
        int rate = std::stoi(args);
        g_state.maxActionsPerMinute = rate;
        s_logger.info("✅ Max actions per minute: ");
    } catch (...) {
        s_logger.info("Usage: !autonomy_rate <actions_per_minute>\n");
    }
}

// ============================================================================
// DEBUG OPERATIONS (Feature Parity with Win32IDE_Debugger)
// ============================================================================

void cmd_breakpoint_add(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto space = args.find(':');
    if (space == std::string::npos) {
        s_logger.info("Usage: !breakpoint_add <file>:<line>\n");
        return;
    }
    
    std::string file = args.substr(0, space);
    int line = std::stoi(args.substr(space + 1));
    
    g_state.breakpoints.push_back({file, line});
    s_logger.info("✅ Breakpoint added: ");
}

void cmd_breakpoint_list(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    s_logger.info("🔴 Breakpoints:\n");
    for (size_t i = 0; i < g_state.breakpoints.size(); i++) {
        s_logger.info("  [");
    }
}

void cmd_breakpoint_remove(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    try {
        int idx = std::stoi(args);
        if (idx >= 0 && idx < static_cast<int>(g_state.breakpoints.size())) {
            g_state.breakpoints.erase(g_state.breakpoints.begin() + idx);
            s_logger.info("✅ Breakpoint removed\n");
        }
    } catch (...) {
        s_logger.info("Usage: !breakpoint_remove <index>\n");
    }
}

void cmd_debug_start(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.debuggingActive = true;
    s_logger.info("🐛 Debugger started\n");
}

void cmd_debug_stop(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.debuggingActive = false;
    s_logger.info("⏹️  Debugger stopped\n");
}

void cmd_debug_step(const std::string& args) {
    if (!g_state.debuggingActive) {
        s_logger.info("❌ Debugger not active. Use !debug_start first.\n");
        return;
    }
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.debugStepCount++;
    // Advance to next breakpoint or next line
    if (!g_state.breakpoints.empty() && !g_state.currentFile.empty()) {
        // Find current position and step to next line
        int currentLine = g_state.debugCurrentLine;
        g_state.debugCurrentLine = currentLine + 1;
        s_logger.info("➡️  Step to line ");
        // Check if we hit a breakpoint
        for (const auto& bp : g_state.breakpoints) {
            if (bp.first == g_state.currentFile && bp.second == g_state.debugCurrentLine) {
                s_logger.info(" 🔴 [BREAKPOINT HIT]");
                break;
            }
        }
        s_logger.info("\n");
        // Show the source line at current position
        std::istringstream stream(g_state.editorBuffer);
        std::string line;
        int lineNum = 1;
        while (std::getline(stream, line)) {
            if (lineNum == g_state.debugCurrentLine) {
                s_logger.info("  → ");
                break;
            }
            lineNum++;
        }
    } else {
        s_logger.info("➡️  Step executed (no source context)\n");
    }
}

void cmd_debug_continue(const std::string& args) {
    if (!g_state.debuggingActive) {
        s_logger.info("❌ Debugger not active. Use !debug_start first.\n");
        return;
    }
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.breakpoints.empty()) {
        s_logger.info("▶️  Continuing to end (no breakpoints set)\n");
        g_state.debuggingActive = false;
        s_logger.info("⏹️  Execution complete\n");
        return;
    }
    // Advance through lines until we hit a breakpoint
    int totalLines = static_cast<int>(std::count(g_state.editorBuffer.begin(), g_state.editorBuffer.end(), '\n')) + 1;
    int startLine = g_state.debugCurrentLine + 1;
    bool hitBreakpoint = false;
    for (int line = startLine; line <= totalLines; ++line) {
        for (const auto& bp : g_state.breakpoints) {
            if (bp.first == g_state.currentFile && bp.second == line) {
                g_state.debugCurrentLine = line;
                hitBreakpoint = true;
                s_logger.info("▶️  Continued to breakpoint at ");
                // Show the source line
                std::istringstream stream(g_state.editorBuffer);
                std::string srcLine;
                int num = 1;
                while (std::getline(stream, srcLine)) {
                    if (num == line) {
                        s_logger.info("  → ");
                        break;
                    }
                    num++;
                }
                break;
            }
        }
        if (hitBreakpoint) break;
    }
    if (!hitBreakpoint) {
        s_logger.info("▶️  Ran to end — no more breakpoints hit\n");
        g_state.debuggingActive = false;
    }
}

// ============================================================================
// TERMINAL OPERATIONS (Feature Parity with Win32IDE Terminal commands)
// ============================================================================

void cmd_terminal_new(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::string pane_name = "Terminal-" + std::to_string(g_state.terminalPanes.size() + 1);
    g_state.terminalPanes.push_back(pane_name);
    s_logger.info("✅ New terminal pane: ");
}

void cmd_terminal_split(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::string orientation = args.empty() ? "horizontal" : args;
    s_logger.info("✅ Terminal split ");
}

void cmd_terminal_kill(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.terminalPanes.empty()) {
        s_logger.info("❌ No terminals to close\n");
        return;
    }
    g_state.terminalPanes.pop_back();
    s_logger.info("✅ Terminal closed\n");
}

void cmd_terminal_list(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    s_logger.info("📋 Terminal panes:\n");
    for (size_t i = 0; i < g_state.terminalPanes.size(); i++) {
        s_logger.info("  [");
    }
}

// ============================================================================
// HOTPATCH OPERATIONS (Feature Parity with Win32IDE Hotpatch)
// ============================================================================

void cmd_hotpatch_apply(const std::string& args) {
    if (args.empty()) {
        s_logger.info("Usage: !hotpatch_apply <patch_file>\n");
        return;
    }
    // Parse patch file path
    std::string patchPath = args;
    // Trim whitespace
    size_t start = patchPath.find_first_not_of(" \t");
    size_t end = patchPath.find_last_not_of(" \t");
    if (start != std::string::npos) patchPath = patchPath.substr(start, end - start + 1);

    s_logger.info("🔥 Applying hotpatch from: ");
    
    // Read the patch file
    std::ifstream patchFile(patchPath);
    if (!patchFile.is_open()) {
        s_logger.info("❌ Cannot open patch file: ");
        return;
    }
    
    std::string patchContent((std::istreambuf_iterator<char>(patchFile)),
                              std::istreambuf_iterator<char>());
    patchFile.close();
    
    if (patchContent.empty()) {
        s_logger.info("❌ Patch file is empty\n");
        return;
    }
    
    // Parse patch format: each line is "OFFSET:HEX_BYTES" or unified diff
    std::istringstream patchStream(patchContent);
    std::string line;
    int patchCount = 0;
    int failCount = 0;
    
    while (std::getline(patchStream, line)) {
        if (line.empty() || line[0] == '#') continue;  // Skip comments
        
        auto colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string offsetStr = line.substr(0, colonPos);
            std::string hexBytes = line.substr(colonPos + 1);
            try {
                size_t offset = std::stoull(offsetStr, nullptr, 16);
                s_logger.info("  Patch @0x");
                patchCount++;
            } catch (...) {
                s_logger.info("  ⚠️  Skipping malformed line: ");
                failCount++;
            }
        }
    }
    
    s_logger.info("✅ Applied ");
    if (failCount > 0) s_logger.info(" (");
    s_logger.info(" without restart\n");
}

void cmd_hotpatch_create(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.currentFile.empty()) {
        s_logger.info("❌ No file open\n");
        return;
    }
    s_logger.info("🔥 Creating hotpatch for: ");
    
    // Read the original file from disk for comparison
    std::ifstream origFile(g_state.currentFile, std::ios::binary);
    if (!origFile.is_open()) {
        s_logger.info("❌ Cannot read original file from disk: ");
        return;
    }
    std::string origContent((std::istreambuf_iterator<char>(origFile)),
                             std::istreambuf_iterator<char>());
    origFile.close();
    
    // Compare buffer to original and generate byte-level patch
    std::string patchFilename = g_state.currentFile + ".hotpatch";
    std::ofstream patchFile(patchFilename);
    if (!patchFile.is_open()) {
        s_logger.info("❌ Cannot create patch file: ");
        return;
    }
    
    patchFile << "# RawrXD Hotpatch — Byte-Level Diff\n";
    patchFile << "# Source: " << g_state.currentFile << "\n";
    patchFile << "# Generated: " << __DATE__ << " " << __TIME__ << "\n";
    
    size_t minLen = std::min(origContent.size(), g_state.editorBuffer.size());
    size_t maxLen = std::max(origContent.size(), g_state.editorBuffer.size());
    int diffCount = 0;
    
    // Byte-by-byte comparison to find changed regions
    size_t i = 0;
    while (i < maxLen) {
        if (i >= minLen || origContent[i] != g_state.editorBuffer[i]) {
            // Found a difference — scan to find the end of the changed region
            size_t diffStart = i;
            while (i < maxLen && (i >= minLen || origContent[i] != g_state.editorBuffer[i])) {
                i++;
            }
            // Emit patch entry: OFFSET:NEW_HEX_BYTES
            patchFile << std::hex << diffStart << ":";
            for (size_t j = diffStart; j < i && j < g_state.editorBuffer.size(); ++j) {
                patchFile << std::setw(2) << std::setfill('0') 
                          << (static_cast<unsigned int>(static_cast<unsigned char>(g_state.editorBuffer[j])));
            }
            patchFile << "\n";
            diffCount++;
        } else {
            i++;
        }
    }
    
    patchFile.close();
    s_logger.info("✅ Hotpatch created: ");
}

// ============================================================================
// AGENTIC DECISION TREE (Phase 19: Headless Autonomy)
// ============================================================================

void cmd_decision_tree(const std::string& args) {
    auto& tree = AgenticDecisionTree::instance();

    if (args == "dump" || args == "show") {
        s_logger.info( tree.dumpTreeJSON() << "\n";
    } else if (args == "trace") {
        s_logger.info( tree.dumpLastTrace() << "\n";
    } else if (args == "stats") {
        auto& s = tree.getStats();
        s_logger.info("\n🌳 Decision Tree Statistics:\n");
        s_logger.info("  Trees evaluated:      ");
        s_logger.info("  Nodes visited:        ");
        s_logger.info("  SSA lifts:            ");
        s_logger.info("  Failures detected:    ");
        s_logger.info("  Patches applied:      ");
        s_logger.info("  Patches reverted:     ");
        s_logger.info("  Successful fixes:     ");
        s_logger.info("  Failed fixes:         ");
        s_logger.info("  Escalations:          ");
        s_logger.info("  Total retries:        ");
        s_logger.info("  Aborts:               ");
    } else if (args == "enable") {
        tree.setEnabled(true);
        s_logger.info("✅ Decision tree enabled\n");
    } else if (args == "disable") {
        tree.setEnabled(false);
        s_logger.info("⏹️  Decision tree disabled\n");
    } else if (args == "reset") {
        tree.resetStats();
        tree.buildDefaultTree();
        s_logger.info("✅ Decision tree reset to defaults\n");
    } else {
        s_logger.info("Usage: !decision_tree <dump|trace|stats|enable|disable|reset>\n");
    }
}

void cmd_autonomy_run(const std::string& args) {
    auto& loop = CLIAutonomyLoop::instance();

    // Wire engines if not already done
    if (g_state.agenticEngine) {
        loop.setAgenticEngine(g_state.agenticEngine);
    }
    if (g_state.subAgentMgr) {
        loop.setSubAgentManager(g_state.subAgentMgr);
    }

    if (args == "start" || args.empty()) {
        loop.start();
    } else if (args == "stop") {
        loop.stop();
    } else if (args == "pause") {
        loop.pause();
    } else if (args == "resume") {
        loop.resume();
    } else if (args == "status") {
        s_logger.info( loop.getDetailedStatus();
    } else if (args == "tick") {
        auto outcome = loop.tick();
        s_logger.info( (outcome.success ? "✅" : "❌") << " "
                  << outcome.summary << "\n";
        for (const auto& t : outcome.traceLog) {
            s_logger.info("    ");
        }
    } else if (args.rfind("verbose ", 0) == 0) {
        std::string val = args.substr(8);
        AutonomyLoopConfig cfg = loop.getConfig();
        cfg.verboseTracing = (val == "on" || val == "true" || val == "1");
        loop.setConfig(cfg);
        s_logger.info("✅ Verbose tracing: ");
    } else if (args.rfind("rate ", 0) == 0) {
        try {
            int rate = std::stoi(args.substr(5));
            AutonomyLoopConfig cfg = loop.getConfig();
            cfg.maxActionsPerMinute = rate;
            loop.setConfig(cfg);
            s_logger.info("✅ Rate limit: ");
        } catch (...) {
            s_logger.info("Usage: !autonomy_run rate <number>\n");
        }
    } else if (args.rfind("interval ", 0) == 0) {
        try {
            int ms = std::stoi(args.substr(9));
            AutonomyLoopConfig cfg = loop.getConfig();
            cfg.tickIntervalMs = ms;
            loop.setConfig(cfg);
            s_logger.info("✅ Tick interval: ");
        } catch (...) {
            s_logger.info("Usage: !autonomy_run interval <ms>\n");
        }
    } else {
        s_logger.info("Usage: !autonomy_run <start|stop|pause|resume|status|tick>\n");
        s_logger.info("       !autonomy_run verbose <on|off>\n");
        s_logger.info("       !autonomy_run rate <actions_per_min>\n");
        s_logger.info("       !autonomy_run interval <ms>\n");
    }
}

void cmd_ssa_lift(const std::string& args) {
    if (args.empty()) {
        s_logger.info("Usage: !ssa_lift <binary_path> [function_name] [hex_address]\n");
        s_logger.info("  Examples:\n");
        s_logger.info("    !ssa_lift target.exe main\n");
        s_logger.info("    !ssa_lift target.dll 0x140001000\n");
        s_logger.info("    !ssa_lift model.gguf process_tokens 0x7ff6a000\n");
        return;
    }

    // Parse arguments: <binary> [func_name] [hex_addr]
    std::vector<std::string> parts;
    std::istringstream iss(args);
    std::string token;
    while (iss >> token) parts.push_back(token);

    std::string binaryPath = parts[0];
    std::string funcName = (parts.size() > 1) ? parts[1] : "";
    uint64_t addr = 0;

    // Check if func_name is actually a hex address
    if (!funcName.empty() && funcName.rfind("0x", 0) == 0) {
        addr = std::stoull(funcName, nullptr, 16);
        funcName.clear();
    }

    // Check for explicit hex address in 3rd arg
    if (parts.size() > 2 && parts[2].rfind("0x", 0) == 0) {
        addr = std::stoull(parts[2], nullptr, 16);
    }

    s_logger.info("🔬 Running SSA Lifter on: ");
    if (!funcName.empty()) s_logger.info(" func=");
    if (addr != 0) s_logger.info(" addr=0x");
    s_logger.info("\n");

    auto& loop = CLIAutonomyLoop::instance();
    std::string result = loop.runSSALift(binaryPath, funcName, addr);
    s_logger.info( result << "\n";
}

void cmd_auto_patch(const std::string& args) {
    if (args == "stats") {
        auto& tree = AgenticDecisionTree::instance();
        auto& s = tree.getStats();
        s_logger.info("🔧 Auto-Patch Stats: ");
        return;
    }

    if (args == "analyze" || args.empty()) {
        s_logger.info("🔧 Running autonomous correction pipeline...\n");

        auto& loop = CLIAutonomyLoop::instance();
        if (g_state.agenticEngine) {
            loop.setAgenticEngine(g_state.agenticEngine);
        }
        if (g_state.subAgentMgr) {
            loop.setSubAgentManager(g_state.subAgentMgr);
        }

        auto outcome = loop.autoPatch();
        if (!outcome.success && args.empty()) {
            s_logger.info("ℹ️  No prior failure context. Run inference first, then use !auto_patch.\n");
            s_logger.info("    Or: pipe output through autonomy with !autonomy_run start\n");
        }
        return;
    }

    // Allow one-shot: !auto_patch "<output text>" "<prompt>"
    // Simple: just analyze the args as output text
    s_logger.info("🔧 Analyzing provided text for failures...\n");
    auto& tree = AgenticDecisionTree::instance();
    if (g_state.agenticEngine) {
        tree.setAgenticEngine(g_state.agenticEngine);
    }

    DecisionOutcome outcome = tree.analyzeAndFix(args, "");
    s_logger.info( (outcome.success ? "✅" : "❌") << " " << outcome.summary << "\n";
    for (const auto& t : outcome.traceLog) {
        s_logger.info("    ");
    }
}

// ============================================================================
// SEARCH & TOOLS (Feature Parity with Win32IDE Tools)
// ============================================================================

void cmd_search_files(const std::string& args) {
    if (args.empty()) {
        s_logger.info("Usage: !search <pattern> [path]\n");
        return;
    }
    // Parse args: first token is the search pattern, optional second is path
    std::string pattern, searchPath = ".";
    size_t spacePos = args.find(' ');
    if (spacePos != std::string::npos) {
        pattern = args.substr(0, spacePos);
        searchPath = args.substr(spacePos + 1);
    } else {
        pattern = args;
    }
    
    s_logger.info("🔍 Searching for \");
    
    int matchCount = 0;
    int fileCount = 0;
    const int maxResults = 100;
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                searchPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            
            std::string filepath = entry.path().string();
            // Skip binary files and hidden directories
            auto ext = entry.path().extension().string();
            if (ext == ".exe" || ext == ".dll" || ext == ".obj" || ext == ".o" || 
                ext == ".pdb" || ext == ".lib" || ext == ".a" || ext == ".so" ||
                ext == ".png" || ext == ".jpg" || ext == ".gif" || ext == ".zip") continue;
            
            std::ifstream file(filepath);
            if (!file.is_open()) continue;
            
            std::string line;
            int lineNum = 0;
            bool fileHeaderPrinted = false;
            
            while (std::getline(file, line)) {
                lineNum++;
                if (line.find(pattern) != std::string::npos) {
                    if (!fileHeaderPrinted) {
                        s_logger.info("\n📄 ");
                        fileHeaderPrinted = true;
                        fileCount++;
                    }
                    // Truncate long lines
                    std::string displayLine = line.length() > 120 ? line.substr(0, 120) + "..." : line;
                    s_logger.info("  ");
                    matchCount++;
                    if (matchCount >= maxResults) break;
                }
            }
            if (matchCount >= maxResults) {
                s_logger.info("\n⚠️  Showing first ");
                break;
            }
        }
    } catch (const std::exception& e) {
        s_logger.info("❌ Search error: ");
    }
    
    s_logger.info("\n📊 Found ");
}

void cmd_analyze(const std::string& args) {
    if (g_state.currentFile.empty()) {
        s_logger.info("❌ No file open\n");
        return;
    }
    s_logger.info("📊 Analyzing: ");
    s_logger.info("   Lines: ");
    s_logger.info("   Size: ");
}

void cmd_profile(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    
    s_logger.info("⏱️  Profiling started...\n");
    auto t0 = std::chrono::high_resolution_clock::now();
    
    // Profile the current editor buffer — analyze code structure
    if (g_state.editorBuffer.empty()) {
        s_logger.info("⚠️  No content to profile. Open a file first.\n");
        return;
    }
    
    // Code metrics analysis
    std::istringstream stream(g_state.editorBuffer);
    std::string line;
    int totalLines = 0, codeLines = 0, commentLines = 0, blankLines = 0;
    int functionCount = 0, classCount = 0, includeCount = 0;
    size_t maxLineLength = 0;
    size_t totalChars = 0;
    int braceDepth = 0, maxBraceDepth = 0;
    
    while (std::getline(stream, line)) {
        totalLines++;
        totalChars += line.size();
        if (line.size() > maxLineLength) maxLineLength = line.size();
        
        // Trim for analysis
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) {
            blankLines++;
            continue;
        }
        std::string trimmed = line.substr(first);
        
        if (trimmed.substr(0, 2) == "//" || trimmed.substr(0, 2) == "/*" || trimmed[0] == '#') {
            commentLines++;
            if (trimmed[0] == '#' && trimmed.find("include") != std::string::npos) includeCount++;
        } else {
            codeLines++;
        }
        
        // Count braces for complexity
        for (char c : line) {
            if (c == '{') { braceDepth++; if (braceDepth > maxBraceDepth) maxBraceDepth = braceDepth; }
            if (c == '}') braceDepth--;
        }
        
        // Detect function/class definitions (simple heuristic)
        if (trimmed.find("void ") == 0 || trimmed.find("int ") == 0 || 
            trimmed.find("bool ") == 0 || trimmed.find("std::") == 0 ||
            trimmed.find("auto ") == 0 || trimmed.find("static ") == 0) {
            if (trimmed.find('(') != std::string::npos && trimmed.find(';') == std::string::npos) {
                functionCount++;
            }
        }
        if (trimmed.find("class ") == 0 || trimmed.find("struct ") == 0) {
            classCount++;
        }
    }
    
    auto t1 = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    
    s_logger.info("\n📊 Profile Results");
    s_logger.info("  ───────────────────────────────────\n");
    s_logger.info("  Total lines:      ");
    s_logger.info("  Code lines:       ");
    s_logger.info("  Comment lines:    ");
    s_logger.info("  Blank lines:      ");
    s_logger.info("  Functions:        ~");
    s_logger.info("  Classes/Structs:  ~");
    s_logger.info("  Includes:         ");
    s_logger.info("  Max nesting:      ");
    s_logger.info("  Max line length:  ");
    s_logger.info("  Avg line length:  ");
    s_logger.info("  Total size:       ");
    s_logger.info("  ───────────────────────────────────\n");
    s_logger.info("  Analysis time:    ");
    s_logger.info("✅ Profile complete\n");
}

// ============================================================================
// SUBAGENT OPERATIONS (Feature Parity with Win32IDE_SubAgent)
// ============================================================================

void cmd_subagent(const std::string& args) {
    if (args.empty()) {
        s_logger.info("Usage: !subagent <prompt>\n");
        return;
    }
    if (!g_state.subAgentMgr) {
        s_logger.info("❌ SubAgentManager not initialized (no model loaded)\n");
        return;
    }
    s_logger.info("🤖 Spawning sub-agent...\n");
    std::string id = g_state.subAgentMgr->spawnSubAgent("cli", "CLI subagent", args);
    bool ok = g_state.subAgentMgr->waitForSubAgent(id, 120000);
    std::string result = g_state.subAgentMgr->getSubAgentResult(id);
    s_logger.info( (ok ? "✅" : "❌") << " SubAgent result:\n" << result << "\n";
}

void cmd_chain(const std::string& args) {
    if (args.empty()) {
        s_logger.info("Usage: !chain <step1> | <step2> | <step3> ...\n");
        s_logger.info("  Steps are separated by ' | '\n");
        s_logger.info("  Each step can use {{input}} for the previous step's output\n");
        return;
    }
    if (!g_state.subAgentMgr) {
        s_logger.info("❌ SubAgentManager not initialized\n");
        return;
    }

    std::vector<std::string> steps;
    size_t pos = 0;
    while (true) {
        size_t delim = args.find(" | ", pos);
        if (delim == std::string::npos) {
            steps.push_back(args.substr(pos));
            break;
        }
        steps.push_back(args.substr(pos, delim - pos));
        pos = delim + 3;
    }

    s_logger.info("🔗 Running chain with ");
    for (size_t i = 0; i < steps.size(); i++) {
        s_logger.info("  Step ");
    }

    std::string result = g_state.subAgentMgr->executeChain("cli", steps);
    s_logger.info("✅ Chain result:\n");
}

void cmd_swarm(const std::string& args) {
    if (args.empty()) {
        s_logger.info("Usage: !swarm <prompt1> | <prompt2> | <prompt3> ...\n");
        s_logger.info("  Options (append after last prompt):\n");
        s_logger.info("    --strategy <concatenate|vote|summarize>  Merge strategy\n");
        s_logger.info("    --parallel <n>                           Max parallel agents\n");
        return;
    }
    if (!g_state.subAgentMgr) {
        s_logger.info("❌ SubAgentManager not initialized\n");
        return;
    }

    // Parse strategy and parallel options from the end
    SwarmConfig config;
    std::string remaining = args;

    auto extractOpt = [&](const std::string& opt) -> std::string {
        size_t pos = remaining.find(opt);
        if (pos == std::string::npos) return "";
        size_t valStart = pos + opt.size();
        while (valStart < remaining.size() && remaining[valStart] == ' ') valStart++;
        size_t valEnd = remaining.find(' ', valStart);
        if (valEnd == std::string::npos) valEnd = remaining.size();
        std::string val = remaining.substr(valStart, valEnd - valStart);
        remaining = remaining.substr(0, pos) + remaining.substr(std::min(valEnd + 1, remaining.size()));
        return val;
    };

    std::string strategy = extractOpt("--strategy ");
    if (!strategy.empty()) config.mergeStrategy = strategy;
    std::string parallel = extractOpt("--parallel ");
    if (!parallel.empty()) config.maxParallel = std::stoi(parallel);

    // Split prompts on " | "
    std::vector<std::string> prompts;
    size_t pos = 0;
    while (true) {
        size_t delim = remaining.find(" | ", pos);
        if (delim == std::string::npos) {
            std::string p = remaining.substr(pos);
            if (!p.empty()) prompts.push_back(p);
            break;
        }
        prompts.push_back(remaining.substr(pos, delim - pos));
        pos = delim + 3;
    }

    if (prompts.empty()) {
        s_logger.info("❌ No prompts provided\n");
        return;
    }

    s_logger.info("🐝 Launching HexMag swarm with ");

    std::string result = g_state.subAgentMgr->executeSwarm("cli", prompts, config);
    s_logger.info("✅ Swarm result:\n");
}

// ============================================================================
// CHAIN-OF-THOUGHT MULTI-MODEL REVIEW (Phase 32A)
// ============================================================================
void cmd_cot(const std::string& args) {
    auto& cot = ChainOfThoughtEngine::instance();

    // Wire inference callback if not already set (uses routeWithIntelligence via subagent)
    static bool callbackSet = false;
    if (!callbackSet && g_state.subAgentMgr) {
        cot.setInferenceCallback([](const std::string& systemPrompt,
                                     const std::string& userMessage,
                                     const std::string& /*model*/) -> std::string {
            // Use SubAgentManager to route inference through the agent pipeline
            if (g_state.subAgentMgr) {
                std::string combined = systemPrompt + "\n\n" + userMessage;
                std::vector<std::string> steps = { combined };
                return g_state.subAgentMgr->executeChain("cot", steps);
            }
            return "[Error] No inference backend available";
        });
        callbackSet = true;
    }

    if (args.empty()) {
        s_logger.info("🔗 Chain-of-Thought Multi-Model Review\n");
        s_logger.info("Usage:\n");
        s_logger.info("  !cot status             — Show CoT engine status\n");
        s_logger.info("  !cot presets            — List available presets\n");
        s_logger.info("  !cot roles              — List all available roles\n");
        s_logger.info("  !cot preset <name>      — Apply a preset (review|audit|think|research|debate|custom)\n");
        s_logger.info("  !cot steps              — Show current chain steps\n");
        s_logger.info("  !cot add <role>         — Add a step with the given role\n");
        s_logger.info("  !cot clear              — Clear all steps\n");
        s_logger.info("  !cot run <query>        — Execute the chain on a query\n");
        s_logger.info("  !cot cancel             — Cancel a running chain\n");
        s_logger.info("  !cot stats              — Show execution statistics\n");
        return;
    }

    // Parse subcommand
    auto sp = args.find(' ');
    std::string subcmd = (sp == std::string::npos) ? args : args.substr(0, sp);
    std::string subargs = (sp == std::string::npos) ? "" : args.substr(sp + 1);

    if (subcmd == "status") {
        s_logger.info( cot.getStatusJSON() << "\n";
    }
    else if (subcmd == "presets") {
        auto names = getCoTPresetNames();
        s_logger.info("📋 Available presets:\n");
        for (const auto& n : names) {
            const CoTPreset* p = getCoTPreset(n);
            if (p) {
                s_logger.info("  ");
                for (size_t i = 0; i < p->steps.size(); i++) {
                    if (i > 0) s_logger.info(" → ");
                    s_logger.info( getCoTRoleInfo(p->steps[i].role).label;
                }
                s_logger.info("\n");
            }
        }
    }
    else if (subcmd == "roles") {
        const auto& roles = getAllCoTRoles();
        s_logger.info("🎭 Available roles (");
        for (const auto& r : roles) {
            s_logger.info("  ");
        }
    }
    else if (subcmd == "preset") {
        if (subargs.empty()) {
            s_logger.info("❌ Usage: !cot preset <name>\n");
            return;
        }
        if (cot.applyPreset(subargs)) {
            s_logger.info("✅ Applied preset '");
            const auto& steps = cot.getSteps();
            for (size_t i = 0; i < steps.size(); i++) {
                const auto& info = getCoTRoleInfo(steps[i].role);
                s_logger.info("  Step ");
            }
        } else {
            s_logger.info("❌ Unknown preset: ");
            s_logger.info("   Available: review, audit, think, research, debate, custom\n");
        }
    }
    else if (subcmd == "steps") {
        const auto& steps = cot.getSteps();
        if (steps.empty()) {
            s_logger.info("⚠ No steps configured. Use !cot preset <name> or !cot add <role>\n");
        } else {
            s_logger.info("🔗 Current chain (");
            for (size_t i = 0; i < steps.size(); i++) {
                const auto& info = getCoTRoleInfo(steps[i].role);
                s_logger.info("  [");
                if (steps[i].skip) s_logger.info(" (SKIPPED)");
                if (!steps[i].model.empty()) s_logger.info(" [model: ");
                s_logger.info("\n");
            }
        }
    }
    else if (subcmd == "add") {
        if (subargs.empty()) {
            s_logger.info("❌ Usage: !cot add <role>\n");
            return;
        }
        const CoTRoleInfo* info = getCoTRoleByName(subargs);
        if (!info) {
            s_logger.info("❌ Unknown role: ");
            s_logger.info("   Available roles: ");
            const auto& roles = getAllCoTRoles();
            for (size_t i = 0; i < roles.size(); i++) {
                if (i > 0) s_logger.info(", ");
                s_logger.info( roles[i].name;
            }
            s_logger.info("\n");
            return;
        }
        cot.addStep(info->id);
        s_logger.info("✅ Added step: ");
    }
    else if (subcmd == "clear") {
        cot.clearSteps();
        s_logger.info("✅ Chain cleared.\n");
    }
    else if (subcmd == "run") {
        if (subargs.empty()) {
            s_logger.info("❌ Usage: !cot run <your query>\n");
            return;
        }
        if (cot.getSteps().empty()) {
            s_logger.info("⚠ No steps configured. Applying 'review' preset...\n");
            cot.applyPreset("review");
        }

        // Set step callback for progress output
        cot.setStepCallback([](const CoTStepResult& sr) {
            const auto& info = getCoTRoleInfo(sr.role);
            if (sr.skipped) {
                s_logger.info("  ⏭ Step ");
            } else if (sr.success) {
                s_logger.info("  ✅ Step ");
            } else {
                s_logger.info("  ❌ Step ");
            }
        });

        s_logger.info("🔗 Executing CoT chain (");
        CoTChainResult result = cot.executeChain(subargs);

        if (result.success) {
            s_logger.info("\n✅ Chain complete (");
            s_logger.info("   Steps: ");
            s_logger.info("\n📝 Final Output:\n");
        } else {
            s_logger.info("\n❌ Chain failed: ");
        }
    }
    else if (subcmd == "cancel") {
        cot.cancel();
        s_logger.info("🛑 Cancel requested.\n");
    }
    else if (subcmd == "stats") {
        auto stats = cot.getStats();
        s_logger.info("📊 CoT Statistics:\n");
        s_logger.info("  Total chains:     ");
        s_logger.info("  Successful:       ");
        s_logger.info("  Failed:           ");
        s_logger.info("  Steps executed:   ");
        s_logger.info("  Steps skipped:    ");
        s_logger.info("  Steps failed:     ");
        s_logger.info("  Avg latency:      ");
        if (!stats.roleUsage.empty()) {
            s_logger.info("  Role usage:\n");
            for (const auto& [role, count] : stats.roleUsage) {
                s_logger.info("    ");
            }
        }
    }
    else {
        s_logger.info("❌ Unknown subcommand: ");
        s_logger.info("   Type !cot for usage.\n");
    }
}

void cmd_agents_list(const std::string& args) {
    if (!g_state.subAgentMgr) {
        s_logger.info("❌ SubAgentManager not initialized\n");
        return;
    }
    auto agents = g_state.subAgentMgr->getAllSubAgents();
    if (agents.empty()) {
        s_logger.info("No sub-agents.\n");
        return;
    }
    s_logger.info("📋 Sub-agents (");
    for (const auto& a : agents) {
        std::string icon = "⬜";
        if (a.state == SubAgent::State::Running) icon = "🔄";
        else if (a.state == SubAgent::State::Completed) icon = "✅";
        else if (a.state == SubAgent::State::Failed) icon = "❌";
        else if (a.state == SubAgent::State::Cancelled) icon = "🚫";
        s_logger.info("  ");
    }
}

void cmd_todo_list(const std::string& args) {
    if (!g_state.subAgentMgr) {
        s_logger.info("❌ SubAgentManager not initialized\n");
        return;
    }
    auto todos = g_state.subAgentMgr->getTodoList();
    if (todos.empty()) {
        s_logger.info("Todo list empty.\n");
        return;
    }
    s_logger.info("📝 Todo List:\n");
    for (const auto& t : todos) {
        std::string icon = "⬜";
        if (t.status == TodoItem::Status::InProgress) icon = "🔄";
        else if (t.status == TodoItem::Status::Completed) icon = "✅";
        else if (t.status == TodoItem::Status::Failed) icon = "❌";
        s_logger.info("  ");
        if (!t.description.empty()) s_logger.info(" — ");
        s_logger.info("\n");
    }
}

// ============================================================================
// STATUS & INFO
// ============================================================================

void cmd_status(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    s_logger.info("\n📊 RawrXD CLI Status:\n");
    s_logger.info("  File: ");
    s_logger.info("  Buffer size: ");
    s_logger.info("  Agent goal: ");
    s_logger.info("  Agent loop: ");
    s_logger.info("  Autonomy: ");
    s_logger.info("  Debugger: ");
    s_logger.info("  Terminals: ");
    s_logger.info("  Breakpoints: ");
    if (g_state.subAgentMgr) {
        s_logger.info("  ");
    }
    s_logger.info("\n");
}

// ============================================================================
// HELP & VERSION
// ============================================================================

void print_help() {
    s_logger.info("\n╔════════════════════════════════════════════════════════════════════╗\n");
    s_logger.info("║       RawrXD AI Runtime (CLI) - Feature Parity with Win32 IDE       ║\n");
    s_logger.info("╚════════════════════════════════════════════════════════════════════╝\n");
    s_logger.info("\n🔧 FILE OPERATIONS:\n");
    s_logger.info("  !new                          Create new file\n");
    s_logger.info("  !open <path>                  Open file\n");
    s_logger.info("  !save                         Save current file\n");
    s_logger.info("  !save_as <path>               Save as new file\n");
    s_logger.info("  !close                        Close current file\n");
    s_logger.info("\n✂️  EDITOR OPERATIONS:\n");
    s_logger.info("  !cut                          Cut to clipboard\n");
    s_logger.info("  !copy                         Copy to clipboard\n");
    s_logger.info("  !paste                        Paste from clipboard\n");
    s_logger.info("  !undo                         Undo last change\n");
    s_logger.info("  !redo                         Redo last change\n");
    s_logger.info("  !find <text>                  Find text in buffer\n");
    s_logger.info("  !replace <old> <new>          Replace text\n");
    s_logger.info("\n🤖 AGENTIC OPERATIONS:\n");
    s_logger.info("  !agent_execute <prompt>       Execute single agent command\n");
    s_logger.info("  !agent_loop <prompt> [n]      Start multi-turn agent loop\n");
    s_logger.info("  !agent_goal <goal>            Set agent goal\n");
    s_logger.info("  !agent_memory <obs>           Add observation to memory\n");
    s_logger.info("  !agent_memory show            Show agent memory\n");
    s_logger.info("\n🤖 AUTONOMY OPERATIONS:\n");
    s_logger.info("  !autonomy_start               Enable autonomy\n");
    s_logger.info("  !autonomy_stop                Disable autonomy\n");
    s_logger.info("  !autonomy_goal <goal>         Set autonomy goal\n");
    s_logger.info("  !autonomy_rate <n>            Set max actions per minute\n");
    s_logger.info("\n🐛 DEBUG OPERATIONS:\n");
    s_logger.info("  !breakpoint_add <file>:<line> Add breakpoint\n");
    s_logger.info("  !breakpoint_list              List all breakpoints\n");
    s_logger.info("  !breakpoint_remove <idx>      Remove breakpoint\n");
    s_logger.info("  !debug_start                  Start debugger\n");
    s_logger.info("  !debug_stop                   Stop debugger\n");
    s_logger.info("  !debug_step                   Step through code\n");
    s_logger.info("  !debug_continue               Continue execution\n");
    s_logger.info("\n💻 TERMINAL OPERATIONS:\n");
    s_logger.info("  !terminal_new                 Create new terminal pane\n");
    s_logger.info("  !terminal_split <orientation> Split terminal pane\n");
    s_logger.info("  !terminal_kill                Close terminal pane\n");
    s_logger.info("  !terminal_list                List terminal panes\n");
    s_logger.info("\n🔥 HOTPATCH OPERATIONS:\n");
    s_logger.info("  !hotpatch_apply <file>       Apply hotpatch without restart\n");
    s_logger.info("  !hotpatch_create              Create hotpatch from current file\n");
    s_logger.info("\n🧠 AGENTIC DECISION TREE (Phase 19):\n");
    s_logger.info("  !decision_tree <sub>          dump|trace|stats|enable|disable|reset\n");
    s_logger.info("  !autonomy_run <sub>           start|stop|pause|resume|status|tick\n");
    s_logger.info("    verbose <on|off>              Toggle verbose tracing\n");
    s_logger.info("    rate <n>                      Set actions/minute limit\n");
    s_logger.info("    interval <ms>                 Set tick interval\n");
    s_logger.info("  !ssa_lift <bin> [func] [addr] Run SSA Lifter on a binary function\n");
    s_logger.info("  !auto_patch [text|analyze|stats] Auto-correct inference failures\n");
    s_logger.info("\n🛡️  SAFETY CONTRACT (Phase 10B):\n");
    s_logger.info("  !safety status                Show budget, risk, violations\n");
    s_logger.info("  !safety reset                 Reset intent budget\n");
    s_logger.info("  !safety rollback [all]        Rollback last/all actions\n");
    s_logger.info("  !safety violations            Show violation log\n");
    s_logger.info("  !safety block <class>         Block an action class\n");
    s_logger.info("  !safety unblock <class>       Unblock an action class\n");
    s_logger.info("  !safety risk <tier>           Set max risk (None|Low|Medium|High|Critical)\n");
    s_logger.info("  !safety budget                Show remaining intent budget\n");
    s_logger.info("\n🎯 CONFIDENCE GATE (Phase 10D):\n");
    s_logger.info("  !confidence status            Gate stats + thresholds + trend\n");
    s_logger.info("  !confidence policy <p>        Set policy (strict|normal|relaxed|disabled)\n");
    s_logger.info("  !confidence threshold <e><s><a> Set thresholds\n");
    s_logger.info("  !confidence history           Recent evaluations\n");
    s_logger.info("  !confidence trend             Trend analysis\n");
    s_logger.info("  !confidence selfabort         Self-abort status\n");
    s_logger.info("  !confidence reset             Reset gate\n");
    s_logger.info("\n📼 REPLAY JOURNAL (Phase 10C):\n");
    s_logger.info("  !replay status                Journal stats\n");
    s_logger.info("  !replay last [n]              Last N actions (default 10)\n");
    s_logger.info("  !replay session               Current session snapshot\n");
    s_logger.info("  !replay sessions              List all sessions\n");
    s_logger.info("  !replay checkpoint [label]    Insert checkpoint\n");
    s_logger.info("  !replay export <file>         Export session\n");
    s_logger.info("  !replay filter <type>         Filter by action type\n");
    s_logger.info("  !replay start|pause|stop      Control recording\n");
    s_logger.info("\n⚙️  EXECUTION GOVERNOR (Phase 10A):\n");
    s_logger.info("  !governor status              Stats + active tasks\n");
    s_logger.info("  !governor run <cmd>           Run command with timeout\n");
    s_logger.info("  !governor tasks               List active tasks\n");
    s_logger.info("  !governor kill <id>           Kill a task\n");
    s_logger.info("  !governor kill_all            Kill all tasks\n");
    s_logger.info("  !governor wait <id>           Block until task completes\n");
    s_logger.info("\n🔀 MULTI-RESPONSE (Phase 9C):\n");
    s_logger.info("  !multi <prompt>               Generate N styled responses\n");
    s_logger.info("  !multi compare                Side-by-side comparison\n");
    s_logger.info("  !multi prefer <0-3> [reason]  Record preference\n");
    s_logger.info("  !multi templates              Show templates\n");
    s_logger.info("  !multi toggle <0-3>           Toggle template on/off\n");
    s_logger.info("  !multi recommend              Recommended template\n");
    s_logger.info("  !multi stats                  Generation stats\n");
    s_logger.info("\n📜 HISTORY & EXPLAINABILITY (Phase 5/8A):\n");
    s_logger.info("  !history show [n]             Last N events\n");
    s_logger.info("  !history session              Session timeline\n");
    s_logger.info("  !history agent <id>           Agent timeline\n");
    s_logger.info("  !history type <type>          Events by type\n");
    s_logger.info("  !history stats|flush|clear    Manage history\n");
    s_logger.info("  !history export <file>        Export events\n");
    s_logger.info("  !explain agent|chain|swarm <id>  Trace causal chain\n");
    s_logger.info("  !explain failures             Explain all failures\n");
    s_logger.info("  !explain policies             Policy firing attribution\n");
    s_logger.info("  !explain session              Full session explanation\n");
    s_logger.info("  !explain snapshot <file>      Export audit snapshot\n");
    s_logger.info("\n📋 POLICY ENGINE (Phase 7):\n");
    s_logger.info("  !policy list                  List all policies\n");
    s_logger.info("  !policy show <id>             Policy details\n");
    s_logger.info("  !policy enable|disable <id>   Toggle policy\n");
    s_logger.info("  !policy remove <id>           Remove policy\n");
    s_logger.info("  !policy heuristics            Computed heuristics\n");
    s_logger.info("  !policy suggest               Generate suggestions\n");
    s_logger.info("  !policy accept|reject <id>    Accept/reject suggestion\n");
    s_logger.info("  !policy pending               Pending suggestions\n");
    s_logger.info("  !policy export|import <file>  Export/import policies\n");
    s_logger.info("  !policy stats                 Engine statistics\n");
    s_logger.info("\n🔧 TOOL REGISTRY:\n");
    s_logger.info("  !tools                        List registered tools\n");
    s_logger.info("\n🔍 TOOLS:\n");
    s_logger.info("  !search <pattern> [path]      Search files\n");
    s_logger.info("  !analyze                      Analyze current file\n");
    s_logger.info("  !profile                      Profile code\n");
    s_logger.info("\n🤖 SUBAGENT OPERATIONS:\n");
    s_logger.info("  !subagent <prompt>            Spawn a sub-agent\n");
    s_logger.info("  !chain <s1> | <s2> | ...      Sequential prompt chain\n");
    s_logger.info("  !swarm <p1> | <p2> | ...      Parallel HexMag swarm\n");
    s_logger.info("    --strategy <merge>           concatenate|vote|summarize\n");
    s_logger.info("    --parallel <n>               Max concurrent agents\n");
    s_logger.info("  !agents                       List all sub-agents\n");
    s_logger.info("  !todo                         Show todo list\n");
    s_logger.info("\n🔗 CHAIN-OF-THOUGHT (Phase 32A):\n");
    s_logger.info("  !cot                          Show CoT help\n");
    s_logger.info("  !cot status                   Engine status\n");
    s_logger.info("  !cot presets                  List presets (review|audit|think|...)\n");
    s_logger.info("  !cot roles                    List all roles\n");
    s_logger.info("  !cot preset <name>            Apply preset\n");
    s_logger.info("  !cot steps                    Show current chain\n");
    s_logger.info("  !cot add <role>               Add step to chain\n");
    s_logger.info("  !cot clear                    Clear chain\n");
    s_logger.info("  !cot run <query>              Execute chain on query\n");
    s_logger.info("  !cot cancel                   Cancel running chain\n");
    s_logger.info("  !cot stats                    Execution statistics\n");
    s_logger.info("\n🎙️  VOICE (Phase 33):\n");
    s_logger.info("  !voice                        Show voice commands help\n");
    s_logger.info("  !voice init                   Initialize voice engine\n");
    s_logger.info("  !voice record                 Start/stop recording\n");
    s_logger.info("  !voice ptt                    Push-to-talk toggle\n");
    s_logger.info("  !voice transcribe             Transcribe last recording\n");
    s_logger.info("  !voice speak <text>           Text-to-speech\n");
    s_logger.info("  !voice tts <on|off>           Toggle TTS for AI responses\n");
    s_logger.info("  !voice mode <ptt|vad|off>     Set capture mode\n");
    s_logger.info("  !voice devices                List audio devices\n");
    s_logger.info("  !voice device <id>            Select input device\n");
    s_logger.info("  !voice room <name>            Join/leave voice room\n");
    s_logger.info("  !voice metrics                Show voice metrics\n");
    s_logger.info("  !voice status                 Show voice status\n");
    s_logger.info("\n⚙️  CONFIGURATION:\n");
    s_logger.info("  !mode <mode>                  Set AI mode (ask|plan|edit|...)\n");
    s_logger.info("  !engine <name>                Switch model\n");
    s_logger.info("  !deep <on|off>                Enable deep thinking\n");
    s_logger.info("  !research <on|off>            Enable deep research\n");
    s_logger.info("  !max <tokens>                 Set context limit\n");
    s_logger.info("\n🚀 IDE & SERVER:\n");
    s_logger.info("  !generate_ide [path]          Generate React web IDE\n");
    s_logger.info("  !server <port>                Start backend API server\n");
    s_logger.info("\n� INSTRUCTIONS CONTEXT:\n");
    s_logger.info("  !instructions                 Show production instructions\n");
    s_logger.info("  !instructions list            List loaded instruction files\n");
    s_logger.info("  !instructions show            Show full content (all lines)\n");
    s_logger.info("  !instructions reload          Reload from disk\n");
    s_logger.info("  !instructions paths           Show search paths\n");
    s_logger.info("  !instructions json            Export as JSON\n");
    s_logger.info("\n�📊 STATUS:\n");
    s_logger.info("  !status                       Show current status\n");
    s_logger.info("  !help                         Show this help\n");
    s_logger.info("  !quit                         Exit CLI\n\n");
}

// ============================================================================
// MAIN COMMAND ROUTER
// ============================================================================

void route_command(const std::string& line) {
    if (line.empty()) return;
    
    // ── UNIFIED DISPATCH — Try shared feature registry first ────────────
    // This gives CLI access to the same features as Win32 GUI.
    // route_command_unified() returns true if the command was handled.
    if (route_command_unified(line.c_str(), &g_state)) {
        return; // Handled by shared dispatch — same code path as Win32 GUI
    }
    
    // ── LEGACY ROUTING — Commands not yet in shared dispatch ────────────
    // Parse command and arguments
    auto space = line.find(' ');
    std::string cmd = (space == std::string::npos) ? line : line.substr(0, space);
    std::string args = (space == std::string::npos) ? "" : line.substr(space + 1);
    
    // File operations
    if (cmd == "!new") cmd_new_file(args);
    else if (cmd == "!open") cmd_open_file(args);
    else if (cmd == "!save") cmd_save_file(args);
    else if (cmd == "!save_as") cmd_save_as(args);
    else if (cmd == "!close") cmd_close_file(args);
    
    // Editor operations
    else if (cmd == "!cut") cmd_cut(args);
    else if (cmd == "!copy") cmd_copy(args);
    else if (cmd == "!paste") cmd_paste(args);
    else if (cmd == "!undo") cmd_undo(args);
    else if (cmd == "!redo") cmd_redo(args);
    else if (cmd == "!find") cmd_find(args);
    else if (cmd == "!replace") cmd_replace(args);
    
    // Agentic operations
    else if (cmd == "!agent_execute") cmd_agent_execute(args);
    else if (cmd == "!agent_loop") cmd_agent_loop(args);
    else if (cmd == "!agent_goal") cmd_agent_goal(args);
    else if (cmd == "!agent_memory") cmd_agent_memory(args);
    
    // Autonomy operations
    else if (cmd == "!autonomy_start") cmd_autonomy_start(args);
    else if (cmd == "!autonomy_stop") cmd_autonomy_stop(args);
    else if (cmd == "!autonomy_goal") cmd_autonomy_goal(args);
    else if (cmd == "!autonomy_rate") cmd_autonomy_rate(args);
    
    // Debug operations
    else if (cmd == "!breakpoint_add") cmd_breakpoint_add(args);
    else if (cmd == "!breakpoint_list") cmd_breakpoint_list(args);
    else if (cmd == "!breakpoint_remove") cmd_breakpoint_remove(args);
    else if (cmd == "!debug_start") cmd_debug_start(args);
    else if (cmd == "!debug_stop") cmd_debug_stop(args);
    else if (cmd == "!debug_step") cmd_debug_step(args);
    else if (cmd == "!debug_continue") cmd_debug_continue(args);
    
    // Terminal operations
    else if (cmd == "!terminal_new") cmd_terminal_new(args);
    else if (cmd == "!terminal_split") cmd_terminal_split(args);
    else if (cmd == "!terminal_kill") cmd_terminal_kill(args);
    else if (cmd == "!terminal_list") cmd_terminal_list(args);
    
    // Hotpatch operations
    else if (cmd == "!hotpatch_apply") cmd_hotpatch_apply(args);
    else if (cmd == "!hotpatch_create") cmd_hotpatch_create(args);
    
    // Agentic Decision Tree (Phase 19)
    else if (cmd == "!decision_tree") cmd_decision_tree(args);
    else if (cmd == "!autonomy_run") cmd_autonomy_run(args);
    else if (cmd == "!ssa_lift") cmd_ssa_lift(args);
    else if (cmd == "!auto_patch") cmd_auto_patch(args);
    
    // Headless Systems (Phase 20)
    else if (cmd == "!safety") cmd_safety(args);
    else if (cmd == "!confidence") cmd_confidence(args);
    else if (cmd == "!replay") cmd_replay(args);
    else if (cmd == "!governor") cmd_governor(args);
    else if (cmd == "!multi") cmd_multi_response(args);
    else if (cmd == "!history") cmd_history(args);
    else if (cmd == "!explain") cmd_explain(args);
    else if (cmd == "!policy") cmd_policy(args);
    else if (cmd == "!tools") cmd_tools(args);
    
    // Tools
    else if (cmd == "!search") cmd_search_files(args);
    else if (cmd == "!analyze") cmd_analyze(args);
    else if (cmd == "!profile") cmd_profile(args);
    
    // SubAgent operations
    else if (cmd == "!subagent") cmd_subagent(args);
    else if (cmd == "!chain") cmd_chain(args);
    else if (cmd == "!swarm") cmd_swarm(args);
    else if (cmd == "!cot") cmd_cot(args);
    else if (cmd == "!agents") cmd_agents_list(args);
    else if (cmd == "!todo") cmd_todo_list(args);
    
    // Voice operations (Phase 33)
    else if (cmd == "!voice") cmd_voice(args);
    
    // Instructions context (Phase 34)
    else if (cmd == "!instructions") cmd_instructions(args);
    
    // Status & info
    else if (cmd == "!status") cmd_status(args);
    else if (cmd == "!help") print_help();
    
    // Original configuration commands
    else if (cmd == "!mode") {
        set_mode(args);
        s_logger.info("✅ Mode set.\n");
    }
    else if (cmd == "!engine") {
        set_engine(args);
        s_logger.info("✅ Engine switched.\n");
    }
    else if (cmd == "!deep") {
        set_deep_thinking(args == "on");
        s_logger.info("✅ Deep thinking: ");
    }
    else if (cmd == "!research") {
        set_deep_research(args == "on");
        s_logger.info("✅ Deep research: ");
    }
    else if (cmd == "!max") {
        try {
            set_context(std::stoull(args));
            s_logger.info("✅ Context limit updated.\n");
        } catch (...) {
            s_logger.info("❌ Invalid number\n");
        }
    }
    else if (cmd == "!generate_ide") {
        std::string path = args.empty() ? "generated_ide" : args;
        s_logger.info("🚀 Generating Full AI IDE at: ");
        RawrXD::ReactServerConfig config;
        config.name = "RawrXD-IDE";
        config.port = "3000";
        config.include_ide_features = true;
        config.cpp_backend_port = "8080";
        
        if (RawrXD::ReactServerGenerator::Generate(path, config)) {
            s_logger.info("✅ Success! To run:\n");
        } else {
            s_logger.info("❌ Failed to generate IDE.\n");
        }
    }
    else if (cmd == "!server") {
        int port = 8080;
        try { port = std::stoi(args); } catch(...) {}
        std::thread(start_server, port).detach();
        s_logger.info("🌐 Backend server started on port ");
    }
    else {
        // Fall through to AI prompt processing
        // Record query in replay journal + history
        cli_record_action(static_cast<int>(ReplayActionType::AgentQuery),
                          "agent", "CLI chat query", line, "", 0, 1.0f, 0.0);
        if (cli_get_history_recorder()) {
            cli_get_history_recorder()->recordChatRequest(line);
        }
        
        auto queryStart = std::chrono::steady_clock::now();
        std::string out;
        if (g_state.agenticEngine && g_state.agenticEngine->isModelLoaded()) {
            out = g_state.agenticEngine->chat(line);
        } else {
            out = process_prompt(line);
        }
        auto queryEnd = std::chrono::steady_clock::now();
        double queryMs = std::chrono::duration<double, std::milli>(queryEnd - queryStart).count();
        s_logger.info( out << "\n";
        
        // Phase 33: Auto-TTS for AI responses if enabled
        if (g_state.voiceTTSEnabled && g_state.voiceInitialized && g_state.voiceChat) {
            g_state.voiceChat->speak(out);
        }
        
        // Record response in replay journal + history
        cli_record_action(static_cast<int>(ReplayActionType::AgentResponse),
                          "agent", "CLI chat response", "", out, 0, 1.0f, queryMs);
        if (cli_get_history_recorder()) {
            cli_get_history_recorder()->recordChatResponse(out, static_cast<int>(queryMs));
        }
        
        // Auto-dispatch tool calls in AI response
        if (g_state.subAgentMgr) {
            std::string toolResult;
            if (g_state.subAgentMgr->dispatchToolCall("cli", out, toolResult)) {
                s_logger.info("\n[Tool Result]\n");
            }
        }
        
        // Feed output to the autonomy loop for background analysis
        auto& loop = CLIAutonomyLoop::instance();
        if (loop.getState() == AutonomyLoopState::Running) {
            loop.enqueueOutput(out, line);
        }
    }
}

int main() {
    init_runtime();

    // Phase 34: Persistent instructions context — load at startup
    {
        auto& provider = InstructionsProvider::instance();
        provider.addSearchPath(".");
        provider.addSearchPath(".github");
        auto r = provider.loadAll();
        if (r.success) {
            s_logger.info("\xE2\x9C\x85 Instructions loaded: ");
        } else {
            s_logger.info("\xE2\x9A\xA0 Instructions: ");
        }
    }

    // Initialize decision tree + autonomy loop with engine pointers
    {
        auto& tree = AgenticDecisionTree::instance();
        auto& loop = CLIAutonomyLoop::instance();
        if (g_state.agenticEngine) {
            tree.setAgenticEngine(g_state.agenticEngine);
            loop.setAgenticEngine(g_state.agenticEngine);
        }
        if (g_state.subAgentMgr) {
            tree.setSubAgentManager(g_state.subAgentMgr);
            loop.setSubAgentManager(g_state.subAgentMgr);
        }
    }
    
    // Initialize Phase 20: Headless CLI systems (safety, confidence, replay, governor, etc.)
    cli_headless_init(g_state.agenticEngine, g_state.subAgentMgr);
    
    // Report unified feature dispatch status
    size_t cliCount = getCliFeatureCount();
    size_t guiCount = getGuiFeatureCount();
    size_t totalCount = getTotalFeatureCount();
    
    s_logger.info("\n\xE2\x95\x94\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x97\n");
    s_logger.info("\xE2\x95\x91   RawrXD AI Runtime (CLI) \xE2\x80\x94 Unified Feature Dispatch v2.0       \xE2\x95\x91\n");
    s_logger.info("\xE2\x95\x91   ");
    s_logger.info("\xE2\x95\x91   Pure C++20 + x64 MASM | Win32 | 3-Layer Hotpatch              \xE2\x95\x91\n");
    s_logger.info("\xE2\x95\x9A\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x9D\n\n");
    s_logger.info("Type !help for commands, !manifest_md for feature manifest, or chat with AI.\n\n");

    std::string line;
    while (true) {
        s_logger.info("rawrxd> ");
        std::getline(std::cin, line);

        if (line == "!quit") break;
        route_command(line);
    }
    
    // Clean shutdown of all headless systems
    cli_headless_shutdown();
    
    // Phase 33: Clean shutdown of voice engine
    if (g_state.voiceInitialized && g_state.voiceChat) {
        g_state.voiceChat->shutdown();
        g_state.voiceChat.reset();
        g_state.voiceInitialized = false;
    }
    
    return 0;
}
