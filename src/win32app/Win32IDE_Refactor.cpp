#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace RawrXD::IDE {

struct TextEdit {
    std::string file;
    int startLine = 0;
    int startCol = 0;
    int endLine = 0;
    int endCol = 0;
    std::string newText;
};

class RefactorEngine {
public:
    // Lightweight rename across workspace text files (LSP-integrated fallback)
    int renameSymbol(const std::string& workspaceRoot, const std::string& oldName, const std::string& newName) {
        int changedFiles = 0;
        for (const auto& entry : fs::recursive_directory_iterator(workspaceRoot)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".h" && ext != ".hpp" && ext != ".c" && ext != ".py" && ext != ".ts" && ext != ".js") continue;

            std::ifstream in(entry.path());
            if (!in.is_open()) continue;
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();

            auto pos = content.find(oldName);
            if (pos == std::string::npos) continue;

            size_t start = 0;
            while ((start = content.find(oldName, start)) != std::string::npos) {
                content.replace(start, oldName.size(), newName);
                start += newName.size();
            }

            std::ofstream out(entry.path(), std::ios::trunc);
            if (!out.is_open()) continue;
            out << content;
            out.close();
            changedFiles++;
        }
        return changedFiles;
    }

    std::string extractFunctionStub(const std::string& functionName) {
        return "void " + functionName + "() {\n    // extracted by Win32IDE RefactorEngine\n}\n";
    }
};

static RefactorEngine g_refactor;

} // namespace RawrXD::IDE

extern "C" {

int RawrXD_IDE_RenameSymbol(const char* workspaceRoot, const char* oldName, const char* newName) {
    if (!workspaceRoot || !oldName || !newName) return 0;
    return RawrXD::IDE::g_refactor.renameSymbol(workspaceRoot, oldName, newName);
}

}
