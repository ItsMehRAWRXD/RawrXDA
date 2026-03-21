#if !defined(_MSC_VER)

#include "vision_encoder.hpp"

namespace RawrXD::Vision {

VisionEncoder::VisionEncoder()
    : modelLoaded_(false),
      modelHandle_(nullptr),
      projectorHandle_(nullptr),
      totalEncoded_(0),
      totalDescriptions_(0),
      totalOCR_(0),
      encodeTimeAccumMs_(0.0) {}

VisionEncoder::~VisionEncoder() = default;

VisionEncoder& VisionEncoder::instance() {
    static VisionEncoder s_instance;
    return s_instance;
}

VisionResult VisionEncoder::loadModel(const VisionModelConfig& config) {
    std::lock_guard<std::mutex> lock(encoderMutex_);
    config_ = config;
    modelLoaded_ = false;
    modelHandle_ = nullptr;
    projectorHandle_ = nullptr;
    return VisionResult::error("Vision backend unavailable on non-MSVC toolchain", -2);
}

bool VisionEncoder::isReady() const {
    return modelLoaded_;
}

VisionResult VisionEncoder::describeImage(const ImageBuffer&, std::string& description) {
    description = "Vision backend unavailable on non-MSVC toolchain";
    return VisionResult::error("describeImage unavailable on non-MSVC toolchain", -2);
}

VisionResult VisionEncoder::extractCodeFromScreenshot(const ImageBuffer&, std::string& code) {
    code.clear();
    return VisionResult::error("extractCodeFromScreenshot unavailable on non-MSVC toolchain", -2);
}

VisionResult VisionEncoder::extractDiagramStructure(const ImageBuffer&, std::string& structuredJson) {
    structuredJson = "{}";
    return VisionResult::error("extractDiagramStructure unavailable on non-MSVC toolchain", -2);
}

VisionResult VisionEncoder::createMultiModalPrompt(const ImageBuffer&, const std::string& userQuery, VisionTextPair& output) {
    output.textPrompt = userQuery;
    output.structuredData = "{}";
    output.relevanceScore = 0.0f;
    output.imageEmb.embedding.clear();
    output.imageEmb.patchEmbeddings.clear();
    output.imageEmb.numPatches = 0;
    output.imageEmb.confidence = 0.0f;
    output.imageEmb.description = "Vision backend unavailable on non-MSVC toolchain";
    return VisionResult::error("createMultiModalPrompt unavailable on non-MSVC toolchain", -2);
}

void ImagePreprocessor::freeBuffer(ImageBuffer& buf) {
    if (buf.data) {
        delete[] buf.data;
        buf.data = nullptr;
    }
    buf.width = 0;
    buf.height = 0;
    buf.channels = 0;
    buf.stride = 0;
    buf.dataSize = 0;
}

void VisionEncoder::shutdown() {
    std::lock_guard<std::mutex> lock(encoderMutex_);
    modelLoaded_ = false;
    modelHandle_ = nullptr;
    projectorHandle_ = nullptr;
}

}  // namespace RawrXD::Vision

#endif  // !defined(_MSC_VER)
