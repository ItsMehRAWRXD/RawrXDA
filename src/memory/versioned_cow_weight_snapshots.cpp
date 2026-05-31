#include "versioned_cow_weight_snapshots.h"
#include <algorithm>

namespace RawrXD::Memory {

VCWSWeightSnapshots::VCWSWeightSnapshots(size_t pageSize)
    : m_pageSize(pageSize) {}

uint64_t VCWSWeightSnapshots::createVersion(const std::string& name, const void* base, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);

    VCWSVersion v;
    v.versionId = m_nextVersionId++;
    v.name = name;
    v.totalBytes = size;
    m_base = const_cast<void*>(base);

    m_versions[v.versionId] = std::move(v);
    return m_versions.rbegin()->first;
}

uint64_t VCWSWeightSnapshots::forkVersion(uint64_t parentVersionId, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto parentIt = m_versions.find(parentVersionId);
    if (parentIt == m_versions.end()) return 0;

    VCWSVersion child;
    child.versionId = m_nextVersionId++;
    child.name = name;
    child.totalBytes = parentIt->second.totalBytes;
    // Copy-on-write: share dirty pages set initially
    child.dirtyPages = parentIt->second.dirtyPages;

    m_versions[child.versionId] = std::move(child);
    return m_versions.rbegin()->first;
}

bool VCWSWeightSnapshots::switchVersion(uint64_t versionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_versions.find(versionId);
    if (it == m_versions.end()) return false;

    // In a real implementation, this would remap pages
    // For now, just validate the version exists
    return true;
}

void VCWSWeightSnapshots::markDirty(uint64_t versionId, size_t offset, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_versions.find(versionId);
    if (it == m_versions.end()) return;

    uint64_t startPage = pageAlign(offset);
    uint64_t endPage = pageAlign(offset + size);

    for (uint64_t page = startPage; page <= endPage; page += m_pageSize) {
        uint64_t pageId = page / m_pageSize;
        it->second.dirtyPages.insert(pageId);

        // Track page metadata
        if (m_pages.find(pageId) == m_pages.end()) {
            VCWSPage p;
            p.pageId = m_nextPageId++;
            p.offset = page;
            p.size = m_pageSize;
            p.dirty = true;
            m_pages[pageId] = std::move(p);
        }
    }
}

void* VCWSWeightSnapshots::getPage(uint64_t versionId, size_t offset) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_versions.find(versionId);
    if (it == m_versions.end()) return nullptr;

    uint64_t pageId = pageAlign(offset) / m_pageSize;
    auto pageIt = m_pages.find(pageId);
    if (pageIt == m_pages.end()) return nullptr;

    return static_cast<char*>(m_base) + pageIt->second.offset;
}

size_t VCWSWeightSnapshots::pageSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pageSize;
}

size_t VCWSWeightSnapshots::versionCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_versions.size();
}

size_t VCWSWeightSnapshots::dirtyPageCount(uint64_t versionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_versions.find(versionId);
    return it == m_versions.end() ? 0 : it->second.dirtyPages.size();
}

uint64_t VCWSWeightSnapshots::pageAlign(size_t offset) const {
    return (offset / m_pageSize) * m_pageSize;
}

} // namespace RawrXD::Memory
