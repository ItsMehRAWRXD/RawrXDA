// ============================================================================
// diagnostic_consumer.cpp — LSP Diagnostic Consumer Implementation
// ============================================================================
// Diagnostic aggregation, filtering, quick-fix dispatch, agentic fix prompt
// building, and severity-based classification.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "lsp/diagnostic_consumer.h"

#include <sstream>
#include <algorithm>
#include <regex>
#include <cstring>
#include <cctype>

namespace RawrXD {
namespace LSP {

// ============================================================================
// Severity names
// ============================================================================

const char* severityToString(DiagnosticSeverity sev) {
    switch (sev) {
        case DiagnosticSeverity::ERROR:       return "error";
        case DiagnosticSeverity::WARNING:     return "warning";
        case DiagnosticSeverity::INFORMATION: return "information";
        case DiagnosticSeverity::HINT:        return "hint";
        default: return "unknown";
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

DiagnosticConsumer::DiagnosticConsumer() = default;
DiagnosticConsumer::~DiagnosticConsumer() = default;

DiagnosticConsumer& DiagnosticConsumer::Global() {
    static DiagnosticConsumer instance;
    return instance;
}

// ============================================================================
// Ingestion
// ============================================================================

DiagResult DiagnosticConsumer::publishDiagnostics(const std::string& file,
                                                   const std::vector<Diagnostic>& diagnostics) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diagnostics[file] = diagnostics;
    notifyCallbacks(file);
    return DiagResult::ok("Published");
}

DiagResult DiagnosticConsumer::addDiagnostic(const Diagnostic& diag) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diagnostics[diag.file].push_back(diag);
    notifyCallbacks(diag.file);
    return DiagResult::ok("Added");
}

void DiagnosticConsumer::clearFile(const std::string& file) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diagnostics.erase(file);
    notifyCallbacks(file);
}

void DiagnosticConsumer::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diagnostics.clear();
}

// ============================================================================
// Querying
// ============================================================================

std::vector<Diagnostic> DiagnosticConsumer::getDiagnostics(const std::string& file) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_diagnostics.find(file);
    if (it == m_diagnostics.end()) return {};
    return it->second;
}

std::vector<Diagnostic> DiagnosticConsumer::getDiagnostics(const std::string& file,
                                                            DiagnosticSeverity minSeverity) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_diagnostics.find(file);
    if (it == m_diagnostics.end()) return {};

    std::vector<Diagnostic> filtered;
    for (const auto& d : it->second) {
        if (static_cast<uint8_t>(d.severity) <= static_cast<uint8_t>(minSeverity)) {
            filtered.push_back(d);
        }
    }
    return filtered;
}

std::vector<Diagnostic> DiagnosticConsumer::query(const DiagnosticFilter& filter) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Diagnostic> results;

    std::regex fileRe;
    bool hasFilePattern = !filter.filePattern.empty();
    if (hasFilePattern) {
        // Convert glob to regex (simple: * → .*, ? → .)
        std::string pattern = filter.filePattern;
        std::string regexStr;
        for (char c : pattern) {
            switch (c) {
                case '*': regexStr += ".*"; break;
                case '?': regexStr += "."; break;
                case '.': regexStr += "\\."; break;
                default: regexStr += c;
            }
        }
        fileRe = std::regex(regexStr, std::regex::icase | std::regex::optimize);
    }

    std::regex codeRe;
    bool hasCodePattern = !filter.codePattern.empty();
    if (hasCodePattern) {
        codeRe = std::regex(filter.codePattern, std::regex::optimize);
    }

    for (const auto& [file, diags] : m_diagnostics) {
        if (hasFilePattern && !std::regex_search(file, fileRe)) continue;

        for (const auto& d : diags) {
            // Severity filter
            if (static_cast<uint8_t>(d.severity) >
                static_cast<uint8_t>(filter.minSeverity)) continue;

            // Source filter
            if (!filter.allSources && d.source != filter.source) continue;

            // Code pattern filter
            if (hasCodePattern && !std::regex_search(d.code, codeRe)) continue;

            results.push_back(d);
        }
    }

    return results;
}

std::vector<Diagnostic> DiagnosticConsumer::getDiagnosticsAtLine(const std::string& file,
                                                                   int line) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_diagnostics.find(file);
    if (it == m_diagnostics.end()) return {};

    std::vector<Diagnostic> results;
    for (const auto& d : it->second) {
        if (d.range.startLine <= line && d.range.endLine >= line) {
            results.push_back(d);
        }
    }
    return results;
}

std::vector<Diagnostic> DiagnosticConsumer::getDiagnosticsInRange(const std::string& file,
                                                                    const TextRange& range) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_diagnostics.find(file);
    if (it == m_diagnostics.end()) return {};

    std::vector<Diagnostic> results;
    for (const auto& d : it->second) {
        // Check overlap
        if (d.range.endLine < range.startLine) continue;
        if (d.range.startLine > range.endLine) continue;
        results.push_back(d);
    }
    return results;
}

// ============================================================================
// Statistics
// ============================================================================

DiagnosticStats DiagnosticConsumer::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    DiagnosticStats stats = {0, 0, 0, 0, 0, 0};
    stats.totalFiles = static_cast<int>(m_diagnostics.size());

    for (const auto& [file, diags] : m_diagnostics) {
        for (const auto& d : diags) {
            switch (d.severity) {
                case DiagnosticSeverity::ERROR:       stats.errorCount++; break;
                case DiagnosticSeverity::WARNING:     stats.warningCount++; break;
                case DiagnosticSeverity::INFORMATION: stats.infoCount++; break;
                case DiagnosticSeverity::HINT:        stats.hintCount++; break;
            }

            // Check if fixable
            for (const auto& provider : m_fixProviders) {
                std::regex re(provider.codePattern, std::regex::optimize);
                if (std::regex_search(d.code, re)) {
                    stats.fixableCount++;
                    break; // Only count once per diagnostic
                }
            }
        }
    }

    return stats;
}

DiagnosticStats DiagnosticConsumer::getFileStats(const std::string& file) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    DiagnosticStats stats = {0, 0, 0, 0, 1, 0};

    auto it = m_diagnostics.find(file);
    if (it == m_diagnostics.end()) {
        stats.totalFiles = 0;
        return stats;
    }

    for (const auto& d : it->second) {
        switch (d.severity) {
            case DiagnosticSeverity::ERROR:       stats.errorCount++; break;
            case DiagnosticSeverity::WARNING:     stats.warningCount++; break;
            case DiagnosticSeverity::INFORMATION: stats.infoCount++; break;
            case DiagnosticSeverity::HINT:        stats.hintCount++; break;
        }
    }

    return stats;
}

// ============================================================================
// Quick Fixes
// ============================================================================

void DiagnosticConsumer::registerQuickFixProvider(const std::string& codePattern,
                                                   QuickFixProvider provider,
                                                   void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fixProviders.push_back({codePattern, provider, userData});
}

std::vector<QuickFix> DiagnosticConsumer::getQuickFixes(const Diagnostic& diag) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<QuickFix> allFixes;

    for (const auto& provider : m_fixProviders) {
        std::regex re(provider.codePattern, std::regex::optimize);
        if (std::regex_search(diag.code, re)) {
            auto fixes = provider.provider(diag, provider.userData);
            allFixes.insert(allFixes.end(), fixes.begin(), fixes.end());
        }
    }

    return allFixes;
}

std::vector<std::pair<Diagnostic, std::vector<QuickFix>>>
DiagnosticConsumer::getFixableDiagnostics(const std::string& file) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<Diagnostic, std::vector<QuickFix>>> result;

    auto it = m_diagnostics.find(file);
    if (it == m_diagnostics.end()) return result;

    for (const auto& diag : it->second) {
        // Check each provider (without holding lock on providers — we already hold m_mutex)
        std::vector<QuickFix> fixes;
        for (const auto& provider : m_fixProviders) {
            std::regex re(provider.codePattern, std::regex::optimize);
            if (std::regex_search(diag.code, re)) {
                auto providerFixes = provider.provider(diag, provider.userData);
                fixes.insert(fixes.end(), providerFixes.begin(), providerFixes.end());
            }
        }

        if (!fixes.empty()) {
            result.push_back({diag, std::move(fixes)});
        }
    }

    return result;
}

// ============================================================================
// Agentic Integration
// ============================================================================

std::string DiagnosticConsumer::buildFixPrompt(const std::string& file,
                                                int maxTokens) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_diagnostics.find(file);
    if (it == m_diagnostics.end()) return "";

    std::ostringstream oss;
    oss << "Fix the following errors in " << file << ":\n\n";

    int tokenBudget = maxTokens - 50; // Reserve for header
    int tokensUsed = 0;

    // Sort by severity (errors first)
    auto sorted = it->second;
    std::sort(sorted.begin(), sorted.end(),
              [](const Diagnostic& a, const Diagnostic& b) {
                  return static_cast<uint8_t>(a.severity) <
                         static_cast<uint8_t>(b.severity);
              });

    for (const auto& d : sorted) {
        std::ostringstream entry;
        entry << severityToString(d.severity) << " "
              << d.code << " at line " << d.range.startLine
              << ": " << d.message << "\n";

        if (!d.relatedInfo.empty()) {
            for (const auto& [info, range] : d.relatedInfo) {
                entry << "  related: " << info
                      << " at line " << range.startLine << "\n";
            }
        }

        std::string entryStr = entry.str();
        int entryTokens = static_cast<int>(entryStr.size()) / 4; // Rough estimate
        if (tokensUsed + entryTokens > tokenBudget) break;

        oss << entryStr;
        tokensUsed += entryTokens;
    }

    return oss.str();
}

const char* DiagnosticConsumer::classifyDiagnostic(const Diagnostic& diag) const {
    // Classify by error code pattern
    const std::string& code = diag.code;

    // MSVC error codes
    if (code.substr(0, 1) == "C") {
        int num = 0;
        if (code.size() > 1) {
            for (size_t i = 1; i < code.size(); ++i) {
                if (std::isdigit(static_cast<unsigned char>(code[i]))) {
                    num = num * 10 + (code[i] - '0');
                }
            }
        }
        if (num >= 2000 && num < 3000) return "syntax";
        if (num >= 2500 && num < 2700) return "type";
        if (num >= 4000) return "linker";
        return "syntax";
    }

    // Clang error codes
    if (code.substr(0, 1) == "E" || code.find("error") != std::string::npos) {
        return "syntax";
    }

    // Include errors
    if (diag.message.find("#include") != std::string::npos ||
        diag.message.find("file not found") != std::string::npos) {
        return "include";
    }

    // Linker errors
    if (code.find("LNK") != std::string::npos ||
        diag.message.find("unresolved external") != std::string::npos) {
        return "linker";
    }

    // Hotpatch errors
    if (diag.source == DiagnosticSource::HOTPATCH) {
        return "hotpatch";
    }

    // Type errors
    if (diag.message.find("cannot convert") != std::string::npos ||
        diag.message.find("type mismatch") != std::string::npos ||
        diag.message.find("incompatible") != std::string::npos) {
        return "type";
    }

    return "unknown";
}

// ============================================================================
// Callbacks
// ============================================================================

void DiagnosticConsumer::registerCallback(DiagnosticCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back({callback, userData});
}

void DiagnosticConsumer::removeCallback(DiagnosticCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.erase(
        std::remove_if(m_callbacks.begin(), m_callbacks.end(),
                        [callback](const CallbackEntry& e) {
                            return e.callback == callback;
                        }),
        m_callbacks.end()
    );
}

void DiagnosticConsumer::notifyCallbacks(const std::string& file) const {
    auto it = m_diagnostics.find(file);
    const auto& diags = (it != m_diagnostics.end()) ? it->second : std::vector<Diagnostic>{};

    // Copy callbacks to avoid holding lock during invocation
    auto callbacks = m_callbacks;

    for (const auto& entry : callbacks) {
        entry.callback(file, diags, entry.userData);
    }
}

} // namespace LSP
} // namespace RawrXD
