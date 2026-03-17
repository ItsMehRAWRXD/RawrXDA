// RawrXD Agentic IDE - Theme Manager
// Fully customizable themes with real-time transparency controls
#pragma once

#include <QObject>
#include <QColor>
#include <QJsonObject>
#include <QMap>
#include <QWidget>
#include <QMutex>
#include <QMutexLocker>

namespace RawrXD {

/**
 * @brief ThemeColors - Complete color scheme for the IDE
 * 
 * Contains all customizable colors for the editor, syntax highlighting,
 * chat interface, and UI elements. Supports transparency/opacity settings.
 */
struct ThemeColors {
    // Editor Colors
    QColor editorBackground;
    QColor editorForeground;
    QColor editorSelection;
    QColor editorCurrentLine;
    QColor editorLineNumbers;
    QColor editorWhitespace;
    QColor editorIndentGuides;
    
    // Syntax Highlighting
    QColor keywordColor;
    QColor stringColor;
    QColor commentColor;
    QColor numberColor;
    QColor functionColor;
    QColor classColor;
    QColor operatorColor;
    QColor preprocessorColor;

    struct LanguageSyntaxColors {
        QColor keyword;
        QColor string;
        QColor comment;
        QColor number;
        QColor function;
        QColor classColor;
        QColor operatorColor;
        QColor preprocessor;
        double syntaxOpacity = 1.0;

        QJsonObject toJson() const;
        static LanguageSyntaxColors fromJson(const QJsonObject& json, const ThemeColors& fallback);
    };

    // Per-language overrides (e.g., "cpp", "python", "javascript")
    QMap<QString, LanguageSyntaxColors> languageSyntax; // key: language slug
    
    // Chat Colors
    QColor chatUserBackground;
    QColor chatUserForeground;
    QColor chatAIBackground;
    QColor chatAIForeground;
    QColor chatSystemBackground;
    QColor chatSystemForeground;
    QColor chatBorder;
    
    // UI Colors
    QColor windowBackground;
    QColor windowForeground;
    QColor dockBackground;
    QColor dockBorder;
    QColor toolbarBackground;
    QColor menuBackground;
    QColor menuForeground;
    QColor buttonBackground;
    QColor buttonForeground;
    QColor buttonHover;
    QColor buttonPressed;
    
    // Transparency (0.0 - 1.0)
    double windowOpacity;
    double dockOpacity;
    double chatOpacity;
    double editorOpacity;
    
    ThemeColors() {
        setDefaultDarkTheme();
    }
    
    void setDefaultDarkTheme();
    void setDefaultLightTheme();
    void setCustomTheme(const QJsonObject& theme);
    QJsonObject toJson() const;
    static ThemeColors fromJson(const QJsonObject& json);
};

/**
 * @brief ThemeManager - Singleton for managing IDE themes and transparency
 * 
 * Features:
 * - Built-in themes (Dark, Light, High Contrast, Glass)
 * - Real-time color customization
 * - Per-element opacity controls
 * - Always-on-top window management
 * - Theme import/export (JSON)
 * - Thread-safe design
 */
class ThemeManager : public QObject {
    Q_OBJECT
    
public:
    static ThemeManager& instance();
    
    // Theme Management
    void loadTheme(const QString& themeName);
    void saveTheme(const QString& themeName);
    void deleteTheme(const QString& themeName);
    QStringList availableThemes() const;
    QString currentThemeName() const { return m_currentThemeName; }
    
    // Color Access
    const ThemeColors& currentColors() const { return m_currentColors; }
    QColor getColor(const QString& colorName) const;

    // Per-language syntax accessors
    ThemeColors::LanguageSyntaxColors languageColors(const QString& langKey) const;
    void updateLanguageColor(const QString& langKey, const QString& role, const QColor& color);
    void updateLanguageOpacity(const QString& langKey, double opacity);
    double languageOpacity(const QString& langKey) const;
    
    // Real-time Updates
    void updateColor(const QString& colorName, const QColor& color);
    void updateOpacity(const QString& element, double opacity);
    
    // Transparency Controls
    void setWindowOpacity(double opacity);
    void setDockOpacity(double opacity);
    void setChatOpacity(double opacity);
    void setEditorOpacity(double opacity);
    
    double windowOpacity() const { return m_currentColors.windowOpacity; }
    double dockOpacity() const { return m_currentColors.dockOpacity; }
    double chatOpacity() const { return m_currentColors.chatOpacity; }
    double editorOpacity() const { return m_currentColors.editorOpacity; }
    
    // Window Management
    void setMainWindow(QWidget* mainWindow);
    void setWindowTransparencyEnabled(bool enabled);
    bool isWindowTransparencyEnabled() const { return m_transparencyEnabled; }
    
    void setAlwaysOnTop(bool enabled);
    bool isAlwaysOnTop() const { return m_alwaysOnTop; }
    
    void setClickThroughEnabled(bool enabled);
    bool isClickThroughEnabled() const { return m_clickThroughEnabled; }
    
    // Apply Theme
    void applyThemeToWidget(QWidget* widget);
    void applyThemeToApplication();
    
    // Theme Import/Export
    bool importTheme(const QString& filePath, QString& errorMsg);
    bool exportTheme(const QString& filePath, QString& errorMsg) const;
    
signals:
    void themeChanged();
    void colorsUpdated();
    void opacityChanged(const QString& element, double opacity);
    void transparencySettingsChanged();
    
private:
    ThemeManager();
    ~ThemeManager() = default;
    
    // Prevent copying
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    
    void initializeDefaultThemes();
    void applyColorsToStylesheet();
    QString generateStylesheet() const;
    void updateWindowTransparency();
    void loadThemesFromDisk();
    void saveThemesToDisk();
    QString getThemeDirectory() const;
    
    ThemeColors m_currentColors;
    QString m_currentThemeName;
    QMap<QString, ThemeColors> m_themes;
    
    bool m_transparencyEnabled = false;
    bool m_alwaysOnTop = false;
    bool m_clickThroughEnabled = false;
    
    QWidget* m_mainWindow = nullptr;
    mutable QMutex m_mutex;
};

} // namespace RawrXD
