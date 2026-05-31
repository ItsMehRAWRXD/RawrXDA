// ============================================================================
// SkillInjectionEngine.cpp — Implementation
// ============================================================================

#include "SkillInjectionEngine.h"
#include <windows.h>
#include <shlwapi.h>
#include <fileapi.h>
#include <strsafe.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <regex>

#pragma comment(lib, "shlwapi.lib")

namespace RawrXD {
namespace SkillSystem {

// ============================================================================
// SINGLETON
// ============================================================================
SkillInjectionEngine& SkillInjectionEngine::Instance() {
    static SkillInjectionEngine instance;
    return instance;
}

// ============================================================================
// LOAD ALL SKILLS
// ============================================================================
bool SkillInjectionEngine::LoadAllSkills() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_skills.clear();
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind;
    std::wstring searchPath = std::wstring(SKILL_REGISTRY_PATH) + L"\\*";
    
    hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        // Registry path doesn't exist yet — not an error
        return true;
    }
    
    do {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(findData.cFileName, L".") != 0 &&
            wcscmp(findData.cFileName, L"..") != 0) {
            
            std::wstring skillPath = std::wstring(SKILL_REGISTRY_PATH) + L"\\" + 
                                    findData.cFileName + L"\\" + SKILL_DEFINITION_FILENAME;
            
            SkillDefinition skill;
            if (ParseSkillMarkdown(skillPath, skill)) {
                if (ValidateSkillSchema(skill)) {
                    skill.schemaValid = true;
                    m_skills[skill.name] = std::move(skill);
                } else {
                    skill.schemaValid = false;
                    m_skills[skill.name] = std::move(skill);
                }
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    
    // Load persisted toggle states
    LoadToggleState();
    
    // Rebuild injection cache
    RebuildInjectionCache();
    
    // Start file watcher for hot-reload
    StartFileWatcher();
    
    return !m_skills.empty();
}

// ============================================================================
// PARSE SKILL MARKDOWN
// ============================================================================
bool SkillInjectionEngine::ParseSkillMarkdown(const std::wstring& filePath, SkillDefinition& outSkill) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    outSkill.loadedFromPath.clear();
    outSkill.loadedFromPath.reserve(filePath.size());
    for (wchar_t wc : filePath) {
        outSkill.loadedFromPath.push_back(static_cast<char>(wc));
    }
    
    // Get file modification time
    WIN32_FILE_ATTRIBUTE_DATA fileAttr;
    if (GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &fileAttr)) {
        outSkill.lastModified = fileAttr.ftLastWriteTime;
    }
    
    std::string line;
    std::string currentSection;
    std::ostringstream sectionContent;
    
    // Parse YAML frontmatter + markdown sections
    bool inFrontMatter = false;
    bool frontMatterDone = false;
    
    while (std::getline(file, line)) {
        // YAML frontmatter
        if (!frontMatterDone) {
            if (line == "---") {
                if (!inFrontMatter) {
                    inFrontMatter = true;
                    continue;
                } else {
                    frontMatterDone = true;
                    inFrontMatter = false;
                    continue;
                }
            }
            if (inFrontMatter) {
                // Parse key: value pairs
                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos) {
                    std::string key = line.substr(0, colonPos);
                    std::string value = line.substr(colonPos + 1);
                    // Trim whitespace
                    auto trim = [](std::string& s) {
                        s.erase(0, s.find_first_not_of(" \\t\""));
                        s.erase(s.find_last_not_of(" \\t\"") + 1);
                    };
                    trim(key);
                    trim(value);
                    
                    if (key == "name") outSkill.name = value;
                    else if (key == "description") outSkill.description = value;
                    else if (key == "version") outSkill.version = value;
                    else if (key == "specialistAgent") outSkill.specialistAgent = value;
                    else if (key == "alwaysInject") outSkill.alwaysInject = (value == "true" || value == "yes");
                    else if (key == "priority") {
                        try {
                            outSkill.priority = std::stoul(value);
                        } catch (...) {
                            outSkill.priority = 100; // Default priority on parse failure
                        }
                    }
                }
                continue;
            }
        }
        
        // Markdown section headers (## Section Name)
        if (line.substr(0, 2) == "##") {
            // Save previous section
            if (!currentSection.empty()) {
                std::string content = sectionContent.str();
                if (currentSection == "Mission") outSkill.mission = content;
                else if (currentSection == "Operating Rules") outSkill.operatingRules = content;
                else if (currentSection == "Quality Gates") outSkill.qualityGates = content;
                else if (currentSection == "Coordination Protocol") outSkill.coordinationProtocol = content;
                else if (currentSection == "Risk Framework") outSkill.riskFramework = content;
                else if (currentSection == "Daily Execution Contract") outSkill.dailyExecutionContract = content;
            }
            
            // Extract section name
            currentSection = line.substr(2);
            // Trim
            auto trim = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \\t#"));
                s.erase(s.find_last_not_of(" \\t#") + 1);
            };
            trim(currentSection);
            sectionContent.str("");
            sectionContent.clear();
            continue;
        }
        
        sectionContent << line << "\\n";
    }
    
    // Save final section
    if (!currentSection.empty()) {
        std::string content = sectionContent.str();
        if (currentSection == "Mission") outSkill.mission = content;
        else if (currentSection == "Operating Rules") outSkill.operatingRules = content;
        else if (currentSection == "Quality Gates") outSkill.qualityGates = content;
        else if (currentSection == "Coordination Protocol") outSkill.coordinationProtocol = content;
        else if (currentSection == "Risk Framework") outSkill.riskFramework = content;
        else if (currentSection == "Daily Execution Contract") outSkill.dailyExecutionContract = content;
    }
    
    // Extract tags from description or dedicated tag lines
    std::regex tagRegex(R"(\\*\\*([^\\*]+)\\*\\*)");
    std::smatch match;
    std::string desc = outSkill.description;
    while (std::regex_search(desc, match, tagRegex)) {
        outSkill.tags.push_back(match[1].str());
        desc = match.suffix().str();
    }
    
    return !outSkill.name.empty();
}

// ============================================================================
// VALIDATE SKILL SCHEMA
// ============================================================================
bool SkillInjectionEngine::ValidateSkillSchema(const SkillDefinition& skill) {
    if (skill.name.empty()) {
        const_cast<SkillDefinition&>(skill).schemaValidationError = "Missing required field: name";
        return false;
    }
    if (skill.specialistAgent.empty()) {
        const_cast<SkillDefinition&>(skill).schemaValidationError = "Missing required field: specialistAgent";
        return false;
    }
    if (skill.mission.empty()) {
        const_cast<SkillDefinition&>(skill).schemaValidationError = "Missing required section: Mission";
        return false;
    }
    return true;
}

// ============================================================================
// CRITICAL: INJECT SKILL CONTEXT — First 520 Lines Guarantee
// ============================================================================
std::string SkillInjectionEngine::InjectSkillContext(
    const std::string& originalPrompt,
    const std::string& targetAgent,
    const std::string& currentPhase
) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_cachedInjection.empty()) {
        return originalPrompt;  // No skills loaded — pass through
    }
    
    // Build injection header
    std::ostringstream injection;
    injection << "# RawrXD Skill Context Injection\\n";
    injection << "# ==========================================\\n";
    injection << "# This context is ALWAYS present regardless of model/agent status\\n";
    injection << "# Target Agent: " << (targetAgent.empty() ? "ANY" : targetAgent) << "\\n";
    injection << "# Current Phase: " << (currentPhase.empty() ? "UNKNOWN" : currentPhase) << "\\n";
    injection << "# Active Skills: " << m_skills.size() << "\\n";
    injection << "# Injection Lines: " << m_totalInjectionLines.load() << " / " << SKILL_INJECTION_LINE_COUNT << "\\n";
    injection << "# ==========================================\\n\\n";
    
    // Add cached skill content
    injection << m_cachedInjection;
    
    // Add separator
    injection << "\\n\\n# === END SKILL INJECTION ===\\n";
    injection << "# Original prompt follows below\\n";
    injection << "# ==========================================\\n\\n";
    
    // Append original prompt
    injection << originalPrompt;
    
    std::string result = injection.str();
    
    // HARD ENFORCEMENT: Trim to exactly 520 lines if exceeded
    // (This should never happen with proper skill curation, but guarantees the contract)
    TrimToLineLimit(result, SKILL_INJECTION_LINE_COUNT);
    
    return result;
}

// ============================================================================
// UI TOGGLE API
// ============================================================================
bool SkillInjectionEngine::IsSkillEnabled(const std::string& skillName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_toggleState.find(skillName);
    if (it != m_toggleState.end()) {
        return it->second;
    }
    // Default: check skill definition
    auto skillIt = m_skills.find(skillName);
    if (skillIt != m_skills.end()) {
        return skillIt->second.enabled;
    }
    return false;
}

void SkillInjectionEngine::SetSkillEnabled(const std::string& skillName, bool enabled) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_toggleState[skillName] = enabled;
    }
    SaveToggleState();
    RebuildInjectionCache();
}

void SkillInjectionEngine::ToggleSkill(const std::string& skillName) {
    SetSkillEnabled(skillName, !IsSkillEnabled(skillName));
}

std::vector<SkillDefinition> SkillInjectionEngine::GetAllSkills() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SkillDefinition> result;
    for (const auto& pair : m_skills) {
        result.push_back(pair.second);
    }
    std::sort(result.begin(), result.end(), 
        [](const SkillDefinition& a, const SkillDefinition& b) {
            return a.priority < b.priority;
        });
    return result;
}

std::vector<SkillDefinition> SkillInjectionEngine::GetActiveSkillsForAgent(const std::string& agentName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SkillDefinition> result;
    for (const auto& pair : m_skills) {
        const SkillDefinition& skill = pair.second;
        if (!IsSkillEnabled(skill.name)) continue;
        if (skill.alwaysInject || skill.specialistAgent == agentName) {
            result.push_back(skill);
        }
    }
    std::sort(result.begin(), result.end(),
        [](const SkillDefinition& a, const SkillDefinition& b) {
            return a.priority < b.priority;
        });
    return result;
}

std::vector<SkillDefinition> SkillInjectionEngine::GetActiveSkillsForPhase(const std::string& phaseName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SkillDefinition> result;
    for (const auto& pair : m_skills) {
        const SkillDefinition& skill = pair.second;
        if (!IsSkillEnabled(skill.name)) continue;
        if (skill.phases.empty() || 
            std::find(skill.phases.begin(), skill.phases.end(), phaseName) != skill.phases.end()) {
            result.push_back(skill);
        }
    }
    return result;
}

// ============================================================================
// PERSISTENCE — Registry Toggle State
// ============================================================================
bool SkillInjectionEngine::SaveToggleState() {
    HKEY hKey;
    LONG result = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        SKILL_TOGGLE_REGISTRY_KEY,
        0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_WRITE, nullptr, &hKey, nullptr
    );
    if (result != ERROR_SUCCESS) return false;
    
    for (const auto& pair : m_toggleState) {
        DWORD value = pair.second ? 1 : 0;
        RegSetValueExA(hKey, pair.first.c_str(), 0, REG_DWORD, 
                      reinterpret_cast<const BYTE*>(&value), sizeof(value));
    }
    
    RegCloseKey(hKey);
    return true;
}

bool SkillInjectionEngine::LoadToggleState() {
    HKEY hKey;
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        SKILL_TOGGLE_REGISTRY_KEY,
        0, KEY_READ, &hKey
    );
    if (result != ERROR_SUCCESS) return false;
    
    DWORD index = 0;
    char valueName[256];
    DWORD valueNameLen = 256;
    DWORD valueType;
    DWORD valueData;
    DWORD valueDataLen = sizeof(valueData);
    
    while (RegEnumValueA(hKey, index++, valueName, &valueNameLen,
                         nullptr, &valueType,
                         reinterpret_cast<BYTE*>(&valueData), &valueDataLen) == ERROR_SUCCESS) {
        if (valueType == REG_DWORD) {
            m_toggleState[valueName] = (valueData != 0);
        }
        valueNameLen = 256;
        valueDataLen = sizeof(valueData);
    }
    
    RegCloseKey(hKey);
    return true;
}

// ============================================================================
// REBUILD INJECTION CACHE
// ============================================================================
void SkillInjectionEngine::RebuildInjectionCache() {
    std::ostringstream cache;
    size_t totalLines = 0;
    size_t totalBytes = 0;
    
    // Sort by priority
    std::vector<SkillDefinition> activeSkills;
    for (const auto& pair : m_skills) {
        if (IsSkillEnabled(pair.first) && pair.second.schemaValid) {
            activeSkills.push_back(pair.second);
        }
    }
    std::sort(activeSkills.begin(), activeSkills.end(),
        [](const SkillDefinition& a, const SkillDefinition& b) {
            return a.priority < b.priority;
        });
    
    for (const auto& skill : activeSkills) {
        cache << "## Skill: " << skill.name << "\\n";
        cache << "**Agent**: " << skill.specialistAgent << "\\n";
        cache << "**Phase**: ";
        for (const auto& phase : skill.phases) {
            cache << phase << " ";
        }
        cache << "\\n\\n";
        
        if (!skill.mission.empty()) {
            cache << skill.mission << "\\n\\n";
        }
        if (!skill.operatingRules.empty()) {
            cache << "### Operating Rules\\n" << skill.operatingRules << "\\n\\n";
        }
        if (!skill.qualityGates.empty()) {
            cache << "### Quality Gates\\n" << skill.qualityGates << "\\n\\n";
        }
        if (!skill.coordinationProtocol.empty()) {
            cache << "### Coordination Protocol\\n" << skill.coordinationProtocol << "\\n\\n";
        }
        if (!skill.riskFramework.empty()) {
            cache << "### Risk Framework\\n" << skill.riskFramework << "\\n\\n";
        }
        if (!skill.dailyExecutionContract.empty()) {
            cache << "### Daily Execution Contract\\n" << skill.dailyExecutionContract << "\\n\\n";
        }
        
        cache << "---\\n\\n";
    }
    
    m_cachedInjection = cache.str();
    
    // Count lines and bytes
    totalLines = std::count(m_cachedInjection.begin(), m_cachedInjection.end(), '\\n');
    totalBytes = m_cachedInjection.size();
    
    m_totalInjectionLines.store(totalLines);
    m_totalInjectionBytes.store(totalBytes);
    
    // HARD ENFORCEMENT: If we exceed 520 lines, trim with warning
    if (totalLines > SKILL_INJECTION_LINE_COUNT) {
        TrimToLineLimit(m_cachedInjection, SKILL_INJECTION_LINE_COUNT);
        m_totalInjectionLines.store(SKILL_INJECTION_LINE_COUNT);
    }
}

// ============================================================================
// TRIM TO LINE LIMIT — Hard enforcement of 520-line guarantee
// ============================================================================
void SkillInjectionEngine::TrimToLineLimit(std::string& content, size_t maxLines) {
    size_t linesKept = 0;
    size_t pos = 0;
    size_t lastValidPos = 0;
    
    while (linesKept < maxLines && pos != std::string::npos) {
        lastValidPos = pos;
        pos = content.find('\\n', pos);
        if (pos != std::string::npos) {
            ++linesKept;
            ++pos;
        }
    }
    
    if (linesKept >= maxLines && lastValidPos > 0) {
        content = content.substr(0, lastValidPos);
        content += "\\n\\n# [TRIMMED: Exceeded " + std::to_string(maxLines) + " line guarantee]\\n";
    }
}

// ============================================================================
// FILE WATCHER — Hot Reload
// ============================================================================
void SkillInjectionEngine::StartFileWatcher() {
    if (m_watcherRunning.exchange(true)) return;
    
    try {
        m_watcherThread = std::thread(&SkillInjectionEngine::WatcherLoop, this);
    } catch (const std::exception& e) {
        OutputDebugStringA("[SkillInjection] Failed to start file watcher thread: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        m_watcherRunning.store(false);
    }
}

void SkillInjectionEngine::StopFileWatcher() {
    m_watcherRunning.store(false);
    if (m_directoryWatcher != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(m_directoryWatcher);
        m_directoryWatcher = INVALID_HANDLE_VALUE;
    }
    if (m_watcherThread.joinable()) {
        m_watcherThread.join();
    }
}

void SkillInjectionEngine::WatcherLoop() {
    m_directoryWatcher = FindFirstChangeNotificationW(
        SKILL_REGISTRY_PATH,
        TRUE,  // Watch subtree
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME
    );
    
    if (m_directoryWatcher == INVALID_HANDLE_VALUE) {
        m_watcherRunning.store(false);
        return;
    }
    
    HANDLE handles[] = { m_directoryWatcher };
    while (m_watcherRunning.load()) {
        DWORD waitResult = WaitForMultipleObjects(1, handles, FALSE, 5000);
        if (waitResult == WAIT_OBJECT_0) {
            // Change detected — reload all skills
            LoadAllSkills();
            FindNextChangeNotification(m_directoryWatcher);
        }
    }
    
    FindCloseChangeNotification(m_directoryWatcher);
    m_directoryWatcher = INVALID_HANDLE_VALUE;
}

void SkillInjectionEngine::InvalidateCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    RebuildInjectionCache();
}

// ============================================================================
// RELOAD SINGLE SKILL
// ============================================================================
bool SkillInjectionEngine::ReloadSkill(const std::string& skillName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_skills.find(skillName);
    if (it == m_skills.end()) return false;
    
    std::wstring skillPath(it->second.loadedFromPath.begin(), 
                          it->second.loadedFromPath.end());
    
    SkillDefinition skill;
    if (ParseSkillMarkdown(skillPath, skill)) {
        if (ValidateSkillSchema(skill)) {
            skill.schemaValid = true;
            m_skills[skillName] = std::move(skill);
            RebuildInjectionCache();
            return true;
        }
    }
    return false;
}

// ============================================================================
// C-API EXPORTS
// ============================================================================
extern "C" {

__declspec(dllexport) const char* __stdcall SkillSystem_InjectContext(const char* prompt) {
    static std::string result;
    result = SkillInjectionEngine::Instance().InjectSkillContext(prompt);
    return result.c_str();
}

__declspec(dllexport) bool __stdcall SkillSystem_IsSkillEnabled(const char* skillName) {
    return SkillInjectionEngine::Instance().IsSkillEnabled(skillName);
}

__declspec(dllexport) bool __stdcall SkillSystem_ToggleSkill(const char* skillName) {
    SkillInjectionEngine::Instance().ToggleSkill(skillName);
    return SkillInjectionEngine::Instance().IsSkillEnabled(skillName);
}

__declspec(dllexport) void __stdcall SkillSystem_InvalidateCache() {
    SkillInjectionEngine::Instance().InvalidateCache();
}

} // extern "C"

} // namespace SkillSystem
} // namespace RawrXD
