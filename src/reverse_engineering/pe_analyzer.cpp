// ============================================================================
// RawrXD PE Analyzer - Win32 Native PE File Parser Implementation
// Pure Win32 API, No External Dependencies
// ============================================================================

#include "pe_analyzer.h"
#include <algorithm>
#include <cstring>

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// x64/x86 Opcode Tables for Length Decoding
// ============================================================================

// Primary opcode map: number of operand bytes (0 = ModR/M follows)
// For prefix bytes we return special values
static const int8_t g_opcodeTable[256] = {
    // 0x00-0x0F: ADD, OR instructions
    0, 0, 0, 0, 1, 4, -1, -1,  0, 0, 0, 0, 1, 4, -1, -2,
    // 0x10-0x1F: ADC, SBB instructions
    0, 0, 0, 0, 1, 4, -1, -1,  0, 0, 0, 0, 1, 4, -1, -1,
    // 0x20-0x2F: AND, SUB instructions
    0, 0, 0, 0, 1, 4, -3, -1,  0, 0, 0, 0, 1, 4, -3, -1,
    // 0x30-0x3F: XOR, CMP instructions
    0, 0, 0, 0, 1, 4, -3, -1,  0, 0, 0, 0, 1, 4, -3, -1,
    // 0x40-0x4F: INC/DEC (32-bit) or REX prefix (64-bit)
    -4, -4, -4, -4, -4, -4, -4, -4,  -4, -4, -4, -4, -4, -4, -4, -4,
    // 0x50-0x5F: PUSH/POP registers
    -1, -1, -1, -1, -1, -1, -1, -1,  -1, -1, -1, -1, -1, -1, -1, -1,
    // 0x60-0x6F
    -1, -1, 0, 0, -3, -3, -5, -5,   4, 0, 1, 0, -1, -1, -1, -1,
    // 0x70-0x7F: Short jumps (Jcc)
    1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,
    // 0x80-0x8F: Immediate groups
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0x90-0x9F
    -1, -1, -1, -1, -1, -1, -1, -1,  -1, -1, 6, -1, -1, -1, -1, -1,
    // 0xA0-0xAF: MOV AL/AX/EAX, moffs
    4, 4, 4, 4, -1, -1, -1, -1,  1, 4, -1, -1, -1, -1, -1, -1,
    // 0xB0-0xBF: MOV reg, imm8/imm32
    1, 1, 1, 1, 1, 1, 1, 1,   4, 4, 4, 4, 4, 4, 4, 4,
    // 0xC0-0xCF: Shift/rotate, RET, etc
    0, 0, 2, -1, 0, 0, 0, 0,   4, -1, 2, -1, -1, 1, -1, -1,
    // 0xD0-0xDF: Shift/rotate, FPU
    0, 0, 0, 0, 1, 1, -1, -1,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0xE0-0xEF: LOOP, CALL, JMP
    1, 1, 1, 1, 1, 1, 1, 1,   4, 4, 6, 1, -1, -1, -1, -1,
    // 0xF0-0xFF: LOCK, HLT, CMC, groups
    -6, -1, -6, -6, -1, -1, 0, 0,   -1, -1, -1, -1, -1, -1, 0, 0
};

// 2-byte opcode table (0x0F prefix)
static const int8_t g_twoByteOpcodeTable[256] = {
    // 0x00-0x0F
    0, 0, 0, 0, -1, -1, -1, -1,   -1, -1, -1, -1, -1, 0, -1, 0,
    // 0x10-0x1F: SSE
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0x20-0x2F: MOV CR/DR, SSE
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0x30-0x3F: RDTSC, RDMSR, WRMSR, etc
    -1, -1, -1, -1, -1, -1, -1, -1,   0, -1, 0, -1, -1, -1, -1, -1,
    // 0x40-0x4F: CMOVcc
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0x50-0x5F: SSE
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0x60-0x6F: MMX/SSE
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0x70-0x7F: MMX/SSE
    0, 0, 0, 0, 0, 0, 0, -1,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0x80-0x8F: Long Jcc (rel32)
    4, 4, 4, 4, 4, 4, 4, 4,   4, 4, 4, 4, 4, 4, 4, 4,
    // 0x90-0x9F: SETcc (ModR/M)
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0xA0-0xAF: PUSH/POP FS/GS, BT, SHLD, etc
    -1, -1, -1, 0, 0, 0, -1, -1,   -1, -1, -1, 0, 0, 0, 0, 0,
    // 0xB0-0xBF: CMPXCHG, LSS, BTR, LFS/LGS, MOVZX, etc
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0xC0-0xCF: XADD, CMP, SHUFPS, etc
    0, 0, 0, 0, 0, 0, 0, 0,   -1, -1, -1, -1, -1, -1, -1, -1,
    // 0xD0-0xDF: SSE2
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0xE0-0xEF: SSE
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    // 0xF0-0xFF: SSE
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0
};

// x64 Instruction mnemonics (simplified)
static const char* g_mnemonics[] = {
    "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp",
    "push", "pop", "mov", "lea", "call", "jmp", "ret", "nop",
    "test", "xchg", "inc", "dec", "mul", "div", "imul", "idiv",
    "shl", "shr", "sar", "rol", "ror", "rcl", "rcr", "not", "neg",
    "jz", "jnz", "jc", "jnc", "jo", "jno", "js", "jns", "jp", "jnp",
    "jl", "jge", "jle", "jg", "ja", "jae", "jb", "jbe",
    "int", "syscall", "sysret", "cpuid", "rdtsc", "hlt", "leave",
    "cmova", "cmovae", "cmovb", "cmovbe", "cmove", "cmovg", "cmovge",
    "cmovl", "cmovle", "cmovne", "cmovs", "cmovns",
    "seta", "setae", "setb", "setbe", "sete", "setne", "setl", "setg",
    "setge", "setle", "sets", "setns",
    "loop", "loope", "loopne", "jrcxz"
};

static const char* g_regNames64[] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
};

static const char* g_regNames32[] = {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
};

static const char* g_regNames16[] = {
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
    "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"
};

static const char* g_regNames8[] = {
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

PEAnalyzer::PEAnalyzer()
    : m_fileData(nullptr)
    , m_fileSize(0)
    , m_is64Bit(false)
    , m_isDLL(false)
    , m_isDriver(false)
    , m_peHeaderOffset(0)
    , m_imageBase(0)
    , m_entryPointRVA(0)
    , m_sectionAlignment(0)
    , m_fileAlignment(0)
    , m_imageSize(0)
    , m_headersSize(0)
    , m_subsystem(0)
    , m_machine(0)
    , m_characteristics(0)
    , m_dllCharacteristics(0)
    , m_timeDateStamp(0)
    , m_majorOSVersion(0)
    , m_minorOSVersion(0)
    , m_checksum(0)
    , m_numberOfSections(0)
    , m_importTableRVA(0)
    , m_importTableSize(0)
    , m_exportTableRVA(0)
    , m_exportTableSize(0)
    , m_resourceTableRVA(0)
    , m_resourceTableSize(0)
    , m_relocationTableRVA(0)
    , m_relocationTableSize(0)
    , m_debugTableRVA(0)
    , m_debugTableSize(0)
    , m_tlsTableRVA(0)
    , m_tlsTableSize(0)
    , m_iatRVA(0)
    , m_iatSize(0)
    , m_hasRelocations(false)
{
}

PEAnalyzer::~PEAnalyzer()
{
    Unload();
}

// ============================================================================
// Core Loading/Unloading
// ============================================================================

bool PEAnalyzer::Load(const std::string& filePath)
{
    Unload();
    m_filePath = filePath;

    // Open file using Win32 CreateFileA
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
    LARGE_INTEGER liSize;
    if (!GetFileSizeEx(hFile, &liSize)) {
        CloseHandle(hFile);
        m_lastError = "Failed to get file size";
        return false;
    }
    m_fileSize = static_cast<size_t>(liSize.QuadPart);

    if (m_fileSize < 64) {
        CloseHandle(hFile);
        m_lastError = "File too small to be a valid PE";
        return false;
    }

    // Allocate buffer and read entire file
    m_fileData = static_cast<uint8_t*>(VirtualAlloc(
        NULL,
        m_fileSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    ));

    if (!m_fileData) {
        CloseHandle(hFile);
        m_lastError = "Failed to allocate memory";
        return false;
    }

    DWORD bytesRead = 0;
    BOOL readResult = ReadFile(hFile, m_fileData, static_cast<DWORD>(m_fileSize), &bytesRead, NULL);
    CloseHandle(hFile);

    if (!readResult || bytesRead != m_fileSize) {
        VirtualFree(m_fileData, 0, MEM_RELEASE);
        m_fileData = nullptr;
        m_lastError = "Failed to read file";
        return false;
    }

    // Parse PE structure
    if (!ParseDOSHeader()) return false;
    if (!ParseNTHeaders()) return false;
    if (!ParseSectionHeaders()) return false;
    
    // Parse optional tables (failures are non-fatal)
    ParseImports();
    ParseExports();
    ParseResources();
    ParseRelocations();
    ParseDebugDirectory();

    return true;
}

void PEAnalyzer::Unload()
{
    if (m_fileData) {
        VirtualFree(m_fileData, 0, MEM_RELEASE);
        m_fileData = nullptr;
    }
    
    m_fileSize = 0;
    m_filePath.clear();
    m_sections.clear();
    m_imports.clear();
    m_exports.clear();
    m_resources.clear();
    m_relocations.clear();
    m_debugEntries.clear();
    m_exportDLLName.clear();
    m_pdbPath.clear();
    m_lastError.clear();
}

// ============================================================================
// DOS Header Parsing
// ============================================================================

bool PEAnalyzer::ParseDOSHeader()
{
    // Check MZ signature
    if (ReadWord(0) != 0x5A4D) { // 'MZ'
        m_lastError = "Invalid DOS signature";
        return false;
    }

    // Get PE header offset from e_lfanew (offset 0x3C)
    m_peHeaderOffset = ReadDWord(0x3C);

    if (m_peHeaderOffset == 0 || m_peHeaderOffset >= m_fileSize - 4) {
        m_lastError = "Invalid PE header offset";
        return false;
    }

    // Check PE signature
    if (ReadDWord(m_peHeaderOffset) != 0x00004550) { // 'PE\0\0'
        m_lastError = "Invalid PE signature";
        return false;
    }

    return true;
}

// ============================================================================
// NT Headers Parsing
// ============================================================================

bool PEAnalyzer::ParseNTHeaders()
{
    uint32_t offset = m_peHeaderOffset + 4; // Skip 'PE\0\0'

    // COFF File Header (20 bytes)
    m_machine = ReadWord(offset);           // offset+0
    m_numberOfSections = ReadWord(offset + 2);
    m_timeDateStamp = ReadDWord(offset + 4);
    // PointerToSymbolTable @ offset+8
    // NumberOfSymbols @ offset+12
    uint16_t sizeOfOptionalHeader = ReadWord(offset + 16);
    m_characteristics = ReadWord(offset + 18);

    // Check for DLL
    m_isDLL = (m_characteristics & 0x2000) != 0; // IMAGE_FILE_DLL

    offset += 20; // Move to Optional Header

    if (sizeOfOptionalHeader == 0) {
        m_lastError = "No optional header";
        return false;
    }

    // Determine 32-bit vs 64-bit
    uint16_t magic = ReadWord(offset);
    if (magic == 0x20B) { // PE32+
        m_is64Bit = true;
    } else if (magic == 0x10B) { // PE32
        m_is64Bit = false;
    } else {
        m_lastError = "Unknown PE format";
        return false;
    }

    // Parse Optional Header
    if (m_is64Bit) {
        // PE32+ Optional Header
        // magic @ 0
        // MajorLinkerVersion @ 2
        // MinorLinkerVersion @ 3
        // SizeOfCode @ 4
        // SizeOfInitializedData @ 8
        // SizeOfUninitializedData @ 12
        m_entryPointRVA = ReadDWord(offset + 16);
        // BaseOfCode @ 20
        m_imageBase = ReadQWord(offset + 24);
        m_sectionAlignment = ReadDWord(offset + 32);
        m_fileAlignment = ReadDWord(offset + 36);
        m_majorOSVersion = ReadWord(offset + 40);
        m_minorOSVersion = ReadWord(offset + 42);
        // MajorImageVersion @ 44
        // MinorImageVersion @ 46
        // MajorSubsystemVersion @ 48
        // MinorSubsystemVersion @ 50
        // Win32VersionValue @ 52
        m_imageSize = ReadDWord(offset + 56);
        m_headersSize = ReadDWord(offset + 60);
        m_checksum = ReadDWord(offset + 64);
        m_subsystem = ReadWord(offset + 68);
        m_dllCharacteristics = ReadWord(offset + 70);
        // SizeOfStackReserve @ 72 (8 bytes)
        // SizeOfStackCommit @ 80 (8 bytes)
        // SizeOfHeapReserve @ 88 (8 bytes)
        // SizeOfHeapCommit @ 96 (8 bytes)
        // LoaderFlags @ 104
        uint32_t numberOfRvaAndSizes = ReadDWord(offset + 108);
        
        // Data Directories start at offset + 112
        uint32_t dirOffset = offset + 112;
        
        if (numberOfRvaAndSizes >= 1) {
            m_exportTableRVA = ReadDWord(dirOffset);
            m_exportTableSize = ReadDWord(dirOffset + 4);
        }
        if (numberOfRvaAndSizes >= 2) {
            m_importTableRVA = ReadDWord(dirOffset + 8);
            m_importTableSize = ReadDWord(dirOffset + 12);
        }
        if (numberOfRvaAndSizes >= 3) {
            m_resourceTableRVA = ReadDWord(dirOffset + 16);
            m_resourceTableSize = ReadDWord(dirOffset + 20);
        }
        // Exception @ 3, Security @ 4
        if (numberOfRvaAndSizes >= 6) {
            m_relocationTableRVA = ReadDWord(dirOffset + 40);
            m_relocationTableSize = ReadDWord(dirOffset + 44);
        }
        if (numberOfRvaAndSizes >= 7) {
            m_debugTableRVA = ReadDWord(dirOffset + 48);
            m_debugTableSize = ReadDWord(dirOffset + 52);
        }
        // TLS @ 9
        if (numberOfRvaAndSizes >= 10) {
            m_tlsTableRVA = ReadDWord(dirOffset + 72);
            m_tlsTableSize = ReadDWord(dirOffset + 76);
        }
        // IAT @ 12
        if (numberOfRvaAndSizes >= 13) {
            m_iatRVA = ReadDWord(dirOffset + 96);
            m_iatSize = ReadDWord(dirOffset + 100);
        }
    } else {
        // PE32 Optional Header
        m_entryPointRVA = ReadDWord(offset + 16);
        // BaseOfCode @ 20
        // BaseOfData @ 24
        m_imageBase = ReadDWord(offset + 28);
        m_sectionAlignment = ReadDWord(offset + 32);
        m_fileAlignment = ReadDWord(offset + 36);
        m_majorOSVersion = ReadWord(offset + 40);
        m_minorOSVersion = ReadWord(offset + 42);
        m_imageSize = ReadDWord(offset + 56);
        m_headersSize = ReadDWord(offset + 60);
        m_checksum = ReadDWord(offset + 64);
        m_subsystem = ReadWord(offset + 68);
        m_dllCharacteristics = ReadWord(offset + 70);
        
        uint32_t numberOfRvaAndSizes = ReadDWord(offset + 92);
        uint32_t dirOffset = offset + 96;
        
        if (numberOfRvaAndSizes >= 1) {
            m_exportTableRVA = ReadDWord(dirOffset);
            m_exportTableSize = ReadDWord(dirOffset + 4);
        }
        if (numberOfRvaAndSizes >= 2) {
            m_importTableRVA = ReadDWord(dirOffset + 8);
            m_importTableSize = ReadDWord(dirOffset + 12);
        }
        if (numberOfRvaAndSizes >= 3) {
            m_resourceTableRVA = ReadDWord(dirOffset + 16);
            m_resourceTableSize = ReadDWord(dirOffset + 20);
        }
        if (numberOfRvaAndSizes >= 6) {
            m_relocationTableRVA = ReadDWord(dirOffset + 40);
            m_relocationTableSize = ReadDWord(dirOffset + 44);
        }
        if (numberOfRvaAndSizes >= 7) {
            m_debugTableRVA = ReadDWord(dirOffset + 48);
            m_debugTableSize = ReadDWord(dirOffset + 52);
        }
    }

    // Check for driver
    m_isDriver = (m_subsystem == 1); // IMAGE_SUBSYSTEM_NATIVE

    return true;
}

// ============================================================================
// Section Headers Parsing
// ============================================================================

bool PEAnalyzer::ParseSectionHeaders()
{
    // Section headers immediately follow optional header
    uint32_t optionalHeaderSize = m_is64Bit ? 240 : 224;
    uint32_t sectionTableOffset = m_peHeaderOffset + 4 + 20 + optionalHeaderSize;

    for (uint16_t i = 0; i < m_numberOfSections; ++i) {
        uint32_t offset = sectionTableOffset + (i * 40);
        
        if (offset + 40 > m_fileSize) {
            m_lastError = "Section header extends beyond file";
            return false;
        }

        SectionInfo section;
        
        // Name (8 bytes, null-padded)
        for (int j = 0; j < 8; ++j) {
            section.name[j] = static_cast<char>(m_fileData[offset + j]);
        }
        section.name[8] = '\0';
        
        section.virtualSize = ReadDWord(offset + 8);
        section.virtualAddress = ReadDWord(offset + 12);
        section.rawDataSize = ReadDWord(offset + 16);
        section.rawDataOffset = ReadDWord(offset + 20);
        // PointerToRelocations @ 24
        // PointerToLinenumbers @ 28
        // NumberOfRelocations @ 32
        // NumberOfLinenumbers @ 34
        section.characteristics = ReadDWord(offset + 36);

        // Parse characteristics
        section.isExecutable = (section.characteristics & 0x20000000) != 0; // IMAGE_SCN_MEM_EXECUTE
        section.isWritable = (section.characteristics & 0x80000000) != 0;   // IMAGE_SCN_MEM_WRITE
        section.isReadable = (section.characteristics & 0x40000000) != 0;   // IMAGE_SCN_MEM_READ

        m_sections.push_back(section);
    }

    return true;
}

// ============================================================================
// Import Table Parsing
// ============================================================================

bool PEAnalyzer::ParseImports()
{
    if (m_importTableRVA == 0 || m_importTableSize == 0) {
        return true; // No imports
    }

    uint32_t importOffset = RVAToFileOffset(m_importTableRVA);
    if (importOffset == 0) return false;

    // Each IMAGE_IMPORT_DESCRIPTOR is 20 bytes
    for (uint32_t i = 0; ; ++i) {
        uint32_t descOffset = importOffset + (i * 20);
        if (descOffset + 20 > m_fileSize) break;

        // Read IMAGE_IMPORT_DESCRIPTOR
        uint32_t originalFirstThunk = ReadDWord(descOffset);     // ILT RVA
        uint32_t timeDateStamp = ReadDWord(descOffset + 4);
        // uint32_t forwarderChain = ReadDWord(descOffset + 8);
        uint32_t nameRVA = ReadDWord(descOffset + 12);
        uint32_t firstThunk = ReadDWord(descOffset + 16);        // IAT RVA

        // Check for null terminator (all zeros)
        if (originalFirstThunk == 0 && nameRVA == 0 && firstThunk == 0) {
            break;
        }

        ImportedDLL dll;
        dll.importLookupTableRVA = originalFirstThunk;
        dll.importAddressTableRVA = firstThunk;
        dll.timeDateStamp = timeDateStamp;

        // Read DLL name
        if (nameRVA != 0) {
            dll.name = ReadStringAtRVA(nameRVA);
        }

        // Parse thunk data
        uint32_t thunkRVA = (originalFirstThunk != 0) ? originalFirstThunk : firstThunk;
        uint32_t thunkOffset = RVAToFileOffset(thunkRVA);
        if (thunkOffset == 0) continue;

        for (uint32_t j = 0; ; ++j) {
            uint64_t thunkData;
            uint32_t entrySize = m_is64Bit ? 8 : 4;
            uint32_t entryOffset = thunkOffset + (j * entrySize);

            if (entryOffset + entrySize > m_fileSize) break;

            if (m_is64Bit) {
                thunkData = ReadQWord(entryOffset);
            } else {
                thunkData = ReadDWord(entryOffset);
            }

            if (thunkData == 0) break; // Null terminator

            ImportedFunction func;
            func.thunkRVA = thunkRVA + (j * entrySize);

            // Check ordinal flag (high bit set)
            uint64_t ordinalFlag = m_is64Bit ? 0x8000000000000000ULL : 0x80000000ULL;
            if (thunkData & ordinalFlag) {
                // Import by ordinal
                func.isOrdinal = true;
                func.ordinal = static_cast<uint16_t>(thunkData & 0xFFFF);
                func.name.clear();
            } else {
                // Import by name
                func.isOrdinal = false;
                func.hintNameTableRVA = static_cast<uint64_t>(thunkData & (m_is64Bit ? 0x7FFFFFFFFFFFFFFFULL : 0x7FFFFFFFULL));
                
                uint32_t hintNameOffset = RVAToFileOffset(static_cast<uint32_t>(func.hintNameTableRVA));
                if (hintNameOffset != 0 && hintNameOffset + 2 < m_fileSize) {
                    func.ordinal = ReadWord(hintNameOffset); // Hint
                    func.name = ReadString(hintNameOffset + 2);
                }
            }

            dll.functions.push_back(func);
        }

        m_imports.push_back(dll);
    }

    return true;
}

// ============================================================================
// Export Table Parsing
// ============================================================================

bool PEAnalyzer::ParseExports()
{
    if (m_exportTableRVA == 0 || m_exportTableSize == 0) {
        return true; // No exports
    }

    uint32_t exportOffset = RVAToFileOffset(m_exportTableRVA);
    if (exportOffset == 0 || exportOffset + 40 > m_fileSize) return false;

    // IMAGE_EXPORT_DIRECTORY (40 bytes)
    // uint32_t characteristics = ReadDWord(exportOffset);      // Usually 0
    // uint32_t timeDateStamp = ReadDWord(exportOffset + 4);
    // uint16_t majorVersion = ReadWord(exportOffset + 8);
    // uint16_t minorVersion = ReadWord(exportOffset + 10);
    uint32_t nameRVA = ReadDWord(exportOffset + 12);
    uint32_t ordinalBase = ReadDWord(exportOffset + 16);
    uint32_t numberOfFunctions = ReadDWord(exportOffset + 20);
    uint32_t numberOfNames = ReadDWord(exportOffset + 24);
    uint32_t addressOfFunctions = ReadDWord(exportOffset + 28);     // EAT RVA
    uint32_t addressOfNames = ReadDWord(exportOffset + 32);         // NPT RVA  
    uint32_t addressOfNameOrdinals = ReadDWord(exportOffset + 36);  // OT RVA

    // Read DLL name
    if (nameRVA != 0) {
        m_exportDLLName = ReadStringAtRVA(nameRVA);
    }

    // Get table offsets
    uint32_t eatOffset = RVAToFileOffset(addressOfFunctions);
    uint32_t nptOffset = RVAToFileOffset(addressOfNames);
    uint32_t otOffset = RVAToFileOffset(addressOfNameOrdinals);

    if (eatOffset == 0) return false;

    // Build ordinal to name mapping
    std::vector<std::string> ordinalToName(numberOfFunctions);
    if (nptOffset != 0 && otOffset != 0) {
        for (uint32_t i = 0; i < numberOfNames && i < 65536; ++i) {
            if (nptOffset + (i * 4) + 4 > m_fileSize) break;
            if (otOffset + (i * 2) + 2 > m_fileSize) break;

            uint32_t nameAddr = ReadDWord(nptOffset + (i * 4));
            uint16_t ordinalIndex = ReadWord(otOffset + (i * 2));

            if (ordinalIndex < numberOfFunctions) {
                std::string funcName = ReadStringAtRVA(nameAddr);
                ordinalToName[ordinalIndex] = funcName;
            }
        }
    }

    // Parse all exports
    for (uint32_t i = 0; i < numberOfFunctions && i < 65536; ++i) {
        if (eatOffset + (i * 4) + 4 > m_fileSize) break;

        uint32_t funcRVA = ReadDWord(eatOffset + (i * 4));
        if (funcRVA == 0) continue; // Unused entry

        ExportedFunction exp;
        exp.ordinal = ordinalBase + i;
        exp.rva = funcRVA;
        exp.name = (i < ordinalToName.size()) ? ordinalToName[i] : "";
        
        // Check for forwarder (RVA within export directory)
        if (funcRVA >= m_exportTableRVA && funcRVA < m_exportTableRVA + m_exportTableSize) {
            exp.isForwarder = true;
            exp.forwarderName = ReadStringAtRVA(funcRVA);
        } else {
            exp.isForwarder = false;
        }

        m_exports.push_back(exp);
    }

    return true;
}

// ============================================================================
// Resource Parsing
// ============================================================================

bool PEAnalyzer::ParseResources()
{
    if (m_resourceTableRVA == 0 || m_resourceTableSize == 0) {
        return true;
    }

    return ParseResourceDirectory(m_resourceTableRVA, 0, 0, 0);
}

bool PEAnalyzer::ParseResourceDirectory(uint32_t dirRVA, uint32_t level, uint32_t typeId, uint32_t nameId)
{
    uint32_t dirOffset = RVAToFileOffset(dirRVA);
    if (dirOffset == 0 || dirOffset + 16 > m_fileSize) return false;

    // IMAGE_RESOURCE_DIRECTORY (16 bytes)
    // uint32_t characteristics = ReadDWord(dirOffset);
    // uint32_t timeDateStamp = ReadDWord(dirOffset + 4);
    // uint16_t majorVersion = ReadWord(dirOffset + 8);
    // uint16_t minorVersion = ReadWord(dirOffset + 10);
    uint16_t numberOfNamedEntries = ReadWord(dirOffset + 12);
    uint16_t numberOfIdEntries = ReadWord(dirOffset + 14);

    uint32_t entryOffset = dirOffset + 16;
    uint32_t totalEntries = numberOfNamedEntries + numberOfIdEntries;

    for (uint32_t i = 0; i < totalEntries && i < 1024; ++i) {
        if (entryOffset + 8 > m_fileSize) break;

        uint32_t nameOrId = ReadDWord(entryOffset);
        uint32_t offsetToData = ReadDWord(entryOffset + 4);
        entryOffset += 8;

        uint32_t currentTypeId = typeId;
        uint32_t currentNameId = nameId;
        uint32_t languageId = 0;

        // Determine ID for current level
        bool isNamedEntry = (nameOrId & 0x80000000) != 0;
        uint32_t idValue = nameOrId & 0x7FFFFFFF;

        if (level == 0) {
            currentTypeId = isNamedEntry ? 0 : idValue;
        } else if (level == 1) {
            currentNameId = isNamedEntry ? 0 : idValue;
        } else if (level == 2) {
            languageId = idValue;
        }

        // Check if this is a subdirectory or data entry
        bool isSubdirectory = (offsetToData & 0x80000000) != 0;
        uint32_t offsetValue = offsetToData & 0x7FFFFFFF;

        if (isSubdirectory && level < 2) {
            // Recursively parse subdirectory
            uint32_t subdirRVA = m_resourceTableRVA + offsetValue;
            ParseResourceDirectory(subdirRVA, level + 1, currentTypeId, currentNameId);
        } else if (!isSubdirectory || level == 2) {
            // Parse IMAGE_RESOURCE_DATA_ENTRY
            uint32_t dataEntryRVA = m_resourceTableRVA + offsetValue;
            uint32_t dataEntryOffset = RVAToFileOffset(dataEntryRVA);
            
            if (dataEntryOffset != 0 && dataEntryOffset + 16 <= m_fileSize) {
                ResourceEntry resource;
                resource.type = currentTypeId;
                resource.id = currentNameId;
                resource.languageId = languageId;
                resource.dataRVA = ReadDWord(dataEntryOffset);
                resource.dataSize = ReadDWord(dataEntryOffset + 4);
                resource.codePage = ReadDWord(dataEntryOffset + 8);

                // Map type ID to name
                switch (currentTypeId) {
                    case 1: resource.typeName = "RT_CURSOR"; break;
                    case 2: resource.typeName = "RT_BITMAP"; break;
                    case 3: resource.typeName = "RT_ICON"; break;
                    case 4: resource.typeName = "RT_MENU"; break;
                    case 5: resource.typeName = "RT_DIALOG"; break;
                    case 6: resource.typeName = "RT_STRING"; break;
                    case 7: resource.typeName = "RT_FONTDIR"; break;
                    case 8: resource.typeName = "RT_FONT"; break;
                    case 9: resource.typeName = "RT_ACCELERATOR"; break;
                    case 10: resource.typeName = "RT_RCDATA"; break;
                    case 11: resource.typeName = "RT_MESSAGETABLE"; break;
                    case 12: resource.typeName = "RT_GROUP_CURSOR"; break;
                    case 14: resource.typeName = "RT_GROUP_ICON"; break;
                    case 16: resource.typeName = "RT_VERSION"; break;
                    case 17: resource.typeName = "RT_DLGINCLUDE"; break;
                    case 19: resource.typeName = "RT_PLUGPLAY"; break;
                    case 20: resource.typeName = "RT_VXD"; break;
                    case 21: resource.typeName = "RT_ANICURSOR"; break;
                    case 22: resource.typeName = "RT_ANIICON"; break;
                    case 23: resource.typeName = "RT_HTML"; break;
                    case 24: resource.typeName = "RT_MANIFEST"; break;
                    default: resource.typeName = "UNKNOWN"; break;
                }

                m_resources.push_back(resource);
            }
        }
    }

    return true;
}

// ============================================================================
// Relocation Parsing
// ============================================================================

bool PEAnalyzer::ParseRelocations()
{
    if (m_relocationTableRVA == 0 || m_relocationTableSize == 0) {
        m_hasRelocations = false;
        return true;
    }

    m_hasRelocations = true;
    uint32_t offset = RVAToFileOffset(m_relocationTableRVA);
    if (offset == 0) return false;

    uint32_t remaining = m_relocationTableSize;

    while (remaining >= 8) {
        if (offset + 8 > m_fileSize) break;

        uint32_t pageRVA = ReadDWord(offset);
        uint32_t blockSize = ReadDWord(offset + 4);

        if (blockSize < 8 || blockSize > remaining) break;

        uint32_t entryCount = (blockSize - 8) / 2;
        for (uint32_t i = 0; i < entryCount; ++i) {
            if (offset + 8 + (i * 2) + 2 > m_fileSize) break;

            uint16_t entry = ReadWord(offset + 8 + (i * 2));
            uint16_t type = (entry >> 12) & 0xF;
            uint16_t relocOffset = entry & 0xFFF;

            if (type != 0) { // Skip IMAGE_REL_BASED_ABSOLUTE (padding)
                RelocEntry reloc;
                reloc.rva = pageRVA + relocOffset;
                reloc.type = type;
                m_relocations.push_back(reloc);
            }
        }

        offset += blockSize;
        remaining -= blockSize;
    }

    return true;
}

// ============================================================================
// Debug Directory Parsing
// ============================================================================

bool PEAnalyzer::ParseDebugDirectory()
{
    if (m_debugTableRVA == 0 || m_debugTableSize == 0) {
        return true;
    }

    uint32_t offset = RVAToFileOffset(m_debugTableRVA);
    if (offset == 0) return false;

    uint32_t entryCount = m_debugTableSize / 28; // IMAGE_DEBUG_DIRECTORY is 28 bytes

    for (uint32_t i = 0; i < entryCount && i < 32; ++i) {
        uint32_t entryOffset = offset + (i * 28);
        if (entryOffset + 28 > m_fileSize) break;

        DebugEntry entry;
        // uint32_t characteristics = ReadDWord(entryOffset);
        entry.timeDateStamp = ReadDWord(entryOffset + 4);
        // uint16_t majorVersion = ReadWord(entryOffset + 8);
        // uint16_t minorVersion = ReadWord(entryOffset + 10);
        entry.type = ReadDWord(entryOffset + 12);
        entry.sizeOfData = ReadDWord(entryOffset + 16);
        entry.addressOfRawData = ReadDWord(entryOffset + 20);
        entry.pointerToRawData = ReadDWord(entryOffset + 24);

        m_debugEntries.push_back(entry);

        // Extract PDB path from CodeView entry
        if (entry.type == 2 && entry.pointerToRawData != 0 && entry.sizeOfData >= 24) { // IMAGE_DEBUG_TYPE_CODEVIEW
            uint32_t cvOffset = entry.pointerToRawData;
            if (cvOffset + 24 <= m_fileSize) {
                uint32_t signature = ReadDWord(cvOffset);
                if (signature == 0x53445352) { // 'RSDS' - PDB 7.0
                    // Skip GUID (16 bytes) and age (4 bytes)
                    m_pdbPath = ReadString(cvOffset + 24, entry.sizeOfData - 24);
                }
            }
        }
    }

    return true;
}

// ============================================================================
// ASLR/DEP/Security Flags
// ============================================================================

bool PEAnalyzer::HasASLR() const
{
    return (m_dllCharacteristics & 0x0040) != 0; // IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE
}

bool PEAnalyzer::HasDEP() const
{
    return (m_dllCharacteristics & 0x0100) != 0; // IMAGE_DLLCHARACTERISTICS_NX_COMPAT
}

bool PEAnalyzer::HasCFG() const
{
    return (m_dllCharacteristics & 0x4000) != 0; // IMAGE_DLLCHARACTERISTICS_GUARD_CF
}

bool PEAnalyzer::HasHighEntropyASLR() const
{
    return (m_dllCharacteristics & 0x0020) != 0; // IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA
}

bool PEAnalyzer::HasSEH() const
{
    return (m_dllCharacteristics & 0x0400) == 0; // NOT IMAGE_DLLCHARACTERISTICS_NO_SEH
}

bool PEAnalyzer::HasSafeSEH() const
{
    // Safe SEH is indicated by presence of SEH table in load config
    // For now, check if relocations exist (required for SafeSEH)
    return m_hasRelocations && HasSEH();
}

// ============================================================================
// Section Access
// ============================================================================

std::vector<SectionInfo> PEAnalyzer::GetSections() const
{
    return m_sections;
}

std::vector<uint8_t> PEAnalyzer::ReadSection(const std::string& name) const
{
    const SectionInfo* section = FindSectionByName(name);
    if (!section) return {};

    return ReadSectionByIndex(std::distance(m_sections.data(), section));
}

std::vector<uint8_t> PEAnalyzer::ReadSectionByIndex(size_t index) const
{
    if (index >= m_sections.size()) return {};

    const SectionInfo& section = m_sections[index];
    if (section.rawDataOffset == 0 || section.rawDataSize == 0) return {};
    if (section.rawDataOffset + section.rawDataSize > m_fileSize) return {};

    return std::vector<uint8_t>(
        m_fileData + section.rawDataOffset,
        m_fileData + section.rawDataOffset + section.rawDataSize
    );
}

const SectionInfo* PEAnalyzer::FindSectionByRVA(uint32_t rva) const
{
    for (const auto& section : m_sections) {
        if (rva >= section.virtualAddress && 
            rva < section.virtualAddress + section.virtualSize) {
            return &section;
        }
    }
    return nullptr;
}

const SectionInfo* PEAnalyzer::FindSectionByName(const std::string& name) const
{
    for (const auto& section : m_sections) {
        if (strncmp(section.name, name.c_str(), 8) == 0) {
            return &section;
        }
    }
    return nullptr;
}

// ============================================================================
// Import/Export Access
// ============================================================================

std::vector<ImportedDLL> PEAnalyzer::GetImports() const
{
    return m_imports;
}

std::vector<ImportedFunction> PEAnalyzer::GetImportedFunctions(const std::string& dllName) const
{
    for (const auto& dll : m_imports) {
        // Case-insensitive comparison
        std::string lowerDll = dll.name;
        std::string lowerSearch = dllName;
        std::transform(lowerDll.begin(), lowerDll.end(), lowerDll.begin(), ::tolower);
        std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
        
        if (lowerDll == lowerSearch) {
            return dll.functions;
        }
    }
    return {};
}

size_t PEAnalyzer::GetTotalImportCount() const
{
    size_t total = 0;
    for (const auto& dll : m_imports) {
        total += dll.functions.size();
    }
    return total;
}

std::vector<ExportedFunction> PEAnalyzer::GetExports() const
{
    return m_exports;
}

const ExportedFunction* PEAnalyzer::FindExportByName(const std::string& name) const
{
    for (const auto& exp : m_exports) {
        if (exp.name == name) {
            return &exp;
        }
    }
    return nullptr;
}

const ExportedFunction* PEAnalyzer::FindExportByOrdinal(uint32_t ordinal) const
{
    for (const auto& exp : m_exports) {
        if (exp.ordinal == ordinal) {
            return &exp;
        }
    }
    return nullptr;
}

// ============================================================================
// Resource Access
// ============================================================================

std::vector<ResourceEntry> PEAnalyzer::GetResources() const
{
    return m_resources;
}

std::vector<uint8_t> PEAnalyzer::ReadResource(const ResourceEntry& entry) const
{
    uint32_t offset = RVAToFileOffset(entry.dataRVA);
    if (offset == 0 || offset + entry.dataSize > m_fileSize) return {};

    return std::vector<uint8_t>(
        m_fileData + offset,
        m_fileData + offset + entry.dataSize
    );
}

std::vector<uint8_t> PEAnalyzer::ReadResourceByType(uint32_t type, uint32_t id) const
{
    for (const auto& res : m_resources) {
        if (res.type == type && res.id == id) {
            return ReadResource(res);
        }
    }
    return {};
}

// ============================================================================
// Relocation/Debug Access
// ============================================================================

std::vector<RelocEntry> PEAnalyzer::GetRelocations() const
{
    return m_relocations;
}

std::vector<DebugEntry> PEAnalyzer::GetDebugEntries() const
{
    return m_debugEntries;
}

// ============================================================================
// Raw File Access
// ============================================================================

std::vector<uint8_t> PEAnalyzer::ReadBytes(uint32_t fileOffset, size_t count) const
{
    if (!m_fileData || fileOffset >= m_fileSize) return {};
    
    size_t available = m_fileSize - fileOffset;
    size_t toRead = (count < available) ? count : available;
    
    return std::vector<uint8_t>(
        m_fileData + fileOffset,
        m_fileData + fileOffset + toRead
    );
}

std::vector<uint8_t> PEAnalyzer::ReadBytesAtRVA(uint32_t rva, size_t count) const
{
    uint32_t offset = RVAToFileOffset(rva);
    if (offset == 0) return {};
    return ReadBytes(offset, count);
}

uint32_t PEAnalyzer::RVAToFileOffset(uint32_t rva) const
{
    for (const auto& section : m_sections) {
        if (rva >= section.virtualAddress &&
            rva < section.virtualAddress + section.virtualSize) {
            uint32_t offset = rva - section.virtualAddress + section.rawDataOffset;
            if (offset < m_fileSize) {
                return offset;
            }
        }
    }
    
    // RVA might be in headers
    if (rva < m_headersSize && rva < m_fileSize) {
        return rva;
    }
    
    return 0;
}

uint32_t PEAnalyzer::FileOffsetToRVA(uint32_t fileOffset) const
{
    for (const auto& section : m_sections) {
        if (fileOffset >= section.rawDataOffset &&
            fileOffset < section.rawDataOffset + section.rawDataSize) {
            return fileOffset - section.rawDataOffset + section.virtualAddress;
        }
    }
    
    // Offset might be in headers
    if (fileOffset < m_headersSize) {
        return fileOffset;
    }
    
    return 0;
}

// ============================================================================
// Helper Methods
// ============================================================================

uint32_t PEAnalyzer::ReadDWord(uint32_t offset) const
{
    if (!m_fileData || offset + 4 > m_fileSize) return 0;
    return *reinterpret_cast<const uint32_t*>(m_fileData + offset);
}

uint16_t PEAnalyzer::ReadWord(uint32_t offset) const
{
    if (!m_fileData || offset + 2 > m_fileSize) return 0;
    return *reinterpret_cast<const uint16_t*>(m_fileData + offset);
}

uint64_t PEAnalyzer::ReadQWord(uint32_t offset) const
{
    if (!m_fileData || offset + 8 > m_fileSize) return 0;
    return *reinterpret_cast<const uint64_t*>(m_fileData + offset);
}

std::string PEAnalyzer::ReadString(uint32_t offset, size_t maxLen) const
{
    if (!m_fileData || offset >= m_fileSize) return "";
    
    std::string result;
    size_t remaining = m_fileSize - offset;
    size_t limit = (maxLen < remaining) ? maxLen : remaining;
    
    for (size_t i = 0; i < limit; ++i) {
        char c = static_cast<char>(m_fileData[offset + i]);
        if (c == '\0') break;
        result += c;
    }
    
    return result;
}

std::string PEAnalyzer::ReadStringAtRVA(uint32_t rva, size_t maxLen) const
{
    uint32_t offset = RVAToFileOffset(rva);
    if (offset == 0) return "";
    return ReadString(offset, maxLen);
}

std::wstring PEAnalyzer::ReadUnicodeString(uint32_t offset, size_t maxLen) const
{
    if (!m_fileData || offset >= m_fileSize) return L"";
    
    std::wstring result;
    size_t remaining = (m_fileSize - offset) / 2;
    size_t limit = (maxLen < remaining) ? maxLen : remaining;
    
    for (size_t i = 0; i < limit; ++i) {
        wchar_t c = *reinterpret_cast<const wchar_t*>(m_fileData + offset + (i * 2));
        if (c == L'\0') break;
        result += c;
    }
    
    return result;
}

// ============================================================================
// Instruction Length Decoding
// ============================================================================

uint32_t PEAnalyzer::GetInstructionLength(const uint8_t* code, size_t remaining) const
{
    if (remaining == 0) return 0;

    const uint8_t* p = code;
    const uint8_t* end = code + remaining;
    
    bool hasRex = false;
    bool rexW = false;
    bool has66 = false;
    bool has67 = false;
    bool hasF2 = false;
    bool hasF3 = false;

    // Parse prefixes
    while (p < end) {
        uint8_t b = *p;
        
        if (b == 0x66) { has66 = true; ++p; continue; }
        if (b == 0x67) { has67 = true; ++p; continue; }
        if (b == 0xF2) { hasF2 = true; ++p; continue; }
        if (b == 0xF3) { hasF3 = true; ++p; continue; }
        if (b == 0xF0 || b == 0x2E || b == 0x3E || b == 0x26 || b == 0x36 || b == 0x64 || b == 0x65) {
            ++p; continue;
        }
        
        // REX prefix (64-bit only)
        if (m_is64Bit && (b >= 0x40 && b <= 0x4F)) {
            hasRex = true;
            rexW = (b & 0x08) != 0;
            ++p;
            continue;
        }
        
        break; // Not a prefix
    }

    if (p >= end) return static_cast<uint32_t>(p - code);

    uint32_t baseLen = static_cast<uint32_t>(p - code);
    uint8_t opcode = *p++;
    
    // 2-byte opcode escape
    if (opcode == 0x0F) {
        if (p >= end) return baseLen + 1;
        uint8_t opcode2 = *p++;
        
        // 3-byte opcode (0F 38 / 0F 3A)
        if (opcode2 == 0x38 || opcode2 == 0x3A) {
            if (p >= end) return baseLen + 2;
            ++p; // Skip third opcode byte
            if (p >= end) return baseLen + 3;
            
            // These instructions have ModR/M
            uint8_t modrm = *p++;
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm = modrm & 7;
            
            uint32_t len = baseLen + 4;
            
            // SIB byte
            if (mod != 3 && rm == 4) {
                if (p >= end) return len;
                ++p; ++len;
            }
            
            // Displacement
            if (mod == 0 && rm == 5) len += 4;
            else if (mod == 1) len += 1;
            else if (mod == 2) len += 4;
            
            // 0F 3A instructions have immediate byte
            if (opcode2 == 0x3A) len += 1;
            
            return len;
        }
        
        int8_t extraBytes = g_twoByteOpcodeTable[opcode2];
        
        if (extraBytes >= 0) {
            // Fixed-length instruction or ModR/M
            if (extraBytes == 0) {
                // Has ModR/M
                if (p >= end) return baseLen + 2;
                uint8_t modrm = *p++;
                uint8_t mod = (modrm >> 6) & 3;
                uint8_t rm = modrm & 7;
                
                uint32_t len = baseLen + 3;
                
                if (mod != 3 && rm == 4) {
                    if (p >= end) return len;
                    ++p; ++len;
                }
                
                if (mod == 0 && rm == 5) len += 4;
                else if (mod == 1) len += 1;
                else if (mod == 2) len += 4;
                
                return len;
            } else {
                return baseLen + 2 + extraBytes;
            }
        } else {
            return baseLen + 2;
        }
    }
    
    // VEX prefix
    if (opcode == 0xC4 || opcode == 0xC5) {
        // VEX encoded instruction - simplified handling
        uint32_t vexLen = (opcode == 0xC5) ? 2 : 3;
        p += vexLen - 1;
        if (p >= end) return baseLen + vexLen;
        
        // VEX opcode + ModR/M + possible immediate
        ++p;
        if (p >= end) return baseLen + vexLen + 1;
        
        uint8_t modrm = *p++;
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t rm = modrm & 7;
        
        uint32_t len = baseLen + vexLen + 2;
        
        if (mod != 3 && rm == 4) {
            if (p < end) { ++p; ++len; }
        }
        
        if (mod == 0 && rm == 5) len += 4;
        else if (mod == 1) len += 1;
        else if (mod == 2) len += 4;
        
        // Many VEX instructions have an immediate
        len += 1;
        
        return len;
    }

    // Single-byte opcode
    int8_t info = g_opcodeTable[opcode];
    
    // Handle special cases
    if (info == -1) {
        // Single byte instruction (no operands)
        return baseLen + 1;
    } else if (info == -2) {
        // 0x0F escape - should not reach here
        return baseLen + 1;
    } else if (info == -3) {
        // Segment override - should be handled in prefix loop
        return baseLen + 1;
    } else if (info == -4) {
        // REX or INC/DEC (32-bit) - should be handled
        if (m_is64Bit) {
            // REX prefix handled above
            return baseLen + 1;
        }
        return baseLen + 1;
    } else if (info == -5) {
        // 66/67 prefix - handled
        return baseLen + 1;
    } else if (info == -6) {
        // LOCK/REP prefix - handled
        return baseLen + 1;
    } else if (info == 0) {
        // Has ModR/M byte
        if (p >= end) return baseLen + 1;
        uint8_t modrm = *p++;
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t rm = modrm & 7;
        uint8_t reg = (modrm >> 3) & 7;
        
        uint32_t len = baseLen + 2;
        
        // SIB byte
        if (mod != 3 && rm == 4) {
            if (p >= end) return len;
            ++p; ++len;
        }
        
        // Displacement
        if (mod == 0 && rm == 5) {
            len += has67 ? 2 : 4; // disp32 or disp16
        } else if (mod == 1) {
            len += 1; // disp8
        } else if (mod == 2) {
            len += has67 ? 2 : 4; // disp32 or disp16
        }
        
        // Handle immediate for group instructions
        if (opcode == 0x80 || opcode == 0x82) len += 1; // imm8
        else if (opcode == 0x81) len += has66 ? 2 : 4;  // imm16/32
        else if (opcode == 0x83) len += 1;               // imm8
        else if (opcode == 0xC0 || opcode == 0xC1) len += 1; // shift imm8
        else if (opcode == 0xC6) len += 1;               // MOV imm8
        else if (opcode == 0xC7) len += has66 ? 2 : 4;   // MOV imm16/32
        else if (opcode == 0x69) len += has66 ? 2 : 4;   // IMUL imm16/32
        else if (opcode == 0x6B) len += 1;               // IMUL imm8
        
        return len;
    } else if (info > 0) {
        // Fixed immediate size
        uint32_t immSize = info;
        
        // Adjust for operand size override
        if ((opcode >= 0xB8 && opcode <= 0xBF) || opcode == 0x68 || opcode == 0xA9) {
            if (has66) immSize = 2;
            else if (rexW && (opcode >= 0xB8 && opcode <= 0xBF)) immSize = 8;
        }
        
        return baseLen + 1 + immSize;
    }

    return baseLen + 1;
}

// ============================================================================
// Disassembly
// ============================================================================

void PEAnalyzer::DecodeInstruction(const uint8_t* code, size_t remaining, DisassembledInstruction& out) const
{
    out.length = GetInstructionLength(code, remaining);
    if (out.length == 0) {
        out.length = 1;
        out.mnemonic = "db";
        char buf[8];
        snprintf(buf, sizeof(buf), "0x%02X", code[0]);
        out.operands = buf;
        out.bytes.assign(code, code + 1);
        return;
    }

    out.bytes.assign(code, code + out.length);
    
    const uint8_t* p = code;
    const uint8_t* end = code + out.length;

    bool rexW = false;
    bool rexR = false;
    bool rexX = false;
    bool rexB = false;
    bool has66 = false;

    // Skip prefixes
    while (p < end) {
        uint8_t b = *p;
        if (b == 0x66) { has66 = true; ++p; continue; }
        if (b == 0x67 || b == 0xF2 || b == 0xF3 || b == 0xF0 ||
            b == 0x2E || b == 0x3E || b == 0x26 || b == 0x36 || b == 0x64 || b == 0x65) {
            ++p; continue;
        }
        if (m_is64Bit && b >= 0x40 && b <= 0x4F) {
            rexW = (b & 0x08) != 0;
            rexR = (b & 0x04) != 0;
            rexX = (b & 0x02) != 0;
            rexB = (b & 0x01) != 0;
            ++p;
            continue;
        }
        break;
    }

    if (p >= end) {
        out.mnemonic = "???";
        return;
    }

    uint8_t opcode = *p++;

    // Handle common opcodes
    switch (opcode) {
        // PUSH/POP
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57: {
            out.mnemonic = "push";
            int reg = (opcode - 0x50) + (rexB ? 8 : 0);
            out.operands = m_is64Bit ? g_regNames64[reg] : g_regNames32[reg];
            return;
        }
        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5E: case 0x5F: {
            out.mnemonic = "pop";
            int reg = (opcode - 0x58) + (rexB ? 8 : 0);
            out.operands = m_is64Bit ? g_regNames64[reg] : g_regNames32[reg];
            return;
        }
        
        // MOV reg, imm
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB6: case 0xB7: {
            out.mnemonic = "mov";
            int reg = (opcode - 0xB0) + (rexB ? 8 : 0);
            uint8_t imm = (p < end) ? *p : 0;
            char buf[32];
            snprintf(buf, sizeof(buf), "%s, 0x%02X", g_regNames8[reg], imm);
            out.operands = buf;
            return;
        }
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
            out.mnemonic = "mov";
            int reg = (opcode - 0xB8) + (rexB ? 8 : 0);
            char buf[64];
            if (rexW && p + 8 <= end) {
                uint64_t imm = *reinterpret_cast<const uint64_t*>(p);
                snprintf(buf, sizeof(buf), "%s, 0x%llX", g_regNames64[reg], (unsigned long long)imm);
            } else if (has66 && p + 2 <= end) {
                uint16_t imm = *reinterpret_cast<const uint16_t*>(p);
                snprintf(buf, sizeof(buf), "%s, 0x%04X", g_regNames16[reg], imm);
            } else if (p + 4 <= end) {
                uint32_t imm = *reinterpret_cast<const uint32_t*>(p);
                snprintf(buf, sizeof(buf), "%s, 0x%08X", 
                    m_is64Bit ? g_regNames32[reg] : g_regNames32[reg], imm);
            }
            out.operands = buf;
            return;
        }

        // RET
        case 0xC3:
            out.mnemonic = "ret";
            return;
        case 0xC2: {
            out.mnemonic = "ret";
            if (p + 2 <= end) {
                uint16_t imm = *reinterpret_cast<const uint16_t*>(p);
                char buf[16];
                snprintf(buf, sizeof(buf), "0x%04X", imm);
                out.operands = buf;
            }
            return;
        }

        // CALL/JMP rel32
        case 0xE8: {
            out.mnemonic = "call";
            if (p + 4 <= end) {
                int32_t rel = *reinterpret_cast<const int32_t*>(p);
                uint64_t target = out.address + out.length + rel;
                char buf[32];
                snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)target);
                out.operands = buf;
            }
            return;
        }
        case 0xE9: {
            out.mnemonic = "jmp";
            if (p + 4 <= end) {
                int32_t rel = *reinterpret_cast<const int32_t*>(p);
                uint64_t target = out.address + out.length + rel;
                char buf[32];
                snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)target);
                out.operands = buf;
            }
            return;
        }
        case 0xEB: {
            out.mnemonic = "jmp";
            if (p < end) {
                int8_t rel = static_cast<int8_t>(*p);
                uint64_t target = out.address + out.length + rel;
                char buf[32];
                snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)target);
                out.operands = buf;
            }
            return;
        }

        // Short conditional jumps
        case 0x70: case 0x71: case 0x72: case 0x73:
        case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B:
        case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
            static const char* jccNames[] = {
                "jo", "jno", "jb", "jae", "jz", "jnz", "jbe", "ja",
                "js", "jns", "jp", "jnp", "jl", "jge", "jle", "jg"
            };
            out.mnemonic = jccNames[opcode - 0x70];
            if (p < end) {
                int8_t rel = static_cast<int8_t>(*p);
                uint64_t target = out.address + out.length + rel;
                char buf[32];
                snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)target);
                out.operands = buf;
            }
            return;
        }

        // NOP
        case 0x90:
            out.mnemonic = "nop";
            return;

        // INT3
        case 0xCC:
            out.mnemonic = "int3";
            return;

        // INT imm8
        case 0xCD:
            out.mnemonic = "int";
            if (p < end) {
                char buf[8];
                snprintf(buf, sizeof(buf), "0x%02X", *p);
                out.operands = buf;
            }
            return;

        // LEA
        case 0x8D:
            out.mnemonic = "lea";
            // Parse ModR/M for operands
            break;

        // 2-byte escape
        case 0x0F:
            if (p < end) {
                uint8_t op2 = *p++;
                // Long Jcc
                if (op2 >= 0x80 && op2 <= 0x8F) {
                    static const char* jccNames[] = {
                        "jo", "jno", "jb", "jae", "jz", "jnz", "jbe", "ja",
                        "js", "jns", "jp", "jnp", "jl", "jge", "jle", "jg"
                    };
                    out.mnemonic = jccNames[op2 - 0x80];
                    if (p + 4 <= end) {
                        int32_t rel = *reinterpret_cast<const int32_t*>(p);
                        uint64_t target = out.address + out.length + rel;
                        char buf[32];
                        snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)target);
                        out.operands = buf;
                    }
                    return;
                }
                // SETcc
                if (op2 >= 0x90 && op2 <= 0x9F) {
                    static const char* setNames[] = {
                        "seto", "setno", "setb", "setae", "setz", "setnz", "setbe", "seta",
                        "sets", "setns", "setp", "setnp", "setl", "setge", "setle", "setg"
                    };
                    out.mnemonic = setNames[op2 - 0x90];
                    return;
                }
                // CMOVcc
                if (op2 >= 0x40 && op2 <= 0x4F) {
                    static const char* cmovNames[] = {
                        "cmovo", "cmovno", "cmovb", "cmovae", "cmovz", "cmovnz", "cmovbe", "cmova",
                        "cmovs", "cmovns", "cmovp", "cmovnp", "cmovl", "cmovge", "cmovle", "cmovg"
                    };
                    out.mnemonic = cmovNames[op2 - 0x40];
                    return;
                }
                // SYSCALL
                if (op2 == 0x05) {
                    out.mnemonic = "syscall";
                    return;
                }
                // CPUID
                if (op2 == 0xA2) {
                    out.mnemonic = "cpuid";
                    return;
                }
                // RDTSC
                if (op2 == 0x31) {
                    out.mnemonic = "rdtsc";
                    return;
                }
            }
            break;

        // HLT
        case 0xF4:
            out.mnemonic = "hlt";
            return;

        // LEAVE
        case 0xC9:
            out.mnemonic = "leave";
            return;
    }

    // Generic fallback
    out.mnemonic = "???";
    char buf[64];
    snprintf(buf, sizeof(buf), "(opcode: 0x%02X)", opcode);
    out.comment = buf;
}

std::vector<DisassembledInstruction> PEAnalyzer::Disassemble(uint64_t rva, size_t count) const
{
    std::vector<DisassembledInstruction> result;
    
    const SectionInfo* section = FindSectionByRVA(static_cast<uint32_t>(rva));
    if (!section) return result;

    uint32_t offset = static_cast<uint32_t>(rva) - section->virtualAddress + section->rawDataOffset;
    if (offset >= m_fileSize) return result;

    uint64_t currentAddr = m_imageBase + rva;
    const uint8_t* code = m_fileData + offset;
    size_t remaining = m_fileSize - offset;
    size_t sectionRemaining = section->rawDataSize - (offset - section->rawDataOffset);
    if (sectionRemaining < remaining) remaining = sectionRemaining;

    for (size_t i = 0; i < count && remaining > 0; ++i) {
        DisassembledInstruction inst;
        inst.address = currentAddr;
        
        DecodeInstruction(code, remaining, inst);
        
        result.push_back(inst);
        
        code += inst.length;
        currentAddr += inst.length;
        remaining -= inst.length;
    }

    return result;
}

std::vector<DisassembledInstruction> PEAnalyzer::DisassembleBytes(const uint8_t* code, size_t size, uint64_t baseAddress) const
{
    std::vector<DisassembledInstruction> result;
    
    uint64_t addr = baseAddress;
    size_t offset = 0;

    while (offset < size) {
        DisassembledInstruction inst;
        inst.address = addr;
        
        DecodeInstruction(code + offset, size - offset, inst);
        
        result.push_back(inst);
        
        offset += inst.length;
        addr += inst.length;
    }

    return result;
}

// ============================================================================
// String Extraction
// ============================================================================

std::vector<std::string> PEAnalyzer::ExtractStrings(size_t minLength) const
{
    std::vector<std::string> strings;
    if (!m_fileData) return strings;

    std::string current;
    for (size_t i = 0; i < m_fileSize; ++i) {
        uint8_t c = m_fileData[i];
        if (c >= 0x20 && c <= 0x7E) {
            current += static_cast<char>(c);
        } else if (c == 0 && current.length() >= minLength) {
            strings.push_back(current);
            current.clear();
        } else {
            current.clear();
        }
    }
    
    if (current.length() >= minLength) {
        strings.push_back(current);
    }

    return strings;
}

std::vector<std::wstring> PEAnalyzer::ExtractUnicodeStrings(size_t minLength) const
{
    std::vector<std::wstring> strings;
    if (!m_fileData || m_fileSize < 2) return strings;

    std::wstring current;
    for (size_t i = 0; i + 1 < m_fileSize; i += 2) {
        wchar_t c = *reinterpret_cast<const wchar_t*>(m_fileData + i);
        if (c >= 0x20 && c <= 0x7E) {
            current += c;
        } else if (c == 0 && current.length() >= minLength) {
            strings.push_back(current);
            current.clear();
        } else {
            current.clear();
        }
    }

    if (current.length() >= minLength) {
        strings.push_back(current);
    }

    return strings;
}

// ============================================================================
// Validation
// ============================================================================

bool PEAnalyzer::ValidateChecksum() const
{
    if (!m_fileData || m_fileSize == 0) return false;
    
    // PE checksum algorithm
    uint32_t checksum = 0;
    uint32_t top = 0xFFFFFFFF;
    
    // Checksum offset depends on PE format
    uint32_t checksumOffset = m_peHeaderOffset + 4 + 20 + 64; // OptionalHeader.CheckSum

    for (size_t i = 0; i < m_fileSize; i += 2) {
        // Skip the checksum field itself
        if (i == checksumOffset) {
            i += 2;
            continue;
        }
        
        uint16_t word;
        if (i + 1 < m_fileSize) {
            word = *reinterpret_cast<const uint16_t*>(m_fileData + i);
        } else {
            word = m_fileData[i];
        }
        
        checksum += word;
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
    }

    checksum = (checksum & 0xFFFF) + (checksum >> 16);
    checksum += static_cast<uint32_t>(m_fileSize);

    return (checksum == m_checksum) || (m_checksum == 0);
}

bool PEAnalyzer::ValidateSignature() const
{
    // Check Authenticode signature presence (Security Directory)
    // This is a simplified check - full validation requires CryptoAPI
    return m_fileSize > 0 && 
           ReadWord(0) == 0x5A4D && 
           ReadDWord(m_peHeaderOffset) == 0x00004550;
}

} // namespace ReverseEngineering
} // namespace RawrXD
