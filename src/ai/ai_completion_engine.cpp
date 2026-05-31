/**
 * @file ai_completion_engine.cpp
 * @brief AI-powered code completion implementation
 * Batch 5 - Item 70: AI completion engine
 */

#include "ai/ai_completion_engine.h"
#include <algorithm>
#include <sstream>
#include <future>
#include <thread>

namespace RawrXD::AI {

AICompletionEngine::AICompletionEngine()
    : m_initialized(false)
    , m_maxTokens(100)
    , m_temperature(0.2f)
    , m_contextWindow(50)
    , m_autoTrigger(true)
    , m_triggerDelay(300)
    , m_cacheSize(100)
    , m_streaming(false) {
}

AICompletionEngine::~AICompletionEngine() {
    shutdown();
}

bool AICompletionEngine::initialize() {
    // Load model if path is set
    if (!m_modelPath.empty()) {
        // Would load the model here
    }
    
    m_initialized = true;
    return true;
}

void AICompletionEngine::shutdown() {
    cancelStreaming();
    m_initialized = false;
}

bool AICompletionEngine::isInitialized() const {
    return m_initialized;
}

void AICompletionEngine::setModel(const std::string& modelPath) {
    m_modelPath = modelPath;
    if (m_initialized) {
        // Reload model
    }
}

void AICompletionEngine::setMaxTokens(int maxTokens) {
    m_maxTokens = maxTokens;
}

void AICompletionEngine::setTemperature(float temperature) {
    m_temperature = std::clamp(temperature, 0.0f, 2.0f);
}

void AICompletionEngine::setContextWindow(int lines) {
    m_contextWindow = lines;
}

CompletionResult AICompletionEngine::getCompletions(const CompletionContext& context) {
    CompletionResult result;
    
    if (!m_initialized) {
        result.error = "Engine not initialized";
        return result;
    }
    
    auto start = std::chrono::steady_clock::now();
    
    // Check cache first
    std::string cacheKey = generateCacheKey(context);
    auto cacheIt = m_cache.find(cacheKey);
    if (cacheIt != m_cache.end()) {
        result = cacheIt->second;
        result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
        );
        return result;
    }
    
    // Generate completions using model
    result.items = generateCompletionItems(context);
    result.isComplete = true;
    
    auto end = std::chrono::steady_clock::now();
    result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Cache result
    if (m_cache.size() >= static_cast<size_t>(m_cacheSize)) {
        m_cache.erase(m_cache.begin());
    }
    m_cache[cacheKey] = result;
    
    return result;
}

std::future<CompletionResult> AICompletionEngine::getCompletionsAsync(const CompletionContext& context) {
    return std::async(std::launch::async, [this, context]() {
        return getCompletions(context);
    });
}

std::optional<CompletionItem> AICompletionEngine::getBestCompletion(const CompletionContext& context) {
    auto result = getCompletions(context);
    
    if (result.items.empty()) {
        return std::nullopt;
    }
    
    // Return highest confidence item
    return *std::max_element(result.items.begin(), result.items.end(),
        [](const CompletionItem& a, const CompletionItem& b) {
            return a.confidence < b.confidence;
        });
}

void AICompletionEngine::requestStreamingCompletion(const CompletionContext& context) {
    if (!m_initialized || m_streaming) {
        return;
    }
    
    m_streaming = true;
    
    // Start streaming in background thread
    std::thread([this, context]() {
        auto items = generateCompletionItems(context);
        
        for (const auto& item : items) {
            if (!m_streaming) break;
            
            // Stream each character
            for (size_t i = 0; i <= item.text.length(); i++) {
                if (!m_streaming) break;
                
                if (m_streamCallback) {
                    m_streamCallback(item.text.substr(0, i));
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        m_streaming = false;
    }).detach();
}

void AICompletionEngine::cancelStreaming() {
    m_streaming = false;
}

bool AICompletionEngine::isStreaming() const {
    return m_streaming;
}

void AICompletionEngine::updateContext(const std::string& filePath, const std::string& content) {
    m_fileContexts[filePath] = content;
}

void AICompletionEngine::clearContext() {
    m_fileContexts.clear();
}

void AICompletionEngine::setProjectContext(const std::string& context) {
    m_projectContext = context;
}

void AICompletionEngine::setAutoTrigger(bool enabled) {
    m_autoTrigger = enabled;
}

bool AICompletionEngine::isAutoTriggerEnabled() const {
    return m_autoTrigger;
}

void AICompletionEngine::setTriggerDelay(int milliseconds) {
    m_triggerDelay = milliseconds;
}

void AICompletionEngine::setTriggerCharacters(const std::vector<char>& chars) {
    m_triggerChars = chars;
}

void AICompletionEngine::clearCache() {
    m_cache.clear();
}

void AICompletionEngine::setCacheSize(int size) {
    m_cacheSize = size;
    
    // Trim cache if needed
    while (m_cache.size() > static_cast<size_t>(size)) {
        m_cache.erase(m_cache.begin());
    }
}

void AICompletionEngine::onCompletionReady(CompletionCallback callback) {
    m_completionCallback = callback;
}

void AICompletionEngine::onStreamToken(StreamCallback callback) {
    m_streamCallback = callback;
}

std::string AICompletionEngine::generateCacheKey(const CompletionContext& context) {
    return context.filePath + ":" + std::to_string(context.line) + ":" + 
           std::to_string(context.column) + ":" + context.prefix;
}

std::vector<CompletionItem> AICompletionEngine::generateCompletionItems(const CompletionContext& context) {
    std::vector<CompletionItem> items;
    
    // Build prompt from context
    std::string prompt = buildPrompt(context);
    
    // Generate completions using model
    // This is a simplified implementation
    // In reality, this would call the AI model
    
    // Example completions based on context
    if (context.language == "cpp" || context.language == "c++") {
        // C++ specific completions
        if (context.prefix.find("std::") != std::string::npos) {
            items.push_back(createItem("vector", "std::vector", "Container", 0.95f));
            items.push_back(createItem("string", "std::string", "String class", 0.90f));
            items.push_back(createItem("map", "std::map", "Associative container", 0.85f));
            items.push_back(createItem("unique_ptr", "std::unique_ptr", "Smart pointer", 0.80f));
        } else if (context.prefix.find("for") != std::string::npos) {
            items.push_back(createItem("(auto& item : container)", "range-based for", "Loop", 0.95f));
            items.push_back(createItem("(int i = 0; i < n; i++)", "classic for", "Loop", 0.85f));
        }
    } else if (context.language == "python") {
        if (context.prefix.find("def ") != std::string::npos) {
            items.push_back(createItem("(self):", "method", "Method definition", 0.90f));
            items.push_back(createItem("():", "function", "Function definition", 0.85f));
        }
    }
    
    // Generic completions
    if (items.empty()) {
        items.push_back(createItem("completion1", "Suggestion 1", "AI suggestion", 0.70f));
        items.push_back(createItem("completion2", "Suggestion 2", "AI suggestion", 0.65f));
        items.push_back(createItem("completion3", "Suggestion 3", "AI suggestion", 0.60f));
    }
    
    // Sort by confidence
    std::sort(items.begin(), items.end(),
        [](const CompletionItem& a, const CompletionItem& b) {
            return a.confidence > b.confidence;
        });
    
    return items;
}

std::string AICompletionEngine::buildPrompt(const CompletionContext& context) {
    std::stringstream prompt;
    
    // Add project context
    if (!m_projectContext.empty()) {
        prompt << "Project: " << m_projectContext << "\n";
    }
    
    // Add file context
    prompt << "File: " << context.filePath << "\n";
    prompt << "Language: " << context.language << "\n\n";
    
    // Add recent files context
    for (const auto& recentFile : context.recentFiles) {
        auto it = m_fileContexts.find(recentFile);
        if (it != m_fileContexts.end()) {
            prompt << "// " << recentFile << "\n";
            // Add first few lines
            std::istringstream stream(it->second);
            std::string line;
            int count = 0;
            while (std::getline(stream, line) && count < 10) {
                prompt << line << "\n";
                count++;
            }
            prompt << "\n";
        }
    }
    
    // Add current context
    prompt << "\n// Current file\n";
    prompt << context.prefix;
    prompt << "[COMPLETION]";
    prompt << context.suffix;
    
    return prompt.str();
}

CompletionItem AICompletionEngine::createItem(const std::string& text,
                                                 const std::string& display,
                                                 const std::string& description,
                                                 float confidence) {
    CompletionItem item;
    item.text = text;
    item.displayText = display;
    item.description = description;
    item.confidence = confidence;
    item.source = "AI";
    item.insertStart = 0;
    item.insertEnd = 0;
    return item;
}

} // namespace RawrXD::AI
