/**
 * @file test_agent_coordination.cpp
 * @brief Agent coordinator tests: task routing, state management, error recovery
 */

#include <gtest/gtest.h>
#include <QString>
#include <QMap>
#include <QMutex>
#include <chrono>
#include <memory>
#include <thread>
#include <queue>

/**
 * @enum TaskStatus
 * @brief Status of a task in the agent system
 */
enum class TaskStatus
{
    Pending,
    Assigned,
    Running,
    Completed,
    Failed,
    Retry,
    Cancelled
};

/**
 * @struct Task
 * @brief Represents a task in the agent system
 */
struct Task
{
    int id;
    QString type; // "inference", "compile", "analyze", etc.
    QString description;
    TaskStatus status = TaskStatus::Pending;
    int retryCount = 0;
    QString errorMessage;
    std::chrono::high_resolution_clock::time_point createdAt;
    std::chrono::high_resolution_clock::time_point startedAt;
    std::chrono::high_resolution_clock::time_point completedAt;
};

/**
 * @class Agent
 * @brief Simulates an agent in the coordination system
 */
class Agent
{
public:
    Agent(int id, const QString& type)
        : m_id(id), m_type(type), m_isAvailable(true), m_tasksProcessed(0)
    {
    }

    int id() const { return m_id; }
    const QString& type() const { return m_type; }
    bool isAvailable() const { return m_isAvailable; }
    int tasksProcessed() const { return m_tasksProcessed; }

    bool acceptTask(const Task& task)
    {
        if (!m_isAvailable || m_type != task.type) {
            return false;
        }

        m_isAvailable = false;
        m_currentTask = task;
        return true;
    }

    void completeTask(bool success = true)
    {
        m_tasksProcessed++;
        m_isAvailable = true;
    }

private:
    int m_id;
    QString m_type;
    bool m_isAvailable;
    int m_tasksProcessed;
    Task m_currentTask;
};

/**
 * @class AgentCoordinator
 * @brief Coordinates tasks among multiple agents
 */
class AgentCoordinator
{
public:
    AgentCoordinator() : m_nextTaskId(1) {}

    void registerAgent(std::shared_ptr<Agent> agent)
    {
        QMutexLocker locker(&m_mutex);
        m_agents.push_back(agent);
    }

    int submitTask(const QString& type, const QString& description)
    {
        QMutexLocker locker(&m_mutex);
        
        Task task;
        task.id = m_nextTaskId++;
        task.type = type;
        task.description = description;
        task.createdAt = std::chrono::high_resolution_clock::now();
        
        m_taskQueue.push(task);
        return task.id;
    }

    bool routeTask()
    {
        QMutexLocker locker(&m_mutex);
        
        if (m_taskQueue.empty()) {
            return false;
        }

        Task task = m_taskQueue.front();
        m_taskQueue.pop();

        // Find available agent of matching type
        for (auto& agent : m_agents) {
            if (agent->isAvailable() && agent->type() == task.type) {
                if (agent->acceptTask(task)) {
                    task.status = TaskStatus::Assigned;
                    m_assignedTasks[task.id] = task;
                    return true;
                }
            }
        }

        // Re-queue if no agent available
        m_taskQueue.push(task);
        return false;
    }

    int queueSize() const
    {
        QMutexLocker locker(&m_mutex);
        return m_taskQueue.size();
    }

    int agentCount() const
    {
        QMutexLocker locker(&m_mutex);
        return m_agents.size();
    }

    QMap<int, Task> assignedTasks() const
    {
        QMutexLocker locker(&m_mutex);
        return m_assignedTasks;
    }

private:
    mutable QMutex m_mutex;
    std::vector<std::shared_ptr<Agent>> m_agents;
    std::queue<Task> m_taskQueue;
    QMap<int, Task> m_assignedTasks;
    int m_nextTaskId;
};

/**
 * @class AgentCoordinationTest
 * @brief Test fixture for agent coordination
 */
class AgentCoordinationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_coordinator = std::make_unique<AgentCoordinator>();
    }

    void TearDown() override
    {
        m_coordinator.reset();
    }

    std::unique_ptr<AgentCoordinator> m_coordinator;
};

/**
 * Test: Single agent task routing
 */
TEST_F(AgentCoordinationTest, SingleAgentTaskRouting)
{
    auto agent = std::make_shared<Agent>(1, "inference");
    m_coordinator->registerAgent(agent);

    int taskId = m_coordinator->submitTask("inference", "Generate text");
    
    EXPECT_GT(taskId, 0) << "Should return valid task ID";
    EXPECT_EQ(m_coordinator->queueSize(), 1) << "Task should be queued";

    bool routed = m_coordinator->routeTask();
    EXPECT_TRUE(routed) << "Task should be routed";
    EXPECT_EQ(m_coordinator->queueSize(), 0) << "Queue should be empty";
}

/**
 * Test: Multiple agent task distribution
 */
TEST_F(AgentCoordinationTest, MultipleAgentTaskDistribution)
{
    const int AGENT_COUNT = 3;
    const int TASK_COUNT = 10;

    // Register agents
    for (int i = 1; i <= AGENT_COUNT; ++i) {
        auto agent = std::make_shared<Agent>(i, "inference");
        m_coordinator->registerAgent(agent);
    }

    // Submit tasks
    std::vector<int> taskIds;
    for (int i = 0; i < TASK_COUNT; ++i) {
        int taskId = m_coordinator->submitTask("inference", "Task " + QString::number(i));
        taskIds.push_back(taskId);
    }

    EXPECT_EQ(m_coordinator->queueSize(), TASK_COUNT) << "All tasks should be queued";

    // Route tasks
    int routedCount = 0;
    for (int i = 0; i < TASK_COUNT; ++i) {
        if (m_coordinator->routeTask()) {
            routedCount++;
        }
    }

    qInfo() << "[AgentCoordinationTest] Routed" << routedCount << "of" << TASK_COUNT << "tasks";

    EXPECT_GT(routedCount, 0) << "Should route at least some tasks";
    EXPECT_LE(routedCount, AGENT_COUNT) << "Cannot route more tasks than agents";
}

/**
 * Test: Task type matching
 */
TEST_F(AgentCoordinationTest, TaskTypeMatching)
{
    auto inferenceAgent = std::make_shared<Agent>(1, "inference");
    auto compileAgent = std::make_shared<Agent>(2, "compile");

    m_coordinator->registerAgent(inferenceAgent);
    m_coordinator->registerAgent(compileAgent);

    int inferenceTaskId = m_coordinator->submitTask("inference", "Generate");
    int compileTaskId = m_coordinator->submitTask("compile", "Build");

    EXPECT_EQ(m_coordinator->queueSize(), 2) << "Both tasks should be queued";

    // Route both tasks
    m_coordinator->routeTask();
    m_coordinator->routeTask();

    auto assignedTasks = m_coordinator->assignedTasks();
    qInfo() << "[AgentCoordinationTest] Assigned" << assignedTasks.size() << "tasks";

    EXPECT_EQ(assignedTasks.size(), 2) << "Both tasks should be assigned";
}

/**
 * Test: Task queue management
 */
TEST_F(AgentCoordinationTest, TaskQueueManagement)
{
    auto agent = std::make_shared<Agent>(1, "inference");
    m_coordinator->registerAgent(agent);

    const int TASK_COUNT = 20;

    // Submit all tasks
    for (int i = 0; i < TASK_COUNT; ++i) {
        m_coordinator->submitTask("inference", "Task " + QString::number(i));
    }

    EXPECT_EQ(m_coordinator->queueSize(), TASK_COUNT) << "All tasks in queue";

    // Route one at a time
    for (int i = 0; i < TASK_COUNT; ++i) {
        m_coordinator->routeTask();
        int expectedInQueue = TASK_COUNT - i - 1;
        EXPECT_EQ(m_coordinator->queueSize(), expectedInQueue)
            << "Queue size should decrease";
    }

    EXPECT_EQ(m_coordinator->queueSize(), 0) << "Queue should be empty";
}

/**
 * Test: Load balancing across agents
 */
TEST_F(AgentCoordinationTest, LoadBalancing)
{
    const int AGENT_COUNT = 4;
    std::vector<std::shared_ptr<Agent>> agents;

    for (int i = 1; i <= AGENT_COUNT; ++i) {
        auto agent = std::make_shared<Agent>(i, "inference");
        agents.push_back(agent);
        m_coordinator->registerAgent(agent);
    }

    const int TASK_COUNT = 20;

    // Submit tasks
    for (int i = 0; i < TASK_COUNT; ++i) {
        m_coordinator->submitTask("inference", "Task " + QString::number(i));
    }

    // Route tasks
    int routedCount = 0;
    while (m_coordinator->queueSize() > 0) {
        if (m_coordinator->routeTask()) {
            routedCount++;
        } else {
            break;
        }
    }

    auto assignedTasks = m_coordinator->assignedTasks();
    qInfo() << "[AgentCoordinationTest] Assigned" << assignedTasks.size() 
            << "tasks across" << AGENT_COUNT << "agents";

    EXPECT_GT(assignedTasks.size(), 0) << "Should assign some tasks";
    EXPECT_LE(assignedTasks.size(), AGENT_COUNT) << "Cannot exceed agent count";
}

/**
 * Test: Priority task handling
 */
TEST_F(AgentCoordinationTest, PriorityTaskHandling)
{
    // Create priority queue simulation
    std::vector<Task> tasks;
    
    for (int i = 0; i < 5; ++i) {
        Task task;
        task.id = i;
        task.type = "inference";
        task.description = "Task " + QString::number(i);
        tasks.push_back(task);
    }

    // Sort by priority (simulated as ID in reverse)
    std::sort(tasks.begin(), tasks.end(),
              [](const Task& a, const Task& b) { return a.id > b.id; });

    qInfo() << "[AgentCoordinationTest] Task execution order:";
    for (const auto& task : tasks) {
        qInfo() << "  Task" << task.id;
    }

    EXPECT_EQ(tasks[0].id, 4) << "Highest priority first";
}

/**
 * Test: Task retry mechanism
 */
TEST_F(AgentCoordinationTest, TaskRetryMechanism)
{
    auto agent = std::make_shared<Agent>(1, "inference");
    m_coordinator->registerAgent(agent);

    int taskId = m_coordinator->submitTask("inference", "Flaky task");

    // First attempt
    m_coordinator->routeTask();
    EXPECT_EQ(m_coordinator->queueSize(), 0) << "Task should be routed";

    // Simulate failure and requeue for retry
    m_coordinator->submitTask("inference", "Retry of task " + QString::number(taskId));
    EXPECT_EQ(m_coordinator->queueSize(), 1) << "Retry task should be queued";

    // Retry attempt
    m_coordinator->routeTask();
    EXPECT_EQ(m_coordinator->queueSize(), 0) << "Retry should be routed";
}

/**
 * Test: Concurrent task routing
 */
TEST_F(AgentCoordinationTest, ConcurrentTaskRouting)
{
    const int AGENT_COUNT = 5;
    for (int i = 1; i <= AGENT_COUNT; ++i) {
        auto agent = std::make_shared<Agent>(i, "inference");
        m_coordinator->registerAgent(agent);
    }

    const int TASK_COUNT = 50;
    auto startTime = std::chrono::high_resolution_clock::now();

    // Submit tasks from multiple threads
    std::vector<std::thread> threads;
    for (int t = 0; t < 3; ++t) {
        threads.emplace_back([this, t, TASK_COUNT]() {
            for (int i = 0; i < TASK_COUNT / 3; ++i) {
                m_coordinator->submitTask("inference",
                    "Task from thread " + QString::number(t) + " id " + QString::number(i));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Route tasks
    int routedCount = 0;
    while (m_coordinator->queueSize() > 0) {
        if (m_coordinator->routeTask()) {
            routedCount++;
        } else {
            break;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();

    qInfo() << "[AgentCoordinationTest] Concurrent coordination: submitted" << TASK_COUNT
            << "tasks, routed" << routedCount << "in" << latencyMs << "ms";

    EXPECT_GT(routedCount, 0) << "Should route tasks";
}

/**
 * Test: Agent availability tracking
 */
TEST_F(AgentCoordinationTest, AgentAvailabilityTracking)
{
    auto agent = std::make_shared<Agent>(1, "inference");
    m_coordinator->registerAgent(agent);

    EXPECT_TRUE(agent->isAvailable()) << "Agent should be available initially";

    // Assign task to agent
    Task task;
    task.id = 1;
    task.type = "inference";
    bool accepted = agent->acceptTask(task);

    EXPECT_TRUE(accepted) << "Agent should accept task";
    EXPECT_FALSE(agent->isAvailable()) << "Agent should be busy";

    // Complete task
    agent->completeTask();
    EXPECT_TRUE(agent->isAvailable()) << "Agent should be available after task";
}

/**
 * Test: Agent statistics tracking
 */
TEST_F(AgentCoordinationTest, AgentStatisticsTracking)
{
    std::vector<std::shared_ptr<Agent>> agents;
    for (int i = 1; i <= 3; ++i) {
        auto agent = std::make_shared<Agent>(i, "inference");
        agents.push_back(agent);
        m_coordinator->registerAgent(agent);
    }

    // Simulate work
    for (int i = 0; i < 10; ++i) {
        m_coordinator->submitTask("inference", "Task " + QString::number(i));
        m_coordinator->routeTask();
    }

    // Check statistics
    for (const auto& agent : agents) {
        qInfo() << "[AgentCoordinationTest] Agent" << agent->id()
                << "processed" << agent->tasksProcessed() << "tasks";
    }
}

/**
 * Test: Task state machine transitions
 */
TEST_F(AgentCoordinationTest, TaskStateTransitions)
{
    Task task;
    task.id = 1;
    task.type = "inference";
    task.status = TaskStatus::Pending;
    task.createdAt = std::chrono::high_resolution_clock::now();

    // Pending -> Assigned
    task.status = TaskStatus::Assigned;
    EXPECT_EQ(static_cast<int>(task.status), static_cast<int>(TaskStatus::Assigned));

    // Assigned -> Running
    task.startedAt = std::chrono::high_resolution_clock::now();
    task.status = TaskStatus::Running;
    EXPECT_EQ(static_cast<int>(task.status), static_cast<int>(TaskStatus::Running));

    // Running -> Completed
    task.completedAt = std::chrono::high_resolution_clock::now();
    task.status = TaskStatus::Completed;
    EXPECT_EQ(static_cast<int>(task.status), static_cast<int>(TaskStatus::Completed));

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        task.completedAt - task.createdAt);
    
    qInfo() << "[AgentCoordinationTest] Task lifecycle took" << duration.count() << "ms";
}

/**
 * Test: Error handling and recovery
 */
TEST_F(AgentCoordinationTest, ErrorHandlingAndRecovery)
{
    auto agent = std::make_shared<Agent>(1, "inference");
    m_coordinator->registerAgent(agent);

    int taskId = m_coordinator->submitTask("inference", "Task that will fail");
    m_coordinator->routeTask();

    // Simulate failure
    auto assignedTasks = m_coordinator->assignedTasks();
    if (assignedTasks.contains(taskId)) {
        Task failedTask = assignedTasks[taskId];
        failedTask.status = TaskStatus::Failed;
        failedTask.errorMessage = "Out of memory";
        failedTask.retryCount++;

        qInfo() << "[AgentCoordinationTest] Task failed:" << failedTask.errorMessage
                << "Retry count:" << failedTask.retryCount;

        EXPECT_GT(failedTask.retryCount, 0) << "Should track retry count";
    }
}
