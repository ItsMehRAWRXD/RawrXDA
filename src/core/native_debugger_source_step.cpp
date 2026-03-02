#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace RawrXD::Core {

struct SourceLineMap {
    uint64_t address = 0;
    std::string file;
    int line = 0;
};

struct StepResult {
    bool success = false;
    uint64_t nextAddress = 0;
    std::string file;
    int line = 0;
};

class SourceStepper {
public:
    void loadMappings(const std::vector<SourceLineMap>& mappings) {
        std::lock_guard<std::mutex> lock(mu_);
        map_ = mappings;
        std::sort(map_.begin(), map_.end(), [](const auto& a, const auto& b) { return a.address < b.address; });
    }

    StepResult stepOverSource(uint64_t currentAddress) {
        std::lock_guard<std::mutex> lock(mu_);
        return stepToDifferentLine(currentAddress, false);
    }

    StepResult stepIntoSource(uint64_t currentAddress) {
        std::lock_guard<std::mutex> lock(mu_);
        return stepToDifferentLine(currentAddress, true);
    }

    StepResult stepOutSource(uint64_t currentAddress, uint64_t returnAddressHint) {
        std::lock_guard<std::mutex> lock(mu_);
        if (returnAddressHint != 0) {
            auto it = std::lower_bound(map_.begin(), map_.end(), returnAddressHint,
                [](const SourceLineMap& e, uint64_t addr) { return e.address < addr; });
            if (it != map_.end()) {
                return {true, it->address, it->file, it->line};
            }
        }
        return stepToDifferentLine(currentAddress, false);
    }

private:
    StepResult stepToDifferentLine(uint64_t currentAddress, bool allowSameFile) {
        if (map_.empty()) return {};

        auto current = std::lower_bound(map_.begin(), map_.end(), currentAddress,
            [](const SourceLineMap& e, uint64_t addr) { return e.address < addr; });
        if (current == map_.end()) return {};

        const std::string curFile = current->file;
        const int curLine = current->line;

        for (auto it = current + 1; it != map_.end(); ++it) {
            if (it->line != curLine && (allowSameFile || it->file == curFile)) {
                return {true, it->address, it->file, it->line};
            }
        }

        return {};
    }

    std::mutex mu_;
    std::vector<SourceLineMap> map_;
};

static SourceStepper g_sourceStepper;

} // namespace RawrXD::Core

extern "C" {

void RawrXD_Debugger_LoadSourceMap(uint64_t* addresses, const char** files, int* lines, int count) {
    if (!addresses || !files || !lines || count <= 0) return;
    std::vector<RawrXD::Core::SourceLineMap> mappings;
    mappings.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        mappings.push_back({addresses[i], files[i] ? files[i] : "", lines[i]});
    }
    RawrXD::Core::g_sourceStepper.loadMappings(mappings);
}

bool RawrXD_Debugger_StepOverSource(uint64_t currentAddress, uint64_t* outNextAddress, const char** outFile, int* outLine) {
    auto r = RawrXD::Core::g_sourceStepper.stepOverSource(currentAddress);
    if (!r.success) return false;
    if (outNextAddress) *outNextAddress = r.nextAddress;
    static thread_local std::string file;
    file = r.file;
    if (outFile) *outFile = file.c_str();
    if (outLine) *outLine = r.line;
    return true;
}

bool RawrXD_Debugger_StepIntoSource(uint64_t currentAddress, uint64_t* outNextAddress, const char** outFile, int* outLine) {
    auto r = RawrXD::Core::g_sourceStepper.stepIntoSource(currentAddress);
    if (!r.success) return false;
    if (outNextAddress) *outNextAddress = r.nextAddress;
    static thread_local std::string file;
    file = r.file;
    if (outFile) *outFile = file.c_str();
    if (outLine) *outLine = r.line;
    return true;
}

bool RawrXD_Debugger_StepOutSource(uint64_t currentAddress, uint64_t returnHint, uint64_t* outNextAddress, const char** outFile, int* outLine) {
    auto r = RawrXD::Core::g_sourceStepper.stepOutSource(currentAddress, returnHint);
    if (!r.success) return false;
    if (outNextAddress) *outNextAddress = r.nextAddress;
    static thread_local std::string file;
    file = r.file;
    if (outFile) *outFile = file.c_str();
    if (outLine) *outLine = r.line;
    return true;
}

}
