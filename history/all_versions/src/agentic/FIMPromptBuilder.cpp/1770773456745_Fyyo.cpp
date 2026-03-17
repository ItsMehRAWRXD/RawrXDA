// =============================================================================
// FIMPromptBuilder.cpp — Fill-in-Middle Prompt Construction Implementation
// =============================================================================
#include "FIMPromptBuilder.h"
#include <sstream>
#include <algorithm>
#include <cmath>

using RawrXD::Agent::FIMPromptBuilder;
using RawrXD::Agent::FIMBuildResult;
using RawrXD::Agent::FIMPrompt;
using RawrXD::Agent::FIMFormat;

FIMPromptBuilder::FIMPromptBuilder() = default;

// ---------------------------------------------------------------------------
// Build from EditorContext
// ---------------------------------------------------------------------------

FIMBuildResult FIMPromptBuilder::Build(const EditorContext& ctx) const {
    if (ctx.full_content.empty()) {
        return FIMBuildResult::fail("Empty editor content");
    }
    if (ctx.cursor_line < 0) {
        return FIMBuildResult::fail("Invalid cursor position");
    }

    std::string prefix, suffix;
    SplitAtCursor(ctx.full_content, ctx.cursor_line, ctx.cursor_column, prefix, suffix);

    // Trim to fit context window
    std::string trimmedPrefix = prefix;
    std::string trimmedSuffix = suffix;
    TrimToFit(trimmedPrefix, trimmedSuffix);

    // Build formatted prompt
    std::string formatted = FormatPrompt(trimmedPrefix, trimmedSuffix, ctx.filename);

    FIMPrompt prompt;
    prompt.prefix = trimmedPrefix;
    prompt.suffix = trimmedSuffix;
    prompt.formatted_prompt = formatted;
    prompt.filename = ctx.filename;
    prompt.prefix_lines = CountLines(trimmedPrefix);
    prompt.suffix_lines = CountLines(trimmedSuffix);
    prompt.estimated_tokens = EstimateTokens(formatted);

    return FIMBuildResult::ok(prompt);
}

// ---------------------------------------------------------------------------
// Build from explicit parts
// ---------------------------------------------------------------------------

FIMBuildResult FIMPromptBuilder::BuildFromParts(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& filename) const
{
    std::string trimmedPrefix = prefix;
    std::string trimmedSuffix = suffix;
    TrimToFit(trimmedPrefix, trimmedSuffix);

    std::string formatted = FormatPrompt(trimmedPrefix, trimmedSuffix, filename);

    FIMPrompt prompt;
    prompt.prefix = trimmedPrefix;
    prompt.suffix = trimmedSuffix;
    prompt.formatted_prompt = formatted;
    prompt.filename = filename;
    prompt.prefix_lines = CountLines(trimmedPrefix);
    prompt.suffix_lines = CountLines(trimmedSuffix);
    prompt.estimated_tokens = EstimateTokens(formatted);

    return FIMBuildResult::ok(prompt);
}

// ---------------------------------------------------------------------------
// Split content at cursor
// ---------------------------------------------------------------------------

void FIMPromptBuilder::SplitAtCursor(
    const std::string& content,
    int line, int col,
    std::string& prefix, std::string& suffix) const
{
    // Find the byte offset for (line, col)
    int currentLine = 0;
    int currentCol = 0;
    size_t splitPos = 0;
    bool found = false;

    for (size_t i = 0; i < content.size(); ++i) {
        if (currentLine == line && currentCol == col) {
            splitPos = i;
            found = true;
            break;
        }
        if (content[i] == '\n') {
            currentLine++;
            currentCol = 0;
        } else {
            currentCol++;
        }
    }

    // If cursor is at end of file
    if (!found) {
        if (currentLine == line) {
            splitPos = content.size();
        } else {
            // Cursor beyond content — treat as end
            splitPos = content.size();
        }
    }

    prefix = content.substr(0, splitPos);
    suffix = (splitPos < content.size()) ? content.substr(splitPos) : "";
}

// ---------------------------------------------------------------------------
// Trim to fit context window
// ---------------------------------------------------------------------------

void FIMPromptBuilder::TrimToFit(std::string& prefix, std::string& suffix) const {
    int totalTokens = EstimateTokens(prefix) + EstimateTokens(suffix);
    if (totalTokens <= m_maxContextTokens) return;

    // Budget allocation
    int prefixBudget = static_cast<int>(m_maxContextTokens * m_prefixRatio);
    int suffixBudget = m_maxContextTokens - prefixBudget;

    // Trim prefix from the front (keep the most recent code)
    int prefixTokens = EstimateTokens(prefix);
    if (prefixTokens > prefixBudget) {
        // Approximate chars to keep
        size_t keepChars = static_cast<size_t>(prefixBudget) * 4;
        if (keepChars < prefix.size()) {
            // Find a newline boundary near the cut point
            size_t cutStart = prefix.size() - keepChars;
            size_t newlineBound = prefix.find('\n', cutStart);
            if (newlineBound != std::string::npos && newlineBound < prefix.size()) {
                prefix = prefix.substr(newlineBound + 1);
            } else {
                prefix = prefix.substr(cutStart);
            }
        }
    }

    // Trim suffix from the back (keep the immediately following code)
    int suffixTokens = EstimateTokens(suffix);
    if (suffixTokens > suffixBudget) {
        size_t keepChars = static_cast<size_t>(suffixBudget) * 4;
        if (keepChars < suffix.size()) {
            // Find a newline boundary near the cut point
            size_t cutEnd = keepChars;
            size_t newlineBound = suffix.rfind('\n', cutEnd);
            if (newlineBound != std::string::npos) {
                suffix = suffix.substr(0, newlineBound + 1);
            } else {
                suffix = suffix.substr(0, cutEnd);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Format with FIM tokens
// ---------------------------------------------------------------------------

std::string FIMPromptBuilder::FormatPrompt(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& filename) const
{
    std::ostringstream oss;

    switch (m_format) {
    case FIMFormat::Qwen:
        oss << "<|fim_prefix|>";
        if (m_includeFilename && !filename.empty()) {
            oss << "# " << filename << "\n";
        }
        oss << prefix;
        oss << "<|fim_suffix|>";
        oss << suffix;
        oss << "<|fim_middle|>";
        break;

    case FIMFormat::DeepSeek:
        oss << "<\xE2\x96\x81" "begin|>";  // <|fim▁begin|> (UTF-8 for ▁)
        if (m_includeFilename && !filename.empty()) {
            oss << "# " << filename << "\n";
        }
        oss << prefix;
        oss << "<\xE2\x96\x81" "hole|>";  // <|fim▁hole|>
        oss << suffix;
        oss << "<\xE2\x96\x81" "end|>";   // <|fim▁end|>
        break;

    case FIMFormat::StarCoder:
    case FIMFormat::CodeLlama:
        oss << "<PRE>";
        if (m_includeFilename && !filename.empty()) {
            oss << " # " << filename << "\n";
        }
        oss << prefix;
        oss << " <SUF>";
        oss << suffix;
        oss << " <MID>";
        break;
    }

    return oss.str();
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

int FIMPromptBuilder::EstimateTokens(const std::string& text) {
    // Rough estimate: ~4 chars per token for code
    // More accurate would use the actual tokenizer, but this is fast
    if (text.empty()) return 0;
    return static_cast<int>(std::ceil(text.size() / 4.0));
}

int FIMPromptBuilder::CountLines(const std::string& text) {
    if (text.empty()) return 0;
    int count = 1;
    for (char c : text) {
        if (c == '\n') count++;
    }
    return count;
}

std::string FIMPromptBuilder::GetLanguageHint(const std::string& language) const {
    // Map language IDs to file extension hints for the model
    if (language == "cpp" || language == "c++") return ".cpp";
    if (language == "c") return ".c";
    if (language == "python") return ".py";
    if (language == "javascript") return ".js";
    if (language == "typescript") return ".ts";
    if (language == "rust") return ".rs";
    if (language == "go") return ".go";
    if (language == "java") return ".java";
    if (language == "csharp" || language == "c#") return ".cs";
    return "";
}
