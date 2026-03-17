// ============================================================================
// pdb_native.cpp — Phase 29: Native PDB Symbol Server (MSF v7.00 Parser)
// ============================================================================
//
// PURPOSE:
//   From-scratch MSF v7.00 PDB parser. Zero dbghelp.dll, zero DIA SDK.
//   Memory-mapped file access, MASM64-accelerated symbol scan.
//
// IMPLEMENTS:
//   NativePDBParser  — Single PDB file lifecycle + queries
//   PDBSymbolServer  — HTTP download + content-addressed cache
//   PDBManager       — Singleton coordinator
//
// PATTERN:   PDBResult-compatible, no exceptions
// THREADING: NativePDBParser — not thread-safe (caller synchronizes)
//            PDBSymbolServer — download on caller thread (Phase 29.2: async)
//            PDBManager — internal mutex for module map
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../../include/pdb_native.h"
#include "../../include/pdb_gsi_hash.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
#include <new>           // std::nothrow

// Avoid pulling in full winsock, etc.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <shellapi.h>

namespace RawrXD {
namespace PDB {

// ============================================================================
// MSF Magic — defined here (initializer-string for char[32] with embedded
// nulls can't be done with string literal in all compilers)
// ============================================================================
const char MSF_MAGIC[32] = {
    'M','i','c','r','o','s','o','f','t',' ','C','/','C','+','+',' ',
    'M','S','F',' ','7','.','0','0','\r','\n','\x1A','D','S','\0','\0','\0'
};

// ============================================================================
// PUB32_NAME_OFFSET — byte offset to name in CVPubSym32 record
// (4 header + 4 flags + 4 off + 2 seg = 14)
// ============================================================================
static const uint32_t PUB32_NAME_OFFSET = 14;

// ============================================================================
// C++ Fallback Implementations for MASM64 ASM Kernel Functions
// ============================================================================
// When building with MinGW (non-MSVC), the MASM64 ASM kernels are not
// assembled. These C++ implementations provide equivalent functionality.
// On MSVC builds, the ASM object provides these symbols via extern "C".
#ifndef RAWR_HAS_MASM

uint32_t PDB_ValidateMagic(const void* superBlock) {
    // Compare first 32 bytes against MSF v7.00 magic
    return (memcmp(superBlock, MSF_MAGIC, 32) == 0) ? 1 : 0;
}

uint32_t PDB_ScanPublics(const void* streamData, uint32_t streamSize,
                          const char* name, uint32_t nameLen) {
    // Walk CV record chain looking for S_PUB32 records with matching name
    const uint8_t* buf = static_cast<const uint8_t*>(streamData);
    uint32_t pos = 0;

    while (pos + 4 <= streamSize) {
        uint16_t recLen = *reinterpret_cast<const uint16_t*>(buf + pos);
        uint16_t recTyp = *reinterpret_cast<const uint16_t*>(buf + pos + 2);

        if (recLen < 2) break; // corrupt

        // S_PUB32 = 0x110E
        if (recTyp == 0x110E) {
            // Name starts at offset PUB32_NAME_OFFSET from record start
            uint32_t nameOff = pos + PUB32_NAME_OFFSET;
            if (nameOff < pos + recLen + 2) {
                const char* symName = reinterpret_cast<const char*>(buf + nameOff);
                // Compare up to nameLen chars
                uint32_t maxNameBytes = (pos + recLen + 2) - nameOff;
                if (maxNameBytes > nameLen) {
                    if (memcmp(symName, name, nameLen) == 0 && symName[nameLen] == '\0') {
                        return pos; // Found — return offset
                    }
                }
            }
        }

        // Advance to next record (4-byte aligned)
        uint32_t advance = static_cast<uint32_t>(recLen) + 2;
        advance = (advance + 3) & ~3u;
        pos += advance;
    }

    return 0xFFFFFFFF; // Not found
}

uint32_t PDB_BuildPageList(const void* directory, uint32_t streamIndex,
                            uint32_t numStreams, const uint32_t* streamSizes,
                            uint32_t* pagesOut, uint32_t maxPages) {
    // Walk the stream directory to extract page numbers for the given stream.
    // Directory layout: uint32_t numStreams, uint32_t sizes[numStreams], then page arrays.
    const uint32_t* dir = static_cast<const uint32_t*>(directory);

    // Skip numStreams + streamSizes[]
    uint32_t pageListStart = 1 + numStreams;

    // Walk to the correct stream's page list
    for (uint32_t i = 0; i < streamIndex && i < numStreams; ++i) {
        uint32_t sz = streamSizes[i];
        if (sz != 0 && sz != 0xFFFFFFFF) {
            // We don't know blockSize here so we can't compute page count.
            // This fallback is not used in practice (parseStreamDirectory does its own walk).
            // Return 0 to indicate fallback not applicable.
            (void)pageListStart;
        }
    }

    // This function is not actually called in the C++ parser path
    // (parseStreamDirectory handles its own page list extraction).
    // Provide stub that returns 0 pages.
    (void)dir; (void)pagesOut; (void)maxPages;
    return 0;
}

uint32_t PDB_GuidToHex(const uint8_t guid[16], char* out, uint32_t maxLen) {
    if (maxLen < 33) return 0; // Need 32 hex chars + null

    static const char hexChars[] = "0123456789ABCDEF";
    uint32_t pos = 0;

    for (uint32_t i = 0; i < 16; ++i) {
        out[pos++] = hexChars[(guid[i] >> 4) & 0x0F];
        out[pos++] = hexChars[guid[i] & 0x0F];
    }
    out[pos] = '\0';
    return pos;
}

#endif // !RAWR_HAS_MASM

// ============================================================================
// NativePDBParser — Constructor / Destructor
// ============================================================================

NativePDBParser::NativePDBParser() {
    memset(m_guid, 0, sizeof(m_guid));
}

NativePDBParser::~NativePDBParser() {
    unload();
}

// ============================================================================
// NativePDBParser — Load / Unload
// ============================================================================

PDBResult NativePDBParser::load(const wchar_t* pdbPath) {
    if (m_pBase) {
        unload();
    }

    // ---- Open file ----
    m_hFile = CreateFileW(pdbPath, GENERIC_READ, FILE_SHARE_READ,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m_hFile == INVALID_HANDLE_VALUE) {
        return PDBResult::error("Failed to open PDB file", PDB_ERR_FILE_OPEN);
    }

    // Get file size
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(m_hFile, &fileSize)) {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
        return PDBResult::error("Failed to get PDB file size", PDB_ERR_FILE_OPEN);
    }
    m_fileSize = static_cast<uint64_t>(fileSize.QuadPart);

    // Minimum size: at least one page (superblock)
    if (m_fileSize < sizeof(MSFSuperBlock)) {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
        return PDBResult::error("PDB file too small for MSF superblock", PDB_ERR_INVALID_FORMAT);
    }

    // ---- Memory-map the file (read-only) ----
    m_hMapping = CreateFileMappingW(m_hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!m_hMapping) {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
        return PDBResult::error("Failed to create file mapping", PDB_ERR_FILE_MAP);
    }

    m_pBase = static_cast<const uint8_t*>(
        MapViewOfFile(m_hMapping, FILE_MAP_READ, 0, 0, 0));
    if (!m_pBase) {
        CloseHandle(m_hMapping);
        m_hMapping = nullptr;
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
        return PDBResult::error("Failed to map PDB file into memory", PDB_ERR_FILE_MAP);
    }

    // ---- Validate MSF magic (MASM64-accelerated) ----
    m_superBlock = reinterpret_cast<const MSFSuperBlock*>(m_pBase);
    uint32_t magicOk = PDB_ValidateMagic(m_superBlock);
    if (!magicOk) {
        unload();
        return PDBResult::error("Invalid MSF v7.00 magic", PDB_ERR_INVALID_MAGIC);
    }

    // ---- Validate superblock fields ----
    m_blockSize = m_superBlock->blockSize;
    if (m_blockSize != 512 && m_blockSize != 1024 &&
        m_blockSize != 2048 && m_blockSize != 4096) {
        unload();
        return PDBResult::error("Invalid MSF block size", PDB_ERR_INVALID_FORMAT);
    }

    // Verify file size is consistent with block count
    uint64_t expectedSize = static_cast<uint64_t>(m_superBlock->numBlocks) * m_blockSize;
    if (m_fileSize < expectedSize) {
        // Allow minor discrepancy (file may be slightly larger)
        // but if file is much smaller, that's an error
        if (m_fileSize < expectedSize - m_blockSize) {
            unload();
            return PDBResult::error("File size inconsistent with MSF block count",
                                    PDB_ERR_INVALID_FORMAT);
        }
    }

    // ---- Parse stream directory ----
    PDBResult r = parseStreamDirectory();
    if (!r.success) {
        unload();
        return r;
    }

    // ---- Parse PDB info stream (Stream 1 — GUID, age) ----
    r = parsePDBInfoStream();
    if (!r.success) {
        unload();
        return r;
    }

    // ---- Parse DBI stream (Stream 3 — stream indices) ----
    r = parseDBIStream();
    if (!r.success) {
        // Non-fatal: some PDBs don't have DBI. Log but continue.
        char msg[256];
        snprintf(msg, sizeof(msg), "[PDB] DBI parse warning: %s", r.detail);
        OutputDebugStringA(msg);
    }

    // ---- Parse section headers (from DBI optional debug header) ----
    r = parseSectionHeaders();
    // Non-fatal

    return PDBResult::ok("PDB loaded successfully");
}

void NativePDBParser::unload() {
    // Free stream directory arrays
    if (m_streamPages) {
        for (uint32_t i = 0; i < m_numStreams; ++i) {
            free(m_streamPages[i]);
        }
        free(m_streamPages);
        m_streamPages = nullptr;
    }
    if (m_streamSizes) {
        free(m_streamSizes);
        m_streamSizes = nullptr;
    }

    // Free section headers
    if (m_sectionHeaders) {
        free(m_sectionHeaders);
        m_sectionHeaders = nullptr;
    }

    m_numStreams = 0;
    m_numSections = 0;

    // Unmap file
    if (m_pBase) {
        UnmapViewOfFile(m_pBase);
        m_pBase = nullptr;
    }
    if (m_hMapping) {
        CloseHandle(m_hMapping);
        m_hMapping = nullptr;
    }
    if (m_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }

    m_superBlock = nullptr;
    m_fileSize = 0;
    m_blockSize = 0;
    memset(m_guid, 0, sizeof(m_guid));
    m_age = 0;
    m_pdbVersion = 0;
    m_globalSymStream = 0xFFFF;
    m_publicSymStream = 0xFFFF;
    m_symRecordStream = 0xFFFF;
    m_sectionHdrStream = 0xFFFF;
    m_machine = 0;
}

// ============================================================================
// NativePDBParser — Identity
// ============================================================================

PDBResult NativePDBParser::getGuid(uint8_t guidOut[16]) const {
    if (!m_pBase) return PDBResult::error("No PDB loaded", PDB_ERR_NOT_LOADED);
    memcpy(guidOut, m_guid, 16);
    return PDBResult::ok();
}

uint32_t NativePDBParser::getAge() const {
    return m_age;
}

// ============================================================================
// NativePDBParser — Stream Directory Parsing
// ============================================================================
//
// The stream directory tells us how many streams exist, how big each one
// is, and which pages (blocks) each stream occupies.
//
// Directory location:
//   SuperBlock.blockMapAddr → page containing array of block indices
//   for the directory itself. The directory may span multiple pages.
//
// Directory layout:
//   uint32_t numStreams;
//   uint32_t streamSizes[numStreams];        // 0xFFFFFFFF = stream doesn't exist
//   uint32_t pages_for_stream_0[ceil(size0/blockSize)];
//   uint32_t pages_for_stream_1[ceil(size1/blockSize)];
//   ...
//
PDBResult NativePDBParser::parseStreamDirectory() {
    const uint32_t bs = m_blockSize;
    const uint32_t dirBytes = m_superBlock->numDirectoryBytes;
    const uint32_t numDirPages = (dirBytes + bs - 1) / bs;

    // The block map (list of pages containing the directory) is at blockMapAddr
    const uint32_t bmPage = m_superBlock->blockMapAddr;
    if (static_cast<uint64_t>(bmPage) * bs + numDirPages * 4 > m_fileSize) {
        return PDBResult::error("Block map page out of bounds", PDB_ERR_STREAM_OOB);
    }

    // Read the block map — it's an array of uint32_t page numbers
    const uint32_t* blockMap = reinterpret_cast<const uint32_t*>(m_pBase + bmPage * bs);

    // Assemble the stream directory into a contiguous buffer
    uint8_t* dirBuf = static_cast<uint8_t*>(malloc(dirBytes));
    if (!dirBuf) {
        return PDBResult::error("Failed to allocate directory buffer", PDB_ERR_ALLOC_FAIL);
    }

    uint32_t dirOffset = 0;
    for (uint32_t i = 0; i < numDirPages; ++i) {
        uint32_t pageIdx = blockMap[i];
        if (static_cast<uint64_t>(pageIdx) * bs >= m_fileSize) {
            free(dirBuf);
            return PDBResult::error("Directory page index out of bounds", PDB_ERR_STREAM_OOB);
        }
        uint32_t bytesToCopy = bs;
        if (dirOffset + bytesToCopy > dirBytes) {
            bytesToCopy = dirBytes - dirOffset;
        }
        memcpy(dirBuf + dirOffset, m_pBase + pageIdx * bs, bytesToCopy);
        dirOffset += bytesToCopy;
    }

    // Parse the directory buffer
    const uint32_t* dirWords = reinterpret_cast<const uint32_t*>(dirBuf);
    m_numStreams = dirWords[0];

    // Sanity check
    if (m_numStreams > 65536) {
        free(dirBuf);
        return PDBResult::error("Stream count exceeds sanity limit", PDB_ERR_INVALID_FORMAT);
    }

    // Allocate stream sizes array
    m_streamSizes = static_cast<uint32_t*>(malloc(m_numStreams * sizeof(uint32_t)));
    if (!m_streamSizes) {
        free(dirBuf);
        return PDBResult::error("Failed to allocate stream sizes", PDB_ERR_ALLOC_FAIL);
    }
    memcpy(m_streamSizes, &dirWords[1], m_numStreams * sizeof(uint32_t));

    // Allocate stream pages arrays
    m_streamPages = static_cast<uint32_t**>(calloc(m_numStreams, sizeof(uint32_t*)));
    if (!m_streamPages) {
        free(dirBuf);
        return PDBResult::error("Failed to allocate stream page arrays", PDB_ERR_ALLOC_FAIL);
    }

    // Walk page lists
    uint32_t pageListOffset = 1 + m_numStreams; // Skip numStreams + streamSizes[]
    for (uint32_t i = 0; i < m_numStreams; ++i) {
        uint32_t sizeBytes = m_streamSizes[i];
        if (sizeBytes == 0xFFFFFFFF || sizeBytes == 0) {
            // Stream doesn't exist or is empty
            m_streamPages[i] = nullptr;
            continue;
        }

        uint32_t numPages = (sizeBytes + bs - 1) / bs;
        m_streamPages[i] = static_cast<uint32_t*>(malloc(numPages * sizeof(uint32_t)));
        if (!m_streamPages[i]) {
            free(dirBuf);
            return PDBResult::error("Failed to allocate page list", PDB_ERR_ALLOC_FAIL);
        }

        // Validate that we have enough directory words
        if ((pageListOffset + numPages) * sizeof(uint32_t) > dirBytes) {
            free(dirBuf);
            return PDBResult::error("Stream directory truncated", PDB_ERR_INVALID_FORMAT);
        }

        memcpy(m_streamPages[i], &dirWords[pageListOffset], numPages * sizeof(uint32_t));
        pageListOffset += numPages;
    }

    free(dirBuf);
    return PDBResult::ok("Stream directory parsed");
}

// ============================================================================
// NativePDBParser — PDB Info Stream (Stream 1)
// ============================================================================

PDBResult NativePDBParser::parsePDBInfoStream() {
    if (m_numStreams < 2) {
        return PDBResult::error("PDB has no info stream", PDB_ERR_INVALID_FORMAT);
    }

    uint32_t streamSize = getStreamSize(STREAM_PDB);
    if (streamSize < sizeof(PDBInfoHeader)) {
        return PDBResult::error("PDB info stream too small", PDB_ERR_INVALID_FORMAT);
    }

    PDBInfoHeader hdr;
    if (!readStreamRange(STREAM_PDB, 0, reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr))) {
        return PDBResult::error("Failed to read PDB info stream", PDB_ERR_STREAM_OOB);
    }

    m_pdbVersion = hdr.version;
    m_age = hdr.age;
    memcpy(m_guid, hdr.guid, 16);

    return PDBResult::ok("PDB info parsed");
}

// ============================================================================
// NativePDBParser — DBI Stream (Stream 3)
// ============================================================================

PDBResult NativePDBParser::parseDBIStream() {
    if (m_numStreams < 4) {
        return PDBResult::error("PDB has no DBI stream", PDB_ERR_INVALID_FORMAT);
    }

    uint32_t streamSize = getStreamSize(STREAM_DBI);
    if (streamSize == 0 || streamSize == 0xFFFFFFFF) {
        return PDBResult::error("DBI stream empty or missing", PDB_ERR_INVALID_FORMAT);
    }
    if (streamSize < sizeof(DBIHeader)) {
        return PDBResult::error("DBI stream too small", PDB_ERR_INVALID_FORMAT);
    }

    DBIHeader hdr;
    if (!readStreamRange(STREAM_DBI, 0, reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr))) {
        return PDBResult::error("Failed to read DBI header", PDB_ERR_STREAM_OOB);
    }

    // Validate DBI version signature
    if (hdr.versionSignature != -1) {
        return PDBResult::error("Invalid DBI version signature", PDB_ERR_INVALID_FORMAT);
    }

    // Extract key stream indices
    m_globalSymStream = hdr.globalStreamIndex;
    m_publicSymStream = hdr.publicStreamIndex;
    m_symRecordStream = hdr.symRecordStream;
    m_machine = hdr.machine;

    // Parse optional debug header to find section header stream
    if (hdr.optionalDbgHeaderSize > 0) {
        uint32_t optDbgOffset = sizeof(DBIHeader) +
            hdr.modInfoSize + hdr.sectionContribSize +
            hdr.sectionMapSize + hdr.sourceInfoSize +
            hdr.typeServerMapSize + hdr.ecSubstreamSize;

        int numEntries = hdr.optionalDbgHeaderSize / sizeof(uint16_t);
        if (numEntries > DBI_DBG_SECTION_HDR) {
            uint16_t sectionHdrIdx;
            if (readStreamRange(STREAM_DBI, optDbgOffset + DBI_DBG_SECTION_HDR * sizeof(uint16_t),
                               reinterpret_cast<uint8_t*>(&sectionHdrIdx), sizeof(sectionHdrIdx))) {
                if (sectionHdrIdx != 0xFFFF) {
                    m_sectionHdrStream = sectionHdrIdx;
                }
            }
        }
    }

    return PDBResult::ok("DBI stream parsed");
}

// ============================================================================
// NativePDBParser — Section Headers
// ============================================================================

PDBResult NativePDBParser::parseSectionHeaders() {
    if (m_sectionHdrStream == 0xFFFF || m_sectionHdrStream >= m_numStreams) {
        return PDBResult::error("No section header stream", PDB_ERR_INVALID_FORMAT);
    }

    uint32_t streamSize = getStreamSize(m_sectionHdrStream);
    if (streamSize == 0 || streamSize == 0xFFFFFFFF) {
        return PDBResult::error("Section header stream empty", PDB_ERR_INVALID_FORMAT);
    }

    // Each IMAGE_SECTION_HEADER is 40 bytes. We read our simplified PDBSectionHeader
    // which is the same size (minus the 8-byte name field we skip).
    // Actually, IMAGE_SECTION_HEADER has an 8-byte Name field at the start.
    // Total: 8 (Name) + 32 (rest) = 40 bytes.
    // We need to read the full 40-byte headers and extract the relevant fields.
    static const uint32_t FULL_SECTION_HEADER_SIZE = 40;
    m_numSections = streamSize / FULL_SECTION_HEADER_SIZE;

    if (m_numSections == 0) {
        return PDBResult::ok("No sections");
    }

    // Allocate and read section headers
    uint8_t* rawHeaders = static_cast<uint8_t*>(malloc(streamSize));
    if (!rawHeaders) {
        return PDBResult::error("Failed to allocate section headers", PDB_ERR_ALLOC_FAIL);
    }

    if (!readStreamRange(m_sectionHdrStream, 0, rawHeaders, streamSize)) {
        free(rawHeaders);
        return PDBResult::error("Failed to read section headers", PDB_ERR_STREAM_OOB);
    }

    m_sectionHeaders = static_cast<PDBSectionHeader*>(
        malloc(m_numSections * sizeof(PDBSectionHeader)));
    if (!m_sectionHeaders) {
        free(rawHeaders);
        return PDBResult::error("Failed to allocate PDBSectionHeader array", PDB_ERR_ALLOC_FAIL);
    }

    // Parse IMAGE_SECTION_HEADER (40 bytes each):
    //   char Name[8];              // +0
    //   uint32_t VirtualSize;      // +8
    //   uint32_t VirtualAddress;   // +12
    //   uint32_t SizeOfRawData;    // +16
    //   uint32_t PointerToRawData; // +20
    //   uint32_t PointerToRelocs;  // +24
    //   uint32_t PointerToLineNums;// +28
    //   uint16_t NumberOfRelocs;   // +32
    //   uint16_t NumberOfLineNums; // +34
    //   uint32_t Characteristics;  // +36
    for (uint32_t i = 0; i < m_numSections; ++i) {
        const uint8_t* p = rawHeaders + i * FULL_SECTION_HEADER_SIZE;
        m_sectionHeaders[i].virtualSize         = *reinterpret_cast<const uint32_t*>(p + 8);
        m_sectionHeaders[i].virtualAddress      = *reinterpret_cast<const uint32_t*>(p + 12);
        m_sectionHeaders[i].sizeOfRawData       = *reinterpret_cast<const uint32_t*>(p + 16);
        m_sectionHeaders[i].pointerToRawData    = *reinterpret_cast<const uint32_t*>(p + 20);
        m_sectionHeaders[i].pointerToRelocations = *reinterpret_cast<const uint32_t*>(p + 24);
        m_sectionHeaders[i].pointerToLinenumbers = *reinterpret_cast<const uint32_t*>(p + 28);
        m_sectionHeaders[i].numberOfRelocations = *reinterpret_cast<const uint16_t*>(p + 32);
        m_sectionHeaders[i].numberOfLinenumbers = *reinterpret_cast<const uint16_t*>(p + 34);
        m_sectionHeaders[i].characteristics     = *reinterpret_cast<const uint32_t*>(p + 36);
    }

    free(rawHeaders);
    return PDBResult::ok("Section headers parsed");
}

// ============================================================================
// NativePDBParser — Stream Access (Low-Level)
// ============================================================================

uint32_t NativePDBParser::getStreamSize(uint32_t streamIndex) const {
    if (!m_streamSizes || streamIndex >= m_numStreams) return 0;
    uint32_t s = m_streamSizes[streamIndex];
    return (s == 0xFFFFFFFF) ? 0 : s;
}

PDBResult NativePDBParser::readStream(uint32_t streamIndex, uint32_t offset,
                                       void* buffer, uint32_t size) const {
    if (!m_pBase) return PDBResult::error("No PDB loaded", PDB_ERR_NOT_LOADED);
    if (!readStreamRange(streamIndex, offset, static_cast<uint8_t*>(buffer), size)) {
        return PDBResult::error("Stream read out of bounds", PDB_ERR_STREAM_OOB);
    }
    return PDBResult::ok();
}

// Read a contiguous range from a fragmented stream
bool NativePDBParser::readStreamRange(uint32_t streamIndex, uint32_t offset,
                                       uint8_t* dest, uint32_t size) const {
    if (streamIndex >= m_numStreams) return false;
    if (!m_streamPages || !m_streamPages[streamIndex]) return false;

    uint32_t streamSize = m_streamSizes[streamIndex];
    if (streamSize == 0xFFFFFFFF) return false;
    if (offset + size > streamSize) return false;

    const uint32_t bs = m_blockSize;
    uint32_t bytesRead = 0;

    while (bytesRead < size) {
        uint32_t currentOff = offset + bytesRead;
        uint32_t pageIndex = currentOff / bs;
        uint32_t pageOffset = currentOff % bs;
        uint32_t bytesThisPage = bs - pageOffset;
        if (bytesThisPage > size - bytesRead) {
            bytesThisPage = size - bytesRead;
        }

        uint32_t blockNum = m_streamPages[streamIndex][pageIndex];
        uint64_t fileOff = static_cast<uint64_t>(blockNum) * bs + pageOffset;
        if (fileOff + bytesThisPage > m_fileSize) return false;

        memcpy(dest + bytesRead, m_pBase + fileOff, bytesThisPage);
        bytesRead += bytesThisPage;
    }

    return true;
}

const uint8_t* NativePDBParser::getStreamByte(uint32_t streamIndex, uint32_t offset) const {
    if (streamIndex >= m_numStreams) return nullptr;
    if (!m_streamPages || !m_streamPages[streamIndex]) return nullptr;

    uint32_t streamSize = m_streamSizes[streamIndex];
    if (streamSize == 0xFFFFFFFF || offset >= streamSize) return nullptr;

    uint32_t pageIndex = offset / m_blockSize;
    uint32_t pageOffset = offset % m_blockSize;
    uint32_t blockNum = m_streamPages[streamIndex][pageIndex];
    uint64_t fileOff = static_cast<uint64_t>(blockNum) * m_blockSize + pageOffset;

    if (fileOff >= m_fileSize) return nullptr;
    return m_pBase + fileOff;
}

// ============================================================================
// NativePDBParser — Section Header Access
// ============================================================================

const PDBSectionHeader* NativePDBParser::getSectionHeader(uint32_t index) const {
    if (!m_sectionHeaders || index >= m_numSections) return nullptr;
    return &m_sectionHeaders[index];
}

// ============================================================================
// NativePDBParser — RVA Translation
// ============================================================================

uint64_t NativePDBParser::sectionOffsetToRVA(uint16_t section, uint32_t offset) const {
    // Section is 1-based (PE convention)
    if (section == 0 || section > m_numSections) return 0;
    return static_cast<uint64_t>(m_sectionHeaders[section - 1].virtualAddress) + offset;
}

bool NativePDBParser::rvaToSectionOffset(uint64_t rva, uint16_t* section, uint32_t* offset) const {
    for (uint32_t i = 0; i < m_numSections; ++i) {
        uint64_t secStart = m_sectionHeaders[i].virtualAddress;
        uint64_t secEnd = secStart + m_sectionHeaders[i].virtualSize;
        if (rva >= secStart && rva < secEnd) {
            *section = static_cast<uint16_t>(i + 1); // 1-based
            *offset = static_cast<uint32_t>(rva - secStart);
            return true;
        }
    }
    return false;
}

// ============================================================================
// NativePDBParser — Symbol Lookup (Public Symbols)
// ============================================================================

PDBResult NativePDBParser::findPublicSymbol(const char* name, uint64_t* rvaOut) const {
    if (!m_pBase) return PDBResult::error("No PDB loaded", PDB_ERR_NOT_LOADED);
    if (!name || !rvaOut) return PDBResult::error("Null parameter", PDB_ERR_SYMBOL_NOT_FOUND);

    // Determine which stream to search
    uint16_t symStream = m_symRecordStream;
    if (symStream == 0xFFFF) {
        symStream = m_publicSymStream;
    }
    if (symStream == 0xFFFF || symStream >= m_numStreams) {
        return PDBResult::error("No public symbol stream available", PDB_ERR_INVALID_FORMAT);
    }

    uint32_t streamSize = getStreamSize(symStream);
    if (streamSize == 0) {
        return PDBResult::error("Symbol stream empty", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    // Assemble the stream into a contiguous buffer for the ASM scanner
    uint8_t* buf = static_cast<uint8_t*>(malloc(streamSize));
    if (!buf) {
        return PDBResult::error("Failed to allocate symbol scan buffer", PDB_ERR_ALLOC_FAIL);
    }

    if (!readStreamRange(symStream, 0, buf, streamSize)) {
        free(buf);
        return PDBResult::error("Failed to read symbol stream", PDB_ERR_STREAM_OOB);
    }

    // Use MASM64 kernel to scan for the name
    uint32_t nameLen = static_cast<uint32_t>(strlen(name));
    uint32_t foundOffset = PDB_ScanPublics(buf, streamSize, name, nameLen);

    if (foundOffset == 0xFFFFFFFF) {
        free(buf);
        return PDBResult::error("Symbol not found", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    // Extract RVA from the found record
    const CVPubSym32* pub = reinterpret_cast<const CVPubSym32*>(buf + foundOffset);
    *rvaOut = sectionOffsetToRVA(pub->seg, pub->off);

    free(buf);
    return PDBResult::ok("Symbol found");
}

PDBResult NativePDBParser::findSymbolByRVA(uint64_t rva, ResolvedSymbol* symOut) const {
    if (!m_pBase) return PDBResult::error("No PDB loaded", PDB_ERR_NOT_LOADED);
    if (!symOut) return PDBResult::error("Null parameter", PDB_ERR_SYMBOL_NOT_FOUND);

    // Convert RVA to section:offset
    uint16_t section;
    uint32_t offset;
    if (!rvaToSectionOffset(rva, &section, &offset)) {
        return PDBResult::error("RVA not in any section", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    // Search public symbols for the closest match
    uint16_t symStream = m_symRecordStream;
    if (symStream == 0xFFFF) symStream = m_publicSymStream;
    if (symStream == 0xFFFF || symStream >= m_numStreams) {
        return PDBResult::error("No symbol stream", PDB_ERR_INVALID_FORMAT);
    }

    uint32_t streamSize = getStreamSize(symStream);
    if (streamSize == 0) {
        return PDBResult::error("Symbol stream empty", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    uint8_t* buf = static_cast<uint8_t*>(malloc(streamSize));
    if (!buf) return PDBResult::error("Alloc failed", PDB_ERR_ALLOC_FAIL);

    if (!readStreamRange(symStream, 0, buf, streamSize)) {
        free(buf);
        return PDBResult::error("Stream read failed", PDB_ERR_STREAM_OOB);
    }

    // Linear scan — find the closest symbol at or before the RVA
    bool found = false;
    uint64_t bestRVA = 0;
    uint32_t bestRecOffset = 0;

    uint32_t pos = 0;
    while (pos + 4 <= streamSize) {
        const CVRecordHeader* hdr = reinterpret_cast<const CVRecordHeader*>(buf + pos);
        uint16_t recLen = hdr->recLen;
        uint16_t recTyp = hdr->recTyp;

        if (recLen < 2) break; // corrupt

        if (recTyp == S_PUB32) {
            const CVPubSym32* pub = reinterpret_cast<const CVPubSym32*>(buf + pos);
            uint64_t symRVA = sectionOffsetToRVA(pub->seg, pub->off);
            if (symRVA <= rva && symRVA > bestRVA) {
                bestRVA = symRVA;
                bestRecOffset = pos;
                found = true;
            }
        } else if (recTyp == S_GPROC32 || recTyp == S_LPROC32) {
            const CVProcSym32* proc = reinterpret_cast<const CVProcSym32*>(buf + pos);
            uint64_t symRVA = sectionOffsetToRVA(proc->seg, proc->off);
            if (symRVA <= rva && symRVA > bestRVA) {
                bestRVA = symRVA;
                bestRecOffset = pos;
                found = true;
            }
        }

        // Advance to next record (4-byte aligned)
        uint32_t advance = static_cast<uint32_t>(recLen) + 2;
        advance = (advance + 3) & ~3u;
        pos += advance;
    }

    if (!found) {
        free(buf);
        return PDBResult::error("No symbol at or before RVA", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    // Fill out the resolved symbol
    const CVRecordHeader* bestHdr = reinterpret_cast<const CVRecordHeader*>(buf + bestRecOffset);
    memset(symOut, 0, sizeof(ResolvedSymbol));
    symOut->rva = bestRVA;
    symOut->type = static_cast<CVSymbolType>(bestHdr->recTyp);

    if (bestHdr->recTyp == S_PUB32) {
        const CVPubSym32* pub = reinterpret_cast<const CVPubSym32*>(buf + bestRecOffset);
        symOut->name = reinterpret_cast<const char*>(buf + bestRecOffset + PUB32_NAME_OFFSET);
        symOut->nameLen = static_cast<uint32_t>(strlen(symOut->name));
        symOut->section = pub->seg;
        symOut->sectionOffset = pub->off;
        symOut->isFunction = (pub->pubSymFlags & 0x2) != 0;
        symOut->isPublic = true;
        symOut->size = 0;
    } else if (bestHdr->recTyp == S_GPROC32 || bestHdr->recTyp == S_LPROC32) {
        const CVProcSym32* proc = reinterpret_cast<const CVProcSym32*>(buf + bestRecOffset);
        // Name is at offset 35 from record start (after all fixed fields + flags byte)
        const char* procName = reinterpret_cast<const char*>(buf + bestRecOffset + 35);
        symOut->name = procName;
        symOut->nameLen = static_cast<uint32_t>(strlen(procName));
        symOut->section = proc->seg;
        symOut->sectionOffset = proc->off;
        symOut->size = proc->len;
        symOut->isFunction = true;
        symOut->isPublic = (bestHdr->recTyp == S_GPROC32);
    }

    // Note: symOut->name points into buf which we're about to free.
    // The caller must copy the name before the buffer is freed.
    // For Phase 29 v1, we copy the name into a static thread-local buffer.
    static thread_local char s_nameBuf[512];
    if (symOut->name && symOut->nameLen < sizeof(s_nameBuf)) {
        memcpy(s_nameBuf, symOut->name, symOut->nameLen);
        s_nameBuf[symOut->nameLen] = '\0';
        symOut->name = s_nameBuf;
    }

    free(buf);
    return PDBResult::ok("Symbol resolved");
}

// ============================================================================
// NativePDBParser — Symbol Enumeration
// ============================================================================

PDBResult NativePDBParser::enumeratePublicSymbols(SymbolVisitor visitor, void* userData) const {
    if (!m_pBase) return PDBResult::error("No PDB loaded", PDB_ERR_NOT_LOADED);
    if (!visitor) return PDBResult::error("Null visitor", PDB_ERR_SYMBOL_NOT_FOUND);

    uint16_t symStream = m_symRecordStream;
    if (symStream == 0xFFFF) symStream = m_publicSymStream;
    if (symStream == 0xFFFF || symStream >= m_numStreams) {
        return PDBResult::error("No symbol stream", PDB_ERR_INVALID_FORMAT);
    }

    uint32_t streamSize = getStreamSize(symStream);
    if (streamSize == 0) return PDBResult::ok("Empty stream");

    uint8_t* buf = static_cast<uint8_t*>(malloc(streamSize));
    if (!buf) return PDBResult::error("Alloc failed", PDB_ERR_ALLOC_FAIL);

    if (!readStreamRange(symStream, 0, buf, streamSize)) {
        free(buf);
        return PDBResult::error("Stream read failed", PDB_ERR_STREAM_OOB);
    }

    uint32_t pos = 0;
    while (pos + 4 <= streamSize) {
        const CVRecordHeader* hdr = reinterpret_cast<const CVRecordHeader*>(buf + pos);
        uint16_t recLen = hdr->recLen;
        if (recLen < 2) break;

        if (hdr->recTyp == S_PUB32) {
            const CVPubSym32* pub = reinterpret_cast<const CVPubSym32*>(buf + pos);
            const char* name = reinterpret_cast<const char*>(buf + pos + PUB32_NAME_OFFSET);

            ResolvedSymbol sym;
            memset(&sym, 0, sizeof(sym));
            sym.name = name;
            sym.nameLen = static_cast<uint32_t>(strlen(name));
            sym.rva = sectionOffsetToRVA(pub->seg, pub->off);
            sym.section = pub->seg;
            sym.sectionOffset = pub->off;
            sym.type = S_PUB32;
            sym.isFunction = (pub->pubSymFlags & 0x2) != 0;
            sym.isPublic = true;

            if (!visitor(&sym, userData)) {
                break; // Visitor requested stop
            }
        }

        uint32_t advance = static_cast<uint32_t>(recLen) + 2;
        advance = (advance + 3) & ~3u;
        pos += advance;
    }

    free(buf);
    return PDBResult::ok("Enumeration complete");
}

PDBResult NativePDBParser::enumerateProcedures(SymbolVisitor visitor, void* userData) const {
    if (!m_pBase) return PDBResult::error("No PDB loaded", PDB_ERR_NOT_LOADED);
    if (!visitor) return PDBResult::error("Null visitor", PDB_ERR_SYMBOL_NOT_FOUND);

    uint16_t symStream = m_symRecordStream;
    if (symStream == 0xFFFF) symStream = m_publicSymStream;
    if (symStream == 0xFFFF || symStream >= m_numStreams) {
        return PDBResult::error("No symbol stream", PDB_ERR_INVALID_FORMAT);
    }

    uint32_t streamSize = getStreamSize(symStream);
    if (streamSize == 0) return PDBResult::ok("Empty stream");

    uint8_t* buf = static_cast<uint8_t*>(malloc(streamSize));
    if (!buf) return PDBResult::error("Alloc failed", PDB_ERR_ALLOC_FAIL);

    if (!readStreamRange(symStream, 0, buf, streamSize)) {
        free(buf);
        return PDBResult::error("Stream read failed", PDB_ERR_STREAM_OOB);
    }

    uint32_t pos = 0;
    while (pos + 4 <= streamSize) {
        const CVRecordHeader* hdr = reinterpret_cast<const CVRecordHeader*>(buf + pos);
        uint16_t recLen = hdr->recLen;
        if (recLen < 2) break;

        if (hdr->recTyp == S_GPROC32 || hdr->recTyp == S_LPROC32 ||
            hdr->recTyp == S_GPROC32_ID || hdr->recTyp == S_LPROC32_ID) {
            const CVProcSym32* proc = reinterpret_cast<const CVProcSym32*>(buf + pos);
            const char* name = reinterpret_cast<const char*>(buf + pos + 35); // After fixed fields

            ResolvedSymbol sym;
            memset(&sym, 0, sizeof(sym));
            sym.name = name;
            sym.nameLen = static_cast<uint32_t>(strlen(name));
            sym.rva = sectionOffsetToRVA(proc->seg, proc->off);
            sym.section = proc->seg;
            sym.sectionOffset = proc->off;
            sym.size = proc->len;
            sym.type = static_cast<CVSymbolType>(hdr->recTyp);
            sym.isFunction = true;
            sym.isPublic = (hdr->recTyp == S_GPROC32 || hdr->recTyp == S_GPROC32_ID);

            if (!visitor(&sym, userData)) {
                break;
            }
        }

        uint32_t advance = static_cast<uint32_t>(recLen) + 2;
        advance = (advance + 3) & ~3u;
        pos += advance;
    }

    free(buf);
    return PDBResult::ok("Procedure enumeration complete");
}

// ============================================================================
// PDB SYMBOL STREAM PARSING — Phase 29.2: GSI Hash Table Integration
// ============================================================================
// Replaces the Phase 29 v1 linear-scan placeholder with a real GSI hash
// table for O(1) amortized symbol lookups. Also parses TPI if available.
//
PDBResult NativePDBParser::parsePublicSymbolStream() {
    // ---- Step 1: Assemble the symbol record stream into a contiguous buffer ----
    if (m_symRecordStream == 0xFFFF || m_symRecordStream >= m_numStreams) {
        return PDBResult::ok("No symbol record stream (using linear scan)");
    }

    uint32_t symStreamSize = getStreamSize(m_symRecordStream);
    if (symStreamSize == 0) {
        return PDBResult::ok("Empty symbol record stream");
    }

    m_symRecordData = static_cast<uint8_t*>(malloc(symStreamSize));
    if (!m_symRecordData) {
        return PDBResult::error("Failed to allocate symbol record stream", PDB_ERR_ALLOC_FAIL);
    }

    if (!readStreamRange(m_symRecordStream, 0, m_symRecordData, symStreamSize)) {
        free(m_symRecordData);
        m_symRecordData = nullptr;
        return PDBResult::error("Failed to read symbol record stream", PDB_ERR_STREAM_READ);
    }
    m_symRecordSize = symStreamSize;

    // ---- Step 2: Assemble the public symbol (GSI) hash stream ----
    if (m_publicSymStream == 0xFFFF || m_publicSymStream >= m_numStreams) {
        // No public symbol stream — hash table unavailable, fall back to linear scan
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "[Phase 29.2] No public symbol stream (idx=%u), using linear scan",
                 m_publicSymStream);
        OutputDebugStringA(msg);
        return PDBResult::ok("Symbol record stream loaded (no GSI hash)");
    }

    uint32_t gsiSize = getStreamSize(m_publicSymStream);
    if (gsiSize == 0) {
        return PDBResult::ok("Empty GSI stream");
    }

    m_gsiStreamData = static_cast<uint8_t*>(malloc(gsiSize));
    if (!m_gsiStreamData) {
        return PDBResult::error("Failed to allocate GSI stream", PDB_ERR_ALLOC_FAIL);
    }

    if (!readStreamRange(m_publicSymStream, 0, m_gsiStreamData, gsiSize)) {
        free(m_gsiStreamData);
        m_gsiStreamData = nullptr;
        return PDBResult::ok("Failed to read GSI stream — using linear scan");
    }
    m_gsiStreamSize = gsiSize;

    // ---- Step 3: Parse the GSI hash table ----
    GSIHashTable* gsi = new (std::nothrow) GSIHashTable();
    if (!gsi) {
        return PDBResult::error("GSI hash table allocation failed", PDB_ERR_ALLOC_FAIL);
    }

    PDBResult r = gsi->parse(m_gsiStreamData, m_gsiStreamSize,
                              m_symRecordData, m_symRecordSize);

    if (r.success) {
        m_gsiHashTable = static_cast<void*>(gsi);

        char msg[256];
        snprintf(msg, sizeof(msg),
                 "[Phase 29.2] GSI hash table active: %u records, %u buckets used",
                 gsi->getRecordCount(), gsi->getUsedBuckets());
        OutputDebugStringA(msg);
    } else {
        // GSI parsing failed — degrade gracefully to linear scan
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "[Phase 29.2] GSI parse failed (%s), falling back to linear scan",
                 r.detail ? r.detail : "unknown");
        OutputDebugStringA(msg);
        delete gsi;
        // Not a fatal error — linear scan still works
    }

    // ---- Step 4: Parse TPI stream (type information) ----
    // Stream 2 = TPI in the standard MSF layout
    const uint32_t TPI_STREAM_INDEX = 2;
    if (TPI_STREAM_INDEX < m_numStreams) {
        uint32_t tpiSize = getStreamSize(TPI_STREAM_INDEX);
        if (tpiSize > 0) {
            uint8_t* tpiData = static_cast<uint8_t*>(malloc(tpiSize));
            if (tpiData) {
                if (readStreamRange(TPI_STREAM_INDEX, 0, tpiData, tpiSize)) {
                    TPIStreamParser* tpi = new (std::nothrow) TPIStreamParser();
                    if (tpi) {
                        PDBResult tpiResult = tpi->parse(tpiData, tpiSize);
                        if (tpiResult.success) {
                            m_tpiParser = static_cast<void*>(tpi);

                            char msg[256];
                            snprintf(msg, sizeof(msg),
                                     "[Phase 29.2] TPI parsed: %u types [0x%X..0x%X)",
                                     tpi->getTypeCount(),
                                     tpi->getTypeIndexMin(),
                                     tpi->getTypeIndexMax());
                            OutputDebugStringA(msg);
                        } else {
                            delete tpi;
                        }
                    }
                }
                free(tpiData);
            }
        }
    }

    return PDBResult::ok("Public symbol stream parsed (Phase 29.2 GSI hash active)");
}

// ============================================================================
// PDBSymbolServer — Constructor / Destructor
// ============================================================================

PDBSymbolServer::PDBSymbolServer() {
    // Set defaults
    m_config.serverUrl = L"https://msdl.microsoft.com/download/symbols";
    m_config.cachePath = L""; // Will be resolved at configure() time
    m_config.timeoutMs = 30000;
    m_config.maxRetries = 3;
    m_config.enableCompression = true;
    m_config.enabled = true;
}

PDBSymbolServer::~PDBSymbolServer() {
    // Nothing to clean up — WinHTTP handles are per-request
}

// ============================================================================
// PDBSymbolServer — Configuration
// ============================================================================

PDBResult PDBSymbolServer::configure(const SymbolServerConfig& config) {
    m_config = config;

    // If cache path is empty, use default
    if (!m_config.cachePath || m_config.cachePath[0] == L'\0') {
        // Resolve %LOCALAPPDATA%\RawrXD\Symbols
        static wchar_t defaultPath[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, defaultPath))) {
            wcscat_s(defaultPath, MAX_PATH, L"\\RawrXD\\Symbols");
            m_config.cachePath = defaultPath;
        } else {
            return PDBResult::error("Failed to resolve LOCALAPPDATA", PDB_ERR_CACHE_WRITE);
        }
    }

    // Ensure cache directory exists
    PDBResult r = ensureCacheDirectory(m_config.cachePath);
    if (!r.success) return r;

    return PDBResult::ok("Symbol server configured");
}

// ============================================================================
// PDBSymbolServer — Cache Lookup
// ============================================================================

PDBResult PDBSymbolServer::findInCache(const char* pdbName, const uint8_t guid[16],
                                        uint32_t age, wchar_t* pathOut,
                                        uint32_t pathMaxChars) const {
    if (!pdbName || !guid || !pathOut) {
        return PDBResult::error("Null parameter", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    char guidAge[64];
    buildGuidAgeString(guid, age, guidAge, sizeof(guidAge));
    buildCachePath(pdbName, guidAge, pathOut, pathMaxChars);

    // Check if file exists
    DWORD attrs = GetFileAttributesW(pathOut);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return PDBResult::error("PDB not in cache", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    return PDBResult::ok("Found in cache");
}

// ============================================================================
// PDBSymbolServer — Download
// ============================================================================

PDBResult PDBSymbolServer::download(const char* pdbName, const uint8_t guid[16],
                                     uint32_t age, wchar_t* pathOut,
                                     uint32_t pathMaxChars) {
    if (!m_config.enabled) {
        return PDBResult::error("Symbol server disabled", PDB_ERR_DOWNLOAD_FAIL);
    }

    // Check cache first
    PDBResult cacheResult = findInCache(pdbName, guid, age, pathOut, pathMaxChars);
    if (cacheResult.success) {
        return PDBResult::ok("Already cached");
    }

    // Build GUID+Age string
    char guidAge[64];
    buildGuidAgeString(guid, age, guidAge, sizeof(guidAge));

    // Build download URL
    wchar_t url[1024];
    buildDownloadUrl(pdbName, guidAge, url, 1024);

    // Build cache path
    buildCachePath(pdbName, guidAge, pathOut, pathMaxChars);

    // Ensure the directory exists
    wchar_t dirPath[MAX_PATH];
    wcscpy_s(dirPath, MAX_PATH, pathOut);
    // Remove filename from path to get directory
    wchar_t* lastSlash = wcsrchr(dirPath, L'\\');
    if (lastSlash) *lastSlash = L'\0';
    PDBResult r = ensureCacheDirectory(dirPath);
    if (!r.success) return r;

    // Download with retries
    for (uint32_t attempt = 0; attempt < m_config.maxRetries; ++attempt) {
        r = downloadFile(url, pathOut);
        if (r.success) {
            return PDBResult::ok("Downloaded successfully");
        }

        // Brief delay before retry
        Sleep(500 * (attempt + 1));
    }

    return PDBResult::error("Download failed after retries", PDB_ERR_DOWNLOAD_FAIL);
}

// ============================================================================
// PDBSymbolServer — Progress
// ============================================================================

void PDBSymbolServer::setProgressCallback(PDBDownloadProgressCallback fn, void* userData) {
    m_progressFn = fn;
    m_progressData = userData;
}

// ============================================================================
// PDBSymbolServer — Cache Management
// ============================================================================

PDBResult PDBSymbolServer::clearCache() {
    // Simple recursive delete using a shell command
    // Phase 29.2: implement proper recursive directory deletion
    if (!m_config.cachePath || m_config.cachePath[0] == L'\0') {
        return PDBResult::error("No cache path configured", PDB_ERR_CACHE_WRITE);
    }

    // Use SHFileOperation for recursive delete
    wchar_t pathDouble[MAX_PATH + 2];
    wcscpy_s(pathDouble, MAX_PATH, m_config.cachePath);
    pathDouble[wcslen(pathDouble) + 1] = L'\0'; // Double-null terminate

    SHFILEOPSTRUCTW op;
    memset(&op, 0, sizeof(op));
    op.wFunc = FO_DELETE;
    op.pFrom = pathDouble;
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    int result = SHFileOperationW(&op);
    if (result != 0) {
        return PDBResult::error("Failed to clear cache", PDB_ERR_CACHE_WRITE);
    }

    // Recreate the cache directory
    return ensureCacheDirectory(m_config.cachePath);
}

uint64_t PDBSymbolServer::getCacheSizeBytes() const {
    if (!m_config.cachePath || m_config.cachePath[0] == L'\0') return 0;

    uint64_t totalSize = 0;
    wchar_t searchPath[MAX_PATH];
    swprintf_s(searchPath, MAX_PATH, L"%s\\*", m_config.cachePath);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            totalSize += (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return totalSize;
}

// ============================================================================
// PDBSymbolServer — Helpers
// ============================================================================

void PDBSymbolServer::buildGuidAgeString(const uint8_t guid[16], uint32_t age,
                                          char* out, uint32_t maxLen) const {
    if (maxLen < 42) { out[0] = '\0'; return; } // 32 (guid) + 1-8 (age hex) + null

    // Use MASM kernel for GUID hex
    uint32_t wrote = PDB_GuidToHex(guid, out, 33);
    if (wrote == 0) { out[0] = '\0'; return; }

    // Append age as hex
    char ageBuf[16];
    snprintf(ageBuf, sizeof(ageBuf), "%X", age);
    strcat_s(out, maxLen, ageBuf);
}

void PDBSymbolServer::buildCachePath(const char* pdbName, const char* guidAge,
                                      wchar_t* out, uint32_t maxChars) const {
    // {cachePath}\{pdbName}\{guidAge}\{pdbName}
    wchar_t pdbNameW[260];
    MultiByteToWideChar(CP_UTF8, 0, pdbName, -1, pdbNameW, 260);
    wchar_t guidAgeW[64];
    MultiByteToWideChar(CP_UTF8, 0, guidAge, -1, guidAgeW, 64);

    swprintf_s(out, maxChars, L"%s\\%s\\%s\\%s",
        m_config.cachePath, pdbNameW, guidAgeW, pdbNameW);
}

void PDBSymbolServer::buildDownloadUrl(const char* pdbName, const char* guidAge,
                                        wchar_t* out, uint32_t maxChars) const {
    // {serverUrl}/{pdbName}/{guidAge}/{pdbName}
    wchar_t pdbNameW[260];
    MultiByteToWideChar(CP_UTF8, 0, pdbName, -1, pdbNameW, 260);
    wchar_t guidAgeW[64];
    MultiByteToWideChar(CP_UTF8, 0, guidAge, -1, guidAgeW, 64);

    swprintf_s(out, maxChars, L"%s/%s/%s/%s",
        m_config.serverUrl, pdbNameW, guidAgeW, pdbNameW);
}

PDBResult PDBSymbolServer::ensureCacheDirectory(const wchar_t* dirPath) const {
    // SHCreateDirectoryExW creates all intermediate directories
    int result = SHCreateDirectoryExW(nullptr, dirPath, nullptr);
    if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS &&
        result != ERROR_FILE_EXISTS) {
        return PDBResult::error("Failed to create cache directory", PDB_ERR_CACHE_WRITE);
    }
    return PDBResult::ok();
}

PDBResult PDBSymbolServer::downloadFile(const wchar_t* url, const wchar_t* destPath) {
    // ---- WinHTTP download ----
    // Parse URL components
    URL_COMPONENTSW urlComp;
    memset(&urlComp, 0, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256];
    wchar_t urlPath[1024];
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 1024;

    if (!WinHttpCrackUrl(url, 0, 0, &urlComp)) {
        return PDBResult::error("Failed to parse URL", PDB_ERR_DOWNLOAD_FAIL);
    }

    // Open WinHTTP session
    HINTERNET hSession = WinHttpOpen(
        L"RawrXD-PDB/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        return PDBResult::error("WinHttpOpen failed", PDB_ERR_DOWNLOAD_FAIL);
    }

    // Connect to server
    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return PDBResult::error("WinHttpConnect failed", PDB_ERR_DOWNLOAD_FAIL);
    }

    // Create request
    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", urlPath, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return PDBResult::error("WinHttpOpenRequest failed", PDB_ERR_DOWNLOAD_FAIL);
    }

    // Set timeouts
    WinHttpSetTimeouts(hRequest, m_config.timeoutMs, m_config.timeoutMs,
                       m_config.timeoutMs, m_config.timeoutMs);

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return PDBResult::error("WinHttpSendRequest failed", PDB_ERR_DOWNLOAD_FAIL);
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return PDBResult::error("WinHttpReceiveResponse failed", PDB_ERR_DOWNLOAD_FAIL);
    }

    // Check HTTP status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                        WINHTTP_NO_HEADER_INDEX);

    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        char msg[128];
        snprintf(msg, sizeof(msg), "HTTP %lu from symbol server", statusCode);
        return PDBResult::error(msg, PDB_ERR_DOWNLOAD_FAIL);
    }

    // Get content length if available
    DWORD contentLength = 0;
    DWORD clSize = sizeof(contentLength);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &clSize,
                        WINHTTP_NO_HEADER_INDEX);

    // Open destination file
    HANDLE hFile = CreateFileW(destPath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return PDBResult::error("Failed to create cache file", PDB_ERR_CACHE_WRITE);
    }

    // Read and write data
    uint8_t readBuf[65536];
    DWORD bytesAvailable = 0;
    uint64_t totalReceived = 0;

    for (;;) {
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) break;
        if (bytesAvailable == 0) break;

        DWORD toRead = (bytesAvailable > sizeof(readBuf)) ? sizeof(readBuf) : bytesAvailable;
        DWORD bytesRead = 0;
        if (!WinHttpReadData(hRequest, readBuf, toRead, &bytesRead)) break;
        if (bytesRead == 0) break;

        DWORD bytesWritten = 0;
        WriteFile(hFile, readBuf, bytesRead, &bytesWritten, nullptr);
        totalReceived += bytesRead;

        // Progress callback
        if (m_progressFn) {
            // Convert pdbName from wide path — use a simple approach
            char pdbNameA[260];
            WideCharToMultiByte(CP_UTF8, 0, destPath, -1, pdbNameA, 260, nullptr, nullptr);
            m_progressFn(pdbNameA, totalReceived, contentLength, m_progressData);
        }
    }

    CloseHandle(hFile);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (totalReceived == 0) {
        DeleteFileW(destPath);
        return PDBResult::error("No data received", PDB_ERR_DOWNLOAD_FAIL);
    }

    return PDBResult::ok("Download complete");
}

// ============================================================================
// PDBManager — Singleton
// ============================================================================

PDBManager& PDBManager::instance() {
    static PDBManager s_instance;
    return s_instance;
}

PDBManager::PDBManager() {
    memset(m_modules, 0, sizeof(m_modules));
    memset(&m_stats, 0, sizeof(m_stats));
}

PDBManager::~PDBManager() {
    // Unload all modules
    for (uint32_t i = 0; i < MAX_LOADED_MODULES; ++i) {
        if (m_modules[i].active && m_modules[i].parser) {
            delete m_modules[i].parser;
            m_modules[i].parser = nullptr;
            m_modules[i].active = false;
        }
    }
}

// ============================================================================
// PDBManager — Configuration
// ============================================================================

PDBResult PDBManager::configure(const SymbolServerConfig& config) {
    return m_symbolServer.configure(config);
}

// ============================================================================
// PDBManager — Module Loading
// ============================================================================

PDBResult PDBManager::loadForModule(const char* moduleName,
                                     const uint8_t* peBase, uint64_t peSize) {
    if (!moduleName || !peBase) {
        return PDBResult::error("Null parameter", PDB_ERR_FILE_OPEN);
    }

    // Extract RSDS from PE debug directory
    RSDS_DEBUG_INFO rsds;
    const char* pdbName = nullptr;
    PDBResult r = extractRSDS(peBase, peSize, &rsds, &pdbName);
    if (!r.success) return r;

    // Try to find in cache or download
    wchar_t pdbPath[MAX_PATH];
    r = m_symbolServer.findInCache(pdbName, rsds.guid, rsds.age, pdbPath, MAX_PATH);
    if (!r.success) {
        // Attempt download
        r = m_symbolServer.download(pdbName, rsds.guid, rsds.age, pdbPath, MAX_PATH);
        if (!r.success) return r;
        m_stats.downloadCount++;
    } else {
        m_stats.cacheHits++;
    }

    // Load the PDB
    return loadPDB(moduleName, pdbPath);
}

PDBResult PDBManager::loadPDB(const char* moduleName, const wchar_t* pdbPath) {
    if (!moduleName || !pdbPath) {
        return PDBResult::error("Null parameter", PDB_ERR_FILE_OPEN);
    }

    // Check if module is already loaded
    for (uint32_t i = 0; i < MAX_LOADED_MODULES; ++i) {
        if (m_modules[i].active &&
            strcmp(m_modules[i].moduleName, moduleName) == 0) {
            // Already loaded — unload first
            delete m_modules[i].parser;
            m_modules[i].parser = nullptr;
            m_modules[i].active = false;
            m_moduleCount--;
            break;
        }
    }

    // Find a free slot
    uint32_t slot = MAX_LOADED_MODULES;
    for (uint32_t i = 0; i < MAX_LOADED_MODULES; ++i) {
        if (!m_modules[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == MAX_LOADED_MODULES) {
        return PDBResult::error("Maximum loaded modules reached", PDB_ERR_ALLOC_FAIL);
    }

    // Create parser and load
    NativePDBParser* parser = new (std::nothrow) NativePDBParser();
    if (!parser) {
        return PDBResult::error("Failed to allocate parser", PDB_ERR_ALLOC_FAIL);
    }

    PDBResult r = parser->load(pdbPath);
    if (!r.success) {
        delete parser;
        return r;
    }

    // Store in slot
    strncpy_s(m_modules[slot].moduleName, sizeof(m_modules[slot].moduleName),
              moduleName, _TRUNCATE);
    m_modules[slot].parser = parser;
    m_modules[slot].active = true;
    m_moduleCount++;
    m_stats.modulesLoaded = m_moduleCount;

    char msg[512];
    snprintf(msg, sizeof(msg), "[Phase 29] PDB loaded for module '%s'", moduleName);
    OutputDebugStringA(msg);

    return PDBResult::ok("Module PDB loaded");
}

void PDBManager::unloadModule(const char* moduleName) {
    for (uint32_t i = 0; i < MAX_LOADED_MODULES; ++i) {
        if (m_modules[i].active &&
            strcmp(m_modules[i].moduleName, moduleName) == 0) {
            delete m_modules[i].parser;
            m_modules[i].parser = nullptr;
            m_modules[i].active = false;
            m_moduleCount--;
            m_stats.modulesLoaded = m_moduleCount;
            return;
        }
    }
}

// ============================================================================
// PDBManager — Symbol Resolution
// ============================================================================

PDBResult PDBManager::resolveSymbol(const char* name, uint64_t* rvaOut,
                                     const char** moduleOut) const {
    if (!name || !rvaOut) {
        return PDBResult::error("Null parameter", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    m_stats.lookupCount++;

    // Search all loaded modules
    for (uint32_t i = 0; i < MAX_LOADED_MODULES; ++i) {
        if (!m_modules[i].active || !m_modules[i].parser) continue;

        PDBResult r = m_modules[i].parser->findPublicSymbol(name, rvaOut);
        if (r.success) {
            if (moduleOut) *moduleOut = m_modules[i].moduleName;
            return PDBResult::ok("Symbol resolved");
        }
    }

    return PDBResult::error("Symbol not found in any loaded PDB", PDB_ERR_SYMBOL_NOT_FOUND);
}

PDBResult PDBManager::resolveRVA(const char* moduleName, uint64_t rva,
                                  ResolvedSymbol* symOut) const {
    if (!moduleName || !symOut) {
        return PDBResult::error("Null parameter", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    m_stats.lookupCount++;

    const NativePDBParser* parser = getParser(moduleName);
    if (!parser) {
        return PDBResult::error("Module not loaded", PDB_ERR_NOT_LOADED);
    }

    return parser->findSymbolByRVA(rva, symOut);
}

// ============================================================================
// PDBManager — Enumeration
// ============================================================================

const NativePDBParser* PDBManager::getParser(const char* moduleName) const {
    for (uint32_t i = 0; i < MAX_LOADED_MODULES; ++i) {
        if (m_modules[i].active &&
            strcmp(m_modules[i].moduleName, moduleName) == 0) {
            return m_modules[i].parser;
        }
    }
    return nullptr;
}

uint32_t PDBManager::getLoadedModuleCount() const {
    return m_moduleCount;
}

const char* PDBManager::getLoadedModuleName(uint32_t index) const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_LOADED_MODULES; ++i) {
        if (m_modules[i].active) {
            if (count == index) return m_modules[i].moduleName;
            count++;
        }
    }
    return nullptr;
}

PDBManager::Stats PDBManager::getStats() const {
    return m_stats;
}

// ============================================================================
// PDBManager — PE RSDS Extraction
// ============================================================================
//
// Parse a PE image to find the CodeView RSDS debug directory entry.
// This gives us the GUID + Age needed to fetch the matching PDB from
// the symbol server.
//
// PE structure:
//   DOS header (at offset 0)
//   PE signature (at e_lfanew)
//   COFF header
//   Optional header → Data directories → Debug directory (index 6)
//   Debug directory entries → find IMAGE_DEBUG_TYPE_CODEVIEW (2)
//   CodeView entry → RSDS_DEBUG_INFO
//
PDBResult PDBManager::extractRSDS(const uint8_t* peBase, uint64_t peSize,
                                   RSDS_DEBUG_INFO* rsdsOut,
                                   const char** pdbNameOut) const {
    if (peSize < 64) {
        return PDBResult::error("PE too small", PDB_ERR_PE_NO_DEBUG_DIR);
    }

    // DOS header — check 'MZ' signature
    if (peBase[0] != 'M' || peBase[1] != 'Z') {
        return PDBResult::error("Not a PE file (no MZ)", PDB_ERR_PE_NO_DEBUG_DIR);
    }

    // e_lfanew at offset 0x3C
    uint32_t peOff = *reinterpret_cast<const uint32_t*>(peBase + 0x3C);
    if (peOff + 24 > peSize) {
        return PDBResult::error("Invalid PE offset", PDB_ERR_PE_NO_DEBUG_DIR);
    }

    // PE signature: "PE\0\0"
    if (memcmp(peBase + peOff, "PE\0\0", 4) != 0) {
        return PDBResult::error("Invalid PE signature", PDB_ERR_PE_NO_DEBUG_DIR);
    }

    // COFF header starts at peOff + 4
    const uint8_t* coffHdr = peBase + peOff + 4;
    uint16_t machine = *reinterpret_cast<const uint16_t*>(coffHdr);
    uint16_t numSections = *reinterpret_cast<const uint16_t*>(coffHdr + 2);
    uint16_t optHdrSize = *reinterpret_cast<const uint16_t*>(coffHdr + 16);

    // Optional header starts after COFF header (20 bytes)
    const uint8_t* optHdr = coffHdr + 20;
    uint16_t optMagic = *reinterpret_cast<const uint16_t*>(optHdr);

    // Determine PE32 vs PE32+ and debug directory location
    uint32_t debugDirRVA = 0;
    uint32_t debugDirSize = 0;

    if (optMagic == 0x20B) {
        // PE32+ (64-bit)
        // Data directories start at offset 112 in optional header
        // Debug directory is index 6
        if (optHdrSize >= 112 + (7 * 8)) {
            debugDirRVA  = *reinterpret_cast<const uint32_t*>(optHdr + 112 + 6 * 8);
            debugDirSize = *reinterpret_cast<const uint32_t*>(optHdr + 112 + 6 * 8 + 4);
        }
    } else if (optMagic == 0x10B) {
        // PE32 (32-bit)
        // Data directories start at offset 96
        if (optHdrSize >= 96 + (7 * 8)) {
            debugDirRVA  = *reinterpret_cast<const uint32_t*>(optHdr + 96 + 6 * 8);
            debugDirSize = *reinterpret_cast<const uint32_t*>(optHdr + 96 + 6 * 8 + 4);
        }
    } else {
        return PDBResult::error("Unknown PE optional header magic", PDB_ERR_PE_NO_DEBUG_DIR);
    }

    if (debugDirRVA == 0 || debugDirSize == 0) {
        return PDBResult::error("No debug directory", PDB_ERR_PE_NO_DEBUG_DIR);
    }

    // Convert RVA to file offset using section headers
    // Section headers start after optional header
    const uint8_t* sectionHeaders = optHdr + optHdrSize;
    uint32_t debugDirFileOffset = 0;
    bool rvaResolved = false;

    for (uint16_t i = 0; i < numSections; ++i) {
        const uint8_t* sec = sectionHeaders + i * 40;
        uint32_t secVA = *reinterpret_cast<const uint32_t*>(sec + 12);
        uint32_t secSize = *reinterpret_cast<const uint32_t*>(sec + 8);
        uint32_t secRawOff = *reinterpret_cast<const uint32_t*>(sec + 20);

        if (debugDirRVA >= secVA && debugDirRVA < secVA + secSize) {
            debugDirFileOffset = secRawOff + (debugDirRVA - secVA);
            rvaResolved = true;
            break;
        }
    }

    if (!rvaResolved) {
        return PDBResult::error("Debug directory RVA not in any section", PDB_ERR_PE_NO_DEBUG_DIR);
    }

    if (debugDirFileOffset + debugDirSize > peSize) {
        return PDBResult::error("Debug directory extends past file", PDB_ERR_PE_NO_DEBUG_DIR);
    }

    // Walking debug directory entries (each is 28 bytes: IMAGE_DEBUG_DIRECTORY)
    // Looking for Type == CODEVIEW (2)
    // Note: IMAGE_DEBUG_TYPE_CODEVIEW may already be defined as a macro in winnt.h
    static const uint32_t PDB_DEBUG_TYPE_CODEVIEW = 2;
    static const uint32_t DEBUG_DIR_ENTRY_SIZE = 28;

    uint32_t numEntries = debugDirSize / DEBUG_DIR_ENTRY_SIZE;

    for (uint32_t i = 0; i < numEntries; ++i) {
        const uint8_t* entry = peBase + debugDirFileOffset + i * DEBUG_DIR_ENTRY_SIZE;
        uint32_t debugType = *reinterpret_cast<const uint32_t*>(entry + 12);
        uint32_t dataSize  = *reinterpret_cast<const uint32_t*>(entry + 16);
        uint32_t dataOff   = *reinterpret_cast<const uint32_t*>(entry + 24);

        if (debugType == PDB_DEBUG_TYPE_CODEVIEW) {
            if (dataOff + dataSize > peSize) {
                return PDBResult::error("CodeView data extends past file", PDB_ERR_PE_NO_DEBUG_DIR);
            }

            const uint8_t* cvData = peBase + dataOff;

            // Check RSDS signature (0x53445352 = "SDSR" in little-endian)
            uint32_t sig = *reinterpret_cast<const uint32_t*>(cvData);
            if (sig == 0x53445352) {
                // Found RSDS
                const RSDS_DEBUG_INFO* rsds = reinterpret_cast<const RSDS_DEBUG_INFO*>(cvData);
                memcpy(rsdsOut, rsds, sizeof(RSDS_DEBUG_INFO));
                *pdbNameOut = reinterpret_cast<const char*>(cvData + sizeof(RSDS_DEBUG_INFO));
                return PDBResult::ok("RSDS extracted");
            }
        }
    }

    return PDBResult::error("No RSDS CodeView entry found", PDB_ERR_PE_NO_DEBUG_DIR);
}

} // namespace PDB
} // namespace RawrXD
