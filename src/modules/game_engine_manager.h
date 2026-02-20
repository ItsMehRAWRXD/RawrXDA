#pragma once
// ============================================================================
// Game Engine Manager — Unified coordinator for Unity & Unreal Engine
// Provides a single entry point for the IDE to manage game engine projects,
// route commands, and coordinate between engine-specific integrations.
// No exceptions. No std::function. Function pointer callbacks only.
// ============================================================================

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <cstdint>

#include "unity_engine_integration.h"
#include "unreal_engine_integration.h"

namespace RawrXD {
namespace GameEngine {

// ============================================================================
// Supported Game Engine Type
// ============================================================================
enum class GameEngineType {
    None = 0,
    Unity,
    Unreal
};

// ============================================================================
// Engine Detection Result
// ============================================================================
struct EngineDetectionResult {
    GameEngineType engine = GameEngineType::None;
    std::string version;
    std::string projectPath;
    std::string projectName;
    bool isValid = false;
    std::string detail;        // Human-readable summary
};

// ============================================================================
// Unified Build Request
// ============================================================================
struct GameBuildRequest {
    GameEngineType engine       = GameEngineType::None;
    std::string platform;        // "Win64", "Android", "iOS", "WebGL", etc.
    std::string configuration;   // "Development", "Shipping", "Debug", "Release"
    std::string outputPath;
    bool developmentBuild        = false;
    bool scriptDebugging         = false;
    bool autoProfile             = false;
    std::vector<std::string> extraArgs;
};

// ============================================================================
// Unified Build Result
// ============================================================================
struct GameBuildResultUnified {
    bool success         = false;
    GameEngineType engine = GameEngineType::None;
    int warningCount     = 0;
    int errorCount       = 0;
    double buildTimeSec  = 0.0;
    std::string outputPath;
    std::string logOutput;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// ============================================================================
// Unified Profiler Snapshot
// ============================================================================
struct GameProfilerSnapshotUnified {
    GameEngineType engine       = GameEngineType::None;
    double frameTimeMs          = 0.0;
    double cpuTimeMs            = 0.0;
    double gpuTimeMs            = 0.0;
    double scriptTimeMs         = 0.0;
    double physicsTimeMs        = 0.0;
    double renderTimeMs         = 0.0;
    int drawCalls               = 0;
    int triangles               = 0;
    double totalMemoryMB        = 0.0;
    double textureMemoryMB      = 0.0;
    double meshMemoryMB         = 0.0;
    double fps                  = 0.0;
    uint64_t frameNumber        = 0;
};

// ============================================================================
// Callbacks
// ============================================================================
typedef void (*GameEngineLogCallback)(const char* message, int severity, GameEngineType engine);
typedef void (*GameEngineBuildCallback)(float progress, const char* step, GameEngineType engine);
typedef void (*GameEngineProfilerCallback)(const GameProfilerSnapshotUnified* snapshot);

// ============================================================================
// GameEngineManager — Unified Coordinator
// ============================================================================
class GameEngineManager {
public:
    GameEngineManager();
    ~GameEngineManager();

    // ── Lifecycle ──
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(); }

    // ── Engine Detection ──
    EngineDetectionResult detectProject(const std::string& path) const;
    GameEngineType identifyEngine(const std::string& projectPath) const;
    bool isUnityProject(const std::string& path) const;
    bool isUnrealProject(const std::string& path) const;

    // ── Project Management ──
    bool openProject(const std::string& path);
    bool closeProject();
    bool isProjectOpen() const;
    GameEngineType getActiveEngine() const;
    std::string getActiveProjectName() const;
    std::string getActiveProjectPath() const;

    // ── Engine Access ──
    UnityEngineIntegration* getUnity()   { return m_unity.get(); }
    UnrealEngineIntegration* getUnreal() { return m_unreal.get(); }
    const UnityEngineIntegration* getUnity() const   { return m_unity.get(); }
    const UnrealEngineIntegration* getUnreal() const { return m_unreal.get(); }

    // ── Unified Build ──
    GameBuildResultUnified buildProject(const GameBuildRequest& request);
    bool buildProjectAsync(const GameBuildRequest& request);
    bool isBuildInProgress() const;
    float getBuildProgress() const;
    void cancelBuild();
    GameBuildResultUnified getLastBuildResult() const;

    // ── Unified Play/PIE ──
    bool enterPlayMode();
    bool exitPlayMode();
    bool pausePlayMode();
    bool resumePlayMode();
    bool isInPlayMode() const;

    // ── Unified Profiler ──
    bool startProfiler();
    bool stopProfiler();
    bool isProfilerRunning() const;
    GameProfilerSnapshotUnified getProfilerSnapshot() const;

    // ── Unified Debug ──
    bool startDebugSession();
    bool stopDebugSession();
    bool setBreakpoint(const std::string& file, int line, const std::string& condition = "");
    bool removeBreakpoint(const std::string& file, int line);
    bool debugContinue();
    bool debugStepOver();
    bool debugStepInto();
    bool debugStepOut();
    bool isDebugActive() const;

    // ── Compile / Script ──
    bool compile();
    bool hasCompileErrors() const;
    std::vector<std::string> getCompileErrorStrings() const;

    // ── Agentic AI ──
    std::string generateAIProjectSummary() const;
    std::string generateAISceneDescription() const;
    bool applyAISuggestion(const std::string& suggestion);

    // ── Installation Discovery ──
    struct EngineInstallation {
        GameEngineType engine;
        std::string version;
        std::string path;
        bool isDefault = false;
    };
    std::vector<EngineInstallation> getAllInstallations() const;
    bool refreshInstallations();

    // ── Callbacks ──
    void setLogCallback(GameEngineLogCallback cb)         { m_logCallback = cb; }
    void setBuildCallback(GameEngineBuildCallback cb)      { m_buildCallback = cb; }
    void setProfilerCallback(GameEngineProfilerCallback cb) { m_profilerCallback = cb; }

    // ── Status / Help ──
    std::string getStatusString() const;
    std::string getHelpText() const;
    std::string toJSON() const;

    // ── Command Routing (for IDE integration) ──
    bool handleCommand(const std::string& command, const std::string& args);
    std::vector<std::string> getAvailableCommands() const;

private:
    void log(const char* message, int severity = 1);
    GameProfilerSnapshotUnified convertUnitySnapshot(const UnityProfilerSnapshot& s) const;
    GameProfilerSnapshotUnified convertUnrealSnapshot(const UnrealProfilerSnapshot& s) const;
    GameBuildResultUnified convertUnityBuildResult(const UnityBuildResult& r) const;
    GameBuildResultUnified convertUnrealBuildResult(const UnrealBuildResult& r) const;

    // ── State ──
    std::atomic<bool> m_initialized{false};
    mutable std::mutex m_mutex;

    std::unique_ptr<UnityEngineIntegration> m_unity;
    std::unique_ptr<UnrealEngineIntegration> m_unreal;

    GameEngineType m_activeEngine = GameEngineType::None;
    std::string m_activeProjectPath;
    GameBuildResultUnified m_lastBuildResult;

    std::vector<EngineInstallation> m_installations;

    // Callbacks
    GameEngineLogCallback      m_logCallback = nullptr;
    GameEngineBuildCallback    m_buildCallback = nullptr;
    GameEngineProfilerCallback m_profilerCallback = nullptr;
};

// ============================================================================
// Global Game Engine Manager
// ============================================================================
extern std::unique_ptr<GameEngineManager> g_gameEngineManager;

} // namespace GameEngine
} // namespace RawrXD
