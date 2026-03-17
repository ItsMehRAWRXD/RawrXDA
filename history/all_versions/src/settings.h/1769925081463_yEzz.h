#pragma once

#include "gui.h"
#include <string>
#include <any>
#include <memory>
#include <vector>

// ============================================================================
// Monaco Settings Definitions
// ============================================================================

enum class MonacoVariantType {
    Core = 0,
    NeonCore = 1,
    NeonHack = 2,
    ZeroDependency = 3,
    Enterprise = 4
};

enum class MonacoThemePreset {
    Default,
    Dark,
    Light,
    Cyberpunk,
    Hacker
};

struct MonacoThemeColors {
    unsigned int background = 0xFF1E1E1E;
    unsigned int foreground = 0xFFD4D4D4;
    unsigned int selection = 0xFF264F78;
    unsigned int lineHighlight = 0xFF2D2D30;
    unsigned int glowColor = 0xFF00FFFF;  // Default cyan glow
    unsigned int glowSecondary = 0xFFFF00FF; // Default magenta secondary
};

struct MonacoSettings {
    MonacoVariantType variant = MonacoVariantType::Core;
    MonacoThemePreset themePreset = MonacoThemePreset::Default;
    MonacoThemeColors colors;
    
    std::string fontFamily = "Consolas";
    int fontSize = 14;
    int lineHeight = 0;       // 0 = auto
    bool fontLigatures = true;
    int fontWeight = 400;
    
    bool enableIntelliSense = false;
    bool enableDebugging = false;
    bool enableNeonEffects = false;
    bool enableESPMode = false;
    
    int glowIntensity = 8;
    int scanlineDensity = 2;
    int glitchProbability = 3;
    bool particlesEnabled = true;
    int particleCount = 1024;
    
    bool espHighlightVariables = true;
    bool espHighlightFunctions = true;
    bool espWallhackSymbols = false;
    
    bool minimapEnabled = true;
    bool minimapRenderCharacters = true;
    int minimapScale = 1;
    
    int renderDelay = 16;
    bool vblankSync = true;
    int predictiveFetchLines = 16;
    bool dirty = false;
};

// ============================================================================
// Main Settings Class
// ============================================================================

class Settings {
public:
    Settings();
    ~Settings();

    void initialize();
    
    // Generic Key-Value Store (Abstracting backend)
    void setValue(const std::string& key, const std::any& value);
    std::any getValue(const std::string& key, const std::any& default_value);
    
    // Specific Persistence Helpers
    static bool LoadCompute(AppState& state, const std::string& path = "settings_compute.ini");
    static bool SaveCompute(const AppState& state, const std::string& path = "settings_compute.ini");
    
    static bool LoadOverclock(AppState& state, const std::string& path = "settings_overclock.ini");
    static bool SaveOverclock(const AppState& state, const std::string& path = "settings_overclock.ini");

    // Accessors
    static MonacoSettings getMonacoSettings() { return MonacoSettings(); } // Stub
    static MonacoThemeColors GetThemePresetColors(MonacoThemePreset preset);

private:
    class Impl;
    Impl* settings_; // Pimpl to hide backend (was void* in broken code)
};
