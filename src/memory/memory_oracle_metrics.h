#pragma once
#include <mutex>
#include <memory>
#include <cstdint>
namespace RawrXD::Memory {
struct OracleAggregateMetrics {
    double pressure;
    uint64_t vram_used;
    uint64_t ram_used;
    double weights[3];
};
class MemoryOracleMetrics {
public:
    void update(double p, uint64_t v, uint64_t r, const double w[3]) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_p = p; m_v = v; m_r = r;
        m_w[0] = w[0]; m_w[1] = w[1]; m_w[2] = w[2];
    }
    OracleAggregateMetrics getSnapshot() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        OracleAggregateMetrics res;
        res.pressure = m_p;
        res.vram_used = m_v;
        res.ram_used = m_r;
        res.weights[0] = m_w[0];
        res.weights[1] = m_w[1];
        res.weights[2] = m_w[2];
        return res;
    }
private:
    mutable std::mutex m_mutex;
    double m_p = 0;
    uint64_t m_v = 0, m_r = 0;
    double m_w[3] = {0.33, 0.33, 0.33};
};
} // namespace RawrXD::Memory
