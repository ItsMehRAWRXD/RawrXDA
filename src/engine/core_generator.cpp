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
    std::cout << "[Core Generator] Initializing (Stubbed)...\n";
}

bool CoreGenerator::Generate(const std::string& name, LanguageType language, 
                            const std::filesystem::path& output_dir) {
    if (generator) return generator->GenerateProject(name, language, output_dir);
    return false;
}

bool CoreGenerator::GenerateFromTemplate(const std::string& template_name,
                                        const std::string& project_name,
                                        const std::filesystem::path& output_dir) {
    if (generator) return generator->GenerateFromTemplate(template_name, project_name, output_dir);
    return false;
}

bool CoreGenerator::GenerateWebApp(const std::string& name, const std::filesystem::path& output_dir) { return false; }
bool CoreGenerator::GenerateCLI(const std::string& name, const std::filesystem::path& output_dir) { return false; }
bool CoreGenerator::GenerateLibrary(const std::string& name, LanguageType lang, const std::filesystem::path& output_dir) { return false; }
bool CoreGenerator::GenerateGame(const std::string& name, const std::filesystem::path& output_dir) { return false; }
bool CoreGenerator::GenerateEmbedded(const std::string& name, const std::filesystem::path& output_dir) { return false; }
bool CoreGenerator::GenerateDataScience(const std::string& name, const std::filesystem::path& output_dir) { return false; }

std::vector<std::string> CoreGenerator::ListLanguages() { return {}; }
std::vector<std::string> CoreGenerator::ListTemplates() { return {}; }
size_t CoreGenerator::GetLanguageCount() const { return 0; }
size_t CoreGenerator::GetTemplateCount() const { return 0; }
bool CoreGenerator::GenerateWithAllFeatures(const std::string&, LanguageType, const std::filesystem::path&) { return false; }

// =========================================================
// UniversalGenerator Stubs
// =========================================================

UniversalGenerator::UniversalGenerator() {}

bool UniversalGenerator::GenerateProject(const std::string& name, LanguageType language, 
                        const std::filesystem::path& output_dir) { return false; }

bool UniversalGenerator::GenerateFromTemplate(const std::string& template_name,
                             const std::string& project_name,
                             const std::filesystem::path& output_dir) { return false; }

std::vector<std::string> UniversalGenerator::ListTemplates() const { return {}; }
std::vector<std::string> UniversalGenerator::ListLanguages() const { return {}; }
bool UniversalGenerator::AddCustomTemplate(const ProjectTemplate& tmpl) { return false; }
bool UniversalGenerator::AddCustomLanguage(const LanguageConfig& config) { return false; }

// Stub language specific generators just in case
bool UniversalGenerator::GenerateCProject(const std::string&, const std::filesystem::path&) { return false; }
bool UniversalGenerator::GenerateCppProject(const std::string&, const std::filesystem::path&) { return false; }
bool UniversalGenerator::GenerateRustProject(const std::string&, const std::filesystem::path&) { return false; }
bool UniversalGenerator::GeneratePythonProject(const std::string&, const std::filesystem::path&) { return false; }
bool UniversalGenerator::GenerateGoProject(const std::string&, const std::filesystem::path&) { return false; }
bool UniversalGenerator::GenerateJavaScriptProject(const std::string&, const std::filesystem::path&) { return false; }
