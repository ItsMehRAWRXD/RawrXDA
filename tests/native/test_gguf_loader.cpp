// tests/native/test_gguf_loader.cpp
#include <cassert>
#include <iostream>
#include "../../native/gguf_native_loader.hpp"

using namespace RawrXD::Native;

int main() {
    std::cout << "Testing Native GGUF Loader..." << std::endl;

    NativeGGUFLoader loader;

    // Test loading (will fail without actual GGUF file, but tests API)
    bool result = loader.load(L"nonexistent.gguf");
    assert(!result); // Should fail for nonexistent file

    // Test metadata access
    std::string model_name = loader.getMetadata("model_name", std::string("unknown"));
    assert(model_name == "unknown"); // Should return default

    uint32_t vocab_size = loader.getMetadata("vocab_size", 0u);
    assert(vocab_size == 0);

    std::cout << "✓ Native GGUF Loader API test passed" << std::endl;
    return 0;
}