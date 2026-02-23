#pragma once
#include <string>
#include <memory>

struct AgentRequest {
    int mode = 0;
    std::string prompt;
    bool deep_thinking = false;
    bool deep_research = false;
    bool no_refusal = false;
    size_t context_limit = 0;
};

class Engine {
public:
    virtual bool load_model(const std::string& path) = 0;
    virtual std::string infer(const AgentRequest& req) = 0;
    virtual const char* name() = 0;
    virtual ~Engine() = default;
};

// EngineRegistry holds shared ownership of engines.
// Engines are registered via shared_ptr and retrieved as raw non-owning pointers.
class EngineRegistry {
public:
    static Engine* get(const std::string& name);
    static void register_engine(std::shared_ptr<Engine> e);
};
