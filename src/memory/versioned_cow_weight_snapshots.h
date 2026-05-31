#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RawrXD::Memory {

struct VCWSVersion {
    uint64_t versionId = 0;
    std::string name;
    std::unordered_set<uint64_t> dirtyPages;
    size_t totalBytes = 0;
};

struct VCWSPage {
    uint64_t pageId = 0;
    size_t offset = 0;
    size_t size = 0;
    bool dirty = false;
};

class VersionedCOWWeightSnapshots {
public:
    explicit VCWSWeightSnapshots(size_t pageSize = 64 * 1024);

    uint64_t createVersion(const std::string& name, const void* base, size_t size);
    uint64_t forkVersion(uint64_t parentVersionId, const std::string& name);
    bool switchVersion(uint64_t versionId);

    void markDirty(uint64_t versionId, size_t offset, size_t size);
    void* getPage(uint64_t versionId, size_t offset);

    size_t pageSize() const;
    size_t versionCount() const;
    size_t dirtyPageCount(uint64_t versionId) const;

private:
    mutable std::mutex m_mutex;
    size_t m_pageSize;
    uint64_t m_nextVersionId = 1;
    uint64_t m_nextPageId = 1;
    std::unordered_map<uint64_t, VCWSVersion> m_versions;
    std::unordered_map<uint64_t, VCWSPage> m_pages;
    void* m_base = nullptr;

    uint64_t pageAlign(size_t offset) const;
};

} // namespace RawrXD::Memory
