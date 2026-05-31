#pragma once
/**
 * features_chat.hpp - Features 29-35: Chat & Q&A
 *
 * 29. Inline Chat (Cmd+K)
 * 30. Sidebar Chat
 * 31. Explain Code
 * 32. @ File References
 * 33. @docs External Documentation
 * 34. @web Real-Time Web Search
 * 35. Image Understanding
 */

#include "ai_ide_features.hpp"

namespace rawrxd {

//=============================================================================
// FEATURE 29: Inline Chat (Cmd+K)
//=============================================================================

class Feature_InlineChat {
public:
    void startSession(const std::string& initialQuery, const std::string& selectedCode) {
        selectedCode_ = selectedCode;
        history_.clear();
        history_.push_back({"user", initialQuery});
    }
    void sendMessage(const std::string& message) {
        history_.push_back({"user", message});
    }
    void acceptSuggestion() {
        if (editor_ && !currentSuggestion_.empty()) {
            editor_->insertAtCursor(currentSuggestion_);
        }
    }
    void dismiss() {
        currentSuggestion_.clear();
        history_.clear();
    }

    std::string getCurrentSuggestion() { return currentSuggestion_; }
    bool hasSuggestion() { return !currentSuggestion_.empty(); }

    void setEditor(EditorIntegration* editor) { editor_ = editor; }
    void setProvider(std::shared_ptr<AIProvider> provider) { provider_ = provider; }

private:
    EditorIntegration* editor_ = nullptr;
    std::shared_ptr<AIProvider> provider_;
    std::string currentSuggestion_;
    std::vector<AIProvider::Message> history_;
    std::string selectedCode_;
};

//=============================================================================
// FEATURE 30: Sidebar Chat
//=============================================================================

class Feature_SidebarChat {
public:
    void startConversation();
    void sendMessage(const std::string& message);
    std::vector<AIProvider::Message> getHistory();
    void clearHistory();
    void exportConversation(const std::string& path);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
    std::vector<AIProvider::Message> history_;
};

//=============================================================================
// FEATURE 31: Explain Code
//=============================================================================

class Feature_ExplainCode {
public:
    std::string explain(const std::string& code);
    std::string explainLine(const std::string& code, uint32_t line);
    std::string explainSymbol(const std::string& symbol, const std::string& context);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
};

//=============================================================================
// FEATURE 32: @ File References
//=============================================================================

class Feature_FileReferences {
public:
    void addFile(const std::string& path);
    void addFiles(const std::vector<std::string>& paths);
    std::string buildContext();
    std::string queryWithContext(const std::string& query);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
    std::vector<std::string> referencedFiles_;
};

//=============================================================================
// FEATURE 33: @docs External Documentation
//=============================================================================

class Feature_ExternalDocs {
public:
    void fetch(const std::string& url);
    void index(const std::string& library);
    std::string query(const std::string& question);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
    std::unordered_map<std::string, std::string> cachedDocs_;
};

//=============================================================================
// FEATURE 34: @web Real-Time Web Search
//=============================================================================

class Feature_WebSearch {
public:
    struct WebResult {
        std::string url;
        std::string title;
        std::string snippet;
        float relevance = 0.0f;
    };

    std::vector<WebResult> search(const std::string& query, uint32_t k = 5);
    std::string summarizeForQuery(const std::string& query);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
};

//=============================================================================
// FEATURE 35: Image Understanding
//=============================================================================

class Feature_ImageUnderstanding {
public:
    std::string analyzeImage(const std::string& imagePath);
    std::string describeDiagram(const std::string& imagePath);
    std::string extractCode(const std::string& imagePath);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
};

} // namespace rawrxd
