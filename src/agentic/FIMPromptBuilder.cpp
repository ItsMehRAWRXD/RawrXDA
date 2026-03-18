// =============================================================================
// FIMPromptBuilder.cpp — Minimal FIM Prompt Builder for CLI
// =============================================================================
#include "FIMPromptBuilder.h"
#include <sstream>
#include <algorithm>

using RawrXD::Agent::FIMPromptBuilder;
using RawrXD::Agent::FIMBuildResult;
using RawrXD::Agent::FIMPrompt;
using RawrXD::Agent::FIMFormat;

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
