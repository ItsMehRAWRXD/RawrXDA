#pragma once

// Clean Qt-like stubs for native/headless builds (no Qt dependency)

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <fstream>
#include <thread>
#include <mutex>
#include <atomic>

class QString {
public:
    QString() = default;
    QString(const char* s) : data_(s ? s : "") {}
    QString(const std::string& s) : data_(s) {}
    bool isEmpty() const { return data_.empty(); }
    size_t size() const { return data_.size(); }
    const char* c_str() const { return data_.c_str(); }
    std::string toStdString() const { return data_; }
    const std::string& str() const { return data_; }
    QString arg(const QString& a0) const {
        std::string out = data_; auto pos = out.find("%1");
        if (pos != std::string::npos) out.replace(pos, 2, a0.data_);
        return QString(out);
    }
    QString arg(double value, int = 0, char = 'f', int precision = 2) const {
        char buf[128]; snprintf(buf, sizeof(buf), "%.*f", precision, value);
        return arg(QString(buf));
    }
    QString& prepend(const char* prefix) { data_.insert(0, prefix); return *this; }
    QString& operator+=(const QString& other) { data_ += other.data_; return *this; }
    friend QString operator+(const QString& lhs, const QString& rhs) { return QString(lhs.data_ + rhs.data_); }
private:
    std::string data_;
};

using QByteArray = std::string;
using qint64 = long long;

class QFile {
public:
    QFile(const QString& filename = "") : filename_(filename.toStdString()) {}
    enum OpenMode { ReadOnly = 0x1, WriteOnly = 0x2, ReadWrite = ReadOnly | WriteOnly, Text = 0x4 };
    bool open(int mode) {
        std::ios_base::openmode om{};
        if (mode & WriteOnly) om |= std::ios_base::out;
        if (mode & ReadOnly) om |= std::ios_base::in;
        file_.open(filename_, om);
        return file_.is_open();
    }
    void close() { file_.close(); }
    bool isOpen() const { return file_.is_open(); }
    qint64 write(const QByteArray& data) { if (!file_.is_open()) return -1; file_.write(data.c_str(), data.size()); return (qint64)data.size(); }
    QByteArray readAll() {
        if (!file_.is_open()) return {}; file_.seekg(0, std::ios::end);
        size_t size = (size_t)file_.tellg(); file_.seekg(0, std::ios::beg);
        QByteArray result(size, '\0'); file_.read(&result[0], size); return result;
    }
    static bool exists(const QString& filename) { std::ifstream f(filename.toStdString()); return f.good(); }
private:
    std::string filename_;
    std::fstream file_;
};

class QDir {
public:
    static bool exists(const QString& path) {
        DWORD attr = GetFileAttributesA(path.toStdString().c_str());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    }
    static QString currentPath() { char buf[MAX_PATH]; GetCurrentDirectoryA(MAX_PATH, buf); return QString(buf); }
};

class QTimer {
public:
    QTimer() : running_(false), interval_(1000), singleShot_(false) {}
    ~QTimer() { stop(); }
    void setInterval(int msec) { interval_ = msec; }
    void setSingleShot(bool s) { singleShot_ = s; }
    void start(int msec) { interval_ = msec; start(); }
    void start() {
        stop(); running_ = true;
        worker_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_));
                if (!running_) break;
                if (onTimeout) onTimeout();
                if (singleShot_) { running_ = false; break; }
            }
        });
    }
    void stop() { running_ = false; if (worker_.joinable()) worker_.join(); }
    std::function<void()> onTimeout;
private:
    std::atomic<bool> running_;
    int interval_;
    bool singleShot_;
    std::thread worker_;
};

class QThread { public: QThread() = default; ~QThread() { quit(); wait(); } void start() { quit(); running_ = true; thread_ = std::thread([this]() { if (onRun) onRun(); running_ = false; }); } void quit() { running_ = false; } void wait() { if (thread_.joinable()) thread_.join(); } bool isRunning() const { return running_; } static void msleep(unsigned long msecs) { std::this_thread::sleep_for(std::chrono::milliseconds(msecs)); } std::function<void()> onRun; private: std::thread thread_; std::atomic<bool> running_{false}; };

class QWidget { public: virtual ~QWidget() = default; void setVisible(bool) {} void setLayout(QWidget* ) {} void show() {} };
class QLayout : public QWidget { public: void addWidget(QWidget* ) {} void addLayout(QLayout* ) {} void addStretch() {} void setContentsMargins(int, int, int, int) {} };
class QVBoxLayout : public QLayout { public: explicit QVBoxLayout(QWidget* = nullptr) {} };
class QHBoxLayout : public QLayout { public: explicit QHBoxLayout(QWidget* = nullptr) {} };
class QLabel : public QWidget { public: explicit QLabel(const QString& text = "", QWidget* = nullptr) : text_(text) {} void setText(const QString& text) { text_ = text; } QString text() const { return text_; } private: QString text_; };

class QAction { public: explicit QAction(const QString& text = "", void* = nullptr) : text_(text) {} void setShortcut(const QString&) {} void setCheckable(bool) {} void setChecked(bool) {} void setDisabled(bool) {} void trigger() { if (onTriggered) onTriggered(); } std::function<void()> onTriggered; QString text() const { return text_; } private: QString text_; };
class QMenu; class QMenuBar : public QWidget { public: QMenu* addMenu(const QString& title); void createWin32Menu(HWND hwnd); static bool Dispatch(int id); private: std::vector<std::unique_ptr<QMenu>> menus_; static std::unordered_map<int, std::function<void()>> s_action_map_; static int s_next_id_; };
class QMenu : public QWidget { public: explicit QMenu(const QString& title = "", QWidget* = nullptr) : title_(title) {} QAction* addAction(const QString& text) { actions.emplace_back(std::make_unique<QAction>(text)); return actions.back().get(); } QAction* addSeparator() { actions.emplace_back(std::make_unique<QAction>(QString("---"))); return actions.back().get(); } void addMenu(QMenu* ) {} QString title() const { return title_; } std::vector<std::unique_ptr<QAction>> actions; private: QString title_; };
inline QMenu* QMenuBar::addMenu(const QString& title) { menus_.emplace_back(std::make_unique<QMenu>(title)); return menus_.back().get(); }
inline void QMenuBar::createWin32Menu(HWND hwnd) {
    HMENU hMenu = CreateMenu();
    for (auto& menu : menus_) {
        HMENU hSubMenu = CreatePopupMenu();
        for (auto& action : menu->actions) {
            const int id = s_next_id_++;
            s_action_map_[id] = action->onTriggered;
            AppendMenuA(hSubMenu, MF_STRING, id, action->text().toStdString().c_str());
        }
        AppendMenuA(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, menu->title().toStdString().c_str());
    }
    SetMenu(hwnd, hMenu);
}
inline bool QMenuBar::Dispatch(int id) { auto it = s_action_map_.find(id); if (it != s_action_map_.end()) { if (it->second) it->second(); return true; } return false; }
inline std::unordered_map<int, std::function<void()>> QMenuBar::s_action_map_{}; inline int QMenuBar::s_next_id_ = 1000;

class QToolBar : public QWidget { public: QAction* addAction(const QString& text) { actions_.emplace_back(std::make_unique<QAction>(text)); return actions_.back().get(); } void addSeparator() { addAction("---"); } void addWidget(QWidget* ) {} void setMovable(bool) {} void createWin32Toolbar(HWND ) { std::cout << "[Toolbar] Created with " << actions_.size() << " actions" << std::endl; } private: std::vector<std::unique_ptr<QAction>> actions_; };

class QDockWidget : public QWidget { public: explicit QDockWidget(const QString& = "", QWidget* = nullptr) {} void setAllowedAreas(int) {} void setWidget(QWidget* ) {} };
class QTabWidget : public QWidget { public: int addTab(QWidget* w, const QString& label) { tabs.emplace_back(w, label); return (int)tabs.size() - 1; } void removeTab(int index) { if (index >= 0 && index < (int)tabs.size()) tabs.erase(tabs.begin() + index); } void setCurrentIndex(int idx) { current_ = idx; } int currentIndex() const { return current_; } void setCurrentWidget(QWidget* w) { for (size_t i = 0; i < tabs.size(); ++i) if (tabs[i].first == w) current_ = (int)i; } int indexOf(QWidget* w) const { for (size_t i = 0; i < tabs.size(); ++i) if (tabs[i].first == w) return (int)i; return -1; } int count() const { return (int)tabs.size(); } QWidget* widget(int idx) const { return (idx >= 0 && idx < (int)tabs.size()) ? tabs[idx].first : nullptr; } void setTabsClosable(bool) {} void setMovable(bool) {} void setDocumentMode(bool) {} void setContextMenuPolicy(int) {} private: std::vector<std::pair<QWidget*, QString>> tabs; int current_ = 0; };

class QApplication { public: QApplication(int& argc, char** ) { std::cout << "[QApplication] Initialized with " << argc << " arguments" << std::endl; } static QApplication* instance() { return instance_; } int exec() { MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg);} return (int)msg.wParam; } void setStyleSheet(const QString&) {} private: static QApplication* instance_; };
inline QApplication* QApplication::instance_ = nullptr;

class QStringList : public std::vector<QString> { public: using std::vector<QString>::vector; };

class QProcess {
public:
    enum ProcessError { FailedToStart, Crashed, Timedout, ReadError, WriteError, UnknownError };
    
    QProcess() : m_exitCode(0), m_running(false) {}
    
    void start(const QString& program, const QStringList& arguments) {
        std::string cmd = program.toStdString();
        for (const auto& arg : arguments) {
            cmd += " \"" + arg.toStdString() + "\"";
        }
        
        // Create pipe for stdout
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        HANDLE hStdOutRead, hStdOutWrite;
        CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
        SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
        
        STARTUPINFOA si = { sizeof(STARTUPINFOA) };
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hStdOutWrite;
        si.hStdError = hStdOutWrite;
        
        PROCESS_INFORMATION pi = {};
        
        m_running = CreateProcessA(
            NULL, const_cast<char*>(cmd.c_str()),
            NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi
        );
        
        CloseHandle(hStdOutWrite);
        
        if (m_running) {
            m_processInfo = pi;
            m_stdoutHandle = hStdOutRead;
        } else {
            CloseHandle(hStdOutRead);
            m_exitCode = -1;
        }
    }
    
    bool waitForFinished(int msecs = 30000) {
        if (!m_running) return true;
        
        DWORD result = WaitForSingleObject(m_processInfo.hProcess, msecs);
        if (result == WAIT_OBJECT_0) {
            GetExitCodeProcess(m_processInfo.hProcess, (LPDWORD)&m_exitCode);
            
            // Read stdout
            char buffer[4096];
            DWORD bytesRead;
            while (ReadFile(m_stdoutHandle, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                m_stdout += buffer;
            }
            
            CloseHandle(m_stdoutHandle);
            CloseHandle(m_processInfo.hProcess);
            CloseHandle(m_processInfo.hThread);
            m_running = false;
            return true;
        }
        return false;
    }
    
    QByteArray readAllStandardOutput() { return m_stdout; }
    int exitCode() const { return m_exitCode; }
    bool isRunning() const { return m_running; }
    
    void terminate() {
        if (m_running) {
            TerminateProcess(m_processInfo.hProcess, 1);
            CloseHandle(m_processInfo.hProcess);
            CloseHandle(m_processInfo.hThread);
            CloseHandle(m_stdoutHandle);
            m_running = false;
        }
    }
    
private:
    PROCESS_INFORMATION m_processInfo;
    HANDLE m_stdoutHandle;
    std::string m_stdout;
    int m_exitCode;
    bool m_running;
};
class QObject { public: virtual ~QObject() = default; };

class QCommandPalette : public QWidget { public: explicit QCommandPalette(QWidget* = nullptr) {} void addCommand(const QString& name, const QString& description, std::function<void()> action) { commands_.push_back(Command{name, description, std::move(action)}); } void show() { std::cout << "[Command Palette] Available commands:" << std::endl; for (size_t i = 0; i < commands_.size(); ++i) { std::cout << "  " << (i + 1) << ". " << commands_[i].name.toStdString() << " - " << commands_[i].description.toStdString() << std::endl; } } void executeCommand(int index) { if (index >= 0 && index < (int)commands_.size()) { commands_[index].action(); } } private: struct Command { QString name; QString description; std::function<void()> action; }; std::vector<Command> commands_; };

class QMessageBox {
public:
    enum StandardButton { Save = 0x1, Discard = 0x2, Cancel = 0x4, Yes = 0x8, No = 0x10, Ok = 0x20 };
    
    static int question(QWidget*, const QString& title, const QString& text, int buttons) {
        int result = MessageBoxA(NULL, text.toStdString().c_str(), title.toStdString().c_str(),
            MB_YESNOCANCEL | MB_ICONQUESTION);
        switch (result) {
            case IDYES: return (buttons & Yes) ? Yes : Save;
            case IDNO: return (buttons & No) ? No : Discard;
            case IDCANCEL: default: return Cancel;
        }
    }
    
    static void about(QWidget*, const QString& title, const QString& text) {
        MessageBoxA(NULL, text.toStdString().c_str(), title.toStdString().c_str(), MB_OK | MB_ICONINFORMATION);
    }
    
    static void information(QWidget*, const QString& title, const QString& text) {
        MessageBoxA(NULL, text.toStdString().c_str(), title.toStdString().c_str(), MB_OK | MB_ICONINFORMATION);
    }
    
    static void warning(QWidget*, const QString& title, const QString& text) {
        MessageBoxA(NULL, text.toStdString().c_str(), title.toStdString().c_str(), MB_OK | MB_ICONWARNING);
    }
    
    static void critical(QWidget*, const QString& title, const QString& text) {
        MessageBoxA(NULL, text.toStdString().c_str(), title.toStdString().c_str(), MB_OK | MB_ICONERROR);
    }
};

class QSettings {
public:
    QSettings(const QString& organization = "", const QString& application = "")
        : m_organization(organization.toStdString())
        , m_application(application.toStdString())
    {
        loadFromFile();
    }
    
    ~QSettings() {
        saveToFile();
    }
    
    void setValue(const QString& key, const QByteArray& value) {
        store_[key.toStdString()] = value;
        saveToFile();
    }
    
    QByteArray value(const QString& key, const QByteArray& def = QByteArray()) const {
        auto it = store_.find(key.toStdString());
        return it == store_.end() ? def : it->second;
    }
    
    void sync() { saveToFile(); }
    
private:
    std::string getSettingsFilePath() const {
        char appData[MAX_PATH];
        if (GetEnvironmentVariableA("LOCALAPPDATA", appData, MAX_PATH) == 0) {
            strcpy(appData, ".");
        }
        std::string path = std::string(appData) + "\\" + m_organization + "\\" + m_application;
        CreateDirectoryA((std::string(appData) + "\\" + m_organization).c_str(), NULL);
        CreateDirectoryA(path.c_str(), NULL);
        return path + "\\settings.ini";
    }
    
    void loadFromFile() {
        std::string filepath = getSettingsFilePath();
        std::ifstream file(filepath);
        if (!file.is_open()) return;
        
        std::string line;
        while (std::getline(file, line)) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key = line.substr(0, eq);
                std::string value = line.substr(eq + 1);
                store_[key] = value;
            }
        }
    }
    
    void saveToFile() const {
        std::string filepath = getSettingsFilePath();
        std::ofstream file(filepath);
        if (!file.is_open()) return;
        
        for (const auto& [key, value] : store_) {
            file << key << "=" << value << "\n";
        }
    }
    
    std::string m_organization;
    std::string m_application;
    mutable std::unordered_map<std::string, QByteArray> store_;
};

class QMutex { public: void lock() { mtx_.lock(); } void unlock() { mtx_.unlock(); } private: std::mutex mtx_; };
class QMutexLocker { public: explicit QMutexLocker(QMutex* m) : m_(m) { if (m_) m_->lock(); } ~QMutexLocker() { if (m_) m_->unlock(); } private: QMutex* m_; };

class QDateTime { public: static long long currentMSecsSinceEpoch() { auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()); return now.time_since_epoch().count(); } };

class QKeySequence { public: QKeySequence() = default; explicit QKeySequence(const char* ) {} static QKeySequence New; };
inline QKeySequence QKeySequence::New = QKeySequence();

class QElapsedTimer { public: void start() { start_ = std::chrono::steady_clock::now(); } long long elapsed() const { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_).count(); } private: std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now(); };

inline std::ostream& qDebug() { return std::cout; }
constexpr int Qt_CustomContextMenu = 0;

