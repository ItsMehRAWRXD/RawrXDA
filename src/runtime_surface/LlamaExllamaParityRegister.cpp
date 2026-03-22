#include "rawrxd/runtime/LlamaExllamaParityRegister.hpp"

namespace RawrXD::Runtime::LlamaExllamaParity {

namespace {
const RoleEntry kRoles[] = {
    {"blk.attn_norm", "blk.*.attn_norm.weight", "input_layernorm"},
    {"blk.ffn_norm", "blk.*.ffn_norm.weight", "post_attention_layernorm"},
    {"blk.attn_q", "blk.*.attn_q.weight", "q_proj"},
    {"blk.attn_k", "blk.*.attn_k.weight", "k_proj"},
    {"blk.attn_v", "blk.*.attn_v.weight", "v_proj"},
    {"blk.attn_output", "blk.*.attn_output.weight", "o_proj"},
    {"blk.ffn_gate", "blk.*.ffn_gate.weight", "gate_proj"},
    {"blk.ffn_up", "blk.*.ffn_up.weight", "up_proj"},
    {"blk.ffn_down", "blk.*.ffn_down.weight", "down_proj"},
    {"token_embd", "token_embd.weight", "embed_tokens"},
    {"output_norm", "output_norm.weight", "norm"},
    {"output", "output.weight", "lm_head"},
    {nullptr, nullptr, nullptr},
};
}  // namespace

const RoleEntry* roleTable() {
    return kRoles;
}

std::size_t roleTableCount() {
    std::size_t n = 0;
    while (kRoles[n].canonicalId != nullptr) {
        ++n;
    }
    return n;
}

}  // namespace RawrXD::Runtime::LlamaExllamaParity
