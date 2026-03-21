// ============================================================================
// sovereign_cir_outline.hpp — Common IR (CIR) outline — Tier G public contract
// ============================================================================
//
// See also: sovereign_som_outline.hpp — same contract under "Sovereign Object
// Model" (SOM) naming: SOM_Unit, SOM_Section aliases, etc.
//
// PURPOSE: Optional data shapes for experiments toward a *self-contained*
// toolchain (assembler/C → sections/symbols/relocs → link → emit). This file
// is NOT required for the IDE target unless you wire it.
// See `sovereign_global_dce.hpp` for optional global dead-code elimination on
// `LinkerContext` (unreachable functions / strip + compact).
//
// TIER G: Reusable contract — docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md. Tier P
// (IDE product) uses MSVC (cl + link.exe) — §6/§7 in
// docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md.
//
// Style: C++20, no exceptions in *new* code using these types; logging for
// any future driver code should use Logger (see .cursorrules).
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RawrXD::SovereignLab
{

/// Relocation kind (architecture- and image-format–specific in a real linker).
enum class CirRelocKind : std::uint32_t
{
    None = 0,
    Abs64 = 1,
    Rel32 = 2,
    /// PE32+ image-relative (e.g. some fixups)
    Dir64 = 3,
};

/// Symbol binding (rough MSVC/COFF analogy).
enum class CirSymbolBinding : std::uint8_t
{
    Local = 0,
    Global = 1,
    Weak = 2,
    Undefined = 3,
};

/// One contiguous blob belonging to a section (future: merge COFF sections).
struct CirSection
{
    std::string name;            // e.g. ".text", ".idata"
    std::uint32_t alignment{1};  // virtual alignment (power of two)
    std::vector<std::uint8_t> rawBytes;
};

/// Symbol within one section (offset) or import placeholder (future).
struct CirSymbol
{
    std::uint32_t id{0};
    std::string name;
    std::uint32_t sectionIndex{0};  // index into LinkerContext::sections
    std::uint32_t offsetInSection{0};
    CirSymbolBinding binding{CirSymbolBinding::Local};
    bool isCode{true};
};

/// Relocation record: patch `rawBytes` at `offset` once the target symbol has a VA.
struct CirRelocation
{
    std::uint32_t sectionIndex{0};
    std::uint32_t offset{0};
    std::uint32_t targetSymbolId{0};
    CirRelocKind kind{CirRelocKind::None};
    /// Optional addend (REL32 / ADDR64 math).
    std::int64_t addend{0};
};

/// Import request (named DLL + function); linker expands to ILT/IAT + hint/name.
struct CirImport
{
    std::string dllName;
    std::string functionName;
    std::uint16_t ordinal{0};  // 0 = use name
    bool byOrdinal{false};
};

/// Container for a single link job (multiple TUs worth of IR before emit).
struct LinkerContext
{
    std::vector<CirSection> sections;
    std::vector<CirSymbol> symbols;
    std::vector<CirRelocation> relocations;
    std::vector<CirImport> imports;
    std::uint32_t entrySymbolId{0};
    std::uint64_t preferredImageBase{0x140000000ull};
};

}  // namespace RawrXD::SovereignLab
