#pragma once

#include <vector>
#include <memory>
#include <string>
#include <functional>

class NativeUndoCommand {
public:
    explicit NativeUndoCommand(const std::string& name = "") : m_name(name) {}
    virtual ~NativeUndoCommand() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual std::string getName() const { return m_name; }
    
protected:
    std::string m_name;
};

// Template command for simple undo/redo with lambdas
class NativeLambdaCommand : public NativeUndoCommand {
public:
    NativeLambdaCommand(const std::string& name,
                       std::function<void()> undoFunc,
                       std::function<void()> redoFunc)
        : NativeUndoCommand(name), m_undoFunc(undoFunc), m_redoFunc(redoFunc) {}
    
    void undo() override {
        if (m_undoFunc) m_undoFunc();
    }
    
    void redo() override {
        if (m_redoFunc) m_redoFunc();
    }
    
private:
    std::function<void()> m_undoFunc;
    std::function<void()> m_redoFunc;
};

// Transaction for grouping multiple commands
class NativeUndoTransaction : public NativeUndoCommand {
public:
    explicit NativeUndoTransaction(const std::string& name = "Transaction")
        : NativeUndoCommand(name) {}
    
    void addCommand(std::unique_ptr<NativeUndoCommand> cmd) {
        m_commands.push_back(std::move(cmd));
    }
    
    void undo() override {
        // Undo in reverse order
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
            (*it)->undo();
        }
    }
    
    void redo() override {
        // Redo in forward order
        for (auto& cmd : m_commands) {
            cmd->redo();
        }
    }
    
    size_t getCommandCount() const { return m_commands.size(); }
    bool isEmpty() const { return m_commands.empty(); }
    
private:
    std::vector<std::unique_ptr<NativeUndoCommand>> m_commands;
};

class NativeUndoStack {
public:
    NativeUndoStack(size_t maxSize = 100) : m_maxSize(maxSize), m_currentIndex(-1) {}
    
    // Push a command and execute its redo
    void push(std::unique_ptr<NativeUndoCommand> cmd) {
        if (!cmd) return;
        
        cmd->redo();
        
        // Remove any commands after current index (when user does something after undo)
        if (m_currentIndex < (int)m_stack.size() - 1) {
            m_stack.erase(m_stack.begin() + m_currentIndex + 1, m_stack.end());
        }
        
        m_stack.push_back(std::move(cmd));
        m_currentIndex++;
        
        // Limit stack size
        if (m_stack.size() > m_maxSize) {
            m_stack.erase(m_stack.begin());
            m_currentIndex--;
        }
        
        onStackChanged();
    }
    
    void undo() {
        if (canUndo()) {
            m_stack[m_currentIndex]->undo();
            m_currentIndex--;
            onStackChanged();
        }
    }
    
    void redo() {
        if (canRedo()) {
            m_currentIndex++;
            m_stack[m_currentIndex]->redo();
            onStackChanged();
        }
    }
    
    bool canUndo() const {
        return m_currentIndex >= 0;
    }
    
    bool canRedo() const {
        return m_currentIndex < (int)m_stack.size() - 1;
    }
    
    std::string getUndoText() const {
        if (canUndo()) {
            return "Undo: " + m_stack[m_currentIndex]->getName();
        }
        return "Nothing to undo";
    }
    
    std::string getRedoText() const {
        if (canRedo()) {
            return "Redo: " + m_stack[m_currentIndex + 1]->getName();
        }
        return "Nothing to redo";
    }
    
    void clear() {
        m_stack.clear();
        m_currentIndex = -1;
        onStackChanged();
    }
    
    size_t size() const {
        return m_stack.size();
    }
    
    size_t getMaxSize() const {
        return m_maxSize;
    }
    
    void setMaxSize(size_t size) {
        m_maxSize = size;
    }
    
    // Callback when stack changes
    void setChangeCallback(std::function<void()> callback) {
        m_changeCallback = callback;
    }
    
private:
    void onStackChanged() {
        if (m_changeCallback) {
            m_changeCallback();
        }
    }
    
    std::vector<std::unique_ptr<NativeUndoCommand>> m_stack;
    int m_currentIndex;
    size_t m_maxSize;
    std::function<void()> m_changeCallback;
};