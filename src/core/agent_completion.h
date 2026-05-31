/**
 * agent_completion.h
 * 
 * C++ helper for emitting structured AGENT_DONE completion contracts.
 * Provides machine-verifiable completion signaling for agent execution lifecycle.
 * 
 * Usage:
 *   AgentCompletion::emitDone("success", "2C", 7, 7, "7f9ca5ddb");
 *   AgentCompletion::emitDoneJson("success", "2C", 7, 7, "7f9ca5ddb");
 */

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace RawrXD {
namespace AgentCompletion {

class ContractEmitter {
public:
    /**
     * Emit text format AGENT_DONE block
     */
    static std::string emitDone(
        const std::string& status,
        const std::string& phase,
        int tasksCompleted,
        int tasksTotal,
        const std::string& commit,
        const std::vector<std::string>& artifacts = {},
        const std::string& nextPhase = "",
        int durationMs = 0,
        int branches = 0,
        bool artifactsVerified = false
    ) {
        std::ostringstream oss;
        
        oss << "\n[AGENT_DONE]\n";
        oss << "status=" << status << "\n";
        oss << "phase=" << phase << "\n";
        oss << "tasks_completed=" << tasksCompleted << "\n";
        oss << "tasks_total=" << tasksTotal << "\n";
        oss << "commit=" << commit << "\n";
        
        if (!artifacts.empty()) {
            oss << "artifacts=" << artifacts.size() << "\n";
        }
        
        if (!nextPhase.empty()) {
            oss << "next=" << nextPhase << "\n";
        }
        
        if (durationMs > 0) {
            oss << "duration_ms=" << durationMs << "\n";
        }
        
        if (branches > 0) {
            oss << "branches=" << branches << "\n";
        }
        
        if (artifactsVerified) {
            oss << "artifacts_verified=true\n";
        }
        
        // Add timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        oss << "timestamp=" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S.")
            << std::setfill('0') << std::setw(3) << ms.count() << "\n";
        
        return oss.str();
    }
    
    /**
     * Emit JSON format AGENT_DONE block
     */
    static std::string emitDoneJson(
        const std::string& status,
        const std::string& phase,
        int tasksCompleted,
        int tasksTotal,
        const std::string& commit,
        const std::vector<std::string>& artifacts = {},
        const std::string& nextPhase = "",
        int durationMs = 0,
        int branches = 0,
        bool artifactsVerified = false
    ) {
        std::ostringstream oss;
        
        oss << "{\n";
        oss << "  \"agent_done\": true,\n";
        oss << "  \"status\": \"" << status << "\",\n";
        oss << "  \"phase\": \"" << phase << "\",\n";
        oss << "  \"tasks_completed\": " << tasksCompleted << ",\n";
        oss << "  \"tasks_total\": " << tasksTotal << ",\n";
        oss << "  \"commit\": \"" << commit << "\",\n";
        
        // Add timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        oss << "  \"timestamp\": \"" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S.")
            << std::setfill('0') << std::setw(3) << ms.count() << "\",\n";
        
        if (!artifacts.empty()) {
            oss << "  \"artifacts\": [\n";
            for (size_t i = 0; i < artifacts.size(); ++i) {
                oss << "    \"" << artifacts[i] << "\"";
                if (i < artifacts.size() - 1) {
                    oss << ",";
                }
                oss << "\n";
            }
            oss << "  ],\n";
        }
        
        if (!nextPhase.empty()) {
            oss << "  \"next\": \"" << nextPhase << "\",\n";
        }
        
        if (durationMs > 0) {
            oss << "  \"duration_ms\": " << durationMs << ",\n";
        }
        
        if (branches > 0) {
            oss << "  \"branches\": " << branches << ",\n";
        }
        
        if (artifactsVerified) {
            oss << "  \"artifacts_verified\": true,\n";
        }
        
        // Remove trailing comma if present
        std::string json = oss.str();
        if (json.rfind(",\n") == json.length() - 2) {
            json = json.substr(0, json.length() - 2) + "\n";
        }
        
        json += "}\n";
        return json;
    }
    
    /**
     * Emit both text and JSON formats
     */
    static std::string emitDoneBoth(
        const std::string& status,
        const std::string& phase,
        int tasksCompleted,
        int tasksTotal,
        const std::string& commit,
        const std::vector<std::string>& artifacts = {},
        const std::string& nextPhase = "",
        int durationMs = 0,
        int branches = 0,
        bool artifactsVerified = false
    ) {
        std::string text = emitDone(status, phase, tasksCompleted, tasksTotal, commit,
                                  artifacts, nextPhase, durationMs, branches, artifactsVerified);
        std::string json = emitDoneJson(status, phase, tasksCompleted, tasksTotal, commit,
                                      artifacts, nextPhase, durationMs, branches, artifactsVerified);
        
        return text + json;
    }
    
    /**
     * Parse AGENT_DONE block from content
     */
    static bool parseDoneBlock(const std::string& content, std::map<std::string, std::string>& result) {
        // Look for text format
        size_t pos = content.find("[AGENT_DONE]");
        if (pos == std::string::npos) {
            return false;
        }
        
        size_t end = content.find("\n\n", pos);
        if (end == std::string::npos) {
            end = content.length();
        }
        
        std::string block = content.substr(pos, end - pos);
        std::istringstream iss(block);
        std::string line;
        
        // Skip [AGENT_DONE] line
        std::getline(iss, line);
        
        while (std::getline(iss, line)) {
            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = line.substr(0, eq_pos);
                std::string value = line.substr(eq_pos + 1);
                result[key] = value;
            }
        }
        
        return !result.empty();
    }
};

} // namespace AgentCompletion
} // namespace RawrXD