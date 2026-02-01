#include "PEParser.hpp"
#include <fstream>

namespace RawrXD::Agentic::Manifestor {

PEParser::PEParser(const std::string& filePath) : m_filePath(filePath) {}

PEParser::~PEParser() {
    cleanup();
}

bool PEParser::load() {
    cleanup();
    
    // Open file
    m_fileHandle = CreateFileA(m_filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m_fileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Get file size
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(m_fileHandle, &fileSize)) {
        cleanup();
        return false;
    }
    m_fileSize = static_cast<size_t>(fileSize.QuadPart);
    
    // Create file mapping
    m_mappingHandle = CreateFileMappingA(m_fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!m_mappingHandle) {
        cleanup();
        return false;
    }
    
    // Map view
    m_moduleBase = MapViewOfFile(m_mappingHandle, FILE_MAP_READ, 0, 0, 0);
    if (!m_moduleBase) {
        cleanup();
        return false;
    }
    
    // Parse PE headers
    m_dosHeader = static_cast<IMAGE_DOS_HEADER*>(m_moduleBase);
    if (m_dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        cleanup();
        return false;
    }
    
    m_ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
        static_cast<uint8_t*>(m_moduleBase) + m_dosHeader->e_lfanew);
    if (m_ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        cleanup();
        return false;
    }
    
    return true;
}

std::vector<PEExport> PEParser::getExports() const {
    std::vector<PEExport> exports;
    
    if (!isValidPE()) {
        return exports;
    }
    
    // Get export directory
    DWORD exportRva = m_ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (exportRva == 0) {
        return exports;
    }
    
    auto* exportDir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
        static_cast<uint8_t*>(m_moduleBase) + exportRva);
    
    auto* functions = reinterpret_cast<DWORD*>(
        static_cast<uint8_t*>(m_moduleBase) + exportDir->AddressOfFunctions);
    auto* names = reinterpret_cast<DWORD*>(
        static_cast<uint8_t*>(m_moduleBase) + exportDir->AddressOfNames);
    auto* ordinals = reinterpret_cast<WORD*>(
        static_cast<uint8_t*>(m_moduleBase) + exportDir->AddressOfNameOrdinals);
    
    for (DWORD i = 0; i < exportDir->NumberOfNames; ++i) {
        PEExport exp;
        exp.name = reinterpret_cast<const char*>(static_cast<uint8_t*>(m_moduleBase) + names[i]);
        exp.ordinal = ordinals[i];
        exp.address = functions[ordinals[i]];
        exp.absoluteAddress = reinterpret_cast<uintptr_t>(m_moduleBase) + exp.address;
        exports.push_back(exp);
    }
    
    return exports;
}

std::vector<PEImport> PEParser::getImports() const {
    std::vector<PEImport> imports;
    
    if (!isValidPE()) return imports;

    DWORD importRva = m_ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (importRva == 0) return imports;

    // Convert RVA to VA within our mapping
    // Note: Using simple offset logic assuming section mapping corresponds to file offset for simple implementation
    // Ideally we should traverse section headers to find which section contains the RVA.
    // For mapped view (LoadLibrary style), RVA + Base is fine.
    // For CreateFileMapping (Raw file), RVA != FileOffset. 
    // Wait, MapViewOfFile maps the file as it is on disk (raw) unless SEC_IMAGE is used.
    // The previous code used CreateFileMapping without SEC_IMAGE, so it is a RAW mapping.
    // We must convert RVA to FileOffset.

    auto rvaToFileOffset = [&](DWORD rva) -> DWORD {
        auto* sectionHeader = IMAGE_FIRST_SECTION(m_ntHeaders);
        for (WORD i = 0; i < m_ntHeaders->FileHeader.NumberOfSections; ++i, ++sectionHeader) {
            if (rva >= sectionHeader->VirtualAddress && 
                rva < sectionHeader->VirtualAddress + sectionHeader->Misc.VirtualSize) {
                return rva - sectionHeader->VirtualAddress + sectionHeader->PointerToRawData;
            }
        }
        return 0; 
    };

    DWORD importOffset = rvaToFileOffset(importRva);
    if (!importOffset) return imports;

    auto* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        static_cast<uint8_t*>(m_moduleBase) + importOffset);

    while (importDesc->Name != 0) {
        DWORD nameOffset = rvaToFileOffset(importDesc->Name);
        if(!nameOffset) break;
        
        std::string dllName = reinterpret_cast<const char*>(static_cast<uint8_t*>(m_moduleBase) + nameOffset);

        DWORD thunkOffset = importDesc->OriginalFirstThunk ? rvaToFileOffset(importDesc->OriginalFirstThunk) : rvaToFileOffset(importDesc->FirstThunk);
        if(!thunkOffset) { importDesc++; continue; }

        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(static_cast<uint8_t*>(m_moduleBase) + thunkOffset);
        
        while (thunk->u1.AddressOfData != 0) {
            PEImport imp;
            imp.dllName = dllName;
            
            if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                imp.isOrdinal = true;
                imp.ordinal = static_cast<WORD>(IMAGE_ORDINAL(thunk->u1.Ordinal));
            } else {
                imp.isOrdinal = false;
                DWORD nameDataOffset = rvaToFileOffset(static_cast<DWORD>(thunk->u1.AddressOfData));
                if(nameDataOffset) {
                    auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                        static_cast<uint8_t*>(m_moduleBase) + nameDataOffset);
                    imp.functionName = importByName->Name;
                }
            }
            imports.push_back(imp);
            thunk++;
        }
        importDesc++;
    }

    return imports;
}

std::vector<PESection> PEParser::getSections() const {
    std::vector<PESection> sections;
    
    if (!isValidPE()) {
        return sections;
    }
    
    auto* sectionHeader = IMAGE_FIRST_SECTION(m_ntHeaders);
    for (int i = 0; i < m_ntHeaders->FileHeader.NumberOfSections; ++i, ++sectionHeader) {
        PESection section;
        section.name = std::string(reinterpret_cast<const char*>(sectionHeader->Name), 8);
        section.virtualAddress = sectionHeader->VirtualAddress;
        section.virtualSize = sectionHeader->Misc.VirtualSize;
        section.rawDataOffset = sectionHeader->PointerToRawData;
        section.rawDataSize = sectionHeader->SizeOfRawData;
        section.characteristics = sectionHeader->Characteristics;
        sections.push_back(section);
    }
    
    return sections;
}

const PEExport* PEParser::findExport(const std::string& name) const {
    static PEExport cachedExport;
    auto exports = getExports();
    for (const auto& exp : exports) {
        if (exp.name == name) {
            cachedExport = exp;
            return &cachedExport;
        }
    }
    return nullptr;
}

uint32_t PEParser::getTimestamp() const {
    return isValidPE() ? m_ntHeaders->FileHeader.TimeDateStamp : 0;
}

uint32_t PEParser::getImageSize() const {
    return isValidPE() ? m_ntHeaders->OptionalHeader.SizeOfImage : 0;
}

uint32_t PEParser::rvaToFileOffset(uint32_t rva) const {
    if (!isValidPE()) {
        return 0;
    }
    
    auto* sectionHeader = IMAGE_FIRST_SECTION(m_ntHeaders);
    for (int i = 0; i < m_ntHeaders->FileHeader.NumberOfSections; ++i, ++sectionHeader) {
        if (rva >= sectionHeader->VirtualAddress &&
            rva < sectionHeader->VirtualAddress + sectionHeader->Misc.VirtualSize) {
            return rva - sectionHeader->VirtualAddress + sectionHeader->PointerToRawData;
        }
    }
    
    return 0;
}

std::vector<PEExport> PEParser::findCapabilityExports() const {
    std::vector<PEExport> capExports;
    auto exports = getExports();
    
    for (const auto& exp : exports) {
        if (exp.name.find("capability_") == 0) {
            capExports.push_back(exp);
        }
    }
    
    return capExports;
}

void PEParser::cleanup() {
    if (m_moduleBase) {
        UnmapViewOfFile(m_moduleBase);
        m_moduleBase = nullptr;
    }
    if (m_mappingHandle) {
        CloseHandle(m_mappingHandle);
        m_mappingHandle = nullptr;
    }
    if (m_fileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
    }
    m_dosHeader = nullptr;
    m_ntHeaders = nullptr;
}

} // namespace RawrXD::Agentic::Manifestor
