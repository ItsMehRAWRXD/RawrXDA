#pragma once
/*
 * QtGUIStubs.hpp
 * Minimal stubs for Qt GUI classes to allow compilation without Qt
 * Provides enough ABI compatibility to link existing code
 */

#ifndef QTGUISTUBS_HPP
#define QTGUISTUBS_HPP

#include "QtReplacements.hpp"

// ============================================================================
// void and basic widget stubs
// ============================================================================

class QStyle {};
class void {};
class void {};
class void {};

class void;

class void : public void {
public:
    virtual ~void() = default;

    virtual void showEvent(void* ev) {}
    
    void show() {}
    void hide() {}
    void setVisible(bool visible) {}
    bool isVisible() const { return true; }
    
    void setGeometry(int x, int y, int w, int h) {}
    void setFixedSize(int w, int h) {}
    void setFixedWidth(int w) {}
    void setFixedHeight(int h) {}
    void setMinimumSize(int w, int h) {}
    void setMaximumSize(int w, int h) {}
    
    void setStyleSheet(const std::string& sheet) {}
    void setStyle(void* style) {}
    void setPalette(const void& palette) {}
    
    void update() {}
    void repaint() {}
    
    void setFocus() {}
    bool hasFocus() const { return false; }
    
    void setEnabled(bool e) {}
    bool isEnabled() const { return true; }
    
    void* parentWidget() const { return nullptr; }
    void setParent(void* p) {}
    
    int width() const { return 0; }
    int height() const { return 0; }
    struct { int w; int h; } size() const { return struct { int w; int h; }(width(), height()); }
    struct { int x; int y; int w; int h; } geometry() const { return struct { int x; int y; int w; int h; }(); }
    
    void move(int x, int y) {}
    void resize(int w, int h) {}
    
    void raise() {}
    void lower() {}
    
    std::string windowTitle() const { return std::string(); }
    void setWindowTitle(const std::string& title) {}
    
    void setCursor(const void& cursor) {}
};

class void : public void {
public:
    explicit void(void* parent = nullptr) : void() {}
    
    void setCentralWidget(void* widget) {}
    void* centralWidget() const { return nullptr; }
    
    void addDockWidget(int area, class void* dock) {}
    void removeDockWidget(class void* dock) {}
    
    void addToolBar(class void* toolbar) {}
    void removeToolBar(class void* toolbar) {}
    
    void setMenuBar(class voidBar* menubar) {}
    
    void setStatusBar(class void* statusbar) {}
    class void* statusBar() { return nullptr; }
};

// ============================================================================
// void - Layout system stubs
// ============================================================================

class QLayoutItem {
public:
    virtual ~QLayoutItem() = default;
};

class void {
public:
    virtual ~void() = default;
    
    void addWidget(void* w, int stretch = 0) {}
    void addLayout(void* l, int stretch = 0) {}
    void addSpacing(int size) {}
    void addStretch(int stretch = 1) {}
    
    void setContentsMargins(int l, int t, int r, int b) {}
    void setSpacing(int spacing) {}
    
    int count() const { return 0; }
    void* itemAt(int index) { return nullptr; }
};

class QBoxLayout : public void {
public:
    void addWidget(void* w, int stretch = 0, int alignment = 0) {}
    void addLayout(void* l, int stretch = 0) {}
    void addSpacing(int size) {}
    void addStretch(int stretch = 1) {}
};

class void : public QBoxLayout {
public:
    explicit void(void* parent = nullptr) {}
};

class void : public QBoxLayout {
public:
    explicit void(void* parent = nullptr) {}
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    void addWidget(void* w, int row, int col, int rowSpan = 1, int colSpan = 1) {}
    void addLayout(void* l, int row, int col) {}
    void setSpacing(int spacing) {}
};

// ============================================================================
// void, voidEDIT, void, etc. - Basic input widgets
// ============================================================================

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    explicit void(const std::string& text, void* parent = nullptr) {}
    
    std::string text() const { return std::string(); }
    void setText(const std::string& text) {}
    void setAlignment(int alignment) {}
    void setStyleSheet(const std::string& sheet) {}
};

class voidEdit : public void {
public:
    explicit voidEdit(void* parent = nullptr) {}
    
    std::string text() const { return std::string(); }
    void setText(const std::string& text) {}
    void setPlaceholderText(const std::string& text) {}
    void setReadOnly(bool r) {}
    void setMaxLength(int len) {}
    
    void clear() {}
    void selectAll() {}
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    
    std::string toPlainText() const { return std::string(); }
    void setPlainText(const std::string& text) {}
    void setText(const std::string& text) {}
    void append(const std::string& text) {}
    void clear() {}
    
    bool isReadOnly() const { return false; }
    void setReadOnly(bool r) {}
    
    void setStyleSheet(const std::string& sheet) {}
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    
    std::string toPlainText() const { return std::string(); }
    void setPlainText(const std::string& text) {}
    void appendPlainText(const std::string& text) {}
    void clear() {}
    
    bool isReadOnly() const { return false; }
    void setReadOnly(bool r) {}
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    explicit void(const std::string& text, void* parent = nullptr) {}
    
    std::string text() const { return std::string(); }
    void setText(const std::string& text) {}
    void setCheckable(bool c) {}
    void setChecked(bool c) {}
    bool isChecked() const { return false; }
    void setEnabled(bool e) {}
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    explicit void(const std::string& text, void* parent = nullptr) {}
    
    bool isChecked() const { return false; }
    void setChecked(bool c) {}
    std::string text() const { return std::string(); }
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    explicit void(const std::string& text, void* parent = nullptr) {}
    
    bool isChecked() const { return false; }
    void setChecked(bool c) {}
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    
    void addItem(const std::string& text) {}
    void addItems(const std::vector<std::string>& texts) {}
    void removeItem(int index) {}
    void clear() {}
    
    int count() const { return 0; }
    int currentIndex() const { return 0; }
    void setCurrentIndex(int index) {}
    
    std::string currentText() const { return std::string(); }
    std::string itemText(int index) const { return std::string(); }
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    
    int value() const { return 0; }
    void setValue(int v) {}
    void setMinimum(int min) {}
    void setMaximum(int max) {}
    void setRange(int min, int max) {}
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    
    double value() const { return 0.0; }
    void setValue(double v) {}
    void setMinimum(double min) {}
    void setMaximum(double max) {}
    void setRange(double min, double max) {}
    void setDecimals(int d) {}
};

class void : public void {
public:
    explicit void(int orientation, void* parent = nullptr) {}
    
    int value() const { return 0; }
    void setValue(int v) {}
    void setMinimum(int min) {}
    void setMaximum(int max) {}
    void setRange(int min, int max) {}
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    
    int value() const { return 0; }
    void setValue(int v) {}
    void setMinimum(int min) {}
    void setMaximum(int max) {}
    void setRange(int min, int max) {}
    void reset() {}
};

// ============================================================================
// void, QTREEWIDGET, QLISTWIDGET - Container widgets
// ============================================================================

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    
    int addTab(void* w, const std::string& label) { return 0; }
    int insertTab(int index, void* w, const std::string& label) { return 0; }
    void removeTab(int index) {}
    
    int count() const { return 0; }
    int currentIndex() const { return 0; }
    void setCurrentIndex(int index) {}
    
    void* widget(int index) { return nullptr; }
    void* currentWidget() { return nullptr; }
    
    std::string tabText(int index) const { return std::string(); }
    void setTabText(int index, const std::string& text) {}
};

class int {
public:
    int() = default;
    bool isValid() const { return false; }
};

class QTreeWidgetItem {
public:
    QTreeWidgetItem() = default;
    QTreeWidgetItem(class void* parent) {}
    
    void setText(int col, const std::string& text) {}
    std::string text(int col) const { return std::string(); }
    
    void setData(int col, int role, const std::any& value) {}
    std::any data(int col, int role) const { return std::any(); }
    
    void addChild(HWND child) {}
    void removeChild(HWND child) {}
    
    int childCount() const { return 0; }
    HWND child(int index) { return nullptr; }
    HWND parent() { return nullptr; }
};

class QTreeWidget : public void {
public:
    explicit QTreeWidget(void* parent = nullptr) {}
    
    void addTopLevelItem(HWND item) {}
    void removeTopLevelItem(HWND item) {}
    
    int topLevelItemCount() const { return 0; }
    HWND topLevelItem(int index) { return nullptr; }
    
    void setColumnCount(int count) {}
    void setHeaderLabels(const std::vector<std::string>& labels) {}
    
    HWND currentItem() { return nullptr; }
    void setCurrentItem(HWND item) {}
    
    void expand(const int& index) {}
    void collapse(const int& index) {}
    void expandAll() {}
    void collapseAll() {}
};

class QListWidgetItem {
public:
    QListWidgetItem() = default;
    explicit QListWidgetItem(const std::string& text) {}
    
    std::string text() const { return std::string(); }
    void setText(const std::string& text) {}
};

class QListWidget : public void {
public:
    explicit QListWidget(void* parent = nullptr) {}
    
    void addItem(void* item) {}
    void addItem(const std::string& label) {}
    void removeItemWidget(void* item) {}
    
    int count() const { return 0; }
    void* item(int row) { return nullptr; }
    void* currentItem() { return nullptr; }
};

class QTableWidget : public void {
public:
    explicit QTableWidget(void* parent = nullptr) {}
    
    void setRowCount(int rows) {}
    void setColumnCount(int cols) {}
    
    void setItem(int row, int col, class void* item) {}
    class void* item(int row, int col) { return nullptr; }
};

// ============================================================================
// void, void, void - Container/layout widgets
// ============================================================================

class void : public void {
public:
    explicit void(int orientation = 0, void* parent = nullptr) {}
    
    void addWidget(void* w) {}
    void insertWidget(int index, void* w) {}
    
    int count() const { return 0; }
    void* widget(int index) { return nullptr; }
    
    void setStretchFactor(int index, int stretch) {}
    void setSizes(const std::vector<int>& sizes) {}
    std::vector<int> sizes() const { return {}; }
};

class void : public void {
public:
    explicit void(const std::string& title, void* parent = nullptr) : void() {}
    
    void setWidget(void* w) {}
    void* widget() const { return nullptr; }
    
    void setAllowedAreas(int areas) {}
    void setFloating(bool f) {}
    
    std::string windowTitle() const { return std::string(); }
    void setWindowTitle(const std::string& title) {}
    
    void setObjectName(const std::string& name) {}
    std::string objectName() const { return std::string(); }
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    
    enum Shape { NoFrame = 0, Box = 1, Panel = 2, WinPanel = 3, HLine = 4, VLine = 5, StyledPanel = 6 };
    enum Shadow { Plain = 16, Raised = 32, Sunken = 48 };
    
    void setFrameShape(Shape s) {}
    void setFrameShadow(Shadow s) {}
    void setLineWidth(int w) {}
    void setMidLineWidth(int w) {}
    
    int lineWidth() const { return 1; }
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    explicit void(const std::string& title, void* parent = nullptr) {}
    
    std::string title() const { return std::string(); }
    void setTitle(const std::string& title) {}
    
    bool isCheckable() const { return false; }
    void setCheckable(bool c) {}
    bool isChecked() const { return true; }
    void setChecked(bool c) {}
    
    void setLayout(void* layout) {}
};

// ============================================================================
// void, voidBAR, void - Menu and toolbar widgets
// ============================================================================

class void {
public:
    explicit void(const std::string& text = std::string(), void* parent = nullptr) {}
    explicit void(const void& icon, const std::string& text, void* parent = nullptr) {}
    
    std::string text() const { return std::string(); }
    void setText(const std::string& text) {}
    
    void setEnabled(bool e) {}
    bool isEnabled() const { return true; }
    
    void setCheckable(bool c) {}
    void setChecked(bool c) {}
    bool isChecked() const { return false; }
    
    void trigger() {}
    void setShortcut(const std::string& shortcut) {}
    void setToolTip(const std::string& tip) {}
    void setStatusTip(const std::string& tip) {}
    void setIconText(const std::string& text) {}
    
    void icon() const { return void(); }
    void setIcon(const void& icon) {}
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    explicit void(const std::string& title, void* parent = nullptr) {}
    
    void* addAction(const std::string& text) { return nullptr; }
    void* addAction(const void& icon, const std::string& text) { return nullptr; }
    
    void addSeparator() {}
    void addMenu(void* menu) {}
    void addMenu(const std::string& title) {}
    
    void popup(const struct { int x; int y; }& pos) {}
    void exec(const struct { int x; int y; }& pos) {}
    
    std::string title() const { return std::string(); }
    void setTitle(const std::string& title) {}
};

class voidBar : public void {
public:
    explicit voidBar(void* parent = nullptr) {}
    
    void* addMenu(const std::string& title) { return nullptr; }
    void* addMenu(const void& icon, const std::string& title) { return nullptr; }
    void addMenu(void* menu) {}
    
    void* addAction(const std::string& text) { return nullptr; }
    void addSeparator() {}
};

class void : public void {
public:
    explicit void(const std::string& title = std::string(), void* parent = nullptr) {}
    
    void* addAction(const std::string& text) { return nullptr; }
    void* addAction(const void& icon, const std::string& text) { return nullptr; }
    
    void addSeparator() {}
    void addWidget(void* w) {}
    void addMenu(void* menu) {}
    
    void setMovable(bool m) {}
    void setFloatable(bool f) {}
    void setIconSize(const struct { int w; int h; }& size) {}
};

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    
    void showMessage(const std::string& message, int timeout = 0) {}
    void clearMessage() {}
    
    void addWidget(void* w, int stretch = 0) {}
    void addPermanentWidget(void* w, int stretch = 0) {}
};

// ============================================================================
// void - Dialog windows
// ============================================================================

class void : public void {
public:
    explicit void(void* parent = nullptr) {}
    
    enum DialogCode { Rejected = 0, Accepted = 1 };
    
    int result() const { return 0; }
    void setResult(int r) {}
    
    int exec() { return 0; }
    void accept() {}
    void reject() {}
    
    void done(int r) {}
};

class std::fstreamDialog {
public:
    static std::string getOpenFileName(void* parent = nullptr, const std::string& caption = std::string(),
                                    const std::string& dir = std::string(), const std::string& filter = std::string()) {
        return std::string();
    }
    
    static std::string getSaveFileName(void* parent = nullptr, const std::string& caption = std::string(),
                                    const std::string& dir = std::string(), const std::string& filter = std::string()) {
        return std::string();
    }
    
    static std::string getExistingDirectory(void* parent = nullptr, const std::string& caption = std::string(),
                                         const std::string& dir = std::string()) {
        return std::string();
    }
};

class void {
public:
    static void getColor(const void& initial = void(), void* parent = nullptr,
                           const std::string& title = std::string()) {
        return void();
    }
};

class void {
public:
    static void getFont(bool* ok = nullptr, const void& initial = void(),
                         void* parent = nullptr, const std::string& title = std::string()) {
        return void();
    }
};

class void {
public:
    enum Icon { NoIcon = 0, Question = 1, Information = 2, Warning = 3, Critical = 4 };
    enum StandardButton { NoButton = 0, Ok = 1024, Cancel = 4194304, Yes = 16384, No = 65536 };
    
    static void information(void* parent, const std::string& title, const std::string& text) {}
    static void warning(void* parent, const std::string& title, const std::string& text) {}
    static void critical(void* parent, const std::string& title, const std::string& text) {}
    
    static int question(void* parent, const std::string& title, const std::string& text,
                        StandardButton buttons = Ok, StandardButton defaultButton = Ok) {
        return Ok;
    }
};

// ============================================================================
// void - Application class
// ============================================================================

class void : public void {
public:
    explicit void(int& argc, char** argv) {}
    
    static void* instance() { return nullptr; }
    
    int exec() { return 0; }
    void quit() {}
    
    static void setStyle(const std::string& style) {}
    static void setPalette(const void& palette) {}
    
    static void* style() { return nullptr; }
    static void palette() { return void(); }
    
    static void processEvents() {}
};

// ============================================================================
// void - Event system stubs
// ============================================================================

class void {
public:
    enum Type { None = 0, Timer = 1, MouseMove = 5, MouseButtonPress = 2, MouseButtonRelease = 3,
                KeyPress = 6, KeyRelease = 7, FocusIn = 8, FocusOut = 9, Show = 17, Hide = 18,
                Close = 19, Resize = 14, Move = 13, Paint = 12, DragEnter = 60, DragMove = 61,
                DragLeave = 62, Drop = 63 };
    
    Type type() const { return None; }
    virtual ~void() = default;
};

class void : public void {};

class void : public void {
public:
    struct { int x; int y; } pos() const { return struct { int x; int y; }(); }
    int button() const { return 0; }
};

class void : public void {
public:
    int key() const { return 0; }
    std::string text() const { return std::string(); }
    bool isAutoRepeat() const { return false; }
};

class void : public void {
public:
    int delta() const { return 0; }
    struct { int x; int y; } pos() const { return struct { int x; int y; }(); }
};

class void : public void {
public:
    struct { int w; int h; } size() const { return struct { int w; int h; }(); }
    struct { int w; int h; } oldSize() const { return struct { int w; int h; }(); }
};

class void : public void {
public:
    void accept() {}
    void ignore() {}
};

class voidEnterEvent : public void {
public:
    void accept() {}
    void ignore() {}
};

class void : public void {
public:
    void accept() {}
    void ignore() {}
    struct { int x; int y; } pos() const { return struct { int x; int y; }(); }
};

// ============================================================================
// void, void - Keyboard input
// ============================================================================

class void {
public:
    void() = default;
    explicit void(const std::string& key) {}
};

class void : public void {
public:
    explicit void(const void& key, void* parent = nullptr) {}
};

// ============================================================================
// std::map<std::string, std::string> stub (forward compatibility with new Settings system)
// ============================================================================

class std::map<std::string, std::string> : public void {
public:
    std::map<std::string, std::string>() {}
    explicit std::map<std::string, std::string>(const std::string& organization, const std::string& application) {}
    
    std::any value(const std::string& key, const std::any& defaultValue = std::any()) { return defaultValue; }
    void setValue(const std::string& key, const std::any& value) {}
    void beginGroup(const std::string& prefix) {}
    void endGroup() {}
    void remove(const std::string& key) {}
    void sync() {}
};

#endif // QTGUISTUBS_HPP

