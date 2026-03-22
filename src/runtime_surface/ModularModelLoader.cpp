#include "rawrxd/runtime/ModularModelLoader.hpp"

#include "../logging/Logger.h"

namespace RawrXD::Runtime {

ModularModelLoader& ModularModelLoader::instance() {
    static ModularModelLoader s;
    return s;
}

ModularModelLoader::RegisterResult ModularModelLoader::registerBackendModule(const std::string& name,
                                                                           void* opaqueHandle) {
    if (name.empty()) {
        return std::unexpected("ModularModelLoader: empty backend name");
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_backends[name] = opaqueHandle;
    RawrXD::Logging::Logger::instance().info("[ModularModelLoader] backend registered: " + name,
                                             "RuntimeSurface");
    return {};
}

void ModularModelLoader::unregisterBackendModule(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_backends.erase(name);
}

std::expected<LayerLoadState, std::string> ModularModelLoader::layerState(const std::string& layerId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_layers.find(layerId);
    if (it == m_layers.end()) {
        return std::unexpected("ModularModelLoader: unknown layer " + layerId);
    }
    return it->second.state;
}

ModularModelLoader::RegisterResult ModularModelLoader::setLayerState(const std::string& layerId,
                                                                     LayerLoadState s) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_layers[layerId].state = s;
    RawrXD::Logging::Logger::instance().info(
        "[ModularModelLoader] layer " + layerId + " state=" + std::to_string(static_cast<int>(s)),
        "RuntimeSurface");
    return {};
}

void ModularModelLoader::setResidentLayerBudget(int n) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_residentBudget = n < 0 ? 0 : n;
    RawrXD::Logging::Logger::instance().info(
        "[ModularModelLoader] resident layer budget=" + std::to_string(m_residentBudget), "RuntimeSurface");
}

}  // namespace RawrXD::Runtime
