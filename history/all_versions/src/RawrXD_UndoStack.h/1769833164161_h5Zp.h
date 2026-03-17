#pragma once
#include "RawrXD_Win32_Foundation.h"
#include <vector>
#include <memory>
// Include SignalSlot mechanism
#include "RawrXD_SignalSlot.h"

namespace RawrXD {

class Editor;

struct UndoCommand {
    virtual ~UndoCommand() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual int id() const { return -1; }
    virtual bool mergeWith(const UndoCommand* other) { return false; }
};

class UndoStack {
    std::vector<std::unique_ptr<UndoCommand>> commands;
    int index = 0; // Current position (0 = empty, 1 = first command executed)
    int cleanIndex = 0; // Where the document was last saved
    bool modified = false;
    
public:
    Signal<bool> canUndoChanged;
    Signal<bool> canRedoChanged;
    Signal<bool> cleanChanged; // Modified state changed
    
    UndoStack();
    UndoStack(Editor* e) : UndoStack() {}ck() {} // Helper constructor
    void push(std::unique_ptr<UndoCommand> cmd);d::unique_ptr<UndoCommand> cmd);
    void undo();
    void redo();void redo();
    
    bool canUndo() const;
    bool canRedo() const;bool canRedo() const;
    
    void setClean();
    bool isClean() const;bool isClean() const;
    
    void clear();  void clear();
};};

} // namespace RawrXD} // namespace RawrXD

