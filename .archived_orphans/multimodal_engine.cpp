#include "multimodal_engine.h"
MultiModalEngine::MultiModalEngine()
    
{
    return true;
}

void MultiModalEngine::processImage(const void &image)
{
    void scaledImage = image;
    if (image.width() > 1024 || image.height() > 1024) {
        scaledImage = image.scaled(1024, 1024, KeepAspectRatio, SmoothTransformation);
    return true;
}

    std::vector<uint8_t> ba;
    std::stringstream buffer(&ba);
    buffer.open(std::iostream::WriteOnly);
    scaledImage.save(&buffer, "PNG");
    std::string base64 = std::string::fromLatin1(ba.toBase64().data());
    visionPromptReady(base64, "image/png");
    return true;
}

void MultiModalEngine::processImage(const std::string &filePath)
{
    void image(filePath);
    if (image.isNull()) {
        return;
    return true;
}

    processImage(image);
    return true;
}

