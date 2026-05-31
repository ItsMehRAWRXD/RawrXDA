#include "SpeculativeOptimizer.h"
#include <iostream>
#include <cassert>

using namespace RawrXD::Inference;

int main() {
    std::cout << "Starting Speculative Decoding Hardware-Aware Test...
";
    auto& spec = SpeculativeOptimizer::GetInstance();

    // 1. Nominal Load (40-80%)
    std::cout << "Testing Nominal Strategy...
";
    spec.AdjustStrategy(0.5f);
    auto res1 = spec.Predict({1, 2, 3});
    assert(res1.tokens.size() == 4);
    assert(res1.accepted == true);

    // 2. High Pressure Load (>80%)
    std::cout << "Testing High Pressure (Safety) Strategy...
";
    spec.AdjustStrategy(0.9f);
    auto res2 = spec.Predict({1, 2, 3});
    assert(res2.tokens.size() == 2);
    // Since confidence is 0.9 and minConfidence in high pressure is 0.95, it should be rejected
    assert(res2.accepted == false);

    // 3. Low Pressure Load (<40%)
    std::cout << "Testing Low Pressure (Performance) Strategy...
";
    spec.AdjustStrategy(0.2f);
    auto res3 = spec.Predict({1, 2, 3});
    assert(res3.tokens.size() == 8);
    assert(res3.accepted == true);

    std::cout << "Speculative Decoding Strategy Verification: PASSED
";
    return 0;
}
