#include "runtime_core.h"
#include "engine_iface.h"
#include "tool_registry.h"
#include "memory_core.h"
#include "hot_patcher.h"
#include "vsix_loader.h"
#include "hf_hub_client.h"
#include "blob_client.h"
#include "gguf_loader.h"
#include "tokenizer.h"
#include "sampler.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>

#include "logging/logger.h"
static Logger s_logger("rawrxd_system");

namespace RawrXD {

class RawrXDSystem {
    static RawrXDSystem* instance;
    static std::mutex init_mutex;
    
    // Core Components
    MemoryCore memory_core;
    HotPatcher hot_patcher;
    VSIXLoader vsix_loader;
    HFHubClient hf_client;
    BlobClient blob_client;
    GGUFLoader gguf_loader;
    Tokenizer tokenizer;
    Sampler sampler;
    
    // State
    std::atomic<bool> initialized{false};
    std::atomic<bool> running{true};
    std::thread system_thread;
    
    RawrXDSystem() : 
        hf_client(".\\hf_cache"),
        blob_client("D:\\ollamamodels", true)  // Local blob storage
    {
    }
    
public:
    static RawrXDSystem* get_instance() {
        std::lock_guard<std::mutex> lock(init_mutex);
        if (!instance) {
            instance = new RawrXDSystem();
        }
        return instance;
    }
    
    bool initialize() {
        if (initialized) return true;
        
        s_logger.info("Initializing RawrXD System...");
        
        // Initialize runtime core
        init_runtime();
        
        // Initialize memory core
        memory_core.initialize();
        
        // Initialize hot patcher
        hot_patcher.initialize();
        
        // Initialize VSIX loader
        vsix_loader.initialize();
        
        // Load default model from local blob storage
        load_default_model();
        
        initialized = true;
        
        // Start system thread
        system_thread = std::thread(&RawrXDSystem::system_loop, this);
        
        s_logger.info("RawrXD System initialized successfully!");
        return true;
    }
    
    void shutdown() {
        running = false;
        if (system_thread.joinable()) {
            system_thread.join();
        }
        
        // Cleanup components
        vsix_loader.shutdown();
        hot_patcher.shutdown();
        memory_core.shutdown();
        
        s_logger.info("RawrXD System shutdown complete.");
    }
    
    bool load_model(const std::string& source, const std::string& model_name) {
        std::string model_path;
        
        if (source == "local") {
            // Load from local file system
            model_path = model_name;
        } else if (source == "huggingface") {
            // Load from HuggingFace Hub
            model_path = hf_client.get_model_path(model_name, "model.gguf");
        } else if (source == "blob") {
            // Load from local blob storage
            std::vector<std::string> blobs = blob_client.list_blobs(model_name);
            if (!blobs.empty()) {
                model_path = "D:\\ollamamodels\\" + model_name + "\\" + blobs[0];
            }
        } else if (source == "gguf") {
            // Direct GGUF file
            model_path = model_name;
        }
        
        if (model_path.empty()) {
            s_logger.error( "Failed to get model path for: " << model_name << std::endl;
            return false;
        }
        
        // Load GGUF model
        if (!gguf_loader.Load(model_path)) {
            s_logger.error( "Failed to load GGUF model: " << model_path << std::endl;
            return false;
        }
        
        // Load tokenizer
        tokenizer.load(model_path);
        
        // Set active engine
        set_engine("RawrXD-AVX512");
        
        s_logger.info("Model loaded successfully: ");
        return true;
    }
    
    std::string process_request(const std::string& prompt, 
                               const std::string& mode = "ask",
                               bool deep_thinking = false,
                               bool deep_research = false,
                               bool no_refusal = false,
                               size_t context_limit = 4096) {
        
        // Set runtime parameters
        set_mode(mode);
        set_deep_thinking(deep_thinking);
        set_deep_research(deep_research);
        set_no_refusal(no_refusal);
        set_context(context_limit);
        
        // Process through runtime core
        return process_prompt(prompt);
    }
    
    // Memory management
    bool allocate_memory(size_t size) {
        return memory_core.allocate(size);
    }
    
    void free_memory(size_t id) {
        memory_core.free(id);
    }
    
    size_t get_memory_usage() {
        return memory_core.get_usage();
    }
    
    // Hot patching
    bool apply_patch(const std::string& patch_data) {
        return hot_patcher.apply_patch(patch_data);
    }
    
    bool revert_patch(const std::string& patch_id) {
        return hot_patcher.revert_patch(patch_id);
    }
    
    // VSIX management
    bool load_vsix(const std::string& vsix_path) {
        return vsix_loader.load(vsix_path);
    }
    
    bool unload_vsix(const std::string& vsix_id) {
        return vsix_loader.unload(vsix_id);
    }
    
    std::vector<std::string> list_vsix_plugins() {
        return vsix_loader.list_plugins();
    }
    
    // Get system status
    std::string get_status() {
        std::stringstream status;
        status << "RawrXD System Status:\n";
        status << "  Initialized: " << (initialized ? "Yes" : "No") << "\n";
        status << "  Running: " << (running ? "Yes" : "No") << "\n";
        status << "  Memory Usage: " << get_memory_usage() << " bytes\n";
        status << "  Active Engine: " << get_active_engine() << "\n";
        status << "  VSIX Plugins: " << list_vsix_plugins().size() << " loaded\n";
        return status.str();
    }
    
private:
    void load_default_model() {
        // Try to load a default model from local blob storage
        std::vector<std::string> models = blob_client.list_blobs("");
        if (!models.empty()) {
            load_model("blob", "");
        } else {
            s_logger.info("No default model found in D:\\ollamamodels");
        }
    }
    
    void system_loop() {
        while (running) {
            // System maintenance tasks
            memory_core.garbage_collect();
            hot_patcher.cleanup_old_patches();
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    const char* get_active_engine() {
        // This would need to be implemented in runtime_core
        // For now, return a default
        return "RawrXD-AVX512";
    }
};

// Static member definitions
RawrXDSystem* RawrXDSystem::instance = nullptr;
std::mutex RawrXDSystem::init_mutex;

// Global accessor
RawrXDSystem* get_rawrxd_system() {
    return RawrXDSystem::get_instance();
}

} // namespace RawrXD