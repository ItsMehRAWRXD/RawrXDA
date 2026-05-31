// =============================================================================
// ToolRegistry.cpp — X-Macro Tool Registry Implementation
// =============================================================================
#include "ToolRegistry.h"
#include "AgentToolHandlers.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <unordered_set>

#include "../../include/multimodal/vision_encoder.h"
#include "../core/rawrxd_subsystem_api.hpp"
#include "../core/unified_hotpatch_manager.hpp"
#include "../modules/game_engine_manager.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h> // IStream for GDI+
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#else
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// Windows <wingdi.h> defines ERROR as 0 which clashes with LogLevel::ERROR
#ifdef ERROR
#undef ERROR
#endif
#include "../../include/debug/ai_debugger.h"
#include "../agent/DiskRecoveryAgent.h"
#include "../core/amd_gpu_accelerator.h"
#include "../core/native_debugger_engine.h"
#include "../p2p/SystemIntegrityProver.h"
#include "DiskRecoveryAgent.h"
#include "SovereignAssembler.h"
#include "agentic_observability.h"
#include <intrin.h>

namespace RawrXD
{
namespace Agent
{

static AgenticObservability& GetObs()
{
    static AgenticObservability instance;
    return instance;
}
static const char* kRegistryComponent = "ToolRegistry";

using RawrXD::Agent::AgentToolRegistry;
using RawrXD::Agent::ToolDescriptor;
using RawrXD::Agent::ToolExecResult;

// Forward declarations — implemented in ToolRegistry_HardwareSecurity_Batch1.cpp (same enclosing namespace).
ToolExecResult HandleGetGpuTelemetry(const json& args);
ToolExecResult HandleTuneVramLimit(const json& args);
ToolExecResult HandleBenchKernel(const json& args);
ToolExecResult HandleCreateMemorySilo(const json& args);
ToolExecResult HandleQuerySiloStats(const json& args);
ToolExecResult HandleSetSiloQuota(const json& args);
ToolExecResult HandleMapModelAperture(const json& args);
ToolExecResult HandleQueryVirtualMemory(const json& args);
ToolExecResult HandleManageLocalEmbeddings(const json& args);
ToolExecResult HandlePurgeTelemetry(const json& args);

namespace
{

const char* InternToolRegistryString(const std::string& value)
{
    static std::vector<std::unique_ptr<std::string>> storage;
    storage.emplace_back(std::make_unique<std::string>(value));
    return storage.back()->c_str();
}

void EnsureDescriptorFromHandlerSchema(const json& fn,
                                       std::vector<ToolDescriptor>& tools,
                                       std::unordered_map<std::string, size_t>& nameIndex)
{
    const std::string name = fn.value("name", "");
    if (name.empty())
    {
        return;
    }
    if (nameIndex.find(name) != nameIndex.end())
    {
        return;
    }

    ToolDescriptor td{};
    td.name = InternToolRegistryString(name);
    const std::string description = fn.value("description", std::string());
    td.description = InternToolRegistryString(description.empty() ? ("Bridged AgentToolHandlers tool: " + name)
                                                                  : description);
    td.params_schema = json::object();
    td.handler = nullptr;
    tools.push_back(std::move(td));
    nameIndex[name] = tools.size() - 1;
}

json BuildRegistryParamSchemaFromHandlerSchema(const json& fn)
{
    json rebuilt = json::object();
    if (!fn.contains("parameters") || !fn["parameters"].is_object())
    {
        return rebuilt;
    }

    const auto& params = fn["parameters"];
    const json props = params.value("properties", json::object());
    json required = params.value("required", json::array());

    std::unordered_set<std::string> requiredSet;
    if (required.is_array())
    {
        for (const auto& r : required)
        {
            if (r.is_string())
            {
                requiredSet.insert(r.get<std::string>());
            }
        }
    }

    if (!props.is_object())
    {
        return rebuilt;
    }

    for (auto pit = props.begin(); pit != props.end(); ++pit)
    {
        json p = json::object();
        const json& in = pit.value();
        p["type"] = in.value("type", "string");
        p["description"] = in.value("description", "");
        if (requiredSet.find(pit.key()) == requiredSet.end())
        {
            p["default"] = nullptr;
        }
        rebuilt[pit.key()] = p;
    }

    return rebuilt;
}

std::string ToLowerAscii(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string ExtractCommandBinary(const std::string& command)
{
    size_t i = 0;
    while (i < command.size() && std::isspace(static_cast<unsigned char>(command[i])))
    {
        ++i;
    }
    if (i >= command.size())
    {
        return {};
    }

    std::string token;
    if (command[i] == '"')
    {
        ++i;
        while (i < command.size() && command[i] != '"')
        {
            token.push_back(command[i++]);
        }
    }
    else
    {
        while (i < command.size() && !std::isspace(static_cast<unsigned char>(command[i])))
        {
            token.push_back(command[i++]);
        }
    }

    const size_t slashPos = token.find_last_of("/\\");
    if (slashPos != std::string::npos)
    {
        token = token.substr(slashPos + 1);
    }
    return ToLowerAscii(token);
}

bool IsAllowedCommandBinary(const std::string& binary)
{
    static const std::array<const char*, 13> kAllowed = {"git",       "git.exe", "cmake",  "cmake.exe", "ninja",
                                                         "ninja.exe", "cl",      "cl.exe", "ctest",     "ctest.exe",
                                                         "rg",        "rg.exe",  "pwsh"};

    for (const char* allowed : kAllowed)
    {
        if (binary == allowed)
        {
            return true;
        }
    }
    return false;
}

bool LooksDestructiveCommand(const std::string& command)
{
    const std::string lower = ToLowerAscii(command);
    static const std::array<const char*, 15> kDangerousFragments = {
        " rm -rf",        "rm -rf ",       " del /f /s /q", " rmdir /s /q", " rd /s /q", " git reset --hard",
        " git clean -fd", " format ",      " diskpart",     " shutdown /s", " mkfs",     " dd if=",
        "cipher /w",      "takeown /f c:", ":(){:|:&};:"};

    for (const char* fragment : kDangerousFragments)
    {
        if (lower.find(fragment) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

std::string NormalizeToolName(const std::string& raw)
{
    if (raw.empty())
        return {};

    // Normalize separators + camelCase -> snake_case
    std::string normalized;
    normalized.reserve(raw.size() + 8);
    bool lastWasUnderscore = false;
    for (char c : raw)
    {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc))
        {
            if (std::isupper(uc) && !normalized.empty() && normalized.back() != '_')
            {
                normalized.push_back('_');
            }
            normalized.push_back(static_cast<char>(std::tolower(uc)));
            lastWasUnderscore = false;
        }
        else
        {
            if (!normalized.empty() && !lastWasUnderscore)
            {
                normalized.push_back('_');
                lastWasUnderscore = true;
            }
        }
    }

    while (!normalized.empty() && normalized.front() == '_')
        normalized.erase(normalized.begin());
    while (!normalized.empty() && normalized.back() == '_')
        normalized.pop_back();

    // Legacy aliases and common model outputs
    if (normalized == "readfile")
        return "read_file";
    if (normalized == "edit_file" || normalized == "editfile")
        return "replace_in_file";
    if (normalized == "writefile")
        return "write_file";
    if (normalized == "replacefile" || normalized == "replaceinfile")
        return "replace_in_file";
    if (normalized == "run_terminal" || normalized == "runterminal" || normalized == "terminal")
        return "execute_command";
    if (normalized == "executecommand" || normalized == "runcommand")
        return "execute_command";
    if (normalized == "search" || normalized == "grep" || normalized == "searchcode")
        return "search_code";
    if (normalized == "diagnostics" || normalized == "getdiagnostics")
        return "get_diagnostics";
    if (normalized == "list_dir")
        return "list_directory";
    if (normalized == "listdir" || normalized == "listdirectory")
        return "list_directory";
    if (normalized == "coverage" || normalized == "getcoverage")
        return "get_coverage";
    if (normalized == "build" || normalized == "runbuild")
        return "run_build";
    if (normalized == "hotpatch" || normalized == "applyhotpatch")
        return "apply_hotpatch";
    if (normalized == "diskrecovery" || normalized == "recoverdisk")
        return "disk_recovery";
    if (normalized == "ghprlist" || normalized == "listpullrequests" || normalized == "list_pull_requests")
        return "gh_pr_list";
    if (normalized == "ghprview" || normalized == "viewpullrequest" || normalized == "view_pull_request")
        return "gh_pr_view";
    if (normalized == "ghissuelist" || normalized == "listissues" || normalized == "list_issues")
        return "gh_issue_list";
    if (normalized == "ghissueview" || normalized == "viewissue" || normalized == "view_issue")
        return "gh_issue_view";
    if (normalized == "gh_create_pr")
        return "gh_create_pr";
    if (normalized == "ghprchecks" || normalized == "prchecks" || normalized == "pullrequestchecks")
        return "gh_pr_checks";
    if (normalized == "ghprdiff" || normalized == "prdiff" || normalized == "pullrequestdiff")
        return "gh_pr_diff";
    if (normalized == "ghprreview" || normalized == "prreview" || normalized == "reviewpullrequest")
        return "gh_pr_review";
    if (normalized == "ghprcomment" || normalized == "prcomment" || normalized == "commentpullrequest")
        return "gh_pr_comment";
    if (normalized == "ghprmerge" || normalized == "prmerge" || normalized == "mergepullrequest")
        return "gh_pr_merge";
    if (normalized == "multifileplan" || normalized == "refactorplan")
        return "propose_multifile_edits";
    if (normalized == "multifilediff" || normalized == "refactorpreview")
        return "preview_multifile_diff";
    if (normalized == "multifileapply" || normalized == "refactorapply")
        return "apply_multifile_edits";
    if (normalized == "renamesymbol" || normalized == "refactorrename" || normalized == "rename_identifier")
        return "refactor_rename_symbol";
    if (normalized == "debugstatus" || normalized == "debug_state")
        return "debug_status";
    if (normalized == "debugmodules" || normalized == "listdebugmodules")
        return "debug_modules";
    if (normalized == "debugdetach" || normalized == "detachdebugger")
        return "debug_detach";
    if (normalized == "debugterminate" || normalized == "terminate_debuggee")
        return "debug_terminate";
    if (normalized == "take_screenshot" || normalized == "capture_screenshot" || normalized == "screen_capture")
        return "screenshot";
    if (normalized == "memory" || normalized == "memoryfile" || normalized == "agentmemory" ||
        normalized == "agent_memory" || normalized == "memories")
        return "memory_file";

    return normalized;
}

// -----------------------------------------------------------------------
// Default tool handlers (stubs — real implementations wire into engine)
// -----------------------------------------------------------------------

ToolExecResult HandleScreenshot(const json& args);
ToolExecResult HandleToolRegistrySelfCheck(const json& args);
ToolExecResult HandleIngameFrameCheck(const json& args);

#ifdef _WIN32
bool EnsureGdiplusStarted()
{
    static std::once_flag s_once;
    static bool s_ok = false;
    std::call_once(s_once,
                   []()
                   {
                       Gdiplus::GdiplusStartupInput input;
                       ULONG_PTR token = 0;
                       const auto st = Gdiplus::GdiplusStartup(&token, &input, nullptr);
                       s_ok = (st == Gdiplus::Ok);
                       (void)token;  // Deliberately leak token for process lifetime.
                   });
    return s_ok;
}

bool GetPngEncoderClsid(CLSID& outClsid)
{
    using namespace Gdiplus;
    UINT num = 0, size = 0;
    if (GetImageEncodersSize(&num, &size) != Ok || size == 0)
        return false;

    std::vector<uint8_t> buf(size);
    ImageCodecInfo* pInfo = reinterpret_cast<ImageCodecInfo*>(buf.data());
    if (GetImageEncoders(num, size, pInfo) != Ok)
        return false;

    for (UINT i = 0; i < num; ++i)
    {
        if (pInfo[i].MimeType && wcscmp(pInfo[i].MimeType, L"image/png") == 0)
        {
            outClsid = pInfo[i].Clsid;
            return true;
        }
    }
    return false;
}

static ToolExecResult CaptureToPngFile(const std::wstring& outPath, const std::string& mode, int x, int y, int width,
                                       int height)
{
    if (!EnsureGdiplusStarted())
        return ToolExecResult::error("GDI+ init failed");

    if (width <= 0 || height <= 0)
    {
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
    }
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen)
        return ToolExecResult::error("Cannot get screen DC");

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem)
    {
        ReleaseDC(nullptr, hdcScreen);
        return ToolExecResult::error("Cannot create compatible DC");
    }

    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, width, height);
    if (!hBmp)
    {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return ToolExecResult::error("Cannot create bitmap");
    }

    HGDIOBJ oldObj = SelectObject(hdcMem, hBmp);

    bool ok = false;
    if (ToLowerAscii(mode) == "active_window")
    {
        HWND hwnd = GetForegroundWindow();
        if (hwnd)
        {
            RECT rc{};
            if (GetWindowRect(hwnd, &rc))
            {
                const int w = (std::max)(1, static_cast<int>(rc.right - rc.left));
                const int h = (std::max)(1, static_cast<int>(rc.bottom - rc.top));
                // Capture the window area into our bitmap, scaled/cropped to requested width/height.
                // Prefer PrintWindow; fallback to BitBlt from screen.
                HDC hdcWin = CreateCompatibleDC(hdcScreen);
                HBITMAP hWinBmp = CreateCompatibleBitmap(hdcScreen, w, h);
                if (hdcWin && hWinBmp)
                {
                    HGDIOBJ oldWin = SelectObject(hdcWin, hWinBmp);
                    if (PrintWindow(hwnd, hdcWin, PW_RENDERFULLCONTENT) != 0)
                    {
                        StretchBlt(hdcMem, 0, 0, width, height, hdcWin, 0, 0, w, h, SRCCOPY);
                        ok = true;
                    }
                    SelectObject(hdcWin, oldWin);
                }
                if (!ok)
                {
                    // Fallback to screen copy.
                    StretchBlt(hdcMem, 0, 0, width, height, hdcScreen, rc.left, rc.top, w, h, SRCCOPY);
                    ok = true;
                }
                if (hWinBmp)
                    DeleteObject(hWinBmp);
                if (hdcWin)
                    DeleteDC(hdcWin);
            }
        }
    }
    else
    {
        // Desktop (or rect crop via x,y)
        ok = (BitBlt(hdcMem, 0, 0, width, height, hdcScreen, x, y, SRCCOPY) != 0);
    }

    SelectObject(hdcMem, oldObj);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    if (!ok)
    {
        DeleteObject(hBmp);
        return ToolExecResult::error("Screenshot capture failed");
    }

    CLSID pngClsid{};
    if (!GetPngEncoderClsid(pngClsid))
    {
        DeleteObject(hBmp);
        return ToolExecResult::error("PNG encoder not available");
    }

    Gdiplus::Bitmap bmp(hBmp, nullptr);
    DeleteObject(hBmp);

    if (bmp.GetLastStatus() != Gdiplus::Ok)
        return ToolExecResult::error("Failed to wrap bitmap");

    const auto st = bmp.Save(outPath.c_str(), &pngClsid, nullptr);
    if (st != Gdiplus::Ok)
        return ToolExecResult::error("Failed to save PNG");

    json out;
    out["path"] = std::string(outPath.begin(), outPath.end());
    out["width"] = width;
    out["height"] = height;
    out["mode"] = mode;
    return ToolExecResult::ok(out.dump());
}
#endif

ToolExecResult HandleReadFile(const json& args)
{
    std::string path = args.value("path", "");
    if (path.empty())
        return ToolExecResult::error("Missing required parameter: path");

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open())
        return ToolExecResult::error("Failed to open file: " + path);

    std::ostringstream oss;
    oss << ifs.rdbuf();
    return ToolExecResult::ok(oss.str());
}

ToolExecResult HandleScreenshot(const json& args)
{
#ifndef _WIN32
    (void)args;
    return ToolExecResult::error("screenshot is only implemented on Windows");
#else
    const std::string mode = args.value("mode", "desktop");
    const int x = args.value("x", 0);
    const int y = args.value("y", 0);
    const int width = args.value("width", 0);
    const int height = args.value("height", 0);

    std::string outPathUtf8 = args.value("path", "");
    if (outPathUtf8.empty())
    {
        char tmpPath[MAX_PATH]{};
        DWORD n = GetTempPathA(MAX_PATH, tmpPath);
        std::string base = (n > 0 && n < MAX_PATH) ? std::string(tmpPath) : std::string(".\\");
        SYSTEMTIME st{};
        GetLocalTime(&st);
        std::ostringstream oss;
        oss << base << "rawrxd_screenshot_" << st.wYear << std::setw(2) << std::setfill('0') << st.wMonth
            << std::setw(2) << std::setfill('0') << st.wDay << "_" << std::setw(2) << std::setfill('0') << st.wHour
            << std::setw(2) << std::setfill('0') << st.wMinute << std::setw(2) << std::setfill('0') << st.wSecond
            << ".png";
        outPathUtf8 = oss.str();
    }

    std::wstring outPathW(outPathUtf8.begin(), outPathUtf8.end());
    return CaptureToPngFile(outPathW, mode, x, y, width, height);
#endif
}

ToolExecResult HandleToolRegistrySelfCheck(const json& args)
{
    (void)args;
    auto& reg = AgentToolRegistry::Instance();
    const auto missing = reg.GetToolsMissingHandlers();
    const auto tools = reg.ListTools();
    std::unordered_set<std::string> registryTools(tools.begin(), tools.end());

    json missingExposed = json::array();
    try
    {
        const json handlerSchemas = AgentToolHandlers::GetAllSchemas();
        for (const auto& schema : handlerSchemas)
        {
            if (!schema.is_object() || schema.value("type", "") != "function" || !schema.contains("function") ||
                !schema["function"].is_object())
            {
                continue;
            }

            const std::string name = schema["function"].value("name", "");
            if (name.empty())
            {
                continue;
            }
            if (registryTools.find(name) == registryTools.end())
            {
                missingExposed.push_back(name);
            }
        }
    }
    catch (...)
    {
    }

    json out;
    out["ok"] = missing.empty() && missingExposed.empty();
    out["tool_count"] = static_cast<int>(tools.size());
    out["missing_handlers"] = missing;
    out["missing_exposed_tools"] = missingExposed;
    return ToolExecResult::ok(out.dump());
}

ToolExecResult HandleIngameFrameCheck(const json& args)
{
#ifndef _WIN32
    (void)args;
    return ToolExecResult::error("ingame_frame_check is only implemented on Windows");
#else
    const std::string mode = args.value("mode", "active_window");  // better default for games
    const bool includeProfiler = args.value("include_profiler", true);
    const bool captureFresh = args.value("capture", true);
    const std::string inputPath = args.value("path", "");

    std::string pngPath;
    if (captureFresh)
    {
        json shotArgs = args;
        shotArgs["mode"] = mode;
        ToolExecResult shot = HandleScreenshot(shotArgs);
        if (!shot.success)
            return shot;
        try
        {
            auto j = json::parse(shot.output);
            pngPath = j.value("path", "");
        }
        catch (...)
        {
            return ToolExecResult::error("Failed to parse screenshot tool output");
        }
    }
    else
    {
        if (inputPath.empty())
            return ToolExecResult::error("ingame_frame_check: set capture=true or provide path");
        pngPath = inputPath;
    }

    // Use VisionEncoder to capture a stable raw frame representation.
    RawrXD::Multimodal::VisionEncoder enc;
    auto loaded = enc.LoadFromFile(pngPath);
    if (!loaded.success || !loaded.image.isValid())
    {
        return ToolExecResult::error(std::string("Failed to load frame: ") + loaded.error);
    }

    // Heuristic z-fighting / clipping score:
    // We approximate "high-frequency alternating pixels" by scanning BMP/PNG raw bytes when available.
    // Since ImageData::rawBytes is file bytes (compressed for PNG), we rely on GDI+ decode for pixels.
    if (!EnsureGdiplusStarted())
        return ToolExecResult::error("GDI+ init failed");

    std::wstring wpath(pngPath.begin(), pngPath.end());
    Gdiplus::Bitmap bmp(wpath.c_str());
    if (bmp.GetLastStatus() != Gdiplus::Ok)
        return ToolExecResult::error("Failed to decode PNG via GDI+");

    const int w = static_cast<int>(bmp.GetWidth());
    const int h = static_cast<int>(bmp.GetHeight());
    if (w <= 4 || h <= 4)
        return ToolExecResult::error("Frame too small for analysis");

    Gdiplus::Rect rc(0, 0, w, h);
    Gdiplus::BitmapData bd{};
    if (bmp.LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok)
        return ToolExecResult::error("Failed to lock frame pixels");

    auto* p = static_cast<const uint8_t*>(bd.Scan0);
    const int stride = bd.Stride;
    const int stepX = std::max(1, w / 256);
    const int stepY = std::max(1, h / 256);

    uint64_t samples = 0;
    uint64_t hiFreqEdges = 0;
    uint64_t alternating = 0;

    auto pix = [&](int xx, int yy) -> uint32_t
    {
        const uint8_t* row = p + yy * stride;
        return *reinterpret_cast<const uint32_t*>(row + xx * 4);
    };
    auto luma = [&](uint32_t argb) -> int
    {
        const int b = (argb) & 0xFF;
        const int g = (argb >> 8) & 0xFF;
        const int r = (argb >> 16) & 0xFF;
        // integer approx of 0.299r + 0.587g + 0.114b
        return (r * 77 + g * 150 + b * 29) >> 8;
    };

    for (int yy = stepY; yy < h - stepY; yy += stepY)
    {
        for (int xx = stepX; xx < w - stepX; xx += stepX)
        {
            const int a = luma(pix(xx - stepX, yy));
            const int b0 = luma(pix(xx, yy));
            const int c = luma(pix(xx + stepX, yy));

            const int d1 = std::abs(b0 - a);
            const int d2 = std::abs(c - b0);
            const int edge = d1 + d2;
            if (edge > 80)  // strong local contrast change
                hiFreqEdges++;

            // Alternating pattern (a high, b low, c high) or (low, high, low)
            if ((a > b0 + 40 && c > b0 + 40) || (b0 > a + 40 && b0 > c + 40))
                alternating++;

            samples++;
        }
    }

    bmp.UnlockBits(&bd);

    const double edgeRate = samples ? (double)hiFreqEdges / (double)samples : 0.0;
    const double altRate = samples ? (double)alternating / (double)samples : 0.0;
    const double zfightScore = std::min(1.0, (edgeRate * 0.6 + altRate * 1.2));

    json out;
    out["path"] = pngPath;
    out["width"] = w;
    out["height"] = h;
    out["metrics"] = {{"samples", static_cast<uint64_t>(samples)},
                      {"hi_freq_edge_rate", edgeRate},
                      {"alternating_rate", altRate},
                      {"z_fighting_score_0_1", zfightScore}};
    out["diagnosis"] = (zfightScore > 0.35) ? "possible_z_fighting_or_clipping" : "no_strong_clipping_signal_detected";

    if (includeProfiler && RawrXD::GameEngine::g_gameEngineManager &&
        RawrXD::GameEngine::g_gameEngineManager->isInitialized())
    {
        auto snap = RawrXD::GameEngine::g_gameEngineManager->getProfilerSnapshot();
        out["engine_profiler"] = {{"engine", static_cast<int>(snap.engine)},
                                  {"frameTimeMs", snap.frameTimeMs},
                                  {"cpuTimeMs", snap.cpuTimeMs},
                                  {"gpuTimeMs", snap.gpuTimeMs},
                                  {"drawCalls", snap.drawCalls},
                                  {"triangles", snap.triangles},
                                  {"fps", snap.fps},
                                  {"totalMemoryMB", snap.totalMemoryMB}};
    }
    else
    {
        out["engine_profiler"] = nullptr;
    }

    return ToolExecResult::ok(out.dump());
#endif
}

ToolExecResult HandleWriteFile(const json& args)
{
    std::string path = args.value("path", "");
    std::string content = args.value("content", "");
    if (path.empty())
        return ToolExecResult::error("Missing required parameter: path");

    // Ensure parent directories exist
    std::filesystem::path p(path);
    if (p.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
        return ToolExecResult::error("Failed to create file: " + path);

    ofs.write(content.data(), content.size());
    ofs.close();
    return ToolExecResult::ok("File written: " + path + " (" + std::to_string(content.size()) + " bytes)");
}

ToolExecResult HandleReplaceInFile(const json& args)
{
    std::string path = args.value("path", "");
    std::string oldStr = args.value("old_string", "");
    std::string newStr = args.value("new_string", "");
    if (path.empty() || oldStr.empty())
        return ToolExecResult::error("Missing required parameters: path, old_string");

    // Read
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open())
        return ToolExecResult::error("Failed to open file: " + path);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    // Find & replace (first occurrence)
    size_t pos = content.find(oldStr);
    if (pos == std::string::npos)
        return ToolExecResult::error("old_string not found in " + path);

    content.replace(pos, oldStr.size(), newStr);

    // Write
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
        return ToolExecResult::error("Failed to write file: " + path);
    ofs.write(content.data(), content.size());
    ofs.close();

    return ToolExecResult::ok("Replaced in " + path + " at offset " + std::to_string(pos));
}

ToolExecResult HandleExecuteCommand(const json& args)
{
    std::string command = args.value("command", "");
    int timeout_ms = args.value("timeout", 30000);
    bool allowUnsafe = args.value("allow_unsafe", false);
    if (command.empty())
        return ToolExecResult::error("Missing required parameter: command");

    const std::string binary = ExtractCommandBinary(command);
    if (binary.empty())
        return ToolExecResult::error("execute_command rejected: empty binary token");

    if (!IsAllowedCommandBinary(binary))
    {
        return ToolExecResult::error("execute_command rejected: binary not allowlisted: " + binary);
    }

    if (LooksDestructiveCommand(command))
    {
        const char* unsafeEnv = std::getenv("RAWRXD_TOOL_EXEC_ALLOW_UNSAFE");
        const bool envAllowsUnsafe = unsafeEnv && std::string(unsafeEnv) == "1";
        if (!allowUnsafe && !envAllowsUnsafe)
        {
            return ToolExecResult::error(
                "execute_command blocked by SafetyGovernor: destructive pattern detected; requires modal approval "
                "(allow_unsafe=true or RAWRXD_TOOL_EXEC_ALLOW_UNSAFE=1)");
        }
    }

#ifdef _WIN32
    // CreateProcess with captured output
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        return ToolExecResult::error("Failed to create pipe");

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    PROCESS_INFORMATION pi{};
    std::string cmdline = "cmd.exe /c " + command;

    auto start = std::chrono::high_resolution_clock::now();

    BOOL created =
        CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    CloseHandle(hWritePipe);

    if (!created)
    {
        CloseHandle(hReadPipe);
        return ToolExecResult::error("CreateProcess failed: " + std::to_string(GetLastError()));
    }

    // Read output
    std::string output;
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
    {
        output.append(buffer, bytesRead);
        // Truncate if excessively large
        if (output.size() > 1024 * 1024)
        {
            output += "\n... [output truncated at 1MB]";
            break;
        }
    }
    CloseHandle(hReadPipe);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeout_ms));
    DWORD exitCode = 0;
    if (waitResult == WAIT_TIMEOUT)
    {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return ToolExecResult::error("Command timed out after " + std::to_string(timeout_ms) + "ms\n" + output, 1);
    }

    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    ToolExecResult result;
    result.success = (exitCode == 0);
    result.output = output;
    result.exit_code = static_cast<int>(exitCode);
    result.elapsed_ms = elapsed;
    return result;
#else
    // POSIX: fork/exec with pipe capture
    int pipefd[2];
    if (pipe(pipefd) != 0)
        return ToolExecResult::error("Failed to create pipe");

    auto start = std::chrono::high_resolution_clock::now();

    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return ToolExecResult::error("fork() failed");
    }

    if (pid == 0)
    {
        // Child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    // Parent
    close(pipefd[1]);

    std::string output;
    char buffer[4096];
    ssize_t bytesRead;

    // Non-blocking read with timeout
    struct pollfd pfd;
    pfd.fd = pipefd[0];
    pfd.events = POLLIN;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true)
    {
        auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0)
        {
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            close(pipefd[0]);
            return ToolExecResult::error("Command timed out after " + std::to_string(timeout_ms) + "ms\n" + output, 1);
        }

        int ready = poll(&pfd, 1, static_cast<int>(std::min(remaining, (long long)1000)));
        if (ready > 0)
        {
            bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1);
            if (bytesRead <= 0)
                break;
            buffer[bytesRead] = '\0';
            output += buffer;
            if (output.size() > 1024 * 1024)
            {
                output += "\n... [output truncated at 1MB]";
                break;
            }
        }
        else if (ready == 0)
        {
            // Check if child exited
            int status;
            pid_t wp = waitpid(pid, &status, WNOHANG);
            if (wp == pid)
                break;
        }
        else
        {
            break;
        }
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1;

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    ToolExecResult result;
    result.success = (exitCode == 0);
    result.output = output;
    result.exit_code = exitCode;
    result.elapsed_ms = elapsed;
    return result;
#endif
}

ToolExecResult HandleSearchCode(const json& args)
{
    std::string query = args.value("query", "");
    std::string pattern = args.value("file_pattern", "*.*");
    bool is_regex = args.value("is_regex", false);
    if (query.empty())
        return ToolExecResult::error("Missing required parameter: query");

    std::ostringstream results;
    int matchCount = 0;
    const int maxMatches = 50;

    // Build glob extension filter from pattern
    std::vector<std::string> extensions;
    if (pattern != "*.*" && pattern != "*")
    {
        // Extract extension from pattern like "*.cpp" or "*.{cpp,h}"
        auto dotPos = pattern.find('.');
        if (dotPos != std::string::npos)
        {
            std::string ext = pattern.substr(dotPos);
            extensions.push_back(ext);
        }
    }

    auto matchesPattern = [&](const std::filesystem::path& p) -> bool
    {
        if (extensions.empty())
            return true;
        std::string ext = p.extension().string();
        for (const auto& e : extensions)
        {
            if (ext == e)
                return true;
        }
        return false;
    };

    // Recursive search through current directory
    std::error_code ec;
    for (auto& entry : std::filesystem::recursive_directory_iterator(
             ".", std::filesystem::directory_options::skip_permission_denied, ec))
    {
        std::error_code entryEc;
        if (!entry.is_regular_file(entryEc))
            continue;
        if (!matchesPattern(entry.path()))
            continue;
        if (matchCount >= maxMatches)
        {
            results << "\n... (truncated at " << maxMatches << " matches)\n";
            break;
        }

        // Read file and search for query
        std::ifstream ifs(entry.path(), std::ios::binary);
        if (!ifs.is_open())
            continue;

        std::string line;
        int lineNum = 0;
        while (std::getline(ifs, line))
        {
            lineNum++;
            size_t pos = std::string::npos;
            if (is_regex)
            {
                // Simple substring match as regex fallback
                pos = line.find(query);
            }
            else
            {
                pos = line.find(query);
            }

            if (pos != std::string::npos)
            {
                results << entry.path().string() << ":" << lineNum << ": ";
                // Truncate long lines
                if (line.size() > 200)
                {
                    size_t start = (pos > 80) ? pos - 80 : 0;
                    results << "..." << line.substr(start, 200) << "...";
                }
                else
                {
                    results << line;
                }
                results << "\n";
                matchCount++;
                if (matchCount >= maxMatches)
                    break;
            }
        }
    }

    if (matchCount == 0)
    {
        results << "No matches found for \"" << query << "\"";
        if (!extensions.empty())
            results << " in " << pattern << " files";
        results << "\n";
    }
    else
    {
        results << "\n" << matchCount << " match(es) found.\n";
    }

    return ToolExecResult::ok(results.str());
}

ToolExecResult HandleGetDiagnostics(const json& args)
{
    std::string file = args.value("file", "");

    auto& registry = SubsystemRegistry::instance();

    // Attempt to retrieve diagnostics from LSP subsystem
    if (registry.isAvailable(SubsystemId::LSPDiagnostics))
    {
        SubsystemParams params{};
        params.id = SubsystemId::LSPDiagnostics;
        SubsystemResult result = registry.invoke(params);
        if (result.success && result.detail)
        {
            return ToolExecResult::ok(result.detail);
        }
    }

    // Fallback: run compiler and parse output for diagnostics
    if (!file.empty())
    {
        // Check if file is a C++ source — run a quick syntax check
        std::filesystem::path p(file);
        std::string ext = p.extension().string();
        if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc")
        {
            // Run cl.exe /Zs (syntax check only) to capture diagnostics
            json execArgs;
            execArgs["command"] = "cl.exe /Zs /EHsc /std:c++20 /W4 \"" + file + "\" 2>&1";
            execArgs["timeout"] = 15000;
            auto r = HandleExecuteCommand(execArgs);
            if (!r.output.empty())
            {
                return ToolExecResult::ok("[diagnostics] " + file + ":\n" + r.output);
            }
        }
    }

    return ToolExecResult::ok("[get_diagnostics] No diagnostics available for: " +
                              (file.empty() ? "(all files)" : file));
}

ToolExecResult HandleListDirectory(const json& args)
{
    std::string path = args.value("path", ".");
    bool recursive = args.value("recursive", false);
    if (path.empty())
        path = ".";

    std::ostringstream oss;
    std::error_code ec;

    try
    {
        if (recursive)
        {
            for (auto& entry : std::filesystem::recursive_directory_iterator(
                     path, std::filesystem::directory_options::skip_permission_denied, ec))
            {
                std::error_code entryEc;
                const bool isDirectory = entry.is_directory(entryEc);
                if (entryEc)
                {
                    continue;
                }
                oss << (isDirectory ? "[DIR]  " : "[FILE] ") << entry.path().string() << "\n";
            }
        }
        else
        {
            for (auto& entry : std::filesystem::directory_iterator(
                     path, std::filesystem::directory_options::skip_permission_denied, ec))
            {
                std::error_code entryEc;
                const bool isDirectory = entry.is_directory(entryEc);
                if (entryEc)
                {
                    continue;
                }
                oss << (isDirectory ? "[DIR]  " : "[FILE] ") << entry.path().filename().string() << "\n";
            }
        }
    }
    catch (const std::filesystem::filesystem_error& ex)
    {
        return ToolExecResult::error(std::string("Directory listing failed: ") + ex.what());
    }

    if (ec)
        return ToolExecResult::error("Directory listing failed: " + ec.message());
    return ToolExecResult::ok(oss.str());
}

ToolExecResult HandleGetCoverage(const json& args)
{
    std::string file = args.value("file", "");
    std::string func = args.value("function_name", "");
    std::string mode = args.value("mode", "diffcov");  // "bbcov" or "diffcov"

    auto& registry = SubsystemRegistry::instance();

    // Dispatch to BBCov (basic-block coverage) or DiffCov (differential coverage)
    SubsystemId targetMode = (mode == "bbcov") ? SubsystemId::BBCov : SubsystemId::DiffCov;

    if (!registry.isAvailable(targetMode))
    {
        return ToolExecResult::error("[get_coverage] " + mode + " subsystem not available");
    }

    SubsystemParams params{};
    params.id = targetMode;
    SubsystemResult result = registry.invoke(params);

    if (!result.success)
    {
        return ToolExecResult::error(std::string("[get_coverage] ") + mode +
                                         " failed: " + (result.detail ? result.detail : "unknown error"),
                                     result.errorCode);
    }

    std::ostringstream oss;
    oss << "[get_coverage] " << mode << " analysis complete";
    if (result.artifactPath)
    {
        oss << " — artifact: " << result.artifactPath;
    }
    oss << " (" << result.latencyMs << "ms)";
    if (!file.empty())
        oss << " [filter: " << file << "]";
    if (!func.empty())
        oss << " [function: " << func << "]";

    return ToolExecResult::ok(oss.str());
}

ToolExecResult HandleRunBuild(const json& args)
{
    std::string target = args.value("target", "all");
    std::string config = args.value("config", "Release");

    std::string cmd = "cmake --build . --config " + config;
    if (target != "all")
        cmd += " --target " + target;

    json execArgs;
    execArgs["command"] = cmd;
    execArgs["timeout"] = 120000;
    return HandleExecuteCommand(execArgs);
}

ToolExecResult HandleAsmAssemble(const json& args)
{
    std::string source = args.value("source", "");
    std::string output = args.value("output", "agent_output.exe");
    if (source.empty())
    {
        return ToolExecResult::error("No assembly source provided");
    }
    std::wstring wout(output.begin(), output.end());
    std::string error;
    if (!SovereignAssembler::AssembleAndLink(source, wout, error))
    {
        return ToolExecResult::error("Assembly failed: " + error);
    }
    // optional test-run (sandboxed)
    if (args.value("test_run", false))
    {
        STARTUPINFOW si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        if (CreateProcessW(wout.c_str(), NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
        {
            TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    return ToolExecResult::ok("Assembled successfully: " + output);
}

ToolExecResult HandleApplyHotpatch(const json& args)
{
    std::string layer = args.value("layer", "");
    std::string target = args.value("target", "");
    std::string data = args.value("data", "");
    if (layer.empty() || target.empty())
        return ToolExecResult::error("Missing required parameters: layer, target");

    // Pre-flight integrity gate: patching is a sovereign operation
    if (!SystemIntegrityProver::Instance().RunBeforeCriticalOp("apply_hotpatch"))
        return ToolExecResult::error("Integrity pre-flight FAILED: apply_hotpatch blocked.");

    auto& manager = UnifiedHotpatchManager::instance();
    UnifiedResult ur;

    if (layer == "memory")
    {
        // Parse target as hex address, data as hex bytes
        uintptr_t addr = 0;
        sscanf(target.c_str(), "%llx", &addr);
        if (addr == 0)
            return ToolExecResult::error("Invalid memory address: " + target);

        // Decode hex data string into byte array
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i + 1 < data.size(); i += 2)
        {
            uint8_t byte = 0;
            sscanf(data.c_str() + i, "%2hhx", &byte);
            bytes.push_back(byte);
        }
        if (bytes.empty())
            return ToolExecResult::error("No patch data provided");

        ur = manager.apply_memory_patch(reinterpret_cast<void*>(addr), bytes.size(), bytes.data());
    }
    else if (layer == "byte")
    {
        // target = filename, data = hex-encoded patch
        std::vector<uint8_t> pattern, replacement;
        // data format: "PATTERN:REPLACEMENT" in hex
        auto colonPos = data.find(':');
        if (colonPos == std::string::npos)
            return ToolExecResult::error("byte layer data must be PATTERN_HEX:REPLACEMENT_HEX");

        std::string patternHex = data.substr(0, colonPos);
        std::string replaceHex = data.substr(colonPos + 1);

        for (size_t i = 0; i + 1 < patternHex.size(); i += 2)
        {
            uint8_t b;
            sscanf(patternHex.c_str() + i, "%2hhx", &b);
            pattern.push_back(b);
        }
        for (size_t i = 0; i + 1 < replaceHex.size(); i += 2)
        {
            uint8_t b;
            sscanf(replaceHex.c_str() + i, "%2hhx", &b);
            replacement.push_back(b);
        }

        ur = manager.apply_byte_search_patch(target.c_str(), pattern, replacement);
    }
    else if (layer == "server")
    {
        // Server patches are registered by name, not via hex data
        // target = patch name to add/remove
        if (data == "remove")
        {
            ur = manager.remove_server_patch(target.c_str());
        }
        else
        {
            return ToolExecResult::error("Server patches must be registered programmatically. "
                                         "Use target=name, data=remove to remove.");
        }
    }
    else
    {
        return ToolExecResult::error("Unknown layer: " + layer + ". Valid: memory, byte, server");
    }

    if (!ur.result.success)
    {
        const char* detail = ur.result.detail.empty() ? "unknown error" : ur.result.detail.c_str();
        return ToolExecResult::error(std::string("[apply_hotpatch] ") + layer + " layer failed: " + detail,
                                     ur.result.errorCode);
    }

    std::ostringstream oss;
    oss << "[apply_hotpatch] " << layer << " layer applied successfully"
        << " | target=" << target << " | seq=" << ur.sequenceId;
    return ToolExecResult::ok(oss.str());
}

ToolExecResult HandleDiskRecovery(const json& args)
{
    std::string action = args.value("action", "");
    if (action.empty())
        return ToolExecResult::error("Missing required parameter: action");

    // Thread-local agent instance (persistent across calls within a session)
    static RawrXD::Recovery::DiskRecoveryAsmAgent agent;

    if (action == "scan" || action == "find")
    {
        int driveNum = agent.FindDrive();
        if (driveNum < 0)
            return ToolExecResult::error("No dying WD device found on PhysicalDrive0-15");
        return ToolExecResult::ok("Found candidate: PhysicalDrive" + std::to_string(driveNum));
    }
    else if (action == "init" || action == "initialize")
    {
        int driveNum = args.value("drive", -1);
        if (driveNum < 0)
            return ToolExecResult::error("Missing parameter: drive (0-15)");
        auto r = agent.Initialize(driveNum);
        if (!r.success)
            return ToolExecResult::error(r.detail, r.errorCode);
        return ToolExecResult::ok(r.detail);
    }
    else if (action == "extract_key" || action == "key")
    {
        auto r = agent.ExtractEncryptionKey();
        if (!r.success)
            return ToolExecResult::error(r.detail, r.errorCode);
        return ToolExecResult::ok(r.detail);
    }
    else if (action == "run" || action == "recover")
    {
        auto r = agent.RunRecovery();
        if (!r.success)
            return ToolExecResult::error(r.detail, r.errorCode);
        return ToolExecResult::ok(r.detail);
    }
    else if (action == "abort" || action == "stop")
    {
        agent.Abort();
        return ToolExecResult::ok("Abort signal sent to recovery worker");
    }
    else if (action == "stats" || action == "status")
    {
        auto stats = agent.GetStats();
        std::ostringstream oss;
        oss << "Good: " << stats.goodSectors << " | Bad: " << stats.badSectors << " | Current LBA: " << stats.currentLBA
            << " / " << stats.totalSectors << " (" << static_cast<int>(stats.ProgressPercent()) << "%)";
        return ToolExecResult::ok(oss.str());
    }
    else if (action == "cleanup" || action == "close")
    {
        // Destructor handles cleanup via RAII, but explicit reset:
        agent = RawrXD::Recovery::DiskRecoveryAsmAgent();
        return ToolExecResult::ok("Recovery context cleaned up");
    }
    else if (action == "carve")
    {
        // Pre-flight: every carve operation must pass full integrity attestation.
        // Physical (AVX-512), W^X policy, binary-on-disk, logic, and EventBus
        // are all verified before we touch the disk image.
        if (!SystemIntegrityProver::Instance().RunBeforeCriticalOp("carve"))
        {
            return ToolExecResult::error("Integrity pre-flight FAILED: carve operation blocked. "
                                         "Check AVX-512 availability, W^X policy, and EventBus health.");
        }

        std::string imagePath = args.value("image_path", "");
        std::string outputDir = args.value("output_dir", "");
        if (imagePath.empty())
            return ToolExecResult::error("Missing parameter: image_path (path to raw disk image)");
        if (outputDir.empty())
            return ToolExecResult::error("Missing parameter: output_dir (directory for carved files)");
        size_t maxFiles = args.value("max_files", 256);

        RawrXD::Recovery::DiskRecoveryAgent carver;
        auto r = carver.CarveKnownSignatures(imagePath, outputDir, maxFiles);
        if (!r.success)
            return ToolExecResult::error(r.detail, r.errorCode);
        return ToolExecResult::ok(r.detail);
    }
    else
    {
        return ToolExecResult::error("Unknown action: " + action +
                                     ". Valid: scan, init, extract_key, run, abort, stats, cleanup, carve");
    }
}

// -----------------------------------------------------------------------
// Git / GitHub handlers (bridge to AgentToolHandlers)
// -----------------------------------------------------------------------

ToolExecResult ToToolExecResult(const ToolCallResult& res)
{
    int exitCode = res.isSuccess() ? 0 : -1;
    if (res.metadata.is_object() && res.metadata.contains("exit_code") && res.metadata["exit_code"].is_number_integer())
    {
        exitCode = res.metadata["exit_code"].get<int>();
    }

    if (res.isSuccess())
    {
        return ToolExecResult::ok(res.output, static_cast<double>(res.durationMs));
    }

    std::string message = res.error;
    if (message.empty())
    {
        message = res.output.empty() ? "Tool execution failed" : res.output;
    }
    return ToolExecResult::error(message, exitCode);
}

ToolExecResult HandleGitStatus(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("git_status", args);
    return ToToolExecResult(res);
}

ToolExecResult HandleMemoryFile(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("memory_file", args);
    return ToToolExecResult(res);
}

ToolExecResult HandleGitDiff(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("git_diff", args);
    return ToToolExecResult(res);
}

ToolExecResult HandleGitCommit(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("git_commit", args);
    return ToToolExecResult(res);
}

ToolExecResult HandleGitHubIssueList(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("gh_issue_list", args);
    return ToToolExecResult(res);
}

ToolExecResult HandleGitHubIssueView(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("gh_issue_view", args);
    return ToToolExecResult(res);
}

ToolExecResult HandleGitHubPrList(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("gh_pr_list", args);
    return ToToolExecResult(res);
}

ToolExecResult HandleGitHubPrView(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("gh_pr_view", args);
    return ToToolExecResult(res);
}

ToolExecResult HandleGitHubCreatePR(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("gh_create_pr", args);
    return ToToolExecResult(res);
}

static std::string EscapeDoubleQuotedArg(const std::string& input)
{
    std::string out;
    out.reserve(input.size() + 8);
    for (char c : input)
    {
        if (c == '"' || c == '\\')
        {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

static ToolExecResult RunGhCommand(const std::string& command)
{
    json forwarded = json::object();
    forwarded["command"] = command;
    auto res = AgentToolHandlers::Instance().Execute("execute_command", forwarded);
    return ToToolExecResult(res);
}

ToolExecResult HandleGitHubPrChecks(const json& args)
{
    const std::string number = args.value("number", "");
    if (number.empty())
    {
        return ToolExecResult::error("gh_pr_checks requires 'number'");
    }
    const std::string command = "gh pr checks " + number;
    return RunGhCommand(command);
}

ToolExecResult HandleGitHubPrDiff(const json& args)
{
    const std::string number = args.value("number", "");
    if (number.empty())
    {
        return ToolExecResult::error("gh_pr_diff requires 'number'");
    }
    const std::string command = "gh pr diff " + number;
    return RunGhCommand(command);
}

ToolExecResult HandleGitHubPrReview(const json& args)
{
    const std::string number = args.value("number", "");
    if (number.empty())
    {
        return ToolExecResult::error("gh_pr_review requires 'number'");
    }

    const std::string event = args.value("event", "comment");
    const std::string body = args.value("body", "");
    std::string command = "gh pr review " + number;

    if (event == "approve")
    {
        command += " --approve";
    }
    else if (event == "request_changes")
    {
        command += " --request-changes";
    }
    else
    {
        command += " --comment";
    }

    if (!body.empty())
    {
        command += " --body \"" + EscapeDoubleQuotedArg(body) + "\"";
    }
    return RunGhCommand(command);
}

ToolExecResult HandleGitHubPrComment(const json& args)
{
    const std::string number = args.value("number", "");
    const std::string body = args.value("body", "");
    if (number.empty() || body.empty())
    {
        return ToolExecResult::error("gh_pr_comment requires 'number' and 'body'");
    }

    const std::string command = "gh pr comment " + number + " --body \"" + EscapeDoubleQuotedArg(body) + "\"";
    return RunGhCommand(command);
}

ToolExecResult HandleGitHubPrMerge(const json& args)
{
    const std::string number = args.value("number", "");
    if (number.empty())
    {
        return ToolExecResult::error("gh_pr_merge requires 'number'");
    }

    const std::string strategy = args.value("strategy", "squash");
    bool deleteBranch = args.value("delete_branch", true);

    std::string command = "gh pr merge " + number;
    if (strategy == "merge")
    {
        command += " --merge";
    }
    else if (strategy == "rebase")
    {
        command += " --rebase";
    }
    else
    {
        command += " --squash";
    }

    if (deleteBranch)
    {
        command += " --delete-branch";
    }
    return RunGhCommand(command);
}

ToolExecResult HandleProposeMultiFileEdits(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("propose_multifile_edits", args);
    return ToToolExecResult(res);
}

ToolExecResult HandlePreviewMultiFileDiff(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("preview_multifile_diff", args);
    return ToToolExecResult(res);
}

ToolExecResult HandleApplyMultiFileEdits(const json& args)
{
    auto res = AgentToolHandlers::Instance().Execute("apply_multifile_edits", args);
    return ToToolExecResult(res);
}

static bool IsWordChar(unsigned char ch)
{
    return std::isalnum(ch) || ch == '_';
}

static size_t ReplaceWholeWordInBuffer(std::string& content, const std::string& from, const std::string& to)
{
    if (from.empty())
    {
        return 0;
    }

    size_t replacements = 0;
    size_t pos = 0;
    while ((pos = content.find(from, pos)) != std::string::npos)
    {
        const bool leftOk = (pos == 0) || !IsWordChar(static_cast<unsigned char>(content[pos - 1]));
        const size_t after = pos + from.size();
        const bool rightOk = (after >= content.size()) || !IsWordChar(static_cast<unsigned char>(content[after]));
        if (leftOk && rightOk)
        {
            content.replace(pos, from.size(), to);
            pos += to.size();
            ++replacements;
            continue;
        }
        pos += from.size();
    }
    return replacements;
}

ToolExecResult HandleRefactorRenameSymbol(const json& args)
{
    const std::string oldSymbol = args.value("old_symbol", "");
    const std::string newSymbol = args.value("new_symbol", "");
    if (oldSymbol.empty() || newSymbol.empty())
    {
        return ToolExecResult::error("refactor_rename_symbol requires 'old_symbol' and 'new_symbol'");
    }

    const bool apply = args.value("apply", false);
    const std::string singlePath = args.value("path", "");
    std::string root = args.value("root", ".");
    const std::string filePattern = args.value("file_pattern", "");
    int maxFiles = args.value("max_files", 200);
    if (maxFiles < 1)
    {
        maxFiles = 1;
    }

    std::vector<std::filesystem::path> targets;
    std::error_code ec;
    if (!singlePath.empty())
    {
        targets.push_back(std::filesystem::path(singlePath));
    }
    else
    {
        for (auto it = std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied, ec);
             it != std::filesystem::recursive_directory_iterator(); ++it)
        {
            if (ec)
            {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file(ec))
            {
                ec.clear();
                continue;
            }

            const std::string pathStr = it->path().string();
            if (!filePattern.empty() && pathStr.find(filePattern) == std::string::npos)
            {
                continue;
            }
            targets.push_back(it->path());
            if (static_cast<int>(targets.size()) >= maxFiles)
            {
                break;
            }
        }
    }

    json report = json::object();
    report["old_symbol"] = oldSymbol;
    report["new_symbol"] = newSymbol;
    report["apply"] = apply;
    report["files_scanned"] = targets.size();
    report["files_changed"] = 0;
    report["replacements"] = 0;
    report["changed_files"] = json::array();

    for (const auto& path : targets)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open())
        {
            continue;
        }

        std::string original((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        std::string updated = original;
        const size_t replaced = ReplaceWholeWordInBuffer(updated, oldSymbol, newSymbol);
        if (replaced == 0)
        {
            continue;
        }

        if (apply)
        {
            std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open())
            {
                continue;
            }
            ofs.write(updated.data(), static_cast<std::streamsize>(updated.size()));
            ofs.close();
        }

        report["files_changed"] = report["files_changed"].get<int>() + 1;
        report["replacements"] = report["replacements"].get<int>() + static_cast<int>(replaced);
        report["changed_files"].push_back(path.string());
    }

    return ToolExecResult::ok(report.dump(2));
}

ToolExecResult HandleTermExec(const json& args)
{
    std::string command = args.value("command", "");
    std::string cwd = args.value("cwd", "");
    int timeout_ms = args.value("timeout", 60000);

    if (command.empty())
        return ToolExecResult::error("Missing required parameter: command");

    auto start = std::chrono::high_resolution_clock::now();

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        return ToolExecResult::error("Failed to create pipe");

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    PROCESS_INFORMATION pi{};
    std::string cmdline = "cmd.exe /c " + command;

    BOOL created = CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                  cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);

    CloseHandle(hWritePipe);

    if (!created)
    {
        CloseHandle(hReadPipe);
        return ToolExecResult::error("CreateProcess failed: " + std::to_string(GetLastError()));
    }

    std::string output;
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
    {
        output.append(buffer, bytesRead);
        if (output.size() > 2 * 1024 * 1024)
        {
            output += "\n... [output truncated at 2MB]";
            break;
        }
    }
    CloseHandle(hReadPipe);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeout_ms));
    DWORD exitCode = 0;
    if (waitResult == WAIT_TIMEOUT)
    {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return ToolExecResult::error("TermExec timed out after " + std::to_string(timeout_ms) + "ms\n" + output, 1);
    }

    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    // POSIX Implementation
    int pipefd[2];
    if (pipe(pipefd) != 0)
        return ToolExecResult::error("Failed to create pipe");

    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return ToolExecResult::error("fork() failed");
    }

    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        if (!cwd.empty())
        {
            chdir(cwd.c_str());
        }
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    close(pipefd[1]);
    std::string output;
    char buffer[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0)
    {
        output.append(buffer, n);
        if (output.size() > 2 * 1024 * 1024)
            break;
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    ToolExecResult result;
    result.success = (exitCode == 0);
    result.output = output;
    result.exit_code = static_cast<int>(exitCode);
    result.elapsed_ms = elapsed;
    return result;
}

ToolExecResult HandleSysGetCapabilities(const json& args)
{
    (void)args;
    json res = json::object();

    // CPU Capabilities via __cpuid
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    bool hasAVX = (cpuInfo[2] & (1 << 28)) != 0;

    int cpuInfo7[4];
    __cpuidex(cpuInfo7, 7, 0);
    bool hasAVX2 = (cpuInfo7[1] & (1 << 5)) != 0;
    bool hasAVX512 = (cpuInfo7[1] & (1 << 16)) != 0;

    res["cpu"]["avx"] = hasAVX;
    res["cpu"]["avx2"] = hasAVX2;
    res["cpu"]["avx512"] = hasAVX512;
    res["cpu"]["architecture"] = "x64";

    // GPU Capabilities
    auto& accel = AMDGPUAccelerator::instance();
    res["gpu"]["initialized"] = accel.isInitialized();
    res["gpu"]["enabled"] = accel.isGPUEnabled();
    res["gpu"]["name"] = accel.getGPUName();
    res["gpu"]["vram_total_bytes"] = accel.getVRAMBytes();
    res["gpu"]["backend"] = accel.getBackendName();
    res["gpu"]["compute_units"] = accel.getComputeUnits();

    // Runtime Tier Suggestion
    std::string tier = "CPU_GENERIC";
    if (accel.isGPUEnabled() && accel.getVRAMBytes() > 4ULL * 1024 * 1024 * 1024)
    {
        tier = "VRAM_ACCELERATED";
    }
    else if (hasAVX512)
    {
        tier = "CPU_AVX512";
    }
    else if (hasAVX2)
    {
        tier = "CPU_AVX2";
    }
    res["suggested_execution_tier"] = tier;

    return ToolExecResult::ok(res.dump(2));
}

// ---------------------------------------------------------------------------
// Debug Tool Handlers — Wire NativeDebuggerEngine into agent tool system
// ---------------------------------------------------------------------------

ToolExecResult HandleDebugLaunch(const json& args)
{
    std::string exePath = args.value("path", "");
    if (exePath.empty())
        return ToolExecResult::error("Missing required parameter: path");
    std::string exeArgs = args.value("args", "");
    std::string workDir = args.value("working_dir", "");

    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    if (!engine.isInitialized())
    {
        RawrXD::Debugger::DebugConfig cfg;
        cfg.symbolPath = "srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols";
        cfg.enableSourceStepping = true;
        cfg.maxEventHistory = 500;
        auto r = engine.initialize(cfg);
        if (!r.success)
            return ToolExecResult::error(std::string("Engine init failed: ") + r.detail);
    }

    auto r = engine.launchProcess(exePath, exeArgs, workDir);
    if (!r.success)
        return ToolExecResult::error(std::string("Launch failed: ") + r.detail);

    json res;
    res["pid"] = engine.getTargetPID();
    res["state"] = "launched";
    res["target"] = exePath;
    return ToolExecResult::ok(res.dump(2));
}

ToolExecResult HandleDebugAttach(const json& args)
{
    int pid = args.value("pid", 0);
    if (pid <= 0)
        return ToolExecResult::error("Missing or invalid parameter: pid");

    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    if (!engine.isInitialized())
    {
        RawrXD::Debugger::DebugConfig cfg;
        cfg.symbolPath = "srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols";
        cfg.enableSourceStepping = true;
        auto r = engine.initialize(cfg);
        if (!r.success)
            return ToolExecResult::error(std::string("Engine init failed: ") + r.detail);
    }

    auto r = engine.attachToProcess(static_cast<uint32_t>(pid));
    if (!r.success)
        return ToolExecResult::error(std::string("Attach failed: ") + r.detail);

    json res;
    res["pid"] = pid;
    res["state"] = "attached";
    return ToolExecResult::ok(res.dump(2));
}

ToolExecResult HandleDebugBreak(const json& args)
{
    (void)args;
    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    auto r = engine.breakExecution();
    if (!r.success)
        return ToolExecResult::error(std::string("Break failed: ") + r.detail);
    return ToolExecResult::ok("Execution paused.");
}

ToolExecResult HandleDebugContinue(const json& args)
{
    (void)args;
    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    auto r = engine.go();
    if (!r.success)
        return ToolExecResult::error(std::string("Continue failed: ") + r.detail);
    return ToolExecResult::ok("Execution resumed.");
}

ToolExecResult HandleDebugStepOver(const json& args)
{
    (void)args;
    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    auto r = engine.stepOver();
    if (!r.success)
        return ToolExecResult::error(std::string("Step over failed: ") + r.detail);
    return ToolExecResult::ok(engine.toJsonStatus());
}

ToolExecResult HandleDebugStepInto(const json& args)
{
    (void)args;
    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    auto r = engine.stepInto();
    if (!r.success)
        return ToolExecResult::error(std::string("Step into failed: ") + r.detail);
    return ToolExecResult::ok(engine.toJsonStatus());
}

ToolExecResult HandleDebugAddBreakpoint(const json& args)
{
    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();

    // Accept symbol, address, or file:line
    std::string symbol = args.value("symbol", "");
    std::string address = args.value("address", "");
    std::string file = args.value("file", "");
    int line = args.value("line", 0);

    RawrXD::Debugger::DebugResult r;
    if (!symbol.empty())
    {
        r = engine.addBreakpointBySymbol(symbol);
    }
    else if (!address.empty())
    {
        uint64_t addr = strtoull(address.c_str(), nullptr, 16);
        r = engine.addBreakpoint(addr);
    }
    else if (!file.empty() && line > 0)
    {
        r = engine.addBreakpointBySourceLine(file, line);
    }
    else
    {
        return ToolExecResult::error("Provide 'symbol', 'address' (hex), or 'file'+'line'.");
    }

    if (!r.success)
        return ToolExecResult::error(std::string("Add breakpoint failed: ") + r.detail);
    return ToolExecResult::ok(engine.toJsonBreakpoints());
}

ToolExecResult HandleDebugRemoveBreakpoint(const json& args)
{
    int bpId = args.value("id", -1);
    if (bpId < 0)
        return ToolExecResult::error("Missing required parameter: id");

    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    auto r = engine.removeBreakpoint(static_cast<uint32_t>(bpId));
    if (!r.success)
        return ToolExecResult::error(std::string("Remove breakpoint failed: ") + r.detail);
    return ToolExecResult::ok(engine.toJsonBreakpoints());
}

ToolExecResult HandleDebugStacktrace(const json& args)
{
    (void)args;
    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    return ToolExecResult::ok(engine.toJsonStack());
}

ToolExecResult HandleDebugRegisters(const json& args)
{
    (void)args;
    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    return ToolExecResult::ok(engine.toJsonRegisters());
}

ToolExecResult HandleDebugMemory(const json& args)
{
    std::string addrStr = args.value("address", "");
    if (addrStr.empty())
        return ToolExecResult::error("Missing required parameter: address (hex)");
    uint64_t address = strtoull(addrStr.c_str(), nullptr, 16);
    uint64_t size = args.value("size", 256);
    if (size > 4096)
        size = 4096;

    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    return ToolExecResult::ok(engine.toJsonMemory(address, size));
}

ToolExecResult HandleDebugDisasm(const json& args)
{
    std::string addrStr = args.value("address", "");
    if (addrStr.empty())
        return ToolExecResult::error("Missing required parameter: address (hex)");
    uint64_t address = strtoull(addrStr.c_str(), nullptr, 16);
    int lines = args.value("lines", 20);
    if (lines > 200)
        lines = 200;

    auto& engine = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    return ToolExecResult::ok(engine.toJsonDisassembly(address, static_cast<uint32_t>(lines)));
}

ToolExecResult HandleDebugAnalyze(const json& args)
{
    (void)args;
    auto& agent = RawrXD::Debug::AIDebugAgent::Instance();
    auto analysis = agent.AnalyzeLastException();
    return ToolExecResult::ok(agent.FormatAnalysisForLLM(analysis));
}

ToolExecResult HandleDebugSnapshot(const json& args)
{
    (void)args;
    auto& agent = RawrXD::Debug::AIDebugAgent::Instance();
    return ToolExecResult::ok(agent.CaptureDebugSnapshot());
}

ToolExecResult HandleDebugSuggestBreakpoints(const json& args)
{
    std::string context = args.value("context", "crash");
    auto& agent = RawrXD::Debug::AIDebugAgent::Instance();
    auto suggestions = agent.SuggestBreakpoints(context);

    json result = json::array();
    for (const auto& s : suggestions)
    {
        result.push_back({{"symbol", s.symbol},
                          {"reason", s.reason},
                          {"type", s.type == RawrXD::Debugger::BreakpointType::Hardware ? "hardware" : "software"}});
    }
    return ToolExecResult::ok(result.dump(2));
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// AgentToolRegistry implementation
// ---------------------------------------------------------------------------

AgentToolRegistry& AgentToolRegistry::Instance()
{
    static AgentToolRegistry instance;
    return instance;
}

AgentToolRegistry::AgentToolRegistry()
{
    InitDescriptors();
}

void AgentToolRegistry::InitDescriptors()
{
// Use X-Macro to populate basic name/description
#define INIT_DESCRIPTOR(tool_name_, tool_desc_)                                                                        \
    {                                                                                                                  \
        ToolDescriptor td;                                                                                             \
        td.name = #tool_name_;                                                                                         \
        td.description = tool_desc_;                                                                                   \
        td.params_schema = json::object();                                                                             \
        td.handler = nullptr;                                                                                          \
        m_tools.push_back(std::move(td));                                                                              \
        m_nameIndex[#tool_name_] = m_tools.size() - 1;                                                                 \
    }

    AGENT_TOOLS_X(INIT_DESCRIPTOR)
#undef INIT_DESCRIPTOR

    // -----------------------------------------------------------------------
    // Reconcile schemas with AgentToolHandlers (single source of truth)
    // -----------------------------------------------------------------------
    // AgentToolHandlers::GetAllSchemas() returns OpenAI function-calling schema:
    // { "type":"function", "function": { "name", "description", "parameters": { "properties", "required" } } }
    // We translate it into ToolRegistry's internal param schema format:
    // params_schema[param] = { type, description, (optional) default }
    //
    // NOTE: ToolRegistry marks a parameter as "optional" if it has a "default".
    // For schemas that don't express defaults, we set default=null for non-required params
    // to keep required detection stable.
    try
    {
        const json handlerSchemas = AgentToolHandlers::GetAllSchemas();
        for (const auto& s : handlerSchemas)
        {
            if (!s.is_object() || s.value("type", "") != "function" || !s.contains("function"))
            {
                continue;
            }
            const auto& fn = s["function"];
            const std::string name = fn.value("name", "");
            if (name.empty())
            {
                continue;
            }
            EnsureDescriptorFromHandlerSchema(fn, m_tools, m_nameIndex);
            auto it = m_nameIndex.find(name);
            if (it == m_nameIndex.end())
            {
                continue;
            }

            ToolDescriptor& td = m_tools[it->second];
            // Note: ToolDescriptor::description is a const char* (static storage expectation).
            // We intentionally do not overwrite it with dynamic strings from JSON.

            // Rebuild param schema from AgentToolHandlers' OpenAI parameters shape.
            json rebuilt = BuildRegistryParamSchemaFromHandlerSchema(fn);

            if (!rebuilt.empty())
            {
                td.params_schema = std::move(rebuilt);
            }
        }
    }
    catch (...)
    {
        // Fail-closed: keep the ToolRegistry-built schemas as a fallback.
    }

    // -----------------------------------------------------------------------
    // Wire parameter schemas programmatically (avoids preprocessor comma issue)
    // -----------------------------------------------------------------------
    auto setParam = [this](const char* tool, const char* param, const char* type, const char* desc)
    {
        auto it = m_nameIndex.find(tool);
        if (it != m_nameIndex.end())
        {
            json p;
            p["type"] = type;
            p["description"] = desc;
            m_tools[it->second].params_schema[param] = p;
        }
    };
    auto setParamWithDefault =
        [this](const char* tool, const char* param, const char* type, const char* desc, json defaultVal)
    {
        auto it = m_nameIndex.find(tool);
        if (it != m_nameIndex.end())
        {
            json p;
            p["type"] = type;
            p["description"] = desc;
            p["default"] = defaultVal;
            m_tools[it->second].params_schema[param] = p;
        }
    };

    // read_file
    setParam("read_file", "path", "string", "Absolute or project-relative path to the file");

    // write_file
    setParam("write_file", "path", "string", "Target file path");
    setParam("write_file", "content", "string", "Full file content to write");

    // replace_in_file
    setParam("replace_in_file", "path", "string", "File to modify");
    setParam("replace_in_file", "old_string", "string", "Exact text to find (include context lines)");
    setParam("replace_in_file", "new_string", "string", "Replacement text");

    // execute_command
    setParam("execute_command", "command", "string", "Shell command to execute");
    setParamWithDefault("execute_command", "timeout", "number", "Timeout in milliseconds (default 30000)", 30000);
    setParam("execute_command", "cwd", "string",
             "Optional working directory (allowlisted). Same as working_directory. Defaults to primary workspace root "
             "when omitted.");
    setParam("execute_command", "working_directory", "string",
             "Optional working directory (allowlisted). Same as cwd.");
    setParamWithDefault("execute_command", "use_integrated_terminal", "boolean",
                        "Mirror command and output to the IDE agent terminal", false);
    setParamWithDefault("execute_command", "mirror_to_ide_agent_terminal", "boolean",
                        "Alias of use_integrated_terminal", false);
    setParamWithDefault("execute_command", "allow_unsafe", "boolean",
                        "Set true only after explicit modal approval for destructive commands", false);

    // screenshot
    setParamWithDefault("screenshot", "mode", "string", "Capture mode: desktop or active_window", "desktop");
    setParamWithDefault("screenshot", "path", "string", "Output PNG path (default: %TEMP%\\rawrxd_screenshot_*.png)",
                        "");
    setParamWithDefault("screenshot", "x", "integer", "Capture origin X (desktop mode only)", 0);
    setParamWithDefault("screenshot", "y", "integer", "Capture origin Y (desktop mode only)", 0);
    setParamWithDefault("screenshot", "width", "integer", "Capture width (0=full screen/window)", 0);
    setParamWithDefault("screenshot", "height", "integer", "Capture height (0=full screen/window)", 0);

    // tool_registry_self_check
    // No params

    // ingame_frame_check
    setParamWithDefault("ingame_frame_check", "capture", "boolean", "Capture a fresh screenshot first", true);
    setParamWithDefault("ingame_frame_check", "mode", "string", "Capture mode: desktop or active_window",
                        "active_window");
    setParamWithDefault("ingame_frame_check", "path", "string", "If capture=false, analyze this image path", "");
    setParamWithDefault("ingame_frame_check", "include_profiler", "boolean",
                        "Attach Unity/Unreal profiler snapshot if available", true);
    setParamWithDefault("ingame_frame_check", "x", "integer", "Capture origin X (desktop mode only)", 0);
    setParamWithDefault("ingame_frame_check", "y", "integer", "Capture origin Y (desktop mode only)", 0);
    setParamWithDefault("ingame_frame_check", "width", "integer", "Capture width (0=full screen/window)", 0);
    setParamWithDefault("ingame_frame_check", "height", "integer", "Capture height (0=full screen/window)", 0);

    // search_code
    setParam("search_code", "query", "string", "Search pattern (regex or literal)");
    setParamWithDefault("search_code", "file_pattern", "string", "Glob filter (default: *.*)", "*.*");
    setParamWithDefault("search_code", "is_regex", "boolean", "Treat query as regex", false);

    // get_diagnostics
    setParam("get_diagnostics", "file", "string", "File path, or empty for all diagnostics");

    // list_directory
    setParam("list_directory", "path", "string", "Directory path to list");
    setParamWithDefault("list_directory", "recursive", "boolean", "Recurse into subdirectories", false);

    // get_coverage
    setParam("get_coverage", "file", "string", "Source file to query coverage for");
    setParam("get_coverage", "function_name", "string", "Optional: specific function to check coverage");

    // run_build
    setParamWithDefault("run_build", "target", "string", "Build target (default: all)", "all");
    setParamWithDefault("run_build", "config", "string", "Build configuration (Release/Debug)", "Release");

    // apply_hotpatch
    setParam("apply_hotpatch", "layer", "string", "Patch layer: memory, byte, or server");
    setParam("apply_hotpatch", "target", "string", "Target address, file, or endpoint");
    setParam("apply_hotpatch", "data", "string", "Patch payload (hex-encoded for memory/byte)");

    // disk_recovery
    setParam("disk_recovery", "action", "string",
             "Action to perform: scan, init, extract_key, run, abort, stats, cleanup, carve");
    setParamWithDefault("disk_recovery", "drive", "number", "Physical drive number (0-15) for init action", -1);
    setParamWithDefault("disk_recovery", "image_path", "string", "Path to raw disk image (for carve action)", "");
    setParamWithDefault("disk_recovery", "output_dir", "string", "Directory for carved files (for carve action)", "");
    setParamWithDefault("disk_recovery", "max_files", "number", "Maximum files to carve (default 256)", 256);

    // git_status
    // No params

    // git_diff
    setParamWithDefault("git_diff", "target", "string", "Diff target (default: HEAD)", "HEAD");

    // git_commit
    setParam("git_commit", "message", "string", "Commit message");

    // GitHub issue / PR tools
    setParamWithDefault("gh_issue_list", "command", "string", "Optional raw gh command override", "");
    setParam("gh_issue_view", "number", "string", "Issue number to view");
    setParamWithDefault("gh_pr_list", "command", "string", "Optional raw gh command override", "");
    setParam("gh_pr_view", "number", "string", "Pull request number to view");

    // gh_create_pr
    setParam("gh_create_pr", "title", "string", "PR title");
    setParam("gh_create_pr", "body", "string", "PR body");

    // Advanced multi-file edit/refactor tools
    setParam("propose_multifile_edits", "edits", "array",
             "Array of edits: {file, type(insert|delete|replace|modify), line_start, line_end, content, reason}");
    setParam("preview_multifile_diff", "edits", "array",
             "Array of edits: {file, type(insert|delete|replace|modify), line_start, line_end, content, reason}");
    setParam("apply_multifile_edits", "edits", "array",
             "Array of edits: {file, type(insert|delete|replace|modify), line_start, line_end, content, reason}");

    // term_exec
    setParam("term_exec", "command", "string", "Shell command to execute");
    setParam("term_exec", "cwd", "string", "Working directory for the command");
    setParamWithDefault("term_exec", "timeout", "number", "Timeout in ms (default 60000)", 60000);

    // sys_get_capabilities
    // No params

    // =====================================================================
    // BATCH 1: Parameter schemas for unsimplified tools
    // =====================================================================

    // get_gpu_telemetry
    setParamWithDefault("get_gpu_telemetry", "backend", "string", "GPU backend: vulkan, hip, cuda, dml, cpu",
                        "primary");
    setParamWithDefault("get_gpu_telemetry", "include_details", "boolean", "Include detailed telemetry", true);

    // tune_vram_limit
    setParam("tune_vram_limit", "limit_mb", "number", "VRAM limit in MB (256-1048576)");
    setParamWithDefault("tune_vram_limit", "apply_immediately", "boolean", "Apply immediately or on next load", false);
    setParamWithDefault("tune_vram_limit", "backend", "string", "Target backend (auto, vulkan, hip, cuda, dml)",
                        "auto");

    // bench_kernel
    setParamWithDefault("bench_kernel", "kernel_name", "string", "Kernel to benchmark (default: all)", "all");
    setParamWithDefault("bench_kernel", "iterations", "number", "Iteration count (1-1000000)", 1000);
    setParamWithDefault("bench_kernel", "thread_count", "number", "Thread count for benchmark", 0);
    setParamWithDefault("bench_kernel", "force_cpu_affinity", "boolean", "Force CPU affinity", false);

    // create_memory_silo
    setParam("create_memory_silo", "silo_name", "string", "Silo name (alphanumeric + underscore)");
    setParam("create_memory_silo", "max_memory_mb", "number", "Max memory in MB (32-262144)");
    setParamWithDefault("create_memory_silo", "max_processes", "number", "Max processes (default 256)", 256);
    setParamWithDefault("create_memory_silo", "enable_iocp", "boolean", "Enable I/O completion port", true);
    setParamWithDefault("create_memory_silo", "enable_cpu_limit", "boolean", "Enable CPU limit", false);
    setParamWithDefault("create_memory_silo", "cpu_limit_percent", "number", "CPU limit %%", 0.0);

    // query_silo_stats
    setParam("query_silo_stats", "silo_id", "string", "Silo ID from create_memory_silo");

    // set_silo_quota
    setParam("set_silo_quota", "silo_id", "string", "Silo ID");
    setParam("set_silo_quota", "new_limit_mb", "number", "New memory limit in MB");

    // map_model_aperture
    setParam("map_model_aperture", "file_path", "string", "Path to model file (GGUF, bin, etc.)");
    setParamWithDefault("map_model_aperture", "offset_mb", "number", "Offset in MB", 0);
    setParamWithDefault("map_model_aperture", "length_mb", "number", "Length in MB (0=entire file)", 0);
    setParamWithDefault("map_model_aperture", "access", "string", "Access mode: read or readwrite", "read");
    setParamWithDefault("map_model_aperture", "use_v3", "boolean", "Use MapViewOfFile3 if available", true);

    // query_virtual_memory
    setParamWithDefault("query_virtual_memory", "address", "string", "Memory address (hex or decimal)", "0");
    setParamWithDefault("query_virtual_memory", "process_id", "number", "Process ID (current if omitted)", 0);

    // manage_local_embeddings
    setParam("manage_local_embeddings", "action", "string", "Action: index, query, clear, stats");
    setParamWithDefault("manage_local_embeddings", "workspace_path", "string", "Workspace path for indexing", "");
    setParamWithDefault("manage_local_embeddings", "query_text", "string", "Query text for semantic search", "");
    setParamWithDefault("manage_local_embeddings", "top_k", "number", "Top K results (1-100)", 10);

    // purge_telemetry
    setParamWithDefault("purge_telemetry", "target", "string", "Target: logs, traces, cache, all", "all");
    setParamWithDefault("purge_telemetry", "max_age_days", "number", "Delete older than N days (-1=all)", -1);
    setParamWithDefault("purge_telemetry", "dry_run", "boolean", "Simulate without deleting", false);

    // asm_assemble
    setParam("asm_assemble", "source", "string", "MASM x64 assembly source code content");
    setParamWithDefault("asm_assemble", "output", "string", "Target executable path (default: agent_output.exe)",
                        "agent_output.exe");
    setParamWithDefault("asm_assemble", "test_run", "boolean",
                        "If true, attempts a dry-run to verify the PE header stability", false);

    // Debug tool param schemas
    setParam("debug_launch", "path", "string", "Absolute path to the executable to launch under debugger");
    setParamWithDefault("debug_launch", "args", "string", "Command-line arguments for the target", "");
    setParamWithDefault("debug_launch", "working_dir", "string", "Working directory for the target", "");
    setParam("debug_attach", "pid", "integer", "Process ID to attach to");
    setParamWithDefault("debug_add_breakpoint", "symbol", "string", "Symbol name (e.g. main, MyClass::Foo)", "");
    setParamWithDefault("debug_add_breakpoint", "address", "string", "Hex address (e.g. 7ff6a1230000)", "");
    setParamWithDefault("debug_add_breakpoint", "file", "string", "Source file path for line breakpoint", "");
    setParamWithDefault("debug_add_breakpoint", "line", "integer", "Source line number", 0);
    setParam("debug_remove_breakpoint", "id", "integer", "Breakpoint ID to remove");
    setParam("debug_memory", "address", "string", "Hex address to read memory from");
    setParamWithDefault("debug_memory", "size", "integer", "Number of bytes to read (max 4096)", 256);
    setParam("debug_disasm", "address", "string", "Hex address to start disassembly");
    setParamWithDefault("debug_disasm", "lines", "integer", "Number of instructions to disassemble (max 200)", 20);
    setParamWithDefault("debug_suggest_breakpoints", "context", "string",
                        "Problem context: crash, hang, corruption, regression", "crash");

    // Wire default handlers
    RegisterHandler("read_file", HandleReadFile);
    RegisterHandler("write_file", HandleWriteFile);
    RegisterHandler("replace_in_file", HandleReplaceInFile);
    RegisterHandler("execute_command", HandleExecuteCommand);
    RegisterHandler("screenshot", HandleScreenshot);
    RegisterHandler("tool_registry_self_check", HandleToolRegistrySelfCheck);
    RegisterHandler("ingame_frame_check", HandleIngameFrameCheck);
    RegisterHandler("search_code", HandleSearchCode);
    RegisterHandler("get_diagnostics", HandleGetDiagnostics);
    RegisterHandler("list_directory", HandleListDirectory);
    RegisterHandler("get_coverage", HandleGetCoverage);
    RegisterHandler("run_build", HandleRunBuild);
    RegisterHandler("asm_assemble", HandleAsmAssemble);
    RegisterHandler("apply_hotpatch", HandleApplyHotpatch);
    RegisterHandler("disk_recovery", HandleDiskRecovery);

    // =====================================================================
    // BATCH 1: UNSIMPLIFIED Hardware, Security, Memory & Sovereign Tools
    // =====================================================================
    // Wire hardware telemetry and optimization handlers
    RegisterHandler("get_gpu_telemetry", HandleGetGpuTelemetry);
    RegisterHandler("tune_vram_limit", HandleTuneVramLimit);
    RegisterHandler("bench_kernel", HandleBenchKernel);

    // Wire memory silo (security) handlers
    RegisterHandler("create_memory_silo", HandleCreateMemorySilo);
    RegisterHandler("query_silo_stats", HandleQuerySiloStats);
    RegisterHandler("set_silo_quota", HandleSetSiloQuota);

    // Wire virtual memory and aperture mapping handlers
    RegisterHandler("map_model_aperture", HandleMapModelAperture);
    RegisterHandler("query_virtual_memory", HandleQueryVirtualMemory);

    // Wire sovereign operations handlers
    RegisterHandler("manage_local_embeddings", HandleManageLocalEmbeddings);
    RegisterHandler("memory_file", HandleMemoryFile);
    RegisterHandler("purge_telemetry", HandlePurgeTelemetry);
    RegisterHandler("git_status", HandleGitStatus);
    RegisterHandler("git_diff", HandleGitDiff);
    RegisterHandler("git_commit", HandleGitCommit);
    RegisterHandler("gh_issue_list", HandleGitHubIssueList);
    RegisterHandler("gh_issue_view", HandleGitHubIssueView);
    RegisterHandler("gh_pr_list", HandleGitHubPrList);
    RegisterHandler("gh_pr_view", HandleGitHubPrView);
    RegisterHandler("gh_create_pr", HandleGitHubCreatePR);
    RegisterHandler("propose_multifile_edits", HandleProposeMultiFileEdits);
    RegisterHandler("preview_multifile_diff", HandlePreviewMultiFileDiff);
    RegisterHandler("apply_multifile_edits", HandleApplyMultiFileEdits);
    RegisterHandler("term_exec", HandleTermExec);
    RegisterHandler("sys_get_capabilities", HandleSysGetCapabilities);

    // Debug tool handlers
    RegisterHandler("debug_launch", HandleDebugLaunch);
    RegisterHandler("debug_attach", HandleDebugAttach);
    RegisterHandler("debug_break", HandleDebugBreak);
    RegisterHandler("debug_continue", HandleDebugContinue);
    RegisterHandler("debug_step_over", HandleDebugStepOver);
    RegisterHandler("debug_step_into", HandleDebugStepInto);
    RegisterHandler("debug_add_breakpoint", HandleDebugAddBreakpoint);
    RegisterHandler("debug_remove_breakpoint", HandleDebugRemoveBreakpoint);
    RegisterHandler("debug_stacktrace", HandleDebugStacktrace);
    RegisterHandler("debug_registers", HandleDebugRegisters);
    RegisterHandler("debug_memory", HandleDebugMemory);
    RegisterHandler("debug_disasm", HandleDebugDisasm);
    RegisterHandler("debug_analyze", HandleDebugAnalyze);
    RegisterHandler("debug_snapshot", HandleDebugSnapshot);
    RegisterHandler("debug_suggest_breakpoints", HandleDebugSuggestBreakpoints);

    // -----------------------------------------------------------------------
    // Final reconciliation pass:
    // Ensure ToolRegistry parameter schemas match AgentToolHandlers catalog.
    // This intentionally runs AFTER local setParam() wiring so we don't drift.
    // -----------------------------------------------------------------------
    try
    {
        const json handlerSchemas = AgentToolHandlers::GetAllSchemas();
        for (const auto& s : handlerSchemas)
        {
            if (!s.is_object() || s.value("type", "") != "function" || !s.contains("function"))
            {
                continue;
            }
            const auto& fn = s["function"];
            const std::string name = fn.value("name", "");
            if (name.empty())
            {
                continue;
            }
            EnsureDescriptorFromHandlerSchema(fn, m_tools, m_nameIndex);
            auto it = m_nameIndex.find(name);
            if (it == m_nameIndex.end())
            {
                continue;
            }

            json rebuilt = BuildRegistryParamSchemaFromHandlerSchema(fn);

            if (!rebuilt.empty())
            {
                m_tools[it->second].params_schema = std::move(rebuilt);
            }
        }
    }
    catch (...)
    {
        // Keep local schema as fallback.
    }
}

json AgentToolRegistry::GetToolSchemas() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    json tools = json::array();

    for (const auto& td : m_tools)
    {
        // Build required array from params that don't have defaults
        json required_params = json::array();
        json params_copy = td.params_schema;  // non-const copy for iteration
        for (auto it = params_copy.begin(); it != params_copy.end(); ++it)
        {
            const std::string& key = it.key();
            auto& val = it.value();
            if (!val.contains("default"))
            {
                required_params.push_back(key);
            }
        }

        tools.push_back(nlohmann::json::object(
            {{"type", "function"},
             {"function",
              nlohmann::json::object({{"name", td.name},
                                      {"description", td.description},
                                      {"parameters", nlohmann::json::object({{"type", "object"},
                                                                             {"properties", td.params_schema},
                                                                             {"required", required_params}})}})}}));
    }
    return tools;
}

std::string AgentToolRegistry::GetSystemPrompt(const std::string& cwd, const std::vector<std::string>& openFiles,
                                               std::vector<std::string>* appliedInstructionSources) const
{
    std::string prompt = AgentToolHandlers::GetSystemPrompt(cwd, openFiles, appliedInstructionSources);
    std::ostringstream addon;
    addon << "\n\nRegistry lane notes:\n"
          << "- Tool count: " << m_tools.size() << "\n"
          << "- Prefer run_build + get_diagnostics after edits in this lane.\n"
          << "- Keep PatchResult fail-closed semantics for all tool outcomes.\n";
    prompt += addon.str();
    return prompt;
}

ToolExecResult AgentToolRegistry::Dispatch(const std::string& tool_name, const json& args)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const std::string normalizedTool = NormalizeToolName(tool_name);
    if (normalizedTool.empty())
    {
        return ToolExecResult::error("Unknown tool: " + tool_name);
    }

    auto it = m_nameIndex.find(normalizedTool);

    // ---- Intent similarity fallback ----
    // When the LLM produces a tool name not in the index, compute bigram-Jaccard
    // similarity against all registered tools and redirect to the best match
    // (if similarity is above a useful threshold).
    if (it == m_nameIndex.end())
    {
        // Check intent cache first (O(1) for repeated misses)
        auto cacheIt = m_intentCache.find(normalizedTool);
        if (cacheIt == m_intentCache.end())
        {
            // Compute bigram set for the query
            auto makeBigrams = [](const std::string& s)
            {
                std::unordered_map<std::string, int> bg;
                if (s.size() < 2)
                {
                    bg[s + " "] = 1;
                    return bg;
                }
                for (size_t i = 0; i + 1 < s.size(); ++i)
                    ++bg[s.substr(i, 2)];
                return bg;
            };
            auto jaccard = [&](const std::unordered_map<std::string, int>& a,
                               const std::unordered_map<std::string, int>& b) -> float
            {
                int intersection = 0, unionVal = 0;
                for (auto& [k, v] : a)
                {
                    auto bit = b.find(k);
                    if (bit != b.end())
                        intersection += std::min(v, bit->second);
                    unionVal += v;
                }
                for (auto& [k, v] : b)
                    unionVal += v;
                unionVal -= intersection;
                return unionVal == 0 ? 0.0f : static_cast<float>(intersection) / static_cast<float>(unionVal);
            };

            auto queryBigrams = makeBigrams(normalizedTool);
            std::vector<IntentMatch> ranked;
            ranked.reserve(m_tools.size());
            for (auto& [name, idx] : m_nameIndex)
            {
                float sim = jaccard(queryBigrams, makeBigrams(name));
                ranked.push_back({name, sim});
            }
            std::sort(ranked.begin(), ranked.end(),
                      [](const IntentMatch& a, const IntentMatch& b) { return a.similarity > b.similarity; });
            if (ranked.size() > 3)
                ranked.resize(3);
            m_intentCache[normalizedTool] = ranked;
            cacheIt = m_intentCache.find(normalizedTool);
        }

        constexpr float kSimilarityThreshold = 0.25f;
        if (!cacheIt->second.empty() && cacheIt->second[0].similarity >= kSimilarityThreshold)
        {
            const std::string& resolved = cacheIt->second[0].toolName;
            GetObs().logWarn(kRegistryComponent, "Dispatch: intent fallback",
                             nlohmann::json::object({{"tool", tool_name},
                                                     {"normalized", normalizedTool},
                                                     {"resolved", resolved},
                                                     {"similarity", cacheIt->second[0].similarity}}));
            GetObs().incrementCounter("tool_registry.intent_fallbacks");
            it = m_nameIndex.find(resolved);
        }
    }

    if (it == m_nameIndex.end())
    {
        GetObs().logWarn(kRegistryComponent, "Dispatch: unknown tool",
                         nlohmann::json::object({{"tool", tool_name}, {"normalized", normalizedTool}}));
        GetObs().incrementCounter("tool_registry.unknown_tool");
        return ToolExecResult::error("Unknown tool: " + tool_name);
    }

    auto& td = m_tools[it->second];
    const bool hasBridgedAgentHandler = !td.handler && AgentToolHandlers::Instance().HasTool(normalizedTool);
    if (!td.handler && !hasBridgedAgentHandler)
    {
        GetObs().logError(kRegistryComponent, "Dispatch: no handler",
                          nlohmann::json::object({{"tool", tool_name}, {"normalized", normalizedTool}}));
        return ToolExecResult::error("No handler registered for tool: " + tool_name);
    }

    // Validate args
    std::string validationError;
    if (!ValidateArgs(normalizedTool, args, validationError))
    {
        ++td.error_count;
        GetObs().logWarn(
            kRegistryComponent, "Dispatch: validation failed",
            nlohmann::json::object({{"tool", tool_name}, {"normalized", normalizedTool}, {"error", validationError}}));
        GetObs().incrementCounter("tool_registry.validation_failures");
        return ToolExecResult::error("Validation failed: " + validationError);
    }

    const bool cacheEligible = IsResultCacheEligibleTool(normalizedTool);
    const std::string cacheKey = cacheEligible ? BuildResultCacheKey(normalizedTool, args) : std::string();
    if (cacheEligible)
    {
        const auto now = std::chrono::steady_clock::now();
        auto cacheIt = m_toolResultCache.find(cacheKey);
        if (cacheIt != m_toolResultCache.end())
        {
            if (cacheIt->second.expiresAt > now)
            {
                ToolExecResult cached = cacheIt->second.result;
                cached.elapsed_ms = 0.0;
                GetObs().incrementCounter("tool_registry.result_cache_hit");
                GetObs().logDebug(kRegistryComponent, "Dispatch: tool result cache hit",
                                  nlohmann::json::object({{"tool", normalizedTool}}));
                return cached;
            }
            m_toolResultCache.erase(cacheIt);
        }
        GetObs().incrementCounter("tool_registry.result_cache_miss");
    }

    // Traced execution with timing
    auto spanId = GetObs().startSpan("tool_dispatch:" + normalizedTool);
    GetObs().logDebug(kRegistryComponent, "Dispatching tool call",
                      nlohmann::json::object({{"tool", tool_name}, {"normalized", normalizedTool}}));

    auto start = std::chrono::high_resolution_clock::now();
    ToolExecResult result = hasBridgedAgentHandler ? ToToolExecResult(AgentToolHandlers::Instance().Execute(normalizedTool, args))
                                                   : td.handler(args);
    auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    ++td.invocation_count;
    if (!result.success)
    {
        ++td.error_count;
        GetObs().endSpan(spanId, true, result.output);
        GetObs().logWarn(
            kRegistryComponent, "Tool call failed",
            nlohmann::json::object(
                {{"tool", normalizedTool}, {"elapsed_ms", result.elapsed_ms}, {"exit_code", result.exit_code}}));
        GetObs().incrementCounter("tool_registry.tool_errors");
    }
    else
    {
        if (cacheEligible)
        {
            const auto ttl = std::chrono::milliseconds(ResultCacheTtlMs(normalizedTool));
            m_toolResultCache[cacheKey] = CachedToolResult{result, std::chrono::steady_clock::now() + ttl};

            // Keep this bounded in case of broad repo sweeps.
            constexpr size_t kMaxResultCacheEntries = 512;
            if (m_toolResultCache.size() > kMaxResultCacheEntries)
            {
                m_toolResultCache.clear();
            }
        }

        GetObs().endSpan(spanId);
        GetObs().logDebug(kRegistryComponent, "Tool call succeeded",
                          nlohmann::json::object({{"tool", normalizedTool}, {"elapsed_ms", result.elapsed_ms}}));
    }

    GetObs().recordHistogram("tool_registry.dispatch_ms", static_cast<float>(result.elapsed_ms));
    GetObs().incrementCounter("tool_registry.total_dispatches");

    return result;
}

void AgentToolRegistry::RegisterHandler(const std::string& tool_name, ToolHandler handler)
{
    const std::string normalizedTool = NormalizeToolName(tool_name);
    auto it = m_nameIndex.find(normalizedTool);
    if (it == m_nameIndex.end() && !normalizedTool.empty())
    {
        ToolDescriptor td{};
        td.name = InternToolRegistryString(normalizedTool);
        td.description = InternToolRegistryString("Dynamically registered tool.");
        td.params_schema = json::object();
        td.handler = nullptr;
        m_tools.push_back(std::move(td));
        it = m_nameIndex.emplace(normalizedTool, m_tools.size() - 1).first;
    }
    if (it != m_nameIndex.end())
    {
        m_tools[it->second].handler = handler;
    }
}

std::vector<std::string> AgentToolRegistry::ListTools() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_tools.size());
    for (const auto& td : m_tools)
    {
        names.emplace_back(td.name);
    }
    return names;
}

std::vector<std::string> AgentToolRegistry::GetToolsMissingHandlers() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> missing;
    missing.reserve(m_tools.size());
    for (const auto& td : m_tools)
    {
        const std::string name = td.name ? td.name : "";
        const bool hasExternalImpl = !name.empty() && AgentToolHandlers::Instance().HasTool(name);
        if (!td.handler && !hasExternalImpl)
        {
            missing.push_back(name);
        }
    }
    // Remove any empty entries (defensive)
    missing.erase(std::remove_if(missing.begin(), missing.end(), [](const std::string& s) { return s.empty(); }),
                  missing.end());
    return missing;
}

uint64_t AgentToolRegistry::GetTotalInvocations() const
{
    uint64_t total = 0;
    for (const auto& td : m_tools)
    {
        total += td.invocation_count;
    }
    return total;
}

uint64_t AgentToolRegistry::GetTotalErrors() const
{
    uint64_t total = 0;
    for (const auto& td : m_tools)
    {
        total += td.error_count;
    }
    return total;
}

bool AgentToolRegistry::ValidateArgs(const std::string& tool_name, const json& args, std::string& error) const
{
    const std::string normalizedTool = NormalizeToolName(tool_name);
    auto it = m_nameIndex.find(normalizedTool);
    if (it == m_nameIndex.end())
    {
        error = "Unknown tool: " + tool_name;
        return false;
    }

    const auto& td = m_tools[it->second];

    // Check required parameters (those without defaults)
    json schema_copy = td.params_schema;  // non-const copy for iteration
    for (auto it2 = schema_copy.begin(); it2 != schema_copy.end(); ++it2)
    {
        const std::string key = it2.key();
        auto& schema = it2.value();
        if (!schema.contains("default") && !args.contains(key))
        {
            error = "Missing required parameter: " + key;
            return false;
        }
        // Type checking
        if (args.contains(key))
        {
            std::string expected_type = schema.value("type", "string");
            const auto& val = args[key];
            if (expected_type == "string" && !val.is_string())
            {
                error = "Parameter '" + key + "' must be a string";
                return false;
            }
            if (expected_type == "number" && !val.is_number())
            {
                error = "Parameter '" + key + "' must be a number";
                return false;
            }
            if (expected_type == "boolean" && !val.is_boolean())
            {
                error = "Parameter '" + key + "' must be a boolean";
                return false;
            }
        }
    }
    return true;
}

bool AgentToolRegistry::IsResultCacheEligibleTool(const std::string& normalizedTool) const
{
    return normalizedTool == "read_file" || normalizedTool == "list_directory" || normalizedTool == "search_code" ||
           normalizedTool == "get_diagnostics" || normalizedTool == "git_status" || normalizedTool == "git_diff";
}

int AgentToolRegistry::ResultCacheTtlMs(const std::string& normalizedTool) const
{
    if (normalizedTool == "read_file")
        return 1500;
    if (normalizedTool == "list_directory")
        return 1200;
    if (normalizedTool == "search_code")
        return 2000;
    if (normalizedTool == "get_diagnostics")
        return 1200;
    if (normalizedTool == "git_status" || normalizedTool == "git_diff")
        return 800;
    return 1000;
}

std::string AgentToolRegistry::BuildResultCacheKey(const std::string& normalizedTool, const json& args) const
{
    return normalizedTool + "|" + args.dump();
}

}  // namespace Agent
}  // namespace RawrXD


// =============================================================================
// AgentToolRegistry::RankByIntent
// =============================================================================
// Bigram-Jaccard ranking for approximate tool name matching.
// Results are memoised in m_intentCache (protected by m_mutex).
// =============================================================================
namespace RawrXD
{
namespace Agent
{

std::vector<AgentToolRegistry::IntentMatch> AgentToolRegistry::RankByIntent(const std::string& query, size_t topK) const
{
    const std::string norm = NormalizeToolName(query);
    std::lock_guard<std::mutex> lock(m_mutex);

    // Return cached result if available
    auto cacheIt = m_intentCache.find(norm);
    if (cacheIt != m_intentCache.end())
    {
        auto result = cacheIt->second;
        if (result.size() > topK)
            result.resize(topK);
        return result;
    }

    // Build bigram multiset for the query
    auto makeBigrams = [](const std::string& s)
    {
        std::unordered_map<std::string, int> bg;
        if (s.size() < 2)
        {
            bg[s + " "] = 1;
            return bg;
        }
        for (size_t i = 0; i + 1 < s.size(); ++i)
            ++bg[s.substr(i, 2)];
        return bg;
    };
    auto jaccard = [&](const std::unordered_map<std::string, int>& a,
                       const std::unordered_map<std::string, int>& b) -> float
    {
        int intersection = 0, unionVal = 0;
        for (auto& [k, v] : a)
        {
            auto bit = b.find(k);
            if (bit != b.end())
                intersection += std::min(v, bit->second);
            unionVal += v;
        }
        for (auto& [k, v] : b)
            unionVal += v;
        unionVal -= intersection;
        return unionVal == 0 ? 0.0f : static_cast<float>(intersection) / static_cast<float>(unionVal);
    };

    const auto queryBigrams = makeBigrams(norm);
    std::vector<IntentMatch> ranked;
    ranked.reserve(m_tools.size());
    for (auto& [name, idx] : m_nameIndex)
    {
        ranked.push_back({name, jaccard(queryBigrams, makeBigrams(name))});
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const IntentMatch& a, const IntentMatch& b) { return a.similarity > b.similarity; });

    // Cache the full sorted list (capped at 16 to keep memory bounded)
    constexpr size_t kCacheDepth = 16;
    if (ranked.size() > kCacheDepth)
        ranked.resize(kCacheDepth);
    m_intentCache[norm] = ranked;

    if (ranked.size() > topK)
        ranked.resize(topK);
    return ranked;
}

}  // namespace Agent
}  // namespace RawrXD
