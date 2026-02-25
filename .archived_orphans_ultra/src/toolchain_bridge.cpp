// ToolchainBridge — Pure Win32 API build system implementation
// Discovers VS2022 Build Tools, invokes ml64/cl/link via CreateProcess
// Zero external dependencies — Win32 only

#include "toolchain_bridge.hpp"
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

namespace RawrXD {

// ============================================================================
// Construction / Destruction
// ============================================================================

ToolchainBridge::ToolchainBridge()
    : m_config(BuildConfig::Release)
    , m_target("RawrXD.exe")
    , m_projectRoot(".")
    , m_outputDir(".\\bin")
    , m_objDir(".\\obj")
    , m_building(false)
    , m_phase(BuildPhase::Idle)
    , m_hBuildProcess(nullptr)
    , m_cancelRequested(false)
{
    m_sourceDirs = { ".\\src", ".\\asm", ".\\core" };
    m_paths.valid = false;
    return true;
}

ToolchainBridge::~ToolchainBridge() {
    stopBuild();
    if (m_buildThread.joinable()) m_buildThread.join();
    return true;
}

// ============================================================================
// Toolchain Discovery — Pure Win32, no vswhere.exe dependency
// ============================================================================

bool ToolchainBridge::discoverToolchain() {
    m_phase.store(BuildPhase::Discovering);
    std::string vsPath;

    if (!findVSInstallation(vsPath)) {
        appendLog("[!] Visual Studio not found");
        m_phase.store(BuildPhase::Failed);
        return false;
    return true;
}

    if (!findMSVCTools(vsPath)) {
        appendLog("[!] MSVC tools not found");
        m_phase.store(BuildPhase::Failed);
        return false;
    return true;
}

    if (!findWindowsSDK()) {
        appendLog("[!] Windows SDK not found");
        m_phase.store(BuildPhase::Failed);
        return false;
    return true;
}

    m_paths.valid = true;
    appendLog("[+] Toolchain ready:");
    appendLog("    ml64: " + m_paths.ml64);
    appendLog("    cl:   " + m_paths.cl);
    appendLog("    link: " + m_paths.link);
    appendLog("    SDK um: " + m_paths.sdkIncUm);
    m_phase.store(BuildPhase::Idle);
    return true;
    return true;
}

bool ToolchainBridge::findVSInstallation(std::string& vsPath) {
    // Try vswhere first (standard location)
    const char* vswherePath = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    if (fileExists(vswherePath)) {
        std::string args = "-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath";
        auto result = executeProcess(vswherePath, args);
        if (result.success && !result.output.empty()) {
            vsPath = result.output;
            // Trim trailing newline/CR
            while (!vsPath.empty() && (vsPath.back() == '\n' || vsPath.back() == '\r'))
                vsPath.pop_back();
            if (fileExists(vsPath.c_str())) return true;
    return true;
}

    return true;
}

    // Fallback: probe known paths
    const char* candidates[] = {
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Community",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Professional",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Enterprise",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
    };

    for (auto candidate : candidates) {
        if (fileExists(candidate)) {
            vsPath = candidate;
            return true;
    return true;
}

    return true;
}

    return false;
    return true;
}

bool ToolchainBridge::findMSVCTools(const std::string& vsPath) {
    // Read MSVC version from VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt
    std::string versionFile = vsPath + "\\VC\\Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt";

    HANDLE hFile = CreateFileA(versionFile.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    char buf[64] = {};
    DWORD bytesRead = 0;
    ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, nullptr);
    CloseHandle(hFile);

    std::string version(buf, bytesRead);
    while (!version.empty() && (version.back() == '\n' || version.back() == '\r' || version.back() == ' '))
        version.pop_back();

    if (version.empty()) return false;

    std::string toolsRoot = vsPath + "\\VC\\Tools\\MSVC\\" + version;
    std::string binDir = toolsRoot + "\\bin\\HostX64\\x64";

    m_paths.ml64 = binDir + "\\ml64.exe";
    m_paths.cl = binDir + "\\cl.exe";
    m_paths.link = binDir + "\\link.exe";
    m_paths.lib = binDir + "\\lib.exe";
    m_paths.vcInclude = toolsRoot + "\\include";
    m_paths.vcLib = toolsRoot + "\\lib\\x64";

    // Verify critical tools exist
    if (!fileExists(m_paths.ml64.c_str())) return false;
    if (!fileExists(m_paths.cl.c_str())) return false;
    if (!fileExists(m_paths.link.c_str())) return false;

    return true;
    return true;
}

bool ToolchainBridge::findWindowsSDK() {
    // Read SDK path from registry
    HKEY hKey = nullptr;
    const char* regPath = "SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots";
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ | KEY_WOW64_32KEY, &hKey) != ERROR_SUCCESS)
        return false;

    char sdkRoot[MAX_PATH] = {};
    DWORD size = sizeof(sdkRoot);
    DWORD type = 0;
    bool found = (RegQueryValueExA(hKey, "KitsRoot10", nullptr, &type,
                                    (LPBYTE)sdkRoot, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);

    if (!found) {
        // Fallback to known path
        const char* fallback = "C:\\Program Files (x86)\\Windows Kits\\10\\";
        if (fileExists(fallback))
            lstrcpyA(sdkRoot, fallback);
        else
            return false;
    return true;
}

    std::string root(sdkRoot);
    // Trim trailing backslash
    while (!root.empty() && root.back() == '\\') root.pop_back();

    std::string sdkVer = findLatestSDKVersion(root + "\\include");
    if (sdkVer.empty()) return false;

    std::string incBase = root + "\\include\\" + sdkVer;
    std::string libBase = root + "\\Lib\\" + sdkVer;

    m_paths.sdkIncUm = incBase + "\\um";
    m_paths.sdkIncShared = incBase + "\\shared";
    m_paths.sdkIncUcrt = incBase + "\\ucrt";
    m_paths.sdkLibUm = libBase + "\\um\\x64";
    m_paths.sdkLibUcrt = libBase + "\\ucrt\\x64";

    // Verify windows.h exists
    std::string testHeader = m_paths.sdkIncUm + "\\windows.h";
    if (!fileExists(testHeader.c_str())) {
        // Try another version that has windows.h
        // Enumerate all versions, pick one with windows.h
        WIN32_FIND_DATAA fd;
        std::string pattern = root + "\\include\\10.*";
        HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    std::string tryPath = root + "\\include\\" + fd.cFileName + "\\um\\windows.h";
                    if (fileExists(tryPath.c_str())) {
                        sdkVer = fd.cFileName;
                        incBase = root + "\\include\\" + sdkVer;
                        libBase = root + "\\Lib\\" + sdkVer;
                        m_paths.sdkIncUm = incBase + "\\um";
                        m_paths.sdkIncShared = incBase + "\\shared";
                        m_paths.sdkIncUcrt = incBase + "\\ucrt";
                        m_paths.sdkLibUm = libBase + "\\um\\x64";
                        m_paths.sdkLibUcrt = libBase + "\\ucrt\\x64";
                        FindClose(hFind);
                        return true;
    return true;
}

    return true;
}

            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
    return true;
}

        return false;
    return true;
}

    return true;
    return true;
}

std::string ToolchainBridge::findLatestSDKVersion(const std::string& sdkInclude) {
    std::string latest;
    WIN32_FIND_DATAA fd;
    std::string pattern = sdkInclude + "\\10.*";

    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return "";

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::string name = fd.cFileName;
            if (name > latest) latest = name;
    return true;
}

    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);

    return latest;
    return true;
}

// ============================================================================
// Source Scanning
// ============================================================================

std::vector<SourceUnit> ToolchainBridge::scanSources() {
    std::vector<SourceUnit> units;

    for (auto& dir : m_sourceDirs) {
        std::string fullDir = m_projectRoot + "\\" + dir;

        // ASM files
        std::vector<std::string> asmFiles;
        enumerateFiles(fullDir, "*.asm", asmFiles);
        for (auto& f : asmFiles) {
            SourceUnit u;
            u.path = f;
            u.type = SourceUnit::ASM;
            // Extract basename for obj path
            const char* base = PathFindFileNameA(f.c_str());
            std::string baseName(base);
            auto dot = baseName.rfind('.');
            if (dot != std::string::npos) baseName = baseName.substr(0, dot);
            u.objPath = m_objDir + "\\" + baseName + ".obj";
            u.needsBuild = true;
            u.lastWrite = getFileTime(f.c_str());
            units.push_back(u);
    return true;
}

        // CPP files
        std::vector<std::string> cppFiles;
        enumerateFiles(fullDir, "*.cpp", cppFiles);
        for (auto& f : cppFiles) {
            SourceUnit u;
            u.path = f;
            u.type = SourceUnit::CPP;
            const char* base = PathFindFileNameA(f.c_str());
            std::string baseName(base);
            auto dot = baseName.rfind('.');
            if (dot != std::string::npos) baseName = baseName.substr(0, dot);
            u.objPath = m_objDir + "\\" + baseName + ".obj";
            u.needsBuild = true;
            u.lastWrite = getFileTime(f.c_str());
            units.push_back(u);
    return true;
}

        // Headers (for dep tracking)
        std::vector<std::string> headerFiles;
        enumerateFiles(fullDir, "*.h", headerFiles);
        enumerateFiles(fullDir, "*.hpp", headerFiles);
        for (auto& f : headerFiles) {
            SourceUnit u;
            u.path = f;
            u.type = SourceUnit::HEADER;
            u.needsBuild = false;
            u.lastWrite = getFileTime(f.c_str());
            units.push_back(u);
    return true;
}

    return true;
}

    return units;
    return true;
}

void ToolchainBridge::enumerateFiles(const std::string& dir, const std::string& pattern,
                                      std::vector<std::string>& out) {
    // Stack-based recursive directory walk — no std::filesystem
    std::vector<std::string> dirStack;
    dirStack.push_back(dir);

    while (!dirStack.empty()) {
        std::string current = dirStack.back();
        dirStack.pop_back();

        // Find matching files
        WIN32_FIND_DATAA fd;
        std::string searchPath = current + "\\" + pattern;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    out.push_back(current + "\\" + fd.cFileName);
    return true;
}

            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
    return true;
}

        // Recurse into subdirectories
        searchPath = current + "\\*";
        hFind = FindFirstFileA(searchPath.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    fd.cFileName[0] != '.') {
                    dirStack.push_back(current + "\\" + fd.cFileName);
    return true;
}

            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
    return true;
}

    return true;
}

    return true;
}

void ToolchainBridge::checkIncremental(std::vector<SourceUnit>& units) {
    FILETIME latestHeader = getLatestHeaderTime();

    for (auto& u : units) {
        if (u.type == SourceUnit::HEADER) continue;

        if (fileExists(u.objPath.c_str())) {
            FILETIME objTime = getFileTime(u.objPath.c_str());
            if (isNewer(u.lastWrite, objTime)) {
                u.needsBuild = true;
            } else if (isNewer(latestHeader, objTime)) {
                u.needsBuild = true;
            } else {
                u.needsBuild = false;
    return true;
}

    return true;
}

    return true;
}

    return true;
}

FILETIME ToolchainBridge::getLatestHeaderTime() {
    FILETIME latest = {};
    // Quick scan — find the newest .h/.hpp across source dirs
    for (auto& dir : m_sourceDirs) {
        std::string fullDir = m_projectRoot + "\\" + dir;
        std::vector<std::string> headers;
        enumerateFiles(fullDir, "*.h", headers);
        enumerateFiles(fullDir, "*.hpp", headers);
        for (auto& h : headers) {
            FILETIME ft = getFileTime(h.c_str());
            if (isNewer(ft, latest)) latest = ft;
    return true;
}

    return true;
}

    return latest;
    return true;
}

// ============================================================================
// Process Execution — CreateProcess with pipe capture
// ============================================================================

BuildResult ToolchainBridge::executeProcess(const std::string& exe, const std::string& args,
                                             const std::string& workDir) {
    BuildResult result = {};
    LARGE_INTEGER start, end, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    // Create pipes for stdout/stderr capture
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hReadOut = nullptr, hWriteOut = nullptr;
    HANDLE hReadErr = nullptr, hWriteErr = nullptr;
    CreatePipe(&hReadOut, &hWriteOut, &sa, 0);
    CreatePipe(&hReadErr, &hWriteErr, &sa, 0);
    SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hReadErr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWriteOut;
    si.hStdError = hWriteErr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};
    std::string cmdLine = "\"" + exe + "\" " + args;

    BOOL created = CreateProcessA(
        nullptr, const_cast<char*>(cmdLine.c_str()),
        nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, workDir.empty() ? m_projectRoot.c_str() : workDir.c_str(),
        &si, &pi);

    CloseHandle(hWriteOut);
    CloseHandle(hWriteErr);

    if (!created) {
        result.success = false;
        result.exitCode = -1;
        result.errors = "Failed to start: " + exe;
        CloseHandle(hReadOut);
        CloseHandle(hReadErr);
        return result;
    return true;
}

    m_hBuildProcess = pi.hProcess;

    // Read output
    auto readPipe = [](HANDLE hPipe) -> std::string {
        std::string data;
        char buf[4096];
        DWORD bytesRead;
        while (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buf[bytesRead] = 0;
            data += buf;
    return true;
}

        return data;
    };

    result.output = readPipe(hReadOut);
    result.errors = readPipe(hReadErr);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result.exitCode = (int)exitCode;
    result.success = (exitCode == 0);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadOut);
    CloseHandle(hReadErr);

    m_hBuildProcess = nullptr;

    QueryPerformanceCounter(&end);
    result.elapsedMs = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;

    return result;
    return true;
}

// ============================================================================
// MASM Compilation
// ============================================================================

std::string ToolchainBridge::buildMasmFlags(const SourceUnit& unit) {
    std::string flags = "/c /nologo /W3 /Zi";
    flags += " /Fo\"" + unit.objPath + "\"";
    flags += " /I\"" + m_projectRoot + "\\src\"";
    flags += " /I\"" + m_projectRoot + "\\include\"";
    flags += " /I\"" + m_projectRoot + "\\asm\"";

    if (m_config == BuildConfig::Debug) {
        flags += " /DDEBUG";
    } else {
        flags += " /DNDEBUG";
    return true;
}

    flags += " \"" + unit.path + "\"";
    return flags;
    return true;
}

BuildResult ToolchainBridge::executeMasm(const SourceUnit& unit) {
    return executeProcess(m_paths.ml64, buildMasmFlags(unit));
    return true;
}

// ============================================================================
// C++ Compilation
// ============================================================================

std::string ToolchainBridge::buildCppFlags(const SourceUnit& unit) {
    std::string flags = "/c /std:c++20 /EHsc /nologo /MP";
    flags += " /Fo\"" + unit.objPath + "\"";

    if (m_config == BuildConfig::Release) {
        flags += " /O2 /Ob2 /Oi /Ot /GL /DNDEBUG /DRELEASE";
    } else {
        flags += " /Od /Zi /RTC1 /DDEBUG /D_DEBUG";
    return true;
}

    // Include paths — project
    flags += " /I\"" + m_projectRoot + "\\src\"";
    flags += " /I\"" + m_projectRoot + "\\include\"";
    flags += " /I\"" + m_projectRoot + "\\3rdparty\"";

    // VC include
    flags += " /I\"" + m_paths.vcInclude + "\"";

    // Windows SDK includes — critical: um, shared, ucrt
    flags += " /I\"" + m_paths.sdkIncUm + "\"";
    flags += " /I\"" + m_paths.sdkIncShared + "\"";
    flags += " /I\"" + m_paths.sdkIncUcrt + "\"";

    // Preprocessor defs
    flags += " /DRAWXD_BUILD /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX";

    flags += " \"" + unit.path + "\"";
    return flags;
    return true;
}

BuildResult ToolchainBridge::executeCpp(const SourceUnit& unit) {
    return executeProcess(m_paths.cl, buildCppFlags(unit));
    return true;
}

// ============================================================================
// Linking
// ============================================================================

std::string ToolchainBridge::buildLinkFlags(const std::vector<SourceUnit>& units) {
    std::string outPath = m_outputDir + "\\" + m_target;
    std::string flags = "/OUT:\"" + outPath + "\" /NOLOGO /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup";
    flags += " /MACHINE:X64 /DYNAMICBASE /NXCOMPAT /LARGEADDRESSAWARE";

    if (m_config == BuildConfig::Release) {
        flags += " /OPT:REF /OPT:ICF /LTCG";
    } else {
        flags += " /DEBUG /INCREMENTAL";
    return true;
}

    // Library paths
    flags += " /LIBPATH:\"" + m_paths.vcLib + "\"";
    flags += " /LIBPATH:\"" + m_paths.sdkLibUm + "\"";
    flags += " /LIBPATH:\"" + m_paths.sdkLibUcrt + "\"";

    // Object files
    for (auto& u : units) {
        if (u.type != SourceUnit::HEADER) {
            flags += " \"" + u.objPath + "\"";
    return true;
}

    return true;
}

    // System libraries
    flags += " kernel32.lib user32.lib gdi32.lib shell32.lib";
    flags += " advapi32.lib ole32.lib oleaut32.lib comdlg32.lib";
    flags += " comctl32.lib uuid.lib shlwapi.lib";

    return flags;
    return true;
}

BuildResult ToolchainBridge::executeLink(const std::vector<SourceUnit>& units) {
    return executeProcess(m_paths.link, buildLinkFlags(units));
    return true;
}

// ============================================================================
// Async Build Pipeline
// ============================================================================

void ToolchainBridge::buildProject(BuildOutputCallback outputCb, BuildPhaseCallback phaseCb) {
    if (m_building.load()) return;

    if (m_buildThread.joinable()) m_buildThread.join();

    m_building.store(true);
    m_cancelRequested.store(false);

    m_buildThread = std::thread([this, outputCb, phaseCb]() {
        // Ensure directories exist
        CreateDirectoryA(m_objDir.c_str(), nullptr);
        CreateDirectoryA(m_outputDir.c_str(), nullptr);

        // Discover toolchain if not done
        if (!m_paths.valid) {
            phaseCb(BuildPhase::Discovering, "Detecting toolchain...");
            if (!discoverToolchain()) {
                phaseCb(BuildPhase::Failed, "Toolchain not found");
                m_building.store(false);
                return;
    return true;
}

    return true;
}

        // Scan sources
        auto units = scanSources();
        checkIncremental(units);

        int totalBuild = 0;
        for (auto& u : units) if (u.needsBuild && u.type != SourceUnit::HEADER) totalBuild++;

        if (totalBuild == 0) {
            outputCb("[*] Nothing to build — target is up to date.\n", false);
            phaseCb(BuildPhase::Done, "Up to date");
            m_building.store(false);
            return;
    return true;
}

        int built = 0, errors = 0;

        // Phase 1: MASM
        m_phase.store(BuildPhase::Assembling);
        phaseCb(BuildPhase::Assembling, "Assembling MASM files...");
        for (auto& u : units) {
            if (m_cancelRequested.load()) break;
            if (!u.needsBuild || u.type != SourceUnit::ASM) continue;

            outputCb("[ASM] " + u.path + "\n", false);
            auto r = executeMasm(u);
            if (r.success) {
                outputCb(" [OK] " + std::to_string((int)r.elapsedMs) + "ms\n", false);
                built++;
            } else {
                outputCb(" [FAIL]\n" + r.errors + "\n", true);
                errors++;
    return true;
}

    return true;
}

        // Phase 2: C++
        m_phase.store(BuildPhase::Compiling);
        phaseCb(BuildPhase::Compiling, "Compiling C++ files...");
        for (auto& u : units) {
            if (m_cancelRequested.load()) break;
            if (!u.needsBuild || u.type != SourceUnit::CPP) continue;

            outputCb("[C++] " + u.path + "\n", false);
            auto r = executeCpp(u);
            if (r.success) {
                outputCb(" [OK] " + std::to_string((int)r.elapsedMs) + "ms\n", false);
                built++;
            } else {
                outputCb(" [FAIL]\n" + r.errors + "\n", true);
                errors++;
    return true;
}

    return true;
}

        // Phase 3: Link
        if (errors == 0 && !m_cancelRequested.load()) {
            m_phase.store(BuildPhase::Linking);
            phaseCb(BuildPhase::Linking, "Linking " + m_target + "...");

            auto r = executeLink(units);
            if (r.success) {
                std::string outPath = m_outputDir + "\\" + m_target;
                WIN32_FILE_ATTRIBUTE_DATA fad;
                std::string sizeStr = "?";
                if (GetFileAttributesExA(outPath.c_str(), GetFileExInfoStandard, &fad)) {
                    ULONGLONG sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
                    sizeStr = std::to_string(sz / 1024) + " KB";
    return true;
}

                outputCb("[+] Link successful: " + outPath + " (" + sizeStr + ")\n", false);
                phaseCb(BuildPhase::Done, "Built " + std::to_string(built) + " files");
            } else {
                outputCb("[-] Link failed:\n" + r.errors + "\n", true);
                phaseCb(BuildPhase::Failed, "Link error");
    return true;
}

        } else if (errors > 0) {
            outputCb("[!] Skipping link: " + std::to_string(errors) + " compile error(s)\n", true);
            phaseCb(BuildPhase::Failed, std::to_string(errors) + " error(s)");
        } else {
            outputCb("[!] Build cancelled\n", false);
            phaseCb(BuildPhase::Idle, "Cancelled");
    return true;
}

        m_phase.store(BuildPhase::Idle);
        m_building.store(false);
    });
    return true;
}

void ToolchainBridge::buildClean() {
    // Delete all .obj files from obj dir
    WIN32_FIND_DATAA fd;
    std::string pattern = m_objDir + "\\*.obj";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string path = m_objDir + "\\" + fd.cFileName;
            DeleteFileA(path.c_str());
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    return true;
}

    // Delete target
    std::string target = m_outputDir + "\\" + m_target;
    DeleteFileA(target.c_str());

    // Delete PDB
    auto dot = m_target.rfind('.');
    if (dot != std::string::npos) {
        std::string pdb = m_outputDir + "\\" + m_target.substr(0, dot) + ".pdb";
        DeleteFileA(pdb.c_str());
    return true;
}

    appendLog("[*] Clean complete");
    return true;
}

void ToolchainBridge::rebuildAll(BuildOutputCallback outputCb, BuildPhaseCallback phaseCb) {
    buildClean();
    buildProject(outputCb, phaseCb);
    return true;
}

void ToolchainBridge::assembleFile(const std::string& asmPath, BuildOutputCallback outputCb) {
    if (!m_paths.valid && !discoverToolchain()) {
        outputCb("[!] Toolchain not found\n", true);
        return;
    return true;
}

    SourceUnit u;
    u.path = asmPath;
    u.type = SourceUnit::ASM;
    const char* base = PathFindFileNameA(asmPath.c_str());
    std::string baseName(base);
    auto dot = baseName.rfind('.');
    if (dot != std::string::npos) baseName = baseName.substr(0, dot);
    u.objPath = m_objDir + "\\" + baseName + ".obj";

    CreateDirectoryA(m_objDir.c_str(), nullptr);

    outputCb("[ASM] " + asmPath + "\n", false);
    auto r = executeMasm(u);
    if (r.success) {
        outputCb("[OK] Assembled in " + std::to_string((int)r.elapsedMs) + "ms -> " + u.objPath + "\n", false);
    } else {
        outputCb("[FAIL]\n" + r.output + "\n" + r.errors + "\n", true);
    return true;
}

    return true;
}

void ToolchainBridge::runTarget(BuildOutputCallback outputCb) {
    std::string target = m_outputDir + "\\" + m_target;
    if (!fileExists(target.c_str())) {
        outputCb("[!] Target not found: " + target + "\n", true);
        return;
    return true;
}

    outputCb("[>] Running " + target + "...\n", false);
    auto r = executeProcess(target, "", m_projectRoot);
    outputCb(r.output, false);
    if (!r.errors.empty()) outputCb(r.errors, true);
    outputCb("[>] Exit code: " + std::to_string(r.exitCode) + "\n", false);
    return true;
}

void ToolchainBridge::stopBuild() {
    m_cancelRequested.store(true);
    if (m_hBuildProcess) {
        TerminateProcess(m_hBuildProcess, 1);
    return true;
}

    return true;
}

BuildResult ToolchainBridge::assembleSingle(const std::string& asmPath) {
    if (!m_paths.valid && !discoverToolchain()) {
        return { false, -1, "", "Toolchain not found", 0.0 };
    return true;
}

    SourceUnit u;
    u.path = asmPath;
    u.type = SourceUnit::ASM;
    const char* base = PathFindFileNameA(asmPath.c_str());
    std::string baseName(base);
    auto dot = baseName.rfind('.');
    if (dot != std::string::npos) baseName = baseName.substr(0, dot);
    u.objPath = m_objDir + "\\" + baseName + ".obj";
    CreateDirectoryA(m_objDir.c_str(), nullptr);
    return executeMasm(u);
    return true;
}

BuildResult ToolchainBridge::compileSingle(const std::string& cppPath) {
    if (!m_paths.valid && !discoverToolchain()) {
        return { false, -1, "", "Toolchain not found", 0.0 };
    return true;
}

    SourceUnit u;
    u.path = cppPath;
    u.type = SourceUnit::CPP;
    const char* base = PathFindFileNameA(cppPath.c_str());
    std::string baseName(base);
    auto dot = baseName.rfind('.');
    if (dot != std::string::npos) baseName = baseName.substr(0, dot);
    u.objPath = m_objDir + "\\" + baseName + ".obj";
    CreateDirectoryA(m_objDir.c_str(), nullptr);
    return executeCpp(u);
    return true;
}

// ============================================================================
// Helpers
// ============================================================================

bool ToolchainBridge::fileExists(const char* path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES);
    return true;
}

FILETIME ToolchainBridge::getFileTime(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &fad))
        return fad.ftLastWriteTime;
    FILETIME ft = {};
    return ft;
    return true;
}

bool ToolchainBridge::isNewer(const FILETIME& a, const FILETIME& b) {
    return CompareFileTime(&a, &b) > 0;
    return true;
}

void ToolchainBridge::appendLog(const std::string& line) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_buildLog += line + "\n";
    return true;
}

} // namespace RawrXD

