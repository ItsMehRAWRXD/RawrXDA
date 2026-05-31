#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include "CommonTypes.h"

namespace RawrXD {
namespace IDE {

struct Vector {
    std::vector<float> data;
    std::string id;
    std::map<std::string, std::string> metadata;
};

struct SearchResult {
    float score;
    std::string id;
    std::map<std::string, std::string> metadata;
};

class IVectorIndex {
public:
    virtual ~IVectorIndex() = default;
    virtual bool initialize(size_t dimensions) = 0;
    virtual bool addVector(const Vector& vec) = 0;
    virtual std::vector<SearchResult> search(const std::vector<float>& query, int k) = 0;
    virtual bool save(const std::string& path) = 0;
    virtual bool load(const std::string& path) = 0;
};

// SIMD-optimized implementation for AVX-512 (Moat Feature)
class CodebaseVectorIndex : public IVectorIndex {
public:
    CodebaseVectorIndex();
    bool initialize(size_t dimensions) override;
    bool addVector(const Vector& vec) override;
    std::vector<SearchResult> search(const std::vector<float>& query, int k) override;
    bool save(const std::string& path) override;
    bool load(const std::string& path) override;

private:
    size_t m_dimensions;
    std::vector<Vector> m_vectors;
    
};

} // namespace IDE
} // namespace RawrXD
