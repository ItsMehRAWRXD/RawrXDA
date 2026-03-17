#pragma once
// ToolchainBridge — Pure Win32 API build system bridge
// Discovers VS2022 Build Tools, invokes ml64/cl/link
// No Qt, no std::filesystem, no external dependencies

#ifndef TOOLCHAIN_BRIDGE_HPP
#define TOOLCHAIN_BRIDGE_HPP

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace RawrXD {

enum class BuildConfig { Debug, Release };
enum class BuildPhase { Idle, Discovering, Assembling, Compiling, Linking, Done, Failed };

struct SourceUnit {
    std::string path;
    std::string objPath;
    enum Type { ASM, CPP, C_SRC, HEADER } type;
    bool needsBuild;
    FILETIME lastWrite;
};

struct BuildResult {
    bool success;
    int exitCode;
    std::string output;
    std::string errors;
    double elapsedMs;
};

struct ToolchainPaths {
    std::string ml64;       // ml64.exe full path
    std::string cl;         // cl.exe full path
    std::string link;       // link.exe full path
    std::string lib;        // lib.exe full path
    std::string vcInclude;  // VC include dir
    std::string vcLib;      // VC lib/x64 dir
    std::string sdkIncUm;   // Windows SDK include/um
    std::string sdkIncShared; // Windows SDK include/shared
    std::string sdkIncUcrt; // Windows SDK include/ucrt
    std::string sdkLibUm;   // Windows SDK lib/um/x64
    std::string sdkLibUcrt; // Windows SDK lib/ucrt/x64
    bool valid;
};

// Callback for real-time build output
using BuildOutputCallback = std::function<void(const std::string& line, bool isError)>;
using BuildPhaseCallback = std::function<void(BuildPhase phase, const std::string& detail)>;

class ToolchainBridge {
public:
    ToolchainBridge();
    ~ToolchainBridge();

    // Discovery
    bool discoverToolchain();
    bool isToolchainValid() const { return m_paths.valid; }
    const ToolchainPaths& getPaths() const { return m_paths; }

    // Configuration
    void setConfig(BuildConfig config) { m_config = config; }
    BuildConfig getConfig() const { return m_config; }
    void setTarget(const std::string& target) { m_target = target; }
    const std::string& getTarget() const { return m_target; }
    void setProjectRoot(const std::string& root) { m_projectRoot = root; }
    void setSourceDirs(const std::vector<std::string>& dirs) { m_sourceDirs = dirs; }
    void setOutputDir(const std::string& dir) { m_outputDir = dir; }
    void setObjDir(const std::string& dir) { m_objDir = dir; }

    // Build operations (async)
    void buildProject(BuildOutputCallback outputCb, BuildPhaseCallback phaseCb);
    void buildClean();
    void rebuildAll(BuildOutputCallback outputCb, BuildPhaseCallback phaseCb);
    void assembleFile(const std::string& asmPath, BuildOutputCallback outputCb);
    void runTarget(BuildOutputCallback outputCb);
    void stopBuild();

    // Sync build (for single files)
    BuildResult assembleSingle(const std::string& asmPath);
    BuildResult compileSingle(const std::string& cppPath);

    // State
    bool isBuilding() const { return m_building.load(); }
    BuildPhase currentPhase() const { return m_phase.load(); }
    const std::string& lastBuildLog() const { return m_buildLog; }

private:
    // Toolchain discovery
    bool findVSInstallation(std::string& vsPath);
    bool findMSVCTools(const std::string& vsPath);
    bool findWindowsSDK();
    std::string findLatestSDKVersion(const std::string& sdkRoot);

    // Source scanning
    std::vector<SourceUnit> scanSources();
    void checkIncremental(std::vector<SourceUnit>& units);
    FILETIME getLatestHeaderTime();

    // Execution
    BuildResult executeProcess(const std::string& exe, const std::string& args,
                               const std::string& workDir = "");
    BuildResult executeMasm(const SourceUnit& unit);
    BuildResult executeCpp(const SourceUnit& unit);
    BuildResult executeLink(const std::vector<SourceUnit>& units);

    // Helpers
    std::string buildMasmFlags(const SourceUnit& unit);
    std::string buildCppFlags(const SourceUnit& unit);
    std::string buildLinkFlags(const std::vector<SourceUnit>& units);
    static bool fileExists(const char* path);
    static FILETIME getFileTime(const char* path);
    static bool isNewer(const FILETIME& a, const FILETIME& b);
    void appendLog(const std::string& line);
    void enumerateFiles(const std::string& dir, const std::string& pattern,
                        std::vector<std::string>& out);

    ToolchainPaths m_paths;
    BuildConfig m_config;
    std::string m_target;
    std::string m_projectRoot;
    std::vector<std::string> m_sourceDirs;
    std::string m_outputDir;
    std::string m_objDir;

    std::atomic<bool> m_building;
    std::atomic<BuildPhase> m_phase;
    std::string m_buildLog;
    std::mutex m_logMutex;

    // Build thread + cancellation
    std::thread m_buildThread;
    HANDLE m_hBuildProcess;
    std::atomic<bool> m_cancelRequested;
};

} // namespace RawrXD

#endif // TOOLCHAIN_BRIDGE_HPP
