// MASM Text Editor Qt Wrapper - PRODUCTION GRADE
// No placeholders, fully functional implementation

#ifndef MASM_EDITOR_WIDGET_H
#define MASM_EDITOR_WIDGET_H


#include <vector>
#include <memory>
#include <unordered_map>

// Assembly language syntax highlighter
class AssemblyHighlighter : public QSyntaxHighlighter {

public:
    explicit AssemblyHighlighter(QTextDocument* parent = nullptr);
    
protected:
    void highlightBlock(const std::string& text) override;
    
private:
    struct HighlightingRule {
        std::regex pattern;
        QTextCharFormat format;
    };
    std::vector<HighlightingRule> highlightingRules;
    QTextCharFormat keywordFormat;
    QTextCharFormat registerFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat labelFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat directiveFormat;
};

// Tab data structure
struct TabData {
    std::string name;
    std::string filePath;
    bool modified;
    int scrollPosition;
    int cursorPosition;
    
    TabData() : modified(false), scrollPosition(0), cursorPosition(0) {}
};

// Custom tab bar with context menu
class EditorTabBar : public QTabBar {

public:
    explicit EditorTabBar(void* parent = nullptr);
    

    void tabCloseRequested(int index);
    void tabRenameRequested(int index);
    void closeAllRequested();
    void closeOthersRequested(int index);
    
protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
};

// Main editor widget
class MASMEditorWidget : public void {

public:
    explicit MASMEditorWidget(void* parent = nullptr);
    ~MASMEditorWidget();
    
    // Tab management
    int newTab(const std::string& name = std::string());
    void closeTab(int index);
    void closeAllTabs();
    void closeOtherTabs(int keepIndex);
    bool switchTab(int index);
    int getTabCount() const;
    int getCurrentTabIndex() const;
    std::string getTabName(int index) const;
    void setTabName(int index, const std::string& name);
    
    // Content management
    std::string getContent(int index = -1) const;
    void setContent(const std::string& content, int index = -1);
    bool isModified(int index = -1) const;
    void setModified(bool modified, int index = -1);
    
    // File operations
    bool loadFile(const std::string& filePath, int index = -1);
    bool saveFile(const std::string& filePath = std::string(), int index = -1);
    std::string getFilePath(int index = -1) const;
    
    // Editor operations
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();
    void find(const std::string& text);
    void findNext();
    void replace(const std::string& find, const std::string& replaceWith);
    void goToLine(int line);
    
    // Statistics
    int getLineCount(int index = -1) const;
    int getCharCount(int index = -1) const;
    std::pair<int, int> getCursorPosition(int index = -1) const;
    

    void tabChanged(int index);
    void contentModified(int index);
    void cursorPositionChanged(int line, int column);
    void tabCountChanged(int count);
    
private:
    void onTabChanged(int index);
    void onTextChanged();
    void onCursorMoved();
    void onTabCloseRequested(int index);
    void onTabRenameRequested(int index);
    void updateStatusBar();
    void onCaretBlink();
    
private:
    void setupUI();
    void setupToolbar();
    void setupConnections();
    QTextEdit* createEditor();
    void updateTabTitle(int index);
    int resolveIndex(int index) const;
    
    // UI components
    QVBoxLayout* mainLayout;
    QToolBar* toolBar;
    EditorTabBar* tabBar;
    QStackedWidget* editorStack;
    QStatusBar* statusBar;
    
    // Data
    std::vector<QTextEdit*> editors;
    std::vector<std::unique_ptr<AssemblyHighlighter>> highlighters;
    std::vector<TabData> tabData;
    
    // State
    int nextTabNumber;
    std::string lastSearchText;
    void** caretTimer;
    bool caretVisible;
};

#endif // MASM_EDITOR_WIDGET_H

