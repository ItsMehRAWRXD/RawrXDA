// RawrXD Agentic IDE - Theme Manager Implementation
// Enterprise-grade theming system with observability
#include "ThemeManager.h"
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>
#include <QDebug>
#include <QDockWidget>
#include <chrono>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace RawrXD {

// Forward declaration for language palette syncing
static void syncLanguagePaletteToBase(ThemeColors& theme);

// ============================================================
// ThemeColors Implementation
// ============================================================

void ThemeColors::setDefaultDarkTheme() {
    // VS Code Dark+ inspired theme colors
    editorBackground = QColor(30, 30, 30);
    editorForeground = QColor(212, 212, 212);
    editorSelection = QColor(38, 79, 120);
    editorCurrentLine = QColor(37, 37, 38);
    editorLineNumbers = QColor(85, 85, 85);
    editorWhitespace = QColor(55, 55, 55);
    editorIndentGuides = QColor(55, 55, 55);
    
    keywordColor = QColor(86, 156, 214);      // Blue
    stringColor = QColor(206, 145, 120);       // Orange/brown
    commentColor = QColor(106, 153, 85);       // Green
    numberColor = QColor(181, 206, 168);       // Light green
    functionColor = QColor(220, 220, 170);     // Yellow
    classColor = QColor(78, 201, 176);         // Teal
    operatorColor = QColor(212, 212, 212);     // White
    preprocessorColor = QColor(190, 190, 190); // Light gray
    
    chatUserBackground = QColor(0, 122, 204);
    chatUserForeground = Qt::white;
    chatAIBackground = QColor(60, 60, 60);
    chatAIForeground = QColor(212, 212, 212);
    chatSystemBackground = QColor(40, 40, 40);
    chatSystemForeground = QColor(150, 150, 150);
    chatBorder = QColor(70, 70, 70);
    
    windowBackground = QColor(45, 45, 48);
    windowForeground = QColor(241, 241, 241);
    dockBackground = QColor(37, 37, 38);
    dockBorder = QColor(60, 60, 60);
    toolbarBackground = QColor(45, 45, 48);
    menuBackground = QColor(45, 45, 48);
    menuForeground = QColor(241, 241, 241);
    buttonBackground = QColor(62, 62, 66);
    buttonForeground = QColor(241, 241, 241);
    buttonHover = QColor(78, 78, 82);
    buttonPressed = QColor(94, 94, 98);
    
    windowOpacity = 1.0;
    dockOpacity = 1.0;
    chatOpacity = 1.0;
    editorOpacity = 1.0;

    syncLanguagePaletteToBase(*this);
}

void ThemeColors::setDefaultLightTheme() {
    // VS Code Light+ inspired theme colors
    editorBackground = QColor(255, 255, 255);
    editorForeground = QColor(0, 0, 0);
    editorSelection = QColor(184, 215, 255);
    editorCurrentLine = QColor(245, 245, 245);
    editorLineNumbers = QColor(120, 120, 120);
    editorWhitespace = QColor(200, 200, 200);
    editorIndentGuides = QColor(200, 200, 200);
    
    keywordColor = QColor(0, 0, 255);          // Blue
    stringColor = QColor(163, 21, 21);          // Red
    commentColor = QColor(0, 128, 0);           // Green
    numberColor = QColor(0, 0, 205);            // Dark blue
    functionColor = QColor(120, 120, 120);      // Gray
    classColor = QColor(0, 0, 255);             // Blue
    operatorColor = QColor(0, 0, 0);            // Black
    preprocessorColor = QColor(128, 128, 128);  // Gray
    
    chatUserBackground = QColor(0, 100, 200);
    chatUserForeground = Qt::white;
    chatAIBackground = QColor(240, 240, 240);
    chatAIForeground = QColor(0, 0, 0);
    chatSystemBackground = QColor(220, 220, 220);
    chatSystemForeground = QColor(100, 100, 100);
    chatBorder = QColor(200, 200, 200);
    
    windowBackground = QColor(240, 240, 240);
    windowForeground = QColor(0, 0, 0);
    dockBackground = QColor(255, 255, 255);
    dockBorder = QColor(225, 225, 225);
    toolbarBackground = QColor(240, 240, 240);
    menuBackground = QColor(240, 240, 240);
    menuForeground = QColor(0, 0, 0);
    buttonBackground = QColor(225, 225, 225);
    buttonForeground = QColor(0, 0, 0);
    buttonHover = QColor(210, 210, 210);
    buttonPressed = QColor(195, 195, 195);
    
    windowOpacity = 1.0;
    dockOpacity = 1.0;
    chatOpacity = 1.0;
    editorOpacity = 1.0;

    syncLanguagePaletteToBase(*this);
}

void ThemeColors::setCustomTheme(const QJsonObject& theme) {
    auto readColor = [&theme](const QString& key, const QColor& defaultVal) -> QColor {
        if (theme.contains(key)) {
            QString colorStr = theme[key].toString();
            QColor color(colorStr);
            if (color.isValid()) return color;
        }
        return defaultVal;
    };
    
    auto readDouble = [&theme](const QString& key, double defaultVal) -> double {
        if (theme.contains(key)) {
            return theme[key].toDouble(defaultVal);
        }
        return defaultVal;
    };
    
    // Editor colors
    editorBackground = readColor("editorBackground", editorBackground);
    editorForeground = readColor("editorForeground", editorForeground);
    editorSelection = readColor("editorSelection", editorSelection);
    editorCurrentLine = readColor("editorCurrentLine", editorCurrentLine);
    editorLineNumbers = readColor("editorLineNumbers", editorLineNumbers);
    editorWhitespace = readColor("editorWhitespace", editorWhitespace);
    editorIndentGuides = readColor("editorIndentGuides", editorIndentGuides);
    
    // Syntax highlighting
    keywordColor = readColor("keywordColor", keywordColor);
    stringColor = readColor("stringColor", stringColor);
    commentColor = readColor("commentColor", commentColor);
    numberColor = readColor("numberColor", numberColor);
    functionColor = readColor("functionColor", functionColor);
    classColor = readColor("classColor", classColor);
    operatorColor = readColor("operatorColor", operatorColor);
    preprocessorColor = readColor("preprocessorColor", preprocessorColor);
    
    // Chat colors
    chatUserBackground = readColor("chatUserBackground", chatUserBackground);
    chatUserForeground = readColor("chatUserForeground", chatUserForeground);
    chatAIBackground = readColor("chatAIBackground", chatAIBackground);
    chatAIForeground = readColor("chatAIForeground", chatAIForeground);
    chatSystemBackground = readColor("chatSystemBackground", chatSystemBackground);
    chatSystemForeground = readColor("chatSystemForeground", chatSystemForeground);
    chatBorder = readColor("chatBorder", chatBorder);
    
    // UI colors
    windowBackground = readColor("windowBackground", windowBackground);
    windowForeground = readColor("windowForeground", windowForeground);
    dockBackground = readColor("dockBackground", dockBackground);
    dockBorder = readColor("dockBorder", dockBorder);
    toolbarBackground = readColor("toolbarBackground", toolbarBackground);
    menuBackground = readColor("menuBackground", menuBackground);
    menuForeground = readColor("menuForeground", menuForeground);
    buttonBackground = readColor("buttonBackground", buttonBackground);
    buttonForeground = readColor("buttonForeground", buttonForeground);
    buttonHover = readColor("buttonHover", buttonHover);
    buttonPressed = readColor("buttonPressed", buttonPressed);
    
    // Opacity
    windowOpacity = readDouble("windowOpacity", windowOpacity);
    dockOpacity = readDouble("dockOpacity", dockOpacity);
    chatOpacity = readDouble("chatOpacity", chatOpacity);
    editorOpacity = readDouble("editorOpacity", editorOpacity);

    // Per-language syntax overrides
    if (theme.contains("languageSyntax") && theme["languageSyntax"].isObject()) {
        QJsonObject langObj = theme["languageSyntax"].toObject();
        for (auto it = langObj.begin(); it != langObj.end(); ++it) {
            if (it.value().isObject()) {
                languageSyntax[it.key()] = ThemeColors::LanguageSyntaxColors::fromJson(it.value().toObject(), *this);
            }
        }
    }
}

QJsonObject ThemeColors::toJson() const {
    QJsonObject json;
    
    // Editor colors
    json["editorBackground"] = editorBackground.name(QColor::HexArgb);
    json["editorForeground"] = editorForeground.name(QColor::HexArgb);
    json["editorSelection"] = editorSelection.name(QColor::HexArgb);
    json["editorCurrentLine"] = editorCurrentLine.name(QColor::HexArgb);
    json["editorLineNumbers"] = editorLineNumbers.name(QColor::HexArgb);
    json["editorWhitespace"] = editorWhitespace.name(QColor::HexArgb);
    json["editorIndentGuides"] = editorIndentGuides.name(QColor::HexArgb);
    
    // Syntax highlighting
    json["keywordColor"] = keywordColor.name(QColor::HexArgb);
    json["stringColor"] = stringColor.name(QColor::HexArgb);
    json["commentColor"] = commentColor.name(QColor::HexArgb);
    json["numberColor"] = numberColor.name(QColor::HexArgb);
    json["functionColor"] = functionColor.name(QColor::HexArgb);
    json["classColor"] = classColor.name(QColor::HexArgb);
    json["operatorColor"] = operatorColor.name(QColor::HexArgb);
    json["preprocessorColor"] = preprocessorColor.name(QColor::HexArgb);
    
    // Chat colors
    json["chatUserBackground"] = chatUserBackground.name(QColor::HexArgb);
    json["chatUserForeground"] = chatUserForeground.name(QColor::HexArgb);
    json["chatAIBackground"] = chatAIBackground.name(QColor::HexArgb);
    json["chatAIForeground"] = chatAIForeground.name(QColor::HexArgb);
    json["chatSystemBackground"] = chatSystemBackground.name(QColor::HexArgb);
    json["chatSystemForeground"] = chatSystemForeground.name(QColor::HexArgb);
    json["chatBorder"] = chatBorder.name(QColor::HexArgb);
    
    // UI colors
    json["windowBackground"] = windowBackground.name(QColor::HexArgb);
    json["windowForeground"] = windowForeground.name(QColor::HexArgb);
    json["dockBackground"] = dockBackground.name(QColor::HexArgb);
    json["dockBorder"] = dockBorder.name(QColor::HexArgb);
    json["toolbarBackground"] = toolbarBackground.name(QColor::HexArgb);
    json["menuBackground"] = menuBackground.name(QColor::HexArgb);
    json["menuForeground"] = menuForeground.name(QColor::HexArgb);
    json["buttonBackground"] = buttonBackground.name(QColor::HexArgb);
    json["buttonForeground"] = buttonForeground.name(QColor::HexArgb);
    json["buttonHover"] = buttonHover.name(QColor::HexArgb);
    json["buttonPressed"] = buttonPressed.name(QColor::HexArgb);
    
    // Opacity
    json["windowOpacity"] = windowOpacity;
    json["dockOpacity"] = dockOpacity;
    json["chatOpacity"] = chatOpacity;
    json["editorOpacity"] = editorOpacity;

    // Per-language syntax overrides
    QJsonObject langObj;
    for (auto it = languageSyntax.constBegin(); it != languageSyntax.constEnd(); ++it) {
        langObj[it.key()] = it.value().toJson();
    }
    json["languageSyntax"] = langObj;
    
    return json;
}

ThemeColors ThemeColors::fromJson(const QJsonObject& json) {
    ThemeColors colors;
    colors.setCustomTheme(json);
    return colors;
}

// Helper to propagate base syntax colors into per-language overrides
static void syncLanguagePaletteToBase(ThemeColors& theme) {
    auto mk = [&theme](double opacity = 1.0) {
        ThemeColors::LanguageSyntaxColors lang;
        lang.keyword = theme.keywordColor;
        lang.string = theme.stringColor;
        lang.comment = theme.commentColor;
        lang.number = theme.numberColor;
        lang.function = theme.functionColor;
        lang.classColor = theme.classColor;
        lang.operatorColor = theme.operatorColor;
        lang.preprocessor = theme.preprocessorColor;
        lang.syntaxOpacity = opacity;
        return lang;
    };
    theme.languageSyntax.clear();
    theme.languageSyntax.insert("cpp", mk());
    theme.languageSyntax.insert("python", mk());
    theme.languageSyntax.insert("javascript", mk());
    theme.languageSyntax.insert("typescript", mk());
    theme.languageSyntax.insert("json", mk());
    theme.languageSyntax.insert("xml", mk());
    theme.languageSyntax.insert("markdown", mk(0.9));
}

QJsonObject ThemeColors::LanguageSyntaxColors::toJson() const {
    QJsonObject obj;
    obj["keyword"] = keyword.name(QColor::HexArgb);
    obj["string"] = string.name(QColor::HexArgb);
    obj["comment"] = comment.name(QColor::HexArgb);
    obj["number"] = number.name(QColor::HexArgb);
    obj["function"] = function.name(QColor::HexArgb);
    obj["class"] = classColor.name(QColor::HexArgb);
    obj["operator"] = operatorColor.name(QColor::HexArgb);
    obj["preprocessor"] = preprocessor.name(QColor::HexArgb);
    obj["syntaxOpacity"] = syntaxOpacity;
    return obj;
}

ThemeColors::LanguageSyntaxColors ThemeColors::LanguageSyntaxColors::fromJson(const QJsonObject& json, const ThemeColors& fallback) {
    ThemeColors::LanguageSyntaxColors lang;
    auto readColor = [&json](const QString& key, const QColor& def) {
        if (json.contains(key)) {
            QColor c(json[key].toString());
            if (c.isValid()) return c;
        }
        return def;
    };
    lang.keyword = readColor("keyword", fallback.keywordColor);
    lang.string = readColor("string", fallback.stringColor);
    lang.comment = readColor("comment", fallback.commentColor);
    lang.number = readColor("number", fallback.numberColor);
    lang.function = readColor("function", fallback.functionColor);
    lang.classColor = readColor("class", fallback.classColor);
    lang.operatorColor = readColor("operator", fallback.operatorColor);
    lang.preprocessor = readColor("preprocessor", fallback.preprocessorColor);
    lang.syntaxOpacity = json.value("syntaxOpacity").toDouble(1.0);
    return lang;
}

// ============================================================
// ThemeManager Implementation
// ============================================================

ThemeManager& ThemeManager::instance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() {
    qDebug() << "[ThemeManager] Initializing theme system...";
    auto startTime = std::chrono::steady_clock::now();
    
    initializeDefaultThemes();
    loadThemesFromDisk();
    loadTheme("Dark"); // Load default theme
    
    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    qDebug() << "[ThemeManager] Theme system initialized in" << durationMs << "ms";
}

void ThemeManager::initializeDefaultThemes() {
    qDebug() << "[ThemeManager] Creating default themes...";
    
    // Dark Theme (default)
    ThemeColors darkTheme;
    darkTheme.setDefaultDarkTheme();
    m_themes["Dark"] = darkTheme;
    
    // Light Theme
    ThemeColors lightTheme;
    lightTheme.setDefaultLightTheme();
    m_themes["Light"] = lightTheme;
    
    // High Contrast Theme
    ThemeColors highContrast;
    highContrast.editorBackground = Qt::black;
    highContrast.editorForeground = Qt::white;
    highContrast.editorSelection = QColor(255, 255, 0);
    highContrast.editorCurrentLine = QColor(40, 40, 40);
    highContrast.editorLineNumbers = QColor(200, 200, 200);
    highContrast.editorWhitespace = QColor(100, 100, 100);
    highContrast.editorIndentGuides = QColor(100, 100, 100);
    
    highContrast.keywordColor = QColor(255, 255, 0);   // Yellow
    highContrast.stringColor = QColor(255, 128, 0);    // Orange
    highContrast.commentColor = QColor(128, 128, 128); // Gray
    highContrast.numberColor = QColor(0, 255, 255);    // Cyan
    highContrast.functionColor = QColor(255, 255, 255);// White
    highContrast.classColor = QColor(255, 255, 255);   // White
    highContrast.operatorColor = QColor(255, 255, 255);
    highContrast.preprocessorColor = QColor(200, 200, 200);
    
    highContrast.chatUserBackground = QColor(0, 0, 128);
    highContrast.chatUserForeground = Qt::white;
    highContrast.chatAIBackground = QColor(128, 0, 0);
    highContrast.chatAIForeground = Qt::white;
    highContrast.chatSystemBackground = QColor(64, 64, 64);
    highContrast.chatSystemForeground = Qt::white;
    highContrast.chatBorder = Qt::white;
    
    highContrast.windowBackground = Qt::black;
    highContrast.windowForeground = Qt::white;
    highContrast.dockBackground = Qt::black;
    highContrast.dockBorder = Qt::white;
    highContrast.toolbarBackground = Qt::black;
    highContrast.menuBackground = Qt::black;
    highContrast.menuForeground = Qt::white;
    highContrast.buttonBackground = QColor(64, 64, 64);
    highContrast.buttonForeground = Qt::white;
    highContrast.buttonHover = QColor(96, 96, 96);
    highContrast.buttonPressed = QColor(128, 128, 128);
    
    highContrast.windowOpacity = 1.0;
    highContrast.dockOpacity = 1.0;
    highContrast.chatOpacity = 1.0;
    highContrast.editorOpacity = 1.0;
    syncLanguagePaletteToBase(highContrast);
    m_themes["High Contrast"] = highContrast;
    
    // Glass Theme (transparency-focused)
    ThemeColors glassTheme;
    glassTheme.editorBackground = QColor(20, 20, 30, 240);
    glassTheme.editorForeground = QColor(220, 220, 220);
    glassTheme.editorSelection = QColor(60, 120, 200, 180);
    glassTheme.editorCurrentLine = QColor(40, 40, 60, 120);
    glassTheme.editorLineNumbers = QColor(130, 130, 150);
    glassTheme.editorWhitespace = QColor(80, 80, 100);
    glassTheme.editorIndentGuides = QColor(80, 80, 100);
    
    glassTheme.keywordColor = QColor(100, 200, 255);
    glassTheme.stringColor = QColor(255, 200, 100);
    glassTheme.commentColor = QColor(100, 150, 100);
    glassTheme.numberColor = QColor(255, 150, 100);
    glassTheme.functionColor = QColor(150, 200, 255);
    glassTheme.classColor = QColor(100, 220, 200);
    glassTheme.operatorColor = QColor(200, 200, 220);
    glassTheme.preprocessorColor = QColor(180, 180, 200);
    
    glassTheme.chatUserBackground = QColor(0, 100, 200, 220);
    glassTheme.chatUserForeground = Qt::white;
    glassTheme.chatAIBackground = QColor(200, 100, 0, 220);
    glassTheme.chatAIForeground = Qt::white;
    glassTheme.chatSystemBackground = QColor(60, 60, 80, 200);
    glassTheme.chatSystemForeground = QColor(200, 200, 200);
    glassTheme.chatBorder = QColor(100, 100, 150, 180);
    
    glassTheme.windowBackground = QColor(10, 10, 20, 200);
    glassTheme.windowForeground = QColor(220, 220, 220);
    glassTheme.dockBackground = QColor(20, 20, 40, 220);
    glassTheme.dockBorder = QColor(100, 100, 150, 180);
    glassTheme.toolbarBackground = QColor(30, 30, 50, 230);
    glassTheme.menuBackground = QColor(25, 25, 45, 240);
    glassTheme.menuForeground = QColor(220, 220, 220);
    glassTheme.buttonBackground = QColor(50, 50, 80, 200);
    glassTheme.buttonForeground = QColor(220, 220, 220);
    glassTheme.buttonHover = QColor(70, 70, 110, 220);
    glassTheme.buttonPressed = QColor(90, 90, 130, 240);
    
    glassTheme.windowOpacity = 0.95;
    glassTheme.dockOpacity = 0.90;
    glassTheme.chatOpacity = 0.92;
    glassTheme.editorOpacity = 0.88;
    syncLanguagePaletteToBase(glassTheme);
    m_themes["Glass"] = glassTheme;
    
    // Monokai Theme (popular programmer theme)
    ThemeColors monokaiTheme;
    monokaiTheme.editorBackground = QColor(39, 40, 34);
    monokaiTheme.editorForeground = QColor(248, 248, 242);
    monokaiTheme.editorSelection = QColor(73, 72, 62);
    monokaiTheme.editorCurrentLine = QColor(56, 56, 48);
    monokaiTheme.editorLineNumbers = QColor(144, 144, 138);
    monokaiTheme.editorWhitespace = QColor(60, 60, 54);
    monokaiTheme.editorIndentGuides = QColor(60, 60, 54);
    
    monokaiTheme.keywordColor = QColor(249, 38, 114);    // Pink
    monokaiTheme.stringColor = QColor(230, 219, 116);    // Yellow
    monokaiTheme.commentColor = QColor(117, 113, 94);    // Gray
    monokaiTheme.numberColor = QColor(174, 129, 255);    // Purple
    monokaiTheme.functionColor = QColor(166, 226, 46);   // Green
    monokaiTheme.classColor = QColor(102, 217, 239);     // Cyan
    monokaiTheme.operatorColor = QColor(249, 38, 114);   // Pink
    monokaiTheme.preprocessorColor = QColor(174, 129, 255);
    
    monokaiTheme.chatUserBackground = QColor(249, 38, 114);
    monokaiTheme.chatUserForeground = Qt::white;
    monokaiTheme.chatAIBackground = QColor(56, 56, 48);
    monokaiTheme.chatAIForeground = QColor(248, 248, 242);
    monokaiTheme.chatSystemBackground = QColor(45, 46, 40);
    monokaiTheme.chatSystemForeground = QColor(144, 144, 138);
    monokaiTheme.chatBorder = QColor(73, 72, 62);
    
    monokaiTheme.windowBackground = QColor(39, 40, 34);
    monokaiTheme.windowForeground = QColor(248, 248, 242);
    monokaiTheme.dockBackground = QColor(45, 46, 40);
    monokaiTheme.dockBorder = QColor(73, 72, 62);
    monokaiTheme.toolbarBackground = QColor(39, 40, 34);
    monokaiTheme.menuBackground = QColor(39, 40, 34);
    monokaiTheme.menuForeground = QColor(248, 248, 242);
    monokaiTheme.buttonBackground = QColor(56, 56, 48);
    monokaiTheme.buttonForeground = QColor(248, 248, 242);
    monokaiTheme.buttonHover = QColor(73, 72, 62);
    monokaiTheme.buttonPressed = QColor(90, 90, 80);
    
    monokaiTheme.windowOpacity = 1.0;
    monokaiTheme.dockOpacity = 1.0;
    monokaiTheme.chatOpacity = 1.0;
    monokaiTheme.editorOpacity = 1.0;
    syncLanguagePaletteToBase(monokaiTheme);
    m_themes["Monokai"] = monokaiTheme;
    
    qDebug() << "[ThemeManager] Created" << m_themes.size() << "default themes";
}

void ThemeManager::loadTheme(const QString& themeName) {
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "[ThemeManager] Loading theme:" << themeName;
    auto startTime = std::chrono::steady_clock::now();
    
    if (!m_themes.contains(themeName)) {
        qWarning() << "[ThemeManager] Theme not found:" << themeName;
        return;
    }
    
    m_currentColors = m_themes[themeName];
    m_currentThemeName = themeName;
    
    locker.unlock();
    
    // Apply theme to application
    applyThemeToApplication();
    
    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    qDebug() << "[ThemeManager] Theme loaded in" << durationMs << "ms";
    
    emit themeChanged();
}

void ThemeManager::saveTheme(const QString& themeName) {
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "[ThemeManager] Saving theme:" << themeName;
    m_themes[themeName] = m_currentColors;
    
    locker.unlock();
    
    saveThemesToDisk();
}

void ThemeManager::deleteTheme(const QString& themeName) {
    QMutexLocker locker(&m_mutex);
    
    // Prevent deletion of built-in themes
    QStringList builtInThemes = {"Dark", "Light", "High Contrast", "Glass", "Monokai"};
    if (builtInThemes.contains(themeName)) {
        qWarning() << "[ThemeManager] Cannot delete built-in theme:" << themeName;
        return;
    }
    
    if (m_themes.contains(themeName)) {
        m_themes.remove(themeName);
        qDebug() << "[ThemeManager] Deleted theme:" << themeName;
        
        locker.unlock();
        saveThemesToDisk();
    }
}

QStringList ThemeManager::availableThemes() const {
    QMutexLocker locker(&m_mutex);
    return m_themes.keys();
}

QColor ThemeManager::getColor(const QString& colorName) const {
    QMutexLocker locker(&m_mutex);

    // Language-specific syntax colors (lang.<key>.<role>)
    QString langKey;
    QString role;
    auto parseLanguage = [&langKey, &role](const QString& name) {
        const QString prefix = "lang.";
        if (!name.startsWith(prefix)) return false;
        QString remaining = name.mid(prefix.size());
        const int dot = remaining.indexOf('.');
        if (dot <= 0) return false;
        langKey = remaining.left(dot);
        role = remaining.mid(dot + 1);
        return true;
    };
    if (parseLanguage(colorName)) {
        auto it = m_currentColors.languageSyntax.constFind(langKey);
        const auto base = m_currentColors;
        ThemeColors::LanguageSyntaxColors lang = (it != m_currentColors.languageSyntax.constEnd()) ? it.value() : ThemeColors::LanguageSyntaxColors{base.keywordColor, base.stringColor, base.commentColor, base.numberColor, base.functionColor, base.classColor, base.operatorColor, base.preprocessorColor, 1.0};
        if (role == "keywordColor" || role == "keyword") return lang.keyword;
        if (role == "stringColor" || role == "string") return lang.string;
        if (role == "commentColor" || role == "comment") return lang.comment;
        if (role == "numberColor" || role == "number") return lang.number;
        if (role == "functionColor" || role == "function") return lang.function;
        if (role == "classColor" || role == "class") return lang.classColor;
        if (role == "operatorColor" || role == "operator") return lang.operatorColor;
        if (role == "preprocessorColor" || role == "preprocessor") return lang.preprocessor;
        return QColor();
    }
    
    // Editor colors
    if (colorName == "editorBackground") return m_currentColors.editorBackground;
    if (colorName == "editorForeground") return m_currentColors.editorForeground;
    if (colorName == "editorSelection") return m_currentColors.editorSelection;
    if (colorName == "editorCurrentLine") return m_currentColors.editorCurrentLine;
    if (colorName == "editorLineNumbers") return m_currentColors.editorLineNumbers;
    if (colorName == "editorWhitespace") return m_currentColors.editorWhitespace;
    if (colorName == "editorIndentGuides") return m_currentColors.editorIndentGuides;
    
    // Syntax highlighting
    if (colorName == "keywordColor") return m_currentColors.keywordColor;
    if (colorName == "stringColor") return m_currentColors.stringColor;
    if (colorName == "commentColor") return m_currentColors.commentColor;
    if (colorName == "numberColor") return m_currentColors.numberColor;
    if (colorName == "functionColor") return m_currentColors.functionColor;
    if (colorName == "classColor") return m_currentColors.classColor;
    if (colorName == "operatorColor") return m_currentColors.operatorColor;
    if (colorName == "preprocessorColor") return m_currentColors.preprocessorColor;
    
    // Chat colors
    if (colorName == "chatUserBackground") return m_currentColors.chatUserBackground;
    if (colorName == "chatUserForeground") return m_currentColors.chatUserForeground;
    if (colorName == "chatAIBackground") return m_currentColors.chatAIBackground;
    if (colorName == "chatAIForeground") return m_currentColors.chatAIForeground;
    if (colorName == "chatSystemBackground") return m_currentColors.chatSystemBackground;
    if (colorName == "chatSystemForeground") return m_currentColors.chatSystemForeground;
    if (colorName == "chatBorder") return m_currentColors.chatBorder;
    
    // UI colors
    if (colorName == "windowBackground") return m_currentColors.windowBackground;
    if (colorName == "windowForeground") return m_currentColors.windowForeground;
    if (colorName == "dockBackground") return m_currentColors.dockBackground;
    if (colorName == "dockBorder") return m_currentColors.dockBorder;
    if (colorName == "toolbarBackground") return m_currentColors.toolbarBackground;
    if (colorName == "menuBackground") return m_currentColors.menuBackground;
    if (colorName == "menuForeground") return m_currentColors.menuForeground;
    if (colorName == "buttonBackground") return m_currentColors.buttonBackground;
    if (colorName == "buttonForeground") return m_currentColors.buttonForeground;
    if (colorName == "buttonHover") return m_currentColors.buttonHover;
    if (colorName == "buttonPressed") return m_currentColors.buttonPressed;
    
    qWarning() << "[ThemeManager] Unknown color name:" << colorName;
    return QColor();
}

void ThemeManager::updateColor(const QString& colorName, const QColor& color) {
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "[ThemeManager] Updating color:" << colorName << "to" << color.name();

    // Language-specific syntax colors (lang.<key>.<role>)
    QString langKey;
    QString role;
    auto parseLanguage = [&langKey, &role](const QString& name) {
        const QString prefix = "lang.";
        if (!name.startsWith(prefix)) return false;
        QString remaining = name.mid(prefix.size());
        const int dot = remaining.indexOf('.');
        if (dot <= 0) return false;
        langKey = remaining.left(dot);
        role = remaining.mid(dot + 1);
        return true;
    };
    if (parseLanguage(colorName)) {
        auto &lang = m_currentColors.languageSyntax[langKey];
        if (role == "keywordColor" || role == "keyword") lang.keyword = color;
        else if (role == "stringColor" || role == "string") lang.string = color;
        else if (role == "commentColor" || role == "comment") lang.comment = color;
        else if (role == "numberColor" || role == "number") lang.number = color;
        else if (role == "functionColor" || role == "function") lang.function = color;
        else if (role == "classColor" || role == "class") lang.classColor = color;
        else if (role == "operatorColor" || role == "operator") lang.operatorColor = color;
        else if (role == "preprocessorColor" || role == "preprocessor") lang.preprocessor = color;
        else return;

        locker.unlock();
        emit colorsUpdated();
        return;
    }
    
    // Editor colors
    if (colorName == "editorBackground") m_currentColors.editorBackground = color;
    else if (colorName == "editorForeground") m_currentColors.editorForeground = color;
    else if (colorName == "editorSelection") m_currentColors.editorSelection = color;
    else if (colorName == "editorCurrentLine") m_currentColors.editorCurrentLine = color;
    else if (colorName == "editorLineNumbers") m_currentColors.editorLineNumbers = color;
    else if (colorName == "editorWhitespace") m_currentColors.editorWhitespace = color;
    else if (colorName == "editorIndentGuides") m_currentColors.editorIndentGuides = color;
    
    // Syntax highlighting
    else if (colorName == "keywordColor") m_currentColors.keywordColor = color;
    else if (colorName == "stringColor") m_currentColors.stringColor = color;
    else if (colorName == "commentColor") m_currentColors.commentColor = color;
    else if (colorName == "numberColor") m_currentColors.numberColor = color;
    else if (colorName == "functionColor") m_currentColors.functionColor = color;
    else if (colorName == "classColor") m_currentColors.classColor = color;
    else if (colorName == "operatorColor") m_currentColors.operatorColor = color;
    else if (colorName == "preprocessorColor") m_currentColors.preprocessorColor = color;
    
    // Chat colors
    else if (colorName == "chatUserBackground") m_currentColors.chatUserBackground = color;
    else if (colorName == "chatUserForeground") m_currentColors.chatUserForeground = color;
    else if (colorName == "chatAIBackground") m_currentColors.chatAIBackground = color;
    else if (colorName == "chatAIForeground") m_currentColors.chatAIForeground = color;
    else if (colorName == "chatSystemBackground") m_currentColors.chatSystemBackground = color;
    else if (colorName == "chatSystemForeground") m_currentColors.chatSystemForeground = color;
    else if (colorName == "chatBorder") m_currentColors.chatBorder = color;
    
    // UI colors
    else if (colorName == "windowBackground") m_currentColors.windowBackground = color;
    else if (colorName == "windowForeground") m_currentColors.windowForeground = color;
    else if (colorName == "dockBackground") m_currentColors.dockBackground = color;
    else if (colorName == "dockBorder") m_currentColors.dockBorder = color;
    else if (colorName == "toolbarBackground") m_currentColors.toolbarBackground = color;
    else if (colorName == "menuBackground") m_currentColors.menuBackground = color;
    else if (colorName == "menuForeground") m_currentColors.menuForeground = color;
    else if (colorName == "buttonBackground") m_currentColors.buttonBackground = color;
    else if (colorName == "buttonForeground") m_currentColors.buttonForeground = color;
    else if (colorName == "buttonHover") m_currentColors.buttonHover = color;
    else if (colorName == "buttonPressed") m_currentColors.buttonPressed = color;
    else {
        qWarning() << "[ThemeManager] Unknown color name:" << colorName;
        return;
    }
    
    locker.unlock();
    
    emit colorsUpdated();
}

void ThemeManager::updateOpacity(const QString& element, double opacity) {
    QMutexLocker locker(&m_mutex);
    
    // Clamp opacity to valid range
    opacity = qBound(0.1, opacity, 1.0);
    
    qDebug() << "[ThemeManager] Updating opacity:" << element << "to" << opacity;
    
    if (element == "window") {
        m_currentColors.windowOpacity = opacity;
    } else if (element == "dock") {
        m_currentColors.dockOpacity = opacity;
    } else if (element == "chat") {
        m_currentColors.chatOpacity = opacity;
    } else if (element == "editor") {
        m_currentColors.editorOpacity = opacity;
    } else {
        qWarning() << "[ThemeManager] Unknown opacity element:" << element;
        return;
    }
    
    locker.unlock();
    
    updateWindowTransparency();
    emit opacityChanged(element, opacity);
}

ThemeColors::LanguageSyntaxColors ThemeManager::languageColors(const QString& langKey) const {
    QMutexLocker locker(&m_mutex);
    auto it = m_currentColors.languageSyntax.constFind(langKey);
    if (it != m_currentColors.languageSyntax.constEnd()) {
        return it.value();
    }

    ThemeColors::LanguageSyntaxColors fallback;
    fallback.keyword = m_currentColors.keywordColor;
    fallback.string = m_currentColors.stringColor;
    fallback.comment = m_currentColors.commentColor;
    fallback.number = m_currentColors.numberColor;
    fallback.function = m_currentColors.functionColor;
    fallback.classColor = m_currentColors.classColor;
    fallback.operatorColor = m_currentColors.operatorColor;
    fallback.preprocessor = m_currentColors.preprocessorColor;
    fallback.syntaxOpacity = 1.0;
    return fallback;
}

void ThemeManager::updateLanguageColor(const QString& langKey, const QString& role, const QColor& color) {
    QString composite = QString("lang.%1.%2").arg(langKey, role);
    updateColor(composite, color);
}

void ThemeManager::updateLanguageOpacity(const QString& langKey, double opacity) {
    QMutexLocker locker(&m_mutex);
    opacity = qBound(0.2, opacity, 1.0);
    auto &lang = m_currentColors.languageSyntax[langKey];
    lang.syntaxOpacity = opacity;
    locker.unlock();
    emit opacityChanged(QStringLiteral("lang.%1.syntax").arg(langKey), opacity);
}

double ThemeManager::languageOpacity(const QString& langKey) const {
    QMutexLocker locker(&m_mutex);
    auto it = m_currentColors.languageSyntax.constFind(langKey);
    if (it != m_currentColors.languageSyntax.constEnd()) {
        return it.value().syntaxOpacity;
    }
    return 1.0;
}

void ThemeManager::setWindowOpacity(double opacity) {
    updateOpacity("window", opacity);
}

void ThemeManager::setDockOpacity(double opacity) {
    updateOpacity("dock", opacity);
}

void ThemeManager::setChatOpacity(double opacity) {
    updateOpacity("chat", opacity);
}

void ThemeManager::setEditorOpacity(double opacity) {
    updateOpacity("editor", opacity);
}

void ThemeManager::setMainWindow(QWidget* mainWindow) {
    m_mainWindow = mainWindow;
    qDebug() << "[ThemeManager] Main window set";
}

void ThemeManager::setWindowTransparencyEnabled(bool enabled) {
    qDebug() << "[ThemeManager] Window transparency enabled:" << enabled;
    m_transparencyEnabled = enabled;
    updateWindowTransparency();
    emit transparencySettingsChanged();
}

void ThemeManager::setAlwaysOnTop(bool enabled) {
    qDebug() << "[ThemeManager] Always on top:" << enabled;
    m_alwaysOnTop = enabled;
    
    if (m_mainWindow) {
        Qt::WindowFlags flags = m_mainWindow->windowFlags();
        if (enabled) {
            flags |= Qt::WindowStaysOnTopHint;
        } else {
            flags &= ~Qt::WindowStaysOnTopHint;
        }
        m_mainWindow->setWindowFlags(flags);
        m_mainWindow->show(); // Required after changing window flags
    }
    
    emit transparencySettingsChanged();
}

void ThemeManager::setClickThroughEnabled(bool enabled) {
    qDebug() << "[ThemeManager] Click-through enabled:" << enabled;
    m_clickThroughEnabled = enabled;
    
#ifdef Q_OS_WIN
    if (m_mainWindow) {
        // Windows-specific: WS_EX_TRANSPARENT allows click-through
        // This is an advanced feature that requires careful handling
        HWND hwnd = reinterpret_cast<HWND>(m_mainWindow->winId());
        if (enabled) {
            SetWindowLong(hwnd, GWL_EXSTYLE, 
                GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT | WS_EX_LAYERED);
        } else {
            SetWindowLong(hwnd, GWL_EXSTYLE, 
                GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
        }
    }
#endif
    
    emit transparencySettingsChanged();
}

void ThemeManager::applyThemeToWidget(QWidget* widget) {
    if (!widget) return;
    
    QString stylesheet = generateStylesheet();
    widget->setStyleSheet(stylesheet);
}

void ThemeManager::applyThemeToApplication() {
    qDebug() << "[ThemeManager] Applying theme to application...";
    auto startTime = std::chrono::steady_clock::now();
    
    QString stylesheet = generateStylesheet();
    qApp->setStyleSheet(stylesheet);
    
    // Apply window transparency
    updateWindowTransparency();
    
    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    qDebug() << "[ThemeManager] Theme applied to application in" << durationMs << "ms";
}

QString ThemeManager::generateStylesheet() const {
    QMutexLocker locker(&m_mutex);
    
    return QString(R"(
        /* ============================================================ */
        /* RawrXD Agentic IDE - Auto-Generated Theme Stylesheet         */
        /* Theme: %1                                                    */
        /* ============================================================ */
        
        /* Main Window */
        QMainWindow {
            background-color: %2;
            color: %3;
        }
        
        /* Central Widget */
        QMainWindow > QWidget {
            background-color: %2;
        }
        
        /* Dock Widgets */
        QDockWidget {
            background-color: %4;
            color: %3;
            border: 1px solid %5;
            titlebar-close-icon: url(none);
            titlebar-normal-icon: url(none);
        }
        
        QDockWidget::title {
            background-color: %4;
            color: %3;
            padding: 6px;
            text-align: left;
        }
        
        QDockWidget::close-button, QDockWidget::float-button {
            background: transparent;
            border: none;
            padding: 2px;
        }
        
        QDockWidget::close-button:hover, QDockWidget::float-button:hover {
            background: %6;
        }
        
        /* Toolbars */
        QToolBar {
            background-color: %7;
            border: none;
            spacing: 3px;
            padding: 2px;
        }
        
        QToolBar::separator {
            background-color: %5;
            width: 1px;
            margin: 4px 2px;
        }
        
        QToolButton {
            background-color: transparent;
            color: %8;
            border: 1px solid transparent;
            padding: 4px 8px;
            border-radius: 3px;
        }
        
        QToolButton:hover {
            background-color: %6;
            border: 1px solid %5;
        }
        
        QToolButton:pressed {
            background-color: %9;
        }
        
        QToolButton:checked {
            background-color: %6;
            border: 1px solid %10;
        }
        
        /* Menus */
        QMenuBar {
            background-color: %11;
            color: %12;
            border-bottom: 1px solid %5;
        }
        
        QMenuBar::item {
            background-color: transparent;
            padding: 4px 10px;
        }
        
        QMenuBar::item:selected {
            background-color: %6;
        }
        
        QMenuBar::item:pressed {
            background-color: %9;
        }
        
        QMenu {
            background-color: %11;
            color: %12;
            border: 1px solid %5;
            padding: 4px;
        }
        
        QMenu::item {
            padding: 6px 30px 6px 20px;
            border-radius: 3px;
        }
        
        QMenu::item:selected {
            background-color: %6;
        }
        
        QMenu::separator {
            height: 1px;
            background: %5;
            margin: 4px 10px;
        }
        
        QMenu::indicator {
            width: 13px;
            height: 13px;
            margin-left: 4px;
        }
        
        /* Status Bar */
        QStatusBar {
            background-color: %7;
            color: %3;
            border-top: 1px solid %5;
        }
        
        QStatusBar::item {
            border: none;
        }
        
        /* Scrollbars */
        QScrollBar:vertical {
            background-color: %4;
            width: 12px;
            border-radius: 6px;
            margin: 0;
        }
        
        QScrollBar::handle:vertical {
            background-color: %13;
            border-radius: 6px;
            min-height: 30px;
            margin: 2px;
        }
        
        QScrollBar::handle:vertical:hover {
            background-color: %14;
        }
        
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
        
        QScrollBar:horizontal {
            background-color: %4;
            height: 12px;
            border-radius: 6px;
            margin: 0;
        }
        
        QScrollBar::handle:horizontal {
            background-color: %13;
            border-radius: 6px;
            min-width: 30px;
            margin: 2px;
        }
        
        QScrollBar::handle:horizontal:hover {
            background-color: %14;
        }
        
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }
        
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
        }
        
        /* Tab Widget */
        QTabWidget::pane {
            background-color: %4;
            border: 1px solid %5;
            border-top: none;
        }
        
        QTabBar::tab {
            background-color: %7;
            color: %3;
            padding: 8px 16px;
            border: 1px solid %5;
            border-bottom: none;
            margin-right: 2px;
        }
        
        QTabBar::tab:selected {
            background-color: %4;
            border-bottom: 2px solid %10;
        }
        
        QTabBar::tab:hover:!selected {
            background-color: %6;
        }
        
        QTabBar::close-button {
            image: url(:/icons/close.png);
            subcontrol-position: right;
        }
        
        QTabBar::close-button:hover {
            background-color: %9;
            border-radius: 2px;
        }
        
        /* Text Inputs */
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: %15;
            color: %16;
            border: 1px solid %5;
            border-radius: 3px;
            padding: 4px;
            selection-background-color: %17;
        }
        
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid %10;
        }
        
        /* Buttons */
        QPushButton {
            background-color: %18;
            color: %8;
            border: 1px solid %5;
            padding: 6px 16px;
            border-radius: 3px;
            min-width: 60px;
        }
        
        QPushButton:hover {
            background-color: %6;
        }
        
        QPushButton:pressed {
            background-color: %9;
        }
        
        QPushButton:disabled {
            background-color: %4;
            color: %13;
        }
        
        /* ComboBox */
        QComboBox {
            background-color: %18;
            color: %8;
            border: 1px solid %5;
            padding: 4px 8px;
            border-radius: 3px;
            min-width: 80px;
        }
        
        QComboBox:hover {
            background-color: %6;
        }
        
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        
        QComboBox::down-arrow {
            image: url(:/icons/arrow-down.png);
            width: 12px;
            height: 12px;
        }
        
        QComboBox QAbstractItemView {
            background-color: %11;
            color: %12;
            border: 1px solid %5;
            selection-background-color: %6;
        }
        
        /* Sliders */
        QSlider::groove:horizontal {
            background: %4;
            height: 6px;
            border-radius: 3px;
        }
        
        QSlider::handle:horizontal {
            background: %10;
            width: 16px;
            height: 16px;
            margin: -5px 0;
            border-radius: 8px;
        }
        
        QSlider::handle:horizontal:hover {
            background: %6;
        }
        
        /* SpinBox */
        QSpinBox, QDoubleSpinBox {
            background-color: %18;
            color: %8;
            border: 1px solid %5;
            padding: 4px;
            border-radius: 3px;
        }
        
        QSpinBox::up-button, QDoubleSpinBox::up-button,
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            background-color: %6;
            border: none;
            width: 16px;
        }
        
        QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
        QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
            background-color: %9;
        }
        
        /* CheckBox */
        QCheckBox {
            color: %3;
            spacing: 8px;
        }
        
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid %5;
            border-radius: 3px;
            background-color: %4;
        }
        
        QCheckBox::indicator:checked {
            background-color: %10;
            border-color: %10;
        }
        
        QCheckBox::indicator:hover {
            border-color: %10;
        }
        
        /* GroupBox */
        QGroupBox {
            color: %3;
            border: 1px solid %5;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 8px;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            color: %3;
        }
        
        /* Progress Bar */
        QProgressBar {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 3px;
            text-align: center;
            color: %3;
        }
        
        QProgressBar::chunk {
            background-color: %10;
            border-radius: 2px;
        }
        
        /* Tree View / List View */
        QTreeView, QListView, QTableView {
            background-color: %4;
            color: %3;
            border: 1px solid %5;
            alternate-background-color: %7;
        }
        
        QTreeView::item, QListView::item, QTableView::item {
            padding: 4px;
        }
        
        QTreeView::item:selected, QListView::item:selected, QTableView::item:selected {
            background-color: %17;
        }
        
        QTreeView::item:hover, QListView::item:hover, QTableView::item:hover {
            background-color: %6;
        }
        
        QHeaderView::section {
            background-color: %7;
            color: %3;
            padding: 4px 8px;
            border: 1px solid %5;
        }
        
        /* Tooltips */
        QToolTip {
            background-color: %11;
            color: %12;
            border: 1px solid %5;
            padding: 4px;
        }
        
        /* Label */
        QLabel {
            color: %3;
        }
    )")
    .arg(m_currentThemeName)
    .arg(m_currentColors.windowBackground.name())       // %2
    .arg(m_currentColors.windowForeground.name())       // %3
    .arg(m_currentColors.dockBackground.name())         // %4
    .arg(m_currentColors.dockBorder.name())             // %5
    .arg(m_currentColors.buttonHover.name())            // %6
    .arg(m_currentColors.toolbarBackground.name())      // %7
    .arg(m_currentColors.buttonForeground.name())       // %8
    .arg(m_currentColors.buttonPressed.name())          // %9
    .arg(m_currentColors.editorSelection.name())        // %10 (accent color)
    .arg(m_currentColors.menuBackground.name())         // %11
    .arg(m_currentColors.menuForeground.name())         // %12
    .arg(m_currentColors.editorLineNumbers.name())      // %13
    .arg(m_currentColors.editorLineNumbers.lighter(120).name()) // %14
    .arg(m_currentColors.editorBackground.name())       // %15
    .arg(m_currentColors.editorForeground.name())       // %16
    .arg(m_currentColors.editorSelection.name())        // %17
    .arg(m_currentColors.buttonBackground.name());      // %18
}

void ThemeManager::updateWindowTransparency() {
    if (!m_mainWindow) return;
    
    if (m_transparencyEnabled) {
        // Set window opacity
        m_mainWindow->setWindowOpacity(m_currentColors.windowOpacity);
        
        // Apply opacity to dock widgets
        for (auto* dock : m_mainWindow->findChildren<QDockWidget*>()) {
            if (dock && dock->widget()) {
                dock->setWindowOpacity(m_currentColors.dockOpacity);
            }
        }
    } else {
        // Reset to full opacity
        m_mainWindow->setWindowOpacity(1.0);
        for (auto* dock : m_mainWindow->findChildren<QDockWidget*>()) {
            if (dock) {
                dock->setWindowOpacity(1.0);
            }
        }
    }
}

QString ThemeManager::getThemeDirectory() const {
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appDataPath + "/themes";
}

void ThemeManager::loadThemesFromDisk() {
    QString themeDir = getThemeDirectory();
    QDir dir(themeDir);
    
    if (!dir.exists()) {
        dir.mkpath(themeDir);
        return;
    }
    
    QStringList filters;
    filters << "*.json";
    QStringList themeFiles = dir.entryList(filters, QDir::Files);
    
    for (const QString& fileName : themeFiles) {
        QString filePath = themeDir + "/" + fileName;
        QFile file(filePath);
        
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject themeObj = doc.object();
                QString themeName = themeObj["name"].toString();
                
                if (!themeName.isEmpty()) {
                    ThemeColors colors = ThemeColors::fromJson(themeObj);
                    m_themes[themeName] = colors;
                    qDebug() << "[ThemeManager] Loaded custom theme:" << themeName;
                }
            }
            file.close();
        }
    }
}

void ThemeManager::saveThemesToDisk() {
    QString themeDir = getThemeDirectory();
    QDir dir;
    dir.mkpath(themeDir);
    
    // Save custom themes only (not built-in ones)
    QStringList builtInThemes = {"Dark", "Light", "High Contrast", "Glass", "Monokai"};
    
    for (auto it = m_themes.begin(); it != m_themes.end(); ++it) {
        if (builtInThemes.contains(it.key())) {
            continue; // Skip built-in themes
        }
        
        QString filePath = themeDir + "/" + it.key() + ".json";
        QFile file(filePath);
        
        if (file.open(QIODevice::WriteOnly)) {
            QJsonObject themeObj = it.value().toJson();
            themeObj["name"] = it.key();
            
            QJsonDocument doc(themeObj);
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();
            
            qDebug() << "[ThemeManager] Saved custom theme:" << it.key();
        }
    }
}

bool ThemeManager::importTheme(const QString& filePath, QString& errorMsg) {
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        errorMsg = QString("Cannot open file: %1").arg(file.errorString());
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (doc.isNull()) {
        errorMsg = QString("Invalid JSON: %1").arg(parseError.errorString());
        return false;
    }
    
    if (!doc.isObject()) {
        errorMsg = "Theme file must contain a JSON object";
        return false;
    }
    
    QJsonObject themeObj = doc.object();
    QString themeName = themeObj["name"].toString();
    
    if (themeName.isEmpty()) {
        // Use filename as theme name
        QFileInfo fileInfo(filePath);
        themeName = fileInfo.baseName();
    }
    
    ThemeColors colors = ThemeColors::fromJson(themeObj);
    
    {
        QMutexLocker locker(&m_mutex);
        m_themes[themeName] = colors;
    }
    
    saveThemesToDisk();
    
    qDebug() << "[ThemeManager] Imported theme:" << themeName;
    return true;
}

bool ThemeManager::exportTheme(const QString& filePath, QString& errorMsg) const {
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        errorMsg = QString("Cannot write file: %1").arg(file.errorString());
        return false;
    }
    
    QJsonObject themeObj;
    {
        QMutexLocker locker(&m_mutex);
        themeObj = m_currentColors.toJson();
        themeObj["name"] = m_currentThemeName;
    }
    
    QJsonDocument doc(themeObj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    qDebug() << "[ThemeManager] Exported theme to:" << filePath;
    return true;
}

} // namespace RawrXD
