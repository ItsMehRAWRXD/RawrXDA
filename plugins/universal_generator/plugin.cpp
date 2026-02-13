#include "universal_generator.h"
#include <iostream>
#include <sstream>

// Global generator instance
UniversalGenerator g_generator;

// Plugin entry points
extern "C" {
    __declspec(dllexport) bool OnLoad() {
        std::cout << "[Universal Generator] Plugin loaded successfully\n";
        std::cout << "Supported languages: " << g_generator.GetSupportedLanguageCount() << "\n";
        std::cout << "Available templates: " << g_generator.GetTemplateCount() << "\n";
        std::cout << "Specialized Generators: React IDE Generator Active\n";
        return true;
    }
    
    __declspec(dllexport) void OnUnload() {
        std::cout << "[Universal Generator] Plugin unloaded\n";
    }
    
    __declspec(dllexport) bool OnCommand(const char* command, const char** args, int arg_count) {
        std::string cmd(command);
        
        if (cmd == "/generate") {
            if (arg_count < 2) {
                std::cout << "Usage: /generate <project_name> <language> [output_dir]\n";
                return false;
            }
            
            std::string project_name(args[0]);
            std::string language_str(args[1]);
            std::filesystem::path output_dir = (arg_count > 2) ? args[2] : std::filesystem::current_path();
            
            // Map string to LanguageType
            LanguageType lang = LanguageType::CPP; // default
            
            // Convert to uppercase for comparison
            std::transform(language_str.begin(), language_str.end(), language_str.begin(), ::toupper);
            
            if (language_str == "C") lang = LanguageType::C;
            else if (language_str == "CPP" || language_str == "C++") lang = LanguageType::CPP;
            else if (language_str == "RUST") lang = LanguageType::RUST;
            else if (language_str == "GO") lang = LanguageType::GO;
            else if (language_str == "PYTHON") lang = LanguageType::PYTHON;
            else if (language_str == "JAVASCRIPT" || language_str == "JS") lang = LanguageType::JAVASCRIPT;
            else if (language_str == "TYPESCRIPT" || language_str == "TS") lang = LanguageType::TYPESCRIPT;
            else if (language_str == "REACT") lang = LanguageType::REACT;
            else if (language_str == "SWIFT") lang = LanguageType::SWIFT;
            else if (language_str == "CSHARP" || language_str == "C#") lang = LanguageType::CSHARP;
            else if (language_str == "UNITY") lang = LanguageType::UNITY;
            else if (language_str == "HASKELL") lang = LanguageType::HASKELL;
            else if (language_str == "ARDUINO") lang = LanguageType::ARDUINO;
            else if (language_str == "ASSEMBLY" || language_str == "ASM") lang = LanguageType::X64;
            
            bool success = g_generator.GenerateProject(project_name, lang, output_dir);
            
            if (success) {
                std::cout << "✓ Generated " << project_name << " in " << output_dir.string() << "\n";
            } else {
                std::cout << "✗ Failed to generate project\n";
            }
            
            return success;
        }
        else if (cmd == "/generate-from-template") {
            if (arg_count < 2) {
                std::cout << "Usage: /generate-from-template <template_name> <project_name> [output_dir]\n";
                return false;
            }
            
            std::string template_name(args[0]);
            std::string project_name(args[1]);
            std::filesystem::path output_dir = (arg_count > 2) ? args[2] : std::filesystem::current_path();
            
            bool success = g_generator.GenerateFromTemplate(template_name, project_name, output_dir);
            
            if (success) {
                std::cout << "✓ Generated " << project_name << " from template " << template_name << "\n";
            } else {
                std::cout << "✗ Failed to generate from template\n";
            }
            
            return success;
        }
        else if (cmd == "/list-languages") {
            auto languages = g_generator.ListLanguages();
            std::cout << "Supported languages (" << languages.size() << "):\n";
            for (const auto& lang : languages) {
                std::cout << "  • " << lang << "\n";
            }
            return true;
        }
        else if (cmd == "/list-templates") {
            auto tmpl_list = g_generator.ListTemplates();
            std::cout << "Available templates (" << tmpl_list.size() << "):\n";
            for (const auto& tmpl : tmpl_list) {
                std::cout << "  • " << tmpl << "\n";
            }
            return true;
        }
        else if (cmd == "/generate-with-tests") {
            if (arg_count < 2) {
                std::cout << "Usage: /generate-with-tests <project_name> <language> [output_dir]\n";
                return false;
            }
            
            std::string project_name(args[0]);
            std::string language_str(args[1]);
            std::filesystem::path output_dir = (arg_count > 2) ? args[2] : std::filesystem::current_path();
            
            // Map language string to enum
            LanguageType lang = LanguageType::CPP; // default
            std::transform(language_str.begin(), language_str.end(), language_str.begin(), ::toupper);
            
            if (language_str == "C") lang = LanguageType::C;
            else if (language_str == "CPP" || language_str == "C++") lang = LanguageType::CPP;
            else if (language_str == "PYTHON") lang = LanguageType::PYTHON;
            else if (language_str == "RUST") lang = LanguageType::RUST;
            
            bool success = g_generator.GenerateWithTests(project_name, lang, output_dir);
            
            if (success) {
                std::cout << "✓ Generated " << project_name << " with tests\n";
            } else {
                std::cout << "✗ Failed to generate project with tests\n";
            }
            
            return success;
        }
        else if (cmd == "/generate-with-ci") {
            if (arg_count < 2) {
                std::cout << "Usage: /generate-with-ci <project_name> <language> [output_dir]\n";
                return false;
            }
            
            std::string project_name(args[0]);
            std::string language_str(args[1]);
            std::filesystem::path output_dir = (arg_count > 2) ? args[2] : std::filesystem::current_path();
            
            LanguageType lang = LanguageType::CPP;
            std::transform(language_str.begin(), language_str.end(), language_str.begin(), ::toupper);
            
            if (language_str == "C") lang = LanguageType::C;
            else if (language_str == "CPP" || language_str == "C++") lang = LanguageType::CPP;
            else if (language_str == "PYTHON") lang = LanguageType::PYTHON;
            else if (language_str == "RUST") lang = LanguageType::RUST;
            
            bool success = g_generator.GenerateWithCI(project_name, lang, output_dir);
            
            if (success) {
                std::cout << "✓ Generated " << project_name << " with CI/CD\n";
            } else {
                std::cout << "✗ Failed to generate project with CI\n";
            }
            
            return success;
        }
        else if (cmd == "/generate-with-docker") {
            if (arg_count < 2) {
                std::cout << "Usage: /generate-with-docker <project_name> <language> [output_dir]\n";
                return false;
            }
            
            std::string project_name(args[0]);
            std::string language_str(args[1]);
            std::filesystem::path output_dir = (arg_count > 2) ? args[2] : std::filesystem::current_path();
            
            LanguageType lang = LanguageType::CPP;
            std::transform(language_str.begin(), language_str.end(), language_str.begin(), ::toupper);
            
            if (language_str == "C") lang = LanguageType::C;
            else if (language_str == "CPP" || language_str == "C++") lang = LanguageType::CPP;
            else if (language_str == "PYTHON") lang = LanguageType::PYTHON;
            else if (language_str == "RUST") lang = LanguageType::RUST;
            else if (language_str == "JAVASCRIPT" || language_str == "JS") lang = LanguageType::JAVASCRIPT;
            
            bool success = g_generator.GenerateWithDocker(project_name, lang, output_dir);
            
            if (success) {
                std::cout << "✓ Generated " << project_name << " with Docker\n";
            } else {
                std::cout << "✗ Failed to generate project with Docker\n";
            }
            
            return success;
        }
        
        return false;
    }
    
    __declspec(dllexport) void OnConfigure(const char* config_json) {
        // Parse configuration JSON and update generator settings
        std::cout << "[Universal Generator] Configuration updated\n";
    }
}
