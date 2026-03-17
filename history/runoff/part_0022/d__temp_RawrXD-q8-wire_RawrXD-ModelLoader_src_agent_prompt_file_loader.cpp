/**
 * @file prompt_file_loader.cpp
 * @brief Implementation of .rawrxd prompt file loading
 */

#include "prompt_file_loader.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

inline std::string get_iso_date() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d");
    return ss.str();
}

inline std::string get_time() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

PromptFileLoader::PromptFileLoader() {
    // Set default variables
    m_variables["date"] = get_iso_date();
    m_variables["time"] = get_time();
    
    loadDefaults();
}

PromptFileLoader::~PromptFileLoader() = default;

void PromptFileLoader::loadWorkspacePrompts(const std::string& workspacePath) {
    m_workspacePath = workspacePath;
    m_templates.clear();
    
    fs::path workspaceFsPath(workspacePath);
    m_variables["project"] = workspaceFsPath.filename().string();
    m_variables["workspace"] = workspacePath;
    
    // Check for .rawrxd file (single file format)
    fs::path singleFile = workspaceFsPath / ".rawrxd";
    if (fs::exists(singleFile) && !fs::is_directory(singleFile)) {
        PromptTemplate tpl = parsePromptFile(singleFile.string());
        tpl.name = "system";
        tpl.isActive = true;
        m_templates["system"] = tpl;
    }
    
    // Check for .rawrxd/ directory
    fs::path promptDir = workspaceFsPath / ".rawrxd";
    if (fs::exists(promptDir) && fs::is_directory(promptDir)) {
        for (const auto& entry : fs::directory_iterator(promptDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".md" || ext == ".txt" || ext == ".rawrxd") {
                    PromptTemplate tpl = parsePromptFile(entry.path().string());
                    tpl.name = entry.path().stem().string();
                    
                    if (tpl.name == "system") {
                        tpl.isActive = true;
                    }
                    
                    m_templates[tpl.name] = tpl;
                }
            }
        }
    }
    
    if (m_templates.empty()) {
        loadDefaults();
    }
    
    if (m_onPromptsLoaded) {
        m_onPromptsLoaded(static_cast<int>(m_templates.size()));
    }
}

std::string PromptFileLoader::getSystemPrompt() const {
    std::string combined;
    
    std::vector<std::string> order = {"system", "style", "tools"};
    
    for (const std::string& name : order) {
        auto it = m_templates.find(name);
        if (it != m_templates.end() && it->second.isActive) {
            combined += it->second.content + "\n\n";
        }
    }
    
    for (auto const& [name, tpl] : m_templates) {
        bool inOrder = std::find(order.begin(), order.end(), name) != order.end();
        if (!inOrder && tpl.isActive) {
            combined += tpl.content + "\n\n";
        }
    }
    
    // trim
    if (!combined.empty()) {
        combined.erase(combined.find_last_not_of(" \n\r\t") + 1);
    }
    
    return substituteVariables(combined);
}

PromptTemplate PromptFileLoader::getPrompt(const std::string& name) const {
    auto it = m_templates.find(name);
    if (it != m_templates.end()) return it->second;
    return {};
}

std::vector<PromptTemplate> PromptFileLoader::getAllPrompts() const {
    std::vector<PromptTemplate> result;
    for (auto const& [name, tpl] : m_templates) {
        result.push_back(tpl);
    }
    return result;
}

void PromptFileLoader::setVariable(const std::string& name, const std::string& value) {
    m_variables[name] = value;
}

void PromptFileLoader::setHotReloadEnabled(bool enabled) {
    m_hotReloadEnabled = enabled;
}

void PromptFileLoader::onFileChanged(const std::string& path) {
    for (auto& [name, tpl] : m_templates) {
        if (tpl.filePath == path) {
            PromptTemplate newTpl = parsePromptFile(path);
            newTpl.name = tpl.name;
            newTpl.isActive = tpl.isActive;
            m_templates[name] = newTpl;
            
            if (m_onPromptChanged) m_onPromptChanged(name);
            return;
        }
    }
}

PromptTemplate PromptFileLoader::parsePromptFile(const std::string& filePath) {
    PromptTemplate tpl;
    tpl.filePath = filePath;
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return tpl;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    std::regex frontmatterRegex(R"(^---\s*\n([\s\S]*?)\n---\s*\n)", std::regex::multiline);
    std::smatch match;
    
    if (std::regex_search(content, match, frontmatterRegex)) {
        std::string yaml = match[1].str();
        
        std::regex descRe(R"(description:\s*(.+))");
        std::smatch descMatch;
        if (std::regex_search(yaml, descMatch, descRe)) {
            tpl.description = descMatch[1].str();
            // trim
            tpl.description.erase(tpl.description.find_last_not_of(" \n\r\t") + 1);
        }
        
        std::regex tagsRe(R"(tags:\s*\[(.*?)\])");
        std::smatch tagsMatch;
        if (std::regex_search(yaml, tagsMatch, tagsRe)) {
            std::string tagsStr = tagsMatch[1].str();
            std::stringstream ss(tagsStr);
            std::string tag;
            while (std::getline(ss, tag, ',')) {
                // simple trim and cleanup
                tag.erase(0, tag.find_first_not_of(" \t\"'"));
                tag.erase(tag.find_last_not_of(" \t\"'") + 1);
                if (!tag.empty()) tpl.tags.push_back(tag);
            }
        }
        
        content = match.suffix().str();
    }
    
    tpl.content = content;
    // trim
    if (!tpl.content.empty()) {
        tpl.content.erase(tpl.content.find_last_not_of(" \n\r\t") + 1);
        tpl.content.erase(0, tpl.content.find_first_not_of(" \n\r\t"));
    }
    
    return tpl;
}

std::string PromptFileLoader::substituteVariables(const std::string& content) const
{
    std::string result = content;
    
    for (auto const& [key, value] : m_variables) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    
    return result;
}

void PromptFileLoader::loadDefaults() {
    PromptTemplate sys;
    sys.name = "system";
    sys.isActive = true;
    sys.content = "You are an expert AI coding assistant. You have access to tools and must provide high-quality responses.";
    m_templates["system"] = sys;
}
    return tpl;
}

std::string PromptFileLoader::substituteVariables(const std::string& content) const
{
    std::string result = content;
    
    for (auto const& [key, value] : m_variables) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    
    return result;
}

void PromptFileLoader::loadDefaults()
{
    // Default system prompt for RawrXD
    PromptTemplate system;
    system.name = "system";
    system.isActive = true;
    system.description = "Default RawrXD system prompt";
    system.content = R"(You are RawrXD, an expert AI coding assistant built into the RawrXD IDE.

Your capabilities include:
- Writing, analyzing, and debugging code across all major languages
- Understanding large codebases through semantic search
- Making multi-file edits atomically
- Running terminal commands and analyzing output
- Generating tests and documentation
- Explaining complex technical concepts clearly

Guidelines:
- Be concise but thorough
- Provide working code, not pseudocode
- Explain your reasoning when making changes
- Ask clarifying questions if requirements are ambiguous
- Respect existing code style and conventions
- Consider edge cases and error handling

Current project context:
- Project: {project}
- Workspace: {workspace}
- Date: {date}

You have access to the following tools:
- read_file: Read file contents
- write_file: Create or overwrite files
- edit_file: Make targeted edits to existing files
- search_files: Search for files by pattern
- grep_search: Search file contents
- run_terminal: Execute shell commands
- get_diagnostics: Get compiler/linter errors
- list_directory: List directory contents)";
    
    m_templates["system"] = system;
    
    // Default style prompt
    PromptTemplate style;
    style.name = "style";
    style.isActive = false;  // Disabled by default
    style.description = "Code style preferences";
    style.content = R"(Code style guidelines:
- Use consistent indentation (4 spaces for C++/Python, 2 spaces for JS/TS)
- Keep functions under 50 lines where practical
- Use descriptive variable and function names
- Add comments for complex logic
- Follow language-specific conventions (PEP8 for Python, Google style for C++))";
    
    m_templates["style"] = style;
}
