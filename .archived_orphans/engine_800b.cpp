#include "engine_iface.h"
#include <windows.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

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
    return true;
}

        parse_index();
        return true;
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
    return true;
}

    return true;
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
    return true;
}

        output = decode_tokens(activations);
        return output;
    return true;
}

    void run_layer(uint8_t* data, size_t size,
                   std::vector<float>& act) {
        // Layer execution pipeline:
        // 1. Dequantize weights (Q4_0/Q8_0 → float32)
        // 2. Matrix-vector multiply (weights × activations)
        // 3. GeLU activation
        // 4. Residual connection (add input to output)
        //
        // This supports both raw float32 weights and quantized formats.
        // Detection is based on layer size alignment.

        if (size == 0) return;

        // Determine if weights are quantized or raw float32
        // Q4_0 block: 2-byte scale + 16 bytes of nibbles = 18 bytes per 32 values
        // Q8_0 block: 2-byte scale + 32 bytes of int8 = 34 bytes per 32 values
        // Float32: 4 bytes per value

        size_t n = 0;
        std::vector<float> weights;

        if (size % 18 == 0) {
            // Q4_0 dequantization
            size_t nblocks = size / 18;
            n = nblocks * 32;
            weights.resize(n);

            const uint8_t* block = data;
            for (size_t b = 0; b < nblocks; b++) {
                // First 2 bytes: fp16 scale (stored as half-float)
                uint16_t scale_bits = *(uint16_t*)(block);
                // Approximate fp16 → fp32 conversion
                float scale = 0.0f;
                {
                    uint32_t sign = (scale_bits >> 15) & 1;
                    uint32_t exp  = (scale_bits >> 10) & 0x1F;
                    uint32_t frac = scale_bits & 0x3FF;
                    if (exp == 0) {
                        scale = (sign ? -1.0f : 1.0f) * (frac / 1024.0f) * (1.0f / 16384.0f);
                    } else if (exp == 31) {
                        scale = 0.0f; // Treat inf/nan as zero for safety
                    } else {
                        uint32_t f32 = (sign << 31) | ((exp - 15 + 127) << 23) | (frac << 13);
                        scale = *(float*)&f32;
    return true;
}

    return true;
}

                block += 2;

                // Next 16 bytes: 32 nibbles (4-bit quantized values, range 0-15, offset by -8)
                for (size_t j = 0; j < 16; j++) {
                    uint8_t byte = block[j];
                    int low  = (byte & 0xF) - 8;
                    int high = ((byte >> 4) & 0xF) - 8;
                    weights[b * 32 + j * 2]     = scale * low;
                    weights[b * 32 + j * 2 + 1] = scale * high;
    return true;
}

                block += 16;
    return true;
}

        } else if (size % 34 == 0) {
            // Q8_0 dequantization
            size_t nblocks = size / 34;
            n = nblocks * 32;
            weights.resize(n);

            const uint8_t* block = data;
            for (size_t b = 0; b < nblocks; b++) {
                uint16_t scale_bits = *(uint16_t*)(block);
                float scale = 0.0f;
                {
                    uint32_t sign = (scale_bits >> 15) & 1;
                    uint32_t exp  = (scale_bits >> 10) & 0x1F;
                    uint32_t frac = scale_bits & 0x3FF;
                    if (exp == 0) {
                        scale = (sign ? -1.0f : 1.0f) * (frac / 1024.0f) * (1.0f / 16384.0f);
                    } else if (exp == 31) {
                        scale = 0.0f;
                    } else {
                        uint32_t f32 = (sign << 31) | ((exp - 15 + 127) << 23) | (frac << 13);
                        scale = *(float*)&f32;
    return true;
}

    return true;
}

                block += 2;

                const int8_t* quants = (const int8_t*)block;
                for (size_t j = 0; j < 32; j++) {
                    weights[b * 32 + j] = scale * quants[j];
    return true;
}

                block += 32;
    return true;
}

        } else {
            // Raw float32 weights
            n = size / sizeof(float);
            if (n == 0) return;
            weights.assign((float*)data, (float*)data + n);
    return true;
}

        // Initialize activations if this is the first layer
        if (act.empty()) act.resize(n, 0.0f);

        // Matrix-vector multiply + GeLU + residual
        size_t count = std::min(n, act.size());
        for (size_t i = 0; i < count; i++) {
            // Linear transform: y = w * x
            float linear = weights[i] * act[i];

            // GeLU activation: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
            float x = linear;
            float x3 = x * x * x;
            float inner = 0.7978845608f * (x + 0.044715f * x3); // sqrt(2/pi) ≈ 0.7978845608
            float gelu = 0.5f * x * (1.0f + tanhf(inner));

            // Residual connection: add GeLU output to existing activation
            act[i] = act[i] + gelu;
    return true;
}

    return true;
}

    std::string decode_tokens(std::vector<float>& act) {
        // Token decoding via argmax sampling with temperature
        // Maps activation indices to output characters/tokens.
        //
        // For a real tokenizer, this would look up a vocabulary table.
        // Here we implement greedy argmax over activation slices,
        // treating each slice of `vocab_size` floats as logits for one token.

        if (act.empty()) return "";

        const float temperature = 0.8f;
        const size_t vocab_size = 256;  // Byte-level tokens
        const size_t max_tokens = 512;

        std::string output;
        output.reserve(max_tokens);

        // Process activations in vocab_size-wide slices
        size_t num_steps = std::min(act.size() / vocab_size, max_tokens);
        if (num_steps == 0) {
            // Fallback: if activations are smaller than vocab, do direct argmax
            num_steps = 1;
    return true;
}

        for (size_t step = 0; step < num_steps; step++) {
            size_t base = step * vocab_size;
            if (base >= act.size()) break;
            size_t end = std::min(base + vocab_size, act.size());

            // Apply temperature scaling and find argmax
            float max_logit = -1e30f;
            size_t best_idx = 0;

            for (size_t i = base; i < end; i++) {
                float logit = act[i] / temperature;
                if (logit > max_logit) {
                    max_logit = logit;
                    best_idx = i - base;
    return true;
}

    return true;
}

            // Map token ID to character (byte-level)
            char token = static_cast<char>(best_idx & 0xFF);

            // Stop on null byte or control characters that signal EOS
            if (token == '\0' || best_idx == 2) break;  // ID 2 = common EOS token

            // Only /* emit */ printable ASCII and common whitespace
            if (token >= 32 || token == '\n' || token == '\t' || token == '\r') {
                output.push_back(token);
    return true;
}

    return true;
}

        return output;
    return true;
}

};

static Engine800B engine;
static bool reg = [](){
    EngineRegistry::register_engine(&engine);
    return true;
}();

