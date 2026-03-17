#pragma once
#include <string>
#include <vector>
#include <memory>
#include "language_server_integration.hpp" // For Range?

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
        // Explicit logic: Store patch
        m_stagedPatches.push_back(candidate);
    }
    
    std::vector<HotPatchCandidate> getStagedPatches() const {
        return m_stagedPatches;
    }
    
    // Validate and auto-correct code responses
    std::string validateAndCorrect(const std::string& code) {
        std::string result = code;
        bool modified = false;
        
        // Common C++ hallucinations and mistakes
        std::vector<std::pair<std::string, std::string>> corrections = {
            {"#include <iostream.h>", "#include <iostream>"},
            {"void main()", "int main()"},
            {"using namespace std;", "using namespace std;"},
            {"NULL", "nullptr"},
            {"malloc(", "new "},
            {"free(", "delete "},
            {"strcpy(", "std::strncpy("},
            {"printf(", "std::cout << "},
            {"scanf(", "std::cin >> "}
        };
        
        for (const auto& [bad, good] : corrections) {
            size_t pos = 0;
            while ((pos = result.find(bad, pos)) != std::string::npos) {
                result.replace(pos, bad.length(), good);
                pos += good.length();
                modified = true;
            }
        }
        
        // Check for unclosed braces (simple heuristic)
        int braceBalance = 0;
        for (char c : result) {
            if (c == '{') braceBalance++;
            else if (c == '}') braceBalance--;
        }
        
        if (braceBalance > 0) {
            // Add missing closing braces
            for (int i = 0; i < braceBalance; i++) {
                result += "\n}";
            }
            modified = true;
        } else if (braceBalance < 0) {
            // Remove excess closing braces
            int toRemove = -braceBalance;
            for (int i = 0; i < toRemove && !result.empty(); i++) {
                size_t lastBrace = result.rfind('}');
                if (lastBrace != std::string::npos) {
                    result.erase(lastBrace, 1);
                }
            }
            modified = true;
        }
        
        return result;
    }
    
private:
    std::vector<HotPatchCandidate> m_stagedPatches;
};
