#pragma once

#include <windows.h>
#include <cstdint>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace RawrXD::Agentic::Hotpatch {

/// Shadow page for copy-on-write protection handling
/// Allows safe patching of code in shared libraries
class ShadowPage {
public:
    /// Create shadow copy of page containing address
    static ShadowPage* create(void* address);
    
    /// Destroy shadow page
    static void destroy(ShadowPage* shadow);
    
    /// Get shadow address corresponding to original address
    void* getShadowAddress(void* originalAddress) const;
    
    /// Get original address from shadow address
    void* getOriginalAddress(void* shadowAddress) const;
    
    /// Check if address is in this shadow page
    bool contains(void* address) const;
    
    /// Write to shadow page (updates both shadow and original if possible)
    bool write(void* address, const void* data, size_t size);
    
    /// Flush changes to original page (if writable)
    bool flush();
    
    /// Get page protection flags
    DWORD getProtection() const { return m_originalProtection; }
    
    /// Get page size
    static size_t getPageSize();
    
    /// Align address to page boundary
    static void* alignToPage(void* address);
    
private:
    ShadowPage(void* originalPage, void* shadowPage, size_t size, DWORD protection);
    ~ShadowPage();
    
    void* m_originalPage = nullptr;
    void* m_shadowPage = nullptr;
    size_t m_pageSize = 0;
    DWORD m_originalProtection = 0;
    bool m_isWritable = false;
    
    static constexpr size_t s_pageSize = 4096;
};

/// Shadow page manager
class ShadowPageManager {
public:
    static ShadowPageManager& instance();
    
    /// Get or create shadow page for address
    ShadowPage* getShadowPage(void* address);
    
    /// Release shadow page
    void releaseShadowPage(void* address);
    
    /// Clear all shadow pages
    void clear();
    
private:
    ShadowPageManager() = default;
    ~ShadowPageManager() { clear(); }
    
    std::unordered_map<void*, ShadowPage*> m_shadowPages;
    std::mutex m_mutex;
};

} // namespace RawrXD::Agentic::Hotpatch
