#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")
#endif

namespace RawrXD::Diagnostics {

enum class InitPhase : std::uint32_t {
    Entry = 0,
    CoreInit,
    Registry,
    UI,
    Menu,
    Ready,
    Runtime
};

class SelfDiagnoser final {
public:
    // Installation and core logging
    static void Install(const char* logPath = nullptr);
    static bool CheckHeap(const char* tag);
    static void CheckHeapOrDie(const char* tag);
    static void SelfLog(const char* fmt, ...);
    static void HardLog(const char* msg);

    // Phase fencing
    static void SetPhase(InitPhase phase, const char* detail = nullptr);
    static InitPhase CurrentPhase();

    // Guarded allocations with provenance
    static void* GuardAlloc(std::size_t size, const char* tag = nullptr, const char* file = nullptr, int line = 0);
    static void GuardFree(void* ptr, const char* tag = nullptr);
    static void TrackAlloc(void* ptr, std::size_t size, const char* file, int line, const char* tag = nullptr);
    static void TrackFree(void* ptr, const char* tag = nullptr);
    static void ReportAlloc(void* ptr, const char* reason);

    // VTable and function-pointer validation
    static void RegisterVTableGuard(void* obj, const char* typeName);
    static bool ValidateVTableGuard(void* obj, const char* context);
    static bool ValidateFunctionPointer(const void* fn, const char* context);

    static void LogStackTrace(
#ifdef _WIN32
        PCONTEXT ctx = nullptr
#else
        void* ctx = nullptr
#endif
    );
    static void CreateSelfDump(
#ifdef _WIN32
        EXCEPTION_POINTERS* exceptionInfo = nullptr
#else
        void* exceptionInfo = nullptr
#endif
    );

#ifdef _WIN32
    static LONG WINAPI SelfExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
#endif
};

} // namespace RawrXD::Diagnostics

#ifndef DIAG_CHECK
#define DIAG_CHECK(tag) ::RawrXD::Diagnostics::SelfDiagnoser::CheckHeapOrDie(tag)
#endif

#define RAWRXD_PHASE_SET(p, detail) ::RawrXD::Diagnostics::SelfDiagnoser::SetPhase((p), (detail))
#define RAWRXD_GUARD_ALLOC(sz, tag) ::RawrXD::Diagnostics::SelfDiagnoser::GuardAlloc((sz), (tag), __FILE__, __LINE__)
#define RAWRXD_TRACK_ALLOC(ptr, sz, tag) ::RawrXD::Diagnostics::SelfDiagnoser::TrackAlloc((ptr), (sz), __FILE__, __LINE__, (tag))
