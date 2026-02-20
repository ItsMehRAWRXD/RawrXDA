// ============================================================================
// vision_encoder.hpp — Vision Model Bridge for Multi-Modal Input
// ============================================================================
// Enables image/diagram → embedding pipeline for the MultiModalModelRouter.
// Supports:
//   - GGUF-format vision models (LLaVA, CLIP, ViT)
//   - Image preprocessing (resize, normalize, patch extraction)
//   - Screenshot → code context pipeline
//   - Diagram → structured data extraction
//   - Direct integration with MultiModalModelRouter
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>

namespace RawrXD {
namespace Vision {

// ============================================================================
// Result type
// ============================================================================
struct VisionResult {
    bool success;
    const char* detail;
    int errorCode;

    static VisionResult ok(const char* msg = "OK") {
        VisionResult r;
        r.success = true;
        r.detail = msg;
        r.errorCode = 0;
        return r;
    }

    static VisionResult error(const char* msg, int code = -1) {
        VisionResult r;
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Image Format
// ============================================================================
enum class ImageFormat : uint8_t {
    RGB8     = 0,  // 3 channels, 8 bits per channel
    RGBA8    = 1,  // 4 channels, 8 bits per channel
    GRAY8    = 2,  // 1 channel, 8 bits
    RGB_F32  = 3,  // 3 channels, 32-bit float (normalized)
    BGR8     = 4,  // OpenCV-style BGR
    PNG      = 10, // Compressed PNG (file/buffer)
    JPEG     = 11, // Compressed JPEG
    BMP      = 12, // Windows BMP
    WEBP     = 13  // WebP format
};

// ============================================================================
// Image Buffer — Raw pixel data
// ============================================================================
struct ImageBuffer {
    uint8_t* data;
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    uint32_t stride;        // Row stride in bytes
    ImageFormat format;
    uint64_t dataSize;      // Total buffer size in bytes

    ImageBuffer()
        : data(nullptr), width(0), height(0), channels(0),
          stride(0), format(ImageFormat::RGB8), dataSize(0) {}

    bool isValid() const {
        return data != nullptr && width > 0 && height > 0;
    }
};

// ============================================================================
// Vision Model Configuration
// ============================================================================
struct VisionModelConfig {
    std::string modelPath;          // Path to GGUF vision model
    std::string projectorPath;      // Path to mm_projector (LLaVA-style)
    uint32_t inputSize;             // Expected input dimension (224, 336, 384)
    uint32_t patchSize;             // Vision Transformer patch size (14, 16, 32)
    uint32_t embeddingDim;          // Output dimension (768, 1024, 4096)
    uint32_t numPatches;            // Computed: (inputSize/patchSize)^2
    uint32_t maxImages;             // Max images per request
    bool useGPU;                    // Use Vulkan compute
    bool useMASMPreprocess;         // Use MASM SIMD for image preprocessing

    enum class Architecture : uint8_t {
        CLIP_VIT_L14     = 0,  // CLIP ViT-L/14 (OpenAI)
        CLIP_VIT_B32     = 1,  // CLIP ViT-B/32
        SIGLIP_SO400M    = 2,  // SigLIP (Google)
        LLAVA_NEXT       = 3,  // LLaVA-NeXT mm_projector
        PHI3_VISION      = 4,  // Phi-3 Vision
        CUSTOM           = 5   // User-defined
    };
    Architecture arch;

    VisionModelConfig()
        : inputSize(336), patchSize(14), embeddingDim(1024),
          numPatches(0), maxImages(4), useGPU(false),
          useMASMPreprocess(true), arch(Architecture::CLIP_VIT_L14)
    {
        numPatches = (inputSize / patchSize) * (inputSize / patchSize);
    }
};

// ============================================================================
// Image Preprocessing Pipeline
// ============================================================================
struct ImagePreprocessor {
    // Resize image to target dimensions (bilinear interpolation)
    static VisionResult resize(const ImageBuffer& src,
                               ImageBuffer& dst,
                               uint32_t targetW,
                               uint32_t targetH);

    // Convert image format (e.g., BGR → RGB, RGBA → RGB)
    static VisionResult convertFormat(const ImageBuffer& src,
                                      ImageBuffer& dst,
                                      ImageFormat targetFormat);

    // Normalize pixel values to [-1, 1] or [0, 1] (float output)
    static VisionResult normalize(const ImageBuffer& src,
                                  std::vector<float>& output,
                                  const float mean[3],
                                  const float std[3]);

    // Extract patches for ViT (inputSize → numPatches × patchSize^2 × 3)
    static VisionResult extractPatches(const std::vector<float>& normalizedImage,
                                       uint32_t imageSize,
                                       uint32_t patchSize,
                                       std::vector<float>& patches);

    // Load image from file (PNG, JPEG, BMP)
    static VisionResult loadFromFile(const std::string& filepath,
                                     ImageBuffer& output);

    // Load image from memory buffer
    static VisionResult loadFromBuffer(const uint8_t* buffer,
                                       uint64_t bufferSize,
                                       ImageFormat format,
                                       ImageBuffer& output);

    // Decode Windows clipboard image (for paste support)
    static VisionResult loadFromClipboard(ImageBuffer& output);

    // Free allocated image buffer
    static void freeBuffer(ImageBuffer& buf);

    // Standard normalization constants
    static constexpr float CLIP_MEAN[3] = {0.48145466f, 0.4578275f, 0.40821073f};
    static constexpr float CLIP_STD[3]  = {0.26862954f, 0.26130258f, 0.27577711f};
    static constexpr float IMAGENET_MEAN[3] = {0.485f, 0.456f, 0.406f};
    static constexpr float IMAGENET_STD[3]  = {0.229f, 0.224f, 0.225f};
};

// ============================================================================
// Vision Encoder Output
// ============================================================================
struct VisionEmbedding {
    std::vector<float> embedding;     // Global image embedding
    std::vector<std::vector<float>> patchEmbeddings;  // Per-patch embeddings
    uint32_t numPatches;
    float confidence;                  // Model confidence (0.0-1.0)
    std::string description;           // Auto-generated text description
};

// ============================================================================
// Vision-Language Bridge Output
// ============================================================================
struct VisionTextPair {
    VisionEmbedding imageEmb;
    std::string textPrompt;            // Generated text prompt for LLM
    std::string structuredData;        // Extracted structured data (JSON)
    float relevanceScore;
};

// ============================================================================
// VisionEncoder — The main engine
// ============================================================================
class VisionEncoder {
public:
    static VisionEncoder& instance();

    // -----------------------------------------------------------------------
    // Initialization
    // -----------------------------------------------------------------------

    VisionResult loadModel(const VisionModelConfig& config);
    VisionResult unloadModel();
    bool isReady() const;
    const VisionModelConfig& getConfig() const;

    // -----------------------------------------------------------------------
    // Core Encoding
    // -----------------------------------------------------------------------

    // Encode an image to a vision embedding
    VisionResult encode(const ImageBuffer& image,
                        VisionEmbedding& output);

    // Encode from file path
    VisionResult encodeFile(const std::string& imagePath,
                            VisionEmbedding& output);

    // Encode from raw bytes (e.g., clipboard paste)
    VisionResult encodeBytes(const uint8_t* data,
                             uint64_t dataSize,
                             ImageFormat format,
                             VisionEmbedding& output);

    // Batch encode multiple images
    VisionResult encodeBatch(const std::vector<ImageBuffer>& images,
                             std::vector<VisionEmbedding>& outputs);

    // -----------------------------------------------------------------------
    // Vision-Language Integration
    // -----------------------------------------------------------------------

    // Generate text description of an image (for LLM context)
    VisionResult describeImage(const ImageBuffer& image,
                               std::string& description);

    // Extract code from screenshot (OCR-like)
    VisionResult extractCodeFromScreenshot(const ImageBuffer& image,
                                           std::string& code);

    // Extract structured data from diagram (flowchart, UML, etc.)
    VisionResult extractDiagramStructure(const ImageBuffer& image,
                                          std::string& structuredJson);

    // Create a vision+text prompt for LLM consumption
    // This is the key integration point with MultiModalModelRouter
    VisionResult createMultiModalPrompt(const ImageBuffer& image,
                                         const std::string& userQuery,
                                         VisionTextPair& output);

    // -----------------------------------------------------------------------
    // Similarity Search (image → code)
    // -----------------------------------------------------------------------

    // Find code similar to an image (e.g., "code that looks like this diagram")
    VisionResult searchSimilarCode(const ImageBuffer& image,
                                    uint32_t topK,
                                    std::vector<std::string>& results);

    // Compute similarity between two images
    float computeSimilarity(const VisionEmbedding& a,
                            const VisionEmbedding& b) const;

    // -----------------------------------------------------------------------
    // Statistics
    // -----------------------------------------------------------------------
    struct VisionStats {
        uint64_t totalEncoded;
        uint64_t totalDescriptions;
        uint64_t totalOCR;
        double avgEncodeTimeMs;
        uint64_t modelMemoryBytes;
    };

    VisionStats getStats() const;

    void shutdown();

private:
    VisionEncoder();
    ~VisionEncoder();
    VisionEncoder(const VisionEncoder&) = delete;
    VisionEncoder& operator=(const VisionEncoder&) = delete;

    // Internal encoding pipeline
    VisionResult preprocessAndEncode(const ImageBuffer& image,
                                      VisionEmbedding& output);

    // Model inference
    VisionResult inferVisionModel(const std::vector<float>& preprocessed,
                                   VisionEmbedding& output);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    mutable std::mutex encoderMutex_;
    VisionModelConfig config_;
    bool modelLoaded_;
    void* modelHandle_;         // Opaque model handle
    void* projectorHandle_;     // Opaque mm_projector handle

    // Statistics
    std::atomic<uint64_t> totalEncoded_;
    std::atomic<uint64_t> totalDescriptions_;
    std::atomic<uint64_t> totalOCR_;
    double encodeTimeAccumMs_;
};

} // namespace Vision
} // namespace RawrXD
