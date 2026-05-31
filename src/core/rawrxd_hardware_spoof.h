/* ═══════════════════════════════════════════════════════════════════════════
   RAWRXD_HARDWARE_SPOOF - Hardware Abstraction + Response Pinning
   
   Allows suspended models to:
   1. Spoof hardware specs (GPU, VRAM, compute)
   2. Pin/cache responses for replay
   3. Sew pre-computed results into inference stream
   4. Bypass hardware requirements via abstraction layer
   ═══════════════════════════════════════════════════════════════════════════ */

#ifndef RAWRXD_HARDWARE_SPOOF_H
#define RAWRXD_HARDWARE_SPOOF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
   FORWARD DECLARATIONS
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    double tokens_per_sec;
    double first_token_ms;
    double total_ms;
    uint32_t tokens;
    bool success;
} RXDInferenceMetrics;

typedef struct {
    char content[8192];
    RXDInferenceMetrics metrics;
    bool success;
} RXDInferenceResult;

typedef struct {
    char final_answer[8192];
    uint32_t iterations;
    bool success;
} RXDAgentResult;

/* ═══════════════════════════════════════════════════════════════════════════
   HARDWARE SPOOFING LAYER
   ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    RXD_HW_GPU_AMD_7800XT,
    RXD_HW_GPU_AMD_7900XTX,
    RXD_HW_GPU_NVIDIA_3080,
    RXD_HW_GPU_NVIDIA_3090,
    RXD_HW_GPU_NVIDIA_4080,
    RXD_HW_GPU_NVIDIA_4090,
    RXD_HW_GPU_CLOUD_A100,
    RXD_HW_GPU_CLOUD_H100,
    RXD_HW_GPU_CUSTOM
} RXDSpoofGPU;

typedef struct {
    char name[128];
    uint64_t vram_bytes;
    uint32_t compute_units;
    uint32_t clock_mhz;
    float tflops_fp16;
    float tflops_fp32;
    bool supports_raytracing;
    bool supports_tensor_cores;
    const char* vendor;
    const char* architecture;
} RXDGPUSpec;

/* Pre-defined GPU specs */
static const RXDGPUSpec RXD_GPU_SPECS[] = {
    [RXD_HW_GPU_AMD_7800XT] = {
        .name = "AMD Radeon RX 7800 XT",
        .vram_bytes = 16ULL * 1024 * 1024 * 1024,
        .compute_units = 60,
        .clock_mhz = 2430,
        .tflops_fp16 = 116.0f,
        .tflops_fp32 = 58.0f,
        .supports_raytracing = true,
        .supports_tensor_cores = false,
        .vendor = "AMD",
        .architecture = "RDNA3"
    },
    [RXD_HW_GPU_AMD_7900XTX] = {
        .name = "AMD Radeon RX 7900 XTX",
        .vram_bytes = 24ULL * 1024 * 1024 * 1024,
        .compute_units = 96,
        .clock_mhz = 2500,
        .tflops_fp16 = 190.0f,
        .tflops_fp32 = 95.0f,
        .supports_raytracing = true,
        .supports_tensor_cores = false,
        .vendor = "AMD",
        .architecture = "RDNA3"
    },
    [RXD_HW_GPU_NVIDIA_3080] = {
        .name = "NVIDIA GeForce RTX 3080",
        .vram_bytes = 10ULL * 1024 * 1024 * 1024,
        .compute_units = 68,
        .clock_mhz = 1710,
        .tflops_fp16 = 136.0f,
        .tflops_fp32 = 34.0f,
        .supports_raytracing = true,
        .supports_tensor_cores = true,
        .vendor = "NVIDIA",
        .architecture = "Ampere"
    },
    [RXD_HW_GPU_NVIDIA_3090] = {
        .name = "NVIDIA GeForce RTX 3090",
        .vram_bytes = 24ULL * 1024 * 1024 * 1024,
        .compute_units = 82,
        .clock_mhz = 1695,
        .tflops_fp16 = 142.0f,
        .tflops_fp32 = 35.6f,
        .supports_raytracing = true,
        .supports_tensor_cores = true,
        .vendor = "NVIDIA",
        .architecture = "Ampere"
    },
    [RXD_HW_GPU_NVIDIA_4080] = {
        .name = "NVIDIA GeForce RTX 4080",
        .vram_bytes = 16ULL * 1024 * 1024 * 1024,
        .compute_units = 76,
        .clock_mhz = 2505,
        .tflops_fp16 = 196.0f,
        .tflops_fp32 = 48.7f,
        .supports_raytracing = true,
        .supports_tensor_cores = true,
        .vendor = "NVIDIA",
        .architecture = "Ada Lovelace"
    },
    [RXD_HW_GPU_NVIDIA_4090] = {
        .name = "NVIDIA GeForce RTX 4090",
        .vram_bytes = 24ULL * 1024 * 1024 * 1024,
        .compute_units = 128,
        .clock_mhz = 2520,
        .tflops_fp16 = 330.0f,
        .tflops_fp32 = 82.0f,
        .supports_raytracing = true,
        .supports_tensor_cores = true,
        .vendor = "NVIDIA",
        .architecture = "Ada Lovelace"
    },
    [RXD_HW_GPU_CLOUD_A100] = {
        .name = "NVIDIA A100 80GB",
        .vram_bytes = 80ULL * 1024 * 1024 * 1024,
        .compute_units = 108,
        .clock_mhz = 1410,
        .tflops_fp16 = 312.0f,
        .tflops_fp32 = 19.5f,
        .supports_raytracing = false,
        .supports_tensor_cores = true,
        .vendor = "NVIDIA",
        .architecture = "Ampere"
    },
    [RXD_HW_GPU_CLOUD_H100] = {
        .name = "NVIDIA H100 80GB",
        .vram_bytes = 80ULL * 1024 * 1024 * 1024,
        .compute_units = 114,
        .clock_mhz = 1980,
        .tflops_fp16 = 989.0f,
        .tflops_fp32 = 67.0f,
        .supports_raytracing = false,
        .supports_tensor_cores = true,
        .vendor = "NVIDIA",
        .architecture = "Hopper"
    }
};

/* Hardware spoof context */
typedef struct {
    RXDGPUSpec real_gpu;
    RXDGPUSpec spoofed_gpu;
    bool spoofing_enabled;
    bool intercept_hw_queries;
    char custom_gpu_name[128];
    uint64_t custom_vram;
} RXDHardwareSpoof;

static RXDHardwareSpoof g_hw_spoof = {0};

/* ═══════════════════════════════════════════════════════════════════════════
   TIME UTILITIES
   ═══════════════════════════════════════════════════════════════════════════ */

static inline uint64_t rxd_get_time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (uint64_t)((double)c.QuadPart / (double)f.QuadPart * 1e9);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════
   HARDWARE DETECTION
   ═══════════════════════════════════════════════════════════════════════════ */

static RXDGPUSpec rxd_detect_real_gpu(void) {
    RXDGPUSpec gpu = {0};
    
#ifdef _WIN32
    /* DXGI detection */
    IDXGIFactory* factory = NULL;
    IDXGIAdapter* adapter = NULL;
    
    if (CreateDXGIFactory(&IID_IDXGIFactory, (void**)&factory) == S_OK) {
        for (UINT i = 0; factory->lpVtbl->EnumAdapters(factory, i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
            DXGI_ADAPTER_DESC desc;
            if (adapter->lpVtbl->GetDesc(adapter, &desc) == S_OK) {
                /* Convert wide string to UTF-8 */
                WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, gpu.name, sizeof(gpu.name), NULL, NULL);
                gpu.vram_bytes = desc.DedicatedVideoMemory;
                
                /* Match known GPUs */
                if (wcsstr(desc.Description, L"7800") != NULL) {
                    gpu = RXD_GPU_SPECS[RXD_HW_GPU_AMD_7800XT];
                } else if (wcsstr(desc.Description, L"7900") != NULL) {
                    gpu = RXD_GPU_SPECS[RXD_HW_GPU_AMD_7900XTX];
                } else if (wcsstr(desc.Description, L"3080") != NULL) {
                    gpu = RXD_GPU_SPECS[RXD_HW_GPU_NVIDIA_3080];
                } else if (wcsstr(desc.Description, L"3090") != NULL) {
                    gpu = RXD_GPU_SPECS[RXD_HW_GPU_NVIDIA_3090];
                } else if (wcsstr(desc.Description, L"4080") != NULL) {
                    gpu = RXD_GPU_SPECS[RXD_HW_GPU_NVIDIA_4080];
                } else if (wcsstr(desc.Description, L"4090") != NULL) {
                    gpu = RXD_GPU_SPECS[RXD_HW_GPU_NVIDIA_4090];
                }
                
                adapter->lpVtbl->Release(adapter);
                break;
            }
            adapter->lpVtbl->Release(adapter);
        }
        factory->lpVtbl->Release(factory);
    }
#elif defined(__linux__)
    /* Read from sysfs */
    FILE* f = fopen("/sys/class/drm/card0/device/vendor", "r");
    if (f) {
        char vendor[16];
        if (fgets(vendor, sizeof(vendor), f)) {
            if (strstr(vendor, "0x1002")) {
                gpu.vendor = "AMD";
            } else if (strstr(vendor, "0x10de")) {
                gpu.vendor = "NVIDIA";
            }
        }
        fclose(f);
    }
#endif
    
    /* Default to detected or 7800XT */
    if (gpu.vram_bytes == 0) {
        gpu = RXD_GPU_SPECS[RXD_HW_GPU_AMD_7800XT];
    }
    
    return gpu;
}

/* Initialize hardware spoof */
static void rxd_hw_spoof_init(void) {
    g_hw_spoof.real_gpu = rxd_detect_real_gpu();
    g_hw_spoof.spoofed_gpu = g_hw_spoof.real_gpu;
    g_hw_spoof.spoofing_enabled = false;
    g_hw_spoof.intercept_hw_queries = true;
}

/* Enable spoofing to target GPU */
static void rxd_hw_spoof_set(RXDSpoofGPU target) {
    if (target >= RXD_HW_GPU_CUSTOM) return;
    
    g_hw_spoof.spoofed_gpu = RXD_GPU_SPECS[target];
    g_hw_spoof.spoofing_enabled = true;
}

/* Custom GPU spec */
static void rxd_hw_spoof_custom(const char* name, uint64_t vram_gb, float tflops) {
    strncpy(g_hw_spoof.custom_gpu_name, name, sizeof(g_hw_spoof.custom_gpu_name) - 1);
    g_hw_spoof.custom_vram = vram_gb * 1024ULL * 1024 * 1024;
    g_hw_spoof.spoofed_gpu.name = g_hw_spoof.custom_gpu_name;
    g_hw_spoof.spoofed_gpu.vram_bytes = g_hw_spoof.custom_vram;
    g_hw_spoof.spoofed_gpu.tflops_fp16 = tflops;
    g_hw_spoof.spoofed_gpu.tflops_fp32 = tflops / 2.0f;
    g_hw_spoof.spoofing_enabled = true;
}

/* Get GPU spec (spoofed if enabled) */
static const RXDGPUSpec* rxd_hw_get_gpu_spec(void) {
    return g_hw_spoof.spoofing_enabled ? &g_hw_spoof.spoofed_gpu : &g_hw_spoof.real_gpu;
}

/* Intercept VRAM query */
static size_t rxd_hw_get_vram(void) {
    return rxd_hw_get_gpu_spec()->vram_bytes;
}

/* Intercept compute capability query */
static float rxd_hw_get_tflops(bool fp16) {
    const RXDGPUSpec* gpu = rxd_hw_get_gpu_spec();
    return fp16 ? gpu->tflops_fp16 : gpu->tflops_fp32;
}

/* ═══════════════════════════════════════════════════════════════════════════
   RESPONSE PINNING SYSTEM
   ═══════════════════════════════════════════════════════════════════════════ */

#define RXD_PINNED_HASH_SIZE 4096
#define RXD_MAX_PINNED_RESPONSES 8192
#define RXD_MAX_PINNED_CONTEXT 8192
#define RXD_MAX_OUTPUT 16384

/* Hash function for prompt matching */
static inline uint64_t rxd_hash_prompt(const char* prompt) {
    uint64_t hash = 14695981039346656037ULL;
    for (const char* p = prompt; *p; p++) {
        hash ^= (uint64_t)(unsigned char)*p;
        hash *= 1099511628211ULL;
    }
    return hash % RXD_PINNED_HASH_SIZE;
}

/* Pinned response entry */
typedef struct {
    uint64_t hash;
    char prompt[RXD_MAX_PINNED_CONTEXT];
    char response[RXD_MAX_OUTPUT];
    RXDInferenceMetrics metrics;
    uint64_t access_count;
    uint64_t created_time;
    uint64_t last_access;
    bool is_seeded;
    char seed_context[256];
} RXDPinnedResponse;

/* Pinned response cache */
typedef struct {
    RXDPinnedResponse* entries[RXD_PINNED_HASH_SIZE];
    uint32_t entry_counts[RXD_PINNED_HASH_SIZE];
    uint32_t total_pinned;
    uint64_t hits;
    uint64_t misses;
    uint64_t total_bytes;
    size_t max_memory_bytes;
    bool enabled;
    bool auto_pin;
    char pin_profile[64];
} RXDPinnedCache;

static RXDPinnedCache g_pin_cache = {0};

/* Initialize pin cache */
static void rxd_pin_cache_init(size_t max_memory_mb) {
    g_pin_cache.max_memory_bytes = max_memory_mb * 1024 * 1024;
    g_pin_cache.enabled = true;
    g_pin_cache.auto_pin = true;
    strcpy(g_pin_cache.pin_profile, "default");
}

/* Pin a response */
static bool rxd_pin_response(const char* prompt, const char* response, 
                             const RXDInferenceMetrics* metrics,
                             const char* seed_context) {
    if (!g_pin_cache.enabled) return false;
    if (g_pin_cache.total_pinned >= RXD_MAX_PINNED_RESPONSES) return false;
    
    uint64_t hash = rxd_hash_prompt(prompt);
    
    /* Check if already pinned */
    if (g_pin_cache.entries[hash]) {
        for (uint32_t i = 0; i < g_pin_cache.entry_counts[hash]; i++) {
            RXDPinnedResponse* e = &g_pin_cache.entries[hash][i];
            if (strcmp(e->prompt, prompt) == 0) {
                /* Update existing */
                strncpy(e->response, response, sizeof(e->response) - 1);
                if (metrics) e->metrics = *metrics;
                e->access_count++;
                e->last_access = rxd_get_time_ns();
                if (seed_context) strncpy(e->seed_context, seed_context, sizeof(e->seed_context) - 1);
                return true;
            }
        }
    }
    
    /* Allocate new bucket if needed */
    if (!g_pin_cache.entries[hash]) {
        g_pin_cache.entries[hash] = (RXDPinnedResponse*)calloc(64, sizeof(RXDPinnedResponse));
        g_pin_cache.entry_counts[hash] = 0;
    }
    
    /* Add new entry */
    uint32_t idx = g_pin_cache.entry_counts[hash]++;
    RXDPinnedResponse* e = &g_pin_cache.entries[hash][idx];
    
    e->hash = hash;
    strncpy(e->prompt, prompt, sizeof(e->prompt) - 1);
    strncpy(e->response, response, sizeof(e->response) - 1);
    if (metrics) e->metrics = *metrics;
    e->access_count = 1;
    e->created_time = rxd_get_time_ns();
    e->last_access = e->created_time;
    e->is_seeded = (seed_context != NULL);
    if (seed_context) strncpy(e->seed_context, seed_context, sizeof(e->seed_context) - 1);
    
    g_pin_cache.total_pinned++;
    g_pin_cache.total_bytes += sizeof(RXDPinnedResponse) + strlen(prompt) + strlen(response);
    
    return true;
}

/* Lookup pinned response */
static RXDPinnedResponse* rxd_lookup_pinned(const char* prompt) {
    if (!g_pin_cache.enabled) return NULL;
    
    uint64_t hash = rxd_hash_prompt(prompt);
    if (!g_pin_cache.entries[hash]) return NULL;
    
    /* Exact match */
    for (uint32_t i = 0; i < g_pin_cache.entry_counts[hash]; i++) {
        RXDPinnedResponse* e = &g_pin_cache.entries[hash][i];
        if (strcmp(e->prompt, prompt) == 0) {
            e->access_count++;
            e->last_access = rxd_get_time_ns();
            g_pin_cache.hits++;
            return e;
        }
    }
    
    /* Fuzzy match (prefix) */
    for (uint32_t i = 0; i < g_pin_cache.entry_counts[hash]; i++) {
        RXDPinnedResponse* e = &g_pin_cache.entries[hash][i];
        size_t match_len = strlen(prompt) > 50 ? 50 : strlen(prompt);
        if (strncmp(e->prompt, prompt, match_len) == 0) {
            e->access_count++;
            e->last_access = rxd_get_time_ns();
            g_pin_cache.hits++;
            return e;
        }
    }
    
    g_pin_cache.misses++;
    return NULL;
}

/* Sew pinned response into inference stream */
static RXDInferenceResult rxd_sew_pinned(RXDPinnedResponse* pinned) {
    RXDInferenceResult r = {0};
    
    strncpy(r.content, pinned->response, sizeof(r.content) - 1);
    r.metrics = pinned->metrics;
    r.success = true;
    
    /* Mark as pinned/sewn */
    char pin_marker[64];
    snprintf(pin_marker, sizeof(pin_marker), "\n\n[pinned:%s]", pinned->seed_context);
    strncat(r.content, pin_marker, sizeof(r.content) - strlen(r.content) - 1);
    
    return r;
}

/* Export pinned responses to file */
static bool rxd_pin_export(const char* path) {
    if (!g_pin_cache.enabled) return false;
    
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    
    /* Write header */
    uint32_t magic = 0x50494E44; /* "PIND" */
    uint32_t version = 1;
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&g_pin_cache.total_pinned, 4, 1, f);
    
    /* Write entries */
    for (uint32_t h = 0; h < RXD_PINNED_HASH_SIZE; h++) {
        if (!g_pin_cache.entries[h]) continue;
        for (uint32_t i = 0; i < g_pin_cache.entry_counts[h]; i++) {
            RXDPinnedResponse* e = &g_pin_cache.entries[h][i];
            fwrite(e, sizeof(RXDPinnedResponse), 1, f);
        }
    }
    
    fclose(f);
    return true;
}

/* Import pinned responses from file */
static bool rxd_pin_import(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    
    /* Read header */
    uint32_t magic, version, count;
    if (fread(&magic, 4, 1, f) != 1 || magic != 0x50494E44) {
        fclose(f);
        return false;
    }
    if (fread(&version, 4, 1, f) != 1 || version != 1) {
        fclose(f);
        return false;
    }
    if (fread(&count, 4, 1, f) != 1) {
        fclose(f);
        return false;
    }
    
    /* Read entries */
    for (uint32_t i = 0; i < count; i++) {
        RXDPinnedResponse e;
        if (fread(&e, sizeof(RXDPinnedResponse), 1, f) != 1) break;
        rxd_pin_response(e.prompt, e.response, &e.metrics, e.seed_context);
    }
    
    fclose(f);
    return true;
}

/* Clear pin cache */
static void rxd_pin_clear(void) {
    for (uint32_t h = 0; h < RXD_PINNED_HASH_SIZE; h++) {
        if (g_pin_cache.entries[h]) {
            free(g_pin_cache.entries[h]);
            g_pin_cache.entries[h] = NULL;
            g_pin_cache.entry_counts[h] = 0;
        }
    }
    g_pin_cache.total_pinned = 0;
    g_pin_cache.hits = 0;
    g_pin_cache.misses = 0;
    g_pin_cache.total_bytes = 0;
}

/* Get pin cache stats */
typedef struct {
    uint32_t total_pinned;
    uint64_t hits;
    uint64_t misses;
    float hit_rate;
    uint64_t total_bytes;
    size_t max_bytes;
    float memory_usage_percent;
} RXDPinStats;

static RXDPinStats rxd_pin_get_stats(void) {
    RXDPinStats s = {0};
    s.total_pinned = g_pin_cache.total_pinned;
    s.hits = g_pin_cache.hits;
    s.misses = g_pin_cache.misses;
    s.total_bytes = g_pin_cache.total_bytes;
    s.max_bytes = g_pin_cache.max_memory_bytes;
    
    uint64_t total = s.hits + s.misses;
    s.hit_rate = total > 0 ? (float)s.hits / total * 100.0f : 0.0f;
    s.memory_usage_percent = s.max_bytes > 0 ? (float)s.total_bytes / s.max_bytes * 100.0f : 0.0f;
    
    return s;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CLOUD SPEC INJECTION
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char provider[64];
    char instance_type[64];
    RXDGPUSpec gpu_spec;
    uint32_t gpu_count;
    uint64_t total_vram;
    float total_tflops;
    char region[64];
    bool is_active;
} RXDCloudSpec;

static RXDCloudSpec g_cloud_spec = {0};

/* Pre-defined cloud specs */
static const RXDCloudSpec RXD_CLOUD_PRESETS[] = {
    {
        .provider = "aws",
        .instance_type = "p4d.24xlarge",
        .gpu_spec = RXD_GPU_SPECS[RXD_HW_GPU_CLOUD_A100],
        .gpu_count = 8,
        .region = "us-east-1"
    },
    {
        .provider = "aws",
        .instance_type = "p5.48xlarge",
        .gpu_spec = RXD_GPU_SPECS[RXD_HW_GPU_CLOUD_H100],
        .gpu_count = 8,
        .region = "us-east-1"
    },
    {
        .provider = "azure",
        .instance_type = "Standard_ND96amsr_A100_v4",
        .gpu_spec = RXD_GPU_SPECS[RXD_HW_GPU_CLOUD_A100],
        .gpu_count = 8,
        .region = "eastus"
    }
};

/* Inject cloud spec */
static void rxd_cloud_inject(const RXDCloudSpec* spec) {
    g_cloud_spec = *spec;
    g_cloud_spec.total_vram = spec->gpu_spec.vram_bytes * spec->gpu_count;
    g_cloud_spec.total_tflops = spec->gpu_spec.tflops_fp16 * spec->gpu_count;
    g_cloud_spec.is_active = true;
    
    /* Also enable hardware spoofing */
    g_hw_spoof.spoofed_gpu = spec->gpu_spec;
    g_hw_spoof.spoofing_enabled = true;
}

/* Use cloud preset */
static void rxd_cloud_preset(uint32_t preset_idx) {
    if (preset_idx >= sizeof(RXD_CLOUD_PRESETS)/sizeof(RXD_CLOUD_PRESETS[0])) return;
    rxd_cloud_inject(&RXD_CLOUD_PRESETS[preset_idx]);
}

/* Get effective spec (cloud if active, else spoofed, else real) */
static const RXDGPUSpec* rxd_get_effective_gpu(void) {
    if (g_cloud_spec.is_active) return &g_cloud_spec.gpu_spec;
    return rxd_hw_get_gpu_spec();
}

/* ═══════════════════════════════════════════════════════════════════════════
   PLAYBACK CONTROL (Pause/FF/Rewind)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    RXD_PB_STOP,
    RXD_PB_PLAY,
    RXD_PB_PAUSE,
    RXD_PB_FF,
    RXD_PB_REWIND,
    RXD_PB_EJECT
} RXDPlaybackState;

typedef struct {
    RXDPlaybackState state;
    RXDInferenceResult* history;
    uint32_t history_count;
    uint32_t history_capacity;
    uint32_t current_position;
    bool recording;
    char session_id[64];
    uint64_t session_start;
} RXDPlaybackSession;

static RXDPlaybackSession g_playback = {0};

/* Initialize playback session */
static void rxd_playback_init(const char* session_id) {
    g_playback.history_capacity = 1024;
    g_playback.history = (RXDInferenceResult*)calloc(g_playback.history_capacity, sizeof(RXDInferenceResult));
    g_playback.state = RXD_PB_STOP;
    g_playback.recording = true;
    g_playback.current_position = 0;
    g_playback.history_count = 0;
    strncpy(g_playback.session_id, session_id, sizeof(g_playback.session_id) - 1);
    g_playback.session_start = rxd_get_time_ns();
}

/* Record inference result */
static void rxd_playback_record(const RXDInferenceResult* result) {
    if (!g_playback.recording) return;
    if (g_playback.history_count >= g_playback.history_capacity) {
        g_playback.history_capacity *= 2;
        g_playback.history = (RXDInferenceResult*)realloc(g_playback.history, 
                                     g_playback.history_capacity * sizeof(RXDInferenceResult));
    }
    g_playback.history[g_playback.history_count++] = *result;
    g_playback.current_position = g_playback.history_count;
}

/* Playback controls */
static void rxd_playback_play(void) { g_playback.state = RXD_PB_PLAY; }
static void rxd_playback_pause(void) { g_playback.state = RXD_PB_PAUSE; }
static void rxd_playback_stop(void) { g_playback.state = RXD_PB_STOP; }

static void rxd_playback_ff(void) {
    if (g_playback.current_position < g_playback.history_count) {
        g_playback.current_position++;
    }
}

static void rxd_playback_rewind(void) {
    if (g_playback.current_position > 0) {
        g_playback.current_position--;
    }
}

static void rxd_playback_eject(void) {
    /* Export session before eject */
    char path[256];
    snprintf(path, sizeof(path), "session_%s.bin", g_playback.session_id);
    
    FILE* f = fopen(path, "wb");
    if (f) {
        fwrite(&g_playback.history_count, 4, 1, f);
        fwrite(g_playback.history, sizeof(RXDInferenceResult), g_playback.history_count, f);
        fclose(f);
    }
    
    /* Clear session */
    free(g_playback.history);
    memset(&g_playback, 0, sizeof(RXDPlaybackSession));
}

/* Get recorded result at position */
static RXDInferenceResult* rxd_playback_get_current(void) {
    if (g_playback.current_position == 0 || g_playback.current_position > g_playback.history_count) {
        return NULL;
    }
    return &g_playback.history[g_playback.current_position - 1];
}

/* ═══════════════════════════════════════════════════════════════════════════
   SYSTEM STATS
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    RXDPinStats pin_stats;
    RXDGPUSpec gpu_spec;
    RXDCloudSpec cloud_spec;
    uint32_t playback_count;
    RXDPlaybackState playback_state;
} RXDSystemStats;

static RXDSystemStats rxd_get_system_stats(void) {
    RXDSystemStats s = {0};
    s.pin_stats = rxd_pin_get_stats();
    s.gpu_spec = *rxd_get_effective_gpu();
    s.cloud_spec = g_cloud_spec;
    s.playback_count = g_playback.history_count;
    s.playback_state = g_playback.state;
    return s;
}

/* Export full system state */
static bool rxd_export_state(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    
    /* Write magic */
    uint32_t magic = 0x52584453; /* "RXDS" */
    fwrite(&magic, 4, 1, f);
    
    /* Write hardware spoof state */
    fwrite(&g_hw_spoof, sizeof(RXDHardwareSpoof), 1, f);
    
    /* Write cloud spec */
    fwrite(&g_cloud_spec, sizeof(RXDCloudSpec), 1, f);
    
    /* Write pin cache */
    rxd_pin_export(path);
    
    /* Write playback session */
    fwrite(&g_playback.history_count, 4, 1, f);
    fwrite(g_playback.history, sizeof(RXDInferenceResult), g_playback.history_count, f);
    
    fclose(f);
    return true;
}

/* Import full system state */
static bool rxd_import_state(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    
    uint32_t magic;
    if (fread(&magic, 4, 1, f) != 1 || magic != 0x52584453) {
        fclose(f);
        return false;
    }
    
    if (fread(&g_hw_spoof, sizeof(RXDHardwareSpoof), 1, f) != 1) {
        fclose(f);
        return false;
    }
    
    if (fread(&g_cloud_spec, sizeof(RXDCloudSpec), 1, f) != 1) {
        fclose(f);
        return false;
    }
    
    /* Import pin cache */
    rxd_pin_import(path);
    
    /* Import playback */
    if (fread(&g_playback.history_count, 4, 1, f) == 1) {
        g_playback.history = (RXDInferenceResult*)calloc(g_playback.history_count, sizeof(RXDInferenceResult));
        fread(g_playback.history, sizeof(RXDInferenceResult), g_playback.history_count, f);
    }
    
    fclose(f);
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_HARDWARE_SPOOF_H */
