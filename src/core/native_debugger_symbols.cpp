// native_debugger_symbols.cpp — P0 Source-level symbol resolution (dbghelp)
// Maps RIP → file:line using PDB; StackWalk64 with source info; line→address for breakpoints.
// Lightweight dbghelp-only API; complements NativeDebuggerEngine (DbgEng).

#include "native_debugger_types.h"
#include <windows.h>
#include <dbghelp.h>
#include <string>
#include <vector>

#pragma comment(lib, "dbghelp.lib")

namespace RawrXD {
namespace Debugger {

struct SourceLocation {
    std::string file;
    int line = 0;
    int column = 0;
    std::string function;
    uint64_t address = 0;
};

class SymbolResolver {
    HANDLE m_hProcess = nullptr;
    bool m_initialized = false;

public:
    SymbolResolver() {
        m_hProcess = GetCurrentProcess();
    }

    void setProcess(HANDLE hProcess) {
        m_hProcess = hProcess;
    }

    bool initialize() {
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        m_initialized = SymInitialize(m_hProcess, nullptr, TRUE) != FALSE;
        return m_initialized;
    }

    void shutdown() {
        if (m_initialized && m_hProcess) {
            SymCleanup(m_hProcess);
            m_initialized = false;
        }
    }

    SourceLocation resolveAddress(uint64_t address) {
        SourceLocation loc;
        loc.address = address;
        if (!m_initialized || !m_hProcess) return loc;

        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(char)];
        PSYMBOL_INFO pSym = (PSYMBOL_INFO)buffer;
        pSym->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSym->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        if (SymFromAddr(m_hProcess, address, &displacement, pSym))
            loc.function = pSym->Name;

        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD lineDisp = 0;
        if (SymGetLineFromAddr64(m_hProcess, address, &lineDisp, &line)) {
            loc.file = line.FileName;
            loc.line = line.LineNumber;
        }
        return loc;
    }

    std::vector<SourceLocation> getStackTrace(HANDLE hThread, const CONTEXT* ctx) {
        std::vector<SourceLocation> stack;
        if (!m_initialized || !ctx) return stack;

        STACKFRAME64 frame = {};
        frame.AddrPC.Offset = ctx->Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = ctx->Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = ctx->Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        CONTEXT currentCtx = *ctx;
        for (int i = 0; i < 64; i++) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, m_hProcess, hThread, &frame,
                             &currentCtx, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
                break;
            if (frame.AddrPC.Offset == 0) break;

            SourceLocation loc = resolveAddress(frame.AddrPC.Offset);
            loc.address = frame.AddrPC.Offset;
            stack.push_back(loc);
        }
        return stack;
    }

    void loadModule(const char* path, uint64_t baseAddr) {
        if (!m_initialized || !m_hProcess) return;
        SymLoadModule64(m_hProcess, nullptr, path, nullptr, baseAddr, 0);
    }

    void unloadModule(uint64_t baseAddr) {
        if (!m_initialized || !m_hProcess) return;
        SymUnloadModule64(m_hProcess, baseAddr);
    }
};

static SymbolResolver g_symbolResolver;

} // namespace Debugger
} // namespace RawrXD

extern "C" {

#ifdef _WIN32
__declspec(dllexport)
#endif
int RawrXD_DebuggerSymbols_Init(void) {
    return RawrXD::Debugger::g_symbolResolver.initialize() ? 1 : 0;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void RawrXD_DebuggerSymbols_Shutdown(void) {
    RawrXD::Debugger::g_symbolResolver.shutdown();
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int RawrXD_DebuggerSymbols_GetSourceForAddress(
    uint64_t address,
    char* fileBuf,
    int fileBufSize,
    int* outLine)
{
    if (!fileBuf || fileBufSize <= 0 || !outLine) return 0;
    auto loc = RawrXD::Debugger::g_symbolResolver.resolveAddress(address);
    if (loc.file.empty()) return 0;
    size_t len = loc.file.size();
    if (len >= (size_t)fileBufSize) len = (size_t)fileBufSize - 1;
    memcpy(fileBuf, loc.file.c_str(), len);
    fileBuf[len] = '\0';
    *outLine = loc.line;
    return 1;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void RawrXD_DebuggerSymbols_GetStackTrace(
    void* hThread,
    void* context,
    void (*callback)(const char* file, int line, const char* func, uint64_t addr, void* userData),
    void* userData)
{
    if (!hThread || !context || !callback) return;
    HANDLE thread = (HANDLE)hThread;
    CONTEXT* ctx = (CONTEXT*)context;
    auto stack = RawrXD::Debugger::g_symbolResolver.getStackTrace(thread, ctx);
    for (const auto& loc : stack)
        callback(loc.file.c_str(), loc.line, loc.function.c_str(), loc.address, userData);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void RawrXD_DebuggerSymbols_LoadModule(const char* path, uint64_t baseAddr) {
    RawrXD::Debugger::g_symbolResolver.loadModule(path, baseAddr);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void RawrXD_DebuggerSymbols_UnloadModule(uint64_t baseAddr) {
    RawrXD::Debugger::g_symbolResolver.unloadModule(baseAddr);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void RawrXD_DebuggerSymbols_SetProcess(void* hProcess) {
    RawrXD::Debugger::g_symbolResolver.setProcess((HANDLE)hProcess);
}

} // extern "C"
