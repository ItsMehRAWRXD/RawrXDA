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
// QWIDGET and basic widget stubs
// ============================================================================

class QStyle {};
class QPalette {};
class QIcon {};
class QCursor {};

class QWidget : public QObject {
public:
    virtual ~QWidget() = default;
    
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
    
    void setStyleSheet(const QString& sheet) {}
    void setStyle(QStyle* style) {}
    void setPalette(const QPalette& palette) {}
    
    void update() {}
    void repaint() {}
    
    void setFocus() {}
    bool hasFocus() const { return false; }
    
    void setEnabled(bool e) {}
    bool isEnabled() const { return true; }
    
    QWidget* parentWidget() const { return nullptr; }
    void setParent(QWidget* p) {}
    
    int width() const { return 0; }
    int height() const { return 0; }
    QSize size() const { return QSize(width(), height()); }
    QRect geometry() const { return QRect(); }
    
    void move(int x, int y) {}
    void resize(int w, int h) {}
    
    void raise() {}
    void lower() {}
    
    QString windowTitle() const { return QString(); }
    void setWindowTitle(const QString& title) {}
    
    void setCursor(const QCursor& cursor) {}
};

class QMainWindow : public QWidget {
public:
    explicit QMainWindow(QWidget* parent = nullptr) : QWidget() {}
    
    void setCentralWidget(QWidget* widget) {}
    QWidget* centralWidget() const { return nullptr; }
    
    void addDockWidget(int area, class QDockWidget* dock) {}
    void removeDockWidget(class QDockWidget* dock) {}
    
    void addToolBar(class QToolBar* toolbar) {}
    void removeToolBar(class QToolBar* toolbar) {}
    
    void setMenuBar(class QMenuBar* menubar) {}
    
    void setStatusBar(class QStatusBar* statusbar) {}
    class QStatusBar* statusBar() { return nullptr; }
};

// ============================================================================
// QLAYOUT - Layout system stubs
// ============================================================================

class QLayoutItem {
public:
    virtual ~QLayoutItem() = default;
};

class QLayout {
public:
    virtual ~QLayout() = default;
    
    void addWidget(QWidget* w, int stretch = 0) {}
    void addLayout(QLayout* l, int stretch = 0) {}
    void addSpacing(int size) {}
    void addStretch(int stretch = 1) {}
    
    void setContentsMargins(int l, int t, int r, int b) {}
    void setSpacing(int spacing) {}
    
    int count() const { return 0; }
    QLayoutItem* itemAt(int index) { return nullptr; }
};

class QBoxLayout : public QLayout {
public:
    void addWidget(QWidget* w, int stretch = 0, int alignment = 0) {}
    void addLayout(QLayout* l, int stretch = 0) {}
    void addSpacing(int size) {}
    void addStretch(int stretch = 1) {}
};

class QVBoxLayout : public QBoxLayout {
public:
    explicit QVBoxLayout(QWidget* parent = nullptr) {}
};

class QHBoxLayout : public QBoxLayout {
public:
    explicit QHBoxLayout(QWidget* parent = nullptr) {}
};

class QGridLayout : public QLayout {
public:
    explicit QGridLayout(QWidget* parent = nullptr) {}
    void addWidget(QWidget* w, int row, int col, int rowSpan = 1, int colSpan = 1) {}
    void addLayout(QLayout* l, int row, int col) {}
    void setSpacing(int spacing) {}
};

// ============================================================================
// QLABEL, QLINEEDIT, QPUSHBUTTON, etc. - Basic input widgets
// ============================================================================

class QLabel : public QWidget {
public:
    explicit QLabel(QWidget* parent = nullptr) {}
    explicit QLabel(const QString& text, QWidget* parent = nullptr) {}
    
    QString text() const { return QString(); }
    void setText(const QString& text) {}
    void setAlignment(int alignment) {}
    void setStyleSheet(const QString& sheet) {}
};

class QLineEdit : public QWidget {
public:
    explicit QLineEdit(QWidget* parent = nullptr) {}
    
    QString text() const { return QString(); }
    void setText(const QString& text) {}
    void setPlaceholderText(const QString& text) {}
    void setReadOnly(bool r) {}
    void setMaxLength(int len) {}
    
    void clear() {}
    void selectAll() {}
};

class QTextEdit : public QWidget {
public:
    explicit QTextEdit(QWidget* parent = nullptr) {}
    
    QString toPlainText() const { return QString(); }
    void setPlainText(const QString& text) {}
    void setText(const QString& text) {}
    void append(const QString& text) {}
    void clear() {}
    
    bool isReadOnly() const { return false; }
    void setReadOnly(bool r) {}
    
    void setStyleSheet(const QString& sheet) {}
};

class QPlainTextEdit : public QWidget {
public:
    explicit QPlainTextEdit(QWidget* parent = nullptr) {}
    
    QString toPlainText() const { return QString(); }
    void setPlainText(const QString& text) {}
    void appendPlainText(const QString& text) {}
    void clear() {}
    
    bool isReadOnly() const { return false; }
    void setReadOnly(bool r) {}
};

class QPushButton : public QWidget {
public:
    explicit QPushButton(QWidget* parent = nullptr) {}
    explicit QPushButton(const QString& text, QWidget* parent = nullptr) {}
    
    QString text() const { return QString(); }
    void setText(const QString& text) {}
    void setCheckable(bool c) {}
    void setChecked(bool c) {}
    bool isChecked() const { return false; }
    void setEnabled(bool e) {}
};

class QCheckBox : public QWidget {
public:
    explicit QCheckBox(QWidget* parent = nullptr) {}
    explicit QCheckBox(const QString& text, QWidget* parent = nullptr) {}
    
    bool isChecked() const { return false; }
    void setChecked(bool c) {}
    QString text() const { return QString(); }
};

class QRadioButton : public QWidget {
public:
    explicit QRadioButton(QWidget* parent = nullptr) {}
    explicit QRadioButton(const QString& text, QWidget* parent = nullptr) {}
    
    bool isChecked() const { return false; }
    void setChecked(bool c) {}
};

class QComboBox : public QWidget {
public:
    explicit QComboBox(QWidget* parent = nullptr) {}
    
    void addItem(const QString& text) {}
    void addItems(const QStringList& texts) {}
    void removeItem(int index) {}
    void clear() {}
    
    int count() const { return 0; }
    int currentIndex() const { return 0; }
    void setCurrentIndex(int index) {}
    
    QString currentText() const { return QString(); }
    QString itemText(int index) const { return QString(); }
};

class QSpinBox : public QWidget {
public:
    explicit QSpinBox(QWidget* parent = nullptr) {}
    
    int value() const { return 0; }
    void setValue(int v) {}
    void setMinimum(int min) {}
    void setMaximum(int max) {}
    void setRange(int min, int max) {}
};

class QDoubleSpinBox : public QWidget {
public:
    explicit QDoubleSpinBox(QWidget* parent = nullptr) {}
    
    double value() const { return 0.0; }
    void setValue(double v) {}
    void setMinimum(double min) {}
    void setMaximum(double max) {}
    void setRange(double min, double max) {}
    void setDecimals(int d) {}
};

class QSlider : public QWidget {
public:
    explicit QSlider(int orientation, QWidget* parent = nullptr) {}
    
    int value() const { return 0; }
    void setValue(int v) {}
    void setMinimum(int min) {}
    void setMaximum(int max) {}
    void setRange(int min, int max) {}
};

class QProgressBar : public QWidget {
public:
    explicit QProgressBar(QWidget* parent = nullptr) {}
    
    int value() const { return 0; }
    void setValue(int v) {}
    void setMinimum(int min) {}
    void setMaximum(int max) {}
    void setRange(int min, int max) {}
    void reset() {}
};

// ============================================================================
// QTABWIDGET, QTREEWIDGET, QLISTWIDGET - Container widgets
// ============================================================================

class QTabWidget : public QWidget {
public:
    explicit QTabWidget(QWidget* parent = nullptr) {}
    
    int addTab(QWidget* w, const QString& label) { return 0; }
    int insertTab(int index, QWidget* w, const QString& label) { return 0; }
    void removeTab(int index) {}
    
    int count() const { return 0; }
    int currentIndex() const { return 0; }
    void setCurrentIndex(int index) {}
    
    QWidget* widget(int index) { return nullptr; }
    QWidget* currentWidget() { return nullptr; }
    
    QString tabText(int index) const { return QString(); }
    void setTabText(int index, const QString& text) {}
};

class QTreeWidgetItem {
public:
    QTreeWidgetItem() = default;
    QTreeWidgetItem(class QTreeWidget* parent) {}
    
    void setText(int col, const QString& text) {}
    QString text(int col) const { return QString(); }
    
    void setData(int col, int role, const QVariant& value) {}
    QVariant data(int col, int role) const { return QVariant(); }
    
    void addChild(QTreeWidgetItem* child) {}
    void removeChild(QTreeWidgetItem* child) {}
    
    int childCount() const { return 0; }
    QTreeWidgetItem* child(int index) { return nullptr; }
    QTreeWidgetItem* parent() { return nullptr; }
};

class QTreeWidget : public QWidget {
public:
    explicit QTreeWidget(QWidget* parent = nullptr) {}
    
    void addTopLevelItem(QTreeWidgetItem* item) {}
    void removeTopLevelItem(QTreeWidgetItem* item) {}
    
    int topLevelItemCount() const { return 0; }
    QTreeWidgetItem* topLevelItem(int index) { return nullptr; }
    
    void setColumnCount(int count) {}
    void setHeaderLabels(const QStringList& labels) {}
    
    QTreeWidgetItem* currentItem() { return nullptr; }
    void setCurrentItem(QTreeWidgetItem* item) {}
    
    void expand(const QModelIndex& index) {}
    void collapse(const QModelIndex& index) {}
    void expandAll() {}
    void collapseAll() {}
};

class QListWidgetItem {
public:
    QListWidgetItem() = default;
    explicit QListWidgetItem(const QString& text) {}
    
    QString text() const { return QString(); }
    void setText(const QString& text) {}
};

class QListWidget : public QWidget {
public:
    explicit QListWidget(QWidget* parent = nullptr) {}
    
    void addItem(QListWidgetItem* item) {}
    void addItem(const QString& label) {}
    void removeItemWidget(QListWidgetItem* item) {}
    
    int count() const { return 0; }
    QListWidgetItem* item(int row) { return nullptr; }
    QListWidgetItem* currentItem() { return nullptr; }
};

class QTableWidget : public QWidget {
public:
    explicit QTableWidget(QWidget* parent = nullptr) {}
    
    void setRowCount(int rows) {}
    void setColumnCount(int cols) {}
    
    void setItem(int row, int col, class QTableWidgetItem* item) {}
    class QTableWidgetItem* item(int row, int col) { return nullptr; }
};

// ============================================================================
// QSPLITTER, QDOCKWIDGET, QFRAME - Container/layout widgets
// ============================================================================

class QSplitter : public QWidget {
public:
    explicit QSplitter(int orientation = 0, QWidget* parent = nullptr) {}
    
    void addWidget(QWidget* w) {}
    void insertWidget(int index, QWidget* w) {}
    
    int count() const { return 0; }
    QWidget* widget(int index) { return nullptr; }
    
    void setStretchFactor(int index, int stretch) {}
    void setSizes(const std::vector<int>& sizes) {}
    std::vector<int> sizes() const { return {}; }
};

class QDockWidget : public QWidget {
public:
    explicit QDockWidget(const QString& title, QWidget* parent = nullptr) : QWidget() {}
    
    void setWidget(QWidget* w) {}
    QWidget* widget() const { return nullptr; }
    
    void setAllowedAreas(int areas) {}
    void setFloating(bool f) {}
    
    QString windowTitle() const { return QString(); }
    void setWindowTitle(const QString& title) {}
    
    void setObjectName(const QString& name) {}
    QString objectName() const { return QString(); }
};

class QFrame : public QWidget {
public:
    explicit QFrame(QWidget* parent = nullptr) {}
    
    enum Shape { NoFrame = 0, Box = 1, Panel = 2, WinPanel = 3, HLine = 4, VLine = 5, StyledPanel = 6 };
    enum Shadow { Plain = 16, Raised = 32, Sunken = 48 };
    
    void setFrameShape(Shape s) {}
    void setFrameShadow(Shadow s) {}
    void setLineWidth(int w) {}
    void setMidLineWidth(int w) {}
    
    int lineWidth() const { return 1; }
};

class QGroupBox : public QWidget {
public:
    explicit QGroupBox(QWidget* parent = nullptr) {}
    explicit QGroupBox(const QString& title, QWidget* parent = nullptr) {}
    
    QString title() const { return QString(); }
    void setTitle(const QString& title) {}
    
    bool isCheckable() const { return false; }
    void setCheckable(bool c) {}
    bool isChecked() const { return true; }
    void setChecked(bool c) {}
    
    void setLayout(QLayout* layout) {}
};

// ============================================================================
// QMENU, QMENUBAR, QTOOLBAR - Menu and toolbar widgets
// ============================================================================

class QAction {
public:
    explicit QAction(const QString& text = QString(), QObject* parent = nullptr) {}
    explicit QAction(const QIcon& icon, const QString& text, QObject* parent = nullptr) {}
    
    QString text() const { return QString(); }
    void setText(const QString& text) {}
    
    void setEnabled(bool e) {}
    bool isEnabled() const { return true; }
    
    void setCheckable(bool c) {}
    void setChecked(bool c) {}
    bool isChecked() const { return false; }
    
    void trigger() {}
    void setShortcut(const QString& shortcut) {}
    void setToolTip(const QString& tip) {}
    void setStatusTip(const QString& tip) {}
    void setIconText(const QString& text) {}
    
    QIcon icon() const { return QIcon(); }
    void setIcon(const QIcon& icon) {}
};

class QMenu : public QWidget {
public:
    explicit QMenu(QWidget* parent = nullptr) {}
    explicit QMenu(const QString& title, QWidget* parent = nullptr) {}
    
    QAction* addAction(const QString& text) { return nullptr; }
    QAction* addAction(const QIcon& icon, const QString& text) { return nullptr; }
    
    void addSeparator() {}
    void addMenu(QMenu* menu) {}
    void addMenu(const QString& title) {}
    
    void popup(const QPoint& pos) {}
    void exec(const QPoint& pos) {}
    
    QString title() const { return QString(); }
    void setTitle(const QString& title) {}
};

class QMenuBar : public QWidget {
public:
    explicit QMenuBar(QWidget* parent = nullptr) {}
    
    QMenu* addMenu(const QString& title) { return nullptr; }
    QMenu* addMenu(const QIcon& icon, const QString& title) { return nullptr; }
    void addMenu(QMenu* menu) {}
    
    QAction* addAction(const QString& text) { return nullptr; }
    void addSeparator() {}
};

class QToolBar : public QWidget {
public:
    explicit QToolBar(const QString& title = QString(), QWidget* parent = nullptr) {}
    
    QAction* addAction(const QString& text) { return nullptr; }
    QAction* addAction(const QIcon& icon, const QString& text) { return nullptr; }
    
    void addSeparator() {}
    void addWidget(QWidget* w) {}
    void addMenu(QMenu* menu) {}
    
    void setMovable(bool m) {}
    void setFloatable(bool f) {}
    void setIconSize(const QSize& size) {}
};

class QStatusBar : public QWidget {
public:
    explicit QStatusBar(QWidget* parent = nullptr) {}
    
    void showMessage(const QString& message, int timeout = 0) {}
    void clearMessage() {}
    
    void addWidget(QWidget* w, int stretch = 0) {}
    void addPermanentWidget(QWidget* w, int stretch = 0) {}
};

// ============================================================================
// QDIALOG - Dialog windows
// ============================================================================

class QDialog : public QWidget {
public:
    explicit QDialog(QWidget* parent = nullptr) {}
    
    enum DialogCode { Rejected = 0, Accepted = 1 };
    
    int result() const { return 0; }
    void setResult(int r) {}
    
    int exec() { return 0; }
    void accept() {}
    void reject() {}
    
    void done(int r) {}
};

class QFileDialog {
public:
    static QString getOpenFileName(QWidget* parent = nullptr, const QString& caption = QString(),
                                    const QString& dir = QString(), const QString& filter = QString()) {
        return QString();
    }
    
    static QString getSaveFileName(QWidget* parent = nullptr, const QString& caption = QString(),
                                    const QString& dir = QString(), const QString& filter = QString()) {
        return QString();
    }
    
    static QString getExistingDirectory(QWidget* parent = nullptr, const QString& caption = QString(),
                                         const QString& dir = QString()) {
        return QString();
    }
};

class QColorDialog {
public:
    static QColor getColor(const QColor& initial = QColor(), QWidget* parent = nullptr,
                           const QString& title = QString()) {
        return QColor();
    }
};

class QFontDialog {
public:
    static QFont getFont(bool* ok = nullptr, const QFont& initial = QFont(),
                         QWidget* parent = nullptr, const QString& title = QString()) {
        return QFont();
    }
};

class QMessageBox {
public:
    enum Icon { NoIcon = 0, Question = 1, Information = 2, Warning = 3, Critical = 4 };
    enum StandardButton { NoButton = 0, Ok = 1024, Cancel = 4194304, Yes = 16384, No = 65536 };
    
    static void information(QWidget* parent, const QString& title, const QString& text) {}
    static void warning(QWidget* parent, const QString& title, const QString& text) {}
    static void critical(QWidget* parent, const QString& title, const QString& text) {}
    
    static int question(QWidget* parent, const QString& title, const QString& text,
                        StandardButton buttons = Ok, StandardButton defaultButton = Ok) {
        return Ok;
    }
};

// ============================================================================
// QAPPLICATION - Application class
// ============================================================================

class QApplication : public QObject {
public:
    explicit QApplication(int& argc, char** argv) {}
    
    static QApplication* instance() { return nullptr; }
    
    int exec() { return 0; }
    void quit() {}
    
    static void setStyle(const QString& style) {}
    static void setPalette(const QPalette& palette) {}
    
    static QStyle* style() { return nullptr; }
    static QPalette palette() { return QPalette(); }
    
    static void processEvents() {}
};

// ============================================================================
// QEVENT - Event system stubs
// ============================================================================

class QEvent {
public:
    enum Type { None = 0, Timer = 1, MouseMove = 5, MouseButtonPress = 2, MouseButtonRelease = 3,
                KeyPress = 6, KeyRelease = 7, FocusIn = 8, FocusOut = 9, Show = 17, Hide = 18,
                Close = 19, Resize = 14, Move = 13, Paint = 12, DragEnter = 60, DragMove = 61,
                DragLeave = 62, Drop = 63 };
    
    Type type() const { return None; }
    virtual ~QEvent() = default;
};

class QMouseEvent : public QEvent {
public:
    QPoint pos() const { return QPoint(); }
    int button() const { return 0; }
};

class QKeyEvent : public QEvent {
public:
    int key() const { return 0; }
    QString text() const { return QString(); }
    bool isAutoRepeat() const { return false; }
};

class QWheelEvent : public QEvent {
public:
    int delta() const { return 0; }
    QPoint pos() const { return QPoint(); }
};

class QResizeEvent : public QEvent {
public:
    QSize size() const { return QSize(); }
    QSize oldSize() const { return QSize(); }
};

class QCloseEvent : public QEvent {
public:
    void accept() {}
    void ignore() {}
};

class QDragEnterEvent : public QEvent {
public:
    void accept() {}
    void ignore() {}
};

class QDropEvent : public QEvent {
public:
    void accept() {}
    void ignore() {}
    QPoint pos() const { return QPoint(); }
};

// ============================================================================
// QSHORTCUT, QKEYSEQUENCE - Keyboard input
// ============================================================================

class QKeySequence {
public:
    QKeySequence() = default;
    explicit QKeySequence(const QString& key) {}
};

class QShortcut : public QObject {
public:
    explicit QShortcut(const QKeySequence& key, QWidget* parent = nullptr) {}
};

// ============================================================================
// QSETTINGS stub (forward compatibility with new Settings system)
// ============================================================================

class QSettings : public QObject {
public:
    QSettings() {}
    explicit QSettings(const QString& organization, const QString& application) {}
    
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) { return defaultValue; }
    void setValue(const QString& key, const QVariant& value) {}
    void beginGroup(const QString& prefix) {}
    void endGroup() {}
    void remove(const QString& key) {}
    void sync() {}
};

#endif // QTGUISTUBS_HPP
