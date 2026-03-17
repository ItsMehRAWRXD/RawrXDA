#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <atomic>
#include <expected>
#include <sstream>
#include <fstream>
#include <iostream>
#include <WebView2.h>
#include <wrl.h>
#include <wil/com.h>

// Simple JSON implementation for WebView2 communication
namespace nlohmann {
    class json {
    private:
        std::string data_;
    public:
        json() = default;
        json(const std::string& str) : data_(str) {}
        json& operator=(const std::string& str) { data_ = str; return *this; }

        std::string dump() const { return data_; }

        json& operator[](const std::string& key) {
            // Simple implementation - just store as string
            data_ = "\"" + key + "\":" + data_;
            return *this;
        }

        json& operator=(const json& other) {
            data_ = other.data_;
            return *this;
        }
    };
}

namespace RawrXD::Agentic::Monaco {

enum class MonacoVariant {
    Core,           // Pure Monaco
    NeonCore,       // Cyberpunk theme
    NeonHack,       // ESP mode
    ZeroDependency, // Minimal
    Enterprise      // LSP + Debug
};

enum class MonacoThemePreset {
    Default,        // VS Code Dark+
    NeonCyberpunk,  // Cyberpunk
    MatrixGreen,    // Matrix
    HackerRed,      // Red theme
    Monokai,        // Monokai
    SolarizedDark,  // Solarized Dark
    SolarizedLight, // Solarized Light
    OneDark,        // One Dark
    Dracula,        // Dracula
    GruvboxDark,    // Gruvbox
    Nord,           // Nord
    Custom          // Custom colors
};

struct MonacoThemeColors {
    COLORREF background = RGB(30, 30, 30);
    COLORREF foreground = RGB(212, 212, 212);
    COLORREF selection = RGB(38, 79, 120);
    COLORREF lineHighlight = RGB(45, 45, 45);
    COLORREF comment = RGB(106, 153, 85);
    COLORREF keyword = RGB(86, 156, 214);
    COLORREF string = RGB(214, 157, 133);
    COLORREF number = RGB(181, 206, 168);
    COLORREF function = RGB(220, 220, 170);
    COLORREF variable = RGB(156, 220, 254);
    COLORREF type = RGB(78, 201, 176);
    COLORREF operatorColor = RGB(212, 212, 212);
    COLORREF error = RGB(255, 100, 100);
    COLORREF warning = RGB(255, 200, 100);
    COLORREF info = RGB(100, 200, 255);
    COLORREF hint = RGB(150, 150, 150);
    COLORREF minimapBackground = RGB(45, 45, 45);
    COLORREF minimapSelection = RGB(60, 60, 60);
    COLORREF minimapHighlight = RGB(90, 90, 90);
};

struct MonacoSettings {
    MonacoVariant variant = MonacoVariant::Core;
    MonacoThemePreset themePreset = MonacoThemePreset::Default;
    MonacoThemeColors colors;
    std::string fontName = "Consolas";
    int fontSize = 12;
    int fontWeight = FW_NORMAL;
    bool fontLigatures = false;
    int renderDelay = 0;
    bool vblankSync = true;
    int predictiveFetchLines = 10;
    bool enableVisualEffects = false;
    int glowIntensity = 8;
    int scanlineDensity = 50;
    int glitchProbability = 0;
    int particleCount = 100;
    bool enableESPMode = false;
    bool espHighlightVariables = true;
    bool espHighlightFunctions = true;
    bool espWallhackSymbols = false;
    bool minimapEnabled = true;
    bool minimapRenderCharacters = false;
    int minimapScale = 1;
    bool enableIntelliSense = false;
    bool enableDebugging = false;
    std::string workspaceRoot;
    bool dirty = false;

    void LoadFromSettings(const MonacoSettings& settings) { *this = settings; }
    void SaveToSettings(MonacoSettings& settings) const { settings = *this; }
};

class Buffer {
private:
    std::vector<std::string> lines_;
    std::mutex mutex_;
    std::atomic<bool> dirty_;
    std::string filePath_;
    std::chrono::system_clock::time_point lastModified_;

public:
    Buffer() : dirty_(false) {
        lines_.push_back(""); // Start with one empty line
    }

    ~Buffer() = default;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Core buffer operations
    std::expected<void, std::string> insertText(const std::string& text, size_t position) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (position > getLength()) {
            return std::unexpected("Position out of bounds");
        }

        size_t lineIndex = getLineIndex(position);
        size_t charIndex = position - getLineStart(lineIndex);

        std::string& line = lines_[lineIndex];
        line.insert(charIndex, text);

        // Handle newlines
        size_t newlinePos = text.find('\n');
        if (newlinePos != std::string::npos) {
            std::string remaining = line.substr(charIndex + newlinePos + 1);
            line = line.substr(0, charIndex + newlinePos);

            // Insert remaining lines
            lines_.insert(lines_.begin() + lineIndex + 1, remaining);
        }

        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> deleteText(size_t start, size_t end) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (start >= end || start >= getLength()) {
            return std::unexpected("Invalid range");
        }

        size_t startLine = getLineIndex(start);
        size_t endLine = getLineIndex(end);
        size_t startChar = start - getLineStart(startLine);
        size_t endChar = end - getLineStart(endLine);

        if (startLine == endLine) {
            // Same line deletion
            lines_[startLine].erase(startChar, endChar - startChar);
        } else {
            // Multi-line deletion
            lines_[startLine] = lines_[startLine].substr(0, startChar) + lines_[endLine].substr(endChar);
            lines_.erase(lines_.begin() + startLine + 1, lines_.begin() + endLine + 1);
        }

        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> replaceText(const std::string& text, size_t start, size_t end) {
        auto deleteResult = deleteText(start, end);
        if (!deleteResult) return deleteResult;

        return insertText(text, start);
    }

    // Line operations
    std::expected<void, std::string> joinLines(size_t line1, size_t line2) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (line1 >= lines_.size() || line2 >= lines_.size() || line1 >= line2) {
            return std::unexpected("Invalid line indices");
        }

        lines_[line1] += lines_[line2];
        lines_.erase(lines_.begin() + line2);

        dirty_ = true;
        return {};
    }

    // Accessors
    size_t getLength() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t length = 0;
        for (const auto& line : lines_) {
            length += line.length() + 1; // +1 for newline
        }
        return length > 0 ? length - 1 : 0; // Don't count trailing newline
    }

    size_t getLineCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lines_.size();
    }

    std::string getLine(size_t lineIndex) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lineIndex >= lines_.size()) return "";
        return lines_[lineIndex];
    }

    size_t getLineStart(size_t lineIndex) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lineIndex >= lines_.size()) return getLength();

        size_t pos = 0;
        for (size_t i = 0; i < lineIndex; ++i) {
            pos += lines_[i].length() + 1; // +1 for newline
        }
        return pos;
    }

    size_t getLineIndex(size_t position) const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t pos = 0;
        for (size_t i = 0; i < lines_.size(); ++i) {
            size_t lineLen = lines_[i].length() + 1;
            if (position < pos + lineLen) {
                return i;
            }
            pos += lineLen;
        }
        return lines_.size() - 1;
    }

    std::string getText(size_t start, size_t end) const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (start >= end || start >= getLength()) return "";

        size_t startLine = getLineIndex(start);
        size_t endLine = getLineIndex(end);
        size_t startChar = start - getLineStart(startLine);
        size_t endChar = end - getLineStart(endLine);

        if (startLine == endLine) {
            return lines_[startLine].substr(startChar, endChar - startChar);
        } else {
            std::string result = lines_[startLine].substr(startChar);
            for (size_t i = startLine + 1; i < endLine; ++i) {
                result += '\n' + lines_[i];
            }
            result += '\n' + lines_[endLine].substr(0, endChar);
            return result;
        }
    }

    std::string getAllText() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string result;
        for (size_t i = 0; i < lines_.size(); ++i) {
            if (i > 0) result += '\n';
            result += lines_[i];
        }
        return result;
    }

    void setText(const std::string& text) {
        std::lock_guard<std::mutex> lock(mutex_);

        lines_.clear();
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            lines_.push_back(line);
        }

        if (lines_.empty()) {
            lines_.push_back("");
        }

        dirty_ = true;
    }

    // File operations
    bool loadFromFile(const std::filesystem::path& path) {
        try {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) return false;

            std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            file.close();

            setText(content);
            filePath_ = path.string();
            lastModified_ = std::filesystem::last_write_time(path);
            dirty_ = false;
            return true;
        } catch (...) {
            return false;
        }
    }

    bool saveToFile(const std::filesystem::path& path) {
        try {
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) return false;

            file << getAllText();
            file.close();

            filePath_ = path.string();
            lastModified_ = std::filesystem::last_write_time(path);
            dirty_ = false;
            return true;
        } catch (...) {
            return false;
        }
    }

    // State
    bool isDirty() const { return dirty_; }
    void setDirty(bool dirty) { dirty_ = dirty; }
    const std::string& getFilePath() const { return filePath_; }
    void setFilePath(const std::string& path) { filePath_ = path; }
};

class MonacoEditor : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
                     public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                     public ICoreWebView2WebMessageReceivedEventHandler,
                     public ICoreWebView2NavigationCompletedEventHandler {
private:
    HWND hwndParent_;
    RECT rect_;
    wil::com_ptr<ICoreWebView2Controller> controller_;
    wil::com_ptr<ICoreWebView2> webview_;
    wil::com_ptr<ICoreWebView2Environment> environment_;
    EventRegistrationToken messageToken_;
    EventRegistrationToken navigationToken_;

    std::unique_ptr<Buffer> buffer_;
    MonacoSettings settings_;
    std::function<void(const std::string&)> onContentChanged_;
    std::function<void()> onReady_;

    bool initialized_ = false;
    std::string htmlContent_;

public:
    MonacoEditor(HWND hwndParent, const RECT& rect, const MonacoSettings& settings = {})
        : hwndParent_(hwndParent), rect_(rect), settings_(settings) {

        buffer_ = std::make_unique<Buffer>();
        InitializeWebView();
    }

    ~MonacoEditor() {
        if (controller_) {
            controller_->Close();
        }
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == IID_IUnknown ||
            riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
            *ppvObject = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
            *ppvObject = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == IID_ICoreWebView2WebMessageReceivedEventHandler) {
            *ppvObject = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == IID_ICoreWebView2NavigationCompletedEventHandler) {
            *ppvObject = static_cast<ICoreWebView2NavigationCompletedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount_; }
    ULONG STDMETHODCALLTYPE Release() override {
        if (--refCount_ == 0) {
            delete this;
            return 0;
        }
        return refCount_;
    }

private:
    ULONG refCount_ = 1;

    void InitializeWebView() {
        // Create HTML content for Monaco editor
        htmlContent_ = CreateMonacoHTML();

        // Create WebView2 environment
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, nullptr, nullptr,
            Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(this).Get());
    }

    std::string CreateMonacoHTML() {
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Monaco Editor</title>
    <script src="https://unpkg.com/monaco-editor@0.45.0/min/vs/loader.min.js"></script>
    <style>
        body {
            margin: 0;
            padding: 0;
            overflow: hidden;
            background-color: #1e1e1e;
        }
        #container {
            width: 100vw;
            height: 100vh;
        }
    </style>
</head>
<body>
    <div id="container"></div>
    <script>
        require.config({ paths: { vs: 'https://unpkg.com/monaco-editor@0.45.0/min/vs' } });
        require(['vs/editor/editor.main'], function() {
            window.editor = monaco.editor.create(document.getElementById('container'), {
                value: '',
                language: 'cpp',
                theme: 'vs-dark',
                fontSize: )" + std::to_string(settings_.fontSize) + R"(,
                fontFamily: ')" + settings_.fontName + R"(',
                minimap: { enabled: )" + (settings_.minimapEnabled ? "true" : "false") + R"( },
                scrollBeyondLastLine: false,
                automaticLayout: true,
                wordWrap: 'off',
                renderWhitespace: 'selection',
                renderControlCharacters: true,
                fontLigatures: )" + (settings_.fontLigatures ? "true" : "false") + R"(
            });

            // Listen for content changes
            window.editor.onDidChangeModelContent(function(e) {
                const content = window.editor.getValue();
                window.chrome.webview.postMessage({ type: 'contentChanged', content: content });
            });

            // Listen for cursor position changes
            window.editor.onDidChangeCursorPosition(function(e) {
                window.chrome.webview.postMessage({
                    type: 'cursorChanged',
                    position: e.position,
                    selection: window.editor.getSelection()
                });
            });

            window.chrome.webview.postMessage({ type: 'ready' });
        });

        // Handle messages from host
        window.chrome.webview.addEventListener('message', function(e) {
            const message = e.data;
            if (message.type === 'setContent') {
                if (window.editor) {
                    window.editor.setValue(message.content);
                }
            } else if (message.type === 'setLanguage') {
                if (window.editor) {
                    const model = window.editor.getModel();
                    if (model) {
                        monaco.editor.setModelLanguage(model, message.language);
                    }
                }
            } else if (message.type === 'setTheme') {
                if (window.editor) {
                    monaco.editor.setTheme(message.theme);
                }
            } else if (message.type === 'insertText') {
                if (window.editor) {
                    const position = window.editor.getPosition();
                    window.editor.executeEdits('', [{
                        range: new monaco.Range(position.lineNumber, position.column,
                                               position.lineNumber, position.column),
                        text: message.text
                    }]);
                }
            }
        });
    </script>
</body>
</html>
        )";
        return html;
    }

public:
    // ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* environment) override {
        if (FAILED(result)) return result;

        environment_ = environment;

        // Create controller
        return environment_->CreateCoreWebView2Controller(
            hwndParent_,
            Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(this).Get());
    }

    // ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* controller) override {
        if (FAILED(result)) return result;

        controller_ = controller;
        controller_->get_CoreWebView2(&webview_);

        // Set up event handlers
        webview_->add_WebMessageReceived(
            Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(this).Get(),
            &messageToken_);

        webview_->add_NavigationCompleted(
            Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(this).Get(),
            &navigationToken_);

        // Configure controller
        controller_->put_Bounds(rect_);

        // Navigate to HTML content
        webview_->NavigateToString(Utf8ToWide(htmlContent_).c_str());

        return S_OK;
    }

    // ICoreWebView2WebMessageReceivedEventHandler
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        wil::unique_cotaskmem_string message;
        args->get_WebMessageAsJson(&message);

        try {
            json msg = json::parse(WideToUtf8(message.get()));
            std::string type = msg["type"];

            if (type == "ready") {
                initialized_ = true;
                if (onReady_) onReady_();
            } else if (type == "contentChanged") {
                std::string content = msg["content"];
                buffer_->setText(content);
                if (onContentChanged_) onContentChanged_(content);
            }
        } catch (...) {
            // Invalid JSON, ignore
        }

        return S_OK;
    }

    // ICoreWebView2NavigationCompletedEventHandler
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) override {
        return S_OK;
    }

    // Public API
    void SetContent(const std::string& content) {
        if (!initialized_ || !webview_) return;

        json message = {
            {"type", "setContent"},
            {"content", content}
        };

        wil::unique_cotaskmem_string jsonStr(Utf8ToWide(message.dump()).c_str());
        webview_->PostWebMessageAsJson(jsonStr.get());
        buffer_->setText(content);
    }

    void SetLanguage(const std::string& language) {
        if (!initialized_ || !webview_) return;

        json message = {
            {"type", "setLanguage"},
            {"language", language}
        };

        wil::unique_cotaskmem_string jsonStr(Utf8ToWide(message.dump()).c_str());
        webview_->PostWebMessageAsJson(jsonStr.get());
    }

    void SetTheme(const std::string& theme) {
        if (!initialized_ || !webview_) return;

        json message = {
            {"type", "setTheme"},
            {"theme", theme}
        };

        wil::unique_cotaskmem_string jsonStr(Utf8ToWide(message.dump()).c_str());
        webview_->PostWebMessageAsJson(jsonStr.get());
    }

    void InsertText(const std::string& text) {
        if (!initialized_ || !webview_) return;

        json message = {
            {"type", "insertText"},
            {"text", text}
        };

        wil::unique_cotaskmem_string jsonStr(Utf8ToWide(message.dump()).c_str());
        webview_->PostWebMessageAsJson(jsonStr.get());
    }

    void Resize(const RECT& rect) {
        rect_ = rect;
        if (controller_) {
            controller_->put_Bounds(rect);
        }
    }

    Buffer* GetBuffer() { return buffer_.get(); }
    bool IsInitialized() const { return initialized_; }

    void SetOnContentChanged(std::function<void(const std::string&)> callback) {
        onContentChanged_ = callback;
    }

    void SetOnReady(std::function<void()> callback) {
        onReady_ = callback;
    }

private:
    static std::string WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string str(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
        str.resize(size - 1);
        return str;
    }

    static std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        std::wstring wstr(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
        wstr.resize(size - 1);
        return wstr;
    }
};

class MonacoEditorManager {
private:
    std::unordered_map<HWND, std::unique_ptr<MonacoEditor>> editors_;
    std::mutex mutex_;

public:
    MonacoEditor* CreateEditor(HWND hwndParent, const RECT& rect, const MonacoSettings& settings = {}) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto editor = std::make_unique<MonacoEditor>(hwndParent, rect, settings);
        auto* editorPtr = editor.get();
        editors_[hwndParent] = std::move(editor);

        return editorPtr;
    }

    void DestroyEditor(HWND hwndParent) {
        std::lock_guard<std::mutex> lock(mutex_);
        editors_.erase(hwndParent);
    }

    MonacoEditor* GetEditor(HWND hwndParent) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = editors_.find(hwndParent);
        return it != editors_.end() ? it->second.get() : nullptr;
    }

    void ResizeEditor(HWND hwndParent, const RECT& rect) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = editors_.find(hwndParent);
        if (it != editors_.end()) {
            it->second->Resize(rect);
        }
    }
};

} // namespace RawrXD::Agentic::Monaco
