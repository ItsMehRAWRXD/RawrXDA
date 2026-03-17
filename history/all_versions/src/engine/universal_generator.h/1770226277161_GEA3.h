#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>

// Undef Windows macros that might conflict
#ifdef PASCAL
#undef PASCAL
#endif

enum class LanguageType {
    // Systems Programming
    C, CPP, RUST, GO, ZIG, NIM, CRYSTAL,
    
    // Scripting
    PYTHON, JAVASCRIPT, TYPESCRIPT, LUA, RUBY, PERL, PHP, BASH, POWERSHELL,
    
    // Functional
    HASKELL, OCAML, FSHARP, CLOJURE, ELIXIR, ERLANG, SCALA, KOTLIN,
    
    // Web
    HTML, CSS, SASS, LESS, REACT, VUE, ANGULAR, SVELTE,
    
    // Mobile
    SWIFT, OBJECTIVE_C, DART, FLUTTER, REACT_NATIVE,
    
    // Game
    CSHARP, UNITY, UNREAL, GODOT,
    
    // Data Science
    R, JULIA, MATLAB, OCTAVE,
    
    // Embedded
    C51, AVR, ARM, ESP32, ARDUINO, PLATFORMIO,
    
    // Assembly
    X86, X64, ARM_ASM, MIPS, RISCV,
    
    // Markup/Config
    JSON, XML, YAML, TOML, INI, MARKDOWN,
    
    // Database
    SQL, PL_SQL, T_SQL, MONGODB, REDIS,
    
    // Other
    COBOL, FORTRAN, PASCAL_LANG, DELPHI, VBNET, ADA, D, V, VALA
};

struct LanguageConfig {
    std::string name;
    std::string file_extension;
    std::string build_system;
    std::string package_manager;
    std::vector<std::string> source_folders;
    std::vector<std::string> test_folders;
    std::unordered_map<std::string, std::string> templates;
    bool requires_compiler;
    bool requires_runtime;
    std::vector<std::string> dependencies;
};

struct ProjectTemplate {
    std::string name;
    std::string description;
    LanguageType language;
    std::unordered_map<std::string, std::string> files; // path -> content
    std::vector<std::string> build_commands;
    std::vector<std::string> run_commands;
};

class UniversalGenerator {
private:
    std::unordered_map<LanguageType, LanguageConfig> language_configs;
    std::unordered_map<std::string, ProjectTemplate> templates;
    
    void InitializeLanguages();
    void InitializeTemplates();
    
    std::string GenerateCMakeLists(const ProjectTemplate& tmpl);
    std::string GenerateMakefile(const ProjectTemplate& tmpl);
    std::string GeneratePackageJson(const ProjectTemplate& tmpl);
    std::string GenerateCargoToml(const ProjectTemplate& tmpl);
    std::string GenerateGoMod(const ProjectTemplate& tmpl);
    std::string GeneratePyProject(const ProjectTemplate& tmpl);
    std::string GenerateCsProj(const ProjectTemplate& tmpl);
    std::string GenerateBuildGradle(const ProjectTemplate& tmpl);
    
    void WriteFile(const std::filesystem::path& path, const std::string& content);
    void CreateDirectoryStructure(const std::filesystem::path& base, const std::vector<std::string>& folders);
    
public:
    UniversalGenerator();
    
    bool GenerateProject(const std::string& name, LanguageType language, 
                        const std::filesystem::path& output_dir);
    
    bool GenerateFromTemplate(const std::string& template_name,
                             const std::string& project_name,
                             const std::filesystem::path& output_dir);
    
    std::vector<std::string> ListTemplates() const;
    std::vector<std::string> ListLanguages() const;
    
    bool AddCustomTemplate(const ProjectTemplate& tmpl);
    bool AddCustomLanguage(const LanguageConfig& config);
    
    // Language-specific generators
    bool GenerateCProject(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateCppProject(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateRustProject(const std::string& name, const std::filesystem::path& output_dir);
    bool GeneratePythonProject(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateGoProject(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateJavaScriptProject(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateReactProject(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateUnityProject(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateUnrealProject(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateArduinoProject(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateAssemblyProject(const std::string& name, const std::filesystem::path& output_dir);
    
    // Advanced features
    bool GenerateWithTests(const std::string& name, LanguageType language, 
                          const std::filesystem::path& output_dir);
    bool GenerateWithCI(const std::string& name, LanguageType language,
                       const std::filesystem::path& output_dir);
    bool GenerateWithDocker(const std::string& name, LanguageType language,
                           const std::filesystem::path& output_dir);
    
    // Statistics
    size_t GetSupportedLanguageCount() const { return language_configs.size(); }
    size_t GetTemplateCount() const { return templates.size(); }
};
