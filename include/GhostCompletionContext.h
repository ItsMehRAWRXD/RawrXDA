// GhostCompletionContext.h — Stub for build compatibility
// Provides minimal GhostCompletionContext for Win32IDE compilation

#pragma once
#include <string>
#include <vector>
#include <optional>

namespace rawrxd {
namespace ghost_completion {

struct GhostCompletionContext {
    std::string filePath;
    int line = 0;
    int column = 0;
    std::vector<std::string> symbolNames;
    std::string surroundingLines;
    std::string language;
    std::string languageId;
    bool lspRunning = false;
    std::string lspSymbolDigest;

    static GhostCompletionContext build(const std::string& path, int ln, int col,
                                         const std::vector<std::string>& syms,
                                         const std::string& surrounding,
                                         const std::string& lang, bool lspOk) {
        GhostCompletionContext ctx;
        ctx.filePath = path;
        ctx.line = ln;
        ctx.column = col;
        ctx.symbolNames = syms;
        ctx.surroundingLines = surrounding;
        ctx.language = lang;
        ctx.languageId = lang;
        ctx.lspRunning = lspOk;
        return ctx;
    }

    std::string toPromptFragment(size_t maxLen = 4096) const {
        std::string result = "File: " + filePath + "\n";
        result += "Language: " + language + "\n";
        if (!symbolNames.empty()) {
            result += "Symbols: ";
            for (const auto& sym : symbolNames) {
                result += sym + " ";
            }
            result += "\n";
        }
        if (!surroundingLines.empty()) {
            result += "Context:\n" + surroundingLines + "\n";
        }
        if (result.size() > maxLen) {
            result.resize(maxLen);
        }
        return result;
    }
};

struct StructuredAiFixPayload {
    std::vector<std::string> edits;
    std::string explanation;
};

inline std::optional<StructuredAiFixPayload> tryParseStructuredAiFixFromModelResponse(const std::string& /*response*/) {
    return std::nullopt;
}

inline std::optional<std::string> applyStructuredAiLineDiffsUtf8(const std::string& before, const std::vector<std::string>& /*edits*/) {
    return before;
}

} // namespace ghost_completion
} // namespace rawrxd
