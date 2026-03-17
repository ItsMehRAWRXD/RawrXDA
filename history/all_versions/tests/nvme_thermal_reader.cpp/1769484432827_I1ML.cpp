#include <windows.h>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
constexpr const char* kMapName = "Global\\SOVEREIGN_NVME_TEMPS";
constexpr const char* kMapNameLocal = "Local\\SOVEREIGN_NVME_TEMPS";
constexpr uint32_t kSignature = 0x45564F53; // "SOVE"
constexpr uint32_t kVersion = 1;
constexpr uint32_t kMaxDrives = 16;

struct SharedLayout {
    uint32_t signature;
    uint32_t version;
    uint32_t count;
    uint32_t reserved;
    int32_t temps[kMaxDrives];
    int32_t wear[kMaxDrives];
    uint64_t timestampMs;
};
}

int main() {
    HANDLE mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, kMapName);
    bool localFallback = false;
    if (!mapping) {
        mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, kMapNameLocal);
        if (!mapping) {
            std::cerr << "OpenFileMappingA failed: " << GetLastError() << "\n";
            return 1;
        }
        localFallback = true;
    }

    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(SharedLayout));
    if (!view) {
        std::cerr << "MapViewOfFile failed: " << GetLastError() << "\n";
        CloseHandle(mapping);
        return 1;
    }

    const auto* data = reinterpret_cast<const SharedLayout*>(view);
    if (data->signature != kSignature || data->version != kVersion) {
        std::cerr << "Invalid signature/version in shared memory.\n";
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        return 1;
    }

    uint32_t count = data->count;
    if (count > kMaxDrives) {
        count = kMaxDrives;
    }

    std::cout << "Sovereign NVMe Temps (count=" << count << ")";
    if (localFallback) {
        std::cout << " [local]";
    }
    std::cout << "\n";
    std::cout << "Timestamp(ms): " << data->timestampMs << "\n";
    for (uint32_t i = 0; i < count; ++i) {
        std::cout << "Drive " << i << ": temp=" << data->temps[i]
                  << "C wear=" << data->wear[i] << "\n";
    }

    UnmapViewOfFile(view);
    CloseHandle(mapping);
    return 0;
}
