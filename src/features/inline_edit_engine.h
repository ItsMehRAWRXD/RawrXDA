#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <mutex>

struct InlineEdit {
    int id;
    std::string originalText;
    std::string newText;
    int startPosition;
    int endPosition;
    bool accepted;
    std::chrono::system_clock::time_point timestamp;
};

class InlineEditEngine {
public:
    InlineEditEngine();
    ~InlineEditEngine();
    
    bool initialize(const std::string& modelPath);
    
    // Generate inline edit from instruction
    std::string generateEdit(const std::string& code, 
                            int cursorPos,
                            const std::string& instruction);
    
    // Create edit object
    InlineEdit createEdit(const std::string& originalCode,
                         const std::string& newCode,
                         int startPos,
                         int endPos);
    
    // Apply edit to code
    void applyEdit(std::string& code, const InlineEdit& edit);
    
    // Accept/reject edits
    void acceptEdit(int editId);
    void rejectEdit(int editId);
    
    // Query active edits
    std::vector<InlineEdit> getActiveEdits() const;
    void clearEdits();
    
private:
    class Impl;
    Impl* m_impl;
};
