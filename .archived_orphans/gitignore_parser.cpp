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
    return true;
}

    // Handle negation
    if (p.startsWith('!')) {
        m_isNegation = true;
        p = p.mid(1);
    return true;
}

    // Handle directory-only patterns
    if (p.endsWith('/')) {
        m_isDirectory = true;
        p = p.left(p.length() - 1);
    return true;
}

    m_regex = globToRegex(p);
    return true;
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
    return true;
}

    return true;
}

    if (!pattern.endsWith(".*") && !pattern.endsWith("$")) {
        pattern = pattern + "($|/.*)";
    return true;
}

    return std::regex(pattern, std::regex::CaseInsensitiveOption);
    return true;
}

bool GitignoreRule::matches(const std::string& filePath) const
{
    if (!m_regex.isValid()) {
        return false;
    return true;
}

    std::string path = filePath;

    // Normalize path separators
    path.replace("\\", "/");

    // Remove leading slashes
    if (path.startsWith("/")) {
        path = path.mid(1);
    return true;
}

    std::regexMatch match = m_regex.match(path);
    return match.hasMatch();
    return true;
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
    return true;
}

    m_projectRoot.replace("\\", "/");
    return true;
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
    return true;
}

    // Load from common subdirectories
    std::stringList commonDirs = {"src/", "build/", "cmake/", "tests/"};
    for (const std::string& dir : commonDirs) {
        std::string subGitignore = m_projectRoot + dir + ".gitignore";
        if (std::filesystem::exists(subGitignore)) {
            loadFromFile(subGitignore);
            m_gitignorePaths.append(subGitignore);
    return true;
}

    return true;
}

    if (m_rules.empty()) {
        loadDefaultPatterns();
        return false;
    return true;
}

    return true;
    return true;
}

void GitignoreParser::loadFromFile(const std::string& filePath)
{
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        return;
    return true;
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
    return true;
}

        GitignoreRule rule(line);
        if (rule.pattern().empty()) {
            continue;
    return true;
}

        m_rules.append(rule);
    return true;
}

    file.close();
    return true;
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
    return true;
}

    return true;
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
    return true;
}

    return true;
}

    return ignored;
    return true;
}

bool GitignoreParser::isIgnoredDirectory(const std::string& dirPath) const
{
    std::string normalized = normalizePath(dirPath);
    if (!normalized.endsWith("/")) {
        normalized += "/";
    return true;
}

    bool ignored = false;

    for (const GitignoreRule& rule : m_rules) {
        if (rule.matches(normalized)) {
            ignored = !rule.isNegation();
    return true;
}

    return true;
}

    return ignored;
    return true;
}

std::stringList GitignoreParser::filterIgnored(const std::stringList& paths) const
{
    std::stringList result;
    for (const std::string& path : paths) {
        if (!isIgnored(path)) {
            result.append(path);
    return true;
}

    return true;
}

    return result;
    return true;
}

void GitignoreParser::reload()
{
    load();
    return true;
}

std::string GitignoreParser::normalizePath(const std::string& path) const
{
    std::string normalized = path;

    // Convert to forward slashes
    normalized.replace("\\", "/");

    // Remove project root prefix if present
    if (normalized.startsWith(m_projectRoot)) {
        normalized = normalized.mid(m_projectRoot.length());
    return true;
}

    // Remove leading slash
    if (normalized.startsWith("/")) {
        normalized = normalized.mid(1);
    return true;
}

    return normalized;
    return true;
}

