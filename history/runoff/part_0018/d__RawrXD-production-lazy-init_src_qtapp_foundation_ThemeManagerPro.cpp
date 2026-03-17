#include "ThemeManagerPro.h"

ThemeManagerPro::ThemeManagerPro(QObject* parent) : QObject(parent) {}
ThemeManagerPro::~ThemeManagerPro() {}

bool ThemeManagerPro::registerTheme(const Theme& theme) {
    QMutexLocker locker(&m_mutex);
    if (theme.name.isEmpty()) return false;
    m_themes[theme.name] = theme;
    if (m_active.isEmpty()) m_active = theme.name;
    return true;
}

bool ThemeManagerPro::removeTheme(const QString& name) {
    QMutexLocker locker(&m_mutex);
    if (!m_themes.contains(name)) return false;
    m_themes.remove(name);
    if (m_active == name && !m_themes.isEmpty()) m_active = m_themes.first().name;
    return true;
}

bool ThemeManagerPro::setActiveTheme(const QString& name) {
    QMutexLocker locker(&m_mutex);
    if (!m_themes.contains(name)) return false;
    m_active = name;
    emit themeChanged(m_themes[name]);
    return true;
}

ThemeManagerPro::Theme ThemeManagerPro::activeTheme() const {
    QMutexLocker locker(&m_mutex);
    return m_themes.value(m_active);
}

QStringList ThemeManagerPro::listThemes() const {
    QMutexLocker locker(&m_mutex);
    return m_themes.keys();
}

QPalette ThemeManagerPro::toPalette(const Theme& theme) const {
    QPalette p;
    p.setColor(QPalette::Window, theme.background);
    p.setColor(QPalette::WindowText, theme.foreground);
    p.setColor(QPalette::Base, theme.background.darker(theme.dark ? 120 : 90));
    p.setColor(QPalette::Text, theme.foreground);
    p.setColor(QPalette::Highlight, theme.accent);
    p.setColor(QPalette::HighlightedText, theme.dark ? Qt::white : Qt::black);
    p.setColor(QPalette::ToolTipBase, theme.background);
    p.setColor(QPalette::ToolTipText, theme.foreground);
    return p;
}
