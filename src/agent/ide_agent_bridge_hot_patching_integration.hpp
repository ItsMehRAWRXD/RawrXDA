// ============================================================================
// File: src/agent/ide_agent_bridge_hot_patching_integration.hpp
// 
// Purpose: Integration of hot patching into IDEAgentBridge
// Seamless wiring for transparent hallucination correction
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#pragma once

#include "ide_agent_bridge.hpp"
#include "agent_hot_patcher.hpp"
#include "gguf_proxy_server.hpp"

#include <memory>
#include <QObject>
#include <QJsonObject>

/**
 * @class IDEAgentBridgeWithHotPatching
 * @brief Extended IDEAgentBridge with real-time hallucination correction
 * 
 * Extends IDEAgentBridge to include hot patching system:
 * - Automatically starts proxy server on initialization
 * - Redirects ModelInvoker to proxy (11435)
 * - Applies real-time corrections transparently
 * - Provides full monitoring and statistics
 * 
 * Usage:
 * \code
 * auto bridge = std::make_unique<IDEAgentBridgeWithHotPatching>();
 * bridge->initializeWithHotPatching();
 * bridge->startHotPatchingProxy();
 * 
 * // Now all model outputs are automatically corrected
 * auto plan = bridge->generateExecutionPlan("user wish");
 * bridge->executeWithApproval(plan);
 * \endcode
 */
class IDEAgentBridgeWithHotPatching : public IDEAgentBridge {
    Q_OBJECT

public:
    explicit IDEAgentBridgeWithHotPatching(QObject* parent = nullptr);
    ~IDEAgentBridgeWithHotPatching() override;

    /**
     * Initialize bridge with hot patching system
     * @note This MUST be called before any agent operations
     */
    void initializeWithHotPatching();

    /**
     * Start hot patching proxy server
     * @return true if started successfully
     */
    bool startHotPatchingProxy();

    /**
     * Stop hot patching proxy server
     */
    void stopHotPatchingProxy();

    /**
     * Get hot patcher instance
     */
    AgentHotPatcher* getHotPatcher() const;

    /**
     * Get proxy server instance
     */
    GGUFProxyServer* getProxyServer() const;

    /**
     * Check if hot patching is active
     */
    bool isHotPatchingActive() const;

    /**
     * Get hot patching statistics
     */
    QJsonObject getHotPatchingStatistics() const;

    /**
     * Enable/disable hot patching at runtime
     */
    void setHotPatchingEnabled(bool enabled);

    /**
     * Load correction patterns from database
     */
    void loadCorrectionPatterns(const QString& databasePath);

    /**
     * Load behavior patches from database
     */
    void loadBehaviorPatches(const QString& databasePath);

    /**
     * Runtime configuration: proxy port
     * @note Changing this requires stopping and restarting the proxy
     */
    QString proxyPort() const { return m_proxyPort; }
    void setProxyPort(const QString& port) {
        if (port != m_proxyPort) {
            m_proxyPort = port;
            emit proxyPortChanged();
        }
    }

    /**
     * Runtime configuration: GGUF backend endpoint
     * @note Changing this requires stopping and restarting the proxy
     */
    QString ggufEndpoint() const { return m_ggufEndpoint; }
    void setGgufEndpoint(const QString& endpoint) {
        if (endpoint != m_ggufEndpoint) {
            m_ggufEndpoint = endpoint;
            emit ggufEndpointChanged();
        }
    }

    /**
     * Guard against ModelInvoker replacement
     * Re-wires proxy endpoint if ModelInvoker is recreated
     */
    void onModelInvokerReplaced();

public slots:
    /**
     * Handle hallucination detected signal
     */
    void onHallucinationDetected(const HallucinationDetection& detection);

    /**
     * Handle hallucination corrected signal
     */
    void onHallucinationCorrected(const HallucinationDetection& correction);

    /**
     * Handle navigation error fixed signal
     */
    void onNavigationErrorFixed(const NavigationFix& fix);

    /**
     * Handle behavior patch applied signal
     */
    void onBehaviorPatchApplied(const BehaviorPatch& patch);

signals:
    /**
     * Emitted when proxy port configuration changes
     */
    void proxyPortChanged();

    /**
     * Emitted when GGUF endpoint configuration changes
     */
    void ggufEndpointChanged();

private:
    // Hot patching components
    std::unique_ptr<AgentHotPatcher> m_hotPatcher;
    std::unique_ptr<GGUFProxyServer> m_proxyServer;

    // Configuration
    bool m_hotPatchingEnabled = false;
    QString m_proxyPort = "11435";
    QString m_ggufEndpoint = "localhost:11434";

    // Logging
    void logCorrection(const HallucinationDetection& correction);
    void logNavigationFix(const NavigationFix& fix);
};

#endif // IDE_AGENT_BRIDGE_HOT_PATCHING_INTEGRATION_HPP
