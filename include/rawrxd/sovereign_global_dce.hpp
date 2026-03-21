// ============================================================================
// sovereign_global_dce.hpp — Global dead code elimination (link-time DCE)
// ============================================================================
//
// Optimization layer: removes **defined** code symbols that are not reachable
// from any root (entry + optional extra roots / exported globals). Uses the
// relocation graph: each relocation from a code region attributes an edge from
// the owning symbol to the relocation target. This keeps merged **global**
// images lean when lowering through `LinkerContext` / SOM (see
// sovereign_cir_outline.hpp, sovereign_som_outline.hpp).
//
// TIER G contract — docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md. Tier P (IDE) may
// use MSVC link `/OPT:REF` instead; this pass is for the sovereign lab path.
// C-side symbol hash table: rawrxd_symbol_registry.h + rawrxd_symbol_registry.c;
// hand-off doc: docs/SOVEREIGN_SYMBOL_REGISTRY_DCE.md.
//
// Returns GlobalDceOutcome (MSVC std::expected requires /std:c++23).
// ============================================================================

#pragma once

#include "rawrxd/sovereign_cir_outline.hpp"

#include <algorithm>
#include <cstdint>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace RawrXD::SovereignLab
{

/// Tunables for global DCE (MSVC /opt:ref analog at IR level).
struct GlobalDceOptions
{
    /// Additional roots besides `ctx.entrySymbolId` (e.g. DLL exports, CRT hooks).
    std::vector<std::uint32_t> extraRootSymbolIds{};
    /// If true, every defined symbol with `CirSymbolBinding::Global` is a root.
    bool preserveGlobalsAsRoots{false};
    /// If true, strip unreachable **data** symbols as well as dead code (after
    /// full reachability). When false, only `isCode` symbols are removed.
    bool stripUnreachableData{false};
};

struct GlobalDceStats
{
    std::uint32_t rootsSeeded{0};
    std::uint32_t reachableSymbols{0};
    std::uint32_t removedCodeSymbols{0};
    std::uint32_t removedDataSymbols{0};
    std::uint64_t strippedSectionBytes{0};
};

/// Result of `applyGlobalDeadCodeElimination` (success when `error` is empty).
struct GlobalDceOutcome
{
    GlobalDceStats stats{};
    std::string error{};
};

namespace detail
{

inline bool isDefined(const CirSymbol& s)
{
    return s.binding != CirSymbolBinding::Undefined;
}

inline std::vector<std::vector<std::uint32_t>> buildSectionSymbolOrder(const LinkerContext& ctx)
{
    std::vector<std::vector<std::uint32_t>> bySec(ctx.sections.size());
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(ctx.symbols.size()); ++i)
    {
        const auto& sym = ctx.symbols[i];
        if (sym.sectionIndex < bySec.size())
        {
            bySec[sym.sectionIndex].push_back(i);
        }
    }
    for (auto& v : bySec)
    {
        std::sort(v.begin(), v.end(), [&](std::uint32_t a, std::uint32_t b)
                  { return ctx.symbols[a].offsetInSection < ctx.symbols[b].offsetInSection; });
    }
    return bySec;
}

inline std::vector<std::uint32_t> computeSymbolSizes(const LinkerContext& ctx,
                                                     const std::vector<std::vector<std::uint32_t>>& order)
{
    std::vector<std::uint32_t> sizes(ctx.symbols.size(), 0);
    for (std::uint32_t si = 0; si < static_cast<std::uint32_t>(ctx.sections.size()); ++si)
    {
        const auto& sec = ctx.sections[si];
        const auto& ord = order[si];
        for (size_t k = 0; k < ord.size(); ++k)
        {
            const std::uint32_t symIdx = ord[k];
            const std::uint32_t off = ctx.symbols[symIdx].offsetInSection;
            std::uint32_t end = static_cast<std::uint32_t>(sec.rawBytes.size());
            if (k + 1 < ord.size())
            {
                end = ctx.symbols[ord[k + 1]].offsetInSection;
            }
            if (end > off)
            {
                sizes[symIdx] = end - off;
            }
        }
    }
    return sizes;
}

/// Symbol index owning `byteOffset` in `sectionIndex`, or UINT32_MAX (gap / OOB).
inline std::uint32_t findOwnerSymbol(const LinkerContext& ctx, const std::vector<std::vector<std::uint32_t>>& order,
                                     const std::vector<std::uint32_t>& symSizes, std::uint32_t sectionIndex,
                                     std::uint32_t byteOffset)
{
    if (sectionIndex >= order.size())
    {
        return UINT32_MAX;
    }
    const auto& ord = order[sectionIndex];
    if (ord.empty())
    {
        return UINT32_MAX;
    }
    size_t lo = 0;
    size_t hi = ord.size();
    while (lo + 1 < hi)
    {
        const size_t mid = (lo + hi) / 2;
        if (ctx.symbols[ord[mid]].offsetInSection <= byteOffset)
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }
    if (ctx.symbols[ord[lo]].offsetInSection > byteOffset)
    {
        return UINT32_MAX;
    }
    const std::uint32_t symIdx = ord[lo];
    const std::uint32_t start = ctx.symbols[symIdx].offsetInSection;
    const std::uint32_t span = symSizes[symIdx];
    if (span == 0)
    {
        return UINT32_MAX;
    }
    if (byteOffset < start || byteOffset >= start + span)
    {
        return UINT32_MAX;
    }
    return symIdx;
}

/// BFS from `reach` along directed edges: reloc owner -> target.
inline void propagateReachability(const LinkerContext& ctx, const std::vector<std::vector<std::uint32_t>>& order,
                                  const std::vector<std::uint32_t>& symSizes, std::unordered_set<std::uint32_t>& reach)
{
    std::queue<std::uint32_t> q;
    for (std::uint32_t s : reach)
    {
        q.push(s);
    }
    while (!q.empty())
    {
        const std::uint32_t u = q.front();
        q.pop();
        for (const auto& r : ctx.relocations)
        {
            const std::uint32_t owner = findOwnerSymbol(ctx, order, symSizes, r.sectionIndex, r.offset);
            if (owner != u)
            {
                continue;
            }
            if (!isDefined(ctx.symbols[u]))
            {
                continue;
            }
            const std::uint32_t v = r.targetSymbolId;
            if (v >= ctx.symbols.size() || !isDefined(ctx.symbols[v]))
            {
                continue;
            }
            if (reach.insert(v).second)
            {
                q.push(v);
            }
        }
    }
}

inline std::vector<std::vector<std::int64_t>> buildDeltaBeforeTables(
    const std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t>>>& mergedIntervals,
    const std::vector<std::uint32_t>& oldSectionSizes)
{
    std::vector<std::vector<std::int64_t>> out(mergedIntervals.size());
    for (std::uint32_t si = 0; si < static_cast<std::uint32_t>(mergedIntervals.size()); ++si)
    {
        const auto& iv = mergedIntervals[si];
        const std::uint32_t sz = oldSectionSizes[si];
        out[si].resize(static_cast<size_t>(sz) + 1u);
        for (std::uint32_t x = 0; x <= sz; ++x)
        {
            std::int64_t d = 0;
            for (const auto& seg : iv)
            {
                if (seg.second <= x)
                {
                    d += static_cast<std::int64_t>(seg.second - seg.first);
                }
            }
            out[si][x] = d;
        }
    }
    return out;
}

}  // namespace detail

/// Global DCE: strip unreachable **defined** symbols from `ctx`, compact section
/// bytes, remap symbol ids, and drop relocations into dead regions.
///
/// Roots: `ctx.entrySymbolId` (if valid) + `opt.extraRootSymbolIds` +
/// optionally every `Global` defined symbol when `preserveGlobalsAsRoots`.
inline GlobalDceOutcome applyGlobalDeadCodeElimination(LinkerContext& ctx, const GlobalDceOptions& opt = {})
{
    if (ctx.symbols.empty())
    {
        return GlobalDceOutcome{{}, "applyGlobalDeadCodeElimination: empty symbol table"};
    }

    // Immutable snapshot — all analysis & reloc filtering use this.
    LinkerContext snap;
    snap.sections = ctx.sections;
    snap.symbols = ctx.symbols;
    snap.relocations = ctx.relocations;
    snap.entrySymbolId = ctx.entrySymbolId;
    snap.preferredImageBase = ctx.preferredImageBase;
    snap.imports = ctx.imports;

    const auto order = detail::buildSectionSymbolOrder(snap);
    const auto symSizes = detail::computeSymbolSizes(snap, order);

    GlobalDceStats stats{};
    std::unordered_set<std::uint32_t> reach;

    auto tryRoot = [&](std::uint32_t id)
    {
        if (id < snap.symbols.size() && detail::isDefined(snap.symbols[id]))
        {
            reach.insert(id);
        }
    };

    if (snap.entrySymbolId < snap.symbols.size())
    {
        tryRoot(snap.entrySymbolId);
    }
    for (std::uint32_t id : opt.extraRootSymbolIds)
    {
        tryRoot(id);
    }
    if (opt.preserveGlobalsAsRoots)
    {
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(snap.symbols.size()); ++i)
        {
            const auto& s = snap.symbols[i];
            if (s.binding == CirSymbolBinding::Global && detail::isDefined(s))
            {
                reach.insert(i);
            }
        }
    }

    stats.rootsSeeded = static_cast<std::uint32_t>(reach.size());
    if (reach.empty())
    {
        return GlobalDceOutcome{{},
                                "applyGlobalDeadCodeElimination: no roots (set entrySymbolId or extraRootSymbolIds)"};
    }

    detail::propagateReachability(snap, order, symSizes, reach);
    stats.reachableSymbols = static_cast<std::uint32_t>(reach.size());

    const std::uint32_t nSym = static_cast<std::uint32_t>(snap.symbols.size());
    std::vector<std::uint8_t> removeSym(nSym, 0);
    for (std::uint32_t i = 0; i < nSym; ++i)
    {
        const auto& s = snap.symbols[i];
        if (!detail::isDefined(s))
        {
            continue;
        }
        if (reach.count(i))
        {
            continue;
        }
        if (s.isCode)
        {
            removeSym[i] = 1;
            ++stats.removedCodeSymbols;
        }
        else if (opt.stripUnreachableData)
        {
            removeSym[i] = 1;
            ++stats.removedDataSymbols;
        }
    }

    std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t>>> intervals(snap.sections.size());
    for (std::uint32_t i = 0; i < nSym; ++i)
    {
        if (!removeSym[i])
        {
            continue;
        }
        const auto& s = snap.symbols[i];
        if (s.sectionIndex >= snap.sections.size())
        {
            continue;
        }
        const std::uint32_t sz = symSizes[i];
        if (sz == 0)
        {
            continue;
        }
        intervals[s.sectionIndex].push_back({s.offsetInSection, s.offsetInSection + sz});
    }

    std::vector<std::uint32_t> oldSectionSizes(snap.sections.size());
    for (std::uint32_t si = 0; si < static_cast<std::uint32_t>(snap.sections.size()); ++si)
    {
        oldSectionSizes[si] = static_cast<std::uint32_t>(snap.sections[si].rawBytes.size());
    }

    std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t>>> mergedIntervals(snap.sections.size());
    for (std::uint32_t si = 0; si < static_cast<std::uint32_t>(snap.sections.size()); ++si)
    {
        auto& iv = intervals[si];
        if (iv.empty())
        {
            continue;
        }
        std::sort(iv.begin(), iv.end());
        std::vector<std::pair<std::uint32_t, std::uint32_t>> merged;
        for (const auto& p : iv)
        {
            if (merged.empty() || p.first > merged.back().second)
            {
                merged.push_back(p);
            }
            else
            {
                merged.back().second = std::max(merged.back().second, p.second);
            }
        }
        mergedIntervals[si] = std::move(merged);
    }

    // Fix double-counting: use merged intervals for byte stats
    stats.strippedSectionBytes = 0;
    for (std::uint32_t si = 0; si < static_cast<std::uint32_t>(mergedIntervals.size()); ++si)
    {
        for (const auto& seg : mergedIntervals[si])
        {
            stats.strippedSectionBytes += static_cast<std::uint64_t>(seg.second - seg.first);
        }
    }

    const auto deltaBefore = detail::buildDeltaBeforeTables(mergedIntervals, oldSectionSizes);

    auto newOffset = [&](std::uint32_t sec, std::uint32_t oldOff) -> std::uint32_t
    {
        if (sec >= deltaBefore.size())
        {
            return oldOff;
        }
        if (oldOff >= deltaBefore[sec].size())
        {
            return oldOff;
        }
        const std::int64_t d = deltaBefore[sec][oldOff];
        return static_cast<std::uint32_t>(static_cast<std::int64_t>(oldOff) - d);
    };

    // Compact section bytes from snapshot.
    for (std::uint32_t si = 0; si < static_cast<std::uint32_t>(ctx.sections.size()); ++si)
    {
        const auto& iv = mergedIntervals[si];
        if (iv.empty())
        {
            continue;
        }
        const auto& raw = snap.sections[si].rawBytes;
        std::vector<std::uint8_t> out;
        out.reserve(raw.size());
        std::uint32_t cursor = 0;
        for (const auto& seg : iv)
        {
            if (seg.first > cursor)
            {
                out.insert(out.end(), raw.begin() + cursor, raw.begin() + seg.first);
            }
            cursor = seg.second;
        }
        if (cursor < raw.size())
        {
            out.insert(out.end(), raw.begin() + cursor, raw.end());
        }
        ctx.sections[si].rawBytes = std::move(out);
    }

    std::unordered_map<std::uint32_t, std::uint32_t> oldToNew;
    std::uint32_t nid = 0;
    for (std::uint32_t i = 0; i < nSym; ++i)
    {
        if (!removeSym[i])
        {
            oldToNew[i] = nid++;
        }
    }

    std::vector<CirSymbol> newSyms;
    newSyms.reserve(nid);
    for (std::uint32_t i = 0; i < nSym; ++i)
    {
        if (removeSym[i])
        {
            continue;
        }
        CirSymbol s = snap.symbols[i];
        if (detail::isDefined(s) && s.sectionIndex < snap.sections.size())
        {
            s.offsetInSection = newOffset(s.sectionIndex, s.offsetInSection);
        }
        s.id = oldToNew[i];
        newSyms.push_back(std::move(s));
    }
    ctx.symbols = std::move(newSyms);

    std::vector<CirRelocation> newRel;
    newRel.reserve(snap.relocations.size());
    for (const auto& r : snap.relocations)
    {
        const std::uint32_t owner = detail::findOwnerSymbol(snap, order, symSizes, r.sectionIndex, r.offset);
        if (owner != UINT32_MAX && owner < removeSym.size() && removeSym[owner])
        {
            continue;
        }
        if (r.targetSymbolId < removeSym.size() && removeSym[r.targetSymbolId])
        {
            continue;
        }
        CirRelocation nr = r;
        if (nr.sectionIndex < deltaBefore.size() && nr.offset < deltaBefore[nr.sectionIndex].size())
        {
            nr.offset = newOffset(nr.sectionIndex, nr.offset);
        }
        auto it = oldToNew.find(nr.targetSymbolId);
        if (it != oldToNew.end())
        {
            nr.targetSymbolId = it->second;
        }
        newRel.push_back(nr);
    }
    ctx.relocations = std::move(newRel);

    auto eit = oldToNew.find(snap.entrySymbolId);
    if (eit != oldToNew.end())
    {
        ctx.entrySymbolId = eit->second;
    }

    ctx.imports = snap.imports;
    ctx.preferredImageBase = snap.preferredImageBase;

    GlobalDceOutcome out{};
    out.stats = stats;
    return out;
}

}  // namespace RawrXD::SovereignLab
