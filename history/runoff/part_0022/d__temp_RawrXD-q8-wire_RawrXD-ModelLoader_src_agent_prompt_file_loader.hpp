/**
 * @file prompt_file_loader.hpp
 * @brief Load and manage .rawrxd prompt files for project-specific AI behavior
 * 
 * Supports:
 * - .rawrxd files in workspace root (project-level prompts)
 * - .rawrxd/ directory for multiple prompt templates
 * - Inheritance and composition of prompts
 * - Variable substitution ({project}, {language}, {user})
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

/**
 * @struct PromptTemplate
 * @brief A loaded prompt template with metadata
 */
struct PromptTemplate {
    std::string name;
    std::string content;
    std::string description;
    std::vector<std::string> tags;
    std::string filePath;
    bool isActive{false};
};

/**
 * @class PromptFileLoader
 * @brief Manages project-specific AI prompt customization
 */
class PromptFileLoader {
public:
    explicit PromptFileLoader();
    ~PromptFileLoader();

    /**
     * @brief Load prompts from workspace
     * @param workspacePath Root path of workspace
     */
    void loadWorkspacePrompts(const std::string& workspacePath);

    /**
     * @brief Get combined system prompt for this workspace
     */
    std::string getSystemPrompt() const;

    /**
     * @brief Get specific prompt template by name
     */
    PromptTemplate getPrompt(const std::string& name) const;

    /**
     * @brief Get all loaded prompt templates
     */
    std::vector<PromptTemplate> getAllPrompts() const;

    /**
     * @brief Set variable for substitution
     */
    void setVariable(const std::string& name, const std::string& value);

    /**
     * @brief Enable/disable hot-reloading of prompt files
     */
    void setHotReloadEnabled(bool enabled);

    /**
     * @brief Check if workspace has custom prompts
     */
    bool hasCustomPrompts() const { return !m_templates.empty(); }

    /**
     * @brief Get workspace path
     */
    std::string workspacePath() const { return m_workspacePath; }

    // Callbacks
    using PromptsLoadedCallback = std::function<void(int count)>;
    using PromptChangedCallback = std::function<void(const std::string& name)>;
    using PromptErrorCallback = std::function<void(const std::string& error)>;

    void setPromptsLoadedCallback(PromptsLoadedCallback cb) { m_onPromptsLoaded = std::move(cb); }
    void setPromptChangedCallback(PromptChangedCallback cb) { m_onPromptChanged = std::move(cb); }
    void setPromptErrorCallback(PromptErrorCallback cb) { m_onPromptError = std::move(cb); }

private:
    void onFileChanged(const std::string& path);
    PromptTemplate parsePromptFile(const std::string& filePath);
    std::string substituteVariables(const std::string& content) const;
    void loadDefaults();

    std::string m_workspacePath;
    std::map<std::string, PromptTemplate> m_templates;
    std::map<std::string, std::string> m_variables;
    bool m_hotReloadEnabled = true;

    PromptsLoadedCallback m_onPromptsLoaded;
    PromptChangedCallback m_onPromptChanged;
    PromptErrorCallback m_onPromptError;
};
