// ============================================================================
// agentic_flow.h — Autonomous Multi-Step Task Execution Engine
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
// Reverse-engineered Agentic Flows inspired by Windsurf/Codeium
// Enables AI to take autonomous multi-step actions with minimal interruption
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <optional>
#include <variant>

namespace RawrXD::Agentic {

// ============================================================================
// Core Types
// ============================================================================

enum class FlowStatus {
    Pending,
    Planning,
    Executing,
    WaitingForInput,
    Paused,
    Completed,
    Failed,
    Cancelled
};

enum class StepStatus {
    Pending,
    Running,
    Waiting,
    Completed,
    Failed,
    Skipped,
    Retrying
};

enum class StepType {
    Analyze,        // Analyze code/context
    Plan,           // Create execution plan
    Edit,           // Modify files
    Search,         // Search codebase
    Build,          // Build/compile
    Test,           // Run tests
    Deploy,         // Deploy changes
    Review,         // Code review
    Refactor,       // Refactoring operation
    Generate,       // Generate code
    Debug,          // Debug/fix issues
    Query,          // Query user/system
    Wait,           // Wait for condition
    Branch,         // Conditional branching
    Loop,           // Loop iteration
    Parallel,       // Parallel execution
    Custom          // Custom action
};

enum class FlowMode {
    Interactive,    // Pause for user input when needed
    Autonomous,     // Make best-effort decisions autonomously
    Aggressive,     // Maximum autonomy, minimal confirmation
    Supervised      // Require approval for each step
};

// ============================================================================
// Flow Context
// ============================================================================

struct FlowContext {
    std::string workingDirectory;
    std::vector<std::string> targetFiles;
    std::unordered_map<std::string, std::string> variables;
    std::unordered_map<std::string, std::string> fileContents;
    std::unordered_map<std::string, std::string> metadata;
    
    // Execution state
    uint32_t currentStepIndex = 0;
    uint32_t iterationCount = 0;
    uint32_t retryCount = 0;
    
    // Results from previous steps
    std::unordered_map<std::string, std::string> stepResults;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    // User input queue
    std::vector<std::string> pendingInputs;
    std::optional<std::string> lastUserResponse;
};

// ============================================================================
// Step Definition
// ============================================================================

struct StepCondition {
    std::string variable;
    std::string operator_;
    std::string value;
    
    bool evaluate(const FlowContext& ctx) const;
};

struct StepAction {
    StepType type = StepType::Custom;
    std::string name;
    std::string description;
    std::string command;
    std::vector<std::string> args;
    std::unordered_map<std::string, std::string> params;
    
    // Input/output mapping
    std::string inputVar;
    std::string outputVar;
    
    // For conditional steps
    std::vector<StepCondition> conditions;
    std::string trueBranch;
    std::string falseBranch;
    
    // For loop steps
    std::string iteratorVar;
    std::string collectionVar;
    uint32_t maxIterations = 100;
    
    // Retry configuration
    uint32_t maxRetries = 3;
    uint32_t retryDelayMs = 1000;
    bool retryOnFailure = true;
    
    // Timeout
    uint32_t timeoutMs = 30000;
    
    // Dependencies
    std::vector<std::string> dependsOn;
    
    // Confirmation
    bool requiresConfirmation = false;
    std::string confirmationMessage;
};

struct FlowStep {
    std::string id;
    std::string name;
    std::string description;
    StepStatus status = StepStatus::Pending;
    StepAction action;
    
    // Execution tracking
    std::chrono::system_clock::time_point started;
    std::chrono::system_clock::time_point completed;
    uint32_t attemptCount = 0;
    
    // Results
    std::optional<std::string> result;
    std::optional<std::string> error;
    std::vector<std::string> output;
    
    // Child steps (for parallel/branch/loop)
    std::vector<std::string> childSteps;
    std::string parentStep;
};

// ============================================================================
// Flow Definition
// ============================================================================

struct FlowDefinition {
    std::string id;
    std::string name;
    std::string description;
    std::string version;
    
    // Trigger conditions
    std::vector<std::string> triggers;
    std::vector<StepCondition> preconditions;
    
    // Steps
    std::vector<FlowStep> steps;
    std::unordered_map<std::string, FlowStep> stepMap;
    
    // Entry point
    std::string entryStep;
    
    // Configuration
    FlowMode mode = FlowMode::Interactive;
    uint32_t maxSteps = 1000;
    uint32_t maxDurationMs = 300000; // 5 minutes
    bool enableRollback = true;
    bool enableCheckpointing = true;
    uint32_t checkpointInterval = 10;
    
    // Error handling
    std::string onFailure; // "abort", "retry", "continue", "ask"
    uint32_t maxGlobalRetries = 3;
    
    // Metadata
    std::unordered_map<std::string, std::string> metadata;
};

// ============================================================================
// Flow Execution State
// ============================================================================

struct FlowCheckpoint {
    uint32_t checkpointId = 0;
    std::chrono::system_clock::time_point created;
    std::string stepId;
    FlowContext context;
    std::unordered_map<std::string, std::string> fileSnapshots;
};

struct FlowExecution {
    std::string executionId;
    std::string flowId;
    FlowStatus status = FlowStatus::Pending;
    FlowMode mode = FlowMode::Interactive;
    
    // Context
    FlowContext context;
    
    // Execution tracking
    uint32_t currentStepIndex = 0;
    std::string currentStepId;
    std::vector<std::string> completedSteps;
    std::vector<std::string> failedSteps;
    std::vector<std::string> skippedSteps;
    
    // Timing
    std::chrono::system_clock::time_point started;
    std::chrono::system_clock::time_point completed;
    std::chrono::milliseconds duration{0};
    
    // Checkpoints
    std::vector<FlowCheckpoint> checkpoints;
    uint32_t currentCheckpoint = 0;
    
    // Results
    std::vector<std::string> outputs;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    // Metrics
    uint32_t totalSteps = 0;
    uint32_t completedStepCount = 0;
    uint32_t failedStepCount = 0;
    uint32_t retryCount = 0;
    float progress = 0.0f;
};

// ============================================================================
// Flow Result
// ============================================================================

struct FlowResult {
    std::string executionId;
    FlowStatus status;
    
    bool success = false;
    std::string summary;
    std::string detailedReport;
    
    std::vector<std::string> outputs;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    std::unordered_map<std::string, std::string> variables;
    std::unordered_map<std::string, std::string> modifiedFiles;
    
    std::chrono::milliseconds duration{0};
    uint32_t stepsExecuted = 0;
    uint32_t stepsFailed = 0;
};

// ============================================================================
// Event System
// ============================================================================

enum class FlowEvent {
    FlowStarted,
    FlowCompleted,
    FlowFailed,
    FlowCancelled,
    FlowPaused,
    FlowResumed,
    StepStarted,
    StepCompleted,
    StepFailed,
    StepRetrying,
    CheckpointCreated,
    CheckpointRestored,
    UserInputRequested,
    UserInputReceived,
    ErrorOccurred,
    WarningRaised
};

struct FlowEventArgs {
    FlowEvent event;
    std::string executionId;
    std::string stepId;
    std::string message;
    std::optional<std::string> error;
    float progress = 0.0f;
    std::optional<FlowStep> step;
    std::optional<FlowContext> context;
};

using FlowCallback = std::function<void(const FlowEventArgs&)>;

// ============================================================================
// Built-in Flow Templates
// ============================================================================

namespace Flows {

// Feature implementation flow
FlowDefinition createFeatureFlow(const std::string& featureName);

// Bug fix flow
FlowDefinition createBugFixFlow(const std::string& bugDescription);

// Refactoring flow
FlowDefinition createRefactorFlow(const std::string& target, 
                                   const std::string& operation);

// Code review flow
FlowDefinition createReviewFlow(const std::vector<std::string>& files);

// Test generation flow
FlowDefinition createTestFlow(const std::string& targetFile);

// Documentation flow
FlowDefinition createDocsFlow(const std::vector<std::string>& files);

// Migration flow
FlowDefinition createMigrationFlow(const std::string& from, 
                                   const std::string& to);

// Optimization flow
FlowDefinition createOptimizeFlow(const std::string& target);

// Debug flow
FlowDefinition createDebugFlow(const std::string& issue);

// Full project analysis flow
FlowDefinition createAnalysisFlow();

} // namespace Flows

// ============================================================================
// Flow Engine Interface
// ============================================================================

class IFlowEngine {
public:
    virtual ~IFlowEngine() = default;
    
    // Configuration
    virtual void setMode(FlowMode mode) = 0;
    virtual FlowMode getMode() const = 0;
    virtual void setCallback(FlowCallback callback) = 0;
    
    // Flow management
    virtual bool registerFlow(const FlowDefinition& flow) = 0;
    virtual bool unregisterFlow(const std::string& flowId) = 0;
    virtual std::vector<std::string> getAvailableFlows() const = 0;
    virtual std::optional<FlowDefinition> getFlow(const std::string& flowId) const = 0;
    
    // Execution
    virtual std::string startFlow(const std::string& flowId,
                                   const FlowContext& initialContext = {}) = 0;
    virtual bool pauseFlow(const std::string& executionId) = 0;
    virtual bool resumeFlow(const std::string& executionId) = 0;
    virtual bool cancelFlow(const std::string& executionId) = 0;
    virtual bool retryStep(const std::string& executionId, 
                          const std::string& stepId) = 0;
    
    // User interaction
    virtual bool provideInput(const std::string& executionId,
                              const std::string& input) = 0;
    virtual std::optional<std::string> waitForInput(const std::string& executionId,
                                                     uint32_t timeoutMs = 60000) = 0;
    
    // Status
    virtual FlowStatus getStatus(const std::string& executionId) const = 0;
    virtual float getProgress(const std::string& executionId) const = 0;
    virtual std::optional<FlowExecution> getExecution(const std::string& executionId) const = 0;
    virtual std::optional<FlowResult> getResult(const std::string& executionId) const = 0;
    
    // Checkpoints
    virtual uint32_t createCheckpoint(const std::string& executionId) = 0;
    virtual bool restoreCheckpoint(const std::string& executionId, 
                                    uint32_t checkpointId) = 0;
    
    // Step execution (for custom steps)
    virtual bool executeStep(const std::string& executionId,
                            const std::string& stepId) = 0;
    virtual bool skipStep(const std::string& executionId,
                         const std::string& stepId) = 0;
};

class FlowEngine final : public IFlowEngine {
public:
    FlowEngine();
    ~FlowEngine() override;

    void setMode(FlowMode mode) override;
    FlowMode getMode() const override;
    void setCallback(FlowCallback callback) override;

    bool registerFlow(const FlowDefinition& flow) override;
    bool unregisterFlow(const std::string& flowId) override;
    std::vector<std::string> getAvailableFlows() const override;
    std::optional<FlowDefinition> getFlow(const std::string& flowId) const override;

    std::string startFlow(const std::string& flowId,
                          const FlowContext& initialContext = {}) override;
    bool pauseFlow(const std::string& executionId) override;
    bool resumeFlow(const std::string& executionId) override;
    bool cancelFlow(const std::string& executionId) override;
    bool retryStep(const std::string& executionId,
                   const std::string& stepId) override;

    bool provideInput(const std::string& executionId,
                      const std::string& input) override;
    std::optional<std::string> waitForInput(const std::string& executionId,
                                            uint32_t timeoutMs = 60000) override;

    FlowStatus getStatus(const std::string& executionId) const override;
    float getProgress(const std::string& executionId) const override;
    std::optional<FlowExecution> getExecution(const std::string& executionId) const override;
    std::optional<FlowResult> getResult(const std::string& executionId) const override;

    uint32_t createCheckpoint(const std::string& executionId) override;
    bool restoreCheckpoint(const std::string& executionId,
                           uint32_t checkpointId) override;

    bool executeStep(const std::string& executionId,
                     const std::string& stepId) override;
    bool skipStep(const std::string& executionId,
                  const std::string& stepId) override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<IFlowEngine> createFlowEngine();

} // namespace RawrXD::Agentic
