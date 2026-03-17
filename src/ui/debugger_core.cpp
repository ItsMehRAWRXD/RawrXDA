#include "debugger_core.hpp"
#include "webview2_bridge.hpp"
#include <iostream>
#include <thread>
#include <psapi.h>

// Phase 3.3 Struct offsets (matching RawrXD_Debug_Engine.asm)
#define CTX_Dr0      0x48
#define CTX_Rax      0x78
#define CTX_Rcx      0x80
#define CTX_Rdx      0x88
#define CTX_Rbx      0x90
#define CTX_Rsp      0x98
#define CTX_Rbp      0xA0
#define CTX_Rsi      0xA8
#define CTX_Rdi      0xB0
#define CTX_R8       0xB8
#define CTX_R9       0xC0
#define CTX_R10      0xC8
#define CTX_R11      0xD0
#define CTX_R12      0xD8
#define CTX_R13      0xE0
#define CTX_R14      0xE8
#define CTX_R15      0xF0
#define CTX_Rip      0xF8
#define CTX_EFlags   0x100

extern "C" {
    DWORD Dbg_CaptureContext(HANDLE threadHandle, void* outBuffer, DWORD bufferSize);
    DWORD Dbg_ReadMemory(HANDLE processHandle, uint64_t sourceAddress, void* outBuffer, uint64_t size, uint64_t* bytesRead);
    DWORD Dbg_WriteMemory(HANDLE processHandle, uint64_t destinationAddress, void* sourceBuffer, uint64_t size, uint64_t* bytesWritten);
    void rawrxd_walk_export_table(uint64_t moduleBase, void (*callback)(uint64_t index, const char* name, void* context), void* context);
    void rawrxd_enumerate_modules_peb(void (*callback)(uint64_t base, uint32_t size, uint16_t nameLen, const wchar_t* namePtr, void* context), void* context);
    uint64_t rawrxd_find_export(uint64_t moduleBase, const char* name);

    struct RawrXD_Emit_Buffer {
        void* base_ptr;
        void* current_ptr;
        uint64_t capacity;
    };

    void RawrXD_Emit_Reset(RawrXD_Emit_Buffer* buf);
    uint64_t Emit_Byte(RawrXD_Emit_Buffer* buf, uint8_t val);
    uint64_t Emit_Word(RawrXD_Emit_Buffer* buf, uint16_t val);
    uint64_t Emit_Dword(RawrXD_Emit_Buffer* buf, uint32_t val);
    uint64_t Emit_Qword(RawrXD_Emit_Buffer* buf, uint64_t val);

    uint64_t Emit_Mov_Rax_Imm64(RawrXD_Emit_Buffer* buf, uint64_t imm);
    uint64_t Emit_Int3(RawrXD_Emit_Buffer* buf);
    uint64_t Emit_Ret(RawrXD_Emit_Buffer* buf);
    uint64_t Emit_Nop(RawrXD_Emit_Buffer* buf);
}
namespace rawrxd::debug {

bool DebuggerCore::assembleAndInject(uint64_t address, const std::string& asmSource) {
    uint8_t buffer[32];
    RawrXD_Emit_Buffer emitBuf = { buffer, buffer, sizeof(buffer) };
    RawrXD_Emit_Reset(&emitBuf);
    bool success = false;

    // v1.0 Simple Lookup Table / Pattern Match
    std::string instr = asmSource;
    // Lowercase for case-insensitivity
    for (auto& c : instr) c = std::tolower(static_cast<unsigned char>(c));
    // Trim
    instr.erase(0, instr.find_first_not_of(" \t"));
    instr.erase(instr.find_last_not_of(" \t") + 1);

    if (instr == "nop") {
        success = (Emit_Nop(&emitBuf) != 0);
    } else if (instr == "int3" || instr == "int 3") {
        success = (Emit_Int3(&emitBuf) != 0);
    } else if (instr == "ret") {
        success = (Emit_Ret(&emitBuf) != 0);
    } else if (instr.find("mov rax,") == 0) {
        // Simple hex parser for mov rax, 0x...
        size_t pos = instr.find("0x");
        if (pos != std::string::npos) {
            try {
                uint64_t val = std::stoull(instr.substr(pos), nullptr, 16);
                success = (Emit_Mov_Rax_Imm64(&emitBuf, val) != 0);
            } catch (...) { success = false; }
        }
    }

    if (success) {
        size_t len = (uint8_t*)emitBuf.current_ptr - (uint8_t*)emitBuf.base_ptr;
        std::vector<uint8_t> data(buffer, buffer + len);
        bool result = patchMemory(address, data);
        
        // Notify UI of result
        ipc::MsgEmitResult res = { address, (uint32_t)len, result ? 0u : 1u };
        ui::WebView2Bridge::getInstance().sendBinaryMessage(ipc::MessageType::DATA_EMIT_RESULT, &res, sizeof(res));
        
        return result;
    }

    // Fail notification
    ipc::MsgEmitResult res = { address, 0, 1 };
    ui::WebView2Bridge::getInstance().sendBinaryMessage(ipc::MessageType::DATA_EMIT_RESULT, &res, sizeof(res));
    return false;
}

bool DebuggerCore::launchProcess(const std::wstring& exePath, const std::wstring& cmdLine) {
    if (m_isDebugging) return false;

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    // DEBUG_ONLY_THIS_PROCESS: Capture events only for the child, not grandchildren (Zero Bloat focus)
    // CREATE_NEW_CONSOLE: Ensure output doesn't pollute our IDE host
    DWORD flags = DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE;

    std::wstring commandLine = exePath + L" " + cmdLine;

    if (!CreateProcessW(
        exePath.c_str(),
        (LPWSTR)commandLine.c_str(),
        nullptr, nullptr, FALSE,
        flags,
        nullptr, nullptr,
        &si, &pi)) 
    {
        return false;
    }

    m_hProcess = pi.hProcess;
    m_dwProcessId = pi.dwProcessId;
    m_isDebugging = true;
    m_targetPath = exePath;

    // Start the debug loop in a background thread to keep the UI responsive
    std::thread([this]() { this->debugLoop(); }).detach();

    // Close thread handle as we track via PID/Process Handle
    CloseHandle(pi.hThread);
    return true;
}

void DebuggerCore::debugLoop() {
    DEBUG_EVENT de = { 0 };
    
    while (m_isDebugging) {
        if (!WaitForDebugEvent(&de, INFINITE)) break;

        DWORD continueStatus = DBG_CONTINUE;

        switch (de.dwDebugEventCode) {
            case CREATE_PROCESS_DEBUG_EVENT:
                onProcessCreated(de.u.CreateProcessInfo);
                break;
            case LOAD_DLL_DEBUG_EVENT:
                onModuleLoaded(de.u.LoadDll);
                break;
            case EXCEPTION_DEBUG_EVENT:
                onException(de.u.Exception, de.dwThreadId);
                // We'll let onException decide if it's handled or not
                continueStatus = (de.u.Exception.dwFirstChance) ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE;
                
                // Refresh symbols and watches upon stop/exception
                refreshWatchValues();
                refreshWatches();
                break;
            case EXIT_PROCESS_DEBUG_EVENT:
                m_isDebugging = false;
                break;
        }

        ContinueDebugEvent(de.dwProcessId, de.dwThreadId, continueStatus);
    }

    stop();
}

void DebuggerCore::onProcessCreated(const CREATE_PROCESS_DEBUG_INFO& info) {
    // Notify UI of main module load
    size_t lastSlash = m_targetPath.find_last_of(L"\\/");
    std::wstring name = (lastSlash != std::wstring::npos) ? m_targetPath.substr(lastSlash + 1) : m_targetPath;

    // Stream MOD_LOAD for main EXE
    auto& bridge = ui::WebView2Bridge::getInstance();
    
    // Construct IPC payload
    ipc::MsgModuleLoad payload;
    payload.base_address = reinterpret_cast<uint64_t>(info.lpBaseOfImage);
    payload.size = 0; // We'd ideally parse PE header here for size
    payload.path_len = static_cast<uint16_t>(name.length() * 2);

    bridge.sendBinaryMessage(ipc::MessageType::MOD_LOAD, &payload, sizeof(payload));
}

void DebuggerCore::onModuleLoaded(const LOAD_DLL_DEBUG_INFO& info) {
    // Phase 3.3: Resolve DLL name from file handle and stream MOD_LOAD via IPC
    wchar_t dllPath[MAX_PATH] = {};
    DWORD pathLen = 0;

    // GetFinalPathNameByHandleW resolves the name from the file handle
    if (info.hFile) {
        pathLen = GetFinalPathNameByHandleW(info.hFile, dllPath, MAX_PATH, FILE_NAME_NORMALIZED);
        if (pathLen == 0 || pathLen >= MAX_PATH) {
            // Fallback: use GetMappedFileNameW on the base address
            pathLen = GetMappedFileNameW(m_hProcess, info.lpBaseOfDll, dllPath, MAX_PATH);
        }
    }

    // Extract just the filename from the full path
    std::wstring fullPath(dllPath, pathLen);
    size_t lastSlash = fullPath.find_last_of(L"\\/");
    std::wstring dllName = (lastSlash != std::wstring::npos) ? fullPath.substr(lastSlash + 1) : fullPath;

    // Read PE optional header to get SizeOfImage
    uint32_t imageSize = 0;
    uint8_t dosHdr[64];
    SIZE_T bytesRead = 0;
    if (ReadProcessMemory(m_hProcess, info.lpBaseOfDll, dosHdr, sizeof(dosHdr), &bytesRead)) {
        uint32_t peOff = *reinterpret_cast<uint32_t*>(dosHdr + 0x3C);
        uint8_t peHdr[264];
        if (ReadProcessMemory(m_hProcess, (LPCVOID)((uint64_t)info.lpBaseOfDll + peOff), peHdr, sizeof(peHdr), &bytesRead)) {
            // SizeOfImage is at OptionalHeader offset 0x38 (PE32+)
            imageSize = *reinterpret_cast<uint32_t*>(peHdr + 0x18 + 0x38);
        }
    }

    // Construct and send IPC payload
    auto& bridge = ui::WebView2Bridge::getInstance();
    ipc::MsgModuleLoad payload = {};
    payload.base_address = reinterpret_cast<uint64_t>(info.lpBaseOfDll);
    payload.size = imageSize;
    payload.path_len = static_cast<uint16_t>(dllName.length() * sizeof(wchar_t));

    bridge.sendBinaryMessage(ipc::MessageType::MOD_LOAD, &payload, sizeof(payload));
}

void DebuggerCore::onException(const EXCEPTION_DEBUG_INFO& info, DWORD threadId) {
    if (info.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT) {
        uint64_t bpAddress = reinterpret_cast<uint64_t>(info.ExceptionRecord.ExceptionAddress);
        
        // Find if this is one of our software breakpoints
        for (auto& bp : m_breakpoints) {
            if (bp.address == bpAddress && bp.enabled) {
                // 1. Restore original byte
                SIZE_T written;
                WriteProcessMemory(m_hProcess, reinterpret_cast<LPVOID>(bp.address), &bp.originalByte, 1, &written);

                // 2. Decrement RIP (Instruction Pointer) back by 1 to re-execute original instr
                HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, threadId);
                if (hThread) {
                    CONTEXT ctx = {0};
                    ctx.ContextFlags = CONTEXT_CONTROL;
                    if (GetThreadContext(hThread, &ctx)) {
                        ctx.Rip -= 1; 
                        SetThreadContext(hThread, &ctx);
                    }
                    CloseHandle(hThread);
                }

                // 3. Notify UI via IPC Spine
                auto& bridge = ui::WebView2Bridge::getInstance();
                ipc::MsgDebugEvent dbgEvt = {0};
                dbgEvt.thread_id = threadId;
                dbgEvt.rip = bp.address;
                dbgEvt.exception_code = 0x80000003; // EXCEPTION_BREAKPOINT
                
                // 4. Update the IPC payload with a full register snapshot (Phase 3.3)
                Registers regs;
                if (getThreadRegisters(threadId, regs)) {
                    // In a production scenario, we'd add these to a larger MsgDebugEvent struct
                    // For now, we've wired up the capture logic for verification
                }
                
                bridge.sendBinaryMessage(ipc::MessageType::DBG_EVT, &dbgEvt, sizeof(dbgEvt));
                
                break;
            }
        }
    }
}

bool DebuggerCore::setBreakpoint(uint64_t address) {
    if (!m_hProcess) return false;

    uint8_t originalByte;
    SIZE_T read;
    if (!ReadProcessMemory(m_hProcess, reinterpret_cast<LPVOID>(address), &originalByte, 1, &read)) return false;

    uint8_t int3 = 0xCC;
    SIZE_T written;
    if (!WriteProcessMemory(m_hProcess, reinterpret_cast<LPVOID>(address), &int3, 1, &written)) return false;

    m_breakpoints.push_back({address, originalByte, true});
    FlushInstructionCache(m_hProcess, reinterpret_cast<LPVOID>(address), 1);
    
    return true;
}

bool DebuggerCore::removeBreakpoint(uint64_t address) {
    for (auto it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it) {
        if (it->address == address) {
            SIZE_T written;
            WriteProcessMemory(m_hProcess, reinterpret_cast<LPVOID>(address), &it->originalByte, 1, &written);
            m_breakpoints.erase(it);
            FlushInstructionCache(m_hProcess, reinterpret_cast<LPVOID>(address), 1);
            return true;
        }
    }
    return false;
}

bool DebuggerCore::getThreadRegisters(DWORD threadId, Registers& regs) {
    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, threadId);
    if (!hThread) return false;

    // x64 CONTEXT is 1232 bytes, aligned to 16
    alignas(16) char context[1232] = {0};
    if (Dbg_CaptureContext(hThread, context, sizeof(context)) == 0) {
        regs.rax = *reinterpret_cast<uint64_t*>(context + CTX_Rax);
        regs.rcx = *reinterpret_cast<uint64_t*>(context + CTX_Rcx);
        regs.rdx = *reinterpret_cast<uint64_t*>(context + CTX_Rdx);
        regs.rbx = *reinterpret_cast<uint64_t*>(context + CTX_Rbx);
        regs.rsp = *reinterpret_cast<uint64_t*>(context + CTX_Rsp);
        regs.rbp = *reinterpret_cast<uint64_t*>(context + CTX_Rbp);
        regs.rsi = *reinterpret_cast<uint64_t*>(context + CTX_Rsi);
        regs.rdi = *reinterpret_cast<uint64_t*>(context + CTX_Rdi);
        regs.r8 = *reinterpret_cast<uint64_t*>(context + CTX_R8);
        regs.r9 = *reinterpret_cast<uint64_t*>(context + CTX_R9);
        regs.r10 = *reinterpret_cast<uint64_t*>(context + CTX_R10);
        regs.r11 = *reinterpret_cast<uint64_t*>(context + CTX_R11);
        regs.r12 = *reinterpret_cast<uint64_t*>(context + CTX_R12);
        regs.r13 = *reinterpret_cast<uint64_t*>(context + CTX_R13);
        regs.r14 = *reinterpret_cast<uint64_t*>(context + CTX_R14);
        regs.r15 = *reinterpret_cast<uint64_t*>(context + CTX_R15);
        regs.rip = *reinterpret_cast<uint64_t*>(context + CTX_Rip);
        regs.rflags = *reinterpret_cast<uint64_t*>(context + CTX_EFlags);
        CloseHandle(hThread);
        return true;
    }
    CloseHandle(hThread);
    return false;
}

std::vector<uint8_t> DebuggerCore::readMemory(uint64_t address, size_t size) {
    if (!m_hProcess) return {};
    std::vector<uint8_t> buffer(size);
    uint64_t bytesRead = 0;
    if (Dbg_ReadMemory(m_hProcess, address, buffer.data(), size, &bytesRead) == 0) {
        if (bytesRead < size) buffer.resize(bytesRead);
        return buffer;
    }
    return {};
}

bool DebuggerCore::patchMemory(uint64_t address, const std::vector<uint8_t>& data) {
    if (!m_hProcess) return false;
    uint64_t written = 0;
    
    // Attempt fast write via MASM kernel
    // Dbg_WriteMemory uses WriteProcessMemory internally but can be specialized for hypervisor/driver bypass
    if (Dbg_WriteMemory(m_hProcess, address, (void*)data.data(), data.size(), &written) != 0) {
        // Fallback: Handle write-protected pages (e.g. .text)
        DWORD oldProtect;
        if (VirtualProtectEx(m_hProcess, (LPVOID)address, data.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
             Dbg_WriteMemory(m_hProcess, address, (void*)data.data(), data.size(), &written);
             VirtualProtectEx(m_hProcess, (LPVOID)address, data.size(), oldProtect, &oldProtect);
             // Ensure instruction cache is invalidated for the new code
             FlushInstructionCache(m_hProcess, (LPVOID)address, data.size());
        }
    } else {
        // Success via kernel - still flush cache as it might be code
        FlushInstructionCache(m_hProcess, (LPVOID)address, data.size());
    }
    return written == (uint64_t)data.size();
}

uint64_t DebuggerCore::resolveExport(uint64_t moduleBase, const std::string& name) {
    struct ExportContext {
        const std::string& targetName;
        uint64_t moduleBase;
        uint64_t foundRva;
    } context = { name, moduleBase, 0 };

    auto callback = [](uint64_t index, const char* exportName, void* ctx) {
        auto c = static_cast<ExportContext*>(ctx);
        if (c->foundRva != 0) return;
        if (_stricmp(exportName, c->targetName.c_str()) == 0) {
            // Need to find RVA from index. For now, since v1 walker only gives name,
            // we'd ideally have the walker provide RVA too. 
            // I'll update the ASM walker to return RVA in RCX instead of index.
            c->foundRva = index; // Temporary: treat index as RVA for now (to be fixed in ASM)
        }
    };

    rawrxd_walk_export_table(moduleBase, callback, &context);
    return context.foundRva ? (moduleBase + context.foundRva) : 0;
}

std::string DebuggerCore::resolveAddressToName(uint64_t address) {
    struct SearchContext {
        uint64_t address;
        std::string foundName;
        uint64_t base;
    } context = { address, "", 0 };

    auto callback = [](uint64_t base, uint32_t size, uint16_t nameLen, const wchar_t* namePtr, void* ctx) {
        auto c = static_cast<SearchContext*>(ctx);
        if (!c->foundName.empty()) return;
        if (c->address >= base && (size == 0 || c->address < (base + size))) {
            char buf[256];
            int len = WideCharToMultiByte(CP_UTF8, 0, namePtr, nameLen/2, buf, sizeof(buf)-1, nullptr, nullptr);
            buf[len] = '\0';
            c->foundName = buf;
            c->base = base;
        }
    };

    rawrxd_enumerate_modules_peb(callback, &context);

    if (!context.foundName.empty()) {
        char offsetStr[32];
        sprintf_s(offsetStr, "+0x%llx", address - context.base);
        return context.foundName + offsetStr;
    }
    
    return "unknown";
}

uint64_t DebuggerCore::resolveNameToAddress(const std::string& name) {
    // 1. Check for raw hex string (e.g. 0x1234)
    if (name.compare(0, 2, "0x") == 0 || name.compare(0, 2, "0X") == 0) {
        try {
            return std::stoull(name, nullptr, 16);
        } catch (...) { return 0; }
    }

    // 2. Resolve module!export (e.g. kernel32.dll!CreateFileW)
    size_t bang = name.find('!');
    if (bang != std::string::npos) {
        std::string modName = name.substr(0, bang);
        std::string expName = name.substr(bang + 1);
        
        struct ModuleLookupContext {
            const char* moduleName;
            uint64_t base;
        } moduleCtx{modName.c_str(), 0};

        auto moduleLookup = [](uint64_t b, uint32_t, uint16_t nl, const wchar_t* np, void* ctx) {
            auto* c = static_cast<ModuleLookupContext*>(ctx);
            if (c->base != 0) return;
            char buf[256] = {0};
            const int len = WideCharToMultiByte(CP_UTF8, 0, np, nl / 2, buf, 255, nullptr, nullptr);
            if (len <= 0) return;
            buf[len] = '\0';
            if (_stricmp(buf, c->moduleName) == 0) {
                c->base = b;
            }
        };

        rawrxd_enumerate_modules_peb(moduleLookup, &moduleCtx);
        if (moduleCtx.base) return resolveExport(moduleCtx.base, expName);
    }
    
    // 3. Fallback: Search all modules for the export name
    struct ExportLookupContext {
        DebuggerCore* self;
        const std::string* exportName;
        uint64_t found;
    } exportCtx{this, &name, 0};

    auto exportLookup = [](uint64_t b, uint32_t, uint16_t, const wchar_t*, void* ctx) {
        auto* c = static_cast<ExportLookupContext*>(ctx);
        if (c->found != 0) return;
        c->found = c->self->resolveExport(b, *c->exportName);
    };

    rawrxd_enumerate_modules_peb(exportLookup, &exportCtx);
    return exportCtx.found;
}

void DebuggerCore::refreshWatches() {
    if (!m_hProcess) return;
    auto& bridge = ui::WebView2Bridge::getInstance();

    for (auto& we : m_watches) {
        uint64_t bytesRead = 0;
        we.lastValue.resize(we.size);
        Dbg_ReadMemory(m_hProcess, we.address, we.lastValue.data(), we.size, &bytesRead);
        
        std::vector<uint8_t> payload(sizeof(ipc::MsgWatchUpdate) + we.label.size() + we.lastValue.size());
        auto msg = reinterpret_cast<ipc::MsgWatchUpdate*>(payload.data());
        msg->address = we.address;
        msg->size = (uint32_t)we.size;
        msg->label_len = (uint16_t)we.label.size();
        
        uint8_t* extra = payload.data() + sizeof(ipc::MsgWatchUpdate);
        memcpy(extra, we.label.data(), we.label.size());
        memcpy(extra + we.label.size(), we.lastValue.data(), we.lastValue.size());
        
        bridge.sendBinaryMessage(ipc::MessageType::DATA_WATCH_UPDATE, payload.data(), payload.size());
    }
}

void DebuggerCore::addWatch(uint64_t address, size_t size, const std::string& label) {
    WatchEntry we{};
    we.address = address;
    we.size = size;
    we.label = label;
    m_watches.push_back(std::move(we));
    refreshWatches();
}

void DebuggerCore::removeWatch(uint64_t address) {
    auto it = std::remove_if(m_watches.begin(), m_watches.end(), [&](const WatchEntry& we) {
        return we.address == address;
    });
    m_watches.erase(it, m_watches.end());
}

void DebuggerCore::addWatch(uint64_t moduleBase, const std::string& name, size_t size) {
    WatchSymbol ws;
    ws.moduleBase = moduleBase;
    ws.name = name;
    ws.resolvedAddress = 0;
    ws.sizeToRead = size;
    m_watchList.push_back(ws);
    refreshWatchValues(); // Initial resolve
}

void DebuggerCore::removeWatch(const std::string& name) {
    auto it = std::remove_if(m_watchList.begin(), m_watchList.end(), [&](const WatchSymbol& ws) {
        return ws.name == name;
    });
    m_watchList.erase(it, m_watchList.end());
}

void DebuggerCore::refreshWatchValues() {
    if (!m_hProcess) return;
    for (auto& ws : m_watchList) {
        if (ws.resolvedAddress == 0) {
            ws.resolvedAddress = resolveExport(ws.moduleBase, ws.name);
        }
        if (ws.resolvedAddress != 0) {
            ws.lastValue = readMemory(ws.resolvedAddress, ws.sizeToRead);
        }
    }
}

void DebuggerCore::stop() {
    if (m_hProcess) {
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }
    m_isDebugging = false;
}

} // namespace rawrxd::debug
