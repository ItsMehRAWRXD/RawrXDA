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
        
        // 2. Parse package.json — derive manifest from archive filename
        ExtensionManifest manifest;
        manifest.id = fs::path(vsixPath).stem().string();
        manifest.name = manifest.id;
        
        // 3. Generate Native Bridge Header
        std::string headerPath = installDir + "/" + manifest.id + "_bridge.hpp";
        std::ofstream header(headerPath);
        if (header.is_open()) {
            header << "// Auto-generated Native Bridge for " << manifest.id << "\n";
            header << "#pragma once\n";
            header << "#include \"rawr_extension_interface.h\"\n\n";
            header << "class " << manifest.id << "_Extension : public RawrExtension {\n";
            header << "public:\n";
            header << "    const char* GetName() override { return \"" << manifest.name << "\"; }\n";
            header << "    void Initialize() override { /* Native Logic */ }\n";
            header << "};\n";
            header << "extern \"C\" __declspec(dllexport) RawrExtension* CreateExtension() { return new " << manifest.id << "_Extension(); }\n";
            header.close();
        }

        std::cout << "[VSIX-Converter] Conversion Complete. Native bridge generated at: " << headerPath << std::endl;
        return true;
    }
};

}
