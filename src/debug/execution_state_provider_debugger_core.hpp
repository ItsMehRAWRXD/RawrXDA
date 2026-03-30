#pragma once

#include "real_time_completion_engine.h"
#include "ui/debugger_core.hpp"

class DebuggerCoreExecutionStateProvider : public IExecutionStateProvider {
public:
    explicit DebuggerCoreExecutionStateProvider(rawrxd::debug::DebuggerCore* debugger);

    bool getLatestSnapshot(ExecutionStateSnapshot& outSnapshot) override;

private:
    rawrxd::debug::DebuggerCore* m_debugger;
};
