#include "video/tubi_backend.h"

#include <windows.h>

#include <cstdlib>
#include <string>
#include <unordered_map>

namespace
{

void writeConsoleLine(HANDLE handle, const std::string& line)
{
    DWORD written = 0;
    std::string output = line + "\n";
    WriteFile(handle, output.data(), static_cast<DWORD>(output.size()), &written, nullptr);
}

bool hasArg(const std::unordered_map<std::string, std::string>& args, const std::string& key)
{
    return args.find(key) != args.end();
}

}  // namespace

int main(int argc, char** argv)
{
    std::unordered_map<std::string, std::string> args;
    for (int i = 1; i + 1 < argc; i += 2)
        args[argv[i]] = argv[i + 1];

    rawrxd::video::TubiRenderRequest request;
    request.jobId = args["--job-id"];
    request.engineName = hasArg(args, "--engine") ? args["--engine"] : "tubi";
    request.provider = args["--provider"];
    request.localModel = args["--model"];
    request.prompt = args["--prompt"];
    request.storyboard = args["--storyboard"];
    request.style = hasArg(args, "--style") ? args["--style"] : "cinematic-gothic";
    request.duration = hasArg(args, "--duration") ? args["--duration"] : "5s";
    request.aspectRatio = hasArg(args, "--aspect") ? args["--aspect"] : "16:9";
    request.resolution = hasArg(args, "--resolution") ? args["--resolution"] : "720p";
    request.negativePrompt = hasArg(args, "--negative-prompt") ? args["--negative-prompt"] : "";
    request.cameraMode = hasArg(args, "--camera") ? args["--camera"] : "cinematic-pan";
    request.seed = hasArg(args, "--seed") ? std::atoi(args["--seed"].c_str()) : 0;
    request.outputDir = hasArg(args, "--out-dir") ? args["--out-dir"] : "video-out";

    HANDLE outHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE errHandle = GetStdHandle(STD_ERROR_HANDLE);

    const auto result = rawrxd::video::renderVideoClip(request);
    if (!result)
    {
        writeConsoleLine(errHandle, result.error());
        return 1;
    }

    writeConsoleLine(outHandle, "frames=" + std::to_string(result->totalFrames));
    writeConsoleLine(outHandle, "fps=" + std::to_string(result->fps));
    writeConsoleLine(outHandle, "size=" + std::to_string(result->width) + "x" + std::to_string(result->height));
    writeConsoleLine(outHandle, "manifest=" + result->manifestPath.string());
    writeConsoleLine(outHandle, "progress=" + result->progressPath.string());
    writeConsoleLine(outHandle, "shot_plan=" + result->shotPlanPath.string());
    writeConsoleLine(outHandle, "contact_sheet=" + result->contactSheetPath.string());
    writeConsoleLine(outHandle, "mp4=" + result->mp4Path.string());
    writeConsoleLine(outHandle, std::string("mp4_created=") + (result->mp4Created ? "true" : "false"));
    return 0;
}
