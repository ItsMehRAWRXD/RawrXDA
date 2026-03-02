#include <string>
#include <vector>
#include <mutex>
#include <sstream>

extern "C" {
bool RawrXD_Debugger_AddConditionalBreakpoint(unsigned long long address, const char* condition, const char* logMessage, bool logOnly);
bool RawrXD_Debugger_EvaluateConditionalBreakpoint(unsigned long long address, unsigned long long rip, unsigned long long rax, unsigned long long rbx, unsigned long long rcx, unsigned long long rdx, bool* shouldBreak, bool* logged);
}

namespace RawrXD::IDE {

struct Logpoint {
    std::string file;
    int line = 0;
    unsigned long long address = 0;
    std::string condition;
    std::string format;
    bool enabled = true;
};

class LogpointBridge {
public:
    bool add(const Logpoint& lp) {
        std::lock_guard<std::mutex> lock(mu_);
        if (lp.address == 0) return false;
        if (!RawrXD_Debugger_AddConditionalBreakpoint(lp.address, lp.condition.c_str(), lp.format.c_str(), true)) {
            return false;
        }
        points_.push_back(lp);
        return true;
    }

    bool onBreakpointHit(unsigned long long address,
                         unsigned long long rip,
                         unsigned long long rax,
                         unsigned long long rbx,
                         unsigned long long rcx,
                         unsigned long long rdx,
                         bool* shouldPause,
                         const char** outLogLine) {
        bool shouldBreak = false;
        bool logged = false;
        if (!RawrXD_Debugger_EvaluateConditionalBreakpoint(address, rip, rax, rbx, rcx, rdx, &shouldBreak, &logged)) {
            return false;
        }

        if (shouldPause) *shouldPause = shouldBreak;

        static thread_local std::string line;
        if (logged) {
            line = formatLog(address, rip, rax, rbx, rcx, rdx);
            if (outLogLine) *outLogLine = line.c_str();
        } else if (outLogLine) {
            *outLogLine = nullptr;
        }

        return true;
    }

private:
    std::string formatLog(unsigned long long address,
                          unsigned long long rip,
                          unsigned long long rax,
                          unsigned long long rbx,
                          unsigned long long rcx,
                          unsigned long long rdx) {
        std::ostringstream ss;
        ss << "[logpoint] addr=0x" << std::hex << address
           << " rip=0x" << rip
           << " rax=0x" << rax
           << " rbx=0x" << rbx
           << " rcx=0x" << rcx
           << " rdx=0x" << rdx;
        return ss.str();
    }

    std::mutex mu_;
    std::vector<Logpoint> points_;
};

static LogpointBridge g_logpoints;

} // namespace RawrXD::IDE

extern "C" {

bool RawrXD_IDE_AddLogpoint(const char* file, int line, unsigned long long address, const char* condition, const char* format) {
    RawrXD::IDE::Logpoint lp;
    lp.file = file ? file : "";
    lp.line = line;
    lp.address = address;
    lp.condition = condition ? condition : "";
    lp.format = format ? format : "";
    return RawrXD::IDE::g_logpoints.add(lp);
}

bool RawrXD_IDE_OnLogpointHit(unsigned long long address,
                              unsigned long long rip,
                              unsigned long long rax,
                              unsigned long long rbx,
                              unsigned long long rcx,
                              unsigned long long rdx,
                              bool* shouldPause,
                              const char** outLogLine) {
    return RawrXD::IDE::g_logpoints.onBreakpointHit(address, rip, rax, rbx, rcx, rdx, shouldPause, outLogLine);
}

}
