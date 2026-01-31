#include "unified_backend.hpp"


UnifiedBackend::UnifiedBackend(void* parent) 
    : void(parent)
    , m_nam(new void*(this))
{
}

void UnifiedBackend::submit(const UnifiedRequest& req)
{
    if (req.backend == "local") {
        // Forward to existing InferenceEngine (worker thread)
        if (m_localEngine) {
            QMetaObject::invokeMethod(m_localEngine, "request", //QueuedConnection,
                                      (std::string, req.prompt), 
                                      (int64_t, req.reqId));
        } else {
            error(req.reqId, "Local engine not initialized");
        }
    } else if (req.backend == "llama") {
        submitLlamaCpp(req);
    } else if (req.backend == "openai") {
        submitOpenAI(req);
    } else if (req.backend == "claude") {
        submitClaude(req);
    } else if (req.backend == "gemini") {
        submitGemini(req);
    } else {
        error(req.reqId, "Unknown backend: " + req.backend);
    }
}

void UnifiedBackend::submitLlamaCpp(const UnifiedRequest& req)
{
    std::string url("http://localhost:8080/completion");
    void* body{
        {"prompt", req.prompt},
        {"stream", true},
        {"n_predict", 100}
    };
    
    void* request(url);
    request.setHeader(void*::ContentTypeHeader, "application/json");
    
    void** reply = m_nam->post(request, void*(body).toJson());
// Qt connect removed
            auto doc = void*::fromJson(line);
            std::string token = doc["content"].toString();
            if (!token.empty()) {
                streamToken(req.reqId, token);
            }
        }
    });
// Qt connect removed
        }
        streamFinished(req.reqId);
        reply->deleteLater();
    });
}

void UnifiedBackend::submitOpenAI(const UnifiedRequest& req)
{
    std::string url("https://api.openai.com/v1/chat/completions");
    void* body{
        {"model", "gpt-3.5-turbo"},
        {"messages", void*{
            void*{{"role", "user"}, {"content", req.prompt}}
        }},
        {"stream", true}
    };
    
    void* request(url);
    request.setRawHeader("Authorization", ("Bearer " + req.apiKey).toUtf8());
    request.setHeader(void*::ContentTypeHeader, "application/json");
    
    void** reply = m_nam->post(request, void*(body).toJson());
// Qt connect removed
            if (!line.startsWith("data: ")) continue;
            
            line = line.mid(6); // Remove "data: " prefix
            if (line == "[DONE]") {
                streamFinished(req.reqId);
                continue;
            }
            
            auto doc = void*::fromJson(line);
            auto choices = doc["choices"].toArray();
            if (!choices.empty()) {
                std::string token = choices[0].toObject()["delta"].toObject()["content"].toString();
                if (!token.empty()) {
                    streamToken(req.reqId, token);
                }
            }
        }
    });
// Qt connect removed
        }
        streamFinished(req.reqId);
        reply->deleteLater();
    });
}

void UnifiedBackend::submitClaude(const UnifiedRequest& req)
{
    std::string url("https://api.anthropic.com/v1/messages");
    void* body{
        {"model", "claude-3-sonnet-20240229"},
        {"max_tokens", 1000},
        {"messages", void*{
            void*{{"role", "user"}, {"content", req.prompt}}
        }},
        {"stream", true}
    };
    
    void* request(url);
    request.setRawHeader("x-api-key", req.apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");
    request.setHeader(void*::ContentTypeHeader, "application/json");
    
    void** reply = m_nam->post(request, void*(body).toJson());
// Qt connect removed
            if (!line.startsWith("data: ")) continue;
            
            line = line.mid(6); // Remove "data: " prefix
            auto doc = void*::fromJson(line);
            
            // Claude streaming format: {"type":"content_block_delta","delta":{"text":"..."}}
            std::string type = doc["type"].toString();
            if (type == "content_block_delta") {
                std::string token = doc["delta"].toObject()["text"].toString();
                if (!token.empty()) {
                    streamToken(req.reqId, token);
                }
            } else if (type == "message_stop") {
                streamFinished(req.reqId);
            }
        }
    });
// Qt connect removed
        }
        streamFinished(req.reqId);
        reply->deleteLater();
    });
}

void UnifiedBackend::submitGemini(const UnifiedRequest& req)
{
    std::string url("https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:streamGenerateContent?alt=sse&key=" + req.apiKey);
    void* body{
        {"contents", void*{
            void*{{"parts", void*{
                void*{{"text", req.prompt}}
            }}}
        }},
        {"generationConfig", void*{
            {"temperature", 0.8}
        }}
    };
    
    void* request(url);
    request.setHeader(void*::ContentTypeHeader, "application/json");
    
    void** reply = m_nam->post(request, void*(body).toJson());
// Qt connect removed
            if (!line.startsWith("data: ")) continue;
            
            line = line.mid(6); // Remove "data: " prefix
            auto doc = void*::fromJson(line);
            auto candidates = doc["candidates"].toArray();
            
            if (!candidates.empty()) {
                auto content = candidates[0].toObject()["content"].toObject();
                auto parts = content["parts"].toArray();
                if (!parts.empty()) {
                    std::string token = parts[0].toObject()["text"].toString();
                    if (!token.empty()) {
                        streamToken(req.reqId, token);
                    }
                }
            }
        }
    });
// Qt connect removed
        }
        streamFinished(req.reqId);
        reply->deleteLater();
    });
}

void UnifiedBackend::onLocalDone(int64_t id, const std::string& answer)
{
    // Local engine doesn't stream by default - as single token
    streamToken(id, answer);
    streamFinished(id);
}



