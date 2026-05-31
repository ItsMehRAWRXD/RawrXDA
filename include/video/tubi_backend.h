#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace rawrxd::video
{

struct TubiRenderRequest
{
    std::string jobId;
    std::string engineName;
    std::string provider;
    std::string localModel;
    std::string prompt;
    std::string storyboard;
    std::string style;
    std::string duration;
    std::string aspectRatio;
    std::string resolution;
    std::string negativePrompt;
    std::string cameraMode;
    int seed = 0;
    std::filesystem::path outputDir;
};

struct TubiRenderResult
{
    int width = 0;
    int height = 0;
    int fps = 0;
    int totalFrames = 0;
    int durationSeconds = 0;
    int shotCount = 0;
    int seedUsed = 0;
    std::filesystem::path framesDir;
    std::filesystem::path manifestPath;
    std::filesystem::path mp4Path;
    std::filesystem::path progressPath;
    std::filesystem::path shotPlanPath;
    std::filesystem::path contactSheetPath;
    std::filesystem::path previewStartPath;
    std::filesystem::path previewMidPath;
    std::filesystem::path previewEndPath;
    std::string encoderDiagnostics;
    std::string extractedTags;
    bool mp4Created = false;
};

std::expected<TubiRenderResult, std::string> renderVideoClip(const TubiRenderRequest& request);

}  // namespace rawrxd::video
