/*
 * rel32_validate.h — x86-64 REL32 displacement range (C99, phase2 linker)
 *
 * REL32 encodes a signed int32 delta from the patch site’s “next RIP”
 * (byte after the 4-byte displacement). Valid range is [INT32_MIN, INT32_MAX]
 * (informally ±2 GiB).
 */
#ifndef RAWRXD_PHASE2_REL32_VALIDATE_H
#define RAWRXD_PHASE2_REL32_VALIDATE_H

#include <limits.h>
#include <stdint.h>

static inline int rel32_delta_fits_i64(int64_t delta)
{
    return delta >= (int64_t)INT32_MIN && delta <= (int64_t)INT32_MAX;
}

#endif /* RAWRXD_PHASE2_REL32_VALIDATE_H */
