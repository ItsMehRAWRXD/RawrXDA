#pragma once
#include <vector>
#include <string>
#include <map>
#include <mutex>

/**
 * @file TemporalKernelVersioning.h
 * @brief Implements reversion support for kernels that fail real-world workload pressure.
 */

struct KernelVersion {
    std::string id;
    std::vector<uint8_t> binary;
    uint64_t cycleCount;
    uint64_t timestamp;
    bool holds_plateau;
};

class TemporalKernelVersioning {
public:
    static TemporalKernelVersioning& Instance() {
        static TemporalKernelVersioning instance;
        return instance;
    }

    void Archive(const std::string& kernelId, const std::vector<uint8_t>& binary, uint64_t cycles) {
        std::lock_guard<std::mutex> lock(m_mutex);
        KernelVersion kv = { kernelId, binary, cycles, GetNow(), true };
        m_history[kernelId].push_back(kv);
        
        // Keep only last 5 versions to prevent leak
        if (m_history[kernelId].size() > 5) {
            m_history[kernelId].erase(m_history[kernelId].begin());
        }
    }

    bool Revert(const std::string& kernelId, std::vector<uint8_t>& outBinary) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_history.count(kernelId) && m_history[kernelId].size() > 1) {
            // Remove current failed version
            m_history[kernelId].pop_back();
            // Get previous stable version
            outBinary = m_history[kernelId].back().binary;
            return true;
        }
        return false;
    }

private:
    uint64_t GetNow(); 
    TemporalKernelVersioning() = default;
    std::mutex m_mutex;
    std::map<std::string, std::vector<KernelVersion>> m_history;
};
