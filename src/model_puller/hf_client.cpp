// ============================================================================
// hf_client.cpp — Hugging Face Hub API Client Implementation
// ============================================================================
// Uses WinHTTP-based DownloadManager to talk to HF REST API.
// Parses JSON responses with nlohmann/json.
// ============================================================================

#include "model_puller/hf_client.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <regex>

using json = nlohmann::json;

namespace RawrXD {

static const char* HF_API_BASE = "https://huggingface.co/api";

// ============================================================================
// Construction
// ============================================================================
HuggingFaceClient::HuggingFaceClient()  = default;
HuggingFaceClient::~HuggingFaceClient() = default;

void HuggingFaceClient::SetToken(const std::string& hfToken) {
    m_token = hfToken;
}

std::string HuggingFaceClient::MakeAuthHeader() const {
    if (m_token.empty()) return {};
    return "Bearer " + m_token;
}

// ============================================================================
// BuildDownloadUrl
// ============================================================================
std::string HuggingFaceClient::BuildDownloadUrl(const std::string& repoId,
                                                 const std::string& filename) {
    // https://huggingface.co/{repo}/resolve/main/{filename}
    return "https://huggingface.co/" + repoId + "/resolve/main/" + filename;
}

// ============================================================================
// ExtractQuantization — parse "Q4_K_M" from filenames
// ============================================================================
std::string HuggingFaceClient::ExtractQuantization(const std::string& filename) {
    // Common GGUF quantization patterns:
    //   model-Q4_K_M.gguf, model.Q5_K_S.gguf, model-IQ2_XXS.gguf, etc.
    static const std::regex quantRe(
        R"(((?:I?Q[0-9]+_(?:[A-Z0-9]+_?)*[A-Z0-9]+)|(?:F(?:16|32))|(?:BF16)))",
        std::regex::icase);

    std::smatch match;
    if (std::regex_search(filename, match, quantRe)) {
        std::string q = match[1].str();
        // Normalize to uppercase
        std::transform(q.begin(), q.end(), q.begin(), ::toupper);
        return q;
    }
    return {};
}

// ============================================================================
// ListGGUFFiles — GET /api/models/{repo}/tree/main, filter .gguf
// ============================================================================
bool HuggingFaceClient::ListGGUFFiles(const std::string& repoId,
                                       std::vector<HFFileInfo>& filesOut) {
    filesOut.clear();
    std::string url = std::string(HF_API_BASE) + "/models/" + repoId + "/tree/main";
    std::string body;

    if (!m_downloader.FetchJSON(url, body, MakeAuthHeader())) {
        std::cerr << "[HF] Failed to list files for " << repoId << "\n";
        return false;
    }

    try {
        json arr = json::parse(body);
        if (!arr.is_array()) return false;

        for (auto& item : arr) {
            std::string type = item.value("type", "");
            if (type != "file") continue;

            std::string rfilename = item.value("rfilename", "");
            if (rfilename.empty()) {
                // Some API responses use "path" instead
                rfilename = item.value("path", "");
            }
            if (rfilename.empty()) continue;

            // Filter to .gguf only
            if (rfilename.size() < 5) continue;
            std::string ext = rfilename.substr(rfilename.size() - 5);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".gguf") continue;

            HFFileInfo fi;
            fi.rfilename = rfilename;
            // Filename is the last component
            auto lastSlash = rfilename.rfind('/');
            fi.filename = (lastSlash != std::string::npos)
                ? rfilename.substr(lastSlash + 1) : rfilename;

            // Size (the HF tree API returns "size" as number)
            if (item.contains("size") && item["size"].is_number()) {
                fi.sizeBytes = item["size"].get<uint64_t>();
            }

            // SHA256 from lfs info
            if (item.contains("lfs") && item["lfs"].is_object()) {
                auto& lfs = item["lfs"];
                fi.sha256 = lfs.value("sha256", "");
                if (fi.sizeBytes == 0 && lfs.contains("size")) {
                    fi.sizeBytes = lfs["size"].get<uint64_t>();
                }
            }

            fi.quantization = ExtractQuantization(fi.filename);
            filesOut.push_back(std::move(fi));
        }
    } catch (const std::exception& e) {
        std::cerr << "[HF] JSON parse error: " << e.what() << "\n";
        return false;
    }

    // Sort by size descending (largest quant first)
    std::sort(filesOut.begin(), filesOut.end(),
        [](const HFFileInfo& a, const HFFileInfo& b) {
            return a.sizeBytes > b.sizeBytes;
        });

    return true;
}

// ============================================================================
// GetRepoInfo
// ============================================================================
bool HuggingFaceClient::GetRepoInfo(const std::string& repoId, HFRepoInfo& infoOut) {
    infoOut = {};
    infoOut.repoId = repoId;

    std::string url = std::string(HF_API_BASE) + "/models/" + repoId;
    std::string body;

    if (!m_downloader.FetchJSON(url, body, MakeAuthHeader())) {
        return false;
    }

    try {
        json j = json::parse(body);
        infoOut.modelName = j.value("modelId", repoId);
        infoOut.author    = j.value("author", "");
        infoOut.downloads = j.value("downloads", int64_t(0));
        infoOut.likes     = j.value("likes", int64_t(0));

        // Try to get card description
        if (j.contains("cardData") && j["cardData"].is_object()) {
            // Description may be in various fields
        }
    } catch (const std::exception&) {
        // Non-critical: we already have repoId
    }

    // Also list GGUF files
    ListGGUFFiles(repoId, infoOut.ggufFiles);

    return true;
}

// ============================================================================
// SearchModels
// ============================================================================
bool HuggingFaceClient::SearchModels(const std::string& query,
                                      std::vector<HFRepoInfo>& resultsOut,
                                      int limit) {
    resultsOut.clear();

    // URL-encode query (minimal: spaces → +)
    std::string encodedQuery = query;
    for (auto& c : encodedQuery) {
        if (c == ' ') c = '+';
    }

    std::string url = std::string(HF_API_BASE) + "/models?search=" + encodedQuery
                    + "&filter=gguf&sort=downloads&direction=-1&limit="
                    + std::to_string(limit);
    std::string body;

    if (!m_downloader.FetchJSON(url, body, MakeAuthHeader())) {
        return false;
    }

    try {
        json arr = json::parse(body);
        if (!arr.is_array()) return false;

        for (auto& item : arr) {
            HFRepoInfo info;
            info.repoId   = item.value("modelId", item.value("id", ""));
            info.author    = item.value("author", "");
            info.downloads = item.value("downloads", int64_t(0));
            info.likes     = item.value("likes", int64_t(0));

            if (!info.repoId.empty()) {
                resultsOut.push_back(std::move(info));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[HF] Search parse error: " << e.what() << "\n";
        return false;
    }

    return true;
}

// ============================================================================
// Download
// ============================================================================
bool HuggingFaceClient::Download(const std::string& repoId,
                                  const std::string& filename,
                                  const std::string& destPath,
                                  ProgressCallback onProgress,
                                  bool resume) {
    std::string url = BuildDownloadUrl(repoId, filename);
    return m_downloader.Download(url, destPath, onProgress, resume, MakeAuthHeader());
}

void HuggingFaceClient::Cancel() {
    m_downloader.Cancel();
}

} // namespace RawrXD
