#pragma once
#ifndef AUTONOMOUS_LOOP_ENGINE_HPP
#define AUTONOMOUS_LOOP_ENGINE_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include "nlohmann/json.hpp"
#include "model_invoker.hpp"
#include "action_executor.hpp"

namespace RawrXD {

// Loop stage enumeration
enum class LoopStage {
    PLANNING,
    EXECUTION,
    VERIFICATION,
    REFLECTION,
    ADAPTATION,
    COMPLETED,
    FAILED
};

// Loop iteration result
struct LoopIteration {
    int iteration;
    LoopStage stage;
    std::string description;
    ExecutionPlan plan;
    PlanResult executionResult;
    std::string reflection;
    std::string nextStep;
    std::chrono::milliseconds duration;
    bool success;
    
    LoopIteration() : iteration(0), stage(LoopStage::PLANNING), success(false), duration(0) {}
};

// Autonomous Loop Engine - implements plan-execute-verify-reflect cycle
class AutonomousLoopEngine {
public:
    using LoopCallback = std::function<void(const LoopIteration&)>;
    using StageCallback = std::function<void(LoopStage, const std::string&)>;
    
    AutonomousLoopEngine();
    ~AutonomousLoopEngine();
    
    // Configuration
    void setModelInvoker(std::shared_ptr<ModelInvoker> invoker);
    void setActionExecutor(std::shared_ptr<ActionExecutor> executor);
    void setMaxIterations(int maxIterations);
    void setProjectRoot(const std::string& root);
    void setModel(const std::string& model);
    
    // Execution
    std::vector<LoopIteration> runAutonomousLoop(const std::string& objective);
    void runAutonomousLoopAsync(const std::string& objective, LoopCallback callback);
    
    // Control
    void cancelLoop();
    void pauseLoop();
    void resumeLoop();
    
    // History
    std::vector<LoopIteration> getLoopHistory();
    void clearLoopHistory();
    
    // Callbacks
    void setIterationCallback(std::function<void(const LoopIteration&)> callback);
    void setStageCallback(std::function<void(LoopStage, const std::string&)> callback);
    void setCompletionCallback(std::function<void(bool, const std::string&)> callback);
    
private:
    std::shared_ptr<ModelInvoker> m_modelInvoker;
    std::shared_ptr<ActionExecutor> m_actionExecutor;
    int m_maxIterations;
    std::string m_projectRoot;
    std::string m_model;
    bool m_cancelled;
    bool m_paused;
    
    std::vector<LoopIteration> m_loopHistory;
    
    std::function<void(const LoopIteration&)> m_iterationCallback;
    std::function<void(LoopStage, const std::string&)> m_stageCallback;
    std::function<void(bool, const std::string&)> m_completionCallback;
    
    // Internal loop methods
    ExecutionPlan planningStage(const std::string& objective, int iterationNumber);
    PlanResult executionStage(const ExecutionPlan& plan);
    bool verificationStage(const PlanResult& result);
    std::string reflectionStage(const ExecutionPlan& plan, const PlanResult& result, bool verified);
    std::string adaptationStage(const std::string& reflection, int currentIteration);
    
    // Helper methods
    void emitStageChange(LoopStage stage, const std::string& description);
    void emitIteration(const LoopIteration& iteration);
};

} // namespace RawrXD

#endif // AUTONOMOUS_LOOP_ENGINE_HPP
