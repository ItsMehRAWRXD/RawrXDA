// MASM/Qt Toggle Settings - Universal Format Loader Configuration
// Location: src/settings/universal_format_loader_settings.h
// Purpose: Runtime toggle between MASM and C++/Qt implementations

#pragma once

#include <QString>
#include <QSettings>
#include <memory>

// ========================================================================================================
// IMPLEMENTATION MODE ENUMERATION
// ========================================================================================================

enum class UniversalLoaderImplementation {
    PURE_MASM,              // Pure x64 MASM assembly implementation
    CPLUSPLUS_QT,           // Original C++/Qt implementation
    AUTO_SELECT             // Automatically select based on availability
};

enum class FormatRouterImplementation {
    PURE_MASM,
    CPLUSPLUS_QT,
    AUTO_SELECT
};

enum class EnhancedLoaderImplementation {
    PURE_MASM,
    CPLUSPLUS_QT,
    AUTO_SELECT
};

// ========================================================================================================
// SETTINGS MANAGER FOR UNIVERSAL FORMAT LOADER
// ========================================================================================================

class UniversalFormatLoaderSettings {
public:
    static UniversalFormatLoaderSettings& instance() {
        static UniversalFormatLoaderSettings s_instance;
        return s_instance;
    }
    
    UniversalFormatLoaderSettings(const UniversalFormatLoaderSettings&) = delete;
    UniversalFormatLoaderSettings& operator=(const UniversalFormatLoaderSettings&) = delete;
    
    // Getters - Current implementation mode
    UniversalLoaderImplementation getUniversalLoaderImpl() const {
        return m_universalLoaderImpl;
    }
    
    FormatRouterImplementation getFormatRouterImpl() const {
        return m_formatRouterImpl;
    }
    
    EnhancedLoaderImplementation getEnhancedLoaderImpl() const {
        return m_enhancedLoaderImpl;
    }
    
    // Setters - Change implementation mode at runtime
    void setUniversalLoaderImpl(UniversalLoaderImplementation impl) {
        m_universalLoaderImpl = impl;
        saveSettings();
    }
    
    void setFormatRouterImpl(FormatRouterImplementation impl) {
        m_formatRouterImpl = impl;
        saveSettings();
    }
    
    void setEnhancedLoaderImpl(EnhancedLoaderImplementation impl) {
        m_enhancedLoaderImpl = impl;
        saveSettings();
    }
    
    // Feature flags
    bool isUniversalLoaderEnabled() const { return m_universalLoaderEnabled; }
    bool isFormatRouterEnabled() const { return m_formatRouterEnabled; }
    bool isEnhancedLoaderEnabled() const { return m_enhancedLoaderEnabled; }
    
    void setUniversalLoaderEnabled(bool enabled) {
        m_universalLoaderEnabled = enabled;
        saveSettings();
    }
    
    void setFormatRouterEnabled(bool enabled) {
        m_formatRouterEnabled = enabled;
        saveSettings();
    }
    
    void setEnhancedLoaderEnabled(bool enabled) {
        m_enhancedLoaderEnabled = enabled;
        saveSettings();
    }
    
    // Performance settings
    bool useCaching() const { return m_useCaching; }
    void setUseCaching(bool enable) {
        m_useCaching = enable;
        saveSettings();
    }
    
    int getCacheTTLSeconds() const { return m_cacheTTLSeconds; }
    void setCacheTTLSeconds(int seconds) {
        m_cacheTTLSeconds = seconds;
        saveSettings();
    }
    
    // Logging and diagnostics
    bool enableDiagnosticLogging() const { return m_diagnosticLogging; }
    void setDiagnosticLogging(bool enable) {
        m_diagnosticLogging = enable;
        saveSettings();
    }
    
    bool enablePerformanceProfiling() const { return m_performanceProfiling; }
    void setPerformanceProfiling(bool enable) {
        m_performanceProfiling = enable;
        saveSettings();
    }
    
    // Load settings from QSettings
    void loadSettings();
    
    // Save settings to QSettings
    void saveSettings();
    
    // Reset to defaults
    void resetToDefaults();
    
    // Print current configuration for debugging
    QString getConfigSummary() const;
    
private:
    UniversalFormatLoaderSettings();
    
    // Implementation modes
    UniversalLoaderImplementation m_universalLoaderImpl;
    FormatRouterImplementation m_formatRouterImpl;
    EnhancedLoaderImplementation m_enhancedLoaderImpl;
    
    // Feature flags
    bool m_universalLoaderEnabled;
    bool m_formatRouterEnabled;
    bool m_enhancedLoaderEnabled;
    
    // Performance settings
    bool m_useCaching;
    int m_cacheTTLSeconds;
    
    // Diagnostics
    bool m_diagnosticLogging;
    bool m_performanceProfiling;
    
    // Qt settings storage
    std::unique_ptr<QSettings> m_qSettings;
};

// ========================================================================================================
// CONVENIENCE FUNCTIONS FOR SETTINGS ACCESS
// ========================================================================================================

inline UniversalLoaderImplementation getUniversalLoaderMode() {
    return UniversalFormatLoaderSettings::instance().getUniversalLoaderImpl();
}

inline FormatRouterImplementation getFormatRouterMode() {
    return UniversalFormatLoaderSettings::instance().getFormatRouterImpl();
}

inline EnhancedLoaderImplementation getEnhancedLoaderMode() {
    return UniversalFormatLoaderSettings::instance().getEnhancedLoaderImpl();
}

inline void setUniversalLoaderMode(UniversalLoaderImplementation impl) {
    UniversalFormatLoaderSettings::instance().setUniversalLoaderImpl(impl);
}

inline void setFormatRouterMode(FormatRouterImplementation impl) {
    UniversalFormatLoaderSettings::instance().setFormatRouterImpl(impl);
}

inline void setEnhancedLoaderMode(EnhancedLoaderImplementation impl) {
    UniversalFormatLoaderSettings::instance().setEnhancedLoaderImpl(impl);
}

// ========================================================================================================
// SETTINGS UI DIALOG
// ========================================================================================================

class UniversalFormatLoaderSettingsDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit UniversalFormatLoaderSettingsDialog(QWidget* parent = nullptr);
    ~UniversalFormatLoaderSettingsDialog() override;
    
private slots:
    void onUniversalLoaderModeChanged(int index);
    void onFormatRouterModeChanged(int index);
    void onEnhancedLoaderModeChanged(int index);
    void onCachingToggled(bool checked);
    void onDiagnosticsToggled(bool checked);
    void onProfilingToggled(bool checked);
    void onApply();
    void onReset();
    
private:
    void setupUI();
    void loadCurrentSettings();
    void applySettings();
    
    // UI elements
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
