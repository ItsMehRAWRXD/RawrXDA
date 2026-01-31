#pragma once

class Rollback {
public:
    // detect if last commit worsens perf
    bool detectRegression();
    // git revert HEAD + rebuild
    bool revertLastCommit();
    // open GitHub issue
    bool openIssue(const std::string& title, const std::string& body);
};

