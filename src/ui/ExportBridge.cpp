#include <windows.h>
#include <string>
#include <vector>
#include "rawrxd_ipc_protocol.h"

// Assuming RawrXD_PE_Exporter.asm exports this
extern "C" void RawrXD_WalkExports(PVOID ImageBase, DWORD ExportRVA, PVOID Callback, PVOID Context);

namespace rawrxd {

struct ExportEntry {
    std::string name;
    WORD ordinal;
    PVOID address;
};

void __stdcall InternalExportCallback(const char* name, WORD ordinal, PVOID address, void* context) {
    auto* exports = static_cast<std::vector<ExportEntry>*>(context);
    exports->push_back({name ? name : "[Ordinal]", ordinal, address});
}

void FetchExportsForModule(PVOID baseAddress) {
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)baseAddress;
    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)((BYTE*)baseAddress + dos->e_lfanew);
    DWORD exportRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

    if (exportRva == 0) return;

    std::vector<ExportEntry> exports;
    RawrXD_WalkExports(baseAddress, exportRva, (PVOID)InternalExportCallback, &exports);

    // IPC transport logic here...
}

} // namespace rawrxd
