#pragma once
// ============================================================================
// Unity Engine Integration — Full IDE Integration for Unity Projects
// Provides: Project management, scene editing, C# scripting bridge,
//           asset pipeline, build system, profiler, and debug adapter.
// No exceptions. PatchResult-compatible error reporting.
// ============================================================================

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#else
typedef void* HANDLE;
#endif

// ── Forward declarations ──
struct PatchResult;

namespace RawrXD {
namespace GameEngine {

// ============================================================================
// Unity Project Descriptor
// ============================================================================
struct UnityProjectInfo {
    std::string projectPath;          // Root path of the Unity project
    std::string projectName;
    std::string unityVersion;         // e.g. "2023.3.0f1"
    std::string editorPath;           // Path to Unity Editor executable
    std::string scriptsPath;          // Assets/Scripts
    std::string assetsPath;           // Assets/
    std::string packagesPath;         // Packages/
    std::string projectSettingsPath;  // ProjectSettings/
    std::string libraryPath;          // Library/ (cached)
    bool isValid         = false;
    bool isOpen          = false;
    bool hasCompileErrors = false;
    int  scriptCount     = 0;
    int  sceneCount      = 0;
    int  prefabCount     = 0;
    int  assetCount      = 0;
};

// ============================================================================
// Unity Scene Hierarchy Node
// ============================================================================
struct UnitySceneNode {
    std::string name;
    std::string tag;
    std::string layer;
    int         instanceId = 0;
    bool        active     = true;
    bool        isStatic   = false;
    std::vector<std::string> components;   // Component type names
    std::vector<UnitySceneNode> children;
};

// ============================================================================
// Unity Scene Descriptor
// ============================================================================
struct UnitySceneInfo {
    std::string scenePath;   // Assets/Scenes/MainScene.unity
    std::string sceneName;
    bool isDirty   = false;
    bool isLoaded  = false;
    int  objectCount = 0;
    std::vector<UnitySceneNode> rootObjects;
};

// ============================================================================
// Unity Asset Descriptor
// ============================================================================
enum class UnityAssetType {
    Unknown = 0,
    Script,         // .cs
    Shader,         // .shader, .compute
    Material,       // .mat
    Texture,        // .png, .jpg, .tga, .psd, .exr
    Model,          // .fbx, .obj, .blend
    Animation,      // .anim
    AnimController, // .controller
    Prefab,         // .prefab
    Scene,          // .unity
    Audio,          // .wav, .mp3, .ogg
    Font,           // .ttf, .otf
    ScriptableObject,
    TextAsset,
    Tilemap,
    SpritePack,
    UIDocument,     // .uxml (UI Toolkit)
    StyleSheet,     // .uss
    VisualEffectGraph,
    ShaderGraph,
    RenderPipeline,
    NavMesh,
    TerrainData
};

struct UnityAssetInfo {
    std::string path;
    std::string guid;     // Unity GUID from .meta file
    std::string name;
    UnityAssetType type = UnityAssetType::Unknown;
    uint64_t sizeBytes  = 0;
    bool isImported     = false;
    std::string importerType;
    std::vector<std::string> dependencies;
    std::vector<std::string> labels;
};

// ============================================================================
// Unity Build Configuration
// ============================================================================
enum class UnityBuildTarget {
    StandaloneWindows64 = 0,
    StandaloneLinux64,
    StandaloneMacOSX,
    iOS,
    Android,
    WebGL,
    PS5,
    XboxSeriesX,
    Switch,
    WSA,        // Universal Windows Platform
    EmbeddedLinux,
    QNX
};

enum class UnityScriptingBackend {
    Mono = 0,
    IL2CPP
};

struct UnityBuildConfig {
    UnityBuildTarget target          = UnityBuildTarget::StandaloneWindows64;
    UnityScriptingBackend backend    = UnityScriptingBackend::IL2CPP;
    bool developmentBuild            = false;
    bool autoConnectProfiler         = false;
    bool deepProfilingSupport        = false;
    bool scriptDebugging             = false;
    bool compression                 = true;
    std::string outputPath;
    std::string defineSymbols;
    std::vector<std::string> scenesInBuild;
};

// ============================================================================
// Unity Build Result
// ============================================================================
struct UnityBuildResult {
    bool success          = false;
    int  warningCount     = 0;
    int  errorCount       = 0;
    double buildTimeSec   = 0.0;
    uint64_t buildSizeBytes = 0;
    std::string outputPath;
    std::string logOutput;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// ============================================================================
// Unity Profiler Snapshot
// ============================================================================
struct UnityProfilerSnapshot {
    double cpuFrameTimeMs   = 0.0;
    double gpuFrameTimeMs   = 0.0;
    double physicsDeltaMs   = 0.0;
    double renderingMs      = 0.0;
    double scriptsMs        = 0.0;
    double animationMs      = 0.0;
    double uiMs             = 0.0;
    int    drawCalls        = 0;
    int    triangles        = 0;
    int    vertices         = 0;
    int    setPassCalls     = 0;
    int    batchesTotal     = 0;
    int    batchesSaved     = 0;
    double totalMemoryMB    = 0.0;
    double textureMemoryMB  = 0.0;
    double meshMemoryMB     = 0.0;
    double audioMemoryMB    = 0.0;
    double gcAllocKB        = 0.0;
    int    gcCollections    = 0;
    double fps              = 0.0;
    uint64_t frameNumber    = 0;
};

// ============================================================================
// Unity C# Compile Error
// ============================================================================
struct UnityCSharpError {
    std::string file;
    int line     = 0;
    int column   = 0;
    std::string code;      // e.g. "CS0246"
    std::string message;
    bool isWarning = false;
};

// ============================================================================
// Unity Debug Adapter — DAP Bridge
// ============================================================================
struct UnityDebugBreakpoint {
    std::string file;
    int line         = 0;
    bool enabled     = true;
    std::string condition;
    int hitCount     = 0;
    bool verified    = false;
};

struct UnityDebugStackFrame {
    int id               = 0;
    std::string name;
    std::string source;
    int line             = 0;
    int column           = 0;
    std::string moduleName;
};

struct UnityDebugVariable {
    std::string name;
    std::string value;
    std::string type;
    int variablesReference = 0;  // >0 means has children
};

// ============================================================================
// Unity Editor Command — Execute C# static methods via -executeMethod
// ============================================================================
struct UnityEditorCommand {
    std::string methodName;   // e.g. "MyBuildScript.PerformBuild"
    std::vector<std::string> args;
    int timeoutMs = 120000;   // 2 minutes default
};

// ============================================================================
// Unity Integration Callbacks (function pointers, no std::function)
// ============================================================================
typedef void (*UnityLogCallback)(const char* message, int severity);
typedef void (*UnityBuildProgressCallback)(float progress, const char* step);
typedef void (*UnityProfilerCallback)(const UnityProfilerSnapshot* snapshot);
typedef void (*UnityCompileCallback)(int errorCount, int warningCount);

// ============================================================================
// UnityEngineIntegration — Main Integration Class
// ============================================================================
class UnityEngineIntegration {
public:
    UnityEngineIntegration();
    ~UnityEngineIntegration();

    // ── Lifecycle ──
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(); }

    // ── Project Management ──
    bool openProject(const std::string& projectPath);
    bool closeProject();
    bool createProject(const std::string& path, const std::string& name,
                       const std::string& templateId = "3d-core");
    bool isProjectOpen() const;
    UnityProjectInfo getProjectInfo() const;
    bool refreshProject();
    
    // ── Unity Editor Discovery ──
    bool detectUnityInstallations();
    std::vector<std::string> getInstalledVersions() const;
    std::string getEditorPath(const std::string& version = "") const;
    bool setPreferredVersion(const std::string& version);

    // ── Scene Management ──
    std::vector<UnitySceneInfo> getScenes() const;
    UnitySceneInfo getActiveScene() const;
    bool openScene(const std::string& scenePath);
    bool saveScene(const std::string& scenePath = "");
    bool createScene(const std::string& scenePath);
    std::vector<UnitySceneNode> getSceneHierarchy(const std::string& scenePath = "") const;

    // ── Asset Pipeline ──
    std::vector<UnityAssetInfo> getAssets(const std::string& directory = "Assets") const;
    UnityAssetInfo getAssetInfo(const std::string& assetPath) const;
    bool importAsset(const std::string& sourcePath, const std::string& destPath = "");
    bool deleteAsset(const std::string& assetPath);
    bool moveAsset(const std::string& oldPath, const std::string& newPath);
    bool refreshAssetDatabase();
    std::vector<std::string> findAssetsByType(UnityAssetType type) const;
    std::vector<std::string> findAssetsByLabel(const std::string& label) const;
    std::string resolveGUID(const std::string& guid) const;

    // ── C# Script Bridge ──
    bool createScript(const std::string& scriptName, const std::string& directory = "Assets/Scripts",
                      const std::string& templateType = "MonoBehaviour");
    std::string generateScriptTemplate(const std::string& className,
                                        const std::string& baseClass = "MonoBehaviour",
                                        const std::string& namespaceName = "") const;
    bool compileScripts();
    std::vector<UnityCSharpError> getCompileErrors() const;
    bool hasCompileErrors() const;
    std::vector<std::string> getScriptClasses() const;
    std::vector<std::string> getMonoBehaviours() const;

    // ── Build System ──
    UnityBuildResult buildProject(const UnityBuildConfig& config);
    bool buildProjectAsync(const UnityBuildConfig& config);
    bool isBuildInProgress() const;
    float getBuildProgress() const;
    void cancelBuild();
    UnityBuildConfig getDefaultBuildConfig() const;
    std::vector<std::string> getSupportedBuildTargets() const;
    UnityBuildResult getLastBuildResult() const;

    // ── Play Mode Control ──
    bool enterPlayMode();
    bool exitPlayMode();
    bool pausePlayMode();
    bool stepFrame();
    bool isInPlayMode() const;
    bool isPaused() const;

    // ── Profiler ──
    bool startProfiler();
    bool stopProfiler();
    bool isProfilerRunning() const;
    UnityProfilerSnapshot getProfilerSnapshot() const;
    std::vector<UnityProfilerSnapshot> getProfilerHistory(int frameCount = 300) const;
    bool saveProfilerData(const std::string& outputPath);
    bool connectProfiler(const std::string& ip = "127.0.0.1", int port = 34999);

    // ── Debug Adapter (DAP) ──
    bool startDebugSession(int port = 56000);
    bool stopDebugSession();
    bool setBreakpoint(const std::string& file, int line, const std::string& condition = "");
    bool removeBreakpoint(const std::string& file, int line);
    std::vector<UnityDebugBreakpoint> getBreakpoints() const;
    bool debugContinue();
    bool debugStepOver();
    bool debugStepInto();
    bool debugStepOut();
    std::vector<UnityDebugStackFrame> getCallStack() const;
    std::vector<UnityDebugVariable> getLocals(int frameId = 0) const;
    std::string evaluateExpression(const std::string& expr, int frameId = 0) const;
    bool isDebugSessionActive() const;

    // ── Editor Commands ──
    bool executeEditorMethod(const UnityEditorCommand& cmd);
    std::string executeEditorMethodSync(const UnityEditorCommand& cmd);

    // ── Package Manager ──
    std::vector<std::string> getInstalledPackages() const;
    bool addPackage(const std::string& packageId, const std::string& version = "");
    bool removePackage(const std::string& packageId);
    bool updatePackage(const std::string& packageId);
    std::string getPackageManifestPath() const;

    // ── Agentic AI Integration ──
    std::string generateAISceneDescription() const;
    std::string generateAIProjectSummary() const;
    bool applyAISuggestion(const std::string& suggestion);
    bool executeAIGeneratedScript(const std::string& csharpCode);

    // ── Callbacks (function pointers only — no std::function) ──
    void setLogCallback(UnityLogCallback cb)              { m_logCallback = cb; }
    void setBuildProgressCallback(UnityBuildProgressCallback cb) { m_buildProgressCallback = cb; }
    void setProfilerCallback(UnityProfilerCallback cb)    { m_profilerCallback = cb; }
    void setCompileCallback(UnityCompileCallback cb)      { m_compileCallback = cb; }

    // ── Status / Help ──
    std::string getStatusString() const;
    std::string getHelpText() const;
    std::string getIntegrationVersion() const { return "1.0.0"; }
    
    // ── Serialization ──
    std::string toJSON() const;
    bool loadConfig(const std::string& jsonPath);
    bool saveConfig(const std::string& jsonPath) const;

private:
    // ── Internal Helpers ──
    bool detectProjectStructure(const std::string& path);
    bool parseProjectSettings();
    bool parsePackageManifest();
    void scanAssetFolder(const std::string& directory, std::vector<UnityAssetInfo>& out) const;
    UnityAssetType classifyAsset(const std::string& extension) const;
    std::string readMetaGUID(const std::string& metaFilePath) const;
    bool launchUnityEditor(const std::string& args);
    bool launchUnityBatchMode(const std::string& args, std::string& output, int timeoutMs = 300000);
    void parseCompileLog(const std::string& log);
    void parseBuildLog(const std::string& log, UnityBuildResult& result);
    std::string buildTargetToString(UnityBuildTarget target) const;
    std::string scriptingBackendToString(UnityScriptingBackend backend) const;

    // ── State ──
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_buildInProgress{false};
    std::atomic<float> m_buildProgress{0.0f};
    std::atomic<bool> m_inPlayMode{false};
    std::atomic<bool> m_isPaused{false};
    std::atomic<bool> m_profilerRunning{false};
    std::atomic<bool> m_debugSessionActive{false};

    mutable std::mutex m_mutex;
    mutable std::mutex m_profilerMutex;
    mutable std::mutex m_debugMutex;

    UnityProjectInfo m_projectInfo;
    UnityBuildResult m_lastBuildResult;
    UnityBuildConfig m_currentBuildConfig;

    std::vector<UnityCSharpError> m_compileErrors;
    std::vector<UnityDebugBreakpoint> m_breakpoints;
    std::vector<UnityProfilerSnapshot> m_profilerHistory;
    std::vector<std::string> m_installedVersions;
    std::unordered_map<std::string, std::string> m_guidToPath;

    std::string m_preferredVersion;
    std::string m_unityEditorPath;
    std::string m_configPath;

    // Callbacks (function pointers)
    UnityLogCallback           m_logCallback = nullptr;
    UnityBuildProgressCallback m_buildProgressCallback = nullptr;
    UnityProfilerCallback      m_profilerCallback = nullptr;
    UnityCompileCallback       m_compileCallback = nullptr;

    // DAP state
    HANDLE m_debugProcess = nullptr;
    int m_debugPort = 56000;
};

} // namespace GameEngine
} // namespace RawrXD
