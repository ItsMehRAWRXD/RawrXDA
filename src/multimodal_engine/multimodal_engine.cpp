#include "multimodal_engine.h"
MultiModalEngine::MultiModalEngine()
    
{
}

void MultiModalEngine::processImage(const void &image)
{
    void scaledImage = image;
    if (image.width() > 1024 || image.height() > 1024) {
        scaledImage = image.scaled(1024, 1024, KeepAspectRatio, SmoothTransformation);
    }

    std::vector<uint8_t> ba;
    std::stringstream buffer(&ba);
    buffer.open(std::iostream::WriteOnly);
    scaledImage.save(&buffer, "PNG");
    std::string base64 = std::string::fromLatin1(ba.toBase64().data());
    visionPromptReady(base64, "image/png");
}

void MultiModalEngine::processImage(const std::string &filePath)
{
    void image(filePath);
    if (image.isNull()) {
        // // qWarning:  "Failed to load image from file path:" << filePath;
        return;
    }
    processImage(image);
}






