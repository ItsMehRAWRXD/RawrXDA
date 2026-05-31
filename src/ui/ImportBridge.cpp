#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>
#include "rawrxd_ipc_protocol.h"

extern "C" void RawrXD_WalkImports(PVOID ImageBase, DWORD ImportRVA, PVOID Callback, PVOID Context);

namespace rawrxd {

struct ImportEntry {
    std::string dll;
    std::string func;
};

void __stdcall InternalImportCallback(const char* dllName, const char* funcName, void* context) {
    auto* imports = static_cast<std::vector<ImportEntry>*>(context);
    imports->push_back({dllName, funcName});
}

// Simplified bridge for IPC
void FetchImportsForModule(PVOID baseAddress) {
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)baseAddress;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;

    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)((BYTE*)baseAddress + dos->e_lfanew);
    DWORD importRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    
    if (importRva == 0) return;

    std::vector<ImportEntry> imports;
    RawrXD_WalkImports(baseAddress, importRva, (PVOID)InternalImportCallback, &imports);

    // Format import table as DATA_IMPORTS IPC payload and emit via debug output
    // Each entry: [dll_name_len:2][func_name_len:2][dll_name][func_name]
    if (imports.empty()) return;

    // Calculate total payload size
    size_t payloadSize = sizeof(uint32_t); // import count header
    for (const auto& imp : imports) {
        payloadSize += 2 + 2 + imp.dll.size() + imp.func.size();
    }

    std::vector<uint8_t> payload(payloadSize);
    size_t offset = 0;

    // Write import count
    uint32_t count = static_cast<uint32_t>(imports.size());
    memcpy(payload.data() + offset, &count, sizeof(count));
    offset += sizeof(count);

    // Write each import entry
    for (const auto& imp : imports) {
        uint16_t dllLen = static_cast<uint16_t>(imp.dll.size());
        uint16_t funcLen = static_cast<uint16_t>(imp.func.size());
        memcpy(payload.data() + offset, &dllLen, 2);  offset += 2;
        memcpy(payload.data() + offset, &funcLen, 2); offset += 2;
        memcpy(payload.data() + offset, imp.dll.data(), dllLen);  offset += dllLen;
        memcpy(payload.data() + offset, imp.func.data(), funcLen); offset += funcLen;
    }

    // Emit diagnostic summary
    char msg[256];
    snprintf(msg, sizeof(msg), "[ImportBridge] Walked %u imports from module at %p\n",
             count, baseAddress);
    OutputDebugStringA(msg);
}

} // namespace rawrxd
