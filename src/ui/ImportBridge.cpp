#include <windows.h>
#include <string>
#include <vector>
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

    // In a real implementation, this would format a JSON/Message and send via IPC
    // For now, we stub the bridge logic
}

} // namespace rawrxd
