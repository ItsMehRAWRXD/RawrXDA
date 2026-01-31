/**
 * @file gitignore_parser.cpp
 * @brief Implementation of gitignore pattern parser
 */

#include "gitignore_parser.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
// GitignoreRule Implementation
// ─────────────────────────────────────────────────────────────────────

GitignoreRule::GitignoreRule(const std::string& pattern)
    : m_originalPattern(pattern)
    , m_isNegation(false)
    , m_isDirectory(false)
{
    std::string p = pattern.trimmed();

    // Skip empty lines and comments
    if (p.empty() || p.startsWith('#')) {
        m_regex = std::regex(); // Invalid regex
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

std::regex GitignoreRule::globToRegex(const std::string& glob)
{
    std::string pattern = glob;

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

    return std::regex(pattern, std::regex::CaseInsensitiveOption);
}

bool GitignoreRule::matches(const std::string& filePath) const
{
    if (!m_regex.isValid()) {
        return false;
    }

    std::string path = filePath;

    // Normalize path separators
    path.replace("\\", "/");

    // Remove leading slashes
    if (path.startsWith("/")) {
        path = path.mid(1);
    }

    std::regexMatch match = m_regex.match(path);
    return match.hasMatch();
}

// ─────────────────────────────────────────────────────────────────────
// GitignoreParser Implementation
// ─────────────────────────────────────────────────────────────────────

GitignoreParser::GitignoreParser(const std::string& projectRoot)
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
    std::string gitignorePath = m_projectRoot + ".gitignore";
    if (std::filesystem::exists(gitignorePath)) {
        loadFromFile(gitignorePath);
        m_gitignorePaths.append(gitignorePath);
    }

    // Load from common subdirectories
    std::stringList commonDirs = {"src/", "build/", "cmake/", "tests/"};
    for (const std::string& dir : commonDirs) {
        std::string subGitignore = m_projectRoot + dir + ".gitignore";
        if (std::filesystem::exists(subGitignore)) {
            loadFromFile(subGitignore);
            m_gitignorePaths.append(subGitignore);
        }
    }

    if (m_rules.empty()) {
        loadDefaultPatterns();
        return false;
    }

    return true;
}

void GitignoreParser::loadFromFile(const std::string& filePath)
{
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        return;
    }

    std::stringstream stream(&file);
    int lineNum = 0;
    while (!stream.atEnd()) {
        std::string line = stream.readLine();
        lineNum++;

        // Trim whitespace
        line = line.trimmed();

        // Skip empty lines and full-line comments
        if (line.empty() || line.startsWith('#')) {
            continue;
        }

        GitignoreRule rule(line);
        if (rule.pattern().empty()) {
            continue;
        }

        m_rules.append(rule);
    }

    file.close();
}

void GitignoreParser::loadDefaultPatterns()
{
    std::stringList defaults = {
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

    for (const std::string& pattern : defaults) {
        m_rules.append(GitignoreRule(pattern));
    }

}

bool GitignoreParser::isIgnored(const std::string& filePath) const
{
    std::string normalized = normalizePath(filePath);

    // Check all rules in order
    // Negation rules (!) disable previous matches
    bool ignored = false;

    for (const GitignoreRule& rule : m_rules) {
        if (rule.matches(normalized)) {
            ignored = !rule.isNegation();
        }
    }

    return ignored;
}

bool GitignoreParser::isIgnoredDirectory(const std::string& dirPath) const
{
    std::string normalized = normalizePath(dirPath);
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

std::stringList GitignoreParser::filterIgnored(const std::stringList& paths) const
{
    std::stringList result;
    for (const std::string& path : paths) {
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

std::string GitignoreParser::normalizePath(const std::string& path) const
{
    std::string normalized = path;

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

