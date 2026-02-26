#pragma once
#include "../core/model_memory_hotpatch.hpp"
#include <map>
#include <string>
#include <vector>

struct Request {
    std::string prompt;
    std::map<std::string, float> params;
};

struct Response {
    std::string text;
    uint32_t tokens;
};

struct ServerHotpatch {
    const char* name;
    bool (*transform)(Request*, Response*);
    uint64_t hit_count;
};

class GGUFServerHotpatch {
public:
    static GGUFServerHotpatch& instance();
    
    void add_patch(const ServerHotpatch& patch);
    bool apply_patches(Request* req, Response* res);

    // Extended API for runtime management
    bool addPatch(const ServerHotpatch& patch) {
        m_patches.push_back(patch);
        return true;
    }
    bool removePatch(const char* name) {
        for (auto it = m_patches.begin(); it != m_patches.end(); ++it) {
            if (it->name && strcmp(it->name, name) == 0) {
                m_patches.erase(it);
                return true;
            }
        }
        return false;
    }
    size_t clearAllPatches() {
        size_t count = m_patches.size();
        m_patches.clear();
        return count;
    }
    const std::vector<ServerHotpatch>& getActivePatches() const {
        return m_patches;
    }

private:
    std::vector<ServerHotpatch> m_patches;
};
