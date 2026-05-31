#include "byte_level_hotpatcher.hpp"
#include <windows.h>
#include <psapi.h>
#include <vector>
#include <string>

#pragma comment(lib, "Psapi.lib")

namespace RawrXD {

struct ByteSearchResult {
    void* address;
    size_t size;
    uint32_t crc32;
};

// Boyer-Moore-Horspool implementation in C++
static void* bmh_search(const uint8_t* data, size_t dataLen, 
                        const uint8_t* pattern, size_t patLen) {
    if (patLen > dataLen) return nullptr;
    
    size_t skip[256];
    for (int i = 0; i < 256; ++i) skip[i] = patLen;
    for (size_t i = 0; i < patLen - 1; ++i) {
        skip[pattern[i]] = patLen - i - 1;
    }
    
    size_t i = patLen - 1;
    while (i < dataLen) {
        int j = (int)patLen - 1;
        int k = (int)i;
        while (j >= 0 && data[k] == pattern[j]) {
            k--; j--;
        }
        if (j < 0) return (void*)(data + k + 1);
        i += skip[data[i]];
    }
    return nullptr;
}

ByteSearchResult direct_search(const char* moduleName, 
                                const unsigned char* pattern, 
                                size_t patternLen) {
    ByteSearchResult result = {nullptr, 0, 0};
    
    HMODULE hMod = GetModuleHandleA(moduleName);
    if (!hMod) return result;
    
    MODULEINFO modInfo;
    if (!GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo))) {
        return result;
    }
    
    uint8_t* base = (uint8_t*)modInfo.lpBaseOfDll;
    size_t size = modInfo.SizeOfImage;
    
    result.address = bmh_search(base, size, pattern, patternLen);
    
    if (result.address) {
        result.size = patternLen;
        // Calculate CRC32 of found region
        uint32_t crc = 0xFFFFFFFF;
        uint8_t* found = (uint8_t*)result.address;
        for (size_t i = 0; i < patternLen; ++i) {
            crc ^= found[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        result.crc32 = ~crc;
    }
    
    return result;
}

bool apply_patch(void* address, const std::vector<uint8_t>& bytes, 
                 std::vector<uint8_t>* oldBytes) {
    if (!address || bytes.empty()) return false;
    
    DWORD oldProtect;
    if (!VirtualProtect(address, bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    
    if (oldBytes) {
        oldBytes->resize(bytes.size());
        memcpy(oldBytes->data(), address, bytes.size());
    }
    
    memcpy(address, bytes.data(), bytes.size());
    VirtualProtect(address, bytes.size(), oldProtect, &oldProtect);
    
    // Flush instruction cache
    FlushInstructionCache(GetCurrentProcess(), address, bytes.size());
    return true;
}

} // namespace RawrXD
