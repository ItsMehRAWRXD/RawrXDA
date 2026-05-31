#pragma once
/**
 * ai_ide_features.hpp - Master header with all 50 AI IDE features
 * 
 * Core architecture providing:
 * - FeatureRegistry: Single point of connection for all features
 * - AIProvider: Abstraction for any LLM backend
 * - EditorIntegration: Interface to editor operations
 * - TerminalInterface: Interface to terminal operations
 * - Helper types: TextEdit, CodeContext, SearchResult, TestResult
 */

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <future>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>
#include <cstring>

namespace rawrxd {

// Forward declarations
class AIProvider;
class CodebaseIndex;
class EditorIntegration;
class TerminalInterface;

//=============================================================================
// HELPER TYPES
//=============================================================================

struct TextEdit {
    size_t start = 0;
    size_t length = 0;
    std::string newText;
    std::string description;
};

struct CodeContext {
    std::string activeFile;
    std::string selection;
    std::vector<std::string> openFiles;
    size_t cursorPosition = 0;
    std::string language;
};

struct SearchResult {
    std::string filePath;
    size_t line = 0;
    size_t column = 0;
    std::string snippet;
    float score = 0.0f;
};

struct TestResult {
    bool passed = false;
    std::string testName;
    std::string error;
    std::string output;
    std::chrono::milliseconds duration{0};
};

//=============================================================================
// FEATURE REGISTRY - Single point of connection for all features
//=============================================================================

class FeatureRegistry {
public:
    struct FeatureInfo {
        std::string id;
        std::string name;
        std::string category;
        bool enabled = true;
        std::function<void()> onEnable;
        std::function<void()> onDisable;
    };

    static FeatureRegistry& instance() {
        static FeatureRegistry reg;
        return reg;
    }

    // Connect all features to AI provider
    void connectAll(std::shared_ptr<AIProvider> provider) {
        std::lock_guard<std::mutex> lock(mutex_);
        provider_ = provider;
    }

    void connectEditor(EditorIntegration* editor) {
        std::lock_guard<std::mutex> lock(mutex_);
        editor_ = editor;
    }

    void connectTerminal(TerminalInterface* terminal) {
        std::lock_guard<std::mutex> lock(mutex_);
        terminal_ = terminal;
    }

    void enableFeature(const std::string& featureId, bool enabled = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = features_.find(featureId);
        if (it != features_.end()) {
            it->second.enabled = enabled;
            if (enabled && it->second.onEnable) it->second.onEnable();
            else if (!enabled && it->second.onDisable) it->second.onDisable();
        }
    }

    bool isFeatureEnabled(const std::string& featureId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = features_.find(featureId);
        return it != features_.end() && it->second.enabled;
    }

    void registerFeature(const FeatureInfo& info) {
        std::lock_guard<std::mutex> lock(mutex_);
        features_[info.id] = info;
    }

    std::vector<FeatureInfo> getAllFeatures() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<FeatureInfo> result;
        for (const auto& kv : features_) {
            result.push_back(kv.second);
        }
        return result;
    }

    std::vector<FeatureInfo> getFeaturesByCategory(const std::string& category) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<FeatureInfo> result;
        for (const auto& kv : features_) {
            if (kv.second.category == category) {
                result.push_back(kv.second);
            }
        }
        return result;
    }

private:
    FeatureRegistry() = default;
    std::unordered_map<std::string, FeatureInfo> features_;
    mutable std::mutex mutex_;
    std::shared_ptr<AIProvider> provider_;
    EditorIntegration* editor_ = nullptr;
    TerminalInterface* terminal_ = nullptr;
};

//=============================================================================
// AI PROVIDER ABSTRACTION - Connect any LLM backend
//=============================================================================

class AIProvider {
public:
    virtual ~AIProvider() = default;

    // Core inference
    virtual std::future<std::string> complete(const std::string& prompt) = 0;
    virtual std::future<std::string> completeStream(
        const std::string& prompt,
        std::function<void(const std::string& token)> onToken) = 0;

    // Embedding for RAG
    virtual std::vector<float> embed(const std::string& text) {
        (void)text;
        return {};
    }

    // Chat with history
    struct Message { std::string role; std::string content; };
    virtual std::future<std::string> chat(
        const std::vector<Message>& history) {
        (void)history;
        std::promise<std::string> p;
        p.set_value("");
        return p.get_future();
    }

    // Model info
    virtual std::string getModelName() const { return "unknown"; }
    virtual uint32_t getContextLength() const { return 4096; }
};

//=============================================================================
// EDITOR INTEGRATION INTERFACE
//=============================================================================

class EditorIntegration {
public:
    virtual ~EditorIntegration() = default;

    // Document operations
    virtual std::string getActiveDocument() { return ""; }
    virtual std::string getSelection() { return ""; }
    virtual void replaceSelection(const std::string& text) { (void)text; }
    virtual void insertAtCursor(const std::string& text) { (void)text; }
    virtual void setGhostText(const std::string& text, size_t position) {
        (void)text; (void)position;
    }
    virtual void clearGhostText() {}

    // Cursor/position
    virtual size_t getCursorPosition() { return 0; }
    virtual void setCursorPosition(size_t position) { (void)position; }

    // File operations
    virtual std::vector<std::string> getOpenFiles() { return {}; }
    virtual std::string getFileContent(const std::string& path) {
        (void)path; return "";
    }

    // Diagnostics
    virtual std::vector<std::pair<size_t, std::string>> getDiagnostics() { return {}; }

    // Notifications
    virtual void showMessage(const std::string& message, bool isError = false) {
        (void)message; (void)isError;
    }
    virtual void showQuickPick(const std::vector<std::string>& items,
                               std::function<void(size_t)> onSelect) {
        (void)items; (void)onSelect;
    }
};

//=============================================================================
// TERMINAL INTERFACE
//=============================================================================

class TerminalInterface {
public:
    virtual ~TerminalInterface() = default;

    virtual std::future<std::string> execute(const std::string& command) {
        (void)command;
        std::promise<std::string> p;
        p.set_value("");
        return p.get_future();
    }
    virtual std::future<int> executeStream(
        const std::string& command,
        std::function<void(const std::string& output)> onOutput) {
        (void)command; (void)onOutput;
        std::promise<int> p;
        p.set_value(0);
        return p.get_future();
    }
    virtual void interrupt() {}
    virtual std::string getCurrentDirectory() { return "."; }
};

} // namespace rawrxd
