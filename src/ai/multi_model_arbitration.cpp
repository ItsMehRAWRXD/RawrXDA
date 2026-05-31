// multi_model_arbitration.cpp - Implementation of multi-model arbitration
// Part of the Copilot-like inference pipeline.

#include "multi_model_arbitration.h"
#include <algorithm>

namespace RawrXD {

MultiModelArbitration::MultiModelArbitration() {
    stats_ = {};
}

MultiModelArbitration::~MultiModelArbitration() {
    CancelBackgroundRefinement();
}

void MultiModelArbitration::SetConfig(const Config& config) {
    config_ = config;
}

void MultiModelArbitration::RegisterModel(const ModelInfo& model) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    models_[model.name] = model;
}

void MultiModelArbitration::UnregisterModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    models_.erase(model_name);
}

ArbitrationDecision MultiModelArbitration::SelectModel(
    const std::string& context,
    std::chrono::milliseconds latency_budget
) {
    ArbitrationDecision decision;
    decision.confidence = 0.0f;
    
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    // Find best model for latency budget
    ModelInfo* best_model = nullptr;
    float best_score = -1.0f;
    
    for (auto& pair : models_) {
        ModelInfo& model = pair.second;
        
        // Skip disabled models
        if (model.tier == ModelTier::SMALL && !config_.enable_small_model) continue;
        if (model.tier == ModelTier::MEDIUM && !config_.enable_medium_model) continue;
        if (model.tier == ModelTier::LARGE && !config_.enable_large_model) continue;
        
        // Skip unloaded models
        if (!model.is_loaded) continue;
        
        // Calculate score
        float score = CalculateScore(model, latency_budget);
        
        if (score > best_score) {
            best_score = score;
            best_model = &model;
        }
    }
    
    if (!best_model) {
        decision.reason = "No suitable model found";
        return decision;
    }
    
    // Build decision
    decision.selected_tier = best_model->tier;
    decision.model_name = best_model->name;
    decision.confidence = best_score;
    decision.expected_latency = best_model->avg_latency;
    decision.expected_quality = best_model->quality_score;
    
    if (best_model->tier == ModelTier::SMALL) {
        decision.reason = "Small model selected for low latency";
    } else if (best_model->tier == ModelTier::MEDIUM) {
        decision.reason = "Medium model selected for balance";
    } else {
        decision.reason = "Large model selected for quality";
    }
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_requests++;
        
        switch (best_model->tier) {
            case ModelTier::SMALL:
                stats_.small_model_requests++;
                stats_.avg_small_latency = (stats_.avg_small_latency * (stats_.small_model_requests - 1) + 
                                           std::chrono::microseconds(best_model->avg_latency.count())) / 
                                          stats_.small_model_requests;
                break;
            case ModelTier::MEDIUM:
                stats_.medium_model_requests++;
                stats_.avg_medium_latency = (stats_.avg_medium_latency * (stats_.medium_model_requests - 1) + 
                                            std::chrono::microseconds(best_model->avg_latency.count())) / 
                                           stats_.medium_model_requests;
                break;
            case ModelTier::LARGE:
                stats_.large_model_requests++;
                stats_.avg_large_latency = (stats_.avg_large_latency * (stats_.large_model_requests - 1) + 
                                           std::chrono::microseconds(best_model->avg_latency.count())) / 
                                          stats_.large_model_requests;
                break;
        }
        
        // Calculate percentages
        if (stats_.total_requests > 0) {
            stats_.small_model_percentage = static_cast<float>(stats_.small_model_requests) / stats_.total_requests;
            stats_.medium_model_percentage = static_cast<float>(stats_.medium_model_requests) / stats_.total_requests;
            stats_.large_model_percentage = static_cast<float>(stats_.large_model_requests) / stats_.total_requests;
        }
        
        stats_.avg_quality = (stats_.avg_quality * (stats_.total_requests - 1) + best_model->quality_score) / 
                            stats_.total_requests;
    }
    
    return decision;
}

void MultiModelArbitration::StartBackgroundRefinement(
    const std::string& context,
    const std::string& draft_completion
) {
    if (refining_.load()) {
        return;
    }
    
    refining_.store(true);
    stop_refinement_.store(false);
    
    // Start refinement thread
    refinement_thread_ = std::thread(
        &MultiModelArbitration::RefinementThread, this,
        context, draft_completion);
}

bool MultiModelArbitration::GetRefinedCompletion(std::string& completion) {
    std::lock_guard<std::mutex> lock(refinement_mutex_);
    
    if (refined_completion_.empty()) {
        return false;
    }
    
    completion = refined_completion_;
    refined_completion_.clear();
    
    return true;
}

void MultiModelArbitration::CancelBackgroundRefinement() {
    stop_refinement_.store(true);
    refining_.store(false);
    
    refinement_cv_.notify_all();
    
    if (refinement_thread_.joinable()) {
        refinement_thread_.join();
    }
}

float MultiModelArbitration::CalculateScore(
    const ModelInfo& model,
    std::chrono::milliseconds latency_budget
) const {
    // Score based on:
    // 1. Latency fit (how well it fits budget)
    // 2. Quality (higher is better)
    // 3. Availability (loaded and active)
    
    float latency_score = 1.0f;
    if (model.avg_latency > latency_budget) {
        latency_score = 1.0f - static_cast<float>(
            (model.avg_latency - latency_budget).count()) / latency_budget.count();
        latency_score = std::max(0.0f, latency_score);
    }
    
    float quality_score = model.quality_score;
    float availability_score = model.is_loaded ? 1.0f : 0.0f;
    
    // Weighted score
    float score = latency_score * 0.4f + quality_score * 0.4f + availability_score * 0.2f;
    
    return score;
}

ModelInfo* MultiModelArbitration::FindBestModel(std::chrono::milliseconds latency_budget) {
    ModelInfo* best_model = nullptr;
    float best_score = -1.0f;
    
    for (auto& pair : models_) {
        ModelInfo& model = pair.second;
        
        if (!model.is_loaded) continue;
        
        float score = CalculateScore(model, latency_budget);
        
        if (score > best_score) {
            best_score = score;
            best_model = &model;
        }
    }
    
    return best_model;
}

void MultiModelArbitration::RefinementThread(
    const std::string& context,
    const std::string& draft_completion
) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Find large model
    ModelInfo* large_model = nullptr;
    {
        std::lock_guard<std::mutex> lock(models_mutex_);
        for (auto& pair : models_) {
            if (pair.second.tier == ModelTier::LARGE && pair.second.is_loaded) {
                large_model = &pair.second;
                break;
            }
        }
    }
    
    if (!large_model) {
        refining_.store(false);
        return;
    }
    
    // TODO: Run large model refinement
    // For now, just simulate
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    if (stop_refinement_.load()) {
        refining_.store(false);
        return;
    }
    
    // Store refined completion
    {
        std::lock_guard<std::mutex> lock(refinement_mutex_);
        refined_completion_ = draft_completion + " (refined)";
    }
    
    refining_.store(false);
    refinement_cv_.notify_all();
}

} // namespace RawrXD