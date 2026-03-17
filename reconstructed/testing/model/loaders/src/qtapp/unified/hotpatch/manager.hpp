// unified_hotpatch_manager.hpp — Pure C++17 hotpatch manager
// Converted from Qt (QObject, signals) to plain C++17 with CallbackList

#pragma once

#include <string>
#include <functional>
#include <vector>
#include <mutex>
#include <cstdint>

struct PatchResult {
    bool success;
    std::string detail;
    int errorCode;

    static PatchResult ok(const std::string& msg = "Applied") {
        return {true, msg, 0};
    }
    static PatchResult error(const std::string& msg, int code = -1) {
        return {false, msg, code};
    }
};

class UnifiedHotpatchManager {
public:
    using PatchCallback = std::function<void(const PatchResult&)>;

    UnifiedHotpatchManager() = default;

    PatchResult performHotpatch(const std::string& patchName,
                                const std::string& targetRegion,
                                const std::vector<uint8_t>& patchData) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (patchName.empty()) {
            auto r = PatchResult::error("Empty patch name", 1);
            notifyResult(r);
            return r;
        }

        if (targetRegion.empty()) {
            auto r = PatchResult::error("Empty target region", 2);
            notifyResult(r);
            return r;
        }

        // Simulate hotpatch application
        m_patchCount++;
        auto r = PatchResult::ok("Hotpatch '" + patchName + "' applied to region '" + targetRegion + "'");
        notifyResult(r);
        return r;
    }

    int patchCount() const { return m_patchCount; }

    void onPatchResult(PatchCallback cb) {
        std::lock_guard<std::mutex> lock(m_cbMutex);
        m_callbacks.push_back(std::move(cb));
    }

private:
    void notifyResult(const PatchResult& result) {
        std::lock_guard<std::mutex> lock(m_cbMutex);
        for (auto& cb : m_callbacks) {
            if (cb) cb(result);
        }
    }

    std::mutex m_mutex;
    std::mutex m_cbMutex;
    int m_patchCount = 0;
    std::vector<PatchCallback> m_callbacks;
};
