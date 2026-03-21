/*
 * som_minimal_usage.c — Lab stub: two RawrSomMinimalAtomicUnit blocks + REL32 math
 *
 * Not a linker; not a PE emitter. Proves the fixup contract from sovereign_som_minimal.h.
 * Production code uses Logger (C++); this C example uses fprintf(stderr, ...).
 *
 * Policy: docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md §6 / §7.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include "rawrxd/sovereign_som_minimal.h"

/* -------------------------------------------------------------------------- */
/* "Comment-logic": pretend two TUs are laid out contiguously in one section. */
/* C++ RawrXD code would use Logger::info(...) here; C example uses fprintf.   */
/* -------------------------------------------------------------------------- */

static void som_minimal_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("[SOM] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/*
 * Callee: single `ret` (0xC3). Placed at simulated RVA 0x1010.
 * Caller: `call rel32` (E8 + 4 placeholder bytes) + `ret` at simulated RVA 0x1000.
 *
 * x86-64 `call rel32` is 5 bytes: opcode E8 at R, then 4-byte displacement.
 * The CPU computes: next_rip = R + 5; target = next_rip + sign_extend(rel32).
 * So: rel32 = (int32_t)(target_rva - (call_opcode_rva + 5)).
 *
 * (If you instead label the *first byte of the 4-byte hole* as H, then
 *  rel32 = (int32_t)(target_rva - (H + 4)), which matches the common
 *  "TargetRVA - (SourceRVA + 4)" when SourceRVA = H.)
 */

#define SIM_CALLEE_RVA 0x1010u
#define SIM_CALLER_RVA 0x1000u

static uint8_t g_callee_bytes[] = {0xC3}; /* ret */

static uint8_t g_caller_bytes[] = {
    0xE8, 0x00, 0x00, 0x00, 0x00, /* call rel32 — hole at bytes [1..4] */
    0xC3                          /* ret */
};

static RawrSomMinimalAtomicUnit g_unit_callee;
static RawrSomMinimalAtomicUnit g_unit_caller;
static RawrSomMinimalFixup g_fixup_call_to_callee;

static void setup_units(void)
{
    memset(&g_unit_callee, 0, sizeof g_unit_callee);
    g_unit_callee.raw_bytes = g_callee_bytes;
    g_unit_callee.raw_size = (uint32_t)sizeof g_callee_bytes;
    g_unit_callee.fixups = NULL;
    g_unit_callee.fixup_count = 0;

    memset(&g_unit_caller, 0, sizeof g_unit_caller);
    g_unit_caller.raw_bytes = g_caller_bytes;
    g_unit_caller.raw_size = (uint32_t)sizeof g_caller_bytes;
    g_unit_caller.fixup_count = 1;
    g_unit_caller.fixups = &g_fixup_call_to_callee;

    memset(&g_fixup_call_to_callee, 0, sizeof g_fixup_call_to_callee);
    g_fixup_call_to_callee.patch_offset = 1u; /* first byte of rel32 after E8 */
    g_fixup_call_to_callee.target_name = "callee";
    g_fixup_call_to_callee.type = RAWR_SOM_RELOC_REL32;
}

int main(void)
{
    setup_units();

    som_minimal_log("Two RawrSomMinimalAtomicUnit blocks (callee: ret; caller: call+ret).");
    som_minimal_log("Simulated RVAs: caller .text @ 0x%X, callee @ 0x%X.", SIM_CALLER_RVA, SIM_CALLEE_RVA);

    /* Resolve "callee" -> SIM_CALLEE_RVA (global symbol map would do this). */
    const uint32_t target_rva = SIM_CALLEE_RVA;
    const uint32_t hole_first_byte_rva = SIM_CALLER_RVA + g_fixup_call_to_callee.patch_offset;

    /*
     * REL32 = TargetRVA - (HoleStartRVA + 4)
     * because next instruction after the 5-byte call is HoleStartRVA + 4... wait:
     * HoleStartRVA = SIM_CALLER_RVA + 1 = 0x1001; next after call = 0x1001 + 4 = 0x1005.
     * Actually next_rip = opcode_rva + 5 = 0x1000 + 5 = 0x1005.
     * rel32 = target_rva - next_rip = 0x1010 - 0x1005 = 0x0B.
     */
    const uint32_t call_opcode_rva = SIM_CALLER_RVA; /* E8 at 0x1000 */
    const uint32_t next_rip = call_opcode_rva + 5u;
    int32_t rel32 = (int32_t)((uint64_t)target_rva - (uint64_t)next_rip);

    som_minimal_log("REL32 patch math: (int32_t)(TargetRVA - (CallOpcodeRVA + 5))");
    som_minimal_log("  TargetRVA     = 0x%X", target_rva);
    som_minimal_log("  CallOpcodeRVA = 0x%X", call_opcode_rva);
    som_minimal_log("  NextRIP       = 0x%X  (after 5-byte call)", next_rip);
    som_minimal_log("  rel32         = %d (0x%X)", (int)rel32, (unsigned)(uint32_t)rel32);

    /* Equivalent form using hole start (first byte of the 4-byte field): */
    const uint32_t hole_start_rva = hole_first_byte_rva;
    int32_t rel32_alt = (int32_t)((uint64_t)target_rva - ((uint64_t)hole_start_rva + 4ull));
    som_minimal_log("Alternate (hole-based): TargetRVA - (HoleStartRVA + 4) => %d (must match).",
                    (int)rel32_alt);

    if (rel32 != rel32_alt)
    {
        som_minimal_log("ERROR: REL32 forms disagree.");
        return 1;
    }

    /* Apply patch into caller buffer (little-endian). */
    memcpy(g_caller_bytes + g_fixup_call_to_callee.patch_offset, &rel32, sizeof rel32);

    som_minimal_log("Patched caller bytes: E8 %02X %02X %02X %02X C3",
                    g_caller_bytes[1], g_caller_bytes[2], g_caller_bytes[3], g_caller_bytes[4]);

    (void)g_unit_callee;
    return 0;
}
