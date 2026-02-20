#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <functional>

class AutoBootstrap {
public:
    static AutoBootstrap* instance();
    static void installZeroTouch();
    static void startWithWish(const std::string& wish);
    void start();

    // Callbacks (replace Qt signals)
    std::function<void(const std::string&)> onWishReceived;
    std::function<void(const std::string&)> onPlanGenerated;
    std::function<void()> onExecutionStarted;
    std::function<void(bool)> onExecutionCompleted;

private:
    AutoBootstrap() = default;
    std::string grabWish();
    bool safetyGate(const std::string& wish);
    void startWithWishInternal(const std::string& wish);
    void executePlan(const std::string& wish, const nlohmann::json& plan);
    static AutoBootstrap* s_instance;
};
