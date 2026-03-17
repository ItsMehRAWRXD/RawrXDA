#include <string>
#include <filesystem>
#include <iostream>

// CoreGenerator stub implementation
class CoreGenerator {
public:
    static CoreGenerator& GetInstance() {
        static CoreGenerator instance;
        return instance;
    }
    
    bool GenerateGame(const std::string& name, const std::filesystem::path& output_dir) {
        std::cout << "[CoreGenerator] Generating game: " << name << std::endl;
        return true;
    }
    
    bool GenerateWithAllFeatures(const std::string& code, int language, const std::filesystem::path& output_dir) {
        std::cout << "[CoreGenerator] Generating with all features\n";
        return true;
    }
    
    bool GenerateCLI(const std::string& code, const std::filesystem::path& output_dir) {
        std::cout << "[CoreGenerator] Generating CLI\n";
        return true;
    }
};
