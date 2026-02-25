#pragma once
#include "universal_generator.h"
#include <memory>
#include <string>

// Core Generator - Integrated directly into the engine
class CoreGenerator {
private:
    std::unique_ptr<UniversalGenerator> generator;
    static CoreGenerator* instance;
    
    CoreGenerator();
    
public:
    static CoreGenerator& GetInstance();
    
    // Initialize the generator
    void Initialize();
    
    // Generate projects directly
    bool Generate(const std::string& name, LanguageType language, 
                  const std::filesystem::path& output_dir);
    
    bool GenerateFromTemplate(const std::string& template_name,
                             const std::string& project_name,
                             const std::filesystem::path& output_dir);
    
    // Quick generation methods
    bool GenerateWebApp(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateCLI(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateLibrary(const std::string& name, LanguageType lang, const std::filesystem::path& output_dir);
    bool GenerateGame(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateEmbedded(const std::string& name, const std::filesystem::path& output_dir);
    bool GenerateDataScience(const std::string& name, const std::filesystem::path& output_dir);
    
    // List available options
    std::vector<std::string> ListLanguages();
    std::vector<std::string> ListTemplates();
    
    // Get generator statistics
    size_t GetLanguageCount() const;
    size_t GetTemplateCount() const;
    
    // Advanced generation
    bool GenerateWithAllFeatures(const std::string& name, LanguageType language,
                                const std::filesystem::path& output_dir);
};
