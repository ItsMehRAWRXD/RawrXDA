#pragma once
#include "model_memory_hotpatch.hpp"
#include <string>
#include <vector>

struct BytePatch {
    size_t offset;
    std::vector<uint8_t> data;
};

/**
 * Byte-Level Layer
 * Precision GGUF binary modification without full reparse.
 */
PatchResult patch_bytes(const char* filename, const BytePatch& patch);
PatchResult search_and_patch_bytes(const char* filename, const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement);
