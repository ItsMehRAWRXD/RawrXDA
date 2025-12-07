#pragma once
#include <QString>

class Rollback {
public:
    // detect if last commit worsens perf
    bool detectRegression();
    // git revert HEAD + rebuild
    bool revertLastCommit();
    // open GitHub issue
    bool openIssue(const QString& title, const QString& body);
};
