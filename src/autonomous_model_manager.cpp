#include "autonomous_model_manager.h"
#include <algorithm>
#include <iostream>

namespace RawrXD {

AutonomousModelManager::AutonomousModelManager() {
    // Default models
    registerModel({ "gpt-4", "GPT-4", 8192, 0.03f, false });
    registerModel({ "gpt-3.5-turbo", "GPT-3.5 Turbo", 4096, 0.002f, false });
    registerModel({ "claude-3-opus", "Claude 3 Opus", 200000, 0.015f, false });
    registerModel({ "local-llama-3", "Llama 3 8B", 8192, 0.0f, true });
}

AutonomousModelManager::~AutonomousModelManager() {}

void AutonomousModelManager::registerModel(const ModelInfo& info) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Overwrite if exists
    auto it = std::find_if(m_models.begin(), m_models.end(), [&](const ModelInfo& m){ return m.id == info.id; });
    if (it != m_models.end()) {
        *it = info;
    } else {
        m_models.push_back(info);
    }
}

AutonomousModelManager::ModelInfo AutonomousModelManager::selectBestModel(const std::string& taskComplexity) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_models.empty()) return {};
    
    // Logic: 
    // "complex" -> Best remote model (GPT-4 or Opus)
    // "simple" -> Fastest/Cheapest (GPT-3.5 or Local)
    // "creative" -> High context (Opus)
    
    if (taskComplexity == "complex") {
        for (const auto& m : m_models) {
            if (m.id == "gpt-4" || m.id == "claude-3-opus") return m;
        }
    } else if (taskComplexity == "simple") {
        for (const auto& m : m_models) {
            if (m.isLocal) return m; // Prefer local for simple
        }
        for (const auto& m : m_models) {
            if (m.id == "gpt-3.5-turbo") return m;
        }
    }
    
    // Default fallback
    return m_models.front();
}

bool AutonomousModelManager::loadLocalModel(const std::string& path) {
    // Real logic would involve CPUInferenceEngine::loadGGUFModel(path)
    // Here we just register it
    registerModel({ "local-custom", "Custom Local Model", 4096, 0.0f, true });
    return true;
}

AutonomousModelManager::ModelInfo AutonomousModelManager::getModel(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& m : m_models) {
        if (m.id == id) return m;
    }
    return {};
}

} // namespace RawrXD


