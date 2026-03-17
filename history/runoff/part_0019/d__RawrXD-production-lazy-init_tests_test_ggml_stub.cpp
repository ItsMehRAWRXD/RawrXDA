#include <iostream>
#include <cmath>
#include "ggml_stub.hpp"

int main() {
    using namespace RawrXD::GGML;
    Context ctx(1024*1024);

    // Create small 2x3 and 3x2 matrices
    auto* a = ctx.new_tensor_2d(GGML_TYPE_F32, 2, 3);
    auto* b = ctx.new_tensor_2d(GGML_TYPE_F32, 3, 2);
    auto* c = ctx.new_tensor_2d(GGML_TYPE_F32, 2, 2);

    // Fill with simple values
    float* ad = static_cast<float*>(a->data);
    float* bd = static_cast<float*>(b->data);
    for (int i = 0; i < 6; ++i) ad[i] = float(i + 1); // 1..6
    for (int i = 0; i < 6; ++i) bd[i] = float(i + 1); // 1..6

    auto* res = ctx.mul_mat(a, b);
    if (!res) {
        std::cerr << "mul_mat returned null" << std::endl;
        return 2;
    }

    float* rd = static_cast<float*>(res->data);
    std::cout << "Result[0,0]=" << rd[0] << " Expected=" << (1*1 + 2*3 + 3*5) << std::endl;
    // Basic sanity check
    if (std::abs(rd[0] - (1*1 + 2*3 + 3*5)) > 1e-3f) {
        std::cerr << "Incorrect result" << std::endl;
        return 3;
    }

    std::cout << "ggml_stub mul_mat basic smoke test passed" << std::endl;
    return 0;
}
