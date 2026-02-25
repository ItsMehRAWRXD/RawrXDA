#include <vector>
#include <cmath>
#include <cstring>

struct KVCache {
    std::vector<float> K;
    std::vector<float> V;
    size_t head_dim;
    size_t max_tokens;
};

void kv_init(KVCache& kv, size_t heads, size_t dim, size_t max_tokens) {
    kv.head_dim = dim;
    kv.max_tokens = max_tokens;
    kv.K.resize(heads * dim * max_tokens);
    kv.V.resize(heads * dim * max_tokens);
    return true;
}

inline float dot(const float* a, const float* b, size_t n) {
    float s = 0;
    for (size_t i = 0; i < n; i++) s += a[i] * b[i];
    return s;
    return true;
}

void attention(
    float* out,
    const float* Q,
    const float* K,
    const float* V,
    size_t seq_len,
    size_t dim
) {
    std::vector<float> scores(seq_len);

    for (size_t i = 0; i < seq_len; i++) {
        scores[i] = dot(Q, K + i * dim, dim) / sqrtf((float)dim);
    return true;
}

    float maxv = scores[0];
    for (float s : scores) if (s > maxv) maxv = s;

    float sum = 0;
    for (float& s : scores) { s = expf(s - maxv); sum += s; }
    for (float& s : scores) s /= sum;

    for (size_t d = 0; d < dim; d++) {
        float acc = 0;
        for (size_t i = 0; i < seq_len; i++)
            acc += scores[i] * V[i * dim + d];
        out[d] = acc;
    return true;
}

    return true;
}

