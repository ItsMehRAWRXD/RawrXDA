// ═══════════════════════════════════════════════════════════════════════════════
// KernelDispatcher.cpp
// Pure Win32 Kernel Dispatch Implementation
// ═══════════════════════════════════════════════════════════════════════════════

#include "kernel_dispatch/KernelDispatcher.hpp"
#include <cstring>
#include <cmath>
#include <sysinfoapi.h>
#include <winbase.h>

namespace RawrXD::Kernel {

    KernelDispatcher::KernelDispatcher()
        : m_pocket_lab_dll(nullptr),
          m_phase3_dll(nullptr),
          m_pfn_pocket_lab_init(nullptr),
          m_pfn_pocket_lab_get_thermal(nullptr),
          m_pfn_pocket_lab_run_cycle(nullptr),
          m_pfn_pocket_lab_get_stats(nullptr),
          m_pfn_pocket_lab_shutdown(nullptr),
          m_pfn_phase3_init(nullptr),
          m_pfn_generate_tokens(nullptr),
          m_pfn_phase3_shutdown(nullptr),
          m_phase3_context(nullptr),
          m_is_loaded(false) {
        memset(&m_system_info, 0, sizeof(m_system_info));
        memset(&m_current_config, 0, sizeof(m_current_config));
    }

    KernelDispatcher::~KernelDispatcher() {
        Shutdown();
    }

    bool KernelDispatcher::Initialize() {
        // Detect system capabilities
        if (!DetectSystemInfo(m_system_info)) {
            return false;
        }

        // Try to load both kernel DLLs (non-fatal if missing)
        LoadDLL(L"pocket_lab_turbo.dll", m_pocket_lab_dll);
        LoadDLL(L"Phase3_Agent_Kernel.dll", m_phase3_dll);

        // Resolve function pointers for PocketLab kernel
        if (m_pocket_lab_dll) {
            void* proc = nullptr;
            if (ResolveProcAddress(m_pocket_lab_dll, "PocketLabInit_Export", proc))
                m_pfn_pocket_lab_init = reinterpret_cast<PFN_PocketLabInit>(proc);
            if (ResolveProcAddress(m_pocket_lab_dll, "PocketLabGetThermal_Export", proc))
                m_pfn_pocket_lab_get_thermal = reinterpret_cast<PFN_PocketLabGetThermal>(proc);
            if (ResolveProcAddress(m_pocket_lab_dll, "PocketLabRunCycle_Export", proc))
                m_pfn_pocket_lab_run_cycle = reinterpret_cast<PFN_PocketLabRunCycle>(proc);
            if (ResolveProcAddress(m_pocket_lab_dll, "PocketLabGetStats_Export", proc))
                m_pfn_pocket_lab_get_stats = reinterpret_cast<PFN_PocketLabGetStats>(proc);
            if (ResolveProcAddress(m_pocket_lab_dll, "PocketLabShutdown_Export", proc))
                m_pfn_pocket_lab_shutdown = reinterpret_cast<PFN_PocketLabShutdown>(proc);
        }

        // Resolve function pointers for Phase3 kernel
        if (m_phase3_dll) {
            void* proc = nullptr;
            if (ResolveProcAddress(m_phase3_dll, "Phase3Initialize_Export", proc))
                m_pfn_phase3_init = reinterpret_cast<PFN_Phase3Initialize>(proc);
            if (ResolveProcAddress(m_phase3_dll, "GenerateTokens_Export", proc))
                m_pfn_generate_tokens = reinterpret_cast<PFN_GenerateTokens>(proc);
            if (ResolveProcAddress(m_phase3_dll, "Phase3Shutdown_Export", proc))
                m_pfn_phase3_shutdown = reinterpret_cast<PFN_Phase3Shutdown>(proc);
        }

        // Initialize Phase3 kernel context if available
        if (m_pfn_phase3_init) {
            m_phase3_context = m_pfn_phase3_init(nullptr, nullptr);
        }

        // Initialize PocketLab if available
        if (m_pfn_pocket_lab_init) {
            m_pfn_pocket_lab_init();
        }

        return true;
    }

    void KernelDispatcher::Shutdown() {
        UnloadKernel();

        if (m_pocket_lab_dll) {
            FreeLibrary(m_pocket_lab_dll);
            m_pocket_lab_dll = nullptr;
        }

        if (m_phase3_dll) {
            FreeLibrary(m_phase3_dll);
            m_phase3_dll = nullptr;
        }
    }

    bool KernelDispatcher::DetectSystemInfo(SystemInfo& out_info) {
        memset(&out_info, 0, sizeof(out_info));

        // Query physical RAM
        MEMORYSTATUSEX mem_status;
        mem_status.dwLength = sizeof(mem_status);
        if (!GlobalMemoryStatusEx(&mem_status)) {
            return false;
        }

        out_info.physical_ram_bytes = mem_status.ullTotalPhys;
        out_info.available_ram_bytes = mem_status.ullAvailPhys;

        // Detect system tier from RAM
        out_info.tier = DetectSystemTierFromRAM(out_info.physical_ram_bytes);

        // Query CPU info
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        out_info.cpu_cores = sys_info.dwNumberOfProcessors;

        // Detect CPU features (simplified)
        int cpu_info[4];
        __cpuid(cpu_info, 1);
        out_info.supports_avx2 = (cpu_info[2] & (1 << 26)) != 0;  // ECX bit 26
        out_info.supports_f16c = (cpu_info[2] & (1 << 29)) != 0;  // ECX bit 29

        __cpuid(cpu_info, 7);
        out_info.supports_avx512f = (cpu_info[1] & (1 << 16)) != 0;  // EBX bit 16

        // GPU detection (simplified - can be extended)
        out_info.primary_gpu = GPUBackend::VULKAN;  // Default to Vulkan on Win32
        out_info.gpu_memory_mb = 4096;  // Conservative estimate

        return true;
    }

    bool KernelDispatcher::AnalyzeModel(const wchar_t* model_path, ModelMetadata& out_metadata) {
        if (!model_path) {
            return false;
        }

        // Open model file
        HANDLE h_file = CreateFileW(
            model_path,
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (h_file == INVALID_HANDLE_VALUE) {
            return false;
        }

        // Get file size
        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(h_file, &file_size)) {
            CloseHandle(h_file);
            return false;
        }

        out_metadata.file_size_bytes = file_size.QuadPart;

        // Read GGUF header (first 512 bytes should be enough)
        constexpr size_t header_size = 512;
        uint8_t header_buffer[header_size];
        DWORD bytes_read = 0;

        if (!ReadFile(h_file, header_buffer, header_size, &bytes_read, nullptr)) {
            CloseHandle(h_file);
            return false;
        }

        CloseHandle(h_file);

        // Analyze header
        return AnalyzeModelFromBuffer(header_buffer, bytes_read, out_metadata);
    }

    bool KernelDispatcher::AnalyzeModelFromBuffer(const uint8_t* gguf_header, size_t header_size, ModelMetadata& out_metadata) {
        if (!gguf_header || header_size < 16) {
            return false;
        }

        memset(&out_metadata, 0, sizeof(out_metadata));

        // Check GGUF magic: "GGUF" in little-endian = 0x46554747
        uint32_t magic = *(uint32_t*)gguf_header;
        if (magic != 0x46554747) {
            return false;  // Not a GGUF file
        }

        // GGUF v3 header layout (after magic):
        //   uint32_t version        (offset 4)
        //   uint64_t tensor_count   (offset 8)
        //   uint64_t metadata_kv_count (offset 16)
        uint32_t version = *(uint32_t*)(gguf_header + 4);
        
        if (version >= 2 && header_size >= 24) {
            uint64_t tensor_count = *(uint64_t*)(gguf_header + 8);
            uint64_t kv_count = *(uint64_t*)(gguf_header + 16);

            // Use tensor count as a rough proxy for model complexity
            // Typical tensor counts: 3B~290, 7B~560, 13B~760, 27B~1100, 70B~1700
            if (tensor_count > 0 && tensor_count < 100000) {
                if (tensor_count < 350)
                    out_metadata.param_tier = ModelTier::SMALL_3B;
                else if (tensor_count < 650)
                    out_metadata.param_tier = ModelTier::MEDIUM_7B;
                else if (tensor_count < 900)
                    out_metadata.param_tier = ModelTier::LARGE_13B;
                else if (tensor_count < 1400)
                    out_metadata.param_tier = ModelTier::XLARGE_27B;
                else
                    out_metadata.param_tier = ModelTier::GIANT_70B;
            } else {
                // Fallback to file-size heuristic
                out_metadata.param_tier = DetectModelTierFromSize(out_metadata.file_size_bytes);
            }
        } else {
            // Old format or insufficient header data — use file-size heuristic
            out_metadata.param_tier = DetectModelTierFromSize(out_metadata.file_size_bytes);
        }

        // Detect quantization from GGUF header metadata
        out_metadata.quant_format = DetectQuantFromGGUFHeader(gguf_header);

        // Set reasonable defaults
        out_metadata.context_length = 4096;
        out_metadata.vocab_size = 128000;

        return true;
    }

    bool KernelDispatcher::SelectKernel(const SystemInfo& sys_info, const ModelMetadata& model_meta, KernelConfig& out_config) {
        memset(&out_config, 0, sizeof(out_config));

        out_config.system_tier = sys_info.tier;
        out_config.model_tier = model_meta.param_tier;
        out_config.quant = model_meta.quant_format;
        out_config.gpu = sys_info.primary_gpu;

        // ─────────────────────────────────────────────────────────────────────────
        // TurboSparse Configuration
        // ─────────────────────────────────────────────────────────────────────────
        out_config.sparse_config.enabled = true;
        out_config.sparse_config.skip_percentage = 20;  // Skip ~20% of neurons
        out_config.sparse_config.bitmap_size_bytes = 256;  // 2048 bits

        // ─────────────────────────────────────────────────────────────────────────
        // PowerInfer Configuration (based on system tier)
        // ─────────────────────────────────────────────────────────────────────────
        out_config.power_infer_config.enabled = true;

        switch (sys_info.tier) {
            case SystemTier::MOBILE:
                out_config.power_infer_config.gpu_ratio_percent = 100;
                out_config.power_infer_config.cpu_ratio_percent = 0;
                out_config.power_infer_config.hot_slab_count = 1024;  // All GPU
                break;

            case SystemTier::WORKSTATION:
                out_config.power_infer_config.gpu_ratio_percent = 70;
                out_config.power_infer_config.cpu_ratio_percent = 30;
                out_config.power_infer_config.hot_slab_count = 716;  // ~70% of 1024
                break;

            case SystemTier::ENTERPRISE:
                out_config.power_infer_config.gpu_ratio_percent = 33;
                out_config.power_infer_config.cpu_ratio_percent = 67;
                out_config.power_infer_config.hot_slab_count = 338;  // ~33% of 1024
                break;

            default:
                out_config.power_infer_config.gpu_ratio_percent = 50;
                out_config.power_infer_config.cpu_ratio_percent = 50;
                out_config.power_infer_config.hot_slab_count = 512;
        }

        // ─────────────────────────────────────────────────────────────────────────
        // KV Cache Configuration (based on model tier)
        // ─────────────────────────────────────────────────────────────────────────
        out_config.kv_config.slot_count = 1024;
        out_config.kv_config.slot_size_bytes = 4096;

        switch (model_meta.param_tier) {
            case ModelTier::SMALL_3B:
            case ModelTier::MEDIUM_7B:
            case ModelTier::MEDIUM_8B:
                out_config.kv_config.max_context_len = 4096;
                break;

            case ModelTier::LARGE_13B:
            case ModelTier::XLARGE_27B:
                out_config.kv_config.max_context_len = 8192;
                break;

            case ModelTier::GIANT_70B:
                out_config.kv_config.max_context_len = 16384;
                break;

            default:
                out_config.kv_config.max_context_len = 4096;
        }

        // ─────────────────────────────────────────────────────────────────────────
        // Prefetch Configuration
        // ─────────────────────────────────────────────────────────────────────────
        out_config.prefetch_config.enabled = true;

        switch (sys_info.tier) {
            case SystemTier::MOBILE:
                out_config.prefetch_config.buffer_size_bytes = 65536;   // 64 KB
                out_config.prefetch_config.lookahead_tokens = 32;
                break;

            case SystemTier::WORKSTATION:
                out_config.prefetch_config.buffer_size_bytes = 262144;  // 256 KB
                out_config.prefetch_config.lookahead_tokens = 64;
                break;

            case SystemTier::ENTERPRISE:
                out_config.prefetch_config.buffer_size_bytes = 524288;  // 512 KB
                out_config.prefetch_config.lookahead_tokens = 128;
                break;

            default:
                out_config.prefetch_config.buffer_size_bytes = 262144;
                out_config.prefetch_config.lookahead_tokens = 64;
        }

        // ─────────────────────────────────────────────────────────────────────────
        // Generation Configuration (tuned parameters)
        // ─────────────────────────────────────────────────────────────────────────
        out_config.generation_config.batch_size = 1;
        out_config.generation_config.temperature = 0.7f;
        out_config.generation_config.top_p = 0.9f;
        out_config.generation_config.top_k = 40;
        out_config.generation_config.max_tokens_per_call = 256;

        return true;
    }

    bool KernelDispatcher::LoadKernel(const KernelConfig& config) {
        // For now, store the configuration
        m_current_config = config;

        // In a real implementation, would:
        // 1. Allocate buffers based on config
        // 2. Initialize Phase3 kernel if available
        // 3. Set up sparse bitmap
        // 4. Configure PowerInfer routing

        m_is_loaded = true;
        return true;
    }

    bool KernelDispatcher::UnloadKernel() {
        if (m_pfn_phase3_shutdown && m_phase3_context) {
            m_pfn_phase3_shutdown(m_phase3_context);
            m_phase3_context = nullptr;
        }

        m_is_loaded = false;
        return true;
    }

    bool KernelDispatcher::InvokeDetectTier(SystemTier& out_tier) {
        if (!m_pocket_lab_dll || !m_pfn_pocket_lab_init) {
            return false;
        }

        // Call PocketLabInit which internally calls DetectTier
        int result = m_pfn_pocket_lab_init();
        if (result != 0) {
            return false;
        }

        out_tier = m_system_info.tier;
        return true;
    }

    bool KernelDispatcher::InvokePowerInferLoop(uint32_t max_tokens, uint64_t& out_tokens_processed) {
        if (!m_pocket_lab_dll || !m_pfn_pocket_lab_run_cycle) {
            return false;
        }

        int result = m_pfn_pocket_lab_run_cycle();
        if (result != 0) {
            return false;
        }

        // Query stats after run
        if (m_pfn_pocket_lab_get_stats) {
            uint64_t tokens, sparse_skips, gpu, cpu;
            m_pfn_pocket_lab_get_stats(&tokens, &sparse_skips, &gpu, &cpu);
            out_tokens_processed = tokens;
        }

        return true;
    }

    bool KernelDispatcher::InvokeGenerateTokens(const uint8_t* prompt, size_t prompt_len, uint8_t* output, size_t output_cap, size_t& out_len) {
        if (!m_phase3_dll || !m_pfn_generate_tokens || !m_phase3_context) {
            return false;
        }

        // Call Phase3 generate
        int result = m_pfn_generate_tokens(m_phase3_context, (const char*)prompt, nullptr);
        if (result != 1) {
            return false;
        }

        out_len = 0;  // Would be populated by kernel
        return true;
    }

    bool KernelDispatcher::IsKernelLoaded() const {
        return m_is_loaded;
    }

    SystemTier KernelDispatcher::GetCurrentSystemTier() const {
        return m_system_info.tier;
    }

    KernelConfig KernelDispatcher::GetCurrentConfig() const {
        return m_current_config;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Private Helper Methods
    // ─────────────────────────────────────────────────────────────────────────────

    bool KernelDispatcher::LoadDLL(const wchar_t* dll_name, HMODULE& out_handle) {
        out_handle = LoadLibraryW(dll_name);
        if (!out_handle) {
            // Try alternative paths
            wchar_t alt_paths[4][256] = {
                L"bin\\",
                L"..\\bin\\",
                L"..\\..\\bin\\",
                L"D:\\rawrxd\\bin\\"
            };

            wchar_t full_path[512];
            for (int i = 0; i < 4; ++i) {
                swprintf_s(full_path, 512, L"%s%s", alt_paths[i], dll_name);
                out_handle = LoadLibraryW(full_path);
                if (out_handle) {
                    return true;
                }
            }

            return false;
        }

        return true;
    }

    bool KernelDispatcher::ResolveProcAddress(HMODULE dll, const char* proc_name, void*& out_proc) {
        if (!dll) {
            return false;
        }

        out_proc = ::GetProcAddress(dll, proc_name);
        return out_proc != nullptr;
    }

    SystemTier KernelDispatcher::DetectSystemTierFromRAM(uint64_t physical_ram) {
        constexpr uint64_t RAM_8GB = 8ULL * 1024 * 1024 * 1024;
        constexpr uint64_t RAM_16GB = 16ULL * 1024 * 1024 * 1024;

        if (physical_ram <= RAM_8GB) {
            return SystemTier::MOBILE;
        } else if (physical_ram <= RAM_16GB) {
            return SystemTier::WORKSTATION;
        } else {
            return SystemTier::ENTERPRISE;
        }
    }

    ModelTier KernelDispatcher::DetectModelTierFromSize(uint64_t file_size) {
        // Heuristic: GGUF file size → parameter count
        // 3.2B Q4_K_M ≈ 2.0 GB
        // 7B Q4_K_M ≈ 4.7 GB
        // 8B Q4_K_M ≈ 4.9 GB
        // 13B Q4_K_M ≈ 7.4 GB
        // 27B Q4_K_M ≈ 17 GB
        // 70B Q4_K_M ≈ 40 GB

        if (file_size < 3ULL * 1024 * 1024 * 1024) {
            return ModelTier::SMALL_3B;
        } else if (file_size < 5ULL * 1024 * 1024 * 1024) {
            return ModelTier::MEDIUM_7B;
        } else if (file_size < 8ULL * 1024 * 1024 * 1024) {
            return ModelTier::LARGE_13B;
        } else if (file_size < 20ULL * 1024 * 1024 * 1024) {
            return ModelTier::XLARGE_27B;
        } else {
            return ModelTier::GIANT_70B;
        }
    }

    QuantFormat KernelDispatcher::DetectQuantFromGGUFHeader(const uint8_t* header) {
        if (!header) {
            return QuantFormat::UNKNOWN;
        }

        // Scan header bytes for known quant type strings that appear in GGUF metadata
        // The "general.file_type" key contains the ftype integer, but scanning for
        // string patterns in the raw header is a pragmatic approach for early detection.
        // GGUF metadata stores strings as length-prefixed, so substrings may appear.
        
        // Scan first 512 bytes for quant signature patterns
        // These are typically found in the tensor type fields or metadata values
        auto scan = [&](const char* pattern, size_t pattern_len) -> bool {
            for (size_t i = 0; i + pattern_len < 512; ++i) {
                if (memcmp(header + i, pattern, pattern_len) == 0)
                    return true;
            }
            return false;
        };

        // Check GGUF v3 file_type field at known offset
        // After header (magic=4, version=4, tensor_count=8, kv_count=8 = 24 bytes),
        // the first KV pairs begin. The file_type is typically one of the first keys.
        // GGUF file_type values: 0=F32, 1=F16, 2=Q4_0, 7=Q5_1, 
        //   10=Q2_K, 11=Q3_K_S, 12=Q3_K_M, 13=Q3_K_L, 14=Q4_K_S, 15=Q4_K_M,
        //   16=Q5_K_S, 17=Q5_K_M, 18=Q6_K
        
        // For now, scan for the "general.file_type" key and read the uint32 value after it
        if (scan("general.file_type", 17)) {
            // Found the key — the value is a uint32 somewhere after this key
            for (size_t i = 0; i + 17 + 12 < 512; ++i) {
                if (memcmp(header + i, "general.file_type", 17) == 0) {
                    // Skip key string + type tag (uint32=4) + value
                    // Layout: key_len(8) + key_str(17) + value_type(4) + value(4)
                    size_t val_offset = i + 17 + 4 + 4;  // approximate
                    if (val_offset + 4 < 512) {
                        uint32_t ftype = *(uint32_t*)(header + val_offset);
                        switch (ftype) {
                            case 0:  return QuantFormat::F32;
                            case 1:  return QuantFormat::F16;
                            case 10: return QuantFormat::Q2_K;
                            case 11: case 12: case 13: return QuantFormat::Q3_K;
                            case 14: return QuantFormat::Q4_K_S;
                            case 15: return QuantFormat::Q4_K_M;
                            case 16: case 17: return QuantFormat::Q5_K;
                            case 18: return QuantFormat::Q6_K;
                            default: return QuantFormat::Q4_K_M;  // Common default
                        }
                    }
                }
            }
        }

        // Fallback: default to Q4_K_M (most common quantization)
        return QuantFormat::Q4_K_M;
    }

}  // namespace RawrXD::Kernel
