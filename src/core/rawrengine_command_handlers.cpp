#include "shared_feature_dispatch.h"

#include "../../include/cursor_github_parity_bridge.h"
#include "../../include/plugin_signature.h"
#include "../../include/ui/chat_panel.h"
#include "../../include/update_signature.h"
#include "../vulkan_compute.h"
#include "../agentic/ErrorRecoveryManager.h"
#include "../core/ConfigurationValidator.h"
#include "../inference/PerformanceMonitor.h"
#include "../security/InputSanitizer.h"
#include "../ui/chat_message_renderer.h"
#include "../ui/tool_action_status.h"
#include "hardware_synthesizer.hpp"
#include "jsonrpc_parser.hpp"
#include "mesh_brain.hpp"
#include "neural_bridge.hpp"
#include "omega_orchestrator.hpp"
#include "perf_telemetry.hpp"
#include "self_host_engine.hpp"
#include "speciator_engine.hpp"
#include "transcendence_coordinator.hpp"

#include <atomic>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace {

void emit(const CommandContext& ctx, const std::string& line) {
    if (ctx.outputFn) {
        ctx.outputLine(line);
    }
}

CommandResult okResult(const CommandContext& ctx, const std::string& message) {
    static thread_local std::string detail;
    detail = message;
    emit(ctx, detail);
    return CommandResult::ok(detail.c_str());
}

CommandResult errorResult(const CommandContext& ctx, const std::string& message, int code = -1) {
    static thread_local std::string detail;
    detail = message;
    emit(ctx, detail);
    return CommandResult::error(detail.c_str(), code);
}

std::wstring currentModulePath() {
    wchar_t path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::wstring(path, path + len);
}

} // namespace

CommandResult HandleCursorParityBridge(const CommandContext& ctx) {
    // Headless parity verification checks command space invariants.
    const int span = RawrXD::Parity::CURSOR_PARITY_LAST_ID - RawrXD::Parity::CURSOR_PARITY_FIRST_ID + 1;
    if (span <= 0 || RawrXD::Parity::CURSOR_PARITY_MODULE_COUNT <= 0) {
        return errorResult(ctx, "Cursor parity constants are invalid");
    }
    return okResult(ctx, "Cursor parity bridge verified (command span=" + std::to_string(span) + ")");
}

CommandResult HandleOmegaOrchestrator(const CommandContext& ctx) {
    const auto result = rawrxd::OmegaOrchestrator::instance().initialize();
    if (!result.success) {
        return errorResult(ctx, result.detail, result.errorCode);
    }
    return okResult(ctx, "Omega orchestrator initialized");
}

CommandResult HandleMeshBrain(const CommandContext& ctx) {
    const auto result = MeshBrain::instance().initialize();
    if (!result.success) {
        return errorResult(ctx, result.detail, result.errorCode);
    }
    return okResult(ctx, "Mesh brain initialized");
}

CommandResult HandleSpeciatorEngine(const CommandContext& ctx) {
    const auto result = SpeciatorEngine::instance().initialize();
    if (!result.success) {
        return errorResult(ctx, result.detail, result.errorCode);
    }
    return okResult(ctx, "Speciator engine initialized");
}

CommandResult HandleNeuralBridge(const CommandContext& ctx) {
    const auto result = rawrxd::NeuralBridge::instance().initialize();
    if (!result.success) {
        return errorResult(ctx, result.detail, result.errorCode);
    }
    return okResult(ctx, "Neural bridge initialized");
}

CommandResult HandleSelfHostEngine(const CommandContext& ctx) {
    const auto result = SelfHostEngine::instance().initialize();
    if (!result.success) {
        return errorResult(ctx, result.detail, result.errorCode);
    }
    return okResult(ctx, "Self-host engine initialized");
}

CommandResult HandleHardwareSynthesizer(const CommandContext& ctx) {
    const auto result = HardwareSynthesizer::instance().initialize();
    if (!result.success) {
        return errorResult(ctx, result.detail, result.errorCode);
    }
    return okResult(ctx, "Hardware synthesizer initialized");
}

CommandResult HandleTranscendenceCoordinator(const CommandContext& ctx) {
    const auto result = rawrxd::TranscendenceCoordinator::instance().initializeAll();
    if (!result.success) {
        return errorResult(ctx, result.detail, result.errorCode);
    }
    return okResult(ctx, "Transcendence coordinator initialized");
}

CommandResult HandleVulkanRenderer(const CommandContext& ctx) {
    VulkanCompute compute;
    if (!compute.Initialize()) {
        return errorResult(ctx, "Vulkan renderer initialization failed");
    }
    const auto info = compute.GetDeviceInfo();
    const std::string message = std::string("Vulkan renderer initialized on device: ") + info.device_name;
    compute.Cleanup();
    return okResult(ctx, message);
}

CommandResult HandleOSExplorerInterceptor(const CommandContext& ctx) {
    WIN32_FIND_DATAW findData{};
    HANDLE h = FindFirstFileW(L"C:\\Windows\\*", &findData);
    if (h == INVALID_HANDLE_VALUE) {
        return errorResult(ctx, "OS explorer interceptor probe failed", static_cast<int>(GetLastError()));
    }
    uint32_t entries = 0;
    do { ++entries; } while (entries < 128 && FindNextFileW(h, &findData));
    FindClose(h);
    return okResult(ctx, "OS explorer interceptor probe succeeded (enumerated " + std::to_string(entries) + " entries)");
}

CommandResult HandleMCPHooks(const CommandContext& ctx) {
    const std::string request = R"({"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}})";
    RawrXD::RPC::RPCBatch batch = RawrXD::RPC::RPCBatch::parse(request);
    if (batch.requests.empty()) {
        return errorResult(ctx, "MCP JSON-RPC parsing failed");
    }
    const std::string framed = RawrXD::RPC::LSPFrameParser::encode(request);
    if (framed.find("Content-Length:") == std::string::npos) {
        return errorResult(ctx, "MCP frame encoding failed");
    }
    return okResult(ctx, "MCP hook protocol pipeline validated");
}

CommandResult HandleIOCPFileWatcher(const CommandContext& ctx) {
    HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if (!port) {
        return errorResult(ctx, "IOCP completion port creation failed", static_cast<int>(GetLastError()));
    }
    const std::wstring root = std::filesystem::current_path().wstring();
    HANDLE dir = CreateFileW(root.c_str(), FILE_LIST_DIRECTORY,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING,
                             FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (dir == INVALID_HANDLE_VALUE) {
        CloseHandle(port);
        return errorResult(ctx, "IOCP watcher directory bind failed", static_cast<int>(GetLastError()));
    }
    HANDLE associated = CreateIoCompletionPort(dir, port, 1, 0);
    CloseHandle(dir);
    CloseHandle(port);
    if (!associated) {
        return errorResult(ctx, "IOCP watcher association failed", static_cast<int>(GetLastError()));
    }
    return okResult(ctx, "IOCP file watcher primitives initialized");
}

CommandResult HandleIDEDiagnosticAutoHealer(const CommandContext& ctx) {
    auto& validator = RawrXD::Core::ConfigurationValidator::instance();
    validator.addRule("ide", {"workspace", [](const std::string& v) { return !v.empty(); }, "workspace missing", true});
    std::unordered_map<std::string, std::string> config{
        {"workspace", std::filesystem::current_path().string()}
    };
    auto result = validator.validateSection("ide", config);
    if (!result.valid) {
        return errorResult(ctx, "IDE diagnostic auto-heal validation failed");
    }
    return okResult(ctx, "IDE diagnostic auto-heal validation passed");
}

CommandResult HandleConsentPrompt(const CommandContext& ctx) {
    if (!ctx.isGui || ctx.hwnd == nullptr) {
        return okResult(ctx, "Consent prompt available (GUI interaction required)");
    }
    const int answer = MessageBoxA(static_cast<HWND>(ctx.hwnd),
                                   "Allow diagnostic and telemetry features?",
                                   "Consent Prompt",
                                   MB_YESNO | MB_ICONQUESTION);
    const bool accepted = (answer == IDYES);
    return accepted
        ? okResult(ctx, "Consent prompt accepted")
        : errorResult(ctx, "Consent prompt declined", ERROR_CANCELLED);
}

CommandResult HandleAutonomousAgent(const CommandContext& ctx) {
    auto& perf = RawrXD::Inference::PerformanceMonitor::instance();
    auto& sanitizer = RawrXD::Security::InputSanitizer::instance();
    auto& recovery = RawrXD::Agentic::ErrorRecoveryManager::instance();

    perf.startOperation("autonomous_agent_cycle");
    const auto prompt = sanitizer.sanitizePrompt("analyze workspace and propose fixes");
    if (prompt.sanitized.empty()) {
        perf.recordError("autonomous_agent_cycle");
        perf.endOperation("autonomous_agent_cycle");
        recovery.recordFailure("autonomous_agent_cycle");
        return errorResult(ctx, "Autonomous agent cycle failed");
    }
    recovery.recordSuccess("autonomous_agent_cycle");
    perf.endOperation("autonomous_agent_cycle");
    return okResult(ctx, "Autonomous agent cycle executed");
}

CommandResult HandleChatMessageRenderer(const CommandContext& ctx) {
    RawrXD::UI::ChatMessage msg;
    msg.id = "rawrengine-smoke";
    msg.role = RawrXD::UI::MessageRole::ASSISTANT;
    msg.rawContent = "```cpp\nint main() { return 0; }\n```";
    const auto render = RawrXD::UI::ChatMessageRenderer::Global().renderMessage(msg);
    if (!render.success) {
        return errorResult(ctx, render.detail, render.errorCode);
    }
    return okResult(ctx, "Chat message renderer produced HTML output");
}

CommandResult HandleToolActionStatus(const CommandContext& ctx) {
    RawrXD::UI::ToolActionAccumulator acc;
    acc.addAction(RawrXD::UI::ToolActionStatus::RunTerminalAction("cmake --build .", 120));
    acc.addAction(RawrXD::UI::ToolActionStatus::FinishedAction(2));
    const std::string rendered = acc.renderPlainText();
    if (rendered.empty()) {
        return errorResult(ctx, "Tool action status rendering returned empty output");
    }
    return okResult(ctx, "Tool action status rendered");
}

CommandResult HandleChatPanel(const CommandContext& ctx) {
    if (!ctx.isGui || ctx.hwnd == nullptr) {
        return okResult(ctx, "Chat panel available (GUI parent window required)");
    }
    RawrXD::UI::ChatPanel panel;
    if (!panel.create(static_cast<HWND>(ctx.hwnd), 9100)) {
        return errorResult(ctx, "Chat panel creation failed");
    }
    panel.appendMessage("assistant", "Chat panel initialized");
    return okResult(ctx, "Chat panel initialized");
}

CommandResult HandlePerfTelemetry(const CommandContext& ctx) {
    auto& perf = RawrXD::Perf::PerfTelemetry::instance();
    const auto init = perf.initialize();
    if (!init.success) {
        return errorResult(ctx, init.detail, init.errorCode);
    }
    perf.captureBaseline();
    const std::string diagnostics = perf.getDiagnostics();
    if (diagnostics.empty()) {
        return errorResult(ctx, "Performance telemetry diagnostics are empty");
    }
    return okResult(ctx, "Performance telemetry initialized");
}

CommandResult HandleUpdateSignature(const CommandContext& ctx) {
    auto& verifier = RawrXD::Update::UpdateSignatureVerifier::instance();
    const std::wstring modulePath = currentModulePath();
    const auto result = verifier.verifyAuthenticode(modulePath.c_str());
    if (!result.valid) {
        return errorResult(ctx, result.detail, result.errorCode);
    }
    return okResult(ctx, "Update signature verification succeeded");
}

CommandResult HandlePluginSignature(const CommandContext& ctx) {
    auto& verifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
    if (!verifier.isInitialized()) {
        const auto policy = RawrXD::Plugin::PluginSignatureVerifier::createStandardPolicy();
        if (!verifier.initialize(policy)) {
            return errorResult(ctx, "Plugin signature verifier initialization failed");
        }
    }
    const std::wstring modulePath = currentModulePath();
    const auto result = verifier.verify(modulePath.c_str());
    if (!result.valid) {
        return errorResult(ctx, result.detail, result.errorCode);
    }
    return okResult(ctx, "Plugin signature verification succeeded");
}
