#include <windows.h>
#include <cstdint>
#include <iostream>
#include <vector>

#include "logging/logger.h"
static Logger s_logger("nvme_thermal_reader");

namespace {
constexpr const char* kMapName = "Global\\SOVEREIGN_NVME_TEMPS";
constexpr const char* kMapNameLocal = "Local\\SOVEREIGN_NVME_TEMPS";
constexpr const char* kMapNameBare = "SOVEREIGN_NVME_TEMPS";
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
    const char* namespaceLabel = "global";
    if (!mapping) {
        mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, kMapNameLocal);
        if (!mapping) {
            mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, kMapNameBare);
            if (!mapping) {
                s_logger.error( "OpenFileMappingA failed: " << GetLastError() << "\n";
                return 1;
            }
            namespaceLabel = "bare";
        } else {
            namespaceLabel = "local";
        }
    }

    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(SharedLayout));
    if (!view) {
        s_logger.error( "MapViewOfFile failed: " << GetLastError() << "\n";
        CloseHandle(mapping);
        return 1;
    }

    const auto* data = reinterpret_cast<const SharedLayout*>(view);
    if (data->signature != kSignature || data->version != kVersion) {
        s_logger.error( "Invalid signature/version in shared memory.\n";
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        return 1;
    }

    uint32_t count = data->count;
    if (count > kMaxDrives) {
        count = kMaxDrives;
    }

    s_logger.info("Sovereign NVMe Temps (count=");
    s_logger.info("Timestamp(ms): ");
    for (uint32_t i = 0; i < count; ++i) {
        s_logger.info("Drive ");
    }

    UnmapViewOfFile(view);
    CloseHandle(mapping);
    return 0;
}
