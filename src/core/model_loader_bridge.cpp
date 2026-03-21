// Win32IDE bridge implementations for legacy model-loader C exports.
// Functional path normalization + CPU inference engine integration.

#include <atomic>
#include <cctype>
#include <filesystem>
#include <mutex>
#include <string>

namespace RawrXD {
class CPUInferenceEngine {
public:
    static CPUInferenceEngine* getInstance();
    bool LoadModel(const std::string& model_path);
};
}

namespace {
std::mutex g_modelLoaderMutex;
std::atomic<bool> g_modelLoaderInitialized{false};
std::string g_loadedModelPath;

bool looksLikeWideString(const char* rawPath) {
    if (!rawPath || rawPath[0] == '\0') {
        return false;
    }
    if (rawPath[1] != '\0') {
        return false;
    }
    int pairCount = 0;
    int highZeroCount = 0;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(rawPath);
    for (int i = 0; i < 128; i += 2) {
        const unsigned char lo = p[i];
        const unsigned char hi = p[i + 1];
        if (lo == 0 && hi == 0) {
            break;
        }
        ++pairCount;
        if (hi == 0) {
            ++highZeroCount;
        }
    }
    return pairCount >= 2 && pairCount == highZeroCount;
}

std::string normalizeModelPath(const char* rawPath) {
    if (!rawPath) {
        return {};
    }
#ifdef _WIN32
    if (looksLikeWideString(rawPath)) {
        const wchar_t* widePath = reinterpret_cast<const wchar_t*>(rawPath);
        return std::filesystem::path(widePath).string();
    }
#endif
    return std::string(rawPath);
}

bool hasLikelyModelExtension(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();
    if (ext.empty()) {
        return true;
    }
    std::string lowered;
    lowered.reserve(ext.size());
    for (char ch : ext) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return lowered == ".gguf" || lowered == ".bin" || lowered == ".model";
}
}

extern "C" bool LoadModel(const char* path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    if (!g_modelLoaderInitialized.load(std::memory_order_acquire)) {
        return false;
    }

    const std::string normalized = normalizeModelPath(path);
    if (normalized.empty()) {
        return false;
    }

    const std::filesystem::path modelPath(normalized);
    std::error_code ec;
    const bool exists = std::filesystem::exists(modelPath, ec);
    if (ec || !exists || !std::filesystem::is_regular_file(modelPath, ec)) {
        return false;
    }
    if (ec || !hasLikelyModelExtension(modelPath)) {
        return false;
    }

    g_loadedModelPath = modelPath.string();
    auto* cpuEngine = RawrXD::CPUInferenceEngine::getInstance();
    if (!cpuEngine) {
        return false;
    }
    return cpuEngine->LoadModel(g_loadedModelPath);
}

extern "C" bool ModelLoaderInit(void) {
    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    g_modelLoaderInitialized.store(true, std::memory_order_release);
    return true;
}

extern "C" bool HotSwapModel(const char* model_id) {
    return LoadModel(model_id);
}

extern "C" bool ModelLoaderShutdown(void) {
    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    g_loadedModelPath.clear();
    g_modelLoaderInitialized.store(false, std::memory_order_release);
    return true;
}
