#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/zero_copy_index.hpp"

namespace rawrxd::agentic::context_search {

using MappedFileView = RawrXD::Search::MappedFileView;

// Default symbol hash for Vector 2: RawrXD64 (FNV-derived, AVX-friendly 64-bit output).
std::uint64_t hash_symbol_rawrxd64(const std::string& symbol);

// Compatibility alias for existing callsites; currently routes to RawrXD64.
std::uint64_t hash_symbol_fnv1a(const std::string& symbol);

bool context_search_init(const std::uint64_t* symbol_hashes, std::size_t count);

// Writes chunk byte offsets to match_indices (truncated to 32-bit).
size_t context_search_scan(const void* mapped_buffer, size_t len, std::uint32_t* match_indices,
                           size_t max_matches = 64);

bool context_map_file(const char* filepath, MappedFileView& outView);
void context_unmap(MappedFileView& view);

}  // namespace rawrxd::agentic::context_search
