#include "self_patch.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <windows.h>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

SelfPatch::SelfPatch() {}

bool SelfPatch::addKernel(const std::string& name, const std::string& templateName) {
    std::string tplPath = "kernels/" + templateName + ".comp";
    std::string outPath = "kernels/" + name + ".comp";
    
    if (fs::exists(outPath)) {
        std::cout << "Kernel " << name << " already exists" << std::endl;
        return true;
    }
    
    if (!fs::exists(tplPath)) {
        std::cerr << "Template not found: " << tplPath << std::endl;
        return false;
    }
    
    std::error_code ec;
    if (!fs::copy_file(tplPath, outPath, fs::copy_options::overwrite_existing, ec)) {
        std::cerr << "Failed to copy template: " << ec.message() << std::endl;
        return false;
    }
    
    // Inject compile command into CMakeLists.txt
    std::ifstream cmakeIn("CMakeLists.txt");
    if (!cmakeIn.is_open()) return false;
    
    std::stringstream buffer;
    buffer << cmakeIn.rdbuf();
    std::string txt = buffer.str();
    cmakeIn.close();
    
    std::string cmd = "\n# Auto-generated shader compilation for " + name + "\n" +
        "add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/" + name + ".comp.spv.h\n" +
        "    COMMAND glslangValidator -V kernels/" + name + ".comp -o ${CMAKE_CURRENT_BINARY_DIR}/tmp_" + name + ".spv\n" +
        "    COMMAND xxd -i tmp_" + name + ".spv > ${CMAKE_CURRENT_BINARY_DIR}/" + name + ".comp.spv.h\n" +
        "    DEPENDS kernels/" + name + ".comp\n" +
        "    COMMENT \"Building " + name + " shader\"\n" +
        "    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}\n" +
        ")\n" +
        "add_custom_target(" + name + "_spv DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/" + name + ".comp.spv.h)\n";
    
    txt += cmd;
    
    std::ofstream cmakeOut("CMakeLists.txt");
    if (!cmakeOut.is_open()) return false;
    cmakeOut << txt;
    cmakeOut.close();
    
    return true;
}

bool SelfPatch::addCpp(const std::string& name, const std::string& deps) {
    std::string hppPath = "src/gpu/" + name + ".hpp";
    std::string cppPath = "src/gpu/" + name + ".cpp";
    
    if (fs::exists(cppPath)) return true;
    
    fs::create_directories("src/gpu");
    
    std::ofstream hppFile(hppPath);
    if (!hppFile.is_open()) return false;
    
    hppFile << "#pragma once\n"
            << "#include <vector>\n"
            << "#include <cstddef>\n\n"
            << "class " << name << " {\n"
            << "public:\n"
            << "    static std::vector<uint8_t> wrap(const float* src, size_t n);\n"
            << "    static void initialize();\n"
            << "    static void cleanup();\n"
            << "};\n";
    hppFile.close();
    
    std::ofstream cppFile(cppPath);
    if (!cppFile.is_open()) return false;
    
    cppFile << "#include \"" << name << ".hpp\"\n"
            << "#include <vulkan/vulkan.h>\n"
            << "#include <iostream>\n"
            << "#include <cstring>\n\n"
            << "extern \"C\" const unsigned char " << deps << "_comp_spv[];\n"
            << "extern \"C\" const unsigned int " << deps << "_comp_spv_len;\n\n"
            << "static VkDevice s_device = VK_NULL_HANDLE;\n"
            << "static VkShaderModule s_shader = VK_NULL_HANDLE;\n\n"
            << "void " << name << "::initialize() { std::cout << \"Initializing " << name << "\" << std::endl; }\n"
            << "void " << name << "::cleanup() {}\n\n"
            << "std::vector<uint8_t> " << name << "::wrap(const float* src, size_t n) {\n"
            << "    std::vector<uint8_t> result(n * sizeof(float));\n"
            << "    std::memcpy(result.data(), src, result.size());\n"
            << "    return result;\n"
            << "}\n";
    cppFile.close();
    
    return true;
}

bool SelfPatch::hotReload() {
    std::cout << "Starting rebuild..." << std::endl;
    
    // Step 1: Build the project
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char command[] = "cmake --build build --config Release --target RawrXD-QtShell";
    
    if (CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 120000); // 2 min
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        if (exitCode != 0) {
            std::cerr << "Build failed with exit code " << exitCode << std::endl;
            return false;
        }
    } else {
        std::cerr << "Failed to start build process: " << GetLastError() << std::endl;
        return false;
    }
    
    std::cout << "Build successful. Restarting..." << std::endl;
    
    // Step 2: Spawn new process
    char binPath[MAX_PATH];
    GetModuleFileNameA(NULL, binPath, MAX_PATH);
    
    STARTUPINFOA si2 = { sizeof(si2) };
    PROCESS_INFORMATION pi2;
    if (CreateProcessA(binPath, NULL, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &si2, &pi2)) {
        CloseHandle(pi2.hProcess);
        CloseHandle(pi2.hThread);
        
        // Step 3: Suicide
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "Exiting old process for hot-reload" << std::endl;
        std::exit(0);
    }
    
    return false;
}

bool SelfPatch::patchFile(const std::string& filename, const std::string& patch) {
    std::ifstream fileIn(filename);
    if (!fileIn.is_open()) return false;
    
    std::stringstream buffer;
    buffer << fileIn.rdbuf();
    std::string content = buffer.str();
    fileIn.close();
    
    if (patch.substr(0, 7) == "APPEND:") {
        content += patch.substr(7);
    } else if (patch.substr(0, 8) == "REPLACE:") {
        std::string payload = patch.substr(8);
        size_t arrow = payload.find("->");
        if (arrow != std::string::npos) {
            std::string oldText = payload.substr(0, arrow);
            std::string newText = payload.substr(arrow + 2);
            size_t pos = 0;
            while ((pos = content.find(oldText, pos)) != std::string::npos) {
                content.replace(pos, oldText.length(), newText);
                pos += newText.length();
            }
        }
    } else {
        content += "\n" + patch;
    }
    
    std::ofstream fileOut(filename, std::ios::trunc);
    if (!fileOut.is_open()) return false;
    fileOut << content;
    fileOut.close();
    
    return true;
}
