#include "tool_registry.h"
#include "tool_registry_init.hpp"
#include "video/tubi_backend.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

static std::unordered_map<std::string, ToolFunc> tools;
static std::mutex tools_mutex;
static std::once_flag tools_once;

namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

inline std::string WorkspaceRoot() {
    const char* env_root = std::getenv("RAWRXD_TOOL_WORKSPACE");
    if (env_root != nullptr && env_root[0] != 0) {
        return std::string(env_root);
    }
    return fs::current_path().string();
}

inline bool IsPathAllowed(const fs::path& path) {
    try {
        const fs::path canonical_root = fs::weakly_canonical(WorkspaceRoot());
        const fs::path canonical_target = fs::weakly_canonical(path);
        const auto rel = canonical_target.lexically_relative(canonical_root);
        if (rel.empty()) {
            return false;
        }
        const std::string rel_text = rel.generic_string();
        return rel_text.rfind("..", 0) != 0;
    } catch (...) {
        return false;
    }
}

inline std::string JsonEscape(const std::string& value) {
    json j = value;
    return j.dump();
}

inline std::string ReadAllText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::string();
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline bool WriteAllText(const fs::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
}

inline bool WriteBmp32(const fs::path& path, int width, int height, const std::vector<std::uint8_t>& rgba) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (rgba.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4u) {
        return false;
    }

    const std::uint32_t fileHeaderSize = 14;
    const std::uint32_t infoHeaderSize = 40;
    const std::uint32_t pixelDataSize = static_cast<std::uint32_t>(width * height * 4);
    const std::uint32_t fileSize = fileHeaderSize + infoHeaderSize + pixelDataSize;

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        return false;
    }

    std::uint8_t fileHeader[14] = {};
    fileHeader[0] = 'B';
    fileHeader[1] = 'M';
    std::memcpy(&fileHeader[2], &fileSize, 4);
    std::uint32_t dataOffset = fileHeaderSize + infoHeaderSize;
    std::memcpy(&fileHeader[10], &dataOffset, 4);
    f.write(reinterpret_cast<const char*>(fileHeader), sizeof(fileHeader));

    std::uint8_t infoHeader[40] = {};
    std::memcpy(&infoHeader[0], &infoHeaderSize, 4);
    std::int32_t w = width;
    std::int32_t h = -height;
    std::memcpy(&infoHeader[4], &w, 4);
    std::memcpy(&infoHeader[8], &h, 4);
    std::uint16_t planes = 1;
    std::uint16_t bpp = 32;
    std::memcpy(&infoHeader[12], &planes, 2);
    std::memcpy(&infoHeader[14], &bpp, 2);
    f.write(reinterpret_cast<const char*>(infoHeader), sizeof(infoHeader));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t idx = static_cast<size_t>((y * width + x) * 4);
            const std::uint8_t bgra[4] = {rgba[idx + 2], rgba[idx + 1], rgba[idx + 0], rgba[idx + 3]};
            f.write(reinterpret_cast<const char*>(bgra), 4);
        }
    }

    return f.good();
}

inline void BlendCircle(std::vector<std::uint8_t>& rgba, int width, int height, float cx, float cy, float radius,
                        std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t alpha) {
    if (radius <= 0.0f) {
        return;
    }
    const int x0 = std::max(0, static_cast<int>(cx - radius));
    const int y0 = std::max(0, static_cast<int>(cy - radius));
    const int x1 = std::min(width - 1, static_cast<int>(cx + radius));
    const int y1 = std::min(height - 1, static_cast<int>(cy + radius));
    const float rr = radius * radius;
    const float blend = static_cast<float>(alpha) / 255.0f;

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            if ((dx * dx) + (dy * dy) > rr) {
                continue;
            }
            const size_t idx = static_cast<size_t>((y * width + x) * 4);
            rgba[idx + 0] = static_cast<std::uint8_t>(rgba[idx + 0] * (1.0f - blend) + r * blend);
            rgba[idx + 1] = static_cast<std::uint8_t>(rgba[idx + 1] * (1.0f - blend) + g * blend);
            rgba[idx + 2] = static_cast<std::uint8_t>(rgba[idx + 2] * (1.0f - blend) + b * blend);
            rgba[idx + 3] = 255;
        }
    }
}

void RegisterCoreTools() {
    ToolRegistry::register_tool("read_file", [](const std::string& input) -> std::string {
        try {
            const json req = json::parse(input.empty() ? "{}" : input);
            const std::string path_raw = req.value("path", "");
            const size_t offset = req.value("offset", static_cast<size_t>(0));
            const size_t limit = req.value("limit", static_cast<size_t>(65536));
            if (path_raw.empty()) {
                return R"({"success":false,"error":"missing path"})";
            }
            fs::path target = fs::path(path_raw);
            if (!target.is_absolute()) {
                target = fs::path(WorkspaceRoot()) / target;
            }
            if (!IsPathAllowed(target)) {
                return R"({"success":false,"error":"access denied"})";
            }
            const std::string text = ReadAllText(target);
            if (text.empty() && !fs::exists(target)) {
                return R"({"success":false,"error":"file not found"})";
            }
            const size_t safe_offset = std::min(offset, text.size());
            const size_t safe_limit = std::min(limit, text.size() - safe_offset);
            const std::string slice = text.substr(safe_offset, safe_limit);
            json resp;
            resp["success"] = true;
            resp["path"] = target.string();
            resp["offset"] = safe_offset;
            resp["bytes"] = slice.size();
            resp["content"] = slice;
            return resp.dump();
        } catch (const std::exception& e) {
            return std::string("{\"success\":false,\"error\":") + JsonEscape(e.what()) + "}";
        }
    });

    ToolRegistry::register_tool("edit_file", [](const std::string& input) -> std::string {
        try {
            const json req = json::parse(input.empty() ? "{}" : input);
            const std::string path_raw = req.value("path", "");
            const std::string old_s = req.value("old_string", "");
            const std::string new_s = req.value("new_string", "");
            if (path_raw.empty()) {
                return R"({"success":false,"error":"missing path"})";
            }
            fs::path target = fs::path(path_raw);
            if (!target.is_absolute()) {
                target = fs::path(WorkspaceRoot()) / target;
            }
            if (!IsPathAllowed(target)) {
                return R"({"success":false,"error":"access denied"})";
            }
            std::string text = ReadAllText(target);
            if (text.empty() && !fs::exists(target)) {
                return R"({"success":false,"error":"file not found"})";
            }
            size_t replaced = 0;
            if (!old_s.empty()) {
                size_t pos = 0;
                while ((pos = text.find(old_s, pos)) != std::string::npos) {
                    text.replace(pos, old_s.size(), new_s);
                    pos += new_s.size();
                    ++replaced;
                }
            }
            if (!WriteAllText(target, text)) {
                return R"({"success":false,"error":"write failed"})";
            }
            json resp;
            resp["success"] = true;
            resp["path"] = target.string();
            resp["replacements"] = replaced;
            return resp.dump();
        } catch (const std::exception& e) {
            return std::string("{\"success\":false,\"error\":") + JsonEscape(e.what()) + "}";
        }
    });

    ToolRegistry::register_tool("run_terminal", [](const std::string& input) -> std::string {
        try {
            const json req = json::parse(input.empty() ? "{}" : input);
            const std::string command = req.value("command", "");
            if (command.empty()) {
                return R"({"success":false,"error":"missing command"})";
            }
            if (command.find("rm -rf") != std::string::npos || command.find("del /f") != std::string::npos ||
                command.find("format ") != std::string::npos) {
                return R"({"success":false,"error":"command blocked by policy"})";
            }
            FILE* pipe = _popen(command.c_str(), "r");
            if (pipe == nullptr) {
                return R"({"success":false,"error":"process spawn failed"})";
            }
            std::string out;
            char buf[512];
            while (std::fgets(buf, static_cast<int>(sizeof(buf)), pipe) != nullptr) {
                out.append(buf);
            }
            const int rc = _pclose(pipe);
            json resp;
            resp["success"] = (rc == 0);
            resp["exit_code"] = rc;
            resp["output"] = out;
            return resp.dump();
        } catch (const std::exception& e) {
            return std::string("{\"success\":false,\"error\":") + JsonEscape(e.what()) + "}";
        }
    });

    ToolRegistry::register_tool("search_code", [](const std::string& input) -> std::string {
        try {
            const json req = json::parse(input.empty() ? "{}" : input);
            const std::string query = req.value("query", "");
            const std::string path_raw = req.value("path", WorkspaceRoot());
            const bool regex = req.value("regex", false);
            if (query.empty()) {
                return R"({"success":false,"error":"missing query"})";
            }
            fs::path root(path_raw);
            if (!root.is_absolute()) {
                root = fs::path(WorkspaceRoot()) / root;
            }
            if (!IsPathAllowed(root)) {
                return R"({"success":false,"error":"access denied"})";
            }
            json hits = json::array();
            std::regex rx;
            if (regex) {
                rx = std::regex(query, std::regex::ECMAScript | std::regex::icase);
            }
            size_t hit_cap = 200;
            for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
                 it != fs::recursive_directory_iterator() && hits.size() < hit_cap; ++it) {
                if (!it->is_regular_file()) {
                    continue;
                }
                const auto ext = it->path().extension().string();
                if (ext != ".cpp" && ext != ".h" && ext != ".hpp" && ext != ".c" && ext != ".asm") {
                    continue;
                }
                const std::string text = ReadAllText(it->path());
                if (text.empty()) {
                    continue;
                }
                bool matched = false;
                if (regex) {
                    matched = std::regex_search(text, rx);
                } else {
                    matched = text.find(query) != std::string::npos;
                }
                if (matched) {
                    json h;
                    h["path"] = it->path().string();
                    hits.push_back(h);
                }
            }
            json resp;
            resp["success"] = true;
            resp["count"] = hits.size();
            resp["hits"] = hits;
            return resp.dump();
        } catch (const std::exception& e) {
            return std::string("{\"success\":false,\"error\":") + JsonEscape(e.what()) + "}";
        }
    });

    ToolRegistry::register_tool("list_dir", [](const std::string& input) -> std::string {
        try {
            const json req = json::parse(input.empty() ? "{}" : input);
            const std::string path_raw = req.value("path", WorkspaceRoot());
            const bool recursive = req.value("recursive", false);
            fs::path root(path_raw);
            if (!root.is_absolute()) {
                root = fs::path(WorkspaceRoot()) / root;
            }
            if (!IsPathAllowed(root)) {
                return R"({"success":false,"error":"access denied"})";
            }
            json entries = json::array();
            const size_t cap = 500;
            if (recursive) {
                for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
                     it != fs::recursive_directory_iterator() && entries.size() < cap; ++it) {
                    json e;
                    e["path"] = it->path().string();
                    e["is_dir"] = it->is_directory();
                    entries.push_back(e);
                }
            } else {
                for (auto it = fs::directory_iterator(root, fs::directory_options::skip_permission_denied);
                     it != fs::directory_iterator() && entries.size() < cap; ++it) {
                    json e;
                    e["path"] = it->path().string();
                    e["is_dir"] = it->is_directory();
                    entries.push_back(e);
                }
            }
            json resp;
            resp["success"] = true;
            resp["count"] = entries.size();
            resp["entries"] = entries;
            return resp.dump();
        } catch (const std::exception& e) {
            return std::string("{\"success\":false,\"error\":") + JsonEscape(e.what()) + "}";
        }
    });

    ToolRegistry::register_tool("generate_image", [](const std::string& input) -> std::string {
        try {
            const json req = json::parse(input.empty() ? "{}" : input);
            const std::string prompt = req.value("prompt", "");
            if (prompt.empty()) {
                return R"({"success":false,"error":"missing prompt"})";
            }

            const int width = std::clamp(req.value("width", 1024), 64, 4096);
            const int height = std::clamp(req.value("height", 1024), 64, 4096);
            const std::string style = req.value("style", std::string("cinematic"));

            fs::path outputPath;
            if (req.contains("output_path") && req["output_path"].is_string()) {
                outputPath = fs::path(req["output_path"].get<std::string>());
                if (!outputPath.is_absolute()) {
                    outputPath = fs::path(WorkspaceRoot()) / outputPath;
                }
            } else {
                const std::string slug = std::to_string(std::hash<std::string>{}(prompt + "|" + style));
                outputPath = fs::path(WorkspaceRoot()) / "generated" / "images" / ("image_" + slug.substr(0, 12) + ".bmp");
            }

            if (!IsPathAllowed(outputPath)) {
                return R"({"success":false,"error":"access denied"})";
            }

            std::error_code ec;
            fs::create_directories(outputPath.parent_path(), ec);
            if (ec) {
                return std::string("{\"success\":false,\"error\":") + JsonEscape("failed to create output directory") + "}";
            }

            std::vector<std::uint8_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);
            const uint32_t seed = static_cast<uint32_t>(std::hash<std::string>{}(prompt + "|" + style));
            const uint8_t r0 = static_cast<uint8_t>((seed >> 0) & 0xFF);
            const uint8_t g0 = static_cast<uint8_t>((seed >> 8) & 0xFF);
            const uint8_t b0 = static_cast<uint8_t>((seed >> 16) & 0xFF);
            const uint8_t r1 = static_cast<uint8_t>((seed >> 24) & 0xFF);
            const uint8_t g1 = static_cast<uint8_t>((seed >> 4) & 0xFF);
            const uint8_t b1 = static_cast<uint8_t>((seed >> 12) & 0xFF);

            for (int y = 0; y < height; ++y) {
                const float t = static_cast<float>(y) / static_cast<float>(std::max(1, height - 1));
                const std::uint8_t rr = static_cast<uint8_t>(r0 + static_cast<uint8_t>((r1 - r0) * t));
                const std::uint8_t gg = static_cast<uint8_t>(g0 + static_cast<uint8_t>((g1 - g0) * t));
                const std::uint8_t bb = static_cast<uint8_t>(b0 + static_cast<uint8_t>((b1 - b0) * t));
                for (int x = 0; x < width; ++x) {
                    const size_t idx = static_cast<size_t>((y * width + x) * 4);
                    rgba[idx + 0] = rr;
                    rgba[idx + 1] = gg;
                    rgba[idx + 2] = bb;
                    rgba[idx + 3] = 255;
                }
            }

            const int circles = 8 + static_cast<int>(seed % 9);
            for (int i = 0; i < circles; ++i) {
                const uint32_t s = seed ^ static_cast<uint32_t>(i * 2654435761u);
                const float cx = static_cast<float>(s % static_cast<uint32_t>(width));
                const float cy = static_cast<float>((s >> 8) % static_cast<uint32_t>(height));
                const float radius =
                    static_cast<float>((s >> 16) % static_cast<uint32_t>(std::max(16, std::min(width, height) / 2))) +
                    12.0f;
                const std::uint8_t rr = static_cast<std::uint8_t>((s >> 4) & 0xFF);
                const std::uint8_t gg = static_cast<std::uint8_t>((s >> 10) & 0xFF);
                const std::uint8_t bb = static_cast<std::uint8_t>((s >> 18) & 0xFF);
                BlendCircle(rgba, width, height, cx, cy, radius, rr, gg, bb, 84);
            }

            if (!WriteBmp32(outputPath, width, height, rgba)) {
                return R"({"success":false,"error":"image write failed"})";
            }

            json resp;
            resp["success"] = true;
            resp["tool"] = "generate_image";
            resp["prompt"] = prompt;
            resp["style"] = style;
            resp["width"] = width;
            resp["height"] = height;
            resp["output_path"] = outputPath.string();
            return resp.dump();
        } catch (const std::exception& e) {
            return std::string("{\"success\":false,\"error\":") + JsonEscape(e.what()) + "}";
        }
    });

    ToolRegistry::register_tool("generate_video", [](const std::string& input) -> std::string {
        try {
            const json req = json::parse(input.empty() ? "{}" : input);
            const std::string prompt = req.value("prompt", "");
            if (prompt.empty()) {
                return R"({"success":false,"error":"missing prompt"})";
            }

            rawrxd::video::TubiRenderRequest request;
            request.jobId = req.value("job_id", std::string());
            if (request.jobId.empty()) {
                const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                request.jobId = "video_" + std::to_string(nowMs);
            }

            request.engineName = req.value("engine", std::string("tubi"));
            request.provider = req.value("provider", std::string("local"));
            request.localModel = req.value("local_model", std::string("headless-default"));
            request.prompt = prompt;
            request.storyboard = req.value("storyboard", prompt);
            request.style = req.value("style", std::string("Cinematic"));
            request.duration = req.value("duration", std::string("6s"));
            request.aspectRatio = req.value("aspect_ratio", std::string("16:9"));
            request.resolution = req.value("resolution", std::string("720p"));
            request.negativePrompt = req.value("negative_prompt", std::string("blurry, low detail"));
            request.cameraMode = req.value("camera_mode", std::string("cinematic-pan"));
            request.seed = req.value("seed", static_cast<int>(std::hash<std::string>{}(request.prompt) & 0x7fffffff));

            if (req.contains("output_dir") && req["output_dir"].is_string()) {
                request.outputDir = fs::path(req["output_dir"].get<std::string>());
                if (!request.outputDir.is_absolute()) {
                    request.outputDir = fs::path(WorkspaceRoot()) / request.outputDir;
                }
            } else {
                request.outputDir = fs::path(WorkspaceRoot()) / "generated" / "videos" / request.jobId;
            }

            if (!IsPathAllowed(request.outputDir)) {
                return R"({"success":false,"error":"access denied"})";
            }

            std::error_code ec;
            fs::create_directories(request.outputDir, ec);
            if (ec) {
                return std::string("{\"success\":false,\"error\":") + JsonEscape("failed to create output directory") + "}";
            }

            const auto rendered = rawrxd::video::renderVideoClip(request);
            if (!rendered) {
                return std::string("{\"success\":false,\"error\":") + JsonEscape(rendered.error()) + "}";
            }

            json resp;
            resp["success"] = true;
            resp["tool"] = "generate_video";
            resp["job_id"] = request.jobId;
            resp["prompt"] = request.prompt;
            resp["style"] = request.style;
            resp["duration"] = request.duration;
            resp["resolution"] = request.resolution;
            resp["aspect_ratio"] = request.aspectRatio;
            resp["output_dir"] = request.outputDir.string();
            resp["manifest_path"] = rendered->manifestPath.string();
            resp["mp4_path"] = rendered->mp4Path.string();
            resp["frames_dir"] = rendered->framesDir.string();
            resp["total_frames"] = rendered->totalFrames;
            return resp.dump();
        } catch (const std::exception& e) {
            return std::string("{\"success\":false,\"error\":") + JsonEscape(e.what()) + "}";
        }
    });
}

} // namespace

void ToolRegistry::register_tool(const std::string& name, ToolFunc fn) {
    std::lock_guard<std::mutex> lock(tools_mutex);
    tools[name] = fn;
}

std::string ToolRegistry::list_tools() {
    std::lock_guard<std::mutex> lock(tools_mutex);
    std::stringstream ss;
    for (const auto& kv : tools) {
        ss << "- " << kv.first << "\n";
    }
    return ss.str();
}

void ToolRegistry::inject_tools(AgentRequest& req) {
    const std::string tools_snapshot = list_tools();
    if (!tools_snapshot.empty()) {
        req.prompt = "[TOOLS AVAILABLE]\n" + tools_snapshot + "\n" + req.prompt;
    }
}

bool ToolRegistry::has_tool(const std::string& name) {
    std::lock_guard<std::mutex> lock(tools_mutex);
    return tools.find(name) != tools.end();
}

bool ToolRegistry::execute_tool(const std::string& name, const std::string& params_json, std::string& out_result) {
    ToolFunc fn;
    {
        std::lock_guard<std::mutex> lock(tools_mutex);
        const auto it = tools.find(name);
        if (it == tools.end()) {
            return false;
        }
        fn = it->second;
    }
    out_result = fn(params_json);
    return true;
}

void ToolRegistry::ensure_core_tools() {
    std::call_once(tools_once, []() {
        RegisterCoreTools();
        register_rawr_inference();
        register_git_mcp_tools();
        register_sovereign_engines();
    });
}

extern "C" __declspec(dllexport) void* __stdcall InitializeToolRegistry() {
    ToolRegistry::ensure_core_tools();
    return reinterpret_cast<void*>(1);
}

extern "C" __declspec(dllexport) int __stdcall ExecuteToolByName(
    const char* tool_name,
    const char* json_params,
    char* result_buffer,
    unsigned long long buffer_size) {
    if (tool_name == nullptr || result_buffer == nullptr || buffer_size == 0) {
        return -32600;
    }

    ToolRegistry::ensure_core_tools();
    std::string result;
    const std::string params = (json_params != nullptr) ? json_params : "{}";
    if (!ToolRegistry::execute_tool(tool_name, params, result)) {
        return -32601;
    }

    const size_t max_copy = static_cast<size_t>(buffer_size - 1);
    const size_t n = std::min(max_copy, result.size());
    memcpy(result_buffer, result.data(), n);
    result_buffer[n] = 0;
    return 0;
}
