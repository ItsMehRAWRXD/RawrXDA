/********************************************************************
 *  RawrZ-Polymorphic-Gen  K1.6  (header-only, C++20)
 *  Randomly generates a unique GGUF/HF stub every run.
 *  Tensor names, shapes, dtypes, offsets → all randomised.
 *  Still loads into the 512 MB MMF pipeline.
 *******************************************************************/
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

namespace rawrz::poly {

using ByteVec = std::vector<char>;
using Offset  = std::size_t;

// ---------- fast RNG ----------
inline std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}
inline int rand_int(int a, int b) { return std::uniform_int_distribution<>(a, b)(rng()); }
inline bool rand_bool() { return rand_int(0, 1) == 1; }

// ---------- random dtype ----------
inline const char* rand_dtype() {
    const char* opts[] = {"fp16", "fp32", "int8", "int4"};
    return opts[rand_int(0, 3)];
}
inline int dtype_bytes(const char* dt) {
    if (std::strcmp(dt, "fp16") == 0) return 2;
    if (std::strcmp(dt, "fp32") == 0) return 4;
    if (std::strcmp(dt, "int8") == 0) return 1;
    return 1; // int4 → pad to byte
}

// ---------- random tensor name ----------
inline std::string rand_tensor_name() {
    static const char* parts[] = {"embed", "self_attn", "mlp", "norm", "gate", "up", "down", "q", "k", "v", "o"};
    std::stringstream ss;
    ss << "model.layers." << rand_int(0, 79) << "." << parts[rand_int(0, 10)] << ".weight";
    return ss.str();
}

// ---------- random shape ----------
inline std::vector<int> rand_shape() {
    int d = rand_int(1, 3); // 1D or 2D
    std::vector<int> s;
    for (int i = 0; i < d; ++i) s.push_back(rand_int(64, 8192));
    return s;
}

// ---------- random config ----------
struct PolyConfig {
    std::string model_type = "llama";
    int num_hidden_layers  = rand_int(12, 80);
    int hidden_size        = rand_int(512, 8192);
    int num_attention_heads = rand_int(8, hidden_size / 64);
    int num_key_value_heads = rand_int(1, num_attention_heads);
    int vocab_size          = rand_int(16000, 64000);
    int max_pos             = rand_int(1024, 16384);
    const char* torch_dtype = rand_dtype();
    std::vector<std::string> tensor_names;
    std::vector<std::vector<int>> tensor_shapes;
    std::vector<Offset> tensor_offsets;
    std::size_t total_bytes = 0;
};

// ---------- synthetic GGUF header ----------
void EmitSyntheticGguf(const std::string& path, const PolyConfig& cfg)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot create GGUF");

    // header
    out.write("GGUF", 4);
    std::uint32_t ver = 3; out.write(reinterpret_cast<const char*>(&ver), 4);
    std::uint64_t tensorCount = cfg.tensor_names.size();
    std::uint64_t metaCount   = 4;
    out.write(reinterpret_cast<const char*>(&tensorCount), 8);
    out.write(reinterpret_cast<const char*>(&metaCount), 8);

    // metadata
    auto write_str = [&](const std::string& s) {
        std::uint32_t len = s.size();
        out.write(reinterpret_cast<const char*>(&len), 4);
        out.write(s.data(), len);
    };
    auto write_u32 = [&](std::uint32_t v) {
        std::uint8_t type = 4; // UINT32
        out.write(reinterpret_cast<const char*>(&type), 1);
        out.write(reinterpret_cast<const char*>(&v), 4);
    };

    write_str("general.name"); write_str("RawrZ-Poly-" + std::to_string(rand_int(1000, 9999)));
    write_str("general.quantization_version"); write_u32(2);
    write_str("general.file_type"); write_u32(rand_int(4, 8)); // Q4_K_M .. Q8_0
    write_str("general.alignment"); write_u32(32);

    // alignment
    std::size_t pos = out.tellp();
    std::size_t pad = (32 - (pos % 32)) % 32;
    out.write(ByteVec(pad, '\0').data(), pad);

    // tensor infos (offsets are random but sequential)
    Offset running = out.tellp();
    for (std::size_t i = 0; i < tensorCount; ++i) {
        write_str(cfg.tensor_names[i]);
        std::uint8_t  type = 0; // F32 for shape
        out.write(reinterpret_cast<const char*>(&type), 1);
        std::uint64_t dim = cfg.tensor_shapes[i].size();
        out.write(reinterpret_cast<const char*>(&dim), 8);
        for (int d : cfg.tensor_shapes[i]) {
            std::uint64_t v = d; out.write(reinterpret_cast<const char*>(&v), 8);
        }
        Offset off = running;
        out.write(reinterpret_cast<const char*>(&off), 8);
        running += std::accumulate(cfg.tensor_shapes[i].begin(), cfg.tensor_shapes[i].end(), 1,
                                   std::multiplies<int>()) * dtype_bytes(cfg.torch_dtype);
    }
    out.close();
}

// ---------- random HF stub ----------
void EmitRandomStub(const std::string& dir, const PolyConfig& cfg, const char* mmfName)
{
    std::string config = R"({
  "model_type": ")" + cfg.model_type + R"(",
  "architectures": ["LlamaForCausalLM"],
  "num_hidden_layers": )" + std::to_string(cfg.num_hidden_layers) + R"(,
  "hidden_size": )" + std::to_string(cfg.hidden_size) + R"(,
  "num_attention_heads": )" + std::to_string(cfg.num_attention_heads) + R"(,
  "num_key_value_heads": )" + std::to_string(cfg.num_key_value_heads) + R"(,
  "vocab_size": )" + std::to_string(cfg.vocab_size) + R"(,
  "max_position_embeddings": )" + std::to_string(cfg.max_pos) + R"(,
  "torch_dtype": ")" + cfg.torch_dtype + R"("
})";
    std::ofstream(dir + "/config.json") << config;
    std::ofstream(dir + "/tokenizer.json") << "{}";
    std::ofstream(dir + "/tokenizer_config.json") << R"({"tokenizer_class":"LlamaTokenizer"})";
    // safetensors index → every tensor points to the same MMF file
    std::ofstream idx(dir + "/model.safetensors.index.json");
    idx << "{\n  \"metadata\": {\"total_size\":" << cfg.total_bytes << "},\n  \"weight_map\": {\n";
    for (std::size_t i = 0; i < cfg.tensor_names.size(); ++i) {
        idx << "    \"" << cfg.tensor_names[i] << "\": \"model.safetensors\"" << (i + 1 < cfg.tensor_names.size() ? "," : "") << "\n";
    }
    idx << "  }\n}\n";
    // zero-byte placeholder that redirects to MMF
    std::ofstream(dir + "/model.safetensors", std::ios::binary);
}

// ---------- polymorphic generator ----------
inline void GeneratePolymorphicModel(const std::string& outDir,
                                     const std::string& mmfName,
                                     int tensorCount = 20)
{
    PolyConfig cfg;
    // random tensors
    for (int i = 0; i < tensorCount; ++i) {
        cfg.tensor_names.push_back(rand_tensor_name());
        cfg.tensor_shapes.push_back(rand_shape());
        cfg.total_bytes += std::accumulate(cfg.tensor_shapes.back().begin(), cfg.tensor_shapes.back().end(), 1,
                                           std::multiplies<int>()) * dtype_bytes(cfg.torch_dtype);
    }
    cfg.total_bytes = ((cfg.total_bytes + 511 * 1024 * 1024) / (512 * 1024 * 1024)) * 512 * 1024 * 1024; // round up to 512 MB boundary

    // emit files
    EmitRandomStub(outDir, cfg, mmfName.c_str());
    EmitSyntheticGguf(outDir + "/model.gguf", cfg);

    std::cout << "[PolyGen] Model stub created in " << outDir << "\n";
    std::cout << "[PolyGen] " << cfg.tensor_names.size() << " tensors, "
              << cfg.total_bytes / (1024.0 * 1024.0) << " MB, dtype=" << cfg.torch_dtype << "\n";
    std::cout << "[PolyGen] Example tensor: " << cfg.tensor_names[0]
              << " shape [" << cfg.tensor_shapes[0][0];
    for (std::size_t i = 1; i < cfg.tensor_shapes[0].size(); ++i) std::cout << "," << cfg.tensor_shapes[0][i];
    std::cout << "]\n";
}

} // namespace rawrz::poly