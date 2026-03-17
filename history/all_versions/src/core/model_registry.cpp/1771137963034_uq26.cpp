/*
 * Model Registry - Production Implementation
 * Manages available AI models, their capabilities, and metadata
 * Supports auto-discovery and runtime model loading/unloading
 */

#include "model_registry.hpp"
#include <cstring>
#include <cstdio>
#include <atomic>
#include <mutex>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <fileapi.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

// ================================================
// Configuration Constants
// ================================================
constexpr size_t MAX_MODELS = 64;
constexpr size_t MAX_PATH_LEN = 512;
constexpr size_t MAX_NAME_LEN = 128;
constexpr size_t MODEL_SCAN_INTERVAL_MS = 30000;  // 30 seconds

// ================================================
// Model Entry Structure
// ================================================
struct ModelEntry {
    char name[MAX_NAME_LEN];
    char display_name[MAX_NAME_LEN];
    char file_path[MAX_PATH_LEN];
    char model_type[32];            // "gguf", "onnx", "pytorch", etc.
    char variant[32];               // "q4_0", "q8_0", "fp16", etc.
    
    ModelCapabilities capabilities;
    ModelMetrics metrics;
    
    uint64_t file_size;
    uint64_t last_modified;
    uint64_t last_accessed;
    
    bool is_available;
    bool is_loaded;
    bool auto_discovered;
    
    void* model_handle;             // Opaque handle for loaded model
};

// ================================================
// Static Storage
// ================================================
static ModelEntry g_models[MAX_MODELS];
static std::atomic<size_t> g_model_count{0};
static std::mutex g_registry_mutex;

// Model discovery paths
static const char* SEARCH_PATHS[] = {
    "models/",
    "../models/",
    "C:/models/",
    "D:/models/",
    "%USERPROFILE%/models/",
    nullptr
};

// Supported model extensions
static const char* MODEL_EXTENSIONS[] = {
    ".gguf",
    ".onnx", 
    ".bin",
    ".pt",
    ".pth",
    ".safetensors",
    nullptr
};

// ================================================
// Utility Functions
// ================================================
static RegistryResult make_result(RegistryStatus status, const char* message) {
    RegistryResult result = {};
    result.status = status;
    if (message) {
        strncpy_s(result.message, sizeof(result.message), message, _TRUNCATE);
    }
    return result;
}

static uint64_t get_file_size(const char* path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &fileInfo)) {
        LARGE_INTEGER size;
        size.HighPart = fileInfo.nFileSizeHigh;
        size.LowPart = fileInfo.nFileSizeLow;
        return size.QuadPart;
    }
#else
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
#endif
    return 0;
}

static uint64_t get_file_modified_time(const char* path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &fileInfo)) {
        LARGE_INTEGER time;
        time.HighPart = fileInfo.ftLastWriteTime.dwHighDateTime;
        time.LowPart = fileInfo.ftLastWriteTime.dwLowDateTime;
        return time.QuadPart;
    }
#else
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
#endif
    return 0;
}

static const char* get_file_extension(const char* path) {
    const char* dot = strrchr(path, '.');
    return dot ? dot : "";
}

static bool is_supported_extension(const char* ext) {
    for (const char** supported = MODEL_EXTENSIONS; *supported; ++supported) {
        if (_strcmpi(ext, *supported) == 0) {
            return true;
        }
    }
    return false;
}

static void parse_model_info(ModelEntry& entry) {
    // Parse model type from extension
    const char* ext = get_file_extension(entry.file_path);
    if (_strcmpi(ext, ".gguf") == 0) {
        strcpy_s(entry.model_type, "gguf");
        
        // Try to extract quantization from filename
        const char* filename = strrchr(entry.file_path, '/');
        if (!filename) filename = strrchr(entry.file_path, '\\');
        if (!filename) filename = entry.file_path;
        else filename++;
        
        // Look for quantization patterns (q4_0, q8_0, etc.)
        if (strstr(filename, "q4_0")) strcpy_s(entry.variant, "q4_0");
        else if (strstr(filename, "q4_1")) strcpy_s(entry.variant, "q4_1"); 
        else if (strstr(filename, "q5_0")) strcpy_s(entry.variant, "q5_0");
        else if (strstr(filename, "q5_1")) strcpy_s(entry.variant, "q5_1");
        else if (strstr(filename, "q8_0")) strcpy_s(entry.variant, "q8_0");
        else if (strstr(filename, "f16")) strcpy_s(entry.variant, "fp16");
        else if (strstr(filename, "f32")) strcpy_s(entry.variant, "fp32");
        else strcpy_s(entry.variant, "unknown");
        
        // Set GGUF capabilities
        entry.capabilities.supports_chat = true;
        entry.capabilities.supports_completion = true;
        entry.capabilities.supports_embedding = false;
        entry.capabilities.supports_streaming = true;
        entry.capabilities.context_length = 4096;  // Default, could be parsed from model
        entry.capabilities.vocab_size = 32000;
        
    } else if (_strcmpi(ext, ".onnx") == 0) {
        strcpy_s(entry.model_type, "onnx");
        strcpy_s(entry.variant, "optimized");
        entry.capabilities.supports_chat = true;
        entry.capabilities.supports_completion = true;
        entry.capabilities.supports_embedding = true;
        entry.capabilities.supports_streaming = false;
        entry.capabilities.context_length = 2048;
        entry.capabilities.vocab_size = 50000;
        
    } else if (_strcmpi(ext, ".pt") == 0 || _strcmpi(ext, ".pth") == 0) {
        strcpy_s(entry.model_type, "pytorch");
        strcpy_s(entry.variant, "native");
        entry.capabilities.supports_chat = true;
        entry.capabilities.supports_completion = true;
        entry.capabilities.supports_embedding = false;
        entry.capabilities.supports_streaming = false;
        entry.capabilities.context_length = 2048;
        entry.capabilities.vocab_size = 30000;
        
    } else {
        strcpy_s(entry.model_type, "unknown");
        strcpy_s(entry.variant, "unknown");
    }
    
    // Initialize metrics
    entry.metrics.total_requests = 0;
    entry.metrics.successful_requests = 0;
    entry.metrics.failed_requests = 0;
    entry.metrics.total_tokens_processed = 0;
    entry.metrics.average_response_time_ms = 0;
    entry.metrics.memory_usage_mb = 0;
    entry.metrics.load_time_ms = 0;
}

static void expand_path(const char* path, char* expanded, size_t expanded_size) {
#ifdef _WIN32
    ExpandEnvironmentStringsA(path, expanded, (DWORD)expanded_size);
#else
    // Simple expansion for Unix-like systems
    if (path[0] == '~') {
        const char* home = getenv("HOME");
        if (home) {
            snprintf(expanded, expanded_size, "%s%s", home, path + 1);
        } else {
            strncpy_s(expanded, expanded_size, path, _TRUNCATE);
        }
    } else {
        strncpy_s(expanded, expanded_size, path, _TRUNCATE);
    }
#endif
}

// ================================================
// Model Discovery Implementation  
// ================================================
static size_t scan_directory_for_models(const char* directory) {
    char expanded_dir[MAX_PATH_LEN];
    expand_path(directory, expanded_dir, sizeof(expanded_dir));
    
    size_t models_found = 0;
    
#ifdef _WIN32
    char search_pattern[MAX_PATH_LEN];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*", expanded_dir);
    
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(search_pattern, &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                const char* ext = get_file_extension(findData.cFileName);
                if (is_supported_extension(ext)) {
                    // Found a model file
                    if (g_model_count.load() < MAX_MODELS) {
                        size_t index = g_model_count.fetch_add(1);
                        ModelEntry& entry = g_models[index];
                        
                        // Build full path
                        snprintf(entry.file_path, sizeof(entry.file_path), 
                                "%s\\%s", expanded_dir, findData.cFileName);
                        
                        // Generate name from filename (without extension)
                        strncpy_s(entry.name, sizeof(entry.name), findData.cFileName, _TRUNCATE);
                        char* dot = strrchr(entry.name, '.');
                        if (dot) *dot = '\0';
                        
                        // Generate display name (prettified)
                        strncpy_s(entry.display_name, sizeof(entry.display_name), entry.name, _TRUNCATE);
                        
                        entry.file_size = get_file_size(entry.file_path);
                        entry.last_modified = get_file_modified_time(entry.file_path);
                        entry.last_accessed = 0;
                        entry.is_available = true;
                        entry.is_loaded = false;
                        entry.auto_discovered = true;
                        entry.model_handle = nullptr;
                        
                        parse_model_info(entry);
                        models_found++;
                    }
                }
            }
        } while (FindNextFileA(hFind, &findData) && g_model_count.load() < MAX_MODELS);
        
        FindClose(hFind);
    }
    
#else
    // POSIX implementation
    DIR* dir = opendir(expanded_dir);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr && g_model_count.load() < MAX_MODELS) {
            if (entry->d_type == DT_REG) {  // Regular file
                const char* ext = get_file_extension(entry->d_name);
                if (is_supported_extension(ext)) {
                    size_t index = g_model_count.fetch_add(1);
                    ModelEntry& model_entry = g_models[index];
                    
                    snprintf(model_entry.file_path, sizeof(model_entry.file_path), 
                            "%s/%s", expanded_dir, entry->d_name);
                    
                    strncpy_s(model_entry.name, sizeof(model_entry.name), entry->d_name, _TRUNCATE);
                    char* dot = strrchr(model_entry.name, '.');
                    if (dot) *dot = '\0';
                    
                    strncpy_s(model_entry.display_name, sizeof(model_entry.display_name), model_entry.name, _TRUNCATE);
                    
                    model_entry.file_size = get_file_size(model_entry.file_path);
                    model_entry.last_modified = get_file_modified_time(model_entry.file_path);
                    model_entry.last_accessed = 0;
                    model_entry.is_available = true;
                    model_entry.is_loaded = false;
                    model_entry.auto_discovered = true;
                    model_entry.model_handle = nullptr;
                    
                    parse_model_info(model_entry);
                    models_found++;
                }
            }
        }
        closedir(dir);
    }
#endif
    
    return models_found;
}

// ================================================
// Public API Implementation
// ================================================
RegistryResult registry_initialize() {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    
    // Clear existing entries
    g_model_count.store(0);
    memset(g_models, 0, sizeof(g_models));
    
    return make_result(REGISTRY_SUCCESS, "Registry initialized");
}

RegistryResult registry_discover_models() {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    
    size_t initial_count = g_model_count.load();
    
    // Scan all configured paths
    for (const char** path = SEARCH_PATHS; *path; ++path) {
        scan_directory_for_models(*path);
    }
    
    size_t found_count = g_model_count.load() - initial_count;
    
    char message[128];
    snprintf(message, sizeof(message), "Discovered %zu models", found_count);
    
    return make_result(REGISTRY_SUCCESS, message);
}

RegistryResult registry_add_model(const char* name, const char* file_path, 
                                 const char* display_name, bool auto_load) {
    if (!name || !file_path) {
        return make_result(REGISTRY_ERROR, "Invalid parameters");
    }
    
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    
    if (g_model_count.load() >= MAX_MODELS) {
        return make_result(REGISTRY_ERROR, "Maximum models exceeded");
    }
    
    // Check if model already exists
    for (size_t i = 0; i < g_model_count.load(); ++i) {
        if (strcmp(g_models[i].name, name) == 0) {
            return make_result(REGISTRY_ERROR, "Model already exists");
        }
    }
    
    // Verify file exists
    if (get_file_size(file_path) == 0) {
        return make_result(REGISTRY_ERROR, "Model file not found or empty");
    }
    
    size_t index = g_model_count.fetch_add(1);
    ModelEntry& entry = g_models[index];
    
    strncpy_s(entry.name, sizeof(entry.name), name, _TRUNCATE);
    strncpy_s(entry.file_path, sizeof(entry.file_path), file_path, _TRUNCATE);
    
    if (display_name) {
        strncpy_s(entry.display_name, sizeof(entry.display_name), display_name, _TRUNCATE);
    } else {
        strncpy_s(entry.display_name, sizeof(entry.display_name), name, _TRUNCATE);
    }
    
    entry.file_size = get_file_size(file_path);
    entry.last_modified = get_file_modified_time(file_path);
    entry.last_accessed = 0;
    entry.is_available = true;
    entry.is_loaded = false;
    entry.auto_discovered = false;
    entry.model_handle = nullptr;
    
    parse_model_info(entry);
    
    return make_result(REGISTRY_SUCCESS, "Model added successfully");
}

RegistryResult registry_remove_model(const char* name) {
    if (!name) {
        return make_result(REGISTRY_ERROR, "Invalid model name");
    }
    
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    
    for (size_t i = 0; i < g_model_count.load(); ++i) {
        if (strcmp(g_models[i].name, name) == 0) {
            // Unload if loaded
            if (g_models[i].is_loaded) {
                // Would implement model unloading here
                g_models[i].is_loaded = false;
                g_models[i].model_handle = nullptr;
            }
            
            // Shift remaining models down
            for (size_t j = i; j < g_model_count.load() - 1; ++j) {
                g_models[j] = g_models[j + 1];
            }
            
            g_model_count.fetch_sub(1);
            memset(&g_models[g_model_count.load()], 0, sizeof(ModelEntry));
            
            return make_result(REGISTRY_SUCCESS, "Model removed");
        }
    }
    
    return make_result(REGISTRY_ERROR, "Model not found");
}

RegistryResult registry_get_model_info(const char* name, ModelInfo* info) {
    if (!name || !info) {
        return make_result(REGISTRY_ERROR, "Invalid parameters");
    }
    
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    
    for (size_t i = 0; i < g_model_count.load(); ++i) {
        if (strcmp(g_models[i].name, name) == 0) {
            const ModelEntry& entry = g_models[i];
            
            strncpy_s(info->name, sizeof(info->name), entry.name, _TRUNCATE);
            strncpy_s(info->display_name, sizeof(info->display_name), entry.display_name, _TRUNCATE);
            strncpy_s(info->file_path, sizeof(info->file_path), entry.file_path, _TRUNCATE);
            strncpy_s(info->model_type, sizeof(info->model_type), entry.model_type, _TRUNCATE);
            strncpy_s(info->variant, sizeof(info->variant), entry.variant, _TRUNCATE);
            
            info->capabilities = entry.capabilities;
            info->metrics = entry.metrics;
            info->file_size = entry.file_size;
            info->last_modified = entry.last_modified;
            info->is_available = entry.is_available;
            info->is_loaded = entry.is_loaded;
            
            return make_result(REGISTRY_SUCCESS, "Model info retrieved");
        }
    }
    
    return make_result(REGISTRY_ERROR, "Model not found");
}

RegistryResult registry_list_models(ModelInfo* models, size_t max_models, size_t* count) {
    if (!models || !count) {
        return make_result(REGISTRY_ERROR, "Invalid parameters");
    }
    
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    
    size_t total_models = g_model_count.load();
    *count = std::min(total_models, max_models);
    
    for (size_t i = 0; i < *count; ++i) {
        const ModelEntry& entry = g_models[i];
        ModelInfo& info = models[i];
        
        strncpy_s(info.name, sizeof(info.name), entry.name, _TRUNCATE);
        strncpy_s(info.display_name, sizeof(info.display_name), entry.display_name, _TRUNCATE);
        strncpy_s(info.file_path, sizeof(info.file_path), entry.file_path, _TRUNCATE);
        strncpy_s(info.model_type, sizeof(info.model_type), entry.model_type, _TRUNCATE);
        strncpy_s(info.variant, sizeof(info.variant), entry.variant, _TRUNCATE);
        
        info.capabilities = entry.capabilities;
        info.metrics = entry.metrics;
        info.file_size = entry.file_size;
        info.last_modified = entry.last_modified;
        info.is_available = entry.is_available;
        info.is_loaded = entry.is_loaded;
    }
    
    return make_result(REGISTRY_SUCCESS, "Models listed");
}

RegistryResult registry_load_model(const char* name, void** handle) {
    if (!name || !handle) {
        return make_result(REGISTRY_ERROR, "Invalid parameters");
    }
    
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    
    for (size_t i = 0; i < g_model_count.load(); ++i) {
        if (strcmp(g_models[i].name, name) == 0) {
            ModelEntry& entry = g_models[i];
            
            if (entry.is_loaded) {
                *handle = entry.model_handle;
                return make_result(REGISTRY_SUCCESS, "Model already loaded");
            }
            
            if (!entry.is_available) {
                return make_result(REGISTRY_ERROR, "Model not available");
            }
            
            // TODO: Implement actual model loading based on type
            // For now, just mark as loaded with a placeholder handle
            entry.model_handle = reinterpret_cast<void*>(i + 1);  // Placeholder
            entry.is_loaded = true;
            *handle = entry.model_handle;
            
            return make_result(REGISTRY_SUCCESS, "Model loaded successfully");
        }
    }
    
    return make_result(REGISTRY_ERROR, "Model not found");
}

RegistryResult registry_unload_model(const char* name) {
    if (!name) {
        return make_result(REGISTRY_ERROR, "Invalid model name");
    }
    
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    
    for (size_t i = 0; i < g_model_count.load(); ++i) {
        if (strcmp(g_models[i].name, name) == 0) {
            ModelEntry& entry = g_models[i];
            
            if (!entry.is_loaded) {
                return make_result(REGISTRY_SUCCESS, "Model not loaded");
            }
            
            // TODO: Implement actual model unloading
            entry.model_handle = nullptr;
            entry.is_loaded = false;
            
            return make_result(REGISTRY_SUCCESS, "Model unloaded");
        }
    }
    
    return make_result(REGISTRY_ERROR, "Model not found");
}

void registry_cleanup() {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    
    // Unload all loaded models
    for (size_t i = 0; i < g_model_count.load(); ++i) {
        if (g_models[i].is_loaded) {
            // TODO: Implement proper model cleanup
            g_models[i].is_loaded = false;
            g_models[i].model_handle = nullptr;
        }
    }
    
    g_model_count.store(0);
    memset(g_models, 0, sizeof(g_models));
}
