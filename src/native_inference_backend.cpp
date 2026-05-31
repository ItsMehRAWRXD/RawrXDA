#include "native_inference_backend.h"

#include "BackendOrchestrator.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <string>
#include <thread>
#include <utility>
#include <windows.h>

namespace RawrXD {

namespace {

std::string MakeDebugPreview(const std::string& text, size_t maxLen = 80) {
    if (text.empty()) {
        return "<empty>";
    }

    std::string preview;
    const size_t limit = text.size() < maxLen ? text.size() : maxLen;
    preview.reserve(limit + 8);
    for (size_t i = 0; i < limit; ++i) {
        const char c = text[i];
        if (c == '\r') {
            preview += "\\r";
        } else if (c == '\n') {
            preview += "\\n";
        } else if (static_cast<unsigned char>(c) < 0x20) {
            preview += '?';
        } else {
            preview.push_back(c);
        }
    }
    if (text.size() > limit) {
        preview += "...";
    }
    return preview;
}

}

std::unique_ptr<INativeInferenceBackend> BackendRegistry::backend_;
std::mutex BackendRegistry::mutex_;

void BackendRegistry::SetBackend(std::unique_ptr<INativeInferenceBackend> backend) {
    std::lock_guard<std::mutex> lock(mutex_);
    backend_ = std::move(backend);
}

INativeInferenceBackend* BackendRegistry::GetBackend() {
    std::lock_guard<std::mutex> lock(mutex_);
    return backend_.get();
}

class BackendOrchestratorChatBackend final : public INativeInferenceBackend {
public:
    bool SubmitInference(const std::vector<int>& prompt_tokens, const GenerationConfig& config) override {
        OutputDebugStringA(("[NativeBackend] SubmitInference called tokens=" +
                            std::to_string(prompt_tokens.size()) +
                            " max_tokens=" + std::to_string(config.max_tokens) +
                            " temp=" + std::to_string(config.temperature) +
                            " top_k=" + std::to_string(config.top_k) +
                            " top_p=" + std::to_string(config.top_p) + "\n")
                               .c_str());

        if (!IsAvailable()) {
            OutputDebugStringA("[NativeBackend] ERROR: Backend !IsAvailable()\n");
            return false;
        }
        if (is_generating_.load()) {
            OutputDebugStringA("[NativeBackend] ERROR: Already generating\n");
            return false;
        }

        std::string prompt;
        prompt.reserve(prompt_tokens.size());
        for (int token : prompt_tokens) {
            if (token >= 0 && token <= 255) {
                prompt.push_back(static_cast<char>(token));
            }
        }

        const size_t sampleCount = prompt_tokens.size() < 6 ? prompt_tokens.size() : 6;
        for (size_t i = 0; i < sampleCount; ++i) {
            OutputDebugStringA(("[NativeBackend] prompt_token[" + std::to_string(i) + "]=" +
                                std::to_string(prompt_tokens[i]) + "\n")
                                   .c_str());
        }

        if (prompt.empty()) {
            OutputDebugStringA("[NativeBackend] ERROR: Prompt tokens decoded to empty prompt\n");
            return false;
        }

        OutputDebugStringA(("[NativeBackend] prompt_preview='" + MakeDebugPreview(prompt) + "'\n").c_str());

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            output_queue_.clear();
        }

        cancel_flag_.store(false);
        completed_.store(false);
        is_generating_.store(true);
        active_request_id_.store(0);

        auto& bo = BackendOrchestrator::Instance();
        InferRequest req{};
        req.id = 0;
        req.prompt = std::move(prompt);
        req.priority = RequestPriority::Normal;
        req.max_tokens = (config.max_tokens > 0) ? config.max_tokens : 512;
        req.stream_cb = [this](const std::string& token) {
            OutputDebugStringA(("[NativeBackend] TOKEN CALLBACK len=" + std::to_string(token.size()) +
                                " preview='" + MakeDebugPreview(token) + "'\n")
                                   .c_str());
            if (cancel_flag_.load()) {
                OutputDebugStringA("[NativeBackend] Callback cancelled\n");
                return;
            }
            std::lock_guard<std::mutex> lock(queue_mutex_);
            output_queue_.push_back(token);
            OutputDebugStringA(("[NativeBackend] Queued token, queue size=" +
                                std::to_string(output_queue_.size()) + "\n")
                                   .c_str());
        };
        req.complete_cb = [this](const std::string& completion, const std::string& metadata) {
            OutputDebugStringA(("[NativeBackend] COMPLETE callback completion_len=" +
                                std::to_string(completion.size()) + " metadata_len=" +
                                std::to_string(metadata.size()) + "\n")
                                   .c_str());
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (!completion.empty()) {
                    output_queue_.push_back(completion);
                }
                if (completion.empty() && !metadata.empty()) {
                    output_queue_.push_back(metadata);
                }
                OutputDebugStringA(("[NativeBackend] COMPLETE queue size=" +
                                    std::to_string(output_queue_.size()) +
                                    " completion_preview='" + MakeDebugPreview(completion) +
                                    "' metadata_preview='" + MakeDebugPreview(metadata) + "'\n")
                                       .c_str());
            }
            is_generating_.store(false);
            completed_.store(true);
            OutputDebugStringA("[NativeBackend] GENERATION MARKED COMPLETE\n");
        };

        OutputDebugStringA("[NativeBackend] SubmitInference ACCEPTED - enqueueing request\n");
        const uint64_t request_id = bo.Enqueue(std::move(req));
        if (request_id == 0) {
            OutputDebugStringA("[NativeBackend] ERROR: Enqueue returned request_id=0\n");
            is_generating_.store(false);
            completed_.store(true);
            return false;
        }

        active_request_id_.store(request_id);
        OutputDebugStringA(("[NativeBackend] SubmitInference ACCEPTED request_id=" +
                            std::to_string(request_id) + "\n")
                               .c_str());
        return true;
    }

    bool GetResult(std::string& out_text, bool& out_done) override {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        const uint64_t callIndex = get_result_call_count_.fetch_add(1, std::memory_order_relaxed) + 1;

        out_text.clear();
        while (!output_queue_.empty()) {
            out_text += output_queue_.front();
            output_queue_.pop_front();
        }

        out_done = completed_.load() && output_queue_.empty();
        if (!out_text.empty() || out_done) {
            OutputDebugStringA(("[NativeBackend] GetResult -> text_len=" + std::to_string(out_text.size()) +
                                " done=" + (out_done ? std::string("true") : std::string("false")) +
                                " call=" + std::to_string(callIndex) +
                                " preview='" + MakeDebugPreview(out_text) + "'\n")
                                   .c_str());
        } else if ((callIndex % 20ULL) == 0ULL) {
            OutputDebugStringA(("[NativeBackend] GetResult -> no data call=" + std::to_string(callIndex) +
                                " queue_size=" + std::to_string(output_queue_.size()) +
                                " generating=" + (is_generating_.load() ? std::string("true") : std::string("false")) +
                                " completed=" + (completed_.load() ? std::string("true") : std::string("false")) +
                                "\n")
                                   .c_str());
        }
        return !out_text.empty() || out_done;
    }

    bool IsAvailable() const override {
        auto& bo = BackendOrchestrator::Instance();
        if (!bo.IsInitialized()) {
            bo.Initialize();
        }
        return bo.IsInitialized();
    }

    void Cancel() override {
        cancel_flag_.store(true);
        const uint64_t request_id = active_request_id_.load();
        OutputDebugStringA(("[NativeBackend] Cancel request_id=" + std::to_string(request_id) + "\n").c_str());
        if (request_id != 0) {
            BackendOrchestrator::Instance().Cancel(request_id);
        }
        is_generating_.store(false);
        completed_.store(true);
        OutputDebugStringA("[NativeBackend] Cancel marked generation complete\n");
    }

private:
    mutable std::mutex queue_mutex_;
    std::deque<std::string> output_queue_;
    std::atomic<bool> is_generating_{false};
    std::atomic<bool> cancel_flag_{false};
    std::atomic<bool> completed_{false};
    std::atomic<uint64_t> active_request_id_{0};
    std::atomic<uint64_t> get_result_call_count_{0};
};

bool InitializeAgenticChatBackend() {
    INativeInferenceBackend* existing = BackendRegistry::GetBackend();
    if (existing && existing->IsAvailable()) {
        OutputDebugStringA("[NativeBackendRegistry] Existing backend already available\n");
        return true;
    }

    auto backend = std::make_unique<BackendOrchestratorChatBackend>();
    if (!backend->IsAvailable()) {
        OutputDebugStringA("[NativeBackendRegistry] BackendOrchestrator unavailable during registration\n");
        return false;
    }

    BackendRegistry::SetBackend(std::move(backend));
    OutputDebugStringA("[NativeBackendRegistry] Agentic chat backend registered\n");
    return true;
}

}  // namespace RawrXD
