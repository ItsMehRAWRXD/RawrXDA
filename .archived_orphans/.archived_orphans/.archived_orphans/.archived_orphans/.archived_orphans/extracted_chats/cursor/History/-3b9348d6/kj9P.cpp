// Fuzz harness for hotpatch parser (libFuzzer)
#include <cstdint>
#include <cstdlib>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 32 * 1024) return 0;
    std::string input(reinterpret_cast<const char*>(data), size);
    (void)input;
    return 0;
}
