#pragma once

// TLS Beaconism IDE - Visual Studio 2022 Professional Edition
// Enterprise-grade development environment with full toolchain integration

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <functional>
#include <memory>
#include <thread>
#include <chrono>
#include <regex>
#include <fstream>
#include <sstream>
#include "toolchain-manager.hpp"

// Forward declarations for enterprise features
class IntelliSenseEngine;
class DebuggerCore;
class ProjectManager;
class BuildSystem;
class ExtensionManager;

// VS2022-style theme configuration
enum class Theme {
    Dark,
    Light,
    Blue,
    HighContrast
};

// Language detection and syntax highlighting
struct LanguageInfo {
    std::string name;
    std::string extension;
    std::string compiler_path;
    std::vector<std::string> keywords;
    std::string syntax_pattern;
    bool supports_debugging;
    bool supports_intellisense;
};

// Professional IDE Core with VS2022 features
class TLSBeaconismIDE {
private:
    // Core systems
    std::unique_ptr<IntelliSenseEngine> intellisense_;
    std::unique_ptr<DebuggerCore> debugger_;
    std::unique_ptr<ProjectManager> project_manager_;
    std::unique_ptr<BuildSystem> build_system_;
    std::unique_ptr<ExtensionManager> extension_manager_;
    
    // Performance monitoring
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> cache_hits_{0};
    std::atomic<uint64_t> cache_misses_{0};
    std::atomic<uint64_t> compile_count_{0};
    std::atomic<uint64_t> debug_sessions_{0};
    
    // File system cache
    std::unordered_map<std::string, std::string> file_cache_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> file_timestamps_;
    std::mutex cache_mutex_;
    
    // Language support
    std::vector<LanguageInfo> supported_languages_;
    std::unordered_map<std::string, std::string> toolchain_paths_;
    std::unique_ptr<ToolchainManager> toolchain_manager_;
    
    // UI state
    Theme current_theme_{Theme::Dark};
    bool show_line_numbers_{true};
    bool enable_word_wrap_{false};
    bool enable_auto_complete_{true};
    
    // Internal helpers
    void InitializeSupportedLanguages();
    
public:
    TLSBeaconismIDE();
    ~TLSBeaconismIDE();
    
    // Core initialization
    bool Initialize();
    void Shutdown();
    
    // VS2022-style file operations
    std::vector<std::string> ListDirectory(const std::string& path);
    std::string ReadFileOptimized(const std::string& path);
    bool WriteFileOptimized(const std::string& path, const std::string& content);
    std::vector<std::string> SearchFiles(const std::string& pattern, const std::string& root_path);
    
    // Language and toolchain management
    bool DetectAllToolchains();
    LanguageInfo DetectLanguage(const std::string& file_path);
    std::vector<LanguageInfo> GetSupportedLanguages() const;
    
    // Professional compilation system
    struct CompileResult {
        bool success;
        std::string output;
        std::string errors;
        std::string executable_path;
        std::chrono::milliseconds compile_time;
        size_t warnings_count;
        size_t errors_count;
    };
    
    CompileResult CompileProject(const std::string& project_path, const std::string& configuration = "Release");
    CompileResult CompileSingleFile(const std::string& file_path, const std::string& output_path = "");
    
    // Debugging features
    bool StartDebugSession(const std::string& executable_path);
    void StopDebugSession();
    bool SetBreakpoint(const std::string& file_path, int line_number);
    bool RemoveBreakpoint(const std::string& file_path, int line_number);
    
    // IntelliSense and code analysis
    std::vector<std::string> GetAutoCompletions(const std::string& file_path, int line, int column);
    std::vector<std::string> GetCodeIssues(const std::string& file_path);
    bool RefactorRename(const std::string& file_path, int line, int column, const std::string& new_name);
    
    // Project management
    bool CreateProject(const std::string& project_name, const std::string& template_type);
    bool OpenProject(const std::string& project_path);
    bool SaveProject();
    std::vector<std::string> GetProjectFiles();
    
    // Performance monitoring
    double GetCacheHitRatio() const;
    uint64_t GetCompileCount() const;
    uint64_t GetDebugSessionCount() const;
    std::string GetPerformanceReport() const;
    
    // Theme and UI management
    void SetTheme(Theme theme);
    Theme GetTheme() const;
    void SetLineNumbers(bool enabled);
    void SetWordWrap(bool enabled);
    void SetAutoComplete(bool enabled);
    
    // Extension system
    bool LoadExtension(const std::string& extension_path);
    void UnloadExtension(const std::string& extension_name);
    std::vector<std::string> GetLoadedExtensions() const;
};

// IntelliSense Engine - VS2022 style code intelligence
class IntelliSenseEngine {
private:
    std::unordered_map<std::string, std::vector<std::string>> symbol_cache_;
    std::unordered_map<std::string, std::vector<std::string>> include_paths_;
    
public:
    bool Initialize();
    std::vector<std::string> GetCompletions(const std::string& file_path, const std::string& prefix);
    std::vector<std::string> GetSignatureHelp(const std::string& file_path, int line, int column);
    bool GoToDefinition(const std::string& symbol, std::string& file_path, int& line);
    std::vector<std::string> FindReferences(const std::string& symbol);
};

// Professional Debugger Core
class DebuggerCore {
private:
    HANDLE debug_process_;
    std::vector<std::pair<std::string, int>> breakpoints_;
    bool debugging_active_;
    
public:
    DebuggerCore();
    ~DebuggerCore();
    
    bool AttachToProcess(DWORD process_id);
    bool StartProcess(const std::string& executable_path);
    void DetachFromProcess();
    
    bool SetBreakpoint(const std::string& file_path, int line);
    bool RemoveBreakpoint(const std::string& file_path, int line);
    
    std::string GetCallStack();
    std::string GetVariableValue(const std::string& variable_name);
    bool StepOver();
    bool StepInto();
    bool StepOut();
    bool Continue();
};

// Enterprise Project Manager
class ProjectManager {
private:
    std::string current_project_path_;
    std::vector<std::string> project_files_;
    std::unordered_map<std::string, std::string> project_settings_;
    
public:
    bool CreateProject(const std::string& name, const std::string& type);
    bool OpenProject(const std::string& path);
    bool SaveProject();
    
    std::vector<std::string> GetFiles() const;
    bool AddFile(const std::string& file_path);
    bool RemoveFile(const std::string& file_path);
    
    std::string GetSetting(const std::string& key) const;
    void SetSetting(const std::string& key, const std::string& value);
};

// Advanced Build System with MSBuild integration
class BuildSystem {
private:
    std::unordered_map<std::string, std::string> build_configurations_;
    std::string msbuild_path_;
    
public:
    bool Initialize();
    TLSBeaconismIDE::CompileResult Build(const std::string& project_path, const std::string& configuration);
    bool Clean(const std::string& project_path);
    bool Rebuild(const std::string& project_path, const std::string& configuration);
    
    std::vector<std::string> GetConfigurations() const;
    void AddConfiguration(const std::string& name, const std::string& settings);
};

// Extension marketplace and manager
struct Extension {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::vector<std::string> supported_languages;
    bool is_installed;
    bool is_enabled;
    std::string download_url;
    size_t downloads;
    double rating;
};

// Extension Manager for VS2022-style extensibility with marketplace
class ExtensionManager {
private:
    std::vector<Extension> available_extensions_;
    std::vector<std::string> loaded_extensions_;
    std::string marketplace_url_;
    
public:
    bool Initialize();
    
    // Marketplace functions
    std::vector<Extension> BrowseMarketplace();
    std::vector<Extension> SearchExtensions(const std::string& query);
    bool InstallExtension(const std::string& extension_id);
    bool UninstallExtension(const std::string& extension_id);
    
    // Extension management
    bool LoadExtension(const std::string& path);
    void UnloadExtension(const std::string& name);
    std::vector<std::string> GetExtensions() const;
    std::vector<Extension> GetInstalledExtensions() const;
    
    // Ratings and reviews
    bool RateExtension(const std::string& extension_id, double rating);
    std::vector<std::string> GetExtensionReviews(const std::string& extension_id);
};