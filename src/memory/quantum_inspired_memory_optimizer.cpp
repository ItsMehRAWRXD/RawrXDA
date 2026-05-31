#include "quantum_inspired_memory_optimizer.hpp"
#include <cstring>
#include <limits>

namespace rawrxd {
namespace memory {

// ============================================================================
// EntropyAnalyzer
// ============================================================================

EntropyAnalyzer::EntropyProfile EntropyAnalyzer::analyze(const float* data, size_t count) {
    if (count == 0 || data == nullptr) {
        return EntropyProfile{};
    }

    EntropyProfile profile;

    std::array<size_t, qimo_config::ENTROPY_BUCKETS> histogram{};
    float min_val = data[0];
    float max_val = data[0];
    for (size_t i = 1; i < count; ++i) {
        min_val = std::min(min_val, data[i]);
        max_val = std::max(max_val, data[i]);
    }
    float range = max_val - min_val;

    if (range > 0.0f) {
        for (size_t i = 0; i < count; ++i) {
            size_t bucket = static_cast<size_t>((data[i] - min_val) / range * (qimo_config::ENTROPY_BUCKETS - 1));
            bucket = std::min(bucket, qimo_config::ENTROPY_BUCKETS - 1);
            histogram[bucket]++;
        }
    }

    profile.shannon_entropy = computeShannonEntropy(histogram, count);
    profile.kolmogorov_estimate = estimateKolmogorov(data, count);
    float max_entropy = std::log2(static_cast<float>(qimo_config::ENTROPY_BUCKETS));
    profile.redundancy = (max_entropy > 0.0f) ? 1.0f - (profile.shannon_entropy / max_entropy) : 0.0f;
    profile.compressibility = profile.redundancy * 0.7f +
                               (1.0f - profile.kolmogorov_estimate / 32.0f) * 0.3f;

    return profile;
}

AnalogPrecision EntropyAnalyzer::recommendPrecision(const EntropyProfile& profile,
                                                       float quality_target) {
    float base_bits = 4.0f + profile.shannon_entropy * 0.5f;
    float quality_factor = 1.0f + (quality_target - 0.95f) * 2.0f;
    base_bits *= quality_factor;
    base_bits = std::clamp(base_bits, qimo_config::MIN_PRECISION, qimo_config::MAX_PRECISION * 32.0f);

    AnalogPrecision precision(base_bits);
    precision.entropy_limit = profile.shannon_entropy;
    precision.reconstruction_error = 1.0f - quality_target;
    return precision;
}

float EntropyAnalyzer::computeShannonEntropy(
    const std::array<size_t, qimo_config::ENTROPY_BUCKETS>& histogram,
    size_t total) {
    float entropy = 0.0f;
    for (size_t count : histogram) {
        if (count > 0) {
            float p = static_cast<float>(count) / total;
            entropy -= p * std::log2(p);
        }
    }
    return entropy;
}

float EntropyAnalyzer::estimateKolmogorov(const float* data, size_t count) {
    size_t distinct = 0;
    std::unordered_map<uint32_t, bool> seen;
    size_t max_len = std::min(count, size_t(16));
    for (size_t len = 1; len <= max_len; ++len) {
        for (size_t i = 0; i + len <= count; i += len) {
            uint32_t hash = 0;
            for (size_t j = 0; j < len; ++j) {
                hash = hash * 31 + static_cast<uint32_t>(data[i + j] * 1000.0f);
            }
            if (!seen.count(hash)) {
                seen[hash] = true;
                distinct++;
            }
        }
    }
    return std::min(32.0f, std::log2(static_cast<float>(distinct + 1)));
}

// ============================================================================
// PredictiveAllocator
// ============================================================================

PredictiveAllocator::PredictiveAllocator()
    : start_time_(std::chrono::steady_clock::now())
{
    history_.reserve(1024);
    predictions_.reserve(64);
}

void PredictiveAllocator::recordAllocation(size_t memory_id, size_t size) {
    auto now = std::chrono::steady_clock::now();
    AllocationHistory& hist = history_[memory_id];
    hist.sizes.push_back(size);
    hist.times.push_back(now);

    float elapsed = std::chrono::duration<float>(now - start_time_).count();
    hist.frequency = (elapsed > 0.0f) ? static_cast<float>(hist.sizes.size()) / elapsed : 0.0f;

    if (hist.sizes.size() >= 2) {
        size_t n = hist.sizes.size();
        size_t window = std::min(size_t(5), n);
        float recent_avg = 0.0f;
        for (size_t i = n - window; i < n; ++i) recent_avg += static_cast<float>(hist.sizes[i]);
        recent_avg /= window;

        float old_avg = 0.0f;
        for (size_t i = 0; i < window && i < n; ++i) old_avg += static_cast<float>(hist.sizes[i]);
        old_avg /= window;

        hist.trend = (old_avg > 0.0f) ? (recent_avg - old_avg) / old_avg : 0.0f;
    }

    detectPatterns(memory_id);
    generatePredictions(memory_id);
}

std::vector<PredictiveAllocator::Prediction> PredictiveAllocator::getPredictions(float min_confidence) {
    std::vector<Prediction> result;
    auto now = std::chrono::steady_clock::now();
    for (const auto& pred : predictions_) {
        if (pred.confidence >= min_confidence && pred.predicted_time > now) {
            result.push_back(pred);
        }
    }
    std::sort(result.begin(), result.end(),
        [](const Prediction& a, const Prediction& b) {
            return a.predicted_time < b.predicted_time;
        });
    return result;
}

size_t PredictiveAllocator::predictNextSize(size_t memory_id) {
    auto it = history_.find(memory_id);
    if (it == history_.end() || it->second.sizes.empty()) return 0;

    const auto& hist = it->second;
    float alpha = 0.3f;
    float predicted = static_cast<float>(hist.sizes.back());
    for (auto rit = hist.sizes.rbegin() + 1; rit != hist.sizes.rend(); ++rit) {
        predicted = alpha * static_cast<float>(*rit) + (1.0f - alpha) * predicted;
    }
    predicted *= (1.0f + hist.trend);
    return static_cast<size_t>(std::max(0.0f, predicted));
}

void PredictiveAllocator::detectPatterns(size_t memory_id) {
    auto it = history_.find(memory_id);
    if (it == history_.end()) return;
    auto& hist = it->second;
    if (hist.sizes.size() < 3) return;

    for (size_t period = 2; period <= hist.sizes.size() / 2; ++period) {
        bool is_periodic = true;
        for (size_t i = period; i < hist.sizes.size(); ++i) {
            if (hist.sizes[i] != hist.sizes[i - period]) {
                is_periodic = false;
                break;
            }
        }
        if (is_periodic) {
            hist.patterns.push_back(period);
            break;
        }
    }
}

void PredictiveAllocator::generatePredictions(size_t memory_id) {
    auto it = history_.find(memory_id);
    if (it == history_.end()) return;
    const auto& hist = it->second;

    Prediction pred;
    pred.memory_id = memory_id;
    pred.predicted_size = predictNextSize(memory_id);
    pred.predicted_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);

    if (!hist.patterns.empty()) {
        pred.confidence = 0.95f;
    } else if (hist.frequency > 1.0f) {
        pred.confidence = 0.85f;
    } else {
        pred.confidence = 0.5f;
    }

    bool found = false;
    for (auto& p : predictions_) {
        if (p.memory_id == memory_id) {
            p = pred;
            found = true;
            break;
        }
    }
    if (!found) predictions_.push_back(pred);
}

// ============================================================================
// TopologyMapper
// ============================================================================

TopologyMapper::TopologyMapper() {
    nodes_.reserve(qimo_config::TOPOLOGY_LAYERS);
}

void TopologyMapper::addNode(int id, const std::vector<int>& neighbors) {
    if (nodes_.find(id) != nodes_.end()) {
        nodes_[id].neighbors = neighbors;
    } else {
        nodes_[id] = Node(id);
        nodes_[id].neighbors = neighbors;
    }
    updateTopology();
}

void TopologyMapper::computeImportance() {
    const int iterations = 20;
    const float damping = 0.85f;

    for (auto& [id, node] : nodes_) {
        node.importance = 1.0f / nodes_.size();
    }

    for (int iter = 0; iter < iterations; ++iter) {
        std::unordered_map<int, float> new_importance;
        for (const auto& [id, node] : nodes_) {
            float contribution = 0.0f;
            for (int neighbor : node.neighbors) {
                auto it = nodes_.find(neighbor);
                if (it != nodes_.end() && !it->second.neighbors.empty()) {
                    contribution += it->second.importance / it->second.neighbors.size();
                }
            }
            new_importance[id] = (1.0f - damping) / nodes_.size() + damping * contribution;
        }
        for (auto& [id, node] : nodes_) {
            node.importance = new_importance[id];
        }
    }

    float max_importance = 0.0f;
    for (const auto& [id, node] : nodes_) {
        max_importance = std::max(max_importance, node.importance);
    }
    if (max_importance > 0.0f) {
        for (auto& [id, node] : nodes_) {
            node.importance /= max_importance;
        }
    }
}

float TopologyMapper::getImportance(int id) const {
    auto it = nodes_.find(id);
    return (it != nodes_.end()) ? it->second.importance : 0.5f;
}

int TopologyMapper::getDepth(int id) const {
    auto it = nodes_.find(id);
    return (it != nodes_.end()) ? it->second.depth : 0;
}

AnalogPrecision TopologyMapper::recommendPrecision(int id) const {
    float importance = getImportance(id);
    int depth = getDepth(id);
    float base_bits = 4.0f + importance * 12.0f - depth * 0.5f;
    base_bits = std::clamp(base_bits, qimo_config::MIN_PRECISION, qimo_config::MAX_PRECISION * 32.0f);
    AnalogPrecision precision(base_bits);
    precision.reconstruction_error = 0.05f * (1.0f - importance);
    return precision;
}

void TopologyMapper::updateTopology() {
    computeImportance();
    computeDepths();
}

void TopologyMapper::computeDepths() {
    std::vector<int> roots;
    std::unordered_map<int, int> in_degree;
    for (const auto& [id, node] : nodes_) in_degree[id] = 0;
    for (const auto& [id, node] : nodes_) {
        for (int neighbor : node.neighbors) {
            if (nodes_.find(neighbor) != nodes_.end()) in_degree[neighbor]++;
        }
    }
    for (const auto& [id, node] : nodes_) {
        if (in_degree[id] == 0) roots.push_back(id);
    }
    for (auto& [id, node] : nodes_) node.depth = 0;
    for (int root : roots) computeDepthDFS(root, 0);
}

void TopologyMapper::computeDepthDFS(int id, int depth) {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) return;
    if (it->second.depth < depth) it->second.depth = depth;
    for (int neighbor : it->second.neighbors) {
        computeDepthDFS(neighbor, depth + 1);
    }
}

// ============================================================================
// TensorDecomposer
// ============================================================================

TensorDecomposer::DecomposedTensor TensorDecomposer::decompose(
    const float* data, size_t d1, size_t d2, size_t d3, size_t target_rank) {
    DecomposedTensor result;
    result.original_shape = {d1, d2, d3};
    result.rank = std::min(target_rank, qimo_config::TENSOR_RANK_MAX);

    size_t total_elements = d1 * d2 * d3;

    float mean = 0.0f;
    for (size_t i = 0; i < total_elements; ++i) mean += data[i];
    mean /= total_elements;

    size_t core_size = result.rank * result.rank * result.rank;
    result.core.resize(core_size);

    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> dist(0, total_elements - 1);
    for (size_t i = 0; i < core_size; ++i) {
        result.core[i] = data[dist(rng)] - mean;
    }

    result.factors.resize(3);
    for (int dim = 0; dim < 3; ++dim) {
        size_t factor_size = result.rank * result.original_shape[dim];
        result.factors[dim].resize(factor_size);
        for (size_t i = 0; i < factor_size; ++i) {
            result.factors[dim][i] = static_cast<float>(rng()) / rng.max() - 0.5f;
        }
    }

    size_t original_bytes = total_elements * sizeof(float);
    size_t compressed_bytes = core_size * sizeof(float);
    for (const auto& factor : result.factors) {
        compressed_bytes += factor.size() * sizeof(float);
    }
    result.compression_ratio = (compressed_bytes > 0)
        ? static_cast<float>(original_bytes) / compressed_bytes : 1.0f;

    return result;
}

std::vector<float> TensorDecomposer::reconstruct(const DecomposedTensor& decomposed) {
    size_t total_elements = decomposed.original_shape[0] *
                           decomposed.original_shape[1] *
                           decomposed.original_shape[2];
    std::vector<float> result(total_elements, 0.0f);
    for (size_t i = 0; i < decomposed.core.size(); ++i) {
        size_t idx = i % total_elements;
        result[idx] += decomposed.core[i] * 0.1f;
    }
    return result;
}

// ============================================================================
// SelfOrganizingHierarchy
// ============================================================================

SelfOrganizingHierarchy::SelfOrganizingHierarchy() {
    tiers_ = {
        MemoryTier(0), MemoryTier(1), MemoryTier(2),
        MemoryTier(3), MemoryTier(4)
    };
    tiers_[0].capacity = 64ULL * 1024 * 1024;
    tiers_[1].capacity = 256ULL * 1024 * 1024;
    tiers_[2].capacity = 1ULL * 1024 * 1024 * 1024;
    tiers_[3].capacity = 4ULL * 1024 * 1024 * 1024;
    tiers_[4].capacity = 16ULL * 1024 * 1024 * 1024;
}

int SelfOrganizingHierarchy::allocate(size_t size, float priority) {
    int tier = selectTier(size, priority);
    if (tier >= 0 && static_cast<size_t>(tier) < tiers_.size()) {
        if (tiers_[tier].used + size <= tiers_[tier].capacity) {
            tiers_[tier].used += size;
            return tier;
        }
    }
    for (int i = static_cast<int>(tiers_.size()) - 1; i >= 0; --i) {
        if (tiers_[i].used + size <= tiers_[i].capacity) {
            tiers_[i].used += size;
            return i;
        }
    }
    return -1;
}

void SelfOrganizingHierarchy::deallocate(int tier, size_t size) {
    if (tier >= 0 && static_cast<size_t>(tier) < tiers_.size()) {
        tiers_[tier].used = (size > tiers_[tier].used) ? 0 : tiers_[tier].used - size;
    }
}

void SelfOrganizingHierarchy::reorganize() {
    for (size_t i = 1; i < tiers_.size(); ++i) {
        float pressure = tiers_[i - 1].utilization();
        if (pressure > 0.9f) {
            size_t to_move = static_cast<size_t>(tiers_[i - 1].capacity * 0.1f);
            if (tiers_[i].used + to_move <= tiers_[i].capacity) {
                tiers_[i].used += to_move;
                tiers_[i - 1].used -= to_move;
            }
        } else if (pressure < 0.5f && tiers_[i].used > 0) {
            size_t to_move = std::min(tiers_[i].used,
                tiers_[i - 1].capacity - tiers_[i - 1].used);
            if (to_move > 0) {
                tiers_[i - 1].used += to_move;
                tiers_[i].used -= to_move;
            }
        }
    }
}

std::vector<SelfOrganizingHierarchy::MemoryTier> SelfOrganizingHierarchy::getStatus() const {
    return tiers_;
}

int SelfOrganizingHierarchy::selectTier(size_t size, float priority) {
    int preferred_tier = static_cast<int>((1.0f - priority) * (tiers_.size() - 1));
    for (int i = preferred_tier; i < static_cast<int>(tiers_.size()); ++i) {
        if (size <= tiers_[i].capacity - tiers_[i].used) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// QuantumInspiredMemoryOptimizer
// ============================================================================

QuantumInspiredMemoryOptimizer::QuantumInspiredMemoryOptimizer()
    : initialized_(false)
    , total_bytes_optimized_(0)
    , total_bytes_saved_(0)
{}

QuantumInspiredMemoryOptimizer::~QuantumInspiredMemoryOptimizer() = default;

bool QuantumInspiredMemoryOptimizer::initialize() {
    if (initialized_) return true;
    entropy_analyzer_ = std::make_unique<EntropyAnalyzer>();
    predictive_allocator_ = std::make_unique<PredictiveAllocator>();
    topology_mapper_ = std::make_unique<TopologyMapper>();
    tensor_decomposer_ = std::make_unique<TensorDecomposer>();
    hierarchy_ = std::make_unique<SelfOrganizingHierarchy>();
    initialized_ = true;
    return true;
}

void QuantumInspiredMemoryOptimizer::shutdown() {
    entropy_analyzer_.reset();
    predictive_allocator_.reset();
    topology_mapper_.reset();
    tensor_decomposer_.reset();
    hierarchy_.reset();
    initialized_ = false;
}

QuantumInspiredMemoryOptimizer::OptimizationResult
QuantumInspiredMemoryOptimizer::optimize(const float* data, size_t count,
                                           float quality_target, float priority) {
    if (!initialized_ || data == nullptr || count == 0) {
        return OptimizationResult();
    }

    OptimizationResult result;
    result.bytes_before = count * sizeof(float);

    auto entropy_profile = entropy_analyzer_->analyze(data, count);
    result.entropy_before = entropy_profile.shannon_entropy;

    auto precision = entropy_analyzer_->recommendPrecision(entropy_profile, quality_target);

    SuperpositionEncoder<float> encoder;
    std::vector<float> data_vec(data, data + count);
    auto encoded = encoder.encode(data_vec);

    auto decomposed = tensor_decomposer_->decompose(
        data, count, 1, 1, static_cast<size_t>(precision.effective_bits));

    result.allocated_tier = hierarchy_->allocate(
        static_cast<size_t>(result.bytes_before / decomposed.compression_ratio),
        priority);

    predictive_allocator_->recordAllocation(
        reinterpret_cast<size_t>(data), result.bytes_before);

    result.compression_ratio = decomposed.compression_ratio * precision.compressionRatio();
    result.bytes_after = static_cast<size_t>(result.bytes_before / result.compression_ratio);
    result.quality = precision.quality() * (1.0f - entropy_profile.redundancy);
    result.latency_estimate = 1.0f + (result.allocated_tier * 0.1f);
    result.entropy_after = entropy_profile.shannon_entropy * (1.0f - entropy_profile.compressibility);

    auto predictions = predictive_allocator_->getPredictions(0.5f);
    if (!predictions.empty()) {
        result.prediction_confidence = predictions[0].confidence;
    }

    total_bytes_optimized_.fetch_add(result.bytes_before);
    total_bytes_saved_.fetch_add(result.bytes_before - result.bytes_after);

    return result;
}

QuantumInspiredMemoryOptimizer::OptimizationResult
QuantumInspiredMemoryOptimizer::optimizeWithTopology(const float* data, size_t count,
                                                       int layer_id, float quality_target) {
    if (!initialized_) return OptimizationResult();

    auto precision = topology_mapper_->recommendPrecision(layer_id);
    float importance = topology_mapper_->getImportance(layer_id);
    float adjusted_quality = quality_target * importance;
    return optimize(data, count, adjusted_quality, importance);
}

std::vector<PredictiveAllocator::Prediction>
QuantumInspiredMemoryOptimizer::predictAllocations(float min_confidence) {
    if (!initialized_) return {};
    return predictive_allocator_->getPredictions(min_confidence);
}

void QuantumInspiredMemoryOptimizer::reorganizeHierarchy() {
    if (initialized_ && hierarchy_) {
        hierarchy_->reorganize();
    }
}

std::vector<SelfOrganizingHierarchy::MemoryTier>
QuantumInspiredMemoryOptimizer::getHierarchyStatus() const {
    if (initialized_ && hierarchy_) {
        return hierarchy_->getStatus();
    }
    return {};
}

void QuantumInspiredMemoryOptimizer::addTopologyNode(int id, const std::vector<int>& neighbors) {
    if (initialized_ && topology_mapper_) {
        topology_mapper_->addNode(id, neighbors);
    }
}

QuantumInspiredMemoryOptimizer::Stats QuantumInspiredMemoryOptimizer::getStats() const {
    Stats stats;
    stats.total_optimized = total_bytes_optimized_.load();
    stats.total_saved = total_bytes_saved_.load();
    stats.average_compression = (stats.total_optimized > 0)
        ? static_cast<float>(stats.total_optimized) /
          (stats.total_optimized - stats.total_saved + 1) : 1.0f;
    return stats;
}

} // namespace memory
} // namespace rawrxd
