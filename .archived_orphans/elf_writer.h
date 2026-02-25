#pragma once

// RawrXD - ELF Writer Header
// API for generating ELF64 object files for Linux.

#include <cstdint>
#include <vector>
#include <string>

namespace ELF64 { struct Ehdr; }

class ELFWriter {
public:
    ELFWriter();
    ~ELFWriter();

    // Set the target architecture (e.g., 0x3E for EM_X86_64)
    void set_architecture(uint16_t arch);

    // Add a section to the ELF file
    void add_section(const std::string& name, const std::vector<uint8_t>& data, uint32_t type, uint64_t flags);

    // Add a symbol to the symbol table
    void add_symbol(const std::string& name, uint64_t value, uint64_t size, uint8_t info, uint8_t other, uint16_t shndx);

    // Generate the final ELF file as a byte vector
    std::vector<uint8_t> write();

private:
    struct Section {
        std::string name;
        std::vector<uint8_t> data;
        uint32_t type;
        uint64_t flags;
    };

    struct Symbol {
        std::string name;
        uint64_t value;
        uint64_t size;
        uint8_t info;
        uint8_t other;
        uint16_t shndx;
    };
    
    void populate_header(ELF64::Ehdr& header);

    uint16_t m_architecture;
    std::vector<Section> m_sections;
    std::vector<Symbol> m_symbols;
};
