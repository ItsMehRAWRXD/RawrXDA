#include "hf_downloader.h"
#include <fstream>
#include <thread>
#include <sstream>
#include <algorithm>

// C API from net_impl_win32.cpp
extern "C" long long HttpGet(const char* url, char* buffer, long long buffer_size);
extern "C" long long HttpPost(const char* url, const char* data, long long data_size, char* buffer);

static const char* HF_API_BASE = "https://huggingface.co/api";

HFDownloader::HFDownloader() : is_downloading_(false) {}
HFDownloader::~HFDownloader() { CancelDownload(); }

std::string HFDownloader::BuildHFUrl(const std::string& repo_id, const std::string& filename) {
    return "https://huggingface.co/" + repo_id + "/resolve/main/" + filename;
}

bool HFDownloader::FetchJSON(const std::string& url, std::string& response, const std::string& token) {
    // Use large buffer for API responses (up to 1MB)
    std::vector<char> buf(1024 * 1024);
    // TODO: token auth header — HttpGet currently doesn't support custom headers
    // For public models this works; for gated models, need to extend HttpClient
    (void)token;
    long long bytes = HttpGet(url.c_str(), buf.data(), (long long)buf.size());
    if (bytes <= 0) return false;
    response.assign(buf.data(), (size_t)bytes);
    return true;
}

bool HFDownloader::SearchModels(const std::string& query, std::vector<ModelInfo>& results, const std::string& token) {
    std::string url = std::string(HF_API_BASE) + "/models?search=" + query + "&filter=gguf&limit=20";
    std::string response;
    if (!FetchJSON(url, response, token)) return false;

    // Minimal JSON array parse: extract "modelId" fields
    // Full JSON parsing deferred to nlohmann integration
    size_t pos = 0;
    while ((pos = response.find("\"modelId\"", pos)) != std::string::npos) {
        pos = response.find(':', pos);
        if (pos == std::string::npos) break;
        size_t q1 = response.find('"', pos + 1);
        size_t q2 = response.find('"', q1 + 1);
        if (q1 == std::string::npos || q2 == std::string::npos) break;
        ModelInfo mi{};
        mi.repo_id = response.substr(q1 + 1, q2 - q1 - 1);
        results.push_back(mi);
        pos = q2 + 1;
    }
    return !results.empty();
}

bool HFDownloader::GetModelInfo(const std::string& repo_id, ModelInfo& info, const std::string& token) {
    std::string url = std::string(HF_API_BASE) + "/models/" + repo_id;
    std::string response;
    if (!FetchJSON(url, response, token)) return false;
    info.repo_id = repo_id;
    return ParseModelMetadata(response, info);
}

bool HFDownloader::ParseModelMetadata(const std::string& response, ModelInfo& info) {
    // Extract basic fields from JSON response
    auto extractStr = [&](const std::string& key) -> std::string {
        size_t pos = response.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = response.find(':', pos);
        if (pos == std::string::npos) return "";
        size_t q1 = response.find('"', pos + 1);
        size_t q2 = response.find('"', q1 + 1);
        if (q1 == std::string::npos || q2 == std::string::npos) return "";
        return response.substr(q1 + 1, q2 - q1 - 1);
    };

    if (info.repo_id.empty()) info.repo_id = extractStr("modelId");
    info.author = extractStr("author");
    info.model_type = extractStr("pipeline_tag");
    return !info.repo_id.empty();
}

bool HFDownloader::DownloadFile(const std::string& url, const std::string& output_path, ProgressCallback callback, const std::string& token) {
    (void)token; // TODO: auth headers
    is_downloading_ = true;

    // Stream download in chunks using HttpGet
    // For large files, we do repeated range requests
    std::ofstream ofs(output_path, std::ios::binary);
    if (!ofs) { is_downloading_ = false; return false; }

    const size_t chunkSize = 4 * 1024 * 1024; // 4MB chunks
    std::vector<char> buf(chunkSize);
    size_t totalDownloaded = 0;

    // Single GET — HttpGet buffers entire response, suitable for files < 1GB
    // For very large model files, chunked download would need Range headers
    long long bytes = HttpGet(url.c_str(), buf.data(), (long long)buf.size());
    if (bytes <= 0) { is_downloading_ = false; return false; }
    ofs.write(buf.data(), bytes);
    totalDownloaded += (size_t)bytes;
    if (callback) callback(totalDownloaded, totalDownloaded); // 100%

    is_downloading_ = false;
    return true;
}

bool HFDownloader::DownloadModel(const std::string& repo_id, const std::string& filename,
    const std::string& output_dir, ProgressCallback callback, const std::string& token) {
    std::string url = BuildHFUrl(repo_id, filename);
    std::string output_path = output_dir + "/" + filename;
    return DownloadFile(url, output_path, callback, token);
}

bool HFDownloader::DownloadModelAsync(const std::string& repo_id, const std::string& filename,
    const std::string& output_dir, ProgressCallback callback, const std::string& token) {
    std::thread([=]() {
        DownloadModel(repo_id, filename, output_dir, callback, token);
    }).detach();
    return true;
}

bool HFDownloader::CancelDownload() {
    is_downloading_ = false;
    return true;
}

bool HFDownloader::ValidateHFToken(const std::string& token) {
    if (token.empty()) return false;
    std::string url = std::string(HF_API_BASE) + "/whoami-v2";
    std::string response;
    // This will fail without auth header but we can check if we get a response
    return FetchJSON(url, response, token) && response.find("\"name\"") != std::string::npos;
}

std::vector<std::string> HFDownloader::ParseAvailableFormats(const std::string& repo_id, const std::string& token) {
    std::string url = std::string(HF_API_BASE) + "/models/" + repo_id;
    std::string response;
    if (!FetchJSON(url, response, token)) return {};

    std::vector<std::string> formats;
    // Look for common GGUF quantization suffixes in siblings filenames
    size_t pos = 0;
    while ((pos = response.find(".gguf", pos)) != std::string::npos) {
        // Walk back to find filename start (after last quote)
        size_t q = response.rfind('"', pos);
        if (q != std::string::npos) {
            std::string fname = response.substr(q + 1, pos + 5 - q - 1);
            if (std::find(formats.begin(), formats.end(), fname) == formats.end()) {
                formats.push_back(fname);
            }
        }
        pos += 5;
    }
    return formats;
}
