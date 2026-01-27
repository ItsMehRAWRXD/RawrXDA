// MASM Text Editor Qt Wrapper - PRODUCTION GRADE
// No placeholders, fully functional implementation

#ifndef MASM_EDITOR_WIDGET_H
#define MASM_EDITOR_WIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QStatusBar>
#include <QTabBar>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QTimer>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <QRegularExpression>

// Assembly language syntax highlighter
class AssemblyHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit AssemblyHighlighter(QTextDocument* parent = nullptr);
    
protected:
    void highlightBlock(const QString& text) override;
    
private:
    struct HighlightingRule {
        QRegularExpression pattern;
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
    QString name;
    QString filePath;
    bool modified;
    int scrollPosition;
    int cursorPosition;
    
    TabData() : modified(false), scrollPosition(0), cursorPosition(0) {}
};

// Custom tab bar with context menu
class EditorTabBar : public QTabBar {
    Q_OBJECT
public:
    explicit EditorTabBar(QWidget* parent = nullptr);
    
signals:
    void tabCloseRequested(int index);
    void tabRenameRequested(int index);
    void closeAllRequested();
    void closeOthersRequested(int index);
    
protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
};

// Main editor widget
class MASMEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MASMEditorWidget(QWidget* parent = nullptr);
    ~MASMEditorWidget();
    
    // Tab management
    int newTab(const QString& name = QString());
    void closeTab(int index);
    void closeAllTabs();
    void closeOtherTabs(int keepIndex);
    bool switchTab(int index);
    int getTabCount() const;
    int getCurrentTabIndex() const;
    QString getTabName(int index) const;
    void setTabName(int index, const QString& name);
    
    // Content management
    QString getContent(int index = -1) const;
    void setContent(const QString& content, int index = -1);
    bool isModified(int index = -1) const;
    void setModified(bool modified, int index = -1);
    
    // File operations
    bool loadFile(const QString& filePath, int index = -1);
    bool saveFile(const QString& filePath = QString(), int index = -1);
    QString getFilePath(int index = -1) const;
    
    // Editor operations
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();
    void find(const QString& text);
    void findNext();
    void replace(const QString& find, const QString& replaceWith);
    void goToLine(int line);
    
    // Statistics
    int getLineCount(int index = -1) const;
    int getCharCount(int index = -1) const;
    QPair<int, int> getCursorPosition(int index = -1) const;
    
signals:
    void tabChanged(int index);
    void contentModified(int index);
    void cursorPositionChanged(int line, int column);
    void tabCountChanged(int count);
    
private slots:
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
    QString lastSearchText;
    QTimer* caretTimer;
    bool caretVisible;
};

#endif // MASM_EDITOR_WIDGET_H
