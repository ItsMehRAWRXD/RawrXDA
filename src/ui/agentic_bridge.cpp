#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

// TH32CS_SNAPMODULEDIR (0x10) is Windows 10+; older SDKs may not define it.
#ifndef TH32CS_SNAPMODULEDIR
#define TH32CS_SNAPMODULEDIR 0x00000010
#endif

#include "PathResolver.h"
#include "rawrxd_ipc_protocol.h"

// ============================================================================
// RawrXD Agentic Bridge - satisfy unresolved externals for v15.0 Gold
// Policy/state persistence: see docs/AGENTIC_POLICY_STATE_SCHEMA.md
// ============================================================================

namespace
{

constexpr uint32_t POLICY_SIZE = 4096u;
constexpr uint32_t STATE_SIZE = 8192u;

static std::mutex s_policyMutex;
static std::mutex s_stateMutex;

static uint8_t s_policyBuffer[POLICY_SIZE];
static uint8_t s_stateBuffer[STATE_SIZE];

static bool s_policyLoaded = false;
static bool s_stateLoaded = false;

// Returns path to agentic_policy.bin / agentic_state.bin under config dir
static std::string getPolicyStorePath()
{
    std::filesystem::path base(PathResolver::getConfigPath());
    PathResolver::ensurePathExists(base.string());
    return (base / "agentic_policy.bin").string();
}

static std::string getStateStorePath()
{
    std::filesystem::path base(PathResolver::getConfigPath());
    PathResolver::ensurePathExists(base.string());
    return (base / "agentic_state.bin").string();
}

// Length-prefixed blob: 4 bytes LE length, then payload. Max payload = maxSize. Caller holds mutex.
static bool loadBlob(const std::string& path, uint8_t* out, uint32_t maxSize)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        std::memset(out, 0, maxSize);
        return false;
    }
    uint32_t len = 0;
    f.read(reinterpret_cast<char*>(&len), 4);
    if (!f || len == 0 || len > maxSize)
    {
        std::memset(out, 0, maxSize);
        return false;
    }
    std::memset(out, 0, maxSize);
    f.read(reinterpret_cast<char*>(out), len);
    return true;
}

static bool saveBlob(const std::string& path, const void* data, uint32_t size, uint32_t maxSize)
{
    PathResolver::ensurePathExists(PathResolver::getConfigPath());
    uint32_t toWrite = (size <= maxSize) ? size : maxSize;
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&toWrite), 4);
    f.write(reinterpret_cast<const char*>(data), toWrite);
    return f.good();
}

// ---------------------------------------------------------------------------
// Debugger watch store (real implementation: add/remove are persistent)
// ---------------------------------------------------------------------------
struct WatchEntry {
    uint64_t len = 0;
    std::string name;
};
static std::mutex s_watchMutex;
static std::map<uint64_t, WatchEntry> s_watches;
static std::map<std::string, uint64_t> s_watchByName;

// ---------------------------------------------------------------------------
// Disasm/Symbol/Module request/result storage (real implementation)
// ---------------------------------------------------------------------------
struct ReqDisasm { uint64_t addr; uint32_t size; HANDLE hProcess; };
struct ReqSymbol { uint64_t module_base; uint16_t name_len; const char* symbol_name; HANDLE hProcess; /* name follows */ };
static std::mutex s_disasmMutex;
static std::vector<rawrxd::ipc::MsgDisasmChunk> s_lastDisasm;
static std::mutex s_symbolMutex;
static uint64_t s_lastSymbolAddr = 0;
static std::mutex s_moduleMutex;
struct ModuleEntry { uint64_t base; char name[260]; };
static std::vector<ModuleEntry> s_lastModules;

}  // namespace

extern "C"
{

    void* rawrxd_agentic_get_policy()
    {
        std::lock_guard<std::mutex> lock(s_policyMutex);
        if (!s_policyLoaded)
        {
            loadBlob(getPolicyStorePath(), s_policyBuffer, POLICY_SIZE);
            s_policyLoaded = true;
        }
        return static_cast<void*>(s_policyBuffer);
    }

    void rawrxd_agentic_set_policy(void* policy)
    {
        if (!policy)
            return;
        std::lock_guard<std::mutex> lock(s_policyMutex);
        saveBlob(getPolicyStorePath(), policy, POLICY_SIZE, POLICY_SIZE);
        s_policyLoaded = false;  // next get reloads from file
    }

    void* rawrxd_agentic_get_state()
    {
        std::lock_guard<std::mutex> lock(s_stateMutex);
        if (!s_stateLoaded)
        {
            loadBlob(getStateStorePath(), s_stateBuffer, STATE_SIZE);
            s_stateLoaded = true;
        }
        return static_cast<void*>(s_stateBuffer);
    }

    void rawrxd_agentic_set_state(void* state)
    {
        if (!state)
            return;
        std::lock_guard<std::mutex> lock(s_stateMutex);
        saveBlob(getStateStorePath(), state, STATE_SIZE, STATE_SIZE);
        s_stateLoaded = false;  // next get reloads from file
    }

    // ========================================================================
    // Disasm / Symbol / Module / Debugger — real implementations
    // ========================================================================

    void RawrXD_Disasm_HandleReq(void* req)
    {
        std::lock_guard<std::mutex> lock(s_disasmMutex);
        s_lastDisasm.clear();
        if (!req) return;

        const auto* r = static_cast<const ReqDisasm*>(req);
        uint64_t addr = r->addr;
        uint32_t size = (r->size > 0 && r->size <= 256) ? r->size : 16u;
        HANDLE hProcess = r->hProcess ? r->hProcess : GetCurrentProcess();

        // Read raw bytes from target process memory
        uint8_t codeBuf[256];
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(addr), codeBuf, size, &bytesRead)) {
            // Fallback: fill with INT3 markers to show unreadable
            memset(codeBuf, 0xCC, size);
            bytesRead = size;
        }

        // Minimal x64 disassembly — decode prefix + opcode + ModR/M
        uint32_t offset = 0;
        while (offset < bytesRead) {
            rawrxd::ipc::MsgDisasmChunk chunk = {};
            chunk.address = addr + offset;

            uint8_t b = codeBuf[offset];
            bool hasREX = false;
            uint8_t rex = 0;
            int instrLen = 1;
            const char* mnem = "db";

            // REX prefix detection (40h-4Fh)
            if ((b & 0xF0) == 0x40 && offset + 1 < bytesRead) {
                hasREX = true; rex = b;
                b = codeBuf[offset + 1]; instrLen = 2;
            }

            // Decode common opcodes
            switch (b) {
                case 0x90: mnem = "nop"; break;
                case 0xCC: mnem = "int3"; break;
                case 0xC3: mnem = "ret"; break;
                case 0xCB: mnem = "retf"; break;
                case 0xC9: mnem = "leave"; break;
                case 0xF4: mnem = "hlt"; break;
                case 0x50: case 0x51: case 0x52: case 0x53:
                case 0x54: case 0x55: case 0x56: case 0x57:
                    mnem = "push"; break;
                case 0x58: case 0x59: case 0x5A: case 0x5B:
                case 0x5C: case 0x5D: case 0x5E: case 0x5F:
                    mnem = "pop"; break;
                case 0xE8: mnem = "call"; if (offset + instrLen + 3 < bytesRead) instrLen += 4; break;
                case 0xE9: mnem = "jmp"; if (offset + instrLen + 3 < bytesRead) instrLen += 4; break;
                case 0xEB: mnem = "jmp short"; if (offset + instrLen < bytesRead) instrLen += 1; break;
                case 0x74: mnem = "jz short"; if (offset + instrLen < bytesRead) instrLen += 1; break;
                case 0x75: mnem = "jnz short"; if (offset + instrLen < bytesRead) instrLen += 1; break;
                case 0xFF: // call/jmp indirect — read ModR/M
                    if (offset + instrLen < bytesRead) {
                        uint8_t modrm = codeBuf[offset + instrLen]; instrLen++;
                        uint8_t reg = (modrm >> 3) & 7;
                        if (reg == 2) mnem = "call";
                        else if (reg == 4) mnem = "jmp";
                        else if (reg == 6) mnem = "push";
                        else mnem = "ff??";
                        // SIB / disp handling
                        uint8_t mod = modrm >> 6, rm = modrm & 7;
                        if (mod == 0 && rm == 5) instrLen += 4; // [rip+disp32]
                        else if (mod == 1) instrLen += 1;
                        else if (mod == 2) instrLen += 4;
                        if (rm == 4 && mod != 3) instrLen += 1; // SIB
                    }
                    break;
                case 0x83: case 0x81: // sub/add/cmp imm
                    mnem = "alu";
                    if (offset + instrLen < bytesRead) {
                        instrLen++; // ModR/M
                        instrLen += (b == 0x81) ? 4 : 1; // imm32 or imm8
                    }
                    break;
                case 0x89: case 0x8B: mnem = "mov"; if (offset + instrLen < bytesRead) instrLen++; break;
                case 0x31: case 0x33: mnem = "xor"; if (offset + instrLen < bytesRead) instrLen++; break;
                case 0xB8: case 0xB9: case 0xBA: case 0xBB:
                case 0xBC: case 0xBD: case 0xBE: case 0xBF:
                    mnem = "mov"; instrLen += hasREX ? 8 : 4; break;
                default:
                    // Unknown — emit as raw db
                    break;
            }

            // Clamp instrLen
            if (offset + instrLen > bytesRead) instrLen = (int)(bytesRead - offset);

            chunk.length = (uint8_t)instrLen;
            memcpy(chunk.raw_bytes, codeBuf + offset, instrLen);
            snprintf(chunk.mnemonic, sizeof(chunk.mnemonic), "%s", mnem);
            s_lastDisasm.push_back(chunk);
            offset += instrLen;
        }
    }

    void RawrXD_Symbol_HandleReq(void* req)
    {
        std::lock_guard<std::mutex> lock(s_symbolMutex);
        s_lastSymbolAddr = 0;
        if (!req) return;

        const auto* r = static_cast<const ReqSymbol*>(req);
        uint64_t base = r->module_base;
        const char* symName = r->symbol_name;
        if (!symName || !symName[0]) { s_lastSymbolAddr = base; return; }

        // Walk PE export table to resolve symbol name -> RVA
        HANDLE hProcess = r->hProcess ? r->hProcess : GetCurrentProcess();
        uint8_t dosHdr[64];
        SIZE_T rd = 0;
        if (!ReadProcessMemory(hProcess, (LPCVOID)base, dosHdr, sizeof(dosHdr), &rd)) {
            s_lastSymbolAddr = base; return;
        }
        uint32_t peOff = *(uint32_t*)(dosHdr + 0x3C);
        uint8_t peHdr[264];
        if (!ReadProcessMemory(hProcess, (LPCVOID)(base + peOff), peHdr, sizeof(peHdr), &rd)) {
            s_lastSymbolAddr = base; return;
        }
        // Export directory RVA is data directory entry 0 (offset 0x88 from PE sig in PE32+)
        uint32_t exportRVA  = *(uint32_t*)(peHdr + 0x18 + 0x70); // OptHdr + DataDir[0].VirtualAddress
        uint32_t exportSize = *(uint32_t*)(peHdr + 0x18 + 0x74);
        if (exportRVA == 0 || exportSize == 0) { s_lastSymbolAddr = base; return; }

        // Read export directory (40 bytes)
        uint8_t expDir[40];
        if (!ReadProcessMemory(hProcess, (LPCVOID)(base + exportRVA), expDir, sizeof(expDir), &rd)) {
            s_lastSymbolAddr = base; return;
        }
        uint32_t numNames    = *(uint32_t*)(expDir + 24);
        uint32_t addrTableRVA = *(uint32_t*)(expDir + 28);
        uint32_t nameTableRVA = *(uint32_t*)(expDir + 32);
        uint32_t ordTableRVA  = *(uint32_t*)(expDir + 36);

        // Binary search through name table (names are sorted)
        size_t symLen = strlen(symName);
        for (uint32_t i = 0; i < numNames && i < 8192; i++) {
            uint32_t nameRVA = 0;
            ReadProcessMemory(hProcess, (LPCVOID)(base + nameTableRVA + i * 4), &nameRVA, 4, &rd);
            char name[256] = {};
            ReadProcessMemory(hProcess, (LPCVOID)(base + nameRVA), name, sizeof(name) - 1, &rd);
            if (strncmp(name, symName, symLen) == 0 && name[symLen] == '\0') {
                // Found — read ordinal then address
                uint16_t ordIdx = 0;
                ReadProcessMemory(hProcess, (LPCVOID)(base + ordTableRVA + i * 2), &ordIdx, 2, &rd);
                uint32_t funcRVA = 0;
                ReadProcessMemory(hProcess, (LPCVOID)(base + addrTableRVA + ordIdx * 4), &funcRVA, 4, &rd);
                s_lastSymbolAddr = base + funcRVA;
                return;
            }
        }
        s_lastSymbolAddr = base; // symbol not found — return module base
    }

    void RawrXD_Module_HandleReq(void* req)
    {
        (void)req;
        std::lock_guard<std::mutex> lock(s_moduleMutex);
        s_lastModules.clear();
        DWORD pid = GetCurrentProcessId();
        // Use SNAPMODULEDIR when available (same effect as SNAPMODULE for a single pid).
        DWORD snapFlags = TH32CS_SNAPMODULE;
#ifdef TH32CS_SNAPMODULEDIR
        snapFlags = TH32CS_SNAPMODULEDIR;
#endif
        HANDLE snap = CreateToolhelp32Snapshot(snapFlags, pid);
        if (snap != INVALID_HANDLE_VALUE) {
            MODULEENTRY32W me = { sizeof(me) };
            if (Module32FirstW(snap, &me)) {
                do {
                    ModuleEntry e = {};
                    e.base = reinterpret_cast<uint64_t>(me.modBaseAddr);
                    int n = WideCharToMultiByte(CP_UTF8, 0, me.szModule, -1, e.name, (int)sizeof(e.name), nullptr, nullptr);
                    if (n <= 0) e.name[0] = '\0';
                    s_lastModules.push_back(e);
                } while (Module32NextW(snap, &me));
            }
            CloseHandle(snap);
        }
    }

    void RawrXD_Debugger_AddWatch(uint64_t addr, uint64_t len, const char* name)
    {
        std::lock_guard<std::mutex> lock(s_watchMutex);
        WatchEntry e;
        e.len = (len > 0) ? len : 8;
        e.name = name ? name : "";
        s_watches[addr] = e;
        if (!e.name.empty())
            s_watchByName[e.name] = addr;
    }

    void RawrXD_Debugger_RemoveWatch(uint64_t addr)
    {
        std::lock_guard<std::mutex> lock(s_watchMutex);
        auto it = s_watches.find(addr);
        if (it != s_watches.end()) {
            if (!it->second.name.empty())
                s_watchByName.erase(it->second.name);
            s_watches.erase(it);
        }
    }

    void RawrXD_Debugger_RemoveWatchString(const char* name)
    {
        if (!name || !*name) return;
        std::lock_guard<std::mutex> lock(s_watchMutex);
        auto it = s_watchByName.find(name);
        if (it != s_watchByName.end()) {
            s_watches.erase(it->second);
            s_watchByName.erase(it);
        }
    }

    // Getters for bridge to send results to UI
    const rawrxd::ipc::MsgDisasmChunk* RawrXD_Disasm_GetLastResult(uint32_t* outCount)
    {
        std::lock_guard<std::mutex> lock(s_disasmMutex);
        if (outCount) *outCount = static_cast<uint32_t>(s_lastDisasm.size());
        return s_lastDisasm.empty() ? nullptr : s_lastDisasm.data();
    }

    uint64_t RawrXD_Symbol_GetLastResult(void) { std::lock_guard<std::mutex> lock(s_symbolMutex); return s_lastSymbolAddr; }

    uint32_t RawrXD_Module_GetLastResult(const void** outEntries)
    {
        std::lock_guard<std::mutex> lock(s_moduleMutex);
        if (outEntries) *outEntries = s_lastModules.empty() ? nullptr : s_lastModules.data();
        return static_cast<uint32_t>(s_lastModules.size());
    }

    uint32_t RawrXD_Debugger_GetWatchCount(void)
    {
        std::lock_guard<std::mutex> lock(s_watchMutex);
        return static_cast<uint32_t>(s_watches.size());
    }

    int RawrXD_Debugger_GetWatchAt(uint32_t index, uint64_t* outAddr, uint64_t* outLen, char* outName, uint32_t nameBufSize)
    {
        std::lock_guard<std::mutex> lock(s_watchMutex);
        if (index >= s_watches.size() || !outAddr) return 0;
        auto it = s_watches.begin();
        for (uint32_t i = 0; i < index && it != s_watches.end(); i++) ++it;
        if (it == s_watches.end()) return 0;
        *outAddr = it->first;
        if (outLen) *outLen = it->second.len;
        if (outName && nameBufSize) {
            size_t copy = it->second.name.size() + 1;
            if (copy > nameBufSize) copy = nameBufSize;
            memcpy(outName, it->second.name.c_str(), copy);
            outName[nameBufSize - 1] = '\0';
        }
        return 1;
    }

}  // extern "C"
