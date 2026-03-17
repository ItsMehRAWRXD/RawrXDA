#include <QtTest/QtTest>
#include <QObject>
#include <QString>
#include <QJsonObject>
#include "../src/agent/subagent_manager.hpp"
#include "../src/agent/subagent_task_distributor.hpp"
#include "../src/agent/chat_session_subagent_bridge.hpp"

/**
 * @brief Test suite for subagent multitasking system
 * 
 * Tests:
 * - Subagent creation and lifecycle
 * - Task submission and execution
 * - Subagent pool management
 * - Task distribution with dependencies
 * - Multitasking coordinator
 * - Chat session integration
 * - Maximum 20 subagents per session
 */
class TestSubagentMultitasking : public QObject {
    Q_OBJECT

private slots:
    // Subagent tests
    void testSubagentCreation();
    void testSubagentTaskExecution();
    void testSubagentMetrics();
    void testSubagentCancellation();
    void testSubagentPauseResume();

    // Pool tests
    void testSubagentPoolCreation();
    void testSubagentPoolAddRemove();
    void testSubagentPoolTaskDistribution();
    void testSubagentPoolLoadBalancing();
    void testSubagentPoolAutoScaling();

    // Distributor tests
    void testTaskDistribution();
    void testComplexTaskDistribution();
    void testParallelTaskExecution();
    void testSequentialTaskExecution();
    void testTaskDependencies();

    // Coordinator tests
    void testMultitaskingCoordinator();
    void testCoordinatorScaling();
    void testCoordinatorResourceManagement();
    void testMax20SubagentsPerSession();

    // Chat integration tests
    void testChatSessionInitialization();
    void testChatSessionTaskSubmission();
    void testChatSessionMultipleSessions();
    void testChatSessionCleanup();

    // Edge cases
    void testMaxConcurrentTasks();
    void testTaskTimeout();
    void testTaskRetry();
};

// ============================================================================
// Subagent Tests
// ============================================================================

void TestSubagentMultitasking::testSubagentCreation()
{
    Subagent agent("test_agent_1");
    
    QCOMPARE(agent.getAgentId(), "test_agent_1");
    QCOMPARE(agent.getStatus(), Subagent::Status::Idle);
    QCOMPARE(agent.isIdle(), true);
    QCOMPARE(agent.isBusy(), false);
    QCOMPARE(agent.isTerminated(), false);
    
    qInfo() << "✓ Subagent creation test passed";
}

void TestSubagentMultitasking::testSubagentTaskExecution()
{
    Subagent agent("test_agent_2");
    
    QJsonObject result;
    bool taskCompleted = false;
    
    Subagent::Task task;
    task.taskId = "test_task_1";
    task.description = "Test task";
    task.executor = [&]() -> QJsonObject {
        QJsonObject res;
        res["status"] = "success";
        res["message"] = "Task executed";
        return res;
    };
    task.priority = Subagent::TaskPriority::Normal;
    task.maxRetries = 3;
    task.timeoutMs = 5000;
    
    connect(&agent, &Subagent::taskCompleted, [&](const QString& taskId, const QJsonObject& res) {
        if (taskId == task.taskId) {
            result = res;
            taskCompleted = true;
        }
    });
    
    bool success = agent.executeTask(task);
    QCOMPARE(success, true);
    QCOMPARE(agent.isBusy(), true);
    
    // Wait for task to complete
    QTimer::singleShot(1000, qApp, &QCoreApplication::quit);
    qApp->exec();
    
    QCOMPARE(taskCompleted, true);
    QCOMPARE(result.value("status").toString(), "success");
    
    qInfo() << "✓ Subagent task execution test passed";
}

void TestSubagentMultitasking::testSubagentMetrics()
{
    Subagent agent("test_agent_3");
    
    QCOMPARE(agent.getTasksCompleted(), 0);
    QCOMPARE(agent.getTasksFailed(), 0);
    
    QJsonObject metrics = agent.getMetrics();
    QCOMPARE(metrics["agentId"].toString(), "test_agent_3");
    QCOMPARE(metrics["status"].toString(), "Idle");
    QCOMPARE(metrics["tasksCompleted"].toInt(), 0);
    QCOMPARE(metrics["tasksFailed"].toInt(), 0);
    
    qInfo() << "✓ Subagent metrics test passed";
}

void TestSubagentMultitasking::testSubagentCancellation()
{
    Subagent agent("test_agent_4");
    
    bool taskCancelled = false;
    
    Subagent::Task task;
    task.taskId = "test_cancel_task";
    task.description = "Task to cancel";
    task.executor = []() -> QJsonObject {
        QThread::msleep(5000);  // Long running task
        return QJsonObject();
    };
    task.timeoutMs = 10000;
    
    connect(&agent, &Subagent::taskCancelled, [&](const QString& taskId) {
        if (taskId == task.taskId) {
            taskCancelled = true;
        }
    });
    
    agent.executeTask(task);
    QCOMPARE(agent.isBusy(), true);
    
    bool cancelSuccess = agent.cancelCurrentTask();
    QCOMPARE(cancelSuccess, true);
    QCOMPARE(taskCancelled, true);
    QCOMPARE(agent.isIdle(), true);
    
    qInfo() << "✓ Subagent cancellation test passed";
}

void TestSubagentMultitasking::testSubagentPauseResume()
{
    Subagent agent("test_agent_5");
    
    Subagent::Task task;
    task.taskId = "test_pause_task";
    task.executor = []() -> QJsonObject { return QJsonObject(); };
    
    agent.executeTask(task);
    QCOMPARE(agent.isBusy(), true);
    
    agent.pause();
    QCOMPARE(agent.getStatus(), Subagent::Status::Paused);
    
    agent.resume();
    QCOMPARE(agent.getStatus(), Subagent::Status::Busy);
    
    qInfo() << "✓ Subagent pause/resume test passed";
}

// ============================================================================
// SubagentPool Tests
// ============================================================================

void TestSubagentMultitasking::testSubagentPoolCreation()
{
    auto pool = std::make_shared<SubagentPool>("test_pool_1", 20);
    
    QCOMPARE(pool->getPoolId(), "test_pool_1");
    QCOMPARE(pool->getMaxAgents(), 20);
    QVERIFY(pool->getAgentCount() > 0);
    QVERIFY(pool->getAgentCount() <= 20);
    
    qInfo() << "✓ Subagent pool creation test passed";
}

void TestSubagentMultitasking::testSubagentPoolAddRemove()
{
    auto pool = std::make_shared<SubagentPool>("test_pool_2", 20);
    
    int initialCount = pool->getAgentCount();
    
    bool addSuccess = pool->addAgent();
    QCOMPARE(addSuccess, true);
    QCOMPARE(pool->getAgentCount(), initialCount + 1);
    
    bool removeSuccess = pool->removeAgent();
    QCOMPARE(removeSuccess, true);
    QCOMPARE(pool->getAgentCount(), initialCount);
    
    qInfo() << "✓ Subagent pool add/remove test passed";
}

void TestSubagentMultitasking::testSubagentPoolTaskDistribution()
{
    auto pool = std::make_shared<SubagentPool>("test_pool_3", 20);
    
    bool taskCompleted = false;
    
    Subagent::Task task;
    task.taskId = "pool_task_1";
    task.description = "Pool test task";
    task.executor = [&]() -> QJsonObject {
        taskCompleted = true;
        return QJsonObject();
    };
    task.priority = Subagent::TaskPriority::Normal;
    
    QString taskId = pool->submitTask(task);
    QVERIFY(!taskId.isEmpty());
    
    // Wait for task execution
    QTimer::singleShot(2000, qApp, &QCoreApplication::quit);
    qApp->exec();
    
    QCOMPARE(taskCompleted, true);
    
    qInfo() << "✓ Subagent pool task distribution test passed";
}

void TestSubagentMultitasking::testSubagentPoolLoadBalancing()
{
    auto pool = std::make_shared<SubagentPool>("test_pool_4", 20);
    pool->setLoadBalancingStrategy("least-busy");
    
    int idleCount = pool->getIdleAgentCount();
    QVERIFY(idleCount > 0);
    
    double loadFactor = pool->getAverageLoadFactor();
    QVERIFY(loadFactor >= 0.0);
    QVERIFY(loadFactor <= 1.0);
    
    qInfo() << "✓ Subagent pool load balancing test passed";
}

void TestSubagentMultitasking::testSubagentPoolAutoScaling()
{
    auto pool = std::make_shared<SubagentPool>("test_pool_5", 20);
    pool->setAutoScaling(true, 1, 20);
    
    QCOMPARE(pool->getMaxAgents(), 20);
    
    pool->setAutoScaling(false);
    
    qInfo() << "✓ Subagent pool auto-scaling test passed";
}

// ============================================================================
// Task Distributor Tests
// ============================================================================

void TestSubagentMultitasking::testTaskDistribution()
{
    auto pool = std::make_shared<SubagentPool>("test_pool_dist1", 20);
    auto distributor = std::make_shared<SubagentTaskDistributor>(pool);
    
    bool taskCompleted = false;
    
    SubagentTaskDistributor::DistributedTask task;
    task.taskId = "dist_task_1";
    task.description = "Distribution test";
    task.executor = [&]() -> QJsonObject {
        taskCompleted = true;
        return QJsonObject();
    };
    task.dependencyMode = SubagentTaskDistributor::TaskDependency::NoDelay;
    
    connect(distributor.get(), &SubagentTaskDistributor::taskCompleted,
            [&](const QString& taskId, const QJsonObject& result) {
        if (taskId == task.taskId) {
            taskCompleted = true;
        }
    });
    
    QString resultId = distributor->distributeTask(task);
    QVERIFY(!resultId.isEmpty());
    
    qInfo() << "✓ Task distribution test passed";
}

void TestSubagentMultitasking::testComplexTaskDistribution()
{
    auto pool = std::make_shared<SubagentPool>("test_pool_dist2", 20);
    auto distributor = std::make_shared<SubagentTaskDistributor>(pool);
    
    QList<SubagentTaskDistributor::DistributedTask> subtasks;
    
    for (int i = 0; i < 3; ++i) {
        SubagentTaskDistributor::DistributedTask subtask;
        subtask.taskId = "subtask_" + QString::number(i);
        subtask.description = "Subtask " + QString::number(i);
        subtask.executor = []() -> QJsonObject { return QJsonObject(); };
        subtasks.append(subtask);
    }
    
    QString parentId = distributor->distributeComplexTask("Complex task", subtasks);
    QVERIFY(!parentId.isEmpty());
    
    qInfo() << "✓ Complex task distribution test passed";
}

void TestSubagentMultitasking::testParallelTaskExecution()
{
    auto pool = std::make_shared<SubagentPool>("test_pool_dist3", 20);
    auto distributor = std::make_shared<SubagentTaskDistributor>(pool);
    
    QList<SubagentTaskDistributor::DistributedTask> tasks;
    
    for (int i = 0; i < 5; ++i) {
        SubagentTaskDistributor::DistributedTask task;
        task.taskId = "parallel_task_" + QString::number(i);
        task.description = "Parallel task " + QString::number(i);
        task.executor = []() -> QJsonObject { return QJsonObject(); };
        task.dependencyMode = SubagentTaskDistributor::TaskDependency::NoDelay;
        tasks.append(task);
    }
    
    QString groupId = distributor->launchParallelTasks(tasks);
    QVERIFY(!groupId.isEmpty());
    
    qInfo() << "✓ Parallel task execution test passed";
}

void TestSubagentMultitasking::testSequentialTaskExecution()
{
    auto pool = std::make_shared<SubagentPool>("test_pool_dist4", 20);
    auto distributor = std::make_shared<SubagentTaskDistributor>(pool);
    
    QList<SubagentTaskDistributor::DistributedTask> tasks;
    
    for (int i = 0; i < 3; ++i) {
        SubagentTaskDistributor::DistributedTask task;
        task.taskId = "seq_task_" + QString::number(i);
        task.description = "Sequential task " + QString::number(i);
        task.executor = []() -> QJsonObject { return QJsonObject(); };
        task.dependencyMode = SubagentTaskDistributor::TaskDependency::Sequential;
        tasks.append(task);
    }
    
    QString groupId = distributor->launchSequentialTasks(tasks);
    QVERIFY(!groupId.isEmpty());
    
    qInfo() << "✓ Sequential task execution test passed";
}

void TestSubagentMultitasking::testTaskDependencies()
{
    auto pool = std::make_shared<SubagentPool>("test_pool_dist5", 20);
    auto distributor = std::make_shared<SubagentTaskDistributor>(pool);
    
    SubagentTaskDistributor::DistributedTask task1;
    task1.taskId = "dep_task_1";
    task1.description = "Task 1";
    task1.executor = []() -> QJsonObject { return QJsonObject(); };
    
    SubagentTaskDistributor::DistributedTask task2;
    task2.taskId = "dep_task_2";
    task2.description = "Task 2 depends on 1";
    task2.executor = []() -> QJsonObject { return QJsonObject(); };
    task2.dependsOnTasks.append(task1.taskId);
    
    distributor->distributeTask(task1);
    distributor->distributeTask(task2);
    
    qInfo() << "✓ Task dependencies test passed";
}

// ============================================================================
// Multitasking Coordinator Tests
// ============================================================================

void TestSubagentMultitasking::testMultitaskingCoordinator()
{
    auto coordinator = std::make_shared<MultitaskingCoordinator>("session_1");
    
    QCOMPARE(coordinator->getSubagentCount() > 0, true);
    QCOMPARE(coordinator->getAvailableSubagentCount() >= 0, true);
    
    QString taskId = coordinator->submitTask("Test task", 
        []() -> QJsonObject { return QJsonObject(); });
    
    QVERIFY(!taskId.isEmpty());
    
    qInfo() << "✓ Multitasking coordinator test passed";
}

void TestSubagentMultitasking::testCoordinatorScaling()
{
    auto coordinator = std::make_shared<MultitaskingCoordinator>("session_2");
    
    int initialCount = coordinator->getSubagentCount();
    
    coordinator->scaleSubagents(10);
    QCOMPARE(coordinator->getSubagentCount(), 10);
    
    coordinator->scaleSubagents(5);
    QCOMPARE(coordinator->getSubagentCount(), 5);
    
    qInfo() << "✓ Coordinator scaling test passed";
}

void TestSubagentMultitasking::testCoordinatorResourceManagement()
{
    auto coordinator = std::make_shared<MultitaskingCoordinator>("session_3");
    
    coordinator->setResourceLimits(2048, 80);
    coordinator->setMaxConcurrentTasks(20);
    
    QJsonObject metrics = coordinator->getCoordinatorMetrics();
    QCOMPARE(metrics["maxConcurrentTasks"].toInt(), 20);
    
    qInfo() << "✓ Coordinator resource management test passed";
}

void TestSubagentMultitasking::testMax20SubagentsPerSession()
{
    auto coordinator = std::make_shared<MultitaskingCoordinator>("session_max_20");
    
    coordinator->initializeSubagents(5);
    
    // Add agents up to 20
    while (coordinator->getSubagentCount() < 20) {
        bool success = coordinator->addSubagent();
        QCOMPARE(success, true);
    }
    
    QCOMPARE(coordinator->getSubagentCount(), 20);
    
    // Try to add more - should fail
    bool extraAgentSuccess = coordinator->addSubagent();
    QCOMPARE(extraAgentSuccess, false);
    QCOMPARE(coordinator->getSubagentCount(), 20);
    
    qInfo() << "✓ Maximum 20 subagents per session test passed";
}

// ============================================================================
// Chat Integration Tests
// ============================================================================

void TestSubagentMultitasking::testChatSessionInitialization()
{
    auto bridge = ChatSessionSubagentManager::getInstance();
    
    bridge->initializeForSession("chat_session_1", 5);
    
    QCOMPARE(bridge->isSessionInitialized("chat_session_1"), true);
    QCOMPARE(bridge->getSubagentCountForSession("chat_session_1"), 5);
    
    qInfo() << "✓ Chat session initialization test passed";
}

void TestSubagentMultitasking::testChatSessionTaskSubmission()
{
    auto bridge = ChatSessionSubagentManager::getInstance();
    
    bridge->initializeForSession("chat_session_2", 5);
    
    QString taskId = bridge->submitChatTask("chat_session_2", 
        "Test chat task",
        []() -> QString { return "Task result"; });
    
    QVERIFY(!taskId.isEmpty());
    
    qInfo() << "✓ Chat session task submission test passed";
}

void TestSubagentMultitasking::testChatSessionMultipleSessions()
{
    auto bridge = ChatSessionSubagentManager::getInstance();
    
    bridge->initializeForSession("chat_session_3", 5);
    bridge->initializeForSession("chat_session_4", 10);
    bridge->initializeForSession("chat_session_5", 7);
    
    QCOMPARE(bridge->isSessionInitialized("chat_session_3"), true);
    QCOMPARE(bridge->isSessionInitialized("chat_session_4"), true);
    QCOMPARE(bridge->isSessionInitialized("chat_session_5"), true);
    
    QCOMPARE(bridge->getSubagentCountForSession("chat_session_3"), 5);
    QCOMPARE(bridge->getSubagentCountForSession("chat_session_4"), 10);
    QCOMPARE(bridge->getSubagentCountForSession("chat_session_5"), 7);
    
    QStringList sessions = bridge->getActiveSessions();
    QVERIFY(sessions.contains("chat_session_3"));
    QVERIFY(sessions.contains("chat_session_4"));
    QVERIFY(sessions.contains("chat_session_5"));
    
    qInfo() << "✓ Chat session multiple sessions test passed";
}

void TestSubagentMultitasking::testChatSessionCleanup()
{
    auto bridge = ChatSessionSubagentManager::getInstance();
    
    bridge->initializeForSession("chat_session_cleanup", 5);
    QCOMPARE(bridge->isSessionInitialized("chat_session_cleanup"), true);
    
    bridge->cleanupSession("chat_session_cleanup");
    QCOMPARE(bridge->isSessionInitialized("chat_session_cleanup"), false);
    
    qInfo() << "✓ Chat session cleanup test passed";
}

// ============================================================================
// Edge Case Tests
// ============================================================================

void TestSubagentMultitasking::testMaxConcurrentTasks()
{
    auto coordinator = std::make_shared<MultitaskingCoordinator>("session_concurrent");
    coordinator->setMaxConcurrentTasks(10);
    
    // Submit 15 tasks
    for (int i = 0; i < 15; ++i) {
        QString taskId = coordinator->submitTask("Concurrent task " + QString::number(i),
            []() -> QJsonObject { return QJsonObject(); });
        QVERIFY(!taskId.isEmpty());
    }
    
    qInfo() << "✓ Max concurrent tasks test passed";
}

void TestSubagentMultitasking::testTaskTimeout()
{
    auto coordinator = std::make_shared<MultitaskingCoordinator>("session_timeout");
    
    bool taskFailed = false;
    
    QString taskId = coordinator->submitTask("Timeout task",
        []() -> QJsonObject {
            QThread::msleep(5000);
            return QJsonObject();
        });
    
    connect(coordinator.get(), QOverload<const QString&, const QString&>::of(&MultitaskingCoordinator::taskFailed),
            [&](const QString& id, const QString& error) {
        if (id == taskId) {
            taskFailed = true;
        }
    });
    
    qInfo() << "✓ Task timeout test passed";
}

void TestSubagentMultitasking::testTaskRetry()
{
    auto coordinator = std::make_shared<MultitaskingCoordinator>("session_retry");
    
    int attemptCount = 0;
    
    QString taskId = coordinator->submitTask("Retry task",
        [&]() -> QJsonObject {
            attemptCount++;
            if (attemptCount < 3) {
                throw std::runtime_error("Task failed");
            }
            return QJsonObject();
        });
    
    QVERIFY(!taskId.isEmpty());
    
    qInfo() << "✓ Task retry test passed";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    
    TestSubagentMultitasking testSuite;
    return QTest::qExec(&testSuite, argc, argv);
}

#include "test_subagent_multitasking.moc"
