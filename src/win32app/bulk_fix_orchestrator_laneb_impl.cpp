// Lane B (RawrEngine): defines BulkFixOrchestrator::~BulkFixOrchestrator for SubAgentManager's
// unique_ptr member — avoids linking full autonomous_subagent.cpp (AgenticEngine / detector deps).
#include "agent/autonomous_subagent.hpp"

BulkFixOrchestrator::~BulkFixOrchestrator() = default;
