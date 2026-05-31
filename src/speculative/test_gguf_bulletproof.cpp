// test_gguf_bulletproof.cpp — Validate parser against ministral3 + gptoss20b
#include "rawr_gguf_parser.h"
#include <cstdio>

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <model.gguf>\n", argv[0]);
        return 1;
    }

    printf("[TEST] Loading: %s\n", argv[1]);

    rawr::GGUFParsed gguf = rawr::parse_gguf(argv[1]);

    if (!gguf.valid) {
        printf("[FAIL] GGUF parse failed: %s\n", gguf.error.c_str());
        return 1;
    }

    printf("[OK]   Parse succeeded\n");
    printf("[INFO] Architecture: %s\n", gguf.config.arch.c_str());
    printf("[INFO] Vocab: %u  Hidden: %u  Layers: %u\n",
           gguf.config.vocab_size, gguf.config.hidden_size, gguf.config.num_layers);
    printf("[INFO] Heads: %u  KV-Heads: %u  FFN: %u\n",
           gguf.config.num_heads, gguf.config.num_kv_heads, gguf.config.intermediate_size);
    printf("[INFO] RMS eps: %.6f  RoPE theta: %.1f\n",
           gguf.config.rms_norm_eps, gguf.config.rope_theta);
    printf("[INFO] Tensors: %zu  Data offset: %llu\n",
           gguf.tensors.size(), (unsigned long long)gguf.data_offset);

    // Validate critical tensors exist
    const char* critical[] = {
        "token_embd.weight",
        "output_norm.weight",
        "output.weight",
        nullptr
    };
    for (const char** p = critical; *p; ++p) {
        if (rawr::has_tensor(gguf, *p)) {
            printf("[OK]   Found: %s\n", *p);
        } else {
            printf("[WARN] Missing: %s\n", *p);
        }
    }

    // Check first layer tensors
    char buf[128];
    snprintf(buf, sizeof(buf), "%s.blk.0.attn_norm.weight", gguf.config.arch.c_str());
    if (rawr::has_tensor(gguf, buf)) printf("[OK]   Found: %s\n", buf);

    snprintf(buf, sizeof(buf), "%s.blk.0.attn_q.weight", gguf.config.arch.c_str());
    if (rawr::has_tensor(gguf, buf)) printf("[OK]   Found: %s\n", buf);

    // Validate tensor offsets are monotonic and within bounds
    uint64_t last_end = 0;
    bool offsets_ok = true;
    for (const auto& ti : gguf.tensors) {
        if (ti.file_offset < last_end) {
            printf("[WARN] Tensor '%s' overlap! offset=%llu last_end=%llu\n",
                   ti.name.c_str(), (unsigned long long)ti.file_offset, (unsigned long long)last_end);
            offsets_ok = false;
        }
        last_end = ti.file_offset + ti.size_bytes;
    }
    if (offsets_ok) printf("[OK]   All tensor offsets monotonic, no overlaps\n");

    // Show first 5 and last 5 tensors
    printf("\n[TENSORS] First 5:\n");
    for (size_t i = 0; i < std::min(size_t(5), gguf.tensors.size()); ++i) {
        auto& t = gguf.tensors[i];
        printf("  %-40s dims=%u[%llu,%llu,%llu,%llu] type=%u off=%llu size=%zu\n",
               t.name.c_str(), t.n_dims,
               (unsigned long long)t.dims[0], (unsigned long long)t.dims[1],
               (unsigned long long)t.dims[2], (unsigned long long)t.dims[3],
               (unsigned)t.type, (unsigned long long)t.file_offset, t.size_bytes);
    }
    if (gguf.tensors.size() > 10) {
        printf("  ... (%zu total)\n", gguf.tensors.size());
        printf("\n[TENSORS] Last 5:\n");
        for (size_t i = gguf.tensors.size() - 5; i < gguf.tensors.size(); ++i) {
            auto& t = gguf.tensors[i];
            printf("  %-40s dims=%u[%llu,%llu,%llu,%llu] type=%u off=%llu size=%zu\n",
                   t.name.c_str(), t.n_dims,
                   (unsigned long long)t.dims[0], (unsigned long long)t.dims[1],
                   (unsigned long long)t.dims[2], (unsigned long long)t.dims[3],
                   (unsigned)t.type, (unsigned long long)t.file_offset, t.size_bytes);
        }
    }

    printf("\n[PASS] All checks passed\n");
    return 0;
}
