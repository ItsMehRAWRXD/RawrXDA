// ============================================================================
// workspace_reasoning_profiles.hpp — Per-Workspace Reasoning Profile Persistence
// ============================================================================
// Architecture: C++20, Win32, no exceptions, no Qt
//
// Maps workspace/repo paths to reasoning profiles so that:
//   - A "fast" profile is remembered for quick utility repos
//   - A "critical" profile is remembered for production repos
//   - Profiles are loaded automatically when a workspace opens
//   - Profiles are saved automatically on change
//
// This is a valuation accelerant: workspace-level intelligence makes
// RawrXD context-aware across projects.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include "../core/model_memory_hotpatch.hpp"

// Forward-declare to avoid circular includes
struct ReasoningProfile;

// ============================================================================
// WorkspaceProfileEntry — One workspace → profile mapping
// ============================================================================
struct WorkspaceProfileEntry {
    std::string     workspacePath;          // Absolute path to workspace root
    std::string     profileName;            // Reasoning profile preset name
    std::string     customProfileJSON;      // Serialized custom profile (if not preset)
    std::string     repoId;                 // Optional: git remote URL hash for portability
    uint64_t        lastUsedEpochMs;        // Last time this workspace was opened
    uint64_t        createdEpochMs;         // When this mapping was created
    int             totalSessions;          // How many times opened with this profile
    bool            isCustom;               // True if using a non-preset custom profile

    // Statistics from last session
    struct SessionStats {
        double      avgLatencyMs;
        double      avgQuality;
        int         requestCount;
        int         bypassCount;
        int         adaptiveAdjustments;
    } lastSessionStats{};
};

// ============================================================================
// WorkspaceProfileConfig — Global configuration for the profile manager
// ============================================================================
struct WorkspaceProfileConfig {
    std::string     persistPath;            // Where to save the workspace→profile map
    bool            autoLoadOnOpen;         // Auto-apply profile when workspace opens
    bool            autoSaveOnChange;       // Auto-save when profile changes
    bool            learnFromUsage;         // Auto-upgrade profile based on session stats
    int             maxEntries;             // Maximum workspace entries to remember
    int             pruneAfterDays;         // Remove entries older than this

    WorkspaceProfileConfig()
        : persistPath("workspace_profiles.json"),
          autoLoadOnOpen(true),
          autoSaveOnChange(true),
          learnFromUsage(true),
          maxEntries(256),
          pruneAfterDays(90) {}
};

// ============================================================================
// WorkspaceReasoningProfileManager — Singleton
// ============================================================================
class WorkspaceReasoningProfileManager {
public:
    static WorkspaceReasoningProfileManager& instance();

    // ---- Configuration ----
    void setConfig(const WorkspaceProfileConfig& config);
    WorkspaceProfileConfig getConfig() const;

    // ---- Workspace Profile Operations ----

    /// Set the reasoning profile for a workspace
    PatchResult setWorkspaceProfile(const std::string& workspacePath,
                                     const std::string& profileName);

    /// Set a custom (non-preset) profile for a workspace
    PatchResult setWorkspaceCustomProfile(const std::string& workspacePath,
                                           const std::string& profileJSON);

    /// Get the profile name for a workspace (returns empty if not set)
    std::string getWorkspaceProfileName(const std::string& workspacePath) const;

    /// Get the full entry for a workspace
    bool getWorkspaceEntry(const std::string& workspacePath,
                           WorkspaceProfileEntry& out) const;

    /// Remove a workspace mapping
    PatchResult removeWorkspaceProfile(const std::string& workspacePath);

    /// Auto-detect best profile for a workspace (heuristic-based)
    std::string suggestProfile(const std::string& workspacePath) const;

    /// Called when a workspace is opened — loads and applies the profile
    PatchResult onWorkspaceOpened(const std::string& workspacePath);

    /// Called when a workspace is closed — saves session stats
    PatchResult onWorkspaceClosed(const std::string& workspacePath,
                                   const WorkspaceProfileEntry::SessionStats& stats);

    // ---- Persistence ----
    PatchResult saveToFile() const;
    PatchResult saveToFile(const std::string& path) const;
    PatchResult loadFromFile();
    PatchResult loadFromFile(const std::string& path);

    // ---- Query ----
    std::vector<WorkspaceProfileEntry> getAllEntries() const;
    std::vector<std::string> getRecentWorkspaces(int count) const;

    // ---- Maintenance ----
    void pruneOldEntries();
    void clearAll();
    int entryCount() const;

private:
    WorkspaceReasoningProfileManager();
    ~WorkspaceReasoningProfileManager() = default;
    WorkspaceReasoningProfileManager(const WorkspaceReasoningProfileManager&) = delete;
    WorkspaceReasoningProfileManager& operator=(const WorkspaceReasoningProfileManager&) = delete;

    // Normalize workspace path for consistent lookup
    std::string normalizePath(const std::string& path) const;

    // Detect workspace characteristics for profile suggestion
    struct WorkspaceCharacteristics {
        bool hasCI;
        bool hasTests;
        bool isLargeCodebase;
        bool hasSecurityPolicies;
        bool isPython;
        bool isCpp;
        int  fileCount;
    };
    WorkspaceCharacteristics analyzeWorkspace(const std::string& path) const;

    mutable std::mutex m_mutex;
    WorkspaceProfileConfig m_config;
    std::unordered_map<std::string, WorkspaceProfileEntry> m_entries;
};
