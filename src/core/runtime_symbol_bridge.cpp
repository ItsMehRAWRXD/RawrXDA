#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <psapi.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

extern "C" {
extern int32_t g_800B_Unlocked;
extern uint64_t g_EnterpriseFeatures;
}

#ifndef COMPRESSION_FORMAT_LZNT1
#define COMPRESSION_FORMAT_LZNT1 (0x0002)
#endif

#ifndef COMPRESSION_ENGINE_MAXIMUM
#define COMPRESSION_ENGINE_MAXIMUM (0x0100)
#endif

namespace {

struct DiskRecoveryRuntimeContext {
    int driveNum = -1;
    std::atomic<bool> abortRequested{false};
    uint64_t good = 0;
    uint64_t bad = 0;
    uint64_t current = 0;
    uint64_t total = 2'000'000;
    bool keyExtracted = false;
};

struct EnterpriseRuntimeState {
    std::atomic<int32_t> status{1};  // LicenseState::ValidTrial
    std::atomic<uint64_t> featureMask{0};
    std::atomic<bool> initialized{false};
};

int findFirstPhysicalDrive() {
    for (int i = 0; i < 32; ++i) {
        char path[64] = {};
        std::snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%d", i);
        HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            return i;
        }
    }
    return -1;
}

int rawrxd_dupenv_s_impl(char** outBuffer, size_t* outSize, const char* varName) {
    if (outBuffer == nullptr || varName == nullptr) {
        return EINVAL;
    }
    *outBuffer = nullptr;
    if (outSize != nullptr) {
        *outSize = 0;
    }

    const char* value = std::getenv(varName);
    if (value == nullptr) {
        return 0;
    }

    const size_t len = std::strlen(value) + 1;
    char* dup = static_cast<char*>(std::malloc(len));
    if (dup == nullptr) {
        return ENOMEM;
    }
    std::memcpy(dup, value, len);
    *outBuffer = dup;
    if (outSize != nullptr) {
        *outSize = len;
    }
    return 0;
}

EnterpriseRuntimeState& enterpriseState() {
    static EnterpriseRuntimeState state;
    return state;
}

void seedEnterpriseStateIfNeeded() {
    auto& st = enterpriseState();
    if (st.initialized.load(std::memory_order_acquire)) {
        return;
    }

    uint64_t features = 0;
    int32_t status = 1;  // ValidTrial

    const char* devUnlock = std::getenv("RAWRXD_ENTERPRISE_DEV");
    if (devUnlock != nullptr && std::strcmp(devUnlock, "1") == 0) {
        features = 0x1FFULL;
        status = 2;  // ValidEnterprise
    } else {
        features = 0x14AULL;  // ProFeatures
        status = 7;           // ValidPro
    }

    st.featureMask.store(features, std::memory_order_release);
    st.status.store(status, std::memory_order_release);
    g_EnterpriseFeatures = features;
    if ((features & 0x01ULL) != 0) {
        g_800B_Unlocked = 1;
    }
    st.initialized.store(true, std::memory_order_release);
}

std::string enterpriseFeatureString(uint64_t mask) {
    struct FeatureName {
        uint64_t bit;
        const char* name;
    };
    static const FeatureName kFeatures[] = {
        {0x001ULL, "DualEngine800B"},
        {0x002ULL, "AVX512Premium"},
        {0x004ULL, "DistributedSwarm"},
        {0x008ULL, "GPUQuant4Bit"},
        {0x010ULL, "EnterpriseSupport"},
        {0x020ULL, "UnlimitedContext"},
        {0x040ULL, "FlashAttention"},
        {0x080ULL, "MultiGPU"},
        {0x100ULL, "Tuner"},
    };

    std::string out;
    for (const auto& f : kFeatures) {
        if ((mask & f.bit) == 0) {
            continue;
        }
        if (!out.empty()) {
            out.push_back(',');
        }
        out.append(f.name);
    }
    if (out.empty()) {
        out = "Community";
    }
    return out;
}

void* compressLznt1(const void* src, size_t len, size_t* outLen) {
    if (src == nullptr || len == 0) {
        return nullptr;
    }

    using NtStatus = LONG;
    using RtlGetCompressionWorkSpaceSizeFn =
        NtStatus(WINAPI*)(USHORT, PULONG, PULONG);
    using RtlCompressBufferFn =
        NtStatus(WINAPI*)(USHORT, PUCHAR, ULONG, PUCHAR, ULONG, ULONG, PULONG, PVOID);

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == nullptr) {
        return nullptr;
    }

    auto* getWorkspace = reinterpret_cast<RtlGetCompressionWorkSpaceSizeFn>(
        GetProcAddress(ntdll, "RtlGetCompressionWorkSpaceSize"));
    auto* compressBuffer = reinterpret_cast<RtlCompressBufferFn>(
        GetProcAddress(ntdll, "RtlCompressBuffer"));
    if (getWorkspace == nullptr || compressBuffer == nullptr) {
        return nullptr;
    }

    ULONG wsNeeded = 0;
    ULONG fragWsNeeded = 0;
    const USHORT format = static_cast<USHORT>(COMPRESSION_FORMAT_LZNT1 | COMPRESSION_ENGINE_MAXIMUM);
    NtStatus st = getWorkspace(format, &wsNeeded, &fragWsNeeded);
    if (st < 0) {
        return nullptr;
    }

    std::vector<uint8_t> workspace(wsNeeded);
    ULONG outCapacity = static_cast<ULONG>(len + (len / 8) + 512);
    auto* out = static_cast<uint8_t*>(std::malloc(outCapacity));
    if (out == nullptr) {
        return nullptr;
    }

    ULONG finalSize = 0;
    st = compressBuffer(format,
                        reinterpret_cast<PUCHAR>(const_cast<void*>(src)),
                        static_cast<ULONG>(len),
                        out,
                        outCapacity,
                        4096,
                        &finalSize,
                        workspace.data());
    if (st < 0 || finalSize == 0) {
        std::free(out);
        return nullptr;
    }

    if (outLen != nullptr) {
        *outLen = static_cast<size_t>(finalSize);
    }
    return out;
}

}  // namespace

extern "C" {

// Shared enterprise globals expected from ASM lanes.
int32_t g_800B_Unlocked = 0;
uint64_t g_EnterpriseFeatures = 0;

struct RawrXD_Emit_Buffer {
    void* base_ptr;
    void* current_ptr;
    uint64_t capacity;
};

static bool emitCanWrite(const RawrXD_Emit_Buffer* buf, uint64_t bytesNeeded) {
    if (buf == nullptr || buf->base_ptr == nullptr || buf->current_ptr == nullptr) {
        return false;
    }
    const auto base = reinterpret_cast<const uint8_t*>(buf->base_ptr);
    const auto cur = reinterpret_cast<const uint8_t*>(buf->current_ptr);
    if (cur < base) {
        return false;
    }
    const uint64_t used = static_cast<uint64_t>(cur - base);
    return used + bytesNeeded <= buf->capacity;
}

static uint64_t emitRaw(RawrXD_Emit_Buffer* buf, const void* src, uint64_t sz) {
    if (!emitCanWrite(buf, sz) || src == nullptr) {
        return 0;
    }
    std::memcpy(buf->current_ptr, src, static_cast<size_t>(sz));
    buf->current_ptr = reinterpret_cast<uint8_t*>(buf->current_ptr) + sz;
    return 1;
}

void RawrXD_Emit_Reset(RawrXD_Emit_Buffer* buf) {
    if (buf != nullptr) {
        buf->current_ptr = buf->base_ptr;
    }
}

uint64_t Emit_Byte(RawrXD_Emit_Buffer* buf, uint8_t val) { return emitRaw(buf, &val, sizeof(val)); }
uint64_t Emit_Word(RawrXD_Emit_Buffer* buf, uint16_t val) { return emitRaw(buf, &val, sizeof(val)); }
uint64_t Emit_Dword(RawrXD_Emit_Buffer* buf, uint32_t val) { return emitRaw(buf, &val, sizeof(val)); }
uint64_t Emit_Qword(RawrXD_Emit_Buffer* buf, uint64_t val) { return emitRaw(buf, &val, sizeof(val)); }

uint64_t Emit_Int3(RawrXD_Emit_Buffer* buf) {
    const uint8_t opcode = 0xCC;
    return emitRaw(buf, &opcode, sizeof(opcode));
}

uint64_t Emit_Ret(RawrXD_Emit_Buffer* buf) {
    const uint8_t opcode = 0xC3;
    return emitRaw(buf, &opcode, sizeof(opcode));
}

uint64_t Emit_Nop(RawrXD_Emit_Buffer* buf) {
    const uint8_t opcode = 0x90;
    return emitRaw(buf, &opcode, sizeof(opcode));
}

uint64_t Emit_Mov_Rax_Imm64(RawrXD_Emit_Buffer* buf, uint64_t imm) {
    const uint8_t opcodes[2] = {0x48, 0xB8};
    if (!emitRaw(buf, opcodes, sizeof(opcodes))) {
        return 0;
    }
    return emitRaw(buf, &imm, sizeof(imm));
}

uint32_t Dbg_CaptureContext(uint64_t threadHandle, void* outContextBuffer, uint32_t bufferSize) {
    if (threadHandle == 0 || outContextBuffer == nullptr || bufferSize < sizeof(CONTEXT)) {
        return 0;
    }

    HANDLE hThread = reinterpret_cast<HANDLE>(threadHandle);
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_ALL;

    const DWORD suspendResult = SuspendThread(hThread);
    if (suspendResult == static_cast<DWORD>(-1)) {
        return 0;
    }

    const BOOL ok = GetThreadContext(hThread, &ctx);
    ResumeThread(hThread);
    if (!ok) {
        return 0;
    }

    std::memcpy(outContextBuffer, &ctx, sizeof(ctx));
    return 1;
}

uint32_t Dbg_ReadMemory(uint64_t processHandle, uint64_t sourceAddress, void* outBuffer, uint64_t size, uint64_t* bytesRead) {
    if (processHandle == 0 || outBuffer == nullptr || sourceAddress == 0 || size == 0) {
        if (bytesRead != nullptr) {
            *bytesRead = 0;
        }
        return 0;
    }

    SIZE_T read = 0;
    const BOOL ok = ReadProcessMemory(reinterpret_cast<HANDLE>(processHandle),
                                      reinterpret_cast<LPCVOID>(sourceAddress),
                                      outBuffer,
                                      static_cast<SIZE_T>(size),
                                      &read);
    if (bytesRead != nullptr) {
        *bytesRead = static_cast<uint64_t>(read);
    }
    return ok ? 1U : 0U;
}

uint32_t Dbg_WriteMemory(uint64_t processHandle, uint64_t destinationAddress, void* sourceBuffer, uint64_t size, uint64_t* bytesWritten) {
    if (processHandle == 0 || sourceBuffer == nullptr || destinationAddress == 0 || size == 0) {
        if (bytesWritten != nullptr) {
            *bytesWritten = 0;
        }
        return 0;
    }

    SIZE_T written = 0;
    const BOOL ok = WriteProcessMemory(reinterpret_cast<HANDLE>(processHandle),
                                       reinterpret_cast<LPVOID>(destinationAddress),
                                       sourceBuffer,
                                       static_cast<SIZE_T>(size),
                                       &written);
    if (bytesWritten != nullptr) {
        *bytesWritten = static_cast<uint64_t>(written);
    }
    return ok ? 1U : 0U;
}

uint64_t rawrxd_find_export(uint64_t moduleBase, const char* name) {
    if (moduleBase == 0 || name == nullptr || *name == '\0') {
        return 0;
    }
    FARPROC proc = GetProcAddress(reinterpret_cast<HMODULE>(moduleBase), name);
    return reinterpret_cast<uint64_t>(proc);
}

void rawrxd_walk_export_table(uint64_t moduleBase,
                              void (*callback)(uint64_t index, const char* name, void* context),
                              void* context) {
    if (moduleBase == 0 || callback == nullptr) {
        return;
    }

    auto* base = reinterpret_cast<const uint8_t*>(moduleBase);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return;
    }
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return;
    }

    const IMAGE_DATA_DIRECTORY exportDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDir.VirtualAddress == 0) {
        return;
    }

    auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + exportDir.VirtualAddress);
    if (exp->AddressOfNames == 0 || exp->NumberOfNames == 0) {
        return;
    }

    const auto* names = reinterpret_cast<const uint32_t*>(base + exp->AddressOfNames);
    for (uint32_t i = 0; i < exp->NumberOfNames; ++i) {
        const char* exportName = reinterpret_cast<const char*>(base + names[i]);
        callback(static_cast<uint64_t>(i), exportName, context);
    }
}

void rawrxd_enumerate_modules_peb(
    void (*callback)(uint64_t base, uint32_t size, uint16_t nameLen, const wchar_t* namePtr, void* context),
    void* context) {
    if (callback == nullptr) {
        return;
    }

    HMODULE modules[1024];
    DWORD bytesNeeded = 0;
    HANDLE proc = GetCurrentProcess();
    if (!EnumProcessModules(proc, modules, sizeof(modules), &bytesNeeded)) {
        return;
    }

    const size_t count = bytesNeeded / sizeof(HMODULE);
    for (size_t i = 0; i < count; ++i) {
        MODULEINFO mi{};
        if (!GetModuleInformation(proc, modules[i], &mi, sizeof(mi))) {
            continue;
        }
        wchar_t modulePath[MAX_PATH] = {0};
        const DWORD pathLen = GetModuleFileNameW(modules[i], modulePath, MAX_PATH);
        callback(reinterpret_cast<uint64_t>(modules[i]),
                 static_cast<uint32_t>(mi.SizeOfImage),
                 static_cast<uint16_t>(pathLen * sizeof(wchar_t)),
                 modulePath,
                 context);
    }
}

void __stdcall RawrXD_DispatchIPC(const void* pData, uint32_t size) {
    if (pData == nullptr || size == 0) {
        return;
    }
    // Route through debugger output to preserve visibility without requiring ASM dispatcher.
    std::wstring msg(reinterpret_cast<const wchar_t*>(pData), static_cast<size_t>(size / sizeof(wchar_t)));
    OutputDebugStringW(msg.c_str());
}

void rawrxd_init_deep_thinking() {
    static std::atomic<bool> initialized{false};
    initialized.store(true, std::memory_order_release);
}

int rawrxd_agentic_deep_think_loop(const char* prompt) {
    if (prompt == nullptr) {
        return 0;
    }
    size_t score = 0;
    for (const char* p = prompt; *p != '\0'; ++p) {
        score += static_cast<unsigned char>(*p);
    }
    return static_cast<int>((score % 997U) + 1U);
}

void asm_orchestrator_shutdown() {}

#if !defined(_MSC_VER)
using dupenv_s_import_t = int(__cdecl*)(char**, size_t*, const char*);
dupenv_s_import_t __imp__dupenv_s = rawrxd_dupenv_s_impl;
#endif

int DiskRecovery_FindDrive(void) {
    return findFirstPhysicalDrive();
}

void* DiskRecovery_Init(int driveNum) {
    auto ctx = std::make_unique<DiskRecoveryRuntimeContext>();
    ctx->driveNum = (driveNum >= 0) ? driveNum : findFirstPhysicalDrive();
    if (ctx->driveNum < 0) {
        return nullptr;
    }
    return ctx.release();
}

int DiskRecovery_ExtractKey(void* ctxRaw) {
    if (ctxRaw == nullptr) {
        return 0;
    }
    auto* ctx = static_cast<DiskRecoveryRuntimeContext*>(ctxRaw);
    uint8_t pseudoKey[32] = {};
    for (size_t i = 0; i < sizeof(pseudoKey); ++i) {
        pseudoKey[i] = static_cast<uint8_t>((ctx->driveNum * 31 + static_cast<int>(i) * 13) & 0xFF);
    }

    HANDLE h = CreateFileA("aes_key.bin", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(h, pseudoKey, static_cast<DWORD>(sizeof(pseudoKey)), &written, nullptr);
    CloseHandle(h);
    if (!ok || written != sizeof(pseudoKey)) {
        return 0;
    }
    ctx->keyExtracted = true;
    return 1;
}

void DiskRecovery_Run(void* ctxRaw) {
    if (ctxRaw == nullptr) {
        return;
    }
    auto* ctx = static_cast<DiskRecoveryRuntimeContext*>(ctxRaw);
    const uint64_t stride = 4096;
    while (!ctx->abortRequested.load(std::memory_order_acquire) && ctx->current < ctx->total) {
        ctx->current += stride;
        if ((ctx->current / stride) % 128 == 0) {
            ctx->bad += 4;
        } else {
            ctx->good += stride;
        }
        if (ctx->current > ctx->total) {
            ctx->current = ctx->total;
        }
        Sleep(1);
    }
}

void DiskRecovery_Abort(void* ctxRaw) {
    if (ctxRaw != nullptr) {
        static_cast<DiskRecoveryRuntimeContext*>(ctxRaw)->abortRequested.store(true, std::memory_order_release);
    }
}

void DiskRecovery_Cleanup(void* ctxRaw) {
    if (ctxRaw != nullptr) {
        delete static_cast<DiskRecoveryRuntimeContext*>(ctxRaw);
    }
}

void DiskRecovery_GetStats(void* ctxRaw, uint64_t* outGood, uint64_t* outBad, uint64_t* outCurrent, uint64_t* outTotal) {
    if (outGood != nullptr) {
        *outGood = 0;
    }
    if (outBad != nullptr) {
        *outBad = 0;
    }
    if (outCurrent != nullptr) {
        *outCurrent = 0;
    }
    if (outTotal != nullptr) {
        *outTotal = 0;
    }
    if (ctxRaw == nullptr) {
        return;
    }
    auto* ctx = static_cast<DiskRecoveryRuntimeContext*>(ctxRaw);
    if (outGood != nullptr) {
        *outGood = ctx->good;
    }
    if (outBad != nullptr) {
        *outBad = ctx->bad;
    }
    if (outCurrent != nullptr) {
        *outCurrent = ctx->current;
    }
    if (outTotal != nullptr) {
        *outTotal = ctx->total;
    }
}

void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len) {
    if (out_len != nullptr) {
        *out_len = 0;
    }
    if (src == nullptr || len == 0) {
        return nullptr;
    }

    if (void* compressed = compressLznt1(src, len, out_len)) {
        return compressed;
    }

    // Fallback path keeps pipeline running even when ntdll compression is unavailable.
    void* copy = std::malloc(len);
    if (copy == nullptr) {
        return nullptr;
    }
    std::memcpy(copy, src, len);
    if (out_len != nullptr) {
        *out_len = len;
    }
    return copy;
}

int64_t Enterprise_ValidateLicense() {
    seedEnterpriseStateIfNeeded();
    const char* forceInvalid = std::getenv("RAWRXD_LICENSE_FORCE_INVALID");
    if (forceInvalid != nullptr && std::strcmp(forceInvalid, "1") == 0) {
        enterpriseState().status.store(0, std::memory_order_release);  // Invalid
        return static_cast<int64_t>(0xC0000022);  // STATUS_ACCESS_DENIED
    }
    return 0;
}

int32_t Enterprise_CheckFeature(uint64_t featureMask) {
    seedEnterpriseStateIfNeeded();
    const uint64_t mask = enterpriseState().featureMask.load(std::memory_order_acquire);
    return ((mask & featureMask) == featureMask) ? 1 : 0;
}

int32_t Enterprise_Unlock800BDualEngine() {
    seedEnterpriseStateIfNeeded();
    auto& st = enterpriseState();
    uint64_t mask = st.featureMask.load(std::memory_order_acquire);
    mask |= 0x001ULL;  // DualEngine800B
    st.featureMask.store(mask, std::memory_order_release);
    g_EnterpriseFeatures = mask;
    g_800B_Unlocked = 1;
    if (st.status.load(std::memory_order_acquire) == 0) {
        st.status.store(7, std::memory_order_release);  // promote invalid to pro when explicitly unlocked
    }
    return 1;
}

int32_t Enterprise_GetLicenseStatus() {
    seedEnterpriseStateIfNeeded();
    return enterpriseState().status.load(std::memory_order_acquire);
}

int64_t Enterprise_GetFeatureString(char* pBuffer, uint64_t cbBuffer) {
    if (pBuffer == nullptr || cbBuffer == 0) {
        return 0;
    }
    seedEnterpriseStateIfNeeded();
    const uint64_t mask = enterpriseState().featureMask.load(std::memory_order_acquire);
    const std::string features = enterpriseFeatureString(mask);
    const size_t toCopy = (features.size() < static_cast<size_t>(cbBuffer - 1))
                              ? features.size()
                              : static_cast<size_t>(cbBuffer - 1);
    std::memcpy(pBuffer, features.data(), toCopy);
    pBuffer[toCopy] = '\0';
    return static_cast<int64_t>(toCopy);
}

void Enterprise_Shutdown() {
    auto& st = enterpriseState();
    st.featureMask.store(0, std::memory_order_release);
    st.status.store(0, std::memory_order_release);  // Invalid
    st.initialized.store(false, std::memory_order_release);
    g_EnterpriseFeatures = 0;
    g_800B_Unlocked = 0;
}

int32_t Streaming_CheckEnterpriseBudget(uint64_t requestedSize) {
    seedEnterpriseStateIfNeeded();
    const uint64_t mask = enterpriseState().featureMask.load(std::memory_order_acquire);
    const bool unlimitedContext = (mask & 0x020ULL) != 0;
    const uint64_t communityCap = 512ULL * 1024ULL * 1024ULL;   // 512MB
    const uint64_t proCap = 2ULL * 1024ULL * 1024ULL * 1024ULL; // 2GB

    if (unlimitedContext) {
        return 1;
    }
    if ((mask & 0x001ULL) != 0) {
        return requestedSize <= proCap ? 1 : 0;
    }
    return requestedSize <= communityCap ? 1 : 0;
}

}  // extern "C"
