#include "pattern_scan.hpp"

#ifdef _WIN32
#include <winnt.h>
#include <cstring>
#include <cstdint>
#endif

namespace RawrXD::Diagnostics {

#ifdef _WIN32
namespace {
    bool IsReadableCodePointer(std::uintptr_t ptr) {
        if (ptr < 0x10000ull) {
            return false;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(ptr), &mbi, sizeof(mbi)) == 0) {
            return false;
        }

        if (mbi.State != MEM_COMMIT) {
            return false;
        }

        const DWORD protect = mbi.Protect & 0xFF;
        switch (protect) {
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
        case PAGE_READONLY:
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
            return true;
        default:
            return false;
        }
    }
}

std::size_t CorruptionScanner::ScanSectionForSuspiciousPointers(const unsigned char* base, std::size_t size, const char* sectionName) {
    if (!base || size < sizeof(std::uintptr_t)) {
        return 0;
    }

    std::size_t suspiciousCount = 0;
    for (std::size_t offset = 0; offset + sizeof(std::uintptr_t) <= size; offset += sizeof(std::uintptr_t)) {
        const auto candidate = *reinterpret_cast<const std::uintptr_t*>(base + offset);
        if (!IsReadableCodePointer(candidate)) {
            continue;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(candidate), &mbi, sizeof(mbi)) != 0) {
            const DWORD protect = mbi.Protect & 0xFF;
            const bool isExecutable = protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
                                      protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
            if (!isExecutable) {
                SelfDiagnoser::SelfLog("SUSPICIOUS PTR: 0x%p at %s+0x%zx", reinterpret_cast<void*>(candidate), sectionName ? sectionName : ".rdata", offset);
                ++suspiciousCount;
            }
        }
    }

    return suspiciousCount;
}

std::size_t CorruptionScanner::ScanModule(HMODULE moduleHandle) {
    if (!moduleHandle) {
        return 0;
    }

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleHandle);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>((const unsigned char*)moduleHandle + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    const auto* section = IMAGE_FIRST_SECTION(const_cast<IMAGE_NT_HEADERS*>(nt));
    std::size_t suspicious = 0;

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const char* name = reinterpret_cast<const char*>(section[i].Name);
        if (std::memcmp(name, ".rdata", 6) == 0 || std::memcmp(name, ".data", 5) == 0) {
            const auto* base = reinterpret_cast<const unsigned char*>(moduleHandle) + section[i].VirtualAddress;
            suspicious += ScanSectionForSuspiciousPointers(base, section[i].Misc.VirtualSize, name);
        }
    }

    return suspicious;
}

std::size_t CorruptionScanner::ScanCurrentModule() {
    return ScanModule(GetModuleHandleW(nullptr));
}
#endif

} // namespace RawrXD::Diagnostics
