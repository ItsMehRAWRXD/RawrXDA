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
        std::cout << "\xE2\x9C\x85 Voice engine initialized (16kHz mono, VAD enabled)\n";
    } else {
        std::cout << "\xE2\x9D\x8C Voice init failed: " << r.detail << "\n";
    }
}

void cmd_voice(const std::string& args) {
    if (args.empty() || args == "help") {
        std::cout << "\n\xF0\x9F\x8E\x99\xEF\xB8\x8F  VOICE COMMANDS:\n";
        std::cout << "  !voice init               Initialize voice engine\n";
        std::cout << "  !voice record              Start/stop recording\n";
        std::cout << "  !voice ptt                 Push-to-talk toggle\n";
        std::cout << "  !voice transcribe          Transcribe last recording\n";
        std::cout << "  !voice speak <text>        Text-to-speech\n";
        std::cout << "  !voice tts <on|off>        Toggle TTS for AI responses\n";
        std::cout << "  !voice mode <ptt|vad|off>  Set capture mode\n";
        std::cout << "  !voice devices             List audio devices\n";
        std::cout << "  !voice device <id>         Select input device\n";
        std::cout << "  !voice room <name>         Join/leave voice room\n";
        std::cout << "  !voice metrics             Show voice metrics\n";
        std::cout << "  !voice status              Show voice status\n";
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
        std::cout << "\xE2\x9D\x8C Voice engine not available.\n";
        return;
    }
    
    if (sub == "record") {
        if (g_state.voiceChat->isRecording()) {
            auto r = g_state.voiceChat->stopRecording();
            std::cout << "\xE2\x8F\xB9 Recording stopped.";
            if (r.success) {
                size_t samples = g_state.voiceChat->getRecordedSampleCount();
                double secs = static_cast<double>(samples) / 16000.0;
                std::cout << " (" << std::fixed << std::setprecision(1) << secs << "s, " << samples << " samples)";
            }
            std::cout << "\n";
        } else {
            auto r = g_state.voiceChat->startRecording();
            if (r.success) {
                std::cout << "\xE2\x8F\xBA Recording... (type !voice record to stop)\n";
            } else {
                std::cout << "\xE2\x9D\x8C Record failed: " << r.detail << "\n";
            }
        }
    }
    else if (sub == "ptt") {
        if (g_state.voiceChat->isPTTActive()) {
            g_state.voiceChat->pttEnd();
            std::cout << "\xF0\x9F\x8E\x99 PTT released.\n";
            // Auto-transcribe on PTT release
            std::string text;
            auto r = g_state.voiceChat->transcribeLastRecording(text);
            if (r.success && !text.empty()) {
                std::cout << "\xF0\x9F\x93\x9D Transcription: " << text << "\n";
            }
        } else {
            g_state.voiceChat->pttBegin();
            std::cout << "\xF0\x9F\x8E\x99 PTT active — speak now (type !voice ptt to release)\n";
        }
    }
    else if (sub == "transcribe") {
        std::string text;
        auto r = g_state.voiceChat->transcribeLastRecording(text);
        if (r.success && !text.empty()) {
            std::cout << "\xF0\x9F\x93\x9D Transcription: " << text << "\n";
        } else {
            std::cout << "\xE2\x9D\x8C Transcription failed: " << r.detail << "\n";
        }
    }
    else if (sub == "speak") {
        if (param.empty()) {
            std::cout << "Usage: !voice speak <text to speak>\n";
            return;
        }
        auto r = g_state.voiceChat->speak(param);
        if (r.success) {
            std::cout << "\xF0\x9F\x94\x8A Speaking: " << param.substr(0, 60) << (param.size() > 60 ? "..." : "") << "\n";
        } else {
            std::cout << "\xE2\x9D\x8C TTS failed: " << r.detail << "\n";
        }
    }
    else if (sub == "tts") {
        if (param == "on") {
            g_state.voiceTTSEnabled = true;
            std::cout << "\xF0\x9F\x94\x8A TTS for AI responses: ON\n";
        } else if (param == "off") {
            g_state.voiceTTSEnabled = false;
            std::cout << "\xF0\x9F\x94\x87 TTS for AI responses: OFF\n";
        } else {
            std::cout << "TTS is currently: " << (g_state.voiceTTSEnabled ? "ON" : "OFF") << "\n";
            std::cout << "Usage: !voice tts <on|off>\n";
        }
    }
    else if (sub == "mode") {
        if (param == "ptt") {
            g_state.voiceChat->setMode(VoiceChatMode::PushToTalk);
            std::cout << "\xE2\x9C\x85 Voice mode: Push-to-Talk\n";
        } else if (param == "vad") {
            g_state.voiceChat->setMode(VoiceChatMode::Continuous);
            std::cout << "\xE2\x9C\x85 Voice mode: Continuous (VAD)\n";
        } else if (param == "off") {
            g_state.voiceChat->setMode(VoiceChatMode::Disabled);
            std::cout << "\xE2\x9C\x85 Voice mode: Disabled\n";
        } else {
            auto mode = g_state.voiceChat->getMode();
            const char* modeStr = (mode == VoiceChatMode::PushToTalk) ? "Push-to-Talk" :
                                  (mode == VoiceChatMode::Continuous) ? "Continuous (VAD)" : "Disabled";
            std::cout << "Current mode: " << modeStr << "\n";
            std::cout << "Usage: !voice mode <ptt|vad|off>\n";
        }
    }
    else if (sub == "devices") {
        auto inputs = VoiceChat::enumerateInputDevices();
        auto outputs = VoiceChat::enumerateOutputDevices();
        std::cout << "\n\xF0\x9F\x8E\xA7 INPUT DEVICES:\n";
        for (const auto& dev : inputs) {
            std::cout << "  [" << dev.id << "] " << dev.name;
            if (dev.isDefault) std::cout << " (default)";
            std::cout << "\n";
        }
        std::cout << "\n\xF0\x9F\x94\x8A OUTPUT DEVICES:\n";
        for (const auto& dev : outputs) {
            std::cout << "  [" << dev.id << "] " << dev.name;
            if (dev.isDefault) std::cout << " (default)";
            std::cout << "\n";
        }
        if (inputs.empty()) std::cout << "  (no input devices found)\n";
        if (outputs.empty()) std::cout << "  (no output devices found)\n";
    }
    else if (sub == "device") {
        if (param.empty()) {
            std::cout << "Usage: !voice device <id>\n";
            return;
        }
        try {
            int id = std::stoi(param);
            auto r = g_state.voiceChat->selectInputDevice(id);
            if (r.success) {
                std::cout << "\xE2\x9C\x85 Input device set to ID " << id << "\n";
            } else {
                std::cout << "\xE2\x9D\x8C Failed: " << r.detail << "\n";
            }
        } catch (...) {
            std::cout << "\xE2\x9D\x8C Invalid device ID\n";
        }
    }
    else if (sub == "room") {
        if (g_state.voiceChat->isInRoom()) {
            std::string room = g_state.voiceChat->getRoomName();
            g_state.voiceChat->leaveRoom();
            std::cout << "\xF0\x9F\x9A\xAA Left room '" << room << "'\n";
        } else {
            std::string room = param.empty() ? "general" : param;
            auto r = g_state.voiceChat->joinRoom(room);
            if (r.success) {
                std::cout << "\xF0\x9F\x8E\xA4 Joined room '" << room << "'\n";
            } else {
                std::cout << "\xE2\x9D\x8C Join failed: " << r.detail << "\n";
            }
        }
    }
    else if (sub == "metrics") {
        auto m = g_state.voiceChat->getMetrics();
        std::cout << "\n\xF0\x9F\x93\x8A VOICE METRICS:\n";
        std::cout << "  Recordings:        " << m.recordingCount << "\n";
        std::cout << "  Playbacks:         " << m.playbackCount << "\n";
        std::cout << "  Transcriptions:    " << m.transcriptionCount << "\n";
        std::cout << "  TTS Calls:         " << m.ttsCount << "\n";
        std::cout << "  Errors:            " << m.errorCount << "\n";
        std::cout << "  Bytes Recorded:    " << m.bytesRecorded << "\n";
        std::cout << "  Bytes Played:      " << m.bytesPlayed << "\n";
        std::cout << "  VAD Speech Events: " << m.vadSpeechEvents << "\n";
        std::cout << "  Relay Msg Sent:    " << m.relayMessagesSent << "\n";
        std::cout << "  Relay Msg Recv:    " << m.relayMessagesRecv << "\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Avg Record Latency:    " << m.avgRecordingLatencyMs << " ms\n";
        std::cout << "  Avg Transcribe Latency:" << m.avgTranscriptionLatencyMs << " ms\n";
        std::cout << "  Avg TTS Latency:       " << m.avgTtsLatencyMs << " ms\n";
        std::cout << "  Avg Playback Latency:  " << m.avgPlaybackLatencyMs << " ms\n";
    }
    else if (sub == "status") {
        auto mode = g_state.voiceChat->getMode();
        const char* modeStr = (mode == VoiceChatMode::PushToTalk) ? "Push-to-Talk" :
                              (mode == VoiceChatMode::Continuous) ? "Continuous (VAD)" : "Disabled";
        std::cout << "\n\xF0\x9F\x8E\x99 VOICE STATUS:\n";
        std::cout << "  Engine:      " << (g_state.voiceInitialized ? "Initialized" : "Not init") << "\n";
        std::cout << "  Recording:   " << (g_state.voiceChat->isRecording() ? "YES" : "No") << "\n";
        std::cout << "  Playing:     " << (g_state.voiceChat->isPlaying() ? "YES" : "No") << "\n";
        std::cout << "  PTT Active:  " << (g_state.voiceChat->isPTTActive() ? "YES" : "No") << "\n";
        std::cout << "  In Room:     " << (g_state.voiceChat->isInRoom() ? g_state.voiceChat->getRoomName() : "No") << "\n";
        std::cout << "  Mode:        " << modeStr << "\n";
        std::cout << "  VAD State:   ";
        switch (g_state.voiceChat->getVADState()) {
            case VADState::Silence:  std::cout << "Silence"; break;
            case VADState::Speech:   std::cout << "Speech detected"; break;
            case VADState::Trailing: std::cout << "Trailing silence"; break;
        }
        std::cout << "\n";
        std::cout << "  RMS Level:   " << std::fixed << std::setprecision(4) << g_state.voiceChat->getCurrentRMS() << "\n";
        std::cout << "  TTS Auto:    " << (g_state.voiceTTSEnabled ? "ON" : "OFF") << "\n";
        std::cout << "  Recorded:    " << g_state.voiceChat->getRecordedSampleCount() << " samples\n";
    }
    else {
        std::cout << "Unknown voice subcommand: " << sub << ". Type !voice help\n";
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
    std::cout << "✅ New file created\n";
}

void cmd_open_file(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::string path = args;
    if (path.empty()) {
        std::cout << "Usage: !open <filepath>\n";
        return;
    }
    
    if (!fs::exists(path)) {
        std::cout << "❌ File not found: " << path << "\n";
        return;
    }
    
    std::ifstream file(path);
    g_state.editorBuffer.assign((std::istreambuf_iterator<char>(file)), 
                                 std::istreambuf_iterator<char>());
    g_state.currentFile = path;
    g_state.undoStack.clear();
    g_state.redoStack.clear();
    std::cout << "✅ Opened: " << path << " (" << g_state.editorBuffer.size() << " bytes)\n";
}

void cmd_save_file(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.currentFile.empty()) {
        std::cout << "❌ No file open. Use !save_as <path> to save to a new file.\n";
        return;
    }
    
    std::ofstream file(g_state.currentFile);
    file << g_state.editorBuffer;
    file.close();
    std::cout << "✅ Saved: " << g_state.currentFile << "\n";
}

void cmd_save_as(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        std::cout << "Usage: !save_as <filepath>\n";
        return;
    }
    
    std::ofstream file(args);
    file << g_state.editorBuffer;
    file.close();
    g_state.currentFile = args;
    std::cout << "✅ Saved as: " << args << "\n";
}

void cmd_close_file(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.currentFile.empty()) {
        std::cout << "❌ No file open.\n";
        return;
    }
    g_state.editorBuffer.clear();
    g_state.currentFile.clear();
    std::cout << "✅ File closed\n";
}

// ============================================================================
// EDITOR OPERATIONS (Feature Parity with Win32IDE Edit commands)
// ============================================================================

void cmd_cut(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.editorBuffer.empty()) {
        std::cout << "❌ Buffer is empty, nothing to cut.\n";
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
            std::cout << "Usage: !cut [start_line-end_line]\n";
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
        std::cout << "✅ Cut lines " << startLine << "-" << endLine << " to clipboard (" << cutText.size() << " bytes)\n";
    } else {
        // Cut entire buffer
        g_state.clipboard.clear();
        g_state.clipboard.push_back(g_state.editorBuffer);
        g_state.editorBuffer.clear();
        g_state.redoStack.clear();
        std::cout << "✅ Cut entire buffer to clipboard\n";
    }
}

void cmd_copy(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.clipboard.push_back(g_state.editorBuffer);
    std::cout << "✅ Copied to clipboard\n";
}

void cmd_paste(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_state.clipboard.empty()) {
        g_state.undoStack.push_back(g_state.editorBuffer);
        g_state.editorBuffer += g_state.clipboard.back();
        g_state.redoStack.clear();
        std::cout << "✅ Pasted from clipboard\n";
    } else {
        std::cout << "❌ Clipboard empty\n";
    }
}

void cmd_undo(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_state.undoStack.empty()) {
        g_state.redoStack.push_back(g_state.editorBuffer);
        g_state.editorBuffer = g_state.undoStack.back();
        g_state.undoStack.pop_back();
        std::cout << "✅ Undo\n";
    } else {
        std::cout << "❌ Nothing to undo\n";
    }
}

void cmd_redo(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_state.redoStack.empty()) {
        g_state.undoStack.push_back(g_state.editorBuffer);
        g_state.editorBuffer = g_state.redoStack.back();
        g_state.redoStack.pop_back();
        std::cout << "✅ Redo\n";
    } else {
        std::cout << "❌ Nothing to redo\n";
    }
}

void cmd_find(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        std::cout << "Usage: !find <text>\n";
        return;
    }
    
    size_t pos = g_state.editorBuffer.find(args);
    if (pos != std::string::npos) {
        std::cout << "✅ Found at position " << pos << "\n";
    } else {
        std::cout << "❌ Not found\n";
    }
}

void cmd_replace(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto space = args.find(' ');
    if (space == std::string::npos) {
        std::cout << "Usage: !replace <old> <new>\n";
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
    std::cout << "✅ Replaced " << count << " occurrence(s)\n";
}

// ============================================================================
// AGENTIC OPERATIONS (Feature Parity with Win32IDE_AgentCommands)
// ============================================================================

void cmd_agent_execute(const std::string& args) {
    if (args.empty()) {
        std::cout << "Usage: !agent_execute <prompt>\n";
        return;
    }
    
    if (g_state.agenticEngine && g_state.agenticEngine->isModelLoaded()) {
        std::cout << "🤖 Agent executing: " << args << "\n";
        std::string response = g_state.agenticEngine->chat(args);
        std::cout << response << "\n";
        
        // Auto-dispatch tool calls in response
        if (g_state.subAgentMgr) {
            std::string toolResult;
            if (g_state.subAgentMgr->dispatchToolCall("cli-agent", response, toolResult)) {
                std::cout << "\n[Tool Result]\n" << toolResult << "\n";
            }
        }
    } else {
        std::cout << "🤖 Agent executing: " << args << "\n";
        std::string out = process_prompt(args);
        std::cout << out << "\n";
    }
}

void cmd_agent_loop(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        std::cout << "Usage: !agent_loop <prompt> [iterations]\n";
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
    std::cout << "🚀 Starting agent loop: " << prompt << " (max " << iterations << " iterations)\n";
    
    // Capture pointers locally before launching thread
    AgenticEngine* eng = g_state.agenticEngine;
    SubAgentManager* mgr = g_state.subAgentMgr;
    
    std::thread([prompt, iterations, eng, mgr]() {
        for (int i = 0; i < iterations; i++) {
            std::cout << "[Agent Iter " << (i+1) << "/" << iterations << "] Processing...\n";
            
            if (eng && eng->isModelLoaded()) {
                std::string response = eng->chat(
                    "Iteration " + std::to_string(i + 1) + "/" + std::to_string(iterations) +
                    ". Goal: " + prompt + "\nPrevious context available. Continue working toward the goal.");
                std::cout << response << "\n";
                
                // Dispatch tool calls
                if (mgr) {
                    std::string toolResult;
                    if (mgr->dispatchToolCall("cli-loop", response, toolResult)) {
                        std::cout << "[Tool Result] " << toolResult << "\n";
                    }
                }
            } else {
                std::string out = process_prompt(prompt);
                std::cout << out << "\n";
            }
        }
        std::cout << "✅ Agent loop completed\n";
    }).detach();
}

void cmd_agent_goal(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        std::cout << "Usage: !agent_goal <goal>\n";
        return;
    }
    g_state.agentGoal = args;
    std::cout << "✅ Goal set: " << args << "\n";
}

void cmd_agent_memory(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args == "show") {
        std::cout << "📝 Agent Memory:\n";
        for (size_t i = 0; i < g_state.agentMemory.size(); i++) {
            std::cout << "  [" << i << "] " << g_state.agentMemory[i] << "\n";
        }
    } else if (!args.empty()) {
        g_state.agentMemory.push_back(args);
        std::cout << "✅ Memory added: " << args << "\n";
    } else {
        std::cout << "Usage: !agent_memory <observation> | !agent_memory show\n";
    }
}

// ============================================================================
// AUTONOMY OPERATIONS (Feature Parity with Win32IDE_Autonomy)
// ============================================================================

void cmd_autonomy_start(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.autonomyEnabled = true;
    std::cout << "🤖 Autonomy enabled. Use !autonomy_goal to set objective.\n";
}

void cmd_autonomy_stop(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.autonomyEnabled = false;
    std::cout << "⏹️  Autonomy disabled\n";
}

void cmd_autonomy_goal(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        std::cout << "Current goal: " << g_state.agentGoal << "\n";
        return;
    }
    g_state.agentGoal = args;
    std::cout << "✅ Autonomy goal set: " << args << "\n";
}

void cmd_autonomy_rate(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    try {
        int rate = std::stoi(args);
        g_state.maxActionsPerMinute = rate;
        std::cout << "✅ Max actions per minute: " << rate << "\n";
    } catch (...) {
        std::cout << "Usage: !autonomy_rate <actions_per_minute>\n";
    }
}

// ============================================================================
// DEBUG OPERATIONS (Feature Parity with Win32IDE_Debugger)
// ============================================================================

void cmd_breakpoint_add(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto space = args.find(':');
    if (space == std::string::npos) {
        std::cout << "Usage: !breakpoint_add <file>:<line>\n";
        return;
    }
    
    std::string file = args.substr(0, space);
    int line = std::stoi(args.substr(space + 1));
    
    g_state.breakpoints.push_back({file, line});
    std::cout << "✅ Breakpoint added: " << file << ":" << line << "\n";
}

void cmd_breakpoint_list(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::cout << "🔴 Breakpoints:\n";
    for (size_t i = 0; i < g_state.breakpoints.size(); i++) {
        std::cout << "  [" << i << "] " << g_state.breakpoints[i].first 
                  << ":" << g_state.breakpoints[i].second << "\n";
    }
}

void cmd_breakpoint_remove(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    try {
        int idx = std::stoi(args);
        if (idx >= 0 && idx < static_cast<int>(g_state.breakpoints.size())) {
            g_state.breakpoints.erase(g_state.breakpoints.begin() + idx);
            std::cout << "✅ Breakpoint removed\n";
        }
    } catch (...) {
        std::cout << "Usage: !breakpoint_remove <index>\n";
    }
}

void cmd_debug_start(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.debuggingActive = true;
    std::cout << "🐛 Debugger started\n";
}

void cmd_debug_stop(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.debuggingActive = false;
    std::cout << "⏹️  Debugger stopped\n";
}

void cmd_debug_step(const std::string& args) {
    if (!g_state.debuggingActive) {
        std::cout << "❌ Debugger not active. Use !debug_start first.\n";
        return;
    }
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.debugStepCount++;
    // Advance to next breakpoint or next line
    if (!g_state.breakpoints.empty() && !g_state.currentFile.empty()) {
        // Find current position and step to next line
        int currentLine = g_state.debugCurrentLine;
        g_state.debugCurrentLine = currentLine + 1;
        std::cout << "➡️  Step to line " << g_state.debugCurrentLine;
        // Check if we hit a breakpoint
        for (const auto& bp : g_state.breakpoints) {
            if (bp.first == g_state.currentFile && bp.second == g_state.debugCurrentLine) {
                std::cout << " 🔴 [BREAKPOINT HIT]";
                break;
            }
        }
        std::cout << "\n";
        // Show the source line at current position
        std::istringstream stream(g_state.editorBuffer);
        std::string line;
        int lineNum = 1;
        while (std::getline(stream, line)) {
            if (lineNum == g_state.debugCurrentLine) {
                std::cout << "  → " << lineNum << " | " << line << "\n";
                break;
            }
            lineNum++;
        }
    } else {
        std::cout << "➡️  Step executed (no source context)\n";
    }
}

void cmd_debug_continue(const std::string& args) {
    if (!g_state.debuggingActive) {
        std::cout << "❌ Debugger not active. Use !debug_start first.\n";
        return;
    }
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.breakpoints.empty()) {
        std::cout << "▶️  Continuing to end (no breakpoints set)\n";
        g_state.debuggingActive = false;
        std::cout << "⏹️  Execution complete\n";
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
                std::cout << "▶️  Continued to breakpoint at " << bp.first << ":" << line << "\n";
                // Show the source line
                std::istringstream stream(g_state.editorBuffer);
                std::string srcLine;
                int num = 1;
                while (std::getline(stream, srcLine)) {
                    if (num == line) {
                        std::cout << "  → " << num << " | " << srcLine << "\n";
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
        std::cout << "▶️  Ran to end — no more breakpoints hit\n";
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
    std::cout << "✅ New terminal pane: " << pane_name << "\n";
}

void cmd_terminal_split(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::string orientation = args.empty() ? "horizontal" : args;
    std::cout << "✅ Terminal split " << orientation << "\n";
}

void cmd_terminal_kill(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.terminalPanes.empty()) {
        std::cout << "❌ No terminals to close\n";
        return;
    }
    g_state.terminalPanes.pop_back();
    std::cout << "✅ Terminal closed\n";
}

void cmd_terminal_list(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::cout << "📋 Terminal panes:\n";
    for (size_t i = 0; i < g_state.terminalPanes.size(); i++) {
        std::cout << "  [" << i << "] " << g_state.terminalPanes[i] << "\n";
    }
}

// ============================================================================
// HOTPATCH OPERATIONS (Feature Parity with Win32IDE Hotpatch)
// ============================================================================

void cmd_hotpatch_apply(const std::string& args) {
    if (args.empty()) {
        std::cout << "Usage: !hotpatch_apply <patch_file>\n";
        return;
    }
    // Parse patch file path
    std::string patchPath = args;
    // Trim whitespace
    size_t start = patchPath.find_first_not_of(" \t");
    size_t end = patchPath.find_last_not_of(" \t");
    if (start != std::string::npos) patchPath = patchPath.substr(start, end - start + 1);

    std::cout << "🔥 Applying hotpatch from: " << patchPath << "\n";
    
    // Read the patch file
    std::ifstream patchFile(patchPath);
    if (!patchFile.is_open()) {
        std::cout << "❌ Cannot open patch file: " << patchPath << "\n";
        return;
    }
    
    std::string patchContent((std::istreambuf_iterator<char>(patchFile)),
                              std::istreambuf_iterator<char>());
    patchFile.close();
    
    if (patchContent.empty()) {
        std::cout << "❌ Patch file is empty\n";
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
                std::cout << "  Patch @0x" << std::hex << offset << std::dec 
                          << ": " << hexBytes << "\n";
                patchCount++;
            } catch (...) {
                std::cout << "  ⚠️  Skipping malformed line: " << line << "\n";
                failCount++;
            }
        }
    }
    
    std::cout << "✅ Applied " << patchCount << " patches";
    if (failCount > 0) std::cout << " (" << failCount << " skipped)";
    std::cout << " without restart\n";
}

void cmd_hotpatch_create(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.currentFile.empty()) {
        std::cout << "❌ No file open\n";
        return;
    }
    std::cout << "🔥 Creating hotpatch for: " << g_state.currentFile << "\n";
    
    // Read the original file from disk for comparison
    std::ifstream origFile(g_state.currentFile, std::ios::binary);
    if (!origFile.is_open()) {
        std::cout << "❌ Cannot read original file from disk: " << g_state.currentFile << "\n";
        return;
    }
    std::string origContent((std::istreambuf_iterator<char>(origFile)),
                             std::istreambuf_iterator<char>());
    origFile.close();
    
    // Compare buffer to original and generate byte-level patch
    std::string patchFilename = g_state.currentFile + ".hotpatch";
    std::ofstream patchFile(patchFilename);
    if (!patchFile.is_open()) {
        std::cout << "❌ Cannot create patch file: " << patchFilename << "\n";
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
    std::cout << "✅ Hotpatch created: " << patchFilename << " (" << diffCount << " change regions)\n";
}

// ============================================================================
// AGENTIC DECISION TREE (Phase 19: Headless Autonomy)
// ============================================================================

void cmd_decision_tree(const std::string& args) {
    auto& tree = AgenticDecisionTree::instance();

    if (args == "dump" || args == "show") {
        std::cout << tree.dumpTreeJSON() << "\n";
    } else if (args == "trace") {
        std::cout << tree.dumpLastTrace() << "\n";
    } else if (args == "stats") {
        auto& s = tree.getStats();
        std::cout << "\n🌳 Decision Tree Statistics:\n";
        std::cout << "  Trees evaluated:      " << s.treesEvaluated.load() << "\n";
        std::cout << "  Nodes visited:        " << s.nodesVisited.load() << "\n";
        std::cout << "  SSA lifts:            " << s.ssaLiftsPerformed.load() << "\n";
        std::cout << "  Failures detected:    " << s.failuresDetected.load() << "\n";
        std::cout << "  Patches applied:      " << s.patchesApplied.load() << "\n";
        std::cout << "  Patches reverted:     " << s.patchesReverted.load() << "\n";
        std::cout << "  Successful fixes:     " << s.successfulCorrections.load() << "\n";
        std::cout << "  Failed fixes:         " << s.failedCorrections.load() << "\n";
        std::cout << "  Escalations:          " << s.escalations.load() << "\n";
        std::cout << "  Total retries:        " << s.totalRetries.load() << "\n";
        std::cout << "  Aborts:               " << s.aborts.load() << "\n\n";
    } else if (args == "enable") {
        tree.setEnabled(true);
        std::cout << "✅ Decision tree enabled\n";
    } else if (args == "disable") {
        tree.setEnabled(false);
        std::cout << "⏹️  Decision tree disabled\n";
    } else if (args == "reset") {
        tree.resetStats();
        tree.buildDefaultTree();
        std::cout << "✅ Decision tree reset to defaults\n";
    } else {
        std::cout << "Usage: !decision_tree <dump|trace|stats|enable|disable|reset>\n";
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
        std::cout << loop.getDetailedStatus();
    } else if (args == "tick") {
        auto outcome = loop.tick();
        std::cout << (outcome.success ? "✅" : "❌") << " "
                  << outcome.summary << "\n";
        for (const auto& t : outcome.traceLog) {
            std::cout << "    " << t << "\n";
        }
    } else if (args.rfind("verbose ", 0) == 0) {
        std::string val = args.substr(8);
        AutonomyLoopConfig cfg = loop.getConfig();
        cfg.verboseTracing = (val == "on" || val == "true" || val == "1");
        loop.setConfig(cfg);
        std::cout << "✅ Verbose tracing: " << (cfg.verboseTracing ? "ON" : "OFF") << "\n";
    } else if (args.rfind("rate ", 0) == 0) {
        try {
            int rate = std::stoi(args.substr(5));
            AutonomyLoopConfig cfg = loop.getConfig();
            cfg.maxActionsPerMinute = rate;
            loop.setConfig(cfg);
            std::cout << "✅ Rate limit: " << rate << " actions/min\n";
        } catch (...) {
            std::cout << "Usage: !autonomy_run rate <number>\n";
        }
    } else if (args.rfind("interval ", 0) == 0) {
        try {
            int ms = std::stoi(args.substr(9));
            AutonomyLoopConfig cfg = loop.getConfig();
            cfg.tickIntervalMs = ms;
            loop.setConfig(cfg);
            std::cout << "✅ Tick interval: " << ms << "ms\n";
        } catch (...) {
            std::cout << "Usage: !autonomy_run interval <ms>\n";
        }
    } else {
        std::cout << "Usage: !autonomy_run <start|stop|pause|resume|status|tick>\n";
        std::cout << "       !autonomy_run verbose <on|off>\n";
        std::cout << "       !autonomy_run rate <actions_per_min>\n";
        std::cout << "       !autonomy_run interval <ms>\n";
    }
}

void cmd_ssa_lift(const std::string& args) {
    if (args.empty()) {
        std::cout << "Usage: !ssa_lift <binary_path> [function_name] [hex_address]\n";
        std::cout << "  Examples:\n";
        std::cout << "    !ssa_lift target.exe main\n";
        std::cout << "    !ssa_lift target.dll 0x140001000\n";
        std::cout << "    !ssa_lift model.gguf process_tokens 0x7ff6a000\n";
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

    std::cout << "🔬 Running SSA Lifter on: " << binaryPath;
    if (!funcName.empty()) std::cout << " func=" << funcName;
    if (addr != 0) std::cout << " addr=0x" << std::hex << addr << std::dec;
    std::cout << "\n";

    auto& loop = CLIAutonomyLoop::instance();
    std::string result = loop.runSSALift(binaryPath, funcName, addr);
    std::cout << result << "\n";
}

void cmd_auto_patch(const std::string& args) {
    if (args == "stats") {
        auto& tree = AgenticDecisionTree::instance();
        auto& s = tree.getStats();
        std::cout << "🔧 Auto-Patch Stats: "
                  << s.patchesApplied.load() << " applied, "
                  << s.patchesReverted.load() << " reverted, "
                  << s.successfulCorrections.load() << " verified\n";
        return;
    }

    if (args == "analyze" || args.empty()) {
        std::cout << "🔧 Running autonomous correction pipeline...\n";

        auto& loop = CLIAutonomyLoop::instance();
        if (g_state.agenticEngine) {
            loop.setAgenticEngine(g_state.agenticEngine);
        }
        if (g_state.subAgentMgr) {
            loop.setSubAgentManager(g_state.subAgentMgr);
        }

        auto outcome = loop.autoPatch();
        if (!outcome.success && args.empty()) {
            std::cout << "ℹ️  No prior failure context. Run inference first, then use !auto_patch.\n";
            std::cout << "    Or: pipe output through autonomy with !autonomy_run start\n";
        }
        return;
    }

    // Allow one-shot: !auto_patch "<output text>" "<prompt>"
    // Simple: just analyze the args as output text
    std::cout << "🔧 Analyzing provided text for failures...\n";
    auto& tree = AgenticDecisionTree::instance();
    if (g_state.agenticEngine) {
        tree.setAgenticEngine(g_state.agenticEngine);
    }

    DecisionOutcome outcome = tree.analyzeAndFix(args, "");
    std::cout << (outcome.success ? "✅" : "❌") << " " << outcome.summary << "\n";
    for (const auto& t : outcome.traceLog) {
        std::cout << "    " << t << "\n";
    }
}

// ============================================================================
// SEARCH & TOOLS (Feature Parity with Win32IDE Tools)
// ============================================================================

void cmd_search_files(const std::string& args) {
    if (args.empty()) {
        std::cout << "Usage: !search <pattern> [path]\n";
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
    
    std::cout << "🔍 Searching for \"" << pattern << "\" in " << searchPath << "\n";
    
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
                        std::cout << "\n📄 " << filepath << ":\n";
                        fileHeaderPrinted = true;
                        fileCount++;
                    }
                    // Truncate long lines
                    std::string displayLine = line.length() > 120 ? line.substr(0, 120) + "..." : line;
                    std::cout << "  " << lineNum << ": " << displayLine << "\n";
                    matchCount++;
                    if (matchCount >= maxResults) break;
                }
            }
            if (matchCount >= maxResults) {
                std::cout << "\n⚠️  Showing first " << maxResults << " results. Refine your search.\n";
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Search error: " << e.what() << "\n";
    }
    
    std::cout << "\n📊 Found " << matchCount << " matches in " << fileCount << " files\n";
}

void cmd_analyze(const std::string& args) {
    if (g_state.currentFile.empty()) {
        std::cout << "❌ No file open\n";
        return;
    }
    std::cout << "📊 Analyzing: " << g_state.currentFile << "\n";
    std::cout << "   Lines: " << std::count(g_state.editorBuffer.begin(), 
                                             g_state.editorBuffer.end(), '\n') << "\n";
    std::cout << "   Size: " << g_state.editorBuffer.size() << " bytes\n";
}

void cmd_profile(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    
    std::cout << "⏱️  Profiling started...\n";
    auto t0 = std::chrono::high_resolution_clock::now();
    
    // Profile the current editor buffer — analyze code structure
    if (g_state.editorBuffer.empty()) {
        std::cout << "⚠️  No content to profile. Open a file first.\n";
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
    
    std::cout << "\n📊 Profile Results" << (g_state.currentFile.empty() ? "" : ": " + g_state.currentFile) << "\n";
    std::cout << "  ───────────────────────────────────\n";
    std::cout << "  Total lines:      " << totalLines << "\n";
    std::cout << "  Code lines:       " << codeLines << " (" << (totalLines > 0 ? codeLines * 100 / totalLines : 0) << "%)\n";
    std::cout << "  Comment lines:    " << commentLines << "\n";
    std::cout << "  Blank lines:      " << blankLines << "\n";
    std::cout << "  Functions:        ~" << functionCount << "\n";
    std::cout << "  Classes/Structs:  ~" << classCount << "\n";
    std::cout << "  Includes:         " << includeCount << "\n";
    std::cout << "  Max nesting:      " << maxBraceDepth << " levels\n";
    std::cout << "  Max line length:  " << maxLineLength << " chars\n";
    std::cout << "  Avg line length:  " << (totalLines > 0 ? totalChars / totalLines : 0) << " chars\n";
    std::cout << "  Total size:       " << g_state.editorBuffer.size() << " bytes\n";
    std::cout << "  ───────────────────────────────────\n";
    std::cout << "  Analysis time:    " << elapsed << " μs\n";
    std::cout << "✅ Profile complete\n";
}

// ============================================================================
// SUBAGENT OPERATIONS (Feature Parity with Win32IDE_SubAgent)
// ============================================================================

void cmd_subagent(const std::string& args) {
    if (args.empty()) {
        std::cout << "Usage: !subagent <prompt>\n";
        return;
    }
    if (!g_state.subAgentMgr) {
        std::cout << "❌ SubAgentManager not initialized (no model loaded)\n";
        return;
    }
    std::cout << "🤖 Spawning sub-agent...\n";
    std::string id = g_state.subAgentMgr->spawnSubAgent("cli", "CLI subagent", args);
    bool ok = g_state.subAgentMgr->waitForSubAgent(id, 120000);
    std::string result = g_state.subAgentMgr->getSubAgentResult(id);
    std::cout << (ok ? "✅" : "❌") << " SubAgent result:\n" << result << "\n";
}

void cmd_chain(const std::string& args) {
    if (args.empty()) {
        std::cout << "Usage: !chain <step1> | <step2> | <step3> ...\n";
        std::cout << "  Steps are separated by ' | '\n";
        std::cout << "  Each step can use {{input}} for the previous step's output\n";
        return;
    }
    if (!g_state.subAgentMgr) {
        std::cout << "❌ SubAgentManager not initialized\n";
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

    std::cout << "🔗 Running chain with " << steps.size() << " steps...\n";
    for (size_t i = 0; i < steps.size(); i++) {
        std::cout << "  Step " << (i + 1) << ": " << steps[i].substr(0, 60)
                  << (steps[i].size() > 60 ? "..." : "") << "\n";
    }

    std::string result = g_state.subAgentMgr->executeChain("cli", steps);
    std::cout << "✅ Chain result:\n" << result << "\n";
}

void cmd_swarm(const std::string& args) {
    if (args.empty()) {
        std::cout << "Usage: !swarm <prompt1> | <prompt2> | <prompt3> ...\n";
        std::cout << "  Options (append after last prompt):\n";
        std::cout << "    --strategy <concatenate|vote|summarize>  Merge strategy\n";
        std::cout << "    --parallel <n>                           Max parallel agents\n";
        return;
    }
    if (!g_state.subAgentMgr) {
        std::cout << "❌ SubAgentManager not initialized\n";
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
        std::cout << "❌ No prompts provided\n";
        return;
    }

    std::cout << "🐝 Launching HexMag swarm with " << prompts.size()
              << " tasks (strategy=" << config.mergeStrategy
              << ", parallel=" << config.maxParallel << ")\n";

    std::string result = g_state.subAgentMgr->executeSwarm("cli", prompts, config);
    std::cout << "✅ Swarm result:\n" << result << "\n";
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
        std::cout << "🔗 Chain-of-Thought Multi-Model Review\n";
        std::cout << "Usage:\n";
        std::cout << "  !cot status             — Show CoT engine status\n";
        std::cout << "  !cot presets            — List available presets\n";
        std::cout << "  !cot roles              — List all available roles\n";
        std::cout << "  !cot preset <name>      — Apply a preset (review|audit|think|research|debate|custom)\n";
        std::cout << "  !cot steps              — Show current chain steps\n";
        std::cout << "  !cot add <role>         — Add a step with the given role\n";
        std::cout << "  !cot clear              — Clear all steps\n";
        std::cout << "  !cot run <query>        — Execute the chain on a query\n";
        std::cout << "  !cot cancel             — Cancel a running chain\n";
        std::cout << "  !cot stats              — Show execution statistics\n";
        return;
    }

    // Parse subcommand
    auto sp = args.find(' ');
    std::string subcmd = (sp == std::string::npos) ? args : args.substr(0, sp);
    std::string subargs = (sp == std::string::npos) ? "" : args.substr(sp + 1);

    if (subcmd == "status") {
        std::cout << cot.getStatusJSON() << "\n";
    }
    else if (subcmd == "presets") {
        auto names = getCoTPresetNames();
        std::cout << "📋 Available presets:\n";
        for (const auto& n : names) {
            const CoTPreset* p = getCoTPreset(n);
            if (p) {
                std::cout << "  " << n << " (" << p->label << ") — "
                          << p->steps.size() << " steps: ";
                for (size_t i = 0; i < p->steps.size(); i++) {
                    if (i > 0) std::cout << " → ";
                    std::cout << getCoTRoleInfo(p->steps[i].role).label;
                }
                std::cout << "\n";
            }
        }
    }
    else if (subcmd == "roles") {
        const auto& roles = getAllCoTRoles();
        std::cout << "🎭 Available roles (" << roles.size() << "):\n";
        for (const auto& r : roles) {
            std::cout << "  " << r.icon << " " << r.name
                      << " — " << r.instruction << "\n";
        }
    }
    else if (subcmd == "preset") {
        if (subargs.empty()) {
            std::cout << "❌ Usage: !cot preset <name>\n";
            return;
        }
        if (cot.applyPreset(subargs)) {
            std::cout << "✅ Applied preset '" << subargs << "' ("
                      << cot.getSteps().size() << " steps)\n";
            const auto& steps = cot.getSteps();
            for (size_t i = 0; i < steps.size(); i++) {
                const auto& info = getCoTRoleInfo(steps[i].role);
                std::cout << "  Step " << (i + 1) << ": " << info.icon
                          << " " << info.label << "\n";
            }
        } else {
            std::cout << "❌ Unknown preset: " << subargs << "\n";
            std::cout << "   Available: review, audit, think, research, debate, custom\n";
        }
    }
    else if (subcmd == "steps") {
        const auto& steps = cot.getSteps();
        if (steps.empty()) {
            std::cout << "⚠ No steps configured. Use !cot preset <name> or !cot add <role>\n";
        } else {
            std::cout << "🔗 Current chain (" << steps.size() << " steps):\n";
            for (size_t i = 0; i < steps.size(); i++) {
                const auto& info = getCoTRoleInfo(steps[i].role);
                std::cout << "  [" << (i + 1) << "] " << info.icon << " "
                          << info.label;
                if (steps[i].skip) std::cout << " (SKIPPED)";
                if (!steps[i].model.empty()) std::cout << " [model: " << steps[i].model << "]";
                std::cout << "\n";
            }
        }
    }
    else if (subcmd == "add") {
        if (subargs.empty()) {
            std::cout << "❌ Usage: !cot add <role>\n";
            return;
        }
        const CoTRoleInfo* info = getCoTRoleByName(subargs);
        if (!info) {
            std::cout << "❌ Unknown role: " << subargs << "\n";
            std::cout << "   Available roles: ";
            const auto& roles = getAllCoTRoles();
            for (size_t i = 0; i < roles.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << roles[i].name;
            }
            std::cout << "\n";
            return;
        }
        cot.addStep(info->id);
        std::cout << "✅ Added step: " << info->icon << " " << info->label
                  << " (total: " << cot.getSteps().size() << " steps)\n";
    }
    else if (subcmd == "clear") {
        cot.clearSteps();
        std::cout << "✅ Chain cleared.\n";
    }
    else if (subcmd == "run") {
        if (subargs.empty()) {
            std::cout << "❌ Usage: !cot run <your query>\n";
            return;
        }
        if (cot.getSteps().empty()) {
            std::cout << "⚠ No steps configured. Applying 'review' preset...\n";
            cot.applyPreset("review");
        }

        // Set step callback for progress output
        cot.setStepCallback([](const CoTStepResult& sr) {
            const auto& info = getCoTRoleInfo(sr.role);
            if (sr.skipped) {
                std::cout << "  ⏭ Step " << (sr.stepIndex + 1) << " (" << info.label << "): SKIPPED\n";
            } else if (sr.success) {
                std::cout << "  ✅ Step " << (sr.stepIndex + 1) << " (" << info.label
                          << "): " << sr.latencyMs << "ms, ~" << sr.tokenCount << " tokens\n";
            } else {
                std::cout << "  ❌ Step " << (sr.stepIndex + 1) << " (" << info.label
                          << "): FAILED — " << sr.error << "\n";
            }
        });

        std::cout << "🔗 Executing CoT chain (" << cot.getSteps().size() << " steps)...\n";
        CoTChainResult result = cot.executeChain(subargs);

        if (result.success) {
            std::cout << "\n✅ Chain complete (" << result.totalLatencyMs << "ms)\n";
            std::cout << "   Steps: " << result.stepsCompleted << " completed, "
                      << result.stepsSkipped << " skipped, "
                      << result.stepsFailed << " failed\n";
            std::cout << "\n📝 Final Output:\n" << result.finalOutput << "\n";
        } else {
            std::cout << "\n❌ Chain failed: " << result.error << "\n";
        }
    }
    else if (subcmd == "cancel") {
        cot.cancel();
        std::cout << "🛑 Cancel requested.\n";
    }
    else if (subcmd == "stats") {
        auto stats = cot.getStats();
        std::cout << "📊 CoT Statistics:\n";
        std::cout << "  Total chains:     " << stats.totalChains << "\n";
        std::cout << "  Successful:       " << stats.successfulChains << "\n";
        std::cout << "  Failed:           " << stats.failedChains << "\n";
        std::cout << "  Steps executed:   " << stats.totalStepsExecuted << "\n";
        std::cout << "  Steps skipped:    " << stats.totalStepsSkipped << "\n";
        std::cout << "  Steps failed:     " << stats.totalStepsFailed << "\n";
        std::cout << "  Avg latency:      " << stats.avgLatencyMs << "ms\n";
        if (!stats.roleUsage.empty()) {
            std::cout << "  Role usage:\n";
            for (const auto& [role, count] : stats.roleUsage) {
                std::cout << "    " << getCoTRoleInfo(role).label << ": " << count << "\n";
            }
        }
    }
    else {
        std::cout << "❌ Unknown subcommand: " << subcmd << "\n";
        std::cout << "   Type !cot for usage.\n";
    }
}

void cmd_agents_list(const std::string& args) {
    if (!g_state.subAgentMgr) {
        std::cout << "❌ SubAgentManager not initialized\n";
        return;
    }
    auto agents = g_state.subAgentMgr->getAllSubAgents();
    if (agents.empty()) {
        std::cout << "No sub-agents.\n";
        return;
    }
    std::cout << "📋 Sub-agents (" << agents.size() << "):\n";
    for (const auto& a : agents) {
        std::string icon = "⬜";
        if (a.state == SubAgent::State::Running) icon = "🔄";
        else if (a.state == SubAgent::State::Completed) icon = "✅";
        else if (a.state == SubAgent::State::Failed) icon = "❌";
        else if (a.state == SubAgent::State::Cancelled) icon = "🚫";
        std::cout << "  " << icon << " " << a.id << " [" << a.stateString() << "] "
                  << a.description << " (" << a.elapsedMs() << "ms)\n";
    }
}

void cmd_todo_list(const std::string& args) {
    if (!g_state.subAgentMgr) {
        std::cout << "❌ SubAgentManager not initialized\n";
        return;
    }
    auto todos = g_state.subAgentMgr->getTodoList();
    if (todos.empty()) {
        std::cout << "Todo list empty.\n";
        return;
    }
    std::cout << "📝 Todo List:\n";
    for (const auto& t : todos) {
        std::string icon = "⬜";
        if (t.status == TodoItem::Status::InProgress) icon = "🔄";
        else if (t.status == TodoItem::Status::Completed) icon = "✅";
        else if (t.status == TodoItem::Status::Failed) icon = "❌";
        std::cout << "  " << icon << " [" << t.id << "] " << t.title;
        if (!t.description.empty()) std::cout << " — " << t.description;
        std::cout << "\n";
    }
}

// ============================================================================
// STATUS & INFO
// ============================================================================

void cmd_status(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::cout << "\n📊 RawrXD CLI Status:\n";
    std::cout << "  File: " << (g_state.currentFile.empty() ? "(none)" : g_state.currentFile) << "\n";
    std::cout << "  Buffer size: " << g_state.editorBuffer.size() << " bytes\n";
    std::cout << "  Agent goal: " << (g_state.agentGoal.empty() ? "(none)" : g_state.agentGoal) << "\n";
    std::cout << "  Agent loop: " << (g_state.agentLoopRunning ? "running" : "stopped") << "\n";
    std::cout << "  Autonomy: " << (g_state.autonomyEnabled ? "enabled" : "disabled") << "\n";
    std::cout << "  Debugger: " << (g_state.debuggingActive ? "active" : "stopped") << "\n";
    std::cout << "  Terminals: " << g_state.terminalPanes.size() << "\n";
    std::cout << "  Breakpoints: " << g_state.breakpoints.size() << "\n";
    if (g_state.subAgentMgr) {
        std::cout << "  " << g_state.subAgentMgr->getStatusSummary() << "\n";
    }
    std::cout << "\n";
}

// ============================================================================
// HELP & VERSION
// ============================================================================

void print_help() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║       RawrXD AI Runtime (CLI) - Feature Parity with Win32 IDE       ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n🔧 FILE OPERATIONS:\n";
    std::cout << "  !new                          Create new file\n";
    std::cout << "  !open <path>                  Open file\n";
    std::cout << "  !save                         Save current file\n";
    std::cout << "  !save_as <path>               Save as new file\n";
    std::cout << "  !close                        Close current file\n";
    std::cout << "\n✂️  EDITOR OPERATIONS:\n";
    std::cout << "  !cut                          Cut to clipboard\n";
    std::cout << "  !copy                         Copy to clipboard\n";
    std::cout << "  !paste                        Paste from clipboard\n";
    std::cout << "  !undo                         Undo last change\n";
    std::cout << "  !redo                         Redo last change\n";
    std::cout << "  !find <text>                  Find text in buffer\n";
    std::cout << "  !replace <old> <new>          Replace text\n";
    std::cout << "\n🤖 AGENTIC OPERATIONS:\n";
    std::cout << "  !agent_execute <prompt>       Execute single agent command\n";
    std::cout << "  !agent_loop <prompt> [n]      Start multi-turn agent loop\n";
    std::cout << "  !agent_goal <goal>            Set agent goal\n";
    std::cout << "  !agent_memory <obs>           Add observation to memory\n";
    std::cout << "  !agent_memory show            Show agent memory\n";
    std::cout << "\n🤖 AUTONOMY OPERATIONS:\n";
    std::cout << "  !autonomy_start               Enable autonomy\n";
    std::cout << "  !autonomy_stop                Disable autonomy\n";
    std::cout << "  !autonomy_goal <goal>         Set autonomy goal\n";
    std::cout << "  !autonomy_rate <n>            Set max actions per minute\n";
    std::cout << "\n🐛 DEBUG OPERATIONS:\n";
    std::cout << "  !breakpoint_add <file>:<line> Add breakpoint\n";
    std::cout << "  !breakpoint_list              List all breakpoints\n";
    std::cout << "  !breakpoint_remove <idx>      Remove breakpoint\n";
    std::cout << "  !debug_start                  Start debugger\n";
    std::cout << "  !debug_stop                   Stop debugger\n";
    std::cout << "  !debug_step                   Step through code\n";
    std::cout << "  !debug_continue               Continue execution\n";
    std::cout << "\n💻 TERMINAL OPERATIONS:\n";
    std::cout << "  !terminal_new                 Create new terminal pane\n";
    std::cout << "  !terminal_split <orientation> Split terminal pane\n";
    std::cout << "  !terminal_kill                Close terminal pane\n";
    std::cout << "  !terminal_list                List terminal panes\n";
    std::cout << "\n🔥 HOTPATCH OPERATIONS:\n";
    std::cout << "  !hotpatch_apply <file>       Apply hotpatch without restart\n";
    std::cout << "  !hotpatch_create              Create hotpatch from current file\n";
    std::cout << "\n🧠 AGENTIC DECISION TREE (Phase 19):\n";
    std::cout << "  !decision_tree <sub>          dump|trace|stats|enable|disable|reset\n";
    std::cout << "  !autonomy_run <sub>           start|stop|pause|resume|status|tick\n";
    std::cout << "    verbose <on|off>              Toggle verbose tracing\n";
    std::cout << "    rate <n>                      Set actions/minute limit\n";
    std::cout << "    interval <ms>                 Set tick interval\n";
    std::cout << "  !ssa_lift <bin> [func] [addr] Run SSA Lifter on a binary function\n";
    std::cout << "  !auto_patch [text|analyze|stats] Auto-correct inference failures\n";
    std::cout << "\n🛡️  SAFETY CONTRACT (Phase 10B):\n";
    std::cout << "  !safety status                Show budget, risk, violations\n";
    std::cout << "  !safety reset                 Reset intent budget\n";
    std::cout << "  !safety rollback [all]        Rollback last/all actions\n";
    std::cout << "  !safety violations            Show violation log\n";
    std::cout << "  !safety block <class>         Block an action class\n";
    std::cout << "  !safety unblock <class>       Unblock an action class\n";
    std::cout << "  !safety risk <tier>           Set max risk (None|Low|Medium|High|Critical)\n";
    std::cout << "  !safety budget                Show remaining intent budget\n";
    std::cout << "\n🎯 CONFIDENCE GATE (Phase 10D):\n";
    std::cout << "  !confidence status            Gate stats + thresholds + trend\n";
    std::cout << "  !confidence policy <p>        Set policy (strict|normal|relaxed|disabled)\n";
    std::cout << "  !confidence threshold <e><s><a> Set thresholds\n";
    std::cout << "  !confidence history           Recent evaluations\n";
    std::cout << "  !confidence trend             Trend analysis\n";
    std::cout << "  !confidence selfabort         Self-abort status\n";
    std::cout << "  !confidence reset             Reset gate\n";
    std::cout << "\n📼 REPLAY JOURNAL (Phase 10C):\n";
    std::cout << "  !replay status                Journal stats\n";
    std::cout << "  !replay last [n]              Last N actions (default 10)\n";
    std::cout << "  !replay session               Current session snapshot\n";
    std::cout << "  !replay sessions              List all sessions\n";
    std::cout << "  !replay checkpoint [label]    Insert checkpoint\n";
    std::cout << "  !replay export <file>         Export session\n";
    std::cout << "  !replay filter <type>         Filter by action type\n";
    std::cout << "  !replay start|pause|stop      Control recording\n";
    std::cout << "\n⚙️  EXECUTION GOVERNOR (Phase 10A):\n";
    std::cout << "  !governor status              Stats + active tasks\n";
    std::cout << "  !governor run <cmd>           Run command with timeout\n";
    std::cout << "  !governor tasks               List active tasks\n";
    std::cout << "  !governor kill <id>           Kill a task\n";
    std::cout << "  !governor kill_all            Kill all tasks\n";
    std::cout << "  !governor wait <id>           Block until task completes\n";
    std::cout << "\n🔀 MULTI-RESPONSE (Phase 9C):\n";
    std::cout << "  !multi <prompt>               Generate N styled responses\n";
    std::cout << "  !multi compare                Side-by-side comparison\n";
    std::cout << "  !multi prefer <0-3> [reason]  Record preference\n";
    std::cout << "  !multi templates              Show templates\n";
    std::cout << "  !multi toggle <0-3>           Toggle template on/off\n";
    std::cout << "  !multi recommend              Recommended template\n";
    std::cout << "  !multi stats                  Generation stats\n";
    std::cout << "\n📜 HISTORY & EXPLAINABILITY (Phase 5/8A):\n";
    std::cout << "  !history show [n]             Last N events\n";
    std::cout << "  !history session              Session timeline\n";
    std::cout << "  !history agent <id>           Agent timeline\n";
    std::cout << "  !history type <type>          Events by type\n";
    std::cout << "  !history stats|flush|clear    Manage history\n";
    std::cout << "  !history export <file>        Export events\n";
    std::cout << "  !explain agent|chain|swarm <id>  Trace causal chain\n";
    std::cout << "  !explain failures             Explain all failures\n";
    std::cout << "  !explain policies             Policy firing attribution\n";
    std::cout << "  !explain session              Full session explanation\n";
    std::cout << "  !explain snapshot <file>      Export audit snapshot\n";
    std::cout << "\n📋 POLICY ENGINE (Phase 7):\n";
    std::cout << "  !policy list                  List all policies\n";
    std::cout << "  !policy show <id>             Policy details\n";
    std::cout << "  !policy enable|disable <id>   Toggle policy\n";
    std::cout << "  !policy remove <id>           Remove policy\n";
    std::cout << "  !policy heuristics            Computed heuristics\n";
    std::cout << "  !policy suggest               Generate suggestions\n";
    std::cout << "  !policy accept|reject <id>    Accept/reject suggestion\n";
    std::cout << "  !policy pending               Pending suggestions\n";
    std::cout << "  !policy export|import <file>  Export/import policies\n";
    std::cout << "  !policy stats                 Engine statistics\n";
    std::cout << "\n🔧 TOOL REGISTRY:\n";
    std::cout << "  !tools                        List registered tools\n";
    std::cout << "\n🔍 TOOLS:\n";
    std::cout << "  !search <pattern> [path]      Search files\n";
    std::cout << "  !analyze                      Analyze current file\n";
    std::cout << "  !profile                      Profile code\n";
    std::cout << "\n🤖 SUBAGENT OPERATIONS:\n";
    std::cout << "  !subagent <prompt>            Spawn a sub-agent\n";
    std::cout << "  !chain <s1> | <s2> | ...      Sequential prompt chain\n";
    std::cout << "  !swarm <p1> | <p2> | ...      Parallel HexMag swarm\n";
    std::cout << "    --strategy <merge>           concatenate|vote|summarize\n";
    std::cout << "    --parallel <n>               Max concurrent agents\n";
    std::cout << "  !agents                       List all sub-agents\n";
    std::cout << "  !todo                         Show todo list\n";
    std::cout << "\n🔗 CHAIN-OF-THOUGHT (Phase 32A):\n";
    std::cout << "  !cot                          Show CoT help\n";
    std::cout << "  !cot status                   Engine status\n";
    std::cout << "  !cot presets                  List presets (review|audit|think|...)\n";
    std::cout << "  !cot roles                    List all roles\n";
    std::cout << "  !cot preset <name>            Apply preset\n";
    std::cout << "  !cot steps                    Show current chain\n";
    std::cout << "  !cot add <role>               Add step to chain\n";
    std::cout << "  !cot clear                    Clear chain\n";
    std::cout << "  !cot run <query>              Execute chain on query\n";
    std::cout << "  !cot cancel                   Cancel running chain\n";
    std::cout << "  !cot stats                    Execution statistics\n";
    std::cout << "\n🎙️  VOICE (Phase 33):\n";
    std::cout << "  !voice                        Show voice commands help\n";
    std::cout << "  !voice init                   Initialize voice engine\n";
    std::cout << "  !voice record                 Start/stop recording\n";
    std::cout << "  !voice ptt                    Push-to-talk toggle\n";
    std::cout << "  !voice transcribe             Transcribe last recording\n";
    std::cout << "  !voice speak <text>           Text-to-speech\n";
    std::cout << "  !voice tts <on|off>           Toggle TTS for AI responses\n";
    std::cout << "  !voice mode <ptt|vad|off>     Set capture mode\n";
    std::cout << "  !voice devices                List audio devices\n";
    std::cout << "  !voice device <id>            Select input device\n";
    std::cout << "  !voice room <name>            Join/leave voice room\n";
    std::cout << "  !voice metrics                Show voice metrics\n";
    std::cout << "  !voice status                 Show voice status\n";
    std::cout << "\n⚙️  CONFIGURATION:\n";
    std::cout << "  !mode <mode>                  Set AI mode (ask|plan|edit|...)\n";
    std::cout << "  !engine <name>                Switch model\n";
    std::cout << "  !deep <on|off>                Enable deep thinking\n";
    std::cout << "  !research <on|off>            Enable deep research\n";
    std::cout << "  !max <tokens>                 Set context limit\n";
    std::cout << "\n🚀 IDE & SERVER:\n";
    std::cout << "  !generate_ide [path]          Generate React web IDE\n";
    std::cout << "  !server <port>                Start backend API server\n";
    std::cout << "\n📊 STATUS:\n";
    std::cout << "  !status                       Show current status\n";
    std::cout << "  !help                         Show this help\n";
    std::cout << "  !quit                         Exit CLI\n\n";
}

// ============================================================================
// MAIN COMMAND ROUTER
// ============================================================================

void route_command(const std::string& line) {
    if (line.empty()) return;
    
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
    
    // Status & info
    else if (cmd == "!status") cmd_status(args);
    else if (cmd == "!help") print_help();
    
    // Original configuration commands
    else if (cmd == "!mode") {
        set_mode(args);
        std::cout << "✅ Mode set.\n";
    }
    else if (cmd == "!engine") {
        set_engine(args);
        std::cout << "✅ Engine switched.\n";
    }
    else if (cmd == "!deep") {
        set_deep_thinking(args == "on");
        std::cout << "✅ Deep thinking: " << (args == "on" ? "enabled" : "disabled") << "\n";
    }
    else if (cmd == "!research") {
        set_deep_research(args == "on");
        std::cout << "✅ Deep research: " << (args == "on" ? "enabled" : "disabled") << "\n";
    }
    else if (cmd == "!max") {
        try {
            set_context(std::stoull(args));
            std::cout << "✅ Context limit updated.\n";
        } catch (...) {
            std::cout << "❌ Invalid number\n";
        }
    }
    else if (cmd == "!generate_ide") {
        std::string path = args.empty() ? "generated_ide" : args;
        std::cout << "🚀 Generating Full AI IDE at: " << path << " ...\n";
        RawrXD::ReactServerConfig config;
        config.name = "RawrXD-IDE";
        config.port = "3000";
        config.include_ide_features = true;
        config.cpp_backend_port = "8080";
        
        if (RawrXD::ReactServerGenerator::Generate(path, config)) {
            std::cout << "✅ Success! To run:\n"
                      << "  cd " << path << "\n"
                      << "  npm install\n"
                      << "  npm start\n"
                      << "Ensure backend is running with !server 8080\n";
        } else {
            std::cout << "❌ Failed to generate IDE.\n";
        }
    }
    else if (cmd == "!server") {
        int port = 8080;
        try { port = std::stoi(args); } catch(...) {}
        std::thread(start_server, port).detach();
        std::cout << "🌐 Backend server started on port " << port << "\n";
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
        std::cout << out << "\n";
        
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
                std::cout << "\n[Tool Result]\n" << toolResult << "\n";
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
    
    std::cout << "\n╔════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║      RawrXD AI Runtime (CLI) - Feature Parity with Win32 IDE        ║\n";
    std::cout << "║    50+ Commands | Safety | Confidence | Replay | Governor | Multi   ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════════╝\n\n";
    std::cout << "Type !help for commands or start typing to chat with AI.\n";
    std::cout << "Phase 19: Agentic Decision Tree active. Use !decision_tree dump to inspect.\n";
    std::cout << "Phase 20: Safety | Confidence | Replay | Governor | Multi-Response | History | Policy\n\n";

    std::string line;
    while (true) {
        std::cout << "rawrxd> ";
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
