// =============================================================================
// FIMPromptBuilder.h — Fill-in-Middle Prompt Construction for Ghost Text
// =============================================================================
// Builds FIM prompts from editor state (cursor position, surrounding code).
// Supports multiple FIM formats:
//   - Qwen2.5-Coder: <|fim_prefix|> / <|fim_suffix|> / <|fim_middle|>
//   - DeepSeek Coder: <|fim▁begin|> / <|fim▁hole|> / <|fim▁end|>
//   - CodeLlama/StarCoder: <PRE> / <SUF> / <MID>
//
// Implements context windowing to fit within model's num_ctx limit.
// Uses O(ND) diffing for change-aware context prioritization.
// =============================================================================
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {
namespace Agent {

// ---------------------------------------------------------------------------
// FIM format variants
// ---------------------------------------------------------------------------
enum class FIMFormat : uint8_t {
    Qwen,           // <|fim_prefix|> ... <|fim_suffix|> ... <|fim_middle|>
    DeepSeek,       // <|fim▁begin|> ... <|fim▁hole|> ... <|fim▁end|>
    StarCoder,      // <PRE> ... <SUF> ... <MID>
    CodeLlama,      // <PRE> ... <SUF> ... <MID> (same tokens, different model)
};

// ---------------------------------------------------------------------------
// Editor context snapshot
// ---------------------------------------------------------------------------
struct EditorContext {
    std::string filename;           // Current file name (for language hints)
    std::string filepath;           // Full path
    std::string full_content;       // Entire file content
    int cursor_line;                // 0-indexed cursor line
    int cursor_column;              // 0-indexed cursor column
    std::string language;           // "cpp", "python", etc.

    // Optional: nearby open files for cross-file context
    std::vector<std::pair<std::string, std::string>> related_files; // [(path, content)]
};

// ---------------------------------------------------------------------------
// Built FIM prompt
// ---------------------------------------------------------------------------
struct FIMPrompt {
    std::string prefix;             // Code before cursor
    std::string suffix;             // Code after cursor
    std::string formatted_prompt;   // Full prompt with FIM tokens
    std::string filename;           // For model hints

    int prefix_lines;               // Lines in prefix
    int suffix_lines;               // Lines in suffix
    int estimated_tokens;           // Rough token estimate (chars / 4)
};

// ---------------------------------------------------------------------------
// FIM prompt build result (no exceptions)
// ---------------------------------------------------------------------------
struct FIMBuildResult {
    bool success;
    FIMPrompt prompt;
    std::string error;

    static FIMBuildResult ok(const FIMPrompt& p) {
        return {true, p, ""};
    }
    static FIMBuildResult fail(const std::string& msg) {
        FIMBuildResult r;
        r.success = false;
        r.error = msg;
        return r;
    }
};

// ---------------------------------------------------------------------------
// FIMPromptBuilder
// ---------------------------------------------------------------------------
class FIMPromptBuilder {
public:
    FIMPromptBuilder();
    ~FIMPromptBuilder() = default;

    // Configure
    void SetFormat(FIMFormat fmt) { m_format = fmt; }
    void SetMaxContextTokens(int tokens) { m_maxContextTokens = tokens; }
    void SetPrefixRatio(float ratio) { m_prefixRatio = ratio; }  // 0.0-1.0, default 0.75
    void SetIncludeFilename(bool include) { m_includeFilename = include; }

    // Build FIM prompt from editor state
    FIMBuildResult Build(const EditorContext& ctx) const;

    // Build with explicit prefix/suffix (for testing)
    FIMBuildResult BuildFromParts(const std::string& prefix,
                                  const std::string& suffix,
                                  const std::string& filename = "") const;

    // Estimate token count (rough: chars/4 for most tokenizers)
    static int EstimateTokens(const std::string& text);

private:
    // Split content at cursor position
    void SplitAtCursor(const std::string& content,
                       int line, int col,
                       std::string& prefix, std::string& suffix) const;

    // Trim to fit within context window
    void TrimToFit(std::string& prefix, std::string& suffix) const;

    // Add language-specific context hints
    std::string GetLanguageHint(const std::string& language) const;

    // Format with FIM tokens
    std::string FormatPrompt(const std::string& prefix,
                             const std::string& suffix,
                             const std::string& filename) const;

    // Count lines in a string
    static int CountLines(const std::string& text);

    FIMFormat m_format = FIMFormat::Qwen;
    int m_maxContextTokens = 4096;
    float m_prefixRatio = 0.75f;        // 75% prefix, 25% suffix
    bool m_includeFilename = true;
};

} // namespace Agent
} // namespace RawrXD
