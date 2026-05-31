// model_residency.cpp - Implementation of model residency tier management
// Part of the Copilot-like inference pipeline.

#include "model_residency.h"
#include <algorithm>
#include <cassert>
#include <cstring>

namespace RawrXD {

ModelResidencyManager::ModelResidencyManager(size_t vram_budget_gb)
    : vram_budget_(vram_budget_gb * 1024 * 1024 * 1024)
    , vram_used_(0)
{
    stats_ = {};
}

ModelResidencyManager::~ModelResidencyManager() {
    StopBackgroundThread();
    
    // Unload all models
    std::lock_guard<std::mutex> lock(models_mutex_);
    for (auto& pair : models_) {
        if (pair.second->current_tier == ModelTier::HOT ||
            pair.second->current_tier == ModelTier::WARM) {
            UnloadModelFromVRAM(*pair.second);
        }
    }
}

void ModelResidencyManager::RegisterModel(const ModelInfo& model) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    auto state = std::make_unique<ModelState>();
    state->info = model;
    state->current_tier = ModelTier::COLD;
    state->is_pinned = false;
    state->pending_requests = 0;
    state->slot.is_valid = false;
    
    models_[model.name] = std::move(state);
}

void ModelResidencyManager::UnregisterModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    auto it = models_.find(model_name);
    if (it == models_.end()) {
        return;
    }
    
    // Unload if resident
    if (it->second->current_tier == ModelTier::HOT ||
        it->second->current_tier == ModelTier::WARM) {
        UnloadModelFromVRAM(*it->second);
    }
    
    // Remove from tier sets
    hot_slots_.erase(model_name);
    warm_slots_.erase(model_name);
    
    models_.erase(it);
}

void* ModelResidencyManager::RequestModel(
    const std::string& model_name,
    ModelTier requested_tier,
    std::chrono::milliseconds timeout)
{
    ModelState* model = nullptr;
    {
        std::lock_guard<std::mutex> lock(models_mutex_);
        auto it = models_.find(model_name);
        if (it == models_.end()) {
            return nullptr;
        }
        model = it->second.get();
    }
    
    std::unique_lock<std::mutex> lock(model->mutex);
    
    // Increment request count
    model->pending_requests++;
    model->info.access_count++;
    model->info.last_access = std::chrono::steady_clock::now();
    
    // Check if already in requested tier
    if (model->current_tier == requested_tier && model->slot.is_valid) {
        return model->slot.vram_ptr;
    }
    
    // Check if in higher tier (can use immediately)
    if (model->current_tier == ModelTier::HOT && requested_tier != ModelTier::COLD) {
        return model->slot.vram_ptr;
    }
    
    // Need to load or promote
    if (requested_tier == ModelTier::HOT) {
        // Check if we have room in HOT tier
        if (hot_slots_.size() >= static_cast<size_t>(policy_.max_hot_models)) {
            // Evict LRU
            if (!EvictLRU(ModelTier::HOT)) {
                // Can't load into HOT, use WARM instead
                requested_tier = ModelTier::WARM;
            }
        }
    }
    
    if (requested_tier == ModelTier::WARM) {
        // Check if we have room in WARM tier
        if (warm_slots_.size() >= static_cast<size_t>(policy_.max_warm_models)) {
            if (!EvictLRU(ModelTier::WARM)) {
                requested_tier = ModelTier::COLD;
            }
        }
    }
    
    // Load model if needed
    if (requested_tier != ModelTier::COLD) {
        if (!model->slot.is_valid) {
            if (!LoadModelIntoVRAM(*model)) {
                model->pending_requests--;
                return nullptr;
            }
        }
        
        // Update tier
        ModelTier old_tier = model->current_tier;
        model->current_tier = requested_tier;
        
        // Update tier sets
        if (old_tier == ModelTier::HOT) hot_slots_.erase(model_name);
        if (old_tier == ModelTier::WARM) warm_slots_.erase(model_name);
        if (requested_tier == ModelTier::HOT) hot_slots_.insert(model_name);
        if (requested_tier == ModelTier::WARM) warm_slots_.insert(model_name);
        
        // Update stats
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            if (old_tier == ModelTier::COLD && requested_tier != ModelTier::COLD) {
                stats_.tier_promotions++;
            }
        }
    }
    
    model->pending_requests--;
    return model->slot.vram_ptr;
}

void ModelResidencyManager::ReleaseModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    auto it = models_.find(model_name);
    if (it == models_.end()) {
        return;
    }
    
    ModelState& model = *it->second;
    std::lock_guard<std::mutex> model_lock(model.mutex);
    
    // Don't unload if pinned
    if (model.is_pinned) {
        return;
    }
    
    // Update last access time
    model.info.last_access = std::chrono::steady_clock::now();
}

void ModelResidencyManager::PinModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    auto it = models_.find(model_name);
    if (it == models_.end()) {
        return;
    }
    
    it->second->is_pinned = true;
}

void ModelResidencyManager::UnpinModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    auto it = models_.find(model_name);
    if (it == models_.end()) {
        return;
    }
    
    it->second->is_pinned = false;
}

bool ModelResidencyManager::PromoteModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    auto it = models_.find(model_name);
    if (it == models_.end()) {
        return false;
    }
    
    ModelState& model = *it->second;
    std::lock_guard<std::mutex> model_lock(model.mutex);
    
    if (model.current_tier == ModelTier::COLD) {
        // Promote to WARM
        if (warm_slots_.size() >= static_cast<size_t>(policy_.max_warm_models)) {
            if (!EvictLRU(ModelTier::WARM)) {
                return false;
            }
        }
        
        if (!LoadModelIntoVRAM(model)) {
            return false;
        }
        
        model.current_tier = ModelTier::WARM;
        warm_slots_.insert(model_name);
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.tier_promotions++;
        return true;
    }
    
    if (model.current_tier == ModelTier::WARM) {
        // Promote to HOT
        if (hot_slots_.size() >= static_cast<size_t>(policy_.max_hot_models)) {
            if (!EvictLRU(ModelTier::HOT)) {
                return false;
            }
        }
        
        model.current_tier = ModelTier::HOT;
        warm_slots_.erase(model_name);
        hot_slots_.insert(model_name);
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.tier_promotions++;
        return true;
    }
    
    return false;
}

bool ModelResidencyManager::DemoteModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    auto it = models_.find(model_name);
    if (it == models_.end()) {
        return false;
    }
    
    ModelState& model = *it->second;
    std::lock_guard<std::mutex> model_lock(model.mutex);
    
    if (model.is_pinned) {
        return false;
    }
    
    if (model.current_tier == ModelTier::HOT) {
        // Demote to WARM
        model.current_tier = ModelTier::WARM;
        hot_slots_.erase(model_name);
        warm_slots_.insert(model_name);
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.tier_demotions++;
        return true;
    }
    
    if (model.current_tier == ModelTier::WARM) {
        // Demote to COLD
        UnloadModelFromVRAM(model);
        model.current_tier = ModelTier::COLD;
        warm_slots_.erase(model_name);
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.tier_demotions++;
        return true;
    }
    
    return false;
}

void ModelResidencyManager::PrefetchModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    auto it = models_.find(model_name);
    if (it == models_.end()) {
        return;
    }
    
    ModelState& model = *it->second;
    
    if (model.current_tier != ModelTier::COLD) {
        return;  // Already resident
    }
    
    // Load into WARM tier in background
    if (warm_slots_.size() >= static_cast<size_t>(policy_.max_warm_models)) {
        if (!EvictLRU(ModelTier::WARM)) {
            return;
        }
    }
    
    std::thread([this, &model]() {
        std::lock_guard<std::mutex> model_lock(model.mutex);
        if (LoadModelIntoVRAM(model)) {
            model.current_tier = ModelTier::WARM;
            warm_slots_.insert(model.info.name);
            
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.tier_promotions++;
        }
    }).detach();
}

void ModelResidencyManager::SetVRAMBudget(size_t budget_gb) {
    vram_budget_ = budget_gb * 1024 * 1024 * 1024;
}

void ModelResidencyManager::SetPolicy(const Policy& policy) {
    policy_ = policy;
}

void ModelResidencyManager::StartBackgroundThread() {
    if (background_running_.load()) {
        return;
    }
    
    background_running_.store(true);
    background_thread_ = std::thread(&ModelResidencyManager::BackgroundThreadLoop, this);
}

void ModelResidencyManager::StopBackgroundThread() {
    if (!background_running_.load()) {
        return;
    }
    
    background_running_.store(false);
    background_cv_.notify_all();
    
    if (background_thread_.joinable()) {
        background_thread_.join();
    }
}

bool ModelResidencyManager::LoadModelIntoVRAM(ModelState& model) {
    // Placeholder: In real implementation, this would:
    // 1. Allocate VRAM
    // 2. Load model weights from disk
    // 3. Initialize GPU resources
    
    // For now, simulate allocation
    if (vram_used_ + model.info.vram_required > vram_budget_) {
        return false;
    }
    
    vram_used_ += model.info.vram_required;
    model.slot.vram_size = model.info.vram_required;
    model.slot.vram_ptr = reinterpret_cast<void*>(0xDEADBEEF);  // Placeholder
    model.slot.is_valid = true;
    model.slot.loaded_at = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.vram_used = vram_used_;
    
    return true;
}

void ModelResidencyManager::UnloadModelFromVRAM(ModelState& model) {
    if (!model.slot.is_valid) {
        return;
    }
    
    // Placeholder: In real implementation, this would:
    // 1. Free GPU resources
    // 2. Deallocate VRAM
    
    vram_used_ -= model.slot.vram_size;
    model.slot.is_valid = false;
    model.slot.vram_ptr = nullptr;
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.vram_used = vram_used_;
    stats_.cache_evictions++;
}

bool ModelResidencyManager::EvictLRU(ModelTier tier) {
    std::string lru_model;
    std::chrono::steady_clock::time_point oldest = std::chrono::steady_clock::now();
    
    for (const auto& name : (tier == ModelTier::HOT ? hot_slots_ : warm_slots_)) {
        auto it = models_.find(name);
        if (it == models_.end()) continue;
        
        ModelState& model = *it->second;
        if (model.is_pinned || model.pending_requests > 0) continue;
        
        if (model.info.last_access < oldest) {
            oldest = model.info.last_access;
            lru_model = name;
        }
    }
    
    if (lru_model.empty()) {
        return false;
    }
    
    return DemoteModel(lru_model);
}

void ModelResidencyManager::BackgroundThreadLoop() {
    while (background_running_.load()) {
        std::unique_lock<std::mutex> lock(background_mutex_);
        background_cv_.wait_for(lock, std::chrono::seconds(30));
        
        if (!background_running_.load()) break;
        
        // Auto-demote idle models
        if (policy_.auto_demote) {
            auto now = std::chrono::steady_clock::now();
            
            // Check HOT tier
            for (auto it = hot_slots_.begin(); it != hot_slots_.end(); ) {
                auto model_it = models_.find(*it);
                if (model_it == models_.end()) {
                    it = hot_slots_.erase(it);
                    continue;
                }
                
                ModelState& model = *model_it->second;
                if (!model.is_pinned && model.pending_requests == 0) {
                    auto idle = std::chrono::duration_cast<std::chrono::seconds>(
                        now - model.info.last_access);
                    if (idle > policy_.hot_timeout) {
                        DemoteModel(*it);
                    }
                }
                ++it;
            }
            
            // Check WARM tier
            for (auto it = warm_slots_.begin(); it != warm_slots_.end(); ) {
                auto model_it = models_.find(*it);
                if (model_it == models_.end()) {
                    it = warm_slots_.erase(it);
                    continue;
                }
                
                ModelState& model = *model_it->second;
                if (!model.is_pinned && model.pending_requests == 0) {
                    auto idle = std::chrono::duration_cast<std::chrono::seconds>(
                        now - model.info.last_access);
                    if (idle > policy_.warm_timeout) {
                        DemoteModel(*it);
                    }
                }
                ++it;
            }
        }
        
        // Auto-promote frequently used models
        if (policy_.auto_promote) {
            for (auto& pair : models_) {
                ModelState& model = *pair.second;
                if (model.info.access_count >= policy_.promotion_threshold) {
                    PromoteModel(pair.first);
                    model.info.access_count = 0;  // Reset counter
                }
            }
        }
    }
}

ModelResidencyManager::ModelState* ModelResidencyManager::FindModel(const std::string& model_name) {
    auto it = models_.find(model_name);
    return it != models_.end() ? it->second.get() : nullptr;
}

const ModelResidencyManager::ModelState* ModelResidencyManager::FindModel(const std::string& model_name) const {
    auto it = models_.find(model_name);
    return it != models_.end() ? it->second.get() : nullptr;
}

} // namespace RawrXD