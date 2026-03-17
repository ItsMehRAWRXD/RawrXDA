// Editor Widget - Enhanced Editor Interface
// Provides editor enhancement integration for all IDE variants

#ifndef EDITORWIDGET_H_
#define EDITORWIDGET_H_

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace EditorWidget {

// Cursor position in the editor
struct Position {
    int line = 0;
    int column = 0;
};

// Text range
struct Range {
    Position start;
    Position end;
};

// Editor edit operation
struct TextEdit {
    Range range;
    std::string newText;
};

// Diagnostic (error/warning)
struct Diagnostic {
    enum class Severity { Error, Warning, Info, Hint };
    
    Range range;
    std::string message;
    Severity severity = Severity::Error;
    std::string source;  // e.g., "clang", "typescript"
    int code = 0;
};

// Inline completion ghost text
struct InlineCompletion {
    std::string text;
    Position position;
    float confidence = 0.0f;
};

// Editor widget interface for AI-enhanced editing
class IEditorWidget {
public:
    virtual ~IEditorWidget() = default;
    
    virtual std::string getContent() const = 0;
    virtual void setContent(const std::string& content) = 0;
    virtual void applyEdits(const std::vector<TextEdit>& edits) = 0;
    
    virtual Position getCursorPosition() const = 0;
    virtual void setCursorPosition(Position pos) = 0;
    virtual std::string getSelectedText() const = 0;
    
    virtual void showInlineCompletion(const InlineCompletion& completion) = 0;
    virtual void dismissInlineCompletion() = 0;
    
    virtual void setDiagnostics(const std::vector<Diagnostic>& diagnostics) = 0;
    virtual std::vector<Diagnostic> getDiagnostics() const = 0;
};

// Factory function to create the default editor widget implementation
std::unique_ptr<IEditorWidget> createEditorWidget();

} // namespace EditorWidget

#endif // EDITORWIDGET_H_
