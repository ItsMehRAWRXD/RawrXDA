/* rawrxd_symbol_registry.c — Sovereign symbol registry implementation */

#include "rawrxd/rawrxd_symbol_registry.h"

#include <stdlib.h>
#include <string.h>

uint32_t rawrxd_fnv1a_hash32(const char *str)
{
    uint32_t hash = 2166136261u;
    if (str == NULL)
    {
        return 0u;
    }
    while (*str != '\0')
    {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

void rawrxd_symbol_registry_init_empty(RawrSymbolRegistry *reg)
{
    if (reg == NULL)
    {
        return;
    }
    reg->symbols = NULL;
    reg->capacity = 0u;
    reg->count = 0u;
    {
        uint32_t i;
        for (i = 0u; i < RAWR_SYM_HASH_SIZE; ++i)
        {
            reg->bucket_head[i] = RAWR_SYM_IDX_NONE;
        }
    }
}

int rawrxd_symbol_registry_init(RawrSymbolRegistry *reg, uint32_t initial_capacity)
{
    if (reg == NULL)
    {
        return -1;
    }
    rawrxd_symbol_registry_init_empty(reg);
    if (initial_capacity == 0u)
    {
        return 0;
    }
    reg->symbols = (RawrSymbol *)calloc(initial_capacity, sizeof(RawrSymbol));
    if (reg->symbols == NULL)
    {
        return -1;
    }
    reg->capacity = initial_capacity;
    {
        uint32_t i;
        for (i = 0u; i < RAWR_SYM_HASH_SIZE; ++i)
        {
            reg->bucket_head[i] = RAWR_SYM_IDX_NONE;
        }
    }
    return 0;
}

void rawrxd_symbol_registry_free(RawrSymbolRegistry *reg)
{
    if (reg == NULL)
    {
        return;
    }
    free(reg->symbols);
    rawrxd_symbol_registry_init_empty(reg);
}

void rawrxd_symbol_registry_clear(RawrSymbolRegistry *reg)
{
    if (reg == NULL)
    {
        return;
    }
    reg->count = 0u;
    {
        uint32_t i;
        for (i = 0u; i < RAWR_SYM_HASH_SIZE; ++i)
        {
            reg->bucket_head[i] = RAWR_SYM_IDX_NONE;
        }
    }
}

const RawrSymbol *rawrxd_symbol_registry_get(const RawrSymbolRegistry *reg, uint32_t index)
{
    if (reg == NULL || reg->symbols == NULL || index >= reg->count)
    {
        return NULL;
    }
    return &reg->symbols[index];
}

uint32_t rawrxd_symbol_registry_find(const RawrSymbolRegistry *reg, const char *name)
{
    if (reg == NULL || name == NULL || reg->symbols == NULL || reg->count == 0u)
    {
        return RAWR_SYM_IDX_NONE;
    }

    const uint32_t h = rawrxd_fnv1a_hash32(name);
    const uint32_t bucket = h & (RAWR_SYM_HASH_SIZE - 1u);
    uint32_t idx = reg->bucket_head[bucket];
    while (idx != RAWR_SYM_IDX_NONE)
    {
        const RawrSymbol *s = &reg->symbols[idx];
        if (s->name != NULL && strcmp(s->name, name) == 0)
        {
            return idx;
        }
        idx = s->next;
    }
    return RAWR_SYM_IDX_NONE;
}

uint32_t rawrxd_symbol_registry_insert(RawrSymbolRegistry *reg, const char *name, uint32_t section_idx,
                                       uint32_t offset, uint8_t is_weak, uint8_t is_defined)
{
    if (reg == NULL || name == NULL)
    {
        return RAWR_SYM_IDX_NONE;
    }
    if (rawrxd_symbol_registry_find(reg, name) != RAWR_SYM_IDX_NONE)
    {
        return RAWR_SYM_IDX_NONE;
    }
    if (reg->symbols == NULL || reg->count >= reg->capacity)
    {
        return RAWR_SYM_IDX_NONE;
    }

    const uint32_t h = rawrxd_fnv1a_hash32(name);
    const uint32_t bucket = h & (RAWR_SYM_HASH_SIZE - 1u);
    const uint32_t idx = reg->count++;

    RawrSymbol *s = &reg->symbols[idx];
    s->name = name;
    s->hash = h;
    s->section_idx = section_idx;
    s->offset = offset;
    s->is_weak = is_weak;
    s->is_defined = is_defined;
    s->reserved[0] = 0;
    s->reserved[1] = 0;
    s->next = reg->bucket_head[bucket];
    reg->bucket_head[bucket] = idx;

    return idx;
}
