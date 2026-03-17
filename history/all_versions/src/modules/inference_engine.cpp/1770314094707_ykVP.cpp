#include "engine_iface.h"
#include "cpu_inference_engine.h"
#include <memory>
#include <iostream>
#include <vector>

// --- ENGINE IMPLEMENTATION ---

class RawrInference : public Engine {
    std::unique_ptr<CPUInference::CPUInferenceEngine> m_engine;
    bool m_loaded = false;

public:
    RawrInference() : m_engine(std::make_unique<CPUInference::CPUInferenceEngine>()) {
        std::cout << "RawrInference: CPU inference engine initialized." << std::endl;
    }

    bool load_model(const std::string& path) override {
        if (!m_engine) return false;
        m_loaded = m_engine->LoadModel(path);
        if (m_loaded) {
            std::cout << "Model loaded successfully via CPUInferenceEngine." << std::endl;
        } else {
            std::cout << "Failed to load model." << std::endl;
        }
        return m_loaded;
    }

    std::string infer(const AgentRequest& req) override {
        if (!m_engine || !m_loaded) {
            return "Error: No model loaded. Load a GGUF model to begin inference.";
        }

        m_engine->SetContextLimit(req.context_limit);
        m_engine->SetMaxMode(req.no_refusal);
        m_engine->SetDeepThinking(req.deep_thinking);
        m_engine->SetDeepResearch(req.deep_research);

        std::vector<int32_t> input_ids = m_engine->Tokenize(req.prompt);
        std::string output;
        m_engine->GenerateStreaming(
            input_ids,
            req.max_tokens,
            [&](const std::string& token) { output += token; },
            nullptr);

        return output;
    }

    const char* name() override { return "RawrXD-CPU"; }
};

static RawrInference g_inference_engine;

void register_rawr_inference() {
    EngineRegistry::register_engine(&g_inference_engine);
}
