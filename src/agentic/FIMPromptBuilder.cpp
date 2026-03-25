<<<<<<< HEAD
// =============================================================================
// FIMPromptBuilder.cpp — Minimal FIM Prompt Builder for CLI
// =============================================================================
#include "FIMPromptBuilder.h"
#include <sstream>
#include <algorithm>

using RawrXD::Agent::FIMPromptBuilder;
=======
// =============================================================================
// FIMPromptBuilder.cpp — Fill-in-Middle Prompt Construction Implementation
// =============================================================================
#include "FIMPromptBuilder.h"
#include "../../include/enterprise_license.h"
#include <sstream>
#include <algorithm>
#include <cmath>

#ifdef ERROR
#undef ERROR
#endif
#include "agentic_observability.h"

// SCAFFOLD_118: FIM prompt builder


static AgenticObservability& GetObs() {
    static AgenticObservability instance;
    return instance;
}
static const char* kFIMComponent = "FIMPromptBuilder";

using RawrXD::Agent::FIMPromptBuilder;
>>>>>>> origin/main
using RawrXD::Agent::FIMBuildResult;
using RawrXD::Agent::FIMPrompt;
using RawrXD::Agent::FIMFormat;

<<<<<<< HEAD
=======
FIMPromptBuilder::FIMPromptBuilder() = default;

// ---------------------------------------------------------------------------
// Build from EditorContext
// ---------------------------------------------------------------------------

FIMBuildResult FIMPromptBuilder::Build(const EditorContext& ctx) const {
    auto timing = GetObs().measureDuration("fim.build_ms");

    if (ctx.full_content.empty()) {
        GetObs().logWarn(kFIMComponent, "Build failed: empty editor content");
        return FIMBuildResult::fail("Empty editor content");
    }
    if (ctx.cursor_line < 0) {
        GetObs().logWarn(kFIMComponent, "Build failed: invalid cursor position");
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

    GetObs().logDebug(kFIMComponent, "FIM prompt built", {
        {"filename", ctx.filename},
        {"prefixLines", prompt.prefix_lines},
        {"suffixLines", prompt.suffix_lines},
        {"estimatedTokens", prompt.estimated_tokens}
    });
    GetObs().recordHistogram("fim.estimated_tokens", static_cast<float>(prompt.estimated_tokens));
    GetObs().incrementCounter("fim.builds_success");

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
    auto& lic = RawrXD::License::EnterpriseLicenseV2::Instance();
    if (!lic.gate(RawrXD::License::FeatureID::PromptLibrary,
            "FIMPromptBuilder::BuildFromParts")) {
        return FIMBuildResult::fail("Prompt Library requires a Professional license");
    }
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

>>>>>>> origin/main
int FIMPromptBuilder::EstimateTokens(const std::string& text) {
    if (text.empty()) return 0;
    return static_cast<int>((text.size() / 4) + 1);
}

FIMPromptBuilder::FIMPromptBuilder() = default;

// ---------------------------------------------------------------------------
// Build from EditorContext - Minimal implementation
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

    // Simple FIM format for testing
    std::string formatted = "<fim_prefix>" + prefix + "<fim_suffix>" + suffix + "<fim_middle>";

    FIMPrompt prompt;
    prompt.prefix = prefix;
    prompt.suffix = suffix;
    prompt.formatted_prompt = formatted;
    prompt.filename = ctx.filename;
    prompt.prefix_lines = std::count(prefix.begin(), prefix.end(), '\n') + 1;
    prompt.suffix_lines = std::count(suffix.begin(), suffix.end(), '\n') + 1;
    prompt.estimated_tokens = (prefix.length() + suffix.length()) / 4; // Rough estimate

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
    std::string formatted = "<fim_prefix>" + prefix + "<fim_suffix>" + suffix + "<fim_middle>";

    FIMPrompt prompt;
    prompt.prefix = prefix;
    prompt.suffix = suffix;
    prompt.formatted_prompt = formatted;
    prompt.filename = filename;
    prompt.prefix_lines = std::count(prefix.begin(), prefix.end(), '\n') + 1;
    prompt.suffix_lines = std::count(suffix.begin(), suffix.end(), '\n') + 1;
    prompt.estimated_tokens = (prefix.length() + suffix.length()) / 4;

    return FIMBuildResult::ok(prompt);
}

// ---------------------------------------------------------------------------
// Split content at cursor
// ---------------------------------------------------------------------------

void FIMPromptBuilder::SplitAtCursor(
    const std::string& content,
    int line, int col,
    std::string& outPrefix,
    std::string& outSuffix) const
{
    std::istringstream iss(content);
    std::string currentLine;
    std::vector<std::string> lines;
    
    while (std::getline(iss, currentLine)) {
        lines.push_back(currentLine);
    }
    
    if (line >= static_cast<int>(lines.size())) {
        outPrefix = content;
        outSuffix = "";
        return;
    }
    
    // Reconstruct prefix (up to cursor)
    std::ostringstream prefixStream;
    for (int i = 0; i < line; ++i) {
        prefixStream << lines[i] << "\n";
    }
    if (col > 0 && col <= static_cast<int>(lines[line].length())) {
        prefixStream << lines[line].substr(0, col);
    } else {
        prefixStream << lines[line];
    }
    outPrefix = prefixStream.str();
    
    // Reconstruct suffix (from cursor)
    std::ostringstream suffixStream;
    if (col > 0 && col < static_cast<int>(lines[line].length())) {
        suffixStream << lines[line].substr(col);
    }
    for (size_t i = line + 1; i < lines.size(); ++i) {
        suffixStream << "\n" << lines[i];
    }
    outSuffix = suffixStream.str();
}
