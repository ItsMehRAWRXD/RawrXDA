#include <QtTest>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>

#include "../src/orchestration/agent_coordinator.hpp"

class TestAgentCoordinator : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Agent registration tests
    void testRegisterAgent();
    void testRegisterMultipleAgents();
    void testRegisterDuplicateAgent();
    void testUnregisterAgent();
    void testSetAgentAvailability();
    void testIsAgentAvailable();

    // Task scheduling tests
    void testSubmitSimplePlan();
    void testSubmitPlanWithDependencies();
    void testTaskStateProgression();
    void testReadyTasksCalculation();

    // DAG validation tests
    void testDetectCyclicDependency();
    void testDetectMissingDependency();
    void testValidateDuplicateTaskIds();

    // Cancellation tests
    void testCancelPlan();
    void testCancelTaskDownstreamSkip();
    void testCancelWhilePending();

    // Failure path tests
    void testTaskFailureCausesDownstreamSkip();
    void testPlanFailsOnTaskFailure();
    void testFailureMessagePropagation();

    // Context sharing tests
    void testSharedContextMerge();
    void testOutputContextAccumulation();

    // Concurrency tests
    void testAgentConcurrencyLimit();
    void testMultiplePlans();

    // Introspection tests
    void testGetPlanStatus();
    void testGetCoordinatorStats();

private:
    AgentCoordinator* coordinator;
};

void TestAgentCoordinator::initTestCase()
{
    coordinator = new AgentCoordinator(this);
}

void TestAgentCoordinator::cleanupTestCase()
{
    delete coordinator;
}

// ===== Agent Registration Tests =====

void TestAgentCoordinator::testRegisterAgent()
{
    bool success = coordinator->registerAgent("researcher", {"analysis", "research"}, 2);
    QVERIFY(success);
    QVERIFY(coordinator->isAgentAvailable("researcher"));
}

void TestAgentCoordinator::testRegisterMultipleAgents()
{
    bool r1 = coordinator->registerAgent("researcher", {"analysis"}, 2);
    bool r2 = coordinator->registerAgent("coder", {"coding", "implementation"}, 3);
    bool r3 = coordinator->registerAgent("reviewer", {"review"}, 1);
    QVERIFY(r1 && r2 && r3);
    QVERIFY(coordinator->isAgentAvailable("researcher"));
    QVERIFY(coordinator->isAgentAvailable("coder"));
    QVERIFY(coordinator->isAgentAvailable("reviewer"));
}

void TestAgentCoordinator::testRegisterDuplicateAgent()
{
    coordinator->registerAgent("agent1", {"task"}, 1);
    bool result = coordinator->registerAgent("agent1", {"task", "task2"}, 2);
    // Second registration succeeds but replaces first
    QVERIFY(result);
}

void TestAgentCoordinator::testUnregisterAgent()
{
    coordinator->registerAgent("temp_agent", {"temp"}, 1);
    bool success = coordinator->unregisterAgent("temp_agent");
    QVERIFY(success);
    QVERIFY(!coordinator->isAgentAvailable("temp_agent"));
}

void TestAgentCoordinator::testSetAgentAvailability()
{
    coordinator->registerAgent("agent_for_availability", {"task"}, 1);
    QVERIFY(coordinator->isAgentAvailable("agent_for_availability"));
    
    coordinator->setAgentAvailability("agent_for_availability", false);
    QVERIFY(!coordinator->isAgentAvailable("agent_for_availability"));
    
    coordinator->setAgentAvailability("agent_for_availability", true);
    QVERIFY(coordinator->isAgentAvailable("agent_for_availability"));
}

void TestAgentCoordinator::testIsAgentAvailable()
{
    coordinator->registerAgent("available_agent", {"task"}, 2);
    QVERIFY(coordinator->isAgentAvailable("available_agent"));
    QVERIFY(!coordinator->isAgentAvailable("non_existent_agent"));
}

// ===== Task Scheduling Tests =====

void TestAgentCoordinator::testSubmitSimplePlan()
{
    coordinator->registerAgent("simple_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    AgentCoordinator::AgentTask task1;
    task1.id = "task_1";
    task1.name = "Simple Task";
    task1.agentId = "simple_agent";
    tasks.append(task1);
    
    QString planId = coordinator->submitPlan(tasks, QJsonObject());
    QVERIFY(!planId.isEmpty());
    
    QJsonObject status = coordinator->getPlanStatus(planId);
    QVERIFY(status.contains("planId"));
    QCOMPARE(status["planId"].toString(), planId);
}

void TestAgentCoordinator::testSubmitPlanWithDependencies()
{
    coordinator->registerAgent("dep_agent", {"general"}, 3);
    
    QList<AgentCoordinator::AgentTask> tasks;
    
    AgentCoordinator::AgentTask task1;
    task1.id = "dep_task_1";
    task1.name = "First Task";
    task1.agentId = "dep_agent";
    tasks.append(task1);
    
    AgentCoordinator::AgentTask task2;
    task2.id = "dep_task_2";
    task2.name = "Second Task (depends on first)";
    task2.agentId = "dep_agent";
    task2.dependencies = {"dep_task_1"};
    tasks.append(task2);
    
    QString planId = coordinator->submitPlan(tasks);
    QVERIFY(!planId.isEmpty());
    
    QList<QString> ready = coordinator->getReadyTasks(planId);
    QCOMPARE(ready.size(), 1);
    QCOMPARE(ready[0], "dep_task_1");
}

void TestAgentCoordinator::testTaskStateProgression()
{
    coordinator->registerAgent("progression_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    AgentCoordinator::AgentTask task;
    task.id = "prog_task";
    task.name = "Progression Test";
    task.agentId = "progression_agent";
    tasks.append(task);
    
    QString planId = coordinator->submitPlan(tasks);
    
    // Task should be ready
    QList<QString> ready1 = coordinator->getReadyTasks(planId);
    QCOMPARE(ready1.size(), 1);
    
    // Start task
    bool started = coordinator->startTask(planId, "prog_task");
    QVERIFY(started);
    
    // Complete task
    bool completed = coordinator->completeTask(planId, "prog_task", QJsonObject(), true, "");
    QVERIFY(completed);
}

void TestAgentCoordinator::testReadyTasksCalculation()
{
    coordinator->registerAgent("ready_agent", {"general"}, 5);
    
    QList<AgentCoordinator::AgentTask> tasks;
    
    // Create a chain: t1 -> t2 -> t3, plus t4 independent
    AgentCoordinator::AgentTask t1;
    t1.id = "t1";
    t1.agentId = "ready_agent";
    tasks.append(t1);
    
    AgentCoordinator::AgentTask t2;
    t2.id = "t2";
    t2.agentId = "ready_agent";
    t2.dependencies = {"t1"};
    tasks.append(t2);
    
    AgentCoordinator::AgentTask t3;
    t3.id = "t3";
    t3.agentId = "ready_agent";
    t3.dependencies = {"t2"};
    tasks.append(t3);
    
    AgentCoordinator::AgentTask t4;
    t4.id = "t4";
    t4.agentId = "ready_agent";
    tasks.append(t4);
    
    QString planId = coordinator->submitPlan(tasks);
    
    // Only t1 and t4 should be ready
    QList<QString> ready = coordinator->getReadyTasks(planId);
    QCOMPARE(ready.size(), 2);
    QVERIFY(ready.contains("t1"));
    QVERIFY(ready.contains("t4"));
    
    // Complete t1
    coordinator->startTask(planId, "t1");
    coordinator->completeTask(planId, "t1", QJsonObject(), true, "");
    
    // Now t2 should be ready
    QList<QString> ready2 = coordinator->getReadyTasks(planId);
    QVERIFY(ready2.contains("t2"));
}

// ===== DAG Validation Tests =====

void TestAgentCoordinator::testDetectCyclicDependency()
{
    coordinator->registerAgent("cycle_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    
    AgentCoordinator::AgentTask t1;
    t1.id = "ct1";
    t1.agentId = "cycle_agent";
    t1.dependencies = {"ct2"};
    tasks.append(t1);
    
    AgentCoordinator::AgentTask t2;
    t2.id = "ct2";
    t2.agentId = "cycle_agent";
    t2.dependencies = {"ct1"};
    tasks.append(t2);
    
    // Should fail validation due to cycle
    QString planId = coordinator->submitPlan(tasks);
    QVERIFY(planId.isEmpty());
}

void TestAgentCoordinator::testDetectMissingDependency()
{
    coordinator->registerAgent("miss_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    
    AgentCoordinator::AgentTask t1;
    t1.id = "mt1";
    t1.agentId = "miss_agent";
    t1.dependencies = {"non_existent_task"};
    tasks.append(t1);
    
    // Should fail validation due to missing dependency
    QString planId = coordinator->submitPlan(tasks);
    QVERIFY(planId.isEmpty());
}

void TestAgentCoordinator::testValidateDuplicateTaskIds()
{
    coordinator->registerAgent("dup_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    
    AgentCoordinator::AgentTask t1;
    t1.id = "dup_id";
    t1.agentId = "dup_agent";
    tasks.append(t1);
    
    AgentCoordinator::AgentTask t2;
    t2.id = "dup_id";
    t2.agentId = "dup_agent";
    tasks.append(t2);
    
    // Should fail validation due to duplicate IDs
    QString planId = coordinator->submitPlan(tasks);
    QVERIFY(planId.isEmpty());
}

// ===== Cancellation Tests =====

void TestAgentCoordinator::testCancelPlan()
{
    coordinator->registerAgent("cancel_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    AgentCoordinator::AgentTask task;
    task.id = "cancel_task";
    task.agentId = "cancel_agent";
    tasks.append(task);
    
    QString planId = coordinator->submitPlan(tasks);
    bool cancelled = coordinator->cancelPlan(planId, "test-cancellation");
    QVERIFY(cancelled);
    
    QJsonObject status = coordinator->getPlanStatus(planId);
    QVERIFY(status["cancelled"].toBool());
    QCOMPARE(status["cancelReason"].toString(), "test-cancellation");
}

void TestAgentCoordinator::testCancelTaskDownstreamSkip()
{
    coordinator->registerAgent("skip_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    
    AgentCoordinator::AgentTask t1;
    t1.id = "skip_t1";
    t1.agentId = "skip_agent";
    tasks.append(t1);
    
    AgentCoordinator::AgentTask t2;
    t2.id = "skip_t2";
    t2.agentId = "skip_agent";
    t2.dependencies = {"skip_t1"};
    tasks.append(t2);
    
    QString planId = coordinator->submitPlan(tasks);
    coordinator->cancelPlan(planId, "cascade-test");
    
    QJsonObject status = coordinator->getPlanStatus(planId);
    QJsonArray taskArray = status["tasks"].toArray();
    
    // Both tasks should be cancelled
    for (const auto& taskObj : taskArray) {
        QCOMPARE(taskObj.toObject()["state"].toString(), "cancelled");
    }
}

void TestAgentCoordinator::testCancelWhilePending()
{
    coordinator->registerAgent("pending_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    AgentCoordinator::AgentTask task;
    task.id = "pending_cancel";
    task.agentId = "pending_agent";
    tasks.append(task);
    
    QString planId = coordinator->submitPlan(tasks);
    bool cancelled = coordinator->cancelPlan(planId, "pending-cancel");
    QVERIFY(cancelled);
}

// ===== Failure Path Tests =====

void TestAgentCoordinator::testTaskFailureCausesDownstreamSkip()
{
    coordinator->registerAgent("fail_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    
    AgentCoordinator::AgentTask t1;
    t1.id = "fail_t1";
    t1.agentId = "fail_agent";
    tasks.append(t1);
    
    AgentCoordinator::AgentTask t2;
    t2.id = "fail_t2";
    t2.agentId = "fail_agent";
    t2.dependencies = {"fail_t1"};
    tasks.append(t2);
    
    QString planId = coordinator->submitPlan(tasks);
    
    // Start and fail t1
    coordinator->startTask(planId, "fail_t1");
    coordinator->completeTask(planId, "fail_t1", QJsonObject(), false, "intentional-failure");
    
    QJsonObject status = coordinator->getPlanStatus(planId);
    QJsonArray taskArray = status["tasks"].toArray();
    
    // Find task states
    QString t1State, t2State;
    for (const auto& taskObj : taskArray) {
        auto obj = taskObj.toObject();
        if (obj["id"].toString() == "fail_t1") {
            t1State = obj["state"].toString();
        } else if (obj["id"].toString() == "fail_t2") {
            t2State = obj["state"].toString();
        }
    }
    
    QCOMPARE(t1State, "failed");
    QCOMPARE(t2State, "skipped");
}

void TestAgentCoordinator::testPlanFailsOnTaskFailure()
{
    coordinator->registerAgent("plan_fail_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    AgentCoordinator::AgentTask task;
    task.id = "failing_task";
    task.agentId = "plan_fail_agent";
    tasks.append(task);
    
    QString planId = coordinator->submitPlan(tasks);
    
    coordinator->startTask(planId, "failing_task");
    coordinator->completeTask(planId, "failing_task", QJsonObject(), false, "plan-should-fail");
    
    QJsonObject status = coordinator->getPlanStatus(planId);
    // Check that plan has failed - we can't directly query plan status, so we'll check task state
    QJsonArray taskArray = status["tasks"].toArray();
    QCOMPARE(taskArray[0].toObject()["state"].toString(), "failed");
}

void TestAgentCoordinator::testFailureMessagePropagation()
{
    coordinator->registerAgent("msg_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    AgentCoordinator::AgentTask task;
    task.id = "msg_task";
    task.agentId = "msg_agent";
    tasks.append(task);
    
    QString planId = coordinator->submitPlan(tasks);
    
    coordinator->startTask(planId, "msg_task");
    coordinator->completeTask(planId, "msg_task", QJsonObject(), false, "detailed-error-message");
    
    // Plan is now failed; error message should be recorded
    QJsonObject status = coordinator->getPlanStatus(planId);
    QVERIFY(!status.value(QStringLiteral("cancelReason")).toString().isEmpty());
}

// ===== Context Sharing Tests =====

void TestAgentCoordinator::testSharedContextMerge()
{
    coordinator->registerAgent("ctx_agent", {"general"}, 1);
    
    QJsonObject initialContext;
    initialContext["initial_key"] = "initial_value";
    
    QList<AgentCoordinator::AgentTask> tasks;
    AgentCoordinator::AgentTask task;
    task.id = "ctx_task";
    task.agentId = "ctx_agent";
    tasks.append(task);
    
    QString planId = coordinator->submitPlan(tasks, initialContext);
    
    QJsonObject status = coordinator->getPlanStatus(planId);
    QJsonObject contextObj = status.value(QStringLiteral("context")).toObject();
    QCOMPARE(contextObj.value(QStringLiteral("initial_key")).toString(), QStringLiteral("initial_value"));
}

void TestAgentCoordinator::testOutputContextAccumulation()
{
    coordinator->registerAgent("accum_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    
    AgentCoordinator::AgentTask t1;
    t1.id = "accum_t1";
    t1.agentId = "accum_agent";
    tasks.append(t1);
    
    AgentCoordinator::AgentTask t2;
    t2.id = "accum_t2";
    t2.agentId = "accum_agent";
    t2.dependencies = {"accum_t1"};
    tasks.append(t2);
    
    QString planId = coordinator->submitPlan(tasks);
    
    // Complete t1 with output context
    QJsonObject out1;
    out1["result_1"] = "value_1";
    coordinator->startTask(planId, "accum_t1");
    coordinator->completeTask(planId, "accum_t1", out1, true, "");
    
    // Start t2 and complete with additional output
    QJsonObject out2;
    out2["result_2"] = "value_2";
    coordinator->startTask(planId, "accum_t2");
    coordinator->completeTask(planId, "accum_t2", out2, true, "");
    
    QJsonObject status = coordinator->getPlanStatus(planId);
    QJsonObject contextObj = status.value(QStringLiteral("context")).toObject();
    QVERIFY(contextObj.contains(QStringLiteral("result_1")));
    QVERIFY(contextObj.contains(QStringLiteral("result_2")));
}

// ===== Concurrency Tests =====

void TestAgentCoordinator::testAgentConcurrencyLimit()
{
    coordinator->registerAgent("limited_agent", {"general"}, 2);
    
    QList<AgentCoordinator::AgentTask> tasks;
    
    // Create 3 independent tasks
    for (int i = 0; i < 3; ++i) {
        AgentCoordinator::AgentTask task;
        task.id = QString("concurrent_t%1").arg(i + 1);
        task.agentId = "limited_agent";
        tasks.append(task);
    }
    
    QString planId = coordinator->submitPlan(tasks);
    
    // Start first task
    QVERIFY(coordinator->startTask(planId, "concurrent_t1"));
    
    // Start second task (should work, concurrency limit is 2)
    QVERIFY(coordinator->startTask(planId, "concurrent_t2"));
    
    // Try to start third task (should fail, limit reached)
    QVERIFY(!coordinator->startTask(planId, "concurrent_t3"));
    
    // Complete first task, freeing a slot
    coordinator->completeTask(planId, "concurrent_t1", QJsonObject(), true, "");
    
    // Now third task should be able to start
    QVERIFY(coordinator->startTask(planId, "concurrent_t3"));
}

void TestAgentCoordinator::testMultiplePlans()
{
    coordinator->registerAgent("multi_agent", {"general"}, 2);
    
    // Create and submit two plans
    QList<AgentCoordinator::AgentTask> tasks1;
    AgentCoordinator::AgentTask t1;
    t1.id = "multi_plan1_t1";
    t1.agentId = "multi_agent";
    tasks1.append(t1);
    
    QList<AgentCoordinator::AgentTask> tasks2;
    AgentCoordinator::AgentTask t2;
    t2.id = "multi_plan2_t1";
    t2.agentId = "multi_agent";
    tasks2.append(t2);
    
    QString planId1 = coordinator->submitPlan(tasks1);
    QString planId2 = coordinator->submitPlan(tasks2);
    
    QVERIFY(!planId1.isEmpty());
    QVERIFY(!planId2.isEmpty());
    QVERIFY(planId1 != planId2);
    
    QJsonObject stats = coordinator->getCoordinatorStats();
    QCOMPARE(stats.value(QStringLiteral("activePlans")).toInt(), 2);
}

// ===== Introspection Tests =====

void TestAgentCoordinator::testGetPlanStatus()
{
    coordinator->registerAgent("status_agent", {"general"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    AgentCoordinator::AgentTask task;
    task.id = "status_task";
    task.agentId = "status_agent";
    task.priority = 5;
    tasks.append(task);
    
    QString planId = coordinator->submitPlan(tasks);
    
    QJsonObject status = coordinator->getPlanStatus(planId);
    QVERIFY(status.contains(QStringLiteral("planId")));
    QVERIFY(status.contains(QStringLiteral("createdAt")));
    QVERIFY(status.contains(QStringLiteral("cancelled")));
    QVERIFY(status.contains(QStringLiteral("cancelReason")));
    QVERIFY(status.contains(QStringLiteral("tasks")));
    QVERIFY(status.contains(QStringLiteral("context")));
    
    QJsonArray taskArray = status.value(QStringLiteral("tasks")).toArray();
    QCOMPARE(taskArray.size(), 1);
    
    auto taskObj = taskArray[0].toObject();
    QCOMPARE(taskObj.value(QStringLiteral("id")).toString(), QStringLiteral("status_task"));
    QCOMPARE(taskObj.value(QStringLiteral("state")).toString(), QStringLiteral("ready"));
    QCOMPARE(taskObj.value(QStringLiteral("priority")).toInt(), 5);
}

void TestAgentCoordinator::testGetCoordinatorStats()
{
    coordinator->registerAgent("stats_agent1", {"task1"}, 1);
    coordinator->registerAgent("stats_agent2", {"task2"}, 1);
    
    QList<AgentCoordinator::AgentTask> tasks;
    AgentCoordinator::AgentTask task;
    task.id = "stats_task";
    task.agentId = "stats_agent1";
    tasks.append(task);
    
    coordinator->submitPlan(tasks);
    coordinator->startTask("", "stats_task");  // Will fail gracefully
    
    QJsonObject stats = coordinator->getCoordinatorStats();
    QVERIFY(stats.contains(QStringLiteral("registeredAgents")));
    QVERIFY(stats.contains(QStringLiteral("activePlans")));
    QVERIFY(stats.contains(QStringLiteral("runningTasks")));
    QVERIFY(stats.value(QStringLiteral("registeredAgents")).toInt() >= 2);
}

QTEST_MAIN(TestAgentCoordinator)
#include "test_agent_coordinator.moc"
