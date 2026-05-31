// ============================================================================
// model_invoker_link_stub.cpp — Functional fallback for ModelInvoker::invoke
// ============================================================================
#include "agent/model_invoker.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Minimal tokenizer for fallback local inference
static std::vector<std::string> simpleTokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
        } else if (std::ispunct(static_cast<unsigned char>(c))) {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
            tokens.push_back(std::string(1, c));
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Simple keyword-based plan generation for when no backend is available
static std::string generateFallbackPlan(const std::string& wish,
                                         const std::vector<std::string>& tools) {
    std::ostringstream plan;
    plan << "{\n";
    plan << "  \"steps\": [\n";

    std::vector<std::string> steps;
    steps.push_back("Analyze the request: \"" + wish + "\"");

    // Detect intent from keywords
    std::string lowerWish = wish;
    for (char& c : lowerWish) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lowerWish.find("fix") != std::string::npos || lowerWish.find("repair") != std::string::npos) {
        steps.push_back("Identify errors or issues in the codebase");
        steps.push_back("Apply targeted fixes to resolve the problems");
        steps.push_back("Verify fixes compile and pass basic checks");
    } else if (lowerWish.find("implement") != std::string::npos || lowerWish.find("add") != std::string::npos) {
        steps.push_back("Design the new feature or function");
        steps.push_back("Implement the feature with proper error handling");
        steps.push_back("Add unit tests for the new functionality");
    } else if (lowerWish.find("refactor") != std::string::npos || lowerWish.find("clean") != std::string::npos) {
        steps.push_back("Analyze current code structure");
        steps.push_back("Apply refactoring patterns for clarity");
        steps.push_back("Verify behavior is preserved after changes");
    } else if (lowerWish.find("test") != std::string::npos) {
        steps.push_back("Identify test coverage gaps");
        steps.push_back("Write comprehensive unit tests");
        steps.push_back("Run tests and fix any failures");
    } else if (lowerWish.find("document") != std::string::npos || lowerWish.find("doc") != std::string::npos) {
        steps.push_back("Analyze code for documentation gaps");
        steps.push_back("Write clear API and usage documentation");
        steps.push_back("Update README and inline comments");
    } else if (lowerWish.find("optimize") != std::string::npos || lowerWish.find("performance") != std::string::npos) {
        steps.push_back("Profile current performance bottlenecks");
        steps.push_back("Apply optimization strategies");
        steps.push_back("Benchmark and verify improvements");
    } else {
        steps.push_back("Analyze the current codebase context");
        steps.push_back("Determine the best approach");
        steps.push_back("Implement the solution with tests");
    }

    steps.push_back("Run final validation and report results");

    for (size_t i = 0; i < steps.size(); ++i) {
        plan << "    {\"step\":" << (i + 1) << ",\"action\":\"" << steps[i] << "\"}";
        if (i + 1 < steps.size()) plan << ",";
        plan << "\n";
    }

    plan << "  ],\n";
    plan << "  \"tools_used\": [";
    for (size_t i = 0; i < tools.size() && i < 3; ++i) {
        plan << "\"" << tools[i] << "\"";
        if (i + 1 < tools.size() && i + 1 < 3) plan << ",";
    }
    plan << "],\n";
    plan << "  \"confidence\": 0.75,\n";
    plan << "  \"fallback\": true\n";
    plan << "}";

    return plan.str();
}

LLMResponse ModelInvoker::invoke(const InvocationParams& params) {
    LLMResponse res;
    res.success = true;
    res.tokensUsed = static_cast<int>(simpleTokenize(params.wish).size());
    res.reasoning = "Fallback local inference: no LLM backend configured. Using keyword-based plan generation.";

    std::string planJson = generateFallbackPlan(params.wish, params.availableTools);
    res.rawOutput = planJson;

    try {
        res.parsedPlan = nlohmann::json::parse(planJson);
    } catch (...) {
        res.parsedPlan = nlohmann::json::object();
        res.parsedPlan["steps"] = nlohmann::json::array();
        res.parsedPlan["fallback"] = true;
    }

    fprintf(stderr, "[INFO] [ModelInvoker] Fallback plan generated for: %s\n", params.wish.c_str());
    return res;
}
