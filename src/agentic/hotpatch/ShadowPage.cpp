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
    }
    
    // Allocate shadow page
    void* shadowPage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!shadowPage) {
        return nullptr;
    }
    
    // Copy original page content
    memcpy(shadowPage, pageStart, pageSize);
    
    return new ShadowPage(pageStart, shadowPage, pageSize, mbi.Protect);
}

void ShadowPage::destroy(ShadowPage* shadow) {
    delete shadow;
}

void* ShadowPage::getShadowAddress(void* originalAddress) const {
    if (!contains(originalAddress)) {
        return nullptr;
    }
    
    uintptr_t offset = reinterpret_cast<uintptr_t>(originalAddress) - 
                      reinterpret_cast<uintptr_t>(m_originalPage);
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_shadowPage) + offset);
}

void* ShadowPage::getOriginalAddress(void* shadowAddress) const {
    uintptr_t shadowStart = reinterpret_cast<uintptr_t>(m_shadowPage);
    uintptr_t shadowAddr = reinterpret_cast<uintptr_t>(shadowAddress);
    
    if (shadowAddr < shadowStart || shadowAddr >= shadowStart + m_pageSize) {
        return nullptr;
    }
    
    uintptr_t offset = shadowAddr - shadowStart;
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_originalPage) + offset);
}

bool ShadowPage::contains(void* address) const {
    uintptr_t addr = reinterpret_cast<uintptr_t>(address);
    uintptr_t start = reinterpret_cast<uintptr_t>(m_originalPage);
    return (addr >= start && addr < start + m_pageSize);
}

bool ShadowPage::write(void* address, const void* data, size_t size) {
    void* shadowAddr = getShadowAddress(address);
    if (!shadowAddr) {
        return false;
    }
    
    // Write to shadow
    memcpy(shadowAddr, data, size);
    
    // Try to write to original if writable
    if (m_isWritable) {
        DWORD oldProtect;
        if (VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(address, data, size);
            VirtualProtect(address, size, oldProtect, &oldProtect);
        }
    }
    
    return true;
}

bool ShadowPage::flush() {
    if (!m_isWritable) {
        return false;
    }
    
    DWORD oldProtect;
    if (!VirtualProtect(m_originalPage, m_pageSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    
    memcpy(m_originalPage, m_shadowPage, m_pageSize);
    VirtualProtect(m_originalPage, m_pageSize, oldProtect, &oldProtect);
    
    return true;
}

size_t ShadowPage::getPageSize() {
    static size_t pageSize = 0;
    if (pageSize == 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        pageSize = si.dwPageSize;
    }
    return pageSize;
}

void* ShadowPage::alignToPage(void* address) {
    size_t pageSize = getPageSize();
    uintptr_t addr = reinterpret_cast<uintptr_t>(address);
    return reinterpret_cast<void*>(addr & ~(pageSize - 1));
}

ShadowPage::ShadowPage(void* originalPage, void* shadowPage, size_t size, DWORD protection)
    : m_originalPage(originalPage), m_shadowPage(shadowPage), m_pageSize(size),
      m_originalProtection(protection) {
    m_isWritable = (protection & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) != 0;
}

ShadowPage::~ShadowPage() {
    if (m_shadowPage) {
        VirtualFree(m_shadowPage, 0, MEM_RELEASE);
    }
}

// ShadowPageManager implementation
ShadowPageManager& ShadowPageManager::instance() {
    static ShadowPageManager inst;
    return inst;
}

ShadowPage* ShadowPageManager::getShadowPage(void* address) {
    void* pageStart = ShadowPage::alignToPage(address);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_shadowPages.find(pageStart);
    if (it != m_shadowPages.end()) {
        return it->second;
    }
    
    ShadowPage* shadow = ShadowPage::create(address);
    if (shadow) {
        m_shadowPages[pageStart] = shadow;
    }
    
    return shadow;
}

void ShadowPageManager::releaseShadowPage(void* address) {
    void* pageStart = ShadowPage::alignToPage(address);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_shadowPages.find(pageStart);
    if (it != m_shadowPages.end()) {
        ShadowPage::destroy(it->second);
        m_shadowPages.erase(it);
    }
}

void ShadowPageManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [addr, shadow] : m_shadowPages) {
        ShadowPage::destroy(shadow);
    }
    m_shadowPages.clear();
}

} // namespace RawrXD::Agentic::Hotpatch
