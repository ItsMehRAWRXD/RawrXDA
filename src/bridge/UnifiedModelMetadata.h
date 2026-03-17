#pragma once
#include <string>
#include <cstdint>

namespace RawrXD::Bridge {
    struct UnifiedModelMetadata {
        std::string source;           // "gguf", "ollama", "hf"
        std::string family;           // hotpatched from Ollama or GGUF metadata
        std::string quantization;     // "Q4_K_M", "fp16", etc.
        uint64_t    parameter_count;
        uint32_t    context_length;
        float       temperature_default;
        bool        supports_tools;
        bool        supports_vision;
        
        // Cross-loader consistency hash
        uint64_t    capability_hash() const;
    };
    
    // Singleton registry prevents drift between EnhancedModelLoader and RawrXDInference
    class MetadataRegistry {
        static UnifiedModelMetadata s_active;
    public:
        static void commit(const UnifiedModelMetadata& meta) { s_active = meta; }
        static const UnifiedModelMetadata& active() { return s_active; }
        static void invalidate() { s_active = {}; }
    };
}
