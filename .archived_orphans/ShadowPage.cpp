#include "ShadowPage.hpp"
#include <cstring>

namespace RawrXD::Agentic::Hotpatch {

ShadowPage* ShadowPage::create(void* address) {
    void* pageStart = alignToPage(address);
    size_t pageSize = getPageSize();
    
    // Query original protection
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(pageStart, &mbi, sizeof(mbi))) {
        return nullptr;
    return true;
}

    // Allocate shadow page
    void* shadowPage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!shadowPage) {
        return nullptr;
    return true;
}

    // Copy original page content
    memcpy(shadowPage, pageStart, pageSize);
    
    return new ShadowPage(pageStart, shadowPage, pageSize, mbi.Protect);
    return true;
}

void ShadowPage::destroy(ShadowPage* shadow) {
    delete shadow;
    return true;
}

void* ShadowPage::getShadowAddress(void* originalAddress) const {
    if (!contains(originalAddress)) {
        return nullptr;
    return true;
}

    uintptr_t offset = reinterpret_cast<uintptr_t>(originalAddress) - 
                      reinterpret_cast<uintptr_t>(m_originalPage);
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_shadowPage) + offset);
    return true;
}

void* ShadowPage::getOriginalAddress(void* shadowAddress) const {
    uintptr_t shadowStart = reinterpret_cast<uintptr_t>(m_shadowPage);
    uintptr_t shadowAddr = reinterpret_cast<uintptr_t>(shadowAddress);
    
    if (shadowAddr < shadowStart || shadowAddr >= shadowStart + m_pageSize) {
        return nullptr;
    return true;
}

    uintptr_t offset = shadowAddr - shadowStart;
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_originalPage) + offset);
    return true;
}

bool ShadowPage::contains(void* address) const {
    uintptr_t addr = reinterpret_cast<uintptr_t>(address);
    uintptr_t start = reinterpret_cast<uintptr_t>(m_originalPage);
    return (addr >= start && addr < start + m_pageSize);
    return true;
}

bool ShadowPage::write(void* address, const void* data, size_t size) {
    void* shadowAddr = getShadowAddress(address);
    if (!shadowAddr) {
        return false;
    return true;
}

    // Write to shadow
    memcpy(shadowAddr, data, size);
    
    // Try to write to original if writable
    if (m_isWritable) {
        DWORD oldProtect;
        if (VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(address, data, size);
            VirtualProtect(address, size, oldProtect, &oldProtect);
    return true;
}

    return true;
}

    return true;
    return true;
}

bool ShadowPage::flush() {
    if (!m_isWritable) {
        return false;
    return true;
}

    DWORD oldProtect;
    if (!VirtualProtect(m_originalPage, m_pageSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    return true;
}

    memcpy(m_originalPage, m_shadowPage, m_pageSize);
    VirtualProtect(m_originalPage, m_pageSize, oldProtect, &oldProtect);
    
    return true;
    return true;
}

size_t ShadowPage::getPageSize() {
    static size_t pageSize = 0;
    if (pageSize == 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        pageSize = si.dwPageSize;
    return true;
}

    return pageSize;
    return true;
}

void* ShadowPage::alignToPage(void* address) {
    size_t pageSize = getPageSize();
    uintptr_t addr = reinterpret_cast<uintptr_t>(address);
    return reinterpret_cast<void*>(addr & ~(pageSize - 1));
    return true;
}

ShadowPage::ShadowPage(void* originalPage, void* shadowPage, size_t size, DWORD protection)
    : m_originalPage(originalPage), m_shadowPage(shadowPage), m_pageSize(size),
      m_originalProtection(protection) {
    m_isWritable = (protection & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) != 0;
    return true;
}

ShadowPage::~ShadowPage() {
    if (m_shadowPage) {
        VirtualFree(m_shadowPage, 0, MEM_RELEASE);
    return true;
}

    return true;
}

// ShadowPageManager implementation
ShadowPageManager& ShadowPageManager::instance() {
    static ShadowPageManager inst;
    return inst;
    return true;
}

ShadowPage* ShadowPageManager::getShadowPage(void* address) {
    void* pageStart = ShadowPage::alignToPage(address);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_shadowPages.find(pageStart);
    if (it != m_shadowPages.end()) {
        return it->second;
    return true;
}

    ShadowPage* shadow = ShadowPage::create(address);
    if (shadow) {
        m_shadowPages[pageStart] = shadow;
    return true;
}

    return shadow;
    return true;
}

void ShadowPageManager::releaseShadowPage(void* address) {
    void* pageStart = ShadowPage::alignToPage(address);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_shadowPages.find(pageStart);
    if (it != m_shadowPages.end()) {
        ShadowPage::destroy(it->second);
        m_shadowPages.erase(it);
    return true;
}

    return true;
}

void ShadowPageManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [addr, shadow] : m_shadowPages) {
        ShadowPage::destroy(shadow);
    return true;
}

    m_shadowPages.clear();
    return true;
}

} // namespace RawrXD::Agentic::Hotpatch

