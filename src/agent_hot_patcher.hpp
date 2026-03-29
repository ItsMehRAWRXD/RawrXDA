#pragma once
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include "language_server_integration.hpp" // For Range

struct HotPatchCandidate {
    std::string uri;
    Range range;
    std::string replacement;
    float confidence;
    bool autoApply;
};

class AgentHotPatcher {
public:
    static AgentHotPatcher* instance() {
        static AgentHotPatcher inst;
        return &inst;
    }

    // Stage a patch. If candidate.autoApply is true, apply immediately.
    void stagePatch(const HotPatchCandidate& candidate) {
        m_stagedPatches.push_back(candidate);
        if (candidate.autoApply) {
            applyPatch(candidate);
        }
    }

    // Apply a patch: splice replacement text into the file at candidate.range.
    // Lines and characters in Range are 0-based.
    bool applyPatch(const HotPatchCandidate& candidate) {
        // Read the target file
        std::ifstream in(candidate.uri);
        if (!in.is_open()) return false;
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
        in.close();

        int startLine = candidate.range.start.line;
        int startChar = candidate.range.start.character;
        int endLine   = candidate.range.end.line;
        int endChar   = candidate.range.end.character;

        if (startLine < 0 || endLine >= static_cast<int>(lines.size())) return false;
        if (startLine > endLine) return false;

        // Build prefix (before range) and suffix (after range)
        std::string prefix = lines[startLine].substr(0, startChar);
        std::string suffix = (endLine < static_cast<int>(lines.size()))
                             ? lines[endLine].substr(std::min(endChar, static_cast<int>(lines[endLine].size())))
                             : "";

        // Splice: keep lines before startLine, insert replacement, keep lines after endLine
        std::vector<std::string> result;
        result.reserve(startLine + 2 + lines.size() - endLine - 1);
        for (int i = 0; i < startLine; ++i) result.push_back(lines[i]);

        // Build the replacement block as line-split strings
        std::string block = prefix + candidate.replacement + suffix;
        std::istringstream ss(block);
        while (std::getline(ss, line)) result.push_back(line);
        // Handle trailing newline in block
        if (!block.empty() && block.back() == '\n') result.push_back("");

        for (int i = endLine + 1; i < static_cast<int>(lines.size()); ++i)
            result.push_back(lines[i]);

        // Write back
        std::ofstream out(candidate.uri, std::ios::trunc);
        if (!out.is_open()) return false;
        for (size_t i = 0; i < result.size(); ++i) {
            out << result[i];
            if (i + 1 < result.size()) out << '\n';
        }
        return true;
    }

    // Apply all staged patches that have autoApply == true (re-entrant safe).
    void commitAllAutoPatches() {
        for (const auto& p : m_stagedPatches) {
            if (p.autoApply) applyPatch(p);
        }
    }

    std::vector<HotPatchCandidate> getStagedPatches() const {
        return m_stagedPatches;
    }

private:
    std::vector<HotPatchCandidate> m_stagedPatches;
};
