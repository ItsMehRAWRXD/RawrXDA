#include "RawrXD_UndoStack.h"

namespace RawrXD {

UndoStack::UndoStack() : index(0), cleanIndex(0) {}

void UndoStack::push(std::unique_ptr<UndoCommand> cmd) {
    // If we are not at the end, remove all commands after current index
    if (index < (int)commands.size()) {
        commands.erase(commands.begin() + index, commands.end());
    }
    
    // Try to merge with previous command
    if (index > 0 && !commands.empty()) {
        if (commands.back()->mergeWith(cmd.get())) {
            // Merged, just update signal if needed? 
            // Usually modified state doesn't change relative to previous.
            return;
        }
    }
    
    // Execute logic for 'redo' is implied to have happened BEFORE pushing for typical undo stacks?
    // Or does push execute it? 
    // Usually usage is: perform action, then push command.
    // So we just store it.
    
    commands.push_back(std::move(cmd));
    index++;
    
    // Update clean index if we just blew past it (i.e. we were before clean index and diverged)
    // If we were at cleanIndex, we are now dirty.
    // If we undid to before cleanIndex, and now push new commands, the old clean path is lost.
    if (cleanIndex > index - 1) { // We were 'behind' the clean point, now we branched.
        // cleanIndex is invalid for this branch unless we handle it complexly. 
        // Simplest: just say not clean.
        cleanIndex = -1; 
    }
    
    canUndoChanged.emit(true);
    canRedoChanged.emit(false);
    
    bool wasClean = (index - 1 == cleanIndex); // This logic is slightly flawed for branching
    // Let's just check clean state
    if (isClean() != wasClean) cleanChanged.emit(isClean());
}

void UndoStack::undo() {
    if (!canUndo()) return;
    
    index--;
    commands[index]->undo();
    
    canUndoChanged.emit(canUndo());
    canRedoChanged.emit(true);
    cleanChanged.emit(isClean());
}

void UndoStack::redo() {
    if (!canRedo()) return;
    
    commands[index]->redo();
    index++;
    
    canUndoChanged.emit(true);
    canRedoChanged.emit(canRedo());
    cleanChanged.emit(isClean());
}

bool UndoStack::canUndo() const { return index > 0; }
bool UndoStack::canRedo() const { return index < (int)commands.size(); }

void UndoStack::setClean() {
    cleanIndex = index;
    cleanChanged.emit(true);
}

bool UndoStack::isClean() const {
    return index == cleanIndex;
}

void UndoStack::clear() {
    commands.clear();
    index = 0;
    cleanIndex = 0;
    canUndoChanged.emit(false);
    canRedoChanged.emit(false);
    cleanChanged.emit(true);
}

} // namespace RawrXD
