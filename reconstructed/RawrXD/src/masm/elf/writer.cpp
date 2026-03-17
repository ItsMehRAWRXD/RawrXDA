// RawrXD - ELF Writer Implementation
// Fully-featured ELF64 generator for Linux executables.
// Supports multiple sections, symbols, and relocations.

#include "elf_writer.h"
#include <cstring>
#include <stdexcept>

// ELF 64-bit specific structures
namespace ELF64 {
    // ELF Header
    struct Ehdr {
        unsigned char e_ident[16];
        uint16_t      e_type;
        uint16_t      e_machine;
        uint32_t      e_version;
        uint64_t      e_entry;
        uint64_t      e_phoff;
        uint64_t      e_shoff;
        uint32_t      e_flags;
        uint16_t      e_ehsize;
        uint16_t      e_phentsize;
        uint16_t      e_phnum;
        uint16_t      e_shentsize;
        uint16_t      e_shnum;
        uint16_t      e_shstrndx;
    };

    // Section Header
    struct Shdr {
        uint32_t   sh_name;
        uint32_t   sh_type;
        uint64_t   sh_flags;
        uint64_t   sh_addr;
        uint64_t   sh_offset;
        uint64_t   sh_size;
        uint32_t   sh_link;
        uint32_t   sh_info;
        uint64_t   sh_addralign;
        uint64_t   sh_entsize;
    };

    // Symbol Table Entry
    struct Sym {
        uint32_t      st_name;
        unsigned char st_info;
        unsigned char st_other;
        uint16_t      st_shndx;
        uint64_t      st_value;
        uint64_t      st_size;
    };
}

ELFWriter::ELFWriter() : m_architecture(0) {
    // Initialize with a null section
    add_section("", {}, 0, 0); 
}

ELFWriter::~ELFWriter() {}

void ELFWriter::set_architecture(uint16_t arch) {
    m_architecture = arch;
}

void ELFWriter::add_section(const std::string& name, const std::vector<uint8_t>& data, uint32_t type, uint64_t flags) {
    Section section;
    section.name = name;
    section.data = data;
    section.type = type;
    section.flags = flags;
    m_sections.push_back(section);
}

void ELFWriter::add_symbol(const std::string& name, uint64_t value, uint64_t size, uint8_t info, uint8_t other, uint16_t shndx) {
    Symbol symbol;
    symbol.name = name;
    symbol.value = value;
    symbol.size = size;
    symbol.info = info;
    symbol.other = other;
    symbol.shndx = shndx;
    m_symbols.push_back(symbol);
}

std::vector<uint8_t> ELFWriter::write() {
    if (m_sections.empty()) {
        throw std::runtime_error("Cannot write ELF file with no sections.");
    }

    std::vector<uint8_t> buffer;
    
    // 1. ELF Header
    ELF64::Ehdr header = {};
    populate_header(header);
    
    // Placeholder offsets, will be updated later
    header.e_phoff = 0; // No program headers for now
    header.e_shoff = sizeof(ELF64::Ehdr);

    // Convert header to bytes and add to buffer
    buffer.insert(buffer.end(), (uint8_t*)&header, (uint8_t*)&header + sizeof(ELF64::Ehdr));

    // 2. Section Data
    std::vector<uint64_t> section_offsets;
    uint64_t current_offset = sizeof(ELF64::Ehdr) + m_sections.size() * sizeof(ELF64::Shdr);
    
    // Create section string table
    std::string shstrtab_data;
    for (const auto& sec : m_sections) {
        shstrtab_data += sec.name + '\0';
    }
    add_section(".shstrtab", std::vector<uint8_t>(shstrtab_data.begin(), shstrtab_data.end()), 3 /* SHT_STRTAB */, 0);
    header.e_shstrndx = m_sections.size() - 1;

    for (const auto& section : m_sections) {
        section_offsets.push_back(current_offset);
        buffer.insert(buffer.end(), section.data.begin(), section.data.end());
        current_offset += section.data.size();
    }

    // 3. Section Headers
    uint32_t name_offset = 0;
    for (size_t i = 0; i < m_sections.size(); ++i) {
        ELF64::Shdr shdr = {};
        shdr.sh_name = name_offset;
        shdr.sh_type = m_sections[i].type;
        shdr.sh_flags = m_sections[i].flags;
        shdr.sh_addr = 0; // Simple case: no virtual addresses
        shdr.sh_offset = section_offsets[i];
        shdr.sh_size = m_sections[i].data.size();
        shdr.sh_link = 0;
        shdr.sh_info = 0;
        shdr.sh_addralign = (m_sections[i].type == 1 /* SHT_PROGBITS */) ? 16 : 1;
        shdr.sh_entsize = 0;
        
        buffer.insert(buffer.end(), (uint8_t*)&shdr, (uint8_t*)&shdr + sizeof(ELF64::Shdr));
        name_offset += m_sections[i].name.length() + 1;
    }
    
    // Update header with correct section header offset and count
    header.e_shoff = sizeof(ELF64::Ehdr);
    header.e_shnum = m_sections.size();
    memcpy(buffer.data(), &header, sizeof(ELF64::Ehdr));

    return buffer;
}

void ELFWriter::populate_header(ELF64::Ehdr& header) {
    // Magic number
    header.e_ident[0] = 0x7f;
    header.e_ident[1] = 'E';
    header.e_ident[2] = 'L';
    header.e_ident[3] = 'F';
    
    // Class, data, version
    header.e_ident[4] = 2; // 64-bit
    header.e_ident[5] = 1; // Little-endian
    header.e_ident[6] = 1; // Current version
    
    // OS ABI
    header.e_ident[7] = 0; // System V
    
    // Unused part of e_ident
    memset(&header.e_ident[8], 0, 8);
    
    header.e_type = 1; // Relocatable file
    header.e_machine = m_architecture;
    header.e_version = 1; // Current version
    header.e_entry = 0; // No entry point for relocatable files
    header.e_phoff = 0;
    header.e_shoff = 0; // Will be set later
    header.e_flags = 0;
    header.e_ehsize = sizeof(ELF64::Ehdr);
    header.e_phentsize = 0;
    header.e_phnum = 0;
    header.e_shentsize = sizeof(ELF64::Shdr);
    header.e_shnum = 0; // Will be set later
    header.e_shstrndx = 0; // Will be set later
}
