// at_docs_indexer.cpp - Implementation (BM25-lite)
#include "at_docs_indexer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace rawrxd::parity {

namespace {
bool is_word_char(unsigned char c) { return std::isalnum(c) || c == '_'; }
} // namespace

std::vector<std::string> AtDocsIndexer::tokenize(std::string_view s) {
    std::vector<std::string> out;
    std::string cur;
    for (unsigned char c : s) {
        if (is_word_char(c)) cur.push_back(static_cast<char>(std::tolower(c)));
        else if (!cur.empty()) { out.push_back(std::move(cur)); cur.clear(); }
    }
    if (!cur.empty()) out.push_back(std::move(cur));
    // Light stop-word trim.
    static const std::unordered_set<std::string> stops = {
        "the","a","an","and","or","of","in","to","is","it","for","on","by","with","as","at"};
    out.erase(std::remove_if(out.begin(), out.end(),
                             [](const std::string& t) { return stops.count(t) || t.size() < 2; }),
              out.end());
    return out;
}

void AtDocsIndexer::add_chunk(DocChunk c) {
    std::lock_guard lk(mu_);
    std::uint32_t idx = static_cast<std::uint32_t>(chunks_.size());
    auto toks = tokenize(c.text);
    std::unordered_map<std::string, std::uint16_t> tf;
    for (auto& t : toks) if (++tf[t] > 65000) tf[t] = 65000;
    // Update avg_len incrementally.
    avg_len_ = (avg_len_ * idx + static_cast<double>(toks.size())) /
               static_cast<double>(idx + 1);
    for (auto& [term, f] : tf) postings_[term].emplace_back(idx, f);
    chunks_.push_back(std::move(c));
}

std::size_t AtDocsIndexer::index_folder(const std::string& doc_id,
                                        const std::filesystem::path& root,
                                        std::size_t max_chunk_chars) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return 0;
    std::size_t count = 0;
    for (auto it = fs::recursive_directory_iterator(root, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        auto ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".md" && ext != ".mdx" && ext != ".rst" && ext != ".txt") continue;
        std::ifstream f(it->path(), std::ios::binary);
        if (!f) continue;
        std::ostringstream ss; ss << f.rdbuf();
        std::string buf = ss.str();
        // Chunk by paragraph boundaries, clamp to max_chunk_chars.
        std::size_t i = 0, line = 1;
        while (i < buf.size()) {
            std::size_t end = std::min(buf.size(), i + max_chunk_chars);
            if (end < buf.size()) {
                auto nl = buf.rfind("\n\n", end);
                if (nl != std::string::npos && nl > i + max_chunk_chars / 2) end = nl;
            }
            DocChunk c;
            c.doc_id = doc_id;
            c.title = it->path().filename().string();
            c.path  = it->path().string();
            c.start_line = static_cast<std::uint32_t>(line);
            c.text = buf.substr(i, end - i);
            c.end_line = static_cast<std::uint32_t>(
                line + std::count(c.text.begin(), c.text.end(), '\n'));
            line = c.end_line + 1;
            add_chunk(std::move(c));
            ++count;
            i = end;
            while (i < buf.size() && (buf[i] == '\n' || buf[i] == ' ')) ++i;
        }
    }
    return count;
}

std::vector<DocSearchHit> AtDocsIndexer::search(std::string_view query,
                                                std::string_view doc_id,
                                                std::size_t top_k) const {
    std::lock_guard lk(mu_);
    auto qtoks = tokenize(query);
    if (qtoks.empty() || chunks_.empty()) return {};
    const double N = static_cast<double>(chunks_.size());
    const double avgdl = std::max(1.0, avg_len_);
    constexpr double k1 = 1.5, b = 0.75;

    std::unordered_map<std::uint32_t, double> scores;
    for (const auto& qt : qtoks) {
        auto it = postings_.find(qt);
        if (it == postings_.end()) continue;
        double df = static_cast<double>(it->second.size());
        double idf = std::log(1.0 + (N - df + 0.5) / (df + 0.5));
        for (auto [idx, tf] : it->second) {
            if (!doc_id.empty() && chunks_[idx].doc_id != doc_id) continue;
            double ld = 0.0;
            // approximate chunk length in tokens (cheap)
            for (char c : chunks_[idx].text) if (c == ' ' || c == '\n') ld += 1.0;
            double norm = 1.0 - b + b * (ld / avgdl);
            double score = idf * (tf * (k1 + 1.0)) / (tf + k1 * norm);
            scores[idx] += score;
        }
    }
    std::vector<DocSearchHit> hits;
    hits.reserve(scores.size());
    for (auto& [i, s] : scores) hits.push_back({ &chunks_[i], s });
    std::partial_sort(hits.begin(),
                      hits.begin() + std::min(top_k, hits.size()),
                      hits.end(),
                      [](const DocSearchHit& a, const DocSearchHit& b_) { return a.score > b_.score; });
    if (hits.size() > top_k) hits.resize(top_k);
    return hits;
}

std::size_t AtDocsIndexer::total_chunks() const {
    std::lock_guard lk(mu_); return chunks_.size();
}

std::vector<std::string> AtDocsIndexer::list_docs() const {
    std::lock_guard lk(mu_);
    std::unordered_set<std::string> s;
    for (const auto& c : chunks_) s.insert(c.doc_id);
    return { s.begin(), s.end() };
}

void AtDocsIndexer::clear() {
    std::lock_guard lk(mu_);
    chunks_.clear(); postings_.clear(); avg_len_ = 0.0;
}

} // namespace rawrxd::parity
