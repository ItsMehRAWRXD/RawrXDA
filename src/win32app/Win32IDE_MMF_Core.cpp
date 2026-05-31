#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <string>

/**
 * @file Win32IDE_MMF_Core.cpp
 * @brief Batch 3 (17/118): Memory Mapped File (MMF) Core.
 * Implements the zero-copy foundation for high-velocity tensor access.
 */

namespace RawrXD::Memory {

// Resolves: MMF_CreateMapping
extern "C" HANDLE MMF_CreateMapping(const char* name, size_t size) {
    LOG_INFO("[MMF] Creating mapping: " + std::string(name) + " Size: " + std::to_string(size));
    
    HANDLE hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        (DWORD)(size >> 32),
        (DWORD)(size & 0xFFFFFFFF),
        name
    );

    if (!hMap) {
        LOG_ERROR("[MMF] Failed to create mapping: " + std::to_string(GetLastError()));
    }
    return hMap;
}

// Resolves: MMF_MapView
extern "C" void* MMF_MapView(HANDLE hMap, size_t offset, size_t size) {
    void* ptr = MapViewOfFile(
        hMap,
        FILE_MAP_ALL_ACCESS,
        (DWORD)(offset >> 32),
        (DWORD)(offset & 0xFFFFFFFF),
        size
    );
    
    if (ptr) {
        LOG_SUCCESS("[MMF] View mapped successfully.");
    }
    return ptr;
}

} // namespace RawrXD::Memory
