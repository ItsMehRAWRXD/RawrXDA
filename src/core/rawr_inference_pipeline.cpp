// rawr_inference_pipeline.cpp — Unified local-inference pipeline implementation
// Wraps rawrxd_serve_inference_plugin so both CLI and Win32IDE use the same call
// path for local GGUF weights.
#include "rawr_inference_pipeline.h"

// Pull in the DLL-bridge helpers from the serve layer.
// Win32IDE links rawrxd_serve_inference_plugin.cpp directly; RawrXD-Serve also
// includes this same TU — only one definition is needed per binary.
#include "../serve/rawrxd_serve_inference_plugin.h"
#include "../serve/rawrxd_serve.h"
#include "inference_parity_trace.h"
#include "parity_cpu_fallback.h"

#include "../gpu_enforcement.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
  #include <windows.h>
#endif

namespace RawrXD {

namespace {
std::string readProcessEnv(const char* name)
{
    if (!name || !name[0])
        return {};
#if defined(_WIN32)
    char buffer[4096] = {};
    const DWORD len = GetEnvironmentVariableA(name, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (len > 0 && len < sizeof(buffer))
        return std::string(buffer, len);
#endif
    const char* value = std::getenv(name);
    return (value && value[0]) ? std::string(value) : std::string();
}

// Strict mode: when enabled (env RAWRXD_PIPELINE_STRICT=1), upstream callers
// must treat a `false` return from runLocalInferencePipeline as fatal — no
// fallback to the agentic bridge or Ollama path. Used for CLI/UI parity tests.
bool isPipelineStrictEnabled()
{
    const std::string value = readProcessEnv("RAWRXD_PIPELINE_STRICT");
    return !value.empty() && value != "0";
}

void pipelineDebugMark(const char* tag)
{
#if defined(_WIN32)
    OutputDebugStringA(tag);
#endif
    std::fputs(tag, stderr);
    std::fflush(stderr);
}
} // namespace

bool isPipelineStrictMode()
{
    static const bool kStrict = isPipelineStrictEnabled();
    return kStrict;
}

namespace {
// Trace path: RAWRXD_PIPELINE_TRACE=<file>. When set, every UI pipeline call
// emits a JSON envelope identical in shape to `rawrxd run --emit-json-trace`.
std::string pipelineTracePath()
{
    return readProcessEnv("RAWRXD_PIPELINE_TRACE");
}

std::string canonicalModelIdentity(const std::string& model)
{
#if defined(_WIN32)
    char out[MAX_PATH];
    DWORD n = GetFullPathNameA(model.c_str(), MAX_PATH, out, nullptr);
    if (n > 0 && n < MAX_PATH)
        return std::string(out, n);
#endif
    return model;
}

std::string currentBuildConfig()
{
#if defined(NDEBUG)
    return "Release";
#else
    return "Debug";
#endif
}

std::string currentBuildCommit()
{
    const std::string value = readProcessEnv("RAWRXD_BUILD_COMMIT");
    return value.empty() ? std::string("unknown") : value;
}

void stampTraceBackend(RawrXD::ParityTrace::Recorder& trace, const char* fallbackBackend)
{
    const auto& st = rxd::gpu::status();
    switch (st.active)
    {
        case rxd::gpu::Backend::Vulkan:
            trace.setBackend("vulkan", st.device_name);
            return;
        case rxd::gpu::Backend::Cuda:
            trace.setBackend("cuda", st.device_name);
            return;
        case rxd::gpu::Backend::Hip:
            trace.setBackend("hip", st.device_name);
            return;
        case rxd::gpu::Backend::None:
        default:
            trace.setBackend(fallbackBackend ? fallbackBackend : "unknown", st.device_name);
            return;
    }
}

void logPipelineModeBanner(const char* mode)
{
    char line[96] = {};
    std::snprintf(line, sizeof(line), "[PIPELINE MODE] %s\n", (mode && mode[0]) ? mode : "unknown");
    pipelineDebugMark(line);
}

const char* currentPipelineMode()
{
    const auto& st = rxd::gpu::status();
    switch (st.active)
    {
        case rxd::gpu::Backend::Vulkan:
        case rxd::gpu::Backend::Cuda:
        case rxd::gpu::Backend::Hip:
            return "gpu";
        case rxd::gpu::Backend::None:
        default:
            return "cpu";
    }
}
} // namespace

bool isInferencePipelineReady()
{
    return RawrXD::Serve::InferencePlugin::hasPlugin();
}

std::string initInferencePipeline(const std::string& modelPath)
{
    if (!RawrXD::Serve::InferencePlugin::hasPlugin())
    {
        std::string detail;
        if (!RawrXD::Serve::InferencePlugin::tryLoad(detail))
            return std::string("InferencePlugin DLL not found: ") + detail;
    }

    std::string err;
    if (!RawrXD::Serve::InferencePlugin::loadModel(modelPath, err))
        return std::string("Model load failed: ") + err;

    return {};
}

bool runLocalInferencePipeline(const PipelineRequest& req, const InferenceCallbacks& cbs)
{
    // Parity CPU fallback short-circuit. Both CLI and UI invoke the same
    // deterministic generator → byte-identical token streams without GPU.
    if (RawrXD::ParityFallback::isActive())
    {
        logPipelineModeBanner("parity-cpu");
        pipelineDebugMark("[PIPELINE PARITY-CPU] deterministic fallback active\n");

        const std::string tracePath = pipelineTracePath();
        RawrXD::ParityTrace::Recorder trace;
        if (!tracePath.empty()) {
            trace.start("ui-pipeline", canonicalModelIdentity(req.model), req.prompt);
            trace.setBuildInfo(currentBuildCommit(), currentBuildConfig());
            trace.setPipelineMode("parity-cpu");
            trace.setBackendDetails("cpu-fallback", "ParityCpuFallback", "n/a", "n/a", "parity-cpu");
            trace.setInferenceConfig(req.numPredict, req.temperature, -1, -1.0f, -1);
        }

        RawrXD::ParityFallback::run(
            req.model, req.prompt, req.numPredict,
            [&cbs, &trace, &tracePath](const std::string& tok, bool done)
            {
                if (!tracePath.empty()) trace.onToken(tok, done);
                if (cbs.onToken) cbs.onToken(tok, done);
            });

        if (!tracePath.empty())
        {
            trace.onComplete();
            RawrXD::ParityTrace::writeJson(trace, tracePath);
            pipelineDebugMark("[PIPELINE TRACE] wrote trace JSON (parity-cpu)\n");
        }
        if (cbs.onComplete) cbs.onComplete({});
        return true;
    }

    if (!RawrXD::Serve::InferencePlugin::hasPlugin())
    {
        if (isPipelineStrictMode())
            pipelineDebugMark("[PIPELINE STRICT] hasPlugin=false — refusing fallback\n");
        return false;
    }

    logPipelineModeBanner(currentPipelineMode());
    pipelineDebugMark("[PIPELINE ACTIVE] runLocalInferencePipeline entered\n");

    using RawrXD::Serve::GenerateRequest;
    using RawrXD::Serve::ChatMessage;

    // Optional structured parity trace.
    const std::string tracePath = pipelineTracePath();
    RawrXD::ParityTrace::Recorder trace;
    if (!tracePath.empty()) {
        trace.start("ui-pipeline", canonicalModelIdentity(req.model), req.prompt);
        trace.setBuildInfo(currentBuildCommit(), currentBuildConfig());
        trace.setPipelineMode(currentPipelineMode());
        stampTraceBackend(trace, "plugin");
        trace.setInferenceConfig(req.numPredict, req.temperature, -1, -1.0f, -1);
    }

    GenerateRequest gr;
    gr.model       = req.model;
    gr.prompt      = req.prompt;
    gr.temperature = req.temperature;
    gr.num_predict = req.numPredict;
    gr.stream      = req.stream;
    for (const auto& m : req.messages)
    {
        ChatMessage cm;
        cm.role    = m.role;
        cm.content = m.content;
        gr.messages.push_back(std::move(cm));
    }

    std::string err;
    RawrXD::Serve::InferencePlugin::generate(
        gr,
        [&cbs, &trace, &tracePath](const std::string& token, bool done)
        {
            if (!tracePath.empty()) trace.onToken(token, done);
            if (cbs.onToken)
                cbs.onToken(token, done);
        },
        err);

    if (!err.empty())
    {
        if (!tracePath.empty()) {
            trace.onError(err);
            RawrXD::ParityTrace::writeJson(trace, tracePath);
        }
        if (cbs.onError)
            cbs.onError(err);
        return false;
    }

    if (!tracePath.empty()) {
        trace.onComplete();
        RawrXD::ParityTrace::writeJson(trace, tracePath);
        pipelineDebugMark("[PIPELINE TRACE] wrote trace JSON\n");
    }
    if (cbs.onComplete)
    {
        // Accumulated text is not tracked here — callers accumulate via onToken.
        cbs.onComplete({});
    }
    pipelineDebugMark("[PIPELINE DONE] runLocalInferencePipeline ok\n");
    return true;
}

} // namespace RawrXD
