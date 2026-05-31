#ifndef RAWRXD_INTERFACES_H
#define RAWRXD_INTERFACES_H

#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <functional>
#include <type_traits>

namespace RawrXD {

    // =========================================================================
    // Sovereign Node — SFINAE-gated agent dispatch
    // Template constraint: SovereignNode::dispatch<T> should only bind for
    // types that provide a ::CycleResult nested type.  BUG: missing typename
    // on dependent type + circular default argument referencing incomplete self.
    // =========================================================================
    struct SovereignNode {
        using StatusFlags = uint64_t;

        // Trait: does T have a nested CycleResult?
        template<typename T, typename = void>
        struct has_cycle_result : std::false_type {};

        template<typename T>
        struct has_cycle_result<T, std::void_t<typename T::CycleResult>>
            : std::true_type {};

        // SOVEREIGN FIX: C2903 / C2760 template deduction repair
        // Corrected: Explicitly prefixing nested name specifier with 'typename'
        // and resolving the circular dependency in the SFINAE-gated dispatcher.
        template<typename T,
                 typename std::enable_if_t<has_cycle_result<T>::value, int> = 0>
        static StatusFlags dispatch(const T& agent) {
            return static_cast<StatusFlags>(sizeof(typename T::CycleResult));
        }

        // Fix: Constant moved down to after struct definition to avoid C2027
        static constexpr size_t kNodeSize();
        };

    inline constexpr size_t SovereignNode::kNodeSize() { return sizeof(SovereignNode); }
    // =========================================================================

    // --- Inference Engine Interface ---
    class InferenceEngine {
    public:
        virtual ~InferenceEngine() = default;

        // ---- Model Lifecycle ----
        virtual bool LoadModel(const std::string& model_path) = 0;
        virtual bool IsModelLoaded() const = 0;

        // ---- Tokenization ----
        virtual std::vector<int32_t> Tokenize(const std::string& text) = 0;
        virtual std::string Detokenize(const std::vector<int32_t>& tokens) = 0;

        // ---- Inference ----
        virtual std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens = 100) = 0;
        virtual std::vector<float> Eval(const std::vector<int32_t>& input_tokens) = 0;

        // ---- Streaming ----
        virtual void GenerateStreaming(
            const std::vector<int32_t>& input_tokens,
            int max_tokens,
            std::function<void(const std::string&)> token_callback,
            std::function<void()> complete_callback,
            std::function<void(int32_t)> token_id_callback = nullptr) = 0;

        // ---- Model Info ----
        virtual int GetVocabSize() const = 0;
        virtual int GetEmbeddingDim() const = 0;
        virtual int GetNumLayers() const = 0;
        virtual int GetNumHeads() const = 0;

        // ---- AI Mode Flags ----
        virtual void SetMaxMode(bool enabled) = 0;
        virtual void SetDeepThinking(bool enabled) = 0;
        virtual void SetDeepResearch(bool enabled) = 0;
        virtual bool IsMaxMode() const = 0;
        virtual bool IsDeepThinking() const = 0;
        virtual bool IsDeepResearch() const = 0;

        // ---- Memory Management ----
        virtual size_t GetMemoryUsage() const = 0;
        virtual void ClearCache() = 0;

        // ---- Engine Identification ----
        virtual const char* GetEngineName() const = 0;
    };

    // --- Renderer Interface ---
#ifndef RAWRXD_RENDERER_INTERFACE_DEFINED
#define RAWRXD_RENDERER_INTERFACE_DEFINED
    class IRenderer {
    public:
        virtual ~IRenderer() = default;
        virtual bool Initialize(HWND hWnd) = 0;
        virtual void Render() = 0;
        virtual void Resize(UINT width, UINT height) = 0;
        virtual void SetTransparency(float alpha) = 0;
        virtual void DrawText(const std::wstring& text, float x, float y, float size, uint32_t color) = 0;
        virtual void DrawRect(float x, float y, float w, float h, uint32_t color) = 0;
        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
    };
#endif

    // --- GGUF Loader Interfaces ---
    enum class GGMLType : uint32_t {
        F32  = 0,
        F16  = 1,
        Q4_0 = 2,
        Q4_1 = 3,
        Q5_0 = 6,
        Q5_1 = 7,
        Q8_0 = 8,
        Q8_1 = 9,
        // K-Quants
        Q2_K = 10,
        Q3_K = 11,
        Q4_K = 12,
        Q5_K = 13,
        Q6_K = 14,
        Q8_K = 15,
        I8 = 16,
        I16 = 17,
        I32 = 18,
        I64 = 19,
        F64 = 20,
        F16_HALF = 21,
        IQ2_XXS = 22,
        IQ2_XS = 23,
        IQ3_XXS = 24,
        IQ1_S = 25,
        IQ4_NL = 26,
        IQ3_S = 27,
        IQ2_S = 28,
        IQ4_XS = 29,
        IQ1_M = 30,
        COUNT 
    };

    struct GGUFHeader {
        uint32_t magic;
        uint32_t version;
        uint64_t tensor_count;
        uint64_t metadata_kv_count;
        uint64_t metadata_offset;
    };

    struct TensorInfo {
        std::string name;
        std::vector<uint64_t> shape;
        GGMLType type;
        uint64_t offset;
        uint64_t size;
        uint64_t size_bytes; // Alias for size to match legacy code
        bool loaded = false; // Track if tensor is loaded into memory
        std::vector<uint8_t> hostData; // CPU fallback storage for tensor data
        const void* data = nullptr; // Pointer to mapped or GPU memory (non-owning)
    };

    struct GGUFMetadata {
        std::string name;
        std::string architecture;
        std::string architecture_type;
        uint64_t parameterCount = 0;
        uint32_t vocabSize = 0;
        uint32_t vocab_size = 0;
        uint32_t contextLength = 0;
        uint32_t context_length = 0;
        std::map<std::string, std::string> properties;
        std::map<std::string, std::string> kv_pairs; // Alias for properties
        
        // Extended metadata
        uint32_t layer_count = 0;
        uint32_t embedding_dim = 0;
        uint32_t head_count = 0;
        uint32_t head_count_kv = 0;
        uint32_t feed_forward_length = 0;

        // Vocab extensions
        std::vector<std::string> tokens;
        std::vector<float> token_scores;
        std::vector<int32_t> token_types;
        std::vector<uint32_t> token_types_u32; // Added for GGUFLoader compatibility
    };

    class IGGUFLoader {
    public:
        virtual ~IGGUFLoader() = default;
        virtual bool Open(const std::string& path) = 0;
        virtual bool Close() = 0;
        virtual bool ParseHeader() = 0;
        virtual GGUFHeader GetHeader() const = 0;
        virtual bool ParseMetadata() = 0;
        virtual GGUFMetadata GetMetadata() const = 0;
        virtual std::vector<TensorInfo> GetTensorInfo() const = 0;
        virtual const std::vector<std::string>& GetVocabulary() const {
            static const std::vector<std::string> kEmpty;
            return kEmpty;
        }
        virtual bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) = 0;
        virtual size_t GetTensorByteSize(const TensorInfo& tensor) const = 0;
        virtual std::string GetTypeString(GGMLType type) const = 0;
        virtual bool BuildTensorIndex() = 0;
        virtual bool LoadZone(const std::string& zone_name, uint64_t max_memory_mb = 512) = 0;
        virtual bool UnloadZone(const std::string& zone_name) = 0;
        virtual bool LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) = 0;
        virtual uint64_t GetFileSize() const = 0;
        virtual uint64_t GetCurrentMemoryUsage() const = 0;
        virtual std::vector<std::string> GetLoadedZones() const = 0;
        virtual std::vector<std::string> GetAllZones() const = 0;
        virtual std::vector<TensorInfo> GetAllTensorInfo() const = 0;
    };

} // namespace RawrXD

#endif // RAWRXD_INTERFACES_H
