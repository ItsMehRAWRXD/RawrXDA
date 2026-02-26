// config/production_config.hpp
// Environment-aware configuration with feature toggles
#pragma once
#include <windows.h>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "../telemetry/async_logger.hpp"

namespace RawrXD {

    class ProductionConfig {
    public:
        struct AIParams {
            std::string model_path;
            int context_length;
            float temperature;
            float top_p;
            int top_k;
            bool speculative_decoding;
            int draft_tokens;           // tokens to draft in speculative mode
            int quantization_level;     // 2, 4, or 8
            int batch_size;
            int num_gpu_layers;
            std::string tokenizer_path;
        } ai;

        struct ResourceLimits {
            size_t max_ram_gb;
            size_t max_vram_gb;
            int max_threads;
            size_t kv_cache_max_mb;
            size_t model_buffer_mb;
        } limits;

        struct ServerConfig {
            uint16_t api_port;
            uint16_t metrics_port;
            int max_connections;
            int request_timeout_ms;
            std::string bind_address;
        } server;

        struct FeatureFlags {
            bool vulkan_compute;
            bool flash_attention;
            bool speculative_decoding;
            bool hierarchical_quantization;
            bool sliding_kv_cache;
            bool sparse_pruning;
            bool seh_protection;
            bool prometheus_metrics;
        } features;

        void load() {
            // Load .env files (lowest priority)
            loadEnvFile(".env");
            loadEnvFile(".env.production");
            loadEnvFile(".env.local");

            // AI Parameters
            ai.model_path = getStr("RAWR_MODEL_PATH", "models/default.gguf");
            ai.context_length = getInt("RAWR_CTX_LEN", 4096);
            ai.temperature = getFloat("RAWR_TEMPERATURE", 0.7f);
            ai.top_p = getFloat("RAWR_TOP_P", 0.9f);
            ai.top_k = getInt("RAWR_TOP_K", 40);
            ai.speculative_decoding = getBool("RAWR_SPEC_DECODE", false);
            ai.draft_tokens = getInt("RAWR_DRAFT_TOKENS", 4);
            ai.quantization_level = getInt("RAWR_QUANT_LEVEL", 4);
            ai.batch_size = getInt("RAWR_BATCH_SIZE", 512);
            ai.num_gpu_layers = getInt("RAWR_GPU_LAYERS", -1); // -1 = auto
            ai.tokenizer_path = getStr("RAWR_TOKENIZER_PATH", "");

            // Resource Limits
            limits.max_ram_gb = static_cast<size_t>(getInt("RAWR_MAX_RAM_GB", 64));
            limits.max_vram_gb = static_cast<size_t>(getInt("RAWR_MAX_VRAM_GB", 16));
            limits.max_threads = getInt("RAWR_MAX_THREADS", 0); // 0 = auto
            limits.kv_cache_max_mb = static_cast<size_t>(getInt("RAWR_KV_CACHE_MB", 512));
            limits.model_buffer_mb = static_cast<size_t>(getInt("RAWR_MODEL_BUFFER_MB", 2048));

            // Server Config
            server.api_port = static_cast<uint16_t>(getInt("RAWR_API_PORT", 8080));
            server.metrics_port = static_cast<uint16_t>(getInt("RAWR_METRICS_PORT", 9090));
            server.max_connections = getInt("RAWR_MAX_CONN", 100);
            server.request_timeout_ms = getInt("RAWR_REQ_TIMEOUT_MS", 30000);
            server.bind_address = getStr("RAWR_BIND_ADDR", "0.0.0.0");

            // Feature Flags
            features.vulkan_compute = getBool("RAWR_FEAT_VULKAN", true);
            features.flash_attention = getBool("RAWR_FEAT_FLASH_ATTN", true);
            features.speculative_decoding = getBool("RAWR_FEAT_SPEC_DECODE", false);
            features.hierarchical_quantization = getBool("RAWR_FEAT_HIER_QUANT", true);
            features.sliding_kv_cache = getBool("RAWR_FEAT_SLIDING_KV", true);
            features.sparse_pruning = getBool("RAWR_FEAT_SPARSE_PRUNE", false);
            features.seh_protection = getBool("RAWR_FEAT_SEH", true);
            features.prometheus_metrics = getBool("RAWR_FEAT_METRICS", true);

            // Auto-detect threads if not set
            if (limits.max_threads == 0) {
                SYSTEM_INFO si;
                GetSystemInfo(&si);
                limits.max_threads = static_cast<int>(si.dwNumberOfProcessors);
            }

            RAWR_LOG_INFO("Config loaded: Model=%s CTX=%d Threads=%d Quant=Q%d",
                          ai.model_path.c_str(), ai.context_length,
                          limits.max_threads, ai.quantization_level);
            RAWR_LOG_INFO("Features: Vulkan=%d FlashAttn=%d SpecDecode=%d HierQuant=%d",
                          features.vulkan_compute, features.flash_attention,
                          features.speculative_decoding, features.hierarchical_quantization);
        }

        bool featureEnabled(const char* flag) const {
            auto it = env_.find(std::string("RAWR_FEAT_") + flag);
            if (it != env_.end()) return it->second == "1" || it->second == "true";
            return false;
        }

        static ProductionConfig& instance() {
            static ProductionConfig cfg;
            return cfg;
        }

    private:
        std::unordered_map<std::string, std::string> env_;

        void loadEnvFile(const char* path) {
            std::ifstream file(path);
            if (!file.is_open()) return;

            std::string line;
            while (std::getline(file, line)) {
                // Skip comments and empty lines
                if (line.empty() || line[0] == '#') continue;

                auto eq = line.find('=');
                if (eq == std::string::npos) continue;

                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);

                // Trim whitespace
                while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
                while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());

                // Strip quotes
                if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                    val = val.substr(1, val.size() - 2);
                }

                env_[key] = val;
            }
        }

        std::string getStr(const char* key, const char* def) const {
            // Check real environment first
            const char* env_val = std::getenv(key);
            if (env_val && env_val[0]) return env_val;

            // Check loaded .env files
            auto it = env_.find(key);
            if (it != env_.end()) return it->second;

            return def;
        }

        int getInt(const char* key, int def) const {
            std::string val = getStr(key, "");
            if (val.empty()) return def;
            try { return std::stoi(val); }
            catch (...) { return def; }
        }

        float getFloat(const char* key, float def) const {
            std::string val = getStr(key, "");
            if (val.empty()) return def;
            try { return std::stof(val); }
            catch (...) { return def; }
        }

        bool getBool(const char* key, bool def) const {
            std::string val = getStr(key, "");
            if (val.empty()) return def;
            return val == "1" || val == "true" || val == "yes" || val == "on";
        }
    };

} // namespace RawrXD
