#include "hardware_provider_interface.h"

namespace RawrXD::Memory {

class SyntheticHardwareProvider : public IHardwareProvider {
public:
    SyntheticHardwareProvider() = default;

    bool initialize() override { return true; }

    void setMockVram(uint64_t used, uint64_t total) {
        m_mockUsed = used;
        m_mockTotal = total;
    }

    HardwareMetrics sample() override {
        HardwareMetrics m{};
        m.vramUsedBytes = m_mockUsed;
        m.vramTotalBytes = m_mockTotal;
        return m;
    }

    std::string getProviderName() const override { return "Synthetic/Mock"; }

private:
    uint64_t m_mockUsed = 0;
    uint64_t m_mockTotal = 8ULL << 30;
};

} // namespace RawrXD::Memory
