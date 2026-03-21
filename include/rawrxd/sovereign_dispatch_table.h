/* ============================================================================
 * sovereign_dispatch_table.h — Late-bind / displacement slots (pure C)
 * ============================================================================
 *
 * PURPOSE: Second "mechanical truth" map: where to patch and what relative or
 * absolute value to apply after final layout. This is a sibling view of
 * RawrSomMinimalFixup (name-based); here you may record *numeric* targets when
 * both ends live in one flat buffer (intra-blob backpatch before PE wrap).
 *
 * Use cases:
 *   - Same-buffer: patch_at and target_offset (pre-layout); compute REL32 as
 *     target_rva - (patch_rva + 4) after section RVA is known. In C++, validate
 *     the signed delta fits int32_t (±2 GiB informal) via sovereign_rel32_validate.hpp
 *     before patching.
 *   - Cross-buffer / multi-module: prefer sovereign_som_minimal.h fixups with
 *     target_name + global symbol resolution.
 *
 * TIER G: Global-use contract — docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md.
 * Tier P: MSVC link.exe for IDE product — §6/§7.
 * ============================================================================ */

#ifndef RAWRXD_SOVEREIGN_DISPATCH_TABLE_H
#define RAWRXD_SOVEREIGN_DISPATCH_TABLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum RawrDispatchKind {
    RAWR_DISPATCH_NONE = 0,
    /** RIP-relative 32-bit; patch 4 bytes at patch_offset */
    RAWR_DISPATCH_REL32 = 1,
    /** Absolute VA or image-relative 64-bit slot (emitter decides) */
    RAWR_DISPATCH_ABS64 = 2
};

/**
 * One late-binding site in a contiguous buffer (indices are byte offsets from
 * buffer base, not RVAs — convert after section base is fixed).
 */
typedef struct RawrDispatchSlot {
    uint32_t patch_offset;
    /** Meaning depends on kind: for REL32, often target byte offset in same blob */
    uint32_t target_offset;
    uint16_t kind; /* RawrDispatchKind */
    uint16_t reserved;
} RawrDispatchSlot;

typedef struct RawrDispatchTable {
    RawrDispatchSlot *slots;
    uint32_t slot_count;
} RawrDispatchTable;

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_SOVEREIGN_DISPATCH_TABLE_H */
