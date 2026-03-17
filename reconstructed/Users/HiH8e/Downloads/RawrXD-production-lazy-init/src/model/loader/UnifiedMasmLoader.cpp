#include "UnifiedMasmLoader.hpp"
#include "../utils/Logger.hpp"
#include "../utils/ErrorReporter.hpp"
#include <mutex>

using namespace MASM;

static std::mutex s_init_mutex;

UnifiedMasmLoader& UnifiedMasmLoader::getInstance() {
    static UnifiedMasmLoader instance;
    return instance;
}

UnifiedMasmLoader::~UnifiedMasmLoader() {
    shutdown();
}

bool UnifiedMasmLoader::initialize(LoaderType loaderType) {
    std::lock_guard<std::mutex> lk(s_init_mutex);
    if (m_initialized) return true;
    m_currentLoader = loaderType;
    m_status = LoaderStatus::Ready;
    m_initialized = true;
    Logger::info("UnifiedMasmLoader initialized");
    return true;
}

void* UnifiedMasmLoader::loadModel(const std::string& modelPath, bool /*async*/) {
    if (!m_initialized) {
        Logger::error("UnifiedMasmLoader::loadModel called before initialize");
        return nullptr;
    }

    Logger::info(std::string("UnifiedMasmLoader: loading model: ") + modelPath);
    // Minimal placeholder: real implementations should be added per loader type
    Logger::warn("UnifiedMasmLoader::loadModel: loader implementations not yet added; returning nullptr");
    return nullptr;
}

bool UnifiedMasmLoader::switchLoader(LoaderType /*newLoaderType*/) {
    Logger::warn("UnifiedMasmLoader::switchLoader: not implemented");
    return false;
}

void UnifiedMasmLoader::unloadModel(void* /*context*/) {
    Logger::info("UnifiedMasmLoader::unloadModel: noop (no context)");
}

void UnifiedMasmLoader::setActiveLayer(void* /*context*/, unsigned int layerIndex) {
    Logger::info(std::string("UnifiedMasmLoader::setActiveLayer: layer=") + std::to_string(layerIndex));
}

int UnifiedMasmLoader::ensureNoLag(void* /*context*/) {
    // Return 0 = resident
    return 0;
}

unsigned int UnifiedMasmLoader::getResidentCount(void* /*context*/) const {
    return 0;
}

void UnifiedMasmLoader::lockLayer(void* /*context*/, unsigned int /*layerIndex*/) {
    Logger::info("UnifiedMasmLoader::lockLayer: noop");
}

void UnifiedMasmLoader::unlockLayer(void* /*context*/, unsigned int /*layerIndex*/) {
    Logger::info("UnifiedMasmLoader::unlockLayer: noop");
}

LoaderStatus UnifiedMasmLoader::getStatus() const {
    return m_status.load();
}

const LoaderMetrics& UnifiedMasmLoader::getMetrics() const {
    // Return sliding window metrics as default
    return m_slidingWindowMetrics;
}

void UnifiedMasmLoader::updateMetrics(uint64_t /*tokens*/, double /*latencyMs*/, bool /*stalled*/) {
    // no-op for now
}

std::string UnifiedMasmLoader::getLoaderDescription(LoaderType /*type*/) const {
    return std::string("UnifiedMasmLoader: placeholder");
}

void UnifiedMasmLoader::shutdown() {
    if (!m_initialized) return;
    m_initialized = false;
    m_status = LoaderStatus::Uninitialized;
    Logger::info("UnifiedMasmLoader shutdown");
}
