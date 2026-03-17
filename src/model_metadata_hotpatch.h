#pragma once
// =============================================================================
// model_metadata_hotpatch.h — C++ bridge to RawrXD_ModelMetadata_Hotpatch.asm
// =============================================================================
// Links against the MASM kernel to force-inject metadata into Ollama model
// entries so they display identically to GitHub/cloud models in the agent tab.
//
// Usage:
//   ModelMetadataHotpatch::init();
//   ModelMetadataBuffer buf = ModelMetadataHotpatch::createBuffer("llama3");
//   ModelMetadataHotpatch::injectDefaults(&buf);
//   // buf now has family, capabilities, agent flag, etc.
// =============================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace RawrXD {

// Must match MASM constants exactly
enum class MetadataFieldID : uint32_t {
    FAMILY          = 0,
    PARAMETER_SIZE  = 1,
    QUANTIZATION    = 2,
    CAPABILITIES    = 3,
    DESCRIPTION     = 4,
    AGENT_CAPABLE   = 5,
    CONTEXT_LENGTH  = 6,
    MAX_TOKENS      = 7
};

// Must match MASM buffer layout exactly (BUF_TOTAL_SIZE = 128)
#pragma pack(push, 8)
struct ModelMetadataBuffer {
    uint64_t magic;             // +0:   'RAWRMETA' = 0x4154454D52574152
    uint32_t version;           // +8:   = 1
    uint32_t flags;             // +12:  bitfield of FLAG_HAS_xxx
    const char* name_ptr;       // +16:  model name
    uint64_t name_len;          // +24
    const char* family_ptr;     // +32:  family (e.g. "transformer")
    uint64_t family_len;        // +40
    const char* param_size_ptr; // +48:  parameter size (e.g. "7B")
    uint64_t param_size_len;    // +56
    const char* quant_ptr;      // +64:  quantization (e.g. "Q4_K_M")
    uint64_t quant_len;         // +72
    const char* caps_ptr;       // +80:  capabilities csv
    uint64_t caps_len;          // +88
    const char* desc_ptr;       // +96:  description
    uint64_t desc_len;          // +104
    uint8_t  agent_flag;        // +112: 1 = agent-capable
    uint8_t  _pad[3];           // +113
    uint32_t context_length;    // +116
    uint32_t max_tokens;        // +120
    uint32_t _reserved;         // +124
};
#pragma pack(pop)

static_assert(sizeof(ModelMetadataBuffer) == 128, "ModelMetadataBuffer must be 128 bytes");

constexpr uint64_t RAWRMETA_MAGIC = 0x4154454D52574152ULL;

// Flag constants matching MASM
constexpr uint32_t META_FLAG_HAS_FAMILY    = 0x00000001;
constexpr uint32_t META_FLAG_HAS_PARAMS    = 0x00000002;
constexpr uint32_t META_FLAG_HAS_QUANT     = 0x00000004;
constexpr uint32_t META_FLAG_HAS_CAPS      = 0x00000008;
constexpr uint32_t META_FLAG_HAS_DESC      = 0x00000010;
constexpr uint32_t META_FLAG_AGENT_CAPABLE = 0x00000020;
constexpr uint32_t META_FLAG_COMPLETE      = 0x0000003F;

struct MetadataStats {
    uint64_t models_patched;
    uint64_t fields_injected;
    uint64_t patch_errors;
    uint64_t agent_forced;
    uint64_t validations_passed;
    uint64_t validations_failed;
};

// =============================================================================
// ASM kernel imports (extern "C" linkage from RawrXD_ModelMetadata_Hotpatch.asm)
// =============================================================================
extern "C" {
    int  asm_metadata_hotpatch_init();
    int  asm_metadata_hotpatch_shutdown();
    int  asm_metadata_inject_defaults(ModelMetadataBuffer* buf);
    int  asm_metadata_scan_and_patch(ModelMetadataBuffer* array, uint64_t count);
    int  asm_metadata_force_agent_capable(ModelMetadataBuffer* buf);
    int  asm_metadata_get_stats(MetadataStats* out);
    int  asm_metadata_set_field(ModelMetadataBuffer* buf, uint32_t field_id,
                                 const void* value, uint64_t value_len);
    int  asm_metadata_validate_buffer(ModelMetadataBuffer* buf);
}

// =============================================================================
// C++ wrapper class — high-level API
// =============================================================================
class ModelMetadataHotpatch {
public:
    // Initialize the MASM hotpatch engine (call once at startup)
    static bool init() {
        return asm_metadata_hotpatch_init() == 0;
    }

    // Shutdown (call at process exit)
    static void shutdown() {
        asm_metadata_hotpatch_shutdown();
    }

    // Create a stamped buffer for a model name
    static ModelMetadataBuffer createBuffer(const std::string& modelName) {
        ModelMetadataBuffer buf{};
        buf.magic = RAWRMETA_MAGIC;
        buf.version = 1;
        buf.flags = 0;
        buf.name_ptr = modelName.c_str();
        buf.name_len = modelName.size();
        buf.family_ptr = nullptr;
        buf.family_len = 0;
        buf.param_size_ptr = nullptr;
        buf.param_size_len = 0;
        buf.quant_ptr = nullptr;
        buf.quant_len = 0;
        buf.caps_ptr = nullptr;
        buf.caps_len = 0;
        buf.desc_ptr = nullptr;
        buf.desc_len = 0;
        buf.agent_flag = 0;
        buf.context_length = 0;
        buf.max_tokens = 0;
        return buf;
    }

    // Inject default metadata into empty fields of a buffer
    static int injectDefaults(ModelMetadataBuffer* buf) {
        return asm_metadata_inject_defaults(buf);
    }

    // Scan an array of buffers and patch all empty fields
    static int scanAndPatch(std::vector<ModelMetadataBuffer>& buffers) {
        if (buffers.empty()) return 0;
        return asm_metadata_scan_and_patch(buffers.data(), buffers.size());
    }

    // Force agent capability on a model
    static bool forceAgentCapable(ModelMetadataBuffer* buf) {
        return asm_metadata_force_agent_capable(buf) == 1;
    }

    // Set a specific field
    static bool setField(ModelMetadataBuffer* buf, MetadataFieldID field,
                         const std::string& value) {
        return asm_metadata_set_field(buf, static_cast<uint32_t>(field),
                                       value.c_str(), value.size()) == 0;
    }

    // Set numeric fields (context_length, max_tokens, agent_capable flag)
    static bool setField(ModelMetadataBuffer* buf, MetadataFieldID field, uint32_t value) {
        return asm_metadata_set_field(buf, static_cast<uint32_t>(field),
                                       reinterpret_cast<const void*>(static_cast<uintptr_t>(value)),
                                       0) == 0;
    }

    // Validate a buffer
    static bool validate(ModelMetadataBuffer* buf) {
        return asm_metadata_validate_buffer(buf) == 1;
    }

    // Get statistics
    static MetadataStats getStats() {
        MetadataStats stats{};
        asm_metadata_get_stats(&stats);
        return stats;
    }

    // Check if metadata is complete (all fields populated)
    static bool isComplete(const ModelMetadataBuffer* buf) {
        return (buf->flags & META_FLAG_COMPLETE) == META_FLAG_COMPLETE;
    }

    // Populate a buffer from OllamaModel data, filling gaps with ASM defaults
    static ModelMetadataBuffer fromOllamaModel(
        const std::string& name,
        const std::string& family,
        const std::string& parameter_size,
        const std::string& quantization_level)
    {
        ModelMetadataBuffer buf = createBuffer(name);

        // Set whatever Ollama gives us
        if (!family.empty()) {
            buf.family_ptr = family.c_str();
            buf.family_len = family.size();
            buf.flags |= META_FLAG_HAS_FAMILY;
        }
        if (!parameter_size.empty()) {
            buf.param_size_ptr = parameter_size.c_str();
            buf.param_size_len = parameter_size.size();
            buf.flags |= META_FLAG_HAS_PARAMS;
        }
        if (!quantization_level.empty()) {
            buf.quant_ptr = quantization_level.c_str();
            buf.quant_len = quantization_level.size();
            buf.flags |= META_FLAG_HAS_QUANT;
        }

        // Let the MASM kernel fill whatever is still empty
        injectDefaults(&buf);

        return buf;
    }
};

} // namespace RawrXD
