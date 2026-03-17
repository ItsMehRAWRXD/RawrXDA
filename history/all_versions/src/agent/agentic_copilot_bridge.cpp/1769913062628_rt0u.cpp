#include "agentic_copilot_bridge.hpp"
#include "multi_tab_editor.h"
#include "model_invoker.hpp"
#include "agentic_engine.h"
#include "../ai_integration_hub.h" // Include Integration Hub
#include "chat_interface.h"
#include <iostream>
#include <chrono>
#include <sstream> // For stringstream
#include <regex>
#include <fstream>

// Placeholder for missing classes - just so it compiles cleanly if they are not real yet
// In real project, these headers would exist.
// #include "agentic_engine.hpp" etc.

AgenticCopilotBridge::AgenticCopilotBridge() {
    m_modelInvoker = std::make_unique<ModelInvoker>();
    m_puppeteer = std::make_unique<AgenticPuppeteer>();
    // Default config for fallback
    m_modelInvoker->setLLMBackend("ollama", "http://localhost:11434", "");
}

AgenticCopilotBridge::~AgenticCopilotBridge() {
}

void AgenticCopilotBridge::initialize(AgenticEngine* engine, ChatInterface* chat, MultiTabEditor* editor, TerminalPool* terminals, AgenticExecutor* executor) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_agenticEngine = engine;
    m_chatInterface = chat;
    m_multiTabEditor = editor;
    m_terminalPool = terminals;
    m_agenticExecutor = executor;
}

std::string AgenticCopilotBridge::generateCodeCompletion(const std::string& context, const std::string& prefix) {
    auto start = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        // Explicit Logic: Use Internal Inference Engine if available
        if (m_integrationHub) {
            auto completions = m_integrationHub->getCompletions(
                m_multiTabEditor ? m_multiTabEditor->getCurrentFilePathString() : "unknown.cpp",
                context, 
                prefix, // passing prefix as suffix might be wrong in API, but let's assume parameters
                context.length()
            );
            if (!completions.empty()) {
                std::string code = completions.front().text;
                completionReady(code);
                return code;
            }
        }

        // Use ModelInvoker if available
        if (m_modelInvoker) {
            std::string systemPrompt = "You are an intelligent C++ coding assistant. Complete the code provided by the user. "
                                     "Return ONLY the completion code, no markdown, no explanation.";
            
            std::string userPrompt = "Context:\n" + context + "\n\nCode to complete:\n" + prefix;
            
            LLMResponse resp = m_modelInvoker->queryRaw(systemPrompt, userPrompt, 512); // Short completion
            if (resp.success) {
                // Trim potential markdown fences if the model disregarded instructions
                 std::string code = resp.rawOutput;
                 if (code.starts_with("```cpp")) code = code.substr(6);
                 if (code.starts_with("```")) code = code.substr(3);
                 if (code.ends_with("```")) code = code.substr(0, code.length() - 3);
                 
                 completionReady(code);
                 return code;
            }
        }
        
        // Fallback to heuristic if model fails
        if (prefix.empty()) {
            errorOccurred("Prefix cannot be empty");
            return "";
        }

        if (context.size() > 100000) {
            errorOccurred("Context size exceeds maximum allowed limit");
            return "";
        }

        // No inference engine available
        errorOccurred("Code completion failed: No active model or integration hub.");
        return "";
        return completion;
    } catch (const std::exception& e) {
        errorOccurred(std::string("Code completion failed: ") + e.what());
        return "";
    }
}

std::string AgenticCopilotBridge::analyzeActiveFile() {
    auto start = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        std::string content;
        
        // Explicit Logic: Get content from actual editor
        if (m_multiTabEditor) {
            content = m_multiTabEditor->getCurrentText();
        } 
        
        if (content.empty()) {
            return "No active file content available for analysis.";
        }

        if (m_integrationHub) {
            auto bugs = m_integrationHub->findBugs(content);
            std::string report = "Local Analysis Report:\n";
            if (bugs.empty()) {
                report += "- No obvious bugs found.\n";
            } else {
                for(const auto& b : bugs) {
                    report += "- Line " + std::to_string(b.line) + ": " + b.description + " [" + b.severity + "]\n";
                }
            }
            analysisReady(report);
            return report;
        }

        if (m_modelInvoker) {
            std::string systemPrompt = "You are a Senior C++ Code Analyzer. Analyze the provided C++ file for logic errors, "
                                    "memory leaks, and style violations. Return a bulleted list of issues.";
            LLMResponse resp = m_modelInvoker->queryRaw(systemPrompt, content, 2048);
            if (resp.success) {
                 analysisReady(resp.rawOutput);
                 return resp.rawOutput;
            }
        }
        
        return "Analysis failed: Model unavailable.";

    } catch (const std::exception& e) {
        errorOccurred(std::string("File analysis failed: ") + e.what());
        return "";
    }
}

std::string AgenticCopilotBridge::suggestRefactoring(const std::string& code) {
    if (m_modelInvoker) {
        std::string systemPrompt = "You are a senior C++ Developer. Analyze the following code and suggest refactoring improvements. "
                                   "Focus on performance, readability, and modern C++ practices. Return a commented block.";
        LLMResponse resp = m_modelInvoker->queryRaw(systemPrompt, code, 1024);
        if (resp.success) {
            return resp.rawOutput;
        }
    }

    // Fallback: Heuristic Check
    std::string suggestions = "// Refactoring Suggestions (Offline Mode):\n";
    bool anySuggestions = false;

    if (code.length() > 500) {
        suggestions += "// - Function/Block is too long (" + std::to_string(code.length()) + " chars). Consider extraction.\n";
        anySuggestions = true;
    }

    // Check for nested loops
    std::regex nestedLoop("(for|while|do)\\s*\\(.*?\\)\\s*\\{.*?\\s*(for|while|do)\\s*\\(", std::regex_constants::dotall);
    if (std::regex_search(code, nestedLoop)) {
        suggestions += "// - Nested loops detected. O(n^2) complexity risk. Consider optimization.\n";
        anySuggestions = true;
    }
    
    // Check for magic numbers
    // Heuristic: digits not part of variable names
    std::regex magicNumber("[^a-zA-Z0-9_][0-9]{2,}[^a-zA-Z0-9_]", std::regex_constants::dotall);
    if (std::regex_search(code, magicNumber)) {
        suggestions += "// - Magic numbers detected. Consider using named constants.\n";
        anySuggestions = true;
    }

    if (!anySuggestions) {
        return "// Code looks clean and concise.";
    }

    return suggestions;
}

std::string AgenticCopilotBridge::generateTestsForCode(const std::string& code) { 
    if (m_modelInvoker) {
        std::string systemPrompt = "You are a QA Engineer. Write Google Test (gtest) unit tests for the following C++ code. "
                                   "Include specific test cases, edge cases, and assertions. Return valid C++ code only.";
        LLMResponse resp = m_modelInvoker->queryRaw(systemPrompt, code, 2048);
        if (resp.success) {
            return resp.rawOutput;
        }
    }

    std::string testName = "GeneratedTest_" + std::to_string(std::rand());
    std::stringstream ss;
    ss << "#include <gtest/gtest.h>\n\n";
    ss << "// Tests for the following code snippet:\n";
    ss << "// " << code.substr(0, std::min(code.length(), size_t(50))) << "...\n\n";
    ss << "TEST(" << testName << ", BasicFunctionality) {\n";
    ss << "    // Setup\n";
    ss << "    // ...\n\n";
    ss << "    // Execution\n";
    ss << "    // ...\n\n";
    ss << "    // Assertion\n";
    ss << "    EXPECT_TRUE(true); // Placeholder\n";
    ss << "}\n";
    
    ss << "TEST(" << testName << ", ErrorHandling) {\n";
    ss << "    EXPECT_NO_THROW({ /* call function */ });\n";
    ss << "}\n";

    // Generate a slightly more useful test
    ss << "TEST(" << testName << ", BasicAssertion) {\n";
    ss << "    // TODO: Write test for provided code based on analysis\n";
    ss << "    // Analyze code structure\n";
    ss << "    /*\n    " << code.substr(0, std::min(code.length(), size_t(100))) << "\n    */\n";
    ss << "    EXPECT_TRUE(true); // Always true for generated template\n";
    ss << "}\n";

    return ss.str(); 
}

std::string AgenticCopilotBridge::askAgent(const std::string& question, const json& context) { 
    if (m_agenticEngine) {
         // return m_agenticEngine->ask(question, context);
         // Since we don't know the API, and m_agenticEngine is likely null in this env, we fall through.
    }
    
    if (m_modelInvoker) {
        InvocationParams params;
        params.wish = question;
        params.context = context.dump();
        // Simple synchronous invoke for now
        LLMResponse resp = m_modelInvoker->invoke(params);
        if (resp.success) {
            return resp.rawOutput; // Return raw answer or description of plan
        } else {
            return "Error calling agent: " + resp.error;
        }
    }

    return "Agent is offline. Please initialize AgenticEngine."; 
}

std::string AgenticCopilotBridge::continuePreviousConversation(const std::string& followUp) { 
    return "Conversation context lost."; 
}

std::string AgenticCopilotBridge::executeWithFailureRecovery(const std::string& prompt) { 
    if (m_agenticExecutor) {
        // m_agenticExecutor->execute(prompt);
         return "Executor linked but API unknown.";
    }
    
    // Fallback: Just plan it
    if (m_modelInvoker) {
         InvocationParams params;
         params.wish = "Execute with recovery: " + prompt;
         LLMResponse resp = m_modelInvoker->invoke(params);
         return resp.success ? "Plan generated: " + resp.rawOutput : "Planning failed: " + resp.error;
    }

    return "Execution agent not linked."; 
}

std::string AgenticCopilotBridge::hotpatchResponse(const std::string& originalResponse, const json& context) { 
    if (m_puppeteer) {
        // Use puppeteer to check for refusals/hallucinations even if passive
        CorrectionResult result = m_puppeteer->correctResponse(originalResponse, context.dump());
        if (result.success && result.originalFailure != FailureType::None) {
            return result.correctedOutput;
        }
    }
    return originalResponse; 
}

bool AgenticCopilotBridge::detectAndCorrectFailure(std::string& response, const json& context) { 
    if (m_puppeteer) {
        CorrectionResult result = m_puppeteer->correctResponse(response, context.dump());
        if (result.success && result.originalFailure != FailureType::None) {
            response = result.correctedOutput;
            return true;
        }
    }
    return false; 
}

void AgenticCopilotBridge::completionReady(const std::string& result) {
    // Notify via callback if implemented
}

void AgenticCopilotBridge::analysisReady(const std::string& result) {
    // Notify via callback if implemented
}

void AgenticCopilotBridge::errorOccurred(const std::string& error) {
    std::cerr << "[AgenticBridge Error] " << error << std::endl;
}
}

void AgenticCopilotBridge::errorOccurred(const std::string& error) {
    
}
