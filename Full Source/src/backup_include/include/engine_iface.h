#pragma once
#include <string>

struct AgentRequest {
    int mode;
    std::string prompt;
    bool deep_thinking;
    bool deep_research;
    bool no_refusal;
    size_t context_limit;
};

class Engine {
public:
    virtual bool load_model(const std::string& path) = 0;
    virtual std::string infer(const AgentRequest& req) = 0;
    virtual const char* name() = 0;
    virtual ~Engine() = default;
};

class EngineRegistry {
public:
    static Engine* get(const std::string& name);
    static void register_engine(Engine* e);
};
