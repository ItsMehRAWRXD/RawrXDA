// Global DCE: link-time removal of unreachable code symbols (sovereign_global_dce.hpp).

#include <algorithm>
#include <cstdio>

#include "rawrxd/sovereign_cir_outline.hpp"
#include "rawrxd/sovereign_global_dce.hpp"

namespace
{
using namespace RawrXD::SovereignLab;

bool fail(const char* msg)
{
    std::fprintf(stderr, "[test_sovereign_global_dce] FAIL: %s\n", msg);
    return false;
}

bool testStripsUnreachableFunction()
{
    LinkerContext ctx{};
    CirSection text{};
    text.name = ".text";
    // main: 8 bytes; dead_fn: 8 bytes at offset 8 (16 bytes total)
    text.rawBytes.resize(16);
    std::fill(text.rawBytes.begin(), text.rawBytes.end(), static_cast<std::uint8_t>(0x90));
    text.rawBytes[8] = 0xC3;  // ret in dead chunk
    ctx.sections.push_back(std::move(text));

    CirSymbol mainSym{};
    mainSym.id = 0;
    mainSym.name = "main";
    mainSym.sectionIndex = 0;
    mainSym.offsetInSection = 0;
    mainSym.binding = CirSymbolBinding::Global;
    mainSym.isCode = true;

    CirSymbol deadSym{};
    deadSym.id = 1;
    deadSym.name = "dead";
    deadSym.sectionIndex = 0;
    deadSym.offsetInSection = 8;
    deadSym.binding = CirSymbolBinding::Local;
    deadSym.isCode = true;

    ctx.symbols = {mainSym, deadSym};
    ctx.entrySymbolId = 0;
    // No relocations: only dead is unreachable

    const auto r = applyGlobalDeadCodeElimination(ctx, {});
    if (!r.error.empty())
    {
        return fail("expected success");
    }
    if (r.stats.removedCodeSymbols != 1u)
    {
        return fail("removedCodeSymbols");
    }
    if (ctx.sections[0].rawBytes.size() != 8u)
    {
        return fail("section size after strip");
    }
    if (ctx.symbols.size() != 1u || ctx.symbols[0].name != "main")
    {
        return fail("symbol table after strip");
    }
    if (ctx.entrySymbolId != 0u)
    {
        return fail("entry id remap");
    }
    return true;
}

bool testKeepsFunctionReferencedByReloc()
{
    LinkerContext ctx{};
    CirSection text{};
    text.name = ".text";
    text.rawBytes.resize(16);
    std::fill(text.rawBytes.begin(), text.rawBytes.end(), static_cast<std::uint8_t>(0x90));
    text.rawBytes[8] = 0xC3;
    ctx.sections.push_back(std::move(text));

    CirSymbol mainSym{};
    mainSym.id = 0;
    mainSym.name = "main";
    mainSym.sectionIndex = 0;
    mainSym.offsetInSection = 0;
    mainSym.binding = CirSymbolBinding::Global;
    mainSym.isCode = true;

    CirSymbol calleeSym{};
    calleeSym.id = 1;
    calleeSym.name = "callee";
    calleeSym.sectionIndex = 0;
    calleeSym.offsetInSection = 8;
    calleeSym.binding = CirSymbolBinding::Local;
    calleeSym.isCode = true;

    ctx.symbols = {mainSym, calleeSym};
    ctx.entrySymbolId = 0;
    CirRelocation rel{};
    rel.sectionIndex = 0;
    rel.offset = 4;
    rel.targetSymbolId = 1;
    rel.kind = CirRelocKind::Rel32;
    ctx.relocations.push_back(rel);

    const auto r = applyGlobalDeadCodeElimination(ctx, {});
    if (!r.error.empty())
    {
        return fail("expected success (reloc)");
    }
    if (r.stats.removedCodeSymbols != 0u)
    {
        return fail("should keep callee");
    }
    if (ctx.sections[0].rawBytes.size() != 16u)
    {
        return fail("section size unchanged");
    }
    if (ctx.symbols.size() != 2u)
    {
        return fail("two symbols");
    }
    return true;
}

}  // namespace

int main()
{
    if (!testStripsUnreachableFunction())
    {
        return 1;
    }
    if (!testKeepsFunctionReferencedByReloc())
    {
        return 1;
    }
    std::fprintf(stderr, "[test_sovereign_global_dce] OK\n");
    return 0;
}
