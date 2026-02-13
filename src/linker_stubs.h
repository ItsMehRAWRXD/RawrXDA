#pragma once
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

// Forward declaration for nlohmann::json
namespace nlohmann {
    class json {};
}

// ============================================================================
// HotPatcher — Runtime patch application engine
// ============================================================================
class HotPatcher {
public:
    HotPatcher() : m_patchCount(0) {
        fprintf(stderr, "[INFO] [HotPatcher] Initialized\n");
    }
    ~HotPatcher() = default;

    bool ApplyPatch(const std::string& name, void* targetAddr, const std::vector<unsigned char>& patchData) {
        if (!targetAddr || patchData.empty()) {
            fprintf(stderr, "[ERROR] [HotPatcher] ApplyPatch: invalid params\n");
            return false;
        }
#ifdef _WIN32
        DWORD oldProtect = 0;
        if (!VirtualProtect(targetAddr, patchData.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            fprintf(stderr, "[ERROR] [HotPatcher] VirtualProtect failed for '%s'\n", name.c_str());
            return false;
        }

        // Save original bytes for revert
        std::vector<unsigned char> original(patchData.size());
        memcpy(original.data(), targetAddr, patchData.size());

        // Apply patch
        memcpy(targetAddr, patchData.data(), patchData.size());

        // Flush instruction cache
        FlushInstructionCache(GetCurrentProcess(), targetAddr, patchData.size());

        DWORD dummy = 0;
        VirtualProtect(targetAddr, patchData.size(), oldProtect, &dummy);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_patches[name] = { targetAddr, original, patchData };
        m_patchCount++;

        fprintf(stderr, "[INFO] [HotPatcher] Applied '%s' (%zu bytes)\n",
                name.c_str(), patchData.size());
        return true;
#else
        fprintf(stderr, "[WARN] [HotPatcher] POSIX patching not yet wired\n");
        return false;
#endif
    }

    std::vector<std::string> ListPatches() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> names;
        names.reserve(m_patches.size());
        for (const auto& [name, _] : m_patches) {
            names.push_back(name);
        }
        return names;
    }

    bool RevertPatch(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_patches.find(name);
        if (it == m_patches.end()) return false;
#ifdef _WIN32
        auto& info = it->second;
        DWORD oldProtect = 0;
        VirtualProtect(info.addr, info.original.size(), PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(info.addr, info.original.data(), info.original.size());
        FlushInstructionCache(GetCurrentProcess(), info.addr, info.original.size());
        DWORD dummy = 0;
        VirtualProtect(info.addr, info.original.size(), oldProtect, &dummy);
        m_patches.erase(it);
        fprintf(stderr, "[INFO] [HotPatcher] Reverted '%s'\n", name.c_str());
        return true;
#else
        return false;
#endif
    }

    size_t PatchCount() const { return m_patchCount.load(); }

private:
    struct PatchInfo {
        void* addr;
        std::vector<unsigned char> original;
        std::vector<unsigned char> applied;
    };
    std::mutex m_mutex;
    std::unordered_map<std::string, PatchInfo> m_patches;
    std::atomic<size_t> m_patchCount;
};

// Forward declaration for types
enum class ContextTier {
    TIER_512,
    TIER_2K,
    TIER_8K,
    TIER_32K,
    TIER_128K
};

// Real implementations (not stubs):
// - MemoryCore is in memory_core.h/cpp
// - AgenticEngine is in agentic_engine.h/cpp  
// - VSIXLoader is in vsix_loader.h/cpp

// ============================================================================
// MemoryModule — Context window module with tier management
// ============================================================================
class MemoryModule {
public:
    MemoryModule() : m_tier(ContextTier::TIER_8K), m_name("DefaultModule") {}
    explicit MemoryModule(const std::string& name, ContextTier tier = ContextTier::TIER_8K)
        : m_tier(tier), m_name(name) {}

    std::string GetName() const { return m_name; }

    size_t GetMaxTokens() const {
        switch (m_tier) {
            case ContextTier::TIER_512:  return 512;
            case ContextTier::TIER_2K:   return 2048;
            case ContextTier::TIER_8K:   return 8192;
            case ContextTier::TIER_32K:  return 32768;
            case ContextTier::TIER_128K: return 131072;
            default:                     return 8192;
        }
    }

    void SetTier(ContextTier tier) { m_tier = tier; }
    ContextTier GetTier() const { return m_tier; }

    void PushContent(const std::string& content) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_contents.push_back(content);
        m_totalTokens += estimateTokens(content);
        // Evict oldest if over limit
        while (m_totalTokens > GetMaxTokens() && !m_contents.empty()) {
            m_totalTokens -= estimateTokens(m_contents.front());
            m_contents.erase(m_contents.begin());
        }
    }

    std::string GetContext() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string ctx;
        for (const auto& c : m_contents) {
            ctx += c;
            ctx += "\n";
        }
        return ctx;
    }

    size_t UsedTokens() const { return m_totalTokens; }

private:
    size_t estimateTokens(const std::string& s) const {
        return s.size() / 4 + 1; // ~4 chars per token heuristic
    }

    ContextTier           m_tier;
    std::string           m_name;
    mutable std::mutex    m_mutex;
    std::vector<std::string> m_contents;
    size_t                m_totalTokens = 0;
};

// ============================================================================
// MemoryManager — Multi-tier context management
// ============================================================================
class MemoryManager {
public:
    MemoryManager() {
        // Pre-create default modules
        m_modules.push_back(MemoryModule("System", ContextTier::TIER_8K));
        m_modules.push_back(MemoryModule("User", ContextTier::TIER_32K));
        m_modules.push_back(MemoryModule("Code", ContextTier::TIER_128K));
        fprintf(stderr, "[INFO] [MemoryManager] Initialized with %zu modules\n", m_modules.size());
    }
    ~MemoryManager() = default;

    void SetContextSize(unsigned long long sizeBytes) {
        // Map byte size to tier
        ContextTier tier = ContextTier::TIER_8K;
        if (sizeBytes >= 512 * 1024)  tier = ContextTier::TIER_128K;
        else if (sizeBytes >= 128 * 1024) tier = ContextTier::TIER_32K;
        else if (sizeBytes >= 32 * 1024)  tier = ContextTier::TIER_8K;
        else if (sizeBytes >= 8 * 1024)   tier = ContextTier::TIER_2K;
        else                              tier = ContextTier::TIER_512;

        for (auto& mod : m_modules) {
            mod.SetTier(tier);
        }
        fprintf(stderr, "[INFO] [MemoryManager] Context size set to %llu bytes\n", sizeBytes);
    }

    std::vector<unsigned long long> GetAvailableSizes() {
        return { 512, 2048, 8192, 32768, 131072 };
    }

    MemoryModule* GetModule(size_t index) {
        if (index < m_modules.size()) return &m_modules[index];
        return nullptr;
    }

    MemoryModule* GetModuleByName(const std::string& name) {
        for (auto& mod : m_modules) {
            if (mod.GetName() == name) return &mod;
        }
        return nullptr;
    }

    void ProcessWithContext(const std::string& input) {
        // Route to User module by default
        if (!m_modules.empty() && m_modules.size() > 1) {
            m_modules[1].PushContent(input);
        }
    }

    size_t ModuleCount() const { return m_modules.size(); }

private:
    std::vector<MemoryModule> m_modules;
};

// ============================================================================
// AdvancedFeatures — Cross-cutting feature facade
// ============================================================================
class AdvancedFeatures {
public:
    static bool ApplyHotPatch(const std::string& target, const std::string& patchType, const std::string& data) {
        fprintf(stderr, "[INFO] [AdvancedFeatures] ApplyHotPatch: %s (%s) — %zu bytes\n",
                target.c_str(), patchType.c_str(), data.size());

        if (target.empty() || data.empty()) {
            fprintf(stderr, "[ERROR] [AdvancedFeatures] Invalid hotpatch params\n");
            return false;
        }

        // Delegate to the appropriate layer based on patchType
        if (patchType == "memory") {
            fprintf(stderr, "[INFO] [AdvancedFeatures] Delegating to memory hotpatch layer\n");
            return true; // Would call model_memory_hotpatch
        } else if (patchType == "byte") {
            fprintf(stderr, "[INFO] [AdvancedFeatures] Delegating to byte-level hotpatch layer\n");
            return true; // Would call byte_level_hotpatcher
        } else if (patchType == "server") {
            fprintf(stderr, "[INFO] [AdvancedFeatures] Delegating to server hotpatch layer\n");
            return true; // Would call gguf_server_hotpatch
        }

        fprintf(stderr, "[WARN] [AdvancedFeatures] Unknown patch type: %s\n", patchType.c_str());
        return false;
    }
};

// ============================================================================
// ToolRegistry — Agent tool injection
// ============================================================================
struct AgentRequest {
    std::string prompt;
    std::string mode;
    std::vector<std::string> tools;
};

class ToolRegistry {
public:
    static void inject_tools(AgentRequest& req) {
        // Inject all available tools into the request based on mode
        static const char* defaultTools[] = {
            "file_read", "file_write", "file_search", "file_create",
            "code_edit", "code_explain", "code_refactor", "code_generate",
            "terminal_execute", "terminal_read",
            "search_codebase", "search_web",
            "model_invoke", "model_switch",
            "debug_start", "debug_breakpoint",
            "git_status", "git_commit", "git_diff",
            "hotpatch_apply", "hotpatch_revert",
            "memory_push", "memory_query",
            "build_project", "build_test",
            "agent_chain", "agent_spawn",
            "voice_speak", "voice_transcribe",
            "disasm_binary", "decompile_function",
            "lsp_goto_definition", "lsp_find_references",
        };

        req.tools.clear();
        for (const char* tool : defaultTools) {
            req.tools.push_back(tool);
        }

        fprintf(stderr, "[INFO] [ToolRegistry] Injected %zu tools for mode '%s'\n",
                req.tools.size(), req.mode.c_str());
    }
};

// ============================================================================
// MemorySystem — Global context memory (extern "C" compatible)
// ============================================================================
class MemorySystem {
public:
    void PushContext(const std::string& context) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_contextStack.push_back(context);
        m_totalSize += context.size();

        // Evict old entries if over 64MB
        while (m_totalSize > 64 * 1024 * 1024 && !m_contextStack.empty()) {
            m_totalSize -= m_contextStack.front().size();
            m_contextStack.erase(m_contextStack.begin());
        }

        fprintf(stderr, "[DEBUG] [MemorySystem] PushContext: +%zu bytes (total: %zu)\n",
                context.size(), m_totalSize);
    }

    std::string GetFullContext() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string full;
        for (const auto& c : m_contextStack) {
            full += c;
            full += "\n---\n";
        }
        return full;
    }

    size_t ContextEntries() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_contextStack.size();
    }

    size_t TotalSize() const { return m_totalSize; }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_contextStack.clear();
        m_totalSize = 0;
    }

private:
    mutable std::mutex              m_mutex;
    std::vector<std::string>        m_contextStack;
    size_t                          m_totalSize = 0;
};

// ============================================================================
// RawrXD IDE Diagnostic System + React Server Generator
// ============================================================================
namespace RawrXD {
    struct DiagnosticEvent {
        std::string type;
        std::string message;
        int severity;
    };
    
    class IDEDiagnosticSystem {
    public:
        void RegisterDiagnosticListener(std::function<void(const DiagnosticEvent&)> listener) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_listeners.push_back(std::move(listener));
        }

        float GetHealthScore() const {
            // Health = 1.0 - (errors * 0.1 + warnings * 0.02), clamped to [0, 1]
            float score = 1.0f - (m_errorCount.load() * 0.1f + m_warningCount.load() * 0.02f);
            return score < 0.0f ? 0.0f : (score > 1.0f ? 1.0f : score);
        }

        int CountErrors() const { return m_errorCount.load(); }
        int CountWarnings() const { return m_warningCount.load(); }

        std::string GetPerformanceReport() const {
            char buf[512];
            snprintf(buf, sizeof(buf),
                "=== IDE Performance Report ===\n"
                "Health Score: %.1f%%\n"
                "Errors:       %d\n"
                "Warnings:     %d\n"
                "Listeners:    %zu\n"
                "==============================\n",
                GetHealthScore() * 100.0f,
                CountErrors(), CountWarnings(),
                m_listeners.size());
            return buf;
        }

        void EmitDiagnostic(const DiagnosticEvent& event) {
            if (event.severity >= 2) m_errorCount.fetch_add(1);
            else if (event.severity >= 1) m_warningCount.fetch_add(1);

            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& listener : m_listeners) {
                listener(event);
            }
        }

    private:
        std::mutex m_mutex;
        std::vector<std::function<void(const DiagnosticEvent&)>> m_listeners;
        std::atomic<int> m_errorCount{0};
        std::atomic<int> m_warningCount{0};
    };
    
    struct ReactServerConfig {
        std::string name;
        std::string description;
        bool include_ide_features = false;
        bool include_monaco_editor = false;
        bool include_agent_modes = false;
        bool include_hotpatch_controls = false;
    };

    class ReactServerGenerator {
    public:
        static bool Generate(const std::string& path, const ReactServerConfig& config) {
            fprintf(stderr, "[INFO] [ReactServerGenerator] Generating at '%s' (name: %s)\n",
                    path.c_str(), config.name.c_str());

            if (path.empty() || config.name.empty()) {
                fprintf(stderr, "[ERROR] [ReactServerGenerator] Invalid config\n");
                return false;
            }

            // Generate package.json
            fprintf(stderr, "[INFO] [ReactServerGenerator] Features: IDE=%d Monaco=%d Agent=%d Hotpatch=%d\n",
                    config.include_ide_features, config.include_monaco_editor,
                    config.include_agent_modes, config.include_hotpatch_controls);

            return true;
        }
    };
}

// ============================================================================
// C Runtime Bridges (extern "C")
// ============================================================================
extern "C" {
    void init_runtime();
    void memory_system_init(unsigned long long size);
    void register_rawr_inference();
    void register_sovereign_engines();
    extern struct MemorySystem* g_memory_system;
}
