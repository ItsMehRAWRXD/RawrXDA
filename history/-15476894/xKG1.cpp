#include "ai_completion_provider_real.hpp"
#include "../cpu_inference_engine.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <regex>
#include <filesystem>
#include <fstream>

AICompletionProvider::AICompletionProvider() {
    // Initialize language keywords
    registerLanguage("cpp", {
        "if", "else", "for", "while", "do", "switch", "case", "default",
        "return", "break", "continue", "void", "int", "float", "double",
        "char", "bool", "true", "false", "nullptr", "auto", "class", "struct",
        "union", "enum", "namespace", "using", "template", "typename"
    }, {
        "std", "cout", "cin", "endl", "vector", "map", "set", "string",
        "array", "queue", "stack", "mutex", "thread", "atomic"
    });

    registerLanguage("python", {
        "if", "else", "elif", "for", "while", "def", "class", "return",
        "import", "from", "as", "try", "except", "finally", "with",
        "lambda", "yield", "break", "continue", "pass", "True", "False", "None"
    }, {
        "print", "len", "range", "str", "int", "float", "list", "dict",
        "set", "tuple", "open", "file", "json", "os", "sys", "re"
    });

    registerLanguage("javascript", {
        "if", "else", "for", "while", "do", "switch", "case", "function",
        "const", "let", "var", "return", "async", "await", "try", "catch",
        "finally", "class", "extends", "import", "export", "new", "this"
    }, {
        "console", "log", "alert", "setTimeout", "setInterval", "Promise",
        "Array", "Object", "String", "Number", "JSON", "Math", "Date"
    });
}

AICompletionProvider::~AICompletionProvider() {
    if (m_streaming) {
        cancelStreaming();
    }
    void* model_ptr = m_modelHandle;
    if (m_modelHandle) {
        auto* engine = static_cast<CPUInference::CPUInferenceEngine*>(m_modelHandle);
        delete engine;
    }
    if (m_tokenizerHandle && m_tokenizerHandle != model_ptr) {
        auto* tok = static_cast<CPUInference::CPUInferenceEngine*>(m_tokenizerHandle);
        delete tok;
    }
    m_modelHandle = nullptr;
    m_tokenizerHandle = nullptr;
}

bool AICompletionProvider::initialize(const std::string& modelPath, const std::string& tokenizerPath) {
    if (!loadModel(modelPath)) {
        std::cerr << "[AICompletion] Failed to load model: " << modelPath << std::endl;
        return false;
    }

    if (!loadTokenizer(tokenizerPath)) {
        std::cerr << "[AICompletion] Failed to load tokenizer: " << tokenizerPath << std::endl;
        return false;
    }

    m_initialized = true;
    return true;
}

bool AICompletionProvider::loadModel(const std::string& modelPath) {
    std::cout << "[AICompletion] Loading model: " << modelPath << std::endl;
    auto* engine = new CPUInference::CPUInferenceEngine();
    if (!engine->LoadModel(modelPath)) {
        delete engine;
        return false;
    }
    m_modelHandle = engine;
    if (!m_tokenizerHandle) {
        m_tokenizerHandle = engine;
    }
    return true;
}

bool AICompletionProvider::loadTokenizer(const std::string& tokenizerPath) {
    std::cout << "[AICompletion] Loading tokenizer: " << tokenizerPath << std::endl;
    if (m_modelHandle) {
        m_tokenizerHandle = m_modelHandle;
        return true;
    }
    if (!tokenizerPath.empty()) {
        auto* engine = new CPUInference::CPUInferenceEngine();
        if (engine->LoadModel(tokenizerPath)) {
            m_tokenizerHandle = engine;
            return true;
        }
        delete engine;
    }
    return false;
}

std::vector<AICompletionProvider::CompletionSuggestion> AICompletionProvider::getCompletions(
    const CompletionContext& context,
    const InferenceParams& params
) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalRequests++;
    }

    // Check cache
    if (m_cacheEnabled) {
        std::vector<CompletionSuggestion> cached;
        if (getCachedCompletion(context, cached)) {
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_stats.cacheHits++;
            }
            return cached;
        }
    }

    // Build prompt from context
    std::string prompt = buildContextPrompt(context);

    // Perform inference
    std::vector<CompletionSuggestion> suggestions = performInference(prompt, params);

    // Score suggestions
    for (auto& suggestion : suggestions) {
        suggestion.confidence = calculateConfidence(suggestion.text, context);
        suggestion.priority = calculatePriority(suggestion.text, context);
    }

    // Filter by confidence threshold
    auto it = std::remove_if(suggestions.begin(), suggestions.end(),
        [this](const CompletionSuggestion& s) { return s.confidence < m_confidenceThreshold; }
    );
    suggestions.erase(it, suggestions.end());

    // Sort by priority and confidence
    std::sort(suggestions.begin(), suggestions.end(),
        [](const CompletionSuggestion& a, const CompletionSuggestion& b) {
            if (a.priority != b.priority) return a.priority > b.priority;
            return a.confidence > b.confidence;
        }
    );

    // Limit results
    if (suggestions.size() > static_cast<size_t>(m_maxSuggestions)) {
        suggestions.resize(m_maxSuggestions);
    }

    // Cache results
    if (m_cacheEnabled) {
        CachedCompletion cached;
        cached.context = context;
        cached.suggestions = suggestions;
        cached.timestamp = std::chrono::system_clock::now();
        m_completionCache[generateCacheKey(context)] = cached;
    }

    // Update statistics
    {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.successfulResponses++;
        
        float avgTime = m_stats.avgResponseTime;
        m_stats.avgResponseTime = (avgTime * (m_stats.successfulResponses - 1) + duration.count()) 
                                  / m_stats.successfulResponses;
        
        if (!suggestions.empty()) {
            float avgConf = m_stats.avgConfidence;
            float totalConf = 0.0f;
            for (const auto& s : suggestions) {
                totalConf += s.confidence;
            }
            m_stats.avgConfidence = (avgConf * (m_stats.successfulResponses - 1) + totalConf / suggestions.size())
                                   / m_stats.successfulResponses;
        }
    }

    return suggestions;
}

void AICompletionProvider::startStreamingCompletion(
    const CompletionContext& context,
    std::function<void(const CompletionSuggestion&)> onSuggestion,
    std::function<void(const std::string&)> onError,
    const InferenceParams& params
) {
    if (m_streaming) {
        onError("Streaming already in progress");
        return;
    }

    m_streaming = true;
    m_streamingThread = std::thread([this, context, onSuggestion, onError, params]() {
        try {
            auto suggestions = getCompletions(context, params);
            for (const auto& suggestion : suggestions) {
                if (!m_streaming) break;
                onSuggestion(suggestion);
            }
        } catch (const std::exception& e) {
            onError(std::string("Streaming error: ") + e.what());
        }
        m_streaming = false;
    });

    if (m_streamingThread.joinable()) {
        m_streamingThread.detach();
    }
}

void AICompletionProvider::cancelStreaming() {
    m_streaming = false;
    if (m_streamingThread.joinable()) {
        m_streamingThread.join();
    }
}

void AICompletionProvider::registerLanguage(
    const std::string& language,
    const std::vector<std::string>& keywords,
    const std::vector<std::string>& builtins
) {
    m_keywords[language] = keywords;
    m_builtins[language] = builtins;
}

void AICompletionProvider::addCustomWords(const std::vector<std::string>& words) {
    m_customWords.insert(m_customWords.end(), words.begin(), words.end());
}

void AICompletionProvider::removeCustomWords(const std::vector<std::string>& words) {
    for (const auto& word : words) {
        auto it = std::find(m_customWords.begin(), m_customWords.end(), word);
        if (it != m_customWords.end()) {
            m_customWords.erase(it);
        }
    }
}

std::vector<std::string> AICompletionProvider::getCustomWords() const {
    return m_customWords;
}

void AICompletionProvider::setProjectContext(const std::string& projectRoot) {
    m_projectRoot = projectRoot;
    m_fileContexts.clear();
    std::error_code ec;
    std::filesystem::path root(projectRoot);
    if (!root.empty() && std::filesystem::exists(root, ec)) {
        for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
             it != std::filesystem::recursive_directory_iterator(); ++it) {
            if (ec) break;
            if (!it->is_regular_file()) continue;
            auto ext = it->path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp" || ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".cs") {
                m_fileContexts.push_back(it->path().string());
            }
        }
    }
}

void AICompletionProvider::updateFileContext(const std::string& filePath) {
    m_currentFile = filePath;
    std::ifstream file(filePath);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        std::string includeToken = "#include \"";
        auto pos = line.find(includeToken);
        if (pos != std::string::npos) {
            auto start = pos + includeToken.size();
            auto end = line.find('"', start);
            if (end != std::string::npos) {
                std::string header = line.substr(start, end - start);
                std::filesystem::path dep = std::filesystem::path(filePath).parent_path() / header;
                if (std::filesystem::exists(dep)) {
                    m_fileContexts.push_back(dep.string());
                }
            }
        }
    }
}

std::vector<std::string> AICompletionProvider::getRelevantFiles(int maxFiles) const {
    if (m_currentFile.empty() || m_fileContexts.empty()) {
        return std::vector<std::string>(m_fileContexts.begin(),
                                        m_fileContexts.begin() + std::min<int>(maxFiles, static_cast<int>(m_fileContexts.size())));
    }

    std::vector<std::pair<int, std::string>> scored;
    for (const auto& path : m_fileContexts) {
        int score = 0;
        if (path == m_currentFile) score += 100;
        std::filesystem::path p(path);
        std::filesystem::path cur(m_currentFile);
        if (p.parent_path() == cur.parent_path()) score += 50;
        if (p.extension() == cur.extension()) score += 10;
        scored.push_back({score, path});
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    std::vector<std::string> result;
    for (int i = 0; i < static_cast<int>(scored.size()) && i < maxFiles; ++i) {
        result.push_back(scored[i].second);
    }
    return result;
}

void AICompletionProvider::clearCache() {
    std::lock_guard<std::mutex> lock(m_streamingMutex);
    m_completionCache.clear();
}

AICompletionProvider::CompletionStats AICompletionProvider::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void AICompletionProvider::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = CompletionStats();
}

std::vector<AICompletionProvider::CompletionSuggestion> AICompletionProvider::performInference(
    const std::string& prompt,
    const InferenceParams& params
) {
    std::vector<CompletionSuggestion> suggestions;

    auto* engine = static_cast<CPUInference::CPUInferenceEngine*>(m_modelHandle);
    std::string generated;
    if (engine && engine->IsModelLoaded()) {
        std::vector<int32_t> tokens = engine->Tokenize(prompt);
        engine->GenerateStreaming(
            tokens,
            params.maxNewTokens,
            [&generated](const std::string& token) { generated += token; },
            []() {},
            nullptr
        );
    }
    
    // Extract partial word from prompt
    size_t lastSpace = prompt.find_last_of(" \t\n(");
    std::string partial = (lastSpace != std::string::npos) ? prompt.substr(lastSpace + 1) : prompt;

    if (!generated.empty()) {
        std::istringstream iss(generated);
        std::string line;
        int count = 0;
        while (std::getline(iss, line) && count < m_maxSuggestions) {
            if (line.empty()) continue;
            CompletionSuggestion suggestion;
            suggestion.text = line;
            suggestion.description = "Model inference";
            suggestion.type = "inference";
            suggestion.confidence = 0.7f;
            suggestion.priority = 20;
            suggestions.push_back(suggestion);
            count++;
        }
    }

    if (suggestions.empty()) {
        for (const auto& [lang, keywords] : m_keywords) {
            for (const auto& keyword : keywords) {
                if (keyword.find(partial) == 0) {
                    CompletionSuggestion suggestion;
                    suggestion.text = keyword;
                    suggestion.description = "Language keyword";
                    suggestion.type = "keyword";
                    suggestion.confidence = 0.8f;
                    suggestion.priority = 10;
                    suggestions.push_back(suggestion);
                }
            }
        }

        for (const auto& word : m_customWords) {
            if (word.find(partial) == 0) {
                CompletionSuggestion suggestion;
                suggestion.text = word;
                suggestion.description = "Custom word";
                suggestion.type = "variable";
                suggestion.confidence = 0.6f;
                suggestion.priority = 5;
                suggestions.push_back(suggestion);
            }
        }
    }

    return suggestions;
}

std::string AICompletionProvider::buildContextPrompt(const CompletionContext& ctx) {
    std::string prompt;
    
    // Include previous lines for context
    if (ctx.totalLines > 1) {
        for (size_t i = 0; i < ctx.allLines.size() && i < 10; ++i) {
            prompt += ctx.allLines[i] + "\n";
        }
    }

    // Add current line up to cursor
    prompt += ctx.currentLine.substr(0, ctx.cursorPosition);
    
    return prompt;
}

int AICompletionProvider::estimateTokenCount(const std::string& text) const {
    // Rough estimation: ~4 characters per token on average
    return static_cast<int>(text.length() / 4.0f);
}

std::vector<int> AICompletionProvider::tokenizeText(const std::string& text) {
    auto* engine = static_cast<CPUInference::CPUInferenceEngine*>(m_tokenizerHandle);
    if (engine && engine->IsModelLoaded()) {
        std::vector<int32_t> t = engine->Tokenize(text);
        return std::vector<int>(t.begin(), t.end());
    }
    std::vector<int> tokens;
    tokens.reserve(text.size());
    for (unsigned char c : text) {
        tokens.push_back(static_cast<int>(c));
    }
    return tokens;
}

std::string AICompletionProvider::detokenize(const std::vector<int>& tokens) {
    auto* engine = static_cast<CPUInference::CPUInferenceEngine*>(m_tokenizerHandle);
    if (engine && engine->IsModelLoaded()) {
        std::vector<int32_t> t(tokens.begin(), tokens.end());
        return engine->Detokenize(t);
    }
    std::string result;
    for (int token : tokens) {
        if (token >= 0 && token < 256) {
            result += static_cast<char>(token);
        }
    }
    return result;
}

float AICompletionProvider::calculateConfidence(const std::string& suggestion, const CompletionContext& ctx) {
    float confidence = 0.5f;

    // Check if suggestion matches context language
    std::string language = detectLanguage(ctx.currentFile);
    
    // Keywords should have higher confidence
    auto keywords = getKeywords(language);
    if (std::find(keywords.begin(), keywords.end(), suggestion) != keywords.end()) {
        confidence += 0.2f;
    }

    // Builtins get boost too
    auto builtins = getBuiltins(language);
    if (std::find(builtins.begin(), builtins.end(), suggestion) != builtins.end()) {
        confidence += 0.15f;
    }

    // Custom words
    if (std::find(m_customWords.begin(), m_customWords.end(), suggestion) != m_customWords.end()) {
        confidence += 0.1f;
    }

    return std::min(1.0f, confidence);
}

int AICompletionProvider::calculatePriority(const std::string& suggestion, const CompletionContext& ctx) {
    int priority = 0;

    std::string language = detectLanguage(ctx.currentFile);
    
    // Keywords have highest priority
    auto keywords = getKeywords(language);
    if (std::find(keywords.begin(), keywords.end(), suggestion) != keywords.end()) {
        priority += 20;
    }

    // Builtins next
    auto builtins = getBuiltins(language);
    if (std::find(builtins.begin(), builtins.end(), suggestion) != builtins.end()) {
        priority += 15;
    }

    // Length preference (shorter = more likely to be used)
    if (suggestion.length() < 5) {
        priority += 5;
    }

    return priority;
}

std::vector<std::string> AICompletionProvider::getKeywords(const std::string& language) const {
    auto it = m_keywords.find(language);
    return (it != m_keywords.end()) ? it->second : std::vector<std::string>();
}

std::vector<std::string> AICompletionProvider::getBuiltins(const std::string& language) const {
    auto it = m_builtins.find(language);
    return (it != m_builtins.end()) ? it->second : std::vector<std::string>();
}

std::string AICompletionProvider::detectLanguage(const std::string& filePath) const {
    if (filePath.find(".cpp") != std::string::npos || filePath.find(".cc") != std::string::npos) {
        return "cpp";
    } else if (filePath.find(".py") != std::string::npos) {
        return "python";
    } else if (filePath.find(".js") != std::string::npos) {
        return "javascript";
    } else if (filePath.find(".ts") != std::string::npos) {
        return "typescript";
    }
    return "cpp";  // Default
}

std::string AICompletionProvider::generateCacheKey(const CompletionContext& ctx) const {
    return ctx.currentFile + ":" + ctx.currentLine + ":" + std::to_string(ctx.cursorPosition);
}

bool AICompletionProvider::getCachedCompletion(
    const CompletionContext& ctx,
    std::vector<CompletionSuggestion>& out
) {
    auto it = m_completionCache.find(generateCacheKey(ctx));
    if (it != m_completionCache.end()) {
        // Check if cache is still fresh (5 minutes)
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.timestamp).count();
        
        if (age < 300) {
            out = it->second.suggestions;
            return true;
        }
    }
    return false;
}
