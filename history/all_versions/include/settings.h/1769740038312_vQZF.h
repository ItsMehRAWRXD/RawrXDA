#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

// Monaco Editor Variant Types
enum class MonacoVariantType {
    Core = 0,
    NeonCore = 1,
    NeonHack = 2,
    ZeroDependency = 3,
    Enterprise = 4
};

// Monaco Editor Theme Presets
enum class MonacoThemePreset {
    Default = 0,
    NeonCyberpunk = 1,
    MatrixGreen = 2,
    HackerRed = 3,
    Monokai = 4,
    SolarizedDark = 5,
    SolarizedLight = 6,
    OneDark = 7,
    Dracula = 8,
    GruvboxDark = 9,
    Nord = 10,
    Custom = 255
};

// Monaco Theme Color Settings
struct MonacoThemeColors {
    uint32_t background = 0x001E1E1E;
    uint32_t foreground = 0x00D4D4D4;
    uint32_t lineNumber = 0x00858585;
    uint32_t cursorColor = 0x00AEAFAD;
    uint32_t selectionBg = 0x00264F78;
    uint32_t lineHighlight = 0x00282828;
    uint32_t keyword = 0x00569CD6;
    uint32_t string = 0x00CE9178;
    uint32_t number = 0x00B5CEA8;
    uint32_t comment = 0x006A9955;
    uint32_t function = 0x00DCDCAA;
    uint32_t type = 0x004EC9B0;
    uint32_t variable = 0x009CDCFE;
    uint32_t operator_ = 0x00D4D4D4;
    uint32_t preprocessor = 0x00C586C0;
    uint32_t constant = 0x004FC1FF;
    uint32_t glowColor = 0x0000FFFF;
    uint32_t glowSecondary = 0x00FF00FF;
    uint32_t scanlineColor = 0x00101010;
    uint32_t particleColor = 0x0000FF66;
};

struct MonacoSettings {
    MonacoVariantType variant = MonacoVariantType::Core;
    MonacoThemePreset themePreset = MonacoThemePreset::Default;
    MonacoThemeColors colors;
    std::string fontFamily = "Consolas";
    int fontSize = 14;
    int lineHeight = 20;
    bool fontLigatures = true;
    int fontWeight = 400;
    bool wordWrap = false;
    int tabSize = 4;
    bool insertSpaces = true;
    bool autoIndent = true;
    bool bracketMatching = true;
    bool autoClosingBrackets = true;
    bool autoClosingQuotes = true;
    bool formatOnPaste = false;
    bool formatOnType = false;
    bool enableIntelliSense = true;
    bool quickSuggestions = true;
    int suggestDelay = 100;
    bool parameterHints = true;
    bool enableNeonEffects = true;
    int glowIntensity = 8;
    int scanlineDensity = 2;
    int glitchProbability = 3;
    bool particlesEnabled = true;
    int particleCount = 1024;
    bool enableESPMode = false;
    bool espHighlightVariables = true;
    bool espHighlightFunctions = true;
    bool espWallhackSymbols = false;
    bool minimapEnabled = true;
    bool minimapRenderCharacters = true;
    int minimapScale = 1;
    bool minimapShowSlider = true;
    bool enableDebugging = false;
    bool inlineDebugging = true;
    bool breakpointGutter = true;
    int renderDelay = 16;
    bool vblankSync = true;
    int predictiveFetchLines = 16;
    bool lazyTokenization = true;
    int lazyTokenizationDelay = 50;
    bool dirty = false;
};

// App state structure for settings
struct AppState {
    MonacoSettings monaco;
    bool enable_gpu_matmul = true;
    bool enable_gpu_attention = true;
    bool enable_cpu_gpu_compare = false;
    bool enable_detailed_quant = false;
    bool compute_settings_dirty = false;
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
    std::string governor_status = "stopped";
    int32_t applied_core_offset_mhz = 0;
    int32_t applied_gpu_offset_mhz = 0;
    uint32_t current_cpu_temp_c = 0;
    uint32_t current_gpu_hotspot_c = 0;
    bool running = false;
};

class SettingsValue {
public:
    SettingsValue() = default;
    explicit SettingsValue(std::string value);
    explicit SettingsValue(const char* value);
    explicit SettingsValue(long long value);
    explicit SettingsValue(double value);
    explicit SettingsValue(bool value);
    explicit SettingsValue(const std::chrono::system_clock::time_point& value);
    explicit SettingsValue(const std::vector<uint8_t>& bytes);

    std::string toString() const;
    long long toLongLong() const;
    double toDouble() const;
    bool toBool() const;
    std::chrono::system_clock::time_point toDateTime() const;
    std::vector<uint8_t> toByteArray() const;
    bool empty() const;

private:
    std::string data_;
    bool binary_ = false;
};

class Settings {
public:
    Settings();
    explicit Settings(std::filesystem::path storagePath);
    ~Settings() = default;

    void initialize();
    void sync();

    void beginGroup(const std::string& group);
    void endGroup();
    void beginWriteArray(const std::string& arrayName);
    void beginReadArray(const std::string& arrayName);
    void endArray();
    void setArrayIndex(std::size_t index);
    std::size_t arraySize() const;

    void setValue(const std::string& key, const SettingsValue& value);
    SettingsValue value(const std::string& key, const SettingsValue& defaultValue = {}) const;
    bool contains(const std::string& key) const;
    void remove(const std::string& key);

    static bool LoadCompute(AppState& state, const std::string& path = "compute.ini");
    static bool SaveCompute(const AppState& state, const std::string& path = "compute.ini");
    static bool LoadOverclock(AppState& state, const std::string& path = "overclock.ini");
    static bool SaveOverclock(const AppState& state, const std::string& path = "overclock.ini");
    static bool LoadMonaco(MonacoSettings& settings, const std::string& path);
    static bool SaveMonaco(const MonacoSettings& settings, const std::string& path);
    static MonacoThemeColors GetThemePresetColors(MonacoThemePreset preset);

private:
    std::string currentPrefix(bool includeArrayIndex = true) const;
    std::string groupPrefix() const;
    void load();
    void save() const;
    static std::filesystem::path DefaultSettingsPath();
    static void EnsureDirectory(const std::filesystem::path& path);

    std::string composeKeyLocked(const std::string& key, bool includeArrayIndex) const;
    std::string composeGroupPrefixLocked() const;
    std::string composeArraySizeKeyLocked() const;

    mutable std::shared_mutex mutex_;
    std::map<std::string, SettingsValue> data_;
    std::filesystem::path storagePath_;
    std::vector<std::string> groupStack_;
    std::string arrayName_;
    std::size_t arrayIndex_ = 0;
    std::size_t arraySize_ = 0;
    bool inReadArray_ = false;
};
