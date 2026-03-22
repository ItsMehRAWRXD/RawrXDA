// agentic_workspace_analyzer.hpp
// Codebase analysis: structure, dependencies, impact assessment for planning

#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Agentic {

// ============================================================================
// File & Dependency Metadata
// ============================================================================

enum class FileType : uint8_t {
    Source,           // .cpp, .cc, .c
    Header,           // .h, .hpp, .hxx
    Test,             // test_*.cpp, *_test.cpp
    CMakeFile,        // CMakeLists.txt
    Configuration,    // .ini, .json, .yaml
    Build,            // .obj, .lib, .exe, .dll
    Documentation,    // .md, .txt
    Assembly,         // .asm, .s
    Unknown
};

struct SourceFile {
    std::string path;               // Relative to workspace root
    FileType type;
    size_t line_count{0};
    std::vector<std::string> includes;    // Files this includes
    std::vector<std::string> included_by; // Files that include this
    std::set<std::string> dependencies;   // All transitive deps
    bool is_critical{false};              // Core infrastructure file
    bool modifies_api{false};             // Changes public interface
    bool has_tests{false};
    std::string last_modified;
    
    SourceFile() : type(FileType::Unknown) {}
};

struct DirectoryNode {
    std::string path;
    std::vector<SourceFile> files;
    std::vector<std::shared_ptr<DirectoryNode>> subdirs;
    bool is_build_dir{false};
    bool is_test_dir{false};
    
    size_t getTotalFiles() const;
    size_t getTotalLines() const;
};

struct DependencyGraph {
    std::map<std::string, SourceFile> files;
    std::map<std::string, std::vector<std::string>> dependents;  // reverse deps
    
    // Critical files that most code depends on
    std::vector<std::string> critical_headers;
    // Isolated modules with minimal deps
    std::vector<std::string> isolated_modules;
    
    bool isCriticalFile(const std::string& path) const;
    std::vector<std::string> getImpactedFiles(const std::string& changed_file) const;
};

// ============================================================================
// Build System Analysis
// ============================================================================

enum class BuildSystem : uint8_t {
    CMake,
    Makefile,
    MSVC,
    Ninja,
    Bazel,
    Unknown
};

struct BuildInfo {
    BuildSystem system;
    std::string build_dir;
    std::string source_dir;
    bool parallel_build_supported{true};
    int recommended_jobs{4};
    
    std::vector<std::string> compile_flags;
    std::vector<std::string> link_flags;
    std::vector<std::string> include_dirs;
    
    BuildInfo() : system(BuildSystem::Unknown) {}
};

// ============================================================================
// Workspace Analysis Results
// ============================================================================

struct WorkspaceAnalysis {
    std::string workspace_root;
    std::chrono::system_clock::time_point analyzed_at;
    
    // Structure
    std::shared_ptr<DirectoryNode> root_dir;
    DependencyGraph dep_graph;
    BuildInfo build_info;
    
    // Metrics
    int total_source_files{0};
    int total_header_files{0};
    int total_test_files{0};
    size_t total_lines_of_code{0};
    
    // Change impact cache
    std::map<std::string, std::vector<std::string>> change_impact_cache;
    
    nlohmann::json toJson() const;
};

// ============================================================================
// Workspace Analyzer  
// ============================================================================

class WorkspaceAnalyzer {
public:
    explicit WorkspaceAnalyzer(const std::string& workspace_root);
    ~WorkspaceAnalyzer();
    
    // Main analysis function
    bool analyze();
    
    // Access results
    const WorkspaceAnalysis& getAnalysis() const { return m_analysis; }
    WorkspaceAnalysis& getMutableAnalysis() { return m_analysis; }
    
    // Impact assessment
    std::vector<std::string> getFilesAffectedBy(const std::string& changed_file) const;
    std::vector<std::string> getFilesAffectingFile(const std::string& target_file) const;
    int estimateRebuildTime(const std::vector<std::string>& changed_files) const;
    
    // Build system detection
    BuildSystem detectBuildSystem() const;
    
    // Query
    FileType detectFileType(const std::string& path) const;
    bool isTestFile(const std::string& path) const;
    bool isCriticalFile(const std::string& path) const;
    
    // Refresh
    void refresh();
    
private:
    // Analysis steps
    bool scanDirectory(const std::string& dir, std::shared_ptr<DirectoryNode>& node, int depth = 0);
    bool analyzeSourceFile(const std::string& path, SourceFile& file);
    bool extractIncludes(const std::string& path, std::vector<std::string>& includes);
    void buildDependencyGraph();
    void identifyCriticalFiles();
    void analyzeWithCMake();
    
    std::string m_root;
    WorkspaceAnalysis m_analysis;
    
    // Configuration
    int m_max_depth{5};
    size_t m_max_file_size{10 * 1024 * 1024};  // 10MB
    std::set<std::string> m_ignore_patterns;
};

} // namespace Agentic
