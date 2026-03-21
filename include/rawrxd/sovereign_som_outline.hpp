// ============================================================================
// sovereign_som_outline.hpp — Sovereign Object Model (SOM) — Tier G contract
// ============================================================================
//
// PURPOSE: Naming aligned with the "Universal Language Fabricator" story: one
// **relocatable unit** schema (sections + symbols + relocs + imports) that
// every frontend lowers into. This header is a thin layer over the same POD
// shapes as `sovereign_cir_outline.hpp` (Common IR / CIR); **SOM** and **CIR**
// refer to the same conceptual contract in this repo.
//
// TIER G: Global-use SOM naming — docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md.
// Tier P (IDE): MSVC cl + link.exe + CI — §6/§7 in
// docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md.
//
// See also: sovereign_som_minimal.h — pure C "atomic" floor (opcodes + fixups).
//
// MSVC analogy (documentation):
//   SOM_Unit          ~ one COFF .obj worth of contributions (pre-link merge)
//   SOM_Section       ~ IMAGE_SECTION_HEADER + raw section bytes
//   SOM_Symbol        ~ COFF symbol table row (simplified)
//   SOM_Reloc         ~ COFF relocation / fixup
//   SOM_Import        ~ __imp_ / import lib request (linker expands to IAT/ILT)
// ============================================================================

#pragma once

#include "rawrxd/sovereign_cir_outline.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace RawrXD::SovereignLab
{

// --- Same POD types, SOM vocabulary (MSVC-parallel names in docs) -----------

using SOM_RelocKind = CirRelocKind;
using SOM_SymbolBinding = CirSymbolBinding;

using SOM_Section = CirSection;
using SOM_Symbol = CirSymbol;
/// Relocation: patch site + target symbol id + kind (+ addend).
using SOM_Reloc = CirRelocation;
using SOM_Import = CirImport;

/// One translation unit’s contribution to the link (e.g. `main.cpp`, `a.asm`).
/// Symbol ids are typically **per-unit** until the linker merges TUs into a
/// `LinkerContext` / global table; unresolved references become linker input.
struct SOM_Unit
{
    /// Display path for diagnostics (optional).
    std::string sourcePath;
    /// Stable id for merge / debug (optional).
    std::uint32_t unitId{0};

    std::vector<SOM_Section> sections;
    std::vector<SOM_Symbol> symbols;
    std::vector<SOM_Reloc> relocations;
    /// Import requests discovered in this TU (deduped at link time).
    std::vector<SOM_Import> imports;
};

/// Full link job after ingesting one or more `SOM_Unit`s (flatten + resolve).
using SOM_LinkContext = LinkerContext;

}  // namespace RawrXD::SovereignLab
