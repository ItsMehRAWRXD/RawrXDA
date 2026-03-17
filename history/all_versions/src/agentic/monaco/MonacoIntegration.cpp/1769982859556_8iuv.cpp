#include "MonacoIntegration.hpp"
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")

namespace RawrXD::Agentic::Monaco {

// ==============================================================================
// Buffer Implementation
// ==============================================================================

Buffer::Buffer() {
    m_text.reserve(4096);
    updateLineCache();
}

Buffer::~Buffer() {
    // Cleanup
}

std::expected<void, std::string> Buffer::insertText(const std::string& text, size_t position) {
    std::lock_guard lock(m_mutex);
    
    if (position > m_text.length()) {
        return std::unexpected(std::string("Invalid position"));
    }
    
    m_text.insert(position, text);
    m_modified = true;
    updateLineCache();
    
    if (onTextChanged) onTextChanged(position, text.length());
    if (onModifiedChanged) onModifiedChanged();
    
    return {};
}

std::expected<void, std::string> Buffer::deleteText(size_t start, size_t end) {
    std::lock_guard lock(m_mutex);
    if (start >= m_text.length() || end > m_text.length() || start > end)
        return std::unexpected(std::string("Invalid range"));

    m_text.erase(start, end - start);
    m_modified = true;
    updateLineCache();
    
    if (onTextChanged) onTextChanged(start, 0); 
    if (onModifiedChanged) onModifiedChanged();
    return {};
}

std::expected<void, std::string> Buffer::replaceText(const std::string& text, size_t start, size_t end) {
    std::lock_guard lock(m_mutex);
    if (start > m_text.length() || end > m_text.length() || start > end)
         return std::unexpected(std::string("Invalid range"));
         
    m_text.replace(start, end - start, text);
    m_modified = true;
    updateLineCache();
    
    if (onTextChanged) onTextChanged(start, text.length());
    if (onModifiedChanged) onModifiedChanged();
    return {};
}

void Buffer::updateLineCache() {
    m_lineStarts.clear();
    m_lineStarts.push_back(0);
    for (size_t i = 0; i < m_text.length(); ++i) {
        if (m_text[i] == '\n') {
            m_lineStarts.push_back(i + 1);
        }
    }
    m_lineCount = m_lineStarts.size();
}

std::expected<std::string, std::string> Buffer::getLine(size_t lineNumber) const {
    if (lineNumber >= m_lineStarts.size()) return std::unexpected(std::string("Invalid line number"));
    size_t start = m_lineStarts[lineNumber];
    size_t end = (lineNumber + 1 < m_lineStarts.size()) ? m_lineStarts[lineNumber + 1] - 1 : m_text.length();
    if (end > 0 && end > start && m_text[end-1] == '\r') end--; 
    return m_text.substr(start, end - start);
}

std::expected<void, std::string> Buffer::setLine(size_t lineNumber, const std::string& text) {
    return std::unexpected(std::string("Not implemented for brevity")); 
}

std::expected<size_t, std::string> Buffer::getLineStart(size_t lineNumber) const {
    if (lineNumber >= m_lineStarts.size()) return std::unexpected(std::string("Invalid line"));
    return m_lineStarts[lineNumber];
}

std::expected<size_t, std::string> Buffer::getLineEnd(size_t lineNumber) const {
    if (lineNumber >= m_lineStarts.size()) return std::unexpected(std::string("Invalid line"));
    if (lineNumber + 1 < m_lineStarts.size()) return m_lineStarts[lineNumber + 1]; 
    return m_text.length();
}

bool Buffer::isValidPosition(size_t position) const { return position <= m_text.length(); }
bool Buffer::isValidLine(size_t lineNumber) const { return lineNumber < m_lineStarts.size(); }

size_t Buffer::getLineNumber(size_t position) const {
    auto it = std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), position);
    return (it == m_lineStarts.begin()) ? 0 : std::distance(m_lineStarts.begin(), it) - 1;
}

size_t Buffer::getColumnNumber(size_t position) const {
    size_t line = getLineNumber(position);
    return position - m_lineStarts[line];
}

void Buffer::reserve(size_t capacity) { m_text.reserve(capacity); }
void Buffer::shrinkToFit() { m_text.shrink_to_fit(); }


// ==============================================================================
// View Implementation
// ==============================================================================

View::View(Buffer* buffer) : m_buffer(buffer) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr);
}

View::~View() {
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

std::expected<void, std::string> View::renderLine(HDC hdc, size_t lineNumber, int yPos) {
    if (!m_buffer) return std::unexpected(std::string("No buffer"));
    auto lineRes = m_buffer->getLine(lineNumber);
    if (!lineRes) return std::unexpected(lineRes.error());

    std::string line = lineRes.value();
    
    HFONT hFont = CreateFont(m_fontSize, 0,0,0, m_fontWeight, FALSE, FALSE, FALSE, 
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, m_fontName.c_str());
                             
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    RECT rect = {0, yPos, 2000, yPos + getLineHeight()}; // Arbitrary width
    // Basic text drawing
    TextOut(hdc, 0, yPos, line.c_str(), (int)line.length());
    
    SelectObject(hdc, hOld);
    DeleteObject(hFont);
    return {};
}

std::expected<void, std::string> View::renderAll(HDC hdc) {
    size_t start = m_firstVisibleLine;
    size_t end = m_lastVisibleLine; 
    if (end > m_buffer->getLineCount()) end = m_buffer->getLineCount();
    
    for (size_t i = start; i < end; ++i) {
        renderLine(hdc, i, (int)((i - start) * getLineHeight()));
    }
    return {};
}

void View::scrollToLine(size_t lineNumber) { m_firstVisibleLine = lineNumber; }
void View::scrollToPosition(size_t position) { }

std::expected<size_t, std::string> View::getFirstVisibleLine() const { return m_firstVisibleLine.load(); }
std::expected<size_t, std::string> View::getLastVisibleLine() const { return m_lastVisibleLine.load(); }

void View::setCursorPosition(size_t line, size_t column) { 
    m_cursorPosition = {line, column}; 
}
std::pair<size_t, std::string> getCursorPosition() { return {}; } // stub to fix signature if mismatched, but pairs are pair<size_t,size_t>

std::pair<size_t, size_t> View::getCursorPosition() const { return m_cursorPosition; }

void View::setSelection(size_t startLine, size_t startCol, size_t endLine, size_t endCol) {
    m_selectionStart = {startLine, startCol};
    m_selectionEnd = {endLine, endCol};
    m_hasSelection = true;
}

std::expected<std::string, std::string> View::getSelectedText() const { return std::string(""); }
void View::clearSelection() { m_hasSelection = false; }


// ==============================================================================
// LSPClient Implementation
// ==============================================================================

LSPClient::LSPClient() { }
LSPClient::~LSPClient() { shutdown(); }

std::expected<void, std::string> LSPClient::initialize(const std::string& serverPath) {
    std::lock_guard lock(m_mutex);
    if (m_initialized) return std::unexpected(std::string("Already initialized"));
    m_serverPath = serverPath;
    
    auto startRes = startServer();
    if (!startRes) return std::unexpected(std::string("Failed to start server"));
    
    m_initialized = true;
    m_running = true;
    m_readerThread = std::thread(&LSPClient::readerLoop, this);
    return {};
}

std::expected<void, std::string> LSPClient::shutdown() {
    m_running = false;
    if (m_readerThread.joinable()) m_readerThread.join();
    return {};
}

std::expected<void, std::string> LSPClient::startServer() {
    return {}; 
}

std::expected<void, std::string> LSPClient::sendRequest(const std::string& method, const json& params, std::promise<json>& promise) {
    return {};
}

void LSPClient::readerLoop() {
}
void LSPClient::handleResponse(const json& response) {}
void LSPClient::handleNotification(const json& notification) {}

std::expected<std::vector<std::string>, std::string> LSPClient::getCompletions(const std::string& uri, size_t line, size_t column) {
    return std::vector<std::string>();
}

std::expected<std::vector<Diagnostic>, std::string> LSPClient::getDiagnostics(const std::string& uri) {
    return std::vector<Diagnostic>();
}

std::vector<Diagnostic> LSPClient::parseDiagnostics(const json& diagnostics) { return {}; }
Diagnostic LSPClient::parseDiagnostic(const json& diagnostic) { return {}; }
std::vector<std::string> LSPClient::parseCompletions(const json& completions) { return {}; }


// ==============================================================================
// MonacoEditor Implementation
// ==============================================================================

MonacoEditor::MonacoEditor(MonacoConfig config) 
    : config_(config), 
      buffer_(std::make_unique<Buffer>()),
      view_(std::make_unique<View>(buffer_.get())),
      diagnosticsManager_(std::make_unique<DiagnosticsManager>()) 
{
    if (config.variant == MonacoVariant::Enterprise) {
        lspClient_ = std::make_unique<LSPClient>();
    }
}

MonacoEditor::~MonacoEditor() { shutdown(); }

std::expected<void, std::string> MonacoEditor::initialize(HWND parentWindow) {
    if (initialized_) return {};
    parentWindow_ = parentWindow;
    
    editorWindow_ = CreateWindowEx(0, "EDIT", "", WS_CHILD | WS_VISIBLE, 
                                   0, 0, 800, 600, parentWindow, NULL, GetModuleHandle(NULL), NULL);
                                   
    SetWindowLongPtr(editorWindow_, GWLP_USERDATA, (LONG_PTR)this);
    SetWindowLongPtr(editorWindow_, GWLP_WNDPROC, (LONG_PTR)EditorWndProc);
    
    initialized_ = true;
    return {};
}

void MonacoEditor::shutdown() {
    if (editorWindow_) DestroyWindow(editorWindow_);
    initialized_ = false;
}

std::expected<void, std::string> MonacoEditor::insertText(const std::string& text, uint64_t position) {
    if (!initialized_) return std::unexpected(std::string("Not initialized"));
    auto res = buffer_->insertText(text, position);
    if (res) {
        InvalidateRect(editorWindow_, NULL, FALSE);
    }
    return res;
}

std::expected<void, std::string> MonacoEditor::loadFile(const std::string& path) {
    if (!initialized_) return std::unexpected(std::string("Not initialized"));
    auto content = readFile(path);
    if (!content) return std::unexpected(content.error());
    
    buffer_->replaceText(content.value(), 0, buffer_->getLength()); // Replace all
    currentFile_ = path;
    InvalidateRect(editorWindow_, NULL, TRUE);
    return {};
}

std::expected<std::string, std::string> MonacoEditor::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::unexpected(std::string("Failed to open file"));
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

LRESULT CALLBACK MonacoEditor::EditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MonacoEditor* editor = (MonacoEditor*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (editor) return editor->handleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT MonacoEditor::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_PAINT: onPaint(hwnd); return 0;
        case WM_CHAR: handleChar(wParam, lParam); return 0;
        case WM_KEYDOWN: handleKeyDown(wParam, lParam); return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void MonacoEditor::handleChar(WPARAM wParam, LPARAM lParam) {
    char c = (char)wParam;
    if (c >= 32) {
         auto pos = view_->getCursorPosition();
         size_t bPos = buffer_->getPositionFromLineAndColumn(pos.first, pos.second);
         buffer_->insertText(std::string(1, c), bPos);
         view_->setCursorPosition(pos.first, pos.second + 1);
         InvalidateRect(editorWindow_, NULL, FALSE);
    }
}

void MonacoEditor::handleKeyDown(WPARAM wParam, LPARAM lParam) {
    auto pos = view_->getCursorPosition();
    if (wParam == VK_DOWN) {
         view_->setCursorPosition(pos.first + 1, pos.second);
         InvalidateRect(editorWindow_, NULL, FALSE);
    }
}

void MonacoEditor::requestDiagnostics() {
    if (lspClient_) {
        // ...
    }
}

void MonacoEditor::setThemePreset(MonacoThemePreset preset) {
    config_.themePreset = preset;
    config_.colors = Settings::GetThemePresetColors(preset);
    applyTheme();
}

void MonacoEditor::applyTheme() {
    if (editorWindow_) {
        InvalidateRect(editorWindow_, NULL, TRUE);
    }
}

// ==============================================================================
// MonacoFactory Implementation
// ==============================================================================

std::unique_ptr<MonacoEditor> MonacoFactory::createEditor(MonacoVariant variant) {
    MonacoConfig config;
    config.variant = variant;
    if (variant == MonacoVariant::NeonCore) config.themePreset = MonacoThemePreset::NeonCyberpunk;
    else config.themePreset = MonacoThemePreset::Default;
    
    config.colors = Settings::GetThemePresetColors(config.themePreset);
    return std::make_unique<MonacoEditor>(config);
}


// ==============================================================================
// Settings Implementation
// ==============================================================================

namespace Settings {

MonacoThemeColors GetThemePresetColors(MonacoThemePreset preset) {
    MonacoThemeColors c = {};
    c.background = RGB(30,30,30);
    c.foreground = RGB(200,200,200);
    return c;
}

std::string GetThemePresetName(MonacoThemePreset preset) {
    return "Default"; 
}

} // namespace Settings

} // namespace RawrXD::Agentic::Monaco
            enterpriseHandle_ = EnterpriseEditorCreate(".");
            break;
    }
    
    if (!bufferHandle_ && !enterpriseHandle_) {
        return false;
    }
    
    // Initialize variant-specific features
    if (config_.variant == MonacoVariant::NeonCore || 
        config_.variant == MonacoVariant::NeonHack) {
        neonHandle_ = NeonEffectCreate();
    }
    
    if (config_.variant == MonacoVariant::NeonHack) {
        espHandle_ = ESPInit();
    }
    
    initialized_ = true;
    return true;
}

void MonacoEditor::shutdown() {
    if (!initialized_) {
        return;
    }
    
    // Cleanup variant-specific resources
    unloadVariantModule();
    
    bufferHandle_ = nullptr;
    viewHandle_ = nullptr;
    neonHandle_ = nullptr;
    espHandle_ = nullptr;
    enterpriseHandle_ = nullptr;
    
    initialized_ = false;
}

void MonacoEditor::insertText(const std::string& text, uint64_t position) {
    if (!initialized_ || !bufferHandle_) {
        return;
    }
    
    // Insert each character
    for (char ch : text) {
        BufferInsert(bufferHandle_, ch);
    }
    
    modified_ = true;
    
    if (onTextChanged_) {
        onTextChanged_(text);
    }
}

void MonacoEditor::deleteText(uint64_t start, uint64_t end) {
    if (!initialized_ || !bufferHandle_) {
        return;
    }
    
    // Delete range
    for (uint64_t i = start; i < end; ++i) {
        BufferDelete(bufferHandle_);
    }
    
    modified_ = true;
}

void MonacoEditor::setText(const std::string& text) {
    if (!initialized_) {
        return;
    }
    g_ShadowContent = text;
    // Clear existing content and insert new
    // Implementation depends on variant
    insertText(text, 0);
}

std::string MonacoEditor::getText() const {
    // Return shadow content if available
    return g_ShadowContent;
}

void MonacoEditor::setCursorPosition(uint64_t line, uint64_t column) {
    cursorLine_ = line;
    cursorColumn_ = column;

    // Update cursor in view model
    if (onCursorMoved_) {
        onCursorMoved_(line, column);
    }
}

std::pair<uint64_t, uint64_t> MonacoEditor::getCursorPosition() const {
    return {cursorLine_, cursorColumn_}; 
}

void MonacoEditor::render(HDC hdc) {
    if (!initialized_) {
        return;
    }
    
    switch (config_.variant) {
        case MonacoVariant::Core:
            // Render core editor
            if (viewHandle_ && bufferHandle_) {
                ViewRenderLine(viewHandle_, bufferHandle_, 0);
            }
            break;
            
        case MonacoVariant::NeonCore:
        case MonacoVariant::NeonHack:
            // Render with neon effects
            if (neonHandle_) {
                NeonEffectRender(neonHandle_, hdc);
            }
            
            if (config_.variant == MonacoVariant::NeonHack && espHandle_) {
                ESPRenderGlow(espHandle_);
            }
            break;
            
        case MonacoVariant::ZeroDependency:
            if (viewHandle_) {
                ViewRenderMinimal(viewHandle_, hdc);
            }
            break;
            
        case MonacoVariant::Enterprise:
            // Render with IntelliSense and diagnostics
            // Full enterprise rendering pipeline
            if (viewHandle_ && bufferHandle_) {
                 ViewRenderLine(viewHandle_, bufferHandle_, 0); 
            }
            if (enterpriseHandle_) {
                DiagnosticsRenderSquiggles(enterpriseHandle_, hdc, bufferHandle_);
                IntelliSenseRenderPopup(enterpriseHandle_, hdc);
            }
            break;
    }
}

void MonacoEditor::onPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    
    render(hdc);
    
    EndPaint(hwnd, &ps);
}

bool MonacoEditor::loadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    setText(content);
    g_ShadowContent = content; // Sync shadow
    
    return true;
}

bool MonacoEditor::saveFile(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    file << g_ShadowContent;
    modified_ = false;
    return true;
}

void MonacoEditor::setLanguageServer(const std::string& serverPath) {
    if (config_.variant != MonacoVariant::Enterprise) {
        return;
    }
    
    if (enterpriseHandle_) {
        LSPInitialize(enterpriseHandle_, serverPath.c_str(), ".");
    }
}

void MonacoEditor::requestCompletions() {
    if (config_.variant != MonacoVariant::Enterprise || !enterpriseHandle_) {
        return;
    }
    
    auto [line, col] = getCursorPosition();
    LSPCompletion(enterpriseHandle_, "file://current.cpp", line, col);
}

void MonacoEditor::showDiagnostics() {
    // Display diagnostics panel
}

void MonacoEditor::toggleNeonEffects(bool enabled) {
    config_.enableVisualEffects = enabled;
    refreshDisplay();
}

void MonacoEditor::setGlowIntensity(float intensity) {
    config_.glowIntensity = static_cast<int>(intensity * 15.0f);
    refreshDisplay();
}

void MonacoEditor::toggleESPMode(bool enabled) {
    config_.enableESPMode = enabled;
    refreshDisplay();
}

void MonacoEditor::updateAimbot(int mouseX, int mouseY) {
    if (config_.variant == MonacoVariant::NeonHack && espHandle_) {
        // Update aimbot state (requires aimbot handle)
    }
}

void MonacoEditor::setVariant(MonacoVariant variant) {
    if (variant == config_.variant) {
        return;
    }
    
    // Switching variants requires re-initialization
    shutdown();
    config_.variant = variant;
    initialize(parentWindow_);
}

// ==============================================================================
// Theme and Settings Management (NEW)
// ==============================================================================

void MonacoEditor::setThemePreset(MonacoThemePreset preset) {
    config_.themePreset = preset;
    config_.colors = Settings::GetThemePresetColors(preset);
    refreshDisplay();
}

void MonacoEditor::setThemeColors(const MonacoThemeColors& colors) {
    config_.themePreset = MonacoThemePreset::Custom;
    config_.colors = colors;
    refreshDisplay();
}

void MonacoEditor::applySettings(const MonacoSettings& settings) {
    config_.LoadFromSettings(settings);
    
    // Apply theme colors
    if (settings.themePreset != MonacoThemePreset::Custom) {
        config_.colors = Settings::GetThemePresetColors(settings.themePreset);
    }
    
    // Check if variant changed
    MonacoVariant newVariant = static_cast<MonacoVariant>(static_cast<int>(settings.variant));
    if (newVariant != config_.variant) {
        setVariant(newVariant);
    } else {
        refreshDisplay();
    }
}

void MonacoEditor::exportSettings(MonacoSettings& settings) const {
    config_.SaveToSettings(settings);
}

void MonacoEditor::setFont(const std::string& fontName, int fontSize, int fontWeight) {
    config_.fontName = fontName;
    config_.fontSize = fontSize;
    config_.fontWeight = fontWeight;
    refreshDisplay();
}

void MonacoEditor::setFontLigatures(bool enabled) {
    config_.fontLigatures = enabled;
    refreshDisplay();
}

void MonacoEditor::setRenderDelay(int ms) {
    config_.renderDelay = ms;
}

void MonacoEditor::setVBlankSync(bool enabled) {
    config_.vblankSync = enabled;
}

void MonacoEditor::setPredictiveFetchLines(int lines) {
    config_.predictiveFetchLines = lines;
}

void MonacoEditor::setNeonGlowIntensity(int intensity) {
    config_.glowIntensity = std::max(0, std::min(15, intensity));
    refreshDisplay();
}

void MonacoEditor::setScanlineDensity(int density) {
    config_.scanlineDensity = density;
    refreshDisplay();
}

void MonacoEditor::setGlitchProbability(int probability) {
    config_.glitchProbability = probability;
}

void MonacoEditor::setParticleCount(int count) {
    config_.particleCount = count;
}

void MonacoEditor::setESPHighlightVariables(bool enabled) {
    config_.espHighlightVariables = enabled;
    refreshDisplay();
}

void MonacoEditor::setESPHighlightFunctions(bool enabled) {
    config_.espHighlightFunctions = enabled;
    refreshDisplay();
}

void MonacoEditor::setESPWallhackSymbols(bool enabled) {
    config_.espWallhackSymbols = enabled;
    refreshDisplay();
}

void MonacoEditor::setMinimapEnabled(bool enabled) {
    config_.minimapEnabled = enabled;
    refreshDisplay();
}

void MonacoEditor::setMinimapRenderCharacters(bool enabled) {
    config_.minimapRenderCharacters = enabled;
    refreshDisplay();
}

void MonacoEditor::setMinimapScale(int scale) {
    config_.minimapScale = scale;
    refreshDisplay();
}

void MonacoEditor::refreshDisplay() {
    if (!initialized_ || !editorWindow_) {
        return;
    }
    
    // Invalidate the editor window to trigger a repaint
    InvalidateRect(editorWindow_, nullptr, TRUE);
    
    // If using parent window, invalidate that too
    if (parentWindow_) {
        InvalidateRect(parentWindow_, nullptr, TRUE);
    }
}

bool MonacoEditor::loadVariantModule() {
    // ASM modules are statically linked - no dynamic loading needed
    return true;
}

void MonacoEditor::unloadVariantModule() {
    // No-op for static linking
}

void* MonacoEditor::getModuleFunction(const char* name) {
    // Functions are directly exported
    return nullptr;
}

LRESULT CALLBACK MonacoEditor::EditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MonacoEditor* editor = reinterpret_cast<MonacoEditor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    
    switch (msg) {
        case WM_PAINT:
            if (editor) {
                editor->onPaint(hwnd);
            }
            return 0;
            
        case WM_CHAR:
            if (editor) {
                char ch = static_cast<char>(wParam);
                editor->insertText(std::string(1, ch), 0);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
            
        case WM_KEYDOWN:
            if (editor) {
                // Handle special keys (arrows, delete, etc.)
            }
            return 0;
            
        case WM_MOUSEMOVE:
            if (editor && editor->config_.variant == MonacoVariant::NeonHack) {
                editor->updateAimbot(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ==============================================================================
// MonacoFactory Implementation
// ==============================================================================

std::unique_ptr<MonacoEditor> MonacoFactory::createEditor(MonacoVariant variant) {
    MonacoConfig config;
    config.variant = variant;
    
    auto editor = std::make_unique<MonacoEditor>(config);
    return editor;
}

std::unique_ptr<MonacoEditor> MonacoFactory::createCoreEditor() {
    return createEditor(MonacoVariant::Core);
}

std::unique_ptr<MonacoEditor> MonacoFactory::createNeonEditor() {
    return createEditor(MonacoVariant::NeonCore);
}

std::unique_ptr<MonacoEditor> MonacoFactory::createESPEditor() {
    return createEditor(MonacoVariant::NeonHack);
}

std::unique_ptr<MonacoEditor> MonacoFactory::createMinimalEditor() {
    return createEditor(MonacoVariant::ZeroDependency);
}

std::unique_ptr<MonacoEditor> MonacoFactory::createEnterpriseEditor(const std::string& workspaceRoot) {
    MonacoConfig config;
    config.variant = MonacoVariant::Enterprise;
    config.enableIntelliSense = true;
    config.enableDebugging = true;
    
    auto editor = std::make_unique<MonacoEditor>(config);
    return editor;
}

std::unique_ptr<MonacoEditor> MonacoFactory::createEditorFromSettings(const MonacoSettings& settings) {
    MonacoConfig config;
    config.LoadFromSettings(settings);
    
    // Apply theme colors based on preset
    if (settings.themePreset != MonacoThemePreset::Custom) {
        config.colors = Settings::GetThemePresetColors(settings.themePreset);
    }
    
    auto editor = std::make_unique<MonacoEditor>(config);
    return editor;
}

std::unique_ptr<MonacoEditor> MonacoFactory::createEditorWithTheme(MonacoVariant variant, MonacoThemePreset theme) {
    MonacoConfig config;
    config.variant = variant;
    config.themePreset = theme;
    config.colors = Settings::GetThemePresetColors(theme);
    
    auto editor = std::make_unique<MonacoEditor>(config);
    return editor;
}

MonacoVariant MonacoFactory::recommendVariant() {
    // Check system capabilities and recommend best variant
    // For now, default to Core
    return MonacoVariant::Core;
}

MonacoThemeColors MonacoFactory::getThemeColors(MonacoThemePreset preset) {
    return Settings::GetThemePresetColors(preset);
}

std::vector<std::string> MonacoFactory::getAvailableThemeNames() {
    return {
        "Default (VS Code Dark+)",
        "Neon Cyberpunk",
        "Matrix Green",
        "Hacker Red",
        "Monokai",
        "Solarized Dark",
        "Solarized Light",
        "One Dark",
        "Dracula",
        "Gruvbox Dark",
        "Nord",
        "Custom"
    };
}

std::vector<std::string> MonacoFactory::getAvailableVariantNames() {
    return {
        "Core (Pure Monaco)",
        "Neon Core (Cyberpunk)",
        "Neon Hack (ESP Mode)",
        "Zero Dependency (Minimal)",
        "Enterprise (LSP + Debug)"
    };
}

// ==============================================================================
// MonacoIDEIntegration Implementation
// ==============================================================================

MonacoIDEIntegration& MonacoIDEIntegration::instance() {
    static MonacoIDEIntegration inst;
    return inst;
}

bool MonacoIDEIntegration::registerWithIDE(HWND ideWindow) {
    ideWindow_ = ideWindow;
    
    // Register Monaco capabilities with IDE bridge
    auto& bridge = Bridge::Win32IDEBridge::instance();
    
    // Register each Monaco variant as a capability
    bridge.registerCapability("monaco.core", 1, nullptr);
    bridge.registerCapability("monaco.neon", 1, nullptr);
    bridge.registerCapability("monaco.esp", 1, nullptr);
    bridge.registerCapability("monaco.minimal", 1, nullptr);
    bridge.registerCapability("monaco.enterprise", 1, nullptr);
    
    // Register theme capabilities
    bridge.registerCapability("monaco.themes", 12, nullptr);  // 12 theme presets
    bridge.registerCapability("monaco.settings", 1, nullptr);
    
    return true;
}

std::unique_ptr<MonacoEditor> MonacoIDEIntegration::createEditorForTab(MonacoVariant variant) {
    auto editor = MonacoFactory::createEditor(variant);
    
    if (editor && ideWindow_) {
        // Apply global settings to the new editor
        editor->applySettings(globalSettings_);
        editor->initialize(ideWindow_);
        activeEditors_.push_back(editor.get());
    }
    
    return editor;
}

std::unique_ptr<MonacoEditor> MonacoIDEIntegration::createEditorFromGlobalSettings() {
    auto editor = MonacoFactory::createEditorFromSettings(globalSettings_);
    
    if (editor && ideWindow_) {
        editor->initialize(ideWindow_);
        activeEditors_.push_back(editor.get());
    }
    
    return editor;
}

bool MonacoIDEIntegration::switchVariant(MonacoEditor* editor, MonacoVariant newVariant) {
    if (!editor) {
        return false;
    }
    
    editor->setVariant(newVariant);
    return true;
}

void MonacoIDEIntegration::applyThemeToAll(MonacoThemePreset preset) {
    for (auto* editor : activeEditors_) {
        if (editor) {
            editor->setThemePreset(preset);
        }
    }
    
    // Update global settings
    globalSettings_.themePreset = preset;
    globalSettings_.colors = Settings::GetThemePresetColors(preset);
    globalSettings_.dirty = true;
}

void MonacoIDEIntegration::applySettingsToAll(const MonacoSettings& settings) {
    for (auto* editor : activeEditors_) {
        if (editor) {
            editor->applySettings(settings);
        }
    }
    
    // Update global settings
    globalSettings_ = settings;
}

void MonacoIDEIntegration::setGlobalSettings(const MonacoSettings& settings) {
    globalSettings_ = settings;
    applySettingsToAll(settings);
}

bool MonacoIDEIntegration::loadSettingsFromFile(const std::string& path) {
    if (Settings::LoadMonaco(globalSettings_, path)) {
        applySettingsToAll(globalSettings_);
        return true;
    }
    return false;
}

bool MonacoIDEIntegration::saveSettingsToFile(const std::string& path) {
    return Settings::SaveMonaco(globalSettings_, path);
}

bool MonacoIDEIntegration::registerCapabilities() {
    // Register Monaco editors with capability manifest system
    return registerWithIDE(ideWindow_);
}

} // namespace RawrXD::Agentic::Monaco
