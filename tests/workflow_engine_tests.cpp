// ============================================================================
// Workflow Engine Tests — Process Automation Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include "../src/workflow/workflow_engine.cpp"

using namespace RawrXD::Workflow;

// Mock Session Manager
class MockWorkflowSessionManager : public Core::SessionManager {
public:
    void SetValue(const std::string& key, const std::string& value) override {
        m_values[key] = value;
    }
    
    std::string GetValue(const std::string& key) override {
        auto it = m_values.find(key);
        return it != m_values.end() ? it->second : "";
    }
    
private:
    std::map<std::string, std::string> m_values;
};

TEST_CASE("Workflow Engine - Basic Operations", "[workflow][automation]") {
    auto sessionManager = std::make_shared<MockWorkflowSessionManager>();
    WorkflowEngine engine(sessionManager);
    
    SECTION("Empty workflow state") {
        // Initially no workflows registered
        SUCCEED(); // Engine created successfully
    }
    
    SECTION("Register workflow") {
        WorkflowDefinition definition;
        definition.id = "test-workflow";
        definition.name = "Test Workflow";
        definition.description = "A test workflow";
        
        engine.RegisterWorkflow(definition);
        
        // Workflow registered (no direct verification in API)
        SUCCEED();
    }
}

TEST_CASE("Workflow Engine - Workflow Execution", "[workflow][execution]") {
    auto sessionManager = std::make_shared<MockWorkflowSessionManager>();
    WorkflowEngine engine(sessionManager);
    
    SECTION("Start workflow") {
        WorkflowDefinition definition;
        definition.id = "simple-workflow";
        definition.name = "Simple Workflow";
        
        WorkflowTask task;
        task.id = "task-1";
        task.name = "Task 1";
        definition.tasks.push_back(task);
        
        engine.RegisterWorkflow(definition);
        
        std::map<std::string, std::string> context;
        auto instance = engine.StartWorkflow("simple-workflow", context);
        
        REQUIRE(instance.definitionId == "simple-workflow");
        REQUIRE(instance.status == WorkflowStatus::RUNNING);
    }
    
    SECTION("Workflow lifecycle") {
        WorkflowDefinition definition;
        definition.id = "lifecycle-workflow";
        
        engine.RegisterWorkflow(definition);
        
        std::map<std::string, std::string> context;
        auto instance = engine.StartWorkflow("lifecycle-workflow", context);
        
        REQUIRE(instance.status == WorkflowStatus::RUNNING);
        
        // Pause workflow
        engine.PauseWorkflow(instance.id);
        REQUIRE(engine.GetWorkflowStatus(instance.id) == WorkflowStatus::PAUSED);
        
        // Resume workflow
        engine.ResumeWorkflow(instance.id);
        REQUIRE(engine.GetWorkflowStatus(instance.id) == WorkflowStatus::RUNNING);
        
        // Cancel workflow
        engine.CancelWorkflow(instance.id);
        REQUIRE(engine.GetWorkflowStatus(instance.id) == WorkflowStatus::CANCELLED);
    }
}

TEST_CASE("Workflow Engine - Metrics", "[workflow][metrics]") {
    auto sessionManager = std::make_shared<MockWorkflowSessionManager>();
    WorkflowEngine engine(sessionManager);
    
    SECTION("Workflow metrics") {
        WorkflowDefinition definition;
        definition.id = "metrics-workflow";
        definition.name = "Metrics Workflow";
        
        engine.RegisterWorkflow(definition);
        
        auto metrics = engine.GetMetrics("metrics-workflow");
        
        REQUIRE(metrics.totalExecutions == 0);
        REQUIRE(metrics.successfulExecutions == 0);
        REQUIRE(metrics.failedExecutions == 0);
        REQUIRE(metrics.averageExecutionTime == 0.0);
    }
    
    SECTION("Report generation") {
        WorkflowDefinition definition;
        definition.id = "report-workflow";
        definition.name = "Report Workflow";
        definition.description = "Workflow for testing reports";
        
        engine.RegisterWorkflow(definition);
        
        auto report = engine.GenerateWorkflowReport("report-workflow");
        
        REQUIRE_FALSE(report.empty());
        REQUIRE(report.find("# Workflow Report") != std::string::npos);
        REQUIRE(report.find("Report Workflow") != std::string::npos);
    }
}
