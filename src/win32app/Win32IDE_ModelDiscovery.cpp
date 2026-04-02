// ============================================================================
// Win32IDE_ModelDiscovery.cpp — Model Discovery Implementation
// ============================================================================
// Provides automatic discovery of AI models:
//   - File system scanning for model files
//   - Path management
//   - Model enumeration
//
// ============================================================================

#include "Win32IDE.h"
#include <filesystem>
#include <algorithm>

// ============================================================================
// CONSTANTS
// ============================================================================
static const std::vector<std::string> DEFAULT_MODEL_PATHS = {
    "F:\\OllamaModels",
    "C:\\Users\\Public\\Models",
    "D:\\Models",
    "E:\\Models"
};

static const std::vector<std::string> MODEL_EXTENSIONS = {
    ".gguf", ".bin", ".safetensors", ".ckpt"
};

// ============================================================================
// MODEL DISCOVERY METHODS
// ============================================================================

void Win32IDE::initModelDiscovery() {
    m_modelDiscoveryEnabled = true;
    m_modelDiscoveryPaths = DEFAULT_MODEL_PATHS;
    m_availableModels.clear();
    m_modelPaths.clear();

    // Check for custom models path from environment variable
    const char* customModelsPath = std::getenv("RAWRXD_MODELS_PATH");
    if (customModelsPath && strlen(customModelsPath) > 0) {
        m_modelDiscoveryPaths.insert(m_modelDiscoveryPaths.begin(), customModelsPath);
    }

    // Initial scan
    scanForModels();

    LOG_INFO("Model discovery initialized");
}

void Win32IDE::shutdownModelDiscovery() {
    m_modelDiscoveryEnabled = false;
    m_availableModels.clear();
    m_modelPaths.clear();
}

void Win32IDE::scanForModels() {
    if (!m_modelDiscoveryEnabled) {
        return;
    }

    m_availableModels.clear();
    m_modelPaths.clear();

    for (const auto& basePath : m_modelDiscoveryPaths) {
        try {
            if (!std::filesystem::exists(basePath)) {
                continue;
            }

            for (const auto& entry : std::filesystem::recursive_directory_iterator(basePath)) {
                if (entry.is_regular_file()) {
                    std::string extension = entry.path().extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                    if (std::find(MODEL_EXTENSIONS.begin(), MODEL_EXTENSIONS.end(), extension) != MODEL_EXTENSIONS.end()) {
                        std::string modelName = entry.path().filename().string();
                        std::string modelPath = entry.path().string();

                        m_availableModels.push_back(modelName);
                        m_modelPaths.push_back(modelPath);
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error&) {
            // Skip directories we can't access
            continue;
        }
    }

    LOG_INFO(std::string("Model discovery completed. Found ") + std::to_string(m_availableModels.size()) + " models");
}

std::vector<std::string> Win32IDE::getAvailableModels() const {
    return m_availableModels;
}

std::vector<std::string> Win32IDE::getModelPaths() const {
    return m_modelPaths;
}

bool Win32IDE::isModelDiscoveryEnabled() const {
    return m_modelDiscoveryEnabled;
}

void Win32IDE::setModelDiscoveryPaths(const std::vector<std::string>& paths) {
    m_modelDiscoveryPaths = paths;
    // Re-scan with new paths
    scanForModels();
}

std::vector<std::string> Win32IDE::getModelDiscoveryPaths() const {
    return m_modelDiscoveryPaths;
}