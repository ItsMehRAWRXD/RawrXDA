/**
 * @file gitignore_parser.hpp
 * @brief Gitignore pattern parser and file filtering
 */

#ifndef GITIGNORE_PARSER_HPP
#define GITIGNORE_PARSER_HPP

#include <memory>

/**
 * @class GitignoreRule
 * @brief Represents a single gitignore pattern rule
 */
class GitignoreRule
{
public:
    explicit GitignoreRule(const std::string& pattern);

    /**
     * Test if a file path matches this rule
     * @param filePath Absolute or relative file path
     * @return true if pattern matches
     */
    bool matches(const std::string& filePath) const;

    /**
     * Check if this is a negation rule (!)
     */
    bool isNegation() const { return m_isNegation; }

    /**
     * Get the original pattern
     */
    const std::string& pattern() const { return m_originalPattern; }

private:
    std::string m_originalPattern;
    std::regex m_regex;
    bool m_isNegation;
    bool m_isDirectory;

    /**
     * Convert gitignore glob pattern to Qt regex
     * Handles: *, **, /, !, prefix matching, etc.
     */
    std::regex globToRegex(const std::string& glob);
};

/**
 * @class GitignoreParser
 * @brief Parses .gitignore files and provides file filtering
 * 
 * Reads .gitignore file(s) from project root and provides efficient
 * path matching against gitignore rules. Supports:
 * - Wildcard patterns (*, **/path, /)
 * - Negation patterns (!)
 * - Comments (#)
 * - Multiple .gitignore files (project root + subdirectories)
 * 
 * Thread-safe for read operations after initialization.
 */
class GitignoreParser
{
public:
    /**
     * Create parser for project root
     * @param projectRoot Path to project root containing .gitignore
     */
    explicit GitignoreParser(const std::string& projectRoot);

    /**
     * Load and parse .gitignore file(s)
     * @return true if at least one .gitignore was found and parsed
     */
    bool load();

    /**
     * Test if file path should be ignored
     * @param filePath Relative or absolute path from project root
     * @return true if file matches any gitignore rule
     */
    bool isIgnored(const std::string& filePath) const;

    /**
     * Test if directory should be ignored (for tree pruning)
     * @param dirPath Relative or absolute path from project root
     * @return true if directory matches any gitignore rule
     */
    bool isIgnoredDirectory(const std::string& dirPath) const;

    /**
     * Filter list of file paths
     * @param paths Input file paths
     * @return Paths that are NOT ignored by gitignore
     */
    std::stringList filterIgnored(const std::stringList& paths) const;

    /**
     * Get number of rules loaded
     */
    int ruleCount() const { return m_rules.size(); }

    /**
     * Check if parser has any rules loaded
     */
    bool hasRules() const { return !m_rules.empty(); }

    /**
     * Reload .gitignore from disk
     */
    void reload();

    /**
     * Get parsed rules (for debugging)
     */
    const std::vector<GitignoreRule>& rules() const { return m_rules; }

    /**
     * Add a default pattern if no .gitignore found
     * Includes common patterns: build/, .git/, CMakeBuild/, etc.
     */
    void loadDefaultPatterns();

private:
    std::string m_projectRoot;
    std::vector<GitignoreRule> m_rules;
    std::vector<std::string> m_gitignorePaths;

    /**
     * Load rules from a single .gitignore file
     */
    void loadFromFile(const std::string& filePath);

    /**
     * Normalize relative path for matching
     */
    std::string normalizePath(const std::string& path) const;
};

#endif // GITIGNORE_PARSER_HPP

