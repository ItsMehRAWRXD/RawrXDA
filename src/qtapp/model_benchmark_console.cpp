/**
 * @brief Console benchmark harness for GGUF models using InferenceEngine
 *
 * Measures:
 * - Load time
 * - Tokenization speed
 * - Prefill latency (KV-cache build)
 * - Decode throughput (tokens/sec)
 * - End-to-end latency per scenario
 * - Detokenization quality (non-empty, human-like)
 *
 * Scenarios:
 * 1) Short Chat
 * 2) Code Completion
 * 3) Math Reasoning
 * 4) Long Context Prefill + Short Decode
 */

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>
#include <numeric>


#include "inference_engine.hpp"

struct BenchResult {
    std::string name;
    double load_ms = 0;
    double tokenize_ms = 0;
    size_t tokens_in = 0;
    double prefill_ms = 0;
    double decode_ms = 0;
    int decode_tokens = 0;
    double tokens_per_sec = 0;
    std::string sample_output;
};

static double now_ms() {
    using clock = std::chrono::steady_clock;
    static auto start = clock::now();
    auto t = clock::now();
    return std::chrono::duration<double, std::milli>(t - start).count();
}

class ModelBench {
private:
    std::unique_ptr<InferenceEngine> engine;

public:
    bool init(const std::string &model_path) {
        auto t0 = now_ms();
        try {
            engine = std::make_unique<InferenceEngine>(model_path);
        } catch (const std::exception &e) {
            
            return false;
        }
        bool ok = engine->loadModel(model_path);
        auto t1 = now_ms();
        if (!ok) {
            
            return false;
        }


        return true;
    }

    BenchResult run_scenario(const std::string &name, const std::string &prompt, int max_decode = 256) {
        BenchResult r; r.name = name;

        // Tokenize
        auto t_tok0 = now_ms();
        auto tokens = engine->tokenize(std::string::fromStdString(prompt));
        auto t_tok1 = now_ms();
        r.tokenize_ms = t_tok1 - t_tok0;
        r.tokens_in = tokens.size();
        if (tokens.empty()) {
            
        }

        // Generate (prefill + decode) using engine->generate
        auto t_gen0 = now_ms();
        auto all_tokens = engine->generate(tokens, max_decode);
        auto t_gen1 = now_ms();

        // Split phases: we cannot directly time prefill vs decode internally here, but we can approximate:
        // prefll_ms ~ first forward over context (when KV not ready). decode_ms ~ remainder
        // Use engine tokensPerSecond for sanity, and end-to-end for user-visible.
        r.decode_ms = t_gen1 - t_gen0; // end-to-end generate
        int new_tokens = static_cast<int>(all_tokens.size()) - static_cast<int>(tokens.size());
        r.decode_tokens = std::max(new_tokens, 0);
        r.tokens_per_sec = (r.decode_tokens > 0 && r.decode_ms > 0.0) ? (1000.0 * r.decode_tokens / r.decode_ms) : 0.0;

        // Extract only new tokens for detokenization
        std::vector<int32_t> new_tok;
        if (all_tokens.size() > tokens.size()) {
            new_tok.assign(all_tokens.begin() + tokens.size(), all_tokens.end());
        }
        auto out = engine->detokenize(new_tok).toStdString();

        // Clean up output
        auto trim = [](std::string s){
            auto start = s.find_first_not_of(" \n\r\t");
            if (start != std::string::npos) s = s.substr(start);
            auto end = s.find_last_not_of(" \n\r\t");
            if (end != std::string::npos) s = s.substr(0, end + 1);
            return s;
        };
        r.sample_output = trim(out);
        return r;
    }
};

static void print_result(const BenchResult &r) {


    if (r.sample_output.empty()) {
        
    } else {
        
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);


    std::string model_path;
    if (argc > 1) {
        model_path = std::string::fromLocal8Bit(argv[1]);
        
    } else if (const char* env = std::getenv("BENCH_MODEL")) {
        model_path = std::string::fromLocal8Bit(env);
        
    } else {
        model_path = "gemma3";
        
    }

    ModelBench bench;
    if (!bench.init(model_path)) {
        
        return 1;
    }

    // Scenarios
    std::string chat_prompt = "User: Hello, how are you?\nAssistant:";
    std::string code_prompt = 
        "You are a helpful coding assistant.\n"
        "User: Write a C++ function that reverses a string.\n"
        "Assistant:";
    std::string math_prompt = 
        "You are a careful math assistant.\n"
        "User: What is (13 * 7) + 42? Show steps.\n"
        "Assistant:";
    std::string long_ctx_prompt;
    {
        std::string base = "The quick brown fox jumps over the lazy dog. ";
        for (int i = 0; i < 1024; ++i) long_ctx_prompt += base; // build long context
        long_ctx_prompt += "\nQuestion: Summarize the previous text in one sentence.\nAssistant:";
    }

    auto r1 = bench.run_scenario("Short Chat", chat_prompt, 128);
    auto r2 = bench.run_scenario("Code Completion", code_prompt, 192);
    auto r3 = bench.run_scenario("Math Reasoning", math_prompt, 192);
    auto r4 = bench.run_scenario("Long Context", long_ctx_prompt, 64);

    print_result(r1);
    print_result(r2);
    print_result(r3);
    print_result(r4);


    return 0;
}

