#include <atomic>

namespace {
std::atomic<unsigned int> g_win32ide_beacon_full{0};
}

bool isBeaconFullActive() {
    return g_win32ide_beacon_full.load(std::memory_order_relaxed) != 0u;
}
