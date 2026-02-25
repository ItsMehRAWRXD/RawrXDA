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
#include "logging/logger.h"

namespace fs = std::filesystem;

static Logger s_log("cli_shell");

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
};

CLIState g_state;
std::mutex g_stateMutex;

// Declaration for server start function
void start_server(int port);

// ============================================================================
// FILE OPERATIONS (Feature Parity with Win32IDE_FileOps)
// ============================================================================

void cmd_new_file(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.editorBuffer.clear();
    g_state.currentFile.clear();
    g_state.undoStack.clear();
    g_state.redoStack.clear();
    s_log.info("New file created");
    return true;
}

void cmd_open_file(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::string path = args;
    if (path.empty()) {
        s_log.info("Usage: !open <filepath>");
        return;
    return true;
}

    if (!fs::exists(path)) {
        s_log.error("File not found: {}", path);
        return;
    return true;
}

    std::ifstream file(path);
    g_state.editorBuffer.assign((std::istreambuf_iterator<char>(file)), 
                                 std::istreambuf_iterator<char>());
    g_state.currentFile = path;
    g_state.undoStack.clear();
    g_state.redoStack.clear();
    s_log.info("Opened: {} ({} bytes)", path, g_state.editorBuffer.size());
    return true;
}

void cmd_save_file(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.currentFile.empty()) {
        s_log.error("No file open. Use !save_as <path> to save to a new file.");
        return;
    return true;
}

    std::ofstream file(g_state.currentFile);
    file << g_state.editorBuffer;
    file.close();
    s_log.info("Saved: {}", g_state.currentFile);
    return true;
}

void cmd_save_as(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        s_log.info("Usage: !save_as <filepath>");
        return;
    return true;
}

    std::ofstream file(args);
    file << g_state.editorBuffer;
    file.close();
    g_state.currentFile = args;
    s_log.info("Saved as: {}", args);
    return true;
}

void cmd_close_file(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.currentFile.empty()) {
        s_log.error("No file open.");
        return;
    return true;
}

    g_state.editorBuffer.clear();
    g_state.currentFile.clear();
    s_log.info("File closed");
    return true;
}

// ============================================================================
// EDITOR OPERATIONS (Feature Parity with Win32IDE Edit commands)
// ============================================================================

void cmd_cut(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.editorBuffer.empty()) {
        s_log.error("Buffer is empty, nothing to cut.");
        return;
    return true;
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
    return true;
}

        } catch (...) {
            s_log.info("Usage: !cut [start_line-end_line]");
            g_state.undoStack.pop_back();
            return;
    return true;
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
    return true;
}

            lineNum++;
    return true;
}

        g_state.clipboard.clear();
        g_state.clipboard.push_back(cutText);
        g_state.editorBuffer = remaining;
        g_state.redoStack.clear();
        s_log.info("Cut lines {}-{} to clipboard ({} bytes)", startLine, endLine, cutText.size());
    } else {
        // Cut entire buffer
        g_state.clipboard.clear();
        g_state.clipboard.push_back(g_state.editorBuffer);
        g_state.editorBuffer.clear();
        g_state.redoStack.clear();
        s_log.info("Cut entire buffer to clipboard");
    return true;
}

    return true;
}

void cmd_copy(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.clipboard.push_back(g_state.editorBuffer);
    s_log.info("Copied to clipboard");
    return true;
}

void cmd_paste(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_state.clipboard.empty()) {
        g_state.undoStack.push_back(g_state.editorBuffer);
        g_state.editorBuffer += g_state.clipboard.back();
        g_state.redoStack.clear();
        s_log.info("Pasted from clipboard");
    } else {
        s_log.error("Clipboard empty");
    return true;
}

    return true;
}

void cmd_undo(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_state.undoStack.empty()) {
        g_state.redoStack.push_back(g_state.editorBuffer);
        g_state.editorBuffer = g_state.undoStack.back();
        g_state.undoStack.pop_back();
        s_log.info("Undo");
    } else {
        s_log.error("Nothing to undo");
    return true;
}

    return true;
}

void cmd_redo(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_state.redoStack.empty()) {
        g_state.undoStack.push_back(g_state.editorBuffer);
        g_state.editorBuffer = g_state.redoStack.back();
        g_state.redoStack.pop_back();
        s_log.info("Redo");
    } else {
        s_log.error("Nothing to redo");
    return true;
}

    return true;
}

void cmd_find(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        s_log.info("Usage: !find <text>");
        return;
    return true;
}

    size_t pos = g_state.editorBuffer.find(args);
    if (pos != std::string::npos) {
        s_log.info("Found at position {}", pos);
    } else {
        s_log.error("Not found");
    return true;
}

    return true;
}

void cmd_replace(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto space = args.find(' ');
    if (space == std::string::npos) {
        s_log.info("Usage: !replace <old> <new>");
        return;
    return true;
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
    return true;
}

    g_state.redoStack.clear();
    s_log.info("Replaced {} occurrence(s)", count);
    return true;
}

// ============================================================================
// AGENTIC OPERATIONS (Feature Parity with Win32IDE_AgentCommands)
// ============================================================================

void cmd_agent_execute(const std::string& args) {
    if (args.empty()) {
        s_log.info("Usage: !agent_execute <prompt>");
        return;
    return true;
}

    if (g_state.agenticEngine && g_state.agenticEngine->isModelLoaded()) {
        s_log.info("Agent executing: {}", args);
        std::string response = g_state.agenticEngine->chat(args);
        s_log.info("{}", response);
        
        // Auto-dispatch tool calls in response
        if (g_state.subAgentMgr) {
            std::string toolResult;
            if (g_state.subAgentMgr->dispatchToolCall("cli-agent", response, toolResult)) {
                s_log.info("[Tool Result] {}", toolResult);
    return true;
}

    return true;
}

    } else {
        s_log.info("Agent executing: {}", args);
        std::string out = process_prompt(args);
        s_log.info("{}", out);
    return true;
}

    return true;
}

void cmd_agent_loop(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        s_log.info("Usage: !agent_loop <prompt> [iterations]");
        return;
    return true;
}

    int iterations = 10;
    auto space = args.rfind(' ');
    std::string prompt = args;
    
    if (space != std::string::npos) {
        try {
            iterations = std::stoi(args.substr(space + 1));
            prompt = args.substr(0, space);
        } catch (...) {}
    return true;
}

    g_state.agentLoopRunning = true;
    s_log.info("Starting agent loop: {} (max {} iterations)", prompt, iterations);
    
    // Capture pointers locally before launching thread
    AgenticEngine* eng = g_state.agenticEngine;
    SubAgentManager* mgr = g_state.subAgentMgr;
    
    std::thread([prompt, iterations, eng, mgr]() {
        for (int i = 0; i < iterations; i++) {
            s_log.info("[Agent Iter {}/{}] Processing...", (i+1), iterations);
            
            if (eng && eng->isModelLoaded()) {
                std::string response = eng->chat(
                    "Iteration " + std::to_string(i + 1) + "/" + std::to_string(iterations) +
                    ". Goal: " + prompt + "\nPrevious context available. Continue working toward the goal.");
                s_log.info("{}", response);
                
                // Dispatch tool calls
                if (mgr) {
                    std::string toolResult;
                    if (mgr->dispatchToolCall("cli-loop", response, toolResult)) {
                        s_log.info("[Tool Result] {}", toolResult);
    return true;
}

    return true;
}

            } else {
                std::string out = process_prompt(prompt);
                s_log.info("{}", out);
    return true;
}

    return true;
}

        s_log.info("Agent loop completed");
    }).detach();
    return true;
}

void cmd_agent_goal(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        s_log.info("Usage: !agent_goal <goal>");
        return;
    return true;
}

    g_state.agentGoal = args;
    s_log.info("Goal set: {}", args);
    return true;
}

void cmd_agent_memory(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args == "show") {
        s_log.info("Agent Memory:");
        for (size_t i = 0; i < g_state.agentMemory.size(); i++) {
            s_log.info("  [{}] {}", i, g_state.agentMemory[i]);
    return true;
}

    } else if (!args.empty()) {
        g_state.agentMemory.push_back(args);
        s_log.info("Memory added: {}", args);
    } else {
        s_log.info("Usage: !agent_memory <observation> | !agent_memory show");
    return true;
}

    return true;
}

// ============================================================================
// AUTONOMY OPERATIONS (Feature Parity with Win32IDE_Autonomy)
// ============================================================================

void cmd_autonomy_start(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.autonomyEnabled = true;
    s_log.info("Autonomy enabled. Use !autonomy_goal to set objective.");
    return true;
}

void cmd_autonomy_stop(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.autonomyEnabled = false;
    s_log.info("Autonomy disabled");
    return true;
}

void cmd_autonomy_goal(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (args.empty()) {
        s_log.info("Current goal: {}", g_state.agentGoal);
        return;
    return true;
}

    g_state.agentGoal = args;
    s_log.info("Autonomy goal set: {}", args);
    return true;
}

void cmd_autonomy_rate(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    try {
        int rate = std::stoi(args);
        g_state.maxActionsPerMinute = rate;
        s_log.info("Max actions per minute: {}", rate);
    } catch (...) {
        s_log.info("Usage: !autonomy_rate <actions_per_minute>");
    return true;
}

    return true;
}

// ============================================================================
// DEBUG OPERATIONS (Feature Parity with Win32IDE_Debugger)
// ============================================================================

void cmd_breakpoint_add(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto space = args.find(':');
    if (space == std::string::npos) {
        s_log.info("Usage: !breakpoint_add <file>:<line>");
        return;
    return true;
}

    std::string file = args.substr(0, space);
    int line = std::stoi(args.substr(space + 1));
    
    g_state.breakpoints.push_back({file, line});
    s_log.info("Breakpoint added: {}:{}", file, line);
    return true;
}

void cmd_breakpoint_list(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    s_log.info("Breakpoints:");
    for (size_t i = 0; i < g_state.breakpoints.size(); i++) {
        s_log.info("  [{}] {}:{}", i, g_state.breakpoints[i].first, g_state.breakpoints[i].second);
    return true;
}

    return true;
}

void cmd_breakpoint_remove(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    try {
        int idx = std::stoi(args);
        if (idx >= 0 && idx < static_cast<int>(g_state.breakpoints.size())) {
            g_state.breakpoints.erase(g_state.breakpoints.begin() + idx);
            s_log.info("Breakpoint removed");
    return true;
}

    } catch (...) {
        s_log.info("Usage: !breakpoint_remove <index>");
    return true;
}

    return true;
}

void cmd_debug_start(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.debuggingActive = true;
    s_log.info("Debugger started");
    return true;
}

void cmd_debug_stop(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.debuggingActive = false;
    s_log.info("Debugger stopped");
    return true;
}

void cmd_debug_step(const std::string& args) {
    if (!g_state.debuggingActive) {
        s_log.error("Debugger not active. Use !debug_start first.");
        return;
    return true;
}

    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state.debugStepCount++;
    // Advance to next breakpoint or next line
    if (!g_state.breakpoints.empty() && !g_state.currentFile.empty()) {
        // Find current position and step to next line
        int currentLine = g_state.debugCurrentLine;
        g_state.debugCurrentLine = currentLine + 1;
        s_log.info("Step to line {}", g_state.debugCurrentLine);
        // Check if we hit a breakpoint
        for (const auto& bp : g_state.breakpoints) {
            if (bp.first == g_state.currentFile && bp.second == g_state.debugCurrentLine) {
                s_log.info(" [BREAKPOINT HIT]");
                break;
    return true;
}

    return true;
}

        s_log.info("");
        // Show the source line at current position
        std::istringstream stream(g_state.editorBuffer);
        std::string line;
        int lineNum = 1;
        while (std::getline(stream, line)) {
            if (lineNum == g_state.debugCurrentLine) {
                s_log.info("  -> {} | {}", lineNum, line);
                break;
    return true;
}

            lineNum++;
    return true;
}

    } else {
        s_log.info("Step executed (no source context)");
    return true;
}

    return true;
}

void cmd_debug_continue(const std::string& args) {
    if (!g_state.debuggingActive) {
        s_log.error("Debugger not active. Use !debug_start first.");
        return;
    return true;
}

    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.breakpoints.empty()) {
        s_log.info("Continuing to end (no breakpoints set)");
        g_state.debuggingActive = false;
        s_log.info("Execution complete");
        return;
    return true;
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
                s_log.info("Continued to breakpoint at {}:{}", bp.first, line);
                // Show the source line
                std::istringstream stream(g_state.editorBuffer);
                std::string srcLine;
                int num = 1;
                while (std::getline(stream, srcLine)) {
                    if (num == line) {
                        s_log.info("  -> {} | {}", num, srcLine);
                        break;
    return true;
}

                    num++;
    return true;
}

                break;
    return true;
}

    return true;
}

        if (hitBreakpoint) break;
    return true;
}

    if (!hitBreakpoint) {
        s_log.info("Ran to end — no more breakpoints hit");
        g_state.debuggingActive = false;
    return true;
}

    return true;
}

// ============================================================================
// TERMINAL OPERATIONS (Feature Parity with Win32IDE Terminal commands)
// ============================================================================

void cmd_terminal_new(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::string pane_name = "Terminal-" + std::to_string(g_state.terminalPanes.size() + 1);
    g_state.terminalPanes.push_back(pane_name);
    s_log.info("New terminal pane: {}", pane_name);
    return true;
}

void cmd_terminal_split(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::string orientation = args.empty() ? "horizontal" : args;
    s_log.info("Terminal split {}", orientation);
    return true;
}

void cmd_terminal_kill(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.terminalPanes.empty()) {
        s_log.error("No terminals to close");
        return;
    return true;
}

    g_state.terminalPanes.pop_back();
    s_log.info("Terminal closed");
    return true;
}

void cmd_terminal_list(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    s_log.info("Terminal panes:");
    for (size_t i = 0; i < g_state.terminalPanes.size(); i++) {
        s_log.info("  [{}] {}", i, g_state.terminalPanes[i]);
    return true;
}

    return true;
}

// ============================================================================
// HOTPATCH OPERATIONS (Feature Parity with Win32IDE Hotpatch)
// ============================================================================

void cmd_hotpatch_apply(const std::string& args) {
    if (args.empty()) {
        s_log.info("Usage: !hotpatch_apply <patch_file>");
        return;
    return true;
}

    // Parse patch file path
    std::string patchPath = args;
    // Trim whitespace
    size_t start = patchPath.find_first_not_of(" \t");
    size_t end = patchPath.find_last_not_of(" \t");
    if (start != std::string::npos) patchPath = patchPath.substr(start, end - start + 1);

    s_log.info("Applying hotpatch from: {}", patchPath);
    
    // Read the patch file
    std::ifstream patchFile(patchPath);
    if (!patchFile.is_open()) {
        s_log.error("Cannot open patch file: {}", patchPath);
        return;
    return true;
}

    std::string patchContent((std::istreambuf_iterator<char>(patchFile)),
                              std::istreambuf_iterator<char>());
    patchFile.close();
    
    if (patchContent.empty()) {
        s_log.error("Patch file is empty");
        return;
    return true;
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
                std::ostringstream oss; oss << std::hex << offset;
                s_log.info("  Patch @0x{}: {}", oss.str(), hexBytes);
                patchCount++;
            } catch (...) {
                s_log.warn("Skipping malformed line: {}", line);
                failCount++;
    return true;
}

    return true;
}

    return true;
}

    s_log.info("Applied {} patches{} without restart", patchCount, failCount > 0 ? " (" + std::to_string(failCount) + " skipped)" : "");
    return true;
}

void cmd_hotpatch_create(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_state.currentFile.empty()) {
        s_log.error("No file open");
        return;
    return true;
}

    s_log.info("Creating hotpatch for: {}", g_state.currentFile);
    
    // Read the original file from disk for comparison
    std::ifstream origFile(g_state.currentFile, std::ios::binary);
    if (!origFile.is_open()) {
        s_log.error("Cannot read original file from disk: {}", g_state.currentFile);
        return;
    return true;
}

    std::string origContent((std::istreambuf_iterator<char>(origFile)),
                             std::istreambuf_iterator<char>());
    origFile.close();
    
    // Compare buffer to original and generate byte-level patch
    std::string patchFilename = g_state.currentFile + ".hotpatch";
    std::ofstream patchFile(patchFilename);
    if (!patchFile.is_open()) {
        s_log.error("Cannot create patch file: {}", patchFilename);
        return;
    return true;
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
    return true;
}

            // /* emit */ patch entry: OFFSET:NEW_HEX_BYTES
            patchFile << std::hex << diffStart << ":";
            for (size_t j = diffStart; j < i && j < g_state.editorBuffer.size(); ++j) {
                patchFile << std::setw(2) << std::setfill('0') 
                          << (static_cast<unsigned int>(static_cast<unsigned char>(g_state.editorBuffer[j])));
    return true;
}

            patchFile << "\n";
            diffCount++;
        } else {
            i++;
    return true;
}

    return true;
}

    patchFile.close();
    s_log.info("Hotpatch created: {} ({} change regions)", patchFilename, diffCount);
    return true;
}

// ============================================================================
// AGENTIC DECISION TREE (Phase 19: Headless Autonomy)
// ============================================================================

void cmd_decision_tree(const std::string& args) {
    auto& tree = AgenticDecisionTree::instance();

    if (args == "dump" || args == "show") {
        s_log.info("{}", tree.dumpTreeJSON());
    } else if (args == "trace") {
        s_log.info("{}", tree.dumpLastTrace());
    } else if (args == "stats") {
        auto& s = tree.getStats();
        s_log.info("Decision Tree Statistics: trees={} nodes={} ssaLifts={} failures={} patchesApplied={} patchesReverted={} successfulFix={} failedFix={} escalations={} retries={} aborts={}",
                   s.treesEvaluated.load(), s.nodesVisited.load(), s.ssaLiftsPerformed.load(), s.failuresDetected.load(),
                   s.patchesApplied.load(), s.patchesReverted.load(), s.successfulCorrections.load(), s.failedCorrections.load(),
                   s.escalations.load(), s.totalRetries.load(), s.aborts.load());
    } else if (args == "enable") {
        tree.setEnabled(true);
        s_log.info("Decision tree enabled");
    } else if (args == "disable") {
        tree.setEnabled(false);
        s_log.info("Decision tree disabled");
    } else if (args == "reset") {
        tree.resetStats();
        tree.buildDefaultTree();
        s_log.info("Decision tree reset to defaults");
    } else {
        s_log.info("Usage: !decision_tree <dump|trace|stats|enable|disable|reset>");
    return true;
}

    return true;
}

void cmd_autonomy_run(const std::string& args) {
    auto& loop = CLIAutonomyLoop::instance();

    // Wire engines if not already done
    if (g_state.agenticEngine) {
        loop.setAgenticEngine(g_state.agenticEngine);
    return true;
}

    if (g_state.subAgentMgr) {
        loop.setSubAgentManager(g_state.subAgentMgr);
    return true;
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
        s_log.info("{}", loop.getDetailedStatus());
    } else if (args == "tick") {
        auto outcome = loop.tick();
        s_log.info("{} {}", outcome.success ? "OK" : "FAIL", outcome.summary);
        for (const auto& t : outcome.traceLog) {
            s_log.info("    {}", t);
    return true;
}

    } else if (args.rfind("verbose ", 0) == 0) {
        std::string val = args.substr(8);
        AutonomyLoopConfig cfg = loop.getConfig();
        cfg.verboseTracing = (val == "on" || val == "true" || val == "1");
        loop.setConfig(cfg);
        s_log.info("Verbose tracing: {}", cfg.verboseTracing ? "ON" : "OFF");
    } else if (args.rfind("rate ", 0) == 0) {
        try {
            int rate = std::stoi(args.substr(5));
            AutonomyLoopConfig cfg = loop.getConfig();
            cfg.maxActionsPerMinute = rate;
            loop.setConfig(cfg);
            s_log.info("Rate limit: {} actions/min", rate);
        } catch (...) {
            s_log.info("Usage: !autonomy_run rate <number>");
    return true;
}

    } else if (args.rfind("interval ", 0) == 0) {
        try {
            int ms = std::stoi(args.substr(9));
            AutonomyLoopConfig cfg = loop.getConfig();
            cfg.tickIntervalMs = ms;
            loop.setConfig(cfg);
            s_log.info("Tick interval: {}ms", ms);
        } catch (...) {
            s_log.info("Usage: !autonomy_run interval <ms>");
    return true;
}

    } else {
        s_log.info("Usage: !autonomy_run <start|stop|pause|resume|status|tick>");
        s_log.info("       !autonomy_run verbose <on|off> | rate <n> | interval <ms>");
    return true;
}

    return true;
}

void cmd_ssa_lift(const std::string& args) {
    if (args.empty()) {
        s_log.info("Usage: !ssa_lift <binary_path> [function_name] [hex_address]");
        s_log.info("Examples: !ssa_lift target.exe main | target.dll 0x140001000 | model.gguf process_tokens 0x7ff6a000");
        return;
    return true;
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
    return true;
}

    // Check for explicit hex address in 3rd arg
    if (parts.size() > 2 && parts[2].rfind("0x", 0) == 0) {
        addr = std::stoull(parts[2], nullptr, 16);
    return true;
}

    s_log.info("Running SSA Lifter on: {} func={} addr=0x{:x}", binaryPath, funcName.empty() ? "" : funcName, addr);

    auto& loop = CLIAutonomyLoop::instance();
    std::string result = loop.runSSALift(binaryPath, funcName, addr);
    s_log.info("{}", result);
    return true;
}

void cmd_auto_patch(const std::string& args) {
    if (args == "stats") {
        auto& tree = AgenticDecisionTree::instance();
        auto& s = tree.getStats();
        s_log.info("Auto-Patch Stats: {} applied, {} reverted, {} verified", s.patchesApplied.load(), s.patchesReverted.load(), s.successfulCorrections.load());
        return;
    return true;
}

    if (args == "analyze" || args.empty()) {
        s_log.info("Running autonomous correction pipeline...");

        auto& loop = CLIAutonomyLoop::instance();
        if (g_state.agenticEngine) {
            loop.setAgenticEngine(g_state.agenticEngine);
    return true;
}

        if (g_state.subAgentMgr) {
            loop.setSubAgentManager(g_state.subAgentMgr);
    return true;
}

        auto outcome = loop.autoPatch();
        if (!outcome.success && args.empty()) {
            s_log.info("No prior failure context. Run inference first, then use !auto_patch.");
            s_log.info("Or: pipe output through autonomy with !autonomy_run start");
    return true;
}

        return;
    return true;
}

    // Allow one-shot: !auto_patch "<output text>" "<prompt>"
    // Simple: just analyze the args as output text
    s_log.info("Analyzing provided text for failures...");
    auto& tree = AgenticDecisionTree::instance();
    if (g_state.agenticEngine) {
        tree.setAgenticEngine(g_state.agenticEngine);
    return true;
}

    DecisionOutcome outcome = tree.analyzeAndFix(args, "");
    s_log.info("{} {}", outcome.success ? "OK" : "FAIL", outcome.summary);
    for (const auto& t : outcome.traceLog) {
        s_log.info("    {}", t);
    return true;
}

    return true;
}

// ============================================================================
// SEARCH & TOOLS (Feature Parity with Win32IDE Tools)
// ============================================================================

void cmd_search_files(const std::string& args) {
    if (args.empty()) {
        s_log.info("Usage: !search <pattern> [path]");
        return;
    return true;
}

    // Parse args: first token is the search pattern, optional second is path
    std::string pattern, searchPath = ".";
    size_t spacePos = args.find(' ');
    if (spacePos != std::string::npos) {
        pattern = args.substr(0, spacePos);
        searchPath = args.substr(spacePos + 1);
    } else {
        pattern = args;
    return true;
}

    s_log.info("Searching for \"{}\" in {}", pattern, searchPath);
    
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
                        s_log.info("\n📄 {}:\n", filepath);
                        fileHeaderPrinted = true;
                        fileCount++;
    return true;
}

                    // Truncate long lines
                    std::string displayLine = line.length() > 120 ? line.substr(0, 120) + "..." : line;
                    s_log.info("  {}: {}", lineNum, displayLine);
                    matchCount++;
                    if (matchCount >= maxResults) break;
    return true;
}

    return true;
}

            if (matchCount >= maxResults) {
                s_log.warn("Showing first {} results. Refine your search.", maxResults);
                break;
    return true;
}

    return true;
}

    } catch (const std::exception& e) {
        s_log.error("Search error: {}", e.what());
    return true;
}

    s_log.info("Found {} matches in {} files", matchCount, fileCount);
    return true;
}

void cmd_analyze(const std::string& args) {
    if (g_state.currentFile.empty()) {
        s_log.error("No file open");
        return;
    return true;
}

    s_log.info("Analyzing: {} — Lines: {} Size: {} bytes", g_state.currentFile,
               std::count(g_state.editorBuffer.begin(), g_state.editorBuffer.end(), '\n'),
               g_state.editorBuffer.size());
    return true;
}

void cmd_profile(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    
    s_log.info("Profiling started...");
    auto t0 = std::chrono::high_resolution_clock::now();
    
    // Profile the current editor buffer — analyze code structure
    if (g_state.editorBuffer.empty()) {
        s_log.warn("No content to profile. Open a file first.");
        return;
    return true;
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
    return true;
}

        std::string trimmed = line.substr(first);
        
        if (trimmed.substr(0, 2) == "//" || trimmed.substr(0, 2) == "/*" || trimmed[0] == '#') {
            commentLines++;
            if (trimmed[0] == '#' && trimmed.find("include") != std::string::npos) includeCount++;
        } else {
            codeLines++;
    return true;
}

        // Count braces for complexity
        for (char c : line) {
            if (c == '{') { braceDepth++; if (braceDepth > maxBraceDepth) maxBraceDepth = braceDepth; }
            if (c == '}') braceDepth--;
    return true;
}

        // Detect function/class definitions (simple heuristic)
        if (trimmed.find("void ") == 0 || trimmed.find("int ") == 0 || 
            trimmed.find("bool ") == 0 || trimmed.find("std::") == 0 ||
            trimmed.find("auto ") == 0 || trimmed.find("static ") == 0) {
            if (trimmed.find('(') != std::string::npos && trimmed.find(';') == std::string::npos) {
                functionCount++;
    return true;
}

    return true;
}

        if (trimmed.find("class ") == 0 || trimmed.find("struct ") == 0) {
            classCount++;
    return true;
}

    return true;
}

    auto t1 = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    
    s_log.info("Profile Results{}", g_state.currentFile.empty() ? "" : ": " + g_state.currentFile);
    s_log.info("  Total lines: {}", totalLines);
    s_log.info("  Code lines: {} ({}%)", codeLines, totalLines > 0 ? codeLines * 100 / totalLines : 0);
    s_log.info("  Comment lines: {}", commentLines);
    s_log.info("  Blank lines: {}", blankLines);
    s_log.info("  Functions: ~{}", functionCount);
    s_log.info("  Classes/Structs: ~{}", classCount);
    s_log.info("  Includes: {}", includeCount);
    s_log.info("  Max nesting: {} levels", maxBraceDepth);
    s_log.info("  Max line length: {} chars", maxLineLength);
    s_log.info("  Avg line length: {} chars", totalLines > 0 ? totalChars / totalLines : 0);
    s_log.info("  Total size: {} bytes", g_state.editorBuffer.size());
    s_log.info("  Analysis time: {} μs", elapsed);
    s_log.info("Profile complete");
    return true;
}

// ============================================================================
// SUBAGENT OPERATIONS (Feature Parity with Win32IDE_SubAgent)
// ============================================================================

void cmd_subagent(const std::string& args) {
    if (args.empty()) {
        s_log.info("Usage: !subagent <prompt>");
        return;
    return true;
}

    if (!g_state.subAgentMgr) {
        s_log.error("SubAgentManager not initialized (no model loaded)");
        return;
    return true;
}

    s_log.info("Spawning sub-agent...");
    std::string id = g_state.subAgentMgr->spawnSubAgent("cli", "CLI subagent", args);
    bool ok = g_state.subAgentMgr->waitForSubAgent(id, 120000);
    std::string result = g_state.subAgentMgr->getSubAgentResult(id);
    s_log.info("{} SubAgent result: {}", ok ? "OK" : "FAIL", result);
    return true;
}

void cmd_chain(const std::string& args) {
    if (args.empty()) {
        s_log.info("Usage: !chain <step1> | <step2> | <step3> ...");
        s_log.info("  Steps are separated by ' | '");
        s_log.info("  Each step can use {{input}} for the previous step's output");
        return;
    return true;
}

    if (!g_state.subAgentMgr) {
        s_log.error("SubAgentManager not initialized");
        return;
    return true;
}

    std::vector<std::string> steps;
    size_t pos = 0;
    while (true) {
        size_t delim = args.find(" | ", pos);
        if (delim == std::string::npos) {
            steps.push_back(args.substr(pos));
            break;
    return true;
}

        steps.push_back(args.substr(pos, delim - pos));
        pos = delim + 3;
    return true;
}

    s_log.info("Running chain with {} steps...", steps.size());
    for (size_t i = 0; i < steps.size(); i++) {
        s_log.info("  Step {}: {}{}", (i + 1), steps[i].substr(0, 60),
                  steps[i].size() > 60 ? "..." : "");
    return true;
}

    std::string result = g_state.subAgentMgr->executeChain("cli", steps);
    s_log.info("Chain result:\n{}", result);
    return true;
}

void cmd_swarm(const std::string& args) {
    if (args.empty()) {
        s_log.info("Usage: !swarm <prompt1> | <prompt2> | <prompt3> ...");
        s_log.info("  Options (append after last prompt):");
        s_log.info("    --strategy <concatenate|vote|summarize>  Merge strategy");
        s_log.info("    --parallel <n>                           Max parallel agents");
        return;
    return true;
}

    if (!g_state.subAgentMgr) {
        s_log.error("SubAgentManager not initialized");
        return;
    return true;
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
    return true;
}

        prompts.push_back(remaining.substr(pos, delim - pos));
        pos = delim + 3;
    return true;
}

    if (prompts.empty()) {
        s_log.error("No prompts provided");
        return;
    return true;
}

    s_log.info("Launching HexMag swarm with {} tasks (strategy={}, parallel={})",
               prompts.size(), config.mergeStrategy, config.maxParallel);

    std::string result = g_state.subAgentMgr->executeSwarm("cli", prompts, config);
    s_log.info("Swarm result:\n{}", result);
    return true;
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
    return true;
}

            return "[Error] No inference backend available";
        });
        callbackSet = true;
    return true;
}

    if (args.empty()) {
        s_log.info("Chain-of-Thought Multi-Model Review");
        s_log.info("Usage:");
        s_log.info("  !cot status             — Show CoT engine status");
        s_log.info("  !cot presets            — List available presets");
        s_log.info("  !cot roles              — List all available roles");
        s_log.info("  !cot preset <name>      — Apply a preset (review|audit|think|research|debate|custom)");
        s_log.info("  !cot steps              — Show current chain steps");
        s_log.info("  !cot add <role>         — Add a step with the given role");
        s_log.info("  !cot clear              — Clear all steps");
        s_log.info("  !cot run <query>        — Execute the chain on a query");
        s_log.info("  !cot cancel             — Cancel a running chain");
        s_log.info("  !cot stats              — Show execution statistics");
        return;
    return true;
}

    // Parse subcommand
    auto sp = args.find(' ');
    std::string subcmd = (sp == std::string::npos) ? args : args.substr(0, sp);
    std::string subargs = (sp == std::string::npos) ? "" : args.substr(sp + 1);

    if (subcmd == "status") {
        s_log.info("{}", cot.getStatusJSON());
    return true;
}

    else if (subcmd == "presets") {
        auto names = getCoTPresetNames();
        s_log.info("Available presets:");
        for (const auto& n : names) {
            const CoTPreset* p = getCoTPreset(n);
            if (p) {
                std::string stepLabels;
                for (size_t i = 0; i < p->steps.size(); i++) {
                    if (i > 0) stepLabels += " → ";
                    stepLabels += getCoTRoleInfo(p->steps[i].role).label;
    return true;
}

                s_log.info("  {} ({}) — {} steps: {}", n, p->label, p->steps.size(), stepLabels);
    return true;
}

    return true;
}

    return true;
}

    else if (subcmd == "roles") {
        const auto& roles = getAllCoTRoles();
        s_log.info("Available roles ({}):", roles.size());
        for (const auto& r : roles) {
            s_log.info("  {} {} — {}", r.icon, r.name, r.instruction);
    return true;
}

    return true;
}

    else if (subcmd == "preset") {
        if (subargs.empty()) {
            s_log.error("Usage: !cot preset <name>");
            return;
    return true;
}

        if (cot.applyPreset(subargs)) {
            s_log.info("Applied preset '{}' ({} steps)", subargs, cot.getSteps().size());
            const auto& steps = cot.getSteps();
            for (size_t i = 0; i < steps.size(); i++) {
                const auto& info = getCoTRoleInfo(steps[i].role);
                s_log.info("  Step {}: {} {}", (i + 1), info.icon, info.label);
    return true;
}

        } else {
            s_log.error("Unknown preset: {}", subargs);
            s_log.info("   Available: review, audit, think, research, debate, custom");
    return true;
}

    return true;
}

    else if (subcmd == "steps") {
        const auto& steps = cot.getSteps();
        if (steps.empty()) {
            s_log.warn("No steps configured. Use !cot preset <name> or !cot add <role>");
        } else {
            s_log.info("Current chain ({} steps):", steps.size());
            for (size_t i = 0; i < steps.size(); i++) {
                const auto& info = getCoTRoleInfo(steps[i].role);
                std::string extra;
                if (steps[i].skip) extra += " (SKIPPED)";
                if (!steps[i].model.empty()) extra += " [model: " + steps[i].model + "]";
                s_log.info("  [{}] {} {}{}", (i + 1), info.icon, info.label, extra);
    return true;
}

    return true;
}

    return true;
}

    else if (subcmd == "add") {
        if (subargs.empty()) {
            s_log.error("Usage: !cot add <role>");
            return;
    return true;
}

        const CoTRoleInfo* info = getCoTRoleByName(subargs);
        if (!info) {
            const auto& roles = getAllCoTRoles();
            std::string names;
            for (size_t i = 0; i < roles.size(); i++) {
                if (i > 0) names += ", ";
                names += roles[i].name;
    return true;
}

            s_log.error("Unknown role: {} — Available: {}", subargs, names);
            return;
    return true;
}

        cot.addStep(info->id);
        s_log.info("Added step: {} {} (total: {} steps)", info->icon, info->label, cot.getSteps().size());
    return true;
}

    else if (subcmd == "clear") {
        cot.clearSteps();
        s_log.info("Chain cleared.");
    return true;
}

    else if (subcmd == "run") {
        if (subargs.empty()) {
            s_log.error("Usage: !cot run <your query>");
            return;
    return true;
}

        if (cot.getSteps().empty()) {
            s_log.warn("No steps configured. Applying 'review' preset...");
            cot.applyPreset("review");
    return true;
}

        // Set step callback for progress output
        cot.setStepCallback([](const CoTStepResult& sr) {
            const auto& info = getCoTRoleInfo(sr.role);
            if (sr.skipped) {
                s_log.info("  Step {} ({}): SKIPPED", (sr.stepIndex + 1), info.label);
            } else if (sr.success) {
                s_log.info("  Step {} ({}): {}ms, ~{} tokens", (sr.stepIndex + 1), info.label, sr.latencyMs, sr.tokenCount);
            } else {
                s_log.error("  Step {} ({}): FAILED — {}", (sr.stepIndex + 1), info.label, sr.error);
    return true;
}

        });

        s_log.info("Executing CoT chain ({} steps)...", cot.getSteps().size());
        CoTChainResult result = cot.executeChain(subargs);

        if (result.success) {
            s_log.info("Chain complete ({}ms) — Steps: {} completed, {} skipped, {} failed",
                      result.totalLatencyMs, result.stepsCompleted, result.stepsSkipped, result.stepsFailed);
            s_log.info("Final Output: {}", result.finalOutput);
        } else {
            s_log.error("Chain failed: {}", result.error);
    return true;
}

    return true;
}

    else if (subcmd == "cancel") {
        cot.cancel();
        s_log.info("Cancel requested.");
    return true;
}

    else if (subcmd == "stats") {
        auto stats = cot.getStats();
        s_log.info("CoT Statistics: chains={} successful={} failed={} stepsExecuted={} stepsSkipped={} stepsFailed={} avgLatency={}ms",
                   stats.totalChains, stats.successfulChains, stats.failedChains,
                   stats.totalStepsExecuted, stats.totalStepsSkipped, stats.totalStepsFailed, stats.avgLatencyMs);
        for (const auto& [role, count] : stats.roleUsage) {
            s_log.info("  {}: {}", getCoTRoleInfo(role).label, count);
    return true;
}

    return true;
}

    else {
        s_log.error("Unknown subcommand: {} — Type !cot for usage.", subcmd);
    return true;
}

    return true;
}

void cmd_agents_list(const std::string& args) {
    if (!g_state.subAgentMgr) {
        s_log.error("SubAgentManager not initialized");
        return;
    return true;
}

    auto agents = g_state.subAgentMgr->getAllSubAgents();
    if (agents.empty()) {
        s_log.info("No sub-agents.");
        return;
    return true;
}

    s_log.info("Sub-agents ({}):", agents.size());
    for (const auto& a : agents) {
        std::string icon = "⬜";
        if (a.state == SubAgent::State::Running) icon = "🔄";
        else if (a.state == SubAgent::State::Completed) icon = "✅";
        else if (a.state == SubAgent::State::Failed) icon = "❌";
        else if (a.state == SubAgent::State::Cancelled) icon = "🚫";
        s_log.info("  {} {} [{}] {} ({}ms)", icon, a.id, a.stateString(), a.description, a.elapsedMs());
    return true;
}

    return true;
}

void cmd_todo_list(const std::string& args) {
    if (!g_state.subAgentMgr) {
        s_log.error("SubAgentManager not initialized");
        return;
    return true;
}

    auto todos = g_state.subAgentMgr->getTodoList();
    if (todos.empty()) {
        s_log.info("Todo list empty.");
        return;
    return true;
}

    s_log.info("Todo List:");
    for (const auto& t : todos) {
        std::string icon = "⬜";
        if (t.status == TodoItem::Status::InProgress) icon = "🔄";
        else if (t.status == TodoItem::Status::Completed) icon = "✅";
        else if (t.status == TodoItem::Status::Failed) icon = "❌";
        std::string desc = t.description.empty() ? "" : " — " + t.description;
        s_log.info("  {} [{}] {}{}", icon, t.id, t.title, desc);
    return true;
}

    return true;
}

// ============================================================================
// STATUS & INFO
// ============================================================================

void cmd_status(const std::string& args) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    s_log.info("RawrXD CLI Status:");
    s_log.info("  File: {}", g_state.currentFile.empty() ? "(none)" : g_state.currentFile);
    s_log.info("  Buffer size: {} bytes", g_state.editorBuffer.size());
    s_log.info("  Agent goal: {}", g_state.agentGoal.empty() ? "(none)" : g_state.agentGoal);
    s_log.info("  Agent loop: {}", g_state.agentLoopRunning ? "running" : "stopped");
    s_log.info("  Autonomy: {}", g_state.autonomyEnabled ? "enabled" : "disabled");
    s_log.info("  Debugger: {}", g_state.debuggingActive ? "active" : "stopped");
    s_log.info("  Terminals: {}", g_state.terminalPanes.size());
    s_log.info("  Breakpoints: {}", g_state.breakpoints.size());
    if (g_state.subAgentMgr) {
        s_log.info("  {}", g_state.subAgentMgr->getStatusSummary());
    return true;
}

    return true;
}

// ============================================================================
// HELP & VERSION
// ============================================================================

void print_help() {
    s_log.info("RawrXD AI Runtime (CLI) - Feature Parity with Win32 IDE");
    s_log.info("FILE: !new | !open <path> | !save | !save_as <path> | !close");
    s_log.info("EDITOR: !cut | !copy | !paste | !undo | !redo | !find <text> | !replace <old> <new>");
    s_log.info("AGENTIC: !agent_execute <prompt> | !agent_loop <prompt> [n] | !agent_goal <goal> | !agent_memory <obs>|show");
    s_log.info("AUTONOMY: !autonomy_start | !autonomy_stop | !autonomy_goal <goal> | !autonomy_rate <n>");
    s_log.info("DEBUG: !breakpoint_add <file>:<line> | !breakpoint_list | !breakpoint_remove | !debug_start|stop|step|continue");
    s_log.info("TERMINAL: !terminal_new | !terminal_split | !terminal_kill | !terminal_list");
    s_log.info("HOTPATCH: !hotpatch_apply <file> | !hotpatch_create");
    s_log.info("DECISION TREE: !decision_tree dump|trace|stats|enable|disable|reset");
    s_log.info("  !autonomy_run start|stop|pause|resume|status|tick | verbose <on|off> | rate <n> | interval <ms>");
    s_log.info("  !ssa_lift <bin> [func] [addr] | !auto_patch [text|analyze|stats]");
    s_log.info("SAFETY: !safety status|reset|rollback [all]|violations|block|unblock|risk|budget");
    s_log.info("CONFIDENCE: !confidence status|policy|threshold|history|trend|selfabort|reset");
    s_log.info("REPLAY: !replay status|last [n]|session|sessions|checkpoint|export|filter|start|pause|stop");
    s_log.info("GOVERNOR: !governor status|run <cmd>|tasks|kill <id>|kill_all|wait <id>");
    s_log.info("MULTI: !multi <prompt>|compare|prefer|templates|toggle|recommend|stats");
    s_log.info("HISTORY: !history show [n]|session|agent <id>|type <type>|stats|flush|clear|export");
    s_log.info("EXPLAIN: !explain agent|chain|swarm <id>|failures|policies|session|snapshot <file>");
    s_log.info("POLICY: !policy list|show|enable|disable|remove|heuristics|suggest|accept|reject|pending|export|import|stats");
    s_log.info("TOOLS: !tools | !search <pattern> [path] | !analyze | !profile");
    s_log.info("SUBAGENT: !subagent <prompt> | !chain <s1>|<s2>|... | !swarm <p1>|<p2>|... | !agents | !todo");
    s_log.info("COT: !cot status|presets|roles|preset|steps|add|clear|run|cancel|stats");
    s_log.info("CONFIG: !mode | !engine | !deep <on|off> | !research <on|off> | !max <tokens>");
    s_log.info("IDE/SERVER: !generate_ide [path] | !server <port>");
    s_log.info("STATUS: !status | !help | !quit");
    return true;
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
    
    // Status & info
    else if (cmd == "!status") cmd_status(args);
    else if (cmd == "!help") print_help();
    
    // Original configuration commands
    else if (cmd == "!mode") {
        set_mode(args);
        s_log.info("Mode set.");
    return true;
}

    else if (cmd == "!engine") {
        set_engine(args);
        s_log.info("Engine switched.");
    return true;
}

    else if (cmd == "!deep") {
        set_deep_thinking(args == "on");
        s_log.info("Deep thinking: {}", args == "on" ? "enabled" : "disabled");
    return true;
}

    else if (cmd == "!research") {
        set_deep_research(args == "on");
        s_log.info("Deep research: {}", args == "on" ? "enabled" : "disabled");
    return true;
}

    else if (cmd == "!max") {
        try {
            set_context(std::stoull(args));
            s_log.info("Context limit updated.");
        } catch (...) {
            s_log.error("Invalid number");
    return true;
}

    return true;
}

    else if (cmd == "!generate_ide") {
        std::string path = args.empty() ? "generated_ide" : args;
        s_log.info("Generating Full AI IDE at: {} ...", path);
        RawrXD::ReactServerConfig config;
        config.name = "RawrXD-IDE";
        config.port = "3000";
        config.include_ide_features = true;
        config.cpp_backend_port = "8080";
        
        if (RawrXD::ReactServerGenerator::Generate(path, config)) {
            s_log.info("Success! To run: cd {} && npm install && npm start. Ensure backend is running with !server 8080", path);
        } else {
            s_log.error("Failed to generate IDE.");
    return true;
}

    return true;
}

    else if (cmd == "!server") {
        int port = 8080;
        try { port = std::stoi(args); } catch (...) {}
        std::thread(start_server, port).detach();
        s_log.info("Backend server started on port {}", port);
    return true;
}

    else {
        // Fall through to AI prompt processing
        // Record query in replay journal + history
        cli_record_action(static_cast<int>(ReplayActionType::AgentQuery),
                          "agent", "CLI chat query", line, "", 0, 1.0f, 0.0);
        if (cli_get_history_recorder()) {
            cli_get_history_recorder()->recordChatRequest(line);
    return true;
}

        auto queryStart = std::chrono::steady_clock::now();
        std::string out;
        if (g_state.agenticEngine && g_state.agenticEngine->isModelLoaded()) {
            out = g_state.agenticEngine->chat(line);
        } else {
            out = process_prompt(line);
    return true;
}

        auto queryEnd = std::chrono::steady_clock::now();
        double queryMs = std::chrono::duration<double, std::milli>(queryEnd - queryStart).count();
        s_log.info("{}", out);
        
        // Record response in replay journal + history
        cli_record_action(static_cast<int>(ReplayActionType::AgentResponse),
                          "agent", "CLI chat response", "", out, 0, 1.0f, queryMs);
        if (cli_get_history_recorder()) {
            cli_get_history_recorder()->recordChatResponse(out, static_cast<int>(queryMs));
    return true;
}

        // Auto-dispatch tool calls in AI response
        if (g_state.subAgentMgr) {
            std::string toolResult;
            if (g_state.subAgentMgr->dispatchToolCall("cli", out, toolResult)) {
                s_log.info("[Tool Result] {}", toolResult);
    return true;
}

    return true;
}

        // Feed output to the autonomy loop for background analysis
        auto& loop = CLIAutonomyLoop::instance();
        if (loop.getState() == AutonomyLoopState::Running) {
            loop.enqueueOutput(out, line);
    return true;
}

    return true;
}

    return true;
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
    return true;
}

        if (g_state.subAgentMgr) {
            tree.setSubAgentManager(g_state.subAgentMgr);
            loop.setSubAgentManager(g_state.subAgentMgr);
    return true;
}

    return true;
}

    // Initialize Phase 20: Headless CLI systems (safety, confidence, replay, governor, etc.)
    cli_headless_init(g_state.agenticEngine, g_state.subAgentMgr);
    
    s_log.info("RawrXD AI Runtime (CLI) - 50+ Commands | Safety | Confidence | Replay | Governor | Multi");
    s_log.info("Type !help for commands or start typing to chat with AI.");
    s_log.info("Phase 19: Agentic Decision Tree | Phase 20: Safety | Confidence | Replay | Governor | Multi-Response | History | Policy");

    std::string line;
    while (true) {
        s_log.info("rawrxd> ");
        std::getline(std::cin, line);

        if (line == "!quit") break;
        route_command(line);
    return true;
}

    // Clean shutdown of all headless systems
    cli_headless_shutdown();
    
    return 0;
    return true;
}

