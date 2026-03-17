#include "model_registry.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <mutex>
#include <windows.h>

namespace RawrXD {

struct ModelVersion {
    std::string name;
    std::string version;
    std::string path;
    int id;
    size_t params;
    bool quantized;
};

class ModelRegistry {
    std::vector<ModelVersion> models_;
    std::mutex mtx_;
    int activeId_ = -1;
    void* userData_;
    
public:
    ModelRegistry(void* ctx) : userData_(ctx) {
        scanModelDirectory();
    }
    
    ~ModelRegistry() = default;
    
    void initialize() {
        scanModelDirectory();
    }
    
    std::vector<ModelVersion> getAllModels() const {
        return models_;
    }
    
    bool setActiveModel(int id) {
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto& m : models_) {
            if (m.id == id) {
                activeId_ = id;
                return true;
            }
        }
        return false;
    }
    
    std::string getActiveModel() const {
        if (activeId_ >= 0 && activeId_ < (int)models_.size()) {
            return models_[activeId_].name;
        }
        return "none";
    }
    
    void loadModel(const std::string& path) {
        // Model loading implementation
    }
    
    void inferStreaming(const std::string& prompt, 
                        std::function<void(const std::string&, bool)> cb) {
        // Inference implementation
        cb("Inference not yet implemented", true);
    }

private:
    void scanModelDirectory() {
        // Scan ./models/ for .gguf files
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA("models\\*.gguf", &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            int id = 0;
            do {
                ModelVersion mv;
                mv.name = fd.cFileName;
                mv.path = "models\\" + std::string(fd.cFileName);
                mv.id = id++;
                mv.params = estimateParams(mv.path);
                mv.quantized = true;
                models_.push_back(mv);
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }
    
    size_t estimateParams(const std::string& path) {
        // Quick estimation based on file size
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            auto size = file.tellg();
            file.close();
            // Rough estimate: 1B params ~ 500MB Q4_K_M
            return static_cast<size_t>(size / (500 * 1024 * 1024));
        }
        return 0;
    }
};

} // namespace RawrXD
