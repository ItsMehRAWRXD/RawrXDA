/**
 * @file source_digestion_engine.hpp
 * @brief RawrXD IDE Source Digestion Engine - C++ Integration
 * 
 * This header provides a C++ interface for the source digestion system,
 * allowing the IDE to perform real-time self-auditing and health checks.
 * 
 * Features:
 * - Component health tracking
 * - Stub detection and reporting
 * - Manifest calculation
 * - Self-audit capabilities
 * - Real-time health monitoring
 * 
 * @author RawrXD IDE Team
 * @version 1.0.0
 */

#ifndef RAWRXD_SOURCE_DIGESTION_ENGINE_HPP
#define RAWRXD_SOURCE_DIGESTION_ENGINE_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <thread>
#include <future>

namespace RawrXD {
namespace Digestion {

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * @brief Severity levels for issues
 */
enum class Severity {
    Critical = 0,
    High = 1,
    Medium = 2,
    Low = 3,
    Info = 4
};

/**
 * @brief Status of a component
 */
enum class ComponentStatus {
    Complete,
    Partial,
    Stub,
    Missing,
    Unknown
};

/**
 * @brief Information about a detected stub
 */
struct StubInfo {
    int lineNumber;
    std::string content;
    std::string patternMatched;
    Severity severity;
    std::string filePath;
    
    StubInfo() : lineNumber(0), severity(Severity::Medium) {}
};

/**
 * @brief Information about a TODO/FIXME comment
 */
struct TodoInfo {
    int lineNumber;
    std::string content;
    std::string type;  // TODO, FIXME, HACK, XXX
    std::string filePath;
    
    TodoInfo() : lineNumber(0) {}
};

/**
 * @brief Analysis results for a single source file
 */
struct FileAnalysis {
    std::string path;
    std::string relativePath;
    std::string extension;
    size_t sizeBytes;
    int lineCount;
    int codeLines;
    int commentLines;
    int blankLines;
    std::vector<StubInfo> stubs;
    std::vector<TodoInfo> todos;
    int healthPoints;
    std::string category;
    bool isComplete;
    double completionPercentage;
    std::string hash;
    std::chrono::system_clock::time_point lastModified;
    
    FileAnalysis() 
        : sizeBytes(0), lineCount(0), codeLines(0), commentLines(0), blankLines(0)
        , healthPoints(100), isComplete(true), completionPercentage(100.0) {}
    
    // Calculate derived metrics
    void calculateHealth() {
        int stubPenalty = static_cast<int>(stubs.size()) * 5;
        int todoPenalty = static_cast<int>(todos.size()) * 2;
        healthPoints = std::max(0, 100 - stubPenalty - todoPenalty);
        
        completionPercentage = std::max(0.0, 100.0 - (stubs.size() * 3.0) - (todos.size() * 1.0));
        isComplete = stubs.empty() && todos.empty() && completionPercentage >= 90.0;
    }
};

/**
 * @brief Status of a major component
 */
struct ComponentInfo {
    std::string name;
    std::vector<std::string> files;
    int totalLines;
    int stubCount;
    int todoCount;
    int healthPoints;
    double completionPercentage;
    ComponentStatus status;
    std::vector<std::string> missingDependencies;
    
    ComponentInfo()
        : totalLines(0), stubCount(0), todoCount(0)
        , healthPoints(100), completionPercentage(100.0)
        , status(ComponentStatus::Unknown) {}
    
    void calculateStatus() {
        healthPoints = std::max(0, std::min(100, 100 - (stubCount * 5) - (todoCount * 2)));
        completionPercentage = std::max(0.0, 100.0 - (stubCount * 3.0) - (todoCount * 1.0));
        
        if (files.empty()) {
            status = ComponentStatus::Missing;
        } else if (stubCount == 0 && todoCount == 0) {
            status = ComponentStatus::Complete;
        } else if (stubCount < 5) {
            status = ComponentStatus::Partial;
        } else {
            status = ComponentStatus::Stub;
        }
    }
};

/**
 * @brief Complete project manifest
 */
struct ProjectManifest {
    std::string projectName;
    std::string sourceRoot;
    std::chrono::system_clock::time_point analysisTimestamp;
    
    int totalFiles;
    int totalLines;
    int totalCodeLines;
    int totalStubs;
    int totalTodos;
    int overallHealth;
    double overallCompletion;
    
    std::map<std::string, std::vector<std::string>> filesByCategory;
    std::map<std::string, ComponentInfo> components;
    std::map<std::string, FileAnalysis> fileAnalyses;
    
    std::vector<std::string> missingFeatures;
    std::vector<std::string> recommendations;
    std::vector<std::string> criticalIssues;
    
    ProjectManifest()
        : totalFiles(0), totalLines(0), totalCodeLines(0)
        , totalStubs(0), totalTodos(0), overallHealth(100)
        , overallCompletion(100.0) {}
};

// ============================================================================
// CONFIGURATION
// ============================================================================

/**
 * @brief Configuration for the digestion engine
 */
struct DigestConfig {
    std::vector<std::string> sourceExtensions = {
        ".cpp", ".c", ".h", ".hpp", ".asm", ".inc",
        ".py", ".ps1", ".bat", ".sh", ".cmake"
    };
    
    std::vector<std::string> excludePatterns = {
        "build/", "obj/", "bin/", ".git/", "CMakeFiles/",
        "autogen/", ".dir/", "Debug/", "Release/", "x64/"
    };
    
    std::vector<std::string> stubPatterns = {
        R"(//\s*TODO)",
        R"(//\s*FIXME)",
        R"(//\s*HACK)",
        R"(//\s*XXX)",
        R"(//\s*STUB)",
        R"(//\s*PLACEHOLDER)",
        R"(//\s*NOT\s+IMPLEMENTED)",
        R"(//\s*coming\s+soon)",
        R"(//\s*No-op)",
        R"(//\s*Simple\s+stub)",
        R"(//\s*Minimal\s+implementation)",
        R"(return\s+(nullptr|NULL|0|false|"")\s*;\s*//.*stub)",
        R"(\{\s*\}\s*//\s*stub)",
        R"(#\s*TODO)"
    };
    
    std::map<std::string, std::string> criticalComponents = {
        {"MainWindow", "qtapp/MainWindow.cpp"},
        {"AgenticEngine", "agentic_engine.cpp"},
        {"InferenceEngine", "qtapp/inference_engine.cpp"},
        {"ModelLoader", "auto_model_loader.cpp"},
        {"ChatInterface", "chat_interface.cpp"},
        {"TerminalManager", "qtapp/TerminalManager.cpp"},
        {"FileBrowser", "file_browser.cpp"},
        {"LSPClient", "lsp_client.cpp"},
        {"GGUFLoader", "gguf_loader.cpp"},
        {"StreamingEngine", "streaming_engine.cpp"},
        {"RefactoringEngine", "refactoring_engine.cpp"},
        {"SecurityManager", "security_manager.cpp"},
        {"TelemetrySystem", "telemetry.cpp"},
        {"ErrorHandler", "error_handler.cpp"},
        {"ConfigManager", "config_manager.cpp"}
    };
    
    std::map<std::string, std::vector<std::string>> categories = {
        {"core", {"main", "ide", "window", "app"}},
        {"ai", {"ai_", "agentic", "model_", "inference", "llm", "gguf", "streaming"}},
        {"editor", {"editor", "syntax", "highlight", "completion", "minimap"}},
        {"terminal", {"terminal", "shell", "powershell", "console"}},
        {"git", {"git_", "git/", "github"}},
        {"lsp", {"lsp_", "language_server"}},
        {"ui", {"ui_", "widget", "panel", "dialog", "menu", "toolbar"}},
        {"network", {"http", "websocket", "api_", "server"}},
        {"masm", {"masm", ".asm", "asm_"}},
        {"testing", {"test_", "test/", "testing", "benchmark"}},
        {"config", {"config", "settings", "preferences"}},
        {"security", {"security", "auth", "crypto", "jwt"}},
        {"monitoring", {"telemetry", "metrics", "monitor", "observability"}}
    };
    
    int maxWorkers = 8;
};

// ============================================================================
// MAIN ENGINE
// ============================================================================

/**
 * @brief Source Digestion Engine
 * 
 * Main class for analyzing and auditing the IDE source codebase.
 */
class SourceDigestionEngine {
public:
    using ProgressCallback = std::function<void(int current, int total, const std::string& message)>;
    
    /**
     * @brief Construct a new Source Digestion Engine
     * @param sourceRoot Path to the source directory
     * @param config Optional configuration
     */
    explicit SourceDigestionEngine(const std::string& sourceRoot, 
                                    const DigestConfig& config = DigestConfig())
        : m_sourceRoot(sourceRoot)
        , m_config(config)
        , m_isRunning(false)
    {
        compilePatterns();
    }
    
    /**
     * @brief Run the full digestion process
     * @param progressCallback Optional callback for progress updates
     * @return The completed project manifest
     */
    ProjectManifest digest(ProgressCallback progressCallback = nullptr) {
        m_isRunning = true;
        m_manifest = ProjectManifest();
        m_manifest.projectName = "RawrXD IDE";
        m_manifest.sourceRoot = m_sourceRoot;
        m_manifest.analysisTimestamp = std::chrono::system_clock::now();
        
        // Phase 1: Discover files
        if (progressCallback) progressCallback(0, 100, "Discovering files...");
        auto files = discoverFiles();
        
        // Phase 2: Analyze files
        if (progressCallback) progressCallback(10, 100, "Analyzing files...");
        analyzeFiles(files, progressCallback);
        
        // Phase 3: Analyze components
        if (progressCallback) progressCallback(70, 100, "Analyzing components...");
        analyzeComponents();
        
        // Phase 4: Calculate metrics
        if (progressCallback) progressCallback(85, 100, "Calculating metrics...");
        calculateOverallMetrics();
        
        // Phase 5: Generate recommendations
        if (progressCallback) progressCallback(95, 100, "Generating recommendations...");
        generateRecommendations();
        
        // Phase 6: Self-audit
        selfAudit();
        
        if (progressCallback) progressCallback(100, 100, "Complete!");
        m_isRunning = false;
        
        return m_manifest;
    }
    
    /**
     * @brief Get the current manifest (may be incomplete if running)
     */
    const ProjectManifest& getManifest() const { return m_manifest; }
    
    /**
     * @brief Check if digestion is currently running
     */
    bool isRunning() const { return m_isRunning; }
    
    /**
     * @brief Get overall health percentage
     */
    int getOverallHealth() const { return m_manifest.overallHealth; }
    
    /**
     * @brief Get overall completion percentage
     */
    double getOverallCompletion() const { return m_manifest.overallCompletion; }
    
    /**
     * @brief Check if a specific component is complete
     */
    bool isComponentComplete(const std::string& componentName) const {
        auto it = m_manifest.components.find(componentName);
        if (it != m_manifest.components.end()) {
            return it->second.status == ComponentStatus::Complete;
        }
        return false;
    }
    
    /**
     * @brief Get all stubs in the project
     */
    std::vector<StubInfo> getAllStubs() const {
        std::vector<StubInfo> allStubs;
        for (const auto& [path, analysis] : m_manifest.fileAnalyses) {
            for (const auto& stub : analysis.stubs) {
                StubInfo s = stub;
                s.filePath = path;
                allStubs.push_back(s);
            }
        }
        return allStubs;
    }
    
    /**
     * @brief Get all TODOs in the project
     */
    std::vector<TodoInfo> getAllTodos() const {
        std::vector<TodoInfo> allTodos;
        for (const auto& [path, analysis] : m_manifest.fileAnalyses) {
            for (const auto& todo : analysis.todos) {
                TodoInfo t = todo;
                t.filePath = path;
                allTodos.push_back(t);
            }
        }
        return allTodos;
    }
    
    /**
     * @brief Export manifest to JSON
     */
    std::string exportJson() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"projectName\": \"" << m_manifest.projectName << "\",\n";
        oss << "  \"sourceRoot\": \"" << escapeJson(m_manifest.sourceRoot) << "\",\n";
        oss << "  \"totalFiles\": " << m_manifest.totalFiles << ",\n";
        oss << "  \"totalLines\": " << m_manifest.totalLines << ",\n";
        oss << "  \"totalCodeLines\": " << m_manifest.totalCodeLines << ",\n";
        oss << "  \"totalStubs\": " << m_manifest.totalStubs << ",\n";
        oss << "  \"totalTodos\": " << m_manifest.totalTodos << ",\n";
        oss << "  \"overallHealth\": " << m_manifest.overallHealth << ",\n";
        oss << "  \"overallCompletion\": " << m_manifest.overallCompletion << ",\n";
        
        // Components
        oss << "  \"components\": {\n";
        bool first = true;
        for (const auto& [name, comp] : m_manifest.components) {
            if (!first) oss << ",\n";
            first = false;
            oss << "    \"" << name << "\": {\n";
            oss << "      \"status\": \"" << componentStatusToString(comp.status) << "\",\n";
            oss << "      \"healthPoints\": " << comp.healthPoints << ",\n";
            oss << "      \"completionPercentage\": " << comp.completionPercentage << ",\n";
            oss << "      \"stubCount\": " << comp.stubCount << ",\n";
            oss << "      \"todoCount\": " << comp.todoCount << "\n";
            oss << "    }";
        }
        oss << "\n  },\n";
        
        // Critical issues
        oss << "  \"criticalIssues\": [";
        for (size_t i = 0; i < m_manifest.criticalIssues.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << escapeJson(m_manifest.criticalIssues[i]) << "\"";
        }
        oss << "],\n";
        
        // Recommendations
        oss << "  \"recommendations\": [";
        for (size_t i = 0; i < m_manifest.recommendations.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << escapeJson(m_manifest.recommendations[i]) << "\"";
        }
        oss << "]\n";
        
        oss << "}\n";
        return oss.str();
    }
    
    /**
     * @brief Export manifest to Markdown
     */
    std::string exportMarkdown() const {
        std::ostringstream oss;
        
        oss << "# RawrXD IDE Source Digestion Report\n\n";
        oss << "## Overview\n\n";
        oss << "| Metric | Value |\n";
        oss << "|--------|-------|\n";
        oss << "| Total Files | " << m_manifest.totalFiles << " |\n";
        oss << "| Total Lines | " << m_manifest.totalLines << " |\n";
        oss << "| Total Stubs | " << m_manifest.totalStubs << " |\n";
        oss << "| Total TODOs | " << m_manifest.totalTodos << " |\n";
        oss << "| Overall Health | " << m_manifest.overallHealth << "% |\n";
        oss << "| Overall Completion | " << std::fixed << std::setprecision(1) 
            << m_manifest.overallCompletion << "% |\n\n";
        
        // Health bar
        int healthBars = m_manifest.overallHealth / 5;
        oss << "### Health Status\n\n```\n[";
        for (int i = 0; i < 20; ++i) {
            oss << (i < healthBars ? "█" : "░");
        }
        oss << "] " << m_manifest.overallHealth << "%\n```\n\n";
        
        // Critical Issues
        if (!m_manifest.criticalIssues.empty()) {
            oss << "## ⚠️ Critical Issues\n\n";
            for (const auto& issue : m_manifest.criticalIssues) {
                oss << "- " << issue << "\n";
            }
            oss << "\n";
        }
        
        // Component Status
        oss << "## Component Status\n\n";
        oss << "| Component | Status | Health | Stubs |\n";
        oss << "|-----------|--------|--------|-------|\n";
        for (const auto& [name, comp] : m_manifest.components) {
            std::string emoji = comp.status == ComponentStatus::Complete ? "✅" :
                               comp.status == ComponentStatus::Partial ? "🟡" :
                               comp.status == ComponentStatus::Stub ? "🟠" : "❌";
            oss << "| " << name << " | " << emoji << " " << componentStatusToString(comp.status)
                << " | " << comp.healthPoints << "% | " << comp.stubCount << " |\n";
        }
        oss << "\n";
        
        // Recommendations
        if (!m_manifest.recommendations.empty()) {
            oss << "## Recommendations\n\n";
            for (const auto& rec : m_manifest.recommendations) {
                oss << "- " << rec << "\n";
            }
        }
        
        return oss.str();
    }

private:
    std::string m_sourceRoot;
    DigestConfig m_config;
    ProjectManifest m_manifest;
    std::atomic<bool> m_isRunning;
    mutable std::mutex m_mutex;
    std::vector<std::regex> m_stubRegexes;
    
    void compilePatterns() {
        for (const auto& pattern : m_config.stubPatterns) {
            try {
                m_stubRegexes.push_back(std::regex(pattern, std::regex::icase));
            } catch (const std::regex_error& e) {
                // Skip invalid patterns
            }
        }
    }
    
    std::vector<std::filesystem::path> discoverFiles() {
        std::vector<std::filesystem::path> files;
        
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(m_sourceRoot)) {
                if (!entry.is_regular_file()) continue;
                
                std::string path = entry.path().string();
                std::string ext = entry.path().extension().string();
                
                // Check extension
                bool validExt = false;
                for (const auto& e : m_config.sourceExtensions) {
                    if (ext == e) {
                        validExt = true;
                        break;
                    }
                }
                if (!validExt) continue;
                
                // Check exclude patterns
                bool excluded = false;
                for (const auto& pattern : m_config.excludePatterns) {
                    if (path.find(pattern) != std::string::npos) {
                        excluded = true;
                        break;
                    }
                }
                if (excluded) continue;
                
                files.push_back(entry.path());
            }
        } catch (const std::filesystem::filesystem_error& e) {
            // Handle error
        }
        
        return files;
    }
    
    void analyzeFiles(const std::vector<std::filesystem::path>& files, 
                      ProgressCallback progressCallback) {
        int processed = 0;
        int total = static_cast<int>(files.size());
        
        for (const auto& file : files) {
            auto analysis = analyzeFile(file);
            if (analysis.lineCount > 0) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_manifest.fileAnalyses[analysis.relativePath] = analysis;
                
                // Update category tracking
                m_manifest.filesByCategory[analysis.category].push_back(analysis.relativePath);
            }
            
            processed++;
            if (progressCallback && processed % 20 == 0) {
                int progress = 10 + (processed * 60 / total);
                progressCallback(progress, 100, "Analyzing: " + file.filename().string());
            }
        }
    }
    
    FileAnalysis analyzeFile(const std::filesystem::path& filePath) {
        FileAnalysis analysis;
        analysis.path = filePath.string();
        analysis.relativePath = std::filesystem::relative(filePath, m_sourceRoot).string();
        analysis.extension = filePath.extension().string();
        
        try {
            analysis.sizeBytes = std::filesystem::file_size(filePath);
            analysis.lastModified = std::chrono::system_clock::now(); // Simplified
            
            std::ifstream file(filePath);
            if (!file.is_open()) return analysis;
            
            std::string line;
            int lineNum = 0;
            bool inMultilineComment = false;
            
            while (std::getline(file, line)) {
                lineNum++;
                analysis.lineCount++;
                
                // Trim
                size_t start = line.find_first_not_of(" \t");
                std::string trimmed = (start == std::string::npos) ? "" : line.substr(start);
                
                // Count line types
                if (trimmed.empty()) {
                    analysis.blankLines++;
                } else if (inMultilineComment) {
                    analysis.commentLines++;
                    if (trimmed.find("*/") != std::string::npos) {
                        inMultilineComment = false;
                    }
                } else if (trimmed.substr(0, 2) == "/*") {
                    analysis.commentLines++;
                    if (trimmed.find("*/") == std::string::npos) {
                        inMultilineComment = true;
                    }
                } else if (trimmed.substr(0, 2) == "//" || trimmed[0] == '#') {
                    analysis.commentLines++;
                } else {
                    analysis.codeLines++;
                }
                
                // Check for stubs
                for (size_t i = 0; i < m_stubRegexes.size(); ++i) {
                    if (std::regex_search(line, m_stubRegexes[i])) {
                        StubInfo stub;
                        stub.lineNumber = lineNum;
                        stub.content = line.substr(0, std::min(line.size(), size_t(100)));
                        stub.patternMatched = m_config.stubPatterns[i];
                        stub.severity = determineSeverity(line);
                        analysis.stubs.push_back(stub);
                        break;
                    }
                }
                
                // Check for TODOs
                std::regex todoRegex(R"((TODO|FIXME|HACK|XXX)\s*:?\s*(.*))", std::regex::icase);
                std::smatch match;
                if (std::regex_search(line, match, todoRegex)) {
                    TodoInfo todo;
                    todo.lineNumber = lineNum;
                    todo.content = match[0].str().substr(0, 100);
                    todo.type = match[1].str();
                    analysis.todos.push_back(todo);
                }
            }
            
            // Determine category
            analysis.category = determineCategory(analysis.relativePath);
            
            // Calculate health
            analysis.calculateHealth();
            
        } catch (const std::exception& e) {
            // Handle error
        }
        
        return analysis;
    }
    
    Severity determineSeverity(const std::string& line) {
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        if (lower.find("critical") != std::string::npos) return Severity::Critical;
        if (lower.find("fixme") != std::string::npos) return Severity::High;
        if (lower.find("bug") != std::string::npos) return Severity::High;
        if (lower.find("todo") != std::string::npos) return Severity::Medium;
        return Severity::Low;
    }
    
    std::string determineCategory(const std::string& path) {
        std::string lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        for (const auto& [category, patterns] : m_config.categories) {
            for (const auto& pattern : patterns) {
                if (lower.find(pattern) != std::string::npos) {
                    return category;
                }
            }
        }
        return "other";
    }
    
    void analyzeComponents() {
        for (const auto& [name, expectedPath] : m_config.criticalComponents) {
            ComponentInfo comp;
            comp.name = name;
            
            // Find matching files
            for (const auto& [path, analysis] : m_manifest.fileAnalyses) {
                std::string lower = path;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                std::string nameLower = name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                
                if (lower.find(expectedPath) != std::string::npos || 
                    lower.find(nameLower) != std::string::npos) {
                    comp.files.push_back(path);
                    comp.totalLines += analysis.lineCount;
                    comp.stubCount += static_cast<int>(analysis.stubs.size());
                    comp.todoCount += static_cast<int>(analysis.todos.size());
                }
            }
            
            comp.calculateStatus();
            m_manifest.components[name] = comp;
        }
    }
    
    void calculateOverallMetrics() {
        int totalHealth = 0;
        double totalCompletion = 0.0;
        
        for (const auto& [path, analysis] : m_manifest.fileAnalyses) {
            m_manifest.totalFiles++;
            m_manifest.totalLines += analysis.lineCount;
            m_manifest.totalCodeLines += analysis.codeLines;
            m_manifest.totalStubs += static_cast<int>(analysis.stubs.size());
            m_manifest.totalTodos += static_cast<int>(analysis.todos.size());
            totalHealth += analysis.healthPoints;
            totalCompletion += analysis.completionPercentage;
        }
        
        if (m_manifest.totalFiles > 0) {
            m_manifest.overallHealth = totalHealth / m_manifest.totalFiles;
            m_manifest.overallCompletion = totalCompletion / m_manifest.totalFiles;
        }
    }
    
    void generateRecommendations() {
        for (const auto& [name, comp] : m_manifest.components) {
            if (comp.status == ComponentStatus::Missing) {
                m_manifest.criticalIssues.push_back("CRITICAL: " + name + " component is missing");
                m_manifest.missingFeatures.push_back(name);
            } else if (comp.status == ComponentStatus::Stub) {
                m_manifest.criticalIssues.push_back(
                    "HIGH: " + name + " is mostly stubs (" + std::to_string(comp.stubCount) + " stubs)");
                m_manifest.recommendations.push_back(
                    "Implement " + name + " - currently at " + 
                    std::to_string(static_cast<int>(comp.completionPercentage)) + "%");
            }
        }
        
        // Find files with most stubs
        std::vector<std::pair<std::string, int>> stubFiles;
        for (const auto& [path, analysis] : m_manifest.fileAnalyses) {
            if (!analysis.stubs.empty()) {
                stubFiles.push_back({path, static_cast<int>(analysis.stubs.size())});
            }
        }
        
        std::sort(stubFiles.begin(), stubFiles.end(), 
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        if (!stubFiles.empty()) {
            m_manifest.recommendations.push_back("Top files needing stub implementation:");
            for (size_t i = 0; i < std::min(stubFiles.size(), size_t(10)); ++i) {
                m_manifest.recommendations.push_back(
                    "  - " + stubFiles[i].first + ": " + 
                    std::to_string(stubFiles[i].second) + " stubs");
            }
        }
    }
    
    void selfAudit() {
        // Basic self-audit checks
        if (m_manifest.totalFiles == 0) {
            m_manifest.criticalIssues.push_back("AUDIT: No files analyzed - check source path");
        }
        
        if (m_manifest.overallHealth < 0 || m_manifest.overallHealth > 100) {
            m_manifest.criticalIssues.push_back("AUDIT: Invalid health metric calculated");
        }
    }
    
    static std::string componentStatusToString(ComponentStatus status) {
        switch (status) {
            case ComponentStatus::Complete: return "complete";
            case ComponentStatus::Partial: return "partial";
            case ComponentStatus::Stub: return "stub";
            case ComponentStatus::Missing: return "missing";
            default: return "unknown";
        }
    }
    
    static std::string escapeJson(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c;
            }
        }
        return result;
    }
};

// ============================================================================
// HEALTH CHECK ENDPOINT
// ============================================================================

/**
 * @brief Health check endpoint for the IDE
 * 
 * Provides real-time health monitoring capabilities.
 */
class HealthCheckEndpoint {
public:
    explicit HealthCheckEndpoint(const std::string& sourceRoot)
        : m_engine(std::make_unique<SourceDigestionEngine>(sourceRoot))
        , m_lastCheck(std::chrono::system_clock::now())
    {}
    
    /**
     * @brief Get quick health status
     */
    struct QuickStatus {
        int healthPoints;
        double completionPercentage;
        int stubCount;
        int todoCount;
        bool isHealthy;
    };
    
    QuickStatus getQuickStatus() {
        const auto& manifest = m_engine->getManifest();
        return {
            manifest.overallHealth,
            manifest.overallCompletion,
            manifest.totalStubs,
            manifest.totalTodos,
            manifest.overallHealth >= 70 && manifest.overallCompletion >= 80.0
        };
    }
    
    /**
     * @brief Run full health check
     */
    ProjectManifest runFullCheck(SourceDigestionEngine::ProgressCallback callback = nullptr) {
        auto manifest = m_engine->digest(callback);
        m_lastCheck = std::chrono::system_clock::now();
        return manifest;
    }
    
    /**
     * @brief Get time since last check
     */
    std::chrono::seconds timeSinceLastCheck() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - m_lastCheck
        );
    }
    
    /**
     * @brief Export health report
     */
    std::string exportHealthReport(const std::string& format = "json") {
        if (format == "json") {
            return m_engine->exportJson();
        } else if (format == "markdown") {
            return m_engine->exportMarkdown();
        }
        return "";
    }

private:
    std::unique_ptr<SourceDigestionEngine> m_engine;
    std::chrono::system_clock::time_point m_lastCheck;
};

} // namespace Digestion
} // namespace RawrXD

#endif // RAWRXD_SOURCE_DIGESTION_ENGINE_HPP
