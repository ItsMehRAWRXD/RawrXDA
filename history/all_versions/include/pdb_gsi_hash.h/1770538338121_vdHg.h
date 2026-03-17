// ============================================================================
// pdb_gsi_hash.h — Phase 29.2: GSI Hash Table for O(1) PDB Symbol Lookup
// ============================================================================
//
// The Global Symbol Index (GSI) hash table is the PDB's primary acceleration
// structure for name-based symbol resolution. Microsoft's implementation uses
// a bucket-chain hash (IPHR_HASH = 4096 buckets) over the public symbol
// stream. This replaces the Phase 29 v1 linear scan with O(1) amortized
// lookups.
//
// GSI on-disk layout (in the public symbol stream, or a separate GSI stream):
//
//   struct GSIHashHeader {
//       uint32_t verSignature;   // 0xFFFFFFFF for new-style hash
//       uint32_t verHdr;         // GSI_HASH_V70 = 0xF12F091A
//       uint32_t hrSize;         // Size of hash record array (bytes)
//       uint32_t numBuckets;     // Number of bucket entries (4096)
//   };
//
//   GSIHashRecord hashRecords[hrSize / sizeof(GSIHashRecord)];
//   uint32_t      bucketBitmap[bitmap_words];  // Compressed bitmap
//   uint32_t      bucketOffsets[popcount(bitmap)]; // Offsets into hashRecords
//
//   struct GSIHashRecord {
//       uint32_t symbolOffset;   // Offset into symbol record stream
//       uint32_t cref;           // Reference count (unused by us)
//   };
//
// Hash function: Microsoft uses a case-insensitive hash truncated to
// IPHR_HASH (4096) buckets. The algorithm iterates each character,
// applying:
//   hash = (hash * 0x1003F) + tolower(c)
//   hash %= IPHR_HASH
//
// For the MASM kernel, we implement a stripped version that operates on
// already-lowercase names (the caller lowercases before the call).
//
// ============================================================================
#pragma once

// Forward declarations — avoid circular include with pdb_native.h
// pdb_gsi_hash.h is included BY pdb_native.h consumers (not the other way)
#include <cstdint>
#include <cstddef>

// We need PDBResult and PDBErrorCode from pdb_native.h.
// Since pdb_native.h will include us AFTER its own definitions, consumers
// always have both headers. But for standalone compilation of pdb_gsi_hash.cpp,
// we include pdb_native.h there.
namespace RawrXD { namespace PDB {
    struct PDBResult;       // Forward declare — defined in pdb_native.h
} }

namespace RawrXD {
namespace PDB {

// ============================================================================
// Constants
// ============================================================================

static constexpr uint32_t IPHR_HASH              = 4096;
static constexpr uint32_t GSI_HASH_SIGNATURE      = 0xFFFFFFFF;
static constexpr uint32_t GSI_HASH_V70            = 0xF12F091A;
static constexpr uint32_t GSI_HASH_RECORD_SIZE    = 8;      // sizeof(GSIHashRecord)
static constexpr uint32_t GSI_BUCKET_BITMAP_WORDS = (IPHR_HASH + 31) / 32;

// ============================================================================
// On-Disk Structures
// ============================================================================

#pragma pack(push, 1)

struct GSIHashHeader {
    uint32_t verSignature;     // 0xFFFFFFFF
    uint32_t verHdr;           // 0xF12F091A
    uint32_t hrSize;           // Total bytes of hash records
    uint32_t numBuckets;       // Bucket count (typically 4096)
};

struct GSIHashRecord {
    uint32_t symbolOffset;     // Byte offset into the symbol record stream
    uint32_t cref;             // Reference count (always 1 for publics)
};

#pragma pack(pop)

// ============================================================================
// GSIHashTable — In-memory representation of the parsed GSI hash table
// ============================================================================
//
// After parsing, each bucket points to a range of GSIHashRecords.
// Lookup: hash the name → get bucket index → walk the bucket's records →
// for each record, read the symbol name from the symbol stream and compare.
//
// This turns the Phase 29 v1 O(n) linear scan into O(1) amortized lookup
// with O(k) worst-case per bucket (k = chain length, typically 1-3).
//

class GSIHashTable {
public:
    GSIHashTable();
    ~GSIHashTable();

    // ---- Parse from raw stream data ----
    // streamData: the GSI/publics hash stream (typically DBI header's
    //             publicSymStream or globalSymStream)
    // streamSize: total bytes of the hash stream
    // symbolStream: the symbol record stream (for name verification)
    // symbolStreamSize: total bytes of the symbol record stream
    PDBResult parse(const uint8_t* streamData, uint32_t streamSize,
                    const uint8_t* symbolStream, uint32_t symbolStreamSize);

    // ---- O(1) Lookup by name ----
    // Returns the offset into the symbol record stream where the matching
    // CVPubSym32 resides, or UINT32_MAX if not found.
    // The caller can then cast: auto* pub = (CVPubSym32*)(symStream + offset + 4)
    // to get the full public symbol record.
    uint32_t findSymbolOffset(const char* name, uint32_t nameLen) const;

    // ---- Hash function (matches Microsoft's PDB hash) ----
    static uint32_t hashName(const char* name, uint32_t nameLen);

    // ---- Statistics ----
    uint32_t getRecordCount() const    { return m_numRecords; }
    uint32_t getUsedBuckets() const    { return m_usedBuckets; }
    float    getLoadFactor() const;
    bool     isValid() const           { return m_valid; }

    // ---- Enumeration ----
    // Walk all hash records in a bucket.
    // Visitor receives (symbolOffset, index). Return false to stop.
    typedef bool (*BucketVisitor)(uint32_t symbolOffset, uint32_t index, void* userData);
    void walkBucket(uint32_t bucketIndex, BucketVisitor visitor, void* userData) const;

private:
    // ---- Bucket storage ----
    // Each bucket is a (start, count) range into m_records.
    struct Bucket {
        uint32_t startIndex;   // Index into m_records array
        uint32_t count;        // Number of records in this bucket
    };

    Bucket       m_buckets[IPHR_HASH];    // 4096 buckets
    GSIHashRecord* m_records;             // All hash records (owned)
    uint32_t     m_numRecords;
    uint32_t     m_usedBuckets;

    // Symbol stream reference (NOT owned — caller must keep alive)
    const uint8_t* m_symbolStream;
    uint32_t       m_symbolStreamSize;

    bool m_valid;

    // ---- Helpers ----
    bool verifySymbolName(uint32_t symbolOffset, const char* name, uint32_t nameLen) const;
    static uint32_t popcount32(uint32_t v);
};

// ============================================================================
// TPI/IPI Type Stream Structures (Phase 29.2)
// ============================================================================
//
// The Type Information (TPI, stream 2) and ID Information (IPI, stream 4)
// streams hold CodeView type records. Parsing these enables richer symbol
// metadata: function signatures, parameter types, struct layouts, etc.
//
// TPI Stream Layout:
//   TPIStreamHeader          (56 bytes)
//   TypeRecord[typeIndexMax - typeIndexMin]
//   Hash data (optional)
//
// We parse enough to resolve function signatures for the hover tooltip.
//

#pragma pack(push, 1)

struct TPIStreamHeader {
    uint32_t version;          // Always 20040203
    uint32_t headerSize;       // sizeof(TPIStreamHeader)
    uint32_t typeIndexMin;     // Usually 0x1000
    uint32_t typeIndexMax;     // First TI not in this stream
    uint32_t typeRecordBytes;  // Size of all type records combined
    // Hash info (for fast lookups)
    uint16_t hashStreamIndex;  // Stream index of hash aux data
    uint16_t hashAuxStreamIndex;
    uint32_t hashKeySize;      // Size of hash key
    uint32_t numHashBuckets;
    // Hash value buffer offset / size
    int32_t  hashValueBufferOffset;
    uint32_t hashValueBufferLength;
    // Index offset buffer
    int32_t  indexOffsetBufferOffset;
    uint32_t indexOffsetBufferLength;
    // Hash adj buffer
    int32_t  hashAdjBufferOffset;
    uint32_t hashAdjBufferLength;
};

// Common CV type leaf kinds we care about
enum CVTypeKind : uint16_t {
    LF_POINTER      = 0x1002,
    LF_PROCEDURE    = 0x1008,
    LF_MFUNCTION    = 0x1009,
    LF_ARGLIST      = 0x1201,
    LF_FIELDLIST    = 0x1203,
    LF_ARRAY        = 0x1503,
    LF_CLASS        = 0x1504,
    LF_STRUCTURE    = 0x1505,
    LF_UNION        = 0x1506,
    LF_ENUM         = 0x1507,
    LF_MODIFIER     = 0x1001,
    LF_MEMBER       = 0x150D,
    LF_NESTTYPE     = 0x1510,
    LF_FUNC_ID      = 0x1601,
    LF_MFUNC_ID     = 0x1602,
    LF_BUILDINFO    = 0x1603,
    LF_SUBSTR_LIST  = 0x1604,
    LF_STRING_ID    = 0x1605,
    LF_UDT_SRC_LINE = 0x1606,
};

// LF_PROCEDURE (0x1008) — function type
struct CVTypeProcedure {
    uint32_t returnType;   // Type index of return type
    uint8_t  callConv;     // Calling convention (CV_call_e)
    uint8_t  funcAttr;     // Function attributes
    uint16_t paramCount;   // Number of parameters
    uint32_t argListType;  // Type index of argument list
};

// LF_ARGLIST (0x1201) — function argument type list
struct CVTypeArgList {
    uint32_t count;        // Number of entries
    // Followed by count * uint32_t typeIndices
};

// Calling conventions
enum CVCallConv : uint8_t {
    CV_CALL_NEAR_C       = 0x00,
    CV_CALL_NEAR_FAST    = 0x04,
    CV_CALL_NEAR_STD     = 0x07,
    CV_CALL_NEAR_SYS     = 0x09,
    CV_CALL_THISCALL     = 0x0B,
    CV_CALL_CLRCALL      = 0x16,
};

#pragma pack(pop)

// ============================================================================
// TPIStreamParser — Read type records from TPI/IPI streams
// ============================================================================

class TPIStreamParser {
public:
    TPIStreamParser();
    ~TPIStreamParser();

    // Parse from raw stream data
    PDBResult parse(const uint8_t* streamData, uint32_t streamSize);

    // Get type record by type index (absolute TI, e.g. 0x1000+)
    // Returns pointer into the parsed data, or nullptr.
    const uint8_t* getTypeRecord(uint32_t typeIndex, uint16_t* leafKindOut,
                                  uint32_t* recordSizeOut) const;

    // Convenience: resolve a procedure type to signature string
    // Writes e.g. "NTSTATUS __stdcall(HANDLE, POBJECT_ATTRIBUTES, ...)"
    PDBResult formatProcedureType(uint32_t typeIndex, char* out, uint32_t maxLen) const;

    // Stats
    uint32_t getTypeIndexMin() const { return m_tiMin; }
    uint32_t getTypeIndexMax() const { return m_tiMax; }
    uint32_t getTypeCount() const    { return m_tiMax - m_tiMin; }
    bool     isValid() const         { return m_valid; }

private:
    // We store an array of offsets: m_offsets[ti - m_tiMin] = byte offset
    // into m_data where that type record begins.
    uint32_t* m_offsets;       // Offset table (owned)
    uint8_t*  m_data;          // Copy of type record data (owned)
    uint32_t  m_dataSize;
    uint32_t  m_tiMin;
    uint32_t  m_tiMax;
    bool      m_valid;

    const char* callConvName(uint8_t cc) const;
};

// ============================================================================
// MASM Kernel Exports — Phase 29.2 Additions
// ============================================================================
//
// PDB_GSIHashLookup: MASM-accelerated GSI hash probe.
// Computes the Microsoft PDB hash and walks the bucket chain entirely
// in registers, avoiding C++ overhead for the hot path.
//
// PDB_HashName: Compute the hash of a symbol name (IPHR_HASH buckets).
//
// C++ fallback implementations are provided when RAWR_HAS_MASM is false.
//

#ifdef __cplusplus
extern "C" {
#endif

// Compute PDB hash for a symbol name.
// Returns: hash value in range [0, IPHR_HASH)
uint32_t PDB_HashName(const char* name, uint32_t nameLen);

// Search a GSI bucket for a matching symbol name.
// records:     Pointer to array of GSIHashRecord for this bucket
// numRecords:  Number of records in the bucket
// symbolStream: Base of the symbol record stream
// name:        Symbol name to find
// nameLen:     Length of name (not including null)
// Returns: symbolOffset of the match, or 0xFFFFFFFF if not found
uint32_t PDB_GSIHashLookup(
    const void*  records,
    uint32_t     numRecords,
    const void*  symbolStream,
    const char*  name,
    uint32_t     nameLen
);

#ifdef __cplusplus
}
#endif

} // namespace PDB
} // namespace RawrXD
