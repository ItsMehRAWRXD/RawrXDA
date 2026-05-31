#include "minimonaco.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <functional>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <streambuf>

namespace fs = std::filesystem;

// =============================================================================
// DEMO 1: Basic Editor Window
// =============================================================================
static void demo1_basic_window() {
    std::cout << "=== Demo 1: Basic Editor Window ===" << std::endl;

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!MiniMonaco::EditorWindow::Register(hInstance)) {
        std::cerr << "Failed to register editor window class" << std::endl;
        return;
    }

    // Create main window
    HWND hwnd = CreateWindowExW(
        0,
        L"MiniMonacoDemo",
        L"MiniMonaco Editor Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1024, 768,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        std::cerr << "Failed to create main window" << std::endl;
        return;
    }

    // Create editor
    MiniMonaco::Config config;
    config.fontSize = 14;
    config.fontFamily = L"Consolas";
    config.showLineNumbers = true;
    config.showMinimap = true;
    config.autoClose = true;

    MiniMonaco::Editor* editor = MiniMonaco::EditorWindow::Create(hwnd, 0, 0, 1024, 768, config);

    if (!editor) {
        std::cerr << "Failed to create editor" << std::endl;
        DestroyWindow(hwnd);
        return;
    }

    // Set some sample code
    std::wstring sampleCode = LR"(// Welcome to MiniMonaco Editor
#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
)";

    editor->setText(sampleCode);

    // Set callbacks
    editor->setChangeCallback([]() {
        std::cout << "[Editor] Content changed" << std::endl;
    });

    editor->setCursorCallback([](int line, int col) {
        std::cout << "[Editor] Cursor at line " << line << ", col " << col << std::endl;
    });

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// =============================================================================
// DEMO 2: Text Buffer Operations
// =============================================================================
static void demo2_text_buffer() {
    std::cout << "=== Demo 2: Text Buffer Operations ===" << std::endl;

    MiniMonaco::TextBuffer buffer;

    // Insert text
    buffer.insert(0, L"Hello, World!", 13);
    std::wcout << L"After insert: " << buffer.text() << std::endl;

    // Insert in middle
    buffer.insert(7, L" Beautiful", 10);
    std::wcout << L"After middle insert: " << buffer.text() << std::endl;

    // Erase
    buffer.erase(7, 10);
    std::wcout << L"After erase: " << buffer.text() << std::endl;

    // Line operations
    buffer.setText(L"Line 1\nLine 2\nLine 3\n");
    std::wcout << L"Line count: " << buffer.lineCount() << std::endl;
    std::wcout << L"Line 2: " << buffer.lineContent(1) << std::endl;

    // Position to line mapping
    size_t pos = 10; // In "Line 2"
    std::wcout << L"Position " << pos << L" is on line " << buffer.lineFromPos(pos) << std::endl;
}

// =============================================================================
// DEMO 3: Syntax Highlighting
// =============================================================================
static void demo3_syntax_highlighting() {
    std::cout << "=== Demo 3: Syntax Highlighting ===" << std::endl;

    // C++ highlighting
    MiniMonaco::GenericHighlighter cppHighlighter("cpp");
    std::wstring cppCode = LR"(
#include <iostream>

class MyClass {
public:
    void doSomething(int value) {
        std::string text = "Hello";
        if (value > 0) {
            return;
        }
    }
};
)";

    auto tokens = cppHighlighter.highlight(cppCode);
    std::cout << "C++ tokens found: " << tokens.size() << std::endl;
    for (const auto& token : tokens) {
        std::wstring text(cppCode.substr(token.start, token.end - token.start));
        std::wcout << L"  [" << token.start << L"-" << token.end << L"] " << text << L" = " << MiniMonaco::Utils::utf8ToWide(token.type) << std::endl;
    }

    // Python highlighting
    MiniMonaco::GenericHighlighter pyHighlighter("python");
    std::wstring pyCode = LR"(
def hello_world():
    """A simple function."""
    print("Hello, World!")
    return True
)";

    auto pyTokens = pyHighlighter.highlight(pyCode);
    std::cout << "Python tokens found: " << pyTokens.size() << std::endl;
    for (const auto& token : pyTokens) {
        std::wstring text(pyCode.substr(token.start, token.end - token.start));
        std::wcout << L"  [" << token.start << L"-" << token.end << L"] " << text << L" = " << MiniMonaco::Utils::utf8ToWide(token.type) << std::endl;
    }
}

// =============================================================================
// DEMO 4: Find and Replace
// =============================================================================
static void demo4_find_replace() {
    std::cout << "=== Demo 4: Find and Replace ===" << std::endl;

    MiniMonaco::TextBuffer buffer;
    buffer.setText(L"Hello World\nHello Universe\nHello Galaxy\n");

    MiniMonaco::FindReplace finder;

    // Find all occurrences
    auto results = finder.find(L"Hello", buffer, true);
    std::cout << "Found " << results.size() << " occurrences of 'Hello'" << std::endl;
    for (const auto& result : results) {
        std::wcout << L"  Line " << result.line << L", pos " << result.position << std::endl;
    }

    // Replace first occurrence
    if (!results.empty()) {
        finder.replace(buffer, results[0].position, results[0].length, L"Hi");
        std::wcout << L"After replace: " << buffer.text() << std::endl;
    }

    // Replace all
    buffer.setText(L"Hello World\nHello Universe\nHello Galaxy\n");
    size_t count = finder.replaceAll(buffer, L"Hello", L"Greetings");
    std::cout << "Replaced " << count << " occurrences" << std::endl;
    std::wcout << L"After replace all: " << buffer.text() << std::endl;
}

// =============================================================================
// DEMO 5: Undo/Redo
// =============================================================================
static void demo5_undo_redo() {
    std::cout << "=== Demo 5: Undo/Redo ===" << std::endl;

    MiniMonaco::TextBuffer buffer;
    MiniMonaco::UndoStack undoStack;

    // Simulate typing
    buffer.insert(0, L"H", 1);
    undoStack.pushInsert(0, L"H");

    buffer.insert(1, L"e", 1);
    undoStack.pushInsert(1, L"e");

    buffer.insert(2, L"l", 1);
    undoStack.pushInsert(2, L"l");

    buffer.insert(3, L"l", 1);
    undoStack.pushInsert(3, L"l");

    buffer.insert(4, L"o", 1);
    undoStack.pushInsert(4, L"o");

    std::wcout << L"After typing: " << buffer.text() << std::endl;

    // Undo last action
    auto undoActions = undoStack.undo();
    for (const auto& action : undoActions) {
        if (action.type == MiniMonaco::UndoAction::Insert) {
            buffer.erase(action.pos, action.text.size());
        }
    }
    std::wcout << L"After undo: " << buffer.text() << std::endl;

    // Redo
    auto redoActions = undoStack.redo();
    for (const auto& action : redoActions) {
        if (action.type == MiniMonaco::UndoAction::Insert) {
            buffer.insert(action.pos, action.text.c_str(), action.text.size());
        }
    }
    std::wcout << L"After redo: " << buffer.text() << std::endl;
}

// =============================================================================
// DEMO 6: Configuration and Theming
// =============================================================================
static void demo6_theming() {
    std::cout << "=== Demo 6: Configuration and Theming ===" << std::endl;

    MiniMonaco::Config darkTheme;
    darkTheme.fontSize = 14;
    darkTheme.fontFamily = L"Consolas";
    darkTheme.bgColor = 0x1E1E1E;
    darkTheme.textColor = 0xD4D4D4;
    darkTheme.selectionBg = 0x264F78;
    darkTheme.cursorColor = 0xAEAFAD;
    darkTheme.lineNumberColor = 0x858585;
    darkTheme.lineNumberBg = 0x1E1E1E;
    darkTheme.currentLineBg = 0x2C2C2C;
    darkTheme.searchHighlightBg = 0x613613;
    darkTheme.syntaxColors["keyword"] = 0x569CD6;
    darkTheme.syntaxColors["type"] = 0x4EC9B0;
    darkTheme.syntaxColors["string"] = 0xCE9178;
    darkTheme.syntaxColors["number"] = 0xB5CEA8;
    darkTheme.syntaxColors["comment"] = 0x6A9955;
    darkTheme.syntaxColors["function"] = 0xDCDCAA;
    darkTheme.syntaxColors["operator"] = 0xD4D4D4;
    darkTheme.syntaxColors["preprocessor"] = 0x569CD6;

    std::cout << "Dark theme configured with " << darkTheme.syntaxColors.size() << " syntax colors" << std::endl;

    MiniMonaco::Config lightTheme;
    lightTheme.fontSize = 14;
    lightTheme.fontFamily = L"Consolas";
    lightTheme.bgColor = 0xFFFFFF;
    lightTheme.textColor = 0x000000;
    lightTheme.selectionBg = 0xADD6FF;
    lightTheme.cursorColor = 0x000000;
    lightTheme.lineNumberColor = 0x237893;
    lightTheme.lineNumberBg = 0xF5F5F5;
    lightTheme.currentLineBg = 0xF5F5F5;
    lightTheme.searchHighlightBg = 0xA8AC94;
    lightTheme.syntaxColors["keyword"] = 0x0000FF;
    lightTheme.syntaxColors["type"] = 0x267F99;
    lightTheme.syntaxColors["string"] = 0xA31515;
    lightTheme.syntaxColors["number"] = 0x098658;
    lightTheme.syntaxColors["comment"] = 0x008000;
    lightTheme.syntaxColors["function"] = 0x795E26;
    lightTheme.syntaxColors["operator"] = 0x000000;
    lightTheme.syntaxColors["preprocessor"] = 0x0000FF;

    std::cout << "Light theme configured with " << lightTheme.syntaxColors.size() << " syntax colors" << std::endl;
}

// =============================================================================
// DEMO 7: Performance Test
// =============================================================================
static void demo7_performance() {
    std::cout << "=== Demo 7: Performance Test ===" << std::endl;

    const int numLines = 10000;
    const int charsPerLine = 80;

    // Generate large file
    std::wstring largeText;
    largeText.reserve(numLines * (charsPerLine + 1));

    for (int i = 0; i < numLines; ++i) {
        for (int j = 0; j < charsPerLine; ++j) {
            largeText.push_back(L'a' + (j % 26));
        }
        largeText.push_back(L'\n');
    }

    auto start = std::chrono::high_resolution_clock::now();

    MiniMonaco::TextBuffer buffer;
    buffer.setText(largeText);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Loaded " << numLines << " lines (" << (largeText.size() / 1024) << " KB) in " << duration.count() << " ms" << std::endl;
    std::cout << "Line count: " << buffer.lineCount() << std::endl;

    // Insert test
    start = std::chrono::high_resolution_clock::now();
    buffer.insert(largeText.size() / 2, L"INSERTED TEXT", 13);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Insert in middle: " << duration.count() << " ms" << std::endl;

    // Syntax highlighting test
    MiniMonaco::GenericHighlighter highlighter("cpp");
    std::wstring code = LR"(
#include <iostream>

int main() {
    std::cout << "Hello" << std::endl;
    return 0;
}
)";

    start = std::chrono::high_resolution_clock::now();
    auto tokens = highlighter.highlight(code);
    end = std::chrono::high_resolution_clock::now();
    auto microDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Syntax highlighting: " << tokens.size() << " tokens in " << microDuration.count() << " us" << std::endl;
}

// =============================================================================
// MAIN
// =============================================================================
int main(int argc, char* argv[]) {
    std::cout << "MiniMonaco Editor Demo" << std::endl;
    std::cout << "======================" << std::endl << std::endl;

    if (argc > 1 && std::string(argv[1]) == "--window") {
        demo1_basic_window();
    } else {
        demo2_text_buffer();
        std::cout << std::endl;

        demo3_syntax_highlighting();
        std::cout << std::endl;

        demo4_find_replace();
        std::cout << std::endl;

        demo5_undo_redo();
        std::cout << std::endl;

        demo6_theming();
        std::cout << std::endl;

        demo7_performance();
        std::cout << std::endl;

        std::cout << "All demos completed successfully!" << std::endl;
        std::cout << "Run with --window to see the GUI demo." << std::endl;
    }

    return 0;
}
