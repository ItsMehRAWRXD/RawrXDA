#include "code_folding.h"

namespace RawrXD::Editor {
    std::vector<FoldRegion> CodeFolding::detectFolds(const std::vector<std::string>& lines) {
        std::vector<FoldRegion> folds;
        std::vector<int> stack;

        for (size_t i = 0; i < lines.size(); ++i) {
            const std::string& line = lines[i];
            bool inString = false;
            bool inComment = false;

            for (size_t j = 0; j < line.size(); ++j) {
                if (inComment) {
                    if (j + 1 < line.size() && line[j] == '*' && line[j+1] == '/') {
                        inComment = false;
                        ++j;
                    }
                    continue;
                }
                if (inString) {
                    if (line[j] == '\\') { ++j; continue; }
                    if (line[j] == '"') inString = false;
                    continue;
                }
                if (j + 1 < line.size() && line[j] == '/' && line[j+1] == '/') break;
                if (j + 1 < line.size() && line[j] == '/' && line[j+1] == '*') { inComment = true; ++j; continue; }
                if (line[j] == '"') { inString = true; continue; }

                if (line[j] == '{') {
                    stack.push_back(static_cast<int>(i));
                } else if (line[j] == '}' && !stack.empty()) {
                    int start = stack.back();
                    stack.pop_back();
                    if (static_cast<size_t>(start) != i) {
                        folds.push_back({static_cast<int>(start), static_cast<int>(i), false});
                    }
                }
            }
        }
        return folds;
    }

    void CodeFolding::toggleFold(std::vector<FoldRegion>& folds, int line) {
        for (auto& fold : folds) {
            if (fold.startLine == line) {
                fold.collapsed = !fold.collapsed;
                return;
            }
        }
    }

    bool CodeFolding::isLineVisible(const std::vector<FoldRegion>& folds, int line) {
        for (const auto& fold : folds) {
            if (fold.collapsed && line > fold.startLine && line < fold.endLine) {
                return false;
            }
        }
        return true;
    }
}
