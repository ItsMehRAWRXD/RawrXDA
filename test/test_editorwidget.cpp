// Test for Cursor Editor Widget Integration
// Validates the IEditorWidget interface implementation

#include "../include/editorwidget.h"
#include <iostream>
#include <memory>
#include <cassert>

using namespace EditorWidget;

void testBasicFunctionality() {
    // Create editor widget
    auto editor = createEditorWidget();
    assert(editor != nullptr);

    // Test setting content
    std::string testContent = "Hello World\nThis is a test\n";
    editor->setContent(testContent);
    assert(editor->getContent() == testContent);

    // Test cursor position
    Position pos = {1, 5}; // Line 1, column 5 ("This ")
    editor->setCursorPosition(pos);
    Position retrievedPos = editor->getCursorPosition();
    assert(retrievedPos.line == pos.line && retrievedPos.column == pos.column);

    // Test diagnostics
    std::vector<Diagnostic> diags = {
        {Range{{0, 0}, {0, 5}}, "Test error", Diagnostic::Severity::Error, "test", 1}
    };
    editor->setDiagnostics(diags);
    auto retrievedDiags = editor->getDiagnostics();
    assert(retrievedDiags.size() == 1);
    assert(retrievedDiags[0].message == "Test error");

    // Test inline completion
    InlineCompletion completion{"completion text", {1, 5}, 0.8f};
    editor->showInlineCompletion(completion);
    editor->dismissInlineCompletion();

    // Test text edits
    std::vector<TextEdit> edits = {
        {Range{{0, 6}, {0, 11}}, "Universe"} // Replace "World" with "Universe"
    };
    editor->applyEdits(edits);
    std::string expected = "Hello Universe\nThis is a test\n";
    assert(editor->getContent() == expected);

    std::cout << "All basic functionality tests passed!" << std::endl;
}

int main() {
    testBasicFunctionality();
    return 0;
}