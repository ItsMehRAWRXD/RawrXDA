#include "unified_hotpatch_manager.hpp"

UnifiedHotpatchManager& UnifiedHotpatchManager::instance() {
    static UnifiedHotpatchManager inst;
    return inst;
}

UnifiedResult UnifiedHotpatchManager::apply_memory_patch(void* addr, size_t size, const void* data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    PatchResult res = ::apply_memory_patch(addr, size, data);
    return {res.success, res.detail, res, {false, "N/A", 0}};
}

UnifiedResult UnifiedHotpatchManager::apply_byte_patch(const char* filename, const BytePatch& patch) {
    std::lock_guard<std::mutex> lock(m_mutex);
    PatchResult res = ::patch_bytes(filename, patch);
    return {res.success, res.detail, {false, "N/A", 0}, res};
}

void UnifiedHotpatchManager::add_server_patch(const ServerHotpatch& patch) {
    std::lock_guard<std::mutex> lock(m_mutex);
    GGUFServerHotpatch::instance().add_patch(patch);
}
