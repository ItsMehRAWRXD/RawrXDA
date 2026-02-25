/**
 * @file agent_chat_breadcrumb.hpp
 * @brief Agent chat breadcrumb / trail UI (stub for patchable build).
 */
#pragma once

#include <string>
#include <vector>
#include <memory>

#if defined(QT_VERSION) || defined(Q_MOC_RUN)
#include <QObject>
#include <QString>
class AgentChatBreadcrumb : public QObject {
    Q_OBJECT
public:
    explicit AgentChatBreadcrumb(QObject* parent = nullptr) : QObject(parent) {}
    void push(const std::string& label) { (void)label; }
    void pop() {}
    void clear() {}
    std::vector<std::string> trail() const { return {}; }
    QString tooltipForModel(const QString& modelName) const { return modelName; }
signals:
    void ollamaModelsUpdated();
};
#else
class AgentChatBreadcrumb {
public:
    AgentChatBreadcrumb(void* parent = nullptr) { (void)parent; }
    virtual ~AgentChatBreadcrumb() = default;
    void push(const std::string& label) { (void)label; }
    void pop() {}
    void clear() {}
    std::vector<std::string> trail() const { return {}; }
    std::string tooltipForModel(const std::string& modelName) const { return modelName; }
};
#endif
