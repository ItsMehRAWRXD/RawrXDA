// =============================================================================
// rawrxd/sovereign_toolchain_layers.hpp — Ingest / Validate / Optimize / Emit
// =============================================================================
// Non-owning **taxonomy** for the sovereign toolchain. Implementation lives in
// phase1/phase2 C, `coff_reader`, `reloc_resolver`, `pe_writer`, and C++ helpers
// under `include/rawrxd/sovereign_*`. See `docs/SOVEREIGN_ENTERPRISE_TOOLCHAIN_LAYERS.md`.
// =============================================================================

#pragma once

#include <cstdint>

namespace rawrxd::sovereign::toolchain
{

/// Logical pipeline stage (1 = ingest … 4 = emit). Ordering is typical; some
/// tools may iterate (e.g. validate after layout).
enum class LayerId : std::uint8_t
{
    Ingest = 1,       ///< COFF .obj, archive members, .asm → object (see phase1 + phase2)
    Validate = 2,     ///< REL32 ±2 GiB, undefined symbols, layout constraints
    Optimize = 3,     ///< DCE / COMDAT (planned; not full LTO in-tree)
    Emit = 4          ///< PE32+ (+ optional debug dir, checksum; signing out-of-band)
};

// --- Header map (documentation anchors; no runtime registry in this file) ---

// Ingest:    toolchain/from_scratch/phase2_linker/coff_reader.h
//            toolchain/from_scratch/phase1_assembler/ (asm → COFF)
// Validate:  include/rawrxd/sovereign_rel32_validate.hpp
//            toolchain/from_scratch/phase2_linker/rel32_validate.h (C mirror)
// Optimize:  (future opt pass / call-graph — see enterprise layers doc)
// Emit:      toolchain/from_scratch/phase2_linker/pe_writer.h
//            toolchain/from_scratch/phase3_imports/import_builder.h

}  // namespace rawrxd::sovereign::toolchain
