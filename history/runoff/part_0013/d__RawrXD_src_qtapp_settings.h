#pragma once

#include <QString>
#include <QVariant>
#include <QSettings>
#include <string>
#include <cstdint>
#include <vector>

// Monaco Editor Variant Types
enum class MonacoVariantType {
    Core = 0,              // MONACO_EDITOR_CORE.ASM - Pure piece tree
    NeonCore = 1,          // NEON_MONACO_CORE.ASM - Cyberpunk visuals
    NeonHack = 2,          // NEON_MONACO_HACK.ASM - ESP wallhack style
    ZeroDependency = 3,    // MONACO_EDITOR_ZERO_DEPENDENCY.ASM - Minimal
    Enterprise = 4         // MONACO_EDITOR_ENTERPRISE.ASM - LSP + debugging
};

// Monaco Editor Theme Presets
enum class MonacoThemePreset {
    Default = 0,           // VS Code Dark+
    NeonCyberpunk = 1,     // Cyan/Magenta glow theme
    MatrixGreen = 2,       // Matrix-style green on black
    HackerRed = 3,         // Red/Orange hacker aesthetic
    Monokai = 4,           // Classic Monokai colors
    SolarizedDark = 5,     // Solarized Dark
    SolarizedLight = 6,    // Solarized Light
    OneDark = 7,           // Atom One Dark
    Dracula = 8,           // Dracula theme
    GruvboxDark = 9,       // Gruvbox Dark
    Nord = 10,             // Nord theme
    Custom = 255           // User-defined custom colors
};

// Monaco Theme Color Settings
struct MonacoThemeColors {
    uint32_t background = 0x001E1E1E;      // Editor background
    uint32_t foreground = 0x00D4D4D4;      // Default text color
    uint32_t lineNumber = 0x00858585;      // Line number color
    uint32_t cursorColor = 0x00AEAFAD;     // Cursor color
    uint32_t selectionBg = 0x00264F78;     // Selection background
    uint32_t lineHighlight = 0x00282828;   // Current line highlight
    uint32_t findMatchBg = 0x00515C6A;     // Find match background
    
    // Syntax highlighting colors
    uint32_t keyword = 0x00569CD6;         // Keywords (if, for, etc.)
    uint32_t string = 0x00CE9178;          // String literals
    uint32_t number = 0x00B5CEA8;          // Numeric literals
    uint32_t comment = 0x006A9955;         // Comments
    uint32_t function = 0x00DCDCAA;        // Function names
    uint32_t type = 0x004EC9B0;            // Type names/classes
    uint32_t variable = 0x009CDCFE;        // Variables
    uint32_t operator_ = 0x00D4D4D4;       // Operators
    uint32_t preprocessor = 0x00C586C0;    // Preprocessor directives
    uint32_t constant = 0x004FC1FF;        // Constants
    
    // Neon-specific colors (for Neon variants)
    uint32_t glowColor = 0x0000FFFF;       // Neon glow primary
    uint32_t glowSecondary = 0x00FF00FF;   // Neon glow secondary
    uint32_t scanlineColor = 0x00202020;   // CRT scanline effect
    uint32_t particleColor = 0x0000FF00;   // Background particle color
};

// Monaco Editor Settings
struct MonacoSettings {
    // Variant selection
    MonacoVariantType variant = MonacoVariantType::Core;
    
    // Theme settings
    MonacoThemePreset themePreset = MonacoThemePreset::Default;
    MonacoThemeColors colors;  // Custom colors (used when themePreset == Custom)
    
    // Font settings
    std::string fontFamily = "Consolas";
    int fontSize = 14;
    int lineHeight = 20;      // 0 = auto (1.4x font size)
    bool fontLigatures = true;
    int fontWeight = 400;     // 100-900, 400=normal, 700=bold
    
    // Editor behavior
    bool wordWrap = false;
    int tabSize = 4;
    bool insertSpaces = true;
    bool autoIndent = true;
    bool bracketMatching = true;
    bool autoClosingBrackets = true;
    bool autoClosingQuotes = true;
    bool formatOnPaste = false;
    bool formatOnType = false;
    
    // IntelliSense (Enterprise only)
    bool enableIntelliSense = true;
    bool quickSuggestions = true;
    int suggestDelay = 100;   // ms
    bool parameterHints = true;
    
    // Visual effects (Neon variants)
    bool enableNeonEffects = true;
    int glowIntensity = 8;        // 0-15
    int scanlineDensity = 2;      // CRT effect
    int glitchProbability = 3;    // 1/256 chance per frame
    bool particlesEnabled = true;
    int particleCount = 1024;
    
    // ESP mode (NeonHack only)
    bool enableESPMode = false;
    bool espHighlightVariables = true;
    bool espHighlightFunctions = true;
    bool espWallhackSymbols = false;
    
    // Minimap
    bool minimapEnabled = true;
    bool minimapRenderCharacters = true;
    int minimapScale = 1;
    bool minimapShowSlider = true;
    
    // Debugging (Enterprise only)
    bool enableDebugging = false;
    bool inlineDebugging = true;
    bool breakpointGutter = true;
    
    // Performance
    int renderDelay = 16;         // ms (60fps default)
    bool vblankSync = true;
    int predictiveFetchLines = 16;
    bool lazyTokenization = true;
    int lazyTokenizationDelay = 50; // ms
    
    bool dirty = false;
};

// App state structure for settings
struct AppState {
    // Monaco Editor settings
    MonacoSettings monaco;
    
    // Compute settings
    bool enable_gpu_matmul = true;
    bool enable_gpu_attention = true;
    bool enable_cpu_gpu_compare = false;
    bool enable_detailed_quant = false;
    bool compute_settings_dirty = false;
    
    // Overclock settings
    bool enable_overclock_governor = true;
    uint32_t target_all_core_mhz = 3600;
    uint32_t boost_step_mhz = 100;
    uint32_t max_cpu_temp_c = 85;
    uint32_t max_gpu_hotspot_c = 90;
    float max_core_voltage = 1.4f;
    float pid_kp = 0.1f;
    float pid_ki = 0.01f;
    float pid_kd = 0.05f;
    float pid_integral_clamp = 500.0f;
    float gpu_pid_kp = 0.1f;
    float gpu_pid_ki = 0.01f;
    float gpu_pid_kd = 0.05f;
    float gpu_pid_integral_clamp = 500.0f;
    bool overclock_settings_dirty = false;
    
    // Runtime state (added for IDE main window compatibility)
    std::string governor_status = "stopped";
    int32_t applied_core_offset_mhz = 0;
    int32_t applied_gpu_offset_mhz = 0;
    uint32_t current_cpu_temp_c = 0;
    uint32_t current_gpu_hotspot_c = 0;
};

class Settings {
public:
    Settings();
    ~Settings();
    
    // Two-phase initialization: call after QApplication exists
    void initialize();
    
    // Qt-based settings (for GUI)
    void setValue(const QString& key, const QVariant& value);
    QVariant getValue(const QString& key, const QVariant& default_value = QVariant());
    
    // File-based settings (for compute/overclock/monaco)
    static bool LoadCompute(AppState& state, const std::string& path);
    static bool SaveCompute(const AppState& state, const std::string& path);
    static bool LoadOverclock(AppState& state, const std::string& path);
    static bool SaveOverclock(const AppState& state, const std::string& path);
    static bool LoadMonaco(MonacoSettings& settings, const std::string& path);
    static bool SaveMonaco(const MonacoSettings& settings, const std::string& path);
    
    // Theme preset loader
    static MonacoThemeColors GetThemePresetColors(MonacoThemePreset preset);
    
private:
    QSettings* settings_;
};
