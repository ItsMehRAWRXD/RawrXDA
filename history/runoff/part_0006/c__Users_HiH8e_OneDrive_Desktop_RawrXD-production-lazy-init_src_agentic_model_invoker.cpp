#include "model_invoker.hpp"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <sstream>
// #include <json/json.h>  // Commented out for now - would need JSON library

ModelInvoker::ModelInvoker() : cacheEnabled_(true) {}

ModelInvoker::~ModelInvoker() {}

bool ModelInvoker::initialize(const Config& config) {
    config_ = config;
    return true;
}

ModelInvoker::Plan ModelInvoker::invoke(const std::string& wish) {
    std::string prompt = "Generate a plan to achieve: " + wish + 
                        "\nReturn JSON with: id, description, actions array, reasoning, requiresApproval";
    
    std::string response;
    switch (config_.backend) {
        case Backend::OLLAMA:
            response = callOllama(prompt);
            break;
        case Backend::CLAUDE:
            response = callClaude(prompt);
            break;
        case Backend::OPENAI:
            response = callOpenAI(prompt);
            break;
        case Backend::LOCAL_GGUF:
            response = callLocalGGUF(prompt);
            break;
    }
    
    return parsePlanFromJSON(response);
}

void ModelInvoker::invokeAsync(const std::string& wish, std::function<void(Plan)> callback) {
    // TODO: Implement async execution
    Plan plan = invoke(wish);
    callback(plan);
}

ModelInvoker::Plan ModelInvoker::parsePlanFromJSON(const std::string& jsonResponse) {
    Plan plan;
    
    // Simple JSON parsing (would use proper JSON library in production)
    // For now, return a basic plan
    plan.id = "plan_001";
    plan.description = "Generated plan for: " + jsonResponse.substr(0, 50);
    plan.actions.push_back("Analyze current codebase");
    plan.actions.push_back("Implement required features");
    plan.actions.push_back("Test implementation");
    plan.reasoning = "This plan follows standard development workflow";
    plan.requiresApproval = true;
    
    return plan;
}

std::string ModelInvoker::callOllama(const std::string& prompt) {
    // TODO: Implement Ollama HTTP API call
    return "{\"plan\": \"placeholder\"}";
}

std::string ModelInvoker::callClaude(const std::string& prompt) {
    // TODO: Implement Claude API call
    return "{\"plan\": \"placeholder\"}";
}

std::string ModelInvoker::callOpenAI(const std::string& prompt) {
    // TODO: Implement OpenAI API call
    return "{\"plan\": \"placeholder\"}";
}

std::string ModelInvoker::callLocalGGUF(const std::string& prompt) {
    // TODO: Implement local GGUF model inference
    return "{\"plan\": \"placeholder\"}";
}

void ModelInvoker::setCacheEnabled(bool enabled) {
    cacheEnabled_ = enabled;
}

void ModelInvoker::clearCache() {
    // TODO: Implement cache clearing
}