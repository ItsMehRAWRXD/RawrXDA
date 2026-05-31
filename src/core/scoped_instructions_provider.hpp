#pragma once

#include <string>
#include <vector>
#include <map>
#include "instructions_provider.hpp"

/**
 * Scope-Aware Custom Instructions Provider
 * 
 * Implements cascading instruction resolution:
 * 1. Project-level (.instructions.md in workspace root)
 * 2. Directory-level (.instructions.md in each subdirectory)
 * 3. File-level (.agent.md, .prompt.md as file-adjacent metadata)
 * 
 * Resolution order (most-specific wins):
 * For file "src/editor/main.cpp":
 *   - src/editor/.instructions.md (if exists)
 *   - src/.instructions.md (if exists)
 *   - .instructions.md (workspace root)
 * 
 * Plus file-adjacent:
 *   - src/editor/main.cpp.agent.md
 *   - src/editor/main.cpp.prompt.md
 * 
 * NO EXTERNAL DEPENDENCIES beyond what's already linked
 */

namespace RawrXD::Core {

enum class InstructionScope {
    PROJECT = 0,      // Workspace root
    DIRECTORY = 1,    // File's containing directory
    FILE = 2,         // File-adjacent (.agent.md, .prompt.md)
};

struct ScopedInstructions {
    std::string content;           // Merged instruction content
    std::vector<std::string> sources; // Which files contributed
    InstructionScope primaryScope = InstructionScope::PROJECT;
    bool empty() const { return content.empty(); }
};

struct ResolvedScopedInstructions {
    std::string promptPayload;
    std::vector<std::string> sources;
    std::vector<std::string> targets;
    bool usedProjectFallback = false;
    bool truncated = false;
    size_t originalBytes = 0;

    bool empty() const { return promptPayload.empty(); }
};

class ScopedInstructionsProvider {
public:
    /**
     * Singleton instance
     */
    static ScopedInstructionsProvider& instance();
    
    /**
     * Set the workspace/project root for scoped resolution
     */
    void setProjectRoot(const std::string& root);
    
    /**
     * Get instructions scoped to a specific file path
     * 
     * Example: getForFile("src/editor/main.cpp")
     * Will look for (in order of specificity):
     *   - .instructions.md in src/editor/
     *   - .instructions.md in src/
     *   - .instructions.md in project root
     *   - main.cpp.agent.md (file-adjacent)
     *   - main.cpp.prompt.md (file-adjacent)
     */
    ScopedInstructions getForFile(const std::string& filePath);
    
    /**
     * Get instructions scoped to a directory
     */
    ScopedInstructions getForDirectory(const std::string& dirPath);
    
    /**
     * Get project-level instructions
     */
    ScopedInstructions getProjectInstructions();

    /**
     * Resolve a single shared prompt payload for one or more target files.
     */
    ResolvedScopedInstructions resolveForTargets(
        const std::vector<std::string>& filePaths,
        size_t maxBytes = 0);

    /**
     * Format a compact trace line describing which scoped instructions applied.
     */
    static std::string formatTelemetry(const ResolvedScopedInstructions& resolved);
    
    /**
     * Load all scoped instructions from project
     */
    InstructionsResult loadAll();
    
    /**
     * Reload all instruction files (for hot-reload scenarios)
     */
    InstructionsResult reload();
    
    /**
     * Clear all cached instructions
     */
    void clear();
    
    /**
     * Get all available scopes
     */
    struct ScopeInfo {
        std::string path;
        InstructionScope scope;
        std::string content;
        bool exists;
    };
    std::vector<ScopeInfo> getAllScopes() const;

private:
    ScopedInstructionsProvider() = default;
    
    std::string projectRoot;
    std::map<std::string, std::string> scopeCache; // path -> content
    
    // Helper: Find .instructions.md files walking up directory tree
    std::vector<std::string> findInstructionFiles(const std::string& startPath);
    
    // Helper: Load and cache a single file
    std::string loadInstructionFile(const std::string& path);
    
    // Helper: Read file-adjacent .agent.md and .prompt.md
    std::vector<std::string> loadFileAdjacentMetadata(const std::string& filePath);
    
    // Helper: Merge multiple instruction sources
    static std::string mergeInstructions(const std::vector<std::string>& sources);
};

} // namespace RawrXD::Core
