#include <cstdint>
#include <cstring>

extern "C" int add_ints(int a, int b) {
    return a + b;
}

extern "C" uint64_t hash64(const char* s) {
    uint64_t h = 1469598103934665603ull;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ull;
    }
    return h;
}

extern "C" void memxor(void* dst, const void* src, size_t n) {
    auto* d = static_cast<uint8_t*>(dst);
    const auto* s = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < n; ++i) d[i] ^= s[i];
}
