#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

// Configuration constants
constexpr size_t MAX_MODELS = 64;
constexpr size_t MAX_PATH_LEN = 512;
constexpr size_t MAX_NAME_LEN = 128;

// Model capabilities and metrics
struct ModelCapabilities {
    bool supports_vision = false;
    bool supports_function_calling = false;
    bool supports_streaming = false;
    bool supports_chat = false;
    bool supports_completion = false;
    bool supports_embedding = false;
    size_t max_context_length = 0;
    size_t context_length = 0;
    size_t vocab_size = 0;
};

struct ModelMetrics {
    float quality_score = 0.0f;
    float latency_ms = 0.0f;
    uint64_t total_requests = 0;
    uint64_t successful_requests = 0;
    uint64_t failed_requests = 0;
    uint64_t total_tokens_processed = 0;
    float average_response_time_ms = 0.0f;
    float memory_usage_mb = 0.0f;
    float load_time_ms = 0.0f;
};

// Registry status and result types
enum class RegistryStatus { Ok = 0, Error, NotFound, AlreadyLoaded };
constexpr RegistryStatus REGISTRY_SUCCESS = RegistryStatus::Ok;
constexpr RegistryStatus REGISTRY_ERROR = RegistryStatus::Error;

struct RegistryResult {
    RegistryStatus status = RegistryStatus::Ok;
    char message[256] = {};
};

// Public model info structure
struct ModelInfo {
    char name[MAX_NAME_LEN];
    char display_name[MAX_NAME_LEN];
    char file_path[MAX_PATH_LEN];
    char model_type[32];
    char variant[32];
    ModelCapabilities capabilities;
    ModelMetrics metrics;
    uint64_t file_size;
    uint64_t last_modified;
    bool is_available;
    bool is_loaded;
};

// Function declarations
RegistryResult registry_get_model_info(const char* name, ModelInfo* info);
RegistryResult registry_list_models(ModelInfo* models, size_t max_models, size_t* count);

namespace RawrXD {

struct ModelVersion;
class ModelRegistry;

} // namespace RawrXD
