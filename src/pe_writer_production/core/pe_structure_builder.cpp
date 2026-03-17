// ============================================================================
// PE Structure Builder Implementation
// Builds complete PE file structure with proper alignment and RVA calculations
// ============================================================================

#include "pe_structure_builder.h"
#include <algorithm>
#include <cstring>
#include <ctime>

namespace pewriter {

// ============================================================================
// PEStructureBuilder Implementation
// ============================================================================

PEStructureBuilder::PEStructureBuilder()
    : currentVirtualAddress_(IMAGE_BASE_OFFSET),
      currentRawAddress_(0),
      entryPointRVA_(0),
      entryPointSet_(false) {
}

void PEStructureBuilder::setConfig(const PEConfig& config) {
    config_ = config;
    // Reset addresses for new build
    currentVirtualAddress_ = IMAGE_BASE_OFFSET;
    currentRawAddress_ = 0;
    entryPointRVA_ = 0;
}

bool PEStructureBuilder::addSection(const std::string& name,
                                   const std::vector<uint8_t>& data,
                                   uint32_t characteristics) {
    SectionInfo section;
    section.name = name;
    section.data = data;
    section.characteristics = characteristics;
    section.isCode = (characteristics & IMAGE_SCN_CNT_CODE) != 0;

    // Calculate virtual and raw addresses
    section.virtualAddress = currentVirtualAddress_;
    section.rawAddress = currentRawAddress_;

    // Auto-set entry point RVA from first code section
    if (section.isCode && !entryPointSet_) {
        entryPointRVA_ = currentVirtualAddress_;
        entryPointSet_ = true;
    }

    // Update current addresses
    uint32_t virtualSize = alignUp(static_cast<uint32_t>(data.size()),
                                  config_.sectionAlignment);
    uint32_t rawSize = alignUp(static_cast<uint32_t>(data.size()),
                              config_.fileAlignment);

    currentVirtualAddress_ += virtualSize;
    currentRawAddress_ += rawSize;

    sections_.push_back(section);
    return true;
}

bool PEStructureBuilder::build() {
    try {
        if (progressCallback_) {
            progressCallback_(0, "Starting PE build");
        }

        // Calculate total size
        uint32_t headersSize = DOS_HEADER_SIZE + DOS_STUB_SIZE + NT_HEADERS_SIZE +
                              static_cast<uint32_t>(sections_.size()) * SECTION_HEADER_SIZE;
        headersSize = alignUp(headersSize, config_.fileAlignment);

        uint32_t totalSize = headersSize + currentRawAddress_;

        // Allocate image buffer
        image_.resize(totalSize);
        std::fill(image_.begin(), image_.end(), 0);

        if (progressCallback_) {
            progressCallback_(10, "Building DOS header");
        }
        if (!buildDOSHeader()) return false;

        if (progressCallback_) {
            progressCallback_(20, "Building NT headers");
        }
        if (!buildNTHeaders()) return false;

        if (progressCallback_) {
            progressCallback_(30, "Building section headers");
        }
        if (!buildSectionHeaders()) return false;

        if (progressCallback_) {
            progressCallback_(40, "Building section data");
        }
        if (!buildSectionData()) return false;

        if (progressCallback_) {
            progressCallback_(100, "PE build completed");
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

const std::vector<uint8_t>& PEStructureBuilder::getImage() const {
    return image_;
}

bool PEStructureBuilder::enableDebugInfo() {
    // Implementation for debug information
    return true;
}

bool PEStructureBuilder::addTLSSupport() {
    // Implementation for TLS
    return true;
}

bool PEStructureBuilder::addExceptionHandling() {
    // Implementation for SEH
    return true;
}

void PEStructureBuilder::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
}

// ============================================================================
// PRIVATE IMPLEMENTATION
// ============================================================================

bool PEStructureBuilder::buildDOSHeader() {
    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(image_.data());

    // DOS header
    dosHeader->e_magic = IMAGE_DOS_SIGNATURE;
    dosHeader->e_cblp = 0x0090;
    dosHeader->e_cp = 0x0003;
    dosHeader->e_cparhdr = 0x0004;
    dosHeader->e_maxalloc = 0xFFFF;
    dosHeader->e_sp = 0x00B8;
    dosHeader->e_lfarlc = DOS_HEADER_SIZE;
    dosHeader->e_lfanew = DOS_HEADER_SIZE + DOS_STUB_SIZE;

    // DOS stub (minimal exit)
    uint8_t dosStub[] = {
        0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,
        0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x54, 0x68,
        0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72,
        0x61, 0x6D, 0x20, 0x63, 0x61, 0x6E, 0x6E, 0x6F,
        0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E,
        0x20, 0x69, 0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20,
        0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A,
        0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    std::memcpy(image_.data() + DOS_HEADER_SIZE, dosStub, DOS_STUB_SIZE);
    return true;
}

bool PEStructureBuilder::buildNTHeaders() {
    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(
        image_.data() + DOS_HEADER_SIZE + DOS_STUB_SIZE);

    // NT signature
    ntHeaders->Signature = IMAGE_NT_SIGNATURE;

    // File header
    ntHeaders->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    ntHeaders->FileHeader.NumberOfSections = static_cast<uint16_t>(sections_.size());
    ntHeaders->FileHeader.TimeDateStamp = getTimestamp();
    ntHeaders->FileHeader.PointerToSymbolTable = 0;
    ntHeaders->FileHeader.NumberOfSymbols = 0;
    ntHeaders->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    ntHeaders->FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE |
                                          IMAGE_FILE_LARGE_ADDRESS_AWARE;

    // Optional header
    auto& opt = ntHeaders->OptionalHeader;
    opt.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    opt.MajorLinkerVersion = 14;
    opt.MinorLinkerVersion = 0;

    // Calculate sizes
    uint32_t codeSize = 0, dataSize = 0;
    for (const auto& section : sections_) {
        if (section.isCode) {
            codeSize += alignUp(static_cast<uint32_t>(section.data.size()),
                               config_.sectionAlignment);
        } else {
            dataSize += alignUp(static_cast<uint32_t>(section.data.size()),
                               config_.sectionAlignment);
        }
    }

    opt.SizeOfCode = codeSize;
    opt.SizeOfInitializedData = dataSize;
    opt.SizeOfUninitializedData = 0;
    opt.AddressOfEntryPoint = entryPointRVA_;
    opt.BaseOfCode = IMAGE_BASE_OFFSET;
    opt.ImageBase = config_.imageBase;
    opt.SectionAlignment = config_.sectionAlignment;
    opt.FileAlignment = config_.fileAlignment;
    opt.MajorOperatingSystemVersion = 6;
    opt.MinorOperatingSystemVersion = 0;
    opt.MajorImageVersion = 0;
    opt.MinorImageVersion = 0;
    opt.MajorSubsystemVersion = 6;
    opt.MinorSubsystemVersion = 0;
    opt.Win32VersionValue = 0;
    opt.SizeOfImage = currentVirtualAddress_;
    opt.SizeOfHeaders = alignUp(DOS_HEADER_SIZE + DOS_STUB_SIZE + NT_HEADERS_SIZE +
                               static_cast<uint32_t>(sections_.size()) * SECTION_HEADER_SIZE,
                               config_.fileAlignment);
    opt.CheckSum = 0; // Will be calculated later
    opt.Subsystem = static_cast<uint16_t>(config_.subsystem);

    // DLL characteristics
    uint16_t dllChars = 0;
    if (config_.enableHighEntropyVA) dllChars |= IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA;
    if (config_.enableASLR) dllChars |= IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;
    if (config_.enableDEP) dllChars |= IMAGE_DLLCHARACTERISTICS_NX_COMPAT;
    dllChars |= IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE;
    opt.DllCharacteristics = dllChars;

    opt.SizeOfStackReserve = config_.stackReserve;
    opt.SizeOfStackCommit = config_.stackCommit;
    opt.SizeOfHeapReserve = config_.heapReserve;
    opt.SizeOfHeapCommit = config_.heapCommit;
    opt.LoaderFlags = 0;
    opt.NumberOfRvaAndSizes = 16;

    // Data directories initialized to zero already via image_ fill
    // NOTE: Do NOT zero them here - import resolver and other components
    // may have already populated entries before buildNTHeaders() runs.
    // The image buffer was zero-filled at allocation.

    return true;
}

bool PEStructureBuilder::buildSectionHeaders() {
    uint32_t headerOffset = DOS_HEADER_SIZE + DOS_STUB_SIZE + NT_HEADERS_SIZE;

    for (size_t i = 0; i < sections_.size(); ++i) {
        auto* sectionHeader = reinterpret_cast<IMAGE_SECTION_HEADER*>(
            image_.data() + headerOffset + i * SECTION_HEADER_SIZE);

        // Copy name (truncate if necessary)
        std::string name = sections_[i].name;
        if (name.length() > 8) name = name.substr(0, 8);
        std::strncpy(sectionHeader->Name, name.c_str(), 8);

        sectionHeader->VirtualSize = static_cast<uint32_t>(sections_[i].data.size());
        sectionHeader->VirtualAddress = sections_[i].virtualAddress;
        sectionHeader->SizeOfRawData = alignUp(static_cast<uint32_t>(sections_[i].data.size()),
                                             config_.fileAlignment);
        // rawAddress is relative to after headers - compute absolute file offset
        uint32_t headersSize = alignUp(DOS_HEADER_SIZE + DOS_STUB_SIZE + NT_HEADERS_SIZE +
                                      static_cast<uint32_t>(sections_.size()) * SECTION_HEADER_SIZE,
                                      config_.fileAlignment);
        sectionHeader->PointerToRawData = headersSize + sections_[i].rawAddress;
        sectionHeader->PointerToRelocations = 0;
        sectionHeader->PointerToLinenumbers = 0;
        sectionHeader->NumberOfRelocations = 0;
        sectionHeader->NumberOfLinenumbers = 0;
        sectionHeader->Characteristics = sections_[i].characteristics;
    }

    return true;
}

bool PEStructureBuilder::buildSectionData() {
    uint32_t headersSize = alignUp(DOS_HEADER_SIZE + DOS_STUB_SIZE + NT_HEADERS_SIZE +
                                  static_cast<uint32_t>(sections_.size()) * SECTION_HEADER_SIZE,
                                  config_.fileAlignment);

    for (const auto& section : sections_) {
        uint32_t offset = headersSize + section.rawAddress;
        if (offset + section.data.size() > image_.size()) {
            return false;
        }
        std::memcpy(image_.data() + offset, section.data.data(), section.data.size());
    }

    return true;
}

uint32_t PEStructureBuilder::alignUp(uint32_t value, uint32_t alignment) const {
    return (value + alignment - 1) & ~(alignment - 1);
}

uint32_t PEStructureBuilder::getTimestamp() const {
    return static_cast<uint32_t>(std::time(nullptr));
}

uint32_t PEStructureBuilder::calculateChecksum() const {
    // Standard PE checksum algorithm matching Windows CheckSumMappedFile()
    // Sum all 16-bit words (skipping CheckSum field), fold carry, add file size
    uint32_t checksum = 0;
    uint32_t fileSize = static_cast<uint32_t>(image_.size());

    // Locate CheckSum field via e_lfanew (not hardcoded stub constants)
    uint32_t checksumOffset = 0;
    if (image_.size() >= sizeof(IMAGE_DOS_HEADER)) {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image_.data());
        // CheckSum = e_lfanew + 4(sig) + 20(file hdr) + 64(opt hdr offset)
        checksumOffset = static_cast<uint32_t>(dos->e_lfanew) + 4 + 20 + 64;
    }

    for (size_t i = 0; i < image_.size(); i += 2) {
        // Skip the checksum field itself (4 bytes = 2 words)
        if (i == checksumOffset || i == checksumOffset + 2) continue;

        uint16_t word = 0;
        if (i + 1 < image_.size()) {
            std::memcpy(&word, image_.data() + i, 2);
        } else {
            word = image_[i]; // Last odd byte
        }

        checksum += word;
        // Fold carry into low 16 bits
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
    }

    // Final fold
    checksum = (checksum & 0xFFFF) + (checksum >> 16);

    // Add file size
    checksum += fileSize;

    return checksum;
}

} // namespace pewriter