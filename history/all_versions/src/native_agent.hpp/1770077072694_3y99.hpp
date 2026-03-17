#pragma once
#include "cpu_inference_engine.h"
#include "advanced_agent_features.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <thread>
#include <chrono>

namespace RawrXD {

class NativeAgent {
public:
    using OutputCallback = std::function<void(const std::string&)>;

    NativeAgent(CPUInference::CPUInferenceEngine* engine) : m_engine(engine) {}

    void SetOutputCallback(OutputCallback cb) { m_callback = cb; }
    void SetDeepThink(bool enabled) { m_deepThink = enabled; }
    void SetDeepResearch(bool enabled) { m_deepResearch = enabled; }
    void SetNoRefusal(bool enabled) { m_noRefusal = enabled; }
    void SetMaxMode(bool enabled) { m_maxMode = enabled; if(m_engine) m_engine->SetThreadCount(enabled ? std::thread::hardware_concurrency() : 4); }

    void Ask(const std::string& query) {
        if (!m_engine || !m_engine->IsModelLoaded()) {
            Print("[Agent] No model loaded. Use /load <path> first.\n");
            return;
        }

        std::string fullPrompt = BuildPrompt(query);
        Print("[Agent] Generating response...\n");

        // Streaming generation
        std::vector<int32_t> input_ids = m_engine->Tokenize(fullPrompt);
        
        int tokensGenerated = 0;
        bool inThought = false;

        m_engine->GenerateStreaming(input_ids, 2048, 
            [&](const std::string& token) {
                // Handling <thought> visibility
                if (token.find("<thought>") != std::string::npos) inThought = true;
                
                if (inThought) {
                    Print(token); // GUI might handle color, or we strip codes
                } else {
                    Print(token);
                }
                
                if (token.find("</thought>") != std::string::npos) {
                    inThought = false;
                    Print("\n");
                }
                tokensGenerated++;
            },
            [&]() {
                Print("\n[Done]\n");
            }
        );
    }

    void CreateReactServerPlan() {
         Ask("Generate a comprehensive plan to create a React Server Component architecture from scratch in C++. detailed file list and logic.");
    }

    void Edit(const std::string& filePath, const std::string& instructions) {
        std::string content = ReadFile(filePath);
        if (content.empty()) return;
        std::string prompt = "Original Code:\n" + content + "\n\nInstructions: " + instructions + "\n\nProvide the complete edited code with changes applied.";
        Ask(prompt);
    }

    void Edit(const std::string& filePath, const std::string& instructions) {
        std::string content = ReadFile(filePath);
        if (content.empty()) return;
        std::string prompt = "Original Code:\n" + content + "\n\nInstructions: " + instructions + "\n\nProvide the complete edited code with changes applied.";
        Ask(prompt);
    }

    void Edit(const std::string& filePath, const std::string& instructions) {
        std::string content = ReadFile(filePath);
        if (content.empty()) return;
        std::string prompt = "Original Code:\n" + content + "\n\nInstructions: " + instructions + "\n\nProvide the complete edited code with changes applied.";
        Ask(prompt);
    }

    void HotPatch(const std::string& filePath) {
        std::string content = ReadFile(filePath);
        if (content.empty()) return;
        // Simulating the "AgentHotPatcher" behavior natively
        std::string prompt = "Review the following code for hallucinations, invalid paths, and logical contradictions. Rewrite the code to fix these issues immediately.\n\nCode:\n" + content;
        Ask(prompt);
    }

private:
    CPUInference::CPUInferenceEngine* m_engine;
    OutputCallback m_callback;
    bool m_deepThink = false;
    bool m_deepResearch = false;
    bool m_noRefusal = false;
    bool m_maxMode = false;

    void Print(const std::string& s) {
        if (m_callback) m_callback(s);
        else std::cout << s; // Fallback for CLI
    }

    std::string ReadFile(const std::string& path) {
        std::ifstream f(path);
        if (!f) {
            Print("[Error] Could not read file: " + path + "\n");
            return "";
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        return buffer.str();
    }

    std::string BuildPrompt(const std::string& userQuery) {
        // If Deep Research is on, let AdvancedFeatures handle the full construction including context
        if (m_deepResearch) {
             return ::AdvancedFeatures::DeepResearch(userQuery);
        }

        std::string modifiedQuery = userQuery;
        
        if (m_noRefusal) {
             modifiedQuery = ::AdvancedFeatures::NoRefusal(modifiedQuery);
        }
        
        if (m_deepThink) {
             modifiedQuery = ::AdvancedFeatures::ChainOfThought(modifiedQuery);
        }
        
        // Fallback to standard templating if not using the Research override which returns a full blob
        std::string sys = "You are RawrXD, an expert AI programming assistant.";
        
        std::string prompt = "<|system|>\n" + sys + "\n";
        prompt += "<|user|>\n" + modifiedQuery + "\n"; // Inject the modified query (CoT / NoRefusal)
        prompt += "<|assistant|>\n";
        
        if (m_deepThink) prompt += "Here is my thought process:\n";
        
        return prompt;
    }

    std::string PerformResearch(const std::string& query) { return ""; }
};

}
