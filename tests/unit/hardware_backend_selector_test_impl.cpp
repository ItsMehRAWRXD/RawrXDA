#include "hardware_backend_selector.h"

void HardwareBackendSelector::initialize() {
}

std::vector<HardwareBackendSelector::BackendInfo> HardwareBackendSelector::detectBackends() const {
    BackendInfo cpuBackend;
    cpuBackend.backend = Backend::CPU;
    cpuBackend.name = "CPU";
    cpuBackend.version = "builtin";
    cpuBackend.available = true;
    cpuBackend.deviceName = "Host CPU";
    cpuBackend.computeCapability = "native";
    cpuBackend.supportsFP16 = true;
    cpuBackend.supportsInt8 = true;
    cpuBackend.details = "Test fallback backend";
    return {cpuBackend};
}

void HardwareBackendSelector::showModal(void* parent) {
    m_dialogHandle = parent;
    if (m_onBackendSelected) {
        const auto backends = detectBackends();
        if (!backends.empty()) {
            m_onBackendSelected(backends.front().backend, backends.front());
        }
    }
}
