#include "git_integration.h"
#include <windows.h>
#include <array>

namespace RawrXD::VCS {
    std::string GitIntegration::runGit(const std::string& repoPath, const std::vector<std::string>& args) {
        std::string cmd = "git";
        for (const auto& arg : args) {
            cmd += " \"" + arg + "\"";
        }

        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        HANDLE hRead, hWrite;
        CreatePipe(&hRead, &hWrite, &sa, 0);
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{sizeof(si)};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;

        PROCESS_INFORMATION pi{};
        std::string cmdLine = "cmd.exe /c cd /d \"" + repoPath + "\" && " + cmd;

        BOOL created = CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

        CloseHandle(hWrite);

        std::string output;
        if (created) {
            char buffer[4096];
            DWORD read;
            while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &read, nullptr) && read > 0) {
                buffer[read] = '\0';
                output += buffer;
            }
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        CloseHandle(hRead);
        return output;
    }

    std::vector<std::string> GitIntegration::getModifiedFiles(const std::string& repoPath) {
        std::string out = runGit(repoPath, {"status", "--porcelain"});
        std::vector<std::string> files;
        std::istringstream stream(out);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.size() > 3) files.push_back(line.substr(3));
        }
        return files;
    }

    std::string GitIntegration::getCurrentBranch(const std::string& repoPath) {
        std::string out = runGit(repoPath, {"rev-parse", "--abbrev-ref", "HEAD"});
        // Trim newline
        out.erase(out.find_last_not_of(" \n\r\t") + 1);
        return out;
    }

    void GitIntegration::stageFile(const std::string& repoPath, const std::string& file) {
        runGit(repoPath, {"add", file});
    }

    void GitIntegration::commit(const std::string& repoPath, const std::string& message) {
        runGit(repoPath, {"commit", "-m", message});
    }
}
