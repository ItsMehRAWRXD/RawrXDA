#include "diff_engine.h"
#include <algorithm>

namespace RawrXD::Editor {
    std::vector<DiffEdit> DiffEngine::computeDiff(const std::vector<std::string>& oldLines, const std::vector<std::string>& newLines) {
        size_t n = oldLines.size();
        size_t m = newLines.size();

        // Myers diff on lines
        std::vector<std::vector<size_t>> dp(n + 1, std::vector<size_t>(m + 1, 0));

        for (size_t i = 1; i <= n; ++i) {
            for (size_t j = 1; j <= m; ++j) {
                if (oldLines[i-1] == newLines[j-1]) {
                    dp[i][j] = dp[i-1][j-1] + 1;
                } else {
                    dp[i][j] = std::max(dp[i-1][j], dp[i][j-1]);
                }
            }
        }

        std::vector<DiffEdit> edits;
        size_t i = n, j = m;
        while (i > 0 || j > 0) {
            if (i > 0 && j > 0 && oldLines[i-1] == newLines[j-1]) {
                edits.push_back({DiffType::UNCHANGED, oldLines[i-1], static_cast<int>(i-1)});
                --i; --j;
            } else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j])) {
                edits.push_back({DiffType::INSERTED, newLines[j-1], static_cast<int>(j-1)});
                --j;
            } else {
                edits.push_back({DiffType::DELETED, oldLines[i-1], static_cast<int>(i-1)});
                --i;
            }
        }
        std::reverse(edits.begin(), edits.end());
        return edits;
    }
}
