// ============================================================================
// RawrXD Binary Patcher - PE Binary Modification Module
// Direct binary patching, section manipulation, import/export editing
// Pure Win32, No External Dependencies
// ============================================================================

#ifndef RAWRXD_BINARY_PATCHER_H
#define RAWRXD_BINARY_PATCHER_H

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// Patch Types
// ============================================================================

enum class PatchType {
    BytePatch,          // Direct byte replacement
    StringPatch,        // String replacement (with padding)
    NopPatch,           // NOP fill
    JumpPatch,          // Insert JMP instruction
    CallPatch,          // Insert CALL instruction
    FunctionHook,       // Full function hook
    ImportRedirect,     // Redirect import
    DataPatch           // Data section modification
};

// ============================================================================
// Patch Entry - Tracks individual patches
// ============================================================================

struct PatchEntry {
    uint64_t    rva;                    // Relative Virtual Address
    uint64_t    fileOffset;             // File offset
    PatchType   type;                   // Type of patch
    std::vector<uint8_t> originalBytes; // Original bytes (for undo)
    std::vector<uint8_t> patchedBytes;  // New bytes
    std::string description;            // Patch description
    bool        applied;                // Whether currently applied
};

// ============================================================================
// Section Entry - For section manipulation
// ============================================================================

struct SectionEntry {
    char        name[8];                // Section name
    uint32_t    virtualSize;            // Virtual size
    uint32_t    virtualAddress;         // RVA
    uint32_t    rawSize;                // Size of raw data
    uint32_t    rawAddress;             // File offset to raw data
    uint32_t    characteristics;        // Section flags
    std::vector<uint8_t> data;          // Section data
};

// ============================================================================
// Import Entry - For import table modification
// ============================================================================

struct ImportEntry {
    std::string dllName;
    std::string functionName;
    uint16_t    ordinal;
    uint64_t    iatRva;                 // Import Address Table RVA
    uint64_t    iltRva;                 // Import Lookup Table RVA
    bool        byOrdinal;
};

// ============================================================================
// Code Cave - For finding caves in PE
// ============================================================================

struct CodeCave {
    uint64_t    rva;
    uint64_t    fileOffset;
    uint32_t    size;
    std::string sectionName;
    bool        isExecutable;
};

// ============================================================================
// Relocation Entry
// ============================================================================

struct RelocationPatch {
    uint64_t    rva;
    uint16_t    type;                   // IMAGE_REL_BASED_*
    int64_t     delta;                  // Adjustment amount
};

// ============================================================================
// Binary Patcher Class
// ============================================================================

class BinaryPatcher {
public:
    BinaryPatcher();
    ~BinaryPatcher();
    
    // ========================================================================
    // File Operations
    // ========================================================================
    
    // Open PE file for patching
    bool Open(const std::string& filePath);
    
    // Save changes to current file
    bool Save();
    
    // Save to new file
    bool SaveAs(const std::string& newPath);
    
    // Close and cleanup
    void Close();
    
    // Check if file is loaded
    bool IsLoaded() const { return m_fileData != nullptr; }
    
    // ========================================================================
    // Basic Patching
    // ========================================================================
    
    // Patch bytes at RVA
    bool PatchBytes(uint64_t rva, const uint8_t* bytes, size_t count);
    bool PatchBytes(uint64_t rva, const std::vector<uint8_t>& bytes);
    
    // Patch bytes at file offset
    bool PatchBytesRaw(uint64_t fileOffset, const uint8_t* bytes, size_t count);
    bool PatchBytesRaw(uint64_t fileOffset, const std::vector<uint8_t>& bytes);
    
    // Patch string (will pad with zeros if shorter)
    bool PatchString(uint64_t rva, const std::string& newString, size_t maxLen = 0);
    bool PatchStringRaw(uint64_t fileOffset, const std::string& newString, size_t maxLen = 0);
    
    // Patch wide string
    bool PatchWideString(uint64_t rva, const std::wstring& newString, size_t maxLen = 0);
    
    // ========================================================================
    // Code Patching
    // ========================================================================
    
    // NOP out bytes at RVA
    bool NopOut(uint64_t rva, size_t count);
    
    // Insert JMP instruction (absolute or relative)
    bool InsertJump(uint64_t fromRva, uint64_t toRva);
    bool InsertJump32(uint64_t fromRva, uint64_t toRva);  // Near jump
    bool InsertJump64(uint64_t fromRva, uint64_t toRva);  // Far jump (14 bytes)
    
    // Insert CALL instruction
    bool InsertCall(uint64_t fromRva, uint64_t toRva);
    bool InsertCall32(uint64_t fromRva, uint64_t toRva);
    
    // Insert RET instruction
    bool InsertRet(uint64_t rva, uint16_t stackBytes = 0);
    
    // Insert INT3 breakpoint
    bool InsertInt3(uint64_t rva);
    
    // ========================================================================
    // Section Operations
    // ========================================================================
    
    // Add new section
    bool AddSection(const std::string& name, uint32_t size, uint32_t characteristics);
    
    // Remove section by name
    bool RemoveSection(const std::string& name);
    
    // Expand section by name
    bool ExpandSection(const std::string& name, uint32_t additionalSize);
    
    // Get section info
    bool GetSection(const std::string& name, SectionEntry& outSection);
    
    // Get all sections
    std::vector<SectionEntry> GetSections();
    
    // Set section characteristics
    bool SetSectionCharacteristics(const std::string& name, uint32_t characteristics);
    
    // ========================================================================
    // Import Table Operations
    // ========================================================================
    
    // Add import
    bool AddImport(const std::string& dllName, const std::string& funcName);
    
    // Remove import
    bool RemoveImport(const std::string& dllName, const std::string& funcName);
    
    // Redirect import to different function
    bool RedirectImport(const std::string& dllName, const std::string& funcName,
                        const std::string& newDll, const std::string& newFunc);
    
    // Get all imports
    std::vector<ImportEntry> GetImports();
    
    // ========================================================================
    // Export Table Operations
    // ========================================================================
    
    // Add export
    bool AddExport(const std::string& name, uint64_t rva, uint16_t ordinal);
    
    // Remove export
    bool RemoveExport(const std::string& name);
    
    // Redirect export
    bool RedirectExport(const std::string& name, uint64_t newRva);
    
    // ========================================================================
    // Code Cave Operations
    // ========================================================================
    
    // Find code caves (runs of zeros/padding)
    std::vector<CodeCave> FindCodeCaves(uint32_t minSize, bool executableOnly = true);
    
    // Find specific byte pattern
    std::vector<uint64_t> FindPattern(const uint8_t* pattern, const uint8_t* mask, size_t length);
    std::vector<uint64_t> FindPattern(const std::string& patternStr);
    
    // ========================================================================
    // Relocation Operations
    // ========================================================================
    
    // Add relocation entry
    bool AddRelocation(uint64_t rva, uint16_t type);
    
    // Remove relocation
    bool RemoveRelocation(uint64_t rva);
    
    // Apply relocations for new base
    bool ApplyRelocations(uint64_t newBase);
    
    // ========================================================================
    // Header Modifications
    // ========================================================================
    
    // Modify entry point
    bool SetEntryPoint(uint64_t rva);
    
    // Modify image base
    bool SetImageBase(uint64_t newBase);
    
    // Modify checksum
    bool RecalculateChecksum();
    
    // Remove ASLR/DEP/etc
    bool ClearSecurityFlags();
    
    // Set DLL characteristics
    bool SetDllCharacteristics(uint16_t flags);
    
    // ========================================================================
    // Patch Management
    // ========================================================================
    
    // Undo last patch
    bool Undo();
    
    // Undo specific patch
    bool UndoPatch(size_t patchIndex);
    
    // Get all patches
    const std::vector<PatchEntry>& GetPatches() const { return m_patches; }
    
    // Clear patch history
    void ClearPatchHistory();
    
    // Export patches to script
    std::string ExportPatchScript(const std::string& format = "python");
    
    // ========================================================================
    // Utility
    // ========================================================================
    
    // Convert RVA to file offset
    uint64_t RvaToFileOffset(uint64_t rva);
    
    // Convert file offset to RVA
    uint64_t FileOffsetToRva(uint64_t offset);
    
    // Read bytes at RVA
    std::vector<uint8_t> ReadBytes(uint64_t rva, size_t count);
    
    // Read bytes at file offset
    std::vector<uint8_t> ReadBytesRaw(uint64_t offset, size_t count);
    
    // Get file size
    uint64_t GetFileSize() const { return m_fileSize; }
    
    // Get last error
    std::string GetLastError() const { return m_lastError; }
    
    // Validate PE structure
    bool ValidatePE();
    
private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    
    bool ParseHeaders();
    bool AllocateFileBuffer(size_t size);
    bool ResizeFileBuffer(size_t newSize);
    
    void RecordPatch(uint64_t rva, const std::vector<uint8_t>& original,
                     const std::vector<uint8_t>& patched, PatchType type,
                     const std::string& description);
    
    // Section alignment helpers
    uint32_t AlignUp(uint32_t value, uint32_t alignment);
    uint32_t GetSectionAlignment() const;
    uint32_t GetFileAlignment() const;
    
    // Find section containing RVA
    IMAGE_SECTION_HEADER* FindSectionByRva(uint64_t rva);
    IMAGE_SECTION_HEADER* FindSectionByName(const std::string& name);
    
    // Update headers after modifications
    bool UpdateSizeOfImage();
    bool UpdateSectionHeaders();
    
    // Machine code generation helpers
    std::vector<uint8_t> GenerateJmp32(int32_t offset);
    std::vector<uint8_t> GenerateJmp64(uint64_t target);
    std::vector<uint8_t> GenerateCall32(int32_t offset);
    std::vector<uint8_t> GenerateCall64(uint64_t target);
    
    // ========================================================================
    // Member Variables
    // ========================================================================
    
    std::string         m_filePath;
    uint8_t*            m_fileData;
    size_t              m_fileSize;
    size_t              m_allocatedSize;
    bool                m_modified;
    
    // PE Headers (pointers into m_fileData)
    IMAGE_DOS_HEADER*           m_dosHeader;
    IMAGE_NT_HEADERS64*         m_ntHeaders64;
    IMAGE_NT_HEADERS32*         m_ntHeaders32;
    IMAGE_SECTION_HEADER*       m_sectionHeaders;
    uint16_t                    m_numSections;
    bool                        m_is64Bit;
    
    // Patch tracking
    std::vector<PatchEntry>     m_patches;
    
    // Error tracking
    std::string                 m_lastError;
};

} // namespace ReverseEngineering
} // namespace RawrXD

#endif // RAWRXD_BINARY_PATCHER_H
