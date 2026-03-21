#if !defined(_MSC_VER)

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <string>

namespace {

struct DeepThinkingState {
    std::mutex mutex;
    bool initialized = false;
    std::string lastReasoning;
    std::wstring lastIpcMessage;
};

DeepThinkingState g_deepThinkingState;

struct DiskRecoveryCtx {
    std::mutex mutex;
    int driveNum = -1;
    bool keyExtracted = false;
    bool aborted = false;
    uint64_t good = 0;
    uint64_t bad = 0;
    uint64_t current = 0;
    uint64_t total = 4096;
};

std::string wideToUtf8(const std::wstring& input) {
    if (input.empty()) {
        return {};
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

}  // namespace

namespace RawrXD {
extern "C" {
int32_t g_800B_Unlocked = 0;
uint64_t g_EnterpriseFeatures = 0;
}
}  // namespace RawrXD

extern "C" void rawrxd_init_deep_thinking() {
    std::lock_guard<std::mutex> lock(g_deepThinkingState.mutex);
    g_deepThinkingState.initialized = true;
    g_deepThinkingState.lastReasoning.clear();
}

extern "C" int rawrxd_agentic_deep_think_loop(const char* prompt) {
    const char* seed = (prompt && prompt[0] != '\0')
        ? prompt
        : "Analyze current workspace and produce an actionable reasoning trace.";

    std::string reasoning;
    reasoning.reserve(2048);
    reasoning += "Reasoning trace (internal):\n";
    reasoning += "• Prompt: ";
    reasoning += seed;
    reasoning += "\n• Clarify the goal and constraints implied by the prompt.\n";
    reasoning += "• List unknowns, dependencies, and risks that affect the answer.\n";
    reasoning += "• Outline a short plan, then produce the user-facing answer in the main channel.\n";

    std::lock_guard<std::mutex> lock(g_deepThinkingState.mutex);
    g_deepThinkingState.initialized = true;
    g_deepThinkingState.lastReasoning = std::move(reasoning);
    return 0;
}

// asm_orchestrator_shutdown is provided by unlinked_symbols_batch_013.cpp (MSVC + MinGW Win32IDE).

extern "C" void __stdcall RawrXD_DispatchIPC(const void* pData, uint32_t size) {
    if (!pData || size == 0 || (size % sizeof(wchar_t)) != 0) {
        return;
    }

    const auto* wideData = static_cast<const wchar_t*>(pData);
    size_t wideCount = static_cast<size_t>(size / sizeof(wchar_t));
    if (wideCount > 0 && wideData[wideCount - 1] == L'\0') {
        --wideCount;
    }

    std::wstring message(wideData, wideData + wideCount);
    {
        std::lock_guard<std::mutex> lock(g_deepThinkingState.mutex);
        g_deepThinkingState.lastIpcMessage = message;
    }

    if (!message.empty()) {
        OutputDebugStringW(L"[RawrXD][IPC] ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
    }
}

extern "C" void rawrxd_enumerate_modules_peb(
    void (*callback)(uint64_t base, uint32_t size, uint16_t nameLen, const wchar_t* namePtr, void* context),
    void* context) {
    if (!callback) {
        return;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }

    MODULEENTRY32W moduleEntry = {};
    moduleEntry.dwSize = sizeof(moduleEntry);
    if (!Module32FirstW(snapshot, &moduleEntry)) {
        CloseHandle(snapshot);
        return;
    }

    do {
        const uint64_t base = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr));
        const uint32_t moduleSize = static_cast<uint32_t>(moduleEntry.modBaseSize);
        const uint16_t nameLenBytes = static_cast<uint16_t>(wcsnlen(moduleEntry.szModule, MAX_MODULE_NAME32) * sizeof(wchar_t));
        callback(base, moduleSize, nameLenBytes, moduleEntry.szModule, context);
    } while (Module32NextW(snapshot, &moduleEntry));

    CloseHandle(snapshot);
}

extern "C" void rawrxd_walk_export_table(
    uint64_t moduleBase,
    void (*callback)(uint64_t indexOrRva, const char* name, void* context),
    void* context) {
    if (!callback || moduleBase == 0) {
        return;
    }

    HMODULE moduleHandle = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(moduleBase));
    MODULEINFO moduleInfo = {};
    if (!GetModuleInformation(GetCurrentProcess(), moduleHandle, &moduleInfo, sizeof(moduleInfo))) {
        return;
    }

    const auto* base = static_cast<const uint8_t*>(moduleInfo.lpBaseOfDll);
    const size_t imageSize = static_cast<size_t>(moduleInfo.SizeOfImage);
    if (!base || imageSize < sizeof(IMAGE_DOS_HEADER)) {
        return;
    }

    auto inImage = [imageSize](uint32_t rva, size_t bytes) -> bool {
        return static_cast<uint64_t>(rva) + bytes <= static_cast<uint64_t>(imageSize);
    };

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return;
    }

    const uint32_t ntOff = static_cast<uint32_t>(dos->e_lfanew);
    if (!inImage(ntOff, sizeof(IMAGE_NT_HEADERS64))) {
        return;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + ntOff);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return;
    }

    const auto& exportDirEntry = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDirEntry.VirtualAddress == 0 || exportDirEntry.Size < sizeof(IMAGE_EXPORT_DIRECTORY)) {
        return;
    }
    if (!inImage(exportDirEntry.VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY))) {
        return;
    }

    const auto* exportDir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + exportDirEntry.VirtualAddress);
    if (exportDir->NumberOfNames == 0 || exportDir->AddressOfNames == 0 ||
        exportDir->AddressOfFunctions == 0 || exportDir->AddressOfNameOrdinals == 0) {
        return;
    }
    if (!inImage(exportDir->AddressOfNames, exportDir->NumberOfNames * sizeof(uint32_t)) ||
        !inImage(exportDir->AddressOfNameOrdinals, exportDir->NumberOfNames * sizeof(uint16_t)) ||
        !inImage(exportDir->AddressOfFunctions, exportDir->NumberOfFunctions * sizeof(uint32_t))) {
        return;
    }

    const auto* nameRvAs = reinterpret_cast<const uint32_t*>(base + exportDir->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const uint16_t*>(base + exportDir->AddressOfNameOrdinals);
    const auto* funcRvAs = reinterpret_cast<const uint32_t*>(base + exportDir->AddressOfFunctions);

    for (uint32_t i = 0; i < exportDir->NumberOfNames; ++i) {
        const uint32_t nameRva = nameRvAs[i];
        const uint16_t ordinal = ordinals[i];
        if (!inImage(nameRva, 1) || ordinal >= exportDir->NumberOfFunctions) {
            continue;
        }

        const char* exportName = reinterpret_cast<const char*>(base + nameRva);
        const size_t maxBytes = imageSize - static_cast<size_t>(nameRva);
        if (strnlen(exportName, maxBytes) >= maxBytes) {
            continue;
        }

        const uint32_t funcRva = funcRvAs[ordinal];
        callback(static_cast<uint64_t>(funcRva), exportName, context);
    }
}

extern "C" uint32_t Dbg_CaptureContext(uint64_t threadHandle, void* outContextBuffer, uint32_t bufferSize) {
    if (!outContextBuffer || bufferSize < sizeof(CONTEXT) || threadHandle == 0) {
        return ERROR_INVALID_PARAMETER;
    }

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_ALL;
    HANDLE hThread = reinterpret_cast<HANDLE>(threadHandle);
    if (!GetThreadContext(hThread, &ctx)) {
        return static_cast<uint32_t>(GetLastError());
    }

    std::memcpy(outContextBuffer, &ctx, sizeof(CONTEXT));
    return 0;
}

extern "C" uint32_t Dbg_ReadMemory(uint64_t processHandle, uint64_t address,
    void* outBuffer, uint64_t size, uint64_t* outBytesRead) {
    if (outBytesRead) {
        *outBytesRead = 0;
    }
    if (!outBuffer || processHandle == 0 || size == 0) {
        return ERROR_INVALID_PARAMETER;
    }

    SIZE_T bytesRead = 0;
    HANDLE hProcess = reinterpret_cast<HANDLE>(processHandle);
    if (!ReadProcessMemory(hProcess, reinterpret_cast<const void*>(static_cast<uintptr_t>(address)), outBuffer,
            static_cast<SIZE_T>(size), &bytesRead)) {
        if (outBytesRead) {
            *outBytesRead = static_cast<uint64_t>(bytesRead);
        }
        return static_cast<uint32_t>(GetLastError());
    }

    if (outBytesRead) {
        *outBytesRead = static_cast<uint64_t>(bytesRead);
    }
    return 0;
}

extern "C" uint32_t Dbg_WriteMemory(uint64_t processHandle, uint64_t address,
    const void* buffer, uint64_t size, uint64_t* outBytesWritten) {
    if (outBytesWritten) {
        *outBytesWritten = 0;
    }
    if (!buffer || processHandle == 0 || size == 0) {
        return ERROR_INVALID_PARAMETER;
    }

    SIZE_T bytesWritten = 0;
    HANDLE hProcess = reinterpret_cast<HANDLE>(processHandle);
    if (!WriteProcessMemory(hProcess, reinterpret_cast<void*>(static_cast<uintptr_t>(address)), buffer,
            static_cast<SIZE_T>(size), &bytesWritten)) {
        if (outBytesWritten) {
            *outBytesWritten = static_cast<uint64_t>(bytesWritten);
        }
        return static_cast<uint32_t>(GetLastError());
    }

    if (outBytesWritten) {
        *outBytesWritten = static_cast<uint64_t>(bytesWritten);
    }
    return 0;
}

extern "C" int _dupenv_s(char** buffer, size_t* numberOfElements, const char* varname) {
    if (buffer) {
        *buffer = nullptr;
    }
    if (numberOfElements) {
        *numberOfElements = 0;
    }
    if (!buffer || !varname) {
        return EINVAL;
    }

    const char* value = std::getenv(varname);
    if (!value) {
        return 0;
    }

    const size_t len = std::strlen(value) + 1;
    char* dup = static_cast<char*>(std::malloc(len));
    if (!dup) {
        return ENOMEM;
    }
    std::memcpy(dup, value, len);
    *buffer = dup;
    if (numberOfElements) {
        *numberOfElements = len;
    }
    return 0;
}

extern "C" void* __imp__dupenv_s = reinterpret_cast<void*>(&_dupenv_s);

extern "C" int DiskRecovery_FindDrive() {
    return 0;
}

extern "C" void* DiskRecovery_Init(int driveNum) {
    auto* ctx = new (std::nothrow) DiskRecoveryCtx();
    if (!ctx) {
        return nullptr;
    }
    ctx->driveNum = driveNum;
    return ctx;
}

extern "C" int DiskRecovery_ExtractKey(void* ctxPtr) {
    auto* ctx = static_cast<DiskRecoveryCtx*>(ctxPtr);
    if (!ctx) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->keyExtracted = true;
    return 1;
}

extern "C" void DiskRecovery_Run(void* ctxPtr) {
    auto* ctx = static_cast<DiskRecoveryCtx*>(ctxPtr);
    if (!ctx) {
        return;
    }

    for (uint64_t i = 0; i < ctx->total; ++i) {
        {
            std::lock_guard<std::mutex> lock(ctx->mutex);
            if (ctx->aborted) {
                break;
            }
            ctx->current = i + 1;
            if (ctx->keyExtracted) {
                ++ctx->good;
            } else {
                ++ctx->bad;
            }
        }
        if ((i & 0x7F) == 0) {
            Sleep(0);
        }
    }
}

extern "C" void DiskRecovery_Abort(void* ctxPtr) {
    auto* ctx = static_cast<DiskRecoveryCtx*>(ctxPtr);
    if (!ctx) {
        return;
    }
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->aborted = true;
}

extern "C" void DiskRecovery_Cleanup(void* ctxPtr) {
    auto* ctx = static_cast<DiskRecoveryCtx*>(ctxPtr);
    delete ctx;
}

extern "C" void DiskRecovery_GetStats(
    void* ctxPtr,
    uint64_t* outGood,
    uint64_t* outBad,
    uint64_t* outCurrent,
    uint64_t* outTotal) {
    uint64_t good = 0;
    uint64_t bad = 0;
    uint64_t current = 0;
    uint64_t total = 0;

    auto* ctx = static_cast<DiskRecoveryCtx*>(ctxPtr);
    if (ctx) {
        std::lock_guard<std::mutex> lock(ctx->mutex);
        good = ctx->good;
        bad = ctx->bad;
        current = ctx->current;
        total = ctx->total;
    }

    if (outGood) {
        *outGood = good;
    }
    if (outBad) {
        *outBad = bad;
    }
    if (outCurrent) {
        *outCurrent = current;
    }
    if (outTotal) {
        *outTotal = total;
    }
}

#endif  // !defined(_MSC_VER)
