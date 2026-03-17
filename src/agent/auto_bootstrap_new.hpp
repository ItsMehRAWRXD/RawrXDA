#pragma once
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class AutoBootstrap {
public:
    static AutoBootstrap* instance();
    static void installZeroTouch();
    static void startWithWish(const std::string& wish);
    
    // Start autonomy loop with zero-touch input
    void start();
    
    // Callbacks replacing signals
    std::function<void(const std::string&)> onWishReceived;
    std::function<void(const std::string&)> onPlanGenerated;
    std::function<void()> onExecutionStarted;
    std::function<void(bool)> onExecutionCompleted;
    
private:
    AutoBootstrap();
    
    // Grab wish from env-var > clipboard > dialog
    std::string grabWish();
    
    // Safety gate to prevent dangerous commands
    bool safetyGate(const std::string& wish);
    
    void startWithWishInternal(const std::string& wish);
    void executePlan(const std::string& wish, const json& plan);
    
    static AutoBootstrap* s_instance;
};
