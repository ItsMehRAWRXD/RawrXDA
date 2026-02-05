#include "engine_iface.h"
#include <windows.h>
#include <vector>
#include <string>

struct Shard {
    HANDLE hFile;
    HANDLE hMap;
    uint8_t* base;
    size_t size;
};

struct LayerIndex {
    int shard_id;
    size_t offset;
    size_t size;
};

class Engine800B : public Engine {
    std::vector<Shard> shards;
    std::vector<LayerIndex> layers;
    size_t n_layers;

public:
    bool load_model(const std::string& path) override {
        // path = directory
        std::vector<std::string> shard_paths = {
            path + "\\shard0.gguf",
            path + "\\shard1.gguf",
            path + "\\shard2.gguf",
            path + "\\shard3.gguf",
            path + "\\shard4.gguf"
        };

        for (auto& p : shard_paths) {
            Shard s{};
            s.hFile = CreateFileA(p.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ, 0,
                                  OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0);
            if (s.hFile == INVALID_HANDLE_VALUE) return false;

            LARGE_INTEGER sz;
            GetFileSizeEx(s.hFile, &sz);
            s.size = (size_t)sz.QuadPart;

            s.hMap = CreateFileMappingA(s.hFile, 0, PAGE_READONLY, 0, 0, 0);
            s.base = (uint8_t*)MapViewOfFile(s.hMap, FILE_MAP_READ, 0, 0, 0);
            if (!s.base) return false;

            shards.push_back(s);
        }

        parse_index();
        return true;
    }

    void parse_index() {
        if (shards.empty()) return;
        // first shard contains layer table
        uint8_t* p = shards[0].base;
        if (!p) return;
        
        uint32_t magic = *(uint32_t*)p;
        if (magic != 0x47475546) return; // "GGUF"

        n_layers = *(uint32_t*)(p + 16);
        size_t table_offset = *(uint64_t*)(p + 24);

        uint8_t* t = p + table_offset;
        for (size_t i = 0; i < n_layers; i++) {
            LayerIndex li;
            li.shard_id = *(uint32_t*)t;
            li.offset   = *(uint64_t*)(t + 4);
            li.size     = *(uint64_t*)(t + 12);
            layers.push_back(li);
            t += 20;
        }
    }

    const char* name() override { return "sovereign800b"; }

    std::string infer(const AgentRequest& req) override {
        std::string output;
        std::vector<float> activations;

        if (layers.empty()) return "Model not loaded or invalid.";

        for (size_t i = 0; i < n_layers; i++) {
            LayerIndex& li = layers[i];
            if (li.shard_id >= shards.size()) continue;
            uint8_t* layer_data = shards[li.shard_id].base + li.offset;
            run_layer(layer_data, li.size, activations);
        }

        output = decode_tokens(activations);
        return output;
    }

    void run_layer(uint8_t* data, size_t size,
                   std::vector<float>& act) {
        // Q4/Q8 -> f16 -> matmul -> gelu -> residual
        // minimal placeholder math (replace with your AVX kernel)
        size_t n = size / sizeof(float);
        if (n == 0) return;
        
        float* w = (float*)data;

        if (act.empty()) act.resize(n, 0.f);

        // Safety check to avoid out of bounds if layer sizes differ
        size_t count = std::min(n, act.size());
        for (size_t i = 0; i < count; i++)
            act[i] = act[i] + w[i];
    }

    std::string decode_tokens(std::vector<float>& act) {
        // naive decode stub (real sampler replaces this)
        std::string s;
        for (int i = 0; i < 64 && i < act.size(); i++)
            s.push_back('a' + (int(act[i]) % 26));
        return s;
    }
};

static Engine800B engine;
static bool reg = [](){
    EngineRegistry::register_engine(&engine);
    return true;
}();
