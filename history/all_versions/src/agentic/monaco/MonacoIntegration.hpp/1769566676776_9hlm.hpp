#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace RawrXD::Agentic::Monaco {

// Monaco Editor Variant Types
enum class MonacoVariant {
    Core,                   // MONACO_EDITOR_CORE.ASM - Pure piece tree
    NeonCore,              // NEON_MONACO_CORE.ASM - Cyberpunk visuals
    NeonHack,              // NEON_MONACO_HACK.ASM - ESP wallhack style
    ZeroDependency,        // MONACO_EDITOR_ZERO_DEPENDENCY.ASM - Minimal
    Enterprise             // MONACO_EDITOR_ENTERPRISE.ASM - LSP + debugging
};

// Monaco Editor Configuration
struct MonacoConfig {
    MonacoVariant variant = MonacoVariant::Core;
    bool enableSyntaxHighlighting = true;
    bool enableIntelliSense = false;        // Enterprise only
    bool enableDebugging = false;           // Enterprise only
    bool enableVisualEffects = false;       // Neon variants
    bool enableESPMode = false;             // NeonHack only
    std::string fontName = "Consolas";
    int fontSize = 14;
    uint32_t backgroundColor = 0x002B2B2B;
    uint32_t textColor = 0x00D4D4D4;
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
    
    // Get recommended variant based on system capabilities
    static MonacoVariant recommendVariant();
};

// Monaco Integration with Win32IDE
class MonacoIDEIntegration {
public:
    static MonacoIDEIntegration& instance();
    
    // Register Monaco editors with IDE
    bool registerWithIDE(HWND ideWindow);
    
    // Create editor instance for IDE tab
    std::unique_ptr<MonacoEditor> createEditorForTab(MonacoVariant variant);
    
    // Switch between Monaco variants at runtime
    bool switchVariant(MonacoEditor* editor, MonacoVariant newVariant);
    
    // Register with capability manifest system
    bool registerCapabilities();
    
private:
    MonacoIDEIntegration() = default;
    HWND ideWindow_ = nullptr;
    std::vector<MonacoEditor*> activeEditors_;
};

} // namespace RawrXD::Agentic::Monaco
