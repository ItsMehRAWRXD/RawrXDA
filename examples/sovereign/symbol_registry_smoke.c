/*
 * symbol_registry_smoke.c — Tier G: FNV-1a RawrSymbolRegistry insert/find/get
 *
 * Build (repo root):
 *   gcc -std=c99 -Wall -I include examples/sovereign/symbol_registry_smoke.c \
 *       toolchain/sovereign_minimal/rawrxd_symbol_registry.c -o build/symbol_registry_smoke
 */

#include <stdio.h>
#include <stdint.h>

#include "rawrxd/rawrxd_symbol_registry.h"

static const char s_name_main[] = "main";
static const char s_name_foo[] = "foo";

int main(void)
{
    RawrSymbolRegistry reg;
    if (rawrxd_symbol_registry_init(&reg, 16u) != 0)
    {
        fprintf(stderr, "[registry] init failed\n");
        return 1;
    }

    const uint32_t h_expect = rawrxd_fnv1a_hash32("foo");

    if (rawrxd_symbol_registry_insert(&reg, s_name_main, 0u, 0u, 0u, 1u) == RAWR_SYM_IDX_NONE)
    {
        fprintf(stderr, "[registry] insert main failed\n");
        rawrxd_symbol_registry_free(&reg);
        return 1;
    }
    if (rawrxd_symbol_registry_insert(&reg, s_name_foo, 0u, 0x2Au, 0u, 1u) == RAWR_SYM_IDX_NONE)
    {
        fprintf(stderr, "[registry] insert foo failed\n");
        rawrxd_symbol_registry_free(&reg);
        return 1;
    }

    const uint32_t idx = rawrxd_symbol_registry_find(&reg, "foo");
    if (idx == RAWR_SYM_IDX_NONE)
    {
        fprintf(stderr, "[registry] find foo failed\n");
        rawrxd_symbol_registry_free(&reg);
        return 1;
    }

    const RawrSymbol *s = rawrxd_symbol_registry_get(&reg, idx);
    if (s == NULL || s->hash != h_expect || s->offset != 0x2Au)
    {
        fprintf(stderr, "[registry] get mismatch\n");
        rawrxd_symbol_registry_free(&reg);
        return 1;
    }

    fprintf(stderr, "[registry] OK idx=%u hash=0x%X section=%u offset=0x%X defined=%u\n", idx, s->hash,
            s->section_idx, s->offset, (unsigned)s->is_defined);

    rawrxd_symbol_registry_free(&reg);
    return 0;
}
