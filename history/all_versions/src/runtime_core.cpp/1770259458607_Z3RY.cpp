#include "runtime_core.h"
#include "memory_system_global.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <iostream>
#include <algorithm>

#include "engine_iface.h"
#include "agent_modes.h"
#include "tool_registry.h"

#include "engine/sovereign_engines.h"
#include "modules/react_generator.h"

// Forward declarations for things that might be in other headers not yet created or linked
namespace MemoryPlugins { void init(size_t); }

void register_rawr_inference();

static std::atomic<bool> running = true;

struct RuntimeState {
    Engine* active_engine;
    AgentMode current_mode;
    size_t context_limit;
    bool deep_thinking;
    bool deep_research;
    bool no_refusal;
};

static RuntimeState g_state;
static std::unordered_map<std::string, Engine*> engines;

Engine* EngineRegistry::get(const std::string& name) {
    if (engines.find(name) != engines.end()) {
        return engines[name];
    }
    return nullptr;
}

void EngineRegistry::register_engine(Engine* e) {
    engines[e->name()] = e;
}

std::string get_active_engine_name() {
    if (g_state.active_engine) return g_state.active_engine->name();
    return "None";
}

void init_runtime() {
    register_rawr_inference();
    register_sovereign_engines();
    
    // Default to first available or nullptr specific logic
    if (!engines.empty()) {
        g_state.active_engine = engines.begin()->second;
    }
    g_state.current_mode = AgentMode::ASK;
    g_state.context_limit = 4096;
    g_state.deep_thinking = false;
    g_state.deep_research = false;
    g_state.no_refusal = false;
    
    // Initialize global subsystems
    memory_system_init(g_state.context_limit);
}

void set_mode(const std::string& mode_str) {
    if (mode_str == "plan") g_state.current_mode = AgentMode::PLAN;
    else if (mode_str == "edit") g_state.current_mode = AgentMode::EDIT;
    else if (mode_str == "bugreport") g_state.current_mode = AgentMode::BUGREPORT;
    else if (mode_str == "codesuggest") g_state.current_mode = AgentMode::CODESUGGEST;
    else g_state.current_mode = AgentMode::ASK;
}

void set_engine(const std::string& engine_name) {
    Engine* e = EngineRegistry::get(engine_name);
    if (e) {
        g_state.active_engine = e;
    }
}

void set_deep_thinking(bool v) { g_state.deep_thinking = v; }
void set_deep_research(bool v) { g_state.deep_research = v; }
void set_no_refusal(bool v) { g_state.no_refusal = v; }
void set_context(size_t tokens) { g_state.context_limit = tokens; }

void runtime_load_model(const std::string& path) {
    if (g_state.active_engine) {
        g_state.active_engine->load_model(path);
    }
}

std::string get_mode() {
    switch(g_state.current_mode) {
        case AgentMode::PLAN: return "plan";
        case AgentMode::EDIT: return "edit";
        case AgentMode::BUGREPORT: return "bugreport";
        case AgentMode::CODESUGGEST: return "codesuggest";
        default: return "ask";
    }
}

std::string get_active_engine() {
    return g_state.active_engine ? g_state.active_engine->name() : "none";
}

bool get_deep_thinking() { return g_state.deep_thinking; }
bool get_deep_research() { return g_state.deep_research; }
size_t get_context() { return g_state.context_limit; }

std::string process_prompt(const std::string& input) {
    if (!g_state.active_engine) {
        return "Error: No engine loaded.";
    }

    // Agent router logic could be expanded here
    AgentRequest req;
    req.mode = g_state.current_mode;
    req.prompt = input;
    req.deep_thinking = g_state.deep_thinking;
    req.deep_research = g_state.deep_research;
    req.no_refusal = g_state.no_refusal;
    req.context_limit = g_state.context_limit;
    req.max_tokens = static_cast<int>(std::min<size_t>(g_state.context_limit, 512));
    if (req.max_tokens <= 0) req.max_tokens = 1;

    // Tool expansion
    ToolRegistry::inject_tools(req);

    // Track input in memory system
    g_memory_system.PushContext(input);

    // Real inference
    std::string result = g_state.active_engine->infer(req);
    
    // Track output in memory system
    g_memory_system.PushContext(result);

    return result;
}
