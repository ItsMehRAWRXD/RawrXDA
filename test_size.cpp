#include <cstdio>
#include <cstdint>
#include <cstddef>
#pragma pack(push, 1)
struct FeatureMask { uint64_t lo; uint64_t hi; };
struct LicenseKeyV2 {
    uint32_t magic;         // 4
    uint16_t version;       // 2
    uint16_t reserved;      // 2
    uint64_t hwid;          // 8
    FeatureMask features;   // 16
    uint32_t tier;          // 4
    uint32_t issueDate;     // 4
    uint32_t expiryDate;    // 4
    uint32_t maxModelGB;    // 4
    uint32_t maxContextTokens; // 4
    uint8_t signature[32];  // 32
    uint8_t padding[4];     // 4
};
#pragma pack(pop)
int main() { printf("sizeof LicenseKeyV2 = %zu\n", sizeof(LicenseKeyV2)); }
