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

private:
    std::vector<ServerHotpatch> m_patches;
};
