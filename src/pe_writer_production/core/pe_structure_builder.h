// ============================================================================
// PE Structure Builder - Core Component
// Builds the complete PE file structure with all headers and sections
// ============================================================================

#pragma once

#include "../pe_writer.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

namespace pewriter {

// ============================================================================
// PE STRUCTURE DEFINITIONS
// ============================================================================

#pragma pack(push, 1)

struct IMAGE_DOS_HEADER {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    int32_t e_lfanew;
};

struct IMAGE_FILE_HEADER {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

struct IMAGE_DATA_DIRECTORY {
    uint32_t VirtualAddress;
    uint32_t Size;
};

struct IMAGE_OPTIONAL_HEADER64 {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[16];
};

struct IMAGE_NT_HEADERS64 {
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
};

struct IMAGE_SECTION_HEADER {
    char Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};

#pragma pack(pop)

// ============================================================================
// SECTION INFORMATION
// ============================================================================

struct SectionInfo {
    std::string name;
    std::vector<uint8_t> data;
    uint32_t virtualAddress;
    uint32_t rawAddress;
    uint32_t characteristics;
    bool isCode;
};

// ============================================================================
// PE STRUCTURE BUILDER CLASS
// ============================================================================

class PEStructureBuilder {
public:
    PEStructureBuilder();
    ~PEStructureBuilder() = default;

    void setConfig(const PEConfig& config);
    bool addSection(const std::string& name, const std::vector<uint8_t>& data,
                   uint32_t characteristics);
    bool build();
    const std::vector<uint8_t>& getImage() const;

    // Advanced features
    bool enableDebugInfo();
    bool addTLSSupport();
    bool addExceptionHandling();

    // Callbacks
    using ProgressCallback = std::function<void(int, const std::string&)>;
    void setProgressCallback(ProgressCallback callback);

private:
    // Header building functions
    bool buildDOSHeader();
    bool buildNTHeaders();
    bool buildSectionHeaders();
    bool buildSectionData();

    // Utility functions
    uint32_t alignUp(uint32_t value, uint32_t alignment) const;
    uint32_t getTimestamp() const;
    uint32_t calculateChecksum() const;

    // Member variables
    PEConfig config_;
    std::vector<uint8_t> image_;
    std::vector<SectionInfo> sections_;
    uint32_t currentVirtualAddress_;
    uint32_t currentRawAddress_;
    uint32_t entryPointRVA_;
    bool entryPointSet_;
    ProgressCallback progressCallback_;

    // Constants
    static constexpr uint32_t DOS_HEADER_SIZE = 64;
    static constexpr uint32_t DOS_STUB_SIZE = 64;
    static constexpr uint32_t NT_HEADERS_SIZE = 264; // 4 + 20 + 240
    static constexpr uint32_t SECTION_HEADER_SIZE = 40;
    static constexpr uint32_t IMAGE_BASE_OFFSET = 0x1000;
};

} // namespace pewriter