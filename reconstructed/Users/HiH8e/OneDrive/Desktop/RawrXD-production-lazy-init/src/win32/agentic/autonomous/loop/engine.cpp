#include "autonomous_loop_engine.hpp"
#include <thread>
#include <sstream>

namespace RawrXD {

AutonomousLoopEngine::AutonomousLoopEngine()
    : m_maxIterations(10), m_projectRoot("."), m_model("llama2"),
      m_cancelled(false), m_paused(false) {}

AutonomousLoopEngine::~AutonomousLoopEngine() {}

void AutonomousLoopEngine::setModelInvoker(std::shared_ptr<ModelInvoker> invoker) {
    m_modelInvoker = invoker;
}

void AutonomousLoopEngine::setActionExecutor(std::shared_ptr<ActionExecutor> executor) {
    m_actionExecutor = executor;
}

void AutonomousLoopEngine::setMaxIterations(int maxIterations) {
    m_maxIterations = maxIterations;
}

void AutonomousLoopEngine::setProjectRoot(const std::string& root) {
    m_projectRoot = root;
}

void AutonomousLoopEngine::setModel(const std::string& model) {
    m_model = model;
}

std::vector<LoopIteration> AutonomousLoopEngine::runAutonomousLoop(const std::string& objective) {
    m_loopHistory.clear();
    std::string currentObjective = objective;
    
    for (int i = 0; i < m_maxIterations && !m_cancelled; ++i) {
        // Wait if paused
        while (m_paused && !m_cancelled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        LoopIteration iteration;
        iteration.iteration = i + 1;
        auto iterStart = std::chrono::steady_clock::now();
        
        // Stage 1: Planning
        emitStageChange(LoopStage::PLANNING, "Planning stage for iteration " + std::to_string(i + 1));
        iteration.stage = LoopStage::PLANNING;
        ExecutionPlan plan = planningStage(currentObjective, i + 1);
        iteration.plan = plan;
        
        if (m_cancelled) break;
        
        // Stage 2: Execution
        emitStageChange(LoopStage::EXECUTION, "Executing planned actions");
        iteration.stage = LoopStage::EXECUTION;
        PlanResult executionResult = executionStage(plan);
        iteration.executionResult = executionResult;
        iteration.success = executionResult.success;
        
        if (m_cancelled) break;
        
        // Stage 3: Verification
        emitStageChange(LoopStage::VERIFICATION, "Verifying execution results");
        iteration.stage = LoopStage::VERIFICATION;
        bool verified = verificationStage(executionResult);
        
        if (m_cancelled) break;
        
        // Stage 4: Reflection
        emitStageChange(LoopStage::REFLECTION, "Reflecting on results");
        iteration.stage = LoopStage::REFLECTION;
        std::string reflection = reflectionStage(plan, executionResult, verified);
        iteration.reflection = reflection;
        
        if (m_cancelled) break;
        
        // Stage 5: Adaptation
        emitStageChange(LoopStage::ADAPTATION, "Adapting for next iteration");
        iteration.stage = LoopStage::ADAPTATION;
        std::string nextStep = adaptationStage(reflection, i + 1);
        iteration.nextStep = nextStep;
        
        auto iterEnd = std::chrono::steady_clock::now();
        iteration.duration = std::chrono::duration_cast<std::chrono::milliseconds>(iterEnd - iterStart);
        
        // Check for completion
        if (verified && executionResult.success) {
            iteration.stage = LoopStage::COMPLETED;
            iteration.description = "Task completed successfully";
            emitIteration(iteration);
            m_loopHistory.push_back(iteration);
            
            emitStageChange(LoopStage::COMPLETED, "Autonomous loop completed successfully");
            if (m_completionCallback) m_completionCallback(true, "Objective achieved");
            break;
        }
        
        // Prepare for next iteration
        currentObjective = nextStep;
        emitIteration(iteration);
        m_loopHistory.push_back(iteration);
    }
    
    if (m_cancelled) {
        emitStageChange(LoopStage::FAILED, "Autonomous loop cancelled by user");
        if (m_completionCallback) m_completionCallback(false, "Loop cancelled");
    } else if (m_loopHistory.size() >= static_cast<size_t>(m_maxIterations)) {
        emitStageChange(LoopStage::FAILED, "Autonomous loop reached maximum iterations");
        if (m_completionCallback) m_completionCallback(false, "Max iterations reached");
    }
    
    return m_loopHistory;
}

void AutonomousLoopEngine::runAutonomousLoopAsync(const std::string& objective, LoopCallback callback) {
    std::thread([this, objective, callback]() {
        std::vector<LoopIteration> results = this->runAutonomousLoop(objective);
        if (callback && !results.empty()) {
            callback(results.back());
        }
    }).detach();
}

void AutonomousLoopEngine::cancelLoop() {
    m_cancelled = true;
}

void AutonomousLoopEngine::pauseLoop() {
    m_paused = true;
}

void AutonomousLoopEngine::resumeLoop() {
    m_paused = false;
}

std::vector<LoopIteration> AutonomousLoopEngine::getLoopHistory() {
    return m_loopHistory;
}

void AutonomousLoopEngine::clearLoopHistory() {
    m_loopHistory.clear();
}

void AutonomousLoopEngine::setIterationCallback(std::function<void(const LoopIteration&)> callback) {
    m_iterationCallback = callback;
}

void AutonomousLoopEngine::setStageCallback(std::function<void(LoopStage, const std::string&)> callback) {
    m_stageCallback = callback;
}

void AutonomousLoopEngine::setCompletionCallback(std::function<void(bool, const std::string&)> callback) {
    m_completionCallback = callback;
}

ExecutionPlan AutonomousLoopEngine::planningStage(const std::string& objective, int iterationNumber) {
    if (!m_modelInvoker) {
        ExecutionPlan plan(objective);
        plan.id = "plan_iter_" + std::to_string(iterationNumber);
        return plan;
    }
    
    // Ask model to generate plan
    std::string wish = "Iteration " + std::to_string(iterationNumber) + ": " + objective;
    LLMResponse response = m_modelInvoker->invoke(wish);
    
    if (response.success) {
        return response.plan;
    } else {
        ExecutionPlan fallback(objective);
        fallback.id = "plan_fallback_" + std::to_string(iterationNumber);
        return fallback;
    }
}

PlanResult AutonomousLoopEngine::executionStage(const ExecutionPlan& plan) {
    if (!m_actionExecutor) {
        return PlanResult(plan.id);
    }
    
    m_actionExecutor->setProjectRoot(m_projectRoot);
    return m_actionExecutor->executePlan(plan);
}

bool AutonomousLoopEngine::verificationStage(const PlanResult& result) {
    // Check if all actions were successful
    bool allSuccessful = true;
    for (const auto& actionResult : result.actionResults) {
        if (actionResult.status != ExecutionStatus::COMPLETED) {
            allSuccessful = false;
            break;
        }
    }
    return allSuccessful;
}

std::string AutonomousLoopEngine::reflectionStage(const ExecutionPlan& plan, const PlanResult& result, bool verified) {
    std::ostringstream ss;
    ss << "Reflection on execution:\n";
    ss << "- Planned actions: " << plan.actions.size() << "\n";
    {
        int successCount = 0;
        for (const auto& ar : result.actionResults) {
            if (ar.status == ExecutionStatus::COMPLETED) ++successCount;
        }
        ss << "- Successful actions: " << successCount << "/" << result.actionResults.size() << "\n";
    }
    ss << "- Verification passed: " << (verified ? "yes" : "no") << "\n";
    ss << "- Total duration: " << result.totalDuration.count() << "ms\n";
    
    if (!verified) {
        ss << "- Issues detected: adjustments needed\n";
    }
    
    return ss.str();
}

std::string AutonomousLoopEngine::adaptationStage(const std::string& reflection, int currentIteration) {
    // Simple adaptation: if reflection indicates issues, suggest refinement
    if (reflection.find("adjustments needed") != std::string::npos) {
        return "Refine approach and retry failed actions";
    }
    return "Continue with next phase";
}

void AutonomousLoopEngine::emitStageChange(LoopStage stage, const std::string& description) {
    if (m_stageCallback) {
        m_stageCallback(stage, description);
    }
}

void AutonomousLoopEngine::emitIteration(const LoopIteration& iteration) {
    if (m_iterationCallback) {
        m_iterationCallback(iteration);
    }
}

} // namespace RawrXD
