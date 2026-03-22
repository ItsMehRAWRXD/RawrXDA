// ============================================================================
// context_assembler.cpp — Ghost Text Context Window Implementation
// ============================================================================
// Priority-based context assembly, FIM prompt building, suffix extraction,
// git diff parsing, and recent edit buffer management.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "agentic/context_assembler.h"

#include <sstream>
#include <algorithm>
#include <regex>
#include <chrono>
#include <cstdio>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace RawrXD {
namespace Context {

// ============================================================================
// Token estimation helper
// ============================================================================

int ContextAssembler::estimateTokens(const std::string& text) {
    if (text.empty()) return 0;
    int count = 0;
    bool inWord = false;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            if (!inWord) { ++count; inWord = true; }
        } else {
            inWord = false;
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') ++count;
        }
    }
    return static_cast<int>(count * 1.3f);
}

std::string ContextAssembler::truncateToTokens(const std::string& text, int maxTokens) {
    if (estimateTokens(text) <= maxTokens) return text;

    // Approximate: 4 chars per token
    size_t maxChars = static_cast<size_t>(maxTokens) * 4;
    if (maxChars >= text.size()) return text;
    return text.substr(0, maxChars);
}

// ============================================================================
// Recent Edit Buffer
// ============================================================================

RecentEditBuffer::RecentEditBuffer(size_t maxEdits) : m_maxEdits(maxEdits) {}

void RecentEditBuffer::recordEdit(const EditEvent& edit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_edits.push_back(edit);
    while (m_edits.size() > m_maxEdits) {
        m_edits.pop_front();
    }
}

std::vector<EditEvent> RecentEditBuffer::getRecent(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<EditEvent> result;
    size_t start = (m_edits.size() > count) ? m_edits.size() - count : 0;
    for (size_t i = start; i < m_edits.size(); ++i) {
        result.push_back(m_edits[i]);
    }
    return result;
}

std::string RecentEditBuffer::formatAsContext(size_t maxTokens) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ostringstream oss;
    oss << "// Recent edits:\n";

    int tokens = 0;
    // Iterate from most recent to oldest
    for (auto it = m_edits.rbegin(); it != m_edits.rend(); ++it) {
        std::string entry = "// " + it->file.filename().string() +
                            " L" + std::to_string(it->line) +
                            ": " + it->newText + "\n";
        int entryTokens = ContextAssembler::estimateTokens(entry);
        if (tokens + entryTokens > static_cast<int>(maxTokens)) break;
        oss << entry;
        tokens += entryTokens;
    }

    return oss.str();
}

void RecentEditBuffer::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_edits.clear();
}

size_t RecentEditBuffer::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_edits.size();
}

// ============================================================================
// Git Diff Parser
// ============================================================================

std::vector<DiffHunk> GitDiffParser::parseUnifiedDiff(const std::string& diffOutput) {
    std::vector<DiffHunk> hunks;

    static const std::regex hunkHeader(
        R"(@@ -(\d+),?(\d*) \+(\d+),?(\d*) @@)",
        std::regex::optimize
    );

    static const std::regex fileHeader(
        R"(\+\+\+ b/(.+))",
        std::regex::optimize
    );

    std::istringstream stream(diffOutput);
    std::string line;
    std::string currentFile;

    DiffHunk* currentHunk = nullptr;

    while (std::getline(stream, line)) {
        std::smatch match;

        if (std::regex_search(line, match, fileHeader)) {
            currentFile = match[1].str();
            continue;
        }

        if (std::regex_search(line, match, hunkHeader)) {
            hunks.push_back({});
            currentHunk = &hunks.back();
            currentHunk->file = currentFile;
            currentHunk->oldStart = std::stoi(match[1].str());
            currentHunk->oldCount = match[2].length() > 0 ? std::stoi(match[2].str()) : 1;
            currentHunk->newStart = std::stoi(match[3].str());
            currentHunk->newCount = match[4].length() > 0 ? std::stoi(match[4].str()) : 1;
            continue;
        }

        if (currentHunk && (line.size() > 0) &&
            (line[0] == '+' || line[0] == '-' || line[0] == ' ')) {
            currentHunk->content += line + "\n";
        }
    }

    return hunks;
}

std::vector<DiffHunk> GitDiffParser::getRecentChanges(const fs::path& repoRoot) {
    // Execute git diff HEAD
    std::string command = "git -C \"" + repoRoot.string() + "\" diff HEAD";

    std::string output;

#ifdef _WIN32
    STARTUPINFOA si{}; si.cb = sizeof(si);
    SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE;
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return {};
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite; si.hStdError = hWrite;
    PROCESS_INFORMATION pi{};
    std::vector<char> cmdBuf(command.begin(), command.end());
    cmdBuf.push_back('\0');
    BOOL ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);
    if (!ok) { CloseHandle(hRead); return {}; }
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hRead, buffer, sizeof(buffer)-1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output.append(buffer, bytesRead);
    }
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, 30000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return {};

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    pclose(pipe);
#endif

    return parseUnifiedDiff(output);
}

std::string GitDiffParser::formatAsContext(const std::vector<DiffHunk>& hunks,
                                            size_t maxTokens) {
    std::ostringstream oss;
    oss << "// Recent git changes:\n";

    int tokens = 0;
    for (const auto& hunk : hunks) {
        std::string entry = "// " + hunk.file +
                            " @@ -" + std::to_string(hunk.oldStart) +
                            " +" + std::to_string(hunk.newStart) + " @@\n" +
                            hunk.content;
        int entryTokens = ContextAssembler::estimateTokens(entry);
        if (tokens + entryTokens > static_cast<int>(maxTokens)) break;
        oss << entry;
        tokens += entryTokens;
    }

    return oss.str();
}

// ============================================================================
// Line splitting helper
// ============================================================================

static std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// ============================================================================
// Context Assembler
// ============================================================================

ContextAssembler::ContextAssembler() = default;
ContextAssembler::~ContextAssembler() = default;

std::string ContextAssembler::extractCurrentFunction(const std::string& fileContent,
                                                      int cursorLine) const {
    auto lines = splitLines(fileContent);
    if (cursorLine < 1 || cursorLine > static_cast<int>(lines.size())) return "";

    // Walk backward to find function start (opening brace at depth 0)
    int braceDepth = 0;
    int funcStart = cursorLine - 1;

    // From cursor, count braces moving backward
    for (int i = cursorLine - 1; i >= 0; --i) {
        for (auto it = lines[i].rbegin(); it != lines[i].rend(); ++it) {
            if (*it == '}') ++braceDepth;
            if (*it == '{') --braceDepth;
        }
        if (braceDepth < 0) {
            // We've found a '{' that opens our scope
            funcStart = i;
            // Walk up to find function signature (first non-blank line before '{')
            while (funcStart > 0 && lines[funcStart].find('{') == std::string::npos) {
                funcStart--;
            }
            // Include lines preceding the brace that form the signature
            while (funcStart > 0 && !lines[funcStart - 1].empty() &&
                   lines[funcStart - 1].find(';') == std::string::npos &&
                   lines[funcStart - 1].find('}') == std::string::npos) {
                funcStart--;
            }
            break;
        }
    }

    // Walk forward to find function end
    braceDepth = 0;
    int funcEnd = cursorLine - 1;
    bool foundOpen = false;

    for (int i = funcStart; i < static_cast<int>(lines.size()); ++i) {
        for (char c : lines[i]) {
            if (c == '{') { braceDepth++; foundOpen = true; }
            if (c == '}') braceDepth--;
        }
        if (foundOpen && braceDepth == 0) {
            funcEnd = i;
            break;
        }
    }

    std::ostringstream oss;
    for (int i = funcStart; i <= funcEnd && i < static_cast<int>(lines.size()); ++i) {
        oss << lines[i] << "\n";
    }
    return oss.str();
}

std::string ContextAssembler::extractImports(const std::string& fileContent) const {
    static const std::regex importRegex(
        R"(^\s*#\s*(?:include|import)\s+[<"][^>"]+[>"])",
        std::regex::optimize
    );

    std::ostringstream oss;
    std::sregex_iterator it(fileContent.begin(), fileContent.end(), importRegex);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        oss << it->str() << "\n";
    }

    return oss.str();
}

std::string ContextAssembler::extractSiblingFunctions(const std::string& fileContent,
                                                       int cursorLine,
                                                       int maxFunctions) const {
    auto lines = splitLines(fileContent);
    std::ostringstream oss;

    static const std::regex funcRegex(
        R"(^\s*(?:[\w:*&<>]+\s+)+(\w+)\s*\([^;]*$)",
        std::regex::optimize
    );

    int found = 0;
    int braceDepth = 0;
    int funcStart = -1;

    for (int i = 0; i < static_cast<int>(lines.size()) && found < maxFunctions; ++i) {
        std::smatch match;
        if (funcStart < 0 && std::regex_search(lines[i], match, funcRegex)) {
            funcStart = i;
            braceDepth = 0;
        }

        if (funcStart >= 0) {
            for (char c : lines[i]) {
                if (c == '{') braceDepth++;
                if (c == '}') braceDepth--;
            }

            if (braceDepth == 0 && lines[i].find('}') != std::string::npos) {
                // Skip the function containing the cursor
                if (cursorLine < funcStart + 1 || cursorLine > i + 1) {
                    // Include this function's signature (first 3 lines)
                    int previewEnd = std::min(funcStart + 3, i + 1);
                    for (int j = funcStart; j < previewEnd; ++j) {
                        oss << lines[j] << "\n";
                    }
                    if (previewEnd < i + 1) oss << "    // ...\n";
                    oss << "}\n\n";
                    found++;
                }
                funcStart = -1;
            }
        }
    }

    return oss.str();
}

std::string ContextAssembler::extractPrefix(const std::string& fileContent,
                                             int cursorLine, int cursorColumn,
                                             int maxTokens) const {
    auto lines = splitLines(fileContent);
    std::ostringstream oss;

    // Build prefix up to cursor position
    int tokenBudget = maxTokens;
    int startLine = std::max(0, cursorLine - 1 - (maxTokens / 10));

    for (int i = startLine; i < cursorLine - 1 && i < static_cast<int>(lines.size()); ++i) {
        std::string line = lines[i] + "\n";
        int lineTokens = estimateTokens(line);
        if (tokenBudget - lineTokens < 0) break;
        oss << line;
        tokenBudget -= lineTokens;
    }

    // Partial last line up to cursor
    if (cursorLine - 1 < static_cast<int>(lines.size())) {
        std::string lastLine = lines[cursorLine - 1];
        if (cursorColumn > 0 && cursorColumn <= static_cast<int>(lastLine.size())) {
            oss << lastLine.substr(0, cursorColumn);
        } else {
            oss << lastLine;
        }
    }

    return oss.str();
}

std::string ContextAssembler::extractSuffix(const std::string& fileContent,
                                             int cursorLine, int cursorColumn,
                                             int maxTokens) const {
    auto lines = splitLines(fileContent);
    std::ostringstream oss;

    // Suffix starts at cursor position
    if (cursorLine - 1 < static_cast<int>(lines.size())) {
        std::string firstLine = lines[cursorLine - 1];
        if (cursorColumn > 0 && cursorColumn < static_cast<int>(firstLine.size())) {
            oss << firstLine.substr(cursorColumn) << "\n";
        }
    }

    int tokenBudget = maxTokens;
    int endLine = std::min(static_cast<int>(lines.size()),
                           cursorLine + (maxTokens / 10));

    for (int i = cursorLine; i < endLine; ++i) {
        std::string line = lines[i] + "\n";
        int lineTokens = estimateTokens(line);
        if (tokenBudget - lineTokens < 0) break;
        oss << line;
        tokenBudget -= lineTokens;
    }

    return oss.str();
}

std::string ContextAssembler::prioritySample(const std::vector<PriorityChunk>& chunks,
                                              int maxTokens) const {
    // Sort by priority (lower = higher priority)
    auto sorted = chunks;
    std::sort(sorted.begin(), sorted.end(),
              [](const PriorityChunk& a, const PriorityChunk& b) {
                  return a.priority < b.priority;
              });

    std::ostringstream oss;
    int remaining = maxTokens;

    for (const auto& chunk : sorted) {
        if (remaining <= 0) break;

        if (chunk.estimatedTokens <= remaining) {
            oss << chunk.content << "\n";
            remaining -= chunk.estimatedTokens;
        } else {
            // Truncate to fit
            oss << truncateToTokens(chunk.content, remaining) << "\n";
            remaining = 0;
        }
    }

    return oss.str();
}

ContextResult ContextAssembler::assemble(const std::string& fileContent,
                                          int cursorLine, int cursorColumn,
                                          const std::string& language) {
    ContextHierarchy hierarchy;
    hierarchy.languageHint = language;
    return assembleWithHierarchy(fileContent, cursorLine, cursorColumn,
                                 language, hierarchy);
}

ContextResult ContextAssembler::assembleWithHierarchy(const std::string& fileContent,
                                                       int cursorLine, int cursorColumn,
                                                       const std::string& language,
                                                       const ContextHierarchy& additionalCtx) {
    std::vector<PriorityChunk> chunks;

    // Priority 1: Current function (2K tokens)
    std::string currentFunc = additionalCtx.currentFunction.empty()
        ? extractCurrentFunction(fileContent, cursorLine)
        : additionalCtx.currentFunction;
    if (!currentFunc.empty()) {
        chunks.push_back({1, truncateToTokens(currentFunc, 2000),
                          estimateTokens(currentFunc), "current_function"});
    }

    // Priority 2: Imports (1K tokens)
    std::string imports = additionalCtx.imports.empty()
        ? extractImports(fileContent)
        : additionalCtx.imports;
    if (!imports.empty()) {
        chunks.push_back({2, truncateToTokens(imports, 1000),
                          estimateTokens(imports), "imports"});
    }

    // Priority 3: Recent edits (3K tokens)
    std::string recentEdits = additionalCtx.recentEdits.empty()
        ? m_editBuffer.formatAsContext(3000)
        : additionalCtx.recentEdits;
    if (!recentEdits.empty()) {
        chunks.push_back({3, truncateToTokens(recentEdits, 3000),
                          estimateTokens(recentEdits), "recent_edits"});
    }

    // Priority 4: Sibling functions (2K tokens)
    std::string siblings = additionalCtx.siblingFunctions.empty()
        ? extractSiblingFunctions(fileContent, cursorLine)
        : additionalCtx.siblingFunctions;
    if (!siblings.empty()) {
        chunks.push_back({4, truncateToTokens(siblings, 2000),
                          estimateTokens(siblings), "siblings"});
    }

    // Priority 5: File summary (0.5K tokens)
    if (!additionalCtx.fileSummary.empty()) {
        chunks.push_back({5, truncateToTokens(additionalCtx.fileSummary, 500),
                          estimateTokens(additionalCtx.fileSummary), "file_summary"});
    }

    std::string assembled = prioritySample(chunks, m_maxTokens);
    int totalTokens = estimateTokens(assembled);

    return ContextResult::ok(std::move(assembled), totalTokens);
}

std::string ContextAssembler::buildFIMPrompt(const std::string& prefix,
                                              const std::string& suffix,
                                              const std::string& context) const {
    // Qwen2.5-Coder / CodeLlama FIM format
    std::ostringstream oss;

    if (!context.empty()) {
        oss << "<|file_sep|>\n" << context << "\n";
    }

    oss << "<|fim_prefix|>" << prefix
        << "<|fim_suffix|>" << suffix
        << "<|fim_middle|>";

    return oss.str();
}

} // namespace Context
} // namespace RawrXD
