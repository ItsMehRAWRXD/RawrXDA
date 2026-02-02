#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace RawrXD {

class AutonomousModelManager {
public:
    struct ModelInfo {
        std::string id;
        std::string name;
        int maxTokens;
        float costPer1k;
        bool isLocal;
    };
    
    AutonomousModelManager();
    ~AutonomousModelManager();
    
    void registerModel(const ModelInfo& info);
    ModelInfo selectBestModel(const std::string& taskComplexity); // "simple", "complex", "creative"
    bool loadLocalModel(const std::string& path);
    ModelInfo getModel(const std::string& id);
    
private:
    std::vector<ModelInfo> m_models;
    std::string m_activeModelId;
    mutable std::mutex m_mutex;
};

} // namespace RawrXD

