#include "CodebaseVectorIndex.h"
#include "ASMKernelThreadGate.h"
#include "ASMThermalBridge.h"
#include "VectorIndexCacheManager.h"
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <mutex>

// External assembly function from RawrXD_AVX512_VectorSearch.asm
extern "C" float cosine_similarity_avx512(const float* a, const float* b, size_t n);

namespace RawrXD {

static ASMKernelThreadGate g_search_gate;
static VectorIndexCacheManager g_vector_cache;

namespace IDE {

CodebaseVectorIndex::CodebaseVectorIndex() : m_dimensions(0) {
}

bool CodebaseVectorIndex::initialize(size_t dimensions) {
    if (dimensions == 0 || dimensions % 16 != 0) {
        // AVX-512 logic expects 16-float alignment for simplicity in this version
        return false;
    }
    m_dimensions = dimensions;
    return true;
}

bool CodebaseVectorIndex::addVector(const Vector& vec) {
    if (vec.data.size() != m_dimensions) {
        return false;
    }
    m_vectors.push_back(vec);
    return true;
}

std::vector<SearchResult> CodebaseVectorIndex::search(const std::vector<float>& query, int k) {
    if (query.size() != m_dimensions) {
        throw std::runtime_error("Query dimensions mismatch");
    }

    // Pin thread to performant core and enter the search gate
    SearchAffinityManager::pin_to_fast_core();
    g_search_gate.enter_search();

    std::vector<SearchResult> results;
    
    // Perform brute-force search using the ASM-optimized kernel
    for (const auto& vec : m_vectors) {
        // Adaptive Throttling: Check thermal bridge state
        if (ASMThermalBridge::instance().should_throttle()) {
            std::this_thread::yield(); 
        }

        float similarity = ::cosine_similarity_avx512(query.data(), vec.data.data(), m_dimensions);
        
        // Record cache access for TTL eviction
        g_vector_cache.record_access(vec.id);
        
        SearchResult res;
        res.score = similarity;
        res.id = vec.id;
        res.metadata = vec.metadata;
        results.push_back(res);
    }

    g_search_gate.leave_search();

    // Sort by descending similarity
    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.score > b.score;
    });

    // Resize to top K
    if ((int)results.size() > k) {
        results.resize(k);
    }

    return results;
}

bool CodebaseVectorIndex::save(const std::string& path) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;

    // Direct binary dump for speed (Sovereign Mode)
    ofs.write(reinterpret_cast<const char*>(&m_dimensions), sizeof(m_dimensions));
    size_t count = m_vectors.size();
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& vec : m_vectors) {
        // Simplified ID/Metadata serialization
        size_t idLen = vec.id.length();
        ofs.write(reinterpret_cast<const char*>(&idLen), sizeof(idLen));
        ofs.write(vec.id.data(), idLen);
        ofs.write(reinterpret_cast<const char*>(vec.data.data()), m_dimensions * sizeof(float));
    }

    return true;
}

bool CodebaseVectorIndex::load(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;

    ifs.read(reinterpret_cast<char*>(&m_dimensions), sizeof(m_dimensions));
    size_t count;
    ifs.read(reinterpret_cast<char*>(&count), sizeof(count));

    m_vectors.clear();
    for (size_t i = 0; i < count; ++i) {
        Vector vec;
        size_t idLen;
        ifs.read(reinterpret_cast<char*>(&idLen), sizeof(idLen));
        vec.id.resize(idLen);
        ifs.read(&vec.id[0], idLen);
        vec.data.resize(m_dimensions);
        ifs.read(reinterpret_cast<char*>(vec.data.data()), m_dimensions * sizeof(float));
        m_vectors.push_back(vec);
    }

    return true;
}

} // namespace IDE
} // namespace RawrXD
