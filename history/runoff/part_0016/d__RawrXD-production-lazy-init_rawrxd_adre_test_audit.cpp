#include "core/ids.h"
#include "core/confidence.h"
#include "core/fact.h"
#include "core/hypothesis.h"
#include "core/belief_state.h"
#include "ckg/ckg_node.h"
#include "ckg/ckg_edge.h"
#include "ckg/ckg_graph.h"
#include "ckg/ckg_storage.h"
#include "agents/agent.h"
#include "agents/agent_action.h"
#include "agents/analysis/cfg_agent.cpp"
#include "agents/analysis/dataflow_agent.cpp"
#include "agents/hypothesis/function_role_agent.cpp"
#include "agents/hypothesis/struct_inference_agent.cpp"
#include "agents/validation/consistency_agent.cpp"
#include "agents/validation/confidence_agent.cpp"
#include "orchestrator/scheduler.h"
#include "orchestrator/orchestrator.cpp"
#include "replay/action_log.h"
#include "replay/deterministic_replay.cpp"
#include "integration/static_bridge.cpp"
#include "integration/scripting_agent.cpp"
#include <iostream>
#include <vector>

int main() {
    std::cout << "[Audit] Starting full system audit..." << std::endl;

    // Core tests
    Confidence conf{50};
    conf.increase(30);
    conf.decrease(10);
    Fact f{1, FactType::Instruction, "mov eax, ebx", conf};
    Hypothesis h{2, "Function is entry", {1}, {}, conf, HypothesisStatus::Proposed};
    BeliefState bs{3, {1}, {2}};

    // CKG tests
    CKGNode node{1, NodeType::Fact, "Instruction Node"};
    CKGEdge edge{1, 2, EdgeType::SUPPORTS};
    CausalKnowledgeGraph graph;
    graph.addNode(node);
    graph.addEdge(edge);

    // Agent tests
    CFGAgent cfg;
    DataflowAgent df;
    FunctionRoleAgent fr;
    StructInferenceAgent si;
    ConsistencyAgent ca;
    ConfidenceAgent coa;
    std::vector<Agent*> agents = {&cfg, &df, &fr, &si, &ca, &coa};
    for (auto* agent : agents) agent->run(bs);

    // Orchestrator tests
    AgentScheduler sched;
    for (auto* agent : agents) sched.enqueue(agent);
    while (Agent* a = sched.next()) a->run(bs);

    // Replay tests
    AgentAction act{1, 3, {1}, {2}, "Initial analysis"};
    ActionLog log{{act}};
    DeterministicReplay replay(log);
    replay.replay();

    // Integration tests
    auto fact = makeInstructionFact(0x401000, "push rbp");
    ScriptingAgent script;
    script.run(bs);

    std::cout << "[Audit] All modules tested. If no errors, all components are present." << std::endl;
    return 0;
}
