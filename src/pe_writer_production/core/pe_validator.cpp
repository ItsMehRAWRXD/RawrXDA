// ============================================================================
// PE Validator Implementation
// Comprehensive validation of PE file structure and correctness
// ============================================================================

#include "pe_validator.h"
#include <algorithm>
#include <cstring>

namespace pewriter {

// ============================================================================
// PEValidator Implementation
// ============================================================================

PEValidator::PEValidator() {}

bool PEValidator::validateConfig(const PEConfig& config) const {
    // Basic config validation
    if (config.sectionAlignment == 0 || config.fileAlignment == 0) {
        return false;
    }

    if (config.sectionAlignment < config.fileAlignment) {
        return false;
    }

    if ((config.sectionAlignment & (config.sectionAlignment - 1)) != 0) {
        return false; // Not power of 2
    }

    if ((config.fileAlignment & (config.fileAlignment - 1)) != 0) {
        return false; // Not power of 2
    }

    return true;
}

ValidationResult PEValidator::validateImage(const std::vector<uint8_t>& image) const {
    ValidationResult result;
    result.isValid = true;

    if (progressCallback_) {
        progressCallback_(0, "Starting PE validation");
    }

    // Basic size check
    if (!checkImageSize(image)) {
        result.isValid = false;
        result.errors.push_back("PE image too small");
        return result;
    }

    if (progressCallback_) {
        progressCallback_(20, "Validating headers");
    }

    // Validate headers
    auto headerResult = validateHeaders(image);
    result.errors.insert(result.errors.end(),
                        headerResult.errors.begin(),
                        headerResult.errors.end());
    result.warnings.insert(result.warnings.end(),
                          headerResult.warnings.begin(),
                          headerResult.warnings.end());

    if (progressCallback_) {
        progressCallback_(40, "Validating sections");
    }

    // Validate sections
    auto sectionResult = validateSections(image);
    result.errors.insert(result.errors.end(),
                        sectionResult.errors.begin(),
                        sectionResult.errors.end());
    result.warnings.insert(result.warnings.end(),
                          sectionResult.warnings.begin(),
                          sectionResult.warnings.end());

    if (progressCallback_) {
        progressCallback_(60, "Validating imports");
    }

    // Validate imports
    auto importResult = validateImports(image);
    result.errors.insert(result.errors.end(),
                        importResult.errors.begin(),
                        importResult.errors.end());
    result.warnings.insert(result.warnings.end(),
                          importResult.warnings.begin(),
                          importResult.warnings.end());

    if (progressCallback_) {
        progressCallback_(80, "Validating relocations");
    }

    // Validate relocations
    auto relocResult = validateRelocations(image);
    result.errors.insert(result.errors.end(),
                        relocResult.errors.begin(),
                        relocResult.errors.end());
    result.warnings.insert(result.warnings.end(),
                          relocResult.warnings.begin(),
                          relocResult.warnings.end());

    if (progressCallback_) {
        progressCallback_(100, "Validation completed");
    }

    result.isValid = result.errors.empty();
    result.message = result.isValid ? "PE validation passed" : "PE validation failed";

    return result;
}

ValidationResult PEValidator::validateHeaders(const std::vector<uint8_t>& image) const {
    ValidationResult result;
    result.isValid = true;

    if (!validateDOSHeader(image)) {
        result.errors.push_back("Invalid DOS header");
        result.isValid = false;
    }

    if (!validateNTHeaders(image)) {
        result.errors.push_back("Invalid NT headers");
        result.isValid = false;
    }

    if (!validateOptionalHeader(image)) {
        result.errors.push_back("Invalid optional header");
        result.isValid = false;
    }

    return result;
}

ValidationResult PEValidator::validateSections(const std::vector<uint8_t>& image) const {
    ValidationResult result;
    result.isValid = true;

    if (!validateSectionHeaders(image)) {
        result.errors.push_back("Invalid section headers");
        result.isValid = false;
    }

    if (!validateSectionData(image)) {
        result.errors.push_back("Invalid section data");
        result.isValid = false;
    }

    return result;
}

ValidationResult PEValidator::validateImports(const std::vector<uint8_t>& image) const {
    ValidationResult result;
    result.isValid = true;

    if (!validateImportDescriptors(image)) {
        result.errors.push_back("Invalid import descriptors");
        result.isValid = false;
    }

    return result;
}

ValidationResult PEValidator::validateRelocations(const std::vector<uint8_t>& image) const {
    ValidationResult result;
    result.isValid = true;

    if (!validateRelocationTable(image)) {
        result.errors.push_back("Invalid relocation table");
        result.isValid = false;
    }

    return result;
}

// ============================================================================
// DETAILED VALIDATION METHODS
// ============================================================================

bool PEValidator::validateDOSHeader(const std::vector<uint8_t>& image) const {
    if (image.size() < sizeof(IMAGE_DOS_HEADER)) return false;

    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
    return dosHeader->e_magic == IMAGE_DOS_SIGNATURE &&
           dosHeader->e_lfanew >= sizeof(IMAGE_DOS_HEADER) &&
           dosHeader->e_lfanew < image.size();
}

bool PEValidator::validateNTHeaders(const std::vector<uint8_t>& image) const {
    if (!validateDOSHeader(image)) return false;

    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
    uint32_t ntOffset = dosHeader->e_lfanew;

    if (ntOffset + sizeof(IMAGE_NT_HEADERS64) > image.size()) return false;

    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image.data() + ntOffset);
    return ntHeaders->Signature == IMAGE_NT_SIGNATURE;
}

bool PEValidator::validateOptionalHeader(const std::vector<uint8_t>& image) const {
    if (!validateNTHeaders(image)) return false;

    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
    uint32_t ntOffset = dosHeader->e_lfanew;
    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image.data() + ntOffset);

    const auto& opt = ntHeaders->OptionalHeader;
    return opt.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
           opt.AddressOfEntryPoint < opt.SizeOfImage &&
           opt.BaseOfCode < opt.SizeOfImage &&
           opt.SectionAlignment >= opt.FileAlignment &&
           (opt.SectionAlignment & (opt.SectionAlignment - 1)) == 0 && // Power of 2
           (opt.FileAlignment & (opt.FileAlignment - 1)) == 0; // Power of 2
}

bool PEValidator::validateSectionHeaders(const std::vector<uint8_t>& image) const {
    if (!validateNTHeaders(image)) return false;

    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
    uint32_t ntOffset = dosHeader->e_lfanew;
    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image.data() + ntOffset);

    uint16_t numSections = ntHeaders->FileHeader.NumberOfSections;
    uint32_t sectionTableOffset = ntOffset + sizeof(IMAGE_NT_HEADERS64);

    if (sectionTableOffset + numSections * sizeof(IMAGE_SECTION_HEADER) > image.size()) {
        return false;
    }

    const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
        image.data() + sectionTableOffset);

    for (uint16_t i = 0; i < numSections; ++i) {
        const auto& section = sections[i];

        // Check RVA bounds
        if (section.VirtualAddress >= ntHeaders->OptionalHeader.SizeOfImage) {
            return false;
        }

        // Check raw data bounds
        if (section.PointerToRawData + section.SizeOfRawData > image.size()) {
            return false;
        }

        // Check virtual size vs raw size
        if (section.Misc.VirtualSize < section.SizeOfRawData) {
            return false;
        }
    }

    return true;
}

bool PEValidator::validateSectionData(const std::vector<uint8_t>& image) const {
    // Basic section data validation
    return validateSectionHeaders(image);
}

bool PEValidator::validateImportDescriptors(const std::vector<uint8_t>& image) const {
    if (!validateNTHeaders(image)) return false;

    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
    uint32_t ntOffset = dosHeader->e_lfanew;
    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image.data() + ntOffset);

    // Check if import directory exists
    uint32_t importRVA = ntHeaders->OptionalHeader.DataDirectory[1].VirtualAddress;
    uint32_t importSize = ntHeaders->OptionalHeader.DataDirectory[1].Size;

    if (importRVA == 0 || importSize == 0) {
        return true; // No imports is valid
    }

    if (!checkRVABounds(importRVA, importSize, image)) {
        return false;
    }

    // Basic import descriptor validation
    const auto* importDesc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(
        image.data() + importRVA);

    // Should end with null descriptor
    bool foundNull = false;
    uint32_t currentRVA = importRVA;

    while (currentRVA < importRVA + importSize - sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
        if (importDesc->OriginalFirstThunk == 0 &&
            importDesc->TimeDateStamp == 0 &&
            importDesc->ForwarderChain == 0 &&
            importDesc->Name == 0 &&
            importDesc->FirstThunk == 0) {
            foundNull = true;
            break;
        }
        importDesc++;
        currentRVA += sizeof(IMAGE_IMPORT_DESCRIPTOR);
    }

    return foundNull;
}

bool PEValidator::validateRelocationTable(const std::vector<uint8_t>& image) const {
    if (!validateNTHeaders(image)) return false;

    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
    uint32_t ntOffset = dosHeader->e_lfanew;
    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image.data() + ntOffset);

    // Check if relocation directory exists
    uint32_t relocRVA = ntHeaders->OptionalHeader.DataDirectory[5].VirtualAddress;
    uint32_t relocSize = ntHeaders->OptionalHeader.DataDirectory[5].Size;

    if (relocRVA == 0 || relocSize == 0) {
        return true; // No relocations is valid
    }

    return checkRVABounds(relocRVA, relocSize, image);
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

bool PEValidator::checkImageSize(const std::vector<uint8_t>& image) const {
    return image.size() >= MIN_PE_SIZE;
}

bool PEValidator::checkAlignment(uint32_t value, uint32_t alignment) const {
    return (value & (alignment - 1)) == 0;
}

bool PEValidator::checkRVABounds(uint32_t rva, uint32_t size, const std::vector<uint8_t>& image) const {
    if (!validateNTHeaders(image)) return false;

    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
    uint32_t ntOffset = dosHeader->e_lfanew;
    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image.data() + ntOffset);

    // Convert RVA to file offset by walking section headers
    uint16_t numSections = ntHeaders->FileHeader.NumberOfSections;
    uint32_t sectionTableOffset = ntOffset + 4 +
        sizeof(IMAGE_FILE_HEADER) +
        ntHeaders->FileHeader.SizeOfOptionalHeader;

    // Bounds-check the section table itself
    if (sectionTableOffset + numSections * sizeof(IMAGE_SECTION_HEADER) > image.size()) {
        return false;
    }

    const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
        image.data() + sectionTableOffset);

    // If the RVA falls within the headers (before the first section VA), file offset == RVA
    uint32_t fileOffset = rva; // default fallback for header RVAs

    for (uint16_t i = 0; i < numSections; ++i) {
        uint32_t sectionVA  = sections[i].VirtualAddress;
        uint32_t sectionRaw = sections[i].PointerToRawData;
        uint32_t sectionVirtualSize = sections[i].Misc.VirtualSize;
        uint32_t sectionRawSize     = sections[i].SizeOfRawData;

        // Use the larger of VirtualSize and SizeOfRawData for coverage
        uint32_t mappedSize = (sectionVirtualSize > sectionRawSize)
                                  ? sectionVirtualSize : sectionRawSize;

        if (rva >= sectionVA && rva < sectionVA + mappedSize) {
            fileOffset = (rva - sectionVA) + sectionRaw;
            break;
        }
    }

    return fileOffset + size <= image.size();
}

bool PEValidator::checkSectionBounds(uint32_t offset, uint32_t size, const std::vector<uint8_t>& image) const {
    return offset + size <= image.size();
}

void PEValidator::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
}

} // namespace pewriter