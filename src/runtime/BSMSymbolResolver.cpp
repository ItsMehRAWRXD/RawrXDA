// ============================================================================
// BSMSymbolResolver.cpp — Binary Symbol Map Resolver Implementation
// ============================================================================
#include "runtime/BSMSymbolResolver.h"
#include <windows.h>
#include <dbghelp.h>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "dbghelp.lib")

namespace RawrXD {
namespace Runtime {

// ============================================================================
// Symbol Resolution Implementation
// ============================================================================
bool BSMSymbolResolver::LoadFromModule(const std::string& modulePath) {
    m_modulePath = modulePath;
    m_symbols.clear();

    // Load the module to get its base address
    HMODULE hModule = GetModuleHandleA(modulePath.c_str());
    if (!hModule) {
        hModule = LoadLibraryA(modulePath.c_str());
        if (!hModule) {
            return false;
        }
    }

    // Initialize DbgHelp
    HANDLE hProcess = GetCurrentProcess();
    SymInitialize(hProcess, nullptr, TRUE);

    // Enumerate symbols
    struct EnumContext {
        BSMSymbolResolver* resolver;
        HMODULE module;
    } ctx = {this, hModule};

    auto enumCallback = [](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext) -> BOOL {
        (void)SymbolSize;
        auto* context = static_cast<EnumContext*>(UserContext);
        BSMSymbol sym;
        sym.name = pSymInfo->Name;
        sym.address = pSymInfo->Address;
        sym.size = pSymInfo->Size;
        sym.isExported = (pSymInfo->Tag == SymTagFunction);
        context->resolver->m_symbols.push_back(sym);
        return TRUE;
    };

    SymEnumSymbols(hProcess, (ULONG64)hModule, "*", enumCallback, &ctx);

    // Also enumerate exports
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dosHeader->e_lfanew);
    DWORD exportDirRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    DWORD exportDirSize = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;

    if (exportDirRVA != 0 && exportDirSize != 0) {
        PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hModule + exportDirRVA);
        DWORD* names = (DWORD*)((BYTE*)hModule + exportDir->AddressOfNames);
        DWORD* functions = (DWORD*)((BYTE*)hModule + exportDir->AddressOfFunctions);
        WORD* ordinals = (WORD*)((BYTE*)hModule + exportDir->AddressOfNameOrdinals);

        for (DWORD i = 0; i < exportDir->NumberOfNames; ++i) {
            const char* name = (const char*)((BYTE*)hModule + names[i]);
            WORD ordinal = ordinals[i];
            DWORD functionRVA = functions[ordinal];

            BSMSymbol sym;
            sym.name = name;
            sym.address = (uint64_t)((BYTE*)hModule + functionRVA);
            sym.size = 0;
            sym.isExported = true;
            m_symbols.push_back(sym);
        }
    }

    return true;
}

void* BSMSymbolResolver::resolveSync(const std::string& name) {
    for (const auto& sym : m_symbols) {
        if (sym.name == name) {
            return (void*)sym.address;
        }
    }
    return nullptr;
}

bool BSMSymbolResolver::ResolveByName(const std::string& name, BSMSymbol& out) const {
    for (const auto& sym : m_symbols) {
        if (sym.name == name) {
            out = sym;
            return true;
        }
    }
    return false;
}

bool BSMSymbolResolver::ResolveByAddress(uint64_t addr, BSMSymbol& out) const {
    for (const auto& sym : m_symbols) {
        if (sym.address == addr) {
            out = sym;
            return true;
        }
    }
    return false;
}

std::vector<BSMSymbol> BSMSymbolResolver::EnumerateExports() const {
    std::vector<BSMSymbol> exports;
    for (const auto& sym : m_symbols) {
        if (sym.isExported) {
            exports.push_back(sym);
        }
    }
    return exports;
}

} // namespace Runtime
} // namespace RawrXD
