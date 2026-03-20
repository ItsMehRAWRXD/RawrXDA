#if !defined(_MSC_VER)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winioctl.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

template <typename T>
T* rvaToPtr(void* imageBase, DWORD rva) {
    return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(imageBase) + rva);
}

}  // namespace

extern "C" int BeaconRouterInit() {
    return 0;
}

extern "C" int asm_scsi_inquiry_quick(HANDLE, void* buffer, uint32_t bufferSize, uint32_t) {
    if (!buffer || bufferSize < 36) {
        return ERROR_INSUFFICIENT_BUFFER;
    }
    std::memset(buffer, 0, bufferSize);
    auto* b = static_cast<uint8_t*>(buffer);
    b[0] = 0x00;  // direct-access block device
    b[2] = 0x05;  // SPC-3
    b[4] = static_cast<uint8_t>(bufferSize > 5 ? (bufferSize - 5) : 0);
    std::memcpy(b + 8, "WD      ", 8);
    std::memcpy(b + 16, "My Book         ", 16);
    std::memcpy(b + 32, "0001", 4);
    return 0;
}

extern "C" int asm_scsi_read_capacity(HANDLE hDevice, uint64_t* totalSectors, uint32_t* sectorSize) {
    if (!totalSectors || !sectorSize) {
        return ERROR_INVALID_PARAMETER;
    }

    DISK_GEOMETRY_EX geom = {};
    DWORD outBytes = 0;
    if (hDevice != INVALID_HANDLE_VALUE &&
        DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                        nullptr, 0, &geom, sizeof(geom), &outBytes, nullptr)) {
        const uint64_t bytesPerSector = geom.Geometry.BytesPerSector ? geom.Geometry.BytesPerSector : 4096;
        const uint64_t diskBytes = static_cast<uint64_t>(geom.DiskSize.QuadPart);
        *sectorSize = static_cast<uint32_t>(bytesPerSector);
        *totalSectors = (bytesPerSector > 0) ? (diskBytes / bytesPerSector) : 0;
        return 0;
    }

    *sectorSize = 4096;
    *totalSectors = 488281250ULL;  // Conservative 2 TB fallback
    return 0;
}

extern "C" int asm_scsi_hammer_read(HANDLE hDevice, uint64_t lba, void* buffer,
                                    uint32_t sectorSize, uint32_t maxRetries, uint32_t timeoutMs) {
    if (hDevice == INVALID_HANDLE_VALUE || !buffer || sectorSize == 0) {
        return ERROR_INVALID_PARAMETER;
    }

    if (maxRetries == 0) {
        maxRetries = 1;
    }

    const uint64_t offset = lba * static_cast<uint64_t>(sectorSize);
    LARGE_INTEGER li{};
    li.QuadPart = static_cast<LONGLONG>(offset);

    for (uint32_t attempt = 0; attempt < maxRetries; ++attempt) {
        if (!SetFilePointerEx(hDevice, li, nullptr, FILE_BEGIN)) {
            continue;
        }
        DWORD bytesRead = 0;
        if (ReadFile(hDevice, buffer, sectorSize, &bytesRead, nullptr) && bytesRead == sectorSize) {
            return 0;
        }
        if (timeoutMs > 0) {
            Sleep(timeoutMs / (attempt + 1));
        }
    }
    return ERROR_READ_FAULT;
}

extern "C" int asm_extract_bridge_key(HANDLE, void* keyBuffer, uint32_t bridgeType) {
    if (!keyBuffer) {
        return -1;
    }
    auto* out = static_cast<uint8_t*>(keyBuffer);
    for (int i = 0; i < 32; ++i) {
        out[i] = static_cast<uint8_t>((0xA5U + i * 13U + bridgeType * 29U) & 0xFFU);
    }
    return 0;
}

extern "C" void RawrXD_WalkImports(PVOID imageBase, DWORD importRva, PVOID callback, PVOID context) {
    if (!imageBase || importRva == 0 || !callback) {
        return;
    }

    using ImportCallback = void(__stdcall*)(const char*, const char*, void*);
    auto cb = reinterpret_cast<ImportCallback>(callback);

    auto* desc = rvaToPtr<IMAGE_IMPORT_DESCRIPTOR>(imageBase, importRva);
    while (desc && desc->Name) {
        const char* dllName = rvaToPtr<const char>(imageBase, desc->Name);
        DWORD thunkRva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
        auto* thunk = rvaToPtr<IMAGE_THUNK_DATA64>(imageBase, thunkRva);

        while (thunk && thunk->u1.AddressOfData) {
            if (IMAGE_SNAP_BY_ORDINAL64(thunk->u1.Ordinal)) {
                char ordinalName[32];
                std::snprintf(ordinalName, sizeof(ordinalName), "#%llu",
                              static_cast<unsigned long long>(IMAGE_ORDINAL64(thunk->u1.Ordinal)));
                cb(dllName, ordinalName, context);
            } else {
                auto* ibn = rvaToPtr<IMAGE_IMPORT_BY_NAME>(imageBase, static_cast<DWORD>(thunk->u1.AddressOfData));
                cb(dllName, reinterpret_cast<const char*>(ibn->Name), context);
            }
            ++thunk;
        }
        ++desc;
    }
}

extern "C" void RawrXD_WalkExports(PVOID imageBase, DWORD exportRva, PVOID callback, PVOID context) {
    if (!imageBase || exportRva == 0 || !callback) {
        return;
    }

    using ExportCallback = void(__stdcall*)(const char*, WORD, PVOID, void*);
    auto cb = reinterpret_cast<ExportCallback>(callback);

    auto* exp = rvaToPtr<IMAGE_EXPORT_DIRECTORY>(imageBase, exportRva);
    if (!exp) {
        return;
    }

    auto* funcRVAs = rvaToPtr<DWORD>(imageBase, exp->AddressOfFunctions);
    auto* nameRVAs = rvaToPtr<DWORD>(imageBase, exp->AddressOfNames);
    auto* ordinals = rvaToPtr<WORD>(imageBase, exp->AddressOfNameOrdinals);

    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        const char* name = rvaToPtr<const char>(imageBase, nameRVAs[i]);
        const WORD ordIndex = ordinals[i];
        const DWORD funcRva = funcRVAs[ordIndex];
        void* addr = rvaToPtr<void>(imageBase, funcRva);
        const WORD ordinal = static_cast<WORD>(exp->Base + ordIndex);
        cb(name, ordinal, addr, context);
    }
}

#endif  // !defined(_MSC_VER)
