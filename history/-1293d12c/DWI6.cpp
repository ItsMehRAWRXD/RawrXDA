#include "ai_chat_panel_manager.hpp"
#include <QDebug>
#include <algorithm>

AIChatPanelManager::AIChatPanelManager(QObject* parent)
    : QObject(parent) {}

AIChatPanel* AIChatPanelManager::createPanel(QWidget* parent)
{
    // Clean up any deleted pointers
    m_panels.erase(std::remove_if(m_panels.begin(), m_panels.end(), [](const QPointer<AIChatPanel>& p){ return p.isNull(); }), m_panels.end());

    if (m_panels.size() >= m_maxPanels) {
        qWarning() << "Max AI chat panels reached:" << m_maxPanels;
        return nullptr;
    }

    auto* panel = new AIChatPanel(parent);
    panel->initialize();
    applyConfig(panel);

    m_panels.push_back(panel);
    emit panelCreated(panel);

    qDebug() << "AIChatPanelManager created panel. Active:" << m_panels.size();
    return panel;
}

bool AIChatPanelManager::destroyPanel(AIChatPanel* panel)
{
    if (!panel) return false;
    auto it = std::find_if(m_panels.begin(), m_panels.end(), [panel](const QPointer<AIChatPanel>& p){ return p.data() == panel; });
    if (it == m_panels.end()) return false;

    (*it)->deleteLater();
    m_panels.erase(it);
    emit panelDestroyed();
    qDebug() << "AIChatPanelManager destroyed panel. Active:" << m_panels.size();
    return true;
}

int AIChatPanelManager::panelCount() const
{
    int count = 0;
    for (const auto& p : m_panels) if (!p.isNull()) ++count;
    return count;
}

void AIChatPanelManager::setCloudConfig(bool enabled, const QString& endpoint, const QString& apiKey)
{
    m_cloudEnabled = enabled;
    m_cloudEndpoint = endpoint;
    m_apiKey = apiKey;

    for (auto& p : m_panels) {
        if (!p.isNull()) p->setCloudConfiguration(m_cloudEnabled, m_cloudEndpoint, m_apiKey);
    }
    qDebug() << "AIChatPanelManager applied cloud config to" << panelCount() << "panels";
}

void AIChatPanelManager::setLocalConfig(bool enabled, const QString& endpoint)
{
    m_localEnabled = enabled;
    m_localEndpoint = endpoint;
    for (auto& p : m_panels) {
        if (!p.isNull()) p->setLocalConfiguration(m_localEnabled, m_localEndpoint);
    }
    qDebug() << "AIChatPanelManager applied local config to" << panelCount() << "panels";
}

void AIChatPanelManager::setRequestTimeout(int timeoutMs)
{
    m_requestTimeout = timeoutMs;
    for (auto& p : m_panels) {
        if (!p.isNull()) p->setRequestTimeout(m_requestTimeout);
    }
}

void AIChatPanelManager::applyConfig(AIChatPanel* panel)
{
    panel->setCloudConfiguration(m_cloudEnabled, m_cloudEndpoint, m_apiKey);
    panel->setLocalConfiguration(m_localEnabled, m_localEndpoint);
    panel->setRequestTimeout(m_requestTimeout);
}
