#include "memory/extreme_memory_optimizer.hpp"

#include <iostream>

int main() {
    using namespace rawrxd;

    const auto config = ExtremeMemoryOptimizer::Config::aggressive(8ULL * 1024ULL * 1024ULL * 1024ULL);
    ExtremeMemoryOptimizer optimizer(config);

    ExtremeMemoryOptimizer::ModelInfo model;
    model.num_layers = 32;
    model.num_heads = 32;
    model.head_dim = 128;
    model.hidden_dim = 4096;
    model.intermediate_dim = 11008;
    model.vocab_size = 32000;
    model.max_context_length = 32768;

    const auto plan = optimizer.create_compression_plan(model);
    optimizer.adjust_for_context_length(8192);

    const auto stats = optimizer.get_memory_stats();

    std::cout << "Compression ratio: " << plan.compression_ratio << "x\n";
    std::cout << "Estimated quality: " << plan.estimated_quality << "\n";
    std::cout << "Estimated latency: " << plan.estimated_latency << "x\n";
    std::cout << "Pressure ratio: " << stats.pressure_ratio << "\n";

    if (plan.precision_assignments.empty()) {
        std::cerr << "No precision assignments generated\n";
        return 1;
    }

    return 0;
}
