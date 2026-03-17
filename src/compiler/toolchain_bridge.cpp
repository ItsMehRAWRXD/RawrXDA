/*==========================================================================
 * RawrXD Toolchain Bridge — Implementation
 *
 * Full compile→assemble→link pipeline with MSVC backend + from-scratch
 * toolchain fallback. Runs build on worker thread, pushes diagnostics
 * and progress to IDE callbacks.
 *=========================================================================*/

#include "toolchain_bridge.hpp"
#include <sstream>
#include <algorithm>
#include <filesystem>  // Only for path manipulation, NOT recursive traversal
#include <regex>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

// Optional: from-scratch toolchain headers (C linkage)
#ifdef RAWRXD_FROM_SCRATCH_TOOLCHAIN
extern "C" {
#include "../../toolchain/from_scratch/phase1_assembler/x64_encoder.h"
#include "../../toolchain/from_scratch/phase2_linker/coff_reader.h"
#include "../../toolchain/from_scratch/phase2_linker/section_merge.h"
#include "../../toolchain/from_scratch/phase2_linker/pe_writer.h"
}
#endif

namespace RawrXD::Compiler {

// =========================================================================
// Construction / Destruction
// =========================================================================

ToolchainBridge::ToolchainBridge() = default;

ToolchainBridge::~ToolchainBridge() {
    cancel_.store(true);
    if (build_thread_.joinable()) {
        build_thread_.join();
    }
}

// =========================================================================
// Toolchain Detection
// =========================================================================

bool ToolchainBridge::detectToolchain() {
    bool msvc = findMSVC();
    bool custom = findFromScratch();

    if (backend_ == ToolchainBackend::Auto) {
        if (msvc)       backend_ = ToolchainBackend::MSVC;
        else if (custom) backend_ = ToolchainBackend::FromScratch;
    }

    if (msvc) {
        emit("[+] Toolchain ready:");
        emit("    ml64: " + msvc_ml64_);
        emit("    cl:   " + msvc_cl_);
        emit("    link: " + msvc_link_);
    }
    if (custom) {
        emit("[+] From-scratch toolchain available (in-process assembler + linker)");
    }

    return msvc || custom;
}

bool ToolchainBridge::findMSVC() {
    std::string vswhere = findVsWhere();
    if (vswhere.empty()) return false;

    std::string vs_path = queryVsPath();
    if (vs_path.empty()) return false;

    // Find MSVC version directory
    std::string tools_base = vs_path + "\\VC\\Tools\\MSVC";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA((tools_base + "\\*").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    std::string best_ver;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            fd.cFileName[0] != '.') {
            std::string ver = fd.cFileName;
            if (ver > best_ver) best_ver = ver;
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);

    if (best_ver.empty()) return false;

    std::string bin_dir = tools_base + "\\" + best_ver + "\\bin\\Hostx64\\x64";

    // Verify tools exist
    msvc_cl_   = bin_dir + "\\cl.exe";
    msvc_ml64_ = bin_dir + "\\ml64.exe";
    msvc_link_ = bin_dir + "\\link.exe";
    msvc_lib_  = bin_dir + "\\lib.exe";
    vc_include_ = tools_base + "\\" + best_ver + "\\include";
    vc_lib_     = tools_base + "\\" + best_ver + "\\lib\\x64";

    if (!PathFileExistsA(msvc_cl_.c_str()) ||
        !PathFileExistsA(msvc_ml64_.c_str()) ||
        !PathFileExistsA(msvc_link_.c_str())) {
        msvc_cl_.clear();
        msvc_ml64_.clear();
        msvc_link_.clear();
        return false;
    }

    // Find Windows SDK
    std::string sdk_base = "C:\\Program Files (x86)\\Windows Kits\\10\\include";
    hFind = FindFirstFileA((sdk_base + "\\*").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        std::string best_sdk;
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                fd.cFileName[0] == '1') {  // 10.0.xxxxx.0
                std::string sdkver = fd.cFileName;
                if (sdkver > best_sdk) best_sdk = sdkver;
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);

        if (!best_sdk.empty()) {
            sdk_include_um_     = sdk_base + "\\" + best_sdk + "\\um";
            sdk_include_shared_ = sdk_base + "\\" + best_sdk + "\\shared";
            sdk_include_ucrt_   = sdk_base + "\\" + best_sdk + "\\ucrt";

            std::string sdk_lib_base = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\" + best_sdk;
            sdk_lib_um_   = sdk_lib_base + "\\um\\x64";
            sdk_lib_ucrt_ = sdk_lib_base + "\\ucrt\\x64";
        }
    }

    // Source vcvars to get environment set up
    std::string vcvars = vs_path + "\\VC\\Auxiliary\\Build\\vcvars64.bat";
    if (PathFileExistsA(vcvars.c_str())) {
        importVcVars(vcvars);
    }

    return true;
}

bool ToolchainBridge::findFromScratch() {
#ifdef RAWRXD_FROM_SCRATCH_TOOLCHAIN
    from_scratch_ready_ = true;
    return true;
#else
    from_scratch_ready_ = false;
    return false;
#endif
}

std::string ToolchainBridge::findVsWhere() {
    std::string path = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    if (PathFileExistsA(path.c_str())) return path;
    return "";
}

std::string ToolchainBridge::queryVsPath() {
    std::string vswhere = findVsWhere();
    if (vswhere.empty()) return "";

    std::string cmd = "\"" + vswhere + "\" -latest -products * "
                      "-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 "
                      "-property installationPath";
    auto result = runProcess(cmd);
    if (result.exit_code != 0) return "";

    // Trim whitespace
    std::string path = result.stdout_text;
    while (!path.empty() && (path.back() == '\n' || path.back() == '\r' || path.back() == ' '))
        path.pop_back();
    return path;
}

void ToolchainBridge::importVcVars(const std::string& vcvars_path) {
    // Run vcvars64 and capture environment
    std::string tmp = std::string(getenv("TEMP") ? getenv("TEMP") : "C:\\Temp") + "\\rawrxd_vcvars.txt";
    std::string cmd = "cmd /c \"\"" + vcvars_path + "\" && set > \"" + tmp + "\"\"";
    runProcess(cmd);

    // Parse environment file
    HANDLE hFile = CreateFileA(tmp.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD size = GetFileSize(hFile, nullptr);
    std::string content(size, '\0');
    DWORD read;
    ReadFile(hFile, content.data(), size, &read, nullptr);
    CloseHandle(hFile);
    DeleteFileA(tmp.c_str());

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        auto eq = line.find('=');
        if (eq != std::string::npos) {
            SetEnvironmentVariableA(line.substr(0, eq).c_str(),
                                    line.substr(eq + 1).c_str());
        }
    }
}

// =========================================================================
// Build — Async
// =========================================================================

bool ToolchainBridge::buildAsync(const BuildTarget& target) {
    if (building_.load()) return false;

    building_.store(true);
    cancel_.store(false);

    if (build_thread_.joinable()) build_thread_.join();

    build_thread_ = std::thread([this, target]() {
        buildSync(target);
        building_.store(false);
    });

    return true;
}

// =========================================================================
// Build — Sync
// =========================================================================

bool ToolchainBridge::buildSync(const BuildTarget& target) {
    auto start = std::chrono::high_resolution_clock::now();

    diags_.clear();
    error_count_ = 0;
    warning_count_ = 0;

    // Ensure output directories
    ensureDir(target.obj_dir);
    ensureDir(target.output_dir);

    // Separate sources by language
    std::vector<std::string> cpp_files, asm_files;
    for (const auto& src : target.source_files) {
        if (cancel_.load()) { emit("[!] Build cancelled."); return false; }
        auto lang = detectLanguage(src);
        if (lang == SourceLang::Cpp || lang == SourceLang::C)
            cpp_files.push_back(src);
        else if (lang == SourceLang::Asm)
            asm_files.push_back(src);
    }

    int total = static_cast<int>(cpp_files.size() + asm_files.size());
    int done = 0;

    BuildProgress prog;
    prog.files_total = total;
    prog.phase = "Assemble";

    // Phase 1: Assemble .asm files
    std::vector<std::string> obj_files;
    for (const auto& src : asm_files) {
        if (cancel_.load()) { emit("[!] Build cancelled."); return false; }

        prog.current_file = src;
        prog.files_done = done;
        if (on_prog_) on_prog_(prog);

        bool ok = compileAsm(src, target);
        if (ok) {
            obj_files.push_back(objPath(src, target));
        }
        done++;
    }

    // Phase 2: Compile C/C++ files
    prog.phase = "Compile";
    for (const auto& src : cpp_files) {
        if (cancel_.load()) { emit("[!] Build cancelled."); return false; }

        prog.current_file = src;
        prog.files_done = done;
        if (on_prog_) on_prog_(prog);

        bool ok = compileCpp(src, target);
        if (ok) {
            obj_files.push_back(objPath(src, target));
        }
        done++;
    }

    // Check for compile errors
    if (error_count_ > 0) {
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        prog.finished = true;
        prog.success = false;
        prog.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        if (on_prog_) on_prog_(prog);

        emit("[!] Build failed: " + std::to_string(error_count_) + " error(s)");
        return false;
    }

    // Phase 3: Link
    prog.phase = "Link";
    prog.current_file = target.name;
    if (on_prog_) on_prog_(prog);

    bool link_ok = linkObjects(obj_files, target);

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    prog.finished = true;
    prog.success = link_ok;
    prog.elapsed_ms = ms;
    prog.files_done = total;
    if (link_ok) {
        prog.output_path = target.output_dir + "\\" + target.name;
    }
    if (on_prog_) on_prog_(prog);

    if (link_ok) {
        emit("[+] Build successful: " + prog.output_path +
             " (" + std::to_string(ms) + "ms)");
    } else {
        emit("[-] Link failed");
    }

    return link_ok;
}

// =========================================================================
// Single-file compile
// =========================================================================

bool ToolchainBridge::compileFile(const std::string& path,
                                  const BuildTarget& target) {
    auto lang = detectLanguage(path);
    if (lang == SourceLang::Cpp || lang == SourceLang::C)
        return compileCpp(path, target);
    else if (lang == SourceLang::Asm)
        return compileAsm(path, target);
    return false;
}

bool ToolchainBridge::assembleFile(const std::string& path,
                                    const BuildTarget& target) {
    return compileAsm(path, target);
}

// =========================================================================
// C++ Compilation (MSVC backend)
// =========================================================================

bool ToolchainBridge::compileCpp(const std::string& src,
                                  const BuildTarget& tgt) {
    if (msvc_cl_.empty()) {
        emitDiag(DiagLevel::Fatal, src, 0, "", "C++ compiler (cl.exe) not found");
        return false;
    }

    std::string obj = objPath(src, tgt);
    if (!needsRebuild(src, obj)) {
        emit("  [SKIP] " + src);
        return true;
    }

    emit("  [C++] " + src);

    std::ostringstream cmd;
    cmd << "\"" << msvc_cl_ << "\"";
    cmd << " /c /std:c++20 /EHsc /nologo";
    cmd << " /Fo\"" << obj << "\"";

    // Config-specific flags
    switch (config_) {
    case BuildConfig::Debug:
        cmd << " /Od /Zi /RTC1 /DDEBUG /D_DEBUG";
        break;
    case BuildConfig::Release:
        cmd << " /O2 /Ob2 /Oi /Ot /GL /DNDEBUG /DRELEASE";
        break;
    case BuildConfig::RelWithDebInfo:
        cmd << " /O2 /Zi /DNDEBUG";
        break;
    }

    // Standard defines
    cmd << " /DRAWXD_BUILD /DUNICODE /D_UNICODE";
    cmd << " /DWIN32_LEAN_AND_MEAN /DNOMINMAX";

    // User defines
    for (const auto& d : tgt.defines)
        cmd << " /D\"" << d << "\"";

    // Include paths — always include SDK paths
    for (const auto& inc : tgt.include_dirs)
        cmd << " /I\"" << inc << "\"";

    if (!vc_include_.empty())
        cmd << " /I\"" << vc_include_ << "\"";
    if (!sdk_include_um_.empty())
        cmd << " /I\"" << sdk_include_um_ << "\"";
    if (!sdk_include_shared_.empty())
        cmd << " /I\"" << sdk_include_shared_ << "\"";
    if (!sdk_include_ucrt_.empty())
        cmd << " /I\"" << sdk_include_ucrt_ << "\"";

    cmd << " \"" << src << "\"";

    auto result = runProcess(cmd.str(), project_root_);

    if (result.exit_code != 0) {
        parseMsvcOutput(result.stdout_text + result.stderr_text);
        emit("  [FAIL] " + src);
        return false;
    }

    emit("  [OK]");
    return true;
}

// =========================================================================
// ASM Compilation (ml64 backend)
// =========================================================================

bool ToolchainBridge::compileAsm(const std::string& src,
                                  const BuildTarget& tgt) {
    // Try custom assembler first if available
    if (backend_ == ToolchainBackend::FromScratch && from_scratch_ready_) {
        return assembleCustom(src, tgt);
    }

    if (msvc_ml64_.empty()) {
        emitDiag(DiagLevel::Fatal, src, 0, "", "MASM64 (ml64.exe) not found");
        return false;
    }

    std::string obj = objPath(src, tgt);
    if (!needsRebuild(src, obj)) {
        emit("  [SKIP] " + src);
        return true;
    }

    emit("  [ASM] " + src);

    std::ostringstream cmd;
    cmd << "\"" << msvc_ml64_ << "\"";
    cmd << " /c /nologo /W3 /Zi";
    cmd << " /Fo\"" << obj << "\"";

    // MASM defines (no /Od, /O2 — ml64 doesn't support them)
    if (config_ == BuildConfig::Debug)
        cmd << " /DDEBUG";
    else
        cmd << " /DNDEBUG";

    // Include paths
    for (const auto& inc : tgt.include_dirs)
        cmd << " /I\"" << inc << "\"";

    cmd << " \"" << src << "\"";

    auto result = runProcess(cmd.str(), project_root_);

    if (result.exit_code != 0) {
        parseMl64Output(result.stdout_text + result.stderr_text);
        emit("  [FAIL] " + src);
        return false;
    }

    emit("  [OK]");
    return true;
}

// =========================================================================
// Linking (MSVC backend)
// =========================================================================

bool ToolchainBridge::linkObjects(const std::vector<std::string>& objs,
                                   const BuildTarget& tgt) {
    if (backend_ == ToolchainBackend::FromScratch && from_scratch_ready_) {
        return linkCustom(objs, tgt);
    }

    if (msvc_link_.empty()) {
        emitDiag(DiagLevel::Fatal, "", 0, "", "Linker (link.exe) not found");
        return false;
    }

    std::string out_path = tgt.output_dir + "\\" + tgt.name;
    emit("[LINK] " + out_path);

    std::ostringstream cmd;
    cmd << "\"" << msvc_link_ << "\"";
    cmd << " /OUT:\"" << out_path << "\"";
    cmd << " /NOLOGO /MACHINE:X64";

    // Subsystem
    switch (tgt.subsystem) {
    case Subsystem::Console:
        cmd << " /SUBSYSTEM:CONSOLE";
        if (!tgt.entry_point.empty())
            cmd << " /ENTRY:" << tgt.entry_point;
        break;
    case Subsystem::Windows:
        cmd << " /SUBSYSTEM:WINDOWS";
        if (!tgt.entry_point.empty())
            cmd << " /ENTRY:" << tgt.entry_point;
        else
            cmd << " /ENTRY:WinMainCRTStartup";
        break;
    }

    // Config flags
    switch (config_) {
    case BuildConfig::Debug:
        cmd << " /DEBUG /INCREMENTAL";
        break;
    case BuildConfig::Release:
        cmd << " /OPT:REF /OPT:ICF /LTCG";
        break;
    case BuildConfig::RelWithDebInfo:
        cmd << " /DEBUG /OPT:REF /OPT:ICF";
        break;
    }

    cmd << " /DYNAMICBASE /NXCOMPAT /LARGEADDRESSAWARE";

    // Library paths
    if (!vc_lib_.empty())
        cmd << " /LIBPATH:\"" << vc_lib_ << "\"";
    if (!sdk_lib_um_.empty())
        cmd << " /LIBPATH:\"" << sdk_lib_um_ << "\"";
    if (!sdk_lib_ucrt_.empty())
        cmd << " /LIBPATH:\"" << sdk_lib_ucrt_ << "\"";

    // Object files
    for (const auto& obj : objs)
        cmd << " \"" << obj << "\"";

    // Libraries
    for (const auto& lib : tgt.link_libs)
        cmd << " \"" << lib << "\"";

    // Default libs
    cmd << " kernel32.lib user32.lib";

    auto result = runProcess(cmd.str(), project_root_);

    if (result.exit_code != 0) {
        parseLinkOutput(result.stdout_text + result.stderr_text);
        return false;
    }

    return true;
}

// =========================================================================
// Custom Toolchain — In-Process Assembler
// =========================================================================

bool ToolchainBridge::assembleCustom(const std::string& src,
                                       const BuildTarget& tgt) {
#ifdef RAWRXD_FROM_SCRATCH_TOOLCHAIN
    emit("  [ASM/CUSTOM] " + src);
    // TODO: Wire to x64_encoder API when parser is complete
    // For now, fall back to ml64
    if (!msvc_ml64_.empty()) {
        emit("  [FALLBACK] Using ml64 (custom parser incomplete)");
        return compileAsm(src, tgt);
    }
    emitDiag(DiagLevel::Fatal, src, 0, "", "Custom assembler not yet complete, and ml64 unavailable");
    return false;
#else
    if (!msvc_ml64_.empty()) return compileAsm(src, tgt);
    emitDiag(DiagLevel::Fatal, src, 0, "", "No assembler available");
    return false;
#endif
}

// =========================================================================
// Custom Toolchain — In-Process Linker
// =========================================================================

bool ToolchainBridge::linkCustom(const std::vector<std::string>& objs,
                                   const BuildTarget& tgt) {
#ifdef RAWRXD_FROM_SCRATCH_TOOLCHAIN
    emit("[LINK/CUSTOM] In-process linker");

    // Read all COFF objects
    std::vector<coff_file_t*> coff_files;
    for (const auto& obj_path : objs) {
        coff_file_t* cf = coff_file_open(obj_path.c_str());
        if (!cf) {
            emitDiag(DiagLevel::Error, obj_path, 0, "LNK1104",
                     "Cannot open input file: " + obj_path);
            for (auto* prev : coff_files) coff_file_free(prev);
            return false;
        }
        coff_files.push_back(cf);
    }

    // Merge sections
    merged_section_t* merged = section_merge_all(
        coff_files.data(), static_cast<int>(coff_files.size()));

    // Build PE
    pe_builder_t* pb = pe_builder_new(COFF_MACHINE_AMD64);
    pe_builder_set_image_base(pb, 0x140000000ULL);

    switch (tgt.subsystem) {
    case Subsystem::Console: pe_builder_set_subsystem(pb, PE_SUBSYS_CONSOLE); break;
    case Subsystem::Windows: pe_builder_set_subsystem(pb, PE_SUBSYS_WINDOWS); break;
    }

    pe_builder_from_merge(pb, /* merge context */ nullptr); // TODO: wire merge_context

    std::string out_path = tgt.output_dir + "\\" + tgt.name;
    int ok = pe_builder_write(pb, out_path.c_str());

    pe_builder_free(pb);
    merged_section_free(merged);
    for (auto* cf : coff_files) coff_file_free(cf);

    return ok == 0;
#else
    return linkObjects(objs, tgt);
#endif
}

// =========================================================================
// Process Execution
// =========================================================================

ToolchainBridge::ProcessResult
ToolchainBridge::runProcess(const std::string& cmdline,
                             const std::string& working_dir) {
    ProcessResult result{};
    auto start = std::chrono::high_resolution_clock::now();

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE out_read, out_write;
    CreatePipe(&out_read, &out_write, &sa, 0);
    SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = out_write;
    si.hStdError = out_write;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::string cmd_copy = cmdline;

    BOOL ok = CreateProcessA(
        nullptr, cmd_copy.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr,
        working_dir.empty() ? nullptr : working_dir.c_str(),
        &si, &pi);

    CloseHandle(out_write);

    if (!ok) {
        CloseHandle(out_read);
        result.exit_code = -1;
        result.stderr_text = "Failed to create process: " + cmdline;
        return result;
    }

    // Read output
    char buffer[4096];
    DWORD bytes_read;
    while (ReadFile(out_read, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) &&
           bytes_read > 0) {
        buffer[bytes_read] = '\0';
        result.stdout_text += buffer;
    }

    CloseHandle(out_read);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    result.exit_code = static_cast<int>(exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    return result;
}

// =========================================================================
// Diagnostic Parsing
// =========================================================================

void ToolchainBridge::parseMsvcOutput(const std::string& output) {
    // MSVC format: file.cpp(line): error C####: message
    std::regex rx(R"((.+)\((\d+)\)\s*:\s*(error|warning|note)\s+(C\d+)\s*:\s*(.+))");
    std::sregex_iterator it(output.begin(), output.end(), rx);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        DiagLevel lvl = DiagLevel::Info;
        if ((*it)[3] == "error")   lvl = DiagLevel::Error;
        if ((*it)[3] == "warning") lvl = DiagLevel::Warning;

        emitDiag(lvl, (*it)[1], std::stoi((*it)[2]), (*it)[4], (*it)[5]);
    }
}

void ToolchainBridge::parseMl64Output(const std::string& output) {
    // ML64 format: file.asm(line) : error A####: message
    //              or: file.asm(line) : fatal error A####: message
    std::regex rx(R"((.+)\((\d+)\)\s*:\s*(?:fatal\s+)?(error|warning)\s+(A\d+)\s*:\s*(.+))");
    std::sregex_iterator it(output.begin(), output.end(), rx);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        DiagLevel lvl = ((*it)[3] == "error") ? DiagLevel::Error : DiagLevel::Warning;
        emitDiag(lvl, (*it)[1], std::stoi((*it)[2]), (*it)[4], (*it)[5]);
    }
}

void ToolchainBridge::parseLinkOutput(const std::string& output) {
    // LINK format: file.obj : error LNK####: message
    std::regex rx(R"((.+)\s*:\s*(error|warning)\s+(LNK\d+)\s*:\s*(.+))");
    std::sregex_iterator it(output.begin(), output.end(), rx);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        DiagLevel lvl = ((*it)[2] == "error") ? DiagLevel::Error : DiagLevel::Warning;
        emitDiag(lvl, (*it)[1], 0, (*it)[3], (*it)[4]);
    }
}

// =========================================================================
// Helpers
// =========================================================================

void ToolchainBridge::emit(const std::string& msg) {
    if (on_out_) on_out_(msg);
}

void ToolchainBridge::emitDiag(DiagLevel lvl, const std::string& file,
                                int line, const std::string& code,
                                const std::string& msg) {
    std::lock_guard<std::mutex> lock(diag_mutex_);

    BuildDiagnostic d;
    d.level   = lvl;
    d.file    = file;
    d.line    = line;
    d.code    = code;
    d.message = msg;
    diags_.push_back(d);

    if (lvl == DiagLevel::Error || lvl == DiagLevel::Fatal) error_count_++;
    if (lvl == DiagLevel::Warning) warning_count_++;

    if (on_diag_) on_diag_(d);
}

std::string ToolchainBridge::objPath(const std::string& src,
                                      const BuildTarget& tgt) {
    // Extract basename, change extension to .obj
    auto slash = src.find_last_of("\\/");
    std::string name = (slash != std::string::npos) ? src.substr(slash + 1) : src;
    auto dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return tgt.obj_dir + "\\" + name + ".obj";
}

bool ToolchainBridge::ensureDir(const std::string& dir) {
    if (PathFileExistsA(dir.c_str())) return true;
    return CreateDirectoryA(dir.c_str(), nullptr) != 0;
}

bool ToolchainBridge::needsRebuild(const std::string& src,
                                     const std::string& obj) {
    if (!PathFileExistsA(obj.c_str())) return true;

    WIN32_FILE_ATTRIBUTE_DATA src_info{}, obj_info{};
    if (!GetFileAttributesExA(src.c_str(), GetFileExInfoStandard, &src_info)) return true;
    if (!GetFileAttributesExA(obj.c_str(), GetFileExInfoStandard, &obj_info)) return true;

    return CompareFileTime(&src_info.ftLastWriteTime, &obj_info.ftLastWriteTime) > 0;
}

SourceLang ToolchainBridge::detectLanguage(const std::string& path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return SourceLang::Unknown;

    std::string ext = path.substr(dot);
    // Lowercase
    for (auto& c : ext) c = static_cast<char>(tolower(c));

    if (ext == ".cpp" || ext == ".cxx" || ext == ".cc") return SourceLang::Cpp;
    if (ext == ".c")   return SourceLang::C;
    if (ext == ".asm") return SourceLang::Asm;
    if (ext == ".h" || ext == ".hpp" || ext == ".hxx") return SourceLang::Header;
    return SourceLang::Unknown;
}

std::vector<std::string> ToolchainBridge::discoverSources(
    const std::vector<std::string>& dirs, bool recursive) {

    std::vector<std::string> results;

    for (const auto& dir : dirs) {
        if (!PathFileExistsA(dir.c_str())) continue;

        // Stack-based directory traversal (no std::filesystem)
        std::vector<std::string> stack = { dir };
        while (!stack.empty()) {
            std::string current = stack.back();
            stack.pop_back();

            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA((current + "\\*").c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) continue;

            do {
                if (fd.cFileName[0] == '.') continue;
                std::string full = current + "\\" + fd.cFileName;

                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (recursive) stack.push_back(full);
                } else {
                    auto lang = detectLanguage(full);
                    if (lang == SourceLang::Cpp || lang == SourceLang::C ||
                        lang == SourceLang::Asm) {
                        results.push_back(full);
                    }
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }

    return results;
}

} // namespace RawrXD::Compiler
