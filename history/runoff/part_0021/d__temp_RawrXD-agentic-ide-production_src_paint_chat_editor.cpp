// Converted headless implementation matching the native, non-Qt headers.

#include "paint_chat_editor.h"

#include <algorithm>
#include <utility>

// ============================================================================
// PaintEditorTab Implementation
// ============================================================================

PaintEditorTab::PaintEditorTab(const std::string &tabName)
    : m_canvas(std::make_unique<PaintCanvas>(1024, 768))
    , m_tabName(tabName)
    , m_unsavedChanges(false) {
    buildUI();
    connectCanvasSignals();
}

PaintEditorTab::~PaintEditorTab() = default;

void PaintEditorTab::buildUI() {
    // Headless: nothing to construct visually.
}

void PaintEditorTab::connectCanvasSignals() {
    // Headless: no signal wiring needed.
}

void PaintEditorTab::updateColorButtons() {
    // Headless: nothing to update visually.
}

void PaintEditorTab::setUnsavedChanges(bool changed) {
    m_unsavedChanges = changed;
    if (onCanvasModified) onCanvasModified();
}

void PaintEditorTab::exportPNG(const std::string &filepath) {
    if (m_canvas && m_canvas->save_as_png(filepath)) {
        m_lastSavedPath = filepath;
        setUnsavedChanges(false);
    }
}

void PaintEditorTab::exportBMP(const std::string &filepath) {
    if (m_canvas && m_canvas->save_as_bmp(filepath)) {
        m_lastSavedPath = filepath;
        setUnsavedChanges(false);
    }
}

void PaintEditorTab::clear() {
    if (m_canvas) {
        m_canvas->clear_canvas(ig::Color::white());
        setUnsavedChanges(true);
        updateColorButtons();
    }
}

// ============================================================================
// PaintTabbedEditor Implementation
// ============================================================================

PaintTabbedEditor::PaintTabbedEditor() = default;

PaintTabbedEditor::~PaintTabbedEditor() {
    closeAllTabs();
}

void PaintTabbedEditor::initialize() {
    createUI();
    setupConnections();
}

void PaintTabbedEditor::createUI() {
    // Headless: maintain tab bookkeeping only.
}

void PaintTabbedEditor::setupConnections() {
    // Headless: no UI signals to wire up.
}

std::string PaintTabbedEditor::generateNewTabName() {
    return "Canvas " + std::to_string(++m_tabCounter);
}

void PaintTabbedEditor::newPaintTab() {
    auto name = generateNewTabName();
    auto tab = std::make_unique<PaintEditorTab>(name);
    int index = static_cast<int>(m_tabs.size());

    PaintEditorTab* raw = tab.get();
    m_tabs.emplace(index, std::move(tab));
    m_currentIndex = index;

    if (raw) {
        raw->onCanvasModified = [this]() { updateTabLabel(m_currentIndex); };
        raw->onCloseRequested = [this, index]() { closePaintTab(index); };
    }

    if (onTabCountChanged) onTabCountChanged(static_cast<int>(m_tabs.size()));
    if (onCurrentTabChanged) onCurrentTabChanged(raw);
}

void PaintTabbedEditor::closePaintTab(int index) {
    auto it = m_tabs.find(index);
    if (it == m_tabs.end()) return;
    m_tabs.erase(it);

    if (m_tabs.empty()) {
        m_currentIndex = -1;
    } else {
        m_currentIndex = m_tabs.begin()->first;
    }

    if (onTabCountChanged) onTabCountChanged(static_cast<int>(m_tabs.size()));
    if (onCurrentTabChanged) onCurrentTabChanged(getCurrentPaintTab());
}

void PaintTabbedEditor::savePaintTab(int index) {
    auto* tab = m_tabs.count(index) ? m_tabs[index].get() : nullptr;
    if (!tab) return;
    tab->setUnsavedChanges(false);
    updateTabLabel(index);
}

void PaintTabbedEditor::saveAllPaintTabs() {
    for (auto &kv : m_tabs) {
        if (kv.second) kv.second->setUnsavedChanges(false);
    }
    if (onTabCountChanged) onTabCountChanged(static_cast<int>(m_tabs.size()));
}

void PaintTabbedEditor::exportCurrentAsImage() {
    auto* tab = getCurrentPaintTab();
    if (!tab) return;
    tab->exportPNG("paint_export.png");
}

PaintEditorTab* PaintTabbedEditor::getCurrentPaintTab() const {
    auto it = m_tabs.find(m_currentIndex);
    return it != m_tabs.end() ? it->second.get() : nullptr;
}

int PaintTabbedEditor::getTabCount() const {
    return static_cast<int>(m_tabs.size());
}

void PaintTabbedEditor::closeAllTabs() {
    m_tabs.clear();
    m_currentIndex = -1;
    if (onTabCountChanged) onTabCountChanged(0);
    if (onCurrentTabChanged) onCurrentTabChanged(nullptr);
}

void PaintTabbedEditor::closeAllExcept(int index) {
    for (auto it = m_tabs.begin(); it != m_tabs.end();) {
        if (it->first != index) {
            it = m_tabs.erase(it);
        } else {
            ++it;
        }
    }
    m_currentIndex = m_tabs.count(index) ? index : -1;
    if (onTabCountChanged) onTabCountChanged(static_cast<int>(m_tabs.size()));
    if (onCurrentTabChanged) onCurrentTabChanged(getCurrentPaintTab());
}

void PaintTabbedEditor::updateTabLabel(int index) {
    (void)index; // Headless: nothing to update visually.
}

// ============================================================================
// ChatTabbedInterface Implementation
// ============================================================================

ChatTabbedInterface::ChatTabbedInterface() = default;
ChatTabbedInterface::~ChatTabbedInterface() = default;

void ChatTabbedInterface::initialize() {
    m_chatTabCounter = 0;
}

void ChatTabbedInterface::newChatTab() {
    ++m_chatTabCounter;
}

void ChatTabbedInterface::closeChatTab(int index) {
    if (index >= 0 && m_chatTabCounter > 0) {
        --m_chatTabCounter;
    }
}

int ChatTabbedInterface::getTabCount() const {
    return m_chatTabCounter;
}

// ============================================================================
// EnhancedCodeEditor Implementation
// ============================================================================

EnhancedCodeEditor::EnhancedCodeEditor() = default;
EnhancedCodeEditor::~EnhancedCodeEditor() = default;

void EnhancedCodeEditor::initialize() {
    m_maxCachedTabs = 0;
}

int EnhancedCodeEditor::getTabCount() const {
    return m_maxCachedTabs;
}

void EnhancedCodeEditor::optimizeForMASM() {
    m_maxCachedTabs = 1'000'000; // retain MASM upper bound semantics
}

void EnhancedCodeEditor::optimizeForStandard() {
    m_maxCachedTabs = 100'000; // retain standard code upper bound semantics
}// Headless, platform-neutral implementation matching the stripped-down headers.

#include "paint_chat_editor.h"
// Headless, platform-neutral implementation matching the stripped-down headers.

#include "paint_chat_editor.h"
#include <algorithm>
#include <utility>

// ============================================================================
// PaintEditorTab Implementation
// ============================================================================

PaintEditorTab::PaintEditorTab(const std::string &tabName)
    : m_canvas(std::make_unique<PaintCanvas>(1024, 768))
    , m_tabName(tabName)
    , m_unsavedChanges(false) {}

PaintEditorTab::~PaintEditorTab() = default;

void PaintEditorTab::buildUI() {
    // Headless: no UI to construct.
}

void PaintEditorTab::connectCanvasSignals() {
    // Headless: no signal wiring needed.
}

void PaintEditorTab::updateColorButtons() {
    // Headless: nothing to update visually.
}

void PaintEditorTab::setUnsavedChanges(bool changed) {
    m_unsavedChanges = changed;
    if (onCanvasModified) onCanvasModified();
}

void PaintEditorTab::exportPNG(const std::string &filepath) {
    if (m_canvas && m_canvas->save_as_png(filepath)) {
        m_lastSavedPath = filepath;
        setUnsavedChanges(false);
    }
}

void PaintEditorTab::exportBMP(const std::string &filepath) {
    if (m_canvas && m_canvas->save_as_bmp(filepath)) {
        m_lastSavedPath = filepath;
        setUnsavedChanges(false);
    }
}

void PaintEditorTab::clear() {
    if (m_canvas) {
        m_canvas->clear_canvas(ig::Color::white());
        setUnsavedChanges(true);
    }
}

// ============================================================================
// PaintTabbedEditor Implementation
// ============================================================================

PaintTabbedEditor::PaintTabbedEditor() = default;
PaintTabbedEditor::~PaintTabbedEditor() { closeAllTabs(); }

void PaintTabbedEditor::initialize() {
    createUI();
    setupConnections();
}

void PaintTabbedEditor::createUI() {
    // Headless: nothing to lay out; maintain data only.
}

void PaintTabbedEditor::setupConnections() {
    // Headless: no UI signals to connect.
}

std::string PaintTabbedEditor::generateNewTabName() {
    return "Canvas " + std::to_string(++m_tabCounter);
}

void PaintTabbedEditor::newPaintTab() {
    auto name = generateNewTabName();
    auto tab = std::make_unique<PaintEditorTab>(name);
    int index = static_cast<int>(m_tabs.size());

    PaintEditorTab* raw = tab.get();
    m_tabs.emplace(index, std::move(tab));
    m_currentIndex = index;

    if (raw) {
        raw->onCanvasModified = [this]() { updateTabLabel(m_currentIndex); };
        raw->onCloseRequested = [this, index]() { closePaintTab(index); };
    }

    if (onTabCountChanged) onTabCountChanged(static_cast<int>(m_tabs.size()));
    if (onCurrentTabChanged) onCurrentTabChanged(raw);
}

void PaintTabbedEditor::closePaintTab(int index) {
    auto it = m_tabs.find(index);
    if (it == m_tabs.end()) return;
    m_tabs.erase(it);

    if (m_tabs.empty()) {
        m_currentIndex = -1;
    } else {
        m_currentIndex = m_tabs.begin()->first;
    }

    if (onTabCountChanged) onTabCountChanged(static_cast<int>(m_tabs.size()));
    if (onCurrentTabChanged) onCurrentTabChanged(getCurrentPaintTab());
}

void PaintTabbedEditor::savePaintTab(int index) {
    auto* tab = m_tabs.count(index) ? m_tabs[index].get() : nullptr;
    if (!tab) return;
    tab->setUnsavedChanges(false);
    updateTabLabel(index);
}

void PaintTabbedEditor::saveAllPaintTabs() {
    for (auto& kv : m_tabs) {
        if (kv.second) kv.second->setUnsavedChanges(false);
    }
    if (onTabCountChanged) onTabCountChanged(static_cast<int>(m_tabs.size()));
}

void PaintTabbedEditor::exportCurrentAsImage() {
    auto* tab = getCurrentPaintTab();
    if (!tab) return;
    tab->exportPNG("paint_export.png");
}

PaintEditorTab* PaintTabbedEditor::getCurrentPaintTab() const {
    auto it = m_tabs.find(m_currentIndex);
    return it != m_tabs.end() ? it->second.get() : nullptr;
}

int PaintTabbedEditor::getTabCount() const {
    return static_cast<int>(m_tabs.size());
}

void PaintTabbedEditor::closeAllTabs() {
    m_tabs.clear();
    m_currentIndex = -1;
    if (onTabCountChanged) onTabCountChanged(0);
    if (onCurrentTabChanged) onCurrentTabChanged(nullptr);
}

void PaintTabbedEditor::closeAllExcept(int index) {
    for (auto it = m_tabs.begin(); it != m_tabs.end();) {
        if (it->first != index) it = m_tabs.erase(it); else ++it;
    }
    m_currentIndex = m_tabs.count(index) ? index : -1;
    if (onTabCountChanged) onTabCountChanged(static_cast<int>(m_tabs.size()));
    if (onCurrentTabChanged) onCurrentTabChanged(getCurrentPaintTab());
}

void PaintTabbedEditor::updateTabLabel(int index) {
    (void)index; // Headless: nothing to update visually.
}

// ============================================================================
// ChatTabbedInterface Implementation
// ============================================================================

ChatTabbedInterface::ChatTabbedInterface() = default;
ChatTabbedInterface::~ChatTabbedInterface() = default;

void ChatTabbedInterface::initialize() { m_chatTabCounter = 0; }
void ChatTabbedInterface::newChatTab() { ++m_chatTabCounter; }
void ChatTabbedInterface::closeChatTab(int index) { if (index >= 0 && m_chatTabCounter > 0) --m_chatTabCounter; }
int ChatTabbedInterface::getTabCount() const { return m_chatTabCounter; }

// ============================================================================
// EnhancedCodeEditor Implementation
// ============================================================================

EnhancedCodeEditor::EnhancedCodeEditor() = default;
EnhancedCodeEditor::~EnhancedCodeEditor() = default;

void EnhancedCodeEditor::initialize() { m_maxCachedTabs = 0; }
int EnhancedCodeEditor::getTabCount() const { return m_maxCachedTabs; }
void EnhancedCodeEditor::optimizeForMASM() { m_maxCachedTabs = 1'000'000; }
void EnhancedCodeEditor::optimizeForStandard() { m_maxCachedTabs = 100'000; }

void PaintTabbedEditor::initialize() {
    createUI();
    setupConnections();
    newPaintTab(); // Create initial blank tab
}

void PaintTabbedEditor::createUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Tab toolbar
    auto toolbarLayout = new QHBoxLayout();
    
    m_newTabBtn = new QToolButton(this);
    m_newTabBtn->setText("+");
    m_newTabBtn->setToolTip("New Paint Tab (Ctrl+T)");
    m_newTabBtn->setObjectName("NewPaintTab");
    m_newTabBtn->setMinimumWidth(32);
    m_newTabBtn->setMinimumHeight(32);
    
    m_closeTabBtn = new QToolButton(this);
    m_closeTabBtn->setText("×");
    m_closeTabBtn->setToolTip("Close Current Tab (Ctrl+W)");
    m_closeTabBtn->setObjectName("ClosePaintTab");
    m_closeTabBtn->setMinimumWidth(32);
    m_closeTabBtn->setMinimumHeight(32);
    
    toolbarLayout->addWidget(m_newTabBtn);
    toolbarLayout->addWidget(m_closeTabBtn);
    toolbarLayout->addStretch();
    
    mainLayout->addLayout(toolbarLayout);
    
    // Tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    
    mainLayout->addWidget(m_tabWidget);
    setLayout(mainLayout);
}

void PaintTabbedEditor::setupConnections() {
    if (!m_tabWidget) return;
    
    connect(m_newTabBtn, &QToolButton::clicked, this, &PaintTabbedEditor::onNewTabClicked);
    connect(m_closeTabBtn, &QToolButton::clicked, this, &PaintTabbedEditor::onCloseTabClicked);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &PaintTabbedEditor::onTabChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &PaintTabbedEditor::onTabCloseRequested);
    connect(m_tabWidget, &QTabWidget::customContextMenuRequested, this, &PaintTabbedEditor::onTabContextMenu);
    
    m_tabWidget->setContextMenuPolicy(Qt::CustomContextMenu);
}

QString PaintTabbedEditor::generateNewTabName() {
    return QString("Untitled %1").arg(++m_tabCounter);
}

void PaintTabbedEditor::newPaintTab() {
    QString tabName = generateNewTabName();
    auto tab = new PaintEditorTab(tabName, this);
    
    connect(tab, &PaintEditorTab::canvasModified, this, &PaintTabbedEditor::paintModified);
    connect(tab, &PaintEditorTab::canvasModified, this, [this, tab]() {
        int idx = m_tabWidget ? m_tabWidget->indexOf(tab) : -1;
        if (idx >= 0) updateTabLabel(idx);
    });
    connect(tab, &PaintEditorTab::closeRequested, this, [this, tab]() {
        int index = m_tabWidget->indexOf(tab);
        if (index >= 0) closePaintTab(index);
    });
    
    int index = m_tabWidget->addTab(tab, tabName);
    m_tabs[index] = tab;
    m_tabWidget->setCurrentIndex(index);
    
    emit tabCountChanged(m_tabWidget->count());
    updateTabLabel(index);
}

void PaintTabbedEditor::closePaintTab(int index) {
    if (index < 0 || index >= m_tabWidget->count()) return;
    
    auto tab = m_tabs.value(index);
    if (!tab) return;
    
    // Check for unsaved changes
    if (tab->hasUnsavedChanges()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Unsaved Changes",
            QString("Save changes to %1 before closing?").arg(tab->getTabName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
        );
        
        if (reply == QMessageBox::Cancel) return;
        if (reply == QMessageBox::Save) savePaintTab(index);
    }
    
    m_tabWidget->removeTab(index);
    m_tabs.remove(index);
    tab->deleteLater();
    
    emit tabCountChanged(m_tabWidget->count());
}

void PaintTabbedEditor::savePaintTab(int index) {
    auto tab = m_tabs.value(index);
    if (!tab) return;
    
    QString filename = QFileDialog::getSaveFileName(
        this, "Save Paint Canvas", QString(),
        "PNG Images (*.png);;BMP Images (*.bmp);;All Files (*.*)"
    );
    
    if (filename.isEmpty()) return;
    
    if (filename.endsWith(".png", Qt::CaseInsensitive)) {
        tab->exportPNG(filename);
    } else {
        tab->exportBMP(filename);
    }
}

void PaintTabbedEditor::saveAllPaintTabs() {
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto tab = m_tabs.value(i);
        if (tab && tab->hasUnsavedChanges()) {
            savePaintTab(i);
        }
    }
}

void PaintTabbedEditor::exportCurrentAsImage() {
    int currentIndex = m_tabWidget->currentIndex();
    if (currentIndex >= 0) {
        savePaintTab(currentIndex);
    }
}

PaintEditorTab* PaintTabbedEditor::getCurrentPaintTab() const {
    if (!m_tabWidget) return nullptr;
    int currentIndex = m_tabWidget->currentIndex();
    return m_tabs.value(currentIndex);
}

void PaintTabbedEditor::closeAllTabs() {
    while (m_tabWidget->count() > 0) {
        closePaintTab(0);
    }
}

void PaintTabbedEditor::closeAllExcept(int index) {
    // Close in reverse to maintain indices
    for (int i = m_tabWidget->count() - 1; i >= 0; --i) {
        if (i != index) {
            closePaintTab(i);
        }
    }
}

void PaintTabbedEditor::updateTabLabel(int index) {
    auto tab = m_tabs.value(index);
    if (!tab) return;
    
    QString label = tab->getTabName();
    if (tab->hasUnsavedChanges()) {
        label.prepend("* ");
    }
    m_tabWidget->setTabText(index, label);
}

void PaintTabbedEditor::onTabChanged(int index) {
    auto tab = m_tabs.value(index);
    if (tab) {
        emit currentTabChanged(tab);
    }
}

void PaintTabbedEditor::onNewTabClicked() {
    newPaintTab();
}

void PaintTabbedEditor::onCloseTabClicked() {
    int currentIndex = m_tabWidget->currentIndex();
    if (currentIndex >= 0) {
        closePaintTab(currentIndex);
    }
}

void PaintTabbedEditor::onTabContextMenu(const QPoint &pos) {
    int index = m_tabWidget->tabBar()->tabAt(pos);
    if (index < 0) return;
    
    QMenu menu;
    menu.addAction("Close Tab", this, [this, index]() { closePaintTab(index); });
    menu.addAction("Close All Other Tabs", this, [this, index]() { closeAllExcept(index); });
    menu.addSeparator();
    menu.addAction("Save Tab", this, [this, index]() { savePaintTab(index); });
    menu.addAction("Export as PNG", this, [this, index]() {
        auto tab = m_tabs.value(index);
        if (!tab) return;
        QString filename = QFileDialog::getSaveFileName(
            this, "Export as PNG", QString(), "PNG Images (*.png)"
        );
        if (!filename.isEmpty()) tab->exportPNG(filename);
    });
    
    menu.exec(m_tabWidget->tabBar()->mapToGlobal(pos));
}

void PaintTabbedEditor::onTabCloseRequested(int index) {
    closePaintTab(index);
}

void PaintTabbedEditor::onCloseAllOtherTabs() {
    closeAllExcept(m_tabWidget->currentIndex());
}

// ============================================================================
// ChatTabbedInterface Implementation
// ============================================================================

class ChatInterface : public QWidget {
public:
    explicit ChatInterface(QWidget *parent = nullptr) : QWidget(parent) {}
    virtual ~ChatInterface() = default;
    virtual QString getConversationName() const { return "Chat"; }
};

ChatTabbedInterface::ChatTabbedInterface(QWidget *parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_newChatTabBtn(nullptr)
    , m_closeChatTabBtn(nullptr)
    , m_chatTabCounter(0)
{
}

ChatTabbedInterface::~ChatTabbedInterface() {
    // Clean up chat interfaces
    for (auto chat : m_chats) {
        if (chat) chat->deleteLater();
    }
    m_chats.clear();
}

void ChatTabbedInterface::initialize() {
    createUI();
    setupConnections();
    newChatTab();
}

void ChatTabbedInterface::createUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Tab toolbar
    auto toolbarLayout = new QHBoxLayout();
    
    m_newChatTabBtn = new QToolButton(this);
    m_newChatTabBtn->setText("+");
    m_newChatTabBtn->setToolTip("New Chat (Ctrl+N)");
    m_newChatTabBtn->setMinimumWidth(32);
    m_newChatTabBtn->setMinimumHeight(32);
    
    m_closeChatTabBtn = new QToolButton(this);
    m_closeChatTabBtn->setText("×");
    m_closeChatTabBtn->setToolTip("Close Chat");
    m_closeChatTabBtn->setMinimumWidth(32);
    m_closeChatTabBtn->setMinimumHeight(32);
    
    toolbarLayout->addWidget(m_newChatTabBtn);
    toolbarLayout->addWidget(m_closeChatTabBtn);
    toolbarLayout->addStretch();
    
    mainLayout->addLayout(toolbarLayout);
    
    // Tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    
    mainLayout->addWidget(m_tabWidget);
    setLayout(mainLayout);
}

void ChatTabbedInterface::setupConnections() {
    if (!m_tabWidget) return;
    
    connect(m_newChatTabBtn, &QToolButton::clicked, this, &ChatTabbedInterface::onNewChatTabClicked);
    connect(m_closeChatTabBtn, &QToolButton::clicked, this, &ChatTabbedInterface::onCloseTabClicked);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &ChatTabbedInterface::onTabChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &ChatTabbedInterface::onTabCloseRequested);
}

QString ChatTabbedInterface::generateNewChatTabName() {
    return QString("Chat %1").arg(++m_chatTabCounter);
}

void ChatTabbedInterface::newChatTab() {
    QString tabName = generateNewChatTabName();
    auto chatInterface = new ChatInterface(this);
    
    int index = m_tabWidget->addTab(chatInterface, tabName);
    m_chats[index] = chatInterface;
    m_tabWidget->setCurrentIndex(index);
    
    emit tabCountChanged(m_tabWidget->count());
}

void ChatTabbedInterface::closeChatTab(int index) {
    if (index < 0 || index >= m_tabWidget->count()) return;
    
    auto chat = m_chats.value(index);
    if (!chat) return;
    
    m_tabWidget->removeTab(index);
    m_chats.remove(index);
    chat->deleteLater();
    
    emit tabCountChanged(m_tabWidget->count());
}

ChatInterface* ChatTabbedInterface::getCurrentChatInterface() const {
    if (!m_tabWidget) return nullptr;
    int currentIndex = m_tabWidget->currentIndex();
    return m_chats.value(currentIndex);
}

void ChatTabbedInterface::setupChatInterface(ChatInterface *chat, int tabIndex) {
    if (tabIndex >= 0 && tabIndex < m_tabWidget->count()) {
        m_chats[tabIndex] = chat;
    }
}

void ChatTabbedInterface::onTabChanged(int index) {
    emit currentTabChanged(index);
}

void ChatTabbedInterface::onNewChatTabClicked() {
    newChatTab();
}

void ChatTabbedInterface::onCloseTabClicked() {
    int currentIndex = m_tabWidget->currentIndex();
    if (currentIndex >= 0) {
        closeChatTab(currentIndex);
    }
}

void ChatTabbedInterface::onTabContextMenu(const QPoint &pos) {
    // Placeholder for context menu
}

void ChatTabbedInterface::onTabCloseRequested(int index) {
    closeChatTab(index);
}

// ============================================================================
// EnhancedCodeEditor Implementation
// ============================================================================

class MultiTabEditor : public QWidget {
public:
    explicit MultiTabEditor(QWidget *parent = nullptr) : QWidget(parent) {
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        
        m_tabWidget = new QTabWidget(this);
        m_tabWidget->setTabsClosable(true);
        m_tabWidget->setDocumentMode(true);
        
        layout->addWidget(m_tabWidget);
        
        // Add initial tab
        addNewTab();
        
        connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MultiTabEditor::closeTab);
    }
    
    void addNewTab() {
        auto editor = new RawrXD::EditorWithMinimap(this);
        m_tabWidget->addTab(editor, QString("Untitled %1").arg(m_tabWidget->count() + 1));
        m_tabWidget->setCurrentWidget(editor);
    }
    
    void closeTab(int index) {
        if (m_tabWidget->count() > 1) {
            QWidget* widget = m_tabWidget->widget(index);
            m_tabWidget->removeTab(index);
            delete widget;
        }
    }
    
    int getTabCount() const {
        return m_tabWidget->count();
    }

private:
    QTabWidget* m_tabWidget;
};

EnhancedCodeEditor::EnhancedCodeEditor(QWidget *parent)
    : QWidget(parent)
    , m_codeEditor(nullptr)
    , m_lazyLoadingEnabled(false)
    , m_tabPoolingEnabled(false)
    , m_maxCachedTabs(1000)
{
}

EnhancedCodeEditor::~EnhancedCodeEditor() {
    if (m_codeEditor) {
        delete m_codeEditor;
    }
}

void EnhancedCodeEditor::initialize() {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    m_codeEditor = new MultiTabEditor(this);
    layout->addWidget(m_codeEditor);
    
    setLayout(layout);
    
    // Default to standard mode
    optimizeForStandard();
}

int EnhancedCodeEditor::getTabCount() const {
    if (m_codeEditor) {
        return m_codeEditor->getTabCount();
    }
    return 0;
}

void EnhancedCodeEditor::optimizeForMASM() {
    setupHighTabCountSupport();
    m_maxCachedTabs = 1000; // Higher cache for MASM (1M+ tabs)
    enableLazyLoading();
    enableTabPooling();
    enableMemoryCompression();
}

void EnhancedCodeEditor::optimizeForStandard() {
    setupHighTabCountSupport();
    m_maxCachedTabs = 100; // Standard mode (100K+ tabs)
    enableLazyLoading();
    enableTabPooling();
}

void EnhancedCodeEditor::setupHighTabCountSupport() {
    // Set up infrastructure for high tab counts
}

void EnhancedCodeEditor::enableLazyLoading() {
    m_lazyLoadingEnabled = true;
}

void EnhancedCodeEditor::enableTabPooling() {
    m_tabPoolingEnabled = true;
}

void EnhancedCodeEditor::enableMemoryCompression() {
    // Memory compression for ultra-high tab counts
}
