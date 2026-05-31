// ============================================================================
// build_test_token_stream_ff_bridge.cpp
// Minimal compile test for the bridge integration
// ============================================================================

#include "token_stream_fast_forward_bridge.h"
#include <iostream>

int main() {
    // Create FF controller
    auto ffConfig = RawrXD::UI::FastForwardConfig{};
    auto ffController = std::make_shared<RawrXD::UI::FastForwardController>(ffConfig);

    // Create bridge
    auto bridgeConfig = RawrXD::UI::BridgeConfig{};
    auto bridge = std::make_unique<RawrXD::UI::TokenStreamFFBridge>(
        ffController, bridgeConfig);

    // Verify initial state
    auto stats = bridge->getStats();
    if (stats.isRunning) {
        std::cerr << "FAIL: Bridge should not be running initially\n";
        return 1;
    }

    if (stats.totalTokensKept != 0) {
        std::cerr << "FAIL: Initial kept tokens should be 0\n";
        return 1;
    }

    if (stats.totalTokensSkipped != 0) {
        std::cerr << "FAIL: Initial skipped tokens should be 0\n";
        return 1;
    }

    // Test shutdown safety
    bridge->shutdown();

    std::cout << "PASS: TokenStreamFFBridge compiles and basic lifecycle works\n";
    return 0;
}
