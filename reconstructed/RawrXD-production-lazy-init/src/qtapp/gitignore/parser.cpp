/**
 * @file gitignore_parser.cpp
 * @brief Implementation of gitignore pattern parser
 */

#include "gitignore_parser.hpp"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
// GitignoreRule Implementation
// ─────────────────────────────────────────────────────────────────────

GitignoreRule::GitignoreRule(const QString& pattern)
    : m_originalPattern(pattern)
    , m_isNegation(false)
    , m_isDirectory(false)
{
    QString p = pattern.trimmed();

    // Skip empty lines and comments
    if (p.isEmpty() || p.startsWith('#')) {
        m_regex = QRegularExpression(); // Invalid regex
        return;
    }

    // Handle negation
    if (p.startsWith('!')) {
        m_isNegation = true;
        p = p.mid(1);
    }

    // Handle directory-only patterns
    if (p.endsWith('/')) {
        m_isDirectory = true;
        p = p.left(p.length() - 1);
    }

    m_regex = globToRegex(p);
}

QRegularExpression GitignoreRule::globToRegex(const QString& glob)
{
    QString pattern = glob;

    // Escape special regex chars except *, ?, [, ]
    pattern.replace("\\", "\\\\");
    pattern.replace(".", "\\.");
    pattern.replace("^", "\\^");
    pattern.replace("$", "\\$");
    pattern.replace("(", "\\(");
    pattern.replace(")", "\\)");
    pattern.replace("+", "\\+");
    pattern.replace("|", "\\|");
    pattern.replace("{", "\\{");
    pattern.replace("}", "\\}");

    // Handle ** (match any number of directories)
    pattern.replace("**", "\x01"); // Placeholder

    // Handle * (match anything except /)
    pattern.replace("*", "[^/]*");

    // Restore ** as match-all
    pattern.replace("\x01", ".*");

    // Anchor pattern
    if (!pattern.startsWith(".*")) {
        if (pattern.startsWith("/")) {
            pattern = "^" + pattern.mid(1);
        } else {
            pattern = "(^|/)" + pattern;
        }
    }

    if (!pattern.endsWith(".*") && !pattern.endsWith("$")) {
        pattern = pattern + "($|/.*)";
    }

    return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
}

bool GitignoreRule::matches(const QString& filePath) const
{
    if (!m_regex.isValid()) {
        return false;
    }

    QString path = filePath;

    // Normalize path separators
    path.replace("\\", "/");

    // Remove leading slashes
    if (path.startsWith("/")) {
        path = path.mid(1);
    }

    QRegularExpressionMatch match = m_regex.match(path);
    return match.hasMatch();
}

// ─────────────────────────────────────────────────────────────────────
// GitignoreParser Implementation
// ─────────────────────────────────────────────────────────────────────

GitignoreParser::GitignoreParser(const QString& projectRoot)
    : m_projectRoot(projectRoot)
{
    // Normalize root path
    if (!m_projectRoot.endsWith("/") && !m_projectRoot.endsWith("\\")) {
        m_projectRoot += "/";
    }
    m_projectRoot.replace("\\", "/");
}

bool GitignoreParser::load()
{
    m_rules.clear();
    m_gitignorePaths.clear();

    // Load from project root
    QString gitignorePath = m_projectRoot + ".gitignore";
    if (QFile::exists(gitignorePath)) {
        loadFromFile(gitignorePath);
        m_gitignorePaths.append(gitignorePath);
    }

    // Load from common subdirectories
    QStringList commonDirs = {"src/", "build/", "cmake/", "tests/"};
    for (const QString& dir : commonDirs) {
        QString subGitignore = m_projectRoot + dir + ".gitignore";
        if (QFile::exists(subGitignore)) {
            loadFromFile(subGitignore);
            m_gitignorePaths.append(subGitignore);
        }
    }

    if (m_rules.isEmpty()) {
        qWarning() << "[GitignoreParser] No .gitignore found, loading defaults";
        loadDefaultPatterns();
        return false;
    }

    qInfo() << "[GitignoreParser] Loaded" << m_rules.size() << "rules from" << m_gitignorePaths.size() << "files";
    return true;
}

void GitignoreParser::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[GitignoreParser] Could not open:" << filePath;
        return;
    }

    QTextStream stream(&file);
    int lineNum = 0;
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        lineNum++;

        // Trim whitespace
        line = line.trimmed();

        // Skip empty lines and full-line comments
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        GitignoreRule rule(line);
        if (rule.pattern().isEmpty()) {
            continue;
        }

        m_rules.append(rule);
        qDebug() << "[GitignoreParser]" << filePath << "line" << lineNum << ":" << line;
    }

    file.close();
}

void GitignoreParser::loadDefaultPatterns()
{
    QStringList defaults = {
        "build/",
        "cmake-build-*/",
        ".git/",
        ".gitignore",
        "CMakeBuild/",
        "CMakeLists.txt.user",
        ".idea/",
        ".vscode/",
        "*.o",
        "*.a",
        "*.lib",
        "*.exe",
        "*.dll",
        "*.so",
        ".DS_Store",
        "Thumbs.db"
    };

    for (const QString& pattern : defaults) {
        m_rules.append(GitignoreRule(pattern));
    }

    qInfo() << "[GitignoreParser] Loaded" << defaults.size() << "default patterns";
}

bool GitignoreParser::isIgnored(const QString& filePath) const
{
    QString normalized = normalizePath(filePath);

    // Check all rules in order
    // Negation rules (!) disable previous matches
    bool ignored = false;

    for (const GitignoreRule& rule : m_rules) {
        if (rule.matches(normalized)) {
            ignored = !rule.isNegation();
            qDebug() << "[GitignoreParser] File:" << normalized << "Rule:" << rule.pattern() << "Match:" << ignored;
        }
    }

    return ignored;
}

bool GitignoreParser::isIgnoredDirectory(const QString& dirPath) const
{
    QString normalized = normalizePath(dirPath);
    if (!normalized.endsWith("/")) {
        normalized += "/";
    }

    bool ignored = false;

    for (const GitignoreRule& rule : m_rules) {
        if (rule.matches(normalized)) {
            ignored = !rule.isNegation();
        }
    }

    return ignored;
}

QStringList GitignoreParser::filterIgnored(const QStringList& paths) const
{
    QStringList result;
    for (const QString& path : paths) {
        if (!isIgnored(path)) {
            result.append(path);
        }
    }
    return result;
}

void GitignoreParser::reload()
{
    load();
}

QString GitignoreParser::normalizePath(const QString& path) const
{
    QString normalized = path;

    // Convert to forward slashes
    normalized.replace("\\", "/");

    // Remove project root prefix if present
    if (normalized.startsWith(m_projectRoot)) {
        normalized = normalized.mid(m_projectRoot.length());
    }

    // Remove leading slash
    if (normalized.startsWith("/")) {
        normalized = normalized.mid(1);
    }

    return normalized;
}
