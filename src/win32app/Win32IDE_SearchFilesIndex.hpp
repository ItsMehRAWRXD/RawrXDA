#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rawrxd::local_server_search
{

struct FileSearchResult
{
    std::string path;
    int line = 0;
    std::string lineText;
};

// Request body fields (path validated upstream).
// backend:
//   auto — token-friendly queries use mmap inverted index; optional Vulkan staging on cold cache;
//          odd punctuation falls back to recursive substring scan.
//   recursive-scan — always directory walk + substring (legacy).
//   mmap-cpu-index — mmap file reads, CPU inverted identifier index, TTL cache.
//   vram-inverted-index — same as mmap + pack & upload index blob to a device-local VkBuffer when Vulkan is available.
//
// Enhancements (JSON): caseSensitive, maxFileBytes, indexTtlSec, snippetMaxChars, forceReindex, excludeDirs[].
struct SearchRequest
{
    std::string root;
    std::string pattern;
    std::string query;
    bool search_content = true;
    bool case_sensitive = false;
    int max_results = 500;
    int max_file_bytes = 2 * 1024 * 1024;
    int index_ttl_sec = 30;
    int snippet_max_chars = 200;
    bool force_reindex = false;
    std::string backend;
    std::vector<std::string> exclude_dirs_extra;
};

struct SearchResponseMetrics
{
    double total_ms = 0;
    double index_build_ms = 0;
    double query_ms = 0;
    double gpu_upload_ms = 0;
    uint64_t vram_resident_bytes = 0;
    std::string backend;
    bool cached_index = false;
    uint64_t index_generation = 0;
    std::string fallback_reason;
};

void executeFileSearch(const SearchRequest& req, std::vector<FileSearchResult>& out, SearchResponseMetrics& met);

}  // namespace rawrxd::local_server_search
