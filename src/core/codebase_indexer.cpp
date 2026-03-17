#pragma once
#ifndef CODEBASE_INDEXER_HPP
#define CODEBASE_INDEXER_HPP

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <array>
#include <functional>
#include <regex>
#include <numeric>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
//  Structs
// ─────────────────────────────────────────────────────────────────────────────

struct SymbolRecord {
    std::string name;
    std::string kind;       // "function", "class", "struct", "variable", "enum", "namespace"
    std::string file;
    int line;
    int column;
    std::string signature;  // full declaration text
    std::array<float, 64> embedding;
};

struct IndexStats {
    size_t fileCount;
    size_t symbolCount;
    size_t indexSizeBytes;
    double indexTimeMs;
    std::string lastUpdated;
};

class CodebaseIndexer {
public:
    static CodebaseIndexer& instance();
    void indexDirectory(const std::string& root, const std::vector<std::string>& extensions);
    void indexFile(const std::string& path);
    std::vector<SymbolRecord> search(const std::string& query, int topK = 10);
    std::vector<SymbolRecord> findByName(const std::string& name);
    std::vector<SymbolRecord> findByKind(const std::string& kind);
    std::vector<SymbolRecord> findInFile(const std::string& file);
    void rebuildIndex();
    void clearIndex();
    IndexStats getStats() const;
    bool saveIndex(const std::string& path) const;
    bool loadIndex(const std::string& path);
    void setOnIndexed(std::function<void(const std::string&, size_t)> cb);
private:
    CodebaseIndexer();
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // CODEBASE_INDEXER_HPP

// ─────────────────────────────────────────────────────────────────────────────
//  Implementation
// ─────────────────────────────────────────────────────────────────────────────

static constexpr uint32_t INDEX_MAGIC   = 0x52415752u; // "RAWR"
static constexpr uint32_t INDEX_VERSION = 1u;

// Helper: generate a 64-dim embedding from a symbol name.
// For each dimension i ∈ [0,64):
//   val[i] = Σ_j  ( char_name[j] * sin(i * j * 0.1) )
// Then normalise to unit vector so cosine similarity == dot product.
static std::array<float, 64> makeEmbedding(const std::string& name) {
    std::array<float, 64> v{};
    v.fill(0.0f);
    for (size_t j = 0; j < name.size(); ++j) {
        float ch = static_cast<float>(static_cast<unsigned char>(name[j]));
        for (int i = 0; i < 64; ++i) {
            v[i] += ch * std::sin(static_cast<float>(i) * static_cast<float>(j) * 0.1f);
        }
    }
    // Normalise
    float norm = 0.0f;
    for (float x : v) norm += x * x;
    norm = std::sqrt(norm);
    if (norm > 1e-9f) {
        for (float& x : v) x /= norm;
    }
    return v;
}

// Helper: get ISO-8601-like timestamp string
static std::string nowTimestamp() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond);
    return buf;
}

// Helper: write length-prefixed string to binary stream
static void writeLPString(std::ofstream& ofs, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len > 0) ofs.write(s.data(), len);
}

// Helper: read length-prefixed string from binary stream
static bool readLPString(std::ifstream& ifs, std::string& out) {
    uint32_t len = 0;
    if (!ifs.read(reinterpret_cast<char*>(&len), sizeof(len))) return false;
    out.resize(len);
    if (len > 0 && !ifs.read(&out[0], len)) return false;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pimpl body
// ─────────────────────────────────────────────────────────────────────────────

struct CodebaseIndexer::Impl {
    std::vector<SymbolRecord>                        symbols_;
    std::unordered_map<std::string, std::vector<size_t>> nameIndex_;
    std::unordered_map<std::string, std::vector<size_t>> fileIndex_;
    mutable std::mutex                               mutex_;
    std::atomic<bool>                                indexing_{ false };
    std::function<void(const std::string&, size_t)> onIndexed_;
    IndexStats                                       stats_{};

    // ── index a single file ──────────────────────────────────────────────────
    void doIndexFile(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return;

        // Patterns (POSIX ERE via std::regex)
        // Function: return-type name(
        std::regex rFunc(
            R"(^\s*(?:(?:inline|static|virtual|explicit|constexpr|extern|friend)\s+)*)"
            R"([\w:*&<>\[\]]+(?:\s*[\*&])?\s+(?:[\w:]+::)*(\w+)\s*\()",
            std::regex::optimize);
        // Class / struct
        std::regex rClass(
            R"(^\s*(class|struct)\s+(\w+))",
            std::regex::optimize);
        // Enum
        std::regex rEnum(
            R"(^\s*enum\s+(class\s+)?(\w+))",
            std::regex::optimize);
        // Namespace
        std::regex rNS(
            R"(^\s*namespace\s+(\w+))",
            std::regex::optimize);
        // Global / member variable (simple): ends with identifier ;
        std::regex rVar(
            R"(^\s*(?:static\s+)?(?:const\s+)?(?:constexpr\s+)?[\w:*&<>\[\]]+\s+(\w+)\s*;)",
            std::regex::optimize);

        std::vector<SymbolRecord> found;
        std::string line;
        int lineNo = 0;

        while (std::getline(ifs, line)) {
            ++lineNo;
            std::smatch m;
            SymbolRecord rec{};
            rec.file      = path;
            rec.line      = lineNo;
            rec.column    = 1;
            rec.signature = line;

            bool matched = false;

            if (std::regex_search(line, m, rClass)) {
                rec.kind = (m[1].str() == "struct") ? "struct" : "class";
                rec.name = m[2].str();
                matched  = true;
            } else if (std::regex_search(line, m, rEnum)) {
                rec.kind = "enum";
                rec.name = m[2].str();
                matched  = true;
            } else if (std::regex_search(line, m, rNS)) {
                rec.kind = "namespace";
                rec.name = m[1].str();
                matched  = true;
            } else if (std::regex_search(line, m, rFunc)) {
                rec.kind = "function";
                rec.name = m[1].str();
                matched  = true;
            } else if (std::regex_search(line, m, rVar)) {
                rec.kind = "variable";
                rec.name = m[1].str();
                matched  = true;
            }

            if (matched && !rec.name.empty()) {
                rec.embedding = makeEmbedding(rec.name);
                found.push_back(std::move(rec));
            }
        }

        if (found.empty()) return;

        {
            std::lock_guard<std::mutex> lk(mutex_);
            size_t base = symbols_.size();
            for (size_t i = 0; i < found.size(); ++i) {
                size_t idx = base + i;
                nameIndex_[found[i].name].push_back(idx);
                fileIndex_[found[i].file].push_back(idx);
            }
            for (auto& r : found) symbols_.push_back(std::move(r));
        }

        if (onIndexed_) onIndexed_(path, found.size());
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  CodebaseIndexer public API
// ─────────────────────────────────────────────────────────────────────────────

CodebaseIndexer::CodebaseIndexer()
    : m_impl(std::make_unique<Impl>()) {}

CodebaseIndexer& CodebaseIndexer::instance() {
    static CodebaseIndexer singleton;
    return singleton;
}

void CodebaseIndexer::setOnIndexed(std::function<void(const std::string&, size_t)> cb) {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    m_impl->onIndexed_ = std::move(cb);
}

void CodebaseIndexer::indexFile(const std::string& path) {
    m_impl->doIndexFile(path);
}

void CodebaseIndexer::indexDirectory(const std::string& root,
                                     const std::vector<std::string>& extensions) {
    bool expected = false;
    if (!m_impl->indexing_.compare_exchange_strong(expected, true))
        return; // already running

    auto t0 = std::chrono::high_resolution_clock::now();
    size_t fileCount = 0;

    namespace fs = std::filesystem;
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        bool match = false;
        for (auto& e : extensions) {
            if (ext == e) { match = true; break; }
        }
        if (!match) continue;

        m_impl->doIndexFile(entry.path().string());
        ++fileCount;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    {
        std::lock_guard<std::mutex> lk(m_impl->mutex_);
        m_impl->stats_.fileCount      = fileCount;
        m_impl->stats_.symbolCount    = m_impl->symbols_.size();
        m_impl->stats_.indexSizeBytes = m_impl->symbols_.size()
                                       * (sizeof(SymbolRecord));
        m_impl->stats_.indexTimeMs    = ms;
        m_impl->stats_.lastUpdated    = nowTimestamp();
    }

    m_impl->indexing_.store(false);
}

std::vector<SymbolRecord> CodebaseIndexer::search(const std::string& query, int topK) {
    std::array<float, 64> qEmb = makeEmbedding(query);

    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    const auto& syms = m_impl->symbols_;
    if (syms.empty()) return {};

    // Build index-score pairs
    std::vector<std::pair<float, size_t>> scores;
    scores.reserve(syms.size());
    for (size_t i = 0; i < syms.size(); ++i) {
        float dot = 0.0f;
        for (int d = 0; d < 64; ++d)
            dot += qEmb[d] * syms[i].embedding[d];
        scores.emplace_back(dot, i);
    }

    int k = std::min(topK, static_cast<int>(scores.size()));
    std::partial_sort(scores.begin(), scores.begin() + k, scores.end(),
        [](const std::pair<float,size_t>& a, const std::pair<float,size_t>& b){
            return a.first > b.first;
        });

    std::vector<SymbolRecord> result;
    result.reserve(k);
    for (int i = 0; i < k; ++i)
        result.push_back(syms[scores[i].second]);
    return result;
}

std::vector<SymbolRecord> CodebaseIndexer::findByName(const std::string& name) {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    auto it = m_impl->nameIndex_.find(name);
    if (it == m_impl->nameIndex_.end()) return {};
    std::vector<SymbolRecord> out;
    out.reserve(it->second.size());
    for (size_t idx : it->second)
        out.push_back(m_impl->symbols_[idx]);
    return out;
}

std::vector<SymbolRecord> CodebaseIndexer::findByKind(const std::string& kind) {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    std::vector<SymbolRecord> out;
    for (auto& s : m_impl->symbols_)
        if (s.kind == kind) out.push_back(s);
    return out;
}

std::vector<SymbolRecord> CodebaseIndexer::findInFile(const std::string& file) {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    auto it = m_impl->fileIndex_.find(file);
    if (it == m_impl->fileIndex_.end()) return {};
    std::vector<SymbolRecord> out;
    out.reserve(it->second.size());
    for (size_t idx : it->second)
        out.push_back(m_impl->symbols_[idx]);
    return out;
}

void CodebaseIndexer::clearIndex() {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    m_impl->symbols_.clear();
    m_impl->nameIndex_.clear();
    m_impl->fileIndex_.clear();
    m_impl->stats_ = IndexStats{};
}

void CodebaseIndexer::rebuildIndex() {
    // Snapshot root info from stats, then clear and re-index.
    // The caller is expected to call indexDirectory again after rebuildIndex
    // if they want a fresh scan; rebuildIndex itself resets internal state.
    clearIndex();
}

IndexStats CodebaseIndexer::getStats() const {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    return m_impl->stats_;
}

bool CodebaseIndexer::saveIndex(const std::string& path) const {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return false;

    // Header
    ofs.write(reinterpret_cast<const char*>(&INDEX_MAGIC),   sizeof(INDEX_MAGIC));
    ofs.write(reinterpret_cast<const char*>(&INDEX_VERSION), sizeof(INDEX_VERSION));
    uint64_t count = m_impl->symbols_.size();
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (auto& s : m_impl->symbols_) {
        writeLPString(ofs, s.name);
        writeLPString(ofs, s.kind);
        writeLPString(ofs, s.file);
        ofs.write(reinterpret_cast<const char*>(&s.line),   sizeof(s.line));
        ofs.write(reinterpret_cast<const char*>(&s.column), sizeof(s.column));
        writeLPString(ofs, s.signature);
        ofs.write(reinterpret_cast<const char*>(s.embedding.data()),
                  64 * sizeof(float));
    }
    return ofs.good();
}

bool CodebaseIndexer::loadIndex(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return false;

    uint32_t magic = 0, version = 0;
    if (!ifs.read(reinterpret_cast<char*>(&magic),   sizeof(magic)))   return false;
    if (!ifs.read(reinterpret_cast<char*>(&version), sizeof(version))) return false;
    if (magic != INDEX_MAGIC || version != INDEX_VERSION)             return false;

    uint64_t count = 0;
    if (!ifs.read(reinterpret_cast<char*>(&count), sizeof(count))) return false;

    std::vector<SymbolRecord> loaded;
    loaded.reserve(static_cast<size_t>(count));

    for (uint64_t i = 0; i < count; ++i) {
        SymbolRecord s{};
        if (!readLPString(ifs, s.name))      return false;
        if (!readLPString(ifs, s.kind))      return false;
        if (!readLPString(ifs, s.file))      return false;
        if (!ifs.read(reinterpret_cast<char*>(&s.line),   sizeof(s.line)))   return false;
        if (!ifs.read(reinterpret_cast<char*>(&s.column), sizeof(s.column))) return false;
        if (!readLPString(ifs, s.signature)) return false;
        if (!ifs.read(reinterpret_cast<char*>(s.embedding.data()),
                      64 * sizeof(float)))   return false;
        loaded.push_back(std::move(s));
    }

    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    m_impl->symbols_.clear();
    m_impl->nameIndex_.clear();
    m_impl->fileIndex_.clear();

    for (size_t i = 0; i < loaded.size(); ++i) {
        m_impl->nameIndex_[loaded[i].name].push_back(i);
        m_impl->fileIndex_[loaded[i].file].push_back(i);
    }
    m_impl->symbols_ = std::move(loaded);
    m_impl->stats_.symbolCount = m_impl->symbols_.size();
    m_impl->stats_.lastUpdated = nowTimestamp();
    return true;
}
