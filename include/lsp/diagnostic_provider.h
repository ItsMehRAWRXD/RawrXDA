#pragma once
/**
 * @file diagnostic_provider.h
 * @brief Real-time diagnostics and error reporting
 * Batch 3 - Item 43: Diagnostic provider
 */

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <future>

namespace RawrXD::LSP {

enum class DiagnosticSeverity {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4
};

enum class DiagnosticTag {
    Unnecessary = 1,
    Deprecated = 2
};

struct DiagnosticRelatedInformation {
    std::string uri;
    uint32_t startLine;
    uint32_t startColumn;
    uint32_t endLine;
    uint32_t endColumn;
    std::string message;
};

struct Diagnostic {
    uint32_t startLine;
    uint32_t startColumn;
    uint32_t endLine;
    uint32_t endColumn;
    DiagnosticSeverity severity;
    std::optional<std::string> code;
    std::optional<std::string> codeDescription;
    std::optional<std::string> source;
    std::string message;
    std::vector<DiagnosticTag> tags;
    std::vector<DiagnosticRelatedInformation> relatedInformation;
    std::optional<std::string> data;
};

struct PublishDiagnosticsParams {
    std::string uri;
    uint32_t version;
    std::vector<Diagnostic> diagnostics;
};

class DiagnosticProvider {
public:
    DiagnosticProvider();
    ~DiagnosticProvider();

    // Configuration
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setDelay(uint32_t milliseconds);

    // Diagnostics
    std::vector<Diagnostic> provideDiagnostics(const std::string& uri,
                                                  const std::string& content);
    void clearDiagnostics(const std::string& uri);

    // Async diagnostics
    std::future<std::vector<Diagnostic>> provideDiagnosticsAsync(const std::string& uri,
                                                                   const std::string& content);

    // Event handling
    using DiagnosticCallback = std::function<void(const PublishDiagnosticsParams&)>;
    void onDiagnostics(DiagnosticCallback callback);

    // Registration
    void registerDiagnosticSource(const std::string& name,
                                  std::function<std::vector<Diagnostic>(
                                      const std::string& uri,
                                      const std::string& content)> source);

    // C++ specific
    std::vector<Diagnostic> getCppDiagnostics(const std::string& uri,
                                                const std::string& content);

    // Related actions
    std::vector<std::string> getCodeActions(const Diagnostic& diagnostic);

private:
    bool m_enabled{true};
    uint32_t m_delay{500};
    DiagnosticCallback m_callback;
    std::vector<std::pair<std::string,
        std::function<std::vector<Diagnostic>(const std::string&, const std::string&)>>> m_sources;
    mutable std::mutex m_mutex;

    void notifyDiagnostics(const std::string& uri,
                           uint32_t version,
                           const std::vector<Diagnostic>& diagnostics);

    // C++ analysis
    std::vector<Diagnostic> analyzeSyntax(const std::string& uri,
                                           const std::string& content);
    std::vector<Diagnostic> analyzeSemantics(const std::string& uri,
                                              const std::string& content);
    std::vector<Diagnostic> analyzeStyle(const std::string& uri,
                                           const std::string& content);
};

// Global provider
DiagnosticProvider& getDiagnosticProvider();

} // namespace RawrXD::LSP
