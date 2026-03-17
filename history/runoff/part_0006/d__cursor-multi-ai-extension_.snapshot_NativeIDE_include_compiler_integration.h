#pragma once

#include "native_ide.h"
#include <unordered_map>
#include <chrono>

// Build configuration enums
enum class OptimizationLevel {
    None,           // -O0
    Basic,          // -O1  
    Performance,    // -O2 + performance flags
    Size,           // -Os + size optimization
    Aggressive,     // -O3 + advanced optimizations
    Custom          // User-defined flags
};

enum class TargetArchitecture {
    x86,
    x64,
    ARM,
    ARM64,
    Auto            // Detect from system
};

enum class BuildConfiguration {
    Debug,
    Release,
    RelWithDebInfo,
    MinSizeRel,
    Custom
};

// Build profile structure
struct BuildProfile {
    std::wstring name;
    OptimizationLevel optimization;
    TargetArchitecture architecture;
    BuildConfiguration configuration;
    std::vector<std::wstring> customFlags;
    std::vector<std::wstring> defines;
    std::vector<std::wstring> includePaths;
    std::vector<std::wstring> libraryPaths;
    std::vector<std::wstring> libraries;
    bool enableProfiling = false;
    bool enableSanitizers = false;
    bool enableLTO = false;  // Link Time Optimization
};

// Dependency management structures
struct Dependency {
    std::wstring name;
    std::wstring version;
    std::wstring path;
    std::wstring type;  // "static", "dynamic", "header-only"
    std::vector<std::wstring> includePaths;
    std::vector<std::wstring> libraryPaths;
    std::vector<std::wstring> linkLibraries;
    std::vector<std::wstring> compileFlags;
    bool isRequired = true;
};

struct DependencyGraph {
    std::unordered_map<std::wstring, Dependency> dependencies;
    std::unordered_map<std::wstring, std::vector<std::wstring>> dependencyTree;
    std::vector<std::wstring> buildOrder;
};

struct BuildCache {
    std::wstring sourceFile;
    std::chrono::system_clock::time_point lastModified;
    std::chrono::system_clock::time_point lastBuilt;
    std::wstring outputFile;
    std::vector<std::wstring> dependencies;
    std::string checksum;
    bool isValid = false;
};

class CompilerIntegration {
private:
    std::wstring m_toolchainPath;
    std::wstring m_gccPath;
    std::wstring m_clangPath;
    std::wstring m_makePath;
    std::wstring m_gdbPath;
    std::wstring m_javacPath;
    std::wstring m_cscPath;
    std::wstring m_gppPath;
    std::wstring m_nasmPath;
    std::wstring m_gradlePath;
    std::wstring m_androidSdkPath;
    
    // Modern Language Compilers (d:\MyCoPilot-Complete-Portable\portable-toolchains\)
    std::wstring m_rustcPath;        // rust-portable\Rust\bin\rustc.exe
    std::wstring m_kotlincPath;      // kotlinc\bin\kotlinc.bat
    std::wstring m_zigPath;          // zig-portable\zig.exe
    std::wstring m_pythonPath;       // python-portable\python.exe
    std::wstring m_nodejsPath;       // nodejs-portable\node.exe
    std::wstring m_goPath;           // go-portable\bin\go.exe
    std::wstring m_cmakePath;        // cmake-portable\cmake-3.28.1-windows-x86_64\bin\cmake.exe
    std::wstring m_graalvmPath;      // graalvm-portable\bin\native-image.exe
    
    std::wstring m_currentCompiler;  // "gcc", "clang", "javac", "csc", "g++", "nasm", "rustc", "kotlinc", "zig", "python", "node", "go"
    
    // Advanced features
    std::unordered_map<std::wstring, BuildProfile> m_buildProfiles;
    std::wstring m_activeBuildProfile;
    mutable std::unordered_map<std::wstring, std::chrono::system_clock::time_point> m_buildCache;
    
    // Portable toolchain management
    std::wstring m_portableToolchainBase;
    std::unordered_map<std::wstring, std::wstring> m_systemCompilerPaths;
    bool m_preferPortable;
    
    DependencyGraph m_dependencyGraph;
    std::unordered_map<std::wstring, BuildCache> m_buildCacheMap;
    std::unordered_map<std::wstring, std::vector<Dependency>> m_languageDependencies;
    std::wstring m_packageCacheDir;
    bool m_enableSmartLinking;
    int m_maxParallelJobs;
    std::vector<CompileResult> m_buildHistory;
    std::wstring m_lastProjectPath;
    
public:
    explicit CompilerIntegration(const std::wstring& toolchainPath);
    ~CompilerIntegration();
    
    bool Initialize();
    bool IsAvailable() const;
    
    // Compiler operations
    CompileResult CompileFile(const std::wstring& sourceFile, 
                             const std::wstring& outputFile = L"",
                             const std::vector<std::wstring>& flags = {},
                             const std::wstring& compiler = L"");
    
    CompileResult BuildProject(const std::wstring& projectPath);
    CompileResult RunMake(const std::wstring& projectPath, const std::wstring& target = L"");
    CompileResult RunCMake(const std::wstring& projectPath);
    CompileResult RunMSBuild(const std::wstring& projectPath);
    CompileResult RunGradle(const std::wstring& projectPath, const std::wstring& task = L"assembleDebug");
    CompileResult BuildAPK(const std::wstring& projectPath);
    CompileResult CompileAllSourceFiles(const std::wstring& projectPath);
    
    // Modern Language Compilation
    CompileResult CompileRust(const std::wstring& sourceFile, const std::wstring& outputFile = L"");
    CompileResult CompileKotlin(const std::wstring& sourceFile, const std::wstring& outputFile = L"");
    CompileResult CompileZig(const std::wstring& sourceFile, const std::wstring& outputFile = L"");
    CompileResult RunPython(const std::wstring& scriptFile, const std::vector<std::wstring>& args = {});
    CompileResult RunNodeJS(const std::wstring& scriptFile, const std::vector<std::wstring>& args = {});
    CompileResult CompileGo(const std::wstring& sourceFile, const std::wstring& outputFile = L"");
    CompileResult CompileWithGraalVM(const std::wstring& sourceFile, const std::wstring& outputFile = L"");
    
    // Compiler settings
    void SetCurrentCompiler(const std::wstring& compiler);
    std::wstring GetCurrentCompiler() const { return m_currentCompiler; }
    
    // Available compilers
    std::vector<std::wstring> GetAvailableCompilers() const;
    std::wstring GetCompilerVersion(const std::wstring& compiler) const;
    
    // Source file operations
    bool ImportSourceFiles(const std::wstring& directory, std::vector<std::wstring>& sourceFiles);
    std::wstring DetectCompilerForFile(const std::wstring& filePath);
    
    // Path utilities
    std::wstring GetCompilerPath(const std::wstring& compiler) const;
    std::wstring GetLinkerPath(const std::wstring& compiler) const;
    std::wstring GetDebuggerPath() const { return m_gdbPath; }
    
    // Advanced build profiles
    void CreateBuildProfile(const std::wstring& name, const BuildProfile& profile);
    std::vector<std::wstring> GetBuildProfiles() const;
    BuildProfile GetBuildProfile(const std::wstring& name) const;
    void SetActiveBuildProfile(const std::wstring& name);
    std::wstring GetActiveBuildProfile() const { return m_activeBuildProfile; }
    
    // Cross-compilation support
    CompileResult CompileWithProfile(const std::wstring& sourceFile,
                                   const std::wstring& outputFile,
                                   const std::wstring& profileName);
    std::vector<std::wstring> GetSupportedArchitectures(const std::wstring& compiler) const;
    
    // Optimization and analysis
    std::vector<std::wstring> GetOptimizationFlags(OptimizationLevel level, const std::wstring& compiler) const;
    std::vector<std::wstring> GetArchitectureFlags(TargetArchitecture arch, const std::wstring& compiler) const;
    CompileResult AnalyzeBinary(const std::wstring& binaryPath);
    
    // Portable Toolchain Management
    bool SetupPortableToolchains(const std::wstring& baseDirectory);
    bool DownloadToolchain(const std::wstring& toolchain, const std::wstring& version = L"latest");
    std::vector<std::wstring> GetAvailableToolchains() const;
    bool VerifyToolchainIntegrity(const std::wstring& toolchain);
    std::wstring GetToolchainVersion(const std::wstring& toolchain) const;
    bool UpdateToolchain(const std::wstring& toolchain);
    
    // Auto-detection and configuration
    bool AutoDetectSystemCompilers();
    std::wstring GetSystemCompilerPath(const std::wstring& compiler) const;
    bool ConfigureForPlatform();
    
    // System information and status
    std::wstring GetToolchainSystemInfo() const;
    std::wstring GetCompilationFlowExplanation() const;
    
    // Intelligent Dependency Management
    bool ScanProjectDependencies(const std::wstring& projectPath);
    std::vector<Dependency> GetProjectDependencies(const std::wstring& projectPath);
    bool ResolveAndInstallDependencies(const std::wstring& projectPath);
    bool AddDependency(const std::wstring& name, const std::wstring& version = L"latest");
    bool RemoveDependency(const std::wstring& name);
    std::vector<std::wstring> GetMissingDependencies();
    DependencyGraph BuildDependencyGraph(const std::wstring& projectPath);
    std::vector<std::wstring> GetBuildOrder(const DependencyGraph& graph);
    bool UpdateDependency(const std::wstring& name, const std::wstring& version);
    
    // Smart Build Cache
    bool IsSourceModified(const std::wstring& sourceFile);
    bool NeedsRebuild(const std::wstring& sourceFile);
    void InvalidateCache(const std::wstring& sourceFile);
    void ClearAllCache();
    std::string ComputeChecksum(const std::wstring& filePath);
    
    // Incremental Build
    CompileResult IncrementalBuild(const std::wstring& projectPath);
    std::vector<std::wstring> GetModifiedFiles(const std::wstring& projectPath);
    CompileResult RebuildModifiedOnly(const std::wstring& projectPath);
    
    // Package Management
    CompileResult InstallPackage(const std::wstring& language, const std::wstring& packageName);
    CompileResult UpdatePackages(const std::wstring& language);
    std::vector<std::wstring> ListInstalledPackages(const std::wstring& language);
    bool DownloadPackage(const std::wstring& packageName, const std::wstring& version);
    bool ExtractPackage(const std::wstring& packagePath, const std::wstring& destination);
    
    // Multi-target Build
    CompileResult BuildForMultipleTargets(const std::wstring& sourceFile, const std::vector<TargetArchitecture>& targets);
    CompileResult CrossCompile(const std::wstring& sourceFile, const std::wstring& targetPlatform);
    
    // Code Analysis
    CompileResult RunStaticAnalysis(const std::wstring& sourceFile);
    CompileResult CheckCodeStyle(const std::wstring& sourceFile);
    CompileResult GenerateDocumentation(const std::wstring& projectPath);
    
    // Performance Profiling
    CompileResult ProfileBuild(const std::wstring& sourceFile);
    CompileResult BenchmarkBinary(const std::wstring& binaryPath);
    
    // Container & Cloud Build
    CompileResult BuildDockerImage(const std::wstring& projectPath, const std::wstring& imageName);
    CompileResult BuildWASM(const std::wstring& sourceFile, const std::wstring& outputFile);
    
    // IDE Integration
    CompileResult GenerateCompileCommands(const std::wstring& projectPath);
    CompileResult ExportBuildConfig(const std::wstring& outputPath);
    bool ImportBuildConfig(const std::wstring& configPath);
    
    // State Persistence
    bool SaveState(const std::wstring& stateFile);
    bool LoadState(const std::wstring& stateFile);
    bool SaveBuildHistory(const std::wstring& historyFile);
    std::vector<CompileResult> LoadBuildHistory(const std::wstring& historyFile);
    bool SaveProjectSettings(const std::wstring& projectPath);
    bool LoadProjectSettings(const std::wstring& projectPath);
    void AutoSaveState();
    bool RestoreLastSession();
    
    // Packer/Encryptor and Hotpatching Support
    CompileResult PackExecutable(const std::wstring& inputExe, const std::wstring& outputExe, bool encrypt = false);
    CompileResult UnpackExecutable(const std::wstring& packedExe, const std::wstring& outputExe);
    CompileResult HotpatchExecutable(const std::wstring& targetExe, const std::wstring& patchData, size_t patchOffset);
    bool IsExecutablePacked(const std::wstring& exePath);
    
    // Validation and information
    bool ValidateSourceFile(const std::wstring& sourceFile);
    std::wstring GetCompilerInfo(const std::wstring& compiler);
    std::vector<std::wstring> GetCompilerFlags(const std::wstring& compiler, const std::wstring& buildType = L"release");
    
private:
    CompileResult ExecuteCommand(const std::wstring& command, const std::wstring& workingDir = L"");
    std::wstring BuildCompileCommand(const std::wstring& compiler,
                                    const std::wstring& sourceFile,
                                    const std::wstring& outputFile,
                                    const std::vector<std::wstring>& flags);
    
    std::wstring DetectProjectType(const std::wstring& projectPath);
    std::wstring FindProjectFile(const std::wstring& projectPath);
    
    // Build system support
    CompileResult RunCMake(const std::wstring& projectPath);
    CompileResult RunMSBuild(const std::wstring& projectPath);
    CompileResult CompileAllSourceFiles(const std::wstring& projectPath);
    
    bool VerifyCompilerExists(const std::wstring& path) const;
    void SetupEnvironmentVariables();
    void InitializeDefaultBuildProfiles();
    
    bool IsCached(const std::wstring& sourceFile) const;
    void UpdateCache(const std::wstring& sourceFile);
    void ClearCache();
    
    CompileResult ParallelBuild(const std::vector<std::wstring>& sourceFiles, int threadCount = 0);
    std::vector<std::wstring> ResolveDependencies(const std::wstring& sourceFile);
    CompileResult BuildWithDependencies(const std::wstring& sourceFile);
    
    // Persistence helpers
    std::wstring SerializeState() const;
    bool DeserializeState(const std::wstring& data);
    std::wstring GetStateFilePath() const;
    std::vector<CompileResult> m_buildHistory;
    std::wstring m_lastProjectPath;
};

