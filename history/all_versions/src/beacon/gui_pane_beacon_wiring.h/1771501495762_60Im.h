#pragma once
// ============================================================================
// gui_pane_beacon_wiring.h — Wires rebased GUI panes to the BeaconHub
// ============================================================================
// Connects FixedDockWidgets (TodoDock, ObservabilityDashboard, TrainingDialog,
// ModelRouterWidget) and SecureHotpatchOrchestrator to the circular beacon
// via PanelBeaconBridge.
//
// Uses existing APIs:
//   - BeaconHub::instance() / PanelBeaconBridge from circular_beacon_system.h
//   - FixedDockWidgets from ui/FixedDockWidgets.h (RawrXD::Window-based)
//   - SecureHotpatchOrchestrator from security/SecureHotpatchOrchestrator.h
//
// No Qt. No exceptions. C++20 only.
// ============================================================================

#include "BeaconClient.h"
#include "../../include/circular_beacon_system.h"
#include "../ui/FixedDockWidgets.h"
#include "../security/SecureHotpatchOrchestrator.h"
#include "../EventBus.h"
#include <string>
#include <iostream>

namespace RawrXD {

class GUIPaneBeaconWiring {
    WinHTTPBeaconClient* m_httpClient;  // Optional outbound HTTP client
    
    // PanelBeaconBridge instances for each GUI pane
    PanelBeaconBridge m_todoBridge;
    PanelBeaconBridge m_obsBridge;
    PanelBeaconBridge m_trainingBridge;
    PanelBeaconBridge m_routerBridge;

public:
    explicit GUIPaneBeaconWiring(WinHTTPBeaconClient* httpClient = nullptr)
        : m_httpClient(httpClient) {}

    // ── Wire TodoDock into beacon ring ──
    void WireTodoDock(TodoDock* todo) {
        if (!todo) return;

        // Register as a beacon panel
        m_todoBridge.init(BeaconKind::PanelAgent, "TodoDock", todo,
            [todo](const BeaconMessage& msg) -> BeaconResponse {
                if (msg.verb && std::string(msg.verb) == "todo.add" && msg.payload) {
                    todo->AddItem(std::string(msg.payload, msg.payloadLen));
                    return {true, 0, "item_added", BeaconKind::PanelAgent};
                }
                if (msg.verb && std::string(msg.verb) == "todo.refresh") {
                    todo->Refresh();
                    return {true, 0, "refreshed", BeaconKind::PanelAgent};
                }
                return {false, 404, "unknown_verb", BeaconKind::PanelAgent};
            });

        // When a todo is toggled, broadcast via beacon and EventBus
        todo->OnItemToggled().connect([this](const TodoItem& item) {
            std::string payload = item.text + "|" + (item.done ? "done" : "pending");
            m_todoBridge.sendTo(BeaconKind::AgenticEngine, "todo.toggled",
                                payload.c_str(), payload.size());

            if (m_httpClient) {
                m_httpClient->Send(BeaconKind::AgenticEngine, "todo.toggled", payload);
            }
        });

        std::cout << "[BeaconWiring] TodoDock wired to beacon ring\n";
    }

    // ── Wire ObservabilityDashboard into beacon ring ──
    void WireObservability(ObservabilityDashboard* obs) {
        if (!obs) return;

        m_obsBridge.init(BeaconKind::PanelTelemetry, "Observability", obs,
            [obs](const BeaconMessage& msg) -> BeaconResponse {
                if (msg.verb && std::string(msg.verb) == "metric.update") {
                    // Received metric update from another beacon (e.g., InferenceEngine)
                    return {true, 0, "metric_received", BeaconKind::PanelTelemetry};
                }
                return {false, 404, "unknown_verb", BeaconKind::PanelTelemetry};
            });

        obs->OnMetricsUpdated().connect([this]() {
            m_obsBridge.broadcast("observability.updated");
        });

        std::cout << "[BeaconWiring] ObservabilityDashboard wired to beacon ring\n";
    }

    // ── Wire TrainingDialog into beacon ring ──
    void WireTraining(TrainingDialog* train) {
        if (!train) return;

        m_trainingBridge.init(BeaconKind::PanelPipeline, "Training", train,
            [train](const BeaconMessage& msg) -> BeaconResponse {
                if (msg.verb && std::string(msg.verb) == "training.progress") {
                    if (msg.payload && msg.payloadLen >= sizeof(int)) {
                        int pct = *reinterpret_cast<const int*>(msg.payload);
                        train->SetProgress(pct);
                        return {true, 0, "progress_set", BeaconKind::PanelPipeline};
                    }
                }
                return {false, 404, "unknown_verb", BeaconKind::PanelPipeline};
            });

        train->OnEpochComplete().connect([this](float loss) {
            std::string payload = std::to_string(loss);
            m_trainingBridge.sendTo(BeaconKind::AgenticEngine, "training.epoch_done",
                                    payload.c_str(), payload.size());
            EventBus::Get().AgentMessage.emit("[Training] Epoch complete, loss: " + payload);
        });

        std::cout << "[BeaconWiring] TrainingDialog wired to beacon ring\n";
    }

    // ── Wire ModelRouterWidget into beacon ring ──
    void WireModelRouter(ModelRouterWidget* router) {
        if (!router) return;

        m_routerBridge.init(BeaconKind::LLMRouter, "ModelRouter", router,
            [router](const BeaconMessage& msg) -> BeaconResponse {
                if (msg.verb && std::string(msg.verb) == "model.add" && msg.payload) {
                    router->AddModel(std::string(msg.payload, msg.payloadLen));
                    return {true, 0, "model_added", BeaconKind::LLMRouter};
                }
                return {false, 404, "unknown_verb", BeaconKind::LLMRouter};
            });

        router->OnModelSelected().connect([this](const std::string& model) {
            m_routerBridge.sendTo(BeaconKind::InferenceEngine, "model.load",
                                  model.c_str(), model.size());

            if (m_httpClient) {
                m_httpClient->Send(BeaconKind::InferenceEngine, "model.load", model);
            }

            EventBus::Get().AgentMessage.emit("[ModelRouter] Selected: " + model);
        });

        std::cout << "[BeaconWiring] ModelRouterWidget wired to beacon ring\n";
    }

    // ── Wire SecureHotpatch into beacon ring for auth-gated patching ──
    void WireSecurityBridge(SecureHotpatchOrchestrator* secPatch) {
        if (!secPatch) return;

        secPatch->OnPatchAuthorized().connect([this](const std::string& name, bool ok) {
            std::string payload = name + "|" + (ok ? "applied" : "denied");
            BeaconHub::instance().send(
                BeaconKind::HotpatchManager,
                BeaconKind::IDECore,
                "hotpatch.auth_result",
                payload.c_str(), payload.size());

            if (m_httpClient) {
                m_httpClient->Send(BeaconKind::IDECore, "hotpatch.auth_result", payload);
            }
        });

        std::cout << "[BeaconWiring] SecureHotpatchOrchestrator wired to beacon ring\n";
    }

    // ── Wire incoming beacon hotpatch commands to HTTP handler ──
    void WireInboundHotpatchHandler() {
        if (!m_httpClient) return;

        m_httpClient->RegisterHandler("hotpatch.apply", [](const BeaconMessage& msg) {
            if (msg.payload && msg.payloadLen > 0) {
                std::string patchId(msg.payload, msg.payloadLen);
                std::cout << "[BeaconWiring] Inbound hotpatch request: " << patchId << "\n";
                // Route through BeaconHub to HotpatchManager
                BeaconHub::instance().send(
                    BeaconKind::IDECore,
                    BeaconKind::HotpatchManager,
                    "hotpatch.apply",
                    msg.payload, msg.payloadLen);
            }
        });

        m_httpClient->RegisterHandler("agentic.execute", [](const BeaconMessage& msg) {
            if (msg.payload && msg.payloadLen > 0) {
                std::cout << "[BeaconWiring] Inbound agentic request via beacon\n";
                BeaconHub::instance().send(
                    BeaconKind::IDECore,
                    BeaconKind::AgenticEngine,
                    "agent.execute",
                    msg.payload, msg.payloadLen);
            }
        });
    }
};

} // namespace RawrXD
