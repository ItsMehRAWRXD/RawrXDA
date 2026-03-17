#pragma once
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include "language_server_integration.hpp" 

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

    void stagePatch(const HotPatchCandidate& candidate) {
        m_stagedPatches.push_back(candidate);
        if (candidate.autoApply) {
            applyPatch(candidate);
        }
    }
    
    std::vector<HotPatchCandidate> getStagedPatches() const {
        return m_stagedPatches;
    }

    bool applyPatch(const HotPatchCandidate& patch) {
        std::ifstream inFile(patch.uri);
        if (!inFile.is_open()) return false;
        
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        std::string content = buffer.str();
        inFile.close();

        // Line-based replacement if range is valid
        std::vector<std::string> lines;
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) lines.push_back(line);

        int startLine = patch.range.start.line;
        int endLine = patch.range.end.line;
        
        if (startLine >= 0 && startLine <= (int)lines.size()) {
            if (endLine >= startLine) {
                auto startIt = lines.begin() + startLine;
                auto endIt = (endLine < (int)lines.size()) ? lines.begin() + endLine + 1 : lines.end();
                
                if (startIt < lines.end()) {
                    lines.erase(startIt, endIt);
                }
                
                std::vector<std::string> newLines;
                std::istringstream riss(patch.replacement);
                std::string rl;
                while (std::getline(riss, rl)) newLines.push_back(rl);
                
                lines.insert(lines.begin() + startLine, newLines.begin(), newLines.end());
            }
        } else {
            lines.push_back(patch.replacement); // Fallback append
        }

        std::ofstream outFile(patch.uri);
        if (!outFile.is_open()) return false;
        
        for (size_t i = 0; i < lines.size(); ++i) {
            outFile << lines[i] << (i < lines.size() - 1 ? "\n" : "");
        }
        return true;
    }
    
    // Validate and auto-correct code responses
    std::string validateAndCorrect(const std::string& code) {
        std::string result = code;
        bool modified = false;
        
        // HALLUCINATION FIXES (Hotpatched)
        std::map<std::string, std::string> corrections = {
            {"#include <iostream.h>", "#include <iostream>"},
            {"void main()", "int main()"},
            {"NULL", "nullptr"},
            {"<cstdio.h>", "<cstdio>"},
            {"gets(", "fgets("} // Safety fix
        };
        
        for (const auto& [bad, good] : corrections) {
            size_t pos = 0;
            while ((pos = result.find(bad, pos)) != std::string::npos) {
                result.replace(pos, bad.length(), good);
                pos += good.length();
                modified = true;
            }
        }
        
        // Auto-fix braces
        int braceBalance = 0;
        for (char c : result) {
            if (c == '{') braceBalance++;
            else if (c == '}') braceBalance--;
        }
        
        if (braceBalance > 0) {
            for (int i = 0; i < braceBalance; i++) result += "\n}";
        }
        
        return result;
    }
    
private:
    std::vector<HotPatchCandidate> m_stagedPatches;
};
