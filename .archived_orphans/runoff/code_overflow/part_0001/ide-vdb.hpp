/**
 * @file ide-vdb.hpp
 * @brief IDE VDB / debug view integration (stub for patchable build).
 */
#pragma once

#include <vector>
#include <string>
#include <mutex>

struct VDBBreakpoint {
    std::string file;
    int line;
};

namespace ide_vdb {
inline std::vector<VDBBreakpoint>& breakpoints() { static std::vector<VDBBreakpoint> bps; return bps; }
inline std::mutex& mtx() { static std::mutex m; return m; }
inline void init() { std::lock_guard<std::mutex> lk(mtx()); breakpoints().clear(); }
inline void shutdown() { std::lock_guard<std::mutex> lk(mtx()); breakpoints().clear(); }
inline void setBreakpoint(const char* file, int line) {
    if (!file) return;
    std::lock_guard<std::mutex> lk(mtx());
    breakpoints().push_back({file, line});
}
inline void clearBreakpoints() { std::lock_guard<std::mutex> lk(mtx()); breakpoints().clear(); }
inline const std::vector<VDBBreakpoint>& getBreakpoints() { return breakpoints(); }
}
