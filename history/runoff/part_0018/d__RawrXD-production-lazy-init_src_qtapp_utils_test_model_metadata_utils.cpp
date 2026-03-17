#include "model_metadata_utils.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace model_metadata;

    // Filename parsing tests
    auto q = parseQuantFromFilename("ggml-model-q4_1.bin");
    assert(q == "q4_1");

    auto q2 = parseQuantFromFilename("model-mxfp4.gguf");
    assert(q2 == "mxfp4");

    auto p = parseParamCountFromFilename("wizardLM-7B.gguf");
    assert(p.has_value() && *p >= 7000000000ULL - 1000ULL && *p <= 7000000000ULL + 1000ULL);

    auto p2 = parseParamCountFromFilename("small-350M-gguf.bin");
    assert(p2.has_value() && *p2 == 350000000ULL);

    std::cout << "model_metadata_utils tests passed" << std::endl;
    return 0;
}
