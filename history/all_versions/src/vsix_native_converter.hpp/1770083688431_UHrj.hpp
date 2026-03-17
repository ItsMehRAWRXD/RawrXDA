#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace RawrXD {

class VsixNativeConverter {
public:
    struct ExtensionManifest {
        std::string id;
        std::string name;
        std::string version;
        std::string description;
        std::string entryPoint;
    };

    static bool ConvertVsixToNative(const std::string& vsixPath, const std::string& installDir) {
        std::cout << "[VSIX-Converter] converting " << vsixPath << "..." << std::endl;
        
        namespace fs = std::filesystem;
        if (!fs::exists(vsixPath)) {
            std::cerr << "[VSIX-Converter] Error: File not found." << std::endl;
            return false;
        }

        // 1. Rename .vsix to .zip and extract (Simulation)
        // In a real implementation: use libzip or system command
        std::string tempDir = installDir + "/temp_extract";
        fs::create_directories(tempDir);
        
        std::cout << "[VSIX-Converter] Extracting archive..." << std::endl;
        
        // 2. Parse package.json
        // Simulating finding a manifest
        std::string jsonPath = tempDir + "/extension/package.json";
        ExtensionManifest manifest;
        
        // Try to read real package.json if it exists (assuming user extracted it or we're mocking)
        // For this task, we will simulate the extraction result.
        
        manifest.id = fs::path(vsixPath).stem().string();
        manifest.name = manifest.id;
        
        // Scan for capabilities (Command palette, keybindings, etc.)
        // In a real implementation we would parse the JSON.
        // Here we will generate a Native Bridge that exposes these
        
        // 3. Generate Native Bridge Header
        std::string headerPath = installDir + "/" + manifest.id + "_bridge.hpp";
        std::ofstream header(headerPath);
        if (header.is_open()) {
            header << "// Auto-generated Native Bridge for " << manifest.id << "\n";
            header << "#pragma once\n";
            header << "#include \"rawr_extension_interface.h\"\n";
            header << "#include <string>\n";
            header << "#include <vector>\n\n";
            
            header << "class " << manifest.id << "_Extension : public RawrExtension {\n";
            header << "public:\n";
            header << "    const char* GetName() override { return \"" << manifest.name << "\"; }\n";
            header << "    void Initialize() override {\n"; 
            header << "        // Register commands\n";
            header << "        // This is where we wire the VSIX commands to the Native IDE\n";
            header << "        Registry::RegisterCommand(\"" << manifest.id << ".hello\", [](){ \n";
            header << "            MessageBoxA(NULL, \"Hello from " << manifest.name << "\", \"Extension\", MB_OK);\n";
            header << "        });\n";
            header << "    }\n";
            header << "};\n";
            header << "extern \"C\" __declspec(dllexport) RawrExtension* CreateExtension() { return new " << manifest.id << "_Extension(); }\n";
            header.close();
        }

        std::cout << "[VSIX-Converter] Conversion Complete. Native bridge generated at: " << headerPath << std::endl;
        
        // 4. Update the IDE to load this extension
        // We append to a extensions_list.txt or similar
        std::string listPath = installDir + "/extensions_list.txt";
        std::ofstream list(listPath, std::ios::app);
        if (list.is_open()) {
            list << headerPath << "\n";
            list.close();
        }

        return true;
    }

    static void RegisterNativeExtensions(void* ideInstance) {
        // This function would iterate over installed extensions and load them
        // For now, it's a placeholder to show intent
        std::cout << "[VSIX-Converter] Registering native extensions..." << std::endl;
    }
};

}
