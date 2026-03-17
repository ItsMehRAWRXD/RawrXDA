#include <cstdint>
#include <vector>

// External MASM function prototype
extern "C" int deflate_brutal_masm(
    const uint8_t* in_data,
    int in_len,
    uint8_t* out_data,
    int out_max
);

int main() {
    // Dummy data
    std::vector<uint8_t> in_data(1024, 0x41); // 1KB of 'A's
    std::vector<uint8_t> out_data(2048);

    // Call the MASM function
    int result = deflate_brutal_masm(
        in_data.data(),
        in_data.size(),
        out_data.data(),
        out_data.size()
    );

    // Check the result (basic check)
    if (result > 0) {
        return 0; // Success
    }

    return 1; // Failure
}
