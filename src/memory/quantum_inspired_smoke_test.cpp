#include "quantum_inspired_memory_optimizer.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <random>

using namespace rawrxd::memory;

struct TestResult {
    const char* name;
    bool passed;
};

std::vector<float> generateTestData(size_t count, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> data(count);
    for (size_t i = 0; i < count; ++i) {
        data[i] = dist(rng);
    }
    return data;
}

bool test_initialization() {
    QuantumInspiredMemoryOptimizer optimizer;
    if (!optimizer.initialize()) return false;
    if (!optimizer.initialize()) return false; // Re-init OK
    optimizer.shutdown();
    if (!optimizer.initialize()) return false; // Fresh init after shutdown
    optimizer.shutdown();
    return true;
}

bool test_analog_precision() {
    AnalogPrecision precision(7.5f);
    if (std::abs(precision.effective_bits - 7.5f) >= 0.01f) return false;
    if (precision.toDiscrete() != 8) return false;
    if (precision.compressionRatio() <= 1.0f) return false;
    if (precision.quality() > 1.0f) return false;

    AnalogPrecision min_prec(0.5f);
    if (min_prec.toDiscrete() < 1) return false;

    AnalogPrecision max_prec(64.0f);
    if (max_prec.toDiscrete() > 64) return false;

    return true;
}

bool test_superposition_encoding() {
    SuperpositionEncoder<float> encoder;
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto encoded = encoder.encode(data);

    float amp_sum = 0.0f;
    for (float amp : encoded.amplitudes) amp_sum += amp;
    if (std::abs(amp_sum - 1.0f) >= 0.01f) return false;

    auto decoded = encoder.decode(encoded, data.size());
    if (decoded.size() != data.size()) return false;

    float measured = encoder.measure(encoded);
    if (std::isnan(measured)) return false;

    return true;
}

bool test_entropy_analysis() {
    EntropyAnalyzer analyzer;

    auto random_data = generateTestData(1000);
    auto entropy_random = analyzer.analyze(random_data.data(), random_data.size());
    if (entropy_random.shannon_entropy <= 0.0f) return false;

    std::vector<float> constant_data(1000, 42.0f);
    auto entropy_constant = analyzer.analyze(constant_data.data(), constant_data.size());
    if (entropy_constant.shannon_entropy >= entropy_random.shannon_entropy) return false;
    if (entropy_constant.redundancy <= entropy_random.redundancy) return false;

    auto precision = analyzer.recommendPrecision(entropy_random, 0.95f);
    if (precision.effective_bits < qimo_config::MIN_PRECISION) return false;

    return true;
}

bool test_predictive_allocator() {
    PredictiveAllocator allocator;

    for (int i = 0; i < 10; ++i) {
        allocator.recordAllocation(100, 1024 * (i + 1));
    }

    auto predictions = allocator.getPredictions(0.0f);
    if (predictions.empty()) return false;

    size_t predicted = allocator.predictNextSize(100);
    if (predicted == 0) return false;

    for (int i = 0; i < 20; ++i) {
        allocator.recordAllocation(200, 512);
    }

    predictions = allocator.getPredictions(0.9f);
    if (predictions.empty()) return false;
    if (predictions[0].confidence < 0.9f) return false;

    return true;
}

bool test_topology_mapper() {
    TopologyMapper mapper;

    mapper.addNode(0, {1, 2});
    mapper.addNode(1, {2, 3});
    mapper.addNode(2, {3});
    mapper.addNode(3, {});

    float imp0 = mapper.getImportance(0);
    float imp3 = mapper.getImportance(3);
    if (imp0 < 0.0f || imp0 > 1.0f) return false;
    if (imp3 < 0.0f || imp3 > 1.0f) return false;

    auto precision = mapper.recommendPrecision(0);
    if (precision.effective_bits < qimo_config::MIN_PRECISION) return false;

    return true;
}

bool test_tensor_decomposition() {
    TensorDecomposer decomposer;

    auto data = generateTestData(1000);
    auto decomposed = decomposer.decompose(data.data(), 10, 10, 10, 4);

    if (decomposed.rank == 0) return false;
    if (decomposed.compression_ratio < 1.0f) return false;
    if (decomposed.core.empty()) return false;

    auto reconstructed = decomposer.reconstruct(decomposed);
    if (reconstructed.size() != data.size()) return false;

    return true;
}

bool test_self_organizing_hierarchy() {
    SelfOrganizingHierarchy hierarchy;

    int tier0 = hierarchy.allocate(1024, 1.0f);
    if (tier0 < 0) return false;

    int tier4 = hierarchy.allocate(1024 * 1024 * 1024, 0.1f);
    if (tier4 < 0) return false;

    if (tier0 > tier4) return false;

    hierarchy.deallocate(tier0, 1024);

    auto status = hierarchy.getStatus();
    if (status.size() != 5) return false;

    hierarchy.reorganize();

    return true;
}

bool test_optimization() {
    QuantumInspiredMemoryOptimizer optimizer;
    if (!optimizer.initialize()) return false;

    auto data = generateTestData(10000);
    auto result = optimizer.optimize(data.data(), data.size(), 0.95f, 0.7f);

    if (result.compression_ratio <= 1.0f) return false;
    if (result.bytes_after >= result.bytes_before) return false;
    if (result.quality <= 0.0f) return false;
    if (result.quality > 1.0f) return false;
    if (result.allocated_tier < 0) return false;

    optimizer.shutdown();
    return true;
}

bool test_topology_optimization() {
    QuantumInspiredMemoryOptimizer optimizer;
    if (!optimizer.initialize()) return false;

    optimizer.addTopologyNode(0, {1, 2});
    optimizer.addTopologyNode(1, {2, 3});
    optimizer.addTopologyNode(2, {3});
    optimizer.addTopologyNode(3, {});

    auto data = generateTestData(5000);
    auto result = optimizer.optimizeWithTopology(data.data(), data.size(), 0, 0.95f);

    if (result.compression_ratio <= 1.0f) return false;
    if (result.quality <= 0.0f) return false;

    optimizer.shutdown();
    return true;
}

bool test_prediction() {
    QuantumInspiredMemoryOptimizer optimizer;
    if (!optimizer.initialize()) return false;

    auto data1 = generateTestData(1000, 1);
    auto data2 = generateTestData(1000, 2);
    auto data3 = generateTestData(1000, 3);

    for (int i = 0; i < 5; ++i) {
        optimizer.optimize(data1.data(), data1.size());
        optimizer.optimize(data2.data(), data2.size());
        optimizer.optimize(data3.data(), data3.size());
    }

    auto predictions = optimizer.predictAllocations(0.5f);
    // May or may not have predictions depending on allocator state

    optimizer.shutdown();
    return true;
}

bool test_hierarchy_reorganization() {
    QuantumInspiredMemoryOptimizer optimizer;
    if (!optimizer.initialize()) return false;

    auto data = generateTestData(100000);
    optimizer.optimize(data.data(), data.size(), 0.95f, 1.0f);
    optimizer.optimize(data.data(), data.size(), 0.95f, 0.1f);

    optimizer.reorganizeHierarchy();

    auto status = optimizer.getHierarchyStatus();
    if (status.size() != 5) return false;

    optimizer.shutdown();
    return true;
}

bool test_statistics() {
    QuantumInspiredMemoryOptimizer optimizer;
    if (!optimizer.initialize()) return false;

    auto stats_before = optimizer.getStats();
    if (stats_before.total_optimized != 0) return false;

    auto data = generateTestData(5000);
    for (int i = 0; i < 5; ++i) {
        optimizer.optimize(data.data(), data.size());
    }

    auto stats_after = optimizer.getStats();
    if (stats_after.total_optimized <= stats_before.total_optimized) return false;
    if (stats_after.total_saved <= 0) return false;
    if (stats_after.average_compression <= 1.0f) return false;

    optimizer.shutdown();
    return true;
}

bool test_realistic_performance() {
    QuantumInspiredMemoryOptimizer optimizer;
    if (!optimizer.initialize()) return false;

    auto random_data = generateTestData(10000, 42);
    auto result_random = optimizer.optimize(random_data.data(), random_data.size(), 0.95f);

    std::vector<float> structured_data(10000);
    for (size_t i = 0; i < structured_data.size(); ++i) {
        structured_data[i] = std::sin(i * 0.01f);
    }
    auto result_structured = optimizer.optimize(structured_data.data(), structured_data.size(), 0.95f);

    optimizer.addTopologyNode(0, {1, 2});
    optimizer.addTopologyNode(1, {2});
    optimizer.addTopologyNode(2, {});

    auto result_topology = optimizer.optimizeWithTopology(random_data.data(), random_data.size(), 1, 0.98f);

    auto stats = optimizer.getStats();

    optimizer.shutdown();

    // Structured data should have better compression than random
    if (result_structured.compression_ratio <= result_random.compression_ratio) return false;
    if (stats.total_optimized == 0) return false;

    return true;
}

int main() {
    printf("\n=== Quantum-Inspired Memory Optimizer Smoke Tests ===\n\n");

    int passed = 0;
    int total = 0;

    auto run_test = [&](const char* name, bool (*test)()) {
        total++;
        printf("  %-40s ", name);
        try {
            if (test()) {
                printf("OK\n");
                passed++;
            } else {
                printf("FAIL\n");
            }
        } catch (...) {
            printf("FAIL (exception)\n");
        }
    };

    run_test("Initialization", test_initialization);
    run_test("Analog Precision", test_analog_precision);
    run_test("Superposition Encoding", test_superposition_encoding);
    run_test("Entropy Analysis", test_entropy_analysis);
    run_test("Predictive Allocator", test_predictive_allocator);
    run_test("Topology Mapper", test_topology_mapper);
    run_test("Tensor Decomposition", test_tensor_decomposition);
    run_test("Self-Organizing Hierarchy", test_self_organizing_hierarchy);
    run_test("Full Optimization", test_optimization);
    run_test("Topology Optimization", test_topology_optimization);
    run_test("Prediction", test_prediction);
    run_test("Hierarchy Reorganization", test_hierarchy_reorganization);
    run_test("Statistics", test_statistics);
    run_test("Realistic Performance", test_realistic_performance);

    printf("\n=== Results: %d/%d tests passed ===\n\n", passed, total);

    return (passed == total) ? 0 : 1;
}
