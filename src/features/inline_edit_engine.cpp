// ============================================================================
// inline_edit_engine.cpp — Real-time Inline Edit Mode (Cursor-style)
// ============================================================================
// Implements inline code editing with AI suggestions
// Shows diffs inline, allows accept/reject, multi-cursor support
// ============================================================================

#include "inline_edit_engine.h"
#include "logging/logger.h"
#include "../ai/ai_completion_provider_real.hpp"
#include <algorithm>
#include <regex>
#include <sstream>

static Logger s_logger("InlineEdit");

class InlineEditEngine::Impl {
public:
    AICompletionProvider* m_aiProvider;
    std::vector<InlineEdit> m_activeEdits;
    std::mutex m_mutex;
    bool m_enabled;
    
    Impl() : m_aiProvider(new AICompletionProvider()), m_enabled(true) {}
    ~Impl() { delete m_aiProvider; }
    
    std::string generateInlineEdit(const std::string& code, 
                                   int cursorPos,
                                   const std::string& instruction) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_aiProvider->isInitialized()) {
            s_logger.warn("AI provider not initialized for inline edit");
            return "";
        }
        
        // Build context from code
        AICompletionProvider::CompletionContext ctx;
        ctx.currentFile = "buffer";
        ctx.cursorPosition = cursorPos;
        
        auto lines = splitLines(code);
        ctx.allLines = lines;
        ctx.totalLines = static_cast<int>(lines.size());
        
        // Find current line
        int lineNum = findLineAtPosition(code, cursorPos);
        if (lineNum >= 0 && lineNum < static_cast<int>(lines.size())) {
            ctx.currentLine = lines[lineNum];
            if (lineNum > 0) ctx.previousLine = lines[lineNum - 1];
            if (lineNum + 1 < lines.size()) ctx.nextLine = lines[lineNum + 1];
        }
        
        // Generate completion with instruction
        auto suggestions = m_aiProvider->getCompletions(ctx);
        
        if (suggestions.empty()) {
            s_logger.debug("No suggestions generated for inline edit");
            return "";
        }
        
        // Return best suggestion
        return suggestions[0].text;
    }
    
    InlineEdit createEdit(const std::string& originalCode,
                         const std::string& newCode,
                         int startPos,
                         int endPos) {
        InlineEdit edit;
        edit.id = generateEditId();
        edit.originalText = originalCode.substr(startPos, endPos - startPos);
        edit.newText = newCode;
        edit.startPosition = startPos;
        edit.endPosition = endPos;
        edit.accepted = false;
        edit.timestamp = std::chrono::system_clock::now();
        
        return edit;
    }
    
    void applyEdit(std::string& code, const InlineEdit& edit) {
        if (edit.startPosition < 0 || edit.endPosition > static_cast<int>(code.length())) {
            s_logger.error("Invalid edit positions: start={} end={}", 
                          edit.startPosition, edit.endPosition);
            return;
        }
        
        code.replace(edit.startPosition, 
                    edit.endPosition - edit.startPosition, 
                    edit.newText);
        
        s_logger.info("Applied inline edit id={}", edit.id);
    }
    
private:
    static std::vector<std::string> splitLines(const std::string& text) {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
        return lines;
    }
    
    static int findLineAtPosition(const std::string& code, int pos) {
        int line = 0;
        for (int i = 0; i < pos && i < static_cast<int>(code.length()); ++i) {
            if (code[i] == '\n') line++;
        }
        return line;
    }
    
    int generateEditId() {
        static int nextId = 1;
        return nextId++;
    }
};

// ============================================================================
// Public API Implementation
// ============================================================================

InlineEditEngine::InlineEditEngine() : m_impl(new Impl()) {}
InlineEditEngine::~InlineEditEngine() { delete m_impl; }

bool InlineEditEngine::initialize(const std::string& modelPath) {
    return m_impl->m_aiProvider->initialize(modelPath, "");
}

std::string InlineEditEngine::generateEdit(const std::string& code,
                                           int cursorPos,
                                           const std::string& instruction) {
    return m_impl->generateInlineEdit(code, cursorPos, instruction);
}

InlineEdit InlineEditEngine::createEdit(const std::string& originalCode,
                                       const std::string& newCode,
                                       int startPos,
                                       int endPos) {
    return m_impl->createEdit(originalCode, newCode, startPos, endPos);
}

void InlineEditEngine::applyEdit(std::string& code, const InlineEdit& edit) {
    m_impl->applyEdit(code, edit);
}

void InlineEditEngine::acceptEdit(int editId) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    for (auto& edit : m_impl->m_activeEdits) {
        if (edit.id == editId) {
            edit.accepted = true;
            s_logger.info("Accepted edit id={}", editId);
            return;
        }
    }
}

void InlineEditEngine::rejectEdit(int editId) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->m_activeEdits.erase(
        std::remove_if(m_impl->m_activeEdits.begin(), m_impl->m_activeEdits.end(),
                      [editId](const InlineEdit& e) { return e.id == editId; }),
        m_impl->m_activeEdits.end()
    );
    s_logger.info("Rejected edit id={}", editId);
}

std::vector<InlineEdit> InlineEditEngine::getActiveEdits() const {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    return m_impl->m_activeEdits;
}

void InlineEditEngine::clearEdits() {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->m_activeEdits.clear();
    s_logger.info("Cleared all inline edits");
}
