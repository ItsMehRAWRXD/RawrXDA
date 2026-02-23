// codebase_rag.cpp — In-house RAG (P0): TF-IDF indexing, no external deps
#include "ai/codebase_rag.hpp"
#include <fstream>
#include <sstream>
#include <memory>
#include <cmath>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace AI {

namespace {

std::vector<std::string> listSourceFiles(const std::string& root) {
    std::vector<std::string> out;
#ifdef _WIN32
    std::string pattern = root + "\\*";
    WIN32_FIND_DATAA fd = {};
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0 ||
                strcmp(fd.cFileName, ".git") == 0 || strcmp(fd.cFileName, "node_modules") == 0 ||
                strcmp(fd.cFileName, "build") == 0) continue;
            auto sub = listSourceFiles(root + "\\" + fd.cFileName);
            for (auto& s : sub) out.push_back(std::move(s));
        } else {
            std::string name = fd.cFileName;
            if (name.size() >= 4) {
                std::string ext = name.substr(name.size() - 4);
                if (ext == ".cpp" || ext == ".hpp" || ext == ".cxx" || ext == ".hxx") { out.push_back(root + "\\" + name); continue; }
                if (name.size() >= 2 && name.substr(name.size() - 2) == ".c") { out.push_back(root + "\\" + name); continue; }
                if (name.size() >= 2 && name.substr(name.size() - 2) == ".h") { out.push_back(root + "\\" + name); continue; }
            }
            if (name.size() >= 3 && name.substr(name.size() - 3) == ".py") out.push_back(root + "\\" + name);
            if (name.size() >= 3 && name.substr(name.size() - 3) == ".js")  out.push_back(root + "\\" + name);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#endif
    return out;
}

} // namespace

std::vector<std::string> CodebaseRAG::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::string cur;
    for (unsigned char c : text) {
        if (std::isalnum(c) || c == '_') cur += static_cast<char>(std::tolower(c));
        else {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

void CodebaseRAG::normalize(std::vector<float>& v) const {
    float n = 0;
    for (float x : v) n += x * x;
    n = std::sqrt(n);
    if (n > 0) for (float& x : v) x /= n;
}

float CodebaseRAG::cosine(const std::vector<float>& a, const std::vector<float>& b) const {
    float d = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); i++) d += a[i] * b[i];
    return d;
}

std::vector<float> CodebaseRAG::embed(const std::string& text) const {
    auto toks = tokenize(text);
    std::vector<float> v(m_vocabSize, 0.f);
    for (const auto& t : toks) {
        size_t idx = std::hash<std::string>{}(t) % m_vocabSize;
        float w = 1.f;
        if (m_idf.count(t)) w = m_idf.at(t);
        v[idx] += w;
    }
    normalize(v);
    return v;
}

void CodebaseRAG::indexFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    int lineNo = 0;
    while (std::getline(f, line)) {
        lineNo++;
        if (line.size() < 4) continue;
        RAGChunk ch;
        ch.file = path;
        ch.line = lineNo;
        ch.text = line;
        ch.embedding = embed(line);
        if (!ch.embedding.empty()) m_chunks.push_back(std::move(ch));
    }
}

void CodebaseRAG::indexDirectory(const std::string& rootPath) {
    m_chunks.clear();
    m_idf.clear();

    auto files = listSourceFiles(rootPath);
    std::unordered_map<std::string, int> docFreq;
    for (const auto& file : files) {
        std::ifstream f(file);
        if (!f) continue;
        std::string line;
        std::unordered_map<std::string, bool> seenInDoc;
        while (std::getline(f, line)) {
            auto toks = tokenize(line);
            for (const auto& t : toks) {
                if (seenInDoc.emplace(t, true).second) docFreq[t]++;
            }
        }
    }
    int ndocs = static_cast<int>(files.size());
    if (ndocs < 1) ndocs = 1;
    for (const auto& [t, df] : docFreq)
        m_idf[t] = std::log(static_cast<float>(ndocs) / (df + 1)) + 1.f;

    for (const auto& file : files) indexFile(file);
}

std::vector<std::pair<float, RAGChunk>> CodebaseRAG::query(const std::string& queryText, int topK) const {
    auto q = embed(queryText);
    std::vector<std::pair<float, RAGChunk>> results;
    for (const auto& ch : m_chunks) {
        float sim = cosine(q, ch.embedding);
        results.push_back({ sim, ch });
    }
    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });
    if (topK > 0 && results.size() > (size_t)topK) results.resize(topK);
    return results;
}

std::string CodebaseRAG::getContextForPrompt(const std::string& queryText, size_t maxChars) const {
    auto results = query(queryText, 15);
    std::string out = "Relevant code context:\n\n";
    for (const auto& [score, ch] : results) {
        if (score < 0.3f) continue;
        if (out.size() >= maxChars) break;
        out += "// " + ch.file + ":" + std::to_string(ch.line) + " (relevance " + std::to_string((int)(score * 100)) + "%)\n";
        out += ch.text + "\n\n";
    }
    return out;
}

// --- C API (for agent/IDE integration) ---
static std::unique_ptr<CodebaseRAG> g_rag;
static std::string g_ragContextResult;
static std::string g_ragQueryResult;

extern "C" {

#ifdef _WIN32
__declspec(dllexport)
#endif
void CodebaseRAG_Index(const char* projectPath) {
    if (!projectPath) return;
    g_rag = std::make_unique<CodebaseRAG>();
    g_rag->indexDirectory(projectPath);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
const char* CodebaseRAG_Query(const char* query, int topK) {
    if (!query || !g_rag) return "";
    auto results = g_rag->query(query, topK > 0 ? topK : 10);
    std::ostringstream oss;
    for (const auto& [score, ch] : results) {
        oss << ch.file << ":" << ch.line << " (score: " << score << ")\n" << ch.text << "\n---\n";
    }
    g_ragQueryResult = oss.str();
    return g_ragQueryResult.c_str();
}

#ifdef _WIN32
__declspec(dllexport)
#endif
const char* CodebaseRAG_GetContext(const char* query) {
    if (!query || !g_rag) return "";
    g_ragContextResult = g_rag->getContextForPrompt(query, 4000);
    return g_ragContextResult.c_str();
}

} // extern "C"

} // namespace AI
} // namespace RawrXD
