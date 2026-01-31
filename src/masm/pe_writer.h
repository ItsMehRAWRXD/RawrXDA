#pragma once

// RawrXD - PE Writer Header
// API for generating PE (Portable Executable) files for Windows.

#include <cstdint>
#include <vector>
#include <string>
#include <map>

class PEWriter {
public:
    PEWriter();
    ~PEWriter();

    // Set target architecture (e.g., 0x8664 for x64, 0x14c for I386)
    void set_architecture(uint16_t arch);

    // Add a section to the PE file
    void add_section(const std::string& name, const std::vector<uint8_t>& data, uint32_t characteristics);

    // Add an imported function from a DLL
    void add_import(const std::string& dll_name, const std::string& function_name);

    // Generate the final PE file as a byte vector
    std::vector<uint8_t> write();

private:
    struct Section {
        std::string name;
        std::vector<uint8_t> data;
        uint32_t characteristics;
    };

    uint16_t m_architecture;
    std::vector<Section> m_sections;
    std::map<std::string, std::vector<std::string>> m_imports;
};
