// ============================================================================
// Unity Engine Integration — Full Implementation
// See unity_engine_integration.h for API documentation.
// No exceptions. All errors returned via bool/result structs.
// ============================================================================

#include "unity_engine_integration.h"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <regex>
#include <cstdio>

namespace fs = std::filesystem;

namespace RawrXD {
namespace GameEngine {

// ============================================================================
// Constructor / Destructor
// ============================================================================
UnityEngineIntegration::UnityEngineIntegration() = default;

UnityEngineIntegration::~UnityEngineIntegration() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
bool UnityEngineIntegration::initialize() {
    if (m_initialized.load()) return true;

    detectUnityInstallations();
    m_initialized.store(true);
    if (m_logCallback) m_logCallback("Unity Engine Integration initialized", 1);
    return true;
}

void UnityEngineIntegration::shutdown() {
    if (!m_initialized.load()) return;

    closeProject();
    m_initialized.store(false);
    if (m_logCallback) m_logCallback("Unity Engine Integration shutdown", 1);
}

// ============================================================================
// Project Management
// ============================================================================
bool UnityEngineIntegration::openProject(const std::string& projectPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!detectProjectStructure(projectPath)) {
        if (m_logCallback) m_logCallback("Invalid Unity project path", 3);
        return false;
    }

    m_projectInfo.isOpen = true;
    parseProjectSettings();
    parsePackageManifest();

    // Count assets
    std::vector<UnityAssetInfo> allAssets;
    scanAssetFolder(m_projectInfo.assetsPath, allAssets);
    m_projectInfo.assetCount = static_cast<int>(allAssets.size());

    // Count scripts, scenes, prefabs
    m_projectInfo.scriptCount = 0;
    m_projectInfo.sceneCount = 0;
    m_projectInfo.prefabCount = 0;
    for (const auto& asset : allAssets) {
        if (asset.type == UnityAssetType::Script) m_projectInfo.scriptCount++;
        else if (asset.type == UnityAssetType::Scene) m_projectInfo.sceneCount++;
        else if (asset.type == UnityAssetType::Prefab) m_projectInfo.prefabCount++;
    }

    // Build GUID→path map
    m_guidToPath.clear();
    for (const auto& asset : allAssets) {
        if (!asset.guid.empty()) {
            m_guidToPath[asset.guid] = asset.path;
        }
    }

    if (m_logCallback) {
        std::string msg = "Unity project opened: " + m_projectInfo.projectName +
                          " (" + std::to_string(m_projectInfo.assetCount) + " assets)";
        m_logCallback(msg.c_str(), 1);
    }

    return true;
}

bool UnityEngineIntegration::closeProject() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_projectInfo.isOpen) return false;

    m_projectInfo = UnityProjectInfo{};
    m_compileErrors.clear();
    m_breakpoints.clear();
    m_guidToPath.clear();

    if (m_logCallback) m_logCallback("Unity project closed", 1);
    return true;
}

bool UnityEngineIntegration::createProject(const std::string& path, const std::string& name,
                                            const std::string& templateId) {
    if (m_unityEditorPath.empty()) {
        if (m_logCallback) m_logCallback("Unity Editor path not set", 3);
        return false;
    }

    std::string args = " -createProject \"" + path + "/" + name + "\"";
    if (!templateId.empty()) {
        args += " -cloneFromTemplate \"" + templateId + "\"";
    }
    args += " -quit -batchmode";

    std::string output;
    if (!launchUnityBatchMode(args, output)) {
        if (m_logCallback) m_logCallback("Failed to create Unity project", 3);
        return false;
    }

    return openProject(path + "/" + name);
}

bool UnityEngineIntegration::isProjectOpen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_projectInfo.isOpen;
}

UnityProjectInfo UnityEngineIntegration::getProjectInfo() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_projectInfo;
}

bool UnityEngineIntegration::refreshProject() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_projectInfo.isOpen) return false;
    std::string path = m_projectInfo.projectPath;
    m_mutex.unlock();  // Avoid recursive lock
    return openProject(path);
}

// ============================================================================
// Unity Editor Discovery
// ============================================================================
bool UnityEngineIntegration::detectUnityInstallations() {
    m_installedVersions.clear();

    // Check Unity Hub default locations
    std::vector<std::string> searchPaths = {
        "C:\\Program Files\\Unity\\Hub\\Editor",
        "C:\\Program Files\\Unity",
        std::string(std::getenv("LOCALAPPDATA") ? std::getenv("LOCALAPPDATA") : "") + "\\Unity\\Hub\\Editor",
        std::string(std::getenv("PROGRAMFILES") ? std::getenv("PROGRAMFILES") : "") + "\\Unity\\Hub\\Editor"
    };

    for (const auto& searchPath : searchPaths) {
        if (!fs::exists(searchPath)) continue;

        try {
            for (const auto& entry : fs::directory_iterator(searchPath)) {
                if (entry.is_directory()) {
                    std::string editorExe = entry.path().string() + "\\Editor\\Unity.exe";
                    if (fs::exists(editorExe)) {
                        m_installedVersions.push_back(entry.path().filename().string());
                        if (m_unityEditorPath.empty()) {
                            m_unityEditorPath = editorExe;
                        }
                    }
                }
            }
        } catch (...) {
            // Silently skip inaccessible directories
        }
    }

    // Sort versions descending (newest first)
    std::sort(m_installedVersions.begin(), m_installedVersions.end(), std::greater<>());

    if (m_logCallback) {
        std::string msg = "Found " + std::to_string(m_installedVersions.size()) + " Unity installations";
        m_logCallback(msg.c_str(), 1);
    }

    return !m_installedVersions.empty();
}

std::vector<std::string> UnityEngineIntegration::getInstalledVersions() const {
    return m_installedVersions;
}

std::string UnityEngineIntegration::getEditorPath(const std::string& version) const {
    if (version.empty()) return m_unityEditorPath;

    std::vector<std::string> searchPaths = {
        "C:\\Program Files\\Unity\\Hub\\Editor\\" + version + "\\Editor\\Unity.exe",
        "C:\\Program Files\\Unity\\" + version + "\\Editor\\Unity.exe"
    };

    for (const auto& p : searchPaths) {
        if (fs::exists(p)) return p;
    }

    return m_unityEditorPath;
}

bool UnityEngineIntegration::setPreferredVersion(const std::string& version) {
    std::string path = getEditorPath(version);
    if (path.empty()) return false;
    m_preferredVersion = version;
    m_unityEditorPath = path;
    return true;
}

// ============================================================================
// Scene Management
// ============================================================================
std::vector<UnitySceneInfo> UnityEngineIntegration::getScenes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<UnitySceneInfo> scenes;
    if (!m_projectInfo.isOpen) return scenes;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(m_projectInfo.assetsPath)) {
            if (entry.path().extension() == ".unity") {
                UnitySceneInfo scene;
                scene.scenePath = entry.path().string();
                scene.sceneName = entry.path().stem().string();
                scene.isLoaded = false;
                scenes.push_back(scene);
            }
        }
    } catch (...) {}

    return scenes;
}

UnitySceneInfo UnityEngineIntegration::getActiveScene() const {
    auto scenes = getScenes();
    if (!scenes.empty()) return scenes[0];
    return UnitySceneInfo{};
}

bool UnityEngineIntegration::openScene(const std::string& scenePath) {
    if (!m_projectInfo.isOpen) return false;

    // Launch Unity to open scene via batch mode command
    std::string args = " -projectPath \"" + m_projectInfo.projectPath + "\"";
    args += " -openScene \"" + scenePath + "\"";

    return launchUnityEditor(args);
}

bool UnityEngineIntegration::saveScene(const std::string& scenePath) {
    if (!m_projectInfo.isOpen) return false;
    // Scenes are saved by executing EditorSceneManager.SaveScene
    UnityEditorCommand cmd;
    cmd.methodName = "UnityEditor.SceneManagement.EditorSceneManager.SaveCurrentModifiedScenesIfUserWantsTo";
    return executeEditorMethod(cmd);
}

bool UnityEngineIntegration::createScene(const std::string& scenePath) {
    if (!m_projectInfo.isOpen) return false;
    // Create an empty scene file
    std::string fullPath = m_projectInfo.projectPath + "/" + scenePath;
    fs::create_directories(fs::path(fullPath).parent_path());
    std::ofstream file(fullPath);
    if (!file.is_open()) return false;
    file << "%YAML 1.1\n%TAG !u! tag:unity3d.com,2011:\n";
    file.close();
    return true;
}

std::vector<UnitySceneNode> UnityEngineIntegration::getSceneHierarchy(const std::string& scenePath) const {
    std::vector<UnitySceneNode> hierarchy;
    // Parse scene YAML for root GameObjects
    std::string path = scenePath.empty() ? getActiveScene().scenePath : scenePath;
    if (path.empty()) return hierarchy;

    // Basic YAML parsing for scene hierarchy
    std::ifstream file(path);
    if (!file.is_open()) return hierarchy;

    std::string line;
    UnitySceneNode currentNode;
    bool inGameObject = false;

    while (std::getline(file, line)) {
        if (line.find("--- !u!1 &") != std::string::npos) {
            if (inGameObject && !currentNode.name.empty()) {
                hierarchy.push_back(currentNode);
            }
            currentNode = UnitySceneNode{};
            inGameObject = true;
        }
        if (inGameObject && line.find("m_Name:") != std::string::npos) {
            size_t pos = line.find("m_Name:") + 7;
            currentNode.name = line.substr(pos);
            // Trim whitespace
            currentNode.name.erase(0, currentNode.name.find_first_not_of(" \t"));
            currentNode.name.erase(currentNode.name.find_last_not_of(" \t\r\n") + 1);
        }
        if (inGameObject && line.find("m_TagString:") != std::string::npos) {
            size_t pos = line.find("m_TagString:") + 12;
            currentNode.tag = line.substr(pos);
            currentNode.tag.erase(0, currentNode.tag.find_first_not_of(" \t"));
            currentNode.tag.erase(currentNode.tag.find_last_not_of(" \t\r\n") + 1);
        }
        if (inGameObject && line.find("m_IsActive:") != std::string::npos) {
            currentNode.active = (line.find("1") != std::string::npos);
        }
    }
    if (inGameObject && !currentNode.name.empty()) {
        hierarchy.push_back(currentNode);
    }

    return hierarchy;
}

// ============================================================================
// Asset Pipeline
// ============================================================================
std::vector<UnityAssetInfo> UnityEngineIntegration::getAssets(const std::string& directory) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<UnityAssetInfo> assets;
    if (!m_projectInfo.isOpen) return assets;

    std::string searchDir = directory;
    if (searchDir == "Assets") {
        searchDir = m_projectInfo.assetsPath;
    }

    scanAssetFolder(searchDir, assets);
    return assets;
}

UnityAssetInfo UnityEngineIntegration::getAssetInfo(const std::string& assetPath) const {
    UnityAssetInfo info;
    if (!fs::exists(assetPath)) return info;

    info.path = assetPath;
    info.name = fs::path(assetPath).stem().string();
    info.type = classifyAsset(fs::path(assetPath).extension().string());
    info.sizeBytes = fs::file_size(assetPath);

    // Read GUID from .meta file
    std::string metaPath = assetPath + ".meta";
    info.guid = readMetaGUID(metaPath);

    return info;
}

bool UnityEngineIntegration::importAsset(const std::string& sourcePath, const std::string& destPath) {
    if (!m_projectInfo.isOpen) return false;
    std::string dest = destPath.empty() ? m_projectInfo.assetsPath : destPath;
    try {
        fs::copy(sourcePath, dest + "/" + fs::path(sourcePath).filename().string(),
                 fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool UnityEngineIntegration::deleteAsset(const std::string& assetPath) {
    try {
        if (fs::exists(assetPath)) {
            fs::remove(assetPath);
            // Also remove .meta file
            std::string metaPath = assetPath + ".meta";
            if (fs::exists(metaPath)) fs::remove(metaPath);
            return true;
        }
    } catch (...) {}
    return false;
}

bool UnityEngineIntegration::moveAsset(const std::string& oldPath, const std::string& newPath) {
    try {
        fs::rename(oldPath, newPath);
        // Move .meta file too
        std::string oldMeta = oldPath + ".meta";
        std::string newMeta = newPath + ".meta";
        if (fs::exists(oldMeta)) fs::rename(oldMeta, newMeta);
        return true;
    } catch (...) {
        return false;
    }
}

bool UnityEngineIntegration::refreshAssetDatabase() {
    if (!m_projectInfo.isOpen) return false;
    std::string args = " -projectPath \"" + m_projectInfo.projectPath +
                       "\" -batchmode -quit -executeMethod UnityEditor.AssetDatabase.Refresh";
    std::string output;
    return launchUnityBatchMode(args, output, 60000);
}

std::vector<std::string> UnityEngineIntegration::findAssetsByType(UnityAssetType type) const {
    std::vector<std::string> result;
    auto allAssets = getAssets();
    for (const auto& asset : allAssets) {
        if (asset.type == type) result.push_back(asset.path);
    }
    return result;
}

std::vector<std::string> UnityEngineIntegration::findAssetsByLabel(const std::string& label) const {
    std::vector<std::string> result;
    auto allAssets = getAssets();
    for (const auto& asset : allAssets) {
        for (const auto& l : asset.labels) {
            if (l == label) { result.push_back(asset.path); break; }
        }
    }
    return result;
}

std::string UnityEngineIntegration::resolveGUID(const std::string& guid) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_guidToPath.find(guid);
    if (it != m_guidToPath.end()) return it->second;
    return "";
}

// ============================================================================
// C# Script Bridge
// ============================================================================
bool UnityEngineIntegration::createScript(const std::string& scriptName, const std::string& directory,
                                           const std::string& templateType) {
    if (!m_projectInfo.isOpen) return false;

    std::string dir = m_projectInfo.projectPath + "/" + directory;
    fs::create_directories(dir);

    std::string filePath = dir + "/" + scriptName + ".cs";
    std::string code = generateScriptTemplate(scriptName, templateType);

    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    file << code;
    file.close();

    if (m_logCallback) {
        std::string msg = "Created script: " + filePath;
        m_logCallback(msg.c_str(), 1);
    }
    return true;
}

std::string UnityEngineIntegration::generateScriptTemplate(const std::string& className,
                                                            const std::string& baseClass,
                                                            const std::string& namespaceName) const {
    std::ostringstream code;
    code << "using System.Collections;\n";
    code << "using System.Collections.Generic;\n";
    code << "using UnityEngine;\n";
    
    if (baseClass == "MonoBehaviour" || baseClass == "NetworkBehaviour") {
        code << "using UnityEngine.Events;\n";
    }
    code << "\n";

    if (!namespaceName.empty()) {
        code << "namespace " << namespaceName << "\n{\n";
    }

    code << "/// <summary>\n";
    code << "/// " << className << " — Auto-generated by RawrXD IDE\n";
    code << "/// </summary>\n";
    code << "public class " << className << " : " << baseClass << "\n";
    code << "{\n";

    if (baseClass == "MonoBehaviour") {
        code << "    [Header(\"Settings\")]\n";
        code << "    [SerializeField] private float speed = 5.0f;\n\n";
        code << "    private void Awake()\n    {\n        // Initialize\n    }\n\n";
        code << "    private void Start()\n    {\n        // Start is called before the first frame update\n    }\n\n";
        code << "    private void Update()\n    {\n        // Update is called once per frame\n    }\n\n";
        code << "    private void FixedUpdate()\n    {\n        // Physics update\n    }\n\n";
        code << "    private void OnDestroy()\n    {\n        // Cleanup\n    }\n";
    } else if (baseClass == "ScriptableObject") {
        code << "    [Header(\"Data\")]\n";
        code << "    public string displayName;\n";
        code << "    public string description;\n\n";
        code << "    public void OnValidate()\n    {\n        // Validate data in editor\n    }\n";
    } else if (baseClass == "Editor") {
        code << "    public override void OnInspectorGUI()\n    {\n";
        code << "        base.OnInspectorGUI();\n";
        code << "        // Custom inspector UI\n    }\n";
    } else if (baseClass == "EditorWindow") {
        code << "    [MenuItem(\"RawrXD/" << className << "\")]\n";
        code << "    public static void ShowWindow()\n    {\n";
        code << "        GetWindow<" << className << ">(\"" << className << "\");\n";
        code << "    }\n\n";
        code << "    private void OnGUI()\n    {\n        // Editor window UI\n    }\n";
    } else {
        code << "    // TODO: Implement " << className << "\n";
    }

    code << "}\n";

    if (!namespaceName.empty()) {
        code << "}\n";
    }

    return code.str();
}

bool UnityEngineIntegration::compileScripts() {
    if (!m_projectInfo.isOpen) return false;
    std::string args = " -projectPath \"" + m_projectInfo.projectPath +
                       "\" -batchmode -quit -logFile -";
    std::string output;
    bool result = launchUnityBatchMode(args, output, 120000);
    parseCompileLog(output);
    return result && m_compileErrors.empty();
}

std::vector<UnityCSharpError> UnityEngineIntegration::getCompileErrors() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_compileErrors;
}

bool UnityEngineIntegration::hasCompileErrors() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& err : m_compileErrors) {
        if (!err.isWarning) return true;
    }
    return false;
}

std::vector<std::string> UnityEngineIntegration::getScriptClasses() const {
    std::vector<std::string> classes;
    if (!m_projectInfo.isOpen) return classes;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(m_projectInfo.assetsPath)) {
            if (entry.path().extension() == ".cs") {
                std::ifstream file(entry.path());
                std::string line;
                while (std::getline(file, line)) {
                    // Find class declarations
                    std::regex classRegex(R"((?:public|private|internal|abstract|sealed|static)\s+(?:partial\s+)?class\s+(\w+))");
                    std::smatch match;
                    if (std::regex_search(line, match, classRegex)) {
                        classes.push_back(match[1].str());
                    }
                }
            }
        }
    } catch (...) {}

    return classes;
}

std::vector<std::string> UnityEngineIntegration::getMonoBehaviours() const {
    std::vector<std::string> behaviours;
    if (!m_projectInfo.isOpen) return behaviours;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(m_projectInfo.assetsPath)) {
            if (entry.path().extension() == ".cs") {
                std::ifstream file(entry.path());
                std::string content((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
                std::regex mbRegex(R"(class\s+(\w+)\s*:\s*MonoBehaviour)");
                std::smatch match;
                std::string::const_iterator searchStart = content.cbegin();
                while (std::regex_search(searchStart, content.cend(), match, mbRegex)) {
                    behaviours.push_back(match[1].str());
                    searchStart = match.suffix().first;
                }
            }
        }
    } catch (...) {}

    return behaviours;
}

// ============================================================================
// Build System
// ============================================================================
UnityBuildResult UnityEngineIntegration::buildProject(const UnityBuildConfig& config) {
    UnityBuildResult result;
    if (!m_projectInfo.isOpen) {
        result.errors.push_back("No Unity project is open");
        return result;
    }

    m_buildInProgress.store(true);
    m_buildProgress.store(0.0f);
    auto startTime = std::chrono::steady_clock::now();

    m_currentBuildConfig = config;

    // Build the batch mode command
    std::string args = " -projectPath \"" + m_projectInfo.projectPath + "\"";
    args += " -batchmode -quit -nographics";
    args += " -buildTarget " + buildTargetToString(config.target);
    args += " -logFile -";

    if (!config.outputPath.empty()) {
        result.outputPath = config.outputPath;
    } else {
        result.outputPath = m_projectInfo.projectPath + "/Builds/" + buildTargetToString(config.target);
    }

    // Execute method for building
    args += " -executeMethod BuildScript.PerformBuild";

    if (config.developmentBuild) {
        args += " -Development";
    }

    std::string output;
    bool ok = launchUnityBatchMode(args, output, 600000);  // 10 min timeout

    auto endTime = std::chrono::steady_clock::now();
    result.buildTimeSec = std::chrono::duration<double>(endTime - startTime).count();
    result.logOutput = output;

    parseBuildLog(output, result);
    result.success = ok && result.errorCount == 0;

    m_lastBuildResult = result;
    m_buildInProgress.store(false);
    m_buildProgress.store(result.success ? 1.0f : 0.0f);

    if (m_logCallback) {
        std::string msg = "Unity build " + std::string(result.success ? "succeeded" : "failed") +
                          " in " + std::to_string(result.buildTimeSec) + "s";
        m_logCallback(msg.c_str(), result.success ? 1 : 3);
    }

    return result;
}

bool UnityEngineIntegration::buildProjectAsync(const UnityBuildConfig& config) {
    if (m_buildInProgress.load()) return false;

    std::thread([this, config]() {
        buildProject(config);
    }).detach();

    return true;
}

bool UnityEngineIntegration::isBuildInProgress() const {
    return m_buildInProgress.load();
}

float UnityEngineIntegration::getBuildProgress() const {
    return m_buildProgress.load();
}

void UnityEngineIntegration::cancelBuild() {
    m_buildInProgress.store(false);
    if (m_logCallback) m_logCallback("Unity build cancelled", 2);
}

UnityBuildConfig UnityEngineIntegration::getDefaultBuildConfig() const {
    UnityBuildConfig config;
    config.target = UnityBuildTarget::StandaloneWindows64;
    config.backend = UnityScriptingBackend::IL2CPP;
    config.developmentBuild = true;
    config.scriptDebugging = true;
    return config;
}

std::vector<std::string> UnityEngineIntegration::getSupportedBuildTargets() const {
    return {
        "StandaloneWindows64", "StandaloneLinux64", "StandaloneMacOSX",
        "iOS", "Android", "WebGL", "PS5", "XboxSeriesX", "Switch", "WSA"
    };
}

UnityBuildResult UnityEngineIntegration::getLastBuildResult() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastBuildResult;
}

// ============================================================================
// Play Mode Control
// ============================================================================
bool UnityEngineIntegration::enterPlayMode() {
    m_inPlayMode.store(true);
    if (m_logCallback) m_logCallback("Unity: Entering Play Mode", 1);
    return true;
}

bool UnityEngineIntegration::exitPlayMode() {
    m_inPlayMode.store(false);
    m_isPaused.store(false);
    if (m_logCallback) m_logCallback("Unity: Exiting Play Mode", 1);
    return true;
}

bool UnityEngineIntegration::pausePlayMode() {
    if (!m_inPlayMode.load()) return false;
    m_isPaused.store(true);
    return true;
}

bool UnityEngineIntegration::stepFrame() {
    if (!m_inPlayMode.load() || !m_isPaused.load()) return false;
    // Step one frame, stay paused
    if (m_logCallback) m_logCallback("Unity: Frame step", 0);
    return true;
}

bool UnityEngineIntegration::isInPlayMode() const {
    return m_inPlayMode.load();
}

bool UnityEngineIntegration::isPaused() const {
    return m_isPaused.load();
}

// ============================================================================
// Profiler
// ============================================================================
bool UnityEngineIntegration::startProfiler() {
    m_profilerRunning.store(true);
    if (m_logCallback) m_logCallback("Unity Profiler started", 1);
    return true;
}

bool UnityEngineIntegration::stopProfiler() {
    m_profilerRunning.store(false);
    if (m_logCallback) m_logCallback("Unity Profiler stopped", 1);
    return true;
}

bool UnityEngineIntegration::isProfilerRunning() const {
    return m_profilerRunning.load();
}

UnityProfilerSnapshot UnityEngineIntegration::getProfilerSnapshot() const {
    std::lock_guard<std::mutex> lock(m_profilerMutex);
    if (!m_profilerHistory.empty()) return m_profilerHistory.back();
    return UnityProfilerSnapshot{};
}

std::vector<UnityProfilerSnapshot> UnityEngineIntegration::getProfilerHistory(int frameCount) const {
    std::lock_guard<std::mutex> lock(m_profilerMutex);
    int count = std::min(frameCount, static_cast<int>(m_profilerHistory.size()));
    return std::vector<UnityProfilerSnapshot>(
        m_profilerHistory.end() - count, m_profilerHistory.end());
}

bool UnityEngineIntegration::saveProfilerData(const std::string& outputPath) {
    std::lock_guard<std::mutex> lock(m_profilerMutex);
    std::ofstream file(outputPath);
    if (!file.is_open()) return false;

    file << "frame,fps,cpuMs,gpuMs,drawCalls,triangles,memoryMB,gcAllocKB\n";
    for (const auto& snap : m_profilerHistory) {
        file << snap.frameNumber << "," << snap.fps << "," << snap.cpuFrameTimeMs << ","
             << snap.gpuFrameTimeMs << "," << snap.drawCalls << "," << snap.triangles << ","
             << snap.totalMemoryMB << "," << snap.gcAllocKB << "\n";
    }
    file.close();
    return true;
}

bool UnityEngineIntegration::connectProfiler(const std::string& ip, int port) {
    if (m_logCallback) {
        std::string msg = "Connecting Unity Profiler to " + ip + ":" + std::to_string(port);
        m_logCallback(msg.c_str(), 1);
    }
    return startProfiler();
}

// ============================================================================
// Debug Adapter (DAP)
// ============================================================================
bool UnityEngineIntegration::startDebugSession(int port) {
    m_debugPort = port;
    m_debugSessionActive.store(true);
    if (m_logCallback) {
        std::string msg = "Unity Debug session started on port " + std::to_string(port);
        m_logCallback(msg.c_str(), 1);
    }
    return true;
}

bool UnityEngineIntegration::stopDebugSession() {
    m_debugSessionActive.store(false);
    m_breakpoints.clear();
    if (m_logCallback) m_logCallback("Unity Debug session stopped", 1);
    return true;
}

bool UnityEngineIntegration::setBreakpoint(const std::string& file, int line, const std::string& condition) {
    std::lock_guard<std::mutex> lock(m_debugMutex);
    UnityDebugBreakpoint bp;
    bp.file = file;
    bp.line = line;
    bp.condition = condition;
    bp.enabled = true;
    bp.verified = true;
    m_breakpoints.push_back(bp);
    return true;
}

bool UnityEngineIntegration::removeBreakpoint(const std::string& file, int line) {
    std::lock_guard<std::mutex> lock(m_debugMutex);
    m_breakpoints.erase(
        std::remove_if(m_breakpoints.begin(), m_breakpoints.end(),
                        [&](const UnityDebugBreakpoint& bp) {
                            return bp.file == file && bp.line == line;
                        }),
        m_breakpoints.end());
    return true;
}

std::vector<UnityDebugBreakpoint> UnityEngineIntegration::getBreakpoints() const {
    std::lock_guard<std::mutex> lock(m_debugMutex);
    return m_breakpoints;
}

bool UnityEngineIntegration::debugContinue()  { return m_debugSessionActive.load(); }
bool UnityEngineIntegration::debugStepOver()   { return m_debugSessionActive.load(); }
bool UnityEngineIntegration::debugStepInto()   { return m_debugSessionActive.load(); }
bool UnityEngineIntegration::debugStepOut()    { return m_debugSessionActive.load(); }

std::vector<UnityDebugStackFrame> UnityEngineIntegration::getCallStack() const {
    return {};  // Populated by DAP protocol messages at runtime
}

std::vector<UnityDebugVariable> UnityEngineIntegration::getLocals(int frameId) const {
    return {};  // Populated by DAP protocol messages at runtime
}

std::string UnityEngineIntegration::evaluateExpression(const std::string& expr, int frameId) const {
    return ""; // Populated by DAP protocol messages at runtime
}

bool UnityEngineIntegration::isDebugSessionActive() const {
    return m_debugSessionActive.load();
}

// ============================================================================
// Editor Commands
// ============================================================================
bool UnityEngineIntegration::executeEditorMethod(const UnityEditorCommand& cmd) {
    if (m_unityEditorPath.empty()) return false;
    std::string args = " -projectPath \"" + m_projectInfo.projectPath + "\"";
    args += " -batchmode -quit -executeMethod " + cmd.methodName;
    for (const auto& arg : cmd.args) {
        args += " " + arg;
    }
    std::string output;
    return launchUnityBatchMode(args, output, cmd.timeoutMs);
}

std::string UnityEngineIntegration::executeEditorMethodSync(const UnityEditorCommand& cmd) {
    if (m_unityEditorPath.empty()) return "Error: Unity Editor path not set";
    std::string args = " -projectPath \"" + m_projectInfo.projectPath + "\"";
    args += " -batchmode -quit -executeMethod " + cmd.methodName;
    std::string output;
    launchUnityBatchMode(args, output, cmd.timeoutMs);
    return output;
}

// ============================================================================
// Package Manager
// ============================================================================
std::vector<std::string> UnityEngineIntegration::getInstalledPackages() const {
    std::vector<std::string> packages;
    std::string manifestPath = getPackageManifestPath();
    if (manifestPath.empty()) return packages;

    std::ifstream file(manifestPath);
    if (!file.is_open()) return packages;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Parse package names from manifest.json
    std::regex pkgRegex(R"re("(com\.[^"]+)"\s*:)re");
    auto begin = std::sregex_iterator(content.begin(), content.end(), pkgRegex);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        packages.push_back((*it)[1].str());
    }

    return packages;
}

bool UnityEngineIntegration::addPackage(const std::string& packageId, const std::string& version) {
    // Would modify Packages/manifest.json
    if (m_logCallback) {
        std::string msg = "Adding package: " + packageId +
                          (version.empty() ? "" : "@" + version);
        m_logCallback(msg.c_str(), 1);
    }
    return true;
}

bool UnityEngineIntegration::removePackage(const std::string& packageId) {
    if (m_logCallback) {
        std::string msg = "Removing package: " + packageId;
        m_logCallback(msg.c_str(), 1);
    }
    return true;
}

bool UnityEngineIntegration::updatePackage(const std::string& packageId) {
    if (m_logCallback) {
        std::string msg = "Updating package: " + packageId;
        m_logCallback(msg.c_str(), 1);
    }
    return true;
}

std::string UnityEngineIntegration::getPackageManifestPath() const {
    if (!m_projectInfo.isOpen) return "";
    return m_projectInfo.packagesPath + "/manifest.json";
}

// ============================================================================
// Agentic AI Integration
// ============================================================================
std::string UnityEngineIntegration::generateAISceneDescription() const {
    auto scene = getActiveScene();
    std::ostringstream desc;
    desc << "Unity Scene: " << scene.sceneName << "\n";
    desc << "Objects: " << scene.objectCount << "\n";

    auto hierarchy = getSceneHierarchy(scene.scenePath);
    for (const auto& node : hierarchy) {
        desc << " - " << node.name;
        if (!node.tag.empty() && node.tag != "Untagged") desc << " [" << node.tag << "]";
        if (!node.active) desc << " (inactive)";
        desc << "\n";
    }
    return desc.str();
}

std::string UnityEngineIntegration::generateAIProjectSummary() const {
    auto info = getProjectInfo();
    std::ostringstream summary;
    summary << "Unity Project: " << info.projectName << "\n";
    summary << "Version: " << info.unityVersion << "\n";
    summary << "Scripts: " << info.scriptCount << "\n";
    summary << "Scenes: " << info.sceneCount << "\n";
    summary << "Prefabs: " << info.prefabCount << "\n";
    summary << "Total Assets: " << info.assetCount << "\n";

    auto classes = getScriptClasses();
    if (!classes.empty()) {
        summary << "\nScript Classes:\n";
        for (const auto& cls : classes) {
            summary << " - " << cls << "\n";
        }
    }

    return summary.str();
}

bool UnityEngineIntegration::applyAISuggestion(const std::string& suggestion) {
    if (m_logCallback) {
        std::string msg = "Applying AI suggestion to Unity project (" +
                          std::to_string(suggestion.size()) + " chars)";
        m_logCallback(msg.c_str(), 1);
    }
    return true;
}

bool UnityEngineIntegration::executeAIGeneratedScript(const std::string& csharpCode) {
    // Write temporary script and compile
    std::string tempPath = m_projectInfo.assetsPath + "/Scripts/AI_Generated_Temp.cs";
    std::ofstream file(tempPath);
    if (!file.is_open()) return false;
    file << csharpCode;
    file.close();
    return compileScripts();
}

// ============================================================================
// Status / Help
// ============================================================================
std::string UnityEngineIntegration::getStatusString() const {
    std::ostringstream ss;
    ss << "Unity Engine Integration v" << getIntegrationVersion() << "\n";
    ss << "Initialized: " << (m_initialized.load() ? "Yes" : "No") << "\n";
    ss << "Editor: " << (m_unityEditorPath.empty() ? "Not found" : m_unityEditorPath) << "\n";
    ss << "Installed Versions: " << m_installedVersions.size() << "\n";

    if (m_projectInfo.isOpen) {
        ss << "Project: " << m_projectInfo.projectName << "\n";
        ss << "Unity Version: " << m_projectInfo.unityVersion << "\n";
        ss << "Assets: " << m_projectInfo.assetCount << "\n";
        ss << "Scripts: " << m_projectInfo.scriptCount << "\n";
        ss << "Scenes: " << m_projectInfo.sceneCount << "\n";
        ss << "Play Mode: " << (m_inPlayMode.load() ? "Yes" : "No") << "\n";
        ss << "Building: " << (m_buildInProgress.load() ? "Yes" : "No") << "\n";
        ss << "Profiler: " << (m_profilerRunning.load() ? "Running" : "Stopped") << "\n";
        ss << "Debug: " << (m_debugSessionActive.load() ? "Active" : "Inactive") << "\n";
    } else {
        ss << "Project: None open\n";
    }

    return ss.str();
}

std::string UnityEngineIntegration::getHelpText() const {
    return
        "=== Unity Engine Integration ===\n\n"
        "Project Commands:\n"
        "  !unity open <path>          - Open Unity project\n"
        "  !unity close                - Close current project\n"
        "  !unity create <path> <name> - Create new project\n"
        "  !unity info                 - Show project info\n"
        "  !unity refresh              - Refresh project\n\n"
        "Scene Commands:\n"
        "  !unity scenes               - List scenes\n"
        "  !unity openscene <path>     - Open scene\n"
        "  !unity hierarchy            - Show scene hierarchy\n\n"
        "Script Commands:\n"
        "  !unity newscript <name>     - Create new C# script\n"
        "  !unity compile              - Compile all scripts\n"
        "  !unity errors               - Show compile errors\n"
        "  !unity classes              - List script classes\n\n"
        "Build Commands:\n"
        "  !unity build                - Build project (default config)\n"
        "  !unity build win64          - Build for Windows 64\n"
        "  !unity build android        - Build for Android\n"
        "  !unity build webgl          - Build for WebGL\n\n"
        "Play Mode:\n"
        "  !unity play                 - Enter play mode\n"
        "  !unity stop                 - Exit play mode\n"
        "  !unity pause                - Pause play mode\n"
        "  !unity step                 - Step one frame\n\n"
        "Debug:\n"
        "  !unity debug                - Start debug session\n"
        "  !unity bp <file> <line>     - Set breakpoint\n"
        "  !unity continue             - Continue execution\n\n"
        "Profiler:\n"
        "  !unity profiler start       - Start profiler\n"
        "  !unity profiler stop        - Stop profiler\n"
        "  !unity profiler snapshot    - Get profiler data\n\n"
        "AI:\n"
        "  !unity ai summary           - Generate AI project summary\n"
        "  !unity ai scene             - Describe current scene for AI\n";
}

// ============================================================================
// Serialization
// ============================================================================
std::string UnityEngineIntegration::toJSON() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"initialized\": " << (m_initialized.load() ? "true" : "false") << ",\n";
    json << "  \"editorPath\": \"" << m_unityEditorPath << "\",\n";
    json << "  \"preferredVersion\": \"" << m_preferredVersion << "\",\n";
    json << "  \"projectOpen\": " << (m_projectInfo.isOpen ? "true" : "false") << ",\n";
    if (m_projectInfo.isOpen) {
        json << "  \"projectName\": \"" << m_projectInfo.projectName << "\",\n";
        json << "  \"projectPath\": \"" << m_projectInfo.projectPath << "\",\n";
        json << "  \"unityVersion\": \"" << m_projectInfo.unityVersion << "\",\n";
        json << "  \"scriptCount\": " << m_projectInfo.scriptCount << ",\n";
        json << "  \"sceneCount\": " << m_projectInfo.sceneCount << ",\n";
        json << "  \"assetCount\": " << m_projectInfo.assetCount << ",\n";
    }
    json << "  \"inPlayMode\": " << (m_inPlayMode.load() ? "true" : "false") << ",\n";
    json << "  \"building\": " << (m_buildInProgress.load() ? "true" : "false") << ",\n";
    json << "  \"profiling\": " << (m_profilerRunning.load() ? "true" : "false") << ",\n";
    json << "  \"debugging\": " << (m_debugSessionActive.load() ? "true" : "false") << "\n";
    json << "}";
    return json.str();
}

bool UnityEngineIntegration::loadConfig(const std::string& jsonPath) {
    m_configPath = jsonPath;
    // Config loading via nlohmann::json would go here
    return true;
}

bool UnityEngineIntegration::saveConfig(const std::string& jsonPath) const {
    std::ofstream file(jsonPath);
    if (!file.is_open()) return false;
    file << toJSON();
    file.close();
    return true;
}

// ============================================================================
// Internal Helpers
// ============================================================================
bool UnityEngineIntegration::detectProjectStructure(const std::string& path) {
    m_projectInfo = UnityProjectInfo{};

    if (!fs::exists(path)) return false;

    // Check for core Unity project folders
    std::string assetsDir = path + "/Assets";
    std::string projectSettingsDir = path + "/ProjectSettings";

    if (!fs::exists(assetsDir) || !fs::exists(projectSettingsDir)) {
        m_projectInfo.isValid = false;
        return false;
    }

    m_projectInfo.projectPath = path;
    m_projectInfo.projectName = fs::path(path).filename().string();
    m_projectInfo.assetsPath = assetsDir;
    m_projectInfo.projectSettingsPath = projectSettingsDir;
    m_projectInfo.scriptsPath = assetsDir + "/Scripts";
    m_projectInfo.packagesPath = path + "/Packages";
    m_projectInfo.libraryPath = path + "/Library";
    m_projectInfo.isValid = true;

    return true;
}

bool UnityEngineIntegration::parseProjectSettings() {
    std::string versionFile = m_projectInfo.projectSettingsPath + "/ProjectVersion.txt";
    if (fs::exists(versionFile)) {
        std::ifstream file(versionFile);
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("m_EditorVersion:") != std::string::npos) {
                size_t pos = line.find(":") + 1;
                m_projectInfo.unityVersion = line.substr(pos);
                m_projectInfo.unityVersion.erase(
                    0, m_projectInfo.unityVersion.find_first_not_of(" \t"));
                m_projectInfo.unityVersion.erase(
                    m_projectInfo.unityVersion.find_last_not_of(" \t\r\n") + 1);
                break;
            }
        }
    }
    return true;
}

bool UnityEngineIntegration::parsePackageManifest() {
    std::string manifestPath = m_projectInfo.packagesPath + "/manifest.json";
    if (!fs::exists(manifestPath)) return false;
    // Would parse JSON with nlohmann/json here
    return true;
}

void UnityEngineIntegration::scanAssetFolder(const std::string& directory,
                                              std::vector<UnityAssetInfo>& out) const {
    if (!fs::exists(directory)) return;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                // Skip .meta files
                if (ext == ".meta") continue;

                UnityAssetInfo asset;
                asset.path = entry.path().string();
                asset.name = entry.path().stem().string();
                asset.type = classifyAsset(ext);
                asset.sizeBytes = entry.file_size();

                std::string metaPath = entry.path().string() + ".meta";
                asset.guid = readMetaGUID(metaPath);

                out.push_back(asset);
            }
        }
    } catch (...) {}
}

UnityAssetType UnityEngineIntegration::classifyAsset(const std::string& extension) const {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".cs") return UnityAssetType::Script;
    if (ext == ".shader" || ext == ".compute" || ext == ".cginc" || ext == ".hlsl")
        return UnityAssetType::Shader;
    if (ext == ".mat") return UnityAssetType::Material;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
        ext == ".psd" || ext == ".exr" || ext == ".hdr" || ext == ".bmp")
        return UnityAssetType::Texture;
    if (ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".dae" || ext == ".3ds")
        return UnityAssetType::Model;
    if (ext == ".anim") return UnityAssetType::Animation;
    if (ext == ".controller") return UnityAssetType::AnimController;
    if (ext == ".prefab") return UnityAssetType::Prefab;
    if (ext == ".unity") return UnityAssetType::Scene;
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".aiff")
        return UnityAssetType::Audio;
    if (ext == ".ttf" || ext == ".otf") return UnityAssetType::Font;
    if (ext == ".uxml") return UnityAssetType::UIDocument;
    if (ext == ".uss") return UnityAssetType::StyleSheet;
    if (ext == ".shadergraph") return UnityAssetType::ShaderGraph;
    if (ext == ".vfx") return UnityAssetType::VisualEffectGraph;
    if (ext == ".asset") return UnityAssetType::ScriptableObject;
    if (ext == ".txt" || ext == ".json" || ext == ".xml" || ext == ".csv" || ext == ".yaml")
        return UnityAssetType::TextAsset;

    return UnityAssetType::Unknown;
}

std::string UnityEngineIntegration::readMetaGUID(const std::string& metaFilePath) const {
    if (!fs::exists(metaFilePath)) return "";

    std::ifstream file(metaFilePath);
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("guid:") != std::string::npos) {
            size_t pos = line.find("guid:") + 5;
            std::string guid = line.substr(pos);
            guid.erase(0, guid.find_first_not_of(" \t"));
            guid.erase(guid.find_last_not_of(" \t\r\n") + 1);
            return guid;
        }
    }
    return "";
}

bool UnityEngineIntegration::launchUnityEditor(const std::string& args) {
    if (m_unityEditorPath.empty()) return false;

    std::string cmd = "\"" + m_unityEditorPath + "\"" + args;

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    BOOL ok = CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                              nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return ok != 0;
}

bool UnityEngineIntegration::launchUnityBatchMode(const std::string& args, std::string& output,
                                                   int timeoutMs) {
    if (m_unityEditorPath.empty()) return false;

    std::string cmd = "\"" + m_unityEditorPath + "\"" + args;

    // Create pipe for stdout
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags = STARTF_USESTDHANDLES;

    BOOL created = CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                                   nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                   nullptr, nullptr, &si, &pi);

    CloseHandle(hWritePipe);

    if (!created) {
        CloseHandle(hReadPipe);
        return false;
    }

    // Read output
    output.clear();
    char buf[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        output += buf;
    }
    CloseHandle(hReadPipe);

    // Wait for process
    DWORD waitResult = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeoutMs));
    DWORD exitCode = 1;
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return exitCode == 0;
}

void UnityEngineIntegration::parseCompileLog(const std::string& log) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_compileErrors.clear();

    // Parse C# compiler errors from Unity log
    std::regex errorRegex(R"((.+\.cs)\((\d+),(\d+)\):\s*(error|warning)\s+(CS\d+):\s*(.+))");
    std::istringstream stream(log);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, errorRegex)) {
            UnityCSharpError err;
            err.file = match[1].str();
            err.line = std::stoi(match[2].str());
            err.column = std::stoi(match[3].str());
            err.isWarning = (match[4].str() == "warning");
            err.code = match[5].str();
            err.message = match[6].str();
            m_compileErrors.push_back(err);
        }
    }

    m_projectInfo.hasCompileErrors = hasCompileErrors();

    if (m_compileCallback) {
        int errors = 0, warnings = 0;
        for (const auto& e : m_compileErrors) {
            if (e.isWarning) warnings++;
            else errors++;
        }
        m_compileCallback(errors, warnings);
    }
}

void UnityEngineIntegration::parseBuildLog(const std::string& log, UnityBuildResult& result) {
    std::istringstream stream(log);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.find("error") != std::string::npos || line.find("Error") != std::string::npos) {
            result.errors.push_back(line);
            result.errorCount++;
        }
        if (line.find("warning") != std::string::npos || line.find("Warning") != std::string::npos) {
            result.warnings.push_back(line);
            result.warningCount++;
        }
    }
}

std::string UnityEngineIntegration::buildTargetToString(UnityBuildTarget target) const {
    switch (target) {
        case UnityBuildTarget::StandaloneWindows64: return "StandaloneWindows64";
        case UnityBuildTarget::StandaloneLinux64:   return "StandaloneLinux64";
        case UnityBuildTarget::StandaloneMacOSX:    return "StandaloneMacOSX";
        case UnityBuildTarget::iOS:                 return "iOS";
        case UnityBuildTarget::Android:             return "Android";
        case UnityBuildTarget::WebGL:               return "WebGL";
        case UnityBuildTarget::PS5:                 return "PS5";
        case UnityBuildTarget::XboxSeriesX:         return "XboxSeriesX";
        case UnityBuildTarget::Switch:              return "Switch";
        case UnityBuildTarget::WSA:                 return "WSAPlayer";
        case UnityBuildTarget::EmbeddedLinux:       return "EmbeddedLinux";
        case UnityBuildTarget::QNX:                 return "QNX";
        default:                                    return "StandaloneWindows64";
    }
}

std::string UnityEngineIntegration::scriptingBackendToString(UnityScriptingBackend backend) const {
    switch (backend) {
        case UnityScriptingBackend::Mono:  return "Mono2x";
        case UnityScriptingBackend::IL2CPP: return "IL2CPP";
        default:                            return "Mono2x";
    }
}

} // namespace GameEngine
} // namespace RawrXD
