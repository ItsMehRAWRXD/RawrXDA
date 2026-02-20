// ============================================================================
// vision_encoder.h — Vision Encoder & Multimodal Input System
// ============================================================================
// Full VisionEncoder implementation wired to UI:
//   - Image loading (PNG, JPEG, BMP, WebP, TIFF)
//   - Base64 encoding for multimodal API payloads
//   - Image preprocessing (resize, normalize, tile)
//   - Clipboard paste support
//   - Drag-and-drop from explorer
//   - Screenshot capture
//   - Image-in-prompt rendering
//   - Integration with @image mentions
//   - Plugin interface for custom image processors
//
// Integrates with:
//   - Win32IDE chat panel (paste/drop/display)
//   - ContextMentionParser (@image resolver)
//   - AgenticBridge (multimodal model routing)
//   - TelemetryExporter (image processing metrics)
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace RawrXD {
namespace Multimodal {

// ============================================================================
// Image format detection
// ============================================================================
enum class ImageFormat {
    Unknown,
    PNG,
    JPEG,
    BMP,
    WebP,
    TIFF,
    GIF,
    SVG,
    ICO
};

// ============================================================================
// Image data
// ============================================================================
struct ImageData {
    std::vector<uint8_t>    rawBytes;       // Raw file bytes
    std::string             base64;         // Base64 encoded
    std::string             mimeType;       // "image/png", etc.
    ImageFormat             format;
    int                     width;
    int                     height;
    int                     channels;       // 1=gray, 3=RGB, 4=RGBA
    std::string             sourcePath;     // Original file path (empty if clipboard)
    std::string             sourceType;     // "file", "clipboard", "screenshot", "url"
    
    bool isValid() const { return !rawBytes.empty() && width > 0 && height > 0; }
    size_t sizeBytes() const { return rawBytes.size(); }
};

// ============================================================================
// Vision processing result
// ============================================================================
struct VisionResult {
    bool        success;
    std::string error;
    ImageData   image;
    
    // Processing metadata
    int64_t     loadTimeMs;
    int64_t     encodeTimeMs;
    int         originalWidth;
    int         originalHeight;
    bool        wasResized;
    bool        wasTiled;
    int         tileCount;
    
    static VisionResult ok(const ImageData& img) {
        VisionResult r;
        r.success = true;
        r.image = img;
        return r;
    }
    static VisionResult fail(const std::string& err) {
        VisionResult r;
        r.success = false;
        r.error = err;
        return r;
    }
};

// ============================================================================
// Vision configuration
// ============================================================================
struct VisionConfig {
    int         maxWidth = 2048;            // Max image width before resize
    int         maxHeight = 2048;           // Max image height before resize
    int         tileSize = 512;             // For tiled processing
    bool        enableTiling = true;        // Break large images into tiles
    int         jpegQuality = 85;           // JPEG compression quality
    size_t      maxFileSizeBytes = 20 * 1024 * 1024; // 20MB max
    bool        stripExif = true;           // Remove EXIF metadata
    
    // Supported formats for the current model
    std::vector<ImageFormat> supportedFormats = {
        ImageFormat::PNG, ImageFormat::JPEG, ImageFormat::BMP,
        ImageFormat::WebP, ImageFormat::GIF
    };
};

// ============================================================================
// Image processor plugin interface (C-ABI)
// ============================================================================
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ImageProcessorInfo {
    char name[64];
    char version[32];
    char description[256];
    char supportedFormats[128]; // Comma-separated: "png,jpeg,webp"
} ImageProcessorInfo;

typedef ImageProcessorInfo* (*ImageProcessor_GetInfo_fn)(void);
typedef int  (*ImageProcessor_Init_fn)(const char* configJson);
typedef int  (*ImageProcessor_Process_fn)(const uint8_t* input, int inputLen,
                                          uint8_t* output, int* outputLen,
                                          int maxOutputLen);
typedef void (*ImageProcessor_Shutdown_fn)(void);

#ifdef __cplusplus
}
#endif

// ============================================================================
// VisionEncoder — Main image processing engine
// ============================================================================
class VisionEncoder {
public:
    VisionEncoder();
    ~VisionEncoder();

    // ---- Configuration ----
    void SetConfig(const VisionConfig& config);
    VisionConfig GetConfig() const;

    // ---- Image Loading ----
    VisionResult LoadFromFile(const std::string& filePath);
    VisionResult LoadFromClipboard();
    VisionResult LoadFromMemory(const uint8_t* data, size_t size, 
                                 const std::string& hint = "");
    VisionResult LoadFromBase64(const std::string& base64Data,
                                 const std::string& mimeType);
    VisionResult LoadFromURL(const std::string& url);
    VisionResult CaptureScreenshot(int x = 0, int y = 0, 
                                    int width = 0, int height = 0);

    // ---- Encoding ----
    std::string EncodeToBase64(const ImageData& image);
    std::string BuildMultimodalPayload(const ImageData& image,
                                        const std::string& prompt);
    
    // ---- Preprocessing ----
    VisionResult Resize(const ImageData& image, int maxWidth, int maxHeight);
    std::vector<ImageData> TileImage(const ImageData& image, int tileSize);
    VisionResult ConvertFormat(const ImageData& image, ImageFormat targetFormat);

    // ---- Format Detection ----
    static ImageFormat DetectFormat(const uint8_t* data, size_t size);
    static std::string FormatToMimeType(ImageFormat format);
    static std::string FormatToExtension(ImageFormat format);
    static ImageFormat ExtensionToFormat(const std::string& ext);

    // ---- Win32 UI Integration ----
#ifdef _WIN32
    // Handle WM_DROPFILES for image drag-and-drop
    VisionResult HandleDropFiles(HDROP hDrop);
    
    // Handle clipboard paste (CF_BITMAP / CF_DIB / CF_HDROP)
    VisionResult HandlePaste(HWND hwnd);
    
    // Render image thumbnail in HWND
    bool RenderThumbnail(HDC hdc, const ImageData& image,
                          int x, int y, int width, int height);
    
    // Create HBITMAP from ImageData
    HBITMAP CreateHBitmap(const ImageData& image);
#endif

    // ---- Plugin System ----
    bool LoadProcessorPlugin(const std::string& dllPath);
    void UnloadProcessorPlugins();

    // ---- Statistics ----
    struct Stats {
        uint64_t imagesLoaded;
        uint64_t imagesEncoded;
        uint64_t totalBytesProcessed;
        uint64_t clipboardPastes;
        uint64_t dragDrops;
        uint64_t screenshots;
        float    avgLoadTimeMs;
        float    avgEncodeTimeMs;
    };
    Stats GetStats() const;

private:
    // Internal helpers
    std::string base64Encode(const uint8_t* data, size_t length);
    std::vector<uint8_t> base64Decode(const std::string& encoded);
    
    // BMP header parsing (built-in, no external libs)
    bool parseBMPHeader(const uint8_t* data, size_t size,
                         int& width, int& height, int& channels);
    
    // PNG signature check and minimal header parse
    bool parsePNGHeader(const uint8_t* data, size_t size,
                         int& width, int& height);
    
    // JPEG SOF marker parse
    bool parseJPEGHeader(const uint8_t* data, size_t size,
                          int& width, int& height);

    // Simple bilinear resize (no external deps)
    std::vector<uint8_t> resizeBilinear(const uint8_t* data,
                                         int srcW, int srcH, int channels,
                                         int dstW, int dstH);

    VisionConfig                m_config;
    mutable std::mutex          m_mutex;
    
    // Plugin processors
    struct LoadedProcessor {
        std::string                    path;
        ImageProcessor_GetInfo_fn      fnGetInfo;
        ImageProcessor_Process_fn      fnProcess;
        ImageProcessor_Shutdown_fn     fnShutdown;
#ifdef _WIN32
        HMODULE                        hModule;
#else
        void*                          hModule;
#endif
    };
    std::vector<LoadedProcessor> m_processors;
    
    // Statistics
    mutable std::mutex          m_statsMutex;
    Stats                       m_stats{};
};

} // namespace Multimodal
} // namespace RawrXD
