#pragma once

#include "ai_chat_panel.hpp"
#include <QObject>
#include <QPointer>
#include <QVector>
#include <QString>

class AIChatPanelManager : public QObject {
    Q_OBJECT
public:
    explicit AIChatPanelManager(QObject* parent = nullptr);

    AIChatPanel* createPanel(QWidget* parent = nullptr);
    bool destroyPanel(AIChatPanel* panel);
    int panelCount() const;
    int maxPanels() const { return m_maxPanels; }

    void setCloudConfig(bool enabled, const QString& endpoint, const QString& apiKey);
    void setLocalConfig(bool enabled, const QString& endpoint);
    void setRequestTimeout(int timeoutMs);

signals:
    void panelCreated(AIChatPanel* panel);
    void panelDestroyed();

private:
    void applyConfig(AIChatPanel* panel);

    QVector<QPointer<AIChatPanel>> m_panels;
    const int m_maxPanels = 100;

    // shared configuration propagated to panels
    bool m_cloudEnabled = false;
    bool m_localEnabled = true;
    QString m_cloudEndpoint = "https://api.openai.com/v1/chat/completions";
    QString m_localEndpoint = "http://localhost:11434/api/generate";
    QString m_apiKey;
    int m_requestTimeout = 30000;
};
