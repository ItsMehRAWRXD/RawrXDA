// =============================================================================
// Win32_DataDiode_Validator.h - Airgapped Binary Integrity Verification
// =============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {

// =============================================================================
// Validation Result Codes
// =============================================================================

enum class DataDiodeValidationResult {
    SUCCESS = 0,
    MANIFEST_PARSE_ERROR = 1,
    BINARY_LOAD_ERROR = 2,
    HASH_COMPUTATION_ERROR = 3,
    HASH_MISMATCH = 4,              // CRITICAL - Binary integrity compromised
    SENTINEL_MISSING = 5,
    EXPORT_PARSE_ERROR = 6,
    EXPORTS_INCOMPLETE = 7
};

// =============================================================================
// Validation Report Structure
// =============================================================================

struct DataDiodeValidationReport {
    std::wstring binaryPath;
    std::wstring manifestPath;
    uint64_t binarySize = 0;
    uint64_t validationTimestamp = 0;
    
    std::string expectedHash;
    std::string actualHash;
    
    std::vector<size_t> sentinelOffsets;
    size_t exportCount = 0;
    
    bool validationPassed = false;
    std::vector<std::string> errors;
};

// =============================================================================
// Main Validation API
// =============================================================================

/**
 * @brief Validate an SSAPYB/Heretic binary transferred via sneakernet
 * 
 * @param binaryPath Path to the .obj or .exe file to validate
 * @param manifestPath Path to the .sneaker-chain.json manifest
 * @param outReport Detailed validation report structure
 * @return DataDiodeValidationResult Success or specific failure code
 * 
 * @note Uses FIPS 140-2 certified CryptoAPI for SHA-256 computation
 * @note Verifies: SHA-256 hash, SSAPYB_SENTINEL presence, required exports
 * 
 * Usage Example (Airgapped Target Machine):
 * @code
 * DataDiodeValidationReport report;
 * auto result = ValidateSneakerChainBinary(
 *     L"D:\\sneakernet\\RawrXD_Heretic_Hotpatch.obj",
 *     L"D:\\sneakernet\\.sneaker-chain.json",
 *     report
 * );
 * 
 * if (result == DataDiodeValidationResult::SUCCESS) {
 *     printf("Binary validated - safe to deploy\n");
 *     // Proceed with integration
 * } else {
 *     printf("Validation failed: %s\n", GetValidationResultString(result).c_str());
 *     // Reject binary, trigger security alert
 * }
 * @endcode
 */
DataDiodeValidationResult ValidateSneakerChainBinary(
    const std::wstring& binaryPath,
    const std::wstring& manifestPath,
    DataDiodeValidationReport& outReport
);

/**
 * @brief Convert validation result code to human-readable string
 */
std::string GetValidationResultString(DataDiodeValidationResult result);

/**
 * @brief Print formatted validation report to console
 */
void PrintValidationReport(const DataDiodeValidationReport& report);

// =============================================================================
// Sneaker Chain Manifest Generator (Source Machine)
// =============================================================================

/**
 * @brief Generate a .sneaker-chain.json manifest for a binary
 * 
 * @param binaryPath Path to the .obj or .exe to package
 * @param manifestOutputPath Output path for manifest JSON
 * @param authorSignature Developer signature string
 * @return bool True if manifest generated successfully
 * 
 * Usage Example (Source Development Machine):
 * @code
 * bool success = GenerateSneakerChainManifest(
 *     L"d:\\rawrxd\\src\\asm\\RawrXD_Heretic_Hotpatch.obj",
 *     L"D:\\sneakernet\\.sneaker-chain.json",
 *     "RawrXD-Dev-Team-2026"
 * );
 * 
 * // Copy both .obj and .sneaker-chain.json to USB drive
 * // Transfer to airgapped target machine
 * @endcode
 */
bool GenerateSneakerChainManifest(
    const std::wstring& binaryPath,
    const std::wstring& manifestOutputPath,
    const std::string& authorSignature
);

} // namespace RawrXD
