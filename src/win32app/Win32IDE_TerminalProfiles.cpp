#include <string>
#include <vector>
#include <mutex>

namespace RawrXD::IDE {

struct TerminalProfile {
    std::string name;
    std::string shellPath;
    std::vector<std::string> args;
};

class TerminalProfileManager {
public:
    TerminalProfileManager() {
#ifdef _WIN32
        profiles_.push_back({"PowerShell", "powershell.exe", {"-NoLogo"}});
        profiles_.push_back({"Command Prompt", "cmd.exe", {"/K"}});
        profiles_.push_back({"Git Bash", "C:\\Program Files\\Git\\bin\\bash.exe", {"--login", "-i"}});
#else
        profiles_.push_back({"Bash", "/bin/bash", {"-l"}});
#endif
    }

    int count() const {
        std::lock_guard<std::mutex> lock(mu_);
        return static_cast<int>(profiles_.size());
    }

    bool addProfile(const TerminalProfile& p) {
        std::lock_guard<std::mutex> lock(mu_);
        for (const auto& existing : profiles_) {
            if (existing.name == p.name) return false;
        }
        profiles_.push_back(p);
        return true;
    }

    const TerminalProfile* getByIndex(int idx) const {
        std::lock_guard<std::mutex> lock(mu_);
        if (idx < 0 || idx >= static_cast<int>(profiles_.size())) return nullptr;
        return &profiles_[idx];
    }

private:
    mutable std::mutex mu_;
    std::vector<TerminalProfile> profiles_;
};

static TerminalProfileManager g_termProfiles;

} // namespace RawrXD::IDE

extern "C" {

int RawrXD_IDE_TerminalProfileCount() {
    return RawrXD::IDE::g_termProfiles.count();
}

bool RawrXD_IDE_AddTerminalProfile(const char* name, const char* shellPath) {
    if (!name || !shellPath) return false;
    RawrXD::IDE::TerminalProfile p;
    p.name = name;
    p.shellPath = shellPath;
    return RawrXD::IDE::g_termProfiles.addProfile(p);
}

}
