#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

namespace RawrXD::Core {

struct DebugContext {
    uint64_t rip = 0;
    uint64_t rax = 0;
    uint64_t rbx = 0;
    uint64_t rcx = 0;
    uint64_t rdx = 0;
};

struct ConditionalBreakpoint {
    uint64_t address = 0;
    std::string condition;
    std::string logMessage;
    bool logOnly = false;
    bool enabled = true;
};

class ConditionalBreakpointManager {
public:
    bool add(const ConditionalBreakpoint& bp) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = std::find_if(bps_.begin(), bps_.end(), [&](const auto& b) { return b.address == bp.address; });
        if (it != bps_.end()) {
            *it = bp;
            return true;
        }
        bps_.push_back(bp);
        return true;
    }

    bool remove(uint64_t address) {
        std::lock_guard<std::mutex> lock(mu_);
        auto sizeBefore = bps_.size();
        bps_.erase(std::remove_if(bps_.begin(), bps_.end(), [&](const auto& b) { return b.address == address; }), bps_.end());
        return bps_.size() != sizeBefore;
    }

    bool evaluate(uint64_t address, const DebugContext& ctx, bool* shouldBreak, bool* logged) {
        if (shouldBreak) *shouldBreak = false;
        if (logged) *logged = false;

        std::lock_guard<std::mutex> lock(mu_);
        auto it = std::find_if(bps_.begin(), bps_.end(), [&](const auto& b) { return b.address == address && b.enabled; });
        if (it == bps_.end()) return false;

        bool cond = evaluateCondition(it->condition, ctx);
        if (!cond) return true;

        if (it->logOnly) {
            if (logged) *logged = true;
            return true;
        }

        if (shouldBreak) *shouldBreak = true;
        return true;
    }

private:
    static bool evaluateCondition(const std::string& expr, const DebugContext& c) {
        if (expr.empty()) return true;

        if (expr.rfind("rip==", 0) == 0) {
            return c.rip == parseU64(expr.substr(5));
        }
        if (expr.rfind("rax==", 0) == 0) {
            return c.rax == parseU64(expr.substr(5));
        }
        if (expr.rfind("rbx==", 0) == 0) {
            return c.rbx == parseU64(expr.substr(5));
        }
        if (expr.rfind("rcx==", 0) == 0) {
            return c.rcx == parseU64(expr.substr(5));
        }
        if (expr.rfind("rdx==", 0) == 0) {
            return c.rdx == parseU64(expr.substr(5));
        }

        return false;
    }

    static uint64_t parseU64(const std::string& s) {
        try {
            if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
                return std::stoull(s, nullptr, 16);
            }
            return std::stoull(s, nullptr, 10);
        } catch (...) {
            return 0;
        }
    }

    std::mutex mu_;
    std::vector<ConditionalBreakpoint> bps_;
};

static ConditionalBreakpointManager g_condBp;

} // namespace RawrXD::Core

extern "C" {

bool RawrXD_Debugger_AddConditionalBreakpoint(uint64_t address, const char* condition, const char* logMessage, bool logOnly) {
    RawrXD::Core::ConditionalBreakpoint bp;
    bp.address = address;
    bp.condition = condition ? condition : "";
    bp.logMessage = logMessage ? logMessage : "";
    bp.logOnly = logOnly;
    bp.enabled = true;
    return RawrXD::Core::g_condBp.add(bp);
}

bool RawrXD_Debugger_RemoveConditionalBreakpoint(uint64_t address) {
    return RawrXD::Core::g_condBp.remove(address);
}

bool RawrXD_Debugger_EvaluateConditionalBreakpoint(uint64_t address, uint64_t rip, uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, bool* shouldBreak, bool* logged) {
    RawrXD::Core::DebugContext ctx{rip, rax, rbx, rcx, rdx};
    return RawrXD::Core::g_condBp.evaluate(address, ctx, shouldBreak, logged);
}

}
