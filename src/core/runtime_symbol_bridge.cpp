#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <psapi.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

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

}  // extern "C"
