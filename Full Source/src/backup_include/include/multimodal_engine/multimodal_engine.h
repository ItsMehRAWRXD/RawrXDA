#ifndef MULTIMODAL_ENGINE_H
#define MULTIMODAL_ENGINE_H

// C++20, no Qt. Image → base64 prompt; callback replaces signal.

#include <string>
#include <vector>
#include <functional>

class MultiModalEngine
{
public:
    using VisionPromptReadyFn = std::function<void(const std::string& base64, const std::string& mime)>;

    MultiModalEngine() = default;

    void setOnVisionPromptReady(VisionPromptReadyFn f) { m_onVisionPromptReady = std::move(f); }

    /** imageData: RGBA or RGB bytes; width, height, bytesPerPixel. */
    void processImage(const uint8_t* imageData, int width, int height, int bytesPerPixel = 4);
    void processImage(const std::string& filePath);

private:
    VisionPromptReadyFn m_onVisionPromptReady;
};

#endif // MULTIMODAL_ENGINE_H
