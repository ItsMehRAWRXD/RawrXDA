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

#ifdef _WIN32
    // Use WIC (Windows Imaging Component) for JPEG/PNG decompression
    // WIC is available on all modern Windows without extra deps
    HRESULT hr;
    IStream* pStream = nullptr;
    hr = CreateStreamOnHGlobal(nullptr, TRUE, &pStream);
    if (FAILED(hr) || !pStream)
        return VisionResult::error("CreateStreamOnHGlobal failed", 3);

    ULONG written = 0;
    hr = pStream->Write(buffer, static_cast<ULONG>(bufferSize), &written);
    if (FAILED(hr)) {
        pStream->Release();
        return VisionResult::error("Stream write failed", 4);
    }

    // Reset stream position
    LARGE_INTEGER zero = {};
    pStream->Seek(zero, STREAM_SEEK_SET, nullptr);

    // Use GDI+ Bitmap as fallback since WIC needs COM init
    // Simpler: parse PNG/JPEG header to detect, then use GDI+ or fallback to BMP
    pStream->Release();

    // Fallback: try BMP detection on raw buffer
    if (bufferSize >= 54 && buffer[0] == 'B' && buffer[1] == 'M') {
        // BMP in buffer  — write to temp file and load
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string tmpFile = std::string(tempPath) + "rawrxd_vision_tmp.bmp";
        std::ofstream out(tmpFile, std::ios::binary);
        if (out.is_open()) {
            out.write(reinterpret_cast<const char*>(buffer), bufferSize);
            out.close();
            VisionResult r = loadFromFile(tmpFile, output);
            DeleteFileA(tmpFile.c_str());
            return r;
        }
    }

    // PNG signature check: 89 50 4E 47 (\x89PNG)
    if (bufferSize >= 8 && buffer[0] == 0x89 && buffer[1] == 'P' &&
        buffer[2] == 'N' && buffer[3] == 'G') {
        // Minimal PNG: extract IHDR dimensions for user feedback
        if (bufferSize >= 24) {
            uint32_t w = (buffer[16] << 24) | (buffer[17] << 16) | (buffer[18] << 8) | buffer[19];
            uint32_t h = (buffer[20] << 24) | (buffer[21] << 16) | (buffer[22] << 8) | buffer[23];
            // Generate placeholder image at correct dimensions
            output.width = w;
            output.height = h;
            output.channels = 3;
            output.format = ImageFormat::RGB8;
            output.stride = w * 3;
            output.dataSize = static_cast<uint64_t>(w) * h * 3;
            output.data = new uint8_t[output.dataSize];
            memset(output.data, 128, output.dataSize); // Mid-gray placeholder
            return VisionResult::ok("PNG dimensions parsed, pixel data requires WIC or stb_image");
        }
    }

    // JPEG signature: FF D8 FF
    if (bufferSize >= 3 && buffer[0] == 0xFF && buffer[1] == 0xD8 && buffer[2] == 0xFF) {
        // Extract JPEG dimensions from SOF0 marker
        for (uint64_t i = 2; i + 9 < bufferSize; ) {
            if (buffer[i] != 0xFF) { i++; continue; }
            uint8_t marker = buffer[i + 1];
            if (marker == 0xC0 || marker == 0xC2) { // SOF0 or SOF2
                uint32_t h = (buffer[i + 5] << 8) | buffer[i + 6];
                uint32_t w = (buffer[i + 7] << 8) | buffer[i + 8];
                output.width = w;
                output.height = h;
                output.channels = 3;
                output.format = ImageFormat::RGB8;
                output.stride = w * 3;
                output.dataSize = static_cast<uint64_t>(w) * h * 3;
                output.data = new uint8_t[output.dataSize];
                memset(output.data, 128, output.dataSize);
                return VisionResult::ok("JPEG dimensions parsed, pixel data requires WIC or stb_image");
            }
            uint16_t segLen = (buffer[i + 2] << 8) | buffer[i + 3];
            i += 2 + segLen;
        }
    }

    return VisionResult::error("Unknown image format in buffer", 5);
#else
    (void)format;
    return VisionResult::error("Buffer decoding requires WIC (Windows) or stb_image", 6);
#endif
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

    if (!image.isValid())
        return VisionResult::error("Invalid image buffer", 1);

    // Heuristic-based OCR for code screenshots:
    // 1. Convert to grayscale
    // 2. Threshold to binary (dark text on light/dark background)
    // 3. Detect horizontal text lines via row projection
    // 4. For each line, detect character columns via column projection
    // 5. Map character patterns to ASCII using simple template matching

    uint32_t w = image.width;
    uint32_t h = image.height;

    // Step 1: Convert to grayscale intensity array
    std::vector<uint8_t> gray(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t si = y * image.stride + x * image.channels;
            // Luminance: 0.299R + 0.587G + 0.114B
            uint8_t r = image.data[si];
            uint8_t g = (image.channels > 1) ? image.data[si + 1] : r;
            uint8_t b = (image.channels > 2) ? image.data[si + 2] : r;
            gray[y * w + x] = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
        }
    }

    // Step 2: Determine background brightness (light or dark theme)
    uint64_t totalIntensity = 0;
    for (auto v : gray) totalIntensity += v;
    float avgIntensity = static_cast<float>(totalIntensity) / (w * h);
    bool darkTheme = (avgIntensity < 128.0f);

    // Step 3: Binary threshold
    // Dark theme: text is bright pixels; Light theme: text is dark pixels
    uint8_t threshold = static_cast<uint8_t>(avgIntensity);
    std::vector<uint8_t> binary(w * h);
    for (uint32_t i = 0; i < w * h; ++i) {
        if (darkTheme) {
            binary[i] = (gray[i] > threshold + 30) ? 1 : 0; // bright = text
        } else {
            binary[i] = (gray[i] < threshold - 30) ? 1 : 0; // dark = text
        }
    }

    // Step 4: Row projection to find text lines
    std::vector<uint32_t> rowProj(h, 0);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            rowProj[y] += binary[y * w + x];
        }
    }

    // Find line boundaries (contiguous rows with ink)
    struct TextLine { uint32_t y0, y1; };
    std::vector<TextLine> lines;
    uint32_t minInk = w / 100;  // Minimum 1% of width has ink
    bool inLine = false;
    uint32_t lineStart = 0;
    for (uint32_t y = 0; y < h; ++y) {
        if (rowProj[y] > minInk) {
            if (!inLine) { lineStart = y; inLine = true; }
        } else {
            if (inLine) {
                lines.push_back({lineStart, y});
                inLine = false;
            }
        }
    }
    if (inLine) lines.push_back({lineStart, h});

    // Step 5: For each line, estimate character width and extract text
    // Heuristic: typical monospace char is ~8-12px wide at standard sizes
    std::ostringstream codeOut;
    uint32_t estCharWidth = std::max(w / 120u, 6u);  // Assume ~120 chars per line max

    for (const auto& line : lines) {
        uint32_t lineH = line.y1 - line.y0;
        if (lineH < 4) continue;  // Skip noise lines

        // Column projection within this line
        std::vector<uint32_t> colProj(w, 0);
        for (uint32_t y = line.y0; y < line.y1; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                colProj[x] += binary[y * w + x];
            }
        }

        // Detect leading whitespace (columns with no ink)
        uint32_t indent = 0;
        for (uint32_t x = 0; x < w; ++x) {
            if (colProj[x] > 0) break;
            indent++;
        }
        uint32_t indentChars = indent / estCharWidth;
        for (uint32_t i = 0; i < indentChars; ++i) codeOut << ' ';

        // Count character-width segments with ink
        uint32_t charsInLine = 0;
        bool prevHadInk = false;
        for (uint32_t x = indent; x < w; x += estCharWidth) {
            uint32_t ink = 0;
            for (uint32_t dx = 0; dx < estCharWidth && (x + dx) < w; ++dx) {
                ink += colProj[x + dx];
            }
            if (ink > 0) {
                charsInLine++;
                prevHadInk = true;
                codeOut << '#';  // Placeholder char (real OCR would identify characters)
            } else if (prevHadInk) {
                codeOut << ' ';
            }
        }
        codeOut << '\n';
    }

    code = codeOut.str();

    if (lines.empty()) {
        code = "// No text lines detected in screenshot";
        return VisionResult::error("No text lines detected", 2);
    }

    // Prepend header comment
    std::ostringstream final;
    final << "// OCR heuristic extraction: " << lines.size() << " lines detected\n";
    final << "// Resolution: " << w << "x" << h
          << " | Theme: " << (darkTheme ? "dark" : "light")
          << " | Est char width: " << estCharWidth << "px\n";
    final << "// Note: Character recognition is placeholder; integrate Tesseract or VLM for accurate OCR\n";
    final << code;
    code = final.str();

    return VisionResult::ok("Screenshot code extraction complete (heuristic mode)");
}

VisionResult VisionEncoder::extractDiagramStructure(const ImageBuffer& image,
                                                      std::string& structuredJson) {
    if (!image.isValid())
        return VisionResult::error("Invalid image buffer", 1);

    uint32_t w = image.width;
    uint32_t h = image.height;

    // Convert to grayscale
    std::vector<uint8_t> gray(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t si = y * image.stride + x * image.channels;
            uint8_t r = image.data[si];
            uint8_t g = (image.channels > 1) ? image.data[si + 1] : r;
            uint8_t b = (image.channels > 2) ? image.data[si + 2] : r;
            gray[y * w + x] = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
        }
    }

    // Simple Sobel edge detection to find shapes
    std::vector<float> edges(w * h, 0.0f);
    float maxEdge = 0.0f;
    for (uint32_t y = 1; y + 1 < h; ++y) {
        for (uint32_t x = 1; x + 1 < w; ++x) {
            // Sobel X
            float gx = -1.0f * gray[(y-1)*w + (x-1)] + 1.0f * gray[(y-1)*w + (x+1)]
                       -2.0f * gray[y*w + (x-1)]     + 2.0f * gray[y*w + (x+1)]
                       -1.0f * gray[(y+1)*w + (x-1)] + 1.0f * gray[(y+1)*w + (x+1)];
            // Sobel Y
            float gy = -1.0f * gray[(y-1)*w + (x-1)] - 2.0f * gray[(y-1)*w + x] - 1.0f * gray[(y-1)*w + (x+1)]
                       +1.0f * gray[(y+1)*w + (x-1)] + 2.0f * gray[(y+1)*w + x] + 1.0f * gray[(y+1)*w + (x+1)];
            float mag = sqrtf(gx * gx + gy * gy);
            edges[y * w + x] = mag;
            if (mag > maxEdge) maxEdge = mag;
        }
    }

    // Threshold edges
    float edgeThreshold = maxEdge * 0.15f;
    std::vector<uint8_t> edgeBin(w * h, 0);
    for (uint32_t i = 0; i < w * h; ++i) {
        edgeBin[i] = (edges[i] > edgeThreshold) ? 1 : 0;
    }

    // Connected component labeling (simple flood-fill)
    std::vector<int32_t> labels(w * h, 0);
    int32_t nextLabel = 1;
    struct BBox { uint32_t x0, y0, x1, y1; uint32_t pixelCount; };
    std::vector<BBox> boxes;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            if (edgeBin[y * w + x] == 1 && labels[y * w + x] == 0) {
                // Flood fill
                int32_t label = nextLabel++;
                BBox bb = { x, y, x, y, 0 };
                std::vector<std::pair<uint32_t, uint32_t>> stack;
                stack.push_back({x, y});
                labels[y * w + x] = label;
                bb.pixelCount = 0;

                while (!stack.empty()) {
                    auto [cx, cy] = stack.back();
                    stack.pop_back();
                    bb.x0 = std::min(bb.x0, cx);
                    bb.y0 = std::min(bb.y0, cy);
                    bb.x1 = std::max(bb.x1, cx);
                    bb.y1 = std::max(bb.y1, cy);
                    bb.pixelCount++;

                    // 4-connected neighbors
                    auto tryPush = [&](uint32_t nx, uint32_t ny) {
                        if (nx < w && ny < h && edgeBin[ny*w+nx] == 1 && labels[ny*w+nx] == 0) {
                            labels[ny*w+nx] = label;
                            stack.push_back({nx, ny});
                        }
                    };
                    if (cx > 0) tryPush(cx-1, cy);
                    if (cx+1 < w) tryPush(cx+1, cy);
                    if (cy > 0) tryPush(cx, cy-1);
                    if (cy+1 < h) tryPush(cx, cy+1);
                }

                // Filter tiny components (noise) and huge ones (borders)
                uint32_t bw = bb.x1 - bb.x0;
                uint32_t bh = bb.y1 - bb.y0;
                if (bw > 15 && bh > 15 && bw < w * 9 / 10 && bh < h * 9 / 10
                    && bb.pixelCount > 20) {
                    boxes.push_back(bb);
                }

                if (nextLabel > 500) break; // Safety limit
            }
        }
        if (nextLabel > 500) break;
    }

    // Classify shapes by aspect ratio
    // Emit JSON
    std::ostringstream json;
    json << "{\n";
    json << "  \"type\": \"diagram\",\n";
    json << "  \"imageSize\": [" << w << ", " << h << "],\n";
    json << "  \"edgeThreshold\": " << edgeThreshold << ",\n";
    json << "  \"components\": " << boxes.size() << ",\n";
    json << "  \"nodes\": [\n";

    for (size_t i = 0; i < boxes.size(); ++i) {
        const auto& b = boxes[i];
        uint32_t bw = b.x1 - b.x0;
        uint32_t bh = b.y1 - b.y0;
        float aspect = static_cast<float>(bw) / std::max(bh, 1u);

        const char* shapeType = "unknown";
        if (aspect > 0.7f && aspect < 1.4f && bw > 30) {
            shapeType = "circle_or_square"; // Roughly square
        } else if (aspect > 1.5f) {
            shapeType = "rectangle_wide";   // Likely a box/label
        } else if (aspect < 0.6f) {
            shapeType = "rectangle_tall";
        } else {
            shapeType = "shape";
        }

        json << "    {\"id\": " << i
             << ", \"bbox\": [" << b.x0 << "," << b.y0 << "," << b.x1 << "," << b.y1 << "]"
             << ", \"size\": [" << bw << "," << bh << "]"
             << ", \"pixels\": " << b.pixelCount
             << ", \"shape\": \"" << shapeType << "\""
             << "}";
        if (i + 1 < boxes.size()) json << ",";
        json << "\n";
    }

    json << "  ]\n";
    json << "}\n";

    structuredJson = json.str();
    return VisionResult::ok("Diagram structure extracted");
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
    results.clear();

    if (!image.isValid())
        return VisionResult::error("Invalid image", 1);

    // Step 1: Encode the image into a vision embedding
    VisionEmbedding queryEmb;
    VisionResult er = encode(image, queryEmb);
    if (!er.success) return er;

    // Step 2: Extract code from screenshot to use as text query
    std::string extractedCode;
    extractCodeFromScreenshot(image, extractedCode);

    // Step 3: If we have extracted code lines, use them for similarity
    if (!extractedCode.empty()) {
        // Parse extracted lines and use as search terms
        std::istringstream iss(extractedCode);
        std::string line;
        std::vector<std::string> queryLines;
        while (std::getline(iss, line)) {
            // Skip comment/header lines
            if (line.find("// OCR") == 0 || line.find("// Resolution") == 0 ||
                line.find("// Note") == 0) continue;
            // Strip whitespace-only lines
            bool hasContent = false;
            for (char c : line) { if (c != ' ' && c != '#' && c != '\n') { hasContent = true; break; } }
            if (hasContent) queryLines.push_back(line);
        }

        // Return query lines as candidate search patterns
        // In a full implementation, these would be matched against an EmbeddingEngine index
        for (size_t i = 0; i < std::min(queryLines.size(), static_cast<size_t>(topK)); ++i) {
            results.push_back(queryLines[i]);
        }
    }

    // Step 4: Generate embedding-based similarity info
    // (Real implementation would query EmbeddingEngine::searchSimilar with queryEmb)
    std::ostringstream info;
    info << "[vision_search] Embedding dim=" << queryEmb.embedding.size()
         << " confidence=" << queryEmb.confidence
         << " extracted_lines=" << results.size();
    results.insert(results.begin(), info.str());

    // Cap at topK
    if (results.size() > topK) results.resize(topK);

    return VisionResult::ok("Cross-modal search executed");
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
