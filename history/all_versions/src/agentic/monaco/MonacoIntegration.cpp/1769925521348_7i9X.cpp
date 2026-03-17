#include "MonacoIntegration.hpp"
#include "../bridge/Win32IDEBridge.hpp"
#include "../manifestor/SelfManifestor.hpp"
#include <stdexcept>
#include <fstream>
#include <sstream>

namespace RawrXD::Agentic::Monaco {

// Helper to shadow content since buffer handle API is opaque
static std::string g_ShadowContent;

// ==============================================================================
// MonacoEditor Implementation
// ==============================================================================

MonacoEditor::MonacoEditor(MonacoConfig config)
    : config_(config) {
}

MonacoEditor::~MonacoEditor() {
    shutdown();
}

bool MonacoEditor::initialize(HWND parentWindow) {
    if (initialized_) {
        return true;
    }
    
    parentWindow_ = parentWindow;
    
    // Load appropriate ASM module based on variant
    if (!loadVariantModule()) {
        return false;
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
            if (enterpriseHandle_) {
                 RECT rect;
                 GetClientRect(parentWindow_, &rect);
                 SetTextColor(hdc, RGB(20, 20, 20));
                 SetBkMode(hdc, TRANSPARENT);
                 DrawTextA(hdc, "Enterprise Editor Mode [Active]", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
