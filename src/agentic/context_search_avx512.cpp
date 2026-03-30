#include "context_search_avx512.h"

#include <algorithm>
#include <array>
#include <bit>
#include <vector>

namespace rawrxd::agentic::context_search {

namespace {
constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;
constexpr std::uint64_t kRawrMixA = 0x9E3779B185EBCA87ull;
constexpr std::uint64_t kRawrMixB = 0xC2B2AE3D27D4EB4Full;

std::array<std::uint64_t, 64> g_symbolTable {};
std::size_t g_symbolCount = 0;
}

std::uint64_t hash_symbol_rawrxd64(const std::string& symbol) {
    std::uint64_t hash = kFnvOffsetBasis ^ (static_cast<std::uint64_t>(symbol.size()) * kRawrMixA);
    for (const unsigned char c : symbol) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= kFnvPrime;
        hash = std::rotl(hash, 7) ^ kRawrMixA;
    }

    // Final avalanche to improve spread for short symbols.
    hash ^= (hash >> 33);
    hash *= kRawrMixA;
    hash ^= (hash >> 29);
    hash *= kRawrMixB;
    hash ^= (hash >> 32);

    return hash;
}

std::uint64_t hash_symbol_fnv1a(const std::string& symbol) {
    return hash_symbol_rawrxd64(symbol);
}

bool context_search_init(const std::uint64_t* symbol_hashes, std::size_t count) {
    if (!symbol_hashes || count == 0) {
        g_symbolCount = 0;
        g_symbolTable.fill(0);
        return false;
    }

    g_symbolTable.fill(0);
    g_symbolCount = std::min<std::size_t>(g_symbolTable.size(), count);
    std::copy_n(symbol_hashes, g_symbolCount, g_symbolTable.begin());

    return RawrXD::Search::initializeContextSearchSymbols(
        std::span<const std::uint64_t>(g_symbolTable.data(), g_symbolCount));
}

size_t context_search_scan(const void* mapped_buffer, size_t len, std::uint32_t* match_indices, size_t max_matches) {
    if (!mapped_buffer || !match_indices || max_matches == 0 || g_symbolCount == 0) {
        return 0;
    }

    std::vector<std::uint64_t> offsets(max_matches, 0);
    const std::size_t matches = RawrXD::Search::findSymbolOffsets(mapped_buffer, len, offsets.data(), max_matches);
    for (std::size_t i = 0; i < matches; ++i) {
        const std::uint64_t value = offsets[i];
        match_indices[i] = value > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<std::uint32_t>(value);
    }
    return matches;
}

bool context_map_file(const char* filepath, MappedFileView& outView) {
    if (!filepath || filepath[0] == '\0') {
        outView = {};
        return false;
    }
    return RawrXD::Search::mapFileReadOnly(std::string(filepath), outView);
}

void context_unmap(MappedFileView& view) {
    RawrXD::Search::unmapFileView(view);
}

}  // namespace rawrxd::agentic::context_search
