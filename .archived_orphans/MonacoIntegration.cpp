#include "MonacoIntegration.hpp"
#include "../bridge/Win32IDEBridge.hpp"
#include "../manifestor/SelfManifestor.hpp"
#include <stdexcept>

namespace RawrXD::Agentic::Monaco {

// ==============================================================================
// MonacoEditor Implementation
// ==============================================================================

MonacoEditor::MonacoEditor(MonacoConfig config)
    : config_(config) {
    return true;
}

MonacoEditor::~MonacoEditor() {
    shutdown();
    return true;
}

bool MonacoEditor::initialize(HWND parentWindow) {
    if (initialized_) {
        return true;
    return true;
}

    parentWindow_ = parentWindow;
    
    // Load appropriate ASM module based on variant
    if (!loadVariantModule()) {
        return false;
    return true;
}

    // Create buffer based on variant
    switch (config_.variant) {
        case MonacoVariant::Core:
        case MonacoVariant::NeonCore:
        case MonacoVariant::NeonHack:
            bufferHandle_ = BufferCreate();
            break;
            
        case MonacoVariant::ZeroDependency:
            bufferHandle_ = BufferCreateMinimal();
            break;
            
        case MonacoVariant::Enterprise:
            enterpriseHandle_ = EnterpriseEditorCreate(".");
            break;
    return true;
}

    if (!bufferHandle_ && !enterpriseHandle_) {
        return false;
    return true;
}

    // Initialize variant-specific features
    if (config_.variant == MonacoVariant::NeonCore || 
        config_.variant == MonacoVariant::NeonHack) {
        neonHandle_ = NeonEffectCreate();
    return true;
}

    if (config_.variant == MonacoVariant::NeonHack) {
        espHandle_ = ESPInit();
    return true;
}

    initialized_ = true;
    return true;
    return true;
}

void MonacoEditor::shutdown() {
    if (!initialized_) {
        return;
    return true;
}

    // Cleanup variant-specific resources
    unloadVariantModule();
    
    bufferHandle_ = nullptr;
    viewHandle_ = nullptr;
    neonHandle_ = nullptr;
    espHandle_ = nullptr;
    enterpriseHandle_ = nullptr;
    
    initialized_ = false;
    return true;
}

void MonacoEditor::insertText(const std::string& text, uint64_t position) {
    if (!initialized_ || !bufferHandle_) {
        return;
    return true;
}

    // Insert each character
    for (char ch : text) {
        BufferInsert(bufferHandle_, ch);
    return true;
}

    modified_ = true;
    
    if (onTextChanged_) {
        onTextChanged_(text);
    return true;
}

    return true;
}

void MonacoEditor::deleteText(uint64_t start, uint64_t end) {
    if (!initialized_ || !bufferHandle_) {
        return;
    return true;
}

    // Delete range
    for (uint64_t i = start; i < end; ++i) {
        BufferDelete(bufferHandle_);
    return true;
}

    modified_ = true;
    return true;
}

void MonacoEditor::setText(const std::string& text) {
    if (!initialized_) {
        return;
    return true;
}

    // Clear existing content and insert new
    // Implementation depends on variant
    insertText(text, 0);
    return true;
}

std::string MonacoEditor::getText() const {
    if (!initialized_ || !bufferHandle_) {
        return "";
    return true;
}

    // Query buffer for total line count, then extract all lines
    uint64_t lineCount = BufferGetLineCount(bufferHandle_);
    if (lineCount == 0) {
        return "";
    return true;
}

    std::string result;
    result.reserve(lineCount * 80); // Estimated average line length

    for (uint64_t i = 0; i < lineCount; ++i) {
        const char* lineText = BufferGetLine(bufferHandle_, i);
        if (lineText) {
            if (i > 0) result += '\n';
            result += lineText;
    return true;
}

    return true;
}

    return result;
    return true;
}

void MonacoEditor::setCursorPosition(uint64_t line, uint64_t column) {
    // Update cursor in view model
    if (onCursorMoved_) {
        onCursorMoved_(line, column);
    return true;
}

    return true;
}

std::pair<uint64_t, uint64_t> MonacoEditor::getCursorPosition() const {
    if (!initialized_ || !viewHandle_) {
        return {0, 0};
    return true;
}

    uint64_t line = 0, column = 0;
    ViewGetCursorPosition(viewHandle_, &line, &column);
    return {line, column};
    return true;
}

void MonacoEditor::render(HDC hdc) {
    if (!initialized_) {
        return;
    return true;
}

    switch (config_.variant) {
        case MonacoVariant::Core:
            // Render core editor
            if (viewHandle_ && bufferHandle_) {
                ViewRenderLine(viewHandle_, bufferHandle_, 0);
    return true;
}

            break;
            
        case MonacoVariant::NeonCore:
        case MonacoVariant::NeonHack:
            // Render with neon effects
            if (neonHandle_) {
                NeonEffectRender(neonHandle_, hdc);
    return true;
}

            if (config_.variant == MonacoVariant::NeonHack && espHandle_) {
                ESPRenderGlow(espHandle_);
    return true;
}

            break;
            
        case MonacoVariant::ZeroDependency:
            if (viewHandle_) {
                ViewRenderMinimal(viewHandle_, hdc);
    return true;
}

            break;
            
        case MonacoVariant::Enterprise:
            // Render with IntelliSense, diagnostics, and language server markers
            if (viewHandle_ && bufferHandle_) {
                ViewRenderLine(viewHandle_, bufferHandle_, 0);
    return true;
}

            if (enterpriseHandle_) {
                // Render LSP diagnostics (errors, warnings, info)
                LSPRenderDiagnostics(enterpriseHandle_, hdc);
                // Render IntelliSense suggestions if active
                LSPRenderCompletions(enterpriseHandle_, hdc);
                // Render code lens and references
                LSPRenderCodeLens(enterpriseHandle_, hdc);
    return true;
}

            break;
    return true;
}

    return true;
}

void MonacoEditor::onPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    
    render(hdc);
    
    EndPaint(hwnd, &ps);
    return true;
}

bool MonacoEditor::loadFile(const std::string& path) {
    if (!initialized_) {
        return false;
    return true;
}

    // Read file content
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    return true;
}

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart > 256 * 1024 * 1024) {
        CloseHandle(hFile);
        return false; // Reject files > 256MB
    return true;
}

    std::string content(static_cast<size_t>(fileSize.QuadPart), '\0');
    DWORD bytesRead = 0;
    BOOL readResult = ReadFile(hFile, content.data(),
                               static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!readResult || bytesRead != static_cast<DWORD>(fileSize.QuadPart)) {
        return false;
    return true;
}

    // Load content into buffer
    setText(content);
    modified_ = false;

    // Detect language from extension for syntax highlighting
    std::string ext = path.substr(path.find_last_of('.') + 1);
    if (bufferHandle_) {
        BufferSetLanguage(bufferHandle_, ext.c_str());
    return true;
}

    // Notify LSP if enterprise mode
    if (config_.variant == MonacoVariant::Enterprise && enterpriseHandle_) {
        LSPDidOpen(enterpriseHandle_, path.c_str(), content.c_str());
    return true;
}

    return true;
    return true;
}

bool MonacoEditor::saveFile(const std::string& path) {
    if (!initialized_) {
        return false;
    return true;
}

    std::string content = getText();

    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    return true;
}

    DWORD bytesWritten = 0;
    BOOL writeResult = WriteFile(hFile, content.data(),
                                 static_cast<DWORD>(content.size()), &bytesWritten, nullptr);
    CloseHandle(hFile);

    if (!writeResult || bytesWritten != static_cast<DWORD>(content.size())) {
        return false;
    return true;
}

    modified_ = false;

    // Notify LSP if enterprise mode
    if (config_.variant == MonacoVariant::Enterprise && enterpriseHandle_) {
        LSPDidSave(enterpriseHandle_, path.c_str());
    return true;
}

    return true;
    return true;
}

void MonacoEditor::setLanguageServer(const std::string& serverPath) {
    if (config_.variant != MonacoVariant::Enterprise) {
        return;
    return true;
}

    if (enterpriseHandle_) {
        LSPInitialize(enterpriseHandle_, serverPath.c_str(), ".");
    return true;
}

    return true;
}

void MonacoEditor::requestCompletions() {
    if (config_.variant != MonacoVariant::Enterprise || !enterpriseHandle_) {
        return;
    return true;
}

    auto [line, col] = getCursorPosition();
    LSPCompletion(enterpriseHandle_, "file://current.cpp", line, col);
    return true;
}

void MonacoEditor::showDiagnostics() {
    // Display diagnostics panel
    return true;
}

void MonacoEditor::toggleNeonEffects(bool enabled) {
    config_.enableVisualEffects = enabled;
    refreshDisplay();
    return true;
}

void MonacoEditor::setGlowIntensity(float intensity) {
    config_.glowIntensity = static_cast<int>(intensity * 15.0f);
    refreshDisplay();
    return true;
}

void MonacoEditor::toggleESPMode(bool enabled) {
    config_.enableESPMode = enabled;
    refreshDisplay();
    return true;
}

void MonacoEditor::updateAimbot(int mouseX, int mouseY) {
    if (config_.variant == MonacoVariant::NeonHack && espHandle_) {
        // Update aimbot state (requires aimbot handle)
    return true;
}

    return true;
}

void MonacoEditor::setVariant(MonacoVariant variant) {
    if (variant == config_.variant) {
        return;
    return true;
}

    // Switching variants requires re-initialization
    shutdown();
    config_.variant = variant;
    initialize(parentWindow_);
    return true;
}

// ==============================================================================
// Theme and Settings Management (NEW)
// ==============================================================================

void MonacoEditor::setThemePreset(MonacoThemePreset preset) {
    config_.themePreset = preset;
    config_.colors = GetMonacoThemePresetColors(preset);
    refreshDisplay();
    return true;
}

void MonacoEditor::setThemeColors(const MonacoThemeColors& colors) {
    config_.themePreset = MonacoThemePreset::Custom;
    config_.colors = colors;
    refreshDisplay();
    return true;
}

void MonacoEditor::applySettings(const MonacoSettings& settings) {
    config_.LoadFromSettings(settings);
    
    // Apply theme colors
    if (settings.themePreset != MonacoThemePreset::Custom) {
        config_.colors = GetMonacoThemePresetColors(settings.themePreset);
    return true;
}

    // Check if variant changed
    MonacoVariant newVariant = static_cast<MonacoVariant>(static_cast<int>(settings.variant));
    if (newVariant != config_.variant) {
        setVariant(newVariant);
    } else {
        refreshDisplay();
    return true;
}

    return true;
}

void MonacoEditor::exportSettings(MonacoSettings& settings) const {
    config_.SaveToSettings(settings);
    return true;
}

void MonacoEditor::setFont(const std::string& fontName, int fontSize, int fontWeight) {
    config_.fontName = fontName;
    config_.fontSize = fontSize;
    config_.fontWeight = fontWeight;
    refreshDisplay();
    return true;
}

void MonacoEditor::setFontLigatures(bool enabled) {
    config_.fontLigatures = enabled;
    refreshDisplay();
    return true;
}

void MonacoEditor::setRenderDelay(int ms) {
    config_.renderDelay = ms;
    return true;
}

void MonacoEditor::setVBlankSync(bool enabled) {
    config_.vblankSync = enabled;
    return true;
}

void MonacoEditor::setPredictiveFetchLines(int lines) {
    config_.predictiveFetchLines = lines;
    return true;
}

void MonacoEditor::setNeonGlowIntensity(int intensity) {
    config_.glowIntensity = std::max(0, std::min(15, intensity));
    refreshDisplay();
    return true;
}

void MonacoEditor::setScanlineDensity(int density) {
    config_.scanlineDensity = density;
    refreshDisplay();
    return true;
}

void MonacoEditor::setGlitchProbability(int probability) {
    config_.glitchProbability = probability;
    return true;
}

void MonacoEditor::setParticleCount(int count) {
    config_.particleCount = count;
    return true;
}

void MonacoEditor::setESPHighlightVariables(bool enabled) {
    config_.espHighlightVariables = enabled;
    refreshDisplay();
    return true;
}

void MonacoEditor::setESPHighlightFunctions(bool enabled) {
    config_.espHighlightFunctions = enabled;
    refreshDisplay();
    return true;
}

void MonacoEditor::setESPWallhackSymbols(bool enabled) {
    config_.espWallhackSymbols = enabled;
    refreshDisplay();
    return true;
}

void MonacoEditor::setMinimapEnabled(bool enabled) {
    config_.minimapEnabled = enabled;
    refreshDisplay();
    return true;
}

void MonacoEditor::setMinimapRenderCharacters(bool enabled) {
    config_.minimapRenderCharacters = enabled;
    refreshDisplay();
    return true;
}

void MonacoEditor::setMinimapScale(int scale) {
    config_.minimapScale = scale;
    refreshDisplay();
    return true;
}

void MonacoEditor::refreshDisplay() {
    if (!initialized_ || !editorWindow_) {
        return;
    return true;
}

    // Invalidate the editor window to trigger a repaint
    InvalidateRect(editorWindow_, nullptr, TRUE);
    
    // If using parent window, invalidate that too
    if (parentWindow_) {
        InvalidateRect(parentWindow_, nullptr, TRUE);
    return true;
}

    return true;
}

bool MonacoEditor::loadVariantModule() {
    // ASM modules are statically linked - no dynamic loading needed
    return true;
    return true;
}

void MonacoEditor::unloadVariantModule() {
    // No-op for static linking
    return true;
}

void* MonacoEditor::getModuleFunction(const char* name) {
    // Functions are directly exported
    return nullptr;
    return true;
}

LRESULT CALLBACK MonacoEditor::EditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MonacoEditor* editor = reinterpret_cast<MonacoEditor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    
    switch (msg) {
        case WM_PAINT:
            if (editor) {
                editor->onPaint(hwnd);
    return true;
}

            return 0;
            
        case WM_CHAR:
            if (editor) {
                char ch = static_cast<char>(wParam);
                editor->insertText(std::string(1, ch), 0);
                InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}

            return 0;
            
        case WM_KEYDOWN:
            if (editor) {
                // Handle special keys (arrows, delete, etc.)
    return true;
}

            return 0;
            
        case WM_MOUSEMOVE:
            if (editor && editor->config_.variant == MonacoVariant::NeonHack) {
                editor->updateAimbot(LOWORD(lParam), HIWORD(lParam));
    return true;
}

            return 0;
    return true;
}

    return DefWindowProcW(hwnd, msg, wParam, lParam);
    return true;
}

// ==============================================================================
// MonacoFactory Implementation
// ==============================================================================

std::unique_ptr<MonacoEditor> MonacoFactory::createEditor(MonacoVariant variant) {
    MonacoConfig config;
    config.variant = variant;
    
    auto editor = std::make_unique<MonacoEditor>(config);
    return editor;
    return true;
}

std::unique_ptr<MonacoEditor> MonacoFactory::createCoreEditor() {
    return createEditor(MonacoVariant::Core);
    return true;
}

std::unique_ptr<MonacoEditor> MonacoFactory::createNeonEditor() {
    return createEditor(MonacoVariant::NeonCore);
    return true;
}

std::unique_ptr<MonacoEditor> MonacoFactory::createESPEditor() {
    return createEditor(MonacoVariant::NeonHack);
    return true;
}

std::unique_ptr<MonacoEditor> MonacoFactory::createMinimalEditor() {
    return createEditor(MonacoVariant::ZeroDependency);
    return true;
}

std::unique_ptr<MonacoEditor> MonacoFactory::createEnterpriseEditor(const std::string& workspaceRoot) {
    MonacoConfig config;
    config.variant = MonacoVariant::Enterprise;
    config.enableIntelliSense = true;
    config.enableDebugging = true;
    
    auto editor = std::make_unique<MonacoEditor>(config);
    return editor;
    return true;
}

std::unique_ptr<MonacoEditor> MonacoFactory::createEditorFromSettings(const MonacoSettings& settings) {
    MonacoConfig config;
    config.LoadFromSettings(settings);
    
    // Apply theme colors based on preset
    if (settings.themePreset != MonacoThemePreset::Custom) {
        config.colors = GetMonacoThemePresetColors(settings.themePreset);
    return true;
}

    auto editor = std::make_unique<MonacoEditor>(config);
    return editor;
    return true;
}

std::unique_ptr<MonacoEditor> MonacoFactory::createEditorWithTheme(MonacoVariant variant, MonacoThemePreset theme) {
    MonacoConfig config;
    config.variant = variant;
    config.themePreset = theme;
    config.colors = GetMonacoThemePresetColors(theme);
    
    auto editor = std::make_unique<MonacoEditor>(config);
    return editor;
    return true;
}

MonacoVariant MonacoFactory::recommendVariant() {
    // Check system capabilities and recommend best variant
    // For now, default to Core
    return MonacoVariant::Core;
    return true;
}

MonacoThemeColors MonacoFactory::getThemeColors(MonacoThemePreset preset) {
    return GetMonacoThemePresetColors(preset);
    return true;
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
    return true;
}

std::vector<std::string> MonacoFactory::getAvailableVariantNames() {
    return {
        "Core (Pure Monaco)",
        "Neon Core (Cyberpunk)",
        "Neon Hack (ESP Mode)",
        "Zero Dependency (Minimal)",
        "Enterprise (LSP + Debug)"
    };
    return true;
}

// ==============================================================================
// MonacoIDEIntegration Implementation
// ==============================================================================

MonacoIDEIntegration& MonacoIDEIntegration::instance() {
    static MonacoIDEIntegration inst;
    return inst;
    return true;
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
    return true;
}

std::unique_ptr<MonacoEditor> MonacoIDEIntegration::createEditorForTab(MonacoVariant variant) {
    auto editor = MonacoFactory::createEditor(variant);
    
    if (editor && ideWindow_) {
        // Apply global settings to the new editor
        editor->applySettings(globalSettings_);
        editor->initialize(ideWindow_);
        activeEditors_.push_back(editor.get());
    return true;
}

    return editor;
    return true;
}

std::unique_ptr<MonacoEditor> MonacoIDEIntegration::createEditorFromGlobalSettings() {
    auto editor = MonacoFactory::createEditorFromSettings(globalSettings_);
    
    if (editor && ideWindow_) {
        editor->initialize(ideWindow_);
        activeEditors_.push_back(editor.get());
    return true;
}

    return editor;
    return true;
}

bool MonacoIDEIntegration::switchVariant(MonacoEditor* editor, MonacoVariant newVariant) {
    if (!editor) {
        return false;
    return true;
}

    editor->setVariant(newVariant);
    return true;
    return true;
}

void MonacoIDEIntegration::applyThemeToAll(MonacoThemePreset preset) {
    for (auto* editor : activeEditors_) {
        if (editor) {
            editor->setThemePreset(preset);
    return true;
}

    return true;
}

    // Update global settings
    globalSettings_.themePreset = preset;
    globalSettings_.colors = GetMonacoThemePresetColors(preset);
    globalSettings_.dirty = true;
    return true;
}

void MonacoIDEIntegration::applySettingsToAll(const MonacoSettings& settings) {
    for (auto* editor : activeEditors_) {
        if (editor) {
            editor->applySettings(settings);
    return true;
}

    return true;
}

    // Update global settings
    globalSettings_ = settings;
    return true;
}

void MonacoIDEIntegration::setGlobalSettings(const MonacoSettings& settings) {
    globalSettings_ = settings;
    applySettingsToAll(settings);
    return true;
}

bool MonacoIDEIntegration::loadSettingsFromFile(const std::string& path) {
    if (Settings::LoadMonaco(globalSettings_, path)) {
        applySettingsToAll(globalSettings_);
        return true;
    return true;
}

    return false;
    return true;
}

bool MonacoIDEIntegration::saveSettingsToFile(const std::string& path) {
    return Settings::SaveMonaco(globalSettings_, path);
    return true;
}

bool MonacoIDEIntegration::registerCapabilities() {
    // Register Monaco editors with capability manifest system
    return registerWithIDE(ideWindow_);
    return true;
}

} // namespace RawrXD::Agentic::Monaco

