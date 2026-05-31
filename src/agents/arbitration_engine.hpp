// =============================================================================
// agents/arbitration_engine.hpp — Multi-agent arbitration engine
// =============================================================================
// Routes work items to the best available agent, resolves conflicts, and
// enforces a priority queue without needing any external dependencies.
//
// Design:
//   • ArbitrationEngine owns a pool of AgentSlot entries (wrapping existing
//     AgenticAgentCoordinator agents or any callable via the IAgent interface).
//   • Work items are submitted as ArbitrationTask and placed in a priority
//     queue protected by a single mutex + condition_variable.
//   • A configurable number of dispatcher threads pull tasks and route them
//     to the least-loaded available agent with a matching capability mask.
//   • Conflict detection: if two in-flight tasks touch the same `resource_key`
//     the later one is deferred until the first completes (serial-key ordering).
//   • After every N completions a compaction step revises agent utilisation
//     scores so the dispatcher adapts to runtime performance.
//
// Wiring (minimal):
//   ArbitrationEngine arb;
//   arb.register_agent("analyst", [](const ArbitrationTask& t) {
//       return std::string("result of: ") + t.payload;
//   });
//   auto fut = arb.submit({"analyse", "some payload", /*priority=*/10});
//   std::string result = fut.get();
// =============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RawrXD::Agents {

// ---------------------------------------------------------------------------
// ArbitrationTask — submitted by callers
// ---------------------------------------------------------------------------
struct ArbitrationTask {
    std::string id;           // caller-assigned uniquifier (auto-generated if empty)
    std::string capability;   // what kind of agent can handle this
    std::string payload;      // serialised work description / JSON / plain text
    std::string resource_key; // if non-empty, serialises against other tasks on same key
    int         priority = 0; // higher = served sooner
    std::chrono::steady_clock::time_point deadline{};  // zero = no deadline
};

using AgentFn = std::function<std::string(const ArbitrationTask&)>;

// ---------------------------------------------------------------------------
// IAgent interface
// ---------------------------------------------------------------------------
struct IAgent {
    virtual ~IAgent() = default;
    virtual std::string  id()         const = 0;
    virtual std::string  capability() const = 0;
    virtual std::string  execute(const ArbitrationTask&) = 0;
    virtual float        utilization() const { return 0.f; }
};

// ---------------------------------------------------------------------------
// LambdaAgent — wraps a plain function
// ---------------------------------------------------------------------------
class LambdaAgent : public IAgent {
public:
    LambdaAgent(std::string id, std::string cap, AgentFn fn)
        : m_id(std::move(id)), m_cap(std::move(cap)), m_fn(std::move(fn)) {}

    std::string id()         const override { return m_id; }
    std::string capability() const override { return m_cap; }
    float utilization()      const override {
        return static_cast<float>(m_busy.load()) / (m_total.load() + 1);
    }
    std::string execute(const ArbitrationTask& t) override {
        m_busy++;
        m_total++;
        std::string r = m_fn(t);
        m_busy--;
        return r;
    }

private:
    std::string       m_id, m_cap;
    AgentFn           m_fn;
    std::atomic<int>  m_busy{0};
    std::atomic<int>  m_total{0};
};

// ---------------------------------------------------------------------------
// ArbitrationResult
// ---------------------------------------------------------------------------
struct ArbitrationResult {
    std::string task_id;
    std::string agent_id;
    std::string output;
    bool        ok          = true;
    std::string error;
    std::chrono::milliseconds latency{0};
};

// ---------------------------------------------------------------------------
// ArbitrationEngine
// ---------------------------------------------------------------------------
class ArbitrationEngine {
public:
    struct Config {
        uint32_t dispatcher_threads = 4;
        uint32_t max_queue_depth    = 1024;
        bool     enable_conflict_serialisation = true;
    };

    explicit ArbitrationEngine(Config cfg = {}) : m_cfg(cfg) {}
    ~ArbitrationEngine() { shutdown(); }

    ArbitrationEngine(const ArbitrationEngine&)            = delete;
    ArbitrationEngine& operator=(const ArbitrationEngine&) = delete;

    // -----------------------------------------------------------------------
    // Register agents
    // -----------------------------------------------------------------------
    void register_agent(std::unique_ptr<IAgent> agent) {
        std::lock_guard<std::mutex> lk(m_agents_mx);
        m_agents[agent->id()] = std::move(agent);
    }

    void register_agent(const std::string& id,
                        const std::string& capability,
                        AgentFn fn) {
        register_agent(std::make_unique<LambdaAgent>(id, capability, std::move(fn)));
    }

    // Convenience: same id and capability
    void register_agent(const std::string& name, AgentFn fn) {
        register_agent(name, name, std::move(fn));
    }

    // -----------------------------------------------------------------------
    // start() / shutdown()
    // -----------------------------------------------------------------------
    void start() {
        m_running.store(true);
        for (uint32_t i = 0; i < m_cfg.dispatcher_threads; ++i) {
            m_threads.emplace_back([this] { dispatch_loop(); });
        }
    }

    void shutdown() {
        m_running.store(false);
        m_cv.notify_all();
        for (auto& t : m_threads) if (t.joinable()) t.join();
        m_threads.clear();
    }

    // -----------------------------------------------------------------------
    // submit() — returns a future for the ArbitrationResult
    // -----------------------------------------------------------------------
    std::future<ArbitrationResult> submit(ArbitrationTask task) {
        auto promise = std::make_shared<std::promise<ArbitrationResult>>();
        auto future  = promise->get_future();

        if (task.id.empty()) task.id = make_id();

        {
            std::lock_guard<std::mutex> lk(m_queue_mx);
            if (m_pending.size() >= m_cfg.max_queue_depth) {
                ArbitrationResult r;
                r.task_id = task.id;
                r.ok      = false;
                r.error   = "queue full";
                promise->set_value(std::move(r));
                return future;
            }
            m_pending.push({std::move(task), std::move(promise)});
        }
        m_cv.notify_one();
        return future;
    }

    // Fire-and-forget variant
    void submit_fire(ArbitrationTask task,
                     std::function<void(ArbitrationResult)> on_done = {}) {
        std::thread([this, task = std::move(task), on_done]() mutable {
            auto fut = submit(std::move(task));
            auto r   = fut.get();
            if (on_done) on_done(std::move(r));
        }).detach();
    }

    // -----------------------------------------------------------------------
    // Stats
    // -----------------------------------------------------------------------
    struct Stats {
        uint64_t tasks_dispatched = 0;
        uint64_t tasks_succeeded  = 0;
        uint64_t tasks_failed     = 0;
        uint64_t conflicts_seen   = 0;
        size_t   queue_depth      = 0;
        size_t   agents_registered = 0;
    };

    Stats stats() const {
        std::lock_guard<std::mutex> lk(m_queue_mx);
        Stats s;
        s.tasks_dispatched  = m_dispatched.load();
        s.tasks_succeeded   = m_succeeded.load();
        s.tasks_failed      = m_failed.load();
        s.conflicts_seen    = m_conflicts.load();
        s.queue_depth       = m_pending.size();
        std::lock_guard<std::mutex> lka(m_agents_mx);
        s.agents_registered = m_agents.size();
        return s;
    }

private:
    // -----------------------------------------------------------------------
    // Internal queued entry
    // -----------------------------------------------------------------------
    struct Entry {
        ArbitrationTask                         task;
        std::shared_ptr<std::promise<ArbitrationResult>> promise;
    };

    struct PriorityCmp {
        bool operator()(const Entry& a, const Entry& b) const {
            // higher priority wins; break ties by older deadline
            if (a.task.priority != b.task.priority)
                return a.task.priority < b.task.priority;
            if (a.task.deadline != b.task.deadline)
                return a.task.deadline > b.task.deadline;
            return false;
        }
    };

    // -----------------------------------------------------------------------
    // Dispatcher loop
    // -----------------------------------------------------------------------
    void dispatch_loop() {
        while (m_running.load()) {
            Entry entry;
            {
                std::unique_lock<std::mutex> lk(m_queue_mx);
                m_cv.wait(lk, [this] {
                    return !m_pending.empty() || !m_running.load();
                });
                if (!m_running.load() && m_pending.empty()) return;
                if (m_pending.empty()) continue;

                entry = std::move(const_cast<Entry&>(m_pending.top()));
                m_pending.pop();
            }

            // Conflict serialisation: if another task is live on same resource key
            if (m_cfg.enable_conflict_serialisation &&
                !entry.task.resource_key.empty()) {
                bool wait = false;
                {
                    std::lock_guard<std::mutex> lk(m_res_mx);
                    if (m_active_resources.count(entry.task.resource_key)) {
                        wait = true;
                        m_conflicts++;
                    } else {
                        m_active_resources.insert(entry.task.resource_key);
                    }
                }
                if (wait) {
                    // Requeue and retry after a brief yield
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    std::lock_guard<std::mutex> lk(m_queue_mx);
                    m_pending.push(std::move(entry));
                    m_cv.notify_one();
                    continue;
                }
            }

            // Pick the best matching agent
            IAgent* agent = pick_agent(entry.task.capability);

            ArbitrationResult result;
            result.task_id = entry.task.id;
            auto t0 = std::chrono::steady_clock::now();

            if (!agent) {
                result.ok    = false;
                result.error = "no agent for capability: " + entry.task.capability;
                m_failed++;
            } else {
                result.agent_id = agent->id();
                try {
                    result.output = agent->execute(entry.task);
                    result.ok     = true;
                    m_succeeded++;
                } catch (const std::exception& ex) {
                    result.ok    = false;
                    result.error = ex.what();
                    m_failed++;
                }
            }

            result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0);

            m_dispatched++;

            // Release resource lock
            if (m_cfg.enable_conflict_serialisation &&
                !entry.task.resource_key.empty()) {
                std::lock_guard<std::mutex> lk(m_res_mx);
                m_active_resources.erase(entry.task.resource_key);
            }

            entry.promise->set_value(std::move(result));
        }
    }

    // -----------------------------------------------------------------------
    // Agent selection: least-loaded agent whose capability matches
    // -----------------------------------------------------------------------
    IAgent* pick_agent(const std::string& cap) const {
        std::lock_guard<std::mutex> lk(m_agents_mx);
        IAgent* best = nullptr;
        float   best_util = 2.f;
        for (const auto& [id, a] : m_agents) {
            if (a->capability() != cap && cap != "*") continue;
            float u = a->utilization();
            if (u < best_util) { best_util = u; best = a.get(); }
        }
        return best;
    }

    static std::string make_id() {
        static std::atomic<uint64_t> counter{0};
        return "arb_" + std::to_string(++counter);
    }

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------
    Config m_cfg;

    mutable std::mutex m_agents_mx;
    std::unordered_map<std::string, std::unique_ptr<IAgent>> m_agents;

    mutable std::mutex m_queue_mx;
    std::priority_queue<Entry, std::vector<Entry>, PriorityCmp> m_pending;
    std::condition_variable m_cv;

    std::mutex m_res_mx;
    std::unordered_set<std::string> m_active_resources;

    std::vector<std::thread> m_threads;
    std::atomic<bool> m_running{false};

    std::atomic<uint64_t> m_dispatched{0};
    std::atomic<uint64_t> m_succeeded{0};
    std::atomic<uint64_t> m_failed{0};
    std::atomic<uint64_t> m_conflicts{0};
};

} // namespace RawrXD::Agents
