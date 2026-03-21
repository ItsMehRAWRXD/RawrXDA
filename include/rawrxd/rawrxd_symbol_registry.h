/* ============================================================================
 * rawrxd_symbol_registry.h — RAWRXD Sovereign Symbol Registry (Tier G, pure C)
 * ============================================================================
 *
 * From-scratch symbol table: FNV-1a hash, separate chaining, O(1) average
 * lookup. Intended for linkers and for feeding reachability / DCE passes
 * (see docs/SOVEREIGN_SYMBOL_REGISTRY_DCE.md and sovereign_global_dce.hpp).
 *
 * Dependencies: C99 + string.h (strcmp) + stdlib.h (malloc/free) for dynamic
 * pool. Hash itself is dependency-free.
 *
 * Tier G — docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md. Tier P IDE: MSVC — §6/§7.
 * ============================================================================ */

#ifndef RAWRXD_SYMBOL_REGISTRY_H
#define RAWRXD_SYMBOL_REGISTRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Must be a power of two (bucket index = hash & (size - 1)). */
#define RAWR_SYM_HASH_SIZE 4096u
#define RAWR_SYM_IDX_NONE 0xFFFFFFFFu

typedef struct RawrSymbol {
    const char *name;
    uint32_t hash;
    uint32_t section_idx;
    uint32_t offset;
    uint8_t is_weak;    /**< 1 = COMDAT / weak; linker may drop duplicate defs */
    uint8_t is_defined; /**< 0 = undefined / import-only until resolved */
    uint8_t reserved[2];
    uint32_t next; /**< Next symbol index in bucket chain, or RAWR_SYM_IDX_NONE */
} RawrSymbol;

typedef struct RawrSymbolRegistry {
    RawrSymbol *symbols;
    uint32_t capacity;
    uint32_t count;
    uint32_t bucket_head[RAWR_SYM_HASH_SIZE];
} RawrSymbolRegistry;

/** FNV-1a 32-bit — "mechanical truth" string hash (low collision, fast). */
uint32_t rawrxd_fnv1a_hash32(const char *str);

/** Zero registry; does not allocate. */
void rawrxd_symbol_registry_init_empty(RawrSymbolRegistry *reg);

/**
 * Allocate @p initial_capacity symbol slots. Returns 0 on success.
 * Fails if @p reg is NULL or malloc fails.
 */
int rawrxd_symbol_registry_init(RawrSymbolRegistry *reg, uint32_t initial_capacity);

void rawrxd_symbol_registry_free(RawrSymbolRegistry *reg);

/** Clear all symbols; keeps capacity. */
void rawrxd_symbol_registry_clear(RawrSymbolRegistry *reg);

/**
 * Insert a symbol. @p name must remain valid for registry lifetime.
 * Returns new symbol index, or RAWR_SYM_IDX_NONE on failure / duplicate name.
 */
uint32_t rawrxd_symbol_registry_insert(RawrSymbolRegistry *reg, const char *name, uint32_t section_idx,
                                     uint32_t offset, uint8_t is_weak, uint8_t is_defined);

/** Find symbol index by name, or RAWR_SYM_IDX_NONE. */
uint32_t rawrxd_symbol_registry_find(const RawrSymbolRegistry *reg, const char *name);

const RawrSymbol *rawrxd_symbol_registry_get(const RawrSymbolRegistry *reg, uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_SYMBOL_REGISTRY_H */
