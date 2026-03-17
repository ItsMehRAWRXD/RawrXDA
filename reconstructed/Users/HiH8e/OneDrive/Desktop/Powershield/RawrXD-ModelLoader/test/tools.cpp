#define NOMINMAX
#include "tools/file_ops.h"
#include "tools/git_client.h"
#include <iostream>
#include <vector>

using namespace RawrXD::Tools;

int main() {
    std::cout << "Phase 2 Tools Test\n";

    // FileOps basic flow
    auto tmpDir = "build/tmp_tools_test";
    auto fileA = std::string(tmpDir) + "/A.txt";
    auto fileB = std::string(tmpDir) + "/B.txt";

    auto r0 = FileOps::ensureDir(tmpDir);
    std::cout << "ensureDir: " << r0.success << " - " << r0.message << "\n";

    auto r1 = FileOps::writeText(fileA, "hello world\n");
    std::cout << "writeText: " << r1.success << " - " << r1.message << "\n";

    auto r2 = FileOps::appendText(fileA, "more text\n");
    std::cout << "appendText: " << r2.success << " - " << r2.message << "\n";

    std::string content;
    auto r3 = FileOps::readText(fileA, content);
    std::cout << "readText: " << r3.success << " - size=" << content.size() << "\n";

    auto r4 = FileOps::copy(fileA, fileB);
    std::cout << "copy: " << r4.success << " - " << r4.message << "\n";

    auto r5 = FileOps::rename(fileB, std::string(tmpDir) + "/C.txt");
    std::cout << "rename: " << r5.success << " - " << r5.message << "\n";

    std::vector<std::string> items;
    auto r6 = FileOps::list(tmpDir, items, false);
    std::cout << "list: " << r6.success << " - count=" << items.size() << "\n";

    auto r7 = FileOps::remove(tmpDir);
    std::cout << "remove: " << r7.success << " - " << r7.message << "\n";

    // GitClient basic checks (works only if git and repo present)
    std::string repoRoot = "."; // current folder
    bool gitAvail = GitClient::isGitAvailable();
    std::cout << "git available: " << (gitAvail ? "yes" : "no") << "\n";

    bool isRepo = GitClient::isRepo(repoRoot);
    std::cout << "is repo: " << (isRepo ? "yes" : "no") << "\n";

    if (gitAvail && isRepo) {
        GitClient git(repoRoot);
        auto s = git.status(true);
        std::cout << "git status exit=" << s.exit_code << "\n";
        auto br = git.currentBranch();
        std::cout << "git branch exit=" << br.exit_code << "\n";
    } else {
        std::cout << "Skipping git tests (git not available or not a repo)\n";
    }

    std::cout << "Phase 2 Tools Test Complete\n";
    return 0;
}
