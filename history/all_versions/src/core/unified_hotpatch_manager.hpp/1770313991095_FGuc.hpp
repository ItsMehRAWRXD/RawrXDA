#pragma once
#include "model_memory_hotpatch.hpp"
#include "byte_level_hotpatcher.hpp"
#include "../server/gguf_server_hotpatch.hpp"
#include <mutex>
#include <vector>

struct UnifiedResult {
    bool success;
    const char* message;
    PatchResult memory_result;
    PatchResult byte_result;
};

class UnifiedHotpatchManager {
public:
    static UnifiedHotpatchManager& instance();

    UnifiedResult apply_memory_patch(void* addr, size_t size, const void* data);
    UnifiedResult apply_byte_patch(const char* filename, const BytePatch& patch);
    void add_server_patch(const ServerHotpatch& patch);

private:
    std::mutex m_mutex;
};
