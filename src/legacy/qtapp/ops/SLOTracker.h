#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>

class SLOTracker {

public:
    struct SLO {
        double target{0.0}; // e.g., 99.9 for availability
        double current{0.0};
        int windowMinutes{1440};
    };

    SLOTracker() = default;

    void defineSLO(const std::string& service, double target, int windowMinutes);
    void recordSuccess(const std::string& service);
    void recordFailure(const std::string& service);
    double availability(const std::string& service) const;
    SLO slo(const std::string& service) const;

    void sloBreached(const std::string& service, double availability, double target);

private:
    void prune(const std::string& service, int64_t nowMs);
    double availabilityLocked(const std::string& service) const;

    struct WindowedCount {
        int successes{0};
        int failures{0};
        int64_t windowStart{0};
        int windowMinutes{1440};
    };

    mutable std::map<std::string, WindowedCount> m_counts;
    mutable std::map<std::string, SLO> m_slos;
    mutable std::mutex m_mutex;
};

