#include "model_hotswap.h"
#include "gpu/streaming_inference.h"

namespace RawrXD::Inference {
    bool ModelHotSwap::loadModel(const std::string& path, ModelType type) {
        std::lock_guard<std::shared_mutex> lock(m_mutex);

        // Graceful unload
        if (m_currentEngine) {
            m_currentEngine->shutdown();
        }

        switch (type) {
            case ModelType::GGUF_VULKAN:
                m_currentEngine = std::make_unique<GPU::StreamingInference>();
                break;
            case ModelType::OLLAMA_REMOTE:
                m_currentEngine = std::make_unique<RemoteOllamaEngine>();
                break;
            default:
                return false;
        }

        bool ok = m_currentEngine->loadModel(path);
        if (!ok) m_currentEngine.reset();
        return ok;
    }

    void ModelHotSwap::generate(const std::vector<uint32_t>& prompt, TokenCallback cb) {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        if (!m_currentEngine) {
            cb(0, 0.0f, true, "No model loaded");
            return;
        }
        m_currentEngine->generateStream(4096, cb);
    }
}
