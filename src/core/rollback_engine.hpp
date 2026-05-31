#pragma once

#include <functional>
#include <stack>
#include <string>
#include <vector>

#include "state_subscription_engine.hpp"

namespace RawrXD::Core {

class RollbackEngine {
public:
    struct Command {
        std::function<bool()> execute;
        std::function<void()> rollback;
        std::string name;
    };

    struct Transaction {
        std::vector<Command> commands;
        std::string txId;
        bool committed = false;
    };

    explicit RollbackEngine(StateSubscriptionEngine& state)
        : stateEngine_(state) {
    }

    bool executeTransaction(Transaction& tx) {
        size_t executed = 0;
        try {
            for (auto& cmd : tx.commands) {
                if (!cmd.execute || !cmd.rollback) {
                    rollbackRange(tx, executed);
                    return false;
                }

                if (!cmd.execute()) {
                    rollbackRange(tx, executed);
                    return false;
                }
                ++executed;
            }
            tx.committed = true;
            return true;
        } catch (...) {
            rollbackRange(tx, executed);
            return false;
        }
    }

    class Savepoint {
    public:
        explicit Savepoint(RollbackEngine& engine)
            : engine_(engine), active_(true) {
            engine_.savepoints_.push(this);
        }

        ~Savepoint() {
            if (active_) {
                engine_.rollbackToSavepoint(this);
            }
            if (!engine_.savepoints_.empty() && engine_.savepoints_.top() == this) {
                engine_.savepoints_.pop();
            }
        }

        void release() noexcept { active_ = false; }

        Savepoint(const Savepoint&) = delete;
        Savepoint& operator=(const Savepoint&) = delete;

    private:
        RollbackEngine& engine_;
        bool active_;

        friend class RollbackEngine;
    };

private:
    void rollbackRange(Transaction& tx, size_t executedCount) {
        for (size_t i = executedCount; i-- > 0;) {
            try {
                tx.commands[i].rollback();
            } catch (...) {
                // Best effort rollback. Do not throw across boundary.
            }
        }
    }

    void rollbackToSavepoint(Savepoint* sp) {
        (void)sp;
        // Hook for persistent snapshot rollback integration.
        // In this batch we keep this deterministic and side-effect safe.
    }

private:
    StateSubscriptionEngine& stateEngine_;
    std::stack<Savepoint*> savepoints_;
};

} // namespace RawrXD::Core
