#pragma once
#include <string>

class Rollback {
public:
    bool detectRegression();
    bool revertLastCommit();
    bool openIssue(const std::string& title, const std::string& body);
};
