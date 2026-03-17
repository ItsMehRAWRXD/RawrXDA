#include "core_generator.h"
#include <iostream>

CoreGenerator* CoreGenerator::instance = nullptr;

CoreGenerator::CoreGenerator() {
    generator = std::make_unique<UniversalGenerator>();
}

CoreGenerator& CoreGenerator::GetInstance() {
    if (!instance) {
        instance = new CoreGenerator();
    }
    return *instance;
}

void CoreGenerator::Initialize() {
    std::cout << "[Core Generator] Initializing...\n";
    std::cout << "  Languages: " << generator->GetSupportedLanguageCount() << "\n";
    std::cout << "  Templates: " << generator->GetTemplateCount() << "\n";
}

bool CoreGenerator::Generate(const std::string& name, LanguageType language, 
                            const std::filesystem::path& output_dir) {
    return generator->GenerateProject(name, language, output_dir);
}

bool CoreGenerator::GenerateFromTemplate(const std::string& template_name,
                                        const std::string& project_name,
                                        const std::filesystem::path& output_dir) {
    return generator->GenerateFromTemplate(template_name, project_name, output_dir);
}

bool CoreGenerator::GenerateWebApp(const std::string& name, const std::filesystem::path& output_dir) {
    return generator->GenerateFromTemplate("web-app", name, output_dir);
}

bool CoreGenerator::GenerateCLI(const std::string& name, const std::filesystem::path& output_dir) {
    return generator->GenerateFromTemplate("cli-tool", name, output_dir);
}

bool CoreGenerator::GenerateLibrary(const std::string& name, LanguageType lang, const std::filesystem::path& output_dir) {
    return generator->GenerateFromTemplate("library", name, output_dir);
}

bool CoreGenerator::GenerateGame(const std::string& name, const std::filesystem::path& output_dir) {
    return generator->GenerateFromTemplate("game", name, output_dir);
}

bool CoreGenerator::GenerateEmbedded(const std::string& name, const std::filesystem::path& output_dir) {
    return generator->GenerateFromTemplate("embedded", name, output_dir);
}

bool CoreGenerator::GenerateDataScience(const std::string& name, const std::filesystem::path& output_dir) {
    return generator->GenerateFromTemplate("data-science", name, output_dir);
}

std::vector<std::string> CoreGenerator::ListLanguages() {
    return generator->ListLanguages();
}

std::vector<std::string> CoreGenerator::ListTemplates() {
    return generator->ListTemplates();
}

size_t CoreGenerator::GetLanguageCount() const {
    return generator->GetSupportedLanguageCount();
}

size_t CoreGenerator::GetTemplateCount() const {
    return generator->GetTemplateCount();
}

bool CoreGenerator::GenerateWithAllFeatures(const std::string& name, LanguageType language,
                                           const std::filesystem::path& output_dir) {
    std::cout << "[Core Generator] Generating " << name << " with all features...\n";
    
    // Generate base project
    if (!generator->GenerateProject(name, language, output_dir)) {
        return false;
    }
    
    // Add tests
    if (!generator->GenerateWithTests(name, language, output_dir)) {
        std::cout << "Warning: Failed to add tests\n";
    }
    
    // Add CI/CD
    if (!generator->GenerateWithCI(name, language, output_dir)) {
        std::cout << "Warning: Failed to add CI/CD\n";
    }
    
    // Add Docker
    if (!generator->GenerateWithDocker(name, language, output_dir)) {
        std::cout << "Warning: Failed to add Docker\n";
    }
    
    std::cout << "✓ Generated " << name << " with all features\n";
    return true;
}
