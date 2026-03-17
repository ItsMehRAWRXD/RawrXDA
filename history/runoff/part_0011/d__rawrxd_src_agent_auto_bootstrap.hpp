#pragma once
#include <string>
#include <vector>

class AutoBootstrap {
public:
    static AutoBootstrap* instance();
    static void installZeroTouch();
    static void startWithWish(const std::string& wish);
    
    // Start autonomy loop with zero-touch input
    void start();
    
    void wishReceived(const std::string& wish);
    void planGenerated(const std::string& planSummary);
    void executionStarted();
    void executionCompleted(bool success);
    
private:
    explicit AutoBootstrap();
    
    // Grab wish from env-var > clipboard > dialog
    std::string grabWish();
    
    // Safety gate to prevent dangerous commands
    bool safetyGate(const std::string& wish);
    
    void startWithWishInternal(const std::string& wish);
    void executePlan(const std::string& wish, const std::vector<std::string>& plan);
    
    static AutoBootstrap* s_instance;
};



