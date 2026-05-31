// ============================================================================
// RawrXD Skill Definition Injection System
// ============================================================================
// Provides Cursor-style .cursorrules / system prompt injection for sovereign IDE.
// First 520 lines of context are ALWAYS injected regardless of model/agent status.
//
// Architecture:
//   - Skill definitions stored in d:\rawrxd\.github\skills\<name>\SKILL.md
//   - Schema validated at load time (no broken skills allowed)
//   - Injection engine prepends active skill context to EVERY model/agent call
//   - UI toggle system enables/disabling per skill with persistence
//   - Hot-reload without IDE restart
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <windows.h>

namespace RawrXD {
namespace SkillSystem {

// ============================================================================
// CONSTANTS — First 520 lines guarantee
// ============================================================================
static constexpr size_t SKILL_INJECTION_LINE_COUNT = 520;
static constexpr size_t SKILL_INJECTION_MAX_BYTES = 32768;  // 32KB hard cap
static constexpr size_t SKILL_SCHEMA_VERSION = 1;
static constexpr const wchar_t* SKILL_REGISTRY_PATH = L"d:\\rawrxd\\.github\\skills";
static constexpr const wchar_t* SKILL_DEFINITION_FILENAME = L"SKILL.md";
static constexpr const wchar_t* SKILL_TOGGLE_REGISTRY_KEY = L"Software\\RawrXD\\SkillSystem";

// ============================================================================
// SKILL DEFINITION SCHEMA (JSON-compatible structure)
// ============================================================================
struct SkillDefinition {
    std::string name;                    // Unique identifier (e.g., "expansion-coordinator")
    std::string description;            // Human-readable purpose
    std::string version;               // Semver for schema evolution
    std::string specialistAgent;        // Which agent owns this skill (e.g., "@AgentPolish")
    std::vector<std::string> tags;      // For filtering/search
    std::vector<std::string> phases;   // Which expansion phases this applies to
    
    // Injection control
    bool enabled = true;               // Default state
    bool alwaysInject = true;          // If true, injects regardless of agent status
    size_t priority = 100;            // Lower = higher priority (injected first)
    
    // Content sections (loaded from SKILL.md)
    std::string mission;              // First section: what this skill does
    std::string operatingRules;       // Rules that must be followed
    std::string qualityGates;        // Validation criteria
    std::string coordinationProtocol; // How to work with other skills
    std::string riskFramework;       // Known risks and mitigations
    std::string dailyExecutionContract; // Day-by-day expectations
    
    // Runtime state (not persisted)
    std::string loadedFromPath;
    FILETIME lastModified = {0};
    bool schemaValid = false;
    std::string schemaValidationError;
};

// ============================================================================
// INJECTION ENGINE — Core guarantee: first 520 lines ALWAYS present
// ============================================================================
class SkillInjectionEngine {
public:
    static SkillInjectionEngine& Instance();
    
    // Load all skills from registry directory
    bool LoadAllSkills();
    
    // Hot-reload a single skill (for development)
    bool ReloadSkill(const std::string& skillName);
    
    // CRITICAL: Inject skill context into ANY prompt context
    // This is the "first 520 lines" guarantee — called before EVERY model/agent invocation
    std::string InjectSkillContext(
        const std::string& originalPrompt,
        const std::string& targetAgent = "",      // e.g., "@AgentPolish", "@ExtensionHost"
        const std::string& currentPhase = ""     // e.g., "phase1", "phase2"
    );
    
    // UI Toggle API
    bool IsSkillEnabled(const std::string& skillName) const;
    void SetSkillEnabled(const std::string& skillName, bool enabled);
    void ToggleSkill(const std::string& skillName);
    
    // Get all skills for UI rendering
    std::vector<SkillDefinition> GetAllSkills() const;
    std::vector<SkillDefinition> GetActiveSkillsForAgent(const std::string& agentName) const;
    std::vector<SkillDefinition> GetActiveSkillsForPhase(const std::string& phaseName) const;
    
    // Persistence
    bool SaveToggleState();
    bool LoadToggleState();
    
    // Force refresh (file watcher callback)
    void InvalidateCache();
    
    // Metrics
    size_t GetTotalInjectionLines() const { return m_totalInjectionLines.load(); }
    size_t GetTotalInjectionBytes() const { return m_totalInjectionBytes.load(); }
    
private:
    SkillInjectionEngine() = default;
    ~SkillInjectionEngine() = default;
    
    bool ParseSkillMarkdown(const std::wstring& filePath, SkillDefinition& outSkill);
    bool ValidateSkillSchema(const SkillDefinition& skill);
    std::string ExtractSection(const std::string& markdown, const std::string& sectionName);
    void RebuildInjectionCache();
    void TrimToLineLimit(std::string& content, size_t maxLines);
    
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, SkillDefinition> m_skills;
    std::unordered_map<std::string, bool> m_toggleState;  // Persisted to registry
    
    // Cached injection content (rebuilt on skill change)
    std::string m_cachedInjection;
    std::atomic<size_t> m_totalInjectionLines{0};
    std::atomic<size_t> m_totalInjectionBytes{0};
    
    // File watcher handle
    HANDLE m_directoryWatcher = INVALID_HANDLE_VALUE;
    std::thread m_watcherThread;
    std::atomic<bool> m_watcherRunning{false};
    
    void StartFileWatcher();
    void StopFileWatcher();
    void WatcherLoop();
    
    // Singleton enforcement
    SkillInjectionEngine(const SkillInjectionEngine&) = delete;
    SkillInjectionEngine& operator=(const SkillInjectionEngine&) = delete;
};

// Forward declaration for header-only consumers
class SkillToggleUI;

// ============================================================================
// INTEGRATION HOOKS — Called from existing agentic/model systems
// ============================================================================

// Called BEFORE every model invocation (ghost text, chat, agent, etc.)
std::string PrependSkillContextToPrompt(const std::string& originalPrompt);

// Called when agent specialization is determined
std::string GetSkillContextForAgent(const std::string& agentName);

// Called for phase transition validation
bool ValidatePhaseTransitionWithSkills(const std::string& fromPhase, const std::string& toPhase);

// ============================================================================
// C-API for backward compatibility with existing MASM/ASM bridges
// ============================================================================
extern "C" {
    __declspec(dllexport) const char* __stdcall SkillSystem_InjectContext(const char* prompt);
    __declspec(dllexport) bool __stdcall SkillSystem_IsSkillEnabled(const char* skillName);
    __declspec(dllexport) bool __stdcall SkillSystem_ToggleSkill(const char* skillName);
    __declspec(dllexport) void __stdcall SkillSystem_InvalidateCache();
}

} // namespace SkillSystem
} // namespace RawrXD
