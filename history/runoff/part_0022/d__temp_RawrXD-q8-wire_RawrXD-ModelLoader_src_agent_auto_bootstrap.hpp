#pragma once
#include <string>
#include <vector>

class AutoBootstrap {
public:
    static AutoBootstrap& instance();
    static void installZeroTouch();
    static void startWithWish(const std::string& wish);
    
    void start();
};
