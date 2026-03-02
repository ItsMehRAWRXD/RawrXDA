#include <string>
#include <fstream>
#include <vector>
#include <mutex>
#include <cstdint>

namespace RawrXD::Core {

struct DumpSummary {
    bool valid = false;
    uint32_t signature = 0;
    uint32_t streamCount = 0;
    uint32_t streamDirectoryRva = 0;
    uint64_t fileSize = 0;
};

class DumpLoader {
public:
    bool open(const std::string& path) {
        std::lock_guard<std::mutex> lock(mu_);
        summary_ = {};

        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;

        in.seekg(0, std::ios::end);
        summary_.fileSize = static_cast<uint64_t>(in.tellg());
        in.seekg(0, std::ios::beg);

        uint32_t sig = 0;
        uint32_t version = 0;
        uint32_t streamCount = 0;
        uint32_t streamDirRva = 0;

        in.read(reinterpret_cast<char*>(&sig), sizeof(sig));
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        in.read(reinterpret_cast<char*>(&streamCount), sizeof(streamCount));
        in.read(reinterpret_cast<char*>(&streamDirRva), sizeof(streamDirRva));

        summary_.signature = sig;
        summary_.streamCount = streamCount;
        summary_.streamDirectoryRva = streamDirRva;
        summary_.valid = (sig == 0x504d444d); // 'MDMP'
        path_ = path;

        return summary_.valid;
    }

    DumpSummary summary() const {
        std::lock_guard<std::mutex> lock(mu_);
        return summary_;
    }

private:
    mutable std::mutex mu_;
    std::string path_;
    DumpSummary summary_;
};

static DumpLoader g_dumpLoader;

} // namespace RawrXD::Core

extern "C" {

bool RawrXD_Debugger_OpenDump(const char* dumpPath) {
    if (!dumpPath) return false;
    return RawrXD::Core::g_dumpLoader.open(dumpPath);
}

bool RawrXD_Debugger_GetDumpSummary(uint32_t* signature, uint32_t* streamCount, uint64_t* fileSize) {
    auto s = RawrXD::Core::g_dumpLoader.summary();
    if (!s.valid) return false;
    if (signature) *signature = s.signature;
    if (streamCount) *streamCount = s.streamCount;
    if (fileSize) *fileSize = s.fileSize;
    return true;
}

}
