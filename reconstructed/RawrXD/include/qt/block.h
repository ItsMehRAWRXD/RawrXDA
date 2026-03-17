// ============================================================================
// qt_block.h — RawrXD Qt Dependency Blocker
// ============================================================================
// REVERSE-ENGINEERED SHIM: Maps every Qt type the codebase references to
// native STL / Win32 equivalents. Force-included via /FI to intercept all
// Qt usage at compile time without modifying source files.
//
// Philosophy: source = linking — no external framework dependencies.
// ============================================================================
#pragma once
#ifndef RAWRXD_QT_BLOCK_H
#define RAWRXD_QT_BLOCK_H

#ifndef RAWRXD_QT_ELIMINATED
#define RAWRXD_QT_ELIMINATED 1
#endif

// ─── Standard Library Backing ───────────────────────────────────────────────
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <list>
#include <mutex>
#include <thread>
#include <functional>
#include <memory>
#include <any>
#include <variant>
#include <optional>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <regex>
#include <cassert>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 1: Qt Macros → No-ops
// ═══════════════════════════════════════════════════════════════════════════════
#ifndef Q_OBJECT
#define Q_OBJECT
#endif
#ifndef Q_GADGET
#define Q_GADGET
#endif
#ifndef Q_NAMESPACE
#define Q_NAMESPACE
#endif
#ifndef Q_SIGNALS
#define Q_SIGNALS public
#endif
#ifndef Q_SLOTS
#define Q_SLOTS
#endif
#ifndef Q_EMIT
#define Q_EMIT
#endif
#ifndef Q_PROPERTY
#define Q_PROPERTY(...)
#endif
#ifndef Q_INVOKABLE
#define Q_INVOKABLE
#endif
#ifndef Q_DECLARE_METATYPE
#define Q_DECLARE_METATYPE(...)
#endif
#ifndef Q_ENUM
#define Q_ENUM(...)
#endif
#ifndef Q_FLAG
#define Q_FLAG(...)
#endif
#ifndef Q_UNUSED
#define Q_UNUSED(x) (void)(x)
#endif
#ifndef Q_ASSERT
#define Q_ASSERT(x) assert(x)
#endif
#ifndef Q_NULLPTR
#define Q_NULLPTR nullptr
#endif
#ifndef signals
#define signals public
#endif
#ifndef slots
#define slots
#endif
#ifndef emit
#define emit
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 2: Core Value Types
// ═══════════════════════════════════════════════════════════════════════════════

#ifndef QSTRING_H
#define QSTRING_H
class QString : public std::string {
public:
    using std::string::string;
    using std::string::operator=;
    QString() = default;
    QString(const std::string& s) : std::string(s) {}
    QString(const char* s) : std::string(s ? s : "") {}
    QString(std::string&& s) : std::string(std::move(s)) {}
    static QString fromStdString(const std::string& s) { return QString(s); }
    static QString fromUtf8(const char* s, int len = -1) { return len < 0 ? QString(s ? s : "") : QString(std::string(s, len)); }
    static QString fromLocal8Bit(const char* s, int len = -1) { return fromUtf8(s, len); }
    static QString fromLatin1(const char* s, int len = -1) { return fromUtf8(s, len); }
    static QString number(int n) { return QString(std::to_string(n)); }
    static QString number(long n) { return QString(std::to_string(n)); }
    static QString number(long long n) { return QString(std::to_string(n)); }
    static QString number(double n, char f = 'g', int prec = 6) { (void)f; (void)prec; return QString(std::to_string(n)); }
    static QString number(unsigned long long n) { return QString(std::to_string(n)); }
    std::string toStdString() const { return *this; }
    const char* toUtf8() const { return c_str(); }
    const char* toLatin1() const { return c_str(); }
    const char* toLocal8Bit() const { return c_str(); }
    int toInt(bool* ok = nullptr, int base = 10) const { try { int v = std::stoi(*this, nullptr, base); if(ok) *ok=true; return v; } catch(...) { if(ok) *ok=false; return 0; } }
    double toDouble(bool* ok = nullptr) const { try { double v = std::stod(*this); if(ok) *ok=true; return v; } catch(...) { if(ok) *ok=false; return 0; } }
    bool isEmpty() const { return empty(); }
    bool isNull() const { return empty(); }
    int length() const { return (int)size(); }
    int count() const { return (int)size(); }
    QString toLower() const { QString r(*this); std::transform(r.begin(), r.end(), r.begin(), ::tolower); return r; }
    QString toUpper() const { QString r(*this); std::transform(r.begin(), r.end(), r.begin(), ::toupper); return r; }
    QString trimmed() const { auto s = find_first_not_of(" \t\r\n"); auto e = find_last_not_of(" \t\r\n"); return (s == npos) ? QString() : QString(substr(s, e-s+1)); }
    QString simplified() const { return trimmed(); }
    bool startsWith(const QString& s) const { return size() >= s.size() && compare(0, s.size(), s) == 0; }
    bool endsWith(const QString& s) const { return size() >= s.size() && compare(size()-s.size(), s.size(), s) == 0; }
    bool contains(const QString& s) const { return find(s) != npos; }
    QString mid(int pos, int len = -1) const { if(pos >= (int)size()) return {}; return QString(len < 0 ? substr(pos) : substr(pos, len)); }
    QString left(int n) const { return QString(substr(0, n)); }
    QString right(int n) const { return (int)size() > n ? QString(substr(size()-n)) : *this; }
    int indexOf(const QString& s, int from = 0) const { auto p = find(s, from); return p == npos ? -1 : (int)p; }
    QString& replace(const QString& before, const QString& after) { size_t p = 0; while((p = find(before, p)) != npos) { std::string::replace(p, before.size(), after); p += after.size(); } return *this; }
    std::vector<QString> split(const QString& sep) const { std::vector<QString> r; size_t s=0,p; while((p=find(sep,s))!=npos){r.push_back(QString(substr(s,p-s)));s=p+sep.size();} r.push_back(QString(substr(s))); return r; }
    QString arg(const QString& a) const { QString r(*this); size_t p = r.find("%1"); if(p!=npos) r.std::string::replace(p,2,a); return r; }
    QString arg(int a) const { return arg(QString::number(a)); }
    QString arg(double a) const { return arg(QString::number(a)); }
};
inline QString operator+(const char* a, const QString& b) { return QString(std::string(a) + std::string(b)); }
#endif

#ifndef QBYTEARRAY_H
#define QBYTEARRAY_H
class QByteArray : public std::string {
public:
    using std::string::string;
    using std::string::operator=;
    QByteArray() = default;
    QByteArray(const char* s, int len) : std::string(s, len) {}
    QByteArray(const char* s) : std::string(s ? s : "") {}
    QByteArray(const std::string& s) : std::string(s) {}
    QByteArray(int size, char ch) : std::string(size, ch) {}
    static QByteArray fromRawData(const char* data, int size) { return QByteArray(data, size); }
    static QByteArray fromHex(const QByteArray&) { return {}; }
    static QByteArray number(int n) { return QByteArray(std::to_string(n)); }
    bool isEmpty() const { return empty(); }
    int length() const { return (int)size(); }
    int count() const { return (int)size(); }
    const char* constData() const { return data(); }
    bool contains(const char* s) const { return find(s) != npos; }
    QByteArray mid(int pos, int len = -1) const { return QByteArray(len < 0 ? substr(pos) : substr(pos, len)); }
    QByteArray left(int n) const { return QByteArray(substr(0, n)); }
    QByteArray toHex() const { return *this; }
    QByteArray toLower() const { QByteArray r(*this); std::transform(r.begin(),r.end(),r.begin(),::tolower); return r; }
    int toInt(bool* ok = nullptr) const { try { int v=std::stoi(*this); if(ok)*ok=true; return v; } catch(...) { if(ok)*ok=false; return 0; } }
    void truncate(int pos) { if(pos < (int)size()) resize(pos); }
    QByteArray& prepend(const char* s) { insert(0, s); return *this; }
};
#endif

#ifndef QSTRINGLIST_H
#define QSTRINGLIST_H
class QStringList : public std::vector<QString> {
public:
    using std::vector<QString>::vector;
    QStringList() = default;
    QStringList(std::initializer_list<QString> il) : std::vector<QString>(il) {}
    bool contains(const QString& s) const { return std::find(begin(), end(), s) != end(); }
    QString join(const QString& sep) const { QString r; for(size_t i=0;i<size();i++){if(i)r+=sep;r+=(*this)[i];} return r; }
    int count() const { return (int)size(); }
    QStringList& operator<<(const QString& s) { push_back(s); return *this; }
    void append(const QString& s) { push_back(s); }
    bool isEmpty() const { return empty(); }
};
#endif

#ifndef QVARIANT_H
#define QVARIANT_H
class QVariant {
    std::any val_;
public:
    QVariant() = default;
    QVariant(int v) : val_(v) {}
    QVariant(double v) : val_(v) {}
    QVariant(bool v) : val_(v) {}
    QVariant(const QString& v) : val_(v) {}
    QVariant(const char* v) : val_(QString(v)) {}
    bool isValid() const { return val_.has_value(); }
    bool isNull() const { return !val_.has_value(); }
    int toInt(bool* ok = nullptr) const { try { int v = std::any_cast<int>(val_); if(ok)*ok=true; return v; } catch(...) { if(ok)*ok=false; return 0; } }
    double toDouble(bool* ok = nullptr) const { try { double v = std::any_cast<double>(val_); if(ok)*ok=true; return v; } catch(...) { if(ok)*ok=false; return 0; } }
    bool toBool() const { try { return std::any_cast<bool>(val_); } catch(...) { return false; } }
    QString toString() const { try { return std::any_cast<QString>(val_); } catch(...) { return {}; } }
    QByteArray toByteArray() const { try { return std::any_cast<QByteArray>(val_); } catch(...) { return {}; } }
    template<typename T> static QVariant fromValue(const T& v) { QVariant q; q.val_ = v; return q; }
    template<typename T> T value() const { try { return std::any_cast<T>(val_); } catch(...) { return T{}; } }
};
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 3: Container Types
// ═══════════════════════════════════════════════════════════════════════════════

#ifndef QLIST_H
#define QLIST_H
template<typename T>
class QList : public std::vector<T> {
public:
    using std::vector<T>::vector;
    int count() const { return (int)this->size(); }
    bool isEmpty() const { return this->empty(); }
    T value(int i, const T& def = T{}) const { return (i >= 0 && i < (int)this->size()) ? (*this)[i] : def; }
    void append(const T& v) { this->push_back(v); }
    bool contains(const T& v) const { return std::find(this->begin(), this->end(), v) != this->end(); }
    int indexOf(const T& v) const { auto it = std::find(this->begin(), this->end(), v); return it != this->end() ? (int)(it - this->begin()) : -1; }
    void removeAt(int i) { if(i >= 0 && i < (int)this->size()) this->erase(this->begin() + i); }
    T takeFirst() { T v = this->front(); this->erase(this->begin()); return v; }
    T takeLast() { T v = this->back(); this->pop_back(); return v; }
    QList<T>& operator<<(const T& v) { this->push_back(v); return *this; }
    T first() const { return this->front(); }
    T last() const { return this->back(); }
    void prepend(const T& v) { this->insert(this->begin(), v); }
};
#endif

#ifndef QVECTOR_H
#define QVECTOR_H
template<typename T> using QVector = QList<T>;
#endif

#ifndef QMAP_H
#define QMAP_H
template<typename K, typename V>
class QMap : public std::map<K, V> {
public:
    using std::map<K, V>::map;
    bool contains(const K& k) const { return this->find(k) != this->end(); }
    V value(const K& k, const V& def = V{}) const { auto it = this->find(k); return it != this->end() ? it->second : def; }
    QList<K> keys() const { QList<K> r; for(auto& p : *this) r.push_back(p.first); return r; }
    QList<V> values() const { QList<V> r; for(auto& p : *this) r.push_back(p.second); return r; }
    void insert(const K& k, const V& v) { (*this)[k] = v; }
    int count() const { return (int)this->size(); }
    bool isEmpty() const { return this->empty(); }
    void remove(const K& k) { this->erase(k); }
};
#endif

#ifndef QHASH_H
#define QHASH_H
template<typename K, typename V>
class QHash : public std::unordered_map<K, V> {
public:
    using std::unordered_map<K, V>::unordered_map;
    bool contains(const K& k) const { return this->find(k) != this->end(); }
    V value(const K& k, const V& def = V{}) const { auto it = this->find(k); return it != this->end() ? it->second : def; }
    QList<K> keys() const { QList<K> r; for(auto& p : *this) r.push_back(p.first); return r; }
    QList<V> values() const { QList<V> r; for(auto& p : *this) r.push_back(p.second); return r; }
    void insert(const K& k, const V& v) { (*this)[k] = v; }
    int count() const { return (int)this->size(); }
    bool isEmpty() const { return this->empty(); }
    void remove(const K& k) { this->erase(k); }
};
namespace std { template<> struct hash<QString> { size_t operator()(const QString& s) const { return hash<string>()(s); } }; }
#endif

#ifndef QSET_H
#define QSET_H
template<typename T>
class QSet : public std::unordered_set<T> {
public:
    using std::unordered_set<T>::unordered_set;
    bool contains(const T& v) const { return this->find(v) != this->end(); }
    QList<T> values() const { return QList<T>(this->begin(), this->end()); }
    QList<T> toList() const { return values(); }
    void insert(const T& v) { std::unordered_set<T>::insert(v); }
    void remove(const T& v) { this->erase(v); }
    bool isEmpty() const { return this->empty(); }
    int count() const { return (int)this->size(); }
};
#endif

#ifndef QPAIR_H
#define QPAIR_H
template<typename T1, typename T2> using QPair = std::pair<T1, T2>;
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 4: QObject Base Class
// ═══════════════════════════════════════════════════════════════════════════════
#ifndef QOBJECT_H
#define QOBJECT_H
class QObject {
public:
    QObject(QObject* parent = nullptr) { (void)parent; }
    virtual ~QObject() = default;
    void setObjectName(const QString& name) { objectName_ = name; }
    QString objectName() const { return objectName_; }
    virtual const char* metaObject() const { return "QObject"; }
    void setProperty(const char*, const QVariant&) {}
    QVariant property(const char*) const { return {}; }
    void deleteLater() {}
    template<typename... Args> static void connect(Args&&...) {}
    template<typename... Args> static void disconnect(Args&&...) {}
    QObject* parent() const { return nullptr; }
    void setParent(QObject*) {}
    QList<QObject*> children() const { return {}; }
    QObject* findChild(const QString& = {}) const { return nullptr; }
    bool inherits(const char*) const { return false; }
    int startTimer(int) { return 0; }
    void killTimer(int) {}
    void moveToThread(void*) {}
protected:
    virtual void timerEvent(void*) {}
    virtual void customEvent(void*) {}
    virtual bool event(void*) { return false; }
    virtual bool eventFilter(QObject*, void*) { return false; }
private:
    QString objectName_;
};
class QMetaObject { public: struct Connection {}; static Connection connectSlotsByName(QObject*) { return {}; } };
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 5: GUI Widget Stubs
// ═══════════════════════════════════════════════════════════════════════════════

#ifndef QWIDGET_H
#define QWIDGET_H
class QWidget : public QObject {
public:
    QWidget(QWidget* parent = nullptr) : QObject(parent) {}
    virtual ~QWidget() = default;
    void show() {} void hide() {} void close() {} void update() {} void repaint() {}
    void resize(int, int) {} void move(int, int) {} void setMinimumSize(int, int) {} void setMaximumSize(int, int) {} void setFixedSize(int, int) {}
    void setGeometry(int, int, int, int) {} void setWindowTitle(const QString&) {} void setToolTip(const QString&) {} void setStyleSheet(const QString&) {}
    void setEnabled(bool) {} void setVisible(bool) {} void setFocus() {} void setCursor(int) {} void setFont(const void*) {} void setLayout(void*) {}
    bool isVisible() const { return false; } bool isEnabled() const { return true; } int width() const { return 0; } int height() const { return 0; }
    QWidget* parentWidget() const { return nullptr; }
    void setAttribute(int, bool = true) {} void setContentsMargins(int,int,int,int) {} void setSizePolicy(int, int) {} void setWindowFlags(int) {}
    void raise() {} void lower() {} void setAcceptDrops(bool) {} void grabKeyboard() {} void releaseKeyboard() {}
    void adjustSize() {} void setContextMenuPolicy(int) {} void addAction(void*) {}
    void setMinimumWidth(int) {} void setMinimumHeight(int) {} void setMaximumWidth(int) {} void setMaximumHeight(int) {}
    QList<QWidget*> findChildren() const { return {}; }
};
#endif

#ifndef QMAINWINDOW_H
#define QMAINWINDOW_H
class QMenuBar; class QStatusBar; class QToolBar; class QDockWidget;
class QMainWindow : public QWidget {
public:
    QMainWindow(QWidget* parent = nullptr) : QWidget(parent) {}
    void setCentralWidget(QWidget*) {} QWidget* centralWidget() const { return nullptr; }
    QMenuBar* menuBar() const { return nullptr; } QStatusBar* statusBar() const { return nullptr; }
    QToolBar* addToolBar(const QString&) { return nullptr; } void addDockWidget(int, QDockWidget*) {}
    void setMenuBar(QMenuBar*) {} void setStatusBar(QStatusBar*) {} void setWindowIcon(const void*) {}
    void showMaximized() {} void showMinimized() {} void showFullScreen() {} void showNormal() {}
};
#endif

#ifndef QLABEL_H
#define QLABEL_H
class QLabel : public QWidget {
public:
    QLabel(QWidget* p = nullptr) : QWidget(p) {} QLabel(const QString& text, QWidget* p = nullptr) : QWidget(p), text_(text) {}
    void setText(const QString& t) { text_ = t; } QString text() const { return text_; }
    void setAlignment(int) {} void setWordWrap(bool) {} void setPixmap(const void*) {} void setOpenExternalLinks(bool) {}
    void setTextFormat(int) {} void setMargin(int) {} void setIndent(int) {} void setBuddy(QWidget*) {}
private: QString text_;
};
#endif

#ifndef QLINEEDIT_H
#define QLINEEDIT_H
class QLineEdit : public QWidget {
public:
    QLineEdit(QWidget* p = nullptr) : QWidget(p) {} QLineEdit(const QString& text, QWidget* p = nullptr) : QWidget(p), text_(text) {}
    void setText(const QString& t) { text_ = t; } QString text() const { return text_; }
    void setPlaceholderText(const QString&) {} void setEchoMode(int) {} void setReadOnly(bool) {} void setMaxLength(int) {}
    void setClearButtonEnabled(bool) {} void setValidator(const void*) {} void selectAll() {} void clear() { text_.clear(); }
    bool isReadOnly() const { return false; }
    enum EchoMode { Normal, Password, NoEcho, PasswordEchoOnEdit };
private: QString text_;
};
#endif

#ifndef QTEXTEDIT_H
#define QTEXTEDIT_H
class QTextEdit : public QWidget {
public:
    QTextEdit(QWidget* p = nullptr) : QWidget(p) {}
    void setText(const QString& t) { text_ = t; } void setPlainText(const QString& t) { text_ = t; } void setHtml(const QString&) {}
    QString toPlainText() const { return text_; } QString toHtml() const { return text_; }
    void append(const QString& t) { text_ += "\n" + t; } void clear() { text_.clear(); }
    void setReadOnly(bool) {} void setLineWrapMode(int) {} void setTabStopDistance(double) {} void setAcceptRichText(bool) {}
    void moveCursor(int) {} void ensureCursorVisible() {} void setFontFamily(const QString&) {} void setFontPointSize(double) {}
    void insertPlainText(const QString&) {} void setTextColor(const void*) {}
    enum LineWrapMode { NoWrap, WidgetWidth, FixedPixelWidth, FixedColumnWidth };
private: QString text_;
};
#endif

#ifndef QPLAINTEXTEDIT_H
#define QPLAINTEXTEDIT_H
class QPlainTextEdit : public QWidget {
public:
    QPlainTextEdit(QWidget* p = nullptr) : QWidget(p) {}
    void setPlainText(const QString& t) { text_ = t; } QString toPlainText() const { return text_; }
    void appendPlainText(const QString& t) { text_ += "\n" + t; } void clear() { text_.clear(); }
    void setReadOnly(bool) {} void setLineWrapMode(int) {} void setTabStopDistance(double) {}
    void moveCursor(int) {} void ensureCursorVisible() {} int blockCount() const { return 1; }
    void insertPlainText(const QString&) {} void setOverwriteMode(bool) {} void setTabChangesFocus(bool) {} void centerCursor() {}
    enum LineWrapMode { NoWrap, WidgetWidth };
private: QString text_;
};
#endif

#ifndef QPUSHBUTTON_H
#define QPUSHBUTTON_H
class QPushButton : public QWidget {
public:
    QPushButton(QWidget* p = nullptr) : QWidget(p) {} QPushButton(const QString& text, QWidget* p = nullptr) : QWidget(p), text_(text) {}
    void setText(const QString& t) { text_ = t; } QString text() const { return text_; }
    void setIcon(const void*) {} void setCheckable(bool) {} bool isChecked() const { return false; } void setChecked(bool) {}
    void setDefault(bool) {} void setFlat(bool) {} void setAutoDefault(bool) {} void setMenu(void*) {} void click() {} void toggle() {}
private: QString text_;
};
#endif

#ifndef QCHECKBOX_H
#define QCHECKBOX_H
class QCheckBox : public QWidget {
public:
    QCheckBox(QWidget* p = nullptr) : QWidget(p) {} QCheckBox(const QString& text, QWidget* p = nullptr) : QWidget(p), text_(text) {}
    void setText(const QString& t) { text_ = t; } QString text() const { return text_; }
    bool isChecked() const { return checked_; } void setChecked(bool c) { checked_ = c; }
    void setTristate(bool) {} int checkState() const { return checked_ ? 2 : 0; } void setCheckState(int s) { checked_ = s != 0; }
private: QString text_; bool checked_ = false;
};
#endif

#ifndef QCOMBOBOX_H
#define QCOMBOBOX_H
class QComboBox : public QWidget {
public:
    QComboBox(QWidget* p = nullptr) : QWidget(p) {}
    void addItem(const QString& text) { items_.push_back(text); } void addItems(const QStringList& items) { for(auto& i : items) items_.push_back(i); }
    QString currentText() const { return idx_ >= 0 && idx_ < (int)items_.size() ? items_[idx_] : QString(); }
    int currentIndex() const { return idx_; } void setCurrentIndex(int i) { idx_ = i; } void setCurrentText(const QString&) {}
    int count() const { return (int)items_.size(); } void clear() { items_.clear(); idx_ = -1; } void setEditable(bool) {}
    QString itemText(int i) const { return i >= 0 && i < (int)items_.size() ? items_[i] : QString(); }
    void removeItem(int i) { if(i >= 0 && i < (int)items_.size()) items_.erase(items_.begin()+i); }
    int findText(const QString& t) const { for(int i=0;i<(int)items_.size();i++) if(items_[i]==t) return i; return -1; }
    void insertItem(int idx, const QString& text) { items_.insert(items_.begin()+idx, text); }
private: std::vector<QString> items_; int idx_ = -1;
};
#endif

#ifndef QLISTWIDGET_H
#define QLISTWIDGET_H
class QListWidgetItem {
public:
    QListWidgetItem(const QString& text = {}) : text_(text) {}
    void setText(const QString& t) { text_ = t; } QString text() const { return text_; }
    void setData(int, const QVariant&) {} QVariant data(int) const { return {}; }
    void setFlags(int) {} void setToolTip(const QString&) {} void setIcon(const void*) {}
    void setCheckState(int) {} void setSelected(bool) {} void setHidden(bool) {}
    void setForeground(const void*) {} void setBackground(const void*) {} void setFont(const void*) {}
private: QString text_;
};
class QListWidget : public QWidget {
public:
    QListWidget(QWidget* p = nullptr) : QWidget(p) {}
    void addItem(const QString& text) { items_.push_back(new QListWidgetItem(text)); }
    void addItem(QListWidgetItem* item) { items_.push_back(item); }
    void insertItem(int row, QListWidgetItem* item) { items_.insert(items_.begin()+row, item); }
    QListWidgetItem* item(int row) const { return (row >= 0 && row < (int)items_.size()) ? items_[row] : nullptr; }
    QListWidgetItem* currentItem() const { return nullptr; } int currentRow() const { return -1; }
    int count() const { return (int)items_.size(); }
    void clear() { for(auto* i : items_) delete i; items_.clear(); }
    QListWidgetItem* takeItem(int row) { if(row < 0 || row >= (int)items_.size()) return nullptr; auto* i = items_[row]; items_.erase(items_.begin()+row); return i; }
    void setCurrentRow(int) {} void scrollToItem(QListWidgetItem*) {} void setSelectionMode(int) {} void setSortingEnabled(bool) {}
    ~QListWidget() { for(auto* i : items_) delete i; }
private: std::vector<QListWidgetItem*> items_;
};
#endif

// ─── Layouts ────────────────────────────────────────────────────────────────
#ifndef QLAYOUT_H
#define QLAYOUT_H
class QLayout : public QObject {
public:
    QLayout(QWidget* p = nullptr) : QObject(p) {}
    void addWidget(QWidget*) {} void removeWidget(QWidget*) {} void setContentsMargins(int,int,int,int) {} void setSpacing(int) {}
    int count() const { return 0; } void setAlignment(int) {} void addItem(void*) {}
};
#endif
#ifndef QVBOXLAYOUT_H
#define QVBOXLAYOUT_H
class QVBoxLayout : public QLayout {
public:
    QVBoxLayout(QWidget* p = nullptr) : QLayout(p) {}
    void addWidget(QWidget*, int = 0, int = 0) {} void addLayout(QLayout*, int = 0) {} void addStretch(int = 0) {} void addSpacing(int) {}
    void insertWidget(int, QWidget*, int = 0) {} void insertLayout(int, QLayout*, int = 0) {} void insertStretch(int, int = 0) {}
};
#endif
#ifndef QHBOXLAYOUT_H
#define QHBOXLAYOUT_H
class QHBoxLayout : public QLayout {
public:
    QHBoxLayout(QWidget* p = nullptr) : QLayout(p) {}
    void addWidget(QWidget*, int = 0, int = 0) {} void addLayout(QLayout*, int = 0) {} void addStretch(int = 0) {} void addSpacing(int) {}
};
#endif
#ifndef QGRIDLAYOUT_H
#define QGRIDLAYOUT_H
class QGridLayout : public QLayout {
public:
    QGridLayout(QWidget* p = nullptr) : QLayout(p) {}
    void addWidget(QWidget*, int, int, int = 1, int = 1) {} void addLayout(QLayout*, int, int, int = 1, int = 1) {}
    void setColumnStretch(int, int) {} void setRowStretch(int, int) {}
};
#endif

// ─── Menus / Actions / Toolbars / DockWidgets ───────────────────────────────
#ifndef QACTION_H
#define QACTION_H
class QAction : public QObject {
public:
    QAction(QObject* p = nullptr) : QObject(p) {} QAction(const QString& text, QObject* p = nullptr) : QObject(p), text_(text) {}
    void setText(const QString& t) { text_ = t; } QString text() const { return text_; }
    void setEnabled(bool e) { enabled_ = e; } bool isEnabled() const { return enabled_; }
    void setCheckable(bool c) { checkable_ = c; } bool isCheckable() const { return checkable_; }
    void setChecked(bool c) { checked_ = c; } bool isChecked() const { return checked_; }
    void setShortcut(const void*) {} void setIcon(const void*) {} void setToolTip(const QString&) {} void setStatusTip(const QString&) {}
    void setData(const QVariant&) {} QVariant data() const { return {}; } void trigger() {} void toggle() {} void setVisible(bool) {}
    void setSeparator(bool) {} void setIconVisibleInMenu(bool) {} void setMenuRole(int) {}
private: QString text_; bool enabled_ = true; bool checkable_ = false; bool checked_ = false;
};
#endif
#ifndef QACTIONGROUP_H
#define QACTIONGROUP_H
class QActionGroup : public QObject {
public:
    QActionGroup(QObject* p = nullptr) : QObject(p) {}
    QAction* addAction(QAction* a) { return a; } QAction* addAction(const QString&) { return nullptr; }
    void setExclusive(bool) {} QList<QAction*> actions() const { return {}; } void setEnabled(bool) {}
};
#endif
#ifndef QMENU_H
#define QMENU_H
class QMenu : public QWidget {
public:
    QMenu(QWidget* p = nullptr) : QWidget(p) {} QMenu(const QString&, QWidget* p = nullptr) : QWidget(p) {}
    QAction* addAction(const QString&) { return nullptr; } QAction* addAction(const void*, const QString&) { return nullptr; }
    QMenu* addMenu(const QString&) { return nullptr; } QMenu* addMenu(const void*, const QString&) { return nullptr; }
    void addSeparator() {} void addAction(QAction*) {} QAction* exec(void* = nullptr) { return nullptr; }
    QAction* menuAction() const { return nullptr; } void clear() {} void popup(void*) {}
    void setTitle(const QString&) {} void setIcon(const void*) {}
};
#endif
#ifndef QMENUBAR_H
#define QMENUBAR_H
class QMenuBar : public QWidget {
public:
    QMenuBar(QWidget* p = nullptr) : QWidget(p) {}
    QMenu* addMenu(const QString&) { return nullptr; } QMenu* addMenu(const void*, const QString&) { return nullptr; }
    void addAction(QAction*) {} QAction* addAction(const QString&) { return nullptr; } void clear() {} void setNativeMenuBar(bool) {}
};
#endif
#ifndef QSTATUSBAR_H
#define QSTATUSBAR_H
class QStatusBar : public QWidget {
public:
    QStatusBar(QWidget* p = nullptr) : QWidget(p) {}
    void showMessage(const QString&, int = 0) {} void clearMessage() {} void addWidget(QWidget*, int = 0) {}
    void addPermanentWidget(QWidget*, int = 0) {} void removeWidget(QWidget*) {}
};
#endif
#ifndef QTOOLBAR_H
#define QTOOLBAR_H
class QToolBar : public QWidget {
public:
    QToolBar(QWidget* p = nullptr) : QWidget(p) {} QToolBar(const QString&, QWidget* p = nullptr) : QWidget(p) {}
    QAction* addAction(const QString&) { return nullptr; } QAction* addAction(const void*, const QString&) { return nullptr; }
    void addSeparator() {} void addWidget(QWidget*) {} void setMovable(bool) {} void setFloatable(bool) {}
    void setIconSize(void*) {} void setToolButtonStyle(int) {} void clear() {} void addAction(QAction*) {} void setOrientation(int) {}
};
#endif
#ifndef QDOCKWIDGET_H
#define QDOCKWIDGET_H
class QDockWidget : public QWidget {
public:
    QDockWidget(QWidget* p = nullptr) : QWidget(p) {} QDockWidget(const QString&, QWidget* p = nullptr) : QWidget(p) {}
    void setWidget(QWidget*) {} QWidget* widget() const { return nullptr; } void setAllowedAreas(int) {} void setFeatures(int) {}
    void setWindowTitle(const QString&) {} void setTitleBarWidget(QWidget*) {} QAction* toggleViewAction() const { return nullptr; }
    enum DockWidgetFeature { DockWidgetClosable = 1, DockWidgetMovable = 2, DockWidgetFloatable = 4, AllDockWidgetFeatures = 7, NoDockWidgetFeatures = 0 };
};
#endif
#ifndef QTABWIDGET_H
#define QTABWIDGET_H
class QTabWidget : public QWidget {
public:
    QTabWidget(QWidget* p = nullptr) : QWidget(p) {}
    int addTab(QWidget*, const QString&) { return 0; } int addTab(QWidget*, const void*, const QString&) { return 0; }
    void setCurrentIndex(int) {} int currentIndex() const { return 0; } int count() const { return 0; }
    QWidget* widget(int) const { return nullptr; } QWidget* currentWidget() const { return nullptr; }
    void removeTab(int) {} void setTabText(int, const QString&) {} void setTabIcon(int, const void*) {}
    void setTabsClosable(bool) {} void setMovable(bool) {} void setDocumentMode(bool) {} void clear() {}
    void setTabPosition(int) {} void setTabToolTip(int, const QString&) {} QString tabText(int) const { return {}; }
    int indexOf(QWidget*) const { return -1; } void insertTab(int, QWidget*, const QString&) {}
    enum TabPosition { North, South, West, East };
};
#endif
#ifndef QSPLITTER_H
#define QSPLITTER_H
class QSplitter : public QWidget {
public:
    QSplitter(QWidget* p = nullptr) : QWidget(p) {} QSplitter(int, QWidget* p = nullptr) : QWidget(p) {}
    void addWidget(QWidget*) {} void setStretchFactor(int, int) {} void setSizes(const QList<int>&) {} QList<int> sizes() const { return {}; }
    void setOrientation(int) {} void setCollapsible(int, bool) {} void insertWidget(int, QWidget*) {}
    int count() const { return 0; } QWidget* widget(int) const { return nullptr; } void setHandleWidth(int) {} void setChildrenCollapsible(bool) {}
};
#endif
#ifndef QTREEVIEW_H
#define QTREEVIEW_H
class QTreeView : public QWidget {
public:
    QTreeView(QWidget* p = nullptr) : QWidget(p) {}
    void setModel(void*) {} void setRootIndex(void*) {} void expandAll() {} void collapseAll() {}
    void setHeaderHidden(bool) {} void setAnimated(bool) {} void setIndentation(int) {} void setSelectionMode(int) {}
    void setExpandsOnDoubleClick(bool) {} void scrollTo(void*) {} void setAlternatingRowColors(bool) {} void setSortingEnabled(bool) {}
    void setEditTriggers(int) {} void setDragDropMode(int) {} void setDragEnabled(bool) {} void setAcceptDrops(bool) {} void setDropIndicatorShown(bool) {}
};
#endif
#ifndef QTREEWIDGET_H
#define QTREEWIDGET_H
class QTreeWidgetItem {
public:
    QTreeWidgetItem() = default; QTreeWidgetItem(const QStringList&) {} QTreeWidgetItem(QTreeWidgetItem*, const QStringList& = {}) {}
    void setText(int, const QString&) {} QString text(int) const { return {}; } void setData(int, int, const QVariant&) {} QVariant data(int, int) const { return {}; }
    void setIcon(int, const void*) {} void setExpanded(bool) {} void setFlags(int) {} void setToolTip(int, const QString&) {}
    void addChild(QTreeWidgetItem*) {} int childCount() const { return 0; } QTreeWidgetItem* child(int) const { return nullptr; }
    QTreeWidgetItem* parent() const { return nullptr; } void removeChild(QTreeWidgetItem*) {} void setCheckState(int, int) {}
    void setForeground(int, const void*) {} void setBackground(int, const void*) {}
};
class QTreeWidget : public QWidget {
public:
    QTreeWidget(QWidget* p = nullptr) : QWidget(p) {}
    void setHeaderLabels(const QStringList&) {} void addTopLevelItem(QTreeWidgetItem*) {} QTreeWidgetItem* currentItem() const { return nullptr; }
    int topLevelItemCount() const { return 0; } QTreeWidgetItem* topLevelItem(int) const { return nullptr; } void clear() {} void expandAll() {} void collapseAll() {}
    void setColumnCount(int) {} QTreeWidgetItem* invisibleRootItem() const { return nullptr; } void setHeaderHidden(bool) {}
    void setSelectionMode(int) {} void setAnimated(bool) {} int columnCount() const { return 0; }
};
#endif
#ifndef QDIALOG_H
#define QDIALOG_H
class QDialog : public QWidget {
public:
    QDialog(QWidget* p = nullptr) : QWidget(p) {} int exec() { return 0; } void accept() {} void reject() {} void done(int) {}
    void setModal(bool) {} void setWindowModality(int) {} void open() {}
    enum DialogCode { Rejected = 0, Accepted = 1 };
};
#endif
#ifndef QMESSAGEBOX_H
#define QMESSAGEBOX_H
class QMessageBox : public QDialog {
public:
    QMessageBox(QWidget* p = nullptr) : QDialog(p) {}
    static int information(QWidget*, const QString&, const QString&, int = 0, int = 0) { return 0; }
    static int warning(QWidget*, const QString&, const QString&, int = 0, int = 0) { return 0; }
    static int critical(QWidget*, const QString&, const QString&, int = 0, int = 0) { return 0; }
    static int question(QWidget*, const QString&, const QString&, int = 0, int = 0) { return 0; }
    void setText(const QString&) {} void setInformativeText(const QString&) {} void setDetailedText(const QString&) {}
    void setIcon(int) {} void setStandardButtons(int) {}
    enum StandardButton { Ok = 0x400, Cancel = 0x400000, Yes = 0x4000, No = 0x10000 };
    enum Icon { NoIcon, Information, Warning, Critical, Question };
};
#endif
#ifndef QPROGRESSBAR_H
#define QPROGRESSBAR_H
class QProgressBar : public QWidget {
public:
    QProgressBar(QWidget* p = nullptr) : QWidget(p) {}
    void setValue(int v) { val_ = v; } int value() const { return val_; } void setRange(int min, int max) { min_=min; max_=max; }
    void setMinimum(int m) { min_ = m; } void setMaximum(int m) { max_ = m; } int minimum() const { return min_; } int maximum() const { return max_; }
    void setFormat(const QString&) {} void setTextVisible(bool) {} void setOrientation(int) {} void setInvertedAppearance(bool) {} void reset() { val_=0; }
private: int val_=0, min_=0, max_=100;
};
#endif
#ifndef QSCROLLAREA_H
#define QSCROLLAREA_H
class QScrollArea : public QWidget {
public:
    QScrollArea(QWidget* p = nullptr) : QWidget(p) {}
    void setWidget(QWidget*) {} QWidget* widget() const { return nullptr; } void setWidgetResizable(bool) {}
    void setHorizontalScrollBarPolicy(int) {} void setVerticalScrollBarPolicy(int) {}
};
#endif
#ifndef QSTACKEDWIDGET_H
#define QSTACKEDWIDGET_H
class QStackedWidget : public QWidget {
public:
    QStackedWidget(QWidget* p = nullptr) : QWidget(p) {}
    int addWidget(QWidget*) { return 0; } void setCurrentIndex(int) {} void setCurrentWidget(QWidget*) {}
    int currentIndex() const { return 0; } QWidget* currentWidget() const { return nullptr; } int count() const { return 0; } QWidget* widget(int) const { return nullptr; }
};
#endif
#ifndef QGROUPBOX_H
#define QGROUPBOX_H
class QGroupBox : public QWidget {
public:
    QGroupBox(QWidget* p = nullptr) : QWidget(p) {} QGroupBox(const QString&, QWidget* p = nullptr) : QWidget(p) {}
    void setTitle(const QString&) {} void setCheckable(bool) {} bool isChecked() const { return false; } void setChecked(bool) {} void setLayout(QLayout*) {}
};
#endif
#ifndef QFRAME_H
#define QFRAME_H
class QFrame : public QWidget {
public:
    QFrame(QWidget* p = nullptr) : QWidget(p) {} void setFrameShape(int) {} void setFrameShadow(int) {} void setLineWidth(int) {} void setMidLineWidth(int) {}
    enum Shape { NoFrame, Box, Panel, StyledPanel, HLine, VLine, WinPanel }; enum Shadow { Plain, Raised, Sunken };
};
#endif
#ifndef QSPINBOX_H
#define QSPINBOX_H
class QSpinBox : public QWidget {
public: QSpinBox(QWidget* p = nullptr) : QWidget(p) {} void setValue(int v) { val_=v; } int value() const { return val_; }
    void setRange(int,int) {} void setMinimum(int) {} void setMaximum(int) {} void setSuffix(const QString&) {} void setPrefix(const QString&) {} void setSingleStep(int) {}
private: int val_=0;
};
class QDoubleSpinBox : public QWidget {
public: QDoubleSpinBox(QWidget* p = nullptr) : QWidget(p) {} void setValue(double v) { val_=v; } double value() const { return val_; }
    void setRange(double,double) {} void setDecimals(int) {} void setSuffix(const QString&) {} void setPrefix(const QString&) {} void setSingleStep(double) {}
private: double val_=0;
};
#endif
#ifndef QSLIDER_H
#define QSLIDER_H
class QSlider : public QWidget {
public: QSlider(QWidget* p = nullptr) : QWidget(p) {} QSlider(int, QWidget* p = nullptr) : QWidget(p) {}
    void setValue(int v) { val_=v; } int value() const { return val_; }
    void setRange(int,int) {} void setOrientation(int) {} void setTickPosition(int) {} void setTickInterval(int) {} void setSingleStep(int) {} void setPageStep(int) {}
private: int val_=0;
};
#endif
#ifndef QRADIOBUTTON_H
#define QRADIOBUTTON_H
class QRadioButton : public QWidget { public: QRadioButton(QWidget* p = nullptr) : QWidget(p) {} QRadioButton(const QString&, QWidget* p = nullptr) : QWidget(p) {} bool isChecked() const { return false; } void setChecked(bool) {} };
#endif
#ifndef QBUTTONGROUP_H
#define QBUTTONGROUP_H
class QButtonGroup : public QObject { public: QButtonGroup(QObject* p = nullptr) : QObject(p) {} void addButton(QWidget*, int=-1) {} void setExclusive(bool) {} int checkedId() const { return -1; } };
#endif
#ifndef QTOOLBUTTON_H
#define QTOOLBUTTON_H
class QToolButton : public QWidget {
public: QToolButton(QWidget* p = nullptr) : QWidget(p) {} void setDefaultAction(QAction*) {} void setPopupMode(int) {} void setMenu(QMenu*) {}
    void setIcon(const void*) {} void setToolButtonStyle(int) {} void setAutoRaise(bool) {} void setArrowType(int) {}
    enum ToolButtonPopupMode { DelayedPopup, MenuButtonPopup, InstantPopup };
};
#endif
#ifndef QTABBAR_H
#define QTABBAR_H
class QTabBar : public QWidget { public: QTabBar(QWidget* p = nullptr) : QWidget(p) {}
    int addTab(const QString&) { return 0; } void setCurrentIndex(int) {} int currentIndex() const { return 0; } int count() const { return 0; }
    void removeTab(int) {} void setTabText(int, const QString&) {} void setTabsClosable(bool) {} void setMovable(bool) {} void setDocumentMode(bool) {}
};
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 6: Threading / Timer / Process
// ═══════════════════════════════════════════════════════════════════════════════
#ifndef QTIMER_H
#define QTIMER_H
class QTimer : public QObject {
public: QTimer(QObject* p = nullptr) : QObject(p) {} void start(int) {} void stop() {} void setInterval(int) {} int interval() const { return 0; }
    bool isActive() const { return false; } void setSingleShot(bool) {} bool isSingleShot() const { return false; }
    static void singleShot(int, QObject*, const char*) {} int remainingTime() const { return 0; }
};
#endif
#ifndef QTHREAD_H
#define QTHREAD_H
class QThread : public QObject {
public: QThread(QObject* p = nullptr) : QObject(p) {} virtual void run() {} void start() {} void quit() {} void terminate() {}
    bool wait(unsigned long = ULONG_MAX) { return true; } bool isRunning() const { return false; } bool isFinished() const { return true; }
    void setPriority(int) {} static QThread* currentThread() { static QThread t; return &t; }
    static void msleep(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
    static void sleep(unsigned long s) { std::this_thread::sleep_for(std::chrono::seconds(s)); }
    void requestInterruption() {} bool isInterruptionRequested() const { return false; } void moveToThread(QThread*) {}
};
#endif
#ifndef QPROCESS_H
#define QPROCESS_H
class QProcess : public QObject {
public: QProcess(QObject* p = nullptr) : QObject(p) {} void start(const QString&, const QStringList& = {}) {} void start(const QString&, int) {}
    bool startDetached(const QString&, const QStringList& = {}) { return false; }
    bool waitForStarted(int = 30000) { return false; } bool waitForFinished(int = 30000) { return false; }
    QByteArray readAllStandardOutput() { return {}; } QByteArray readAllStandardError() { return {}; } QByteArray readAll() { return {}; }
    void write(const QByteArray&) {} void closeWriteChannel() {} int exitCode() const { return 0; } void kill() {} void terminate() {} int state() const { return 0; }
    void setWorkingDirectory(const QString&) {} void setProcessChannelMode(int) {} void setEnvironment(const QStringList&) {}
    static int execute(const QString&, const QStringList& = {}) { return 0; }
    enum ProcessState { NotRunning, Starting, Running }; enum ProcessChannel { StandardOutput, StandardError };
    enum ProcessChannelMode { SeparateChannels, MergedChannels, ForwardedChannels }; enum ExitStatus { NormalExit, CrashExit };
};
#endif
#ifndef QMUTEX_H
#define QMUTEX_H
class QMutex { std::mutex mtx_; public: void lock() { mtx_.lock(); } void unlock() { mtx_.unlock(); } bool tryLock() { return mtx_.try_lock(); } bool try_lock() { return mtx_.try_lock(); } };
class QMutexLocker { QMutex* m_; public: QMutexLocker(QMutex* m) : m_(m) { if(m_) m_->lock(); } ~QMutexLocker() { if(m_) m_->unlock(); } void unlock() { if(m_){m_->unlock();m_=nullptr;} } void relock() { if(m_) m_->lock(); } };
class QReadWriteLock { public: void lockForRead() {} void lockForWrite() {} void unlock() {} bool tryLockForRead() { return true; } bool tryLockForWrite() { return true; } };
class QWaitCondition { public: void wait(QMutex*, unsigned long = ULONG_MAX) {} void wakeOne() {} void wakeAll() {} };
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 7: File / Dir / IO / Network / Settings / Paths
// ═══════════════════════════════════════════════════════════════════════════════
using qint64 = int64_t; using qint32 = int32_t; using qint16 = int16_t; using qint8 = int8_t;
using quint64 = uint64_t; using quint32 = uint32_t; using quint16 = uint16_t; using quint8 = uint8_t;
using qreal = double; using qsizetype = ptrdiff_t;

#ifndef QFILE_H
#define QFILE_H
class QFile {
    std::string path_; std::fstream fs_;
public:
    QFile(const QString& path = {}) : path_(path) {}
    bool open(int mode) { (void)mode; fs_.open(path_, std::ios::in | std::ios::out); return fs_.is_open(); }
    void close() { fs_.close(); } bool exists() const { return std::filesystem::exists(path_); }
    static bool exists(const QString& path) { return std::filesystem::exists(std::string(path)); }
    QByteArray readAll() { std::stringstream ss; ss << fs_.rdbuf(); return QByteArray(ss.str()); }
    qint64 write(const QByteArray& data) { fs_.write(data.data(), data.size()); return data.size(); }
    bool remove() { return std::filesystem::remove(path_); } static bool remove(const QString& path) { return std::filesystem::remove(std::string(path)); }
    QString fileName() const { return QString(std::filesystem::path(path_).filename().string()); }
    qint64 size() const { try { return std::filesystem::file_size(path_); } catch(...) { return 0; } }
    bool isOpen() const { return fs_.is_open(); } void setFileName(const QString& f) { path_ = f; }
    bool copy(const QString& dest) { try { std::filesystem::copy_file(path_, std::string(dest)); return true; } catch(...) { return false; } }
    bool rename(const QString& newName) { try { std::filesystem::rename(path_, std::string(newName)); return true; } catch(...) { return false; } }
    enum OpenModeFlag { ReadOnly = 1, WriteOnly = 2, ReadWrite = 3, Append = 4, Truncate = 8, Text = 16 }; using OpenMode = int;
};
#endif
#ifndef QFILEINFO_H
#define QFILEINFO_H
class QFileInfo {
    std::filesystem::path p_;
public:
    QFileInfo(const QString& path = {}) : p_(std::string(path)) {}
    bool exists() const { return std::filesystem::exists(p_); } bool isFile() const { return std::filesystem::is_regular_file(p_); } bool isDir() const { return std::filesystem::is_directory(p_); }
    QString fileName() const { return QString(p_.filename().string()); } QString filePath() const { return QString(p_.string()); }
    QString absoluteFilePath() const { return QString(std::filesystem::absolute(p_).string()); }
    QString absolutePath() const { return QString(std::filesystem::absolute(p_).parent_path().string()); }
    QString suffix() const { auto e = p_.extension().string(); return e.empty() ? QString() : QString(e.substr(1)); }
    QString baseName() const { return QString(p_.stem().string()); } QString path() const { return QString(p_.parent_path().string()); }
    qint64 size() const { try { return std::filesystem::file_size(p_); } catch(...) { return 0; } }
    bool isReadable() const { return true; } bool isWritable() const { return true; } bool isSymLink() const { return std::filesystem::is_symlink(p_); }
    QString canonicalFilePath() const { try { return QString(std::filesystem::canonical(p_).string()); } catch(...) { return filePath(); } }
};
#endif
#ifndef QDIR_H
#define QDIR_H
class QChar { char c_; public: QChar():c_(0){} QChar(char c):c_(c){} QChar(int c):c_((char)c){} char toLatin1() const{return c_;} bool isNull() const{return c_==0;} bool isLetter() const{return std::isalpha(c_);} bool isDigit() const{return std::isdigit(c_);} bool isSpace() const{return std::isspace(c_);} operator char() const{return c_;} };
class QDir {
    std::filesystem::path p_;
public:
    QDir(const QString& path = ".") : p_(std::string(path)) {}
    bool exists() const { return std::filesystem::exists(p_); }
    bool mkpath(const QString& dir) const { try { return std::filesystem::create_directories(std::string(dir)); } catch(...) { return false; } }
    bool mkdir(const QString& dir) const { try { return std::filesystem::create_directory(p_ / std::string(dir)); } catch(...) { return false; } }
    QString absolutePath() const { return QString(std::filesystem::absolute(p_).string()); } QString path() const { return QString(p_.string()); }
    QStringList entryList(int = 0) const { QStringList r; try { for(auto& e : std::filesystem::directory_iterator(p_)) r.push_back(QString(e.path().filename().string())); } catch(...) {} return r; }
    static QString homePath() { char* h = getenv("USERPROFILE"); return h ? QString(h) : QString("C:\\Users"); }
    static QString tempPath() { return QString(std::filesystem::temp_directory_path().string()); }
    static QString currentPath() { return QString(std::filesystem::current_path().string()); }
    static QChar separator() { return QChar((char)std::filesystem::path::preferred_separator); }
    enum Filter { Dirs=1, Files=2, NoSymLinks=4, NoDotAndDotDot=0x1000, AllEntries=7 }; enum SortFlag { Name=0, Time=1, Size=2, Type=4 };
};
#endif
#ifndef QIODEVICE_H
#define QIODEVICE_H
class QIODevice : public QObject {
public: QIODevice(QObject* p = nullptr) : QObject(p) {} virtual bool open(int) { return false; } virtual void close() {} virtual bool isOpen() const { return false; }
    virtual QByteArray readAll() { return {}; } virtual qint64 write(const QByteArray&) { return 0; } virtual qint64 read(char*, qint64) { return 0; }
    enum OpenModeFlag { ReadOnly=1, WriteOnly=2, ReadWrite=3, Append=4, Text=16 };
};
#endif
#ifndef QSETTINGS_H
#define QSETTINGS_H
class QSettings { std::map<std::string, QVariant> store_;
public: QSettings(const QString& = {}, const QString& = {}) {} QSettings(int, int, const QString& = {}, const QString& = {}) {}
    void setValue(const QString& key, const QVariant& val) { store_[key] = val; }
    QVariant value(const QString& key, const QVariant& def = {}) const { auto it = store_.find(key); return it != store_.end() ? it->second : def; }
    bool contains(const QString& key) const { return store_.find(key) != store_.end(); } void remove(const QString& key) { store_.erase(key); }
    void beginGroup(const QString&) {} void endGroup() {} QStringList allKeys() const { QStringList r; for(auto& p : store_) r.push_back(QString(p.first)); return r; } void sync() {}
    enum Scope { UserScope, SystemScope }; enum Format { NativeFormat, IniFormat, InvalidFormat };
};
#endif
#ifndef QSTANDARDPATHS_H
#define QSTANDARDPATHS_H
class QStandardPaths {
public:
    enum StandardLocation { DesktopLocation, HomeLocation, TempLocation, AppDataLocation, CacheLocation, GenericDataLocation, AppConfigLocation, ConfigLocation, DownloadLocation, DocumentsLocation };
    static QString writableLocation(int loc) {
        switch(loc) { case TempLocation: return QString(std::filesystem::temp_directory_path().string());
            case HomeLocation: { char* h=getenv("USERPROFILE"); return h?QString(h):QString("C:\\Users"); }
            case AppDataLocation: case AppConfigLocation: { char* a=getenv("APPDATA"); return a?QString(a):QString("C:\\"); }
            case CacheLocation: { char* l=getenv("LOCALAPPDATA"); return l?QString(l):QString("C:\\"); }
            default: return QString("."); } }
    static QStringList standardLocations(int loc) { return { writableLocation(loc) }; }
};
#endif
#ifndef QURL_H
#define QURL_H
class QUrl { QString url_;
public: QUrl() = default; QUrl(const QString& url) : url_(url) {}
    static QUrl fromLocalFile(const QString& path) { return QUrl("file:///" + path); }
    QString toString() const { return url_; } QString toLocalFile() const { auto s=url_; if(s.startsWith("file:///")) s=s.mid(8); return s; }
    QString scheme() const { auto i=url_.indexOf(":"); return i>=0?url_.left(i):QString(); }
    QString host() const { return {}; } int port() const { return -1; } QString path() const { return toLocalFile(); }
    bool isValid() const { return !url_.isEmpty(); } bool isEmpty() const { return url_.isEmpty(); } bool isLocalFile() const { return url_.startsWith("file://"); }
    void setScheme(const QString&) {} void setHost(const QString&) {} void setPort(int) {} void setPath(const QString& p) { url_ = p; }
    QString toEncoded() const { return url_; }
};
#endif

// ─── Network Stubs ──────────────────────────────────────────────────────────
#ifndef QNETWORKACCESSMANAGER_H
#define QNETWORKACCESSMANAGER_H
class QNetworkRequest { QUrl url_; public: QNetworkRequest(const QUrl& url = {}) : url_(url) {} QUrl url() const { return url_; } void setUrl(const QUrl& u) { url_=u; }
    void setRawHeader(const QByteArray&, const QByteArray&) {} void setHeader(int, const QVariant&) {} QByteArray rawHeader(const QByteArray&) const { return {}; }
    enum KnownHeaders { ContentTypeHeader, ContentLengthHeader, LocationHeader, UserAgentHeader }; };
class QNetworkReply : public QObject { public: QNetworkReply(QObject* p = nullptr) : QObject(p) {} QByteArray readAll() { return {}; } int error() const { return 0; }
    QUrl url() const { return {}; } int attribute(int) const { return 0; } void abort() {} bool isFinished() const { return true; } QByteArray rawHeader(const QByteArray&) const { return {}; } int statusCode() const { return 200; }
    enum NetworkError { NoError=0, ConnectionRefusedError, RemoteHostClosedError, HostNotFoundError, TimeoutError, ContentNotFoundError, UnknownNetworkError=99 }; };
class QNetworkAccessManager : public QObject { public: QNetworkAccessManager(QObject* p = nullptr) : QObject(p) {}
    QNetworkReply* get(const QNetworkRequest&) { return nullptr; } QNetworkReply* post(const QNetworkRequest&, const QByteArray&) { return nullptr; }
    QNetworkReply* put(const QNetworkRequest&, const QByteArray&) { return nullptr; } QNetworkReply* deleteResource(const QNetworkRequest&) { return nullptr; } QNetworkReply* head(const QNetworkRequest&) { return nullptr; } };
#endif
#ifndef QNETWORKREQUEST_H
#define QNETWORKREQUEST_H
#endif
#ifndef QNETWORKREPLY_H
#define QNETWORKREPLY_H
#endif
#ifndef QTCPSOCKET_H
#define QTCPSOCKET_H
class QTcpSocket : public QObject { public: QTcpSocket(QObject* p = nullptr) : QObject(p) {} void connectToHost(const QString&, int) {} void disconnectFromHost() {}
    bool waitForConnected(int = 30000) { return false; } bool waitForReadyRead(int = 30000) { return false; } QByteArray readAll() { return {}; } qint64 write(const QByteArray&) { return 0; }
    void close() {} bool isOpen() const { return false; } int state() const { return 0; } enum SocketState { UnconnectedState, ConnectedState, BoundState, ClosingState, ListeningState }; };
class QTcpServer : public QObject { public: QTcpServer(QObject* p = nullptr) : QObject(p) {} bool listen(const void* = nullptr, int = 0) { return false; } void close() {}
    bool isListening() const { return false; } QTcpSocket* nextPendingConnection() { return nullptr; } int serverPort() const { return 0; } };
#endif
#ifndef QWEBSOCKET_H
#define QWEBSOCKET_H
class QWebSocket : public QObject { public: QWebSocket(const QString& = {}, int = 0, QObject* p = nullptr) : QObject(p) {} void open(const QUrl&) {} void close() {}
    qint64 sendTextMessage(const QString&) { return 0; } qint64 sendBinaryMessage(const QByteArray&) { return 0; } bool isValid() const { return false; } QUrl requestUrl() const { return {}; } int state() const { return 0; } };
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 8: JSON Types
// ═══════════════════════════════════════════════════════════════════════════════
#ifndef QJSONVALUE_H
#define QJSONVALUE_H
class QJsonValue; class QJsonArray; class QJsonObject;
class QJsonObject : public std::map<QString, QJsonValue> { public:
    bool contains(const QString& key) const { return find(key) != end(); }
    QJsonValue value(const QString& key) const; void insert(const QString& key, const QJsonValue& val);
    QStringList keys() const { QStringList r; for(auto& p : *this) r.push_back(p.first); return r; }
    bool isEmpty() const { return empty(); } int count() const { return (int)size(); } void remove(const QString& key) { erase(key); } };
class QJsonArray : public std::vector<QJsonValue> { public: using std::vector<QJsonValue>::vector;
    int count() const { return (int)size(); } bool isEmpty() const { return empty(); } void append(const QJsonValue& v) { push_back(v); } };
class QJsonValue {
    enum Type { Null, Bool, Double, String, Array, Object } type_ = Null;
    double dval_ = 0; bool bval_ = false; QString sval_; QJsonObject oval_; QJsonArray aval_;
public:
    QJsonValue() : type_(Null) {} QJsonValue(bool v) : type_(Bool), bval_(v) {} QJsonValue(int v) : type_(Double), dval_(v) {}
    QJsonValue(double v) : type_(Double), dval_(v) {} QJsonValue(const QString& v) : type_(String), sval_(v) {} QJsonValue(const char* v) : type_(String), sval_(v) {}
    QJsonValue(const QJsonObject& v) : type_(Object), oval_(v) {} QJsonValue(const QJsonArray& v) : type_(Array), aval_(v) {}
    bool isNull() const { return type_ == Null; } bool isBool() const { return type_ == Bool; } bool isDouble() const { return type_ == Double; }
    bool isString() const { return type_ == String; } bool isArray() const { return type_ == Array; } bool isObject() const { return type_ == Object; } bool isUndefined() const { return type_ == Null; }
    bool toBool(bool def = false) const { return type_ == Bool ? bval_ : def; } double toDouble(double def = 0) const { return type_ == Double ? dval_ : def; }
    int toInt(int def = 0) const { return type_ == Double ? (int)dval_ : def; } QString toString(const QString& def = {}) const { return type_ == String ? sval_ : def; }
    QJsonObject toObject() const { return type_ == Object ? oval_ : QJsonObject(); } QJsonArray toArray() const { return type_ == Array ? aval_ : QJsonArray(); }
};
inline QJsonValue QJsonObject::value(const QString& key) const { auto it = find(key); return it != end() ? it->second : QJsonValue(); }
inline void QJsonObject::insert(const QString& key, const QJsonValue& val) { (*this)[key] = val; }
class QJsonDocument { QJsonObject obj_; QJsonArray arr_; bool isObj_ = true;
public: QJsonDocument() = default; QJsonDocument(const QJsonObject& o) : obj_(o), isObj_(true) {} QJsonDocument(const QJsonArray& a) : arr_(a), isObj_(false) {}
    static QJsonDocument fromJson(const QByteArray&, void* = nullptr) { return {}; } QByteArray toJson(int = 0) const { return QByteArray("{}"); }
    QJsonObject object() const { return obj_; } QJsonArray array() const { return arr_; }
    bool isObject() const { return isObj_; } bool isArray() const { return !isObj_; } bool isNull() const { return obj_.empty() && arr_.empty(); }
    enum JsonFormat { Indented, Compact }; };
#endif
#ifndef QJSONOBJECT_H
#define QJSONOBJECT_H
#endif
#ifndef QJSONARRAY_H
#define QJSONARRAY_H
#endif
#ifndef QJSONDOCUMENT_H
#define QJSONDOCUMENT_H
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 9: Graphics / Painting / Misc
// ═══════════════════════════════════════════════════════════════════════════════
#ifndef QCOLOR_H
#define QCOLOR_H
class QColor { int r_=0,g_=0,b_=0,a_=255; public: QColor()=default; QColor(int r,int g,int b,int a=255):r_(r),g_(g),b_(b),a_(a){} QColor(const char*){} QColor(const QString&){}
    int red() const{return r_;} int green() const{return g_;} int blue() const{return b_;} int alpha() const{return a_;} bool isValid() const{return true;} QString name() const{return QString("#000000");}
    static QColor fromRgb(int r,int g,int b,int a=255){return QColor(r,g,b,a);} };
#endif
#ifndef QFONT_H
#define QFONT_H
class QFont { public: QFont()=default; QFont(const QString&,int=-1,int=-1,bool=false){}
    void setFamily(const QString&){} void setPointSize(int){} void setPixelSize(int){} void setBold(bool){} void setItalic(bool){} void setWeight(int){} void setFixedPitch(bool){} void setStyleHint(int){}
    QString family() const{return{};} int pointSize() const{return 12;} bool bold() const{return false;} bool italic() const{return false;}
    enum Weight{Thin=100,Light=300,Normal=400,Bold=700,Black=900}; enum StyleHint{AnyStyle,SansSerif,Serif,TypeWriter,Monospace,Cursive,Fantasy,System}; };
#endif
#ifndef QICON_H
#define QICON_H
class QIcon{public:QIcon()=default;QIcon(const QString&){}bool isNull()const{return true;}static QIcon fromTheme(const QString&){return{};}};
#endif
#ifndef QPAINTER_H
#define QPAINTER_H
class QPainter{public:QPainter()=default;QPainter(QWidget*){}void begin(QWidget*){}void end(){}void drawText(int,int,const QString&){}void drawRect(int,int,int,int){}
    void drawLine(int,int,int,int){}void drawEllipse(int,int,int,int){}void fillRect(int,int,int,int,const QColor&){}void setPen(const QColor&){}void setBrush(const QColor&){}
    void setFont(const QFont&){}void save(){}void restore(){}void setRenderHint(int,bool=true){}enum RenderHint{Antialiasing=1,TextAntialiasing=2,SmoothPixmapTransform=4};};
#endif
#ifndef QKEYSEQUENCE_H
#define QKEYSEQUENCE_H
class QKeySequence{public:QKeySequence()=default;QKeySequence(const QString&){}QKeySequence(int){}QString toString()const{return{};}static QKeySequence fromString(const QString&){return{};}};
#endif
#ifndef QKEYEVENT_H
#define QKEYEVENT_H
class QKeyEvent{public:int key()const{return 0;}int modifiers()const{return 0;}QString text()const{return{};}bool isAutoRepeat()const{return false;}void accept(){}void ignore(){}};
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 10: Filesystem Models / Watchers / Dialogs / Misc
// ═══════════════════════════════════════════════════════════════════════════════
#ifndef QFILESYSTEMMODEL_H
#define QFILESYSTEMMODEL_H
class QModelIndex{public:bool isValid()const{return false;}int row()const{return-1;}int column()const{return-1;}QModelIndex parent()const{return{};}void* internalPointer()const{return nullptr;}};
class QAbstractItemModel:public QObject{public:QAbstractItemModel(QObject*p=nullptr):QObject(p){}virtual QModelIndex index(int,int,const QModelIndex&={})const{return{};}
    virtual QModelIndex parent(const QModelIndex&)const{return{};}virtual int rowCount(const QModelIndex&={})const{return 0;}virtual int columnCount(const QModelIndex&={})const{return 0;}
    virtual QVariant data(const QModelIndex&,int=0)const{return{};}virtual bool setData(const QModelIndex&,const QVariant&,int=0){return false;}};
class QFileSystemModel:public QAbstractItemModel{public:QFileSystemModel(QObject*p=nullptr):QAbstractItemModel(p){}
    QModelIndex setRootPath(const QString&){return{};}QString filePath(const QModelIndex&)const{return{};}QString fileName(const QModelIndex&)const{return{};}
    bool isDir(const QModelIndex&)const{return false;}QModelIndex index(const QString&,int=0)const{return{};}QModelIndex index(int,int,const QModelIndex&={})const override{return{};}
    void setFilter(int){}void setNameFilters(const QStringList&){}void setNameFilterDisables(bool){}void setReadOnly(bool){}qint64 size(const QModelIndex&)const{return 0;}};
#endif
#ifndef QFILESYSTEMWATCHER_H
#define QFILESYSTEMWATCHER_H
class QFileSystemWatcher:public QObject{public:QFileSystemWatcher(QObject*p=nullptr):QObject(p){}QFileSystemWatcher(const QStringList&,QObject*p=nullptr):QObject(p){}
    bool addPath(const QString&){return true;}bool addPaths(const QStringList&){return true;}bool removePath(const QString&){return true;}bool removePaths(const QStringList&){return true;}
    QStringList files()const{return{};}QStringList directories()const{return{};}};
#endif
#ifndef QFILEDIALOG_H
#define QFILEDIALOG_H
class QFileDialog{public:static QString getOpenFileName(QWidget*=nullptr,const QString&={},const QString&={},const QString&={}){return{};}
    static QString getSaveFileName(QWidget*=nullptr,const QString&={},const QString&={},const QString&={}){return{};}
    static QString getExistingDirectory(QWidget*=nullptr,const QString&={},const QString&={}){return{};}
    static QStringList getOpenFileNames(QWidget*=nullptr,const QString&={},const QString&={},const QString&={}){return{};}};
#endif
#ifndef QDATETIME_H
#define QDATETIME_H
class QDateTime{public:QDateTime()=default;static QDateTime currentDateTime(){return{};}static QDateTime currentDateTimeUtc(){return{};}
    QString toString(const QString&={})const{return{};}qint64 toMSecsSinceEpoch()const{return 0;}static QDateTime fromMSecsSinceEpoch(qint64){return{};}bool isValid()const{return false;}};
class QDateTimeAxis{};class QValueAxis{};
#endif
#ifndef QUUID_H
#define QUUID_H
class QUuid{public:QUuid()=default;static QUuid createUuid(){return{};}QString toString()const{return{};}bool isNull()const{return true;}};
#endif
#ifndef QREGULAREXPRESSION_H
#define QREGULAREXPRESSION_H
class QRegularExpressionMatch{public:bool hasMatch()const{return false;}QString captured(int=0)const{return{};}int capturedStart(int=0)const{return-1;}int capturedLength(int=0)const{return 0;}};
class QRegularExpression{QString p_;public:QRegularExpression()=default;QRegularExpression(const QString&p):p_(p){}QRegularExpressionMatch match(const QString&)const{return{};}QString pattern()const{return p_;}bool isValid()const{return true;}
    enum PatternOption{NoPatternOption=0,CaseInsensitiveOption=1,DotMatchesEverythingOption=2,MultilineOption=4};};
#endif
#ifndef QTEXTSTREAM_H
#define QTEXTSTREAM_H
class QTextStream{public:QTextStream()=default;QTextStream(QFile*){}QTextStream(QByteArray*){}QTextStream(QString*){}
    QString readAll(){return{};}QString readLine(){return{};}bool atEnd()const{return true;}
    QTextStream&operator<<(const QString&){return*this;}QTextStream&operator<<(const char*){return*this;}QTextStream&operator<<(int){return*this;}QTextStream&operator<<(double){return*this;}void flush(){}void setCodec(const char*){}};
#endif
#ifndef QTEXTDOCUMENT_H
#define QTEXTDOCUMENT_H
class QTextDocument:public QObject{public:QTextDocument(QObject*p=nullptr):QObject(p){}QTextDocument(const QString&,QObject*p=nullptr):QObject(p){}
    QString toPlainText()const{return{};}QString toHtml()const{return{};}void setPlainText(const QString&){}void setHtml(const QString&){}
    bool isModified()const{return false;}void setModified(bool){}int blockCount()const{return 0;}void clear(){}bool isEmpty()const{return true;}};
class QTextCursor{public:QTextCursor()=default;QTextCursor(QTextDocument*){}void insertText(const QString&){}void movePosition(int,int=0,int=0){}
    bool hasSelection()const{return false;}QString selectedText()const{return{};}void clearSelection(){}int position()const{return 0;}void setPosition(int,int=0){}
    int blockNumber()const{return 0;}int columnNumber()const{return 0;}void select(int){}void beginEditBlock(){}void endEditBlock(){}
    enum MoveMode{MoveAnchor,KeepAnchor};enum MoveOperation{NoMove,Start,End,StartOfLine,EndOfLine,Up,Down,Left,Right,WordLeft,WordRight,StartOfBlock,EndOfBlock};
    enum SelectionType{WordUnderCursor,LineUnderCursor,BlockUnderCursor,Document};};
#endif
#ifndef QCRYPTOGRAPHICHASH_H
#define QCRYPTOGRAPHICHASH_H
class QCryptographicHash{public:enum Algorithm{Md5,Sha1,Sha256,Sha512};QCryptographicHash(Algorithm){}void addData(const QByteArray&){}void addData(const char*,int){}QByteArray result()const{return{};}void reset(){}static QByteArray hash(const QByteArray&,Algorithm){return{};}};
#endif
#ifndef QSHAREDPOINTER_H
#define QSHAREDPOINTER_H
template<typename T>using QSharedPointer=std::shared_ptr<T>;template<typename T>using QScopedPointer=std::unique_ptr<T>;template<typename T>using QWeakPointer=std::weak_ptr<T>;
#endif
#ifndef QMIMEDATA_H
#define QMIMEDATA_H
class QMimeData:public QObject{public:QMimeData()=default;bool hasText()const{return false;}bool hasUrls()const{return false;}bool hasFormat(const QString&)const{return false;}
    QString text()const{return{};}void setText(const QString&){}QList<QUrl> urls()const{return{};}void setUrls(const QList<QUrl>&){}
    QByteArray data(const QString&)const{return{};}void setData(const QString&,const QByteArray&){}};
#endif
#ifndef QSYNTAXHIGHLIGHTER_H
#define QSYNTAXHIGHLIGHTER_H
class QSyntaxHighlighter:public QObject{public:QSyntaxHighlighter(QObject*p=nullptr):QObject(p){}QSyntaxHighlighter(QTextDocument*):QObject(nullptr){}
    virtual void highlightBlock(const QString&){}void rehighlight(){}void rehighlightBlock(void*){}QTextDocument*document()const{return nullptr;}void setDocument(QTextDocument*){}
protected:void setFormat(int,int,const QColor&){}void setFormat(int,int,const QFont&){}void setCurrentBlockState(int){}int currentBlockState()const{return-1;}int previousBlockState()const{return-1;}};
#endif
#ifndef QABSTRACTITEMVIEW_H
#define QABSTRACTITEMVIEW_H
class QAbstractItemView:public QWidget{public:QAbstractItemView(QWidget*p=nullptr):QWidget(p){}
    enum SelectionMode{NoSelection,SingleSelection,MultiSelection,ExtendedSelection,ContiguousSelection};
    enum EditTrigger{NoEditTriggers=0,CurrentChanged=1,DoubleClicked=2,AllEditTriggers=31};enum DragDropMode{NoDragDrop,DragOnly,DropOnly,DragDrop,InternalMove};};
#endif

// ─── Application ────────────────────────────────────────────────────────────
#ifndef QAPPLICATION_H
#define QAPPLICATION_H
class QCoreApplication{public:QCoreApplication(int&,char**){}static QCoreApplication*instance(){return nullptr;}static void processEvents(){}static int exec(){return 0;}
    static void quit(){}static void exit(int=0){}static QString applicationDirPath(){return{};}static QString applicationFilePath(){return{};}
    static QString applicationName(){return{};}static void setApplicationName(const QString&){}static void setApplicationVersion(const QString&){}
    static void setOrganizationName(const QString&){}static void setOrganizationDomain(const QString&){}static QStringList arguments(){return{};}static void setAttribute(int,bool=true){}};
class QApplication:public QCoreApplication{public:QApplication(int&argc,char**argv):QCoreApplication(argc,argv){}
    static void setStyle(const QString&){}static void setFont(const QFont&){}static QFont font(){return{};}static void setWindowIcon(const QIcon&){}
    static void beep(){}static QWidget*focusWidget(){return nullptr;}static QWidget*activeWindow(){return nullptr;}
    static void setOverrideCursor(int){}static void restoreOverrideCursor(){}static QList<QWidget*>allWidgets(){return{};}static QList<QWidget*>topLevelWidgets(){return{};}};
#endif

// ─── Rare stubs ─────────────────────────────────────────────────────────────
#ifndef QCHART_H
#define QCHART_H
class QChart{};class QChartView:public QWidget{public:QChartView(QWidget*p=nullptr):QWidget(p){}};class QLineSeries{};class QBarSeries{};
#endif
#ifndef QAUDIODEVICE_H
#define QAUDIODEVICE_H
class QAudioDevice{};class QAudioDeviceInfo{};class QAudioSource{};class QMediaDevices{};
#endif
#ifndef QWEBENGINEVIEW_H
#define QWEBENGINEVIEW_H
class QWebEngineView:public QWidget{public:QWebEngineView(QWidget*p=nullptr):QWidget(p){}void setUrl(const QUrl&){}void load(const QUrl&){}};
class QWebChannel:public QObject{public:QWebChannel(QObject*=nullptr){}void registerObject(const QString&,QObject*){}};
#endif
#ifndef QSYSINFO_H
#define QSYSINFO_H
class QSysInfo{public:static QString productType(){return"windows";}static QString prettyProductName(){return"Windows";}static QString currentCpuArchitecture(){return"x86_64";}
    static QString kernelType(){return"winnt";}static QString kernelVersion(){return{};}static QString machineHostName(){return{};}};
#endif
#ifndef QSYSTEMTRAYICON_H
#define QSYSTEMTRAYICON_H
class QSystemTrayIcon:public QObject{public:QSystemTrayIcon(QObject*p=nullptr):QObject(p){}QSystemTrayIcon(const QIcon&,QObject*p=nullptr):QObject(p){}
    void show(){}void hide(){}void setIcon(const QIcon&){}void setToolTip(const QString&){}void setContextMenu(QMenu*){}
    void showMessage(const QString&,const QString&,int=0,int=10000){}bool isVisible()const{return false;}static bool isSystemTrayAvailable(){return false;}
    enum MessageIcon{NoIcon,Information,Warning,Critical};};
#endif
#ifndef QLIBRARY_H
#define QLIBRARY_H
class QLibrary{public:QLibrary(const QString&={}){}bool load(){return false;}bool unload(){return true;}bool isLoaded()const{return false;}void*resolve(const char*){return nullptr;}void setFileName(const QString&){}QString errorString()const{return{};}};
#endif
#ifndef QTEMPORARYDIR_H
#define QTEMPORARYDIR_H
class QTemporaryDir{public:QTemporaryDir()=default;bool isValid()const{return true;}QString path()const{return QDir::tempPath();}void setAutoRemove(bool){}};
#endif
#ifndef QSAVEFILE_H
#define QSAVEFILE_H
class QSaveFile:public QFile{public:QSaveFile(const QString&name={}):QFile(name){}bool commit(){return true;}void cancelWriting(){}};
#endif
#ifndef QSQLDATABASE_H
#define QSQLDATABASE_H
class QSqlDatabase{public:static QSqlDatabase addDatabase(const QString&){return{};}void setDatabaseName(const QString&){}bool open(){return false;}void close(){}bool isOpen()const{return false;}QString lastError()const{return{};}};
#endif
#ifndef QPROGRESSDIALOG_H
#define QPROGRESSDIALOG_H
class QProgressDialog:public QDialog{public:QProgressDialog(QWidget*p=nullptr):QDialog(p){}QProgressDialog(const QString&,const QString&,int,int,QWidget*=nullptr):QDialog(){}
    void setValue(int){}void setLabelText(const QString&){}void setCancelButtonText(const QString&){}void setRange(int,int){}void setMinimumDuration(int){}bool wasCanceled()const{return false;}void cancel(){}void reset(){}};
#endif
#ifndef QUNDOSTACK_H
#define QUNDOSTACK_H
class QUndoStack:public QObject{public:QUndoStack(QObject*p=nullptr):QObject(p){}void push(void*){}void undo(){}void redo(){}bool canUndo()const{return false;}bool canRedo()const{return false;}
    void clear(){}void setClean(){}bool isClean()const{return true;}QAction*createUndoAction(QObject*,const QString&={}){return nullptr;}QAction*createRedoAction(QObject*,const QString&={}){return nullptr;}};
class QUndoGroup:public QObject{public:QUndoGroup(QObject*p=nullptr):QObject(p){}void addStack(QUndoStack*){}void setActiveStack(QUndoStack*){}QUndoStack*activeStack()const{return nullptr;}};
class QUndoView:public QWidget{public:QUndoView(QWidget*p=nullptr):QWidget(p){}void setStack(QUndoStack*){}void setGroup(QUndoGroup*){}};
#endif
#ifndef QGRAPHICSVIEW_H
#define QGRAPHICSVIEW_H
class QGraphicsView:public QWidget{public:QGraphicsView(QWidget*p=nullptr):QWidget(p){}};
#endif
#ifndef QTABLEWIDGET_H
#define QTABLEWIDGET_H
class QTableWidget:public QWidget{public:QTableWidget(QWidget*p=nullptr):QWidget(p){}QTableWidget(int,int,QWidget*p=nullptr):QWidget(p){}
    void setRowCount(int){}void setColumnCount(int){}int rowCount()const{return 0;}int columnCount()const{return 0;}
    void setHorizontalHeaderLabels(const QStringList&){}void setVerticalHeaderLabels(const QStringList&){}void setItem(int,int,void*){}void clear(){}};
#endif
#ifndef QTEXTCODEC_H
#define QTEXTCODEC_H
class QTextCodec{public:static QTextCodec*codecForName(const char*){return nullptr;}QByteArray fromUnicode(const QString&)const{return{};}QString toUnicode(const QByteArray&)const{return{};}};
#endif
#ifndef QFUTUREWATCHER_H
#define QFUTUREWATCHER_H
template<typename T>class QFutureWatcher:public QObject{public:QFutureWatcher(QObject*p=nullptr):QObject(p){}void setFuture(const void*){}T result()const{return T{};}bool isFinished()const{return true;}bool isRunning()const{return false;}void cancel(){}void waitForFinished(){}};
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 11: Debug / Logging → OutputDebugString + stderr
// ═══════════════════════════════════════════════════════════════════════════════
#ifndef QDEBUG_H
#define QDEBUG_H
class QDebug{std::ostringstream ss_;public:QDebug()=default;~QDebug(){
#ifdef _WIN32
    OutputDebugStringA(ss_.str().c_str());
#endif
    std::cerr<<ss_.str()<<std::endl;}
    QDebug&operator<<(const char*s){ss_<<s<<" ";return*this;}QDebug&operator<<(const std::string&s){ss_<<s<<" ";return*this;}
    QDebug&operator<<(const QString&s){ss_<<std::string(s)<<" ";return*this;}QDebug&operator<<(int v){ss_<<v<<" ";return*this;}
    QDebug&operator<<(unsigned int v){ss_<<v<<" ";return*this;}QDebug&operator<<(long v){ss_<<v<<" ";return*this;}
    QDebug&operator<<(unsigned long v){ss_<<v<<" ";return*this;}QDebug&operator<<(long long v){ss_<<v<<" ";return*this;}
    QDebug&operator<<(unsigned long long v){ss_<<v<<" ";return*this;}QDebug&operator<<(double v){ss_<<v<<" ";return*this;}
    QDebug&operator<<(bool v){ss_<<(v?"true":"false")<<" ";return*this;}QDebug&operator<<(const void*v){ss_<<v<<" ";return*this;}
    QDebug&nospace(){return*this;}QDebug&noquote(){return*this;}QDebug&space(){return*this;}QDebug&quote(){return*this;}QDebug&maybeSpace(){return*this;}};
inline QDebug qDebug(){return QDebug();}inline QDebug qInfo(){return QDebug();}inline QDebug qWarning(){return QDebug();}inline QDebug qCritical(){return QDebug();}
#ifndef QStringLiteral
#define QStringLiteral(x) QString(x)
#endif
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 12: Qt:: namespace constants
// ═══════════════════════════════════════════════════════════════════════════════
namespace Qt {
    enum Orientation{Horizontal=1,Vertical=2};
    enum AlignmentFlag{AlignLeft=1,AlignRight=2,AlignHCenter=4,AlignTop=0x20,AlignBottom=0x40,AlignVCenter=0x80,AlignCenter=AlignHCenter|AlignVCenter};
    enum ItemFlag{NoItemFlags=0,ItemIsSelectable=1,ItemIsEditable=2,ItemIsDragEnabled=4,ItemIsDropEnabled=8,ItemIsUserCheckable=16,ItemIsEnabled=32,ItemIsTristate=64};
    enum CheckState{Unchecked=0,PartiallyChecked=1,Checked=2};
    enum ScrollBarPolicy{ScrollBarAsNeeded=0,ScrollBarAlwaysOff=1,ScrollBarAlwaysOn=2};
    enum ContextMenuPolicy{NoContextMenu=0,DefaultContextMenu=1,ActionsContextMenu=2,CustomContextMenu=3,PreventContextMenu=4};
    enum CursorShape{ArrowCursor=0,UpArrowCursor=1,CrossCursor=2,WaitCursor=3,BusyCursor=16,PointingHandCursor=13,ForbiddenCursor=14};
    enum WidgetAttribute{WA_DeleteOnClose=55,WA_TranslucentBackground=120,WA_StyledBackground=95};
    enum WindowType{Widget=0,Window=1,Dialog=3,Popup=9,FramelessWindowHint=0x800,WindowStaysOnTopHint=0x40000};
    enum ItemDataRole{DisplayRole=0,EditRole=2,DecorationRole=1,ToolTipRole=3,StatusTipRole=4,WhatsThisRole=5,UserRole=0x100};
    enum SortOrder{AscendingOrder=0,DescendingOrder=1};
    enum Key{Key_Return=0x01000004,Key_Enter=0x01000005,Key_Escape=0x01000000,Key_Tab=0x01000001,Key_Backspace=0x01000003,Key_Delete=0x01000007,Key_Space=0x20};
    enum KeyboardModifier{NoModifier=0,ShiftModifier=0x02000000,ControlModifier=0x04000000,AltModifier=0x08000000,MetaModifier=0x10000000};
    enum DockWidgetArea{LeftDockWidgetArea=1,RightDockWidgetArea=2,TopDockWidgetArea=4,BottomDockWidgetArea=8,AllDockWidgetAreas=0xFF};
    enum ToolBarArea{LeftToolBarArea=1,RightToolBarArea=2,TopToolBarArea=4,BottomToolBarArea=8,AllToolBarAreas=0xFF};
    enum ToolButtonStyle{ToolButtonIconOnly=0,ToolButtonTextOnly=1,ToolButtonTextBesideIcon=2,ToolButtonTextUnderIcon=3};
    enum WindowModality{NonModal=0,WindowModal=1,ApplicationModal=2};
    enum ConnectionType{AutoConnection=0,DirectConnection=1,QueuedConnection=2,UniqueConnection=0x80};
    enum FocusPolicy{NoFocus=0,TabFocus=1,ClickFocus=2,StrongFocus=11,WheelFocus=15};
    enum TextFormat{PlainText=0,RichText=1,AutoText=2,MarkdownText=3};
}
using namespace Qt;

#endif // RAWRXD_QT_BLOCK_H
