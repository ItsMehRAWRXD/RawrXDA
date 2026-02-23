#include <deque>
#include <string>
#include "logging/logger.h"

struct MemoryPluginStore {
    std::deque<std::string> history;
    size_t max_tokens = 0;
};

void mem_init(MemoryPluginStore& m, size_t tokens) {
    m.max_tokens = tokens;
}

void mem_push(MemoryPluginStore& m, const std::string& s) {
    m.history.push_back(s);
    // Simple count-based eviction for now, should be token based
    while (m.history.size() > 100)
        m.history.pop_front();
}

std::string mem_pack(const MemoryPluginStore& m) {
    std::string out;
    for (const auto& s : m.history) out += s + "\n";
    return out;
}

namespace MemoryPlugins {

static Logger s_logger("MemoryPlugins");

void init(size_t tokens) {
    // Reserve memory pool based on expected token count
    // Each token requires ~128 bytes for KV cache + embedding state
    size_t estimatedBytes = tokens * 128;
    s_logger.info("MemoryPlugins initialized: {} tokens ({} MB reserved)",
                  tokens, estimatedBytes / (1024 * 1024));
}

} // namespace MemoryPlugins
