#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include "hardware_provider_interface.h"

namespace RawrXD::Memory {

class HardwareTelemetryBridge {
public:
    void addProvider(std::shared_ptr<IHardwareProvider> provider) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_providers.push_back(provider);
    }

    HardwareMetrics pollAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        HardwareMetrics aggregate;
        for (auto& p : m_providers) {
            auto m = p->poll();
            aggregate.vramUsage += m.vramUsage;
            aggregate.vramTotal += m.vramTotal;
            aggregate.ramUsage += m.ramUsage;
            aggregate.ramTotal += m.ramTotal;
        }
        return aggregate;
    }

private:
    std::vector<std::shared_ptr<IHardwareProvider>> m_providers;
    std::mutex m_mutex;
};

}
