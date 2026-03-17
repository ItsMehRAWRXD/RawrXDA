#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

class AgentChatBreadcrumb : public QObject {
    Q_OBJECT

public:
    explicit AgentChatBreadcrumb(QObject* parent = nullptr)
        : QObject(parent) {}

    QString tooltipForModel(const QString& modelName) const {
        if (modelName.isEmpty()) {
            return QString();
        }
        if (m_modelTooltips.contains(modelName)) {
            return m_modelTooltips.value(modelName);
        }
        return QString("<b>%1</b><br/>Source: Ollama").arg(modelName);
    }

    void setModelTooltip(const QString& modelName, const QString& tooltip) {
        if (modelName.isEmpty()) {
            return;
        }
        if (tooltip.isEmpty()) {
            m_modelTooltips.remove(modelName);
        } else {
            m_modelTooltips.insert(modelName, tooltip);
        }
    }

    void updateOllamaModels(const QStringList& models) {
        m_ollamaModels = models;
        emit ollamaModelsUpdated();
    }

    QStringList ollamaModels() const {
        return m_ollamaModels;
    }

signals:
    void ollamaModelsUpdated();

private:
    QHash<QString, QString> m_modelTooltips;
    QStringList m_ollamaModels;
};
