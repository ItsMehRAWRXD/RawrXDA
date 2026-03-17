// ============================================================================
// PE Validator - Validates PE File Structure and Correctness
// Comprehensive validation of headers, sections, imports, and relocations
// ============================================================================

#pragma once

#include "../pe_writer.h"
#include <vector>
#include <string>
#include <functional>

namespace pewriter {

// ============================================================================
// VALIDATION RESULT
// ============================================================================

struct ValidationResult {
    bool isValid;
    std::string message;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

// ============================================================================
// PE VALIDATOR CLASS
// ============================================================================

class PEValidator {
public:
    PEValidator();
    ~PEValidator() = default;

    bool validateConfig(const PEConfig& config) const;
    ValidationResult validateImage(const std::vector<uint8_t>& image) const;
    ValidationResult validateHeaders(const std::vector<uint8_t>& image) const;
    ValidationResult validateSections(const std::vector<uint8_t>& image) const;
    ValidationResult validateImports(const std::vector<uint8_t>& image) const;
    ValidationResult validateRelocations(const std::vector<uint8_t>& image) const;

    // Detailed validation
    bool validateDOSHeader(const std::vector<uint8_t>& image) const;
    bool validateNTHeaders(const std::vector<uint8_t>& image) const;
    bool validateOptionalHeader(const std::vector<uint8_t>& image) const;
    bool validateSectionHeaders(const std::vector<uint8_t>& image) const;
    bool validateSectionData(const std::vector<uint8_t>& image) const;

    // Security validation
    bool validateSecurityFeatures(const std::vector<uint8_t>& image) const;
    bool validateCodeIntegrity(const std::vector<uint8_t>& image) const;

    // Callbacks
    using ProgressCallback = std::function<void(int, const std::string&)>;
    void setProgressCallback(ProgressCallback callback);

private:
    // Validation helpers
    bool checkImageSize(const std::vector<uint8_t>& image) const;
    bool checkAlignment(uint32_t value, uint32_t alignment) const;
    bool checkRVABounds(uint32_t rva, uint32_t size, const std::vector<uint8_t>& image) const;
    bool checkSectionBounds(uint32_t offset, uint32_t size, const std::vector<uint8_t>& image) const;

    // Header validation
    bool validateDOSSignature(const std::vector<uint8_t>& image) const;
    bool validatePESignature(const std::vector<uint8_t>& image) const;
    bool validateMachineType(const std::vector<uint8_t>& image) const;
    bool validateOptionalHeaderMagic(const std::vector<uint8_t>& image) const;

    // Section validation
    bool validateSectionTable(const std::vector<uint8_t>& image) const;
    bool validateSectionNames(const std::vector<uint8_t>& image) const;
    bool validateSectionRVAs(const std::vector<uint8_t>& image) const;

    // Import validation
    bool validateImportDescriptors(const std::vector<uint8_t>& image) const;
    bool validateILT(const std::vector<uint8_t>& image) const;
    bool validateIAT(const std::vector<uint8_t>& image) const;

    // Relocation validation
    bool validateRelocationTable(const std::vector<uint8_t>& image) const;

    // Member variables
    ProgressCallback progressCallback_;

    // Constants
    static constexpr uint32_t MIN_PE_SIZE = 0x200;
    static constexpr uint16_t IMAGE_DOS_SIGNATURE = 0x5A4D;
    static constexpr uint32_t IMAGE_NT_SIGNATURE = 0x00004550;
    static constexpr uint16_t IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20B;
    static constexpr uint16_t IMAGE_FILE_MACHINE_AMD64 = 0x8664;
};

} // namespace pewriter