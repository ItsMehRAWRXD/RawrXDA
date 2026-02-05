#include <deque>
#include <string>

struct MemoryPlugin {
    std::deque<std::string> history;
    size_t max_tokens;
};

void mem_init(MemoryPlugin& m, size_t tokens){
    m.max_tokens=tokens;
}

void mem_push(MemoryPlugin& m,const std::string& s){
    m.history.push_back(s);
    // Simple count-based eviction for now, should be token based
    while(m.history.size() > 100) // arbitrary limit for example if token counting isn't implemented here
        m.history.pop_front();
}

std::string mem_pack(const MemoryPlugin& m){
    std::string out;
    for(auto& s:m.history) out+=s+"\n";
    return out;
}

namespace MemoryPlugins {
    void init(size_t tokens) {
        // Global init stub
    }
}
