#pragma once
#include <string>

class AutoUpdate {
public:
    explicit AutoUpdate() {}
    bool checkAndInstall();
};
