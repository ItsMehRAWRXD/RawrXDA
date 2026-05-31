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
#include "model_puller/model_puller.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>


namespace
{
std::string normalizeModelPath(const std::string& rawPath)
{
    if (rawPath.empty())
    {
        return rawPath;
    }

    const size_t first = rawPath.find_first_not_of(" \t\r\n\"");
    if (first == std::string::npos)
    {
        return {};
    }

    const size_t last = rawPath.find_last_not_of(" \t\r\n\"");
    std::string out = rawPath.substr(first, last - first + 1);
    std::replace(out.begin(), out.end(), '/', '\\');
    return out;
}

void traceDiscoveryPath(const std::string& path, bool exists)
{
    const std::string msg = "[ModelDiscovery.cpp] scan path='" + path + "' exists=" + (exists ? "1" : "0") + "\n";
    OutputDebugStringA(msg.c_str());
}
}  // namespace

// ============================================================================
// CONSTANTS
// ============================================================================
#include "Win32IDE_PathResolver.h"

static const std::vector<std::string> DEFAULT_MODEL_PATHS = {"F:\\OllamaModels", "C:\\Users\\Public\\Models",
                                                             "D:\\Models", "E:\\Models",
                                                             RawrXDPathResolver::GetUserDocumentsPath() + "\\Models",
                                                             RawrXDPathResolver::GetAppDataPath() + "\\RawrXD\\Models"};

static const std::vector<std::string> MODEL_EXTENSIONS = {".gguf", ".bin", ".safetensors", ".ckpt"};

// ============================================================================
// MODEL DISCOVERY METHODS
// ============================================================================

void Win32IDE::initModelDiscovery()
{
    m_modelDiscoveryEnabled = true;
    m_modelDiscoveryPaths = DEFAULT_MODEL_PATHS;
    {
        std::lock_guard<std::mutex> modelListLock(m_availableModelsMutex);
        m_availableModels.clear();
    }
    m_modelPaths.clear();

    // Check for custom models path from environment variable
    const char* customModelsPath = std::getenv("RAWRXD_MODELS_PATH");
    if (customModelsPath && strlen(customModelsPath) > 0 && strlen(customModelsPath) <= 1024)
    {
        m_modelDiscoveryPaths.insert(m_modelDiscoveryPaths.begin(), normalizeModelPath(customModelsPath));
    }

    const char* ollamaModelsPath = std::getenv("OLLAMA_MODELS");
    if (ollamaModelsPath && strlen(ollamaModelsPath) > 0 && strlen(ollamaModelsPath) <= 2048)
    {
        m_modelDiscoveryPaths.insert(m_modelDiscoveryPaths.begin(), normalizeModelPath(ollamaModelsPath));
    }

    // Add Model Puller's managed models directory
    try
    {
        auto& puller = RawrXD::ModelPuller::Instance();
        std::string pullerBase = puller.GetIndex().GetModelsBasePath();
        if (!pullerBase.empty())
        {
            m_modelDiscoveryPaths.insert(m_modelDiscoveryPaths.begin(), normalizeModelPath(pullerBase));
        }
    }
    catch (...)
    {
        // Model Puller not initialized yet — skip
    }

    // Initial scan
    scanForModels();

    LOG_INFO("Model discovery initialized");
}

void Win32IDE::shutdownModelDiscovery()
{
    m_modelDiscoveryEnabled = false;
    {
        std::lock_guard<std::mutex> modelListLock(m_availableModelsMutex);
        m_availableModels.clear();
    }
    m_modelPaths.clear();
}

void Win32IDE::scanForModels()
{
    if (!m_modelDiscoveryEnabled)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> modelListLock(m_availableModelsMutex);
        m_availableModels.clear();
    }
    m_modelPaths.clear();

    static constexpr size_t kMaxDiscoveredModels = 50000;
    static constexpr int kMaxDiscoveryDepth = 12;

    auto scanPass = [this]()
    {
        std::lock_guard<std::mutex> modelListLock(m_availableModelsMutex);
        for (const auto& rawBasePath : m_modelDiscoveryPaths)
        {
            try
            {
                const std::string basePath = normalizeModelPath(rawBasePath);
                std::error_code existsEc;
                const bool exists = !basePath.empty() && std::filesystem::exists(basePath, existsEc);
                traceDiscoveryPath(basePath, exists);
                if (!exists)
                {
                    continue;
                }

                std::filesystem::recursive_directory_iterator it(
                    basePath, std::filesystem::directory_options::skip_permission_denied);
                std::filesystem::recursive_directory_iterator end;
                for (; it != end; ++it)
                {
                    if (it.depth() > kMaxDiscoveryDepth)
                    {
                        it.disable_recursion_pending();
                        continue;
                    }
                    const auto& entry = *it;
                    if (entry.is_regular_file())
                    {
                        std::string extension = entry.path().extension().string();
                        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                        if (std::find(MODEL_EXTENSIONS.begin(), MODEL_EXTENSIONS.end(), extension) !=
                            MODEL_EXTENSIONS.end())
                        {
                            std::string modelName = entry.path().filename().string();
                            std::string modelPath = entry.path().string();

                            m_availableModels.push_back(modelName);
                            m_modelPaths.push_back(modelPath);
                            if (m_availableModels.size() >= kMaxDiscoveredModels)
                            {
                                LOG_INFO("Model discovery capped at maximum entries");
                                return;
                            }
                        }
                    }
                }
            }
            catch (const std::filesystem::filesystem_error&)
            {
                // Skip directories we can't access
                continue;
            }
        }
    };

    scanPass();
    bool discoveredAny = false;
    {
        std::lock_guard<std::mutex> modelListLock(m_availableModelsMutex);
        discoveredAny = !m_availableModels.empty();
    }
    if (!discoveredAny)
    {
        OutputDebugStringA("[ModelDiscovery.cpp] first pass empty, retrying after 500ms\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        scanPass();
    }

    size_t discoveredCount = 0;
    {
        std::lock_guard<std::mutex> modelListLock(m_availableModelsMutex);
        discoveredCount = m_availableModels.size();
    }
    LOG_INFO(std::string("Model discovery completed. Found ") + std::to_string(discoveredCount) + " models");
}

std::vector<std::string> Win32IDE::getAvailableModels() const
{
    std::lock_guard<std::mutex> modelListLock(m_availableModelsMutex);
    return m_availableModels;
}

std::vector<std::string> Win32IDE::getModelPaths() const
{
    return m_modelPaths;
}

bool Win32IDE::isModelDiscoveryEnabled() const
{
    return m_modelDiscoveryEnabled;
}

void Win32IDE::setModelDiscoveryPaths(const std::vector<std::string>& paths)
{
    m_modelDiscoveryPaths = paths;
    // Re-scan with new paths
    scanForModels();
}

std::vector<std::string> Win32IDE::getModelDiscoveryPaths() const
{
    return m_modelDiscoveryPaths;
}
