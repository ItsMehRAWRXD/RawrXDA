// ============================================================================
// Win32IDE_GhostText.h — Ghost Text (Inline Completion) Interface
// ============================================================================
// Defines Win32IDE_GhostText, the inline completion surface for the IDE.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>

namespace RawrXD {

// ---------------------------------------------------------------------------
// GhostTextSuggestion — Single inline completion item
// ---------------------------------------------------------------------------
struct GhostTextSuggestion {
    std::string text;           // The suggested completion text
    std::string displayText;    // Shorter display version
    float confidence = 0.0f;    // 0.0–1.0
    int startOffset = 0;      // Relative to cursor
    int endOffset = 0;        // Relative to cursor
};

// Forward declaration from ContextFusionEngine
struct DiagnosticInfo;

// ---------------------------------------------------------------------------
// Win32IDE_GhostText — Inline completion controller
// ---------------------------------------------------------------------------
class Win32IDE_GhostText {
public:
    virtual ~Win32IDE_GhostText() = default;

    // Show / hide ghost text at current cursor position
    virtual void ShowSuggestion(const GhostTextSuggestion& suggestion) = 0;
    virtual void HideSuggestion() = 0;
    virtual void Hide() = 0;
    virtual void Clear() = 0;
    virtual bool IsShowing() const = 0;

    // Accept / dismiss
    virtual void AcceptSuggestion() = 0;
    virtual void DismissSuggestion() = 0;

    // Configuration
    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsEnabled() const = 0;
    virtual void SetDelayMs(int ms) = 0;

    // Context query (for ContextFusionEngine)
    virtual std::string GetCurrentLine() const = 0;
    virtual int GetCursorPosition() const = 0;
    virtual std::string GetRecentLines(int count) const = 0;

    // Request suggestion from provider
    virtual void RequestSuggestion(
        const std::string& prefix,
        const std::string& suffix,
        const std::string& filePath,
        const std::string& languageId,
        const std::vector<std::string>& symbols,
        const std::vector<DiagnosticInfo>& diagnostics
    ) = 0;

    // Callbacks
    using SuggestionCallback = std::function<void(const std::vector<GhostTextSuggestion>&)>;
    virtual void SetSuggestionProvider(SuggestionCallback cb) = 0;
};

} // namespace RawrXD
