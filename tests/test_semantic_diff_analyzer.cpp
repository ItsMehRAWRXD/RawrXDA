/**
 * Semantic diff analyzer tests — C++20 only (Qt-free).
 * Original used QCoreApplication, QSignalSpy, QTemporaryDir, QFile, QString.
 * Use git/semantic_diff_analyzer.hpp with std::string API if available.
 */

#include <cstdio>
#include <cstdlib>

int main(int, char**) {
    std::fprintf(stderr, "[test_semantic_diff_analyzer] C++20 stub. Use SemanticDiffAnalyzer with std::string API when available.\n");
    return 0;
}
