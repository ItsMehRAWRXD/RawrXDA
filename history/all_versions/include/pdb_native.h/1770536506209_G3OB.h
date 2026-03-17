// ============================================================================
// pdb_native.h — Phase 29: Native PDB Symbol Server (From-Scratch MSF v7.00)
// ============================================================================
//
// PURPOSE:
//   Zero-dependency PDB/MSF parser. No dbghelp.dll, no DIA SDK.
//   Replaces Microsoft debug interfaces entirely.
//
// ARCHITECTURE:
//   Memory-mapped MSF v7.00 multi-stream file parsing.
//   MASM64-accelerated symbol lookup (Phase 29.2).
//   WinHTTP-based symbol server download (reuses existing ModelConnection pattern).
//   Content-addressed local cache (%LOCALAPPDATA%\RawrXD\Symbols\).
//
// SCOPE (Phase 29 v1):
//   ✅ MSF v7.00 superblock + stream directory parsing
//   ✅ DBI stream (Debug Information) — section contributions, module info
//   ✅ Public symbols stream — S_PUB32 records
//   ✅ Global symbols stream — S_GPROC32, S_LPROC32, S_GDATA32, S_LDATA32
//   ✅ RSDS CodeView debug directory matching (GUID + Age from PE)
//   ✅ Microsoft Symbol Server download (symsrv wire protocol)
//   ✅ Local content-addressed cache (WinDbg/VS compatible layout)
//   ✅ RVA ↔ symbol name resolution
//   ❌ Full type system (TPI/IPI) — Phase 29.2
//   ❌ Line number tables — Phase 29.2
//   ❌ Source file mapping — Phase 29.2
//
// INTEGRATION:
//   RE Suite:  Disassembly → symbol annotation, import → binding
//   LSP:       textDocument/definition, textDocument/hover for external symbols
//   MonacoCore: Ctrl+Click on call targets resolves through PDB
//
// PATTERN:   PatchResult-compatible, no exceptions
// THREADING: All mmap/parse on caller thread. HTTP downloads on dedicated thread.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <cstddef>

namespace RawrXD {
namespace PDB {

// ============================================================================
// PDB Result (PatchResult-compatible)
// ============================================================================
struct PDBResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static PDBResult ok(const char* msg = "Success") {
        return { true, msg, 0 };
    }
    static PDBResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// Error codes
enum PDBErrorCode : int {
    PDB_OK                  = 0,
    PDB_ERR_FILE_OPEN       = 1,
    PDB_ERR_FILE_MAP        = 2,
    PDB_ERR_INVALID_MAGIC   = 3,
    PDB_ERR_INVALID_FORMAT  = 4,
    PDB_ERR_STREAM_OOB      = 5,
    PDB_ERR_SYMBOL_NOT_FOUND = 6,
    PDB_ERR_DOWNLOAD_FAIL   = 7,
    PDB_ERR_CACHE_WRITE     = 8,
    PDB_ERR_PE_NO_DEBUG_DIR = 9,
    PDB_ERR_NOT_LOADED      = 10,
    PDB_ERR_CORRUPT_RECORD  = 11,
    PDB_ERR_ALLOC_FAIL      = 12,
};

// ============================================================================
// MSF v7.00 On-Disk Structures (packed, read directly from mmap)
// ============================================================================
// Reference: https://llvm.org/docs/PDB/MsfFile.html
//
// MSF file layout:
//   [SuperBlock]  — page 0
//   [FPM1]        — page 1
//   [FPM2]        — page 2
//   [Data pages]  — pages 3..N
//
// The SuperBlock points to the stream directory, which lists all streams.
// Each stream is stored as a sequence of non-contiguous pages (blocks).

#pragma pack(push, 1)

// MSF SuperBlock — always at page 0
struct MSFSuperBlock {
    char        magic[32];          // "Microsoft C/C++ MSF 7.00\r\n\x1ADS\0\0\0"
    uint32_t    blockSize;          // Page size: 512, 1024, 2048, or 4096
    uint32_t    freeBlockMapBlock;  // Block index of active FPM (1 or 2)
    uint32_t    numBlocks;          // Total blocks in file
    uint32_t    numDirectoryBytes;  // Size of stream directory in bytes
    uint32_t    unknown;            // Always 0
    uint32_t    blockMapAddr;       // Block index of the stream directory map
};
static_assert(sizeof(MSFSuperBlock) == 56, "MSFSuperBlock must be 56 bytes");

// Expected magic bytes (first 32 bytes of any PDB)
// Defined in pdb_native.cpp to avoid initializer-string length issues
extern const char MSF_MAGIC[32];

#pragma pack(pop)

// ============================================================================
// Well-Known Stream Indices
// ============================================================================
enum MSFStreamIndex : uint32_t {
    STREAM_OLD_DIRECTORY = 0,   // Old MSF directory (unused in v7)
    STREAM_PDB           = 1,   // PDB info: GUID, age, named streams
    STREAM_TPI           = 2,   // Type info (LF_* records)
    STREAM_DBI           = 3,   // Debug info (modules, section contributions)
    STREAM_IPI           = 4,   // ID info (item ID records)
};

// ============================================================================
// CodeView Symbol Record Types (subset for Phase 29 v1)
// ============================================================================
enum CVSymbolType : uint16_t {
    S_PUB32          = 0x110E,  // Public symbol (exported name + RVA)
    S_GPROC32        = 0x1110,  // Global procedure start
    S_GPROC32_ID     = 0x1112,  // Global procedure start (ID variant)
    S_LPROC32        = 0x1114,  // Local procedure start
    S_LPROC32_ID     = 0x1116,  // Local procedure start (ID variant)
    S_LDATA32        = 0x110C,  // Local data
    S_GDATA32        = 0x110D,  // Global data
    S_PROCREF        = 0x1125,  // Procedure reference
    S_LPROCREF       = 0x1126,  // Local procedure reference
    S_THUNK32        = 0x1102,  // Thunk
    S_BLOCK32        = 0x1103,  // Block start
    S_END            = 0x0006,  // End of scope
    S_CONSTANT       = 0x1107,  // Constant
    S_UDT            = 0x1108,  // User-defined type
    S_SECTION        = 0x1136,  // PE section
    S_COFFGROUP      = 0x1137,  // COFF group
    S_EXPORT         = 0x1138,  // Export
    S_COMPILE3       = 0x113C,  // Compiler info v3
    S_ENVBLOCK       = 0x113D,  // Environment block
};

// ============================================================================
// CodeView On-Disk Symbol Record Structures (packed)
// ============================================================================
#pragma pack(push, 1)

// Common header for all CV symbol records
struct CVRecordHeader {
    uint16_t    recLen;     // Record length (excludes this 2-byte field)
    uint16_t    recTyp;     // CVSymbolType
};

// S_PUB32 — Public symbol
struct CVPubSym32 {
    uint16_t    recLen;
    uint16_t    recTyp;         // S_PUB32 = 0x110E
    uint32_t    pubSymFlags;    // Bit 0: code, Bit 1: function, Bit 2: managed
    uint32_t    off;            // Offset within section
    uint16_t    seg;            // Section index (1-based)
    // char     name[];         // Null-terminated name follows
};

// S_GPROC32 / S_LPROC32 — Procedure symbol
struct CVProcSym32 {
    uint16_t    recLen;
    uint16_t    recTyp;         // S_GPROC32 or S_LPROC32
    uint32_t    pParent;        // Offset to parent
    uint32_t    pEnd;           // Offset to S_END
    uint32_t    pNext;          // Offset to next symbol
    uint32_t    len;            // Procedure length in bytes
    uint32_t    dbgStart;       // Debug start offset
    uint32_t    dbgEnd;         // Debug end offset
    uint32_t    typind;         // Type index
    uint32_t    off;            // Offset within section
    uint16_t    seg;            // Section index (1-based)
    uint8_t     flags;          // CV_PROCFLAGS
    // char     name[];         // Null-terminated name follows
};

// S_GDATA32 / S_LDATA32 — Data symbol
struct CVDataSym32 {
    uint16_t    recLen;
    uint16_t    recTyp;         // S_GDATA32 or S_LDATA32
    uint32_t    typind;         // Type index
    uint32_t    off;            // Offset within section
    uint16_t    seg;            // Section index (1-based)
    // char     name[];         // Null-terminated name follows
};

// S_SECTION — PE section descriptor
struct CVSectionSym {
    uint16_t    recLen;
    uint16_t    recTyp;         // S_SECTION
    uint16_t    isec;           // Section index
    uint8_t     align;          // Alignment
    uint8_t     reserved;
    uint32_t    rva;            // Section RVA
    uint32_t    cb;             // Section size
    uint32_t    characteristics;
    // char     name[];         // Null-terminated section name
};

#pragma pack(pop)

// ============================================================================
// DBI Header (Debug Information stream header)
// ============================================================================
#pragma pack(push, 1)

struct DBIHeader {
    int32_t     versionSignature;   // -1
    uint32_t    versionHeader;      // 19990903 = V70
    uint32_t    age;                // PDB age (matches PE debug dir)
    uint16_t    globalStreamIndex;  // Stream # for global symbols
    uint16_t    buildNumber;
    uint16_t    publicStreamIndex;  // Stream # for public symbols
    uint16_t    pdbDllVersion;
    uint16_t    symRecordStream;    // Stream # for symbol records
    uint16_t    pdbDllRbld;
    int32_t     modInfoSize;        // Size of module info substream
    int32_t     sectionContribSize; // Size of section contribution substream
    int32_t     sectionMapSize;     // Size of section map substream
    int32_t     sourceInfoSize;     // Size of source info substream
    int32_t     typeServerMapSize;
    uint32_t    mfcTypeServerIndex;
    int32_t     optionalDbgHeaderSize; // Size of optional debug header substream
    int32_t     ecSubstreamSize;
    uint16_t    flags;
    uint16_t    machine;            // IMAGE_FILE_MACHINE_*
    uint32_t    padding;
};

// DBI Section Contribution Entry (v1)
struct DBISectionContrib {
    uint16_t    isect;          // Section index (1-based)
    uint16_t    padding1;
    int32_t     off;            // Offset within section
    int32_t     cb;             // Size
    uint32_t    characteristics;
    uint16_t    imod;           // Module index
    uint16_t    padding2;
    uint32_t    dataCrc;
    uint32_t    relocCrc;
};

// DBI Optional Debug Header (array of stream indices)
// Indexed by: FPO=0, Exception=1, Fixup=2, OmapToSrc=3, OmapFromSrc=4,
//             SectionHdr=5, TokenRidMap=6, Xdata=7, Pdata=8, NewFPO=9, SectionHdrOrig=10
enum DBIOptionalDbgIndex : int {
    DBI_DBG_FPO              = 0,
    DBI_DBG_EXCEPTION        = 1,
    DBI_DBG_FIXUP            = 2,
    DBI_DBG_OMAP_TO_SRC      = 3,
    DBI_DBG_OMAP_FROM_SRC    = 4,
    DBI_DBG_SECTION_HDR      = 5,
    DBI_DBG_TOKEN_RID_MAP    = 6,
    DBI_DBG_XDATA            = 7,
    DBI_DBG_PDATA            = 8,
    DBI_DBG_NEW_FPO          = 9,
    DBI_DBG_SECTION_HDR_ORIG = 10,
};

#pragma pack(pop)

// ============================================================================
// PDB Info Stream Header (Stream 1)
// ============================================================================
#pragma pack(push, 1)

struct PDBInfoHeader {
    uint32_t    version;        // 20000404 = VC70
    uint32_t    signature;      // Seconds since 1970-01-01 (build timestamp)
    uint32_t    age;            // PDB age (incremented on each build)
    uint8_t     guid[16];       // Unique identifier (matches RSDS in PE)
};

#pragma pack(pop)

// ============================================================================
// RSDS CodeView Debug Directory (found in PE .debug section)
// ============================================================================
#pragma pack(push, 1)

struct RSDS_DEBUG_INFO {
    uint32_t    signature;      // 'SDSR' (0x53445352)
    uint8_t     guid[16];       // GUID matching PDB info stream
    uint32_t    age;            // Age matching PDB info stream
    // char     pdbPath[];      // Null-terminated path to PDB
};

#pragma pack(pop)

// ============================================================================
// PE Section Header (for RVA → offset translation)
// ============================================================================
#pragma pack(push, 1)

struct PDBSectionHeader {
    uint32_t    virtualSize;
    uint32_t    virtualAddress;     // Section RVA
    uint32_t    sizeOfRawData;
    uint32_t    pointerToRawData;
    uint32_t    pointerToRelocations;
    uint32_t    pointerToLinenumbers;
    uint16_t    numberOfRelocations;
    uint16_t    numberOfLinenumbers;
    uint32_t    characteristics;
};

#pragma pack(pop)

// ============================================================================
// Resolved Symbol (output from PDB queries)
// ============================================================================
struct ResolvedSymbol {
    const char*     name;           // Symbol name (points into mapped PDB — stable while loaded)
    uint32_t        nameLen;        // Name length (not including null terminator)
    uint64_t        rva;            // Relative virtual address
    uint32_t        size;           // Symbol size (procedure length, or 0 for data)
    uint16_t        section;        // PE section index (1-based)
    uint32_t        sectionOffset;  // Offset within section
    CVSymbolType    type;           // Symbol record type
    bool            isFunction;     // true if S_GPROC32 / S_LPROC32 / S_PUB32(function)
    bool            isPublic;       // true if from public symbol stream
};

// ============================================================================
// Symbol Server Configuration
// ============================================================================
struct SymbolServerConfig {
    const wchar_t*  serverUrl;          // Default: L"https://msdl.microsoft.com/download/symbols"
    const wchar_t*  cachePath;          // Default: L"%LOCALAPPDATA%\\RawrXD\\Symbols"
    uint32_t        timeoutMs;          // HTTP timeout (default: 30000)
    uint32_t        maxRetries;         // Download retries (default: 3)
    bool            enableCompression;  // Accept gzip/cab (default: true)
    bool            enabled;            // Master toggle (default: true)
};

// ============================================================================
// Download Progress Callback
// ============================================================================
typedef void (*PDBDownloadProgressCallback)(
    const char* fileName,       // e.g. "ntdll.pdb"
    uint64_t    bytesReceived,
    uint64_t    totalBytes,     // 0 if unknown
    void*       userData
);

// ============================================================================
// NativePDBParser — Core MSF v7.00 Parser (Single PDB Instance)
// ============================================================================
//
// Lifecycle: load() → [query] → unload()
// Threading: Not thread-safe. Caller must synchronize.
// Memory:    File is memory-mapped read-only. No copies.
//
class NativePDBParser {
public:
    NativePDBParser();
    ~NativePDBParser();

    // Non-copyable (owns mmap handles)
    NativePDBParser(const NativePDBParser&)             = delete;
    NativePDBParser& operator=(const NativePDBParser&)  = delete;

    // ---- Lifecycle ----
    PDBResult   load(const wchar_t* pdbPath);
    void        unload();
    bool        isLoaded() const { return m_pBase != nullptr; }

    // ---- Identity ----
    // Returns the GUID and age from Stream 1 (PDB info)
    PDBResult   getGuid(uint8_t guidOut[16]) const;
    uint32_t    getAge() const;

    // ---- Symbol Lookup ----
    // Find symbol by name. Returns RVA in *rvaOut. O(n) scan — Phase 29.2 adds hash table.
    PDBResult   findPublicSymbol(const char* name, uint64_t* rvaOut) const;

    // Find symbol by RVA. Returns closest symbol at or before the RVA.
    PDBResult   findSymbolByRVA(uint64_t rva, ResolvedSymbol* symOut) const;

    // Enumerate all public symbols. Calls visitor for each.
    // Return false from visitor to stop iteration.
    typedef bool (*SymbolVisitor)(const ResolvedSymbol* sym, void* userData);
    PDBResult   enumeratePublicSymbols(SymbolVisitor visitor, void* userData) const;

    // Enumerate all procedure symbols (global + local).
    PDBResult   enumerateProcedures(SymbolVisitor visitor, void* userData) const;

    // ---- Stream Access (low-level) ----
    uint32_t    getStreamCount() const { return m_numStreams; }
    uint32_t    getStreamSize(uint32_t streamIndex) const;
    PDBResult   readStream(uint32_t streamIndex, uint32_t offset,
                           void* buffer, uint32_t size) const;

    // ---- Section Headers (from DBI optional debug header) ----
    uint32_t    getSectionCount() const { return m_numSections; }
    const PDBSectionHeader* getSectionHeader(uint32_t index) const;

    // ---- RVA Translation ----
    uint64_t    sectionOffsetToRVA(uint16_t section, uint32_t offset) const;
    bool        rvaToSectionOffset(uint64_t rva, uint16_t* section, uint32_t* offset) const;

private:
    // ---- MSF Parsing ----
    PDBResult   parseStreamDirectory();
    PDBResult   parsePDBInfoStream();
    PDBResult   parseDBIStream();
    PDBResult   parseSectionHeaders();
    PDBResult   parsePublicSymbolStream();

    // ---- Stream Page Access ----
    // Returns pointer to byte at (streamIndex, offset), or nullptr if out of bounds.
    // Note: Streams are non-contiguous on disk — this does page-walking.
    const uint8_t*  getStreamByte(uint32_t streamIndex, uint32_t offset) const;

    // Copy a contiguous range from a (possibly fragmented) stream into a linear buffer.
    bool            readStreamRange(uint32_t streamIndex, uint32_t offset,
                                    uint8_t* dest, uint32_t size) const;

    // ---- File Mapping ----
    HANDLE          m_hFile         = INVALID_HANDLE_VALUE;
    HANDLE          m_hMapping      = nullptr;
    const uint8_t*  m_pBase         = nullptr;  // Memory-mapped file base
    uint64_t        m_fileSize      = 0;

    // ---- Superblock ----
    const MSFSuperBlock* m_superBlock = nullptr;
    uint32_t        m_blockSize     = 0;

    // ---- Stream Directory ----
    uint32_t        m_numStreams     = 0;
    uint32_t*       m_streamSizes    = nullptr;  // Array [numStreams]
    uint32_t**      m_streamPages    = nullptr;  // Array of block arrays [numStreams][...]
    // Note: m_streamSizes and m_streamPages are heap-allocated during parseStreamDirectory().

    // ---- PDB Info (Stream 1) ----
    uint8_t         m_guid[16]      = {};
    uint32_t        m_age           = 0;
    uint32_t        m_pdbVersion    = 0;

    // ---- DBI (Stream 3) ----
    uint16_t        m_globalSymStream   = 0xFFFF;
    uint16_t        m_publicSymStream   = 0xFFFF;
    uint16_t        m_symRecordStream   = 0xFFFF;
    uint16_t        m_sectionHdrStream  = 0xFFFF;
    uint16_t        m_machine           = 0;

    // ---- Section Headers ----
    PDBSectionHeader* m_sectionHeaders  = nullptr;  // Heap-allocated array
    uint32_t          m_numSections     = 0;
};

// ============================================================================
// PDBSymbolServer — HTTP download + content-addressed cache
// ============================================================================
//
// Cache Layout (WinDbg/VS compatible):
//   {cachePath}/{pdbName}/{GUID_AGE}/{pdbName}
//
// Example:
//   C:\Users\X\AppData\Local\RawrXD\Symbols\ntdll.pdb\1234ABCD5678EF901/ntdll.pdb
//
// Download URL:
//   https://msdl.microsoft.com/download/symbols/{pdbName}/{GUID_AGE}/{pdbName}
//
class PDBSymbolServer {
public:
    PDBSymbolServer();
    ~PDBSymbolServer();

    PDBSymbolServer(const PDBSymbolServer&)             = delete;
    PDBSymbolServer& operator=(const PDBSymbolServer&)  = delete;

    // ---- Configuration ----
    PDBResult   configure(const SymbolServerConfig& config);
    const SymbolServerConfig& getConfig() const { return m_config; }

    // ---- Cache Lookup ----
    // Check if a PDB is already in the local cache.
    // Returns the full path in pathOut (must be at least MAX_PATH wchar_t).
    PDBResult   findInCache(const char* pdbName, const uint8_t guid[16],
                            uint32_t age, wchar_t* pathOut, uint32_t pathMaxChars) const;

    // ---- Download ----
    // Download a PDB from the symbol server.
    // guidAge is the concatenated hex string: "{GUID_hex}{Age_hex}"
    PDBResult   download(const char* pdbName, const uint8_t guid[16], uint32_t age,
                         wchar_t* pathOut, uint32_t pathMaxChars);

    // ---- Progress ----
    void setProgressCallback(PDBDownloadProgressCallback fn, void* userData);

    // ---- Cache Management ----
    PDBResult   clearCache();
    uint64_t    getCacheSizeBytes() const;

private:
    // ---- Helpers ----
    void        buildGuidAgeString(const uint8_t guid[16], uint32_t age,
                                   char* out, uint32_t maxLen) const;
    void        buildCachePath(const char* pdbName, const char* guidAge,
                               wchar_t* out, uint32_t maxChars) const;
    void        buildDownloadUrl(const char* pdbName, const char* guidAge,
                                 wchar_t* out, uint32_t maxChars) const;
    PDBResult   ensureCacheDirectory(const wchar_t* dirPath) const;
    PDBResult   downloadFile(const wchar_t* url, const wchar_t* destPath);

    SymbolServerConfig          m_config;
    PDBDownloadProgressCallback m_progressFn    = nullptr;
    void*                       m_progressData  = nullptr;
};

// ============================================================================
// PDBManager — Singleton coordinator (Phase 29 entry point)
// ============================================================================
//
// The PDBManager is the top-level interface for the rest of RawrXD.
// It owns the symbol server, cache, and loaded PDB instances.
//
// Pattern: Follows Phase 25/28 singleton pattern.
//
// Usage from RE Suite / LSP:
//   auto& pdb = PDBManager::instance();
//   pdb.configure(config);
//   pdb.loadForModule("ntdll.dll", peBase, peSize);
//   auto sym = pdb.resolveRVA(rva);
//
class PDBManager {
public:
    static PDBManager& instance();

    // ---- Configuration ----
    PDBResult   configure(const SymbolServerConfig& config);

    // ---- Module Loading ----
    // Load PDB for a PE module by reading its debug directory.
    // The PE must be memory-mapped at peBase with peSize bytes.
    PDBResult   loadForModule(const char* moduleName,
                              const uint8_t* peBase, uint64_t peSize);

    // Load PDB from explicit path.
    PDBResult   loadPDB(const char* moduleName, const wchar_t* pdbPath);

    // Unload a previously loaded PDB.
    void        unloadModule(const char* moduleName);

    // ---- Symbol Resolution ----
    // Resolve a symbol name to an RVA (searches all loaded PDBs).
    PDBResult   resolveSymbol(const char* name, uint64_t* rvaOut,
                              const char** moduleOut) const;

    // Resolve an RVA within a specific module.
    PDBResult   resolveRVA(const char* moduleName, uint64_t rva,
                           ResolvedSymbol* symOut) const;

    // ---- Enumeration ----
    // Get the parser for a loaded module (for direct stream access).
    const NativePDBParser* getParser(const char* moduleName) const;

    // Get list of loaded module names.
    uint32_t    getLoadedModuleCount() const;
    const char* getLoadedModuleName(uint32_t index) const;

    // ---- Symbol Server ----
    PDBSymbolServer& getSymbolServer() { return m_symbolServer; }

    // ---- Statistics ----
    struct Stats {
        uint32_t    modulesLoaded;
        uint64_t    symbolsIndexed;
        uint64_t    lookupCount;
        uint64_t    cacheHits;
        uint64_t    cacheMisses;
        uint64_t    downloadCount;
        uint64_t    downloadBytes;
    };
    Stats       getStats() const;

private:
    PDBManager();
    ~PDBManager();
    PDBManager(const PDBManager&)               = delete;
    PDBManager& operator=(const PDBManager&)    = delete;

    // ---- PE Parsing Helpers ----
    PDBResult   extractRSDS(const uint8_t* peBase, uint64_t peSize,
                            RSDS_DEBUG_INFO* rsdsOut, const char** pdbNameOut) const;

    // ---- Internal Storage ----
    // Module name → parser mapping (max 64 simultaneously loaded PDBs)
    static const uint32_t MAX_LOADED_MODULES = 64;
    struct LoadedModule {
        char                moduleName[260];
        NativePDBParser*    parser;
        bool                active;
    };
    LoadedModule            m_modules[MAX_LOADED_MODULES];
    uint32_t                m_moduleCount = 0;

    PDBSymbolServer         m_symbolServer;
    mutable Stats           m_stats;
};

// ============================================================================
// ASM Kernel Exports (Phase 29.2 — MASM64 accelerated)
// ============================================================================
// These are implemented in RawrXD_PDBKernel.asm and called from the C++
// parser for performance-critical operations.
// On non-MSVC builds (MinGW), C++ fallback implementations are used instead.
#ifdef RAWR_HAS_MASM
extern "C" {
#endif
    // Validate MSF superblock magic (32-byte compare, returns 1 on match)
    uint32_t PDB_ValidateMagic(const void* superBlock);

    // Scan public symbol records for a name match.
    // Returns offset within stream, or 0xFFFFFFFF if not found.
    // Uses SIMD string comparison for batch scanning.
    uint32_t PDB_ScanPublics(const void* streamData, uint32_t streamSize,
                              const char* name, uint32_t nameLen);

    // Build page list for a stream from the stream directory.
    // Writes block numbers to pagesOut. Returns page count.
    uint32_t PDB_BuildPageList(const void* directory, uint32_t streamIndex,
                                uint32_t numStreams, const uint32_t* streamSizes,
                                uint32_t* pagesOut, uint32_t maxPages);

    // Compute GUID string from 16-byte GUID for cache path construction.
    // Writes hex string to out (32 chars + null). Returns length.
    uint32_t PDB_GuidToHex(const uint8_t guid[16], char* out, uint32_t maxLen);
#ifdef RAWR_HAS_MASM
}
#endif

} // namespace PDB
} // namespace RawrXD
