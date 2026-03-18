#pragma once

#include "self_diagnose.hpp"

#include <cstddef>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace RawrXD::Diagnostics {

class CorruptionScanner final {
public:
#ifdef _WIN32
    static std::size_t ScanCurrentModule();
    static std::size_t ScanModule(HMODULE moduleHandle);
    static std::size_t ScanSectionForSuspiciousPointers(const unsigned char* base, std::size_t size, const char* sectionName);
#else
    static std::size_t ScanCurrentModule() { return 0; }
    static std::size_t ScanModule(void*) { return 0; }
    static std::size_t ScanSectionForSuspiciousPointers(const unsigned char*, std::size_t, const char*) { return 0; }
#endif
};

} // namespace RawrXD::Diagnostics
