// rawrxd_cli_real.cpp — Real CLI entry point for TITAN inference engine
// This is the standalone test harness for the restored engine.
// Usage: RawrXD_CLI_Real.exe --model path/to/model.gguf --chat "prompt"
#include "cpu_inference_engine.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <iostream>

namespace {

class ScopedQuietStdout
{
  public:
    explicit ScopedQuietStdout(bool enabled)
        : m_enabled(enabled)
    {
        if (!m_enabled)
        {
            return;
        }

        std::fflush(stdout);
        m_savedStdoutFd = _dup(_fileno(stdout));
        if (m_savedStdoutFd == -1)
        {
            m_enabled = false;
            return;
        }

        m_nullFd = _open("NUL", _O_WRONLY);
        if (m_nullFd == -1)
        {
            _close(m_savedStdoutFd);
            m_savedStdoutFd = -1;
            m_enabled = false;
            return;
        }

        if (_dup2(m_nullFd, _fileno(stdout)) == -1)
        {
            _close(m_nullFd);
            _close(m_savedStdoutFd);
            m_nullFd = -1;
            m_savedStdoutFd = -1;
            m_enabled = false;
        }
    }

    ~ScopedQuietStdout()
    {
        if (!m_enabled)
        {
            return;
        }

        std::fflush(stdout);
        (void)_dup2(m_savedStdoutFd, _fileno(stdout));
        _close(m_savedStdoutFd);
        _close(m_nullFd);
    }

    void writeResponse(const std::string& text) const
    {
        if (!m_enabled)
        {
            std::fwrite(text.data(), 1, text.size(), stdout);
            std::fflush(stdout);
            return;
        }

        if (m_savedStdoutFd != -1 && !text.empty())
        {
            (void)_write(m_savedStdoutFd, text.data(), static_cast<unsigned int>(text.size()));
        }
    }

    void writeResponseChar(char ch) const
    {
        if (!m_enabled)
        {
            std::fputc(ch, stdout);
            std::fflush(stdout);
            return;
        }

        if (m_savedStdoutFd != -1)
        {
            (void)_write(m_savedStdoutFd, &ch, 1);
        }
    }

  private:
    mutable bool m_enabled = false;
    int          m_savedStdoutFd = -1;
    int          m_nullFd = -1;
};

} // namespace

int main(int argc, char* argv[]) {
    std::fprintf(stderr, "[TITAN] RawrXD CLI - Real Engine\n");
    std::fprintf(stderr, "[TITAN] Engine: %s\n", "CPUInferenceEngine (Restored)");

    std::string model_path;
    std::string prompt;
    int max_tokens = 64;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            model_path = argv[++i];
        } else if (strcmp(argv[i], "--chat") == 0 && i + 1 < argc) {
            prompt = argv[++i];
        } else if (strcmp(argv[i], "--max-tokens") == 0 && i + 1 < argc) {
            max_tokens = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        }
    }

    const bool quietChat = !prompt.empty() && !verbose;
    if (quietChat) {
        SetEnvironmentVariableA("RAWRXD_TRACE_STEPS", "0");
    }

    ScopedQuietStdout quietStdout(quietChat);

    auto* engine = RawrXD::CPUInferenceEngine::getInstance();
    std::fprintf(stderr, "[TITAN] Singleton: %p\n", (void*)engine);

    if (model_path.empty()) {
        // Dry run - test that the engine initializes without a model
        std::fprintf(stderr, "[TITAN] No --model specified. Engine status:\n");
        std::fprintf(stderr, "  IsModelLoaded: %s\n", engine->IsModelLoaded() ? "yes" : "no");
        std::fprintf(stderr, "  GetEngineName: %s\n", engine->GetEngineName());
        std::fprintf(stderr, "  VocabSize: %d\n", engine->GetVocabSize());
        std::fprintf(stderr, "  EmbeddingDim: %d\n", engine->GetEmbeddingDim());
        std::fprintf(stderr, "  NumLayers: %d\n", engine->GetNumLayers());
        std::fprintf(stderr, "  NumHeads: %d\n", engine->GetNumHeads());
        std::fprintf(stderr, "[TITAN] Dry run complete. Pass --model <path.gguf> to load.\n");
        return 0;
    }

    std::fprintf(stderr, "[TITAN] Loading model: %s\n", model_path.c_str());
    if (!engine->LoadModel(model_path)) {
        std::fprintf(stderr, "[TITAN] ERROR: Failed to load model: %s\n", model_path.c_str());
        return 1;
    }

    std::fprintf(stderr, "[TITAN] Model loaded. VocabSize=%d EmbDim=%d Layers=%d Heads=%d\n",
                 engine->GetVocabSize(), engine->GetEmbeddingDim(),
                 engine->GetNumLayers(), engine->GetNumHeads());

    if (!prompt.empty()) {
        auto tokens = engine->Tokenize(prompt);
        if (verbose) {
            std::fprintf(stderr, "[TITAN] Tokenizing: \"%s\"\n", prompt.c_str());
            std::fprintf(stderr, "[TITAN] Token count: %zu\n", tokens.size());
            std::fprintf(stderr, "[TITAN] Tokens: [");
            for (size_t i = 0; i < tokens.size(); i++) {
                if (i > 0) std::fprintf(stderr, ", ");
                std::fprintf(stderr, "%d", tokens[i]);
            }
            std::fprintf(stderr, "]\n");
            std::fprintf(stderr, "[TITAN] Detokenize: \"%s\"\n", engine->Detokenize(tokens).c_str());
            std::fprintf(stderr, "[TITAN] Generating response (max %d tokens)...\n", max_tokens);
        }

        // Use streaming to show tokens as they arrive
        auto tok32 = std::vector<int32_t>(tokens.begin(), tokens.end());
        engine->GenerateStreaming(tok32, max_tokens,
            [&quietStdout](const std::string& piece) { quietStdout.writeResponse(piece); },
            [&quietStdout, verbose]() {
                quietStdout.writeResponseChar('\n');
                if (verbose) {
                    std::fprintf(stderr, "[TITAN] Generation complete.\n");
                }
            },
            nullptr
        );
    } else {
        // Interactive REPL
        std::fprintf(stderr, "[TITAN] Interactive mode. Type 'exit' to quit.\n");
        std::string line;
        while (std::fprintf(stderr, ">>> "), std::getline(std::cin, line)) {
            if (line == "exit" || line == "quit") break;
            auto tokens = engine->Tokenize(line);
            auto tok32 = std::vector<int32_t>(tokens.begin(), tokens.end());
            engine->GenerateStreaming(tok32, 128,
                [](const std::string& piece) { std::fwrite(piece.data(), 1, piece.size(), stdout); std::fflush(stdout); },
                []() { std::fputc('\n', stdout); std::fflush(stdout); },
                nullptr
            );
        }
    }

    return 0;
}
