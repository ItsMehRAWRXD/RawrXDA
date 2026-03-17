#pragma once
// ============================================================================
// RawrXD PE Analyzer - Win32 Native PE File Parser
// Pure Win32 API, No External Dependencies
// ============================================================================

#ifndef RAWRXD_PE_ANALYZER_H
#define RAWRXD_PE_ANALYZER_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// PE Structures (Native Win32)
// ============================================================================

struct SectionInfo {
    char        name[16];           // Section name (null-terminated)
    uint32_t    virtualSize;        // Virtual size in memory
    uint32_t    virtualAddress;     // RVA of section
    uint32_t    rawDataSize;        // Size of raw data on disk
    uint32_t    rawDataOffset;      // File offset to raw data
    uint32_t    characteristics;    // Section characteristics
    bool        isExecutable;       // Contains code
    bool        isWritable;         // Writable data
    bool        isReadable;         // Readable
};

struct ImportedFunction {
    std::string name;               // Function name (empty if ordinal-only)
    uint16_t    ordinal;            // Ordinal number
    uint64_t    thunkRVA;           // RVA of IAT entry
    uint64_t    hintNameTableRVA;   // RVA of hint/name entry
    bool        isOrdinal;          // True if import by ordinal
};

struct ImportedDLL {
    std::string name;                           // DLL name
    uint64_t    importLookupTableRVA;           // ILT RVA
    uint64_t    importAddressTableRVA;          // IAT RVA
    uint32_t    timeDateStamp;                  // Bind timestamp
    std::vector<ImportedFunction> functions;    // Imported functions
};

struct ExportedFunction {
    std::string name;               // Function name
    uint32_t    ordinal;            // Export ordinal
    uint32_t    rva;                // Function RVA
    bool        isForwarder;        // True if forwarded
    std::string forwarderName;      // Forwarded to DLL.function
};

struct ResourceEntry {
    uint32_t    type;               // Resource type ID
    std::string typeName;           // Type name (if named)
    uint32_t    id;                 // Resource ID
    std::string name;               // Resource name (if named)
    uint32_t    languageId;         // Language ID
    uint32_t    dataRVA;            // RVA of resource data
    uint32_t    dataSize;           // Size of resource data
    uint32_t    codePage;           // Code page
};

struct DisassembledInstruction {
    uint64_t                address;    // Virtual address
    std::vector<uint8_t>    bytes;      // Raw instruction bytes
    std::string             mnemonic;   // Instruction mnemonic
    std::string             operands;   // Operands string
    std::string             comment;    // Auto-generated comment
    uint32_t                length;     // Instruction length
};

struct RelocEntry {
    uint32_t    rva;                // RVA of relocation
    uint16_t    type;               // Relocation type
};

struct DebugEntry {
    uint32_t    type;               // Debug type
    uint32_t    sizeOfData;         // Size of debug data
    uint32_t    addressOfRawData;   // RVA of debug data
    uint32_t    pointerToRawData;   // File offset of debug data
    uint32_t    timeDateStamp;      // Timestamp
};

// ============================================================================
// PE Analyzer Class
// ============================================================================

class PEAnalyzer {
public:
    PEAnalyzer();
    ~PEAnalyzer();

    // ========================================================================
    // Core Loading/Unloading
    // ========================================================================
    bool Load(const std::string& filePath);
    void Unload();
    bool IsLoaded() const { return m_fileData != nullptr; }
    const std::string& GetFilePath() const { return m_filePath; }

    // ========================================================================
    // PE Header Information
    // ========================================================================
    bool Is64Bit() const { return m_is64Bit; }
    bool IsDLL() const { return m_isDLL; }
    bool IsDriver() const { return m_isDriver; }
    
    uint64_t GetImageBase() const { return m_imageBase; }
    uint32_t GetEntryPoint() const { return m_entryPointRVA; }
    uint32_t GetSectionAlignment() const { return m_sectionAlignment; }
    uint32_t GetFileAlignment() const { return m_fileAlignment; }
    uint32_t GetImageSize() const { return m_imageSize; }
    uint32_t GetHeadersSize() const { return m_headersSize; }
    uint16_t GetSubsystem() const { return m_subsystem; }
    uint16_t GetMachine() const { return m_machine; }
    uint16_t GetCharacteristics() const { return m_characteristics; }
    uint16_t GetDllCharacteristics() const { return m_dllCharacteristics; }
    uint32_t GetTimeDateStamp() const { return m_timeDateStamp; }
    uint16_t GetMajorOSVersion() const { return m_majorOSVersion; }
    uint16_t GetMinorOSVersion() const { return m_minorOSVersion; }
    uint32_t GetChecksum() const { return m_checksum; }

    // ASLR/DEP/CFG flags
    bool HasASLR() const;
    bool HasDEP() const;
    bool HasCFG() const;
    bool HasHighEntropyASLR() const;
    bool HasSEH() const;
    bool HasSafeSEH() const;

    // ========================================================================
    // Sections
    // ========================================================================
    std::vector<SectionInfo> GetSections() const;
    std::vector<uint8_t> ReadSection(const std::string& name) const;
    std::vector<uint8_t> ReadSectionByIndex(size_t index) const;
    const SectionInfo* FindSectionByRVA(uint32_t rva) const;
    const SectionInfo* FindSectionByName(const std::string& name) const;
    size_t GetSectionCount() const { return m_sections.size(); }

    // ========================================================================
    // Imports
    // ========================================================================
    std::vector<ImportedDLL> GetImports() const;
    std::vector<ImportedFunction> GetImportedFunctions(const std::string& dllName) const;
    size_t GetImportedDLLCount() const { return m_imports.size(); }
    size_t GetTotalImportCount() const;

    // ========================================================================
    // Exports
    // ========================================================================
    std::vector<ExportedFunction> GetExports() const;
    const ExportedFunction* FindExportByName(const std::string& name) const;
    const ExportedFunction* FindExportByOrdinal(uint32_t ordinal) const;
    size_t GetExportCount() const { return m_exports.size(); }
    std::string GetExportDLLName() const { return m_exportDLLName; }

    // ========================================================================
    // Resources
    // ========================================================================
    std::vector<ResourceEntry> GetResources() const;
    std::vector<uint8_t> ReadResource(const ResourceEntry& entry) const;
    std::vector<uint8_t> ReadResourceByType(uint32_t type, uint32_t id) const;
    bool HasResources() const { return !m_resources.empty(); }

    // ========================================================================
    // Relocations
    // ========================================================================
    std::vector<RelocEntry> GetRelocations() const;
    bool HasRelocations() const { return m_hasRelocations; }

    // ========================================================================
    // Debug Information
    // ========================================================================
    std::vector<DebugEntry> GetDebugEntries() const;
    std::string GetPDBPath() const { return m_pdbPath; }
    bool HasDebugInfo() const { return !m_debugEntries.empty(); }

    // ========================================================================
    // Raw File Access
    // ========================================================================
    std::vector<uint8_t> ReadBytes(uint32_t fileOffset, size_t count) const;
    std::vector<uint8_t> ReadBytesAtRVA(uint32_t rva, size_t count) const;
    uint32_t RVAToFileOffset(uint32_t rva) const;
    uint32_t FileOffsetToRVA(uint32_t fileOffset) const;
    size_t GetFileSize() const { return m_fileSize; }
    const uint8_t* GetRawData() const { return m_fileData; }

    // ========================================================================
    // Disassembly (Basic x64/x86)
    // ========================================================================
    std::vector<DisassembledInstruction> Disassemble(uint64_t rva, size_t count) const;
    std::vector<DisassembledInstruction> DisassembleBytes(const uint8_t* code, size_t size, uint64_t baseAddress) const;

    // ========================================================================
    // Strings
    // ========================================================================
    std::vector<std::string> ExtractStrings(size_t minLength = 4) const;
    std::vector<std::wstring> ExtractUnicodeStrings(size_t minLength = 4) const;

    // ========================================================================
    // Validation
    // ========================================================================
    bool ValidateChecksum() const;
    bool ValidateSignature() const;
    std::string GetLastError() const { return m_lastError; }

private:
    // Internal parsing methods
    bool ParseDOSHeader();
    bool ParseNTHeaders();
    bool ParseSectionHeaders();
    bool ParseImports();
    bool ParseExports();
    bool ParseResources();
    bool ParseResourceDirectory(uint32_t dirRVA, uint32_t level, uint32_t typeId, uint32_t nameId);
    bool ParseRelocations();
    bool ParseDebugDirectory();
    
    // Helper methods
    uint32_t ReadDWord(uint32_t offset) const;
    uint16_t ReadWord(uint32_t offset) const;
    uint64_t ReadQWord(uint32_t offset) const;
    std::string ReadString(uint32_t offset, size_t maxLen = 256) const;
    std::string ReadStringAtRVA(uint32_t rva, size_t maxLen = 256) const;
    std::wstring ReadUnicodeString(uint32_t offset, size_t maxLen = 256) const;

    // x64/x86 instruction length decoder
    uint32_t GetInstructionLength(const uint8_t* code, size_t remaining) const;
    void DecodeInstruction(const uint8_t* code, size_t remaining, DisassembledInstruction& out) const;

    // Member data
    std::string             m_filePath;
    uint8_t*                m_fileData;
    size_t                  m_fileSize;
    bool                    m_is64Bit;
    bool                    m_isDLL;
    bool                    m_isDriver;
    
    // DOS Header
    uint32_t                m_peHeaderOffset;
    
    // NT Headers
    uint64_t                m_imageBase;
    uint32_t                m_entryPointRVA;
    uint32_t                m_sectionAlignment;
    uint32_t                m_fileAlignment;
    uint32_t                m_imageSize;
    uint32_t                m_headersSize;
    uint16_t                m_subsystem;
    uint16_t                m_machine;
    uint16_t                m_characteristics;
    uint16_t                m_dllCharacteristics;
    uint32_t                m_timeDateStamp;
    uint16_t                m_majorOSVersion;
    uint16_t                m_minorOSVersion;
    uint32_t                m_checksum;
    uint16_t                m_numberOfSections;
    
    // Data Directories
    uint32_t                m_importTableRVA;
    uint32_t                m_importTableSize;
    uint32_t                m_exportTableRVA;
    uint32_t                m_exportTableSize;
    uint32_t                m_resourceTableRVA;
    uint32_t                m_resourceTableSize;
    uint32_t                m_relocationTableRVA;
    uint32_t                m_relocationTableSize;
    uint32_t                m_debugTableRVA;
    uint32_t                m_debugTableSize;
    uint32_t                m_tlsTableRVA;
    uint32_t                m_tlsTableSize;
    uint32_t                m_iatRVA;
    uint32_t                m_iatSize;
    
    // Parsed data
    std::vector<SectionInfo>        m_sections;
    std::vector<ImportedDLL>        m_imports;
    std::vector<ExportedFunction>   m_exports;
    std::vector<ResourceEntry>      m_resources;
    std::vector<RelocEntry>         m_relocations;
    std::vector<DebugEntry>         m_debugEntries;
    std::string                     m_exportDLLName;
    std::string                     m_pdbPath;
    bool                            m_hasRelocations;
    
    // Error state
    mutable std::string             m_lastError;
};

} // namespace ReverseEngineering
} // namespace RawrXD

#endif // RAWRXD_PE_ANALYZER_H
