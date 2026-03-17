// MASM Text Editor Qt Wrapper
// Integrates x64 assembly editor as dockable widget in MainWindow

#ifndef MASM_EDITOR_WIDGET_H
#define MASM_EDITOR_WIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QStatusBar>
#include <QTabBar>
#include <QVBoxLayout>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <map>
#include <memory>

// Forward declaration of MASM functions
extern "C" {
    void InitializeGapBuffer(void* buffer_ptr, unsigned long long size);
    void InitializeTabManager(void* mgr_ptr);
    void InsertCharacter(void* buffer, unsigned char ch, unsigned long long pos);
    void DeleteCharacter(void* buffer, unsigned long long pos);
    void CreateTab(void* mgr);
    void CloseTab(void* mgr, unsigned long long tab_id);
    void RenderText(void* render_buf, void* gap_buf, unsigned long long cursor);
    void RenderCaret(void* render_buf, unsigned long long cursor);
    void HandleKeyboardInput(void* buffer, unsigned int key, unsigned long long pos);
    void SwitchToTab(void* mgr, unsigned long long tab_id);
}

// Assembly Text Highlighter
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
};

// MASM Text Editor Widget
class MASMEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MASMEditorWidget(QWidget* parent = nullptr);
    ~MASMEditorWidget();
    
    void newTab(const QString& name = "Untitled");
    void closeTab(int index);
    void switchTab(int index);
    QString getTabContent(int index) const;
    void setTabContent(int index, const QString& content);
    int getTabCount() const;
    int getCurrentTabIndex() const;
    
signals:
    void contentChanged(int tabIndex);
    void tabSwitched(int tabIndex);
    void tabClosed(int tabIndex);
    
private slots:
    void onTabChanged(int index);
    void onTextChanged();
    void onCursorPositionChanged();
    void onCustomContextMenu(const QPoint& pos);
    
private:
    void setupUI();
    void setupConnections();
    void setupAssemblyHighlighting();
    void updateStatusBar();
    
    // UI Components
    QVBoxLayout* mainLayout;
    QTabBar* tabBar;
    std::vector<QTextEdit*> editors;
    std::map<int, QString> tabNames;
    QToolBar* toolBar;
    QStatusBar* statusBar;
    
    // MASM internals (for potential native acceleration)
    void* gapBuffers[10000000];  // Pointers to native gap buffers (lazy-loaded)
    void* tabManager;
    int currentTabIndex;
    unsigned long long cursorPosition;
    
    // Syntax highlighting
    AssemblyHighlighter* highlighter;
    
    // Settings
    bool showLineNumbers;
    bool enableNativeAcceleration;
    int tabSize;
    int maxTabs;
};

// Tab Bar with Custom Context Menu
class CustomTabBar : public QTabBar {
    Q_OBJECT
public:
    explicit CustomTabBar(QWidget* parent = nullptr);
    
signals:
    void tabClosedRequested(int index);
    void tabRenamedRequested(int index);
    
protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
};

#endif // MASM_EDITOR_WIDGET_H
