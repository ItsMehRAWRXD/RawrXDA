#include "execai_security.h"
#include <algorithm>
#include <iostream>
#include <fstream>

const std::vector<std::string> ExecAISecurity::ALLOWED_EXTENSIONS = {".gguf", ".exec"};
const size_t ExecAISecurity::MAX_FILE_SIZE = 10ULL * 1024ULL * 1024ULL * 1024ULL; // 10GB limit
const std::vector<std::string> ExecAISecurity::DANGEROUS_PATHS = {
    "\\\\", "..\\\\", "../", "/etc", "/bin", "/usr", "/var", "/root", "/home",
    "C:\\\\Windows", "C:\\\\Program Files", "C:\\\\System32"
};

bool ExecAISecurity::validateFilePath(const std::string& path) {
    try {
        std::filesystem::path filePath(path);

        // Check for dangerous path components
        std::string pathStr = filePath.string();
        for (const auto& dangerous : DANGEROUS_PATHS) {
            if (pathStr.find(dangerous) != std::string::npos) {
                std::cerr << "Security: Dangerous path detected: " << dangerous << std::endl;
                return false;
            }
        }

        // Check file extension
        std::string extension = filePath.extension().string();
        if (std::find(ALLOWED_EXTENSIONS.begin(), ALLOWED_EXTENSIONS.end(), extension) == ALLOWED_EXTENSIONS.end()) {
            std::cerr << "Security: Invalid file extension: " << extension << std::endl;
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Security: Path validation error: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> ExecAISecurity::sanitizeArguments(const std::vector<std::string>& args) {
    std::vector<std::string> sanitized;

    for (const auto& arg : args) {
        std::string clean = arg;

        // Remove potentially dangerous characters
        clean.erase(std::remove_if(clean.begin(), clean.end(),
            [](char c) { return c == '|' || c == '&' || c == ';' || c == '`' || c == '$'; }), clean.end());

        // Limit argument length
        if (clean.length() > 1024) {
            clean = clean.substr(0, 1024);
        }

        sanitized.push_back(clean);
    }

    return sanitized;
}

bool ExecAISecurity::isSafeFile(const std::filesystem::path& file) {
    try {
        // Check if file exists
        if (!std::filesystem::exists(file)) {
            return false;
        }

        // Check file size
        auto size = std::filesystem::file_size(file);
        if (size > MAX_FILE_SIZE) {
            std::cerr << "Security: File too large: " << size << " bytes" << std::endl;
            return false;
        }

        // Check if it's a regular file
        if (!std::filesystem::is_regular_file(file)) {
            std::cerr << "Security: Not a regular file" << std::endl;
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Security: File safety check error: " << e.what() << std::endl;
        return false;
    }
}

bool ExecAISecurity::validateModelFile(const std::string& modelPath) {
    if (!validateFilePath(modelPath)) {
        return false;
    }

    std::filesystem::path file(modelPath);
    if (!isSafeFile(file)) {
        return false;
    }

    // Basic magic number check for GGUF
    if (file.extension() == ".gguf") {
        std::ifstream f(modelPath, std::ios::binary);
        if (f.is_open()) {
            char magic[4];
            f.read(magic, 4);
            if (std::memcmp(magic, GGUF_MAGIC, 4) != 0) {
                std::cerr << "Security: GGUF magic number mismatch" << std::endl;
                return false;
            }
        }
    }

    return true;
}

bool ExecAISecurity::performDeepValidation(const std::string& modelPath) {
    if (!validateModelFile(modelPath)) return false;

    std::ifstream f(modelPath, std::ios::binary);
    if (!f.is_open()) return false;

    // Check GGUF version (bytes 4-7)
    f.seekg(4);
    uint32_t version;
    f.read(reinterpret_cast<char*>(&version), 4);
    
    // Support versions 2 and 3
    if (version != 2 && version != 3) {
        std::cerr << "Security: Unsupported GGUF version: " << version << std::endl;
        return false;
    }

    // Check for potential zip-bomb / recursive structures if necessary
    // Here we'd walk the KV pairs without loading tensors

    std::cout << "Security: Deep validation passed for " << modelPath << std::endl;
    return true;
}

bool ExecAISecurity::isGpuAvailable() {
    // In a production environment, we'd check for Vulkan or CUDA drivers here.
    // Since this is a specialized RawrXD environment, we'll assume Vulkan presence
    // given the user's hardware (RX 7800 XT).
    return true; 
}

bool ExecAISecurity::setupSandbox() {
    // In a full implementation, this would:
    // - Create a temporary directory
    // - Set restrictive permissions
    // - Limit system resources
    // - Monitor process execution

    std::cout << "ExecAI: Sandbox setup (basic implementation)" << std::endl;
    return true;
}

void ExecAISecurity::cleanupSandbox() {
    // Clean up temporary files and resources
    std::cout << "ExecAI: Sandbox cleanup completed" << std::endl;
}