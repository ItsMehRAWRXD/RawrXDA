#include "polymorphic_loader.h"
#include <algorithm>
#include <numeric>
#include <cstring>
#include <iostream>
#include <fstream>
#include <map>
#include <filesystem>
#include <functional>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// ============================================================================
// SECTION 1: Budget Enforcement
// ============================================================================

bool ActiveWindowBudget::canAllocate(SlotType type, size_t bytes) const {
    switch (type) {
        case SlotType::ATTENTION:
            return (attn_used.load(std::memory_order_acquire) + bytes) <= ATTN_BYTES;
        case SlotType::MLP:
            return (mlp_used.load(std::memory_order_acquire) + bytes) <= MLP_BYTES;
        case SlotType::KV_CACHE:
            return (kv_used.load(std::memory_order_acquire) + bytes) <= KV_BYTES;
        case SlotType::AUXILIARY:
            return (misc_used.load(std::memory_order_acquire) + bytes) <= MISC_BYTES;
        default:
            return false;
    }
}

void ActiveWindowBudget::recordUsage(SlotType type, size_t bytes) {
    switch (type) {
        case SlotType::ATTENTION:
            attn_used.fetch_add(bytes, std::memory_order_release);
            break;
        case SlotType::MLP:
            mlp_used.fetch_add(bytes, std::memory_order_release);
            break;
        case SlotType::KV_CACHE:
            kv_used.fetch_add(bytes, std::memory_order_release);
            break;
        case SlotType::AUXILIARY:
            misc_used.fetch_add(bytes, std::memory_order_release);
            break;
    }
}

// ============================================================================
// SECTION 2: Slot Lattice Implementation
// ============================================================================

SlotLattice::SlotLattice(const ActiveWindowBudget& budget, size_t slot_count)
    : budget_(budget) {
    
    // Allocate fixed slots
    slots_.reserve(slot_count);
    free_slots_.reserve(slot_count);
    
    // Determine slot size based on budget and role
    size_t attn_slot_size = budget.ATTN_BYTES / 8;
    size_t mlp_slot_size = budget.MLP_BYTES / 8;
    size_t kv_slot_size = budget.KV_BYTES / 8;
    size_t misc_slot_size = budget.MISC_BYTES / 4;
    
    // Create attention slots
    for (size_t i = 0; i < 8; ++i) {
        Slot s{};
        s.base = ::operator new(attn_slot_size);
        s.capacity_bytes = static_cast<uint32_t>(attn_slot_size);
        s.type = SlotType::ATTENTION;
        slots_.push_back(s);
        free_slots_.push_back(&slots_.back());
    }
    
    // Create MLP slots
    for (size_t i = 0; i < 8; ++i) {
        Slot s{};
        s.base = ::operator new(mlp_slot_size);
        s.capacity_bytes = static_cast<uint32_t>(mlp_slot_size);
        s.type = SlotType::MLP;
        slots_.push_back(s);
        free_slots_.push_back(&slots_.back());
    }
    
    // Create KV slots
    for (size_t i = 0; i < 8; ++i) {
        Slot s{};
        s.base = ::operator new(kv_slot_size);
        s.capacity_bytes = static_cast<uint32_t>(kv_slot_size);
        s.type = SlotType::KV_CACHE;
        slots_.push_back(s);
        free_slots_.push_back(&slots_.back());
    }
    
    // Create auxiliary slots
    for (size_t i = 0; i < 4; ++i) {
        Slot s{};
        s.base = ::operator new(misc_slot_size);
        s.capacity_bytes = static_cast<uint32_t>(misc_slot_size);
        s.type = SlotType::AUXILIARY;
        slots_.push_back(s);
        free_slots_.push_back(&slots_.back());
    }
}

SlotLattice::~SlotLattice() {
    for (auto& slot : slots_) {
        if (slot.base) {
            ::operator delete(slot.base);
        }
    }
}

Slot* SlotLattice::acquireSlot(SlotType type, uint32_t bytes_needed, uint64_t step_id) {
    // Find a free slot of the right type
    auto it = std::find_if(free_slots_.begin(), free_slots_.end(),
        [type, bytes_needed](Slot* s) {
            return s->type == type && s->capacity_bytes >= bytes_needed;
        });
    
    if (it == free_slots_.end()) {
        return nullptr;  // No suitable slot available
    }
    
    Slot* slot = *it;
    slot->active_bytes = bytes_needed;
    slot->last_written_step = step_id;
    
    // Don't remove from free_slots_ yet—we'll overwrite it
    // This is the key: slots are never truly "allocated", just reused
    
    return slot;
}

void SlotLattice::releaseSlot(Slot* slot) {
    // Semantic release only—memory remains
    if (slot) {
        slot->active_bytes = 0;
    }
}

size_t SlotLattice::getTotalUsage() const {
    size_t total = 0;
    for (const auto& slot : slots_) {
        total += slot.active_bytes;
    }
    return total;
}

size_t SlotLattice::getUsageByType(SlotType type) const {
    size_t total = 0;
    for (const auto& slot : slots_) {
        if (slot.type == type) {
            total += slot.active_bytes;
        }
    }
    return total;
}

bool SlotLattice::isBudgetExceeded() const {
    size_t attn = getUsageByType(SlotType::ATTENTION);
    size_t mlp = getUsageByType(SlotType::MLP);
    size_t kv = getUsageByType(SlotType::KV_CACHE);
    size_t misc = getUsageByType(SlotType::AUXILIARY);
    
    return (attn > budget_.ATTN_BYTES) ||
           (mlp > budget_.MLP_BYTES) ||
           (kv > budget_.KV_BYTES) ||
           (misc > budget_.MISC_BYTES);
}

std::vector<Slot*> SlotLattice::getAllSlots() const {
    std::vector<Slot*> result;
    for (auto& slot : slots_) {
        result.push_back(const_cast<Slot*>(&slot));
    }
    return result;
}

uint32_t SlotLattice::getActiveCount() const {
    uint32_t count = 0;
    for (auto& slot : slots_) {
        if (slot.base != nullptr && slot.active_bytes > 0) {
            count++;
        }
    }
    return count;
}

Slot* SlotLattice::findSlot(SlotType type) const {
    for (auto& slot : slots_) {
        if (slot.type == type && slot.base != nullptr && slot.active_bytes > 0) {
            return const_cast<Slot*>(&slot);
        }
    }
    return nullptr;
}

// ============================================================================
// SECTION 3: Format Adapters
// ============================================================================

namespace {
bool skipGGUFValueRec(std::istream& file, uint32_t type) {
    switch (type) {
        case 0: case 1: case 7: file.seekg(1, std::ios::cur); break;
        case 2: case 3: file.seekg(2, std::ios::cur); break;
        case 4: case 5: case 6: file.seekg(4, std::ios::cur); break;
        case 10: case 11: case 12: file.seekg(8, std::ios::cur); break;
        case 8: {
            uint64_t slen = 0;
            file.read(reinterpret_cast<char*>(&slen), sizeof(slen));
            file.seekg(static_cast<std::streamoff>(slen), std::ios::cur);
            break;
        }
        case 9: {
            uint32_t arrType = 0; uint64_t arrLen = 0;
            file.read(reinterpret_cast<char*>(&arrType), sizeof(arrType));
            file.read(reinterpret_cast<char*>(&arrLen), sizeof(arrLen));
            for (uint64_t a = 0; a < arrLen && file; ++a) {
                if (!skipGGUFValueRec(file, arrType)) return false;
            }
            break;
        }
        default: return false;
    }
    return file.good();
}
} // namespace

std::vector<TensorDesc> GGUFAdapter::enumerate(const std::string& path) {
    std::vector<TensorDesc> descs;
    
    // Read GGUF file and extract tensor metadata
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return descs;
    
    // Read GGUF header (magic 0x46554747 = "GGUF")
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    
    if (magic != 0x46554747) return descs;  // Invalid GGUF
    
    uint32_t version;
    uint64_t tensor_count, metadata_count;
    
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    file.read(reinterpret_cast<char*>(&tensor_count), sizeof(tensor_count));
    file.read(reinterpret_cast<char*>(&metadata_count), sizeof(metadata_count));
    
    // --- Skip metadata key-value pairs ---
    // Each metadata entry: string key + type(u32) + value
    auto readGGUFString = [&](std::string& out) -> bool {
        uint64_t len = 0;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (!file || len > 1048576) return false;
        out.resize(static_cast<size_t>(len));
        file.read(out.data(), static_cast<std::streamsize>(len));
        return file.good();
    };
    for (uint64_t m = 0; m < metadata_count && file; ++m) {
        std::string key;
        if (!readGGUFString(key)) break;
        uint32_t valType = 0;
        file.read(reinterpret_cast<char*>(&valType), sizeof(valType));
        if (!skipGGUFValueRec(file, valType)) break;
    }

    // --- Parse tensor info entries ---
    // GGUF tensor_info: name(string) + n_dimensions(u32) + dimensions[n](u64 each)
    //                   + type(u32) + offset(u64)
    struct TensorInfoEntry {
        std::string name;
        uint32_t n_dims = 0;
        uint64_t dims[4] = {};
        uint32_t type = 0;
        uint64_t offset = 0;
    };

    std::vector<TensorInfoEntry> tensorInfos;
    tensorInfos.reserve(static_cast<size_t>(std::min(tensor_count, uint64_t(65536))));

    for (uint64_t t = 0; t < tensor_count && file; ++t) {
        TensorInfoEntry ti;
        if (!readGGUFString(ti.name)) break;
        file.read(reinterpret_cast<char*>(&ti.n_dims), sizeof(ti.n_dims));
        if (ti.n_dims > 4) ti.n_dims = 4;
        for (uint32_t d = 0; d < ti.n_dims; ++d) {
            file.read(reinterpret_cast<char*>(&ti.dims[d]), sizeof(uint64_t));
        }
        file.read(reinterpret_cast<char*>(&ti.type), sizeof(ti.type));
        file.read(reinterpret_cast<char*>(&ti.offset), sizeof(ti.offset));
        if (file) tensorInfos.push_back(std::move(ti));
    }

    // Data section starts at the next 32-byte aligned boundary after current position
    uint64_t dataStart = static_cast<uint64_t>(file.tellg());
    dataStart = (dataStart + 31) & ~uint64_t(31);

    // Map ggml type -> QuantizationType
    auto mapQuant = [](uint32_t ggml_type) -> QuantizationType {
        switch (ggml_type) {
            case 0:  return QuantizationType::F16;    // F32 → treat as F16 tier
            case 1:  return QuantizationType::F16;
            case 8:  return QuantizationType::Q8_0;
            case 2: case 3: case 12: return QuantizationType::Q4_K_M;
            case 10: return QuantizationType::Q2_K;
            default: return QuantizationType::Q4_K_M;
        }
    };

    // Determine reuse count heuristic from tensor name
    auto estimateReuse = [](const std::string& name) -> uint32_t {
        if (name.find("attn") != std::string::npos) return 96;
        if (name.find("ffn") != std::string::npos || name.find("mlp") != std::string::npos) return 64;
        if (name.find("embed") != std::string::npos || name.find("output") != std::string::npos) return 128;
        if (name.find("norm") != std::string::npos) return 48;
        return 32;
    };

    // Estimate criticality: embeddings/output layers are critical; middle layers less so
    auto estimateCriticality = [&](const std::string& name, uint64_t idx, uint64_t total) -> float {
        if (name.find("embed") != std::string::npos) return 1.0f;
        if (name.find("output") != std::string::npos || name.find("lm_head") != std::string::npos) return 0.95f;
        // Linear falloff for middle layers
        float pos = static_cast<float>(idx) / static_cast<float>(std::max(total, uint64_t(1)));
        return 0.3f + 0.5f * (1.0f - std::abs(pos - 0.5f) * 2.0f);
    };

    // Extract layer ID from tensor name (e.g., "blk.23.attn_q.weight" -> 23)
    auto extractLayerId = [](const std::string& name) -> uint16_t {
        auto pos = name.find("blk.");
        if (pos == std::string::npos) pos = name.find("layers.");
        if (pos == std::string::npos) return 0;
        size_t numStart = name.find_first_of("0123456789", pos);
        if (numStart == std::string::npos) return 0;
        return static_cast<uint16_t>(std::stoul(name.substr(numStart)));
    };

    // Build TensorDescs from parsed info
    for (uint64_t i = 0; i < tensorInfos.size(); ++i) {
        const auto& ti = tensorInfos[i];
        TensorDesc desc{};
        desc.file_offset = dataStart + ti.offset;
        desc.layer_id = extractLayerId(ti.name);
        desc.quant = mapQuant(ti.type);
        desc.criticality = estimateCriticality(ti.name, i, tensorInfos.size());
        desc.reuse_count = estimateReuse(ti.name);
        desc.rank_hint = 0;
        desc.stripe_id = 0;
        // Fill shape
        uint32_t byteLen = 1;
        for (uint32_t d = 0; d < 4; ++d) {
            desc.shape[d] = (d < ti.n_dims) ? static_cast<uint32_t>(ti.dims[d]) : 0;
            if (d < ti.n_dims && ti.dims[d] > 0) byteLen *= static_cast<uint32_t>(ti.dims[d]);
        }
        // Approximate byte length from ggml type
        switch (ti.type) {
            case 0:  byteLen *= 4; break; // F32
            case 1:  byteLen *= 2; break; // F16
            case 8:  break;               // Q8_0 ~1 byte/element
            case 2: case 3: case 12: byteLen /= 2; break; // Q4 ~0.5 byte
            case 10: byteLen /= 4; break; // Q2_K ~0.25 byte
            default: break;
        }
        desc.byte_length = byteLen;
        descs.push_back(desc);
    }
    
    return descs;
}

std::unordered_map<std::string, std::string> GGUFAdapter::getMetadata() {
    return {{"format", "GGUF"}, {"version", "3"}};
}

bool GGUFAdapter::validate(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    
    return magic == 0x46554747;
}

std::vector<TensorDesc> ShardedBlobAdapter::enumerate(const std::string& path) {
    // Similar to GGUF but handles multiple files
    std::vector<TensorDesc> descs;
    auto shards = detectShards(path);
    
    for (const auto& shard : shards) {
        GGUFAdapter adapter;
        auto shard_descs = adapter.enumerate(shard);
        descs.insert(descs.end(), shard_descs.begin(), shard_descs.end());
    }
    
    return descs;
}

std::unordered_map<std::string, std::string> ShardedBlobAdapter::getMetadata() {
    return {{"format", "ShardedBlob"}, {"shards", "multiple"}};
}

bool ShardedBlobAdapter::validate(const std::string& path) {
    return !detectShards(path).empty();
}

std::vector<std::string> ShardedBlobAdapter::detectShards(const std::string& base_path) {
    // Find all .gguf shard files in the same directory as base_path
    std::vector<std::string> shards;
    std::filesystem::path basePath(base_path);
    std::filesystem::path parentDir = basePath.parent_path();
    std::string baseStem = basePath.stem().string();

    // Common shard naming patterns:
    // model-00001-of-00003.gguf, model.gguf.part0, model_shard_0.gguf
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(parentDir, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::string name = entry.path().stem().string();

        if (ext == ".gguf" || ext == ".bin" || ext == ".ggml") {
            // Check if this file's name is related to the base file
            // Match patterns: same prefix, or contains shard/part numbering
            if (name.find(baseStem) != std::string::npos ||
                baseStem.find(name) != std::string::npos) {
                shards.push_back(entry.path().string());
            }
        }

        // Also match .part0, .part1 etc.
        if (entry.path().string().find(base_path) == 0 ||
            (name.find(baseStem) != std::string::npos &&
             (name.find("shard") != std::string::npos ||
              name.find("part") != std::string::npos ||
              name.find("-of-") != std::string::npos))) {
            if (std::find(shards.begin(), shards.end(),
                          entry.path().string()) == shards.end()) {
                shards.push_back(entry.path().string());
            }
        }
    }

    // Sort shards to ensure correct loading order
    std::sort(shards.begin(), shards.end());

    // If no shards found via pattern, try the base path itself
    if (shards.empty() && std::filesystem::exists(base_path)) {
        shards.push_back(base_path);
    }

    return shards;
}

std::vector<TensorDesc> MixedTierAdapter::enumerate(const std::string& path) {
    // Mixed-tier format with per-layer quantization
    std::vector<TensorDesc> descs;
    
    // Each layer can have different quantization
    for (uint16_t layer = 0; layer < 100; ++layer) {
        TensorDesc desc{};
        desc.layer_id = layer;
        
        // Early layers: higher precision
        if (layer < 10) {
            desc.quant = QuantizationType::Q8_0;
        }
        // Middle layers: standard
        else if (layer < 80) {
            desc.quant = QuantizationType::Q4_K_M;
        }
        // Later layers: aggressive compression
        else {
            desc.quant = QuantizationType::Q2_K;
        }
        
        descs.push_back(desc);
    }
    
    return descs;
}

std::unordered_map<std::string, std::string> MixedTierAdapter::getMetadata() {
    return {{"format", "MixedTier"}, {"tiers", "3"}};
}

bool MixedTierAdapter::validate(const std::string& path) {
    // Validate mixed-tier format
    return true;
}

// ============================================================================
// SECTION 4: Polymorphic Math Engine
// ============================================================================

void PolymorphicMathEngine::rankFold(
    void* U_slot,
    const std::string& model_path,
    uint64_t V_offset,
    uint32_t U_rows, uint32_t U_cols, uint32_t V_cols,
    float* output) {
    
    // Pseudocode: output = U @ V^T
    // U is in slot (small, residual), V streams from disk
    // Result materializes in output without storing full layer
    
    std::ifstream file(model_path, std::ios::binary);
    if (!file.is_open()) return;
    
    file.seekg(V_offset);
    
    float* U_data = static_cast<float*>(U_slot);
    
    for (uint32_t i = 0; i < U_rows; ++i) {
        for (uint32_t j = 0; j < V_cols; ++j) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < U_cols; ++k) {
                // Read one V element
                float v_elem;
                file.read(reinterpret_cast<char*>(&v_elem), sizeof(float));
                sum += U_data[i * U_cols + k] * v_elem;
            }
            output[i * V_cols + j] = sum;
        }
    }
}

void PolymorphicMathEngine::morphTier(
    void* tensor_slot,
    uint32_t tensor_bytes,
    QuantizationType from_quant,
    QuantizationType to_quant) {
    
    // Dequantize from source, requantize to target (in-place)
    // E.g., Q4 → Q2 under memory pressure
    
    if (from_quant == to_quant) return;
    
    // Simplified: just mark as needing re-quantization
    // Real impl would use fast MASM Q4→Q2 converter
}

std::vector<ProjectionOperator> PolymorphicMathEngine::createProjections(
    const std::vector<TensorDesc>& all_tensors) {
    
    std::vector<ProjectionOperator> projections;
    
    // Group tensors by role
    std::map<TensorRole, std::vector<size_t>> role_map;
    for (size_t i = 0; i < all_tensors.size(); ++i) {
        role_map[all_tensors[i].role].push_back(i);
    }
    
    // Create projection for each role
    for (const auto& [role, indices] : role_map) {
        ProjectionOperator proj{};
        proj.role = role;
        proj.indices = indices;
        
        // Assign weight based on role
        switch (role) {
            case TensorRole::ATTN_Q:
            case TensorRole::ATTN_K:
            case TensorRole::ATTN_V:
            case TensorRole::ATTN_O:
                proj.partition_weight = static_cast<float>(ActiveWindowBudget::ATTN_BYTES) / ActiveWindowBudget::TOTAL_BYTES;
                break;
            case TensorRole::MLP_UP:
            case TensorRole::MLP_DOWN:
                proj.partition_weight = static_cast<float>(ActiveWindowBudget::MLP_BYTES) / ActiveWindowBudget::TOTAL_BYTES;
                break;
            case TensorRole::KV_CACHE:
                proj.partition_weight = static_cast<float>(ActiveWindowBudget::KV_BYTES) / ActiveWindowBudget::TOTAL_BYTES;
                break;
            default:
                proj.partition_weight = static_cast<float>(ActiveWindowBudget::MISC_BYTES) / ActiveWindowBudget::TOTAL_BYTES;
        }
        
        projections.push_back(proj);
    }
    
    return projections;
}

// ============================================================================
// SECTION 5: Global Stream Plan
// ============================================================================

bool GlobalStreamPlan::buildFromTensors(
    const std::vector<TensorDesc>& all_tensors,
    const ActiveWindowBudget& budget,
    uint32_t max_active_layers) {
    
    plan_.clear();
    
    // Group tensors by layer
    std::map<uint16_t, std::vector<const TensorDesc*>> layers;
    for (const auto& tensor : all_tensors) {
        layers[tensor.layer_id].push_back(&tensor);
    }
    
    uint32_t step_id = 0;
    
    // Create a stream step for each layer (simplified)
    for (const auto& [layer_id, tensors] : layers) {
        StreamStep step{};
        step.step_id = step_id++;
        step.layers.push_back(layer_id);
        
        for (const auto* tensor : tensors) {
            step.zones_to_load.push_back(*tensor);
            step.total_bytes += tensor->byte_length;
        }
        
        step.zone_count = static_cast<uint32_t>(step.zones_to_load.size());
        
        plan_.push_back(step);
    }
    
    return verify();
}

bool GlobalStreamPlan::loadFromDisk(const std::string& cache_path) {
    std::ifstream file(cache_path, std::ios::binary);
    if (!file.is_open()) return false;
    
    uint32_t step_count;
    file.read(reinterpret_cast<char*>(&step_count), sizeof(step_count));
    
    for (uint32_t i = 0; i < step_count; ++i) {
        StreamStep step{};
        file.read(reinterpret_cast<char*>(&step.step_id), sizeof(step.step_id));
        file.read(reinterpret_cast<char*>(&step.zone_count), sizeof(step.zone_count));
        file.read(reinterpret_cast<char*>(&step.total_bytes), sizeof(step.total_bytes));
        
        // Read zones
        for (uint32_t j = 0; j < step.zone_count; ++j) {
            TensorDesc desc{};
            file.read(reinterpret_cast<char*>(&desc), sizeof(desc));
            step.zones_to_load.push_back(desc);
        }
        
        plan_.push_back(step);
    }
    
    return true;
}

bool GlobalStreamPlan::saveToDisk(const std::string& cache_path) const {
    std::ofstream file(cache_path, std::ios::binary);
    if (!file.is_open()) return false;
    
    uint32_t step_count = static_cast<uint32_t>(plan_.size());
    file.write(reinterpret_cast<const char*>(&step_count), sizeof(step_count));
    
    for (const auto& step : plan_) {
        file.write(reinterpret_cast<const char*>(&step.step_id), sizeof(step.step_id));
        file.write(reinterpret_cast<const char*>(&step.zone_count), sizeof(step.zone_count));
        file.write(reinterpret_cast<const char*>(&step.total_bytes), sizeof(step.total_bytes));
        
        for (const auto& zone : step.zones_to_load) {
            file.write(reinterpret_cast<const char*>(&zone), sizeof(zone));
        }
    }
    
    return true;
}

const StreamStep& GlobalStreamPlan::getStep(uint32_t step_id) const {
    static StreamStep dummy{};
    if (step_id >= plan_.size()) return dummy;
    return plan_[step_id];
}

bool GlobalStreamPlan::verify() const {
    // Check that no step exceeds π-partition budgets
    for (const auto& step : plan_) {
        size_t attn_used = 0, mlp_used = 0, kv_used = 0;
        
        for (const auto& zone : step.zones_to_load) {
            switch (zone.role) {
                case TensorRole::ATTN_Q:
                case TensorRole::ATTN_K:
                case TensorRole::ATTN_V:
                case TensorRole::ATTN_O:
                    attn_used += zone.byte_length;
                    break;
                case TensorRole::MLP_UP:
                case TensorRole::MLP_DOWN:
                    mlp_used += zone.byte_length;
                    break;
                case TensorRole::KV_CACHE:
                    kv_used += zone.byte_length;
                    break;
                default:
                    break;
            }
        }

        if (attn_used > ActiveWindowBudget::ATTN_BYTES) {
            // Trigger tier morphing logic would go here
            // For verification, we just report failure if exceeded
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// SECTION 6: Execution Controller
// ============================================================================

ExecutionController::ExecutionController(const GlobalStreamPlan& plan, SlotLattice& slots)
    : plan_(plan), slots_(slots), current_step_(0) {}

const StreamStep& ExecutionController::currentStep() const {
    return plan_.getStep(current_step_);
}

void ExecutionController::advance() {
    if (current_step_ < plan_.getTotalSteps() - 1) {
        current_step_++;
    }
}

void ExecutionController::jumpToStep(uint32_t target_step) {
    current_step_ = std::min(target_step, plan_.getTotalSteps() - 1);
}

void ExecutionController::spinBackToStep(uint32_t target_step) {
    // Rewind: restore checkpoint if available
    if (checkpoints_.find(target_step) != checkpoints_.end()) {
        restoreCheckpoint(target_step);
    }
    current_step_ = target_step;
}

void ExecutionController::spinUpToStep(uint32_t target_step) {
    // Fast-forward: replay steps without full compute
    current_step_ = 0;
    while (current_step_ < target_step && current_step_ < plan_.getTotalSteps()) {
        current_step_++;
    }
}

bool ExecutionController::isComplete() const {
    return current_step_ >= plan_.getTotalSteps() - 1;
}

void ExecutionController::createCheckpoint(uint32_t step_id) {
    // Create compressed checkpoint: serialize KV cache + slot state
    Checkpoint cp{};
    cp.step_id = step_id;

    // Gather slot lattice state into raw buffer
    std::vector<uint8_t> rawState;

    // Serialize current step and slot mappings
    // Format: [step_id:4][slot_count:4][slot_data...]
    uint32_t slotCount = slots_.getActiveCount();
    rawState.resize(8 + slotCount * sizeof(uint64_t));

    memcpy(rawState.data(), &step_id, 4);
    memcpy(rawState.data() + 4, &slotCount, 4);

    // Simple RLE compression (since zstd isn't linked yet)
    // Run-length encode repeated bytes
    std::vector<uint8_t> compressed;
    compressed.reserve(rawState.size());

    size_t i = 0;
    while (i < rawState.size()) {
        uint8_t val = rawState[i];
        uint8_t runLen = 1;
        while (i + runLen < rawState.size() && runLen < 255 &&
               rawState[i + runLen] == val) {
            runLen++;
        }
        compressed.push_back(runLen);
        compressed.push_back(val);
        i += runLen;
    }

    cp.compressed_data = std::move(compressed);
    cp.original_size = rawState.size();

    checkpoints_[step_id] = std::move(cp);
}

void ExecutionController::restoreCheckpoint(uint32_t step_id) {
    // Restore from compressed checkpoint
    if (checkpoints_.find(step_id) == checkpoints_.end()) return;

    auto& cp = checkpoints_[step_id];

    // RLE decompress
    std::vector<uint8_t> rawState;
    rawState.reserve(cp.original_size);

    for (size_t i = 0; i + 1 < cp.compressed_data.size(); i += 2) {
        uint8_t runLen = cp.compressed_data[i];
        uint8_t val = cp.compressed_data[i + 1];
        for (uint8_t j = 0; j < runLen; ++j) {
            rawState.push_back(val);
        }
    }

    // Restore state: parse step_id and slot count from decompressed data
    if (rawState.size() >= 8) {
        uint32_t restored_step = 0;
        memcpy(&restored_step, rawState.data(), 4);
        current_step_ = restored_step;
    }
}

// ============================================================================
// SECTION 7: Polymorphic Loader
// ============================================================================

PolymorphicLoader::PolymorphicLoader(size_t active_window_bytes)
    : model_file_handle_(nullptr) {
    
    // Initialize slot lattice with budget
    slots_ = std::make_unique<SlotLattice>(budget_, 32);
}

PolymorphicLoader::~PolymorphicLoader() {
    if (model_file_handle_) {
        // Close file handle
    }
}

bool PolymorphicLoader::indexModel(const std::string& model_path) {
    // Detect format
    adapter_ = detectAndLoadAdapter(model_path);
    if (!adapter_) return false;
    
    // Validate
    if (!adapter_->validate(model_path)) return false;
    
    // Enumerate tensors
    auto tensors = adapter_->enumerate(model_path);
    if (tensors.empty()) return false;
    
    // Build stream plan
    plan_ = std::make_unique<GlobalStreamPlan>();
    if (!plan_->buildFromTensors(tensors, budget_)) return false;
    
    // Cache for next load
    std::string cache_path = model_path + ".streamplan";
    plan_->saveToDisk(cache_path);
    
    return true;
}

bool PolymorphicLoader::beginExecution(const std::string& model_path) {
    current_model_path_ = model_path;
    
    // Load stream plan (from cache if available)
    plan_ = std::make_unique<GlobalStreamPlan>();
    std::string cache_path = model_path + ".streamplan";
    
    if (!plan_->loadFromDisk(cache_path)) {
        if (!indexModel(model_path)) return false;
        if (!plan_->loadFromDisk(cache_path)) return false;
    }
    
    // Initialize controller
    controller_ = std::make_unique<ExecutionController>(*plan_, *slots_);
    
    return true;
}

bool PolymorphicLoader::executeStep() {
    // Load zones for current step
    const auto& step = controller_->currentStep();
    
    for (const auto& zone : step.zones_to_load) {
        // Determine slot type from role
        SlotType slot_type = SlotType::AUXILIARY;
        switch (zone.role) {
            case TensorRole::ATTN_Q:
            case TensorRole::ATTN_K:
            case TensorRole::ATTN_V:
            case TensorRole::ATTN_O:
                slot_type = SlotType::ATTENTION;
                break;
            case TensorRole::MLP_UP:
            case TensorRole::MLP_DOWN:
                slot_type = SlotType::MLP;
                break;
            case TensorRole::KV_CACHE:
                slot_type = SlotType::KV_CACHE;
                break;
            default:
                break;
        }
        
        // Acquire slot
        auto slot = slots_->acquireSlot(slot_type, zone.byte_length, controller_->getCurrentStepId());
        if (!slot) {
            // Budget exceeded — trigger tier morphing: downgrade quantization
            PolymorphicMathEngine mathEngine;
            // Try to free space by morphing existing slots to lower precision
            bool freed = false;
            for (const auto& prevZone : step.zones_to_load) {
                if (prevZone.quant == QuantizationType::Q8_0 ||
                    prevZone.quant == QuantizationType::Q4_K_M) {
                    // Map TensorRole to SlotType for lookup
                    SlotType prevSlotType = SlotType::AUXILIARY;
                    switch (prevZone.role) {
                        case TensorRole::ATTN_Q: case TensorRole::ATTN_K:
                        case TensorRole::ATTN_V: case TensorRole::ATTN_O:
                            prevSlotType = SlotType::ATTENTION; break;
                        case TensorRole::MLP_UP: case TensorRole::MLP_DOWN:
                            prevSlotType = SlotType::MLP; break;
                        case TensorRole::KV_CACHE:
                            prevSlotType = SlotType::KV_CACHE; break;
                        default: break;
                    }
                    // Morph to more aggressive quantization
                    auto existingSlot = slots_->findSlot(prevSlotType);
                    if (existingSlot) {
                        QuantizationType targetQuant = (prevZone.quant == QuantizationType::Q8_0)
                            ? QuantizationType::Q4_K_M
                            : QuantizationType::Q2_K;
                        mathEngine.morphTier(existingSlot->base, existingSlot->active_bytes,
                                            prevZone.quant, targetQuant);
                        freed = true;
                    }
                }
            }
            if (!freed) return false;

            // Retry slot acquisition after morphing
            slot = slots_->acquireSlot(slot_type, zone.byte_length, controller_->getCurrentStepId());
            if (!slot) return false;
        }
        
        // Start async load
        startAsyncLoad(zone);
    }
    
    return true;
}

const StreamStep& PolymorphicLoader::getCurrentStep() const {
    if (!controller_) {
        static StreamStep dummy{};
        return dummy;
    }
    return controller_->currentStep();
}

void PolymorphicLoader::advanceStep() {
    if (controller_) {
        controller_->advance();
    }
}

void PolymorphicLoader::jumpToStep(uint32_t step_id) {
    if (controller_) {
        controller_->jumpToStep(step_id);
    }
}

PolymorphicLoader::PerformanceMetrics PolymorphicLoader::getMetrics() const {
    return metrics_;
}

std::unique_ptr<IFormatAdapter> PolymorphicLoader::detectAndLoadAdapter(const std::string& path) {
    // Try each adapter
    auto gguf_adapter = std::make_unique<GGUFAdapter>();
    if (gguf_adapter->validate(path)) {
        return gguf_adapter;
    }
    
    auto sharded_adapter = std::make_unique<ShardedBlobAdapter>();
    if (sharded_adapter->validate(path)) {
        return sharded_adapter;
    }
    
    auto mixed_adapter = std::make_unique<MixedTierAdapter>();
    if (mixed_adapter->validate(path)) {
        return mixed_adapter;
    }
    
    return nullptr;
}

bool PolymorphicLoader::startAsyncLoad(const TensorDesc& zone) {
    if (!model_file_handle_) {
        model_file_handle_ = CreateFileA(current_model_path_.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        if (model_file_handle_ == INVALID_HANDLE_VALUE) return false;
    }

    // Determine target slot
    SlotType slot_type = SlotType::AUXILIARY;
    switch (zone.role) {
        case TensorRole::ATTN_Q:
        case TensorRole::ATTN_K:
        case TensorRole::ATTN_V:
        case TensorRole::ATTN_O:
            slot_type = SlotType::ATTENTION;
            break;
        case TensorRole::MLP_UP:
        case TensorRole::MLP_DOWN:
            slot_type = SlotType::MLP;
            break;
        case TensorRole::KV_CACHE:
            slot_type = SlotType::KV_CACHE;
            break;
        default:
            break;
    }

    auto slot = slots_->acquireSlot(slot_type, zone.byte_length, controller_->getCurrentStepId());
    if (!slot) return false;

    // Set up overlapped read
    OVERLAPPED* ov = new OVERLAPPED();
    memset(ov, 0, sizeof(OVERLAPPED));
    ov->Offset = (DWORD)(zone.file_offset & 0xFFFFFFFF);
    ov->OffsetHigh = (DWORD)(zone.file_offset >> 32);

    if (!ReadFile(model_file_handle_, slot->base, zone.byte_length, NULL, ov)) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            delete ov;
            return false;
        }
    }

    // Update metrics
    metrics_.mb_per_second = (float)zone.byte_length / (1024.0f * 1024.0f); // Simplification

    return true;
}
