#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// qt_block.h — Reverse-Engineered Qt Blocking Shim
// ═══════════════════════════════════════════════════════════════════════════════
// PURPOSE: Intercept and neutralize ALL Qt symbol references at compile time.
//          This header defines every Qt class, macro, and keyword as a no-op
//          stub so that legacy Qt-contaminated code compiles cleanly under
//          the native Win32/C++20 RawrXD build without Qt SDK installed.
//
// STRATEGY: "Block, don't bridge."
//   - Qt classes → empty structs / minimal stubs with std:: backing
//   - Qt macros  → preprocessor no-ops
//   - signals/slots/emit → neutralized keywords
//   - Inheritance from QObject/QWidget → stripped by preprocessor in headers
//
// This file is force-included via /FI in CMake before any source code.
// ═══════════════════════════════════════════════════════════════════════════════

#ifndef RAWRXD_QT_BLOCK_H
#define RAWRXD_QT_BLOCK_H

// ─────────────────────────────────────────────────────────────────────────────
// Standard library includes — MUST come first since this is force-included
// before any source file and std:: types are needed for the stubs below.
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cctype>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <type_traits>
#include <filesystem>

// Ensure we are in Qt-eliminated mode
#ifndef RAWRXD_QT_ELIMINATED
#define RAWRXD_QT_ELIMINATED 1
#endif
#ifndef RAWRXD_NO_QT
#define RAWRXD_NO_QT 1
#endif

// ─────────────────────────────────────────────────────────────────────────────
// 0. Prevent real Qt headers from being included if they somehow exist
// ─────────────────────────────────────────────────────────────────────────────
#define QOBJECT_H
#define QWIDGET_H
#define QAPPLICATION_H
#define QCOREAPPLICATION_H
#define QSTRING_H
#define QSTRINGLIST_H
#define QBYTEARRAY_H
#define QVARIANT_H
#define QLIST_H
#define QVECTOR_H
#define QMAP_H
#define QHASH_H
#define QSET_H
#define QDIR_H
#define QFILE_H
#define QFILEINFO_H
#define QLAYOUT_H
#define QBOXLAYOUT_H
#define QGRIDLAYOUT_H
#define QMAINWINDOW_H
#define QDIALOG_H
#define QMENU_H
#define QACTION_H
#define QLABEL_H
#define QLINEEDIT_H
#define QTEXTEDIT_H
#define QPLAINTEXTEDIT_H
#define QPUSHBUTTON_H
#define QCOMBOBOX_H
#define QCHECKBOX_H
#define QTIMER_H
#define QTHREAD_H
#define QPROCESS_H
#define QJSONDOCUMENT_H
#define QJSONOBJECT_H
#define QJSONARRAY_H
#define QSETTINGS_H
#define QMESSAGEBOX_H
#define QFILEDIALOG_H
#define QTCPSERVER_H
#define QTCPSOCKET_H
#define QNETWORKACCESSMANAGER_H
#define QNETWORKREPLY_H
#define QNETWORKREQUEST_H
#define QPAINTER_H
#define QEVENT_H
#define QSYNTAXHIGHLIGHTER_H
#define QUUID_H
#define QURL_H
#define QDATETIME_H
#define QIODEVICE_H
#define QTEXTSTREAM_H
#define QTEXTCODEC_H
#define QREGULAREXPRESSION_H
#define QQUEUE_H
#define QMUTEX_H
#define QWAITCONDITION_H
#define QFUTURE_H
#define QFUTUREWATCHER_H
#define QTEMPORARYDIR_H
#define QFILESYSTEMWATCHER_H
#define QCRYPTOGRAPHICHASH_H
#define QSTANDARDPATHS_H
#define QSYSINFO_H
#define QRANDOMGENERATOR_H
#define QLOGGINGCATEGORY_H
#define QMETAOBJECT_H
#define QLIBRARY_H
#define QSAVEFILE_H
#define QELAPSEDTIMER_H
#define QWEBSOCKET_H
#define QWEBCHANNEL_H
#define QWEBENGINEVIEW_H
#define QSQLDATABASE_H
#define QUNDOSTACK_H
#define QUNDOGROUP_H
#define QUNDOVIEW_H
#define QSYSTEMTRAYICON_H
#define QFILESYSTEMMODEL_H
#define QSTATUSBAR_H
#define QTOOLBAR_H

// ─────────────────────────────────────────────────────────────────────────────
// 1. Qt Macros → No-ops
// ─────────────────────────────────────────────────────────────────────────────
#ifndef Q_OBJECT
#define Q_OBJECT
#endif
#ifndef Q_PROPERTY
#define Q_PROPERTY(...)
#endif
#ifndef Q_ENUM
#define Q_ENUM(...)
#endif
#ifndef Q_INVOKABLE
#define Q_INVOKABLE
#endif
#ifndef Q_DECLARE_FLAGS
#define Q_DECLARE_FLAGS(...)
#endif
#ifndef Q_DECLARE_OPERATORS_FOR_FLAGS
#define Q_DECLARE_OPERATORS_FOR_FLAGS(...)
#endif
#ifndef Q_MOC_RUN
#define Q_MOC_RUN 0
#endif
#ifndef Q_UNUSED
#define Q_UNUSED(x) (void)(x)
#endif
#ifndef Q_EMIT
#define Q_EMIT
#endif
#ifndef Q_SIGNAL
#define Q_SIGNAL
#endif
#ifndef Q_SLOT
#define Q_SLOT
#endif
#ifndef Q_SIGNALS
#define Q_SIGNALS public
#endif
#ifndef Q_SLOTS
#define Q_SLOTS
#endif
#ifndef Q_CLASSINFO
#define Q_CLASSINFO(...)
#endif
#ifndef Q_INTERFACES
#define Q_INTERFACES(...)
#endif
#ifndef Q_FLAGS
#define Q_FLAGS(...)
#endif
#ifndef Q_DISABLE_COPY
#define Q_DISABLE_COPY(Class)
#endif
#ifndef Q_DISABLE_MOVE
#define Q_DISABLE_MOVE(Class)
#endif
#ifndef Q_DISABLE_COPY_MOVE
#define Q_DISABLE_COPY_MOVE(Class)
#endif
#ifndef QStringLiteral
#define QStringLiteral(s) std::string(s)
#endif
#ifndef Q_NULLPTR
#define Q_NULLPTR nullptr
#endif
#ifndef Q_DECL_OVERRIDE
#define Q_DECL_OVERRIDE override
#endif
#ifndef Q_DECL_FINAL
#define Q_DECL_FINAL final
#endif
#ifndef Q_REQUIRED_RESULT
#define Q_REQUIRED_RESULT
#endif
#ifndef Q_ASSERT
#define Q_ASSERT(cond) ((void)0)
#endif
#ifndef qDebug
#define qDebug(...) ((void)0)
#endif
#ifndef qWarning
#define qWarning(...) ((void)0)
#endif
#ifndef qCritical
#define qCritical(...) ((void)0)
#endif
#ifndef qFatal
#define qFatal(...) ((void)0)
#endif
#ifndef qInfo
#define qInfo(...) ((void)0)
#endif
#ifndef qPrintable
#define qPrintable(s) (s).c_str()
#endif
#ifndef qUtf8Printable
#define qUtf8Printable(s) (s).c_str()
#endif
#ifndef foreach
#define foreach(var, container) for (var : container)
#endif

// ─────────────────────────────────────────────────────────────────────────────
// 2. signals/slots/emit keywords → neutralized
// ─────────────────────────────────────────────────────────────────────────────
// "signals:" in class bodies becomes "public:"
// "slots" in "public slots:" etc. becomes nothing
// "emit" becomes nothing (inline passthrough)
#ifndef signals
#define signals public
#endif
#ifndef slots
#define slots
#endif
// emit is trickier — it's used as `emit someSignal(args)` which should just call someSignal()
#ifndef emit
#define emit
#endif

// ─────────────────────────────────────────────────────────────────────────────
// 3. Qt Type Stubs — Empty/minimal structs that let code compile
// ─────────────────────────────────────────────────────────────────────────────

// Forward-declare everything first so cross-references resolve
class QObject;
class QWidget;
class QMainWindow;
class QDialog;
class QApplication;
class QCoreApplication;
class QLayout;
class QHBoxLayout;
class QVBoxLayout;
class QGridLayout;

// ── QString ──────────────────────────────────────────────────────────────────
// Backed by std::string. Minimal surface to satisfy compile-time references.
class QString {
    std::string d;
public:
    QString() = default;
    QString(const char* s) : d(s ? s : "") {}
    QString(const std::string& s) : d(s) {}
    QString(const QString&) = default;
    QString(QString&&) = default;
    QString& operator=(const QString&) = default;
    QString& operator=(QString&&) = default;
    QString& operator=(const char* s) { d = s ? s : ""; return *this; }

    const char* toStdString_c() const { return d.c_str(); }
    std::string toStdString() const { return d; }
    const char* toUtf8() const { return d.c_str(); }
    const char* toLatin1() const { return d.c_str(); }
    const char* toLocal8Bit() const { return d.c_str(); }
    bool isEmpty() const { return d.empty(); }
    bool isNull() const { return d.empty(); }
    int length() const { return (int)d.size(); }
    int size() const { return (int)d.size(); }
    void clear() { d.clear(); }
    bool contains(const QString& s) const { return d.find(s.d) != std::string::npos; }
    bool startsWith(const QString& s) const { return d.rfind(s.d, 0) == 0; }
    bool endsWith(const QString& s) const { return d.size() >= s.d.size() && d.compare(d.size()-s.d.size(), s.d.size(), s.d) == 0; }
    QString trimmed() const { auto s = d; while(!s.empty()&&std::isspace((unsigned char)s.front())) s.erase(s.begin()); while(!s.empty()&&std::isspace((unsigned char)s.back())) s.pop_back(); return s; }
    QString toLower() const { auto s = d; for(auto& c:s) c=(char)std::tolower((unsigned char)c); return s; }
    QString toUpper() const { auto s = d; for(auto& c:s) c=(char)std::toupper((unsigned char)c); return s; }
    QString mid(int pos, int len=-1) const { if(pos>=(int)d.size()) return {}; return len<0?d.substr(pos):d.substr(pos,len); }
    QString left(int n) const { return d.substr(0, n); }
    QString right(int n) const { return (int)d.size()>n?d.substr(d.size()-n):d; }
    int indexOf(const QString& s, int from=0) const { auto p=d.find(s.d,from); return p==std::string::npos?-1:(int)p; }
    QChar at(int i) const;
    QString& append(const QString& s) { d += s.d; return *this; }
    QString& prepend(const QString& s) { d = s.d + d; return *this; }
    QString replaced(const QString& before, const QString& after) const { auto s=d; auto p=s.find(before.d); while(p!=std::string::npos){s.replace(p,before.d.size(),after.d);p=s.find(before.d,p+after.d.size());}return s; }
    QString& replace(const QString& before, const QString& after) { auto r=replaced(before,after); d=r.d; return *this; }
    QStringList split(const QString& sep) const;
    static QString number(int n) { return std::to_string(n); }
    static QString number(double n) { return std::to_string(n); }
    static QString fromStdString(const std::string& s) { return s; }
    static QString fromUtf8(const char* s, int len=-1) { return len<0?QString(s):QString(std::string(s,len)); }
    static QString fromLatin1(const char* s) { return QString(s); }
    int toInt(bool* ok=nullptr) const { try{auto v=std::stoi(d);if(ok)*ok=true;return v;}catch(...){if(ok)*ok=false;return 0;} }
    double toDouble(bool* ok=nullptr) const { try{auto v=std::stod(d);if(ok)*ok=true;return v;}catch(...){if(ok)*ok=false;return 0;} }

    // Operator overloads
    QString operator+(const QString& o) const { return d + o.d; }
    QString& operator+=(const QString& o) { d += o.d; return *this; }
    bool operator==(const QString& o) const { return d == o.d; }
    bool operator!=(const QString& o) const { return d != o.d; }
    bool operator<(const QString& o) const { return d < o.d; }
    operator std::string() const { return d; }
    friend QString operator+(const char* a, const QString& b) { return std::string(a) + b.d; }

    // arg() for format strings — simplified
    QString arg(const QString& val) const {
        auto s = d;
        auto pos = s.find("%1");
        if (pos == std::string::npos) pos = s.find("%2");
        if (pos == std::string::npos) pos = s.find("%3");
        if (pos != std::string::npos) s.replace(pos, 2, val.d);
        return s;
    }
    QString arg(int val) const { return arg(QString(std::to_string(val))); }
    QString arg(double val) const { return arg(QString(std::to_string(val))); }
    QString arg(const char* val) const { return arg(QString(val ? val : "")); }
    QString arg(const std::string& val) const { return arg(QString(val)); }
};

// ── QChar ────────────────────────────────────────────────────────────────────
class QChar {
    char c_;
public:
    QChar() : c_(0) {}
    QChar(char c) : c_(c) {}
    char toLatin1() const { return c_; }
    bool isSpace() const { return std::isspace((unsigned char)c_) != 0; }
    bool isDigit() const { return std::isdigit((unsigned char)c_) != 0; }
    bool isLetter() const { return std::isalpha((unsigned char)c_) != 0; }
    bool operator==(QChar o) const { return c_ == o.c_; }
    bool operator!=(QChar o) const { return c_ != o.c_; }
};
inline QChar QString::at(int i) const { return (i >= 0 && i < (int)d.size()) ? QChar(d[i]) : QChar(); }

// ── QStringList ──────────────────────────────────────────────────────────────
class QStringList : public std::vector<QString> {
public:
    using std::vector<QString>::vector;
    QStringList() = default;
    QStringList(std::initializer_list<QString> il) : std::vector<QString>(il) {}
    QString join(const QString& sep) const {
        std::string r;
        for (size_t i = 0; i < size(); i++) {
            if (i) r += std::string(sep);
            r += std::string((*this)[i]);
        }
        return r;
    }
    bool contains(const QString& s) const {
        for (auto& v : *this) if (v == s) return true;
        return false;
    }
    QStringList filter(const QString& s) const {
        QStringList r;
        for (auto& v : *this) if (std::string(v).find(std::string(s)) != std::string::npos) r.push_back(v);
        return r;
    }
};
inline QStringList QString::split(const QString& sep) const {
    QStringList r;
    std::string s = d;
    std::string sp = sep.toStdString();
    size_t pos;
    while ((pos = s.find(sp)) != std::string::npos) {
        r.push_back(s.substr(0, pos));
        s = s.substr(pos + sp.size());
    }
    r.push_back(s);
    return r;
}

// ── QByteArray ───────────────────────────────────────────────────────────────
class QByteArray {
    std::vector<char> d;
public:
    QByteArray() = default;
    QByteArray(const char* s, int len = -1) { if(s){if(len<0)len=(int)strlen(s);d.assign(s,s+len);} }
    QByteArray(int size, char ch) : d(size, ch) {}
    const char* data() const { return d.data(); }
    const char* constData() const { return d.data(); }
    char* data() { return d.data(); }
    int size() const { return (int)d.size(); }
    int length() const { return (int)d.size(); }
    bool isEmpty() const { return d.empty(); }
    void clear() { d.clear(); }
    void resize(int s) { d.resize(s); }
    void append(const char* s, int len) { d.insert(d.end(), s, s+len); }
    void append(const QByteArray& o) { d.insert(d.end(), o.d.begin(), o.d.end()); }
    QByteArray mid(int pos, int len=-1) const { if(pos>=(int)d.size()) return {}; QByteArray r; int n=len<0?(int)d.size()-pos:len; r.d.assign(d.begin()+pos,d.begin()+pos+n); return r; }
    static QByteArray fromHex(const QByteArray&) { return {}; }
    QByteArray toHex() const { return {}; }
    char operator[](int i) const { return d[i]; }
    char& operator[](int i) { return d[i]; }
    bool operator==(const QByteArray& o) const { return d == o.d; }
    bool operator!=(const QByteArray& o) const { return d != o.d; }
    operator std::string() const { return std::string(d.begin(), d.end()); }
};

// ── QVariant ─────────────────────────────────────────────────────────────────
class QVariant {
    std::string s_; int i_ = 0; double d_ = 0; int type_ = 0;
public:
    QVariant() = default;
    QVariant(int v) : i_(v), type_(1) {}
    QVariant(double v) : d_(v), type_(2) {}
    QVariant(const QString& v) : s_(v), type_(3) {}
    QVariant(const char* v) : s_(v ? v : ""), type_(3) {}
    QVariant(bool v) : i_(v?1:0), type_(4) {}
    bool isValid() const { return type_ != 0; }
    bool isNull() const { return type_ == 0; }
    int toInt() const { return i_; }
    double toDouble() const { return d_; }
    bool toBool() const { return i_ != 0; }
    QString toString() const { return s_; }
    template<typename T> T value() const {
        if constexpr (std::is_same_v<T,int>) return i_;
        else if constexpr (std::is_same_v<T,double>) return d_;
        else if constexpr (std::is_same_v<T,bool>) return i_!=0;
        else if constexpr (std::is_same_v<T,QString>) return QString(s_);
        else if constexpr (std::is_same_v<T,std::string>) return s_;
        else return T{};
    }
};

// ── Container Type Aliases ───────────────────────────────────────────────────
template<typename T> using QList = std::vector<T>;
template<typename T> using QVector = std::vector<T>;
template<typename K, typename V> using QMap = std::map<K, V>;
template<typename K, typename V> using QHash = std::unordered_map<K, V>;
template<typename T> using QSet = std::unordered_set<T>;
template<typename T> using QQueue = std::vector<T>;
template<typename A, typename B> using QPair = std::pair<A, B>;
template<typename T> using QPointer = T*;
template<typename T> using QSharedPointer = std::shared_ptr<T>;

// ── QObject ──────────────────────────────────────────────────────────────────
class QObject {
public:
    QObject(QObject* parent = nullptr) { (void)parent; }
    virtual ~QObject() = default;
    QString objectName() const { return name_; }
    void setObjectName(const QString& n) { name_ = n; }
    virtual bool event(class QEvent*) { return false; }
    virtual bool eventFilter(QObject*, class QEvent*) { return false; }
    void installEventFilter(QObject*) {}
    void removeEventFilter(QObject*) {}
    template<typename T> T findChild(const QString& = {}) const { return T{}; }
    template<typename T> std::vector<T> findChildren(const QString& = {}) const { return {}; }
    void setProperty(const char*, const QVariant&) {}
    QVariant property(const char*) const { return {}; }
    void deleteLater() { /* no-op in non-Qt */ }
    static bool connect(const QObject*, const char*, const QObject*, const char*) { return true; }
    static bool disconnect(const QObject*, const char*, const QObject*, const char*) { return true; }
    // Templated signal/slot connect
    template<typename Func1, typename Func2>
    static bool connect(const QObject*, Func1, const QObject*, Func2) { return true; }
    template<typename Func1, typename Func2>
    static bool connect(const QObject*, Func1, Func2) { return true; }
    bool isWidgetType() const { return false; }
    bool isWindowType() const { return false; }
    const QMetaObject* metaObject() const { return nullptr; }
    void moveToThread(QThread*) {}
    QThread* thread() const { return nullptr; }
    QObject* parent() const { return nullptr; }
    void setParent(QObject*) {}
protected:
    virtual void timerEvent(QTimerEvent*) {}
    virtual void childEvent(class QChildEvent*) {}
    virtual void customEvent(QEvent*) {}
private:
    QString name_;
};

// ── QMetaObject ──────────────────────────────────────────────────────────────
class QMetaObject {
public:
    const char* className() const { return ""; }
    static bool invokeMethod(QObject*, const char*) { return false; }
};

// ── QEvent ───────────────────────────────────────────────────────────────────
class QEvent {
public:
    enum Type { None = 0, Timer = 1, Close = 19, Resize = 14, Paint = 12, Show = 17, KeyPress = 6, KeyRelease = 7 };
    QEvent(Type t = None) : t_(t) {}
    virtual ~QEvent() = default;
    Type type() const { return t_; }
    void accept() { a_ = true; }
    void ignore() { a_ = false; }
    bool isAccepted() const { return a_; }
private:
    Type t_; bool a_ = false;
};
class QTimerEvent : public QEvent { public: QTimerEvent(int id=0):QEvent(Timer),id_(id){} int timerId()const{return id_;} private: int id_; };
class QChildEvent : public QEvent { public: QChildEvent():QEvent(None){} };
class QCloseEvent : public QEvent { public: QCloseEvent():QEvent(Close){} };
class QShowEvent : public QEvent { public: QShowEvent():QEvent(Show){} };
class QResizeEvent : public QEvent { public: QResizeEvent():QEvent(Resize){} int w=0,h=0; };
class QEnterEvent : public QEvent { public: QEnterEvent():QEvent(None){} };
class QPaintEvent : public QEvent { public: QPaintEvent():QEvent(Paint){} };
class QKeyEvent : public QEvent {
public:
    QKeyEvent():QEvent(KeyPress){} int key()const{return 0;} QString text()const{return{};} bool isAutoRepeat()const{return false;}
    int modifiers()const{return 0;}
};
class QMouseEvent : public QEvent {
public:
    QMouseEvent():QEvent(None){} int x()const{return 0;} int y()const{return 0;} int button()const{return 0;} int buttons()const{return 0;} int modifiers()const{return 0;}
    QPoint pos()const;
    QPoint globalPos()const;
};
class QContextMenuEvent : public QEvent { public: QContextMenuEvent():QEvent(None){} QPoint globalPos()const; QPoint pos()const; };
class QDragEnterEvent : public QEvent { public: QDragEnterEvent():QEvent(None){} void acceptProposedAction(){} const class QMimeData* mimeData()const{return nullptr;} };
class QDropEvent : public QEvent { public: QDropEvent():QEvent(None){} const class QMimeData* mimeData()const{return nullptr;} };

class QKeySequence {
public:
    QKeySequence() = default;
    QKeySequence(const QString&) {}
    QKeySequence(int) {}
    enum StandardKey { New=0, Open, Save, SaveAs, Close, Quit, Undo, Redo, Cut, Copy, Paste, Find, Replace, SelectAll, ZoomIn, ZoomOut };
    QKeySequence(StandardKey) {}
    QString toString() const { return {}; }
};

// ── Geometry Types ───────────────────────────────────────────────────────────
class QPoint { public: QPoint()=default; QPoint(int x,int y):x_(x),y_(y){} int x()const{return x_;} int y()const{return y_;} private: int x_=0,y_=0; };
class QSize { public: QSize()=default; QSize(int w,int h):w_(w),h_(h){} int width()const{return w_;} int height()const{return h_;} bool isEmpty()const{return w_<=0||h_<=0;} private: int w_=0,h_=0; };
class QRect { public: QRect()=default; QRect(int x,int y,int w,int h):x_(x),y_(y),w_(w),h_(h){} int x()const{return x_;} int y()const{return y_;} int width()const{return w_;} int height()const{return h_;} QPoint topLeft()const{return{x_,y_};} QSize size()const{return{w_,h_};} bool contains(const QPoint&)const{return false;} private: int x_=0,y_=0,w_=0,h_=0; };
using QRgb = unsigned int;

// Deferred inline implementations
inline QPoint QMouseEvent::pos() const { return {}; }
inline QPoint QMouseEvent::globalPos() const { return {}; }
inline QPoint QContextMenuEvent::globalPos() const { return {}; }
inline QPoint QContextMenuEvent::pos() const { return {}; }

// ── Graphics Types ───────────────────────────────────────────────────────────
class QColor {
public:
    QColor()=default; QColor(int r,int g,int b,int a=255):r_(r),g_(g),b_(b),a_(a){} QColor(const QString&){}
    QColor(unsigned int){} int red()const{return r_;} int green()const{return g_;} int blue()const{return b_;} int alpha()const{return a_;}
    bool isValid()const{return true;} QString name()const{return "#000000";}
    static QColor fromRgb(int r,int g,int b,int a=255){return{r,g,b,a};}
private: int r_=0,g_=0,b_=0,a_=255;
};
class QFont {
public:
    QFont()=default; QFont(const QString&,int=-1){} void setPointSize(int){} void setFamily(const QString&){} void setBold(bool){}
    void setItalic(bool){} void setFixedPitch(bool){} QString family()const{return{};} int pointSize()const{return 10;}
    void setPixelSize(int){} bool bold()const{return false;} bool italic()const{return false;}
};
class QIcon { public: QIcon()=default; QIcon(const QString&){} bool isNull()const{return true;} static QIcon fromTheme(const QString&){return{};} };
class QBrush { public: QBrush()=default; QBrush(const QColor&){} };
class QPainter { public: QPainter()=default; QPainter(QWidget*){} void begin(QWidget*){} void end(){} void drawText(int,int,const QString&){} void fillRect(const QRect&,const QColor&){} void setPen(const QColor&){} void setFont(const QFont&){} void setBrush(const QBrush&){} };
class QTextCursor { public: QTextCursor()=default; enum MoveOperation{Start=0,End=1,StartOfLine=2,EndOfLine=3,Left=4,Right=5,Up=6,Down=7}; enum MoveMode{MoveAnchor=0,KeepAnchor=1}; void movePosition(MoveOperation,MoveMode=MoveAnchor,int=1){} int position()const{return 0;} void insertText(const QString&){} bool hasSelection()const{return false;} QString selectedText()const{return{};} void select(int){} int blockNumber()const{return 0;} int columnNumber()const{return 0;} };
class QTextCharFormat { public: void setForeground(const QBrush&){} void setBackground(const QBrush&){} void setFontWeight(int){} void setFontItalic(bool){} void setFontUnderline(bool){} };
class QTextDocument { public: QTextDocument(QObject* p=nullptr){(void)p;} QString toPlainText()const{return{};} void setPlainText(const QString&){} };
class QSyntaxHighlighter { public: QSyntaxHighlighter(QObject* p=nullptr){(void)p;} QSyntaxHighlighter(QTextDocument* p){(void)p;} virtual ~QSyntaxHighlighter()=default; void rehighlight(){} protected: virtual void highlightBlock(const QString&)=0; void setFormat(int,int,const QTextCharFormat&){} void setFormat(int,int,const QColor&){} };

// ── QWidget / GUI Stubs ──────────────────────────────────────────────────────
class QWidget : public QObject {
public:
    QWidget(QWidget* parent = nullptr) : QObject(parent) {}
    virtual ~QWidget() = default;
    void show() {} void hide() {} void close() {} void update() {} void repaint() {}
    void setWindowTitle(const QString&) {} void setMinimumSize(int,int) {} void setMaximumSize(int,int) {}
    void resize(int,int) {} void move(int,int) {} void setFixedSize(int,int) {} void setGeometry(int,int,int,int) {}
    void setVisible(bool) {} bool isVisible() const { return false; }
    void setEnabled(bool) {} bool isEnabled() const { return true; }
    void setFocus() {} bool hasFocus() const { return false; }
    void setToolTip(const QString&) {} void setStatusTip(const QString&) {}
    void setStyleSheet(const QString&) {} void setCursor(int) {} void setFont(const QFont&) {}
    QSize size() const { return {}; } QRect geometry() const { return {}; }
    int width() const { return 0; } int height() const { return 0; }
    QWidget* parentWidget() const { return nullptr; }
    void setAttribute(int, bool = true) {} void setWindowFlags(int) {}
    void addAction(class QAction*) {} void setLayout(QLayout*) {}
    void setSizePolicy(int, int) {} virtual QSize sizeHint() const { return {}; }
    void setContentsMargins(int,int,int,int) {}
    virtual void paintEvent(QPaintEvent*) {} virtual void keyPressEvent(QKeyEvent*) {} virtual void keyReleaseEvent(QKeyEvent*) {}
    virtual void mousePressEvent(QMouseEvent*) {} virtual void mouseReleaseEvent(QMouseEvent*) {} virtual void mouseMoveEvent(QMouseEvent*) {}
    virtual void closeEvent(QCloseEvent*) {} virtual void resizeEvent(QResizeEvent*) {} virtual void showEvent(QShowEvent*) {}
    virtual void enterEvent(QEnterEvent*) {} virtual void contextMenuEvent(QContextMenuEvent*) {}
    virtual void dragEnterEvent(QDragEnterEvent*) {} virtual void dropEvent(QDropEvent*) {}
};
class QFrame : public QWidget { public: QFrame(QWidget* p=nullptr):QWidget(p){} void setFrameShape(int){} void setFrameShadow(int){} void setLineWidth(int){} enum Shape{NoFrame=0,Box=1,Panel=2,StyledPanel=3,HLine=4,VLine=5}; enum Shadow{Plain=0x10,Raised=0x20,Sunken=0x30}; };
class QMainWindow : public QWidget {
public:
    QMainWindow(QWidget* p=nullptr):QWidget(p){}
    void setCentralWidget(QWidget*){} QWidget* centralWidget()const{return nullptr;}
    void addDockWidget(int,class QDockWidget*){} void removeDockWidget(class QDockWidget*){}
    class QMenuBar* menuBar()const;
    class QStatusBar* statusBar()const;
    void addToolBar(class QToolBar*){} void addToolBar(int,class QToolBar*){}
};
class QDialog : public QWidget { public: QDialog(QWidget* p=nullptr):QWidget(p){} enum DialogCode{Rejected=0,Accepted=1}; virtual int exec(){return 0;} void accept(){} void reject(){} };
class QSplitter : public QWidget { public: QSplitter(QWidget* p=nullptr):QWidget(p){} QSplitter(int,QWidget* p=nullptr):QWidget(p){} void addWidget(QWidget*){} void setStretchFactor(int,int){} void setSizes(const QList<int>&){} void setHandleWidth(int){} };
class QStackedWidget : public QWidget { public: QStackedWidget(QWidget* p=nullptr):QWidget(p){} int addWidget(QWidget*){return 0;} void setCurrentIndex(int){} void setCurrentWidget(QWidget*){} int currentIndex()const{return 0;} QWidget* currentWidget()const{return nullptr;} int count()const{return 0;} QWidget* widget(int)const{return nullptr;} };
class QScrollArea : public QWidget { public: QScrollArea(QWidget* p=nullptr):QWidget(p){} void setWidget(QWidget*){} void setWidgetResizable(bool){} };
class QDockWidget : public QWidget { public: QDockWidget(const QString& = {}, QWidget* p=nullptr):QWidget(p){} void setWidget(QWidget*){} QWidget* widget()const{return nullptr;} void setAllowedAreas(int){} void setFeatures(int){} enum DockWidgetFeature{DockWidgetClosable=1,DockWidgetMovable=2,DockWidgetFloatable=4,AllDockWidgetFeatures=7}; };
class QTabWidget : public QWidget { public: QTabWidget(QWidget* p=nullptr):QWidget(p){} int addTab(QWidget*,const QString&){return 0;} int addTab(QWidget*,const QIcon&,const QString&){return 0;} void removeTab(int){} void setCurrentIndex(int){} int currentIndex()const{return 0;} int count()const{return 0;} QWidget* widget(int)const{return nullptr;} void setTabsClosable(bool){} void setMovable(bool){} void setTabPosition(int){} QString tabText(int)const{return{};} void setTabText(int,const QString&){} void setTabIcon(int,const QIcon&){} };
class QTabBar : public QWidget { public: QTabBar(QWidget* p=nullptr):QWidget(p){} int count()const{return 0;} int currentIndex()const{return 0;} void setCurrentIndex(int){} };
class QGroupBox : public QWidget { public: QGroupBox(const QString& = {}, QWidget* p=nullptr):QWidget(p){} void setTitle(const QString&){} void setCheckable(bool){} };
class QButtonGroup : public QObject { public: QButtonGroup(QObject* p=nullptr):QObject(p){} };
class QGraphicsView : public QWidget { public: QGraphicsView(QWidget* p=nullptr):QWidget(p){} };

// ── Input Widgets ────────────────────────────────────────────────────────────
class QLabel : public QWidget { public: QLabel(QWidget* p=nullptr):QWidget(p){} QLabel(const QString&,QWidget* p=nullptr):QWidget(p){} void setText(const QString&){} QString text()const{return{};} void setPixmap(int){} void setAlignment(int){} void setWordWrap(bool){} void setTextInteractionFlags(int){} };
class QLineEdit : public QWidget { public: QLineEdit(QWidget* p=nullptr):QWidget(p){} QLineEdit(const QString&,QWidget* p=nullptr):QWidget(p){} void setText(const QString&){} QString text()const{return{};} void setPlaceholderText(const QString&){} void setReadOnly(bool){} void clear(){} void selectAll(){} void setEchoMode(int){} enum EchoMode{Normal=0,Password=2}; void setMaxLength(int){} };
class QTextEdit : public QWidget {
public:
    QTextEdit(QWidget* p=nullptr):QWidget(p){} void setText(const QString&){} void setPlainText(const QString&){} void setHtml(const QString&){}
    QString toPlainText()const{return{};} void append(const QString&){} void clear(){} void setReadOnly(bool){} void setFont(const QFont&){}
    QTextCursor textCursor()const{return{};} void setTextCursor(const QTextCursor&){} QTextDocument* document()const{return nullptr;} void moveCursor(int){} void ensureCursorVisible(){}
    void setLineWrapMode(int){} void setAcceptRichText(bool){} void setTabStopDistance(int){} void insertPlainText(const QString&){} enum LineWrapMode{NoWrap=0,WidgetWidth=1};
};
class QPlainTextEdit : public QWidget {
public:
    QPlainTextEdit(QWidget* p=nullptr):QWidget(p){} void setPlainText(const QString&){} QString toPlainText()const{return{};} void appendPlainText(const QString&){}
    void clear(){} void setReadOnly(bool){} QTextCursor textCursor()const{return{};} void setTextCursor(const QTextCursor&){} QTextDocument* document()const{return nullptr;}
    void ensureCursorVisible(){} void moveCursor(int){} void setFont(const QFont&){} void setTabStopDistance(int){} void setLineWrapMode(int){} enum LineWrapMode{NoWrap=0,WidgetWidth=1};
};
class QPushButton : public QWidget { public: QPushButton(QWidget* p=nullptr):QWidget(p){} QPushButton(const QString&,QWidget* p=nullptr):QWidget(p){} void setText(const QString&){} QString text()const{return{};} void setIcon(const QIcon&){} void setCheckable(bool){} void setChecked(bool){} bool isChecked()const{return false;} void setFlat(bool){} void setDefault(bool){} };
class QToolButton : public QWidget { public: QToolButton(QWidget* p=nullptr):QWidget(p){} void setText(const QString&){} void setIcon(const QIcon&){} void setToolButtonStyle(int){} void setMenu(QMenu*){} void setPopupMode(int){} void setDefaultAction(class QAction*){} enum ToolButtonPopupMode{DelayedPopup=0,MenuButtonPopup=1,InstantPopup=2}; };
class QCheckBox : public QWidget { public: QCheckBox(QWidget* p=nullptr):QWidget(p){} QCheckBox(const QString&,QWidget* p=nullptr):QWidget(p){} void setChecked(bool){} bool isChecked()const{return false;} };
class QRadioButton : public QWidget { public: QRadioButton(QWidget* p=nullptr):QWidget(p){} QRadioButton(const QString&,QWidget* p=nullptr):QWidget(p){} void setChecked(bool){} bool isChecked()const{return false;} };
class QComboBox : public QWidget { public: QComboBox(QWidget* p=nullptr):QWidget(p){} void addItem(const QString&){} void addItem(const QString&,const QVariant&){} void addItems(const QStringList&){} int currentIndex()const{return 0;} QString currentText()const{return{};} void setCurrentIndex(int){} void setCurrentText(const QString&){} void clear(){} int count()const{return 0;} QString itemText(int)const{return{};} void setEditable(bool){} };
class QSpinBox : public QWidget { public: QSpinBox(QWidget* p=nullptr):QWidget(p){} void setValue(int){} int value()const{return 0;} void setRange(int,int){} void setMinimum(int){} void setMaximum(int){} void setSuffix(const QString&){} void setPrefix(const QString&){} };
class QDoubleSpinBox : public QWidget { public: QDoubleSpinBox(QWidget* p=nullptr):QWidget(p){} void setValue(double){} double value()const{return 0;} void setRange(double,double){} void setDecimals(int){} };
class QSlider : public QWidget { public: QSlider(QWidget* p=nullptr):QWidget(p){} QSlider(int,QWidget* p=nullptr):QWidget(p){} void setValue(int){} int value()const{return 0;} void setRange(int,int){} void setOrientation(int){} };
class QProgressBar : public QWidget { public: QProgressBar(QWidget* p=nullptr):QWidget(p){} void setValue(int){} void setRange(int,int){} void setMaximum(int){} int value()const{return 0;} void setFormat(const QString&){} void setTextVisible(bool){} };
class QProgressDialog : public QDialog { public: QProgressDialog(const QString& = {},const QString& = {},int=0,int=100,QWidget* p=nullptr):QDialog(p){} void setValue(int){} void setMaximum(int){} void setWindowTitle(const QString&){} void setAutoClose(bool){} bool wasCanceled()const{return false;} };

// ── Item Views ───────────────────────────────────────────────────────────────
class QModelIndex { public: QModelIndex()=default; bool isValid()const{return false;} int row()const{return -1;} int column()const{return -1;} QVariant data(int=0)const{return{};} QModelIndex parent()const{return{};} };
class QAbstractItemView : public QWidget { public: QAbstractItemView(QWidget* p=nullptr):QWidget(p){} };
class QListWidget : public QWidget { public: QListWidget(QWidget* p=nullptr):QWidget(p){} void addItem(const QString&){} void addItem(class QListWidgetItem*){} int count()const{return 0;} void clear(){} class QListWidgetItem* currentItem()const{return nullptr;} int currentRow()const{return -1;} class QListWidgetItem* item(int)const{return nullptr;} void setCurrentRow(int){} class QListWidgetItem* takeItem(int){return nullptr;} };
class QListWidgetItem { public: QListWidgetItem(const QString& = {},QListWidget* p=nullptr){(void)p;} void setText(const QString&){} QString text()const{return{};} void setIcon(const QIcon&){} void setToolTip(const QString&){} void setData(int,const QVariant&){} QVariant data(int)const{return{};} };
class QTableWidget : public QWidget { public: QTableWidget(QWidget* p=nullptr):QWidget(p){} QTableWidget(int,int,QWidget* p=nullptr):QWidget(p){} void setRowCount(int){} void setColumnCount(int){} };
class QTreeWidget : public QWidget { public: QTreeWidget(QWidget* p=nullptr):QWidget(p){} void setColumnCount(int){} void setHeaderLabels(const QStringList&){} void addTopLevelItem(class QTreeWidgetItem*){} void clear(){} int topLevelItemCount()const{return 0;} class QTreeWidgetItem* topLevelItem(int)const{return nullptr;} class QTreeWidgetItem* currentItem()const{return nullptr;} void setCurrentItem(class QTreeWidgetItem*){} };
class QTreeWidgetItem { public: QTreeWidgetItem()=default; QTreeWidgetItem(const QStringList&){} QTreeWidgetItem(QTreeWidget*){} QTreeWidgetItem(QTreeWidgetItem*){} void setText(int,const QString&){} QString text(int)const{return{};} void setIcon(int,const QIcon&){} void addChild(QTreeWidgetItem*){} int childCount()const{return 0;} QTreeWidgetItem* child(int)const{return nullptr;} void setData(int,int,const QVariant&){} QVariant data(int,int)const{return{};} void setExpanded(bool){} void setToolTip(int,const QString&){} };
class QTreeView : public QWidget { public: QTreeView(QWidget* p=nullptr):QWidget(p){} void setModel(void*){} void expandAll(){} void collapseAll(){} void setHeaderHidden(bool){} void setIndentation(int){} };
class QFileSystemModel : public QObject { public: QFileSystemModel(QObject* p=nullptr):QObject(p){} QModelIndex setRootPath(const QString&){return{};} QModelIndex index(const QString&)const{return{};} QString filePath(const QModelIndex&)const{return{};} };

// ── Layouts ──────────────────────────────────────────────────────────────────
class QLayout : public QObject { public: QLayout(QWidget* p=nullptr):QObject(p){} virtual void addWidget(QWidget*){} void setContentsMargins(int,int,int,int){} void setSpacing(int){} void addItem(void*){} };
class QHBoxLayout : public QLayout { public: QHBoxLayout(QWidget* p=nullptr):QLayout(p){} void addWidget(QWidget*,int=0)override{} void addWidget(QWidget*,int,int){} void addLayout(QLayout*,int=0){} void addStretch(int=0){} void addSpacing(int){} void insertWidget(int,QWidget*,int=0){} };
class QVBoxLayout : public QLayout { public: QVBoxLayout(QWidget* p=nullptr):QLayout(p){} void addWidget(QWidget*,int=0)override{} void addWidget(QWidget*,int,int){} void addLayout(QLayout*,int=0){} void addStretch(int=0){} void addSpacing(int){} void insertWidget(int,QWidget*,int=0){} };
class QGridLayout : public QLayout { public: QGridLayout(QWidget* p=nullptr):QLayout(p){} void addWidget(QWidget*,int,int,int=1,int=1){} void addLayout(QLayout*,int,int,int=1,int=1){} };

// ── Menu / Actions ───────────────────────────────────────────────────────────
class QAction : public QObject { public: QAction(QObject* p=nullptr):QObject(p){} QAction(const QString&,QObject* p=nullptr):QObject(p){} QAction(const QIcon&,const QString&,QObject* p=nullptr):QObject(p){} void setText(const QString&){} QString text()const{return{};} void setShortcut(const QKeySequence&){} void setCheckable(bool){} void setChecked(bool){} bool isChecked()const{return false;} void setEnabled(bool){} void setIcon(const QIcon&){} void setToolTip(const QString&){} void setStatusTip(const QString&){} void trigger(){} void setVisible(bool){} void setData(const QVariant&){} QVariant data()const{return{};} };
class QActionGroup : public QObject { public: QActionGroup(QObject* p=nullptr):QObject(p){} QAction* addAction(QAction* a){return a;} };
class QMenu : public QWidget { public: QMenu(QWidget* p=nullptr):QWidget(p){} QMenu(const QString&,QWidget* p=nullptr):QWidget(p){} QAction* addAction(const QString&){static QAction a;return &a;} QAction* addAction(const QIcon&,const QString&){static QAction a;return &a;} QMenu* addMenu(const QString&){static QMenu m;return &m;} QMenu* addMenu(const QIcon&,const QString&){static QMenu m;return &m;} void addSeparator(){} QAction* exec(const QPoint& = {}){return nullptr;} void clear(){} QAction* addAction(const QString&,const QObject*,const char*){static QAction a;return &a;} };
class QMenuBar : public QWidget { public: QMenuBar(QWidget* p=nullptr):QWidget(p){} QMenu* addMenu(const QString&){static QMenu m;return &m;} QMenu* addMenu(const QIcon&,const QString&){static QMenu m;return &m;} void clear(){} };
class QToolBar : public QWidget { public: QToolBar(QWidget* p=nullptr):QWidget(p){} QToolBar(const QString&,QWidget* p=nullptr):QWidget(p){} QAction* addAction(const QString&){static QAction a;return &a;} QAction* addAction(const QIcon&,const QString&){static QAction a;return &a;} void addWidget(QWidget*){} void addSeparator(){} void setMovable(bool){} void setIconSize(const QSize&){} void setToolButtonStyle(int){} };
class QStatusBar : public QWidget { public: QStatusBar(QWidget* p=nullptr):QWidget(p){} void showMessage(const QString&,int=0){} void clearMessage(){} void addWidget(QWidget*,int=0){} void addPermanentWidget(QWidget*,int=0){} };
class QSystemTrayIcon : public QObject { public: QSystemTrayIcon(QObject* p=nullptr):QObject(p){} QSystemTrayIcon(const QIcon&,QObject* p=nullptr):QObject(p){} void setIcon(const QIcon&){} void setToolTip(const QString&){} void show(){} void hide(){} void setContextMenu(QMenu*){} void showMessage(const QString&,const QString&,int=0,int=10000){} bool isSystemTrayAvailable(){return false;} };

// Deferred inline implementations for QMainWindow
inline QMenuBar* QMainWindow::menuBar() const { static QMenuBar mb; return &mb; }
inline QStatusBar* QMainWindow::statusBar() const { static QStatusBar sb; return &sb; }

// ── Undo System ──────────────────────────────────────────────────────────────
class QUndoStack : public QObject { public: QUndoStack(QObject* p=nullptr):QObject(p){} void push(void*){} bool canUndo()const{return false;} bool canRedo()const{return false;} void undo(){} void redo(){} void clear(){} QString undoText()const{return{};} QString redoText()const{return{};} };
class QUndoGroup : public QObject { public: QUndoGroup(QObject* p=nullptr):QObject(p){} void addStack(QUndoStack*){} void setActiveStack(QUndoStack*){} QUndoStack* activeStack()const{return nullptr;} };
class QUndoView : public QWidget { public: QUndoView(QWidget* p=nullptr):QWidget(p){} QUndoView(QUndoStack*,QWidget* p=nullptr):QWidget(p){} void setStack(QUndoStack*){} void setGroup(QUndoGroup*){} };

// ── Dialogs ──────────────────────────────────────────────────────────────────
class QFileDialog {
public:
    static QString getOpenFileName(QWidget* p=nullptr,const QString& = {},const QString& = {},const QString& = {}){(void)p;return{};}
    static QString getSaveFileName(QWidget* p=nullptr,const QString& = {},const QString& = {},const QString& = {}){(void)p;return{};}
    static QString getExistingDirectory(QWidget* p=nullptr,const QString& = {},const QString& = {}){(void)p;return{};}
    static QStringList getOpenFileNames(QWidget* p=nullptr,const QString& = {},const QString& = {},const QString& = {}){(void)p;return{};}
};
class QMessageBox {
public:
    enum StandardButton{NoButton=0,Ok=1,Cancel=2,Yes=4,No=8,Abort=16,Retry=32,Ignore=64};
    enum Icon{NoIcon=0,Information=1,Warning=2,Critical=3,Question=4};
    static StandardButton information(QWidget*,const QString&,const QString&,int=Ok){return Ok;}
    static StandardButton warning(QWidget*,const QString&,const QString&,int=Ok){return Ok;}
    static StandardButton critical(QWidget*,const QString&,const QString&,int=Ok){return Ok;}
    static StandardButton question(QWidget*,const QString&,const QString&,int=Yes|No){return Yes;}
};

// ── Timer / Thread / Process ─────────────────────────────────────────────────
class QTimer : public QObject {
public:
    QTimer(QObject* p=nullptr):QObject(p){} void start(int=0){} void stop(){} bool isActive()const{return false;}
    void setInterval(int){} int interval()const{return 0;} void setSingleShot(bool){}
    static void singleShot(int,QObject*,const char*){} static void singleShot(int,std::function<void()>){}
};
class QThread : public QObject {
public:
    QThread(QObject* p=nullptr):QObject(p){} virtual void run(){} void start(){} void quit(){} void wait(){}
    bool isRunning()const{return false;} bool isFinished()const{return true;}
    static QThread* currentThread(){static QThread t;return &t;} static void msleep(unsigned long){} static void sleep(unsigned long){}
    void requestInterruption(){} bool isInterruptionRequested()const{return false;}
};
class QProcess : public QObject {
public:
    QProcess(QObject* p=nullptr):QObject(p){} void start(const QString&,const QStringList& = {}){} void setProgram(const QString&){} void setArguments(const QStringList&){}
    void setWorkingDirectory(const QString&){} int exitCode()const{return 0;} QByteArray readAllStandardOutput(){return{};} QByteArray readAllStandardError(){return{};}
    bool waitForFinished(int=-1){return true;} bool waitForStarted(int=-1){return true;} void kill(){} void terminate(){}
    enum ProcessState{NotRunning=0,Starting=1,Running=2}; ProcessState state()const{return NotRunning;}
    void write(const QByteArray&){} void closeWriteChannel(){}
    static int execute(const QString&,const QStringList& = {}){return 0;}
};
class QElapsedTimer { public: QElapsedTimer()=default; void start(){} int64_t elapsed()const{return 0;} int64_t nsecsElapsed()const{return 0;} bool isValid()const{return false;} void invalidate(){} };
class QMutex { public: void lock(){} void unlock(){} bool tryLock(){return true;} };
class QWaitCondition { public: void wait(QMutex*){} void wakeOne(){} void wakeAll(){} };
template<typename T> class QFuture { public: T result()const{return T{};} bool isFinished()const{return true;} void waitForFinished(){} };
template<typename T> class QFutureWatcher : public QObject { public: QFutureWatcher(QObject* p=nullptr):QObject(p){} void setFuture(const QFuture<T>&){} QFuture<T> future()const{return{};} };
using QAtomicInt = std::atomic<int>;

// ── File / IO Types ──────────────────────────────────────────────────────────
class QIODevice : public QObject {
public:
    QIODevice(QObject* p=nullptr):QObject(p){}
    enum OpenModeFlag{NotOpen=0,ReadOnly=1,WriteOnly=2,ReadWrite=3,Append=4,Truncate=8,Text=16};
    virtual bool open(int){return false;} virtual void close(){} virtual bool isOpen()const{return false;}
    virtual QByteArray readAll(){return{};} virtual QByteArray read(int){return{};} virtual int write(const QByteArray&){return 0;}
};
class QFile : public QIODevice {
public:
    QFile()=default; QFile(const QString&){} bool open(int)override{return false;} void close()override{} bool exists()const{return false;}
    static bool exists(const QString&){return false;} static bool remove(const QString&){return false;} static bool copy(const QString&,const QString&){return false;}
    static bool rename(const QString&,const QString&){return false;} QByteArray readAll()override{return{};} int write(const QByteArray&)override{return 0;}
    QString fileName()const{return{};} bool isOpen()const override{return false;} int64_t size()const{return 0;}
};
class QFileInfo {
public:
    QFileInfo()=default; QFileInfo(const QString&){} bool exists()const{return false;} bool isFile()const{return false;} bool isDir()const{return false;}
    QString fileName()const{return{};} QString filePath()const{return{};} QString absoluteFilePath()const{return{};} QString absolutePath()const{return{};}
    QString baseName()const{return{};} QString suffix()const{return{};} QString completeSuffix()const{return{};} int64_t size()const{return 0;}
};
class QDir {
public:
    QDir()=default; QDir(const QString&){} bool exists()const{return false;} bool mkpath(const QString&)const{return false;}
    static QDir current(){return{};} static QDir home(){return{};} static QDir temp(){return{};}
    static QString currentPath(){return{};} static QString homePath(){return{};} static QString tempPath(){return{};}
    static QString separator(){return "\\";}
    QStringList entryList(const QStringList& = {},int=0)const{return{};} QString absolutePath()const{return{};} QString path()const{return{};}
    bool cd(const QString&){return false;} bool cdUp(){return false;} static QString cleanPath(const QString& p){return p;}
};
class QSaveFile : public QIODevice { public: QSaveFile(const QString& = {}):QIODevice(nullptr){} bool open(int)override{return false;} bool commit(){return false;} };
class QTextStream { public: QTextStream()=default; QTextStream(QIODevice*){} QTextStream(QFile*){} QTextStream(QString*){} QTextStream& operator<<(const QString&){return*this;} QTextStream& operator<<(const char*){return*this;} QTextStream& operator<<(int){return*this;} QString readAll(){return{};} QString readLine(){return{};} bool atEnd()const{return true;} };
class QTextCodec { public: static QTextCodec* codecForName(const char*){static QTextCodec c;return &c;} QByteArray fromUnicode(const QString&)const{return{};} QString toUnicode(const QByteArray&)const{return{};} };
class QTemporaryDir { public: QTemporaryDir()=default; bool isValid()const{return false;} QString path()const{return{};} };
class QFileSystemWatcher : public QObject { public: QFileSystemWatcher(QObject* p=nullptr):QObject(p){} void addPath(const QString&){} void addPaths(const QStringList&){} void removePath(const QString&){} };

// ── Settings / Paths ─────────────────────────────────────────────────────────
class QSettings {
public:
    QSettings()=default; QSettings(const QString&,const QString&){} QSettings(const QString&,int){}
    QVariant value(const QString&,const QVariant& def = {})const{return def;} void setValue(const QString&,const QVariant&){}
    bool contains(const QString&)const{return false;} void remove(const QString&){} void sync(){} void beginGroup(const QString&){} void endGroup(){}
    QStringList childGroups()const{return{};} QStringList childKeys()const{return{};} QStringList allKeys()const{return{};}
};
class QStandardPaths {
public:
    enum StandardLocation{DesktopLocation=0,DocumentsLocation=1,MusicLocation=2,TempLocation=3,HomeLocation=4,AppDataLocation=5,ConfigLocation=6,DownloadLocation=7,CacheLocation=8,AppConfigLocation=9};
    static QString writableLocation(StandardLocation){return{};} static QStringList standardLocations(StandardLocation){return{};}
};
class QRegularExpression {
public:
    QRegularExpression()=default; QRegularExpression(const QString&){} bool isValid()const{return true;}
    struct QRegularExpressionMatch { bool hasMatch()const{return false;} QString captured(int=0)const{return{};} };
    QRegularExpressionMatch match(const QString&)const{return {};} 
};
class QUrl { public: QUrl()=default; QUrl(const QString&){} QString toString()const{return{};} QString toLocalFile()const{return{};} static QUrl fromLocalFile(const QString&){return{};} bool isValid()const{return false;} bool isEmpty()const{return true;} };
class QDateTime { public: QDateTime()=default; static QDateTime currentDateTime(){return{};} static QDateTime currentDateTimeUtc(){return{};} QString toString(const QString& = {})const{return{};} int64_t toMSecsSinceEpoch()const{return 0;} static QDateTime fromMSecsSinceEpoch(int64_t){return{};} bool isValid()const{return false;} };
class QUuid { public: QUuid()=default; static QUuid createUuid(){return{};} QString toString()const{return{};} bool isNull()const{return true;} };
class QSysInfo { public: static QString prettyProductName(){return "Windows";} static QString currentCpuArchitecture(){return "x86_64";} static QString productType(){return "windows";} static QString productVersion(){return "10";} };

// ── JSON Types ───────────────────────────────────────────────────────────────
class QJsonValue;
class QJsonObject {
    std::map<std::string,std::string> d;
public:
    QJsonObject()=default;
    void insert(const QString& k,const QJsonValue& v);
    bool contains(const QString& k)const{return d.count(std::string(k))>0;}
    QJsonValue value(const QString& k)const;
    QJsonValue operator[](const QString& k)const;
    bool isEmpty()const{return d.empty();} int size()const{return (int)d.size();}
    QStringList keys()const{QStringList r;for(auto&p:d)r.push_back(p.first);return r;}
};
class QJsonArray {
    std::vector<std::string> d;
public:
    QJsonArray()=default; int size()const{return (int)d.size();} bool isEmpty()const{return d.empty();}
    void append(const QJsonValue&);
    QJsonValue at(int i)const;
};
class QJsonValue {
    std::string s_; double n_=0; bool b_=false; int t_=0;
public:
    QJsonValue()=default; QJsonValue(const QString& v):s_(v),t_(3){} QJsonValue(const char* v):s_(v?v:""),t_(3){} QJsonValue(double v):n_(v),t_(2){} QJsonValue(int v):n_(v),t_(2){} QJsonValue(bool v):b_(v),t_(4){}
    bool isString()const{return t_==3;} bool isDouble()const{return t_==2;} bool isBool()const{return t_==4;} bool isNull()const{return t_==0;} bool isObject()const{return t_==5;} bool isArray()const{return t_==6;}
    QString toString()const{return s_;} double toDouble()const{return n_;} int toInt()const{return (int)n_;} bool toBool()const{return b_;} QJsonObject toObject()const{return{};} QJsonArray toArray()const{return{};}
};
inline void QJsonObject::insert(const QString& k,const QJsonValue& v){d[std::string(k)]=std::string(v.toString());}
inline QJsonValue QJsonObject::value(const QString& k)const{auto it=d.find(std::string(k));return it!=d.end()?QJsonValue(QString(it->second)):QJsonValue();}
inline QJsonValue QJsonObject::operator[](const QString& k)const{return value(k);}
inline void QJsonArray::append(const QJsonValue& v){d.push_back(std::string(v.toString()));}
inline QJsonValue QJsonArray::at(int i)const{return i>=0&&i<(int)d.size()?QJsonValue(QString(d[i])):QJsonValue();}
class QJsonDocument {
public:
    QJsonDocument()=default; QJsonDocument(const QJsonObject& o):obj_(o){} QJsonDocument(const QJsonArray&){}
    static QJsonDocument fromJson(const QByteArray&){return{};} QByteArray toJson()const{return{};}
    QJsonObject object()const{return obj_;} QJsonArray array()const{return{};} bool isNull()const{return true;} bool isObject()const{return false;} bool isArray()const{return false;}
private: QJsonObject obj_;
};

// ── Network Types ────────────────────────────────────────────────────────────
class QNetworkRequest { public: QNetworkRequest()=default; QNetworkRequest(const QUrl&){} void setHeader(int,const QVariant&){} void setRawHeader(const QByteArray&,const QByteArray&){} QUrl url()const{return{};} };
class QNetworkReply : public QIODevice { public: QNetworkReply(QObject* p=nullptr):QIODevice(p){} QByteArray readAll()override{return{};} int error()const{return 0;} QString errorString()const{return{};} bool isFinished()const{return true;} int attribute(int)const{return 0;} };
class QNetworkAccessManager : public QObject { public: QNetworkAccessManager(QObject* p=nullptr):QObject(p){} QNetworkReply* get(const QNetworkRequest&){return nullptr;} QNetworkReply* post(const QNetworkRequest&,const QByteArray&){return nullptr;} QNetworkReply* put(const QNetworkRequest&,const QByteArray&){return nullptr;} QNetworkReply* deleteResource(const QNetworkRequest&){return nullptr;} };
class QTcpServer : public QObject { public: QTcpServer(QObject* p=nullptr):QObject(p){} bool listen(int=0,int=0){return false;} void close(){} bool isListening()const{return false;} int serverPort()const{return 0;} };
class QTcpSocket : public QIODevice { public: QTcpSocket(QObject* p=nullptr):QIODevice(p){} void connectToHost(const QString&,int){} void disconnectFromHost(){} bool waitForConnected(int=-1){return false;} };
class QWebSocket : public QObject { public: QWebSocket(const QString& = {},int=0,QObject* p=nullptr):QObject(p){} void open(const QUrl&){} void close(){} void sendTextMessage(const QString&){} void sendBinaryMessage(const QByteArray&){} bool isValid()const{return false;} };
class QWebChannel : public QObject { public: QWebChannel(QObject* p=nullptr):QObject(p){} void registerObject(const QString&,QObject*){} };
class QWebEngineView : public QWidget { public: QWebEngineView(QWidget* p=nullptr):QWidget(p){} void setUrl(const QUrl&){} void load(const QUrl&){} void setHtml(const QString&){} };

// ── Multimedia Stubs ─────────────────────────────────────────────────────────
class QAudioFormat { public: void setSampleRate(int){} void setChannelCount(int){} void setSampleFormat(int){} int sampleRate()const{return 0;} int channelCount()const{return 0;} enum SampleFormat{Int16=0,Float=1}; };
class QAudioDevice { public: QString description()const{return{};} QAudioFormat preferredFormat()const{return{};} bool isNull()const{return true;} };
class QAudioDeviceInfo { public: static QAudioDeviceInfo defaultInputDevice(){return{};} static QAudioDeviceInfo defaultOutputDevice(){return{};} QString deviceName()const{return{};} };
class QAudioSource : public QObject { public: QAudioSource(const QAudioFormat& = {},QObject* p=nullptr):QObject(p){} QAudioSource(const QAudioDevice&,const QAudioFormat& = {},QObject* p=nullptr):QObject(p){} void start(QIODevice*){} void stop(){} };
using QAudioInput = QAudioSource;
namespace QAudio { enum State { ActiveState=0, SuspendedState=1, StoppedState=2, IdleState=3 }; }
class QMediaDevices : public QObject { public: QMediaDevices(QObject* p=nullptr):QObject(p){} static QList<QAudioDevice> audioInputs(){return{};} static QList<QAudioDevice> audioOutputs(){return{};} static QAudioDevice defaultAudioInput(){return{};} };

// ── Charts Stubs ─────────────────────────────────────────────────────────────
class QChart : public QWidget { public: QChart(QWidget* p=nullptr):QWidget(p){} void addSeries(void*){} void createDefaultAxes(){} void setTitle(const QString&){} void setTheme(int){} void legend(){} };
class QChartView : public QWidget { public: QChartView(QWidget* p=nullptr):QWidget(p){} QChartView(QChart*,QWidget* p=nullptr):QWidget(p){} };
class QLineSeries : public QObject { public: QLineSeries(QObject* p=nullptr):QObject(p){} void append(double,double){} void setName(const QString&){} void clear(){} };
class QBarSeries : public QObject { public: QBarSeries(QObject* p=nullptr):QObject(p){} };
class QValueAxis : public QObject { public: QValueAxis(QObject* p=nullptr):QObject(p){} void setRange(double,double){} void setTitleText(const QString&){} };
class QDateTimeAxis : public QObject { public: QDateTimeAxis(QObject* p=nullptr):QObject(p){} void setRange(const QDateTime&,const QDateTime&){} void setFormat(const QString&){} void setTitleText(const QString&){} };

// ── Crypto ───────────────────────────────────────────────────────────────────
class QCryptographicHash {
public:
    enum Algorithm{Md5=0,Sha1=1,Sha256=2,Sha512=3};
    QCryptographicHash(Algorithm){}  void addData(const QByteArray&){} void addData(const char*,int){} QByteArray result()const{return{};}
    static QByteArray hash(const QByteArray&,Algorithm){return{};}
};

// ── Misc ─────────────────────────────────────────────────────────────────────
class QRandomGenerator { public: static QRandomGenerator* global(){static QRandomGenerator g;return &g;} uint32_t generate(){return 0;} double generateDouble(){return 0;} uint32_t bounded(uint32_t h){(void)h;return 0;} };
class QLoggingCategory { public: QLoggingCategory(const char*){} bool isDebugEnabled()const{return false;} };
class QLibrary : public QObject { public: QLibrary(const QString& = {},QObject* p=nullptr):QObject(p){} bool load(){return false;} bool isLoaded()const{return false;} void* resolve(const char*){return nullptr;} };
class QMimeData { public: bool hasText()const{return false;} bool hasUrls()const{return false;} bool hasFormat(const QString&)const{return false;} QString text()const{return{};} QList<QUrl> urls()const{return{};} QByteArray data(const QString&)const{return{};} void setText(const QString&){} void setData(const QString&,const QByteArray&){} };
class QSqlDatabase { public: static QSqlDatabase addDatabase(const QString&){return{};} bool open(){return false;} void close(){} bool isOpen()const{return false;} };

// ── QCoreApplication / QApplication ──────────────────────────────────────────
class QCoreApplication : public QObject {
public:
    QCoreApplication(int&,char**):QObject(nullptr){} static QCoreApplication* instance(){return nullptr;}
    static void processEvents(){} static void quit(){} static int exec(){return 0;}
    static QString applicationDirPath(){return{};} static QString applicationFilePath(){return{};}
    static void setOrganizationName(const QString&){} static void setApplicationName(const QString&){} static void setApplicationVersion(const QString&){}
};
class QApplication : public QCoreApplication { public: QApplication(int& argc,char** argv):QCoreApplication(argc,argv){} static QApplication* instance(){return nullptr;} static void setStyle(const QString&){} static int exec(){return 0;} };

// ─────────────────────────────────────────────────────────────────────────────
// 5. Namespace Qt — common enums
// ─────────────────────────────────────────────────────────────────────────────
namespace Qt {
    enum AlignmentFlag { AlignLeft=0x1, AlignRight=0x2, AlignHCenter=0x4, AlignTop=0x20, AlignBottom=0x40, AlignVCenter=0x80, AlignCenter=AlignHCenter|AlignVCenter };
    enum Orientation { Horizontal=0x1, Vertical=0x2 };
    enum WindowType { Widget=0, Window=1, Dialog=2, Popup=4, ToolTip=8, WindowStaysOnTopHint=0x40000 };
    enum WidgetAttribute { WA_DeleteOnClose=55, WA_OpaquePaintEvent=4, WA_TranslucentBackground=0x7E };
    enum ContextMenuPolicy { DefaultContextMenu=1, CustomContextMenu=3 };
    enum CursorShape { ArrowCursor=0, WaitCursor=3, PointingHandCursor=13, IBeamCursor=4 };
    enum ItemDataRole { DisplayRole=0, DecorationRole=1, EditRole=2, ToolTipRole=3, StatusTipRole=4, UserRole=0x100 };
    enum SortOrder { AscendingOrder=0, DescendingOrder=1 };
    enum TextInteractionFlag { TextSelectableByMouse=1, TextSelectableByKeyboard=2, LinksAccessibleByMouse=4 };
    enum DockWidgetArea { LeftDockWidgetArea=1, RightDockWidgetArea=2, TopDockWidgetArea=4, BottomDockWidgetArea=8, AllDockWidgetAreas=0xf };
    enum ToolBarArea { LeftToolBarArea=1, RightToolBarArea=2, TopToolBarArea=4, BottomToolBarArea=8 };
    enum ToolButtonStyle { ToolButtonIconOnly=0, ToolButtonTextOnly=1, ToolButtonTextBesideIcon=2, ToolButtonTextUnderIcon=3 };
    enum Key { Key_Escape=0x01000000, Key_Return=0x01000004, Key_Enter=0x01000005, Key_Tab=0x01000001, Key_Backspace=0x01000003, Key_Delete=0x01000007, Key_F1=0x01000030, Key_F5=0x01000034, Key_F11=0x0100003A };
    enum KeyboardModifier { NoModifier=0, ShiftModifier=0x02000000, ControlModifier=0x04000000, AltModifier=0x08000000 };
    enum CheckState { Unchecked=0, PartiallyChecked=1, Checked=2 };
    enum ScrollBarPolicy { ScrollBarAsNeeded=0, ScrollBarAlwaysOff=1, ScrollBarAlwaysOn=2 };
    enum FocusPolicy { NoFocus=0, TabFocus=1, ClickFocus=2, StrongFocus=11 };
    enum SizePolicy { Fixed=0, Minimum=1, Maximum=4, Preferred=5, Expanding=7, Ignored=13 };
    enum ConnectionType { AutoConnection=0, DirectConnection=1, QueuedConnection=2, UniqueConnection=0x80 };
    enum MouseButton { NoButton=0, LeftButton=1, RightButton=2, MiddleButton=4 };
    enum GlobalColor { white=3, black=2, red=7, green=8, blue=9, cyan=10, magenta=11, yellow=12, gray=5, darkGray=4, lightGray=6, transparent=19 };
    enum TextFormat { PlainText=0, RichText=1, AutoText=2, MarkdownText=3 };
    enum WindowState { WindowNoState=0, WindowMinimized=1, WindowMaximized=2, WindowFullScreen=4 };
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Compatibility: SIGNAL/SLOT macros used with QObject::connect
// ─────────────────────────────────────────────────────────────────────────────
#ifndef SIGNAL
#define SIGNAL(a) #a
#endif
#ifndef SLOT
#define SLOT(a) #a
#endif

#endif // RAWRXD_QT_BLOCK_H
