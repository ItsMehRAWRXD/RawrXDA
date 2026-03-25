#include <atomic>

bool isBeaconFullActive() {
    static std::atomic<bool> s_full{false};
    return s_full.load(std::memory_order_relaxed);
}
