// ============================================================================
// RawrXD Binary Patcher - Implementation
// Direct binary patching, section manipulation, import/export editing
// Pure Win32, No External Dependencies
// ============================================================================

#include "binary_patcher.h"
#include <cstring>
#include <cstdio>
#include <sstream>
#include <algorithm>

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// Constructor / Destructor
// ============================================================================

BinaryPatcher::BinaryPatcher()
    : m_fileData(nullptr)
    , m_fileSize(0)
    , m_allocatedSize(0)
    , m_modified(false)
    , m_dosHeader(nullptr)
    , m_ntHeaders64(nullptr)
    , m_ntHeaders32(nullptr)
    , m_sectionHeaders(nullptr)
    , m_numSections(0)
    , m_is64Bit(false)
{
}

BinaryPatcher::~BinaryPatcher()
{
    Close();
}

// ============================================================================
// File Operations
// ============================================================================

bool BinaryPatcher::Open(const std::string& filePath)
{
    Close(); // Close any existing file
    
    // Open file with Win32 API
    HANDLE hFile = CreateFileA(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        m_lastError = "Failed to open file";
        return false;
    }
    
    // Get file size
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        m_lastError = "Failed to get file size";
        return false;
    }
    
    m_fileSize = static_cast<size_t>(fileSize.QuadPart);
    
    // Allocate buffer with extra space for modifications
    m_allocatedSize = m_fileSize + (1024 * 1024); // Extra 1MB
    m_fileData = static_cast<uint8_t*>(VirtualAlloc(
        NULL,
        m_allocatedSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    ));
    
    if (!m_fileData) {
        CloseHandle(hFile);
        m_lastError = "Failed to allocate memory";
        return false;
    }
    
    // Read file
    DWORD bytesRead;
    if (!ReadFile(hFile, m_fileData, static_cast<DWORD>(m_fileSize), &bytesRead, NULL) ||
        bytesRead != m_fileSize) {
        VirtualFree(m_fileData, 0, MEM_RELEASE);
        m_fileData = nullptr;
        CloseHandle(hFile);
        m_lastError = "Failed to read file";
        return false;
    }
    
    CloseHandle(hFile);
    m_filePath = filePath;
    
    // Parse PE headers
    if (!ParseHeaders()) {
        VirtualFree(m_fileData, 0, MEM_RELEASE);
        m_fileData = nullptr;
        m_lastError = "Invalid PE file";
        return false;
    }
    
    return true;
}

bool BinaryPatcher::Save()
{
    return SaveAs(m_filePath);
}

bool BinaryPatcher::SaveAs(const std::string& newPath)
{
    if (!m_fileData || m_fileSize == 0) {
        m_lastError = "No file loaded";
        return false;
    }
    
    // Recalculate checksum before saving
    RecalculateChecksum();
    
    // Create output file
    HANDLE hFile = CreateFileA(
        newPath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        m_lastError = "Failed to create output file";
        return false;
    }
    
    // Write data
    DWORD bytesWritten;
    if (!WriteFile(hFile, m_fileData, static_cast<DWORD>(m_fileSize), &bytesWritten, NULL) ||
        bytesWritten != m_fileSize) {
        CloseHandle(hFile);
        m_lastError = "Failed to write file";
        return false;
    }
    
    CloseHandle(hFile);
    m_modified = false;
    return true;
}

void BinaryPatcher::Close()
{
    if (m_fileData) {
        VirtualFree(m_fileData, 0, MEM_RELEASE);
        m_fileData = nullptr;
    }
    
    m_fileSize = 0;
    m_allocatedSize = 0;
    m_modified = false;
    m_dosHeader = nullptr;
    m_ntHeaders64 = nullptr;
    m_ntHeaders32 = nullptr;
    m_sectionHeaders = nullptr;
    m_numSections = 0;
    m_is64Bit = false;
    m_patches.clear();
    m_filePath.clear();
}

bool BinaryPatcher::ParseHeaders()
{
    if (!m_fileData || m_fileSize < sizeof(IMAGE_DOS_HEADER)) {
        return false;
    }
    
    // DOS Header
    m_dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(m_fileData);
    if (m_dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    
    // NT Headers
    if (static_cast<size_t>(m_dosHeader->e_lfanew) + sizeof(IMAGE_NT_HEADERS32) > m_fileSize) {
        return false;
    }
    
    uint8_t* ntHeaderPtr = m_fileData + m_dosHeader->e_lfanew;
    uint32_t signature = *reinterpret_cast<uint32_t*>(ntHeaderPtr);
    if (signature != IMAGE_NT_SIGNATURE) {
        return false;
    }
    
    // Check architecture
    uint16_t machine = *reinterpret_cast<uint16_t*>(ntHeaderPtr + 4);
    m_is64Bit = (machine == IMAGE_FILE_MACHINE_AMD64);
    
    if (m_is64Bit) {
        m_ntHeaders64 = reinterpret_cast<IMAGE_NT_HEADERS64*>(ntHeaderPtr);
        m_ntHeaders32 = nullptr;
        m_numSections = m_ntHeaders64->FileHeader.NumberOfSections;
        m_sectionHeaders = reinterpret_cast<IMAGE_SECTION_HEADER*>(
            ntHeaderPtr + sizeof(IMAGE_NT_HEADERS64)
        );
    } else {
        m_ntHeaders32 = reinterpret_cast<IMAGE_NT_HEADERS32*>(ntHeaderPtr);
        m_ntHeaders64 = nullptr;
        m_numSections = m_ntHeaders32->FileHeader.NumberOfSections;
        m_sectionHeaders = reinterpret_cast<IMAGE_SECTION_HEADER*>(
            ntHeaderPtr + sizeof(IMAGE_NT_HEADERS32)
        );
    }
    
    return true;
}

// ============================================================================
// Basic Patching
// ============================================================================

bool BinaryPatcher::PatchBytes(uint64_t rva, const uint8_t* bytes, size_t count)
{
    uint64_t offset = RvaToFileOffset(rva);
    if (offset == 0 && rva != 0) {
        m_lastError = "Invalid RVA";
        return false;
    }
    return PatchBytesRaw(offset, bytes, count);
}

bool BinaryPatcher::PatchBytes(uint64_t rva, const std::vector<uint8_t>& bytes)
{
    return PatchBytes(rva, bytes.data(), bytes.size());
}

bool BinaryPatcher::PatchBytesRaw(uint64_t fileOffset, const uint8_t* bytes, size_t count)
{
    if (!m_fileData || fileOffset + count > m_fileSize) {
        m_lastError = "Offset out of range";
        return false;
    }
    
    // Record original bytes
    std::vector<uint8_t> original(m_fileData + fileOffset, m_fileData + fileOffset + count);
    std::vector<uint8_t> patched(bytes, bytes + count);
    
    // Apply patch
    memcpy(m_fileData + fileOffset, bytes, count);
    
    // Record for undo
    uint64_t rva = FileOffsetToRva(fileOffset);
    RecordPatch(rva, original, patched, PatchType::BytePatch, "Byte patch");
    
    m_modified = true;
    return true;
}

bool BinaryPatcher::PatchBytesRaw(uint64_t fileOffset, const std::vector<uint8_t>& bytes)
{
    return PatchBytesRaw(fileOffset, bytes.data(), bytes.size());
}

bool BinaryPatcher::PatchString(uint64_t rva, const std::string& newString, size_t maxLen)
{
    uint64_t offset = RvaToFileOffset(rva);
    if (offset == 0 && rva != 0) {
        m_lastError = "Invalid RVA";
        return false;
    }
    return PatchStringRaw(offset, newString, maxLen);
}

bool BinaryPatcher::PatchStringRaw(uint64_t fileOffset, const std::string& newString, size_t maxLen)
{
    if (!m_fileData) {
        m_lastError = "No file loaded";
        return false;
    }
    
    // Determine max length
    if (maxLen == 0) {
        // Find null terminator in original
        maxLen = 0;
        while (fileOffset + maxLen < m_fileSize && m_fileData[fileOffset + maxLen] != 0) {
            ++maxLen;
        }
        ++maxLen; // Include null terminator
    }
    
    if (fileOffset + maxLen > m_fileSize) {
        m_lastError = "String extends beyond file";
        return false;
    }
    
    // Prepare padded string
    std::vector<uint8_t> padded(maxLen, 0);
    size_t copyLen = std::min(newString.size(), maxLen - 1);
    memcpy(padded.data(), newString.c_str(), copyLen);
    
    // Record original
    std::vector<uint8_t> original(m_fileData + fileOffset, m_fileData + fileOffset + maxLen);
    
    // Apply patch
    memcpy(m_fileData + fileOffset, padded.data(), maxLen);
    
    // Record for undo
    uint64_t rva = FileOffsetToRva(fileOffset);
    RecordPatch(rva, original, padded, PatchType::StringPatch, "String patch");
    
    m_modified = true;
    return true;
}

bool BinaryPatcher::PatchWideString(uint64_t rva, const std::wstring& newString, size_t maxLen)
{
    uint64_t offset = RvaToFileOffset(rva);
    if (offset == 0 && rva != 0) {
        m_lastError = "Invalid RVA";
        return false;
    }
    
    if (maxLen == 0) {
        maxLen = (newString.size() + 1) * sizeof(wchar_t);
    }
    
    if (offset + maxLen > m_fileSize) {
        m_lastError = "String extends beyond file";
        return false;
    }
    
    std::vector<uint8_t> padded(maxLen, 0);
    size_t copyLen = std::min(newString.size() * sizeof(wchar_t), maxLen - sizeof(wchar_t));
    memcpy(padded.data(), newString.c_str(), copyLen);
    
    std::vector<uint8_t> original(m_fileData + offset, m_fileData + offset + maxLen);
    memcpy(m_fileData + offset, padded.data(), maxLen);
    
    RecordPatch(rva, original, padded, PatchType::StringPatch, "Wide string patch");
    m_modified = true;
    return true;
}

// ============================================================================
// Code Patching
// ============================================================================

bool BinaryPatcher::NopOut(uint64_t rva, size_t count)
{
    std::vector<uint8_t> nops(count, 0x90);
    return PatchBytes(rva, nops);
}

bool BinaryPatcher::InsertJump(uint64_t fromRva, uint64_t toRva)
{
    // Calculate displacement
    int64_t displacement = static_cast<int64_t>(toRva) - static_cast<int64_t>(fromRva) - 5;
    
    if (displacement >= INT32_MIN && displacement <= INT32_MAX) {
        return InsertJump32(fromRva, toRva);
    } else {
        return InsertJump64(fromRva, toRva);
    }
}

bool BinaryPatcher::InsertJump32(uint64_t fromRva, uint64_t toRva)
{
    // JMP rel32 (E9 xx xx xx xx)
    int32_t displacement = static_cast<int32_t>(toRva - fromRva - 5);
    
    std::vector<uint8_t> jmpCode = GenerateJmp32(displacement);
    return PatchBytes(fromRva, jmpCode);
}

bool BinaryPatcher::InsertJump64(uint64_t fromRva, uint64_t toRva)
{
    // JMP [RIP+0]; DQ target (FF 25 00 00 00 00 xx xx xx xx xx xx xx xx)
    std::vector<uint8_t> jmpCode = GenerateJmp64(toRva);
    return PatchBytes(fromRva, jmpCode);
}

bool BinaryPatcher::InsertCall(uint64_t fromRva, uint64_t toRva)
{
    int64_t displacement = static_cast<int64_t>(toRva) - static_cast<int64_t>(fromRva) - 5;
    
    if (displacement >= INT32_MIN && displacement <= INT32_MAX) {
        return InsertCall32(fromRva, toRva);
    } else {
        return InsertCall64(fromRva, toRva);
    }
}

bool BinaryPatcher::InsertCall32(uint64_t fromRva, uint64_t toRva)
{
    // CALL rel32 (E8 xx xx xx xx)
    int32_t displacement = static_cast<int32_t>(toRva - fromRva - 5);
    
    std::vector<uint8_t> callCode = GenerateCall32(displacement);
    return PatchBytes(fromRva, callCode);
}

bool BinaryPatcher::InsertRet(uint64_t rva, uint16_t stackBytes)
{
    if (stackBytes == 0) {
        // RET (C3)
        uint8_t ret = 0xC3;
        return PatchBytes(rva, &ret, 1);
    } else {
        // RET imm16 (C2 xx xx)
        uint8_t code[] = { 0xC2, static_cast<uint8_t>(stackBytes & 0xFF), 
                           static_cast<uint8_t>((stackBytes >> 8) & 0xFF) };
        return PatchBytes(rva, code, 3);
    }
}

bool BinaryPatcher::InsertInt3(uint64_t rva)
{
    uint8_t int3 = 0xCC;
    return PatchBytes(rva, &int3, 1);
}

// ============================================================================
// Section Operations
// ============================================================================

bool BinaryPatcher::AddSection(const std::string& name, uint32_t size, uint32_t characteristics)
{
    if (!m_fileData || !m_sectionHeaders) {
        m_lastError = "No file loaded";
        return false;
    }
    
    // Get alignment values
    uint32_t sectionAlign = GetSectionAlignment();
    uint32_t fileAlign = GetFileAlignment();
    
    // Calculate new section position
    IMAGE_SECTION_HEADER* lastSection = &m_sectionHeaders[m_numSections - 1];
    
    uint32_t newVirtualAddress = AlignUp(
        lastSection->VirtualAddress + lastSection->Misc.VirtualSize,
        sectionAlign
    );
    
    uint32_t newRawAddress = AlignUp(
        lastSection->PointerToRawData + lastSection->SizeOfRawData,
        fileAlign
    );
    
    uint32_t alignedSize = AlignUp(size, sectionAlign);
    uint32_t alignedRawSize = AlignUp(size, fileAlign);
    
    // Check if we have room for another section header
    size_t sectionHeadersEnd = reinterpret_cast<size_t>(m_sectionHeaders + m_numSections + 1) -
                                reinterpret_cast<size_t>(m_fileData);
    
    if (sectionHeadersEnd > m_sectionHeaders[0].PointerToRawData) {
        m_lastError = "No room for section header";
        return false;
    }
    
    // Resize file if needed
    size_t newFileSize = newRawAddress + alignedRawSize;
    if (newFileSize > m_allocatedSize) {
        if (!ResizeFileBuffer(newFileSize + (1024 * 1024))) {
            m_lastError = "Failed to expand file buffer";
            return false;
        }
        // Re-parse headers after resize
        ParseHeaders();
    }
    
    // Zero new section data area
    memset(m_fileData + newRawAddress, 0, alignedRawSize);
    
    // Setup new section header
    IMAGE_SECTION_HEADER* newSection = &m_sectionHeaders[m_numSections];
    memset(newSection, 0, sizeof(IMAGE_SECTION_HEADER));
    
    // Copy name (max 8 chars, null padded)
    strncpy(reinterpret_cast<char*>(newSection->Name), name.c_str(), 8);
    
    newSection->Misc.VirtualSize = size;
    newSection->VirtualAddress = newVirtualAddress;
    newSection->SizeOfRawData = alignedRawSize;
    newSection->PointerToRawData = newRawAddress;
    newSection->Characteristics = characteristics;
    
    // Update number of sections
    ++m_numSections;
    if (m_is64Bit) {
        m_ntHeaders64->FileHeader.NumberOfSections = m_numSections;
    } else {
        m_ntHeaders32->FileHeader.NumberOfSections = m_numSections;
    }
    
    // Update SizeOfImage
    m_fileSize = newFileSize;
    UpdateSizeOfImage();
    
    m_modified = true;
    return true;
}

bool BinaryPatcher::RemoveSection(const std::string& name)
{
    IMAGE_SECTION_HEADER* section = FindSectionByName(name);
    if (!section) {
        m_lastError = "Section not found";
        return false;
    }
    
    // Find section index
    size_t sectionIdx = section - m_sectionHeaders;
    
    // Don't allow removing first or last section (more complex)
    if (sectionIdx == 0 || sectionIdx == m_numSections - 1) {
        m_lastError = "Cannot remove first or last section";
        return false;
    }
    
    // Shift subsequent sections
    memmove(section, section + 1, (m_numSections - sectionIdx - 1) * sizeof(IMAGE_SECTION_HEADER));
    
    // Clear last section
    memset(&m_sectionHeaders[m_numSections - 1], 0, sizeof(IMAGE_SECTION_HEADER));
    
    // Update count
    --m_numSections;
    if (m_is64Bit) {
        m_ntHeaders64->FileHeader.NumberOfSections = m_numSections;
    } else {
        m_ntHeaders32->FileHeader.NumberOfSections = m_numSections;
    }
    
    m_modified = true;
    return true;
}

bool BinaryPatcher::ExpandSection(const std::string& name, uint32_t additionalSize)
{
    IMAGE_SECTION_HEADER* section = FindSectionByName(name);
    if (!section) {
        m_lastError = "Section not found";
        return false;
    }
    
    // For simplicity, only allow expanding last section
    size_t sectionIdx = section - m_sectionHeaders;
    if (sectionIdx != m_numSections - 1) {
        m_lastError = "Can only expand last section";
        return false;
    }
    
    uint32_t fileAlign = GetFileAlignment();
    uint32_t sectionAlign = GetSectionAlignment();
    
    uint32_t newVirtualSize = section->Misc.VirtualSize + additionalSize;
    uint32_t newRawSize = AlignUp(newVirtualSize, fileAlign);
    
    // Resize file
    size_t newFileSize = section->PointerToRawData + newRawSize;
    if (newFileSize > m_allocatedSize) {
        if (!ResizeFileBuffer(newFileSize + (1024 * 1024))) {
            m_lastError = "Failed to expand file buffer";
            return false;
        }
        ParseHeaders();
        section = FindSectionByName(name);
    }
    
    // Zero new area
    size_t oldEnd = section->PointerToRawData + section->SizeOfRawData;
    memset(m_fileData + oldEnd, 0, newFileSize - oldEnd);
    
    // Update section header
    section->Misc.VirtualSize = newVirtualSize;
    section->SizeOfRawData = newRawSize;
    
    m_fileSize = newFileSize;
    UpdateSizeOfImage();
    
    m_modified = true;
    return true;
}

bool BinaryPatcher::GetSection(const std::string& name, SectionEntry& outSection)
{
    IMAGE_SECTION_HEADER* section = FindSectionByName(name);
    if (!section) {
        return false;
    }
    
    memcpy(outSection.name, section->Name, 8);
    outSection.virtualSize = section->Misc.VirtualSize;
    outSection.virtualAddress = section->VirtualAddress;
    outSection.rawSize = section->SizeOfRawData;
    outSection.rawAddress = section->PointerToRawData;
    outSection.characteristics = section->Characteristics;
    
    // Copy data
    if (section->PointerToRawData + section->SizeOfRawData <= m_fileSize) {
        outSection.data.resize(section->SizeOfRawData);
        memcpy(outSection.data.data(), m_fileData + section->PointerToRawData, section->SizeOfRawData);
    }
    
    return true;
}

std::vector<SectionEntry> BinaryPatcher::GetSections()
{
    std::vector<SectionEntry> sections;
    
    for (uint16_t i = 0; i < m_numSections; ++i) {
        SectionEntry entry;
        memcpy(entry.name, m_sectionHeaders[i].Name, 8);
        entry.virtualSize = m_sectionHeaders[i].Misc.VirtualSize;
        entry.virtualAddress = m_sectionHeaders[i].VirtualAddress;
        entry.rawSize = m_sectionHeaders[i].SizeOfRawData;
        entry.rawAddress = m_sectionHeaders[i].PointerToRawData;
        entry.characteristics = m_sectionHeaders[i].Characteristics;
        sections.push_back(entry);
    }
    
    return sections;
}

bool BinaryPatcher::SetSectionCharacteristics(const std::string& name, uint32_t characteristics)
{
    IMAGE_SECTION_HEADER* section = FindSectionByName(name);
    if (!section) {
        m_lastError = "Section not found";
        return false;
    }
    
    section->Characteristics = characteristics;
    m_modified = true;
    return true;
}

// ============================================================================
// Import Table Operations
// ============================================================================

bool BinaryPatcher::AddImport(const std::string& dllName, const std::string& funcName)
{
    // Adding imports requires rebuilding the import table
    // This is a complex operation that requires:
    // 1. Finding or creating space for new import descriptor
    // 2. Creating new ILT and IAT entries
    // 3. Adding DLL name and function name strings
    // 4. Updating data directory
    
    m_lastError = "AddImport requires import table rebuild - complex operation";
    // Full implementation would need to:
    // - Create new section or expand existing one
    // - Build new import table structure
    // - Copy existing imports
    // - Add new import
    // - Update data directory
    return false;
}

bool BinaryPatcher::RemoveImport(const std::string& dllName, const std::string& funcName)
{
    m_lastError = "RemoveImport requires import table rebuild - complex operation";
    return false;
}

bool BinaryPatcher::RedirectImport(const std::string& dllName, const std::string& funcName,
                                   const std::string& newDll, const std::string& newFunc)
{
    // Simple case: if new strings fit in existing space, just overwrite
    // Complex case: requires rebuilding import table
    
    // Find import directory
    IMAGE_DATA_DIRECTORY* importDir = nullptr;
    if (m_is64Bit) {
        importDir = &m_ntHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    } else {
        importDir = &m_ntHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    }
    
    if (importDir->VirtualAddress == 0) {
        m_lastError = "No import table";
        return false;
    }
    
    uint64_t offset = RvaToFileOffset(importDir->VirtualAddress);
    IMAGE_IMPORT_DESCRIPTOR* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        m_fileData + offset
    );
    
    // Scan for matching DLL and function
    while (importDesc->Name != 0) {
        uint64_t nameOffset = RvaToFileOffset(importDesc->Name);
        char* dllNamePtr = reinterpret_cast<char*>(m_fileData + nameOffset);
        
        if (_stricmp(dllNamePtr, dllName.c_str()) == 0) {
            // Found DLL, now scan functions
            uint64_t iltRva = importDesc->OriginalFirstThunk ? 
                              importDesc->OriginalFirstThunk : importDesc->FirstThunk;
            
            if (m_is64Bit) {
                IMAGE_THUNK_DATA64* thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(
                    m_fileData + RvaToFileOffset(iltRva)
                );
                
                while (thunk->u1.AddressOfData != 0) {
                    if (!(thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
                        uint64_t hintOffset = RvaToFileOffset(thunk->u1.AddressOfData);
                        IMAGE_IMPORT_BY_NAME* hint = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                            m_fileData + hintOffset
                        );
                        
                        if (strcmp(reinterpret_cast<char*>(hint->Name), funcName.c_str()) == 0) {
                            // Found the import! Patch the name if it fits
                            size_t oldLen = strlen(reinterpret_cast<char*>(hint->Name));
                            if (newFunc.size() <= oldLen) {
                                memset(hint->Name, 0, oldLen);
                                memcpy(hint->Name, newFunc.c_str(), newFunc.size());
                                
                                // Also patch DLL name if needed
                                size_t oldDllLen = strlen(dllNamePtr);
                                if (newDll.size() <= oldDllLen) {
                                    memset(dllNamePtr, 0, oldDllLen);
                                    memcpy(dllNamePtr, newDll.c_str(), newDll.size());
                                    m_modified = true;
                                    return true;
                                }
                            }
                            m_lastError = "New names too long";
                            return false;
                        }
                    }
                    ++thunk;
                }
            } else {
                IMAGE_THUNK_DATA32* thunk = reinterpret_cast<IMAGE_THUNK_DATA32*>(
                    m_fileData + RvaToFileOffset(iltRva)
                );
                
                while (thunk->u1.AddressOfData != 0) {
                    if (!(thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32)) {
                        uint64_t hintOffset = RvaToFileOffset(thunk->u1.AddressOfData);
                        IMAGE_IMPORT_BY_NAME* hint = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                            m_fileData + hintOffset
                        );
                        
                        if (strcmp(reinterpret_cast<char*>(hint->Name), funcName.c_str()) == 0) {
                            size_t oldLen = strlen(reinterpret_cast<char*>(hint->Name));
                            if (newFunc.size() <= oldLen) {
                                memset(hint->Name, 0, oldLen);
                                memcpy(hint->Name, newFunc.c_str(), newFunc.size());
                                
                                size_t oldDllLen = strlen(dllNamePtr);
                                if (newDll.size() <= oldDllLen) {
                                    memset(dllNamePtr, 0, oldDllLen);
                                    memcpy(dllNamePtr, newDll.c_str(), newDll.size());
                                    m_modified = true;
                                    return true;
                                }
                            }
                            m_lastError = "New names too long";
                            return false;
                        }
                    }
                    ++thunk;
                }
            }
        }
        ++importDesc;
    }
    
    m_lastError = "Import not found";
    return false;
}

std::vector<ImportEntry> BinaryPatcher::GetImports()
{
    std::vector<ImportEntry> imports;
    
    if (!m_fileData) return imports;
    
    IMAGE_DATA_DIRECTORY* importDir = nullptr;
    if (m_is64Bit) {
        importDir = &m_ntHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    } else {
        importDir = &m_ntHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    }
    
    if (importDir->VirtualAddress == 0) return imports;
    
    uint64_t offset = RvaToFileOffset(importDir->VirtualAddress);
    if (offset == 0) return imports;
    
    IMAGE_IMPORT_DESCRIPTOR* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        m_fileData + offset
    );
    
    while (importDesc->Name != 0) {
        uint64_t nameOffset = RvaToFileOffset(importDesc->Name);
        std::string dllName = reinterpret_cast<char*>(m_fileData + nameOffset);
        
        uint64_t iltRva = importDesc->OriginalFirstThunk ? 
                          importDesc->OriginalFirstThunk : importDesc->FirstThunk;
        uint64_t iatRva = importDesc->FirstThunk;
        
        if (m_is64Bit) {
            IMAGE_THUNK_DATA64* thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(
                m_fileData + RvaToFileOffset(iltRva)
            );
            uint64_t iatOffset = iatRva;
            
            while (thunk->u1.AddressOfData != 0) {
                ImportEntry entry;
                entry.dllName = dllName;
                entry.iatRva = iatOffset;
                entry.iltRva = iltRva;
                
                if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) {
                    entry.ordinal = static_cast<uint16_t>(thunk->u1.Ordinal & 0xFFFF);
                    entry.byOrdinal = true;
                } else {
                    uint64_t hintOffset = RvaToFileOffset(thunk->u1.AddressOfData);
                    IMAGE_IMPORT_BY_NAME* hint = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                        m_fileData + hintOffset
                    );
                    entry.functionName = reinterpret_cast<char*>(hint->Name);
                    entry.ordinal = hint->Hint;
                    entry.byOrdinal = false;
                }
                
                imports.push_back(entry);
                ++thunk;
                iatOffset += 8;
            }
        } else {
            IMAGE_THUNK_DATA32* thunk = reinterpret_cast<IMAGE_THUNK_DATA32*>(
                m_fileData + RvaToFileOffset(iltRva)
            );
            uint64_t iatOffset = iatRva;
            
            while (thunk->u1.AddressOfData != 0) {
                ImportEntry entry;
                entry.dllName = dllName;
                entry.iatRva = iatOffset;
                entry.iltRva = iltRva;
                
                if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32) {
                    entry.ordinal = static_cast<uint16_t>(thunk->u1.Ordinal & 0xFFFF);
                    entry.byOrdinal = true;
                } else {
                    uint64_t hintOffset = RvaToFileOffset(thunk->u1.AddressOfData);
                    IMAGE_IMPORT_BY_NAME* hint = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                        m_fileData + hintOffset
                    );
                    entry.functionName = reinterpret_cast<char*>(hint->Name);
                    entry.ordinal = hint->Hint;
                    entry.byOrdinal = false;
                }
                
                imports.push_back(entry);
                ++thunk;
                iatOffset += 4;
            }
        }
        
        ++importDesc;
    }
    
    return imports;
}

// ============================================================================
// Export Table Operations
// ============================================================================

bool BinaryPatcher::AddExport(const std::string& name, uint64_t rva, uint16_t ordinal)
{
    m_lastError = "AddExport requires export table rebuild - complex operation";
    return false;
}

bool BinaryPatcher::RemoveExport(const std::string& name)
{
    m_lastError = "RemoveExport requires export table rebuild - complex operation";
    return false;
}

bool BinaryPatcher::RedirectExport(const std::string& name, uint64_t newRva)
{
    if (!m_fileData) {
        m_lastError = "No file loaded";
        return false;
    }
    
    IMAGE_DATA_DIRECTORY* exportDir = nullptr;
    if (m_is64Bit) {
        exportDir = &m_ntHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    } else {
        exportDir = &m_ntHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    }
    
    if (exportDir->VirtualAddress == 0) {
        m_lastError = "No export table";
        return false;
    }
    
    uint64_t offset = RvaToFileOffset(exportDir->VirtualAddress);
    IMAGE_EXPORT_DIRECTORY* exportTable = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
        m_fileData + offset
    );
    
    uint32_t* nameRvas = reinterpret_cast<uint32_t*>(
        m_fileData + RvaToFileOffset(exportTable->AddressOfNames)
    );
    uint16_t* ordinals = reinterpret_cast<uint16_t*>(
        m_fileData + RvaToFileOffset(exportTable->AddressOfNameOrdinals)
    );
    uint32_t* functions = reinterpret_cast<uint32_t*>(
        m_fileData + RvaToFileOffset(exportTable->AddressOfFunctions)
    );
    
    for (DWORD i = 0; i < exportTable->NumberOfNames; ++i) {
        uint64_t nameOffset = RvaToFileOffset(nameRvas[i]);
        const char* exportName = reinterpret_cast<const char*>(m_fileData + nameOffset);
        
        if (strcmp(exportName, name.c_str()) == 0) {
            uint16_t ordinal = ordinals[i];
            functions[ordinal] = static_cast<uint32_t>(newRva);
            m_modified = true;
            return true;
        }
    }
    
    m_lastError = "Export not found";
    return false;
}

// ============================================================================
// Code Cave Operations
// ============================================================================

std::vector<CodeCave> BinaryPatcher::FindCodeCaves(uint32_t minSize, bool executableOnly)
{
    std::vector<CodeCave> caves;
    
    for (uint16_t i = 0; i < m_numSections; ++i) {
        IMAGE_SECTION_HEADER* section = &m_sectionHeaders[i];
        
        if (executableOnly && !(section->Characteristics & IMAGE_SCN_MEM_EXECUTE)) {
            continue;
        }
        
        // Check for padding at end of section
        uint32_t usedSize = section->Misc.VirtualSize;
        uint32_t allocSize = section->SizeOfRawData;
        
        if (allocSize > usedSize && (allocSize - usedSize) >= minSize) {
            CodeCave cave;
            cave.rva = section->VirtualAddress + usedSize;
            cave.fileOffset = section->PointerToRawData + usedSize;
            cave.size = allocSize - usedSize;
            memcpy(cave.sectionName.data(), section->Name, 8);
            cave.isExecutable = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
            caves.push_back(cave);
        }
        
        // Scan for runs of zeros within section
        uint8_t* sectionData = m_fileData + section->PointerToRawData;
        uint32_t runStart = 0;
        uint32_t runLength = 0;
        
        for (uint32_t j = 0; j < section->SizeOfRawData; ++j) {
            if (sectionData[j] == 0x00 || sectionData[j] == 0xCC) {
                if (runLength == 0) {
                    runStart = j;
                }
                ++runLength;
            } else {
                if (runLength >= minSize) {
                    CodeCave cave;
                    cave.rva = section->VirtualAddress + runStart;
                    cave.fileOffset = section->PointerToRawData + runStart;
                    cave.size = runLength;
                    memcpy(cave.sectionName.data(), section->Name, 8);
                    cave.isExecutable = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
                    caves.push_back(cave);
                }
                runLength = 0;
            }
        }
        
        // Check final run
        if (runLength >= minSize) {
            CodeCave cave;
            cave.rva = section->VirtualAddress + runStart;
            cave.fileOffset = section->PointerToRawData + runStart;
            cave.size = runLength;
            memcpy(cave.sectionName.data(), section->Name, 8);
            cave.isExecutable = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
            caves.push_back(cave);
        }
    }
    
    return caves;
}

std::vector<uint64_t> BinaryPatcher::FindPattern(const uint8_t* pattern, const uint8_t* mask, size_t length)
{
    std::vector<uint64_t> results;
    
    for (uint16_t i = 0; i < m_numSections; ++i) {
        IMAGE_SECTION_HEADER* section = &m_sectionHeaders[i];
        uint8_t* sectionData = m_fileData + section->PointerToRawData;
        
        for (uint32_t j = 0; j + length <= section->SizeOfRawData; ++j) {
            bool match = true;
            
            for (size_t k = 0; k < length && match; ++k) {
                if (mask[k] == 0xFF && sectionData[j + k] != pattern[k]) {
                    match = false;
                }
            }
            
            if (match) {
                results.push_back(section->VirtualAddress + j);
            }
        }
    }
    
    return results;
}

std::vector<uint64_t> BinaryPatcher::FindPattern(const std::string& patternStr)
{
    // Parse pattern string like "48 8B ?? 00 ?? FF"
    std::vector<uint8_t> pattern;
    std::vector<uint8_t> mask;
    
    std::istringstream iss(patternStr);
    std::string token;
    
    while (iss >> token) {
        if (token == "?" || token == "??") {
            pattern.push_back(0x00);
            mask.push_back(0x00);
        } else {
            pattern.push_back(static_cast<uint8_t>(strtoul(token.c_str(), nullptr, 16)));
            mask.push_back(0xFF);
        }
    }
    
    return FindPattern(pattern.data(), mask.data(), pattern.size());
}

// ============================================================================
// Relocation Operations
// ============================================================================

bool BinaryPatcher::AddRelocation(uint64_t rva, uint16_t type)
{
    m_lastError = "AddRelocation requires relocation table rebuild";
    return false;
}

bool BinaryPatcher::RemoveRelocation(uint64_t rva)
{
    m_lastError = "RemoveRelocation requires relocation table rebuild";
    return false;
}

bool BinaryPatcher::ApplyRelocations(uint64_t newBase)
{
    if (!m_fileData) {
        m_lastError = "No file loaded";
        return false;
    }
    
    uint64_t oldBase;
    IMAGE_DATA_DIRECTORY* relocDir;
    
    if (m_is64Bit) {
        oldBase = m_ntHeaders64->OptionalHeader.ImageBase;
        relocDir = &m_ntHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    } else {
        oldBase = m_ntHeaders32->OptionalHeader.ImageBase;
        relocDir = &m_ntHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    }
    
    if (relocDir->VirtualAddress == 0 || relocDir->Size == 0) {
        m_lastError = "No relocation table";
        return false;
    }
    
    int64_t delta = static_cast<int64_t>(newBase) - static_cast<int64_t>(oldBase);
    
    uint64_t relocOffset = RvaToFileOffset(relocDir->VirtualAddress);
    uint8_t* relocData = m_fileData + relocOffset;
    uint32_t processed = 0;
    
    while (processed < relocDir->Size) {
        IMAGE_BASE_RELOCATION* block = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
            relocData + processed
        );
        
        if (block->SizeOfBlock == 0) break;
        
        uint32_t numEntries = (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
        uint16_t* entries = reinterpret_cast<uint16_t*>(block + 1);
        
        for (uint32_t i = 0; i < numEntries; ++i) {
            uint16_t type = entries[i] >> 12;
            uint16_t offset = entries[i] & 0xFFF;
            
            if (type == IMAGE_REL_BASED_ABSOLUTE) continue;
            
            uint64_t targetRva = block->VirtualAddress + offset;
            uint64_t targetOffset = RvaToFileOffset(targetRva);
            
            if (type == IMAGE_REL_BASED_HIGHLOW) {
                uint32_t* target = reinterpret_cast<uint32_t*>(m_fileData + targetOffset);
                *target += static_cast<uint32_t>(delta);
            } else if (type == IMAGE_REL_BASED_DIR64) {
                uint64_t* target = reinterpret_cast<uint64_t*>(m_fileData + targetOffset);
                *target += delta;
            }
        }
        
        processed += block->SizeOfBlock;
    }
    
    // Update image base
    if (m_is64Bit) {
        m_ntHeaders64->OptionalHeader.ImageBase = newBase;
    } else {
        m_ntHeaders32->OptionalHeader.ImageBase = static_cast<uint32_t>(newBase);
    }
    
    m_modified = true;
    return true;
}

// ============================================================================
// Header Modifications
// ============================================================================

bool BinaryPatcher::SetEntryPoint(uint64_t rva)
{
    if (!m_fileData) {
        m_lastError = "No file loaded";
        return false;
    }
    
    if (m_is64Bit) {
        m_ntHeaders64->OptionalHeader.AddressOfEntryPoint = static_cast<uint32_t>(rva);
    } else {
        m_ntHeaders32->OptionalHeader.AddressOfEntryPoint = static_cast<uint32_t>(rva);
    }
    
    m_modified = true;
    return true;
}

bool BinaryPatcher::SetImageBase(uint64_t newBase)
{
    if (!m_fileData) {
        m_lastError = "No file loaded";
        return false;
    }
    
    if (m_is64Bit) {
        m_ntHeaders64->OptionalHeader.ImageBase = newBase;
    } else {
        m_ntHeaders32->OptionalHeader.ImageBase = static_cast<uint32_t>(newBase);
    }
    
    m_modified = true;
    return true;
}

bool BinaryPatcher::RecalculateChecksum()
{
    if (!m_fileData || m_fileSize == 0) {
        m_lastError = "No file loaded";
        return false;
    }
    
    // Clear existing checksum
    if (m_is64Bit) {
        m_ntHeaders64->OptionalHeader.CheckSum = 0;
    } else {
        m_ntHeaders32->OptionalHeader.CheckSum = 0;
    }
    
    // Calculate PE checksum
    uint32_t checksum = 0;
    uint32_t* data = reinterpret_cast<uint32_t*>(m_fileData);
    size_t words = m_fileSize / 4;
    
    for (size_t i = 0; i < words; ++i) {
        uint64_t sum = static_cast<uint64_t>(checksum) + data[i];
        checksum = static_cast<uint32_t>(sum) + static_cast<uint32_t>(sum >> 32);
    }
    
    // Handle remaining bytes
    size_t remaining = m_fileSize % 4;
    if (remaining > 0) {
        uint32_t lastWord = 0;
        memcpy(&lastWord, m_fileData + (words * 4), remaining);
        uint64_t sum = static_cast<uint64_t>(checksum) + lastWord;
        checksum = static_cast<uint32_t>(sum) + static_cast<uint32_t>(sum >> 32);
    }
    
    // Fold to 16 bits
    checksum = (checksum & 0xFFFF) + (checksum >> 16);
    checksum = (checksum & 0xFFFF) + (checksum >> 16);
    
    // Add file size
    checksum += static_cast<uint32_t>(m_fileSize);
    
    // Store checksum
    if (m_is64Bit) {
        m_ntHeaders64->OptionalHeader.CheckSum = checksum;
    } else {
        m_ntHeaders32->OptionalHeader.CheckSum = checksum;
    }
    
    return true;
}

bool BinaryPatcher::ClearSecurityFlags()
{
    if (!m_fileData) {
        m_lastError = "No file loaded";
        return false;
    }
    
    uint16_t dllChars;
    if (m_is64Bit) {
        dllChars = m_ntHeaders64->OptionalHeader.DllCharacteristics;
    } else {
        dllChars = m_ntHeaders32->OptionalHeader.DllCharacteristics;
    }
    
    // Clear ASLR, DEP, SEH, CFG, etc
    dllChars &= ~IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;    // ASLR
    dllChars &= ~IMAGE_DLLCHARACTERISTICS_NX_COMPAT;       // DEP
    dllChars &= ~IMAGE_DLLCHARACTERISTICS_NO_SEH;          // SEH
    dllChars &= ~IMAGE_DLLCHARACTERISTICS_GUARD_CF;        // CFG
    dllChars &= ~IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY; // Integrity check
    
    if (m_is64Bit) {
        m_ntHeaders64->OptionalHeader.DllCharacteristics = dllChars;
    } else {
        m_ntHeaders32->OptionalHeader.DllCharacteristics = dllChars;
    }
    
    m_modified = true;
    return true;
}

bool BinaryPatcher::SetDllCharacteristics(uint16_t flags)
{
    if (!m_fileData) {
        m_lastError = "No file loaded";
        return false;
    }
    
    if (m_is64Bit) {
        m_ntHeaders64->OptionalHeader.DllCharacteristics = flags;
    } else {
        m_ntHeaders32->OptionalHeader.DllCharacteristics = flags;
    }
    
    m_modified = true;
    return true;
}

// ============================================================================
// Patch Management
// ============================================================================

bool BinaryPatcher::Undo()
{
    if (m_patches.empty()) {
        m_lastError = "No patches to undo";
        return false;
    }
    
    return UndoPatch(m_patches.size() - 1);
}

bool BinaryPatcher::UndoPatch(size_t patchIndex)
{
    if (patchIndex >= m_patches.size()) {
        m_lastError = "Invalid patch index";
        return false;
    }
    
    PatchEntry& patch = m_patches[patchIndex];
    if (!patch.applied) {
        m_lastError = "Patch not applied";
        return false;
    }
    
    // Restore original bytes
    if (patch.fileOffset + patch.originalBytes.size() <= m_fileSize) {
        memcpy(m_fileData + patch.fileOffset, 
               patch.originalBytes.data(), 
               patch.originalBytes.size());
        patch.applied = false;
        m_modified = true;
        return true;
    }
    
    m_lastError = "Patch offset out of range";
    return false;
}

void BinaryPatcher::ClearPatchHistory()
{
    m_patches.clear();
}

std::string BinaryPatcher::ExportPatchScript(const std::string& format)
{
    std::ostringstream oss;
    
    if (format == "python") {
        oss << "# RawrXD Patch Script\n";
        oss << "# Generated for: " << m_filePath << "\n\n";
        oss << "patches = [\n";
        
        for (const auto& patch : m_patches) {
            oss << "    {\n";
            oss << "        'rva': 0x" << std::hex << patch.rva << ",\n";
            oss << "        'offset': 0x" << patch.fileOffset << ",\n";
            oss << "        'original': bytes([";
            for (size_t i = 0; i < patch.originalBytes.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << "0x" << static_cast<int>(patch.originalBytes[i]);
            }
            oss << "]),\n";
            oss << "        'patched': bytes([";
            for (size_t i = 0; i < patch.patchedBytes.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << "0x" << static_cast<int>(patch.patchedBytes[i]);
            }
            oss << "]),\n";
            oss << "        'description': '" << patch.description << "'\n";
            oss << "    },\n";
        }
        
        oss << "]\n";
    } else if (format == "c") {
        oss << "// RawrXD Patch Data\n";
        oss << "// Generated for: " << m_filePath << "\n\n";
        
        for (size_t i = 0; i < m_patches.size(); ++i) {
            const auto& patch = m_patches[i];
            oss << "// Patch " << i << ": " << patch.description << "\n";
            oss << "const uint8_t patch" << i << "_bytes[] = { ";
            for (size_t j = 0; j < patch.patchedBytes.size(); ++j) {
                if (j > 0) oss << ", ";
                oss << "0x" << std::hex << static_cast<int>(patch.patchedBytes[j]);
            }
            oss << " };\n";
            oss << "#define PATCH" << i << "_RVA 0x" << std::hex << patch.rva << "\n\n";
        }
    }
    
    return oss.str();
}

// ============================================================================
// Utility
// ============================================================================

uint64_t BinaryPatcher::RvaToFileOffset(uint64_t rva)
{
    for (uint16_t i = 0; i < m_numSections; ++i) {
        IMAGE_SECTION_HEADER* section = &m_sectionHeaders[i];
        
        if (rva >= section->VirtualAddress && 
            rva < section->VirtualAddress + section->SizeOfRawData) {
            return (rva - section->VirtualAddress) + section->PointerToRawData;
        }
    }
    
    // Check if in headers
    uint32_t headerSize = m_is64Bit ? 
        m_ntHeaders64->OptionalHeader.SizeOfHeaders :
        m_ntHeaders32->OptionalHeader.SizeOfHeaders;
    
    if (rva < headerSize) {
        return rva;
    }
    
    return 0;
}

uint64_t BinaryPatcher::FileOffsetToRva(uint64_t offset)
{
    for (uint16_t i = 0; i < m_numSections; ++i) {
        IMAGE_SECTION_HEADER* section = &m_sectionHeaders[i];
        
        if (offset >= section->PointerToRawData && 
            offset < section->PointerToRawData + section->SizeOfRawData) {
            return (offset - section->PointerToRawData) + section->VirtualAddress;
        }
    }
    
    // Assume in headers
    return offset;
}

std::vector<uint8_t> BinaryPatcher::ReadBytes(uint64_t rva, size_t count)
{
    uint64_t offset = RvaToFileOffset(rva);
    return ReadBytesRaw(offset, count);
}

std::vector<uint8_t> BinaryPatcher::ReadBytesRaw(uint64_t offset, size_t count)
{
    std::vector<uint8_t> result;
    
    if (!m_fileData || offset + count > m_fileSize) {
        return result;
    }
    
    result.resize(count);
    memcpy(result.data(), m_fileData + offset, count);
    return result;
}

bool BinaryPatcher::ValidatePE()
{
    if (!m_fileData || m_fileSize < sizeof(IMAGE_DOS_HEADER)) {
        m_lastError = "File too small";
        return false;
    }
    
    if (!m_dosHeader || m_dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        m_lastError = "Invalid DOS signature";
        return false;
    }
    
    if (m_is64Bit) {
        if (!m_ntHeaders64 || m_ntHeaders64->Signature != IMAGE_NT_SIGNATURE) {
            m_lastError = "Invalid NT signature";
            return false;
        }
    } else {
        if (!m_ntHeaders32 || m_ntHeaders32->Signature != IMAGE_NT_SIGNATURE) {
            m_lastError = "Invalid NT signature";
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// Internal Helpers
// ============================================================================

bool BinaryPatcher::AllocateFileBuffer(size_t size)
{
    if (m_fileData) {
        VirtualFree(m_fileData, 0, MEM_RELEASE);
    }
    
    m_fileData = static_cast<uint8_t*>(VirtualAlloc(
        NULL,
        size,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    ));
    
    if (m_fileData) {
        m_allocatedSize = size;
        return true;
    }
    
    return false;
}

bool BinaryPatcher::ResizeFileBuffer(size_t newSize)
{
    if (newSize <= m_allocatedSize) {
        return true;
    }
    
    uint8_t* newBuffer = static_cast<uint8_t*>(VirtualAlloc(
        NULL,
        newSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    ));
    
    if (!newBuffer) {
        return false;
    }
    
    memcpy(newBuffer, m_fileData, m_fileSize);
    VirtualFree(m_fileData, 0, MEM_RELEASE);
    
    m_fileData = newBuffer;
    m_allocatedSize = newSize;
    
    return true;
}

void BinaryPatcher::RecordPatch(uint64_t rva, const std::vector<uint8_t>& original,
                                 const std::vector<uint8_t>& patched, PatchType type,
                                 const std::string& description)
{
    PatchEntry entry;
    entry.rva = rva;
    entry.fileOffset = RvaToFileOffset(rva);
    entry.type = type;
    entry.originalBytes = original;
    entry.patchedBytes = patched;
    entry.description = description;
    entry.applied = true;
    
    m_patches.push_back(entry);
}

uint32_t BinaryPatcher::AlignUp(uint32_t value, uint32_t alignment)
{
    if (alignment == 0) return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

uint32_t BinaryPatcher::GetSectionAlignment() const
{
    if (m_is64Bit && m_ntHeaders64) {
        return m_ntHeaders64->OptionalHeader.SectionAlignment;
    } else if (m_ntHeaders32) {
        return m_ntHeaders32->OptionalHeader.SectionAlignment;
    }
    return 0x1000; // Default 4KB
}

uint32_t BinaryPatcher::GetFileAlignment() const
{
    if (m_is64Bit && m_ntHeaders64) {
        return m_ntHeaders64->OptionalHeader.FileAlignment;
    } else if (m_ntHeaders32) {
        return m_ntHeaders32->OptionalHeader.FileAlignment;
    }
    return 0x200; // Default 512 bytes
}

IMAGE_SECTION_HEADER* BinaryPatcher::FindSectionByRva(uint64_t rva)
{
    for (uint16_t i = 0; i < m_numSections; ++i) {
        IMAGE_SECTION_HEADER* section = &m_sectionHeaders[i];
        
        uint32_t virtualEnd = section->VirtualAddress + 
            std::max(section->Misc.VirtualSize, section->SizeOfRawData);
        
        if (rva >= section->VirtualAddress && rva < virtualEnd) {
            return section;
        }
    }
    return nullptr;
}

IMAGE_SECTION_HEADER* BinaryPatcher::FindSectionByName(const std::string& name)
{
    for (uint16_t i = 0; i < m_numSections; ++i) {
        char sectionName[9] = { 0 };
        memcpy(sectionName, m_sectionHeaders[i].Name, 8);
        
        if (_stricmp(sectionName, name.c_str()) == 0) {
            return &m_sectionHeaders[i];
        }
    }
    return nullptr;
}

bool BinaryPatcher::UpdateSizeOfImage()
{
    if (m_numSections == 0) return false;
    
    IMAGE_SECTION_HEADER* lastSection = &m_sectionHeaders[m_numSections - 1];
    uint32_t sectionAlign = GetSectionAlignment();
    uint32_t newSizeOfImage = AlignUp(
        lastSection->VirtualAddress + lastSection->Misc.VirtualSize,
        sectionAlign
    );
    
    if (m_is64Bit) {
        m_ntHeaders64->OptionalHeader.SizeOfImage = newSizeOfImage;
    } else {
        m_ntHeaders32->OptionalHeader.SizeOfImage = newSizeOfImage;
    }
    
    return true;
}

bool BinaryPatcher::UpdateSectionHeaders()
{
    // Recalculate section offsets and sizes
    return true;
}

std::vector<uint8_t> BinaryPatcher::GenerateJmp32(int32_t offset)
{
    // E9 xx xx xx xx
    std::vector<uint8_t> code = { 0xE9, 0, 0, 0, 0 };
    memcpy(&code[1], &offset, 4);
    return code;
}

std::vector<uint8_t> BinaryPatcher::GenerateJmp64(uint64_t target)
{
    // FF 25 00 00 00 00 [8 byte address]
    std::vector<uint8_t> code = { 
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0, 0, 0, 0, 0, 0, 0, 0 
    };
    memcpy(&code[6], &target, 8);
    return code;
}

std::vector<uint8_t> BinaryPatcher::GenerateCall32(int32_t offset)
{
    // E8 xx xx xx xx
    std::vector<uint8_t> code = { 0xE8, 0, 0, 0, 0 };
    memcpy(&code[1], &offset, 4);
    return code;
}

std::vector<uint8_t> BinaryPatcher::GenerateCall64(uint64_t target)
{
    // FF 15 02 00 00 00 EB 08 [8 byte address]
    // Or simply: MOV RAX, target; CALL RAX
    std::vector<uint8_t> code = { 
        0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,  // MOV RAX, imm64
        0xFF, 0xD0                            // CALL RAX
    };
    memcpy(&code[2], &target, 8);
    return code;
}

} // namespace ReverseEngineering
} // namespace RawrXD
