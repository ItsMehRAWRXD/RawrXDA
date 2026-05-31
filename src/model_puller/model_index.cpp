// ============================================================================
// model_index.cpp — Local Model Registry Implementation
// ============================================================================
// JSON-backed model index stored at %APPDATA%\RawrXD\models\index.json.
// ============================================================================

#include "model_puller/model_index.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

using json = nlohmann::json;

namespace RawrXD {

// ============================================================================
// JSON serialization helpers
// ============================================================================
static json EntryToJson(const ModelEntry& e) {
    json j;
    j["id"]           = e.id;
    j["name"]         = e.name;
    j["quantization"] = e.quantization;
    j["path"]         = e.path;
    j["absolutePath"]  = e.absolutePath;
    j["size_bytes"]   = e.sizeBytes;
    j["sha256"]       = e.sha256;
    j["source"]       = e.source;
    j["downloaded_at"] = e.downloadedAt;
    j["architecture"] = e.architecture;
    j["tags"]         = e.tags;
    j["active"]       = e.active;
    return j;
}

static ModelEntry JsonToEntry(const json& j) {
    ModelEntry e;
    e.id           = j.value("id", "");
    e.name         = j.value("name", "");
    e.quantization = j.value("quantization", "");
    e.path         = j.value("path", "");
    e.absolutePath = j.value("absolutePath", j.value("absolute_path", ""));
    e.sizeBytes    = j.value("size_bytes", uint64_t(0));
    e.sha256       = j.value("sha256", "");
    e.source       = j.value("source", "");
    e.downloadedAt = j.value("downloaded_at", "");
    e.architecture = j.value("architecture", "");
    e.tags         = j.value("tags", "");
    e.active       = j.value("active", false);
    return e;
}

// ============================================================================
// Construction
// ============================================================================
ModelIndex::ModelIndex()  = default;
ModelIndex::~ModelIndex() = default;

// ============================================================================
// Initialize — determine base path, load existing
// ============================================================================
bool ModelIndex::Initialize() {
    // Default: %APPDATA%\RawrXD\models
#ifdef _WIN32
    wchar_t appDataW[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appDataW))) {
        char buf[MAX_PATH * 3] = {};
        int len = WideCharToMultiByte(CP_UTF8, 0, appDataW, -1, buf, sizeof(buf), nullptr, nullptr);
        if (len > 0) {
            m_basePath = std::string(buf) + "\\RawrXD\\models";
        }
    }
#else
    const char* home = getenv("HOME");
    if (home) {
        m_basePath = std::string(home) + "/.rawrxd/models";
    }
#endif

    if (m_basePath.empty()) {
        m_basePath = "models"; // fallback: current dir
    }

    std::error_code ec;
    std::filesystem::create_directories(m_basePath, ec);

    return Load();
}

bool ModelIndex::Initialize(const std::string& customBasePath) {
    m_basePath = customBasePath;
    std::error_code ec;
    std::filesystem::create_directories(m_basePath, ec);
    return Load();
}

// ============================================================================
// CRUD
// ============================================================================
bool ModelIndex::AddModel(const ModelEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for duplicate ID
    for (auto& m : m_models) {
        if (m.id == entry.id) {
            m = entry; // update existing
            return Save();
        }
    }

    m_models.push_back(entry);
    return Save();
}

bool ModelIndex::RemoveModel(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::remove_if(m_models.begin(), m_models.end(),
        [&](const ModelEntry& e) { return e.id == id; });
    if (it == m_models.end()) return false;

    m_models.erase(it, m_models.end());
    return Save();
}

bool ModelIndex::UpdateModel(const ModelEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& m : m_models) {
        if (m.id == entry.id) {
            m = entry;
            return Save();
        }
    }
    return false;
}

bool ModelIndex::SetActive(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    bool found = false;
    for (auto& m : m_models) {
        if (m.id == id) {
            m.active = true;
            found = true;
        } else {
            m.active = false;
        }
    }

    if (found) Save();
    return found;
}

// ============================================================================
// Queries
// ============================================================================
bool ModelIndex::GetModel(const std::string& id, ModelEntry& out) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& m : m_models) {
        if (m.id == id) { out = m; return true; }
    }
    return false;
}

bool ModelIndex::GetModelByPath(const std::string& absPath, ModelEntry& out) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& m : m_models) {
        if (m.absolutePath == absPath) { out = m; return true; }
    }
    return false;
}

std::vector<ModelEntry> ModelIndex::GetAllModels() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_models;
}

const ModelEntry* ModelIndex::GetActiveModel() const {
    // Note: caller must hold lock or call is only safe for single-threaded usage
    for (auto& m : m_models) {
        if (m.active) return &m;
    }
    return nullptr;
}

bool ModelIndex::HasModel(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& m : m_models) {
        if (m.id == id) return true;
    }
    return false;
}

// ============================================================================
// Search — matches name, id, quantization, tags
// ============================================================================
std::vector<ModelEntry> ModelIndex::Search(const std::string& query) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ModelEntry> results;

    std::string lowerQ = query;
    std::transform(lowerQ.begin(), lowerQ.end(), lowerQ.begin(), ::tolower);

    for (auto& m : m_models) {
        auto contains = [&](const std::string& field) {
            std::string lower = field;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return lower.find(lowerQ) != std::string::npos;
        };

        if (contains(m.id) || contains(m.name) || contains(m.quantization) ||
            contains(m.tags) || contains(m.source) || contains(m.architecture)) {
            results.push_back(m);
        }
    }

    return results;
}

// ============================================================================
// Persistence — JSON file I/O
// ============================================================================
std::string ModelIndex::GetIndexFilePath() const {
    return (std::filesystem::path(m_basePath) / "index.json").string();
}

bool ModelIndex::Save() const {
    // Caller should already hold m_mutex (all public callers do)
    json root;
    root["version"] = 1;
    json arr = json::array();
    for (auto& m : m_models) {
        arr.push_back(EntryToJson(m));
    }
    root["models"] = arr;

    std::string path = GetIndexFilePath();

    // Write to temp file then rename for atomicity
    std::string tmpPath = path + ".tmp";
    {
        std::ofstream ofs(tmpPath, std::ios::trunc);
        if (!ofs.is_open()) {
            std::cerr << "[ModelIndex] Failed to open " << tmpPath << " for writing\n";
            return false;
        }
        ofs << root.dump(2);
        ofs.flush();
        if (!ofs.good()) {
            std::cerr << "[ModelIndex] Write error to " << tmpPath << "\n";
            return false;
        }
    }

    // Atomic rename
    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        // Fallback: copy
        try {
            std::filesystem::copy_file(tmpPath, path, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmpPath, ec);
        } catch (...) {
            return false;
        }
    }

    return true;
}

bool ModelIndex::Load() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_models.clear();

    std::string path = GetIndexFilePath();
    if (!std::filesystem::exists(path)) {
        return true; // empty registry is valid
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    ifs.close();

    if (content.empty()) return true;

    try {
        json root = json::parse(content);

        if (root.contains("models") && root["models"].is_array()) {
            for (auto& jm : root["models"]) {
                m_models.push_back(JsonToEntry(jm));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ModelIndex] JSON parse error: " << e.what() << "\n";
        return false;
    }

    return true;
}

// ============================================================================
// Utilities
// ============================================================================
std::string ModelIndex::GenerateId(const std::string& name, const std::string& quant) {
    // e.g. "Qwen2.5-Coder-32B-Instruct" + "Q4_K_M" → "qwen25-coder-32b-instruct-q4km"
    std::string combined = name;
    if (!quant.empty()) {
        combined += "-" + quant;
    }

    std::string id;
    id.reserve(combined.size());
    for (char c : combined) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            id += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (c == '-' || c == '_') {
            if (!id.empty() && id.back() != '-') {
                id += '-';
            }
        }
        // Skip dots, spaces, etc.
    }

    // Trim trailing dash
    while (!id.empty() && id.back() == '-') {
        id.pop_back();
    }

    return id;
}

std::string ModelIndex::NowISO8601() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    struct tm tmBuf = {};
#ifdef _WIN32
    gmtime_s(&tmBuf, &t);
#else
    gmtime_r(&t, &tmBuf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);
    return std::string(buf);
}

} // namespace RawrXD
