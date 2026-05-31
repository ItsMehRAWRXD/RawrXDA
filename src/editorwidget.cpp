// Editor Widget - Enhanced Editor Implementation
// Manages editor state, inline completions, and diagnostics

#include "editorwidget.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <mutex>

namespace EditorWidget {

class EditorWidgetImpl : public IEditorWidget {
public:
    EditorWidgetImpl() = default;
    
    std::string getContent() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_content;
    }
    
    void setContent(const std::string& content) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_content = content;
        rebuildLineIndex();
    }
    
    void applyEdits(const std::vector<TextEdit>& edits) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Apply edits in reverse order to preserve positions
        auto sortedEdits = edits;
        std::sort(sortedEdits.begin(), sortedEdits.end(), 
            [](const TextEdit& a, const TextEdit& b) {
                if (a.range.start.line != b.range.start.line)
                    return a.range.start.line > b.range.start.line;
                return a.range.start.column > b.range.start.column;
            });
        
        for (const auto& edit : sortedEdits) {
            size_t startOffset = positionToOffset(edit.range.start);
            size_t endOffset = positionToOffset(edit.range.end);
            
            if (startOffset <= m_content.size() && endOffset <= m_content.size()) {
                m_content.replace(startOffset, endOffset - startOffset, edit.newText);
            }
        }
        
        rebuildLineIndex();
        std::cout << "[EditorWidget] Applied " << edits.size() << " edits" << std::endl;
    }
    
    Position getCursorPosition() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_cursor;
    }
    
    void setCursorPosition(Position pos) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cursor = pos;
    }
    
    std::string getSelectedText() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_selectionStart.line == m_selectionEnd.line && 
            m_selectionStart.column == m_selectionEnd.column) {
            return "";
        }
        size_t startOff = positionToOffset(m_selectionStart);
        size_t endOff = positionToOffset(m_selectionEnd);
        if (startOff < endOff && endOff <= m_content.size()) {
            return m_content.substr(startOff, endOff - startOff);
        }
        return "";
    }
    
    void showInlineCompletion(const InlineCompletion& completion) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentCompletion = completion;
        m_hasCompletion = true;
        std::cout << "[EditorWidget] Showing inline completion (" 
                  << completion.confidence << " confidence)" << std::endl;
    }
    
    void dismissInlineCompletion() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_hasCompletion = false;
    }
    
    void setDiagnostics(const std::vector<Diagnostic>& diagnostics) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_diagnostics = diagnostics;
    }
    
    std::vector<Diagnostic> getDiagnostics() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_diagnostics;
    }
    
private:
    void rebuildLineIndex() {
        m_lineOffsets.clear();
        m_lineOffsets.push_back(0);
        for (size_t i = 0; i < m_content.size(); ++i) {
            if (m_content[i] == '\n') {
                m_lineOffsets.push_back(i + 1);
            }
        }
    }
    
    size_t positionToOffset(Position pos) const {
        if (pos.line < 0 || pos.line >= static_cast<int>(m_lineOffsets.size())) {
            return m_content.size();
        }
        return m_lineOffsets[pos.line] + pos.column;
    }
    
    std::string m_content;
    std::vector<size_t> m_lineOffsets;
    Position m_cursor;
    Position m_selectionStart;
    Position m_selectionEnd;
    InlineCompletion m_currentCompletion;
    bool m_hasCompletion = false;
    std::vector<Diagnostic> m_diagnostics;
    mutable std::mutex m_mutex;
};

} // namespace EditorWidget

