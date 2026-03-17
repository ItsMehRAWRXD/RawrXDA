#pragma once

// RawrXD - Mach-O Writer Header
// API for generating Mach-O 64-bit object files for macOS.

#include <cstdint>
#include <vector>
#include <string>

namespace MachO64 { struct mach_header_64; }

class MachOWriter {
public:
    MachOWriter();
    ~MachOWriter();

    // Set the target architecture (e.g., CPU_TYPE_X86_64)
    void set_architecture(uint32_t cputype, uint32_t cpusubtype);

    // Add a section to a segment
    void add_section(const std::string& name, const std::string& segname, const std::vector<uint8_t>& data);

    // Generate the final Mach-O file as a byte vector
    std::vector<uint8_t> write();

private:
    struct Section {
        std::string name;
        std::string segname;
        std::vector<uint8_t> data;
    };

    void populate_header(MachO64::mach_header_64& header);

    uint32_t m_cputype;
    uint32_t m_cpusubtype;
    std::vector<Section> m_sections;
};
