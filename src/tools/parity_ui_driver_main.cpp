// parity_ui_driver_main.cpp — Headless UI-side parity driver.
//
// Calls RawrXD::runLocalInferencePipeline() exactly the same way Win32IDE.cpp
// does at line ~9397, but with no window / no message loop. Lets the UI half
// of the CLI/UI parity check run autonomously in CI / scripts.
//
// Pair with `rawrxd run ... --emit-json-trace cli.json` and
// `scripts\compare_parity_trace.ps1`.
//
// Required env for hardware-free runs:
//   RAWRXD_PARITY_CPU=1            (bypass GPU gate, deterministic generator)
//   RAWRXD_PIPELINE_TRACE=ui.json  (writer in rawr_inference_pipeline.cpp)
//
// Usage:
//   rawrxd-parity-ui-driver --model <name> --prompt "<text>" [--num-predict N]

#include "../core/rawr_inference_pipeline.h"
#include "../gpu_enforcement.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void printUsage()
{
    std::fputs(
        "rawrxd-parity-ui-driver\n"
        "  --model <name>          Model identifier (any string in parity-cpu mode)\n"
        "  --prompt \"<text>\"       Single-turn prompt\n"
        "  --num-predict <N>       Token budget (default 32 in parity-cpu mode)\n"
        "  --help                  This message\n"
        "\n"
        "Env:\n"
        "  RAWRXD_PARITY_CPU=1            Required on hardware-free hosts\n"
        "  RAWRXD_PIPELINE_TRACE=<file>   Where the UI trace JSON is written\n"
        "  RAWRXD_PIPELINE_STRICT=1       Fail-closed (no agentic fallback)\n",
        stderr);
}

} // namespace

int main(int argc, char* argv[])
{
    std::string model;
    std::string prompt;
    int         numPredict = 32;

    for (int i = 1; i < argc; ++i)
    {
        const char* a = argv[i];
        if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0)
        {
            printUsage();
            return 0;
        }
        else if (std::strcmp(a, "--model") == 0 && i + 1 < argc)
        {
            model = argv[++i];
        }
        else if (std::strcmp(a, "--prompt") == 0 && i + 1 < argc)
        {
            prompt = argv[++i];
        }
        else if (std::strcmp(a, "--num-predict") == 0 && i + 1 < argc)
        {
            numPredict = std::atoi(argv[++i]);
            if (numPredict <= 0) numPredict = 32;
        }
        else
        {
            std::fprintf(stderr, "Unknown arg: %s\n", a);
            printUsage();
            return 2;
        }
    }

    if (model.empty() || prompt.empty())
    {
        std::fputs("error: --model and --prompt are required\n", stderr);
        printUsage();
        return 2;
    }

    // Mirror Win32IDE.cpp pattern: enforce GPU first (parity-cpu env bypasses).
    rxd::gpu::require();

    const std::string initErr = RawrXD::initInferencePipeline(model);
    if (!initErr.empty())
    {
        std::fprintf(stderr, "[UI-DRIVER] init failed: %s\n", initErr.c_str());
        // In parity-cpu mode the short-circuit fires inside runLocalInferencePipeline
        // before any DLL is touched, so init failures here are non-fatal — try to run.
    }

    RawrXD::PipelineRequest req;
    req.model      = model;
    req.prompt     = prompt;
    req.numPredict = numPredict;
    req.stream     = true;

    int  tokenCount = 0;
    bool sawError   = false;

    RawrXD::InferenceCallbacks cbs;
    cbs.onToken = [&](const std::string& tok, bool done)
    {
        if (!tok.empty())
        {
            ++tokenCount;
            std::fprintf(stderr, "[UI TOKEN] %s\n", tok.c_str());
            std::fputs(tok.c_str(), stdout);
        }
        if (done) std::fputs("\n", stdout);
    };
    cbs.onComplete = [&](const std::string& /*accum*/) {};
    cbs.onError    = [&](const std::string& err)
    {
        sawError = true;
        std::fprintf(stderr, "[UI-DRIVER] error: %s\n", err.c_str());
    };

    const bool ok = RawrXD::runLocalInferencePipeline(req, cbs);
    std::fprintf(stderr,
                 "[UI-DRIVER] done ok=%d tokens=%d error=%d\n",
                 ok ? 1 : 0, tokenCount, sawError ? 1 : 0);

    return (ok && !sawError) ? 0 : 1;
}
