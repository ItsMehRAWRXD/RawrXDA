#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QToolButton>
#include <QLabel>
#include <QMap>
#include <memory>

class QVBoxLayout;
class CanvasWidget;
class FeaturesViewMenu;

/**
 * @class PaintEditorTab
 * @brief Individual paint editor tab with canvas and tools
 */
class PaintEditorTab : public QWidget {
    Q_OBJECT
public:
    explicit PaintEditorTab(const QString &tabName, QWidget *parent = nullptr);
    ~PaintEditorTab();
    
    CanvasWidget* getCanvas() { return m_canvas; }
    QString getTabName() const { return m_tabName; }
    bool hasUnsavedChanges() const { return m_unsavedChanges; }
    void setUnsavedChanges(bool changed) { m_unsavedChanges = changed; }
    void exportPNG(const QString &filepath);
    void exportBMP(const QString &filepath);

private:
    CanvasWidget *m_canvas;
    QString m_tabName;
    bool m_unsavedChanges;
};

/**
 * @class PaintTabbedEditor
 * @brief Multi-tab paint editor with +/- and X buttons like code editor
 * Supports unlimited paint tabs with full tab management
 */
class PaintTabbedEditor : public QWidget {
    Q_OBJECT

public:
    explicit PaintTabbedEditor(QWidget *parent = nullptr);
    ~PaintTabbedEditor();
    
    void initialize();
    void newPaintTab();
    void closePaintTab(int index);
    void closePaintTab(PaintEditorTab *tab);
    void saveCurrentPaint();
    void savePaintAs();
    void exportCurrentAsImage();
    PaintEditorTab* getCurrentPaintTab() const;
    int getTabCount() const { return m_tabWidget->count(); }

signals:
    void tabCountChanged(int count);
    void currentTabChanged(PaintEditorTab *tab);
    void paintModified(PaintEditorTab *tab);

private slots:
    void onTabChanged(int index);
    void onNewTabClicked();
    void onTabContextMenu(const QPoint &pos);
    void onCloseTabClicked();
    void onCloseAllOtherTabs();
    void onCloseAllTabs();

private:
    void createTabBar();
    void setupConnections();
    QString generateNewTabName();
    
    QTabWidget *m_tabWidget;
    QToolButton *m_newTabBtn;
    QToolButton *m_closeTabBtn;
    int m_tabCounter;
    
    QMap<int, PaintEditorTab*> m_tabs;
};

/**
 * @class ChatTabbedInterface
 * @brief Chat interface with multi-tab support (supports 100 tabs)
 */
class ChatTabbedInterface : public QWidget {
    Q_OBJECT

public:
    explicit ChatTabbedInterface(QWidget *parent = nullptr);
    ~ChatTabbedInterface();
    
    void initialize();
    void newChatTab();
    void closeChatTab(int index);
    int getTabCount() const { return m_tabWidget->count(); }
    class ChatInterface* getCurrentChatInterface() const;

signals:
    void tabCountChanged(int count);
    void currentChatTabChanged(int index);

private slots:
    void onTabChanged(int index);
    void onNewChatTabClicked();
    void onCloseTabClicked();
    void onTabContextMenu(const QPoint &pos);

private:
    void createTabBar();
    QString generateNewChatTabName();
    
    QTabWidget *m_tabWidget;
    QToolButton *m_newChatTabBtn;
    QToolButton *m_closeChatTabBtn;
    int m_chatTabCounter;
};

/**
 * @class EnhancedMultiTabEditor
 * @brief Extended code editor supporting ultra-high tab counts for MASM (1M+ tabs)
 */
class EnhancedMultiTabEditor : public QWidget {
    Q_OBJECT

public:
    explicit EnhancedMultiTabEditor(QWidget *parent = nullptr);
    ~EnhancedMultiTabEditor();
    
    void initialize();
    int getTabCount() const;
    void optimizeForHighTabCount(int estimatedCount);

signals:
    void tabCountWarning(int count);
    void performanceAlert(const QString &message);

private:
    // Optimizations for high tab counts
    void enableLazyLoading();
    void enableTabPooling();
    void enableMemoryCompression();
    
    bool m_lazyLoadingEnabled;
    bool m_tabPoolingEnabled;
    bool m_memoryCompressionEnabled;
    int m_maxCachedTabs;
};
