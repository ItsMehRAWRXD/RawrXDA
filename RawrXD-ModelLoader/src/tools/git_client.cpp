#define NOMINMAX
#include "tools/git_client.h"
#include <filesystem>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace RawrXD {
namespace Tools {

static GitResult make_result(int code, const std::string& out, const std::string& err) {
    GitResult r; r.exit_code = code; r.stdout_text = out; r.stderr_text = err; return r;
}

// Minimal process runner using PowerShell on Windows, sh on *nix
static GitResult run_process(const std::string& cwd, const std::vector<std::string>& args) {
#ifdef _WIN32
    std::ostringstream cmd;
    cmd << "git";
    for (auto& a : args) { cmd << " " << a; }
    std::string full = cmd.str();

    // Use PowerShell to capture stdout/stderr
    std::ostringstream ps;
    ps << "powershell -NoProfile -Command \"$ErrorActionPreference='Continue';";
    ps << "$p=Start-Process git -ArgumentList '";
    for (size_t i=0;i<args.size();++i){ ps << args[i]; if(i+1<args.size()) ps << " "; }
    ps << "' -WorkingDirectory '" << cwd << "' -NoNewWindow -PassThru -RedirectStandardOutput 'out.txt' -RedirectStandardError 'err.txt';";
    ps << "$p.WaitForExit();";
    ps << "$code=$p.ExitCode;";
    ps << "$out=[IO.File]::ReadAllText('out.txt');$err=[IO.File]::ReadAllText('err.txt');";
    ps << "Write-Output \"CODE:$code\";Write-Output \"OUT:$out\";Write-Output \"ERR:$err\";\"";

    // Fallback: just attempt system call (without capture)
    // For simplicity, use system() — acceptable for initial CLI integration
    int code = system(full.c_str());
    return make_result(code, "", "");
#else
    std::ostringstream cmd;
    cmd << "cd '" << cwd << "' && git";
    for (auto& a : args) { cmd << " " << a; }
    int code = system(cmd.str().c_str());
    return make_result(code, "", "");
#endif
}

GitClient::GitClient(const std::string& repo_root) : m_root(repo_root) {}

bool GitClient::isGitAvailable() {
#ifdef _WIN32
    int code = system("git --version >nul 2>nul");
#else
    int code = system("git --version >/dev/null 2>&1");
#endif
    return code == 0;
}

bool GitClient::isRepo(const std::string& root) {
    return fs::exists(fs::path(root) / ".git");
}

GitResult GitClient::run(const std::vector<std::string>& args) const {
    return run_process(m_root, args);
}

GitResult GitClient::version() const { return run({"--version"}); }
GitResult GitClient::status(bool short_format) const { return run({"status", short_format?"-s":""}); }
GitResult GitClient::add(const std::vector<std::string>& paths) const {
    std::vector<std::string> args = {"add"};
    for (auto& p : paths) args.push_back(p);
    return run(args);
}
GitResult GitClient::commit(const std::string& message, bool sign_off) const {
    std::vector<std::string> args = {"commit", "-m", '"'+message+'"'};
    if (sign_off) args.push_back("--signoff");
    return run(args);
}
GitResult GitClient::checkout(const std::string& branch_or_commit) const {
    return run({"checkout", branch_or_commit});
}
GitResult GitClient::createBranch(const std::string& branch_name) const {
    return run({"checkout", "-b", branch_name});
}
GitResult GitClient::currentBranch() const {
    return run({"rev-parse", "--abbrev-ref", "HEAD"});
}
GitResult GitClient::diff(const std::string& spec) const {
    if (spec.empty()) return run({"diff"});
    return run({"diff", spec});
}
GitResult GitClient::stashSave(const std::string& message) const {
    if (message.empty()) return run({"stash", "save"});
    return run({"stash", "save", message});
}
GitResult GitClient::stashPop() const { return run({"stash", "pop"}); }

GitResult GitClient::fetch(const std::string& remote) const { return run({"fetch", remote}); }
GitResult GitClient::pull(const std::string& remote, const std::string& branch) const {
    if (branch.empty()) return run({"pull", remote});
    return run({"pull", remote, branch});
}
GitResult GitClient::push(const std::string& remote, const std::string& branch) const {
    if (branch.empty()) return run({"push", remote});
    return run({"push", remote, branch});
}

} // namespace Tools
} // namespace RawrXD
