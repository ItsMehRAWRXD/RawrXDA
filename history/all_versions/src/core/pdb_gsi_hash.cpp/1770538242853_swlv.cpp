// ============================================================================
// pdb_gsi_hash.cpp — Phase 29.2: GSI Hash Table + TPI Type Parser
// ============================================================================
//
// Implements:
//   1. GSIHashTable — O(1) public symbol lookup via Microsoft's GSI hash
//   2. TPIStreamParser — Type record resolver for function signatures
//   3. C++ fallback for PDB_HashName and PDB_GSIHashLookup (MinGW builds)
//
// This replaces the Phase 29 v1 linear-scan placeholder in pdb_native.cpp
// (NativePDBParser::parsePublicSymbolStream) with the real GSI hash table.
//
// Copyright (c) RawrXD Project. No exceptions. No STL allocators in hot path.
// ============================================================================

#include "pdb_gsi_hash.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <new>

#ifdef _WIN32
#include <windows.h>
#else
#include <strings.h>
#endif

namespace RawrXD {
namespace PDB {

// ============================================================================
// C++ Fallback — MASM Kernel Stubs (when RAWR_HAS_MASM is not defined)
// ============================================================================
//
// These implement the exact same algorithm as the MASM kernel in
// RawrXD_PDBKernel.asm but in portable C++. On MSVC builds with
// RAWR_HAS_MASM=1, these are unused — the linker resolves to the
// .asm object instead.
//

#ifndef RAWR_HAS_MASM

extern "C" uint32_t PDB_HashName(const char* name, uint32_t nameLen) {
    // Microsoft PDB hash algorithm (case-insensitive):
    //   hash = 0
    //   for each char c in name:
    //       hash += tolower(c)
    //       hash += (hash << 10)
    //       hash ^= (hash >> 6)
    //   hash += (hash << 3)
    //   hash ^= (hash >> 11)
    //   hash += (hash << 15)
    //   return hash % IPHR_HASH
    //
    // This matches the hashSz() function used by Microsoft's PDB library.

    if (!name || nameLen == 0) return 0;

    uint32_t hash = 0;

    for (uint32_t i = 0; i < nameLen; ++i) {
        uint8_t c = static_cast<uint8_t>(name[i]);
        // Case-insensitive: lowercase ASCII letters
        if (c >= 'A' && c <= 'Z') c += 32;

        hash += c;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash % IPHR_HASH;
}

extern "C" uint32_t PDB_GSIHashLookup(
    const void*  records,
    uint32_t     numRecords,
    const void*  symbolStream,
    const char*  name,
    uint32_t     nameLen)
{
    if (!records || !symbolStream || !name || nameLen == 0) return 0xFFFFFFFF;

    const GSIHashRecord* recs = static_cast<const GSIHashRecord*>(records);
    const uint8_t* symStream = static_cast<const uint8_t*>(symbolStream);

    for (uint32_t i = 0; i < numRecords; ++i) {
        uint32_t symOff = recs[i].symbolOffset;

        // At symOff in the symbol stream: CVRecordHeader (recLen:2, recTyp:2)
        // For S_PUB32: flags(4) + off(4) + seg(2) = 10 bytes after recTyp
        // Name starts at symOff + 2 (recLen) + 2 (recTyp) + 4 (flags) + 4 (off) + 2 (seg) = symOff + 14
        // But recLen field is at symOff, recTyp at symOff+2

        // Read recTyp
        uint16_t recTyp = *reinterpret_cast<const uint16_t*>(symStream + symOff + 2);
        if (recTyp != 0x110E) continue; // S_PUB32 = 0x110E

        // Name at offset 14 from record start
        const char* recName = reinterpret_cast<const char*>(symStream + symOff + 14);

        // Compare: case-insensitive, exact length match
        bool match = true;
        for (uint32_t j = 0; j < nameLen; ++j) {
            char a = recName[j];
            char b = name[j];
            // tolower
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) { match = false; break; }
        }

        if (match && recName[nameLen] == '\0') {
            return symOff;
        }
    }

    return 0xFFFFFFFF;
}

#endif // !RAWR_HAS_MASM


// ============================================================================
// GSIHashTable — Constructor / Destructor
// ============================================================================

GSIHashTable::GSIHashTable()
    : m_records(nullptr)
    , m_numRecords(0)
    , m_usedBuckets(0)
    , m_symbolStream(nullptr)
    , m_symbolStreamSize(0)
    , m_valid(false)
{
    memset(m_buckets, 0, sizeof(m_buckets));
}

GSIHashTable::~GSIHashTable() {
    if (m_records) {
        free(m_records);
        m_records = nullptr;
    }
}

// ============================================================================
// GSIHashTable::popcount32 — Population count (number of set bits)
// ============================================================================

uint32_t GSIHashTable::popcount32(uint32_t v) {
    // Kernighan's bit counting
    uint32_t count = 0;
    while (v) {
        v &= v - 1;
        count++;
    }
    return count;
}

// ============================================================================
// GSIHashTable::parse — Parse GSI hash stream from raw bytes
// ============================================================================
//
// Stream layout:
//   GSIHashHeader        (16 bytes)
//   GSIHashRecord[N]     (hrSize bytes)
//   uint32_t bitmap[128] (4096 bits = 512 bytes)
//   uint32_t offsets[M]  (4 * popcount(bitmap) bytes)
//
// The bitmap indicates which of the 4096 buckets have records.
// offsets[i] gives the byte offset into the hash record array where
// bucket i's records begin (for the i-th SET bit in the bitmap).
//

PDBResult GSIHashTable::parse(const uint8_t* streamData, uint32_t streamSize,
                               const uint8_t* symbolStream, uint32_t symbolStreamSize)
{
    if (!streamData || streamSize < sizeof(GSIHashHeader)) {
        return PDBResult::error("GSI stream too small", PDB_ERR_PARSE_FAIL);
    }
    if (!symbolStream || symbolStreamSize == 0) {
        return PDBResult::error("Null symbol stream", PDB_ERR_PARSE_FAIL);
    }

    // Store symbol stream reference
    m_symbolStream = symbolStream;
    m_symbolStreamSize = symbolStreamSize;

    // ---- Read Header ----
    const GSIHashHeader* hdr = reinterpret_cast<const GSIHashHeader*>(streamData);

    // Validate signature/version
    if (hdr->verSignature != GSI_HASH_SIGNATURE || hdr->verHdr != GSI_HASH_V70) {
        // Some PDBs use an older format without the new-style header.
        // Fall back to treating the entire stream as flat hash records.
        // Phase 29.3: handle legacy GSI format
        return PDBResult::error("Unsupported GSI hash version", PDB_ERR_PARSE_FAIL);
    }

    uint32_t hrSize = hdr->hrSize;
    if (hrSize == 0 || hrSize % GSI_HASH_RECORD_SIZE != 0) {
        return PDBResult::error("Invalid GSI hash record size", PDB_ERR_PARSE_FAIL);
    }

    m_numRecords = hrSize / GSI_HASH_RECORD_SIZE;

    // ---- Validate stream has enough data ----
    // Header(16) + Records(hrSize) + Bitmap(512) + Offsets(variable)
    uint32_t afterRecords = sizeof(GSIHashHeader) + hrSize;
    uint32_t bitmapSize = GSI_BUCKET_BITMAP_WORDS * sizeof(uint32_t); // 128 * 4 = 512
    if (afterRecords + bitmapSize > streamSize) {
        return PDBResult::error("GSI stream truncated (no bitmap)", PDB_ERR_PARSE_FAIL);
    }

    // ---- Copy hash records ----
    m_records = static_cast<GSIHashRecord*>(malloc(hrSize));
    if (!m_records) {
        return PDBResult::error("GSI hash record allocation failed", PDB_ERR_ALLOC_FAIL);
    }
    memcpy(m_records, streamData + sizeof(GSIHashHeader), hrSize);

    // ---- Parse bitmap ----
    const uint32_t* bitmap = reinterpret_cast<const uint32_t*>(
        streamData + afterRecords);

    // Count used buckets (total set bits in bitmap)
    m_usedBuckets = 0;
    for (uint32_t w = 0; w < GSI_BUCKET_BITMAP_WORDS; ++w) {
        m_usedBuckets += popcount32(bitmap[w]);
    }

    // ---- Parse bucket offsets ----
    uint32_t offsetsStart = afterRecords + bitmapSize;
    uint32_t offsetsSize = m_usedBuckets * sizeof(uint32_t);
    if (offsetsStart + offsetsSize > streamSize) {
        // Some PDBs don't have the full offset table. Degrade gracefully.
        // We can still use the bitmap to determine which buckets are populated
        // but can't know the exact boundaries. Fall back to linear assignment.
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "GSI offset table truncated (expected %u bytes, have %u)",
                 offsetsSize, streamSize - offsetsStart);
        OutputDebugStringA(msg);
        // Continue with degraded mode
        offsetsSize = 0;
    }

    const uint32_t* offsets = nullptr;
    if (offsetsSize > 0) {
        offsets = reinterpret_cast<const uint32_t*>(streamData + offsetsStart);
    }

    // ---- Build bucket ranges ----
    // Zero all buckets first
    for (uint32_t b = 0; b < IPHR_HASH; ++b) {
        m_buckets[b].startIndex = 0;
        m_buckets[b].count = 0;
    }

    if (offsets && m_usedBuckets > 0) {
        // Walk bitmap, assign offsets to buckets
        uint32_t offsetIdx = 0;

        for (uint32_t b = 0; b < IPHR_HASH; ++b) {
            uint32_t word = bitmap[b / 32];
            uint32_t bit  = b % 32;

            if (word & (1u << bit)) {
                // This bucket is populated
                uint32_t startByte = offsets[offsetIdx];
                uint32_t startRec  = startByte / GSI_HASH_RECORD_SIZE;

                // End = start of next populated bucket, or end of records
                uint32_t endRec;
                if (offsetIdx + 1 < m_usedBuckets) {
                    endRec = offsets[offsetIdx + 1] / GSI_HASH_RECORD_SIZE;
                } else {
                    endRec = m_numRecords;
                }

                m_buckets[b].startIndex = startRec;
                m_buckets[b].count = (endRec > startRec) ? (endRec - startRec) : 0;

                offsetIdx++;
            }
        }
    } else if (m_usedBuckets > 0) {
        // Degraded mode: we know which buckets are populated but not
        // their exact record ranges. Rebuild by hashing all records.
        // This is O(n) one-time work but avoids the linear scan per lookup.

        // First pass: count records per bucket
        uint32_t bucketCounts[IPHR_HASH];
        memset(bucketCounts, 0, sizeof(bucketCounts));

        for (uint32_t i = 0; i < m_numRecords; ++i) {
            uint32_t symOff = m_records[i].symbolOffset;
            // Read name from symbol stream at PUB32_NAME_OFFSET (14)
            if (symOff + 14 >= m_symbolStreamSize) continue;

            const char* recName = reinterpret_cast<const char*>(
                m_symbolStream + symOff + 14);
            uint32_t nameLen = static_cast<uint32_t>(
                strnlen(recName, m_symbolStreamSize - (symOff + 14)));

            uint32_t bucket = PDB_HashName(recName, nameLen);
            if (bucket < IPHR_HASH) bucketCounts[bucket]++;
        }

        // Assign start indices
        uint32_t acc = 0;
        for (uint32_t b = 0; b < IPHR_HASH; ++b) {
            m_buckets[b].startIndex = acc;
            m_buckets[b].count = bucketCounts[b];
            acc += bucketCounts[b];
        }

        // Second pass: scatter records into bucket order
        GSIHashRecord* sorted = static_cast<GSIHashRecord*>(malloc(m_numRecords * sizeof(GSIHashRecord)));
        if (!sorted) {
            return PDBResult::error("Sorted record allocation failed", PDB_ERR_ALLOC_FAIL);
        }

        // Reset counts for scatter
        uint32_t bucketPos[IPHR_HASH];
        for (uint32_t b = 0; b < IPHR_HASH; ++b) {
            bucketPos[b] = m_buckets[b].startIndex;
        }

        for (uint32_t i = 0; i < m_numRecords; ++i) {
            uint32_t symOff = m_records[i].symbolOffset;
            if (symOff + 14 >= m_symbolStreamSize) continue;

            const char* recName = reinterpret_cast<const char*>(
                m_symbolStream + symOff + 14);
            uint32_t nameLen = static_cast<uint32_t>(
                strnlen(recName, m_symbolStreamSize - (symOff + 14)));

            uint32_t bucket = PDB_HashName(recName, nameLen);
            if (bucket < IPHR_HASH && bucketPos[bucket] < m_numRecords) {
                sorted[bucketPos[bucket]++] = m_records[i];
            }
        }

        // Replace records with sorted version
        free(m_records);
        m_records = sorted;
    }

    m_valid = true;

    char msg[256];
    snprintf(msg, sizeof(msg),
             "[Phase 29.2] GSI hash table parsed: %u records, %u/%u buckets used (load %.2f)",
             m_numRecords, m_usedBuckets, IPHR_HASH, getLoadFactor());
    OutputDebugStringA(msg);

    return PDBResult::ok("GSI hash table parsed");
}

// ============================================================================
// GSIHashTable::hashName — Microsoft PDB hash function
// ============================================================================

uint32_t GSIHashTable::hashName(const char* name, uint32_t nameLen) {
    return PDB_HashName(name, nameLen);
}

// ============================================================================
// GSIHashTable::findSymbolOffset — O(1) amortized lookup
// ============================================================================

uint32_t GSIHashTable::findSymbolOffset(const char* name, uint32_t nameLen) const {
    if (!m_valid || !name || nameLen == 0) return UINT32_MAX;

    uint32_t bucket = PDB_HashName(name, nameLen);
    if (bucket >= IPHR_HASH) return UINT32_MAX;

    const Bucket& b = m_buckets[bucket];
    if (b.count == 0) return UINT32_MAX;

    // Use MASM kernel for bucket chain walk
    return PDB_GSIHashLookup(
        m_records + b.startIndex,
        b.count,
        m_symbolStream,
        name,
        nameLen
    );
}

// ============================================================================
// GSIHashTable::verifySymbolName — Compare name at symbol offset
// ============================================================================

bool GSIHashTable::verifySymbolName(uint32_t symbolOffset, const char* name,
                                     uint32_t nameLen) const
{
    if (symbolOffset + 14 + nameLen >= m_symbolStreamSize) return false;

    const char* recName = reinterpret_cast<const char*>(
        m_symbolStream + symbolOffset + 14);

    // Case-insensitive comparison
    for (uint32_t i = 0; i < nameLen; ++i) {
        char a = recName[i];
        char b = name[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return false;
    }

    return (recName[nameLen] == '\0');
}

// ============================================================================
// GSIHashTable::getLoadFactor
// ============================================================================

float GSIHashTable::getLoadFactor() const {
    if (m_numRecords == 0) return 0.0f;
    return static_cast<float>(m_numRecords) / static_cast<float>(IPHR_HASH);
}

// ============================================================================
// GSIHashTable::walkBucket — Enumerate all records in a specific bucket
// ============================================================================

void GSIHashTable::walkBucket(uint32_t bucketIndex, BucketVisitor visitor,
                               void* userData) const
{
    if (!m_valid || bucketIndex >= IPHR_HASH || !visitor) return;

    const Bucket& b = m_buckets[bucketIndex];
    for (uint32_t i = 0; i < b.count; ++i) {
        uint32_t idx = b.startIndex + i;
        if (idx >= m_numRecords) break;
        if (!visitor(m_records[idx].symbolOffset, i, userData)) break;
    }
}

// ============================================================================
// TPIStreamParser — Constructor / Destructor
// ============================================================================

TPIStreamParser::TPIStreamParser()
    : m_offsets(nullptr)
    , m_data(nullptr)
    , m_dataSize(0)
    , m_tiMin(0)
    , m_tiMax(0)
    , m_valid(false)
{
}

TPIStreamParser::~TPIStreamParser() {
    if (m_offsets) { free(m_offsets); m_offsets = nullptr; }
    if (m_data) { free(m_data); m_data = nullptr; }
}

// ============================================================================
// TPIStreamParser::parse — Parse TPI stream from raw bytes
// ============================================================================
//
// Stream layout:
//   TPIStreamHeader           (56 bytes)
//   TypeRecord[0..N-1]        (typeRecordBytes total)
//
// Each TypeRecord is:
//   uint16_t recLen;           // Length of data after this field
//   uint16_t leafKind;         // LF_* type
//   uint8_t  data[recLen-2];   // Leaf-specific data
//
// We build an offset table so getTypeRecord(ti) is O(1).
//

PDBResult TPIStreamParser::parse(const uint8_t* streamData, uint32_t streamSize) {
    if (!streamData || streamSize < sizeof(TPIStreamHeader)) {
        return PDBResult::error("TPI stream too small", PDB_ERR_PARSE_FAIL);
    }

    const TPIStreamHeader* hdr = reinterpret_cast<const TPIStreamHeader*>(streamData);

    // Validate version
    if (hdr->version != 20040203) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Unsupported TPI version: %u (expected 20040203)",
                 hdr->version);
        return PDBResult::error(msg, PDB_ERR_PARSE_FAIL);
    }

    m_tiMin = hdr->typeIndexMin;
    m_tiMax = hdr->typeIndexMax;
    uint32_t typeCount = m_tiMax - m_tiMin;
    uint32_t typeRecordBytes = hdr->typeRecordBytes;

    // Validate sizes
    uint32_t dataStart = hdr->headerSize;
    if (dataStart + typeRecordBytes > streamSize) {
        return PDBResult::error("TPI type record data extends past stream", PDB_ERR_PARSE_FAIL);
    }

    if (typeCount == 0) {
        m_valid = true;
        return PDBResult::ok("TPI stream empty (no types)");
    }

    // ---- Allocate offset table ----
    m_offsets = static_cast<uint32_t*>(calloc(typeCount, sizeof(uint32_t)));
    if (!m_offsets) {
        return PDBResult::error("TPI offset table allocation failed", PDB_ERR_ALLOC_FAIL);
    }

    // ---- Copy type record data ----
    m_dataSize = typeRecordBytes;
    m_data = static_cast<uint8_t*>(malloc(m_dataSize));
    if (!m_data) {
        free(m_offsets);
        m_offsets = nullptr;
        return PDBResult::error("TPI data allocation failed", PDB_ERR_ALLOC_FAIL);
    }
    memcpy(m_data, streamData + dataStart, m_dataSize);

    // ---- Walk type records and build offset table ----
    uint32_t offset = 0;
    uint32_t ti = 0;

    while (offset + 4 <= m_dataSize && ti < typeCount) {
        uint16_t recLen = *reinterpret_cast<const uint16_t*>(m_data + offset);

        // Store offset of this type record (including the recLen field)
        m_offsets[ti] = offset;

        // Advance: recLen doesn't include itself (2 bytes)
        uint32_t totalRecSize = static_cast<uint32_t>(recLen) + 2;

        // Align to 4-byte boundary
        totalRecSize = (totalRecSize + 3) & ~3u;

        offset += totalRecSize;
        ti++;
    }

    // Fill remaining entries with invalid offset if records were truncated
    for (; ti < typeCount; ++ti) {
        m_offsets[ti] = UINT32_MAX;
    }

    m_valid = true;

    char msg[256];
    snprintf(msg, sizeof(msg),
             "[Phase 29.2] TPI stream parsed: %u types [0x%X..0x%X), %u bytes",
             typeCount, m_tiMin, m_tiMax, m_dataSize);
    OutputDebugStringA(msg);

    return PDBResult::ok("TPI stream parsed");
}

// ============================================================================
// TPIStreamParser::getTypeRecord — O(1) type record access by index
// ============================================================================

const uint8_t* TPIStreamParser::getTypeRecord(uint32_t typeIndex,
                                                uint16_t* leafKindOut,
                                                uint32_t* recordSizeOut) const
{
    if (!m_valid || typeIndex < m_tiMin || typeIndex >= m_tiMax) return nullptr;

    uint32_t idx = typeIndex - m_tiMin;
    uint32_t offset = m_offsets[idx];
    if (offset == UINT32_MAX || offset + 4 > m_dataSize) return nullptr;

    uint16_t recLen = *reinterpret_cast<const uint16_t*>(m_data + offset);
    uint16_t leafKind = *reinterpret_cast<const uint16_t*>(m_data + offset + 2);

    if (leafKindOut) *leafKindOut = leafKind;
    if (recordSizeOut) *recordSizeOut = recLen;

    // Return pointer to data AFTER recLen and leafKind (the leaf-specific data)
    return m_data + offset + 4;
}

// ============================================================================
// TPIStreamParser::callConvName — Human-readable calling convention name
// ============================================================================

const char* TPIStreamParser::callConvName(uint8_t cc) const {
    switch (cc) {
        case CV_CALL_NEAR_C:    return "__cdecl";
        case CV_CALL_NEAR_FAST: return "__fastcall";
        case CV_CALL_NEAR_STD:  return "__stdcall";
        case CV_CALL_NEAR_SYS:  return "__syscall";
        case CV_CALL_THISCALL:  return "__thiscall";
        case CV_CALL_CLRCALL:   return "__clrcall";
        default:                return "__unknown";
    }
}

// ============================================================================
// TPIStreamParser::formatProcedureType — Build human-readable signature
// ============================================================================
//
// Given a type index pointing to LF_PROCEDURE, produce a string like:
//   "NTSTATUS __stdcall(HANDLE, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, ...)"
//
// For Phase 29.2, we emit the calling convention and parameter count.
// Full type name resolution (mapping TI to string) requires the names
// hash or IPI stream, which is Phase 29.3.
//

PDBResult TPIStreamParser::formatProcedureType(uint32_t typeIndex,
                                                char* out, uint32_t maxLen) const
{
    if (!out || maxLen < 32) {
        return PDBResult::error("Buffer too small", PDB_ERR_PARSE_FAIL);
    }

    uint16_t leafKind = 0;
    uint32_t recSize = 0;
    const uint8_t* data = getTypeRecord(typeIndex, &leafKind, &recSize);
    if (!data) {
        snprintf(out, maxLen, "<type 0x%X not found>", typeIndex);
        return PDBResult::error("Type not found", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    if (leafKind != LF_PROCEDURE && leafKind != LF_MFUNCTION) {
        snprintf(out, maxLen, "<type 0x%X is not a procedure (leaf=0x%04X)>",
                 typeIndex, leafKind);
        return PDBResult::error("Not a procedure type", PDB_ERR_PARSE_FAIL);
    }

    if (recSize < sizeof(CVTypeProcedure)) {
        snprintf(out, maxLen, "<truncated procedure type>");
        return PDBResult::error("Truncated procedure record", PDB_ERR_PARSE_FAIL);
    }

    const CVTypeProcedure* proc = reinterpret_cast<const CVTypeProcedure*>(data);

    // Build: "returnTI callConv(paramCount params)"
    const char* cc = callConvName(proc->callConv);

    // Try to get argument list
    uint32_t argCount = proc->paramCount;
    uint16_t argLeaf = 0;
    const uint8_t* argData = getTypeRecord(proc->argListType, &argLeaf, nullptr);

    if (argData && argLeaf == LF_ARGLIST) {
        const CVTypeArgList* args = reinterpret_cast<const CVTypeArgList*>(argData);
        argCount = args->count; // More reliable than proc->paramCount

        // Build parameter list with type indices
        int written = snprintf(out, maxLen, "TI:0x%X %s(", proc->returnType, cc);
        if (written < 0 || static_cast<uint32_t>(written) >= maxLen) {
            return PDBResult::ok("Signature truncated");
        }

        const uint32_t* argTypes = reinterpret_cast<const uint32_t*>(argData + 4);
        for (uint32_t i = 0; i < argCount && static_cast<uint32_t>(written) < maxLen - 16; ++i) {
            if (i > 0) {
                written += snprintf(out + written, maxLen - written, ", ");
            }
            written += snprintf(out + written, maxLen - written, "TI:0x%X", argTypes[i]);
        }
        snprintf(out + written, maxLen - written, ")");
    } else {
        snprintf(out, maxLen, "TI:0x%X %s(%u params)", proc->returnType, cc, argCount);
    }

    return PDBResult::ok("Procedure type formatted");
}

} // namespace PDB
} // namespace RawrXD
