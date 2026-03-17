#ifndef SWARMLINK_HOTSWAP_H
#define SWARMLINK_HOTSWAP_H

#include <string>
#include <vector>
#include <cstddef>
#include <mutex>

class AgenticModelManager {
public:
    AgenticModelManager();
    ~AgenticModelManager();

    // [B] Implement runtime model hot-swap 
    bool HotSwapModel(const std::string& gguf_path);

    // [C] Activate SwarmLink multi-GPU dispatch 
    void ActivateSwarmLink(int num_gpus, size_t vram_split);

private:
    std::mutex model_mutex;
    std::string current_model_path;
    int active_gpus;
    size_t memory_split;
    bool is_model_loaded;
};

#endif // SWARMLINK_HOTSWAP_H