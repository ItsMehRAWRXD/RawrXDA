#include "scoped_instructions_provider.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace RawrXD::Core {

namespace {

std::string JoinTargets(const std::vector<std::string>& targets) {
    std::ostringstream out;
    for (size_t i = 0; i < targets.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << targets[i];
    }
    return out.str();
}

std::string BuildSegmentKey(const ScopedInstructions& scoped) {
    std::ostringstream out;
    out << static_cast<int>(scoped.primaryScope) << '\n';
    for (const auto& source : scoped.sources) {
        out << source << '\n';
    }
    out << "===\n" << scoped.content;
    return out.str();
}

void AppendUnique(std::vector<std::string>& output,
                  std::unordered_set<std::string>& seen,
                  const std::vector<std::string>& values) {
    for (const auto& value : values) {
        if (!value.empty() && seen.insert(value).second) {
            output.push_back(value);
        }
    }
}

} // namespace

ScopedInstructionsProvider& ScopedInstructionsProvider::instance() {
    static ScopedInstructionsProvider inst;
    return inst;
}

void ScopedInstructionsProvider::setProjectRoot(const std::string& root) {
    projectRoot = root;
    scopeCache.clear();
}

std::string ScopedInstructionsProvider::loadInstructionFile(const std::string& path) {
    auto it = scopeCache.find(path);
    if (it != scopeCache.end()) {
        return it->second;
    }
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        scopeCache[path] = content;
        return content;
    } catch (...) {
        return "";
    }
}

std::vector<std::string> ScopedInstructionsProvider::findInstructionFiles(
    const std::string& startPath) {
    
    std::vector<std::string> found;
    
    try {
        // Walk up from startPath to projectRoot
        fs::path current(startPath);
        fs::path root(projectRoot.empty() ? "/" : projectRoot);
        
        while (current.string().length() >= root.string().length()) {
            fs::path instructionsFile = current / ".instructions.md";
            
            if (fs::exists(instructionsFile)) {
                found.push_back(instructionsFile.string());
            }
            
            if (current == root) break;
            current = current.parent_path();
        }
    } catch (...) {
        fprintf(stderr, "[ScopedInstructionsProvider] Filesystem error during scan, skipping\n");
    }
    
    return found;
}

std::vector<std::string> ScopedInstructionsProvider::loadFileAdjacentMetadata(
    const std::string& filePath) {
    
    std::vector<std::string> metadata;
    
    try {
        // Look for .agent.md and .prompt.md adjacent to file
        fs::path file(filePath);
        fs::path dir = file.parent_path();
        std::string base = file.filename().string();
        
        fs::path agentFile = dir / (base + ".agent.md");
        fs::path promptFile = dir / (base + ".prompt.md");
        
        if (fs::exists(agentFile)) {
            std::string content = loadInstructionFile(agentFile.string());
            if (!content.empty()) {
                metadata.push_back(content);
            }
        }
        
        if (fs::exists(promptFile)) {
            std::string content = loadInstructionFile(promptFile.string());
            if (!content.empty()) {
                metadata.push_back(content);
            }
        }
    } catch (...) {
        fprintf(stderr, "[ScopedInstructionsProvider] Filesystem error loading metadata, skipping\n");
    }
    
    return metadata;
}

std::string ScopedInstructionsProvider::mergeInstructions(
    const std::vector<std::string>& sources) {
    
    if (sources.empty()) return "";
    
    std::stringstream merged;
    for (size_t i = 0; i < sources.size(); i++) {
        if (i > 0) merged << "\n\n---\n\n";
        merged << sources[i];
    }
    
    return merged.str();
}

ScopedInstructions ScopedInstructionsProvider::getForFile(
    const std::string& filePath) {
    
    ScopedInstructions result;
    
    try {
        fs::path file(filePath);
        std::string dir = file.parent_path().string();
        
        // 1. Find directory-scoped instructions (walk up directory tree)
        auto dirInstructions = findInstructionFiles(dir);
        
        // 2. Load each directory-scoped file
        std::vector<std::string> sources;
        for (const auto& dirInstr : dirInstructions) {
            std::string content = loadInstructionFile(dirInstr);
            if (!content.empty()) {
                sources.push_back(content);
            }
        }
        
        // 3. Load file-adjacent metadata (.agent.md, .prompt.md)
        auto adjacent = loadFileAdjacentMetadata(filePath);
        for (const auto& adj : adjacent) {
            sources.push_back(adj);
        }
        
        // 4. Merge all sources
        result.content = mergeInstructions(sources);
        
        // 5. Set sources for tracing
        for (const auto& dirInstr : dirInstructions) {
            result.sources.push_back(dirInstr);
        }
        if (fs::exists(file.parent_path() / (file.filename().string() + ".agent.md"))) {
            result.sources.push_back(filePath + ".agent.md");
        }
        if (fs::exists(file.parent_path() / (file.filename().string() + ".prompt.md"))) {
            result.sources.push_back(filePath + ".prompt.md");
        }
        
        // Determine primary scope
        result.primaryScope = dirInstructions.empty() ? 
            InstructionScope::FILE : InstructionScope::DIRECTORY;
        
    } catch (...) {
        fprintf(stderr, "[ScopedInstructionsProvider] Error resolving file instructions, returning empty\n");
    }
    
    return result;
}

ScopedInstructions ScopedInstructionsProvider::getForDirectory(
    const std::string& dirPath) {
    
    ScopedInstructions result;
    
    try {
        auto files = findInstructionFiles(dirPath);
        
        std::vector<std::string> sources;
        for (const auto& file : files) {
            std::string content = loadInstructionFile(file);
            if (!content.empty()) {
                sources.push_back(content);
            }
        }
        
        result.content = mergeInstructions(sources);
        result.sources = files;
        result.primaryScope = InstructionScope::DIRECTORY;
        
    } catch (...) {
        fprintf(stderr, "[ScopedInstructionsProvider] Error resolving directory instructions, returning empty\n");
    }
    
    return result;
}

ScopedInstructions ScopedInstructionsProvider::getProjectInstructions() {
    ScopedInstructions result;
    
    if (projectRoot.empty()) {
        return result;
    }
    
    try {
        fs::path root(projectRoot);
        fs::path instructionsFile = root / ".instructions.md";
        
        if (fs::exists(instructionsFile)) {
            result.content = loadInstructionFile(instructionsFile.string());
            result.sources.push_back(instructionsFile.string());
            result.primaryScope = InstructionScope::PROJECT;
        }
    } catch (...) {
        fprintf(stderr, "[ScopedInstructionsProvider] Error loading project instructions, returning empty\n");
    }
    
    return result;
}

ResolvedScopedInstructions ScopedInstructionsProvider::resolveForTargets(
    const std::vector<std::string>& filePaths,
    size_t maxBytes) {

    ResolvedScopedInstructions result;

    std::vector<std::string> uniqueTargets;
    std::unordered_set<std::string> seenTargets;
    for (const auto& filePath : filePaths) {
        if (!filePath.empty() && seenTargets.insert(filePath).second) {
            uniqueTargets.push_back(filePath);
        }
    }
    result.targets = uniqueTargets;

    struct Segment {
        ScopedInstructions scoped;
        std::vector<std::string> targets;
    };

    std::vector<Segment> segments;
    std::unordered_map<std::string, size_t> segmentIndex;
    std::unordered_set<std::string> seenSources;

    for (const auto& target : uniqueTargets) {
        ScopedInstructions scoped = getForFile(target);
        if (scoped.empty()) {
            continue;
        }

        const std::string key = BuildSegmentKey(scoped);
        auto existing = segmentIndex.find(key);
        if (existing == segmentIndex.end()) {
            segmentIndex.emplace(key, segments.size());
            segments.push_back(Segment{scoped, {target}});
        } else {
            segments[existing->second].targets.push_back(target);
        }

        AppendUnique(result.sources, seenSources, scoped.sources);
    }

    if (segments.empty()) {
        ScopedInstructions projectScoped = getProjectInstructions();
        if (!projectScoped.empty()) {
            result.usedProjectFallback = true;
            segments.push_back(Segment{projectScoped, {}});
            AppendUnique(result.sources, seenSources, projectScoped.sources);
        }
    }

    if (segments.empty()) {
        return result;
    }

    std::ostringstream payload;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            payload << "\n\n---\n\n";
        }

        const Segment& segment = segments[i];
        if (segment.targets.empty()) {
            payload << "## Project Scoped Instructions\n";
        } else {
            payload << "## Scoped Instructions for " << JoinTargets(segment.targets) << "\n";
        }
        payload << segment.scoped.content;
    }

    result.promptPayload = payload.str();
    result.originalBytes = result.promptPayload.size();
    if (maxBytes > 0 && result.promptPayload.size() > maxBytes) {
        result.truncated = true;
        if (maxBytes > 36) {
            result.promptPayload.resize(maxBytes - 36);
            result.promptPayload += "\n... [scoped instructions truncated]";
        } else {
            result.promptPayload.resize(maxBytes);
        }
    }

    return result;
}

std::string ScopedInstructionsProvider::formatTelemetry(
    const ResolvedScopedInstructions& resolved) {

    if (resolved.empty()) {
        return "";
    }

    std::ostringstream out;
    out << "Scoped instruction trace: " << resolved.sources.size() << " source(s)";
    if (!resolved.targets.empty()) {
        out << ", " << resolved.targets.size() << " target file(s)";
    }
    out << ", resolution="
        << (resolved.usedProjectFallback ? "project-fallback" : "target-scoped");
    if (resolved.truncated) {
        out << ", truncated from " << resolved.originalBytes << " bytes";
    }

    return out.str();
}

InstructionsResult ScopedInstructionsProvider::loadAll() {
    if (projectRoot.empty()) {
        return InstructionsResult::error("Project root not set", -1);
    }
    
    try {
        // Walk entire project tree and load all .instructions.md files
        for (const auto& entry : fs::recursive_directory_iterator(projectRoot)) {
            if (entry.path().filename() == ".instructions.md") {
                loadInstructionFile(entry.path().string());
            }
        }
        
        return InstructionsResult::ok("All instructions loaded");
    } catch (...) {
        return InstructionsResult::error("Failed to load instructions", -1);
    }
}

InstructionsResult ScopedInstructionsProvider::reload() {
    scopeCache.clear();
    return loadAll();
}

void ScopedInstructionsProvider::clear() {
    scopeCache.clear();
}

std::vector<ScopedInstructionsProvider::ScopeInfo> 
ScopedInstructionsProvider::getAllScopes() const {
    
    std::vector<ScopeInfo> scopes;
    
    for (const auto& [path, content] : scopeCache) {
        ScopeInfo info;
        info.path = path;
        info.content = content;
        info.exists = true;
        
        if (path.find(".instructions.md") != std::string::npos) {
            // Determine scope type from path
            if (path.find(projectRoot) == 0 && 
                path.find('/', projectRoot.length() + 1) == std::string::npos) {
                info.scope = InstructionScope::PROJECT;
            } else {
                info.scope = InstructionScope::DIRECTORY;
            }
        } else {
            info.scope = InstructionScope::FILE;
        }
        
        scopes.push_back(info);
    }
    
    return scopes;
}

} // namespace RawrXD::Core
