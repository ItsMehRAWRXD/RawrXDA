// =============================================================================
// RawrXD Encoder Engine C++ Integration Header
// Provides C++ interface to the MASM encoder engine
// =============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>

namespace rawrxd {
namespace encoder {

// =============================================================================
// Status Constants (match MASM definitions)
// =============================================================================
enum class Status : uint32_t {
    OK = 0,
    InvalidInput = 1,
    BufferTooSmall = 2,
    Unsupported = 3,
    CallbackError = 4
};

// =============================================================================
// Codec Types (match MASM definitions)
// =============================================================================
enum class CodecType : uint32_t {
    Raw = 0,            // Passthrough (identity)
    Base64Standard = 1, // Standard Base64 (+/)
    Base64Url = 2,      // URL-Safe Base64 (-_)
    Base64Avx = 3,      // AVX-512 accelerated Base64
    Hex = 4,            // Hexadecimal encoding (lowercase)
    HexUpper = 5,       // Uppercase Hex
    Url = 6,            // Percent-encoding
    Xor = 7,            // XOR cipher (stream)
    Rot13 = 8,          // ROT13 transformation
    Rle = 9,            // Run-Length Encoding
    Lz77Stub = 10,      // LZ77 placeholder
    Custom = 255        // User-defined callback
};

// =============================================================================
// SIMD Capability Levels
// =============================================================================
enum class SimdLevel : uint8_t {
    SSE2 = 0,
    AVX2 = 1,
    AVX512 = 2
};

// =============================================================================
// Encoder Context Structure (must match MASM layout exactly)
// =============================================================================
#pragma pack(push, 8)
struct EncoderContext {
    // Input/Output Management
    const void* input_buffer;       // Source data pointer
    uint64_t    input_length;       // Source length in bytes
    void*       output_buffer;      // Destination buffer
    uint64_t    output_capacity;    // Max output size
    uint64_t    output_length;      // Actual bytes written
    
    // Codec Configuration
    uint32_t    codec_type;         // CodecType enum
    uint32_t    codec_flags;        // Behavior flags
    uint32_t    xor_key;            // XOR key (for Xor codec)
    uint8_t     url_safe;           // URL-safe flag for Base64
    
    // Performance & State
    uint8_t     simd_available;     // SimdLevel detection result
    uint16_t    reserved_align1;    // Alignment padding
    uint32_t    thread_id;          // Worker thread identifier
    uint64_t    benchmark_cycles;   // Cycle counter
    
    // Callbacks (for Custom codec)
    void*       encode_callback;    // User encode function
    void*       decode_callback;    // User decode function
    
    // Error Handling
    uint8_t     strict_mode;        // 0 = permissive, 1 = strict
    uint8_t     replacement_char;   // Used for invalid sequences
    uint8_t     padding_alignment;  // Output alignment
    uint8_t     reserved_align2[5]; // Pad to 64 bytes
};
#pragma pack(pop)

static_assert(sizeof(EncoderContext) == 96, "EncoderContext size mismatch with MASM definition");

// =============================================================================
// External MASM Function Declarations
// =============================================================================
extern "C" {
    uint32_t DetectSimdSupport();
    uint32_t EncoderInitializeContext(EncoderContext* ctx, uint32_t codec_type, uint32_t flags);
    uint32_t EncoderProcess(EncoderContext* ctx, uint32_t direction);
    uint64_t EncoderGetRequiredCapacity(EncoderContext* ctx, uint64_t input_length);
    void     EncoderResetStream(EncoderContext* ctx);
    void     EncoderSetXorKey(EncoderContext* ctx, uint32_t key);
    uint32_t EncoderGetVersion();
    
    // Individual codec functions (for direct use)
    uint64_t Base64EncodeScalar(const void* src, uint64_t len, void* dst, uint32_t url_safe);
    uint64_t Base64DecodeScalar(const void* src, uint64_t len, void* dst);
    uint64_t HexEncode(const void* src, uint64_t len, void* dst, uint32_t uppercase);
    uint64_t HexDecode(const void* src, uint64_t len, void* dst);
    uint64_t UrlEncode(const void* src, uint64_t len, void* dst, uint64_t capacity);
    uint64_t UrlDecode(const void* src, uint64_t len, void* dst);
    uint64_t XorCrypt(EncoderContext* ctx, const void* src, uint64_t len, void* dst);
    uint64_t Rot13Transform(const void* src, uint64_t len, void* dst);
    uint64_t RleEncode(const void* src, uint64_t len, void* dst, uint64_t capacity);
}

// =============================================================================
// High-Level C++ Encoder Class
// =============================================================================
class Encoder {
public:
    explicit Encoder(CodecType type = CodecType::Base64Standard, uint32_t flags = 0) {
        auto status = static_cast<Status>(EncoderInitializeContext(&ctx_, static_cast<uint32_t>(type), flags));
        if (status != Status::OK) {
            throw std::runtime_error("Failed to initialize encoder context");
        }
    }
    
    // Get detected SIMD level
    SimdLevel getSimdLevel() const {
        return static_cast<SimdLevel>(ctx_.simd_available);
    }
    
    // Set XOR key for XOR codec
    void setXorKey(uint32_t key) {
        ctx_.xor_key = key;
    }
    
    // Reset stream state (for XOR cipher continuity)
    void resetStream() {
        EncoderResetStream(&ctx_);
    }
    
    // Calculate required output buffer size
    size_t getRequiredCapacity(size_t inputLength) const {
        return static_cast<size_t>(EncoderGetRequiredCapacity(const_cast<EncoderContext*>(&ctx_), inputLength));
    }
    
    // Encode data
    std::vector<uint8_t> encode(const void* data, size_t length) {
        if (!data || length == 0) {
            return {};
        }
        
        size_t capacity = getRequiredCapacity(length);
        std::vector<uint8_t> output(capacity);
        
        ctx_.input_buffer = data;
        ctx_.input_length = length;
        ctx_.output_buffer = output.data();
        ctx_.output_capacity = capacity;
        ctx_.output_length = 0;
        
        auto status = static_cast<Status>(EncoderProcess(&ctx_, 0)); // 0 = encode
        if (status != Status::OK) {
            throw std::runtime_error("Encoding failed with status: " + std::to_string(static_cast<uint32_t>(status)));
        }
        
        output.resize(ctx_.output_length);
        return output;
    }
    
    // Encode string
    std::string encodeString(const std::string& input) {
        auto result = encode(input.data(), input.size());
        return std::string(result.begin(), result.end());
    }
    
    // Encode from vector
    std::vector<uint8_t> encode(const std::vector<uint8_t>& data) {
        return encode(data.data(), data.size());
    }
    
    // Decode data
    std::vector<uint8_t> decode(const void* data, size_t length) {
        if (!data || length == 0) {
            return {};
        }
        
        // For decoding, output is typically smaller than input
        size_t capacity = length; // Conservative estimate
        std::vector<uint8_t> output(capacity);
        
        ctx_.input_buffer = data;
        ctx_.input_length = length;
        ctx_.output_buffer = output.data();
        ctx_.output_capacity = capacity;
        ctx_.output_length = 0;
        
        auto status = static_cast<Status>(EncoderProcess(&ctx_, 1)); // 1 = decode
        if (status != Status::OK) {
            throw std::runtime_error("Decoding failed with status: " + std::to_string(static_cast<uint32_t>(status)));
        }
        
        output.resize(ctx_.output_length);
        return output;
    }
    
    // Decode string
    std::string decodeString(const std::string& input) {
        auto result = decode(input.data(), input.size());
        return std::string(result.begin(), result.end());
    }
    
    // Decode from vector
    std::vector<uint8_t> decode(const std::vector<uint8_t>& data) {
        return decode(data.data(), data.size());
    }
    
    // Get raw context for advanced usage
    EncoderContext& getContext() { return ctx_; }
    const EncoderContext& getContext() const { return ctx_; }
    
private:
    EncoderContext ctx_{};
};

// =============================================================================
// Convenience Functions (Standalone, No Context Required)
// =============================================================================
namespace utils {

// Base64 encoding
inline std::string base64Encode(const std::string& input, bool urlSafe = false) {
    if (input.empty()) return {};
    
    size_t capacity = ((input.size() + 2) / 3) * 4 + 4;
    std::string output(capacity, '\0');
    
    uint64_t written = Base64EncodeScalar(input.data(), input.size(), 
                                          output.data(), urlSafe ? 1 : 0);
    output.resize(written);
    return output;
}

inline std::string base64Encode(const std::vector<uint8_t>& input, bool urlSafe = false) {
    if (input.empty()) return {};
    
    size_t capacity = ((input.size() + 2) / 3) * 4 + 4;
    std::string output(capacity, '\0');
    
    uint64_t written = Base64EncodeScalar(input.data(), input.size(),
                                          output.data(), urlSafe ? 1 : 0);
    output.resize(written);
    return output;
}

// Base64 decoding
inline std::vector<uint8_t> base64Decode(const std::string& input) {
    if (input.empty()) return {};
    
    std::vector<uint8_t> output(input.size());
    uint64_t written = Base64DecodeScalar(input.data(), input.size(), output.data());
    output.resize(written);
    return output;
}

// Hex encoding
inline std::string hexEncode(const std::vector<uint8_t>& input, bool uppercase = false) {
    if (input.empty()) return {};
    
    std::string output(input.size() * 2, '\0');
    HexEncode(input.data(), input.size(), output.data(), uppercase ? 1 : 0);
    return output;
}

inline std::string hexEncode(const std::string& input, bool uppercase = false) {
    if (input.empty()) return {};
    
    std::string output(input.size() * 2, '\0');
    HexEncode(input.data(), input.size(), output.data(), uppercase ? 1 : 0);
    return output;
}

// Hex decoding
inline std::vector<uint8_t> hexDecode(const std::string& input) {
    if (input.empty()) return {};
    
    std::vector<uint8_t> output(input.size() / 2 + 1);
    uint64_t written = HexDecode(input.data(), input.size(), output.data());
    output.resize(written);
    return output;
}

// URL encoding
inline std::string urlEncode(const std::string& input) {
    if (input.empty()) return {};
    
    size_t capacity = input.size() * 3; // Worst case
    std::string output(capacity, '\0');
    
    uint64_t written = UrlEncode(input.data(), input.size(), output.data(), capacity);
    output.resize(written);
    return output;
}

// URL decoding
inline std::string urlDecode(const std::string& input) {
    if (input.empty()) return {};
    
    std::string output(input.size(), '\0');
    uint64_t written = UrlDecode(input.data(), input.size(), output.data());
    output.resize(written);
    return output;
}

// ROT13
inline std::string rot13(const std::string& input) {
    if (input.empty()) return {};
    
    std::string output(input.size(), '\0');
    Rot13Transform(input.data(), input.size(), output.data());
    return output;
}

// XOR cipher (requires context for state)
inline std::vector<uint8_t> xorCipher(const std::vector<uint8_t>& input, uint32_t key) {
    if (input.empty()) return {};
    
    EncoderContext ctx{};
    ctx.xor_key = key;
    
    std::vector<uint8_t> output(input.size());
    XorCrypt(&ctx, input.data(), input.size(), output.data());
    return output;
}

} // namespace utils

// =============================================================================
// Version Information
// =============================================================================
inline uint32_t getVersion() {
    return EncoderGetVersion();
}

inline std::string getVersionString() {
    uint32_t ver = getVersion();
    return std::to_string((ver >> 16) & 0xFF) + "." + std::to_string(ver & 0xFFFF);
}

} // namespace encoder
} // namespace rawrxd
