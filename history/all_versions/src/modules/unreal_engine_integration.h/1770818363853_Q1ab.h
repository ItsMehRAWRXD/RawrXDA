#pragma once
// ============================================================================
// Unreal Engine Integration — Full IDE Integration for Unreal Engine Projects
// Provides: UProject management, Blueprint support, C++ build system (UBT),
//           level editing, asset browser, Live Coding, profiler, and debug.
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

namespace RawrXD {
namespace GameEngine {

// ============================================================================
// Unreal Project Descriptor
// ============================================================================
struct UnrealProjectInfo {
    std::string projectPath;         // Root path (contains .uproject)
    std::string projectName;
    std::string uprojectFilePath;    // Full path to .uproject file
    std::string engineVersion;       // e.g. "5.4.0"
    std::string enginePath;          // Root of UE installation
    std::string sourcePath;          // Source/
    std::string contentPath;         // Content/
    std::string configPath;          // Config/
    std::string pluginsPath;         // Plugins/
    std::string intermediatesPath;   // Intermediate/
    std::string binariesPath;        // Binaries/
    std::string savedPath;           // Saved/
    bool isValid           = false;
    bool isOpen            = false;
    bool isBlueprintOnly   = false;  // No C++ source
    bool hasCompileErrors  = false;
    int  cppFileCount      = 0;
    int  headerFileCount   = 0;
    int  blueprintCount    = 0;
    int  levelCount        = 0;
    int  pluginCount       = 0;
    int  assetCount        = 0;
    std::string targetPlatform;       // "Win64", "Linux", etc.
    std::string buildConfiguration;   // "Development", "Shipping", etc.
};

// ============================================================================
// Unreal Module Descriptor (from .Build.cs)
// ============================================================================
struct UnrealModuleInfo {
    std::string name;
    std::string type;           // "Runtime", "Editor", "Developer", "Program"
    std::string loadingPhase;   // "Default", "PreDefault", "PostConfigInit"
    std::string directory;
    std::vector<std::string> publicDependencies;
    std::vector<std::string> privateDependencies;
    std::vector<std::string> publicIncludePaths;
    int sourceFileCount = 0;
};

// ============================================================================
// Unreal Level / World Descriptor
// ============================================================================
struct UnrealActorInfo {
    std::string name;
    std::string className;        // e.g. "AStaticMeshActor", "ACharacter"
    std::string label;
    std::string folderPath;       // Editor folder in world outliner
    bool isHidden   = false;
    bool isSelected = false;
    double locationX = 0.0, locationY = 0.0, locationZ = 0.0;
    double rotationP = 0.0, rotationY = 0.0, rotationR = 0.0;
    double scaleX = 1.0, scaleY = 1.0, scaleZ = 1.0;
    std::vector<std::string> components;
    std::vector<std::string> tags;
};

struct UnrealLevelInfo {
    std::string levelPath;       // /Game/Maps/MainLevel
    std::string levelName;
    bool isPersistent = false;
    bool isLoaded     = false;
    bool isVisible    = true;
    bool isDirty      = false;
    int  actorCount   = 0;
    std::vector<UnrealActorInfo> actors;
    std::vector<std::string> subLevels;
};

// ============================================================================
// Unreal Asset Descriptor
// ============================================================================
enum class UnrealAssetType {
    Unknown = 0,
    Blueprint,          // .uasset (Blueprint)
    BlueprintInterface,
    CppClass,           // .h/.cpp
    Level,              // .umap
    Material,           // .uasset (Material)
    MaterialInstance,
    MaterialFunction,
    StaticMesh,         // .uasset (StaticMesh)
    SkeletalMesh,
    Texture,            // .uasset (Texture2D)
    Sound,              // .uasset (SoundWave)
    SoundCue,
    MetaSoundsPatch,
    Animation,          // .uasset (AnimSequence)
    AnimBlueprint,
    AnimMontage,
    Skeleton,
    PhysicsAsset,
    Particle,           // Niagara or Cascade
    NiagaraSystem,
    NiagaraEmitter,
    WidgetBlueprint,    // UMG Widget
    DataTable,
    DataAsset,
    CurveFloat,
    Enum,
    Struct,
    SubsystemBlueprint,
    GameplayAbility,
    GameplayEffect,
    GameplayTag,
    AIBehaviorTree,
    AIBlackboard,
    MediaSource,
    LevelSequence,
    FoliageType,
    LandscapeLayer,
    PhysicalMaterial,
    PCG,                // Procedural Content Generation
    WorldPartition
};

struct UnrealAssetInfo {
    std::string objectPath;      // /Game/Meshes/SM_Chair
    std::string diskPath;        // Content/Meshes/SM_Chair.uasset
    std::string name;
    std::string className;       // Blueprint class name
    UnrealAssetType type  = UnrealAssetType::Unknown;
    uint64_t sizeBytes    = 0;
    bool isEngineContent  = false;
    bool isPluginContent  = false;
    std::vector<std::string> referencedBy;
    std::vector<std::string> referencesTo;
    std::vector<std::string> tags;
};

// ============================================================================
// Unreal Build Configuration
// ============================================================================
enum class UnrealBuildTarget {
    Win64 = 0,
    Linux,
    Mac,
    Android,
    IOS,
    PS5,
    XboxSeriesX,
    Switch,
    HoloLens,
    TVOS
};

enum class UnrealBuildConfig {
    Debug = 0,
    DebugGame,
    Development,
    Shipping,
    Test
};

enum class UnrealBuildTargetType {
    Game = 0,
    Client,
    Server,
    Editor,
    Program
};

struct UnrealBuildSettings {
    UnrealBuildTarget platform          = UnrealBuildTarget::Win64;
    UnrealBuildConfig configuration     = UnrealBuildConfig::Development;
    UnrealBuildTargetType targetType    = UnrealBuildTargetType::Editor;
    bool useUnityBuild                  = true;  // UBT Unity Build (batched compilation)
    bool usePCH                         = true;
    bool useSharedPCH                   = true;
    bool enableLiveCoding               = true;
    bool enableHotReload                = false;
    int  maxParallelActions             = 0;     // 0 = auto
    std::string customBuildArgs;
    std::string cookedPlatformDir;
    std::vector<std::string> extraModules;
    std::vector<std::string> disabledPlugins;
};

// ============================================================================
// Unreal Build Result
// ============================================================================
struct UnrealBuildResult {
    bool success          = false;
    int  warningCount     = 0;
    int  errorCount       = 0;
    double buildTimeSec   = 0.0;
    int  compiledFiles    = 0;
    int  totalFiles       = 0;
    int  linkedModules    = 0;
    std::string outputBinary;
    std::string logOutput;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<std::string> compiledModules;
};

// ============================================================================
// Unreal C++ Compile Error
// ============================================================================
struct UnrealCppError {
    std::string file;
    int line     = 0;
    int column   = 0;
    std::string code;       // e.g. "C2065", "LNK2019"
    std::string message;
    bool isWarning = false;
    bool isLinkerError = false;
    std::string module;     // Which module the error belongs to
};

// ============================================================================
// Unreal Profiler / Insights Data
// ============================================================================
struct UnrealProfilerSnapshot {
    double gameThreadMs      = 0.0;
    double renderThreadMs    = 0.0;
    double rhiThreadMs       = 0.0;
    double gpuMs             = 0.0;
    double frameTimeMs       = 0.0;
    int    drawCalls         = 0;
    int    triangles         = 0;
    int    meshDrawCommands  = 0;
    int    primitives        = 0;
    double physicsTimeMs     = 0.0;
    double animationTimeMs   = 0.0;
    double niagaraTimeMs     = 0.0;
    double blueprintTimeMs   = 0.0;
    double totalMemoryMB     = 0.0;
    double textureMemoryMB   = 0.0;
    double meshMemoryMB      = 0.0;
    double physicsMemoryMB   = 0.0;
    double audioMemoryMB     = 0.0;
    double streamingPoolMB   = 0.0;
    double fps               = 0.0;
    uint64_t frameNumber     = 0;
    // Unreal Insights specific
    int    activeTimers      = 0;
    int    activeCounters    = 0;
};

// ============================================================================
// Blueprint Node Descriptor (for AI-assisted Blueprint editing)
// ============================================================================
struct UnrealBlueprintNode {
    std::string nodeId;
    std::string nodeType;            // "FunctionCall", "Event", "Variable", "Branch", etc.
    std::string displayName;
    std::string comment;
    double posX = 0.0, posY = 0.0;  // Graph position
    std::vector<std::string> inputPins;
    std::vector<std::string> outputPins;
    std::vector<std::string> connections;  // Connected node IDs
};

struct UnrealBlueprintGraph {
    std::string graphName;
    std::string graphType;           // "EventGraph", "Function", "Macro"
    std::vector<UnrealBlueprintNode> nodes;
    int nodeCount        = 0;
    int connectionCount  = 0;
};

// ============================================================================
// Unreal Debug State
// ============================================================================
struct UnrealDebugBreakpoint {
    std::string file;
    int line         = 0;
    bool enabled     = true;
    std::string condition;
    int hitCount     = 0;
    bool verified    = false;
};

struct UnrealDebugStackFrame {
    int id               = 0;
    std::string name;      // Function name (may be demangled)
    std::string source;
    int line             = 0;
    std::string module;    // DLL/module name
};

struct UnrealDebugVariable {
    std::string name;
    std::string value;
    std::string type;
    int variablesReference = 0;
    bool isUObject = false;   // True if this is a UObject-derived type
};

// ============================================================================
// Callbacks (function pointers only — no std::function)
// ============================================================================
typedef void (*UnrealLogCallback)(const char* message, int severity);
typedef void (*UnrealBuildProgressCallback)(int compiled, int total, const char* currentFile);
typedef void (*UnrealProfilerCallback)(const UnrealProfilerSnapshot* snapshot);
typedef void (*UnrealCompileCallback)(int errorCount, int warningCount);
typedef void (*UnrealLiveCodingCallback)(bool success, const char* module);

// ============================================================================
// UnrealEngineIntegration — Main Integration Class
// ============================================================================
class UnrealEngineIntegration {
public:
    UnrealEngineIntegration();
    ~UnrealEngineIntegration();

    // ── Lifecycle ──
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(); }

    // ── Project Management ──
    bool openProject(const std::string& uprojectPath);
    bool closeProject();
    bool createProject(const std::string& path, const std::string& name,
                       const std::string& templateId = "ThirdPerson");
    bool isProjectOpen() const;
    UnrealProjectInfo getProjectInfo() const;
    bool refreshProject();

    // ── Engine Discovery ──
    bool detectEngineInstallations();
    std::vector<std::string> getInstalledVersions() const;
    std::string getEnginePath(const std::string& version = "") const;
    bool setPreferredVersion(const std::string& version);

    // ── Module Management ──
    std::vector<UnrealModuleInfo> getModules() const;
    UnrealModuleInfo getModuleInfo(const std::string& moduleName) const;
    bool createModule(const std::string& name, const std::string& type = "Runtime");
    bool addModuleDependency(const std::string& module, const std::string& dependency);
    bool generateProjectFiles();

    // ── Level / World Management ──
    std::vector<UnrealLevelInfo> getLevels() const;
    UnrealLevelInfo getActiveLevel() const;
    bool openLevel(const std::string& levelPath);
    bool saveLevel(const std::string& levelPath = "");
    bool createLevel(const std::string& levelPath);
    std::vector<UnrealActorInfo> getWorldOutliner() const;
    bool spawnActor(const std::string& className, double x, double y, double z);
    bool deleteActor(const std::string& actorName);

    // ── Asset Browser ──
    std::vector<UnrealAssetInfo> getAssets(const std::string& directory = "/Game") const;
    UnrealAssetInfo getAssetInfo(const std::string& objectPath) const;
    bool importAsset(const std::string& sourcePath, const std::string& destPath = "/Game");
    bool deleteAsset(const std::string& objectPath);
    bool moveAsset(const std::string& oldPath, const std::string& newPath);
    bool renameAsset(const std::string& objectPath, const std::string& newName);
    std::vector<std::string> findAssetsByType(UnrealAssetType type) const;
    std::vector<std::string> findAssetsByClass(const std::string& className) const;
    std::vector<std::string> getAssetReferences(const std::string& objectPath) const;
    std::vector<std::string> getAssetDependencies(const std::string& objectPath) const;
    bool fixupRedirectors();

    // ── C++ Source Bridge ──
    bool createCppClass(const std::string& className, const std::string& parentClass = "AActor",
                        const std::string& moduleName = "");
    std::string generateClassHeader(const std::string& className,
                                    const std::string& parentClass = "AActor",
                                    const std::string& moduleName = "",
                                    bool withTick = true, bool withBeginPlay = true) const;
    std::string generateClassSource(const std::string& className,
                                    const std::string& parentClass = "AActor",
                                    const std::string& moduleName = "",
                                    bool withTick = true, bool withBeginPlay = true) const;
    std::string generateBuildCs(const std::string& moduleName,
                                const std::vector<std::string>& dependencies = {}) const;
    bool compileProject();
    std::vector<UnrealCppError> getCompileErrors() const;
    bool hasCompileErrors() const;

    // ── Build System (UnrealBuildTool) ──
    UnrealBuildResult buildProject(const UnrealBuildSettings& settings);
    bool buildProjectAsync(const UnrealBuildSettings& settings);
    bool isBuildInProgress() const;
    float getBuildProgress() const;
    void cancelBuild();
    UnrealBuildSettings getDefaultBuildSettings() const;
    UnrealBuildResult getLastBuildResult() const;

    // ── Live Coding ──
    bool enableLiveCoding();
    bool disableLiveCoding();
    bool triggerLiveCodingCompile();
    bool isLiveCodingEnabled() const;
    bool isLiveCodingCompiling() const;

    // ── Hot Reload (Legacy) ──
    bool triggerHotReload();
    bool isHotReloadInProgress() const;

    // ── Cook / Package ──
    bool cookContent(UnrealBuildTarget platform);
    bool packageProject(UnrealBuildTarget platform, const std::string& outputDir);
    bool isCookInProgress() const;

    // ── Play-In-Editor (PIE) ──
    bool startPIE(int playerCount = 1, bool dedicated = false);
    bool stopPIE();
    bool pausePIE();
    bool resumePIE();
    bool isInPIE() const;
    bool isPIEPaused() const;

    // ── Profiler / Unreal Insights ──
    bool startProfiler();
    bool stopProfiler();
    bool isProfilerRunning() const;
    UnrealProfilerSnapshot getProfilerSnapshot() const;
    std::vector<UnrealProfilerSnapshot> getProfilerHistory(int frameCount = 300) const;
    bool startUnrealInsightsTrace(const std::string& channels = "default,cpu,gpu,frame,memory");
    bool stopUnrealInsightsTrace();
    bool openInsightsTrace(const std::string& tracePath);

    // ── Blueprint Support ──
    std::vector<std::string> getBlueprints(const std::string& directory = "/Game") const;
    UnrealBlueprintGraph getBlueprintGraph(const std::string& blueprintPath,
                                            const std::string& graphName = "EventGraph") const;
    bool createBlueprint(const std::string& name, const std::string& parentClass,
                         const std::string& directory = "/Game/Blueprints");
    bool compileBlueprint(const std::string& blueprintPath);
    bool compileAllBlueprints();
    std::string blueprintGraphToText(const UnrealBlueprintGraph& graph) const;

    // ── Debug (C++ and Blueprint) ──
    bool startDebugSession(const std::string& executable = "");
    bool stopDebugSession();
    bool attachDebugger(int processId);
    bool setBreakpoint(const std::string& file, int line, const std::string& condition = "");
    bool removeBreakpoint(const std::string& file, int line);
    std::vector<UnrealDebugBreakpoint> getBreakpoints() const;
    bool debugContinue();
    bool debugStepOver();
    bool debugStepInto();
    bool debugStepOut();
    std::vector<UnrealDebugStackFrame> getCallStack() const;
    std::vector<UnrealDebugVariable> getLocals(int frameId = 0) const;
    std::string evaluateExpression(const std::string& expr, int frameId = 0) const;
    bool isDebugSessionActive() const;

    // ── Console Commands ──
    bool executeConsoleCommand(const std::string& command);
    std::string executeConsoleCommandWithOutput(const std::string& command);

    // ── Plugin Management ──
    std::vector<std::string> getPlugins() const;
    std::vector<std::string> getEnabledPlugins() const;
    bool enablePlugin(const std::string& pluginName);
    bool disablePlugin(const std::string& pluginName);
    bool createPlugin(const std::string& name, const std::string& templateType = "Blank");
    std::string getPluginDescriptor(const std::string& pluginName) const;

    // ── Source Control ──
    bool isSourceControlEnabled() const;
    std::string getSourceControlProvider() const;
    bool checkOutFile(const std::string& filePath);
    bool markForAdd(const std::string& filePath);
    bool markForDelete(const std::string& filePath);
    bool revertFile(const std::string& filePath);

    // ── Automation / Testing ──
    bool runAutomationTest(const std::string& testName);
    bool runAllAutomationTests();
    std::vector<std::string> getAutomationTests() const;
    std::string getAutomationTestResults() const;

    // ── Agentic AI Integration ──
    std::string generateAIWorldDescription() const;
    std::string generateAIProjectSummary() const;
    std::string generateAIBlueprintDescription(const std::string& blueprintPath) const;
    bool applyAISuggestion(const std::string& suggestion);
    bool executeAIGeneratedCpp(const std::string& cppCode, const std::string& moduleName);
    std::string convertBlueprintToCpp(const std::string& blueprintPath) const;
    std::string convertCppToBlueprint(const std::string& cppCode) const;

    // ── Callbacks ──
    void setLogCallback(UnrealLogCallback cb)                  { m_logCallback = cb; }
    void setBuildProgressCallback(UnrealBuildProgressCallback cb) { m_buildProgressCallback = cb; }
    void setProfilerCallback(UnrealProfilerCallback cb)        { m_profilerCallback = cb; }
    void setCompileCallback(UnrealCompileCallback cb)          { m_compileCallback = cb; }
    void setLiveCodingCallback(UnrealLiveCodingCallback cb)    { m_liveCodingCallback = cb; }

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
    bool detectProjectStructure(const std::string& uprojectPath);
    bool parseUProjectFile(const std::string& uprojectPath);
    bool parseBuildCsFiles();
    void scanContentFolder(const std::string& directory, std::vector<UnrealAssetInfo>& out) const;
    UnrealAssetType classifyAsset(const std::string& extension, const std::string& className) const;
    std::string getUBTPath() const;
    std::string getUATPath() const;
    std::string getEditorCmdPath() const;
    bool launchUBT(const std::string& args, std::string& output, int timeoutMs = 600000);
    bool launchUAT(const std::string& args, std::string& output, int timeoutMs = 600000);
    bool launchEditorCmd(const std::string& args, std::string& output, int timeoutMs = 300000);
    void parseUBTLog(const std::string& log, UnrealBuildResult& result);
    std::string buildTargetToString(UnrealBuildTarget target) const;
    std::string buildConfigToString(UnrealBuildConfig config) const;
    std::string targetTypeToString(UnrealBuildTargetType type) const;
    std::string resolveModuleDirectory(const std::string& moduleName) const;

    // ── State ──
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_buildInProgress{false};
    std::atomic<float> m_buildProgress{0.0f};
    std::atomic<bool> m_inPIE{false};
    std::atomic<bool> m_piePaused{false};
    std::atomic<bool> m_profilerRunning{false};
    std::atomic<bool> m_debugSessionActive{false};
    std::atomic<bool> m_liveCodingEnabled{false};
    std::atomic<bool> m_liveCodingCompiling{false};
    std::atomic<bool> m_cookInProgress{false};

    mutable std::mutex m_mutex;
    mutable std::mutex m_profilerMutex;
    mutable std::mutex m_debugMutex;
    mutable std::mutex m_buildMutex;

    UnrealProjectInfo m_projectInfo;
    UnrealBuildResult m_lastBuildResult;
    UnrealBuildSettings m_currentBuildSettings;

    std::vector<UnrealModuleInfo> m_modules;
    std::vector<UnrealCppError> m_compileErrors;
    std::vector<UnrealDebugBreakpoint> m_breakpoints;
    std::vector<UnrealProfilerSnapshot> m_profilerHistory;
    std::vector<std::string> m_installedVersions;

    std::string m_preferredVersion;
    std::string m_enginePath;
    std::string m_configPath;

    // Callbacks (function pointers)
    UnrealLogCallback           m_logCallback = nullptr;
    UnrealBuildProgressCallback m_buildProgressCallback = nullptr;
    UnrealProfilerCallback      m_profilerCallback = nullptr;
    UnrealCompileCallback       m_compileCallback = nullptr;
    UnrealLiveCodingCallback    m_liveCodingCallback = nullptr;

    // Debug state
    HANDLE m_debugProcess = nullptr;
};

} // namespace GameEngine
} // namespace RawrXD
