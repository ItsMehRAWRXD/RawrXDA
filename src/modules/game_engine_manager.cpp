// ============================================================================
// Game Engine Manager — Unified Coordinator Implementation
// Routes all game-engine commands to the correct backend (Unity / Unreal).
// No exceptions. All errors returned via result structs.
// ============================================================================

#include "game_engine_manager.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

// SCAFFOLD_257: Game engine manager


namespace fs = std::filesystem;

namespace RawrXD {
namespace GameEngine {

// ============================================================================
// Global Instance
// ============================================================================
std::unique_ptr<GameEngineManager> g_gameEngineManager;

// ============================================================================
// Constructor / Destructor
// ============================================================================
GameEngineManager::GameEngineManager()
    : m_unity(std::make_unique<UnityEngineIntegration>())
    , m_unreal(std::make_unique<UnrealEngineIntegration>())
{}

GameEngineManager::~GameEngineManager() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
bool GameEngineManager::initialize() {
    if (m_initialized.load()) return true;

    bool unityOk  = m_unity->initialize();
    bool unrealOk = m_unreal->initialize();

    refreshInstallations();

    m_initialized.store(true);
    log("GameEngineManager initialized");
    log(("  Unity backend:  " + std::string(unityOk  ? "OK" : "no installations")).c_str());
    log(("  Unreal backend: " + std::string(unrealOk ? "OK" : "no installations")).c_str());

    return true;
}

void GameEngineManager::shutdown() {
    if (!m_initialized.load()) return;

    closeProject();
    m_unity->shutdown();
    m_unreal->shutdown();
    m_initialized.store(false);
    log("GameEngineManager shutdown");
}

// ============================================================================
// Engine Detection
// ============================================================================
EngineDetectionResult GameEngineManager::detectProject(const std::string& path) const {
    EngineDetectionResult result;

    if (isUnityProject(path)) {
        result.engine = GameEngineType::Unity;
        result.isValid = true;
        result.projectPath = path;

        // Try to extract project name from directory
        result.projectName = fs::path(path).filename().string();

        // Get version from ProjectSettings/ProjectVersion.txt if available
        std::string versionFile = path + "/ProjectSettings/ProjectVersion.txt";
        if (fs::exists(versionFile)) {
            std::ifstream f(versionFile);
            if (f.is_open()) std::getline(f, result.version);
        }

        result.detail = "Unity project: " + result.projectName;
    } else if (isUnrealProject(path)) {
        result.engine = GameEngineType::Unreal;
        result.isValid = true;
        result.projectPath = path;

        // Find .uproject to get name
        try {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.path().extension() == ".uproject") {
                    result.projectName = entry.path().stem().string();
                    break;
                }
            }
        } catch (...) {}

        result.detail = "Unreal project: " + result.projectName;
    } else {
        result.detail = "No game engine project detected";
    }

    return result;
}

GameEngineType GameEngineManager::identifyEngine(const std::string& projectPath) const {
    if (isUnityProject(projectPath)) return GameEngineType::Unity;
    if (isUnrealProject(projectPath)) return GameEngineType::Unreal;
    return GameEngineType::None;
}

bool GameEngineManager::isUnityProject(const std::string& path) const {
    return fs::exists(path + "/Assets") && fs::exists(path + "/ProjectSettings");
}

bool GameEngineManager::isUnrealProject(const std::string& path) const {
    // Check for .uproject file
    if (fs::exists(path) && fs::is_directory(path)) {
        try {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.path().extension() == ".uproject") return true;
            }
        } catch (...) {}
    }
    // Or direct .uproject path
    if (fs::path(path).extension() == ".uproject" && fs::exists(path)) return true;
    return false;
}

// ============================================================================
// Project Management
// ============================================================================
bool GameEngineManager::openProject(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Close any current project first
    if (m_activeEngine != GameEngineType::None) {
        m_mutex.unlock();
        closeProject();
        m_mutex.lock();
    }

    GameEngineType type = identifyEngine(path);

    bool ok = false;
    switch (type) {
        case GameEngineType::Unity:
            ok = m_unity->openProject(path);
            break;
        case GameEngineType::Unreal:
            ok = m_unreal->openProject(path);
            break;
        default:
            log("No game engine project found at path", 3);
            return false;
    }

    if (ok) {
        m_activeEngine = type;
        m_activeProjectPath = path;
        log(("Opened " + std::string(type == GameEngineType::Unity ? "Unity" : "Unreal") +
             " project").c_str());
    }

    return ok;
}

bool GameEngineManager::closeProject() {
    std::lock_guard<std::mutex> lock(m_mutex);
    bool ok = false;

    switch (m_activeEngine) {
        case GameEngineType::Unity:
            ok = m_unity->closeProject();
            break;
        case GameEngineType::Unreal:
            ok = m_unreal->closeProject();
            break;
        default:
            return false;
    }

    m_activeEngine = GameEngineType::None;
    m_activeProjectPath.clear();
    log("Project closed");
    return ok;
}

bool GameEngineManager::isProjectOpen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeEngine != GameEngineType::None;
}

GameEngineType GameEngineManager::getActiveEngine() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeEngine;
}

std::string GameEngineManager::getActiveProjectName() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->getProjectInfo().projectName;
        case GameEngineType::Unreal: return m_unreal->getProjectInfo().projectName;
        default: return "";
    }
}

std::string GameEngineManager::getActiveProjectPath() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeProjectPath;
}

// ============================================================================
// Unified Build
// ============================================================================
GameBuildResultUnified GameEngineManager::buildProject(const GameBuildRequest& request) {
    GameEngineType engine = request.engine;
    if (engine == GameEngineType::None) engine = m_activeEngine;

    GameBuildResultUnified result;
    result.engine = engine;

    switch (engine) {
        case GameEngineType::Unity: {
            UnityBuildConfig config;
            // Map platform string
            if (request.platform == "Win64" || request.platform == "Windows")
                config.target = UnityBuildTarget::StandaloneWindows64;
            else if (request.platform == "Linux")
                config.target = UnityBuildTarget::StandaloneLinux64;
            else if (request.platform == "macOS" || request.platform == "Mac")
                config.target = UnityBuildTarget::StandaloneMacOSX;
            else if (request.platform == "Android")
                config.target = UnityBuildTarget::Android;
            else if (request.platform == "iOS")
                config.target = UnityBuildTarget::iOS;
            else if (request.platform == "WebGL")
                config.target = UnityBuildTarget::WebGL;
            else
                config.target = UnityBuildTarget::StandaloneWindows64;

            config.developmentBuild = request.developmentBuild;
            config.scriptDebugging = request.scriptDebugging;
            config.autoConnectProfiler = request.autoProfile;
            config.outputPath = request.outputPath;

            auto unityResult = m_unity->buildProject(config);
            result = convertUnityBuildResult(unityResult);
            break;
        }
        case GameEngineType::Unreal: {
            UnrealBuildSettings settings;
            // Map platform string
            if (request.platform == "Win64" || request.platform == "Windows")
                settings.platform = UnrealBuildTarget::Win64;
            else if (request.platform == "Linux")
                settings.platform = UnrealBuildTarget::Linux;
            else if (request.platform == "Mac")
                settings.platform = UnrealBuildTarget::Mac;
            else if (request.platform == "Android")
                settings.platform = UnrealBuildTarget::Android;
            else if (request.platform == "iOS")
                settings.platform = UnrealBuildTarget::IOS;
            else
                settings.platform = UnrealBuildTarget::Win64;

            // Map configuration
            if (request.configuration == "Debug")
                settings.configuration = UnrealBuildConfig::Debug;
            else if (request.configuration == "Shipping")
                settings.configuration = UnrealBuildConfig::Shipping;
            else
                settings.configuration = UnrealBuildConfig::Development;

            auto unrealResult = m_unreal->buildProject(settings);
            result = convertUnrealBuildResult(unrealResult);
            break;
        }
        default:
            result.errors.push_back("No engine active for build");
            break;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastBuildResult = result;
    }

    return result;
}

bool GameEngineManager::buildProjectAsync(const GameBuildRequest& request) {
    GameEngineType engine = request.engine;
    if (engine == GameEngineType::None) engine = m_activeEngine;

    switch (engine) {
        case GameEngineType::Unity: {
            UnityBuildConfig config;
            config.target = UnityBuildTarget::StandaloneWindows64;
            return m_unity->buildProjectAsync(config);
        }
        case GameEngineType::Unreal: {
            UnrealBuildSettings settings;
            return m_unreal->buildProjectAsync(settings);
        }
        default: return false;
    }
}

bool GameEngineManager::isBuildInProgress() const {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->isBuildInProgress();
        case GameEngineType::Unreal: return m_unreal->isBuildInProgress();
        default: return false;
    }
}

float GameEngineManager::getBuildProgress() const {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->getBuildProgress();
        case GameEngineType::Unreal: return m_unreal->getBuildProgress();
        default: return 0.0f;
    }
}

void GameEngineManager::cancelBuild() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  m_unity->cancelBuild(); break;
        case GameEngineType::Unreal: m_unreal->cancelBuild(); break;
        default: break;
    }
}

GameBuildResultUnified GameEngineManager::getLastBuildResult() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastBuildResult;
}

// ============================================================================
// Unified Play / PIE
// ============================================================================
bool GameEngineManager::enterPlayMode() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->enterPlayMode();
        case GameEngineType::Unreal: return m_unreal->startPIE();
        default: return false;
    }
}

bool GameEngineManager::exitPlayMode() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->exitPlayMode();
        case GameEngineType::Unreal: return m_unreal->stopPIE();
        default: return false;
    }
}

bool GameEngineManager::pausePlayMode() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->pausePlayMode();
        case GameEngineType::Unreal: return m_unreal->pausePIE();
        default: return false;
    }
}

bool GameEngineManager::resumePlayMode() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->enterPlayMode();
        case GameEngineType::Unreal: return m_unreal->resumePIE();
        default: return false;
    }
}

bool GameEngineManager::isInPlayMode() const {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->isInPlayMode();
        case GameEngineType::Unreal: return m_unreal->isInPIE();
        default: return false;
    }
}

// ============================================================================
// Unified Profiler
// ============================================================================
bool GameEngineManager::startProfiler() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->startProfiler();
        case GameEngineType::Unreal: return m_unreal->startProfiler();
        default: return false;
    }
}

bool GameEngineManager::stopProfiler() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->stopProfiler();
        case GameEngineType::Unreal: return m_unreal->stopProfiler();
        default: return false;
    }
}

bool GameEngineManager::isProfilerRunning() const {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->isProfilerRunning();
        case GameEngineType::Unreal: return m_unreal->isProfilerRunning();
        default: return false;
    }
}

GameProfilerSnapshotUnified GameEngineManager::getProfilerSnapshot() const {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return convertUnitySnapshot(m_unity->getProfilerSnapshot());
        case GameEngineType::Unreal: return convertUnrealSnapshot(m_unreal->getProfilerSnapshot());
        default: return GameProfilerSnapshotUnified{};
    }
}

// ============================================================================
// Unified Debug
// ============================================================================
bool GameEngineManager::startDebugSession() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->startDebugSession();
        case GameEngineType::Unreal: return m_unreal->startDebugSession();
        default: return false;
    }
}

bool GameEngineManager::stopDebugSession() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->stopDebugSession();
        case GameEngineType::Unreal: return m_unreal->stopDebugSession();
        default: return false;
    }
}

bool GameEngineManager::setBreakpoint(const std::string& file, int line,
                                       const std::string& condition) {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->setBreakpoint(file, line, condition);
        case GameEngineType::Unreal: return m_unreal->setBreakpoint(file, line, condition);
        default: return false;
    }
}

bool GameEngineManager::removeBreakpoint(const std::string& file, int line) {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->removeBreakpoint(file, line);
        case GameEngineType::Unreal: return m_unreal->removeBreakpoint(file, line);
        default: return false;
    }
}

bool GameEngineManager::debugContinue() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->debugContinue();
        case GameEngineType::Unreal: return m_unreal->debugContinue();
        default: return false;
    }
}

bool GameEngineManager::debugStepOver() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->debugStepOver();
        case GameEngineType::Unreal: return m_unreal->debugStepOver();
        default: return false;
    }
}

bool GameEngineManager::debugStepInto() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->debugStepInto();
        case GameEngineType::Unreal: return m_unreal->debugStepInto();
        default: return false;
    }
}

bool GameEngineManager::debugStepOut() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->debugStepOut();
        case GameEngineType::Unreal: return m_unreal->debugStepOut();
        default: return false;
    }
}

bool GameEngineManager::isDebugActive() const {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->isDebugSessionActive();
        case GameEngineType::Unreal: return m_unreal->isDebugSessionActive();
        default: return false;
    }
}

// ============================================================================
// Compile / Script
// ============================================================================
bool GameEngineManager::compile() {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->compileScripts();
        case GameEngineType::Unreal: return m_unreal->compileProject();
        default: return false;
    }
}

bool GameEngineManager::hasCompileErrors() const {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->hasCompileErrors();
        case GameEngineType::Unreal: return m_unreal->hasCompileErrors();
        default: return false;
    }
}

std::vector<std::string> GameEngineManager::getCompileErrorStrings() const {
    std::vector<std::string> result;
    switch (m_activeEngine) {
        case GameEngineType::Unity: {
            auto errors = m_unity->getCompileErrors();
            for (const auto& e : errors) {
                result.push_back(e.file + "(" + std::to_string(e.line) + "," +
                                 std::to_string(e.column) + "): " +
                                 (e.isWarning ? "warning" : "error") + " " +
                                 e.code + ": " + e.message);
            }
            break;
        }
        case GameEngineType::Unreal: {
            auto errors = m_unreal->getCompileErrors();
            for (const auto& e : errors) {
                result.push_back(e.file + "(" + std::to_string(e.line) + "): " +
                                 (e.isWarning ? "warning" : "error") + " " +
                                 e.code + ": " + e.message);
            }
            break;
        }
        default: break;
    }
    return result;
}

// ============================================================================
// Agentic AI
// ============================================================================
std::string GameEngineManager::generateAIProjectSummary() const {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->generateAIProjectSummary();
        case GameEngineType::Unreal: return m_unreal->generateAIProjectSummary();
        default: return "No game engine project active.\n";
    }
}

std::string GameEngineManager::generateAISceneDescription() const {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->generateAISceneDescription();
        case GameEngineType::Unreal: return m_unreal->generateAIWorldDescription();
        default: return "No game engine project active.\n";
    }
}

bool GameEngineManager::applyAISuggestion(const std::string& suggestion) {
    switch (m_activeEngine) {
        case GameEngineType::Unity:  return m_unity->applyAISuggestion(suggestion);
        case GameEngineType::Unreal: return m_unreal->applyAISuggestion(suggestion);
        default: return false;
    }
}

// ============================================================================
// Installation Discovery
// ============================================================================
std::vector<GameEngineManager::EngineInstallation> GameEngineManager::getAllInstallations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_installations;
}

bool GameEngineManager::refreshInstallations() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_installations.clear();

    // Unity installations
    auto unityVersions = m_unity->getInstalledVersions();
    for (const auto& ver : unityVersions) {
        EngineInstallation inst;
        inst.engine = GameEngineType::Unity;
        inst.version = ver;
        inst.path = m_unity->getEditorPath(ver);
        inst.isDefault = false;
        m_installations.push_back(inst);
    }

    // Unreal installations
    auto unrealVersions = m_unreal->getInstalledVersions();
    for (const auto& ver : unrealVersions) {
        EngineInstallation inst;
        inst.engine = GameEngineType::Unreal;
        inst.version = ver;
        inst.path = m_unreal->getEnginePath(ver);
        m_installations.push_back(inst);
    }

    log(("Found " + std::to_string(m_installations.size()) + " engine installations").c_str());
    return true;
}

// ============================================================================
// Status / Help
// ============================================================================
std::string GameEngineManager::getStatusString() const {
    std::ostringstream ss;
    ss << "=== Game Engine Manager ===\n";
    ss << "Initialized: " << (m_initialized.load() ? "Yes" : "No") << "\n";
    ss << "Active Engine: ";
    switch (m_activeEngine) {
        case GameEngineType::Unity:  ss << "Unity";  break;
        case GameEngineType::Unreal: ss << "Unreal"; break;
        default:                     ss << "None";   break;
    }
    ss << "\n";

    if (isProjectOpen()) {
        ss << "Project: " << getActiveProjectName() << "\n";
        ss << "Path: " << m_activeProjectPath << "\n";
        ss << "Build In Progress: " << (isBuildInProgress() ? "Yes" : "No") << "\n";
        ss << "Play Mode: " << (isInPlayMode() ? "Active" : "Inactive") << "\n";
        ss << "Profiler: " << (isProfilerRunning() ? "Running" : "Stopped") << "\n";
        ss << "Debug: " << (isDebugActive() ? "Active" : "Inactive") << "\n";
    }

    ss << "\nInstallations: " << m_installations.size() << "\n";
    for (const auto& inst : m_installations) {
        ss << "  " << (inst.engine == GameEngineType::Unity ? "[Unity]" : "[Unreal]")
           << " " << inst.version << " → " << inst.path << "\n";
    }

    return ss.str();
}

std::string GameEngineManager::getHelpText() const {
    return
        "=== Game Engine Manager Help ===\n\n"
        "Commands:\n"
        "  !engine detect <path>     - Auto-detect engine type\n"
        "  !engine open <path>       - Open game project (auto-detect)\n"
        "  !engine close             - Close current project\n"
        "  !engine status            - Show status\n"
        "  !engine build             - Build (default settings)\n"
        "  !engine play              - Enter play mode / PIE\n"
        "  !engine stop              - Exit play mode\n"
        "  !engine pause             - Pause play mode\n"
        "  !engine compile           - Compile scripts / C++\n"
        "  !engine profiler start    - Start profiler\n"
        "  !engine profiler stop     - Stop profiler\n"
        "  !engine debug start       - Start debug session\n"
        "  !engine debug stop        - Stop debug session\n"
        "  !engine bp <file> <line>  - Set breakpoint\n"
        "  !engine ai summary        - AI project summary\n"
        "  !engine ai scene          - AI scene description\n"
        "  !engine installations     - List engine installations\n\n"
        "Engine-specific commands:\n"
        "  !unity ...                - Unity-specific commands\n"
        "  !unreal ...               - Unreal-specific commands\n\n"
        "Type '!unity help' or '!unreal help' for engine-specific help.\n";
}

std::string GameEngineManager::toJSON() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"initialized\": " << (m_initialized.load() ? "true" : "false") << ",\n";
    json << "  \"activeEngine\": ";
    switch (m_activeEngine) {
        case GameEngineType::Unity:  json << "\"Unity\"";  break;
        case GameEngineType::Unreal: json << "\"Unreal\""; break;
        default:                     json << "\"None\"";   break;
    }
    json << ",\n";
    json << "  \"projectOpen\": " << (isProjectOpen() ? "true" : "false") << ",\n";
    json << "  \"projectName\": \"" << getActiveProjectName() << "\",\n";
    json << "  \"projectPath\": \"" << m_activeProjectPath << "\",\n";
    json << "  \"buildInProgress\": " << (isBuildInProgress() ? "true" : "false") << ",\n";
    json << "  \"playMode\": " << (isInPlayMode() ? "true" : "false") << ",\n";
    json << "  \"profiling\": " << (isProfilerRunning() ? "true" : "false") << ",\n";
    json << "  \"debugging\": " << (isDebugActive() ? "true" : "false") << ",\n";
    json << "  \"installations\": " << m_installations.size() << "\n";
    json << "}";
    return json.str();
}

// ============================================================================
// Command Routing
// ============================================================================
bool GameEngineManager::handleCommand(const std::string& command, const std::string& args) {
    if (command == "detect") {
        auto result = detectProject(args);
        log(result.detail.c_str());
        return result.isValid;
    }
    if (command == "open")   return openProject(args);
    if (command == "close")  return closeProject();
    if (command == "status") { log(getStatusString().c_str(), 0); return true; }
    if (command == "build")  { auto r = buildProject(GameBuildRequest{}); return r.success; }
    if (command == "play")   return enterPlayMode();
    if (command == "stop")   return exitPlayMode();
    if (command == "pause")  return pausePlayMode();
    if (command == "resume") return resumePlayMode();
    if (command == "compile") return compile();
    if (command == "profiler_start") return startProfiler();
    if (command == "profiler_stop")  return stopProfiler();
    if (command == "debug_start")    return startDebugSession();
    if (command == "debug_stop")     return stopDebugSession();
    if (command == "debug_continue") return debugContinue();
    if (command == "step_over")      return debugStepOver();
    if (command == "step_into")      return debugStepInto();
    if (command == "step_out")       return debugStepOut();
    if (command == "ai_summary")    { log(generateAIProjectSummary().c_str(), 0); return true; }
    if (command == "ai_scene")      { log(generateAISceneDescription().c_str(), 0); return true; }
    if (command == "help")          { log(getHelpText().c_str(), 0); return true; }
    if (command == "installations") {
        for (const auto& inst : getAllInstallations()) {
            std::string msg = (inst.engine == GameEngineType::Unity ? "[Unity] " : "[Unreal] ") +
                              inst.version + " → " + inst.path;
            log(msg.c_str(), 0);
        }
        return true;
    }

    log(("Unknown game engine command: " + command).c_str(), 2);
    return false;
}

std::vector<std::string> GameEngineManager::getAvailableCommands() const {
    return {
        "detect", "open", "close", "status", "build", "play", "stop", "pause",
        "resume", "compile", "profiler_start", "profiler_stop",
        "debug_start", "debug_stop", "debug_continue", "step_over", "step_into",
        "step_out", "ai_summary", "ai_scene", "help", "installations"
    };
}

// ============================================================================
// Internal Helpers
// ============================================================================
void GameEngineManager::log(const char* message, int severity) {
    if (m_logCallback) {
        m_logCallback(message, severity, m_activeEngine);
    }
}

GameProfilerSnapshotUnified GameEngineManager::convertUnitySnapshot(
    const UnityProfilerSnapshot& s) const {
    GameProfilerSnapshotUnified u;
    u.engine        = GameEngineType::Unity;
    u.frameTimeMs   = s.cpuFrameTimeMs;
    u.cpuTimeMs     = s.cpuFrameTimeMs;
    u.gpuTimeMs     = s.gpuFrameTimeMs;
    u.scriptTimeMs  = s.scriptsMs;
    u.physicsTimeMs = s.physicsDeltaMs;
    u.renderTimeMs  = s.renderingMs;
    u.drawCalls     = s.drawCalls;
    u.triangles     = s.triangles;
    u.totalMemoryMB = s.totalMemoryMB;
    u.textureMemoryMB = s.textureMemoryMB;
    u.meshMemoryMB  = s.meshMemoryMB;
    u.fps           = s.fps;
    u.frameNumber   = s.frameNumber;
    return u;
}

GameProfilerSnapshotUnified GameEngineManager::convertUnrealSnapshot(
    const UnrealProfilerSnapshot& s) const {
    GameProfilerSnapshotUnified u;
    u.engine        = GameEngineType::Unreal;
    u.frameTimeMs   = s.frameTimeMs;
    u.cpuTimeMs     = s.gameThreadMs;
    u.gpuTimeMs     = s.gpuMs;
    u.scriptTimeMs  = s.blueprintTimeMs;
    u.physicsTimeMs = s.physicsTimeMs;
    u.renderTimeMs  = s.renderThreadMs;
    u.drawCalls     = s.drawCalls;
    u.triangles     = s.triangles;
    u.totalMemoryMB = s.totalMemoryMB;
    u.textureMemoryMB = s.textureMemoryMB;
    u.meshMemoryMB  = s.meshMemoryMB;
    u.fps           = s.fps;
    u.frameNumber   = s.frameNumber;
    return u;
}

GameBuildResultUnified GameEngineManager::convertUnityBuildResult(
    const UnityBuildResult& r) const {
    GameBuildResultUnified u;
    u.engine       = GameEngineType::Unity;
    u.success      = r.success;
    u.warningCount = r.warningCount;
    u.errorCount   = r.errorCount;
    u.buildTimeSec = r.buildTimeSec;
    u.outputPath   = r.outputPath;
    u.logOutput    = r.logOutput;
    u.errors       = r.errors;
    u.warnings     = r.warnings;
    return u;
}

GameBuildResultUnified GameEngineManager::convertUnrealBuildResult(
    const UnrealBuildResult& r) const {
    GameBuildResultUnified u;
    u.engine       = GameEngineType::Unreal;
    u.success      = r.success;
    u.warningCount = r.warningCount;
    u.errorCount   = r.errorCount;
    u.buildTimeSec = r.buildTimeSec;
    u.logOutput    = r.logOutput;
    u.errors       = r.errors;
    u.warnings     = r.warnings;
    return u;
}

} // namespace GameEngine
} // namespace RawrXD
