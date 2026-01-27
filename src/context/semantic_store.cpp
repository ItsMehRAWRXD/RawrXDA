#define NOMINMAX
#include "context/semantic_store.h"
#include <algorithm>
#include <cmath>

namespace RawrXD {
namespace Context {

void SemanticStore::upsert(const EmbeddingItem& item) {
    for (auto& it : m_items) {
        if (it.id == item.id) { it = item; return; }
    }
    m_items.push_back(item);
}

bool SemanticStore::remove(const std::string& id) {
    auto n = m_items.size();
    m_items.erase(std::remove_if(m_items.begin(), m_items.end(), [&](const EmbeddingItem& e){ return e.id == id; }), m_items.end());
    return m_items.size() != n;
}

std::vector<SearchResult> SemanticStore::search(const std::vector<float>& query, size_t top_k) const {
    std::vector<SearchResult> all;
    for (const auto& it : m_items) {
        if (it.vec.empty()) continue;
        float s = cosine(query, it.vec);
        all.push_back({it.id, it.text, s});
    }
    std::sort(all.begin(), all.end(), [](const SearchResult& a, const SearchResult& b){ return a.score > b.score; });
    if (all.size() > top_k) all.resize(top_k);
    return all;
}

float cosine(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || b.empty() || a.size() != b.size()) return 0.0f;
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (size_t i=0;i<a.size();++i){ dot += a[i]*b[i]; na += a[i]*a[i]; nb += b[i]*b[i]; }
    double denom = std::sqrt(na) * std::sqrt(nb);
    if (denom == 0.0) return 0.0f;
    return static_cast<float>(dot / denom);
}

} // namespace Context
} // namespace RawrXD
