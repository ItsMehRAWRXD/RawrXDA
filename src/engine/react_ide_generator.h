#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

struct ReactIDETemplate {
    std::string name;
    std::string description;
    std::vector<std::string> features;
    std::unordered_map<std::string, std::string> files;
    std::vector<std::string> dependencies;
    std::vector<std::string> devDependencies;
    std::vector<std::string> buildCommands;
    std::vector<std::string> runCommands;
};

class ReactIDEGenerator {
private:
    std::unordered_map<std::string, ReactIDETemplate> templates;
    
    void InitializeTemplates();
    
    std::string GeneratePackageJson(const ReactIDETemplate& tmpl);
    std::string GenerateTsConfig();
    std::string GenerateViteConfig();
    std::string GenerateTailwindConfig();
    std::string GenerateMainTsx();
    std::string GenerateAppTsx();
    std::string GenerateIndexHtml();
    std::string GenerateCodeEditor();
    std::string GenerateEngineBridge();
    std::string GenerateMemoryPanel();
    std::string GenerateHotPatchPanel();
    std::string GenerateVSIXLoaderPanel();
    std::string GenerateModelLoaderPanel();
    std::string GenerateChatInterface();
    std::string GenerateSubAgentPanel();
    std::string GenerateHistoryPanel();
    std::string GeneratePolicyPanel();
    std::string GenerateSettingsPanel();
    
    void WriteFile(const std::filesystem::path& path, const std::string& content);
    void CreateDirectoryStructure(const std::filesystem::path& base);
    
public:
    ReactIDEGenerator();
    
    bool GenerateIDE(const std::string& name, const std::string& template_name,
                     const std::filesystem::path& output_dir);
    
    bool GenerateMinimalIDE(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateFullIDE(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateAgenticIDE(const std::string& name, const std::filesystem::path& output_dir);
    
    std::vector<std::string> ListTemplates() const;
    std::vector<std::string> GetTemplateFeatures(const std::string& template_name) const;
    
    // Language-specific IDEs
    bool GenerateCppIDE(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateRustIDE(const std::string& name, const std::filesystem::path& output_dir);
    bool GeneratePythonIDE(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateMultiLanguageIDE(const std::string& name, const std::filesystem::path& output_dir);
    
    // Advanced features
    bool GenerateIDEWithTests(const std::string& name, const std::string& template_name,
                             const std::filesystem::path& output_dir);
    bool GenerateIDEWithCI(const std::string& name, const std::string& template_name,
                          const std::filesystem::path& output_dir);
    bool GenerateIDEWithDocker(const std::string& name, const std::string& template_name,
                              const std::filesystem::path& output_dir);
    
    // Statistics
    size_t GetTemplateCount() const { return templates.size(); }
};
