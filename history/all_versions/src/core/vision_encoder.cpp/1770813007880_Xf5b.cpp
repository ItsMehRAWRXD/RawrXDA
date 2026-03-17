// ============================================================================
// vision_encoder.cpp — Vision Model Bridge Implementation
// ============================================================================
// NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "vision_encoder.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <wingdi.h>
#endif

namespace RawrXD {
namespace Vision {

// ============================================================================
// ImagePreprocessor — Static implementations
// ============================================================================

VisionResult ImagePreprocessor::resize(const ImageBuffer& src,
                                        ImageBuffer& dst,
                                        uint32_t targetW,
                                        uint32_t targetH) {
    if (!src.isValid())
        return VisionResult::error("Invalid source image", 1);

    uint32_t channels = src.channels;
    dst.width = targetW;
    dst.height = targetH;
    dst.channels = channels;
    dst.format = src.format;
    dst.stride = targetW * channels;
    dst.dataSize = static_cast<uint64_t>(targetW) * targetH * channels;
    dst.data = new uint8_t[dst.dataSize];

    // Bilinear interpolation
    float scaleX = static_cast<float>(src.width) / targetW;
    float scaleY = static_cast<float>(src.height) / targetH;

    for (uint32_t y = 0; y < targetH; ++y) {
        float srcY = y * scaleY;
        uint32_t y0 = static_cast<uint32_t>(srcY);
        uint32_t y1 = std::min(y0 + 1, src.height - 1);
        float fy = srcY - y0;

        for (uint32_t x = 0; x < targetW; ++x) {
            float srcX = x * scaleX;
            uint32_t x0 = static_cast<uint32_t>(srcX);
            uint32_t x1 = std::min(x0 + 1, src.width - 1);
            float fx = srcX - x0;

            for (uint32_t c = 0; c < channels; ++c) {
                float v00 = src.data[y0 * src.stride + x0 * channels + c];
                float v10 = src.data[y0 * src.stride + x1 * channels + c];
                float v01 = src.data[y1 * src.stride + x0 * channels + c];
                float v11 = src.data[y1 * src.stride + x1 * channels + c];

                float v = v00 * (1 - fx) * (1 - fy) +
                          v10 * fx * (1 - fy) +
                          v01 * (1 - fx) * fy +
                          v11 * fx * fy;

                dst.data[y * dst.stride + x * channels + c] =
                    static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
            }
        }
    }

    return VisionResult::ok("Image resized");
}

VisionResult ImagePreprocessor::convertFormat(const ImageBuffer& src,
                                               ImageBuffer& dst,
                                               ImageFormat targetFormat) {
    if (!src.isValid())
        return VisionResult::error("Invalid source image", 1);

    // Only handle common conversions
    if (src.format == ImageFormat::BGR8 && targetFormat == ImageFormat::RGB8) {
        dst = src;
        dst.data = new uint8_t[src.dataSize];
        dst.format = ImageFormat::RGB8;

        for (uint64_t i = 0; i < src.dataSize; i += 3) {
            dst.data[i]     = src.data[i + 2]; // R
            dst.data[i + 1] = src.data[i + 1]; // G
            dst.data[i + 2] = src.data[i];     // B
        }
        return VisionResult::ok("Converted BGR→RGB");
    }

    if (src.format == ImageFormat::RGBA8 && targetFormat == ImageFormat::RGB8) {
        dst.width = src.width;
        dst.height = src.height;
        dst.channels = 3;
        dst.format = ImageFormat::RGB8;
        dst.stride = src.width * 3;
        dst.dataSize = static_cast<uint64_t>(src.width) * src.height * 3;
        dst.data = new uint8_t[dst.dataSize];

        for (uint32_t y = 0; y < src.height; ++y) {
            for (uint32_t x = 0; x < src.width; ++x) {
                uint32_t si = y * src.stride + x * 4;
                uint32_t di = y * dst.stride + x * 3;
                dst.data[di]     = src.data[si];
                dst.data[di + 1] = src.data[si + 1];
                dst.data[di + 2] = src.data[si + 2];
            }
        }
        return VisionResult::ok("Converted RGBA→RGB");
    }

    return VisionResult::error("Unsupported format conversion", 2);
}

VisionResult ImagePreprocessor::normalize(const ImageBuffer& src,
                                           std::vector<float>& output,
                                           const float mean[3],
                                           const float std[3]) {
    if (!src.isValid())
        return VisionResult::error("Invalid source image", 1);

    if (src.channels < 3)
        return VisionResult::error("Need at least 3 channels", 2);

    // Output: CHW format (channels-first), normalized
    output.resize(3 * src.width * src.height);

    for (uint32_t y = 0; y < src.height; ++y) {
        for (uint32_t x = 0; x < src.width; ++x) {
            uint32_t si = y * src.stride + x * src.channels;

            for (uint32_t c = 0; c < 3; ++c) {
                float pixel = static_cast<float>(src.data[si + c]) / 255.0f;
                float normalized = (pixel - mean[c]) / std[c];

                // CHW layout: channel * H * W + y * W + x
                output[c * src.height * src.width + y * src.width + x] = normalized;
            }
        }
    }

    return VisionResult::ok("Image normalized");
}

VisionResult ImagePreprocessor::extractPatches(
    const std::vector<float>& normalizedImage,
    uint32_t imageSize,
    uint32_t patchSize,
    std::vector<float>& patches)
{
    uint32_t numPatchesPerDim = imageSize / patchSize;
    uint32_t totalPatches = numPatchesPerDim * numPatchesPerDim;
    uint32_t patchPixels = patchSize * patchSize * 3;  // 3 channels

    patches.resize(totalPatches * patchPixels);

    for (uint32_t py = 0; py < numPatchesPerDim; ++py) {
        for (uint32_t px = 0; px < numPatchesPerDim; ++px) {
            uint32_t patchIdx = py * numPatchesPerDim + px;

            for (uint32_t c = 0; c < 3; ++c) {
                for (uint32_t y = 0; y < patchSize; ++y) {
                    for (uint32_t x = 0; x < patchSize; ++x) {
                        uint32_t imgY = py * patchSize + y;
                        uint32_t imgX = px * patchSize + x;

                        // Source: CHW format
                        float val = normalizedImage[
                            c * imageSize * imageSize + imgY * imageSize + imgX];

                        // Dest: patch-first, then channel-pixel
                        uint32_t outIdx = patchIdx * patchPixels +
                                          c * patchSize * patchSize +
                                          y * patchSize + x;
                        patches[outIdx] = val;
                    }
                }
            }
        }
    }

    return VisionResult::ok("Patches extracted");
}

VisionResult ImagePreprocessor::loadFromFile(const std::string& filepath,
                                              ImageBuffer& output) {
    // Minimal BMP loader for Windows (no external deps)
    // For PNG/JPEG, would need stb_image or WIC
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
        return VisionResult::error("Cannot open image file", 1);

    // Read magic bytes
    uint8_t magic[2];
    file.read(reinterpret_cast<char*>(magic), 2);

    if (magic[0] == 'B' && magic[1] == 'M') {
        // BMP file
        file.seekg(0);

        // Read BITMAPFILEHEADER (14 bytes)
        uint8_t bfh[14];
        file.read(reinterpret_cast<char*>(bfh), 14);

        uint32_t dataOffset = *reinterpret_cast<uint32_t*>(&bfh[10]);

        // Read BITMAPINFOHEADER (40 bytes minimum)
        uint8_t bih[40];
        file.read(reinterpret_cast<char*>(bih), 40);

        int32_t width = *reinterpret_cast<int32_t*>(&bih[4]);
        int32_t height = *reinterpret_cast<int32_t*>(&bih[8]);
        uint16_t bpp = *reinterpret_cast<uint16_t*>(&bih[14]);

        if (bpp != 24 && bpp != 32)
            return VisionResult::error("Only 24/32-bit BMP supported", 3);

        bool flipped = (height > 0);
        if (height < 0) height = -height;

        output.width = static_cast<uint32_t>(width);
        output.height = static_cast<uint32_t>(height);
        output.channels = (bpp == 32) ? 4 : 3;
        output.format = (bpp == 32) ? ImageFormat::RGBA8 : ImageFormat::BGR8;
        output.stride = output.width * output.channels;
        output.dataSize = static_cast<uint64_t>(output.stride) * output.height;
        output.data = new uint8_t[output.dataSize];

        // BMP row padding: rows are padded to 4-byte boundary
        uint32_t bmpStride = ((width * (bpp / 8) + 3) / 4) * 4;
        std::vector<uint8_t> rowBuf(bmpStride);

        file.seekg(dataOffset);

        for (int32_t y = 0; y < height; ++y) {
            file.read(reinterpret_cast<char*>(rowBuf.data()), bmpStride);

            uint32_t dstY = flipped ? (height - 1 - y) : y;
            memcpy(output.data + dstY * output.stride,
                   rowBuf.data(),
                   output.stride);
        }

        return VisionResult::ok("BMP loaded");
    }

    // For PNG/JPEG — would integrate stb_image or WIC here
    return VisionResult::error("Only BMP format supported without external deps", 4);
}

VisionResult ImagePreprocessor::loadFromBuffer(const uint8_t* buffer,
                                                uint64_t bufferSize,
                                                ImageFormat format,
                                                ImageBuffer& output) {
    if (!buffer || bufferSize == 0)
        return VisionResult::error("Empty buffer", 1);

    if (format == ImageFormat::RGB8 || format == ImageFormat::BGR8 ||
        format == ImageFormat::RGBA8) {
        // Assume raw pixel data — need dimensions from caller
        return VisionResult::error("Raw format needs dimensions", 2);
    }

    return VisionResult::error("Compressed format decoding not yet implemented", 3);
}

#ifdef _WIN32
VisionResult ImagePreprocessor::loadFromClipboard(ImageBuffer& output) {
    if (!OpenClipboard(nullptr))
        return VisionResult::error("Cannot open clipboard", 1);

    HANDLE hData = GetClipboardData(CF_BITMAP);
    if (!hData) {
        CloseClipboard();
        return VisionResult::error("No bitmap in clipboard", 2);
    }

    HBITMAP hBmp = static_cast<HBITMAP>(hData);
    BITMAP bmp;
    GetObject(hBmp, sizeof(BITMAP), &bmp);

    output.width = static_cast<uint32_t>(bmp.bmWidth);
    output.height = static_cast<uint32_t>(bmp.bmHeight);
    output.channels = 3;
    output.format = ImageFormat::BGR8;
    output.stride = output.width * 3;
    output.dataSize = static_cast<uint64_t>(output.stride) * output.height;
    output.data = new uint8_t[output.dataSize];

    HDC hdc = GetDC(nullptr);
    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = -bmp.bmHeight;  // Top-down
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    GetDIBits(hdc, hBmp, 0, bmp.bmHeight, output.data,
              reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

    ReleaseDC(nullptr, hdc);
    CloseClipboard();

    return VisionResult::ok("Clipboard image loaded");
}
#else
VisionResult ImagePreprocessor::loadFromClipboard(ImageBuffer& output) {
    (void)output;
    return VisionResult::error("Clipboard not supported on this platform", 99);
}
#endif

void ImagePreprocessor::freeBuffer(ImageBuffer& buf) {
    if (buf.data) {
        delete[] buf.data;
        buf.data = nullptr;
    }
    buf.width = 0;
    buf.height = 0;
    buf.dataSize = 0;
}

// ============================================================================
// VisionEncoder — Singleton
// ============================================================================

VisionEncoder& VisionEncoder::instance() {
    static VisionEncoder inst;
    return inst;
}

VisionEncoder::VisionEncoder()
    : modelLoaded_(false), modelHandle_(nullptr), projectorHandle_(nullptr),
      totalEncoded_(0), totalDescriptions_(0), totalOCR_(0),
      encodeTimeAccumMs_(0.0)
{}

VisionEncoder::~VisionEncoder() {
    shutdown();
}

// ============================================================================
// Model Loading
// ============================================================================

VisionResult VisionEncoder::loadModel(const VisionModelConfig& config) {
    std::lock_guard<std::mutex> lock(encoderMutex_);

    if (modelLoaded_) {
        unloadModel();
    }

    config_ = config;

    // Compute derived values
    config_.numPatches = (config_.inputSize / config_.patchSize) *
                         (config_.inputSize / config_.patchSize);

    // TODO: Load GGUF vision model via streaming_gguf_loader
    // For now, set up the pipeline without actual model weights
    modelHandle_ = nullptr;

    if (!config.projectorPath.empty()) {
        // TODO: Load mm_projector for LLaVA-style models
        projectorHandle_ = nullptr;
    }

    modelLoaded_ = true;
    return VisionResult::ok("Vision encoder initialized (awaiting model weights)");
}

VisionResult VisionEncoder::unloadModel() {
    std::lock_guard<std::mutex> lock(encoderMutex_);

    modelHandle_ = nullptr;
    projectorHandle_ = nullptr;
    modelLoaded_ = false;

    return VisionResult::ok("Vision model unloaded");
}

bool VisionEncoder::isReady() const {
    return modelLoaded_;
}

const VisionModelConfig& VisionEncoder::getConfig() const {
    return config_;
}

// ============================================================================
// Internal Pipeline
// ============================================================================

VisionResult VisionEncoder::preprocessAndEncode(const ImageBuffer& image,
                                                  VisionEmbedding& output) {
    if (!image.isValid())
        return VisionResult::error("Invalid image buffer", 1);

    auto t0 = std::chrono::high_resolution_clock::now();

    // Step 1: Convert to RGB if needed
    ImageBuffer rgb;
    bool needFree = false;

    if (image.format == ImageFormat::BGR8) {
        ImagePreprocessor::convertFormat(image, rgb, ImageFormat::RGB8);
        needFree = true;
    } else if (image.format == ImageFormat::RGBA8) {
        ImagePreprocessor::convertFormat(image, rgb, ImageFormat::RGB8);
        needFree = true;
    } else {
        rgb = image;  // Assume RGB8
    }

    // Step 2: Resize to model input size
    ImageBuffer resized;
    bool resizedFree = false;
    if (rgb.width != config_.inputSize || rgb.height != config_.inputSize) {
        ImagePreprocessor::resize(rgb, resized, config_.inputSize,
                                  config_.inputSize);
        resizedFree = true;
    } else {
        resized = rgb;
    }

    // Step 3: Normalize (CLIP normalization by default)
    std::vector<float> normalized;
    const float* mean = ImagePreprocessor::CLIP_MEAN;
    const float* std_dev = ImagePreprocessor::CLIP_STD;

    if (config_.arch == VisionModelConfig::Architecture::PHI3_VISION) {
        mean = ImagePreprocessor::IMAGENET_MEAN;
        std_dev = ImagePreprocessor::IMAGENET_STD;
    }

    VisionResult nr = ImagePreprocessor::normalize(resized, normalized,
                                                    mean, std_dev);
    if (!nr.success) {
        if (needFree) ImagePreprocessor::freeBuffer(rgb);
        if (resizedFree) ImagePreprocessor::freeBuffer(resized);
        return nr;
    }

    // Step 4: Extract patches
    std::vector<float> patches;
    VisionResult pr = ImagePreprocessor::extractPatches(
        normalized, config_.inputSize, config_.patchSize, patches);
    if (!pr.success) {
        if (needFree) ImagePreprocessor::freeBuffer(rgb);
        if (resizedFree) ImagePreprocessor::freeBuffer(resized);
        return pr;
    }

    // Step 5: Run through vision model (or generate placeholder embedding)
    VisionResult mr = inferVisionModel(patches, output);

    // Cleanup
    if (needFree) ImagePreprocessor::freeBuffer(rgb);
    if (resizedFree) ImagePreprocessor::freeBuffer(resized);

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    encodeTimeAccumMs_ += ms;
    totalEncoded_.fetch_add(1);

    return mr;
}

VisionResult VisionEncoder::inferVisionModel(const std::vector<float>& preprocessed,
                                               VisionEmbedding& output) {
    if (modelHandle_) {
        // TODO: Route through GGUF inference engine
        // model_inference(modelHandle_, preprocessed.data(), ...)
    }

    // Fallback: Generate a hash-based embedding from the patch data
    // This enables the pipeline to work end-to-end without model weights
    output.embedding.resize(config_.embeddingDim, 0.0f);
    output.numPatches = config_.numPatches;
    output.confidence = 0.5f;  // Low confidence without real model

    // Hash-based embedding: use patch statistics as embedding features
    if (!preprocessed.empty()) {
        uint32_t dim = config_.embeddingDim;

        // Compute basic statistics across patches
        float sum = 0.0f, sumSq = 0.0f;
        for (float v : preprocessed) {
            sum += v;
            sumSq += v * v;
        }
        float mean = sum / preprocessed.size();
        float variance = sumSq / preprocessed.size() - mean * mean;

        // Fill embedding dimensions with hashed patch features
        for (uint32_t d = 0; d < dim; ++d) {
            float feature = 0.0f;

            // Sample from preprocessed at stride intervals
            uint32_t stride = static_cast<uint32_t>(preprocessed.size()) / dim;
            if (stride == 0) stride = 1;
            uint32_t idx = d * stride;
            if (idx < preprocessed.size()) {
                feature = preprocessed[idx];
            }

            // Mix with statistics
            feature += mean * sinf(static_cast<float>(d) * 0.1f);
            feature += sqrtf(variance) * cosf(static_cast<float>(d) * 0.05f);

            output.embedding[d] = feature;
        }

        // L2 normalize
        float norm = 0.0f;
        for (float v : output.embedding) norm += v * v;
        norm = sqrtf(norm);
        if (norm > 1e-10f) {
            for (float& v : output.embedding) v /= norm;
        }
    }

    output.description = "[placeholder: vision model not loaded]";
    return VisionResult::ok("Vision embedding generated (placeholder mode)");
}

// ============================================================================
// Public API
// ============================================================================

VisionResult VisionEncoder::encode(const ImageBuffer& image,
                                    VisionEmbedding& output) {
    if (!modelLoaded_)
        return VisionResult::error("Model not loaded", 1);
    return preprocessAndEncode(image, output);
}

VisionResult VisionEncoder::encodeFile(const std::string& imagePath,
                                        VisionEmbedding& output) {
    ImageBuffer image;
    VisionResult lr = ImagePreprocessor::loadFromFile(imagePath, image);
    if (!lr.success) return lr;

    VisionResult er = encode(image, output);
    ImagePreprocessor::freeBuffer(image);
    return er;
}

VisionResult VisionEncoder::encodeBytes(const uint8_t* data,
                                         uint64_t dataSize,
                                         ImageFormat format,
                                         VisionEmbedding& output) {
    ImageBuffer image;
    VisionResult lr = ImagePreprocessor::loadFromBuffer(data, dataSize,
                                                         format, image);
    if (!lr.success) return lr;

    VisionResult er = encode(image, output);
    ImagePreprocessor::freeBuffer(image);
    return er;
}

VisionResult VisionEncoder::encodeBatch(const std::vector<ImageBuffer>& images,
                                         std::vector<VisionEmbedding>& outputs) {
    outputs.resize(images.size());
    uint32_t failed = 0;

    for (size_t i = 0; i < images.size(); ++i) {
        VisionResult r = encode(images[i], outputs[i]);
        if (!r.success) failed++;
    }

    if (failed > 0) {
        return VisionResult::error("Some encodings failed",
                                    static_cast<int>(failed));
    }
    return VisionResult::ok("Batch encoding complete");
}

// ============================================================================
// Vision-Language Integration
// ============================================================================

VisionResult VisionEncoder::describeImage(const ImageBuffer& image,
                                           std::string& description) {
    VisionEmbedding emb;
    VisionResult er = encode(image, emb);
    if (!er.success) return er;

    totalDescriptions_.fetch_add(1);

    // TODO: Route embedding through LLM with image description prompt
    // For now, provide basic image metadata
    std::ostringstream ss;
    ss << "Image: " << image.width << "x" << image.height
       << " (" << image.channels << " channels)"
       << " encoded to " << emb.embedding.size() << " dimensions"
       << " with " << emb.numPatches << " patches";
    description = ss.str();

    return VisionResult::ok("Description generated");
}

VisionResult VisionEncoder::extractCodeFromScreenshot(const ImageBuffer& image,
                                                       std::string& code) {
    totalOCR_.fetch_add(1);

    // TODO: Implement OCR pipeline
    // Options: Tesseract integration, or route through multimodal LLM
    code = "// [OCR extraction not yet implemented — needs Tesseract or VLM]";
    return VisionResult::error("OCR not yet implemented", -1);
}

VisionResult VisionEncoder::extractDiagramStructure(const ImageBuffer& image,
                                                      std::string& structuredJson) {
    // TODO: Route through VLM for structured extraction
    structuredJson = R"({"type": "diagram", "status": "extraction_not_implemented"})";
    return VisionResult::error("Diagram extraction not yet implemented", -1);
}

VisionResult VisionEncoder::createMultiModalPrompt(const ImageBuffer& image,
                                                     const std::string& userQuery,
                                                     VisionTextPair& output) {
    // Encode image
    VisionResult er = encode(image, output.imageEmb);
    if (!er.success) return er;

    // Create combined prompt
    std::ostringstream ss;
    ss << "[IMAGE: " << image.width << "x" << image.height
       << " encoded=" << output.imageEmb.embedding.size() << "d]\n"
       << "User query: " << userQuery;

    output.textPrompt = ss.str();
    output.relevanceScore = output.imageEmb.confidence;

    return VisionResult::ok("Multi-modal prompt created");
}

// ============================================================================
// Similarity
// ============================================================================

VisionResult VisionEncoder::searchSimilarCode(const ImageBuffer& image,
                                               uint32_t topK,
                                               std::vector<std::string>& results) {
    // TODO: Bridge to EmbeddingEngine for cross-modal search
    (void)image;
    (void)topK;
    results.clear();
    return VisionResult::error("Cross-modal search not yet implemented", -1);
}

float VisionEncoder::computeSimilarity(const VisionEmbedding& a,
                                        const VisionEmbedding& b) const {
    if (a.embedding.size() != b.embedding.size()) return 0.0f;

    // Cosine similarity
    float dot = 0.0f, normA = 0.0f, normB = 0.0f;
    for (size_t i = 0; i < a.embedding.size(); ++i) {
        dot   += a.embedding[i] * b.embedding[i];
        normA += a.embedding[i] * a.embedding[i];
        normB += b.embedding[i] * b.embedding[i];
    }

    float denom = sqrtf(normA) * sqrtf(normB);
    if (denom < 1e-10f) return 0.0f;
    return dot / denom;
}

// ============================================================================
// Statistics
// ============================================================================

VisionEncoder::VisionStats VisionEncoder::getStats() const {
    VisionStats stats = {};
    stats.totalEncoded = totalEncoded_.load();
    stats.totalDescriptions = totalDescriptions_.load();
    stats.totalOCR = totalOCR_.load();

    if (stats.totalEncoded > 0) {
        stats.avgEncodeTimeMs = encodeTimeAccumMs_ / stats.totalEncoded;
    }

    return stats;
}

// ============================================================================
// Shutdown
// ============================================================================

void VisionEncoder::shutdown() {
    std::lock_guard<std::mutex> lock(encoderMutex_);
    modelHandle_ = nullptr;
    projectorHandle_ = nullptr;
    modelLoaded_ = false;
}

} // namespace Vision
} // namespace RawrXD
