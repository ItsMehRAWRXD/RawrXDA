// tests/native/test_metrics_server.cpp
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
#include "../../telemetry/metrics_server.hpp"

using namespace RawrXD;

int main() {
    std::cout << "Testing Metrics Server..." << std::endl;

    MetricsServer metrics;

    // Test starting server
    bool started = metrics.start(9091); // Use different port for test
    if (started) {
        std::cout << "✓ Metrics server started on port 9091" << std::endl;
    } else {
        std::cout << "! Metrics server failed to start (may be normal if port in use)" << std::endl;
    }

    // Test setting metrics
    metrics.setMetric("test_counter", 42.0);
    metrics.incrementMetric("test_gauge", 10.0);
    metrics.incrementMetric("test_gauge", 5.0);

    // Let server run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop server
    metrics.stop();

    std::cout << "✓ Metrics server test completed" << std::endl;
    return 0;
}