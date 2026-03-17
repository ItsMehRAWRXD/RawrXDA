// Win32UIStubs.hpp — Win32 + STL only. No third-party UI framework.
// Dialog/layout/control stubs for code that builds without any Qt dependency.
#pragma once

#include "win32_file_dialog.h"
#include <any>
#include <map>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// Base: Object, WindowBase, DialogBase
// ============================================================================

struct NativeStyle
{
};
struct NativeObject
{
};
struct WindowBase;

struct WindowBase
{
    virtual ~WindowBase() = default;
    virtual void showEvent(void* ev) { (void)ev; }
    void show() {}
    void hide() {}
    void setVisible(bool) {}
    bool isVisible() const { return true; }
    void setGeometry(int, int, int, int) {}
    void setFixedSize(int, int) {}
    void setFixedWidth(int) {}
    void setFixedHeight(int) {}
    void setMinimumSize(int, int) {}
    void setMaximumSize(int, int) {}
    void setStyleSheet(const std::string&) {}
    void setStyle(NativeStyle*) {}
    void setPalette(const void*) {}
    void update() {}
    void repaint() {}
    void setFocus() {}
    bool hasFocus() const { return false; }
    void setEnabled(bool) {}
    bool isEnabled() const { return true; }
    void* parentWidget() const { return nullptr; }
    void setParent(void*) {}
    int width() const { return 0; }
    int height() const { return 0; }
    std::pair<int, int> size() const { return {0, 0}; }
    std::pair<std::pair<int, int>, std::pair<int, int>> geometry() const { return {{0, 0}, {0, 0}}; }
    void move(int, int) {}
    void resize(int, int) {}
    void raise() {}
    void lower() {}
    std::string windowTitle() const { return {}; }
    void setWindowTitle(const std::string&) {}
    void setToolTip(const std::string&) {}
    void setCursor(const void*) {}
};

struct MainWindowBase : WindowBase
{
    explicit MainWindowBase(void* parent = nullptr) { (void)parent; }
    void setCentralWidget(void*) {}
    void* centralWidget() const { return nullptr; }
    void addDockWidget(int, void*) {}
    void removeDockWidget(void*) {}
    void addToolBar(void*) {}
    void removeToolBar(void*) {}
    void setMenuBar(void*) {}
    void setStatusBar(void*) {}
    void* statusBar() { return nullptr; }
};

// ============================================================================
// Layouts
// ============================================================================

struct LayoutItem
{
    virtual ~LayoutItem() = default;
};

struct Layout
{
    virtual ~Layout() = default;
    void addWidget(void*, int = 0) {}
    void addLayout(void*, int = 0) {}
    void addSpacing(int) {}
    void addStretch(int = 1) {}
    void setContentsMargins(int, int, int, int) {}
    void setSpacing(int) {}
    int count() const { return 0; }
    LayoutItem* itemAt(int) { return nullptr; }
};

struct BoxLayout : Layout
{
    void addWidget(void*, int = 0, int = 0) {}
    void addLayout(void*, int = 0) {}
    void addSpacing(int) {}
    void addStretch(int = 1) {}
};

struct VerticalLayout : BoxLayout
{
    explicit VerticalLayout(void* parent = nullptr) { (void)parent; }
};

struct HorizontalLayout : BoxLayout
{
    explicit HorizontalLayout(void* parent = nullptr) { (void)parent; }
};

struct GridLayout : Layout
{
    explicit GridLayout(void* parent = nullptr) { (void)parent; }
    void addWidget(void*, int row, int col, int rowSpan = 1, int colSpan = 1) {}
    void addLayout(void*, int row, int col) {}
    void setSpacing(int) {}
};

// ============================================================================
// Controls
// ============================================================================

struct Label : WindowBase
{
    explicit Label(void* parent = nullptr) { (void)parent; }
    explicit Label(const std::string& text, void* parent = nullptr)
    {
        (void)text;
        (void)parent;
    }
    std::string text() const { return {}; }
    void setText(const std::string&) {}
    void setAlignment(int) {}
    void setStyleSheet(const std::string&) {}
};

struct LineEdit : WindowBase
{
    explicit LineEdit(void* parent = nullptr) { (void)parent; }
    std::string text() const { return {}; }
    void setText(const std::string&) {}
    void setPlaceholderText(const std::string&) {}
    void setReadOnly(bool) {}
    void setMaxLength(int) {}
    void clear() {}
    void selectAll() {}
};

struct PushButton : WindowBase
{
    explicit PushButton(void* parent = nullptr) { (void)parent; }
    explicit PushButton(const std::string& text, void* parent = nullptr)
    {
        (void)text;
        (void)parent;
    }
    std::string text() const { return {}; }
    void setText(const std::string&) {}
    void setCheckable(bool) {}
    void setChecked(bool) {}
    bool isChecked() const { return false; }
    void setEnabled(bool) {}
};

struct CheckBox : WindowBase
{
    explicit CheckBox(void* parent = nullptr) { (void)parent; }
    explicit CheckBox(const std::string& text, void* parent = nullptr)
    {
        (void)text;
        (void)parent;
    }
    bool isChecked() const { return false; }
    void setChecked(bool) {}
    std::string text() const { return {}; }
};

struct SimpleVariant
{
    SimpleVariant() : m_int(0), m_hasInt(false), m_hasDouble(false), m_hasBool(false), m_hasString(false) {}
    explicit SimpleVariant(int v) : m_int(v), m_hasInt(true), m_hasDouble(false), m_hasBool(false), m_hasString(false)
    {
    }
    explicit SimpleVariant(double v)
        : m_double(v), m_hasInt(false), m_hasDouble(true), m_hasBool(false), m_hasString(false)
    {
    }
    explicit SimpleVariant(bool v) : m_bool(v), m_hasInt(false), m_hasDouble(false), m_hasBool(true), m_hasString(false)
    {
    }
    explicit SimpleVariant(const std::string& v)
        : m_string(v), m_hasInt(false), m_hasDouble(false), m_hasBool(false), m_hasString(true)
    {
    }
    int toInt() const { return m_hasInt ? m_int : (m_hasDouble ? (int)m_double : 0); }
    double toDouble() const { return m_hasDouble ? m_double : (m_hasInt ? (double)m_int : 0.0); }
    bool toBool() const { return m_hasBool ? m_bool : (m_hasInt ? m_int != 0 : false); }
    std::string toString() const
    {
        return m_hasString ? m_string
                           : (m_hasInt ? std::to_string(m_int) : (m_hasDouble ? std::to_string(m_double) : ""));
    }

  private:
    int m_int;
    double m_double;
    bool m_bool;
    std::string m_string;
    bool m_hasInt, m_hasDouble, m_hasBool, m_hasString;
};

struct ComboBox : WindowBase
{
    explicit ComboBox(void* parent = nullptr) { (void)parent; }
    void addItem(const std::string&) {}
    void addItem(const std::string& text, int value)
    {
        (void)text;
        (void)value;
    }
    void addItems(const std::vector<std::string>&) {}
    void removeItem(int) {}
    void clear() {}
    int count() const { return 0; }
    int currentIndex() const { return 0; }
    void setCurrentIndex(int) {}
    std::string currentText() const { return {}; }
    std::string itemText(int) const { return {}; }
    SimpleVariant currentData() const { return SimpleVariant(currentIndex()); }
};

struct SpinBox : WindowBase
{
    explicit SpinBox(void* parent = nullptr) { (void)parent; }
    int value() const { return 0; }
    void setValue(int) {}
    void setMinimum(int) {}
    void setMaximum(int) {}
    void setRange(int, int) {}
    void setSingleStep(int) {}
};

struct DoubleSpinBox : WindowBase
{
    explicit DoubleSpinBox(void* parent = nullptr) { (void)parent; }
    double value() const { return 0.0; }
    void setValue(double) {}
    void setMinimum(double) {}
    void setMaximum(double) {}
    void setRange(double, double) {}
    void setDecimals(int) {}
    void setSingleStep(double) {}
};

struct GroupBox : WindowBase
{
    explicit GroupBox(void* parent = nullptr) { (void)parent; }
    explicit GroupBox(const std::string& title, void* parent = nullptr)
    {
        (void)title;
        (void)parent;
    }
    std::string title() const { return {}; }
    void setTitle(const std::string&) {}
    bool isCheckable() const { return false; }
    void setCheckable(bool) {}
    bool isChecked() const { return true; }
    void setChecked(bool) {}
    void setLayout(void*) {}
};

// ============================================================================
// Dialog
// ============================================================================

struct DialogBase : WindowBase
{
    explicit DialogBase(void* parent = nullptr) { (void)parent; }
    enum DialogCode
    {
        Rejected = 0,
        Accepted = 1
    };
    int result() const { return 0; }
    void setResult(int) {}
    int exec() { return 0; }
    void accept() {}
    void reject() {}
    void done(int) {}
    void setModal(bool) {}
};

// File dialogs: use RawrXD::getOpenFileName, getSaveFileName, getExistingDirectory from win32_file_dialog.h

// ============================================================================
// Settings (in-memory key/value; use registry or file for persistence)
// ============================================================================

struct SimpleSettings
{
    SimpleSettings() = default;
    explicit SimpleSettings(const std::string& organization, const std::string& application)
    {
        (void)organization;
        (void)application;
    }
    SimpleVariant value(const std::string& key, const SimpleVariant& defaultValue = SimpleVariant()) const
    {
        std::string fullKey = m_prefix.empty() ? key : m_prefix + "/" + key;
        auto it = m_store.find(fullKey);
        return it != m_store.end() ? it->second : defaultValue;
    }
    SimpleVariant value(const std::string& key, int defaultVal) const { return value(key, SimpleVariant(defaultVal)); }
    SimpleVariant value(const std::string& key, double defaultVal) const
    {
        return value(key, SimpleVariant(defaultVal));
    }
    SimpleVariant value(const std::string& key, const std::string& defaultVal) const
    {
        return value(key, SimpleVariant(defaultVal));
    }
    SimpleVariant value(const std::string& key, bool defaultVal) const { return value(key, SimpleVariant(defaultVal)); }
    void setValue(const std::string& key, const std::string& value) { m_store[fullKey(key)] = SimpleVariant(value); }
    void setValue(const std::string& key, int value) { m_store[fullKey(key)] = SimpleVariant(value); }
    void setValue(const std::string& key, double value) { m_store[fullKey(key)] = SimpleVariant(value); }
    void setValue(const std::string& key, bool value) { m_store[fullKey(key)] = SimpleVariant(value); }
    void beginGroup(const std::string& prefix) { m_prefix = prefix; }
    void endGroup() { m_prefix.clear(); }
    void remove(const std::string& key) { m_store.erase(fullKey(key)); }
    void sync() {}

  private:
    std::string m_prefix;
    std::map<std::string, SimpleVariant> m_store;
    std::string fullKey(const std::string& key) const { return m_prefix.empty() ? key : m_prefix + "/" + key; }
};
