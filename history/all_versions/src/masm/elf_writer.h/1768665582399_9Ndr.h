#pragma once

// Placeholder for ELF (Executable and Linkable Format) writer for Linux
// This file will contain the implementation for generating ELF executables.

// References:
// ELF Specification: https://refspecs.linuxfoundation.org/elf/elf.pdf

#include <cstdint>
#include <vector>
#include <string>

class ELFWriter {
public:
    ELFWriter();
    ~ELFWriter();

    void set_architecture(uint16_t arch);
    void add_section(const std::string& name, const std::vector<uint8_t>& data, uint32_t type, uint64_t flags);
    void add_symbol(const std::string& name, uint64_t value, uint64_t size, uint8_t info, uint8_t other, uint16_t shndx);
    
    std::vector<uint8_t> write();

private:
    // ELF Header
    // Section Headers
    // Symbol Table
    // String Table
    // ... and other ELF structures
};
