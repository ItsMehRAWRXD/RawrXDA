// ═══════════════════════════════════════════════════════════════════════════════
// KernelDispatcher.hpp
// Pure Win32 API Kernel Dispatch Layer — Zero Dependencies
// 
// Routes LLM inference to optimal MASM x64 kernels based on:
//  - Model size (70B, 120B, 800B)
//  - Quantization level (Q4_K_M, Q2_K, etc.)
//  - Available system RAM/VRAM
//  - GPU backend (CUDA, Vulkan, ROCm)
//
// C++20, no Qt, no STL containers except array/optional
// ═══════════════════════════════════════════════════════════════════════════════

#pragma once

#include <cstdint>
#include <array>
#include <optional>
#include <windows.h>

namespace RawrXD::Kernel {

    // ─────────────────────────────────────────────────────────────────────────────
    // Kernel Tier Detection
    // ─────────────────────────────────────────────────────────────────────────────

    /// System resource tier based on available RAM
    enum class SystemTier : uint32_t {
        MOBILE = 0,         // <= 8 GB RAM → 70B Q4 (16 MB pin, 30 tok/s)
        WORKSTATION = 1,    // 8-16 GB RAM → 120B Q4 (1 GB pin, 22 tok/s)
        ENTERPRISE = 2,     // >= 16 GB RAM → 800B Q4 (4 GB pin, 0.7 tok/s)
        UNKNOWN = 0xFF
    };

    /// Model parameter tier
    enum class ModelTier : uint32_t {
        SMALL_3B = 0,       // 3.2B (llama3.2)
        MEDIUM_7B = 1,      // 7B (llama3.1)
        MEDIUM_8B = 2,      // 8B (llama3.1)
        LARGE_13B = 3,      // 13B (codellama)
        XLARGE_27B = 4,     // 27B (gemma3)
        GIANT_70B = 5,      // 70B+
        UNKNOWN = 0xFF
    };

    /// Quantization format
    enum class QuantFormat : uint32_t {
        F32 = 0,
        F16 = 1,
        Q4_K_M = 2,         // Most common, 4.7 GB for 7B
        Q4_K_S = 3,
        Q3_K = 4,
        Q2_K = 5,           // Aggressive, ~2.3 GB for 7B
        Q5_K = 6,
        Q6_K = 7,
        IQ4_NL = 8,
        UNKNOWN = 0xFF
    };

    /// GPU backend type
    enum class GPUBackend : uint32_t {
        NONE = 0,           // CPU only
        CUDA = 1,
        VULKAN = 2,
        ROCm = 3,
        METAL = 4,
        UNKNOWN = 0xFF
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // Model Metadata
    // ─────────────────────────────────────────────────────────────────────────────

    struct ModelMetadata {
        ModelTier param_tier;           // Detected from model size
        QuantFormat quant_format;       // Detected from GGUF header
        uint64_t file_size_bytes;       // Total model size
        uint32_t vocab_size;            // Tokenizer vocab
        uint32_t context_length;        // Max context window
        uint32_t num_layers;            // Transformer layers
        uint32_t num_heads;             // Attention heads
        uint32_t hidden_dim;            // Hidden dimension
        uint8_t reserved[32];           // Future use
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // System Info
    // ─────────────────────────────────────────────────────────────────────────────

    struct SystemInfo {
        SystemTier tier;                // Detected from available RAM
        uint64_t physical_ram_bytes;    // Total RAM
        uint64_t available_ram_bytes;   // Free RAM
        GPUBackend primary_gpu;         // Detected GPU
        uint32_t gpu_memory_mb;         // VRAM size
        uint32_t cpu_cores;             // Physical cores
        bool supports_avx512f;          // CPU feature flags
        bool supports_avx2;
        bool supports_f16c;
        uint8_t reserved[32];           // Future use
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // Kernel Configuration Output
    // ─────────────────────────────────────────────────────────────────────────────

    struct KernelConfig {
        SystemTier system_tier;
        ModelTier model_tier;
        QuantFormat quant;
        GPUBackend gpu;

        // TurboSparse parameters
        struct {
            bool enabled;
            uint32_t skip_percentage;   // ~20% = skip 1/5 neurons
            uint32_t bitmap_size_bytes;
        } sparse_config;

        // PowerInfer parameters
        struct {
            bool enabled;
            uint32_t gpu_ratio_percent; // 0-100, GPU neurons
            uint32_t cpu_ratio_percent; // 0-100, CPU neurons
            uint32_t hot_slab_count;    // First N slabs = GPU
        } power_infer_config;

        // KV Cache parameters
        struct {
            uint32_t slot_count;        // Number of KV cache slots
            uint32_t slot_size_bytes;   // Per-slot size
            uint32_t max_context_len;   // Max sequence length
        } kv_config;

        // Prefetch parameters
        struct {
            bool enabled;
            uint32_t buffer_size_bytes;
            uint32_t lookahead_tokens;  // How many tokens ahead to prefetch
        } prefetch_config;

        // Token generation
        struct {
            uint32_t batch_size;
            float temperature;
            float top_p;
            uint32_t top_k;
            uint32_t max_tokens_per_call;
        } generation_config;

        uint8_t reserved[128];          // Future use
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // Main Dispatcher Class
    // ─────────────────────────────────────────────────────────────────────────────

    class KernelDispatcher {
    public:
        KernelDispatcher();
        ~KernelDispatcher();

        // Initialization
        bool Initialize();
        void Shutdown();

        // System detection
        bool DetectSystemInfo(SystemInfo& out_info);

        // Model analysis
        bool AnalyzeModel(const wchar_t* model_path, ModelMetadata& out_metadata);
        bool AnalyzeModelFromBuffer(const uint8_t* gguf_header, size_t header_size, ModelMetadata& out_metadata);

        // Kernel selection
        bool SelectKernel(const SystemInfo& sys_info, const ModelMetadata& model_meta, KernelConfig& out_config);

        // Kernel loading & execution
        bool LoadKernel(const KernelConfig& config);
        bool UnloadKernel();

        // Low-level kernel invocation (type-erased function pointers)
        // These call into the loaded MASM kernels
        bool InvokeDetectTier(SystemTier& out_tier);
        bool InvokePowerInferLoop(uint32_t max_tokens, uint64_t& out_tokens_processed);
        bool InvokeGenerateTokens(const uint8_t* prompt, size_t prompt_len, uint8_t* output, size_t output_cap, size_t& out_len);

        // Query current state
        bool IsKernelLoaded() const;
        SystemTier GetCurrentSystemTier() const;
        KernelConfig GetCurrentConfig() const;

    private:
        // DLL loading
        HMODULE m_pocket_lab_dll;       // pocket_lab_turbo.dll handle
        HMODULE m_phase3_dll;           // Phase3_Agent_Kernel.dll handle

        // Kernel function pointers
        using PFN_PocketLabInit = int (*)(void);
        using PFN_PocketLabGetStats = void (*)(uint64_t* out_tokens, uint64_t* out_sparse_skips, uint64_t* out_gpu, uint64_t* out_cpu);
        using PFN_PocketLabRunCycle = int (*)(void);
        using PFN_PocketLabShutdown = void (*)(void);

        using PFN_Phase3Initialize = void* (*)(void* phase1_ctx, void* phase2_ctx);
        using PFN_GenerateTokens = int (*)(void* context, const char* prompt, void* params);
        using PFN_Phase3Shutdown = void (*)(void* context);

        PFN_PocketLabInit m_pfn_pocket_lab_init;
        PFN_PocketLabGetStats m_pfn_pocket_lab_get_stats;
        PFN_PocketLabRunCycle m_pfn_pocket_lab_run_cycle;
        PFN_PocketLabShutdown m_pfn_pocket_lab_shutdown;

        PFN_Phase3Initialize m_pfn_phase3_init;
        PFN_GenerateTokens m_pfn_generate_tokens;
        PFN_Phase3Shutdown m_pfn_phase3_shutdown;

        // State
        SystemInfo m_system_info;
        KernelConfig m_current_config;
        void* m_phase3_context;
        bool m_is_loaded;

        // Helper methods
        bool LoadDLL(const wchar_t* dll_name, HMODULE& out_handle);
        bool GetProcAddress(HMODULE dll, const char* proc_name, void*& out_proc);
        SystemTier DetectSystemTierFromRAM(uint64_t physical_ram);
        ModelTier DetectModelTierFromSize(uint64_t file_size);
        QuantFormat DetectQuantFromGGUFHeader(const uint8_t* header);
    };

}  // namespace RawrXD::Kernel
