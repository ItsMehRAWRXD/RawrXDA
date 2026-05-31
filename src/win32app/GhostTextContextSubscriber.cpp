// GhostTextContextSubscriber.cpp — Ghost Text adapter for ContextFusionEngine
// Makes ghost text consume unified context instead of building its own.
// Integrates PrefixCache for speculative prefetch and latency masking.

#include "GhostTextContextSubscriber.h"
#include "PrefixCache.h"
#include <chrono>
#include <sstream>

namespace RawrXD {

// Latency telemetry (zero-overhead when not observed)
static thread_local std::chrono::steady_clock::time_point g_requestStart;
static thread_local int64_t g_lastLatencyUs = 0;
static thread_local int g_requestCount = 0;
static thread_local uint64_t g_staleFrames = 0;
static thread_local uint64_t g_totalFrames = 0;

GhostTextContextSubscriber::GhostTextContextSubscriber(Win32IDE_GhostText* ghostText)
    : m_ghostText(ghostText) {
}

void GhostTextContextSubscriber::OnContextUpdate(const ContextFrame& frame) {
    if (!m_enabled || !m_ghostText) return;
    
    // Count every frame seen for rate diagnostics
    g_totalFrames++;
    
    // Notify cache of keystroke activity (for idle detection)
    auto& cache = GetGlobalPrefixCache();
    cache.OnKeystroke();
    
    // Check for valid prefetch result first (latency masking)
    PrefixCacheKey cacheKey;
    cacheKey.fileHash = PrefixCache::HashFile(frame.filePath);
    cacheKey.lineHash = PrefixCache::HashLine(frame.CurrentLine());
    cacheKey.cursorPos = static_cast<uint32_t>(frame.cursor.column);
    cacheKey.languageId = PrefixCache::HashLanguage(frame.languageId);
    
    std::string cachedSuggestion;
    if (cache.HasValidPrefetch(cacheKey) && cache.GetPrefetchResult(cacheKey, cachedSuggestion)) {
        // Prefetch hit! Display immediately without inference
        GhostTextSuggestion suggestion;
        suggestion.text = cachedSuggestion;
        suggestion.displayText = cachedSuggestion;
        suggestion.confidence = 0.9f;  // High confidence for cached
        suggestion.startOffset = 0;
        suggestion.endOffset = static_cast<int>(cachedSuggestion.length());
        
        m_ghostText->ShowSuggestion(suggestion);
        
        // Log prefetch hit
        OutputDebugStringA(("[GhostText] PREFETCH HIT: latency_masked=true prefix_len=" +
            std::to_string(frame.cursor.column) + "\n").c_str());
        return;
    }
    
    // Debounce: don't request on every keystroke
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastUpdate).count();
    
    if (elapsed < m_debounceMs) {
        return; // Too soon
    }
    
    // Only request if context actually changed
    if (frame.version == m_lastVersion) {
        return; // No change
    }
    
    // Causality guard: detect out-of-order frame delivery
    if (frame.version < m_lastVersion) {
        g_staleFrames++;
        double staleRate = (g_totalFrames > 0)
            ? (static_cast<double>(g_staleFrames) / static_cast<double>(g_totalFrames))
            : 0.0;
        OutputDebugStringA(
            ("[GhostTextContextSubscriber] CAUSALITY VIOLATION: frame.version=" +
             std::to_string(frame.version) + " < lastVersion=" +
             std::to_string(m_lastVersion) +
             " stale_rate=" + std::to_string(static_cast<int>(staleRate * 1000)) + "\n").c_str());
        // Do not process stale frames
        return;
    }
    
    m_lastVersion = frame.version;
    m_lastUpdate = now;
    
    // Check if we should request ghost text
    if (ShouldRequest(frame)) {
        g_requestStart = std::chrono::steady_clock::now();
        RequestGhostText(frame);
        g_lastLatencyUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - g_requestStart).count();
        g_requestCount++;
        
        // Runtime telemetry log (visible in DebugView / IDE console)
        double staleRate = (g_totalFrames > 0)
            ? (static_cast<double>(g_staleFrames) / static_cast<double>(g_totalFrames))
            : 0.0;
        std::ostringstream telemetry;
        telemetry << "[GhostText] frame_version=" << frame.version
                  << " latency_us=" << g_lastLatencyUs
                  << " cursor=(" << frame.cursor.line << "," << frame.cursor.column << ")"
                  << " file=" << frame.filePath
                  << " lang=" << frame.languageId
                  << " stale_frames=" << g_staleFrames
                  << " total_frames=" << g_totalFrames
                  << " stale_rate=" << static_cast<int>(staleRate * 1000)
                  << "\n";
        OutputDebugStringA(telemetry.str().c_str());
    }
}

void GhostTextContextSubscriber::OnContextEvent(const ContextEvent& event) {
    // Handle specific events
    auto& cache = GetGlobalPrefixCache();
    
    switch (event.type) {
        case ContextEvent::AI_ACCEPTED:
            // Clear ghost text on accept
            if (m_ghostText) {
                m_ghostText->Clear();
            }
            // Cache the accepted suggestion for future use
            // (suggestion text is in event.payload if available)
            break;
            
        case ContextEvent::AI_REJECTED:
            // Clear ghost text on reject
            if (m_ghostText) {
                m_ghostText->Clear();
            }
            // Invalidate any pending prefetches
            cache.CancelPendingPrefetches();
            break;
            
        case ContextEvent::CURSOR_MOVED:
            // Hide ghost text when cursor moves
            if (m_ghostText) {
                m_ghostText->Hide();
            }
            // Notify cache of cursor movement (for idle detection)
            cache.OnCursorMove();
            break;
            
        case ContextEvent::EDITOR_CHANGED:
            // Invalidate cache on significant editor changes
            cache.CancelPendingPrefetches();
            break;
            
        default:
            break;
    }
}

bool GhostTextContextSubscriber::ShouldRequest(const ContextFrame& frame) const {
    // Don't request if:
    // 1. No file open
    if (frame.filePath.empty()) return false;
    
    // 2. Cursor at end of empty line (no context)
    std::string currentLine = frame.CurrentLine();
    if (currentLine.empty() && frame.cursor.column == 0) return false;
    
    // 3. Selection active (user is selecting, not typing)
    if (frame.HasSelection()) return false;
    
    // 4. Language not supported
    if (frame.languageId == "plaintext") return false;
    
    // 5. In string literal or comment (optional, can be enhanced)
    // For now, allow everywhere
    
    return true;
}

void GhostTextContextSubscriber::RequestGhostText(const ContextFrame& frame) {
    if (!m_ghostText) return;
    
    // Build context for ghost text request
    // Use unified context instead of building our own
    std::string context = frame.CurrentLine();
    std::string prefix = context.substr(0, frame.cursor.column);
    std::string suffix = "";
    if (frame.cursor.column < (int)context.length()) {
        suffix = context.substr(frame.cursor.column);
    }
    
    // Get recent symbols from LSP for context enrichment
    std::vector<std::string> symbols;
    for (const auto& sym : frame.symbols) {
        if (sym.line >= frame.visible.startLine && sym.line <= frame.visible.endLine) {
            symbols.push_back(sym.name);
        }
    }
    
    // Queue speculative prefetch for next keystroke (latency masking)
    auto& cache = GetGlobalPrefixCache();
    if (cache.IsIdle(150)) {  // 150ms idle threshold
        cache.QueuePrefetch(prefix, frame.filePath, frame.languageId, symbols);
        OutputDebugStringA(("[GhostText] PREFETCH QUEUED: idle=true prefix_len=" +
            std::to_string(prefix.length()) + "\n").c_str());
    }
    
    // Request ghost text through existing pipeline
    // The pipeline now receives unified context instead of ad-hoc data
    m_ghostText->RequestSuggestion(
        prefix,
        suffix,
        frame.filePath,
        frame.languageId,
        symbols,
        frame.diagnostics
    );
}

// ── Latency Telemetry (zero-overhead when not queried) ────────────────────

int64_t GhostTextContextSubscriber::GetLastLatencyUs() {
    return g_lastLatencyUs;
}

int GhostTextContextSubscriber::GetRequestCount() {
    return g_requestCount;
}

uint64_t GhostTextContextSubscriber::GetStaleFrameCount() {
    return g_staleFrames;
}

uint64_t GhostTextContextSubscriber::GetTotalFrameCount() {
    return g_totalFrames;
}

double GhostTextContextSubscriber::GetStaleRate() {
    if (g_totalFrames == 0) return 0.0;
    return static_cast<double>(g_staleFrames) / static_cast<double>(g_totalFrames);
}

void GhostTextContextSubscriber::ResetTelemetry() {
    g_lastLatencyUs = 0;
    g_requestCount = 0;
    g_staleFrames = 0;
    g_totalFrames = 0;
}

} // namespace RawrXD