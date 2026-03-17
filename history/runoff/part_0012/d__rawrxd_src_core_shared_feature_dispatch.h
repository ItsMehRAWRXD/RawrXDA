// ============================================================================
// shared_feature_dispatch.h — Unified Feature Dispatch for CLI + Win32 GUI
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Both CLI (cli_shell.cpp) and Win32 GUI (Win32IDE) route through this.
// MASM64 hot-paths call these via extern "C" bridge.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_SHARED_FEATURE_DISPATCH_H
#define RAWRXD_SHARED_FEATURE_DISPATCH_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>

// ============================================================================
// FEATURE CATEGORIES — Shared between CLI & GUI
// ============================================================================

enum class FeatureGroup : uint32_t {
    FileOps          = 0x1000,
    Editing          = 0x2000,
    View             = 0x3000,
    Terminal         = 0x4000,
    Agent            = 0x4100,
    Autonomy         = 0x4200,
    SubAgent         = 0x4300,
    Debug            = 0x4400,
    Hotpatch         = 0x4500,
    ReverseEng       = 0x4600,
    AIMode           = 0x4700,
    LLMRouter        = 0x4800,
    Swarm            = 0x4900,
    Voice            = 0x4A00,
    Tools            = 0x5000,
    Modules          = 0x6000,
    Help             = 0x7000,
    Git              = 0x8000,
    Server           = 0x9000,
    Security         = 0x9100,
    Performance      = 0x9200,
    Compiler         = 0x9300,
    Settings         = 0x9400,
    Themes           = 0x9500,
    LSP              = 0x9600,
    GhostText        = 0x9700,
    Decompiler       = 0x9800,
    Session          = 0x9900,
    Streaming        = 0x9A00,
    Annotations      = 0x9B00
};

// ============================================================================
// COMMAND RESULT — Replacing PatchResult pattern for commands
// ============================================================================

struct CommandResult {
    bool        success;
    const char* detail;
    int         errorCode;
    
    static CommandResult ok(const char* msg = "OK") { return {true, msg, 0}; }
    static CommandResult error(const char* msg, int code = -1) { return {false, msg, code}; }
};

// ============================================================================
// COMMAND CONTEXT — Passed to every feature handler
// ============================================================================

struct CommandContext {
    const char*   rawInput;      // Original user input (CLI) or "" (GUI)
    const char*   args;          // Arguments after command name
    void*         idePtr;        // Win32IDE* if GUI, nullptr if CLI
    void*         cliStatePtr;   // CLIState* if CLI, nullptr if GUI
    uint32_t      commandId;     // IDM_* for GUI dispatch, 0 for CLI
    bool          isGui;         // true = Win32 GUI, false = CLI
    bool          isHeadless;    // true = headless/test mode
    
    // Output callback — CLI prints to stdout, GUI shows in status/panel
    void (*outputFn)(const char* text, void* userData);
    void* outputUserData;
    
    void output(const char* text) const {
        if (outputFn) outputFn(text, outputUserData);
    }
    
    void outputLine(const std::string& text) const {
        if (outputFn) {
            std::string line = text + "\n";
            outputFn(line.c_str(), outputUserData);
        }
    }
};

// ============================================================================
// FEATURE HANDLER — Function pointer type for all features
// ============================================================================

using FeatureHandler = CommandResult(*)(const CommandContext& ctx);

// ============================================================================
// FEATURE DESCRIPTOR — Registration entry
// ============================================================================

struct FeatureDescriptor {
    const char*     id;           // "file.new", "agent.loop", etc.
    const char*     name;         // "New File"
    const char*     description;  // "Create a new empty file"
    FeatureGroup    group;
    uint32_t        commandId;    // Win32 IDM_* (for GUI routing)
    const char*     cliCommand;   // CLI command string ("!new", "/new")
    const char*     shortcut;     // "Ctrl+N"
    FeatureHandler  handler;      // Actual implementation
    bool            guiSupported; // Available in Win32 GUI
    bool            cliSupported; // Available in CLI
    bool            asmHotPath;   // Has x64 MASM fast-path
};

// ============================================================================
// SHARED FEATURE REGISTRY — Singleton, thread-safe
// ============================================================================

class SharedFeatureRegistry {
public:
    static SharedFeatureRegistry& instance() {
        static SharedFeatureRegistry s_instance;
        return s_instance;
    }

    // ── Registration ───────────────────────────────────────────────────────
    
    bool registerFeature(const FeatureDescriptor& desc) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_byId.count(desc.id)) return false; // already registered
        
        size_t idx = m_features.size();
        m_features.push_back(desc);
        m_byId[desc.id] = idx;
        
        if (desc.commandId != 0) {
            m_byCommandId[desc.commandId] = idx;
        }
        if (desc.cliCommand && desc.cliCommand[0] != '\0') {
            m_byCli[desc.cliCommand] = idx;
        }
        
        m_totalRegistered.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // ── Dispatch by ID ─────────────────────────────────────────────────────
    // NOTE: Handler runs OUTSIDE lock to prevent re-entrancy deadlock.
    //       If a handler dispatches another command, the mutex is not held.
    
    CommandResult dispatch(const char* featureId, const CommandContext& ctx) {
        FeatureHandler handler = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_byId.find(featureId);
            if (it == m_byId.end()) {
                return CommandResult::error("Feature not found");
            }
            handler = m_features[it->second].handler;
            if (!handler) {
                return CommandResult::error("Feature has no handler");
            }
        }
        // Handler runs OUTSIDE lock — re-entrancy safe
        m_dispatchCount.fetch_add(1, std::memory_order_relaxed);
        return handler(ctx);
    }

    // ── Dispatch by Win32 command ID ────────────────────────────────────────
    // NOTE: Handler runs OUTSIDE lock to prevent re-entrancy deadlock.
    
    CommandResult dispatchByCommandId(uint32_t cmdId, const CommandContext& ctx) {
        FeatureHandler handler = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_byCommandId.find(cmdId);
            if (it == m_byCommandId.end()) {
                return CommandResult::error("Command ID not registered");
            }
            handler = m_features[it->second].handler;
            if (!handler) {
                return CommandResult::error("Feature has no handler");
            }
        }
        // Handler runs OUTSIDE lock — re-entrancy safe
        m_dispatchCount.fetch_add(1, std::memory_order_relaxed);
        return handler(ctx);
    }

    // ── Dispatch by CLI command string ──────────────────────────────────────
    // NOTE: Handler runs OUTSIDE lock to prevent re-entrancy deadlock.
    
    CommandResult dispatchByCli(const char* cliCmd, const CommandContext& ctx) {
        FeatureHandler handler = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_byCli.find(cliCmd);
            if (it == m_byCli.end()) {
                return CommandResult::error("CLI command not registered");
            }
            handler = m_features[it->second].handler;
            if (!handler) {
                return CommandResult::error("Feature has no handler");
            }
        }
        // Handler runs OUTSIDE lock — re-entrancy safe
        m_dispatchCount.fetch_add(1, std::memory_order_relaxed);
        return handler(ctx);
    }

    // ── Query ───────────────────────────────────────────────────────────────
    
    const FeatureDescriptor* findById(const char* id) const {
        auto it = m_byId.find(id);
        if (it == m_byId.end()) return nullptr;
        return &m_features[it->second];
    }

    const FeatureDescriptor* findByCommandId(uint32_t cmdId) const {
        auto it = m_byCommandId.find(cmdId);
        if (it == m_byCommandId.end()) return nullptr;
        return &m_features[it->second];
    }

    const FeatureDescriptor* findByCli(const char* cliCmd) const {
        auto it = m_byCli.find(cliCmd);
        if (it == m_byCli.end()) return nullptr;
        return &m_features[it->second];
    }

    std::vector<const FeatureDescriptor*> getByGroup(FeatureGroup group) const {
        std::vector<const FeatureDescriptor*> result;
        uint32_t base = static_cast<uint32_t>(group);
        for (const auto& f : m_features) {
            if (static_cast<uint32_t>(f.group) == base) {
                result.push_back(&f);
            }
        }
        return result;
    }

    std::vector<const FeatureDescriptor*> getCliFeatures() const {
        std::vector<const FeatureDescriptor*> result;
        for (const auto& f : m_features) {
            if (f.cliSupported) result.push_back(&f);
        }
        return result;
    }

    std::vector<const FeatureDescriptor*> getGuiFeatures() const {
        std::vector<const FeatureDescriptor*> result;
        for (const auto& f : m_features) {
            if (f.guiSupported) result.push_back(&f);
        }
        return result;
    }

    const std::vector<FeatureDescriptor>& allFeatures() const { 
        return m_features; 
    }

    size_t totalRegistered() const { 
        return m_totalRegistered.load(std::memory_order_relaxed); 
    }
    
    uint64_t totalDispatched() const { 
        return m_dispatchCount.load(std::memory_order_relaxed); 
    }

    // ── Manifest Generation (for cross-IDE alignment) ───────────────────────
    
    std::string generateManifestJSON() const {
        std::string json = "{\n  \"features\": [\n";
        for (size_t i = 0; i < m_features.size(); ++i) {
            const auto& f = m_features[i];
            json += "    {\n";
            json += "      \"id\": \"" + std::string(f.id) + "\",\n";
            json += "      \"name\": \"" + std::string(f.name) + "\",\n";
            json += "      \"description\": \"" + std::string(f.description) + "\",\n";
            json += "      \"commandId\": " + std::to_string(f.commandId) + ",\n";
            json += "      \"cliCommand\": \"" + std::string(f.cliCommand ? f.cliCommand : "") + "\",\n";
            json += "      \"shortcut\": \"" + std::string(f.shortcut ? f.shortcut : "") + "\",\n";
            json += "      \"guiSupported\": " + std::string(f.guiSupported ? "true" : "false") + ",\n";
            json += "      \"cliSupported\": " + std::string(f.cliSupported ? "true" : "false") + ",\n";
            json += "      \"asmHotPath\": " + std::string(f.asmHotPath ? "true" : "false") + "\n";
            json += "    }";
            if (i + 1 < m_features.size()) json += ",";
            json += "\n";
        }
        json += "  ],\n";
        json += "  \"totalRegistered\": " + std::to_string(m_totalRegistered.load()) + ",\n";
        json += "  \"totalDispatched\": " + std::to_string(m_dispatchCount.load()) + "\n";
        json += "}\n";
        return json;
    }

    std::string generateManifestMarkdown() const {
        std::string md;
        md += "# RawrXD Feature Manifest\n\n";
        md += "| ID | Name | CLI | GUI | MASM | Shortcut |\n";
        md += "|---|---|---|---|---|---|\n";
        for (const auto& f : m_features) {
            md += "| " + std::string(f.id);
            md += " | " + std::string(f.name);
            md += " | " + std::string(f.cliSupported ? "YES" : "-");
            md += " | " + std::string(f.guiSupported ? "YES" : "-");
            md += " | " + std::string(f.asmHotPath ? "ASM" : "-");
            md += " | " + std::string(f.shortcut ? f.shortcut : "");
            md += " |\n";
        }
        md += "\nTotal: " + std::to_string(m_features.size()) + " features\n";
        return md;
    }

private:
    SharedFeatureRegistry() = default;
    SharedFeatureRegistry(const SharedFeatureRegistry&) = delete;
    SharedFeatureRegistry& operator=(const SharedFeatureRegistry&) = delete;

    std::vector<FeatureDescriptor>                          m_features;
    std::unordered_map<std::string, size_t>                 m_byId;
    std::unordered_map<uint32_t, size_t>                    m_byCommandId;
    std::unordered_map<std::string, size_t>                 m_byCli;
    mutable std::mutex                                      m_mutex;
    std::atomic<size_t>                                     m_totalRegistered{0};
    std::atomic<uint64_t>                                   m_dispatchCount{0};
};

// ============================================================================
// EXTERN "C" BRIDGE — For x64 MASM hot-paths to call into dispatch
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// MASM can call these to dispatch features via the registry
int rawrxd_dispatch_feature(const char* featureId, const char* args, void* idePtr);
int rawrxd_dispatch_command(uint32_t commandId, void* idePtr);
int rawrxd_dispatch_cli(const char* cliCommand, const char* args, void* cliStatePtr);
int rawrxd_get_feature_count(void);

#ifdef __cplusplus
}
#endif

// ============================================================================
// REGISTRATION MACRO — Simplifies static feature registration
// ============================================================================

#define REGISTER_FEATURE(id, name, desc, group, cmdId, cliCmd, shortcut, handler, gui, cli, asm) \
    static bool s_reg_##__COUNTER__ = []() { \
        FeatureDescriptor fd{}; \
        fd.id = id; fd.name = name; fd.description = desc; \
        fd.group = group; fd.commandId = cmdId; \
        fd.cliCommand = cliCmd; fd.shortcut = shortcut; \
        fd.handler = handler; fd.guiSupported = gui; \
        fd.cliSupported = cli; fd.asmHotPath = asm; \
        SharedFeatureRegistry::instance().registerFeature(fd); \
        return true; \
    }()

#endif // RAWRXD_SHARED_FEATURE_DISPATCH_H
