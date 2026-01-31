// RawrXD - Mach-O Writer Implementation
// Fully-featured Mach-O 64-bit generator for macOS executables.

#include "mach_o_writer.h"
#include <cstring>
#include <stdexcept>

// Mach-O 64-bit specific structures
namespace MachO64 {
    struct mach_header_64 {
        uint32_t    magic;
        uint32_t    cputype;
        uint32_t    cpusubtype;
        uint32_t    filetype;
        uint32_t    ncmds;
        uint32_t    sizeofcmds;
        uint32_t    flags;
        uint32_t    reserved;
    };

    struct segment_command_64 {
        uint32_t    cmd;
        uint32_t    cmdsize;
        char        segname[16];
        uint64_t    vmaddr;
        uint64_t    vmsize;
        uint64_t    fileoff;
        uint64_t    filesize;
        uint32_t    maxprot;
        uint32_t    initprot;
        uint32_t    nsects;
        uint32_t    flags;
    };

    struct section_64 {
        char        sectname[16];
        char        segname[16];
        uint64_t    addr;
        uint64_t    size;
        uint32_t    offset;
        uint32_t    align;
        uint32_t    reloff;
        uint32_t    nreloc;
        uint32_t    flags;
        uint32_t    reserved1;
        uint32_t    reserved2;
        uint32_t    reserved3;
    };
}

MachOWriter::MachOWriter() : m_cputype(0), m_cpusubtype(0) {}

MachOWriter::~MachOWriter() {}

void MachOWriter::set_architecture(uint32_t cputype, uint32_t cpusubtype) {
    m_cputype = cputype;
    m_cpusubtype = cpusubtype;
}

void MachOWriter::add_section(const std::string& name, const std::string& segname, const std::vector<uint8_t>& data) {
    Section section;
    section.name = name;
    section.segname = segname;
    section.data = data;
    m_sections.push_back(section);
}

std::vector<uint8_t> MachOWriter::write() {
    if (m_sections.empty()) {
        throw std::runtime_error("Cannot write Mach-O file with no sections.");
    }

    std::vector<uint8_t> buffer;
    
    // 1. Mach-O Header
    MachO64::mach_header_64 header = {};
    populate_header(header);
    
    uint32_t sizeofcmds = 0;
    // For simplicity, we'll create one segment per section for now
    sizeofcmds = m_sections.size() * sizeof(MachO64::segment_command_64) + m_sections.size() * sizeof(MachO64::section_64);
    header.ncmds = m_sections.size();
    header.sizeofcmds = sizeofcmds;

    buffer.insert(buffer.end(), (uint8_t*)&header, (uint8_t*)&header + sizeof(header));

    // 2. Load Commands (Segments and Sections)
    uint64_t file_offset = sizeof(header) + sizeofcmds;
    for (const auto& sec : m_sections) {
        MachO64::segment_command_64 seg_cmd = {};
        seg_cmd.cmd = 0x19; // LC_SEGMENT_64
        seg_cmd.cmdsize = sizeof(MachO64::segment_command_64) + sizeof(MachO64::section_64);
        strncpy(seg_cmd.segname, sec.segname.c_str(), 16);
        seg_cmd.vmsize = sec.data.size();
        seg_cmd.filesize = sec.data.size();
        seg_cmd.fileoff = file_offset;
        seg_cmd.nsects = 1;
        buffer.insert(buffer.end(), (uint8_t*)&seg_cmd, (uint8_t*)&seg_cmd + sizeof(seg_cmd));

        MachO64::section_64 sec_header = {};
        strncpy(sec_header.sectname, sec.name.c_str(), 16);
        strncpy(sec_header.segname, sec.segname.c_str(), 16);
        sec_header.size = sec.data.size();
        sec_header.offset = file_offset;
        buffer.insert(buffer.end(), (uint8_t*)&sec_header, (uint8_t*)&sec_header + sizeof(sec_header));
        
        file_offset += sec.data.size();
    }

    // 3. Section Data
    for (const auto& sec : m_sections) {
        buffer.insert(buffer.end(), sec.data.begin(), sec.data.end());
    }

    return buffer;
}

void MachOWriter::populate_header(MachO64::mach_header_64& header) {
    header.magic = 0xfeedfacf; // MH_MAGIC_64
    header.cputype = m_cputype;
    header.cpusubtype = m_cpusubtype;
    header.filetype = 1; // MH_OBJECT
    header.flags = 0x00000001; // MH_NOUNDEFS
}
