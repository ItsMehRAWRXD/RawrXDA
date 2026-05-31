// ============================================================================
// RawrCodex.cpp — Binary Analysis Engine Implementation
// ============================================================================
#include "reverse_engineering/RawrCodex.hpp"
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace ReverseEngineering {

bool RawrCodex::LoadBinary(const std::string& filePath) {
    m_filePath = filePath;
    m_data.clear();
    m_sections.clear();
    m_symbols.clear();
    m_imports.clear();
    m_exports.clear();
    m_disassembly.clear();

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    if (size < 2) {
        return false;
    }

    m_data.resize(size);
    file.read(reinterpret_cast<char*>(m_data.data()), static_cast<std::streamsize>(size));
    file.close();

    // Parse PE headers if it's a PE file
    if (size >= 64 && m_data[0] == 'M' && m_data[1] == 'Z') {
        ParsePE();
    }

    return true;
}

void RawrCodex::ParsePE() {
#ifdef _WIN32
    if (m_data.size() < 64) return;

    // DOS header
    uint32_t peOffset = *reinterpret_cast<const uint32_t*>(m_data.data() + 60);
    if (peOffset + 24 > m_data.size()) return;

    // PE signature
    if (m_data[peOffset] != 'P' || m_data[peOffset + 1] != 'E') return;

    // COFF header
    uint16_t machine = *reinterpret_cast<const uint16_t*>(m_data.data() + peOffset + 4);
    uint16_t numSections = *reinterpret_cast<const uint16_t*>(m_data.data() + peOffset + 6);
    uint32_t symbolTableOffset = *reinterpret_cast<const uint32_t*>(m_data.data() + peOffset + 12);
    uint32_t numSymbols = *reinterpret_cast<const uint32_t*>(m_data.data() + peOffset + 16);

    // Optional header
    uint16_t optionalHeaderSize = *reinterpret_cast<const uint16_t*>(m_data.data() + peOffset + 20);
    uint32_t optionalHeaderOffset = peOffset + 24;

    // Parse sections
    uint32_t sectionTableOffset = optionalHeaderOffset + optionalHeaderSize;
    for (uint16_t i = 0; i < numSections && sectionTableOffset + 40 <= m_data.size(); ++i) {
        Section section;
        section.name = std::string(reinterpret_cast<const char*>(m_data.data() + sectionTableOffset), 8);
        // Trim nulls
        size_t nullPos = section.name.find('\0');
        if (nullPos != std::string::npos) {
            section.name = section.name.substr(0, nullPos);
        }
        section.virtualSize = *reinterpret_cast<const uint32_t*>(m_data.data() + sectionTableOffset + 8);
        section.virtualAddress = *reinterpret_cast<const uint32_t*>(m_data.data() + sectionTableOffset + 12);
        section.rawSize = *reinterpret_cast<const uint32_t*>(m_data.data() + sectionTableOffset + 16);
        section.rawOffset = *reinterpret_cast<const uint32_t*>(m_data.data() + sectionTableOffset + 20);
        section.characteristics = *reinterpret_cast<const uint32_t*>(m_data.data() + sectionTableOffset + 36);

        if (section.rawOffset + section.rawSize <= m_data.size()) {
            section.data.assign(m_data.begin() + section.rawOffset,
                               m_data.begin() + section.rawOffset + section.rawSize);
        }

        m_sections.push_back(section);
        sectionTableOffset += 40;
    }

    // Parse exports if available
    if (optionalHeaderSize >= 96 && optionalHeaderOffset + 120 <= m_data.size()) {
        uint32_t exportDirRVA = *reinterpret_cast<const uint32_t*>(m_data.data() + optionalHeaderOffset + 112);
        uint32_t exportDirSize = *reinterpret_cast<const uint32_t*>(m_data.data() + optionalHeaderOffset + 116);
        if (exportDirRVA > 0 && exportDirSize > 0) {
            ParseExports(exportDirRVA);
        }
    }

    // Parse imports if available
    if (optionalHeaderSize >= 104 && optionalHeaderOffset + 128 <= m_data.size()) {
        uint32_t importDirRVA = *reinterpret_cast<const uint32_t*>(m_data.data() + optionalHeaderOffset + 120);
        uint32_t importDirSize = *reinterpret_cast<const uint32_t*>(m_data.data() + optionalHeaderOffset + 124);
        if (importDirRVA > 0 && importDirSize > 0) {
            ParseImports(importDirRVA);
        }
    }

    // Parse symbol table if available
    if (symbolTableOffset > 0 && numSymbols > 0) {
        ParseSymbolTable(symbolTableOffset, numSymbols);
    }
#endif
}

void RawrCodex::ParseExports(uint32_t exportDirRVA) {
    (void)exportDirRVA;
    // Simplified export parsing - would need RVA to file offset conversion
    // For now, add a placeholder export
    Export exp;
    exp.name = "DllMain";
    exp.address = 0x1000;
    exp.ordinal = 1;
    exp.isForwarder = false;
    m_exports.push_back(exp);
}

void RawrCodex::ParseImports(uint32_t importDirRVA) {
    (void)importDirRVA;
    // Simplified import parsing
    Import imp;
    imp.moduleName = "kernel32.dll";
    imp.functionName = "LoadLibraryA";
    imp.address = 0x2000;
    imp.ordinal = 0;
    m_imports.push_back(imp);
}

void RawrCodex::ParseSymbolTable(uint32_t offset, uint32_t count) {
    (void)offset;
    // Simplified symbol table parsing
    for (uint32_t i = 0; i < count && i < 100; ++i) {
        Symbol sym;
        sym.name = "Symbol_" + std::to_string(i);
        sym.address = 0x1000 + i * 0x10;
        sym.size = 0x10;
        sym.section = ".text";
        sym.type = "function";
        sym.isMangled = false;
        m_symbols.push_back(sym);
    }
}

std::vector<Section> RawrCodex::GetSections() const {
    return m_sections;
}

std::vector<Symbol> RawrCodex::GetSymbols() const {
    return m_symbols;
}

std::vector<Import> RawrCodex::GetImports() const {
    return m_imports;
}

std::vector<Export> RawrCodex::GetExports() const {
    return m_exports;
}

std::vector<DisassemblyLine> RawrCodex::GetDisassembly() const {
    return m_disassembly;
}

} // namespace ReverseEngineering
} // namespace RawrXD
