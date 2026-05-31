#include "video/tubi_backend.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

extern "C" int RawrXD_TubiFillFrameRGBA(unsigned char* dst, unsigned int width, unsigned int height,
                                        unsigned int stride, unsigned int frameIndex, unsigned int totalFrames,
                                        unsigned int seed, unsigned int flags);

namespace rawrxd::video
{
namespace
{

struct ShotBeat
{
    std::string text;
    int startFrame = 0;
    int endFrame = 0;
};

std::string jsonEscape(const std::string& value)
{
    std::string out;
    out.reserve(value.size() + 16);
    for (const char ch : value)
    {
        switch (ch)
        {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out;
}

std::string toLowerCopy(std::string value)
{
    for (char& ch : value)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

bool endsWith(const std::string& value, const char* suffix)
{
    const size_t suffixLen = std::strlen(suffix);
    return value.size() >= suffixLen && value.compare(value.size() - suffixLen, suffixLen, suffix) == 0;
}

unsigned int hashPromptSeed(const TubiRenderRequest& request)
{
    const std::string seedText = request.prompt + "|" + request.storyboard + "|" + request.style + "|" +
                                 request.localModel + "|" + request.negativePrompt + "|" + request.cameraMode;
    unsigned int hash = 2166136261u;
    for (const unsigned char ch : seedText)
    {
        hash ^= ch;
        hash *= 16777619u;
    }
    return hash ? hash : 0xA53C9E11u;
}

std::string trimCopy(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
        ++start;
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
        --end;
    return value.substr(start, end - start);
}

void replaceAll(std::string& text, const std::string& needle, const std::string& replacement)
{
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos)
    {
        text.replace(pos, needle.size(), replacement);
        pos += replacement.size();
    }
}

std::vector<std::string> splitStoryboardBeats(const std::string& storyboard)
{
    std::string normalized = storyboard;
    replaceAll(normalized, "\r\n", "\n");
    replaceAll(normalized, ";", "\n");
    replaceAll(normalized, "|", "\n");
    std::vector<std::string> beats;
    size_t cursor = 0;
    while (cursor <= normalized.size())
    {
        const size_t next = normalized.find('\n', cursor);
        const std::string beat =
            trimCopy(normalized.substr(cursor, next == std::string::npos ? std::string::npos : next - cursor));
        if (!beat.empty())
            beats.push_back(beat);
        if (next == std::string::npos)
            break;
        cursor = next + 1;
    }
    if (beats.empty() && !trimCopy(storyboard).empty())
        beats.push_back(trimCopy(storyboard));
    return beats;
}

std::vector<ShotBeat> buildShotPlan(const TubiRenderRequest& request, int totalFrames)
{
    std::vector<std::string> beats = splitStoryboardBeats(request.storyboard);
    if (beats.empty())
        beats.push_back(trimCopy(request.prompt));

    std::vector<ShotBeat> shots;
    shots.reserve(beats.size());
    int cursor = 0;
    for (size_t i = 0; i < beats.size(); ++i)
    {
        const int remainingFrames = totalFrames - cursor;
        const int remainingShots = static_cast<int>(beats.size() - i);
        const int shotFrames = std::max(1, remainingFrames / std::max(1, remainingShots));
        ShotBeat beat;
        beat.text = beats[i];
        beat.startFrame = cursor;
        beat.endFrame =
            (i + 1 == beats.size()) ? std::max(cursor, totalFrames - 1) : std::max(cursor, cursor + shotFrames - 1);
        cursor = beat.endFrame + 1;
        shots.push_back(beat);
    }
    return shots;
}

std::string extractPromptTags(const TubiRenderRequest& request)
{
    const std::string prompt = toLowerCopy(request.prompt + " " + request.storyboard + " " + request.style + " " +
                                           request.cameraMode + " " + request.negativePrompt);
    std::vector<std::string> tags;
    const auto addTag = [&tags](const std::string& tag)
    {
        if (std::find(tags.begin(), tags.end(), tag) == tags.end())
            tags.push_back(tag);
    };

    if (prompt.find("castlevania") != std::string::npos)
        addTag("castlevania");
    if (prompt.find("gothic") != std::string::npos || prompt.find("castle") != std::string::npos)
        addTag("gothic");
    if (prompt.find("vampire") != std::string::npos || prompt.find("moon") != std::string::npos)
        addTag("night");
    if (prompt.find("torch") != std::string::npos || prompt.find("fire") != std::string::npos)
        addTag("firelight");
    if (prompt.find("battle") != std::string::npos || prompt.find("fight") != std::string::npos)
        addTag("action");
    if (prompt.find("cinematic") != std::string::npos)
        addTag("cinematic");
    if (prompt.find("pixel") != std::string::npos)
        addTag("pixel-art");
    if (prompt.find("anime") != std::string::npos)
        addTag("anime");

    std::ostringstream joined;
    for (size_t i = 0; i < tags.size(); ++i)
    {
        if (i)
            joined << ", ";
        joined << tags[i];
    }
    return joined.str();
}

int parseDurationSeconds(std::string value)
{
    value = toLowerCopy(value);
    if (value.empty())
        return 5;

    int multiplier = 1;
    if (endsWith(value, "ms"))
    {
        multiplier = 0;
    }
    else if (endsWith(value, "s"))
    {
        value.pop_back();
    }
    else if (endsWith(value, "m"))
    {
        value.pop_back();
        multiplier = 60;
    }
    else if (endsWith(value, "h"))
    {
        value.pop_back();
        multiplier = 3600;
    }

    int numeric = 0;
    for (const char ch : value)
    {
        if (!std::isdigit(static_cast<unsigned char>(ch)))
            break;
        numeric = (numeric * 10) + (ch - '0');
    }
    if (numeric <= 0)
        numeric = 5;
    if (multiplier == 0)
        return 1;
    return numeric * multiplier;
}

std::pair<int, int> parseAspectRatio(const std::string& aspectRatio)
{
    int a = 16;
    int b = 9;
    if (std::sscanf(aspectRatio.c_str(), "%d:%d", &a, &b) != 2 || a <= 0 || b <= 0)
        return {16, 9};
    return {a, b};
}

int parseVerticalResolution(const std::string& resolution)
{
    const std::string value = toLowerCopy(resolution);
    if (value == "320p")
        return 320;
    if (value == "480p")
        return 480;
    if (value == "720p")
        return 720;
    if (value == "1080p")
        return 1080;
    if (value == "1440p")
        return 1440;
    if (value == "4k")
        return 2160;
    return 720;
}

std::pair<int, int> computeDimensions(const std::string& resolution, const std::string& aspectRatio)
{
    const int vertical = parseVerticalResolution(resolution);
    const auto [num, den] = parseAspectRatio(aspectRatio);

    if (num >= den)
    {
        const int width = std::max(2, (vertical * num) / den);
        return {width, vertical};
    }

    const int width = vertical;
    const int height = std::max(2, (width * den) / num);
    return {width, height};
}

int selectFrameRate(int durationSeconds)
{
    if (durationSeconds <= 30)
        return 12;
    if (durationSeconds <= 120)
        return 10;
    if (durationSeconds <= 600)
        return 6;
    return 4;
}

int selectTotalFrames(int durationSeconds, int fps)
{
    const long long requestedFrames = static_cast<long long>(durationSeconds) * static_cast<long long>(fps);
    constexpr int MAX_RENDERED_FRAMES = 900;
    return static_cast<int>(std::clamp<long long>(requestedFrames, 12, MAX_RENDERED_FRAMES));
}

void overlayRect(std::vector<std::uint8_t>& rgba, int width, int height, int x0, int y0, int x1, int y1, std::uint8_t r,
                 std::uint8_t g, std::uint8_t b)
{
    x0 = std::clamp(x0, 0, width);
    x1 = std::clamp(x1, 0, width);
    y0 = std::clamp(y0, 0, height);
    y1 = std::clamp(y1, 0, height);
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            const size_t idx = static_cast<size_t>((y * width) + x) * 4u;
            rgba[idx + 0] = r;
            rgba[idx + 1] = g;
            rgba[idx + 2] = b;
            rgba[idx + 3] = 255;
        }
    }
}

void overlayCircle(std::vector<std::uint8_t>& rgba, int width, int height, int cx, int cy, int radius, std::uint8_t r,
                   std::uint8_t g, std::uint8_t b)
{
    const int rr = radius * radius;
    const int x0 = std::max(0, cx - radius);
    const int x1 = std::min(width, cx + radius);
    const int y0 = std::max(0, cy - radius);
    const int y1 = std::min(height, cy + radius);
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            const int dx = x - cx;
            const int dy = y - cy;
            if ((dx * dx) + (dy * dy) > rr)
                continue;
            const size_t idx = static_cast<size_t>((y * width) + x) * 4u;
            rgba[idx + 0] = static_cast<std::uint8_t>(std::min<int>(255, rgba[idx + 0] / 2 + r / 2));
            rgba[idx + 1] = static_cast<std::uint8_t>(std::min<int>(255, rgba[idx + 1] / 2 + g / 2));
            rgba[idx + 2] = static_cast<std::uint8_t>(std::min<int>(255, rgba[idx + 2] / 2 + b / 2));
            rgba[idx + 3] = 255;
        }
    }
}

void applyPromptOverlays(std::vector<std::uint8_t>& rgba, int width, int height, int frameIndex, int totalFrames,
                         const TubiRenderRequest& request)
{
    const std::string prompt =
        toLowerCopy(request.prompt + " " + request.storyboard + " " + request.style + " " + request.cameraMode);
    const std::string negativePrompt = toLowerCopy(request.negativePrompt);
    const bool gothic = prompt.find("castlevania") != std::string::npos || prompt.find("gothic") != std::string::npos ||
                        prompt.find("vampire") != std::string::npos || prompt.find("castle") != std::string::npos;
    if (!gothic)
        return;

    const int horizon = (height * 62) / 100;
    overlayRect(rgba, width, height, 0, horizon, width, height, 8, 6, 12);

    const int cameraShift = prompt.find("dolly") != std::string::npos || prompt.find("pan") != std::string::npos
                                ? ((frameIndex * std::max(1, width / 220)) % std::max(8, width / 12)) - (width / 24)
                                : 0;
    const int castleX = width / 2 - width / 7 + cameraShift;
    const int castleW = width / 3;
    const int castleTop = height / 4;
    overlayRect(rgba, width, height, castleX, castleTop, castleX + castleW, horizon, 18, 14, 22);
    overlayRect(rgba, width, height, castleX - width / 14, height / 6, castleX, horizon, 14, 10, 18);
    overlayRect(rgba, width, height, castleX + castleW, height / 5, castleX + castleW + width / 12, horizon, 14, 10,
                18);

    for (int i = 0; i < 10; ++i)
    {
        const int wx = castleX + 16 + (i * castleW) / 10;
        const int wy = castleTop + 24 + ((i % 3) * 48);
        overlayRect(rgba, width, height, wx, wy, wx + 10, wy + 18, 230, 152, 44);
    }

    const int moonX = width - width / 5 - cameraShift / 3;
    const int moonY = height / 5;
    const int moonPulse = 10 + ((frameIndex * 17) % std::max(16, totalFrames));
    if (negativePrompt.find("moon") == std::string::npos)
        overlayCircle(rgba, width, height, moonX, moonY, width / 18 + moonPulse / 4, 214, 224, 255);

    const int torchX = castleX + castleW / 2;
    const int torchY = horizon - 20;
    const int flame = 8 + ((frameIndex * 7) % 18);
    if (negativePrompt.find("fire") == std::string::npos && negativePrompt.find("torch") == std::string::npos)
        overlayCircle(rgba, width, height, torchX, torchY, flame, 255, 108, 32);
}

std::expected<void, std::string> writeBmp32(const std::filesystem::path& path, const std::vector<std::uint8_t>& rgba,
                                            int width, int height)
{
    if (width <= 0 || height <= 0)
        return std::unexpected("invalid BMP dimensions");

    BITMAPFILEHEADER fileHeader{};
    BITMAPINFOHEADER infoHeader{};
    const DWORD pixelBytes = static_cast<DWORD>(rgba.size());
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + pixelBytes;
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = -height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = pixelBytes;

    std::ofstream out(path, std::ios::binary);
    if (!out)
        return std::unexpected("failed to create BMP file");

    out.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    out.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));

    std::vector<std::uint8_t> bgra(rgba.size());
    for (size_t i = 0; i + 3 < rgba.size(); i += 4)
    {
        bgra[i + 0] = rgba[i + 2];
        bgra[i + 1] = rgba[i + 1];
        bgra[i + 2] = rgba[i + 0];
        bgra[i + 3] = rgba[i + 3];
    }
    out.write(reinterpret_cast<const char*>(bgra.data()), static_cast<std::streamsize>(bgra.size()));
    return {};
}

void writeProgressFile(const std::filesystem::path& path, const std::string& phase, int currentFrame, int totalFrames,
                       const std::string& note)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return;
    const int percent = totalFrames > 0 ? std::clamp((currentFrame * 100) / totalFrames, 0, 100) : 0;
    out << "{\n"
        << "  \"phase\": \"" << jsonEscape(phase) << "\",\n"
        << "  \"current_frame\": " << currentFrame << ",\n"
        << "  \"total_frames\": " << totalFrames << ",\n"
        << "  \"percent\": " << percent << ",\n"
        << "  \"note\": \"" << jsonEscape(note) << "\"\n"
        << "}\n";
}

std::expected<void, std::string> copyFileLossy(const std::filesystem::path& from, const std::filesystem::path& to)
{
    std::error_code ec;
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
        return std::unexpected("failed to copy file: " + from.string() + " -> " + to.string() + " (" + ec.message() +
                               ")");
    return {};
}

std::expected<std::filesystem::path, std::string> writeContactSheet(
    const std::filesystem::path& outputDir, const std::vector<std::filesystem::path>& framePaths, int frameWidth,
    int frameHeight)
{
    if (framePaths.empty())
        return std::unexpected("no frames available for contact sheet");

    const int columns = static_cast<int>(framePaths.size());
    const int rows = 1;
    const int padding = 4;
    const int sheetWidth = (frameWidth * columns) + (padding * (columns + 1));
    const int sheetHeight = (frameHeight * rows) + (padding * (rows + 1));
    std::vector<std::uint8_t> sheet(static_cast<size_t>(sheetWidth) * static_cast<size_t>(sheetHeight) * 4u, 22);

    for (size_t i = 0; i < sheet.size(); i += 4)
        sheet[i + 3] = 255;

    for (size_t index = 0; index < framePaths.size(); ++index)
    {
        std::ifstream in(framePaths[index], std::ios::binary);
        if (!in)
            return std::unexpected("failed to open preview frame for contact sheet");

        BITMAPFILEHEADER fileHeader{};
        BITMAPINFOHEADER infoHeader{};
        in.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
        in.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
        if (infoHeader.biBitCount != 32)
            return std::unexpected("contact sheet currently expects 32-bit preview BMP files");

        std::vector<std::uint8_t> bgra(static_cast<size_t>(frameWidth) * static_cast<size_t>(frameHeight) * 4u);
        in.read(reinterpret_cast<char*>(bgra.data()), static_cast<std::streamsize>(bgra.size()));

        const int dstX = padding + static_cast<int>(index) * (frameWidth + padding);
        const int dstY = padding;
        for (int y = 0; y < frameHeight; ++y)
        {
            for (int x = 0; x < frameWidth; ++x)
            {
                const size_t srcIdx = static_cast<size_t>((y * frameWidth) + x) * 4u;
                const size_t dstIdx = static_cast<size_t>(((dstY + y) * sheetWidth) + (dstX + x)) * 4u;
                sheet[dstIdx + 0] = bgra[srcIdx + 2];
                sheet[dstIdx + 1] = bgra[srcIdx + 1];
                sheet[dstIdx + 2] = bgra[srcIdx + 0];
                sheet[dstIdx + 3] = 255;
            }
        }
    }

    const std::filesystem::path contactSheetPath = outputDir / "contact_sheet.bmp";
    auto writeResult = writeBmp32(contactSheetPath, sheet, sheetWidth, sheetHeight);
    if (!writeResult)
        return std::unexpected(writeResult.error());
    return contactSheetPath;
}

std::string quoteForCmd(const std::filesystem::path& path)
{
    return "\"" + path.string() + "\"";
}

bool findExecutableOnPath(const char* exeName, std::filesystem::path& outPath)
{
    char buffer[MAX_PATH] = {};
    DWORD result = SearchPathA(nullptr, exeName, nullptr, MAX_PATH, buffer, nullptr);
    if (result == 0 || result >= MAX_PATH)
        return false;
    outPath = buffer;
    return true;
}

bool addCandidateIfExists(const std::filesystem::path& candidate, std::filesystem::path& outPath)
{
    std::error_code ec;
    if (!candidate.empty() && std::filesystem::exists(candidate, ec) && !ec)
    {
        outPath = candidate;
        return true;
    }
    return false;
}

bool findExecutableFromEnv(const char* envName, std::filesystem::path& outPath)
{
    char buffer[4096] = {};
    const DWORD len = GetEnvironmentVariableA(envName, buffer, static_cast<DWORD>(std::size(buffer)));
    if (len == 0 || len >= std::size(buffer))
        return false;
    return addCandidateIfExists(std::filesystem::path(buffer), outPath);
}

bool findBundledFfmpeg(std::filesystem::path& outPath)
{
    if (findExecutableFromEnv("FFMPEG_PATH", outPath) || findExecutableFromEnv("RAWRXD_FFMPEG_PATH", outPath))
        return true;

    wchar_t moduleBuf[MAX_PATH] = {};
    const DWORD moduleLen = GetModuleFileNameW(nullptr, moduleBuf, MAX_PATH);
    std::vector<std::filesystem::path> candidates;
    if (moduleLen > 0 && moduleLen < MAX_PATH)
    {
        const std::filesystem::path modulePath(moduleBuf);
        const std::filesystem::path moduleDir = modulePath.parent_path();
        candidates.push_back(moduleDir / "ffmpeg.exe");
        candidates.push_back(moduleDir / "tools" / "ffmpeg.exe");
        candidates.push_back(moduleDir / "third_party" / "ffmpeg.exe");
        candidates.push_back(moduleDir.parent_path() / "tools" / "ffmpeg.exe");
    }

    candidates.push_back(std::filesystem::path("C:/ffmpeg/bin/ffmpeg.exe"));
    candidates.push_back(std::filesystem::path("C:/Program Files/ffmpeg/bin/ffmpeg.exe"));
    candidates.push_back(std::filesystem::path("C:/Program Files (x86)/ffmpeg/bin/ffmpeg.exe"));
    candidates.push_back(std::filesystem::path("C:/tools/ffmpeg/bin/ffmpeg.exe"));
    candidates.push_back(std::filesystem::path("D:/ffmpeg/bin/ffmpeg.exe"));
    candidates.push_back(std::filesystem::path("F:/ffmpeg/bin/ffmpeg.exe"));

    for (const auto& candidate : candidates)
    {
        if (addCandidateIfExists(candidate, outPath))
            return true;
    }
    return false;
}

bool tryEncodeMp4(const std::filesystem::path& framesDir, const std::filesystem::path& outputMp4, int fps,
                  std::string& diagnostics)
{
    std::filesystem::path ffmpegPath;
    if (!findExecutableOnPath("ffmpeg.exe", ffmpegPath) && !findExecutableOnPath("ffmpeg", ffmpegPath) &&
        !findBundledFfmpeg(ffmpegPath))
    {
        diagnostics = "ffmpeg not found on PATH or common install locations";
        return false;
    }

    const std::string command = quoteForCmd(ffmpegPath) + " -y -framerate " + std::to_string(fps) + " -i " +
                                quoteForCmd(framesDir / "frame_%05d.bmp") +
                                " -c:v libx264 -pix_fmt yuv420p -movflags +faststart " + quoteForCmd(outputMp4);
    std::vector<char> cmdLine(command.begin(), command.end());
    cmdLine.push_back('\0');

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        framesDir.string().c_str(), &si, &pi))
    {
        diagnostics = "CreateProcessA(ffmpeg) failed with " + std::to_string(GetLastError());
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (exitCode != 0)
    {
        diagnostics = "ffmpeg exited with code " + std::to_string(exitCode);
        return false;
    }

    diagnostics = "ok";
    return true;
}

}  // namespace

std::expected<TubiRenderResult, std::string> renderVideoClip(const TubiRenderRequest& request)
{
    if (request.outputDir.empty())
        return std::unexpected("outputDir is empty");

    std::error_code ec;
    std::filesystem::create_directories(request.outputDir, ec);
    if (ec)
        return std::unexpected("failed to create output directory: " + ec.message());

    const auto [width, height] = computeDimensions(request.resolution, request.aspectRatio);
    const int durationSeconds = parseDurationSeconds(request.duration);
    const int fps = selectFrameRate(durationSeconds);
    const int totalFrames = selectTotalFrames(durationSeconds, fps);
    const unsigned int seed = request.seed != 0 ? static_cast<unsigned int>(request.seed) : hashPromptSeed(request);
    const std::vector<ShotBeat> shots = buildShotPlan(request, totalFrames);
    const std::string extractedTags = extractPromptTags(request);
    const std::filesystem::path framesDir = request.outputDir / "frames";
    const std::filesystem::path progressPath = request.outputDir / "render_progress.json";
    const std::filesystem::path shotPlanPath = request.outputDir / "shot_plan.json";
    std::filesystem::create_directories(framesDir, ec);
    if (ec)
        return std::unexpected("failed to create frames directory: " + ec.message());

    writeProgressFile(progressPath, "initializing", 0, totalFrames, "Preparing tubi render backend");
    {
        std::ofstream shotsOut(shotPlanPath, std::ios::binary);
        shotsOut << "{\n"
                 << "  \"job_id\": \"" << jsonEscape(request.jobId) << "\",\n"
                 << "  \"camera_mode\": \"" << jsonEscape(request.cameraMode) << "\",\n"
                 << "  \"beats\": [\n";
        for (size_t i = 0; i < shots.size(); ++i)
        {
            shotsOut << "    {\"index\": " << i << ", \"start_frame\": " << shots[i].startFrame
                     << ", \"end_frame\": " << shots[i].endFrame << ", \"text\": \"" << jsonEscape(shots[i].text)
                     << "\"}";
            if (i + 1 != shots.size())
                shotsOut << ",";
            shotsOut << "\n";
        }
        shotsOut << "  ]\n"
                 << "}\n";
    }

    std::vector<std::uint8_t> frame(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);
    for (int frameIndex = 0; frameIndex < totalFrames; ++frameIndex)
    {
        const int asmResult =
            RawrXD_TubiFillFrameRGBA(frame.data(), static_cast<unsigned int>(width), static_cast<unsigned int>(height),
                                     static_cast<unsigned int>(width * 4), static_cast<unsigned int>(frameIndex),
                                     static_cast<unsigned int>(totalFrames), seed, 0u);
        if (asmResult == 0)
            return std::unexpected("MASM frame kernel failed");

        applyPromptOverlays(frame, width, height, frameIndex, totalFrames, request);

        char frameName[64] = {};
        std::snprintf(frameName, sizeof(frameName), "frame_%05d.bmp", frameIndex);
        const std::filesystem::path framePath = framesDir / frameName;
        auto bmpWrite = writeBmp32(framePath, frame, width, height);
        if (!bmpWrite)
            return std::unexpected(bmpWrite.error());

        if (frameIndex == 0 || frameIndex == totalFrames / 2 || frameIndex + 1 == totalFrames)
        {
            const std::string note = frameIndex == 0                 ? "Captured preview-start frame"
                                     : frameIndex == totalFrames / 2 ? "Captured preview-mid frame"
                                                                     : "Captured preview-end frame";
            writeProgressFile(progressPath, "rendering", frameIndex + 1, totalFrames, note);
        }
        else if (((frameIndex + 1) % std::max(1, totalFrames / 8)) == 0)
        {
            writeProgressFile(progressPath, "rendering", frameIndex + 1, totalFrames, "Generating frame sequence");
        }
    }

    TubiRenderResult result;
    result.width = width;
    result.height = height;
    result.fps = fps;
    result.totalFrames = totalFrames;
    result.durationSeconds = durationSeconds;
    result.shotCount = static_cast<int>(shots.size());
    result.seedUsed = static_cast<int>(seed);
    result.framesDir = framesDir;
    result.manifestPath = request.outputDir / "render_backend_manifest.json";
    result.mp4Path = request.outputDir / (request.jobId.empty() ? "tubi_output.mp4" : request.jobId + ".mp4");
    result.progressPath = progressPath;
    result.shotPlanPath = shotPlanPath;
    result.extractedTags = extractedTags;

    const std::filesystem::path firstFrame = framesDir / "frame_00000.bmp";
    char midFrameName[64] = {};
    std::snprintf(midFrameName, sizeof(midFrameName), "frame_%05d.bmp", totalFrames / 2);
    const std::filesystem::path midFrame = framesDir / midFrameName;
    char lastFrameName[64] = {};
    std::snprintf(lastFrameName, sizeof(lastFrameName), "frame_%05d.bmp", totalFrames - 1);
    const std::filesystem::path lastFrame = framesDir / lastFrameName;

    result.previewStartPath = request.outputDir / "preview_start.bmp";
    result.previewMidPath = request.outputDir / "preview_mid.bmp";
    result.previewEndPath = request.outputDir / "preview_end.bmp";
    auto copyStart = copyFileLossy(firstFrame, result.previewStartPath);
    if (!copyStart)
        return std::unexpected(copyStart.error());
    auto copyMid = copyFileLossy(midFrame, result.previewMidPath);
    if (!copyMid)
        return std::unexpected(copyMid.error());
    auto copyEnd = copyFileLossy(lastFrame, result.previewEndPath);
    if (!copyEnd)
        return std::unexpected(copyEnd.error());

    auto contactSheet = writeContactSheet(
        request.outputDir, {result.previewStartPath, result.previewMidPath, result.previewEndPath}, width, height);
    if (!contactSheet)
        return std::unexpected(contactSheet.error());
    result.contactSheetPath = *contactSheet;

    std::string ffmpegDiagnostics;
    writeProgressFile(progressPath, "encoding", totalFrames, totalFrames, "Attempting MP4 mux");
    result.mp4Created = tryEncodeMp4(framesDir, result.mp4Path, fps, ffmpegDiagnostics);
    result.encoderDiagnostics = ffmpegDiagnostics;
    writeProgressFile(progressPath, result.mp4Created ? "completed" : "completed-without-mp4", totalFrames, totalFrames,
                      result.mp4Created ? "MP4 created" : ffmpegDiagnostics);

    {
        std::ofstream manifest(result.manifestPath, std::ios::binary);
        manifest << "{\n"
                 << "  \"job_id\": \"" << jsonEscape(request.jobId) << "\",\n"
                 << "  \"engine\": \"" << jsonEscape(request.engineName) << "\",\n"
                 << "  \"provider\": \"" << jsonEscape(request.provider) << "\",\n"
                 << "  \"local_model\": \"" << jsonEscape(request.localModel) << "\",\n"
                 << "  \"prompt\": \"" << jsonEscape(request.prompt) << "\",\n"
                 << "  \"negative_prompt\": \"" << jsonEscape(request.negativePrompt) << "\",\n"
                 << "  \"storyboard\": \"" << jsonEscape(request.storyboard) << "\",\n"
                 << "  \"style\": \"" << jsonEscape(request.style) << "\",\n"
                 << "  \"camera_mode\": \"" << jsonEscape(request.cameraMode) << "\",\n"
                 << "  \"duration\": \"" << jsonEscape(request.duration) << "\",\n"
                 << "  \"duration_seconds\": " << result.durationSeconds << ",\n"
                 << "  \"aspect_ratio\": \"" << jsonEscape(request.aspectRatio) << "\",\n"
                 << "  \"resolution\": \"" << jsonEscape(request.resolution) << "\",\n"
                 << "  \"seed\": " << result.seedUsed << ",\n"
                 << "  \"shot_count\": " << result.shotCount << ",\n"
                 << "  \"tags\": \"" << jsonEscape(result.extractedTags) << "\",\n"
                 << "  \"render_width\": " << result.width << ",\n"
                 << "  \"render_height\": " << result.height << ",\n"
                 << "  \"fps\": " << result.fps << ",\n"
                 << "  \"rendered_frames\": " << result.totalFrames << ",\n"
                 << "  \"frames_dir\": \"" << jsonEscape(result.framesDir.string()) << "\",\n"
                 << "  \"progress_path\": \"" << jsonEscape(result.progressPath.string()) << "\",\n"
                 << "  \"shot_plan_path\": \"" << jsonEscape(result.shotPlanPath.string()) << "\",\n"
                 << "  \"preview_start\": \"" << jsonEscape(result.previewStartPath.string()) << "\",\n"
                 << "  \"preview_mid\": \"" << jsonEscape(result.previewMidPath.string()) << "\",\n"
                 << "  \"preview_end\": \"" << jsonEscape(result.previewEndPath.string()) << "\",\n"
                 << "  \"contact_sheet\": \"" << jsonEscape(result.contactSheetPath.string()) << "\",\n"
                 << "  \"mp4_path\": \"" << jsonEscape(result.mp4Path.string()) << "\",\n"
                 << "  \"mp4_created\": " << (result.mp4Created ? "true" : "false") << ",\n"
                 << "  \"encoder\": \"" << jsonEscape(result.encoderDiagnostics) << "\"\n"
                 << "}\n";
    }

    return result;
}

}  // namespace rawrxd::video
