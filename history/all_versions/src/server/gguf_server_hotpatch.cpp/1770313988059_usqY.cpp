#include "gguf_server_hotpatch.hpp"

GGUFServerHotpatch& GGUFServerHotpatch::instance() {
    static GGUFServerHotpatch inst;
    return inst;
}

void GGUFServerHotpatch::add_patch(const ServerHotpatch& patch) {
    m_patches.push_back(patch);
}

bool GGUFServerHotpatch::apply_patches(Request* req, Response* res) {
    bool any_applied = false;
    for (auto& patch : m_patches) {
        if (patch.transform && patch.transform(req, res)) {
            patch.hit_count++;
            any_applied = true;
        }
    }
    return any_applied;
}
