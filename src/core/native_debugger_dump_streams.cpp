#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <cstdint>

namespace RawrXD::Core {

struct DumpStreamInfo {
    uint32_t type = 0;
    uint32_t size = 0;
    uint32_t rva = 0;
};

class DumpStreamBrowser {
public:
    bool open(const std::string& path) {
        std::lock_guard<std::mutex> lock(mu_);
        streams_.clear();

        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;

        uint32_t sig = 0, ver = 0, count = 0, dirRva = 0;
        in.read(reinterpret_cast<char*>(&sig), 4);
        in.read(reinterpret_cast<char*>(&ver), 4);
        in.read(reinterpret_cast<char*>(&count), 4);
        in.read(reinterpret_cast<char*>(&dirRva), 4);
        if (sig != 0x504d444d || count == 0 || dirRva == 0) return false;

        in.seekg(dirRva, std::ios::beg);
        for (uint32_t i = 0; i < count; ++i) {
            DumpStreamInfo s{};
            in.read(reinterpret_cast<char*>(&s.type), 4);
            in.read(reinterpret_cast<char*>(&s.size), 4);
            in.read(reinterpret_cast<char*>(&s.rva), 4);
            streams_.push_back(s);
        }

        return true;
    }

    int count() const {
        std::lock_guard<std::mutex> lock(mu_);
        return static_cast<int>(streams_.size());
    }

    bool get(int index, DumpStreamInfo* out) const {
        if (!out) return false;
        std::lock_guard<std::mutex> lock(mu_);
        if (index < 0 || index >= static_cast<int>(streams_.size())) return false;
        *out = streams_[static_cast<size_t>(index)];
        return true;
    }

private:
    mutable std::mutex mu_;
    std::vector<DumpStreamInfo> streams_;
};

static DumpStreamBrowser g_dumpStreams;

} // namespace RawrXD::Core

extern "C" {

bool RawrXD_Debugger_OpenDumpStreams(const char* dumpPath) {
    if (!dumpPath) return false;
    return RawrXD::Core::g_dumpStreams.open(dumpPath);
}

int RawrXD_Debugger_DumpStreamCount() {
    return RawrXD::Core::g_dumpStreams.count();
}

bool RawrXD_Debugger_GetDumpStream(int index, unsigned int* outType, unsigned int* outSize, unsigned int* outRva) {
    RawrXD::Core::DumpStreamInfo s{};
    if (!RawrXD::Core::g_dumpStreams.get(index, &s)) return false;
    if (outType) *outType = s.type;
    if (outSize) *outSize = s.size;
    if (outRva) *outRva = s.rva;
    return true;
}

}
