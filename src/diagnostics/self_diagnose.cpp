#include "self_diagnose.hpp"

#ifdef _WIN32
#include <crtdbg.h>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <array>
#include <mutex>
#include <processthreadsapi.h>
#include <winnt.h>
#include <winbase.h>
#endif

namespace RawrXD::Diagnostics {

#ifdef _WIN32
namespace {
    constexpr size_t kLogPathCapacity = MAX_PATH;
    constexpr size_t kLogBufferCapacity = 2048;
    constexpr std::uint32_t kAllocMagic = 0xA11C0C0D;
    constexpr std::uint64_t kVtableCanary = 0xCAFEBABEDEADC0DEull;

    struct AllocMeta {
        std::uint32_t magic;
        std::size_t size;
        const char* file;
        int line;
        const char* tag;
        void* base;
    };

    struct AllocationRecord {
        void* ptr = nullptr;
        std::size_t size = 0;
        const char* file = nullptr;
        int line = 0;
        const char* tag = nullptr;
    };

    struct VTableRecord {
        void* obj = nullptr;
        void** expected = nullptr;
        const char* type = nullptr;
    };

    char g_logPath[kLogPathCapacity] = "D:\\rawrxd\\crash_diag.txt";
    volatile LONG g_installState = 0;
    volatile LONG g_symbolsInitialized = 0;
    std::atomic<std::uint32_t> g_phaseValue{static_cast<std::uint32_t>(InitPhase::Entry)};

    std::mutex g_allocMutex;
    std::array<AllocationRecord, 8192> g_allocs{};
    std::mutex g_vtMutex;
    std::array<VTableRecord, 2048> g_vtRecords{};

    void SafeCopyPath(const char* src) {
        if (!src || !*src) {
            return;
        }
        std::strncpy(g_logPath, src, kLogPathCapacity - 1);
        g_logPath[kLogPathCapacity - 1] = '\0';
    }

    void WriteLineRaw(const char* text) {
        if (!text) {
            return;
        }

        HANDLE file = CreateFileA(
            g_logPath,
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
            nullptr);

        if (file != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
            WriteFile(file, "\r\n", 2, &written, nullptr);
            CloseHandle(file);
        }

        OutputDebugStringA(text);
        OutputDebugStringA("\n");
    }

    void WriteFormattedLine(const char* fmt, va_list args) {
        char buffer[kLogBufferCapacity] = {};
#if defined(_MSC_VER)
        _vsnprintf_s(buffer, _TRUNCATE, fmt, args);
#else
        std::vsnprintf(buffer, sizeof(buffer), fmt, args);
#endif
        WriteLineRaw(buffer);
    }

    void HardWrite(const char* text) {
        if (!text) {
            return;
        }

        HANDLE file = CreateFileA(
            "D:\\rawrxd\\hard_diag.log",
            FILE_APPEND_DATA,
            0,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
            nullptr);

        if (file != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
            WriteFile(file, "\r\n", 2, &written, nullptr);
            CloseHandle(file);
        }
    }

    bool ValidateProcessHeaps() {
        HANDLE heaps[64] = {};
        const DWORD count = GetProcessHeaps(static_cast<DWORD>(sizeof(heaps) / sizeof(heaps[0])), heaps);
        if (count == 0) {
            return false;
        }

        const DWORD limit = (count > (sizeof(heaps) / sizeof(heaps[0]))) ? static_cast<DWORD>(sizeof(heaps) / sizeof(heaps[0])) : count;
        for (DWORD i = 0; i < limit; ++i) {
            if (!HeapValidate(heaps[i], 0, nullptr)) {
                return false;
            }
        }

        return true;
    }

    bool ValidateCrtHeap() {
#ifdef _DEBUG
        return _CrtCheckMemory() != 0;
#else
        return true;
#endif
    }

    void EnableCrtChecks() {
#ifdef _DEBUG
        int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
        flags |= _CRTDBG_ALLOC_MEM_DF;
        flags |= _CRTDBG_LEAK_CHECK_DF;
        flags |= _CRTDBG_CHECK_ALWAYS_DF;
        _CrtSetDbgFlag(flags);
        _CrtSetBreakAlloc(0);
#endif
    }

    void ConfigureSymbols() {
        if (InterlockedCompareExchange(&g_symbolsInitialized, 1, 0) == 0) {
            SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
            SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        }
    }

    void WalkStack(PCONTEXT ctx) {
        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();

        STACKFRAME64 frame = {};
#ifdef _M_X64
        frame.AddrPC.Offset = ctx->Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = ctx->Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = ctx->Rsp;
        frame.AddrStack.Mode = AddrModeFlat;
#else
        frame.AddrPC.Offset = ctx->Eip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = ctx->Ebp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = ctx->Esp;
        frame.AddrStack.Mode = AddrModeFlat;
#endif

        char symbolStorage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolStorage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME - 1;

        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;

        for (unsigned frameIndex = 0; frameIndex < 64; ++frameIndex) {
            if (!StackWalk64(
#ifdef _M_X64
                IMAGE_FILE_MACHINE_AMD64,
#else
                IMAGE_FILE_MACHINE_I386,
#endif
                process,
                thread,
                &frame,
                ctx,
                nullptr,
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                nullptr)) {
                break;
            }

            const DWORD64 addr = frame.AddrPC.Offset;
            if (addr == 0) {
                break;
            }

            DWORD64 symbolDisplacement = 0;
            const char* symbolName = "???";
            const char* moduleName = "???";
            char lineBuffer[256] = {};

            if (SymFromAddr(process, addr, &symbolDisplacement, symbol)) {
                symbolName = symbol->Name;
            }

            IMAGEHLP_MODULE64 module = {};
            module.SizeOfStruct = sizeof(module);
            if (SymGetModuleInfo64(process, addr, &module)) {
                moduleName = module.ModuleName;
            }

            if (SymGetLineFromAddr64(process, addr, &lineDisplacement, &line)) {
#if defined(_MSC_VER)
                _snprintf_s(lineBuffer, _TRUNCATE, "%s(%lu)", line.FileName ? line.FileName : "?", line.LineNumber);
#else
                std::snprintf(lineBuffer, sizeof(lineBuffer), "%s(%lu)", line.FileName ? line.FileName : "?", line.LineNumber);
#endif
            } else {
                std::strcpy(lineBuffer, "?");
            }

#if defined(_MSC_VER)
            char out[512] = {};
            _snprintf_s(out, _TRUNCATE, "  %s!%s+0x%llX [%s]", moduleName, symbolName, static_cast<unsigned long long>(symbolDisplacement), lineBuffer);
#else
            char out[512] = {};
            std::snprintf(out, sizeof(out), "  %s!%s+0x%llX [%s]", moduleName, symbolName, static_cast<unsigned long long>(symbolDisplacement), lineBuffer);
#endif
            WriteLineRaw(out);
        }
    }

    AllocationRecord* FindAllocRecord(void* ptr) {
        if (!ptr) {
            return nullptr;
        }
        std::lock_guard<std::mutex> lock(g_allocMutex);
        for (auto& rec : g_allocs) {
            if (rec.ptr == ptr) {
                return &rec;
            }
        }
        return nullptr;
    }

    void RecordAlloc(void* ptr, std::size_t size, const char* file, int line, const char* tag) {
        std::lock_guard<std::mutex> lock(g_allocMutex);
        for (auto& rec : g_allocs) {
            if (rec.ptr == nullptr) {
                rec.ptr = ptr;
                rec.size = size;
                rec.file = file;
                rec.line = line;
                rec.tag = tag;
                return;
            }
        }
    }

    void RecordFree(void* ptr) {
        std::lock_guard<std::mutex> lock(g_allocMutex);
        for (auto& rec : g_allocs) {
            if (rec.ptr == ptr) {
                rec = AllocationRecord{};
                return;
            }
        }
    }

    void RecordVTable(void* obj, const char* typeName) {
        if (!obj) {
            return;
        }
        std::lock_guard<std::mutex> lock(g_vtMutex);
        for (auto& rec : g_vtRecords) {
            if (rec.obj == nullptr) {
                rec.obj = obj;
                rec.expected = *(void***)obj;
                rec.type = typeName;
                return;
            }
        }
    }

    bool IsExecutableAddress(const void* fn) {
        if (!fn) {
            return false;
        }
        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(fn, &mbi, sizeof(mbi)) == 0) {
            return false;
        }

        if (mbi.State != MEM_COMMIT) {
            return false;
        }

        const DWORD protect = mbi.Protect & 0xFF;
        return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
               protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
    }
}
#endif // _WIN32

void SelfDiagnoser::Install(const char* logPath) {
#ifdef _WIN32
    if (logPath && *logPath) {
        SafeCopyPath(logPath);
    }

    if (InterlockedCompareExchange(&g_installState, 1, 0) != 0) {
        return;
    }

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
    EnableCrtChecks();
    ConfigureSymbols();
    SetUnhandledExceptionFilter(SelfExceptionFilter);
    SelfLog("self_diagnoser_installed");
    SetPhase(InitPhase::Entry, "Install");
    CheckHeap("Entry");
#else
    (void)logPath;
#endif
}

bool SelfDiagnoser::CheckHeap(const char* tag) {
#ifdef _WIN32
    const bool processHeapOk = ValidateProcessHeaps();
    const bool crtHeapOk = ValidateCrtHeap();

    if (!processHeapOk || !crtHeapOk) {
        SelfLog("HEAP CORRUPTION DETECTED at: %s", tag ? tag : "(null)");
        SelfLog("Stack trace:");
        LogStackTrace(nullptr);
        CreateSelfDump(nullptr);
        return false;
    }
#else
    (void)tag;
#endif
    return true;
}

void SelfDiagnoser::CheckHeapOrDie(const char* tag) {
    if (!CheckHeap(tag)) {
#ifdef _WIN32
        if (IsDebuggerPresent()) {
            DebugBreak();
        }
        TerminateProcess(GetCurrentProcess(), 0xDEAD);
#else
        std::abort();
#endif
    }
}

void SelfDiagnoser::SelfLog(const char* fmt, ...) {
#ifdef _WIN32
    va_list args;
    va_start(args, fmt);
    WriteFormattedLine(fmt, args);
    va_end(args);
#else
    (void)fmt;
#endif
}

void SelfDiagnoser::HardLog(const char* msg) {
#ifdef _WIN32
    HardWrite(msg);
#else
    (void)msg;
#endif
}

void SelfDiagnoser::SetPhase(InitPhase phase, const char* detail) {
#ifdef _WIN32
    g_phaseValue.store(static_cast<std::uint32_t>(phase), std::memory_order_release);
    SelfLog("PHASE: %u %s", static_cast<unsigned>(g_phaseValue.load()), detail ? detail : "");
    CheckHeap("PhaseFence");
#else
    (void)phase;
    (void)detail;
#endif
}

InitPhase SelfDiagnoser::CurrentPhase() {
#ifdef _WIN32
    return static_cast<InitPhase>(g_phaseValue.load(std::memory_order_acquire));
#else
    return InitPhase::Entry;
#endif
}

void* SelfDiagnoser::GuardAlloc(std::size_t size, const char* tag, const char* file, int line) {
#ifdef _WIN32
    const std::size_t aligned = (size + 0xFFF) & ~static_cast<std::size_t>(0xFFF);
    const std::size_t total = aligned + 0x2000;

    BYTE* mem = reinterpret_cast<BYTE*>(VirtualAlloc(nullptr, total, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!mem) {
        return nullptr;
    }

    DWORD oldProtect = 0;
    VirtualProtect(mem, 0x1000, PAGE_NOACCESS, &oldProtect);

    BYTE* user = mem + 0x1000;
    VirtualProtect(user + aligned, 0x1000, PAGE_NOACCESS, &oldProtect);

    auto* meta = reinterpret_cast<AllocMeta*>(user - sizeof(AllocMeta));
    meta->magic = kAllocMagic;
    meta->size = size;
    meta->file = file;
    meta->line = line;
    meta->tag = tag;
    meta->base = mem;

    std::memset(user, 0xAA, size);
    RecordAlloc(user, size, file, line, tag);
    return user;
#else
    (void)size;
    (void)tag;
    (void)file;
    (void)line;
    return nullptr;
#endif
}

void SelfDiagnoser::GuardFree(void* ptr, const char* tag) {
#ifdef _WIN32
    if (!ptr) {
        return;
    }

    auto* meta = reinterpret_cast<AllocMeta*>(reinterpret_cast<BYTE*>(ptr) - sizeof(AllocMeta));
    if (meta->magic != kAllocMagic) {
        SelfLog("GUARD FREE MAGIC MISMATCH %p", ptr);
        return;
    }

    std::memset(ptr, 0xDD, meta->size);
    RecordFree(ptr);
    BYTE* base = reinterpret_cast<BYTE*>(meta->base);
    (void)tag;
    VirtualFree(base, 0, MEM_RELEASE);
#else
    (void)ptr;
    (void)tag;
#endif
}

void SelfDiagnoser::TrackAlloc(void* ptr, std::size_t size, const char* file, int line, const char* tag) {
#ifdef _WIN32
    if (!ptr) {
        return;
    }
    RecordAlloc(ptr, size, file, line, tag);
#else
    (void)ptr;
    (void)size;
    (void)file;
    (void)line;
    (void)tag;
#endif
}

void SelfDiagnoser::TrackFree(void* ptr, const char* tag) {
#ifdef _WIN32
    (void)tag;
    if (!ptr) {
        return;
    }
    RecordFree(ptr);
#else
    (void)ptr;
    (void)tag;
#endif
}

void SelfDiagnoser::ReportAlloc(void* ptr, const char* reason) {
#ifdef _WIN32
    AllocationRecord* rec = FindAllocRecord(ptr);
    if (!rec) {
        SelfLog("ALLOC REPORT: %p unknown (%s)", ptr, reason ? reason : "");
        return;
    }
    SelfLog("ALLOC REPORT: %p size=%zu %s:%d tag=%s (%s)",
        rec->ptr,
        rec->size,
        rec->file ? rec->file : "?",
        rec->line,
        rec->tag ? rec->tag : "",
        reason ? reason : "");
#else
    (void)ptr;
    (void)reason;
#endif
}

void SelfDiagnoser::RegisterVTableGuard(void* obj, const char* typeName) {
#ifdef _WIN32
    RecordVTable(obj, typeName);
    // Stash a canary adjacent to expected vtable to make tampering obvious when revalidated.
    if (obj) {
        __try {
            volatile std::uint64_t* canary = reinterpret_cast<std::uint64_t*>(
                reinterpret_cast<std::uint8_t*>(obj) + sizeof(void*));
            *canary ^= kVtableCanary;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // Do not propagate; keep registration best-effort.
        }
    }
#else
    (void)obj;
    (void)typeName;
#endif
}

bool SelfDiagnoser::ValidateVTableGuard(void* obj, const char* context) {
#ifdef _WIN32
    if (!obj) {
        return false;
    }

    void** current = *(void***)obj;
    std::lock_guard<std::mutex> lock(g_vtMutex);
    for (auto& rec : g_vtRecords) {
        if (rec.obj == obj) {
            if (rec.expected != current) {
                SelfLog("VTABLE CORRUPTED @ %p (type=%s ctx=%s) expected=%p found=%p",
                    obj,
                    rec.type ? rec.type : "?",
                    context ? context : "?",
                    rec.expected,
                    current);
                return false;
            }
            return true;
        }
    }
    return true;
#else
    (void)obj;
    (void)context;
    return true;
#endif
}

bool SelfDiagnoser::ValidateFunctionPointer(const void* fn, const char* context) {
#ifdef _WIN32
    if (IsExecutableAddress(fn)) {
        return true;
    }

    SelfLog("INVALID FUNCTION PTR %p (%s)", fn, context ? context : "?");
    return false;
#else
    (void)fn;
    (void)context;
    return true;
#endif
}

void SelfDiagnoser::LogStackTrace(
#ifdef _WIN32
    PCONTEXT ctx
#else
    void* ctx
#endif
) {
#ifdef _WIN32
    CONTEXT localContext = {};
    if (!ctx) {
        RtlCaptureContext(&localContext);
        ctx = &localContext;
    }

    if (InterlockedCompareExchange(&g_symbolsInitialized, 1, 1) == 0) {
        return;
    }

    WalkStack(ctx);
#else
    (void)ctx;
#endif
}

void SelfDiagnoser::CreateSelfDump(
#ifdef _WIN32
    EXCEPTION_POINTERS* exceptionInfo
#else
    void* exceptionInfo
#endif
) {
#ifdef _WIN32
    HANDLE file = CreateFileA(
        "D:\\rawrxd\\self_crash.dmp",
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION mei = {};
    MINIDUMP_EXCEPTION_INFORMATION* meiPtr = nullptr;
    if (exceptionInfo) {
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = exceptionInfo;
        mei.ClientPointers = FALSE;
        meiPtr = &mei;
    }

    MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        file,
        static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory),
        meiPtr,
        nullptr,
        nullptr);

    CloseHandle(file);
    SelfLog("dump_written: D:\\rawrxd\\self_crash.dmp");
#else
    (void)exceptionInfo;
#endif
}

#ifdef _WIN32
LONG WINAPI SelfDiagnoser::SelfExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    const DWORD code = exceptionInfo && exceptionInfo->ExceptionRecord ? exceptionInfo->ExceptionRecord->ExceptionCode : 0;
    const void* address = exceptionInfo && exceptionInfo->ExceptionRecord ? exceptionInfo->ExceptionRecord->ExceptionAddress : nullptr;

    SelfLog("EXCEPTION: 0x%08lX at %p", static_cast<unsigned long>(code), address);
    if (exceptionInfo && exceptionInfo->ContextRecord) {
        SelfLog("Stack trace:");
        LogStackTrace(exceptionInfo->ContextRecord);
    }
    CreateSelfDump(exceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace RawrXD::Diagnostics
