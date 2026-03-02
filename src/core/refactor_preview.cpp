#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace RawrXD::Core {

struct Hunk {
    int startLine = 0;
    int endLine = 0;
    std::string before;
    std::string after;
    bool selected = true;
};

struct RefactorPreview {
    std::string file;
    std::vector<Hunk> hunks;
};

class RefactorPreviewEngine {
public:
    RefactorPreview buildReplacePreview(const std::string& file, const std::string& oldText, const std::string& newText) {
        RefactorPreview p;
        p.file = file;

        std::ifstream in(file);
        if (!in.is_open()) return p;

        std::string line;
        int lineNo = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            auto pos = line.find(oldText);
            if (pos != std::string::npos) {
                Hunk h;
                h.startLine = lineNo;
                h.endLine = lineNo;
                h.before = line;
                std::string replaced = line;
                while ((pos = replaced.find(oldText)) != std::string::npos) {
                    replaced.replace(pos, oldText.size(), newText);
                }
                h.after = replaced;
                p.hunks.push_back(h);
            }
        }
        return p;
    }

    bool applySelected(const RefactorPreview& p) {
        std::ifstream in(p.file);
        if (!in.is_open()) return false;

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
        in.close();

        std::vector<std::string> backup = lines;

        for (const auto& h : p.hunks) {
            if (!h.selected) continue;
            if (h.startLine <= 0 || h.startLine > static_cast<int>(lines.size())) continue;
            lines[static_cast<size_t>(h.startLine - 1)] = h.after;
        }

        std::ofstream out(p.file, std::ios::trunc);
        if (!out.is_open()) {
            restoreBackup(p.file, backup);
            return false;
        }
        for (size_t i = 0; i < lines.size(); ++i) {
            out << lines[i];
            if (i + 1 < lines.size()) out << "\n";
        }
        return true;
    }

private:
    static void restoreBackup(const std::string& file, const std::vector<std::string>& backup) {
        std::ofstream out(file, std::ios::trunc);
        if (!out.is_open()) return;
        for (size_t i = 0; i < backup.size(); ++i) {
            out << backup[i];
            if (i + 1 < backup.size()) out << "\n";
        }
    }
};

static RefactorPreviewEngine g_refPreview;
static RefactorPreview g_lastPreview;

} // namespace RawrXD::Core

extern "C" {

int RawrXD_Core_BuildRefactorPreview(const char* filePath, const char* oldText, const char* newText) {
    if (!filePath || !oldText || !newText) return 0;
    RawrXD::Core::g_lastPreview = RawrXD::Core::g_refPreview.buildReplacePreview(filePath, oldText, newText);
    return static_cast<int>(RawrXD::Core::g_lastPreview.hunks.size());
}

bool RawrXD_Core_ApplyRefactorPreview() {
    if (RawrXD::Core::g_lastPreview.file.empty()) return false;
    return RawrXD::Core::g_refPreview.applySelected(RawrXD::Core::g_lastPreview);
}

}
