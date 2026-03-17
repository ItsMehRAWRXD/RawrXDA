#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../../qtapp/settings.h"  // For MonacoSettings, MonacoThemeColors, etc.

namespace RawrXD::Agentic::Monaco {

// Monaco Editor Variant Types (matches MonacoVariantType in settings.h)
enum class MonacoVariant {
    Core = 0,              // MONACO_EDITOR_CORE.ASM - Pure piece tree
    NeonCore = 1,          // NEON_MONACO_CORE.ASM - Cyberpunk visuals
    NeonHack = 2,          // NEON_MONACO_HACK.ASM - ESP wallhack style
    ZeroDependency = 3,    // MONACO_EDITOR_ZERO_DEPENDENCY.ASM - Minimal
    Enterprise = 4         // MONACO_EDITOR_ENTERPRISE.ASM - LSP + debugging
};

// Monaco Editor Configuration - Now integrated with global settings
struct MonacoConfig {
    // Core settings from global MonacoSettings
    MonacoVariant variant = MonacoVariant::Core;
    MonacoThemePreset themePreset = MonacoThemePreset::Default;
    MonacoThemeColors colors;  // Active theme colors
    
    // Font settings
    std::string fontName = "Consolas";
    int fontSize = 14;
    int lineHeight = 0;       // 0 = auto
    bool fontLigatures = true;
    int fontWeight = 400;
    
    // Legacy compatibility flags
    bool enableSyntaxHighlighting = true;
    bool enableIntelliSense = false;        // Enterprise only
    bool enableDebugging = false;           // Enterprise only
    bool enableVisualEffects = false;       // Neon variants
    bool enableESPMode = false;             // NeonHack only
    
    // Neon effect settings
    int glowIntensity = 8;
    int scanlineDensity = 2;
    int glitchProbability = 3;
    bool particlesEnabled = true;
    int particleCount = 1024;
    
    // ESP mode settings
    bool espHighlightVariables = true;
    bool espHighlightFunctions = true;
    bool espWallhackSymbols = false;
    
    // Minimap settings
    bool minimapEnabled = true;
    bool minimapRenderCharacters = true;
    int minimapScale = 1;
    
    // Performance settings
    int renderDelay = 16;
    bool vblankSync = true;
    int predictiveFetchLines = 16;
    
    // Initialize from global settings
    void LoadFromSettings(const MonacoSettings& settings) {
        variant = static_cast<MonacoVariant>(static_cast<int>(settings.variant));
        themePreset = settings.themePreset;
        colors = settings.colors;
        
        fontName = settings.fontFamily;
        fontSize = settings.fontSize;
        lineHeight = settings.lineHeight;
        fontLigatures = settings.fontLigatures;
        fontWeight = settings.fontWeight;
        
        enableIntelliSense = settings.enableIntelliSense;
        enableDebugging = settings.enableDebugging;
        enableVisualEffects = settings.enableNeonEffects;
        enableESPMode = settings.enableESPMode;
        
        glowIntensity = settings.glowIntensity;
        scanlineDensity = settings.scanlineDensity;
        glitchProbability = settings.glitchProbability;
        particlesEnabled = settings.particlesEnabled;
        particleCount = settings.particleCount;
        
        espHighlightVariables = settings.espHighlightVariables;
        espHighlightFunctions = settings.espHighlightFunctions;
        espWallhackSymbols = settings.espWallhackSymbols;
        
        minimapEnabled = settings.minimapEnabled;
        minimapRenderCharacters = settings.minimapRenderCharacters;
        minimapScale = settings.minimapScale;
        
        renderDelay = settings.renderDelay;
        vblankSync = settings.vblankSync;
        predictiveFetchLines = settings.predictiveFetchLines;
    }
    
    // Export to global settings
    void SaveToSettings(MonacoSettings& settings) const {
        settings.variant = static_cast<MonacoVariantType>(static_cast<int>(variant));
        settings.themePreset = themePreset;
        settings.colors = colors;
        
        settings.fontFamily = fontName;
        settings.fontSize = fontSize;
        settings.lineHeight = lineHeight;
        settings.fontLigatures = fontLigatures;
        settings.fontWeight = fontWeight;
        
        settings.enableIntelliSense = enableIntelliSense;
        settings.enableDebugging = enableDebugging;
        settings.enableNeonEffects = enableVisualEffects;
        settings.enableESPMode = enableESPMode;
        
        settings.glowIntensity = glowIntensity;
        settings.scanlineDensity = scanlineDensity;
        settings.glitchProbability = glitchProbability;
        settings.particlesEnabled = particlesEnabled;
        settings.particleCount = particleCount;
        
        settings.espHighlightVariables = espHighlightVariables;
        settings.espHighlightFunctions = espHighlightFunctions;
        settings.espWallhackSymbols = espWallhackSymbols;
        
        settings.minimapEnabled = minimapEnabled;
        settings.minimapRenderCharacters = minimapRenderCharacters;
        settings.minimapScale = minimapScale;
        
        settings.renderDelay = renderDelay;
        settings.vblankSync = vblankSync;
        settings.predictiveFetchLines = predictiveFetchLines;
        
        settings.dirty = true;
    }
    
    // Helper to get background color
    uint32_t getBackgroundColor() const { return colors.background; }
    uint32_t getForegroundColor() const { return colors.foreground; }
    uint32_t getGlowColor() const { return colors.glowColor; }
    uint32_t getGlowSecondary() const { return colors.glowSecondary; }
};

// Forward declarations for ASM exports
extern "C" {
    // Core editor functions (exported from MONACO_EDITOR_CORE.ASM)
    void* __stdcall BufferCreate();
    void __stdcall BufferInsert(void* buffer, char ch);
    void __stdcall BufferDelete(void* buffer);
    void __stdcall ViewRenderLine(void* view, void* buffer, uint64_t lineNum);
    void __stdcall ApplyEdit(void* buffer, const char* text, uint64_t pos);
    void __stdcall Undo(void* buffer);
    
    // Neon effects (exported from NEON_MONACO_CORE.ASM)
    void* __stdcall NeonEffectCreate();
    void __stdcall NeonEffectRender(void* effect, HDC hdc);
    
    // ESP mode (exported from NEON_MONACO_HACK.ASM)
    void* __stdcall ESPInit();
    void __stdcall ESPRenderGlow(void* espState);
    void __stdcall AimbotUpdate(void* aimbot, int mouseX, int mouseY);
    
    // Zero dependency (exported from MONACO_EDITOR_ZERO_DEPENDENCY.ASM)
    void* __stdcall BufferCreateMinimal();
    void __stdcall ViewRenderMinimal(void* view, HDC hdc);
    
    // Enterprise features (exported from MONACO_EDITOR_ENTERPRISE.ASM)
    void* __stdcall EnterpriseEditorCreate(const char* workspaceRoot);
    void* __stdcall LSPInitialize(void* client, const char* serverPath, const char* workspace);
    void* __stdcall LSPCompletion(void* client, const char* uri, uint64_t line, uint64_t col);
    void __stdcall IntelliSenseRenderPopup(void* intellisense, HDC hdc);
    void __stdcall DiagnosticsRenderSquiggles(void* diagnostics, HDC hdc, void* buffer);
}

// Monaco Editor Wrapper Class
class MonacoEditor {
public:
    MonacoEditor(MonacoConfig config = {});
    ~MonacoEditor();
    
    // Initialization
    bool initialize(HWND parentWindow);
    void shutdown();
    
    // Text manipulation
    void insertText(const std::string& text, uint64_t position);
    void deleteText(uint64_t start, uint64_t end);
    void setText(const std::string& text);
    std::string getText() const;
    
    // Cursor management
    void setCursorPosition(uint64_t line, uint64_t column);
    std::pair<uint64_t, uint64_t> getCursorPosition() const;
    
    // Rendering
    void render(HDC hdc);
    void onPaint(HWND hwnd);
    
    // File operations
    bool loadFile(const std::string& path);
    bool saveFile(const std::string& path);
    
    // Language Server Protocol (Enterprise variant only)
    void setLanguageServer(const std::string& serverPath);
    void requestCompletions();
    void showDiagnostics();
    
    // Visual effects (Neon variants only)
    void toggleNeonEffects(bool enabled);
    void setGlowIntensity(float intensity);
    
    // ESP mode (NeonHack variant only)
    void toggleESPMode(bool enabled);
    void updateAimbot(int mouseX, int mouseY);
    
    // Configuration
    void setVariant(MonacoVariant variant);
    MonacoVariant getVariant() const { return config_.variant; }
    const MonacoConfig& getConfig() const { return config_; }
    
    // Theme and settings management (NEW)
    void setThemePreset(MonacoThemePreset preset);
    MonacoThemePreset getThemePreset() const { return config_.themePreset; }
    void setThemeColors(const MonacoThemeColors& colors);
    const MonacoThemeColors& getThemeColors() const { return config_.colors; }
    void applySettings(const MonacoSettings& settings);
    void exportSettings(MonacoSettings& settings) const;
    
    // Font management (NEW)
    void setFont(const std::string& fontName, int fontSize, int fontWeight = 400);
    void setFontLigatures(bool enabled);
    
    // Performance settings (NEW)
    void setRenderDelay(int ms);
    void setVBlankSync(bool enabled);
    void setPredictiveFetchLines(int lines);
    
    // Neon effect settings (NEW)
    void setNeonGlowIntensity(int intensity);  // 0-15
    void setScanlineDensity(int density);
    void setGlitchProbability(int probability);
    void setParticleCount(int count);
    
    // ESP mode settings (NEW)
    void setESPHighlightVariables(bool enabled);
    void setESPHighlightFunctions(bool enabled);
    void setESPWallhackSymbols(bool enabled);
    
    // Minimap settings (NEW)
    void setMinimapEnabled(bool enabled);
    void setMinimapRenderCharacters(bool enabled);
    void setMinimapScale(int scale);
    
    // Refresh after settings change (NEW)
    void refreshDisplay();
    
    // Event handlers
    using OnTextChanged = std::function<void(const std::string&)>;
    using OnCursorMoved = std::function<void(uint64_t line, uint64_t col)>;
    
    void setOnTextChanged(OnTextChanged callback) { onTextChanged_ = callback; }
    void setOnCursorMoved(OnCursorMoved callback) { onCursorMoved_ = callback; }
    
    // Status
    bool isInitialized() const { return initialized_; }
    bool isModified() const { return modified_; }
    
private:
    MonacoConfig config_;
    bool initialized_ = false;
    bool modified_ = false;
    
    HWND parentWindow_ = nullptr;
    HWND editorWindow_ = nullptr;
    
    // ASM module handles
    void* bufferHandle_ = nullptr;
    void* viewHandle_ = nullptr;
    void* neonHandle_ = nullptr;
    void* espHandle_ = nullptr;
    void* enterpriseHandle_ = nullptr;
    
    // Event callbacks
    OnTextChanged onTextChanged_;
    OnCursorMoved onCursorMoved_;
    
    // Internal methods
    bool loadVariantModule();
    void unloadVariantModule();
    void* getModuleFunction(const char* name);
    
    // Window procedure
    static LRESULT CALLBACK EditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Factory for creating Monaco editors
class MonacoFactory {
public:
    static std::unique_ptr<MonacoEditor> createEditor(MonacoVariant variant);
    static std::unique_ptr<MonacoEditor> createCoreEditor();
    static std::unique_ptr<MonacoEditor> createNeonEditor();
    static std::unique_ptr<MonacoEditor> createESPEditor();
    static std::unique_ptr<MonacoEditor> createMinimalEditor();
    static std::unique_ptr<MonacoEditor> createEnterpriseEditor(const std::string& workspaceRoot);
    
    // Create editor from settings (NEW)
    static std::unique_ptr<MonacoEditor> createEditorFromSettings(const MonacoSettings& settings);
    static std::unique_ptr<MonacoEditor> createEditorWithTheme(MonacoVariant variant, MonacoThemePreset theme);
    
    // Get recommended variant based on system capabilities
    static MonacoVariant recommendVariant();
    
    // Theme utilities (NEW)
    static MonacoThemeColors getThemeColors(MonacoThemePreset preset);
    static std::vector<std::string> getAvailableThemeNames();
    static std::vector<std::string> getAvailableVariantNames();
};

// Monaco Integration with Win32IDE
class MonacoIDEIntegration {
public:
    static MonacoIDEIntegration& instance();
    
    // Register Monaco editors with IDE
    bool registerWithIDE(HWND ideWindow);
    
    // Create editor instance for IDE tab
    std::unique_ptr<MonacoEditor> createEditorForTab(MonacoVariant variant);
    
    // Create editor from global settings (NEW)
    std::unique_ptr<MonacoEditor> createEditorFromGlobalSettings();
    
    // Switch between Monaco variants at runtime
    bool switchVariant(MonacoEditor* editor, MonacoVariant newVariant);
    
    // Apply theme to all active editors (NEW)
    void applyThemeToAll(MonacoThemePreset preset);
    void applySettingsToAll(const MonacoSettings& settings);
    
    // Get/Set global Monaco settings (NEW)
    const MonacoSettings& getGlobalSettings() const { return globalSettings_; }
    void setGlobalSettings(const MonacoSettings& settings);
    bool loadSettingsFromFile(const std::string& path);
    bool saveSettingsToFile(const std::string& path);
    
    // Register with capability manifest system
    bool registerCapabilities();
    
private:
    MonacoIDEIntegration() = default;
    HWND ideWindow_ = nullptr;
    MonacoSettings globalSettings_;  // Global settings instance
    std::vector<MonacoEditor*> activeEditors_;
};

} // namespace RawrXD::Agentic::Monaco
