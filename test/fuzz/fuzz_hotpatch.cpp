#include "../../src/core/proxy_hotpatcher.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (!data || size == 0 || size > (1u << 15)) {
        return 0;
    }

    std::vector<char> buffer(size + 64, '\0');
    std::memcpy(buffer.data(), data, size);
    const size_t outLen = std::min(size, buffer.size() - 1);

    ProxyHotpatcher& patcher = ProxyHotpatcher::instance();

    TokenBias bias{};
    bias.tokenId = static_cast<uint32_t>(data[0]);
    bias.biasValue = (data[0] % 16) * 0.125f;
    bias.permanent = true;
    patcher.add_token_bias(bias);

    OutputRewriteRule rewrite{};
    rewrite.name = "fuzz_rule";
    rewrite.pattern = "AAAA";
    rewrite.replacement = "BBBB";
    rewrite.hitCount = 0;
    rewrite.enabled = true;
    patcher.add_rewrite_rule(rewrite);

    patcher.apply_rewrites(buffer.data(), outLen, buffer.size());

    std::vector<float> logits(256, 0.0f);
    patcher.apply_token_biases(logits.data(), logits.size());

    patcher.clear_rewrite_rules();
    patcher.clear_token_biases();
    return 0;
}
